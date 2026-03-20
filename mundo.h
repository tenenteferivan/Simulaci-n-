#pragma once
#include <vector>
#include <list>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <sstream>
#include <iostream>
#include <string>
#include "tags.h"
#include "agente.h"

struct Celda {
    float pasto = 0.0f;
    float agua = 0.0f;
    float carne = 0.0f;
    float feromona_a = 0.0f;
    float feromona_b = 0.0f;
    float mineral = 0.0f;
    float refugio = 0.0f;
    uint32_t tipo = TAG_NONE;
};

class Mundo {
public:
    int width = 0;
    int height = 0;
    float escala = 200.0f;

    std::vector<Celda> mapa;

    float temperatura_global = 22.0f;
    long long ciclo_dia = 0;

    std::list<Agente> gente;

    std::vector<Agente*> cell_head;

    std::vector<float> altitude;
    std::vector<float> moisture;
    std::vector<float> roughness;
    std::vector<float> fertility;

    std::vector<int> food_distance;
    std::vector<int> water_distance;

    Mundo(int w, int h, float s) : width(w), height(h), escala(s) {
        int gw = std::max(1, (int)std::ceil(width / escala));
        int gh = std::max(1, (int)std::ceil(height / escala));
        int total = gw * gh;

        mapa.resize(total);
        cell_head.assign(total, nullptr);

        altitude.assign(total, 0.0f);
        moisture.assign(total, 0.0f);
        roughness.assign(total, 0.0f);
        fertility.assign(total, 0.0f);

        food_distance.assign(total, 999999);
        water_distance.assign(total, 999999);

        seed_world();
    }

    int grid_cols() const { return std::max(1, (int)std::ceil(width / escala)); }
    int grid_rows() const { return std::max(1, (int)std::ceil(height / escala)); }

