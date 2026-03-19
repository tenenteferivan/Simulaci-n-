#pragma once
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "tags.h"

enum Accion {
    ACT_DESCANSAR,
    ACT_HUIR,
    ACT_COMER,
    ACT_BEBER,
    ACT_SOCIALIZAR,
    ACT_EXPLORAR,
    ACT_ASENTARSE,
    ACT_REPRODUCIR,
    ACT_MIGRAR_FOOD,
    ACT_MIGRAR_WATER,
    ACT_PELEAR
};

struct Evento {
    enum Tipo { FUEGO, PELIGRO, COMIDA, BEBIDA, SOCIAL, NACIMIENTO, ASENTARSE, PELEA } tipo;
    int tick;
    int agente_id;
};

struct Agente {
    float x = 0.0f;
    float y = 0.0f;

    float energia = 80.0f;
    float hambre = 20.0f;
    float sed = 20.0f;
    float salud = 100.0f;

    float curiosidad = 0.5f;
    float socialidad = 0.5f;
    float agresividad = 0.3f;
    float miedo_base = 0.3f;
    float decision_bias = 0.5f;
    float loyalty = 0.5f;

    float adn_glotoneria = 0.5f;
    float adn_cobardia = 0.5f;
    float adn_social = 0.5f;
    float adn_curiosidad = 0.5f;
    float adn_resistencia_estres = 0.5f;
    float repro_rate = 0.02f;

    uint32_t tags = 0;
    int next_in_cell = -1;

    int tribe_id = 0;
    bool alive = true;
    int dead_ticks = 0;
    int age = 0;
    int lifespan = 120;

    std::vector<Evento> memoria;
    size_t memoria_max = 20;

    Agente() {
        energia = 50.0f + (rand() % 51);
        hambre = rand() % 40;
        sed = rand() % 40;
        curiosidad = (rand() % 100) / 100.0f;
        socialidad = (rand() % 100) / 100.0f;
        agresividad = (rand() % 100) / 100.0f;
        miedo_base = (rand() % 100) / 100.0f;
        decision_bias = (rand() % 100) / 100.0f;
        loyalty = (rand() % 100) / 100.0f;
        salud = 70.0f + (rand() % 31);

        adn_glotoneria = (rand() % 100) / 100.0f;
        adn_cobardia = (rand() % 100) / 100.0f;
        adn_social = (rand() % 100) / 100.0f;
        adn_curiosidad = (rand() % 100) / 100.0f;
        adn_resistencia_estres = (rand() % 100) / 100.0f;
        repro_rate = 0.01f + (rand() % 30) / 1000.0f;
        lifespan = 60 + (rand() % 120);
    }

    void add_event(Evento::Tipo tipo, int tick, int agente_id = -1) {
        if (memoria.size() >= memoria_max) {
            memoria.erase(memoria.begin());
        }
        memoria.push_back(Evento{tipo, tick, agente_id});
    }

    float recuerdo_peligro(int tick) const {
        float bonus = 0.0f;
        int revisados = 0;

        for (auto it = memoria.rbegin(); it != memoria.rend() && revisados < 6; ++it, ++revisados) {
            int edad_evento = tick - it->tick;
            if (edad_evento < 0) continue;
            if (edad_evento > 30) break;

            if (it->tipo == Evento::FUEGO || it->tipo == Evento::PELIGRO) {
                bonus += 0.12f / (1.0f + edad_evento * 0.25f);
            }
        }

        return std::min(bonus, 1.0f);
    }

    float impulso_hambre() const {
        return std::clamp(((100.0f - energia) / 100.0f + hambre / 100.0f) * adn_glotoneria, 0.0f, 1.0f);
    }

    float impulso_sed(bool agua_cerca) const {
        float base = (sed / 100.0f) * adn_glotoneria;
        if (agua_cerca) base *= 0.7f;
        return std::clamp(base, 0.0f, 1.0f);
    }

