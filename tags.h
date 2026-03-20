#pragma once
#include <cstdint>

enum TerrainTags : uint32_t {
   TAG_FUEGO       = 1u << 8,
    TAG_NONE       = 0,
    TAG_PASTO      = 1u << 0,
    TAG_AGUA       = 1u << 1,
    TAG_MUERTE     = 1u << 2,
    TAG_FERTIL     = 1u << 3,
    TAG_REFUGIO    = 1u << 4,
    TAG_PELIGRO    = 1u << 5,
    TAG_FEROMONA_A = 1u << 6,
    TAG_FEROMONA_B = 1u << 7,

    TAG_BOSQUE     = 1u << 8,
    TAG_MONTANA    = 1u << 9,
    TAG_COLINA     = 1u << 10,
    TAG_CUEVA      = 1u << 11,
    TAG_DESIERTO   = 1u << 12,
    TAG_PANTANO    = 1u << 13,
    TAG_MINERAL    = 1u << 14,
    TAG_PIEDRA     = 1u << 15,
    TAG_LAVA       = 1u << 16
};
