#include <iostream>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <ctime>
#include <csignal>
#include <atomic>
#include <numeric>
#include "mundo.h"
#include "tags.h"

struct Suceso {
    std::string msg;
    long long t;
};

static std::atomic<bool> running(true);

void on_sigint(int) {
    running = false;
}

static float norm_dist(int d, int maxd) {
    if (maxd <= 0) return 0.0f;
    return 1.0f - std::clamp((float)d / (float)maxd, 0.0f, 1.0f);
}

static int action_duration(Accion ac, const Agente& ag) {
    int base = 10;
    switch (ac) {
        case ACT_HUIR:         base = 12; break;
        case ACT_COMER:        base = 10; break;
        case ACT_BEBER:        base = 10; break;
        case ACT_SOCIALIZAR:   base = 14; break;
        case ACT_EXPLORAR:     base = 16; break;
        case ACT_ASENTARSE:    base = 18; break;
        case ACT_REPRODUCIR:   base = 14; break;
        case ACT_MIGRAR_FOOD:  base = 16; break;
        case ACT_MIGRAR_WATER: base = 16; break;
        case ACT_PELEAR:       base = 8;  break;
        default:               base = 10; break;
    }
    base += (int)(ag.disciplina * 8.0f);
    base += (int)(ag.inteligencia * 4.0f);
    return std::clamp(base, 6, 40);
}