    void seed_world() {
        int gw = grid_cols();
        int gh = grid_rows();
        size_t n = mapa.size();

        auto r01 = []() -> float { return (rand() % 1000) / 1000.0f; };

        for (size_t i = 0; i < n; ++i) {
            altitude[i] = r01();
            moisture[i] = r01();
            roughness[i] = r01();
            fertility[i] = r01();
        }

        smooth_pass(altitude, 2);
        smooth_pass(moisture, 2);
        smooth_pass(roughness, 1);
        smooth_pass(fertility, 1);

        for (size_t i = 0; i < n; ++i) {
            float alt = altitude[i];
            float moi = moisture[i];
            float rou = roughness[i];
            float fer = fertility[i];

            mapa[i].agua = std::clamp(0.10f + moi * 0.75f + (1.0f - alt) * 0.18f, 0.0f, 1.5f);
            mapa[i].pasto = std::clamp(0.15f + fer * 0.65f + moi * 0.16f - alt * 0.08f, 0.0f, 1.8f);
            mapa[i].mineral = std::clamp(alt * 0.45f + rou * 0.35f, 0.0f, 1.3f);
            mapa[i].refugio = std::clamp((alt * 0.30f + rou * 0.25f) * (rand() % 100 < 20 ? 1.3f : 0.8f), 0.0f, 1.4f);
            refresh_cell(i);
        }

        // zonas iniciales más vivas
        for (int p = 0; p < 6; ++p) {
            size_t c = (size_t)(rand() % (int)n);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int x = (int)(c % gw) + dx;
                    int y = (int)(c / gw) + dy;
                    if (x < 0 || y < 0 || x >= gw || y >= gh) continue;
                    size_t idx = (size_t)y * gw + (size_t)x;
                    mapa[idx].pasto = std::min(2.0f, mapa[idx].pasto + 0.35f);
                    mapa[idx].agua = std::min(1.8f, mapa[idx].agua + 0.30f);
                    fertility[idx] = std::min(1.0f, fertility[idx] + 0.12f);
                    moisture[idx] = std::min(1.0f, moisture[idx] + 0.18f);
                    refresh_cell(idx);
                }
            }
        }

        refresh_all_cells();
    }

    void smooth_pass(std::vector<float>& field, int passes) {
        if (field.empty()) return;
        std::vector<float> tmp(field.size(), 0.0f);

        for (int p = 0; p < passes; ++p) {
            for (int y = 0; y < grid_rows(); ++y) {
                for (int x = 0; x < grid_cols(); ++x) {
                    float sum = 0.0f;
                    int count = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = x + dx;
                            int ny = y + dy;
                            if (nx < 0 || ny < 0 || nx >= grid_cols() || ny >= grid_rows()) continue;
                            sum += field[ny * grid_cols() + nx];
                            count++;
                        }
                    }
                    tmp[y * grid_cols() + x] = sum / std::max(1, count);
                }
            }
            field.swap(tmp);
        }
    }

    size_t coord_to_idx(float x, float y) const {
        int gw = grid_cols();
        int gh = grid_rows();
        int cx = (int)(x / escala);
        int cy = (int)(y / escala);
        cx = std::clamp(cx, 0, gw - 1);
        cy = std::clamp(cy, 0, gh - 1);
        return (size_t)cy * gw + (size_t)cx;
    }

    void refresh_cell(size_t idx) {
        if (idx >= mapa.size()) return;

        uint32_t keep = mapa[idx].tipo & (TAG_PELIGRO);
        mapa[idx].tipo = keep;

        float alt = altitude[idx];
        float moi = moisture[idx];
        float rou = roughness[idx];

        if (alt > 0.80f) {
            mapa[idx].tipo |= TAG_MONTANA | TAG_PIEDRA;
            if (rou > 0.60f) mapa[idx].tipo |= TAG_CUEVA;
        } else if (alt > 0.64f) {
            mapa[idx].tipo |= TAG_COLINA | TAG_PIEDRA;
        } else if (moi < 0.18f && alt > 0.35f) {
            mapa[idx].tipo |= TAG_DESIERTO;
        } else if (moi > 0.72f && alt < 0.42f) {
            mapa[idx].tipo |= TAG_PANTANO | TAG_AGUA;
        } else {
            mapa[idx].tipo |= TAG_FERTIL;
        }

        if (mapa[idx].agua > 0.50f) mapa[idx].tipo |= TAG_AGUA;
        if (mapa[idx].pasto > 0.45f) mapa[idx].tipo |= TAG_PASTO;
        if (mapa[idx].refugio > 0.35f) mapa[idx].tipo |= TAG_REFUGIO;
        if (mapa[idx].mineral > 0.35f) mapa[idx].tipo |= TAG_MINERAL | TAG_PIEDRA;
        if (mapa[idx].carne > 0.20f) mapa[idx].tipo |= TAG_MUERTE;

        if (mapa[idx].feromona_a > 0.12f) mapa[idx].tipo |= TAG_FEROMONA_A;
        if (mapa[idx].feromona_b > 0.12f) mapa[idx].tipo |= TAG_FEROMONA_B;
    }

    void refresh_all_cells() {
        for (size_t i = 0; i < mapa.size(); ++i) refresh_cell(i);
    }

    template<typename Member>
    void diffuse_field(Member Celda::*member, float spread, float decay) {
        size_t n = mapa.size();
        std::vector<float> next(n, 0.0f);
        int gw = grid_cols();
        int gh = grid_rows();

        for (size_t i = 0; i < n; ++i) {
            float v = mapa[i].*member;
            if (v <= 0.0f) continue;

            int x = (int)(i % gw);
            int y = (int)(i / gw);

            int neigh = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    neigh++;
                }
            }

            float remain = v * (1.0f - spread);
            next[i] += remain;

            if (neigh > 0) {
                float share = (v * spread) / (float)neigh;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                        next[(size_t)ny * gw + (size_t)nx] += share;
                    }
                }
            }
        }

        for (size_t i = 0; i < n; ++i) {
            mapa[i].*member = std::clamp(next[i] * decay, 0.0f, 3.0f);
        }
    }

    void actualizar() {
        ciclo_dia++;
        temperatura_global = 20.0f + 12.0f * std::sin((float)ciclo_dia * 0.01f) + frand(-0.8f, 0.8f);

        // clima aleatorio
        if (ciclo_dia % 220 == 0) {
            // lluvia
            for (size_t i = 0; i < mapa.size(); ++i) {
                if ((rand() % 100) < 15) {
                    mapa[i].agua = std::min(2.5f, mapa[i].agua + 0.15f);
                    moisture[i] = std::min(1.0f, moisture[i] + 0.10f);
                }
            }
        }

        if (ciclo_dia % 350 == 0) {
            // sequía
            for (size_t i = 0; i < mapa.size(); ++i) {
                mapa[i].agua = std::max(0.0f, mapa[i].agua - 0.10f);
                moisture[i] = std::max(0.0f, moisture[i] - 0.05f);
            }
        }

        // difusión de agua y feromonas
        diffuse_field(&Celda::agua, 0.18f, 0.998f);
        diffuse_field(&Celda::feromona_a, 0.26f, 0.93f);
        diffuse_field(&Celda::feromona_b, 0.26f, 0.93f);

        for (size_t i = 0; i < mapa.size(); ++i) {
            mapa[i].carne = std::max(0.0f, mapa[i].carne * 0.994f);
            mapa[i].refugio = std::clamp(mapa[i].refugio * 0.9995f, 0.0f, 2.0f);
        }

        refresh_all_cells();
    }

    void build_grid() {
        std::fill(cell_head.begin(), cell_head.end(), nullptr);
        for (auto &a : gente) {
            if (!a.vivo) continue;
            size_t idx = coord_to_idx(a.x, a.y);
            a.cell_idx = idx;
            a.next_in_cell = cell_head[idx];
            cell_head[idx] = &a;
        }
    }

    int count_alive() const {
        int c = 0;
        for (const auto &a : gente) if (a.vivo) c++;
        return c;
    }

    int count_dead_pending() const {
        int c = 0;
        for (const auto &a : gente) if (!a.vivo && a.dead_ticks < 20) c++;
        return c;
    }

    void query_neighbors(const Agente& a, std::vector<Agente*>& out) const {
        out.clear();
        if (!a.vivo) return;

        int gw = grid_cols();
        int gh = grid_rows();
        int cx = (int)(a.x / escala);
        int cy = (int)(a.y / escala);

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;

                Agente* head = cell_head[(size_t)ny * gw + (size_t)nx];
                for (Agente* p = head; p != nullptr; p = p->next_in_cell) {
                    if (p != &a) out.push_back(p);
                }
            }
        }
    }

    int count_neighbors_with_tag(const std::vector<Agente*>& neighbors, uint32_t tag) const {
        int count = 0;
        for (auto* p : neighbors) {
            if (p && p->vivo && (p->tags & tag)) count++;
        }
        return count;
    }

    int count_neighbors_same_tribe(const std::vector<Agente*>& neighbors, int tribe) const {
        int count = 0;
        for (auto* p : neighbors) {
            if (p && p->vivo && p->tribe_id == tribe) count++;
        }
        return count;
    }

    int count_neighbors_enemy(const std::vector<Agente*>& neighbors, int tribe) const {
        int count = 0;
        for (auto* p : neighbors) {
            if (p && p->vivo && p->tribe_id != tribe) count++;
        }
        return count;
    }

    bool cell_has(size_t idx, uint32_t tag) const {
        return idx < mapa.size() && (mapa[idx].tipo & tag) != 0;
    }

    float take_food(size_t idx, float amount) {
        if (idx >= mapa.size()) return 0.0f;
        float got = std::min(amount, mapa[idx].pasto);
        mapa[idx].pasto -= got;
        refresh_cell(idx);
        return got;
    }

    float take_water(size_t idx, float amount) {
        if (idx >= mapa.size()) return 0.0f;
        float got = std::min(amount, mapa[idx].agua);
        mapa[idx].agua -= got;
        refresh_cell(idx);
        return got;
    }

    float take_meat(size_t idx, float amount) {
        if (idx >= mapa.size()) return 0.0f;
        float got = std::min(amount, mapa[idx].carne);
        mapa[idx].carne -= got;
        refresh_cell(idx);
        return got;
    }

    float take_wood(size_t idx, float amount) {
        if (idx >= mapa.size()) return 0.0f;
        float got = std::min(amount, mapa[idx].refugio);
        mapa[idx].refugio -= got;
        refresh_cell(idx);
        return got;
    }

    bool try_build_shelter(size_t idx) {
        if (idx >= mapa.size()) return false;
        float stone = std::min(0.06f, mapa[idx].mineral);
        float wood = std::min(0.08f, mapa[idx].pasto);
        float gain = stone * 0.9f + wood * 0.55f;
        if (gain <= 0.02f) return false;

        mapa[idx].mineral = std::max(0.0f, mapa[idx].mineral - stone);
        mapa[idx].pasto = std::max(0.0f, mapa[idx].pasto - wood);
        mapa[idx].refugio = std::clamp(mapa[idx].refugio + gain, 0.0f, 2.0f);
        refresh_cell(idx);
        return true;
    }

    size_t best_neighbor_by_gradient(const Agente& a, const std::vector<int>& dist_map) const {
        int gw = grid_cols();
        int gh = grid_rows();
        size_t cur = coord_to_idx(a.x, a.y);
        int cx = (int)(cur % gw);
        int cy = (int)(cur / gw);

        size_t best = cur;
        int best_val = dist_map[cur];

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                size_t ni = (size_t)ny * gw + (size_t)nx;
                if (dist_map[ni] < best_val) {
                    best_val = dist_map[ni];
                    best = ni;
                }
            }
        }

        return best;
    }

    size_t best_neighbor_cell_with_tag(const Agente& a, uint32_t wanted) const {
        int gw = grid_cols();
        int gh = grid_rows();
        size_t cur = coord_to_idx(a.x, a.y);
        int cx = (int)(cur % gw);
        int cy = (int)(cur / gw);

        size_t best = cur;
        int best_dist = 999999;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                size_t ni = (size_t)ny * gw + (size_t)nx;
                if (!(mapa[ni].tipo & wanted)) continue;
                int dist = std::abs(dx) + std::abs(dy);
                if (dist < best_dist) {
                    best_dist = dist;
                    best = ni;
                }
            }
        }

        return best;
    }

    void move_agent_toward_cell(Agente& a, size_t target_idx, float step = 120.0f) const {
        if (target_idx >= mapa.size()) return;
        size_t cur = coord_to_idx(a.x, a.y);
        int gw = grid_cols();
        int cx = (int)(cur % gw);
        int cy = (int)(cur / gw);
        int tx = (int)(target_idx % gw);
        int ty = (int)(target_idx / gw);

        if (tx > cx) a.x += step;
        if (tx < cx) a.x -= step;
        if (ty > cy) a.y += step;
        if (ty < cy) a.y -= step;

        a.x = std::clamp(a.x, 0.0f, (float)width - 0.001f);
        a.y = std::clamp(a.y, 0.0f, (float)height - 0.001f);
    }

    void move_agent_away_from_cell(Agente& a, size_t threat_idx, float step = 120.0f) const {
        if (threat_idx >= mapa.size()) return;
        size_t cur = coord_to_idx(a.x, a.y);
        int gw = grid_cols();
        int cx = (int)(cur % gw);
        int cy = (int)(cur / gw);
        int tx = (int)(threat_idx % gw);
        int ty = (int)(threat_idx / gw);

        if (tx > cx) a.x -= step;
        if (tx < cx) a.x += step;
        if (ty > cy) a.y -= step;
        if (ty < cy) a.y += step;

        a.x = std::clamp(a.x, 0.0f, (float)width - 0.001f);
        a.y = std::clamp(a.y, 0.0f, (float)height - 0.001f);
    }

    void compute_distance_map(std::vector<int>& dist, float threshold_food, bool use_water) const {
        const int INF = 999999;
        size_t n = mapa.size();
        dist.assign(n, INF);

        std::deque<int> q;
        for (size_t i = 0; i < n; ++i) {
            float v = use_water ? mapa[i].agua : mapa[i].pasto;
            if (!use_water && mapa[i].carne > threshold_food) v = mapa[i].carne;
            if (v >= threshold_food) {
                dist[i] = 0;
                q.push_back((int)i);
            }
        }

        int gw = grid_cols();
        int gh = grid_rows();

        while (!q.empty()) {
            int i = q.front();
            q.pop_front();
            int cx = i % gw;
            int cy = i / gw;

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    int ni = ny * gw + nx;
                    if (dist[ni] > dist[i] + 1) {
                        dist[ni] = dist[i] + 1;
                        q.push_back(ni);
                    }
                }
            }
        }
    }

    void update_world_resources(int living_population, long long tick) {
        size_t n = mapa.size();
        if (n == 0) return;

        std::vector<int> occupancy(n, 0);
        int gw = grid_cols();
        int gh = grid_rows();

        for (auto &a : gente) {
            if (!a.vivo) continue;
            size_t idx = coord_to_idx(a.x, a.y);
            occupancy[idx]++;
        }

        float crowd_base = std::min(0.85f, living_population * 0.0012f);

        // eventos del mundo
        if (tick % 180 == 0) {
            // lluvia localizada
            size_t c = (size_t)(rand() % (int)n);
            int cx = (int)(c % gw);
            int cy = (int)(c / gw);
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    size_t idx = (size_t)ny * gw + (size_t)nx;
                    mapa[idx].agua = std::min(2.5f, mapa[idx].agua + 0.12f + frand(0.0f, 0.10f));
                    moisture[idx] = std::min(1.0f, moisture[idx] + 0.15f);
                }
            }
        }

        if (tick % 300 == 0) {
            // sequía parcial
            for (size_t i = 0; i < n; ++i) {
                if ((rand() % 100) < 25) {
                    mapa[i].agua = std::max(0.0f, mapa[i].agua - 0.05f);
                    moisture[i] = std::max(0.0f, moisture[i] - 0.03f);
                }
            }
        }

        for (size_t i = 0; i < n; ++i) {
            float crowd = crowd_base + occupancy[i] * 0.02f;

            float food_growth = fertility[i] * 0.020f + moisture[i] * 0.010f;
            float water_growth = moisture[i] * 0.018f + ((mapa[i].tipo & TAG_AGUA) ? 0.012f : 0.0f);
            float refuge_growth = ((mapa[i].tipo & TAG_CUEVA) ? 0.008f : 0.002f) + ((mapa[i].tipo & TAG_PIEDRA) ? 0.004f : 0.0f);
            float mineral_growth = ((mapa[i].tipo & TAG_MINERAL) ? 0.003f : 0.0005f);

            if (mapa[i].tipo & TAG_BOSQUE) food_growth += 0.010f;
            if (mapa[i].tipo & TAG_PANTANO) water_growth += 0.010f;
            if (mapa[i].tipo & TAG_DESIERTO) food_growth *= 0.55f;
            if (mapa[i].tipo & TAG_MONTANA) water_growth *= 0.75f;

            mapa[i].pasto += food_growth * (1.0f - std::min(0.92f, crowd));
            mapa[i].agua += water_growth * (1.0f - std::min(0.92f, crowd * 0.5f));
            mapa[i].refugio += refuge_growth;
            mapa[i].mineral += mineral_growth;

            mapa[i].pasto -= occupancy[i] * 0.0015f;
            mapa[i].agua -= occupancy[i] * 0.0010f;

            mapa[i].pasto = std::clamp(mapa[i].pasto, 0.0f, 3.0f);
            mapa[i].agua = std::clamp(mapa[i].agua, 0.0f, 3.0f);
            mapa[i].refugio = std::clamp(mapa[i].refugio, 0.0f, 2.0f);
            mapa[i].mineral = std::clamp(mapa[i].mineral, 0.0f, 2.0f);

            refresh_cell(i);
        }
    }

    std::vector<size_t> choose_spawn_centers(int groups) const {
        struct Cand { size_t idx; float score; };

        std::vector<Cand> cands;
        cands.reserve(mapa.size());

        for (size_t i = 0; i < mapa.size(); ++i) {
            float score =
                mapa[i].pasto * 2.0f +
                mapa[i].agua * 2.0f +
                mapa[i].refugio * 1.0f +
                fertility[i] * 1.2f +
                moisture[i] * 0.8f +
                mapa[i].mineral * 0.2f;

            if (mapa[i].tipo & TAG_PANTANO) score += 0.10f;
            if (mapa[i].tipo & TAG_BOSQUE) score += 0.20f;
            if (mapa[i].tipo & TAG_REFUGIO) score += 0.15f;
            if (mapa[i].tipo & TAG_DESIERTO) score -= 0.30f;
            if (mapa[i].tipo & TAG_MONTANA) score -= 0.08f;

            cands.push_back({i, score});
        }

        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
            return a.score > b.score;
        });

        std::vector<size_t> chosen;
        int min_sep = std::max(4, std::max(grid_cols(), grid_rows()) / 3);

        for (const auto &c : cands) {
            int cx = (int)(c.idx % grid_cols());
            int cy = (int)(c.idx / grid_cols());
            bool ok = true;

            for (size_t other : chosen) {
                int ox = (int)(other % grid_cols());
                int oy = (int)(other / grid_cols());
                int dist = std::abs(cx - ox) + std::abs(cy - oy);
                if (dist < min_sep) { ok = false; break; }
            }

            if (ok) {
                chosen.push_back(c.idx);
                if ((int)chosen.size() >= groups) break;
            }
        }

        if (chosen.empty() && !cands.empty()) chosen.push_back(cands.front().idx);
        return chosen;
    }

    void dibujar_estado(long long tick) const {
        int gw = grid_cols();
        int gh = grid_rows();
        std::vector<char> cells(mapa.size(), '.');
        std::vector<const char*> colors(mapa.size(), "\033[0m");

        static const char* tribe_colors[] = {
            "\033[97m", "\033[96m", "\033[92m", "\033[93m",
            "\033[91m", "\033[95m", "\033[94m"
        };

        for (size_t i = 0; i < mapa.size(); ++i) {
            const Celda& c = mapa[i];
            if (c.carne > 0.30f) { cells[i] = '%'; colors[i] = "\033[91m"; }
            else if (c.agua > 0.60f) { cells[i] = '~'; colors[i] = "\033[94m"; }
            else if (c.pasto > 0.70f) { cells[i] = '#'; colors[i] = "\033[92m"; }
            else if (c.refugio > 0.50f) { cells[i] = '^'; colors[i] = "\033[93m"; }
            else if (c.tipo & TAG_MONTANA) { cells[i] = 'M'; colors[i] = "\033[97m"; }
            else if (c.tipo & TAG_CUEVA) { cells[i] = 'U'; colors[i] = "\033[90m"; }
            else if (c.tipo & TAG_DESIERTO) { cells[i] = ':'; colors[i] = "\033[33m"; }
            else if (c.tipo & TAG_PANTANO) { cells[i] = ';'; colors[i] = "\033[96m"; }
            else if (c.tipo & TAG_BOSQUE) { cells[i] = 'T'; colors[i] = "\033[32m"; }
            else { cells[i] = '.'; colors[i] = "\033[37m"; }
        }

        for (const auto &a : gente) {
            if (!a.vivo) continue;
            size_t idx = coord_to_idx(a.x, a.y);
            cells[idx] = (char)('A' + (a.tribe_id % 26));
            colors[idx] = tribe_colors[a.tribe_id % 7];
        }

        std::ostringstream out;
        out << "\033[2J\033[H";
        out << "=== MUNDO | tick " << tick
            << " | pop " << count_alive()
            << " | temp " << (int)temperatura_global << "C ===\n";

        for (int y = 0; y < gh; ++y) {
            for (int x = 0; x < gw; ++x) {
                size_t idx = (size_t)y * gw + (size_t)x;
                out << colors[idx] << cells[idx];
            }
            out << "\033[0m\n";
        }
        out << "\033[0m";
        std::cout << out.str();
    }
};
