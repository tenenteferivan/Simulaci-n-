#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <utility>
#include "tags.h"
#include "agente.h"

struct Mundo {
    float width = 0.0f;
    float height = 0.0f;
    float cell_size = 1000.0f;
    size_t grid_cols = 0;
    size_t grid_rows = 0;

    std::vector<int> cell_head;
    std::vector<int> agent_next_in_cell;
    std::vector<Agente> agentes;
    std::vector<uint32_t> cell_tags;

    std::vector<float> altitude;
    std::vector<float> moisture;
    std::vector<float> temperature;
    std::vector<float> roughness;
    std::vector<float> fertility;

    std::vector<float> food_amount;
    std::vector<float> water_amount;
    std::vector<float> wood_amount;
    std::vector<float> stone_amount;
    std::vector<float> mineral_amount;
    std::vector<float> shelter_amount;

    std::vector<int> food_distance;
    std::vector<int> water_distance;

    void init_grid(float w, float h, float csize) {
        width = w;
        height = h;
        cell_size = csize;
        grid_cols = std::max((size_t)1, (size_t)std::ceil(width / cell_size));
        grid_rows = std::max((size_t)1, (size_t)std::ceil(height / cell_size));

        size_t n = grid_cols * grid_rows;
        cell_head.assign(n, -1);
        agent_next_in_cell.assign(agentes.size(), -1);
        cell_tags.assign(n, 0);

        altitude.assign(n, 0.0f);
        moisture.assign(n, 0.0f);
        temperature.assign(n, 0.0f);
        roughness.assign(n, 0.0f);
        fertility.assign(n, 0.0f);

        food_amount.assign(n, 0.0f);
        water_amount.assign(n, 0.0f);
        wood_amount.assign(n, 0.0f);
        stone_amount.assign(n, 0.0f);
        mineral_amount.assign(n, 0.0f);
        shelter_amount.assign(n, 0.0f);

        food_distance.assign(n, 999999);
        water_distance.assign(n, 999999);

        seed_world();
    }

    void seed_world() {
        size_t n = grid_cols * grid_rows;
        if (n == 0) return;

        auto r01 = []() -> float {
            return (rand() % 1000) / 1000.0f;
        };

        for (size_t i = 0; i < n; ++i) {
            altitude[i] = r01();
            moisture[i] = r01();
            temperature[i] = r01();
            roughness[i] = r01();
            fertility[i] = r01();
        }

        smooth_pass(altitude, 2);
        smooth_pass(moisture, 2);
        smooth_pass(temperature, 1);
        smooth_pass(roughness, 1);
        smooth_pass(fertility, 1);

        for (size_t i = 0; i < n; ++i) {
            float alt = altitude[i];
            float moi = moisture[i];
            float temp = temperature[i];
            float rou = roughness[i];
            float fer = fertility[i];

            water_amount[i] = std::clamp(0.05f + moi * 0.60f + (1.0f - alt) * 0.20f, 0.0f, 1.0f);
            food_amount[i] = std::clamp(0.10f + fer * 0.55f + moi * 0.18f - alt * 0.08f, 0.0f, 1.0f);
            wood_amount[i] = std::clamp((moi * 0.55f + fer * 0.45f) * (alt < 0.78f ? 1.0f : 0.25f), 0.0f, 1.0f);
            stone_amount[i] = std::clamp(alt * 0.70f + rou * 0.35f, 0.0f, 1.0f);
            mineral_amount[i] = std::clamp((alt * 0.45f + rou * 0.55f) * (alt > 0.55f ? 1.0f : 0.25f), 0.0f, 1.0f);
            shelter_amount[i] = std::clamp((stone_amount[i] * 0.35f + wood_amount[i] * 0.20f) * r01(), 0.0f, 1.0f);

            refresh_resource_tags(i);
        }
    }

