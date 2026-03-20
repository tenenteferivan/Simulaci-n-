#pragma once
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "tags.h"

static inline float frand(float a, float b) {
    return a + ((rand() % 10001) / 10000.0f) * (b - a);
}

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
    long long tick;
    int agente_id;
    float intensidad = 1.0f;
    int duracion = 40;
};

struct ADN {
    float velocidad_max;
    float fuerza_ataque;
    float eficiencia_metabolica;
    float rango_vision;
    float fertilidad;
    float resistencia_clima;

    float agresividad;
    float sociabilidad;
    float curiosidad;

    float construccion;
    float migracion;
    float conflicto;
    float tradicion;

    uint8_t color[3];
    std::string nombre_linaje;

    ADN() {
        velocidad_max = frand(3.0f, 10.0f);
        fuerza_ataque = frand(8.0f, 40.0f);
        eficiencia_metabolica = frand(0.0008f, 0.0045f);
        rango_vision = frand(100.0f, 450.0f);
        fertilidad = frand(0.02f, 0.16f);
        resistencia_clima = frand(0.0f, 1.0f);

        agresividad = frand(0.0f, 1.0f);
        sociabilidad = frand(0.0f, 1.0f);
        curiosidad = frand(0.0f, 1.0f);

        construccion = frand(0.0f, 1.0f);
        migracion = frand(0.0f, 1.0f);
        conflicto = frand(0.0f, 1.0f);
        tradicion = frand(0.0f, 1.0f);

        color[0] = (uint8_t)(rand() % 256);
        color[1] = (uint8_t)(rand() % 256);
        color[2] = (uint8_t)(rand() % 256);

        nombre_linaje = "L" + std::to_string(rand() % 100000);
    }
};

class Agente {
public:
    static constexpr int N_IN = 14;
    static constexpr int N_H  = 24;
    static constexpr int N_OUT = 10;

    uint64_t uid = 0;

    float x = 0.0f;
    float y = 0.0f;

    float energia = 80.0f;
    float salud = 100.0f;
    float hidratacion = 100.0f;
    float hambre = 20.0f;
    float sed = 20.0f;

    int edad = 0;
    int esperanza_vida = 2500;

    bool vivo = true;
    bool alive = true;
    int dead_ticks = 0;

    int tribe_id = 0;

    float inteligencia = 0.5f;
    float apego = 0.5f;
    float hostilidad = 0.0f;
    float disciplina = 0.5f;
    float decision_bias = 0.5f;

    uint32_t tags = 0;

    Accion accion_actual = ACT_EXPLORAR;
    int ticks_en_accion = 0;
    size_t objetivo_celda = 0;
    int objetivo_agente = -1;

    Agente* next_in_cell = nullptr;
    size_t cell_idx = 0;

    std::vector<Evento> memoria;
    size_t memoria_max = 80;

    ADN genetica;

    float pesos_h[N_H][N_IN];
    float bias_h[N_H];
    float pesos_o[N_OUT][N_H];
    float bias_o[N_OUT];
    float memoria_corta[8];

    Agente(float start_x = 0.0f, float start_y = 0.0f) : x(start_x), y(start_y) {
        uid = ((uint64_t)rand() << 32) ^ (uint64_t)rand();

        energia = frand(55.0f, 110.0f);
        salud = frand(70.0f, 100.0f);
        hidratacion = frand(70.0f, 100.0f);
        hambre = frand(0.0f, 35.0f);
        sed = frand(0.0f, 35.0f);

        inteligencia = frand(0.20f, 0.95f);
        apego = frand(0.05f, 0.95f);
        hostilidad = frand(0.0f, 0.7f);
        disciplina = frand(0.1f, 0.95f);
        decision_bias = frand(0.1f, 0.9f);

        esperanza_vida = (int)frand(1800.0f, 9000.0f);
        genetica.nombre_linaje = "L" + std::to_string(rand() % 100000);

        for (int i = 0; i < N_H; ++i) {
            bias_h[i] = frand(-1.0f, 1.0f);
            for (int j = 0; j < N_IN; ++j) pesos_h[i][j] = frand(-1.0f, 1.0f);
        }
        for (int i = 0; i < N_OUT; ++i) {
            bias_o[i] = frand(-1.0f, 1.0f);
            for (int j = 0; j < N_H; ++j) pesos_o[i][j] = frand(-1.0f, 1.0f);
        }
        for (int i = 0; i < 8; ++i) memoria_corta[i] = 0.0f;
    }

