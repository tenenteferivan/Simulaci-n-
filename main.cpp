#include <iostream>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <ctime>
#include <csignal>
#include <atomic>
#include "mundo.h"
#include "tags.h"

static std::atomic<bool> running(true);

void on_sigint(int) {
    running = false;
}

int main() {
    std::srand((unsigned)std::time(nullptr));
    std::signal(SIGINT, on_sigint);

    Mundo m;

    const int NUM_AGENTES = 100;        // cambialo cuando quieras
    const int GRUPOS_INICIALES = 4;     // grupos balanceados
    const int TOTAL_TICKS = 100000000;  // simbólico, se corta con Ctrl+C
    const int SUMMARY_EVERY = 10;

    m.agentes.resize(NUM_AGENTES);
    m.init_grid(20000.0f, 20000.0f, 400.0f);

    auto spawns = m.choose_spawn_centers(GRUPOS_INICIALES);
    if (spawns.empty()) spawns.push_back(0);

    for (int i = 0; i < NUM_AGENTES; i++) {
        int group = i % (int)spawns.size();
        size_t center_idx = spawns[group];

        int cx = (int)(center_idx % m.grid_cols);
        int cy = (int)(center_idx / m.grid_cols);

        float jitter = m.cell_size * 1.8f;
        float sx = cx * m.cell_size + m.cell_size * 0.5f + (((rand() % 2001) - 1000) / 1000.0f) * jitter;
        float sy = cy * m.cell_size + m.cell_size * 0.5f + (((rand() % 2001) - 1000) / 1000.0f) * jitter;

        m.agentes[i].x = std::clamp(sx, 0.0f, m.width);
        m.agentes[i].y = std::clamp(sy, 0.0f, m.height);
        m.agentes[i].tribe_id = group;

        if (rand() % 2 == 0) {
            m.agentes[i].tags |= TAG_INFLAMABLE;
        }
    }

    std::vector<int> neighbors;
    std::vector<Agente> newborns;

    for (long long tick = 0; running && tick < TOTAL_TICKS; ++tick) {
        int deaths_this_tick = 0;
        int births_this_tick = 0;

        for (auto &a : m.agentes) {
            if (!a.alive) {
                if (a.dead_ticks < 20) a.dead_ticks++;
            }
        }

        m.agentes.erase(
            std::remove_if(m.agentes.begin(), m.agentes.end(),
                [](const Agente &a) {
                    return (!a.alive && a.dead_ticks >= 20);
                }),
            m.agentes.end()
        );

        m.build_grid();
        m.update_world_resources(m.count_alive(), (int)tick);
        m.compute_distance_map(m.food_distance, m.food_amount, 0.45f);
        m.compute_distance_map(m.water_distance, m.water_amount, 0.35f);

        newborns.clear();

        int en_fuego = 0;
        int comiendo = 0;
        int bebiendo = 0;
        int huyendo = 0;
        int socializando = 0;
        int asentando = 0;
        int reproduciendo = 0;
        int explorando = 0;
        int descansando = 0;
        int peleando = 0;

        for (int i = 0; i < (int)m.agentes.size(); i++) {
            Agente &a = m.agentes[i];
            if (!a.alive) continue;

            a.age++;
            a.energia = std::clamp(a.energia - (0.25f + (1.0f - a.adn_resistencia_estres) * 0.12f), 0.0f, 100.0f);
            a.hambre  = std::clamp(a.hambre  + 0.30f + (1.0f - a.adn_glotoneria) * 0.05f, 0.0f, 100.0f);
            a.sed     = std::clamp(a.sed     + 0.28f + (1.0f - a.adn_glotoneria) * 0.05f, 0.0f, 100.0f);

            if (a.age > a.lifespan || a.energia <= 0.0f || a.salud <= 0.0f) {
                a.mark_dead();
                deaths_this_tick++;
                continue;
            }

            size_t idx = m.cell_index(a.x, a.y);

            neighbors.clear();
            m.query_neighbors(i, neighbors);

            int vecinos_en_fuego = m.count_neighbors_with_tag(neighbors, TAG_EN_FUEGO);
            bool celda_con_fuego = m.cell_has(idx, TAG_EN_FUEGO);
            bool celda_con_comida = m.food_amount[idx] > 0.06f;
            bool celda_con_agua = m.water_amount[idx] > 0.06f;
            float shelter_here = m.shelter_amount[idx];
            float food_here = m.food_amount[idx];
            float water_here = m.water_amount[idx];

            int enemy_neighbors = 0;
            int ally_neighbors = 0;
            int enemy_target = -1;

            for (int nb : neighbors) {
                if (nb < 0 || nb >= (int)m.agentes.size()) continue;
                if (!m.agentes[nb].alive) continue;

                if (m.agentes[nb].tribe_id == a.tribe_id) {
                    ally_neighbors++;
                } else {
                    enemy_neighbors++;
                    if (enemy_target == -1) enemy_target = nb;
                }
            }

            float hambre_u = a.impulso_hambre();
            float sed_u = a.impulso_sed(celda_con_agua);
            float miedo_u = a.impulso_miedo(celda_con_fuego, vecinos_en_fuego, (int)tick);
            miedo_u *= (1.0f - std::min(0.45f, shelter_here * 0.25f));

            float social_u = a.impulso_social((float)ally_neighbors);
            float curiosidad_u = a.impulso_curiosidad(std::max(hambre_u, sed_u), miedo_u);
            float descanso_u = a.impulso_descanso(std::max(hambre_u, sed_u), miedo_u);
            float asentarse_u = a.impulso_asentarse(shelter_here, social_u, miedo_u);

            float repro_u = a.impulso_reproducir(shelter_here, food_here, water_here, social_u);
            float pelea_u = a.impulso_pelear(enemy_neighbors, miedo_u);

            Accion accion = ACT_EXPLORAR;
            float mejor = curiosidad_u;

            if (miedo_u > mejor && !(enemy_neighbors > 0 && pelea_u > miedo_u)) {
                mejor = miedo_u;
                accion = ACT_HUIR;
            }

            if (sed_u > mejor) {
                mejor = sed_u;
                accion = celda_con_agua ? ACT_BEBER : ACT_MIGRAR_WATER;
            }

            if (hambre_u > mejor) {
                mejor = hambre_u;
                accion = celda_con_comida ? ACT_COMER : ACT_MIGRAR_FOOD;
            }

            if (pelea_u > mejor) {
                mejor = pelea_u;
                accion = ACT_PELEAR;
            }

            if (repro_u > mejor) {
                mejor = repro_u;
                accion = ACT_REPRODUCIR;
            }

            if (asentarse_u > mejor) {
                mejor = asentarse_u;
                accion = ACT_ASENTARSE;
            }

            if (social_u > mejor) {
                mejor = social_u;
                accion = ACT_SOCIALIZAR;
            }

            if (descanso_u > mejor) {
                mejor = descanso_u;
                accion = ACT_DESCANSAR;
            }

            switch (accion) {
                case ACT_HUIR: {
                    huyendo++;
                    size_t threat = m.best_neighbor_cell_with_tag(i, TAG_EN_FUEGO);
                    if (threat != idx) {
                        m.move_agent_away_from_cell(a, threat, 250.0f);
                    } else {
                        a.x += (rand() % 3 - 1) * 120.0f;
                        a.y += (rand() % 3 - 1) * 120.0f;
                    }
                    a.energia -= 0.8f;
                    a.add_event(Evento::PELIGRO, (int)tick, -1);
                    break;
                }

                case ACT_COMER: {
                    comiendo++;
                    float got = m.take_food(idx, 0.22f);
                    if (got > 0.0f) {
                        a.energia = std::min(100.0f, a.energia + got * 65.0f);
                        a.hambre = std::max(0.0f, a.hambre - got * 80.0f);
                        a.add_event(Evento::COMIDA, (int)tick, -1);
                    } else {
                        size_t target = m.best_neighbor_by_gradient(i, m.food_distance);
                        if (target != idx) m.move_agent_toward_cell(a, target, 220.0f);
                    }
                    a.energia = std::clamp(a.energia - 0.2f, 0.0f, 100.0f);
                    break;
                }

                case ACT_BEBER: {
                    bebiendo++;
                    float got = m.take_water(idx, 0.22f);
                    if (got > 0.0f) {
                        a.sed = std::max(0.0f, a.sed - got * 85.0f);
                        a.energia = std::min(100.0f, a.energia + got * 10.0f);
                        a.salud = std::min(100.0f, a.salud + got * 3.0f);
                        a.add_event(Evento::BEBIDA, (int)tick, -1);
                    } else {
                        size_t target = m.best_neighbor_by_gradient(i, m.water_distance);
                        if (target != idx) m.move_agent_toward_cell(a, target, 220.0f);
                    }
                    a.energia = std::clamp(a.energia - 0.15f, 0.0f, 100.0f);
                    break;
                }

                case ACT_SOCIALIZAR: {
                    socializando++;
                    if (!neighbors.empty()) {
                        int nb = neighbors[rand() % neighbors.size()];
                        if (nb >= 0 && nb < (int)m.agentes.size() && m.agentes[nb].alive) {
                            a.energia = std::min(100.0f, a.energia + 0.5f);
                            a.socialidad = std::min(1.0f, a.socialidad + 0.001f);
                            a.loyalty = std::min(1.0f, a.loyalty + 0.0007f);

                            if (m.agentes[nb].tribe_id == a.tribe_id) {
                                if (!m.agentes[nb].memoria.empty() && (rand() % 100) < 30) {
                                    a.memoria.push_back(m.agentes[nb].memoria.back());
                                    if (a.memoria.size() > a.memoria_max) a.memoria.erase(a.memoria.begin());
                                }
                            }

                            a.add_event(Evento::SOCIAL, (int)tick, nb);
                        }
                    } else {
                        a.x += (rand() % 3 - 1) * 90.0f;
                        a.y += (rand() % 3 - 1) * 90.0f;
                    }
                    break;
                }

                case ACT_DESCANSAR: {
                    descansando++;
                    a.energia = std::min(100.0f, a.energia + 1.4f + shelter_here * 0.8f);
                    a.hambre = std::min(100.0f, a.hambre + 0.05f);
                    a.sed = std::min(100.0f, a.sed + 0.05f);
                    break;
                }

                case ACT_EXPLORAR: {
                    explorando++;
                    a.x += (rand() % 3 - 1) * 140.0f;
                    a.y += (rand() % 3 - 1) * 140.0f;
                    a.curiosidad = std::min(1.0f, a.curiosidad + 0.0005f);
                    break;
                }

                case ACT_ASENTARSE: {
                    asentando++;
                    if (m.try_build_shelter(idx)) {
                        a.energia = std::max(0.0f, a.energia - 0.6f);
                        a.add_event(Evento::ASENTARSE, (int)tick, -1);
                    } else {
                        a.x += (rand() % 3 - 1) * 70.0f;
                        a.y += (rand() % 3 - 1) * 70.0f;
                    }
                    break;
                }

                case ACT_REPRODUCIR: {
                    if (a.energia > 72.0f && a.salud > 55.0f && food_here > 0.20f && water_here > 0.20f) {
                        newborns.push_back(a.make_child(m.width, m.height));
                        reproduciendo++;
                        births_this_tick++;
                        a.energia = std::max(0.0f, a.energia - 26.0f);
                        a.hambre = std::min(100.0f, a.hambre + 6.0f);
                        a.sed = std::min(100.0f, a.sed + 4.0f);
                        a.add_event(Evento::NACIMIENTO, (int)tick, -1);
                    } else {
                        a.x += (rand() % 3 - 1) * 70.0f;
                        a.y += (rand() % 3 - 1) * 70.0f;
                    }
                    break;
                }

                case ACT_MIGRAR_FOOD: {
                    size_t target = m.best_neighbor_by_gradient(i, m.food_distance);
                    if (target != idx) {
                        m.move_agent_toward_cell(a, target, 220.0f);
                    } else {
                        a.x += (rand() % 3 - 1) * 90.0f;
                        a.y += (rand() % 3 - 1) * 90.0f;
                    }
                    a.energia = std::max(0.0f, a.energia - 0.25f);
                    break;
                }

                case ACT_MIGRAR_WATER: {
                    size_t target = m.best_neighbor_by_gradient(i, m.water_distance);
                    if (target != idx) {
                        m.move_agent_toward_cell(a, target, 220.0f);
                    } else {
                        a.x += (rand() % 3 - 1) * 90.0f;
                        a.y += (rand() % 3 - 1) * 90.0f;
                    }
                    a.energia = std::max(0.0f, a.energia - 0.25f);
                    break;
                }

                case ACT_PELEAR: {
                    peleando++;
                    if (enemy_target != -1 && enemy_target < (int)m.agentes.size() && m.agentes[enemy_target].alive) {
                        Agente &t = m.agentes[enemy_target];
                        float dmg = 3.5f + a.agresividad * 4.0f;
                        t.salud -= dmg;
                        a.energia -= 2.0f;
                        a.add_event(Evento::PELEA, (int)tick, enemy_target);
                        t.add_event(Evento::PELIGRO, (int)tick, i);

                        if (t.salud <= 0.0f) {
                            t.mark_dead();
                            deaths_this_tick++;
                        }
                    } else {
                        a.x += (rand() % 3 - 1) * 110.0f;
                        a.y += (rand() % 3 - 1) * 110.0f;
                    }
                    break;
                }
            }

            a.x = std::clamp(a.x, 0.0f, m.width);
            a.y = std::clamp(a.y, 0.0f, m.height);

            size_t new_idx = m.cell_index(a.x, a.y);
            m.resolver_quimica(a, m.cell_tags[new_idx]);

            if (a.tags & TAG_EN_FUEGO) {
                a.add_event(Evento::FUEGO, (int)tick, -1);
                a.salud -= 1.5f;
                en_fuego++;
            }

            if (celda_con_fuego || vecinos_en_fuego > 0) {
                a.add_event(Evento::PELIGRO, (int)tick, -1);
            }

            if (a.salud <= 0.0f || a.energia <= 0.0f) {
                a.mark_dead();
                deaths_this_tick++;
            }
        }

        if (!newborns.empty()) {
            m.agentes.reserve(m.agentes.size() + newborns.size());
            for (auto &n : newborns) m.agentes.push_back(std::move(n));
        }

        if (tick % SUMMARY_EVERY == 0) {
            std::cout << "--- Tick " << tick << " ---\n";
            std::cout << "Vivos: " << m.count_alive()
                      << " | Muertos pendientes: " << m.count_dead_pending()
                      << " | Nacimientos: " << births_this_tick
                      << " | Muertes: " << deaths_this_tick << "\n";
            std::cout << "Agentes en fuego: " << en_fuego << "\n";
            std::cout << "Comiendo: " << comiendo
                      << " | Bebiendo: " << bebiendo
                      << " | Huyendo: " << huyendo
                      << " | Socializando: " << socializando
                      << " | Asentando: " << asentando
                      << " | Explorando: " << explorando
                      << " | Descansando: " << descansando
                      << " | Peleando: " << peleando << "\n";

            for (int i = 0; i < 3 && i < (int)m.agentes.size(); i++) {
                if (!m.agentes[i].alive) continue;
                neighbors.clear();
                m.query_neighbors(i, neighbors);
                std::cout << "Agente " << i << " vecinos: ";
                for (int n : neighbors) std::cout << n << " ";
                std::cout << "\n";
            }
        }
    }

    std::cout << "\nSimulación detenida.\n";
    return 0;
}