int main() {
    std::srand((unsigned)std::time(nullptr));
    std::signal(SIGINT, on_sigint);

    Mundo m(10000, 10000, 200.0f);

    const int NUM_AGENTES = 1000;     // cambia esto si quieres
    const int GRUPOS = 4;
    const int SUMMARY_EVERY = 25;
    const long long TOTAL_TICKS = 100000000LL;

    auto spawns = m.choose_spawn_centers(GRUPOS);
    if (spawns.empty()) spawns.push_back(0);

    // poblamiento inicial en grupos separados
    for (int i = 0; i < NUM_AGENTES; ++i) {
        int group = i % (int)spawns.size();
        size_t center = spawns[group];

        int cx = (int)(center % m.grid_cols());
        int cy = (int)(center / m.grid_cols());

        float jitter = m.escala * 1.8f;
        float sx = cx * m.escala + m.escala * 0.5f + (((rand() % 2001) - 1000) / 1000.0f) * jitter;
        float sy = cy * m.escala + m.escala * 0.5f + (((rand() % 2001) - 1000) / 1000.0f) * jitter;

        m.gente.emplace_back(std::clamp(sx, 0.0f, (float)m.width - 0.001f),
                             std::clamp(sy, 0.0f, (float)m.height - 0.001f));
        auto &a = m.gente.back();
        a.tribe_id = group;
        a.genetica.nombre_linaje = "T" + std::to_string(group) + "-" + std::to_string(rand() % 100000);

        if (group == 0) a.genetica.construccion = std::min(1.0f, a.genetica.construccion + 0.20f);
        if (group == 1) a.genetica.migracion    = std::min(1.0f, a.genetica.migracion + 0.20f);
        if (group == 2) a.genetica.conflicto    = std::min(1.0f, a.genetica.conflicto + 0.20f);
        if (group == 3) a.genetica.tradicion    = std::min(1.0f, a.genetica.tradicion + 0.20f);

        if (rand() % 2) a.tags |= TAG_NONE;
    }

    std::vector<Agente*> neighbors;
    std::vector<Agente> newborns;
    std::vector<Suceso> cronicas;

    long long linaje_nacimientos = 0;

    for (long long tick = 0; running && tick < TOTAL_TICKS; ++tick) {
        int births_this_tick = 0;
        int deaths_this_tick = 0;
        int comiendo = 0;
        int bebiendo = 0;
        int huyendo = 0;
        int socializando = 0;
        int asentando = 0;
        int explorando = 0;
        int descansando = 0;
        int peleando = 0;
        int reproduciendo = 0;
        int vivos = 0;

        // limpieza de muertos viejos
        for (auto &a : m.gente) {
            if (!a.vivo) a.dead_ticks++;
        }
        for (auto it = m.gente.begin(); it != m.gente.end(); ) {
            if (!it->vivo && it->dead_ticks >= 20) it = m.gente.erase(it);
            else ++it;
        }

        // mundo
        m.actualizar();
        m.build_grid();
        m.update_world_resources(m.count_alive(), tick);
        m.compute_distance_map(m.food_distance, 0.45f, false);
        m.compute_distance_map(m.water_distance, 0.35f, true);

        newborns.clear();

        float suma_edad = 0.0f;
        float suma_energia = 0.0f;
        float suma_salud = 0.0f;
        float suma_hambre = 0.0f;
        float suma_sed = 0.0f;

        for (auto it = m.gente.begin(); it != m.gente.end(); ++it) {
            Agente &a = *it;
            if (!a.vivo) continue;

            vivos++;
            a.envejecer();

            // desgaste suave para que dure muchísimo más
            a.energia -= 0.045f + (1.0f - a.genetica.eficiencia_metabolica) * 0.010f;
            a.hambre  += 0.035f;
            a.sed     += 0.030f;
            a.hidratacion = std::max(0.0f, a.hidratacion - 0.02f);

            if (a.energia <= 0.0f || a.salud <= 0.0f || a.hidratacion <= 0.0f || a.edad > a.esperanza_vida) {
                size_t idx = m.coord_to_idx(a.x, a.y);
                m.mapa[idx].carne += 0.85f;
                m.mapa[idx].feromona_b += 0.12f;
                m.refresh_cell(idx);

                a.mark_dead();
                deaths_this_tick++;
                continue;
            }

            size_t idx = m.coord_to_idx(a.x, a.y);
            neighbors.clear();
            m.query_neighbors(a, neighbors);

            int allies = m.count_neighbors_same_tribe(neighbors, a.tribe_id);
            int enemies = m.count_neighbors_enemy(neighbors, a.tribe_id);
            int fire_neighbors = m.count_neighbors_with_tag(neighbors, TAG_FUEGO);

            bool celda_fuego = m.cell_has(idx, TAG_PELIGRO) || m.mapa[idx].feromona_b > 0.20f;
            bool celda_comida = m.mapa[idx].pasto > 0.10f;
            bool celda_agua = m.mapa[idx].agua > 0.10f;
            bool celda_carne = m.mapa[idx].carne > 0.05f;
            float shelter_here = m.mapa[idx].refugio;
            float food_here = m.mapa[idx].pasto;
            float water_here = m.mapa[idx].agua;
            float meat_here = m.mapa[idx].carne;

            // memoria social y de peligro con feromonas
            float food_pher = m.mapa[idx].feromona_a;
            float danger_pher = m.mapa[idx].feromona_b;

            // Brain inputs (14)
            std::vector<float> inputs = {
                a.energia / 120.0f,
                a.salud / 100.0f,
                a.hidratacion / 100.0f,
                a.hambre / 100.0f,
                a.sed / 100.0f,
                std::clamp(food_here / 1.5f, 0.0f, 1.0f),
                std::clamp(water_here / 1.5f, 0.0f, 1.0f),
                std::clamp(meat_here / 1.5f, 0.0f, 1.0f),
                std::clamp(shelter_here / 2.0f, 0.0f, 1.0f),
                std::clamp(m.temperatura_global / 40.0f, 0.0f, 1.0f),
                std::clamp((float)allies / 12.0f, 0.0f, 1.0f),
                std::clamp((float)enemies / 12.0f, 0.0f, 1.0f),
                norm_dist(m.food_distance[idx], m.grid_cols() + m.grid_rows()),
                norm_dist(m.water_distance[idx], m.grid_cols() + m.grid_rows())
            };

            std::vector<float> outputs;
            a.procesar_cerebro(inputs, outputs);

            auto N = [&](int i) -> float {
                return std::clamp((outputs[i] + 1.0f) * 0.5f, 0.0f, 1.0f);
            };

            float hambre_u = a.impulso_hambre();
            float sed_u = a.impulso_sed(celda_agua);
            float miedo_u = a.impulso_miedo(celda_fuego, fire_neighbors, tick, danger_pher);
            float social_u = a.impulso_social((float)allies, tick, food_pher);
            float curiosidad_u = a.impulso_curiosidad(std::max(hambre_u, sed_u), miedo_u);
            float descanso_u = a.impulso_descanso(std::max(hambre_u, sed_u), miedo_u);
            float asentarse_u = a.impulso_asentarse(shelter_here, social_u, miedo_u);
            float repro_u = a.impulso_reproducir(shelter_here, food_here, water_here, social_u);
            float pelea_u = a.impulso_pelear(enemies, miedo_u);

            // inercia
            bool emergencia = a.debe_interrumpir(celda_fuego, sed_u, hambre_u, miedo_u);
            if (a.ticks_en_accion <= 0 || emergencia) {
                Accion nueva = ACT_EXPLORAR;
                float mejor = curiosidad_u * (0.6f + 0.4f * std::max(N(0), N(1))) * a.genetica.curiosidad;

                float score_huir  = miedo_u   * (0.6f + 0.4f * N(0));
                float score_comer = hambre_u  * (0.6f + 0.4f * N(2));
                float score_beber = sed_u     * (0.6f + 0.4f * N(3));
                float score_repro = repro_u   * (0.6f + 0.4f * N(4)) * a.genetica.fertilidad;
                float score_build = asentarse_u * (0.6f + 0.4f * N(5)) * a.genetica.construccion;
                float score_social = social_u * (0.6f + 0.4f * N(6)) * (0.5f + a.apego) * a.genetica.tradicion;
                float score_fight = pelea_u * (0.6f + 0.4f * N(7)) * a.genetica.conflicto;
                float score_mig_food = hambre_u * (0.6f + 0.4f * N(8)) * a.genetica.migracion;
                float score_mig_water = sed_u * (0.6f + 0.4f * N(9)) * a.genetica.migracion;
                float score_rest = descanso_u * (0.6f + 0.4f * N(0));

                if (score_huir > mejor && !(enemies > 0 && score_fight > score_huir)) {
                    mejor = score_huir; nueva = ACT_HUIR;
                }
                if (score_beber > mejor) {
                    mejor = score_beber;
                    nueva = celda_agua ? ACT_BEBER : ACT_MIGRAR_WATER;
                }
                if (score_comer > mejor) {
                    mejor = score_comer;
                    nueva = (celda_comida || celda_carne) ? ACT_COMER : ACT_MIGRAR_FOOD;
                }
                if (score_fight > mejor) {
                    mejor = score_fight; nueva = ACT_PELEAR;
                }
                if (score_repro > mejor) {
                    mejor = score_repro; nueva = ACT_REPRODUCIR;
                }
                if (score_build > mejor) {
                    mejor = score_build; nueva = ACT_ASENTARSE;
                }
                if (score_social > mejor) {
                    mejor = score_social; nueva = ACT_SOCIALIZAR;
                }
                if (score_rest > mejor) {
                    mejor = score_rest; nueva = ACT_DESCANSAR;
                }
                if (score_mig_food > mejor) {
                    mejor = score_mig_food; nueva = ACT_MIGRAR_FOOD;
                }
                if (score_mig_water > mejor) {
                    mejor = score_mig_water; nueva = ACT_MIGRAR_WATER;
                }

                a.iniciar_accion(nueva, action_duration(nueva, a));
                a.objetivo_agente = -1;

                if (nueva == ACT_MIGRAR_FOOD) a.objetivo_celda = m.best_neighbor_by_gradient(a, m.food_distance);
                else if (nueva == ACT_MIGRAR_WATER) a.objetivo_celda = m.best_neighbor_by_gradient(a, m.water_distance);
                else if (nueva == ACT_PELEAR) a.objetivo_celda = m.best_neighbor_cell_with_tag(a, TAG_PELIGRO);
                else a.objetivo_celda = idx;
            }

            // acción persistente
            switch (a.accion_actual) {
                case ACT_HUIR: {
                    huyendo++;
                    size_t threat = m.best_neighbor_cell_with_tag(a, TAG_PELIGRO);
                    if (threat != idx) m.move_agent_away_from_cell(a, threat, a.genetica.velocidad_max * 18.0f);
                    else {
                        float mx = (outputs[0] * 2.0f - 1.0f);
                        float my = (outputs[1] * 2.0f - 1.0f);
                        a.x += mx * a.genetica.velocidad_max * 18.0f;
                        a.y += my * a.genetica.velocidad_max * 18.0f;
                    }
                    a.energia -= 0.22f;
                    a.hostilidad = std::clamp(a.hostilidad + 0.002f, 0.0f, 1.0f);
                    a.add_event(Evento::PELIGRO, tick, -1, 1.2f, 140);
                    m.mapa[idx].feromona_b += 0.06f;
                    break;
                }

                case ACT_COMER: {
                    comiendo++;
                    float got = m.take_food(idx, 0.12f);
                    if (got <= 0.0f && celda_carne) got = m.take_meat(idx, 0.12f);
                    if (got > 0.0f) {
                        a.energia = std::min(120.0f, a.energia + got * 90.0f);
                        a.hambre = std::max(0.0f, a.hambre - got * 95.0f);
                        a.apego = std::clamp(a.apego + 0.001f, 0.0f, 1.0f);
                        a.add_event(Evento::COMIDA, tick, -1, 0.9f, 120);
                        m.mapa[idx].feromona_a += 0.08f;
                    } else {
                        size_t target = a.objetivo_celda;
                        if (target >= m.mapa.size() || m.food_distance[target] >= 999999) {
                            target = m.best_neighbor_by_gradient(a, m.food_distance);
                            a.objetivo_celda = target;
                        }
                        if (target != idx) m.move_agent_toward_cell(a, target, a.genetica.velocidad_max * 15.0f);
                    }
                    a.energia = std::max(0.0f, a.energia - 0.05f);
                    break;
                }

                case ACT_BEBER: {
                    bebiendo++;
                    float got = m.take_water(idx, 0.12f);
                    if (got > 0.0f) {
                        a.hidratacion = std::min(120.0f, a.hidratacion + got * 100.0f);
                        a.sed = std::max(0.0f, a.sed - got * 100.0f);
                        a.energia = std::min(120.0f, a.energia + got * 12.0f);
                        a.add_event(Evento::BEBIDA, tick, -1, 0.8f, 120);
                        m.mapa[idx].feromona_a += 0.04f;
                    } else {
                        size_t target = a.objetivo_celda;
                        if (target >= m.mapa.size() || m.water_distance[target] >= 999999) {
                            target = m.best_neighbor_by_gradient(a, m.water_distance);
                            a.objetivo_celda = target;
                        }
                        if (target != idx) m.move_agent_toward_cell(a, target, a.genetica.velocidad_max * 15.0f);
                    }
                    a.energia = std::max(0.0f, a.energia - 0.04f);
                    break;
                }

                case ACT_SOCIALIZAR: {
                    socializando++;
                    if (!neighbors.empty()) {
                        Agente* best = nullptr;
                        for (auto* p : neighbors) {
                            if (!p || !p->vivo) continue;
                            if (p->tribe_id == a.tribe_id) {
                                if (!best || p->apego > best->apego) best = p;
                            }
                        }
                        if (!best) best = neighbors[rand() % neighbors.size()];
                        if (best && best->vivo) {
                            a.energia = std::min(120.0f, a.energia + 0.25f);
                            a.apego = std::clamp(a.apego + 0.003f, 0.0f, 1.0f);
                            a.hostilidad = std::clamp(a.hostilidad - 0.001f, 0.0f, 1.0f);
                            a.add_event(Evento::SOCIAL, tick, (int)best->uid, 0.7f, 180);

                            if (best->tribe_id == a.tribe_id) {
                                a.aprender_observando(*best, tick, 0.18f);
                                best->aprender_observando(a, tick, 0.10f);
                            } else {
                                a.hostilidad = std::clamp(a.hostilidad + 0.002f, 0.0f, 1.0f);
                            }
                        }
                    } else {
                        a.x += (rand() % 3 - 1) * a.genetica.velocidad_max * 7.0f;
                        a.y += (rand() % 3 - 1) * a.genetica.velocidad_max * 7.0f;
                    }
                    break;
                }

                case ACT_DESCANSAR: {
                    descansando++;
                    a.energia = std::min(120.0f, a.energia + 1.8f + shelter_here * 1.2f);
                    a.salud = std::min(120.0f, a.salud + shelter_here * 0.15f);
                    a.hambre = std::min(100.0f, a.hambre + 0.02f);
                    a.sed = std::min(100.0f, a.sed + 0.02f);
                    break;
                }

                case ACT_EXPLORAR: {
                    explorando++;
                    a.x += (outputs[0]) * a.genetica.velocidad_max * 10.0f;
                    a.y += (outputs[1]) * a.genetica.velocidad_max * 10.0f;
                    a.inteligencia = std::min(1.0f, a.inteligencia + 0.0002f);
                    a.genetica.curiosidad = std::min(1.0f, a.genetica.curiosidad + 0.0005f);
                    break;
                }

                case ACT_ASENTARSE: {
                    asentando++;
                    if (m.try_build_shelter(idx)) {
                        a.energia = std::max(0.0f, a.energia - 0.10f);
                        a.add_event(Evento::ASENTARSE, tick, -1, 1.0f, 240);
                    } else {
                        a.x += (rand() % 3 - 1) * a.genetica.velocidad_max * 5.0f;
                        a.y += (rand() % 3 - 1) * a.genetica.velocidad_max * 5.0f;
                    }
                    break;
                }

                case ACT_REPRODUCIR: {
                    if (a.energia > 55.0f && a.salud > 45.0f && (food_here > 0.12f || meat_here > 0.12f) && water_here > 0.12f) {
                        const Agente* mate = nullptr;
                        for (auto* p : neighbors) {
                            if (!p || !p->vivo) continue;
                            if (p->tribe_id == a.tribe_id && p->energia > 40.0f && p->salud > 35.0f) {
                                if (!mate || p->apego > mate->apego) mate = p;
                            }
                        }

                        if (!mate && a.energia > 75.0f && (rand() % 100) < 5) {
                            newborns.push_back(a.heredar(a));
                        } else if (mate) {
                            newborns.push_back(a.heredar(*mate));
                        }

                        if (!newborns.empty()) {
                            reproduciendo++;
                            births_this_tick++;
                            linaje_nacimientos++;
                            a.energia = std::max(0.0f, a.energia - 12.0f);
                            a.hambre = std::min(100.0f, a.hambre + 3.0f);
                            a.sed = std::min(100.0f, a.sed + 2.0f);
                            a.add_event(Evento::NACIMIENTO, tick, -1, 1.4f, 260);
                        }
                    } else {
                        a.x += (rand() % 3 - 1) * a.genetica.velocidad_max * 4.0f;
                        a.y += (rand() % 3 - 1) * a.genetica.velocidad_max * 4.0f;
                    }
                    break;
                }

                case ACT_MIGRAR_FOOD: {
                    size_t target = a.objetivo_celda;
                    if (target >= m.mapa.size() || m.food_distance[target] >= 999999) {
                        target = m.best_neighbor_by_gradient(a, m.food_distance);
                        a.objetivo_celda = target;
                    }
                    if (target != idx) m.move_agent_toward_cell(a, target, a.genetica.velocidad_max * 11.0f);
                    else a.x += (rand() % 3 - 1) * a.genetica.velocidad_max * 5.0f;
                    explorando++;
                    a.energia = std::max(0.0f, a.energia - 0.05f);
                    break;
                }

                case ACT_MIGRAR_WATER: {
                    size_t target = a.objetivo_celda;
                    if (target >= m.mapa.size() || m.water_distance[target] >= 999999) {
                        target = m.best_neighbor_by_gradient(a, m.water_distance);
                        a.objetivo_celda = target;
                    }
                    if (target != idx) m.move_agent_toward_cell(a, target, a.genetica.velocidad_max * 11.0f);
                    else a.x += (rand() % 3 - 1) * a.genetica.velocidad_max * 5.0f;
                    explorando++;
                    a.energia = std::max(0.0f, a.energia - 0.05f);
                    break;
                }

                case ACT_PELEAR: {
                    peleando++;
                    Agente* victim = nullptr;
                    for (auto* p : neighbors) {
                        if (!p || !p->vivo) continue;
                        if (p->tribe_id != a.tribe_id) {
                            victim = p;
                            break;
                        }
                    }
                    if (victim) {
                        float dmg = a.genetica.fuerza_ataque * 0.08f 
                        + a.genetica.agresividad * 1.8f 
                        + a.hostilidad * 1.0f;
                        victim->salud -= dmg;
                        a.energia -= 1.2f;
                        a.hostilidad = std::clamp(a.hostilidad + 0.004f, 0.0f, 1.0f);
                        a.add_event(Evento::PELEA, tick, (int)victim->uid, 1.3f, 200);
                        victim->add_event(Evento::PELIGRO, tick, (int)a.uid, 1.2f, 180);
                        m.mapa[idx].feromona_b += 0.10f;

                        if (victim->salud <= 0.0f) {
                            victim->mark_dead();
                            size_t cidx = m.coord_to_idx(victim->x, victim->y);
                            m.mapa[cidx].carne += 1.1f;
                            m.mapa[cidx].feromona_b += 0.15f;
                            m.refresh_cell(cidx);
                            deaths_this_tick++;
                        }
                    } else {
                        a.x += (rand() % 3 - 1) * a.genetica.velocidad_max * 8.0f;
                        a.y += (rand() % 3 - 1) * a.genetica.velocidad_max * 8.0f;
                    }
                    break;
                }
            }

            a.x = std::clamp(a.x, 0.0f, (float)m.width - 0.001f);
            a.y = std::clamp(a.y, 0.0f, (float)m.height - 0.001f);

            size_t new_idx = m.coord_to_idx(a.x, a.y);
            m.refresh_cell(new_idx);

            if (a.tags & TAG_FUEGO) {
                a.add_event(Evento::FUEGO, tick, -1, 1.3f, 200);
                a.salud -= 1.2f;
                m.mapa[new_idx].feromona_b += 0.08f;
            }

            if (celda_fuego || enemies > 0) {
                a.add_event(Evento::PELIGRO, tick, -1, 1.0f, 160);
            }

            if (a.salud <= 0.0f || a.energia <= 0.0f || a.hidratacion <= 0.0f) {
                size_t cidx = m.coord_to_idx(a.x, a.y);
                m.mapa[cidx].carne += 0.9f;
                m.mapa[cidx].feromona_b += 0.10f;
                m.refresh_cell(cidx);
                a.mark_dead();
                deaths_this_tick++;
            }

            if (a.ticks_en_accion > 0) a.ticks_en_accion--;

            suma_edad += (float)a.edad;
            suma_energia += a.energia;
            suma_salud += a.salud;
            suma_hambre += a.hambre;
            suma_sed += a.sed;
        }

        for (auto &b : newborns) {
            m.gente.push_back(std::move(b));
        }

        // anti-extinción
        if (m.count_alive() < 40) {
            auto centers = m.choose_spawn_centers(3);
            if (centers.empty()) centers.push_back(0);

            for (int i = 0; i < 30; ++i) {
                size_t c = centers[i % centers.size()];
                int cx = (int)(c % m.grid_cols());
                int cy = (int)(c / m.grid_cols());
                float jitter = m.escala * 1.6f;
                float sx = cx * m.escala + m.escala * 0.5f + (((rand() % 2001) - 1000) / 1000.0f) * jitter;
                float sy = cy * m.escala + m.escala * 0.5f + (((rand() % 2001) - 1000) / 1000.0f) * jitter;
                m.gente.emplace_back(std::clamp(sx, 0.0f, (float)m.width - 0.001f),
                                     std::clamp(sy, 0.0f, (float)m.height - 0.001f));
                auto &a = m.gente.back();
                a.tribe_id = (int)(i % 3);
                cronicas.push_back({"Llegaron pioneros para sostener la historia.", tick});
            }
        }

        // cronicas
        if (births_this_tick > 0) {
            cronicas.push_back({"Nacen nuevos linajes en el mapa.", tick});
        }
        if (deaths_this_tick > 0) {
            cronicas.push_back({"La muerte deja carne, miedo y una enseñanza.", tick});
        }
        if (tick % 250 == 0 && m.count_alive() > 0) {
            cronicas.push_back({"Las tribus siguen moviéndose, construyendo y peleando.", tick});
        }
        if (m.count_alive() == 0) {
            cronicas.push_back({"La civilización colapsó. Nuevos pioneros tendrán que levantarla.", tick});
        }

        // resumen
        if (tick % SUMMARY_EVERY == 0) {
            int alive = m.count_alive();
            float avg_age = alive ? suma_edad / alive : 0.0f;
            float avg_energy = alive ? suma_energia / alive : 0.0f;
            float avg_health = alive ? suma_salud / alive : 0.0f;
            float avg_hunger = alive ? suma_hambre / alive : 0.0f;
            float avg_thirst = alive ? suma_sed / alive : 0.0f;

            std::cout << "\n--- Tick " << tick << " ---\n";
            std::cout << "Vivos: " << alive
                      << " | Muertos pendientes: " << m.count_dead_pending()
                      << " | Nacimientos: " << births_this_tick
                      << " | Muertes: " << deaths_this_tick << "\n";
            std::cout << "Edad media: " << (int)avg_age
                      << " | Energia media: " << (int)avg_energy
                      << " | Salud media: " << (int)avg_health
                      << " | Hambre media: " << (int)avg_hunger
                      << " | Sed media: " << (int)avg_thirst << "\n";

            std::cout << "Comiendo: " << comiendo
                      << " | Bebiendo: " << bebiendo
                      << " | Huyendo: " << huyendo
                      << " | Socializando: " << socializando
                      << " | Asentando: " << asentando
                      << " | Explorando: " << explorando
                      << " | Descansando: " << descansando
                      << " | Peleando: " << peleando << "\n";

            m.dibujar_estado(tick);

            std::cout << "\n--- Cronicas ---\n";
            int shown = 0;
            for (auto it = cronicas.rbegin(); it != cronicas.rend() && shown < 4; ++it, ++shown) {
                std::cout << "[" << it->t << "] " << it->msg << "\n";
            }
        }

        if (!running) break;
    }

    std::cout << "\nSimulación detenida.\n";
    return 0;
}