    void add_event(Evento::Tipo tipo, long long tick, int agente_id = -1, float intensidad = 1.0f, int duracion = 40) {
        if (memoria.size() >= memoria_max) memoria.erase(memoria.begin());
        memoria.push_back(Evento{tipo, tick, agente_id, intensidad, duracion});
    }

    void iniciar_accion(Accion a, int duracion, size_t target = 0, int target_agent = -1) {
        accion_actual = a;
        ticks_en_accion = std::max(1, duracion);
        objetivo_celda = target;
        objetivo_agente = target_agent;
    }

    bool debe_interrumpir(bool celda_fuego, float sed_u, float hambre_u, float miedo_u) const {
        if (celda_fuego) return true;
        if (salud < 18.0f) return true;
        if (energia < 5.0f) return true;
        if (sed_u > 0.92f) return true;
        if (hambre_u > 0.95f) return true;
        if (miedo_u > 0.95f) return true;
        return false;
    }

    void procesar_cerebro(const std::vector<float>& inputs, std::vector<float>& outputs) {
        float hidden[N_H];
        for (int i = 0; i < N_H; ++i) {
            float suma = bias_h[i];
            for (int j = 0; j < N_IN; ++j) suma += inputs[j] * pesos_h[i][j];
            hidden[i] = 1.0f / (1.0f + std::exp(-suma));
        }

        outputs.assign(N_OUT, 0.0f);
        for (int o = 0; o < N_OUT; ++o) {
            float suma = bias_o[o];
            for (int h = 0; h < N_H; ++h) suma += hidden[h] * pesos_o[o][h];
            outputs[o] = std::tanh(suma);
        }

        for (int i = 0; i < 8; ++i) memoria_corta[i] = hidden[i];
    }

    float recuerdo_peligro(long long tick) const {
        float bonus = 0.0f;
        int revisados = 0;

        for (auto it = memoria.rbegin(); it != memoria.rend() && revisados < 12; ++it, ++revisados) {
            long long edad_evento = tick - it->tick;
            if (edad_evento < 0) continue;
            if (edad_evento > it->duracion * 4) break;

            float peso = it->intensidad / (1.0f + (float)edad_evento * 0.08f);
            if (it->tipo == Evento::FUEGO || it->tipo == Evento::PELIGRO) bonus += peso;
        }

        return std::min(bonus, 1.0f);
    }

    float recuerdo_social(long long tick) const {
        float bonus = 0.0f;
        int revisados = 0;

        for (auto it = memoria.rbegin(); it != memoria.rend() && revisados < 10; ++it, ++revisados) {
            long long edad_evento = tick - it->tick;
            if (edad_evento < 0) continue;
            if (edad_evento > it->duracion * 5) break;

            if (it->tipo == Evento::SOCIAL || it->tipo == Evento::NACIMIENTO || it->tipo == Evento::ASENTARSE) {
                bonus += it->intensidad / (1.0f + (float)edad_evento * 0.06f);
            }
        }

        return std::min(bonus, 1.0f);
    }

    float impulso_hambre() const {
        return std::clamp(((100.0f - energia) / 100.0f + hambre / 100.0f) * genetica.agresividad * 0.35f + genetica.fertilidad * 0.35f, 0.0f, 1.0f);
    }

    float impulso_sed(bool agua_cerca) const {
        float base = (sed / 100.0f) * (0.4f + genetica.migracion * 0.6f);
        if (agua_cerca) base *= 0.65f;
        return std::clamp(base, 0.0f, 1.0f);
    }