    void smooth_pass(std::vector<float> &field, int passes) {
        if (field.empty()) return;
        std::vector<float> tmp(field.size(), 0.0f);

        for (int p = 0; p < passes; ++p) {
            for (size_t y = 0; y < grid_rows; ++y) {
                for (size_t x = 0; x < grid_cols; ++x) {
                    float sum = 0.0f;
                    int count = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = (int)x + dx;
                            int ny = (int)y + dy;
                            if (nx < 0 || ny < 0 || nx >= (int)grid_cols || ny >= (int)grid_rows) continue;
                            size_t idx = (size_t)ny * grid_cols + (size_t)nx;
                            sum += field[idx];
                            count++;
                        }
                    }
                    tmp[y * grid_cols + x] = sum / std::max(1, count);
                }
            }
            field.swap(tmp);
        }
    }

    size_t cell_index(float x, float y) const {
        if (grid_cols == 0 || grid_rows == 0) return 0;
        size_t cx = std::min(grid_cols - 1, (size_t)(x / cell_size));
        size_t cy = std::min(grid_rows - 1, (size_t)(y / cell_size));
        return cy * grid_cols + cx;
    }

    void refresh_resource_tags(size_t idx) {
        if (idx >= cell_tags.size()) return;

        uint32_t keep = cell_tags[idx] & (TAG_EN_FUEGO | TAG_ACIDO);
        cell_tags[idx] = keep;

        float alt = altitude[idx];
        float moi = moisture[idx];
        float temp = temperature[idx];
        float rou = roughness[idx];

        if (alt > 0.78f) {
            cell_tags[idx] |= TAG_MONTANA | TAG_PIEDRA;
            if (rou > 0.55f) cell_tags[idx] |= TAG_CUEVA;
            if (mineral_amount[idx] > 0.30f) cell_tags[idx] |= TAG_MINERAL;
        } else if (alt > 0.62f) {
            cell_tags[idx] |= TAG_COLINA | TAG_PIEDRA;
            if (stone_amount[idx] > 0.25f) cell_tags[idx] |= TAG_PIEDRA;
        } else if (water_amount[idx] > 0.70f) {
            cell_tags[idx] |= TAG_AGUA | TAG_LAGO | TAG_MOJADO;
        } else if (moi > 0.72f && alt < 0.40f) {
            cell_tags[idx] |= TAG_PANTANO | TAG_MOJADO;
        } else if (moi < 0.20f && temp > 0.55f) {
            cell_tags[idx] |= TAG_DESIERTO;
        } else {
            cell_tags[idx] |= TAG_PRADERA;
        }

        if (wood_amount[idx] > 0.35f) {
            cell_tags[idx] |= TAG_BOSQUE | TAG_INFLAMABLE;
        }

        if (food_amount[idx] > 0.35f) {
            cell_tags[idx] |= TAG_COMIDA;
        }

        if (water_amount[idx] > 0.25f) {
            cell_tags[idx] |= TAG_MOJADO;
        }

        if (shelter_amount[idx] > 0.35f) {
            cell_tags[idx] |= TAG_REFUGIO;
        }

        if (wood_amount[idx] > 0.55f || (cell_tags[idx] & TAG_BOSQUE)) {
            cell_tags[idx] |= TAG_INFLAMABLE;
        }
    }

    void refresh_all_resource_tags() {
        for (size_t i = 0; i < cell_tags.size(); ++i) refresh_resource_tags(i);
    }

    void build_grid() {
        if (agent_next_in_cell.size() != agentes.size()) {
            agent_next_in_cell.assign(agentes.size(), -1);
        }

        std::fill(cell_head.begin(), cell_head.end(), -1);
        for (size_t i = 0; i < agentes.size(); ++i) {
            if (!agentes[i].alive) continue;
            size_t idx = cell_index(agentes[i].x, agentes[i].y);
            agent_next_in_cell[i] = cell_head[idx];
            cell_head[idx] = (int)i;
        }
    }

    int count_alive() const {
        int c = 0;
        for (const auto &a : agentes) if (a.alive) c++;
        return c;
    }

    int count_dead_pending() const {
        int c = 0;
        for (const auto &a : agentes) if (!a.alive && a.dead_ticks < 20) c++;
        return c;
    }

    void query_neighbors(int ai, std::vector<int> &out) const {
        out.clear();
        if (ai < 0 || ai >= (int)agentes.size()) return;
        if (!agentes[ai].alive) return;

        const Agente &a = agentes[ai];
        size_t cx = std::min(grid_cols - 1, (size_t)(a.x / cell_size));
        size_t cy = std::min(grid_rows - 1, (size_t)(a.y / cell_size));

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int ncx = (int)cx + dx;
                int ncy = (int)cy + dy;
                if (ncx < 0 || ncy < 0 || ncx >= (int)grid_cols || ncy >= (int)grid_rows) continue;

                int head = cell_head[ncy * grid_cols + ncx];
                for (int idx = head; idx != -1; idx = agent_next_in_cell[idx]) {
                    if (idx != ai) out.push_back(idx);
                }
            }
        }
    }

    inline void resolver_quimica(Agente &a, uint32_t cell) {
        if ((a.tags & TAG_INFLAMABLE) && (cell & TAG_EN_FUEGO)) {
            a.tags |= TAG_EN_FUEGO;
            a.salud -= 5.0f;
        }

        if ((a.tags & TAG_EN_FUEGO) && (cell & TAG_MOJADO)) {
            a.tags &= ~TAG_EN_FUEGO;
        }

        if (cell & TAG_ACIDO) {
            a.salud -= 2.0f;
        }

        if (a.salud < 0.0f) a.salud = 0.0f;
    }

    inline bool cell_has(size_t idx, uint32_t tag) const {
        return idx < cell_tags.size() && (cell_tags[idx] & tag) != 0;
    }

    inline float take_food(size_t idx, float amount) {
        if (idx >= food_amount.size()) return 0.0f;
        float got = std::min(amount, food_amount[idx]);
        food_amount[idx] -= got;
        refresh_resource_tags(idx);
        return got;
    }

    inline float take_water(size_t idx, float amount) {
        if (idx >= water_amount.size()) return 0.0f;
        float got = std::min(amount, water_amount[idx]);
        water_amount[idx] -= got;
        refresh_resource_tags(idx);
        return got;
    }

    inline float take_wood(size_t idx, float amount) {
        if (idx >= wood_amount.size()) return 0.0f;
        float got = std::min(amount, wood_amount[idx]);
        wood_amount[idx] -= got;
        refresh_resource_tags(idx);
        return got;
    }

    inline float take_stone(size_t idx, float amount) {
        if (idx >= stone_amount.size()) return 0.0f;
        float got = std::min(amount, stone_amount[idx]);
        stone_amount[idx] -= got;
        refresh_resource_tags(idx);
        return got;
    }

    inline bool try_build_shelter(size_t idx) {
        if (idx >= shelter_amount.size()) return false;

        float wood = take_wood(idx, 0.08f);
        float stone = take_stone(idx, 0.04f);
        float gain = wood * 0.8f + stone * 0.5f;

        if (gain <= 0.02f) return false;

        shelter_amount[idx] = std::clamp(shelter_amount[idx] + gain, 0.0f, 1.5f);
        refresh_resource_tags(idx);
        return true;
    }

    inline int count_neighbors_with_tag(const std::vector<int> &neighbors, uint32_t tag) const {
        int count = 0;
        for (int idx : neighbors) {
            if (idx >= 0 && idx < (int)agentes.size() && agentes[idx].alive && (agentes[idx].tags & tag)) {
                count++;
            }
        }
        return count;
    }

    inline size_t best_neighbor_cell_with_tag(int ai, uint32_t wanted) const {
        if (ai < 0 || ai >= (int)agentes.size()) return 0;
        if (!agentes[ai].alive) return 0;

        const Agente &a = agentes[ai];
        size_t cur = cell_index(a.x, a.y);
        size_t cx = cur % grid_cols;
        size_t cy = cur / grid_cols;

        size_t best = cur;
        int best_dist = 999999;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = (int)cx + dx;
                int ny = (int)cy + dy;
                if (nx < 0 || ny < 0 || nx >= (int)grid_cols || ny >= (int)grid_rows) continue;

                size_t ni = (size_t)ny * grid_cols + (size_t)nx;
                if (!(cell_tags[ni] & wanted)) continue;

                int dist = std::abs(dx) + std::abs(dy);
                if (dist < best_dist) {
                    best_dist = dist;
                    best = ni;
                }
            }
        }

        return best;
    }

    inline size_t best_neighbor_by_gradient(int ai, const std::vector<int> &dist_map) const {
        if (ai < 0 || ai >= (int)agentes.size()) return 0;
        if (!agentes[ai].alive) return 0;

        const Agente &a = agentes[ai];
        size_t cur = cell_index(a.x, a.y);
        size_t cx = cur % grid_cols;
        size_t cy = cur / grid_cols;

        size_t best = cur;
        int best_val = dist_map[cur];

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = (int)cx + dx;
                int ny = (int)cy + dy;
                if (nx < 0 || ny < 0 || nx >= (int)grid_cols || ny >= (int)grid_rows) continue;

                size_t ni = (size_t)ny * grid_cols + (size_t)nx;
                if (dist_map[ni] < best_val) {
                    best_val = dist_map[ni];
                    best = ni;
                }
            }
        }

        return best;
    }

    inline void move_agent_toward_cell(Agente &a, size_t target_idx, float step = 200.0f) const {
        if (grid_cols == 0 || grid_rows == 0) return;

        size_t cur = cell_index(a.x, a.y);
        int cx = (int)(cur % grid_cols);
        int cy = (int)(cur / grid_cols);
        int tx = (int)(target_idx % grid_cols);
        int ty = (int)(target_idx / grid_cols);

        if (tx > cx) a.x += step;
        if (tx < cx) a.x -= step;
        if (ty > cy) a.y += step;
        if (ty < cy) a.y -= step;

        a.x = std::clamp(a.x, 0.0f, width);
        a.y = std::clamp(a.y, 0.0f, height);
    }

    inline void move_agent_away_from_cell(Agente &a, size_t threat_idx, float step = 200.0f) const {
        if (grid_cols == 0 || grid_rows == 0) return;

        size_t cur = cell_index(a.x, a.y);
        int cx = (int)(cur % grid_cols);
        int cy = (int)(cur / grid_cols);
        int tx = (int)(threat_idx % grid_cols);
        int ty = (int)(threat_idx / grid_cols);

        if (tx > cx) a.x -= step;
        if (tx < cx) a.x += step;
        if (ty > cy) a.y -= step;
        if (ty < cy) a.y += step;

        a.x = std::clamp(a.x, 0.0f, width);
        a.y = std::clamp(a.y, 0.0f, height);
    }

    void update_world_resources(int living_population, int tick) {
        if (cell_tags.empty()) return;

        size_t n = cell_tags.size();
        std::vector<int> occupancy(n, 0);

        for (size_t i = 0; i < n; ++i) {
            for (int idx = cell_head[i]; idx != -1; idx = agent_next_in_cell[idx]) {
                occupancy[i]++;
            }
        }

        if (tick % 41 == 0) {
            for (int p = 0; p < 4; ++p) {
                size_t c = (size_t)(rand() % (int)n);
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int x = (int)(c % grid_cols) + dx;
                        int y = (int)(c / grid_cols) + dy;
                        if (x < 0 || y < 0 || x >= (int)grid_cols || y >= (int)grid_rows) continue;
                        size_t idx = (size_t)y * grid_cols + (size_t)x;
                        water_amount[idx] = std::min(1.0f, water_amount[idx] + 0.18f + (rand() % 200) / 1000.0f);
                        moisture[idx] = std::min(1.0f, moisture[idx] + 0.04f);
                    }
                }
            }
        }

        if (tick % 67 == 0) {
            for (int p = 0; p < 2; ++p) {
                size_t c = (size_t)(rand() % (int)n);
                food_amount[c] = std::max(0.0f, food_amount[c] - 0.25f);
            }
        }

        if (tick % 113 == 0) {
            for (size_t i = 0; i < n; ++i) {
                water_amount[i] = std::max(0.0f, water_amount[i] - 0.02f);
                moisture[i] = std::max(0.0f, moisture[i] - 0.01f);
            }
        }

        if (tick % 131 == 0) {
            size_t c = (size_t)(rand() % (int)n);
            cell_tags[c] |= TAG_ACIDO;
        }

        if (tick % 17 == 0) {
            std::vector<uint32_t> next_tags = cell_tags;

            for (size_t i = 0; i < n; ++i) {
                if (cell_tags[i] & TAG_EN_FUEGO) {
                    if (water_amount[i] > 0.42f && (rand() % 100) < 45) {
                        next_tags[i] &= ~TAG_EN_FUEGO;
                    }

                    int cx = (int)(i % grid_cols);
                    int cy = (int)(i / grid_cols);
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = cx + dx;
                            int ny = cy + dy;
                            if (nx < 0 || ny < 0 || nx >= (int)grid_cols || ny >= (int)grid_rows) continue;
                            size_t ni = (size_t)ny * grid_cols + (size_t)nx;

                            bool flammable = (cell_tags[ni] & TAG_INFLAMABLE) != 0;
                            bool dry = water_amount[ni] < 0.25f;
                            if (flammable && dry && (rand() % 100) < 10) {
                                next_tags[ni] |= TAG_EN_FUEGO;
                            }
                        }
                    }
                }
            }

            cell_tags.swap(next_tags);
        }

        float crowd_base = std::min(0.80f, living_population * 0.0018f);

        for (size_t i = 0; i < n; ++i) {
            float crowd = crowd_base + occupancy[i] * 0.04f;
            float food_growth = fertility[i] * 0.018f + moisture[i] * 0.008f + ((cell_tags[i] & TAG_BOSQUE) ? 0.008f : 0.0f);
            float water_growth = moisture[i] * 0.012f + ((cell_tags[i] & TAG_AGUA) ? 0.010f : 0.0f);
            float wood_growth = ((cell_tags[i] & TAG_BOSQUE) ? 0.012f : 0.003f);
            float stone_growth = ((cell_tags[i] & TAG_PIEDRA) ? 0.004f : 0.001f);
            float mineral_growth = ((cell_tags[i] & TAG_MINERAL) ? 0.003f : 0.0005f);

            food_amount[i] += food_growth * (1.0f - std::min(0.90f, crowd));
            water_amount[i] += water_growth * (1.0f - std::min(0.90f, crowd * 0.5f));
            wood_amount[i] += wood_growth * (1.0f - std::min(0.80f, crowd));
            stone_amount[i] += stone_growth;
            mineral_amount[i] += mineral_growth;

            food_amount[i] -= occupancy[i] * 0.003f;
            water_amount[i] -= occupancy[i] * 0.002f;

            food_amount[i] = std::clamp(food_amount[i], 0.0f, 1.25f);
            water_amount[i] = std::clamp(water_amount[i], 0.0f, 1.15f);
            wood_amount[i] = std::clamp(wood_amount[i], 0.0f, 1.20f);
            stone_amount[i] = std::clamp(stone_amount[i], 0.0f, 1.20f);
            mineral_amount[i] = std::clamp(mineral_amount[i], 0.0f, 1.20f);
            shelter_amount[i] = std::clamp(shelter_amount[i] * 0.9995f, 0.0f, 1.50f);

            refresh_resource_tags(i);
        }
    }

    void compute_distance_map(std::vector<int> &dist, const std::vector<float> &amount, float threshold) const {
        const int INF = 999999;
        size_t n = amount.size();
        dist.assign(n, INF);

        std::deque<int> q;
        for (size_t i = 0; i < n; ++i) {
            if (amount[i] >= threshold) {
                dist[i] = 0;
                q.push_back((int)i);
            }
        }

        while (!q.empty()) {
            int i = q.front();
            q.pop_front();

            int cx = i % (int)grid_cols;
            int cy = i / (int)grid_cols;

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = cx + dx;
                    int ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= (int)grid_cols || ny >= (int)grid_rows) continue;

                    int ni = ny * (int)grid_cols + nx;
                    if (dist[ni] > dist[i] + 1) {
                        dist[ni] = dist[i] + 1;
                        q.push_back(ni);
                    }
                }
            }
        }
    }

    std::vector<size_t> choose_spawn_centers(int groups) const {
        struct Cand {
            size_t idx;
            float score;
        };

        std::vector<Cand> cands;
        cands.reserve(cell_tags.size());

        for (size_t i = 0; i < cell_tags.size(); ++i) {
            float score =
                food_amount[i] * 2.0f +
                water_amount[i] * 2.0f +
                fertility[i] * 1.2f +
                shelter_amount[i] * 0.8f +
                wood_amount[i] * 0.5f +
                stone_amount[i] * 0.25f;

            if (cell_tags[i] & TAG_PRADERA) score += 0.20f;
            if (cell_tags[i] & TAG_BOSQUE) score += 0.15f;
            if (cell_tags[i] & TAG_REFUGIO) score += 0.10f;
            if (cell_tags[i] & TAG_MONTANA) score -= 0.10f;
            if (cell_tags[i] & TAG_DESIERTO) score -= 0.25f;

            cands.push_back({i, score});
        }

        std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
            return a.score > b.score;
        });

        std::vector<size_t> chosen;
        int min_sep = (int)std::max(grid_cols, grid_rows) / 3;
        if (min_sep < 4) min_sep = 4;

        for (const auto &c : cands) {
            int cx = (int)(c.idx % grid_cols);
            int cy = (int)(c.idx / grid_cols);
            bool ok = true;

            for (size_t other : chosen) {
                int ox = (int)(other % grid_cols);
                int oy = (int)(other / grid_cols);
                int dist = std::abs(cx - ox) + std::abs(cy - oy);
                if (dist < min_sep) {
                    ok = false;
                    break;
                }
            }

            if (ok) {
                chosen.push_back(c.idx);
                if ((int)chosen.size() >= groups) break;
            }
        }

        if (chosen.empty() && !cands.empty()) {
            chosen.push_back(cands.front().idx);
        }

        return chosen;
    }
};