    float impulso_miedo(bool celda_fuego, int vecinos_en_fuego, int tick) const {
        float base = miedo_base * adn_cobardia;
        base += celda_fuego ? 0.55f : 0.0f;
        base += std::min(1.0f, vecinos_en_fuego * 0.12f);
        base += recuerdo_peligro(tick);
        return std::clamp(base, 0.0f, 1.0f);
    }

    float impulso_social(float neighbor_count) const {
        return std::clamp((neighbor_count / 12.0f) * socialidad * adn_social, 0.0f, 1.0f);
    }

    float impulso_curiosidad(float hambre_u, float miedo_u) const {
        return std::clamp(curiosidad * adn_curiosidad * (1.0f - miedo_u) * (1.0f - hambre_u), 0.0f, 1.0f);
    }

    float impulso_descanso(float hambre_u, float miedo_u) const {
        return std::clamp((1.0f - energia / 100.0f) * (1.0f - miedo_u) * (1.0f - hambre_u) * (0.5f + adn_resistencia_estres), 0.0f, 1.0f);
    }

    float impulso_asentarse(float shelter_here, float social_u, float fear_u) const {
        return std::clamp((1.0f - fear_u) * (social_u + 0.15f) * (1.0f - std::min(1.0f, shelter_here)) * (0.5f + adn_resistencia_estres), 0.0f, 1.0f);
    }

    float impulso_reproducir(float shelter_here, float food_here, float water_here, float social_u) const {
        float base = repro_rate;
        base *= (energia / 100.0f);
        base *= (salud / 100.0f);
        base *= (1.0f - hambre / 120.0f);
        base *= (1.0f - sed / 120.0f);
        base *= (0.3f + social_u);
        base *= (0.3f + shelter_here);
        base *= (0.3f + food_here);
        base *= (0.3f + water_here);
        return std::clamp(base * 2.0f, 0.0f, 1.0f);
    }

    float impulso_pelear(int enemy_neighbors, float fear_u) const {
        return std::clamp(agresividad * (0.35f + enemy_neighbors * 0.16f) * (1.0f - fear_u), 0.0f, 1.0f);
    }

    void mark_dead() {
        alive = false;
        dead_ticks = 0;
    }

    Agente make_child(float world_w, float world_h) const {
        Agente c = *this;

        c.x = std::clamp(x + (float)((rand() % 401) - 200), 0.0f, world_w);
        c.y = std::clamp(y + (float)((rand() % 401) - 200), 0.0f, world_h);

        c.energia = 45.0f + (rand() % 25);
        c.hambre = 15.0f + (rand() % 20);
        c.sed = 15.0f + (rand() % 20);
        c.salud = 70.0f + (rand() % 20);

        c.age = 0;
        c.alive = true;
        c.dead_ticks = 0;
        c.tags &= ~(TAG_EN_FUEGO | TAG_ACIDO);

        auto mutate = [](float &v, float scale) {
            float delta = ((rand() % 2001) - 1000) / 1000.0f * scale;
            v = std::clamp(v + delta, 0.0f, 1.0f);
        };

        mutate(c.curiosidad, 0.08f);
        mutate(c.socialidad, 0.08f);
        mutate(c.agresividad, 0.08f);
        mutate(c.miedo_base, 0.08f);
        mutate(c.decision_bias, 0.08f);
        mutate(c.loyalty, 0.08f);
        mutate(c.adn_glotoneria, 0.06f);
        mutate(c.adn_cobardia, 0.06f);
        mutate(c.adn_social, 0.06f);
        mutate(c.adn_curiosidad, 0.06f);
        mutate(c.adn_resistencia_estres, 0.06f);

        c.repro_rate = std::clamp(c.repro_rate + (((rand() % 2001) - 1000) / 1000.0f) * 0.01f, 0.001f, 0.08f);
        c.lifespan = std::max(30, lifespan + (rand() % 31) - 15);
        c.tribe_id = tribe_id;

        c.memoria.clear();
        int keep = std::min(3, (int)memoria.size());
        for (int i = (int)memoria.size() - keep; i < (int)memoria.size(); ++i) {
            if (i >= 0) c.memoria.push_back(memoria[i]);
        }

        return c;
    }
};