    float impulso_miedo(bool celda_fuego, int vecinos_en_fuego, long long tick, float peligro_feromona) const {
        float base = (0.25f + genetica.conflicto * 0.15f) * (1.0f - genetica.agresividad);
        base += celda_fuego ? 0.60f : 0.0f;
        base += std::min(1.0f, vecinos_en_fuego * 0.12f);
        base += recuerdo_peligro(tick);
        base += peligro_feromona * 0.45f;
        return std::clamp(base, 0.0f, 1.0f);
    }

    float impulso_social(float ally_count, long long tick, float food_feromona) const {
        float base = (ally_count / 12.0f) * genetica.sociabilidad;
        base += recuerdo_social(tick) * 0.12f;
        base += apego * 0.15f;
        base += food_feromona * 0.10f;
        return std::clamp(base, 0.0f, 1.0f);
    }

    float impulso_curiosidad(float hambre_u, float miedo_u) const {
        return std::clamp(genetica.curiosidad * (1.0f - miedo_u) * (1.0f - hambre_u) * (0.4f + inteligencia), 0.0f, 1.0f);
    }

    float impulso_descanso(float hambre_u, float miedo_u) const {
        return std::clamp((1.0f - energia / 100.0f) * (1.0f - miedo_u) * (1.0f - hambre_u) * (0.3f + disciplina), 0.0f, 1.0f);
    }

    float impulso_asentarse(float shelter_here, float social_u, float fear_u) const {
        return std::clamp((1.0f - fear_u) * (social_u + 0.12f) * (1.0f - std::min(1.0f, shelter_here)) * genetica.construccion, 0.0f, 1.0f);
    }

    float impulso_reproducir(float shelter_here, float food_here, float water_here, float social_u) const {
        float base = genetica.fertilidad;
        base *= (energia / 100.0f);
        base *= (salud / 100.0f);
        base *= (1.0f - hambre / 120.0f);
        base *= (1.0f - sed / 120.0f);
        base *= (0.25f + social_u);
        base *= (0.25f + shelter_here);
        base *= (0.25f + food_here);
        base *= (0.25f + water_here);
        base *= (0.30f + apego);
        return std::clamp(base * 2.2f, 0.0f, 1.0f);
    }

    float impulso_pelear(int enemy_neighbors, float fear_u) const {
        return std::clamp(genetica.agresividad * genetica.conflicto * (0.30f + enemy_neighbors * 0.16f) * (1.0f - fear_u) + hostilidad * 0.25f, 0.0f, 1.0f);
    }

    void envejecer() {
        ++edad;
        float gasto = genetica.eficiencia_metabolica * 14.0f;
        energia -= gasto;
        hidratacion -= gasto * 0.6f;
        hambre += gasto * 1.2f;
        sed += gasto * 1.2f;
        if (energia <= 0.0f || hidratacion <= 0.0f || edad > esperanza_vida) {
            mark_dead();
        }
    }

    void mark_dead() {
        vivo = false;
        alive = false;
        dead_ticks = 0;
    }

    void aprender_observando(const Agente& otro, long long tick, float fuerza = 0.15f) {
        float f = std::clamp(fuerza * genetica.sociabilidad, 0.0f, 1.0f);

        inteligencia = std::clamp(inteligencia + (otro.inteligencia - inteligencia) * 0.02f * f, 0.0f, 1.0f);
        apego = std::clamp(apego + (otro.apego - apego) * 0.02f * f, 0.0f, 1.0f);
        hostilidad = std::clamp(hostilidad + (otro.hostilidad - hostilidad) * 0.02f * f, 0.0f, 1.0f);
        disciplina = std::clamp(disciplina + (otro.disciplina - disciplina) * 0.02f * f, 0.0f, 1.0f);

        genetica.sociabilidad = std::clamp(genetica.sociabilidad + (otro.genetica.sociabilidad - genetica.sociabilidad) * 0.015f * f, 0.0f, 1.0f);
        genetica.curiosidad = std::clamp(genetica.curiosidad + (otro.genetica.curiosidad - genetica.curiosidad) * 0.015f * f, 0.0f, 1.0f);
        genetica.agresividad = std::clamp(genetica.agresividad + (otro.genetica.agresividad - genetica.agresividad) * 0.015f * f, 0.0f, 1.0f);

        if (!otro.memoria.empty() && (rand() % 100) < 40) {
            Evento e = otro.memoria.back();
            e.tick = tick;
            e.intensidad *= 0.7f;
            e.duracion = std::max(20, (int)(e.duracion * 0.8f));
            add_event(e.tipo, e.tick, e.agente_id, e.intensidad, e.duracion);
        }
    }

    Agente heredar(const Agente& pareja) const {
        Agente hijo(x, y);

        hijo.uid = ((uint64_t)rand() << 32) ^ (uint64_t)rand();
        hijo.tribe_id = (tribe_id == pareja.tribe_id) ? tribe_id : ((rand() % 2) ? tribe_id : pareja.tribe_id);

        auto mix = [](float a, float b) {
            return (a + b) * 0.5f;
        };
        auto noise = []() {
            return ((rand() % 2001) - 1000) / 1000.0f;
        };

        hijo.genetica.velocidad_max = mix(genetica.velocidad_max, pareja.genetica.velocidad_max);
        hijo.genetica.fuerza_ataque = mix(genetica.fuerza_ataque, pareja.genetica.fuerza_ataque);
        hijo.genetica.eficiencia_metabolica = mix(genetica.eficiencia_metabolica, pareja.genetica.eficiencia_metabolica);
        hijo.genetica.rango_vision = mix(genetica.rango_vision, pareja.genetica.rango_vision);
        hijo.genetica.fertilidad = mix(genetica.fertilidad, pareja.genetica.fertilidad);
        hijo.genetica.resistencia_clima = mix(genetica.resistencia_clima, pareja.genetica.resistencia_clima);

        hijo.genetica.agresividad = mix(genetica.agresividad, pareja.genetica.agresividad);
        hijo.genetica.sociabilidad = mix(genetica.sociabilidad, pareja.genetica.sociabilidad);
        hijo.genetica.curiosidad = mix(genetica.curiosidad, pareja.genetica.curiosidad);

        hijo.genetica.construccion = mix(genetica.construccion, pareja.genetica.construccion);
        hijo.genetica.migracion = mix(genetica.migracion, pareja.genetica.migracion);
        hijo.genetica.conflicto = mix(genetica.conflicto, pareja.genetica.conflicto);
        hijo.genetica.tradicion = mix(genetica.tradicion, pareja.genetica.tradicion);

        for (int c = 0; c < 3; ++c) {
            hijo.genetica.color[c] = (uint8_t)((genetica.color[c] + pareja.genetica.color[c]) / 2);
        }

        hijo.genetica.nombre_linaje = genetica.nombre_linaje.substr(0, 6) + "-" + pareja.genetica.nombre_linaje.substr(0, 6);

        auto mutate = [&](float &v, float scale, float lo = 0.0f, float hi = 1.0f) {
            if ((rand() % 100) < 5) v = std::clamp(v + noise() * scale, lo, hi);
        };

        mutate(hijo.genetica.velocidad_max, 1.0f, 2.0f, 12.0f);
        mutate(hijo.genetica.fuerza_ataque, 4.0f, 5.0f, 80.0f);
        mutate(hijo.genetica.eficiencia_metabolica, 0.0010f, 0.0004f, 0.01f);
        mutate(hijo.genetica.rango_vision, 40.0f, 60.0f, 600.0f);
        mutate(hijo.genetica.fertilidad, 0.03f, 0.005f, 0.35f);
        mutate(hijo.genetica.resistencia_clima, 0.10f, 0.0f, 1.0f);

        mutate(hijo.genetica.agresividad, 0.12f);
        mutate(hijo.genetica.sociabilidad, 0.12f);
        mutate(hijo.genetica.curiosidad, 0.12f);
        mutate(hijo.genetica.construccion, 0.12f);
        mutate(hijo.genetica.migracion, 0.12f);
        mutate(hijo.genetica.conflicto, 0.12f);
        mutate(hijo.genetica.tradicion, 0.12f);

        hijo.energia = frand(45.0f, 85.0f);
        hijo.salud = frand(65.0f, 95.0f);
        hijo.hidratacion = frand(55.0f, 95.0f);
        hijo.hambre = frand(10.0f, 25.0f);
        hijo.sed = frand(10.0f, 25.0f);
        hijo.edad = 0;
        hijo.esperanza_vida = std::max(1500, esperanza_vida + (rand() % 800) - 400);
        hijo.vivo = true;
        hijo.alive = true;
        hijo.dead_ticks = 0;
        hijo.accion_actual = ACT_EXPLORAR;
        hijo.ticks_en_accion = 0;
        hijo.objetivo_celda = 0;
        hijo.objetivo_agente = -1;
        hijo.tags &= ~(TAG_FUEGO | TAG_PELIGRO | TAG_MUERTE);

        hijo.apego = std::clamp((apego + pareja.apego) * 0.5f + noise() * 0.08f, 0.0f, 1.0f);
        hijo.hostilidad = std::clamp((hostilidad + pareja.hostilidad) * 0.5f + noise() * 0.05f, 0.0f, 1.0f);
        hijo.inteligencia = std::clamp((inteligencia + pareja.inteligencia) * 0.5f + noise() * 0.06f, 0.0f, 1.0f);
        hijo.disciplina = std::clamp((disciplina + pareja.disciplina) * 0.5f + noise() * 0.06f, 0.0f, 1.0f);
        hijo.decision_bias = std::clamp((decision_bias + pareja.decision_bias) * 0.5f + noise() * 0.06f, 0.0f, 1.0f);

        for (int i = 0; i < N_H; ++i) {
            hijo.bias_h[i] = ((rand() % 2) ? bias_h[i] : pareja.bias_h[i]);
            if ((rand() % 100) < 5) hijo.bias_h[i] += noise() * 0.15f;
            for (int j = 0; j < N_IN; ++j) {
                hijo.pesos_h[i][j] = ((rand() % 2) ? pesos_h[i][j] : pareja.pesos_h[i][j]);
                if ((rand() % 100) < 5) hijo.pesos_h[i][j] += noise() * 0.15f;
            }
        }

        for (int o = 0; o < N_OUT; ++o) {
            hijo.bias_o[o] = ((rand() % 2) ? bias_o[o] : pareja.bias_o[o]);
            if ((rand() % 100) < 5) hijo.bias_o[o] += noise() * 0.15f;
            for (int h = 0; h < N_H; ++h) {
                hijo.pesos_o[o][h] = ((rand() % 2) ? pesos_o[o][h] : pareja.pesos_o[o][h]);
                if ((rand() % 100) < 5) hijo.pesos_o[o][h] += noise() * 0.15f;
            }
        }

        hijo.memoria.clear();
        int keep = std::min(8, (int)memoria.size());
        for (int i = (int)memoria.size() - keep; i < (int)memoria.size(); ++i) {
            if (i >= 0) hijo.memoria.push_back(memoria[i]);
        }

        hijo.memoria_corta[0] = inteligencia;
        hijo.memoria_corta[1] = apego;
        hijo.memoria_corta[2] = hostilidad;
        hijo.memoria_corta[3] = genetica.agresividad;
        hijo.memoria_corta[4] = genetica.sociabilidad;
        hijo.memoria_corta[5] = genetica.curiosidad;
        hijo.memoria_corta[6] = genetica.construccion;
        hijo.memoria_corta[7] = genetica.migracion;

        return hijo;
    }
};


