/*
 * sprite.c -- part of SpaceRobots2
 * Copyright (C) 2020-2022 Michael Banack <github@banack.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <png.h>

#include "sprite.h"
#include "Random.h"

typedef enum SpriteSource {
    SPRITE_SOURCE_SPACE,
    SPRITE_SOURCE_NAJU,
    SPRITE_SOURCE_ALTAIR,
    SPRITE_SOURCE_URSA,
    SPRITE_SOURCE_VEGA,
    SPRITE_SOURCE_ARANEA,

    SPRITE_SOURCE_MAX,
    SPRITE_SOURCE_INVALID,
} SpriteSource;

typedef struct SpriteSpec {
    SpriteType type;
    SpriteSource source;
    uint32 x, y, w, h;
} SpriteSpec;

#define SW 129
#define SH 103
#define SOX(_i) ((_i % 4) * SW)
#define SOY(_i) ((_i / 4) * SH)

#define SPRITE_SPACE(_set, _i) \
    { SPRITE_SPACE_ ## _set ## _BASE,       SPRITE_SOURCE_SPACE, \
        SOX(_i) + 1,   SOY(_i) + 1,   101, 101, }, \
    { SPRITE_SPACE_ ## _set ## _FIGHTER,    SPRITE_SOURCE_SPACE, \
        SOX(_i) + 103, SOY(_i) + 1,  11,  11, }, \
    { SPRITE_SPACE_ ## _set ## _MISSILE,    SPRITE_SOURCE_SPACE, \
        SOX(_i) + 115, SOY(_i) + 1,   7,   7, }, \
    { SPRITE_SPACE_ ## _set ## _POWER_CORE, SPRITE_SOURCE_SPACE, \
        SOX(_i) + 123, SOY(_i) + 1,   5,   5, }

#define SPRITE_ALTAIR(_set, _i) \
    { SPRITE_ALTAIR_ ## _set ## _BASE,       SPRITE_SOURCE_ALTAIR, \
        SOX(_i) + 1,   SOY(_i) + 1,   101, 101, }, \
    { SPRITE_ALTAIR_ ## _set ## _FIGHTER,    SPRITE_SOURCE_ALTAIR, \
        SOX(_i) + 103, SOY(_i) + 3,  11,  11, }, \
    { SPRITE_ALTAIR_ ## _set ## _MISSILE,    SPRITE_SOURCE_ALTAIR, \
        SOX(_i) + 115, SOY(_i) + 5,   7,   7, }, \
    { SPRITE_ALTAIR_ ## _set ## _POWER_CORE, SPRITE_SOURCE_ALTAIR, \
        SOX(_i) + 123, SOY(_i) + 7,   5,   5, }

#define SPRITE_URSA(_set, _i) \
    { SPRITE_URSA_ ## _set ## _BASE,       SPRITE_SOURCE_URSA, \
        SOX(_i) + 2,   SOY(_i) + 2,   101, 101, }, \
    { SPRITE_URSA_ ## _set ## _FIGHTER,    SPRITE_SOURCE_URSA, \
        SOX(_i) + 103, SOY(_i) + 1,  11,  11, }, \
    { SPRITE_URSA_ ## _set ## _MISSILE,    SPRITE_SOURCE_URSA, \
        SOX(_i) + 115, SOY(_i) + 1,   7,   7, }, \
    { SPRITE_URSA_ ## _set ## _POWER_CORE, SPRITE_SOURCE_URSA, \
        SOX(_i) + 123, SOY(_i) + 1,   5,   5, }

#define SPRITE_VEGA(_set, _i) \
    { SPRITE_VEGA_ ## _set ## _BASE,       SPRITE_SOURCE_VEGA, \
        SOX(_i) + 2,   SOY(_i) + 2,   101, 101, }, \
    { SPRITE_VEGA_ ## _set ## _FIGHTER,    SPRITE_SOURCE_VEGA, \
        SOX(_i) + 103, SOY(_i) + 1,  11,  11, }, \
    { SPRITE_VEGA_ ## _set ## _MISSILE,    SPRITE_SOURCE_VEGA, \
        SOX(_i) + 115, SOY(_i) + 1,   7,   7, }, \
    { SPRITE_VEGA_ ## _set ## _POWER_CORE, SPRITE_SOURCE_VEGA, \
        SOX(_i) + 123, SOY(_i) + 1,   5,   5, }

#define SPRITE_ARANEA(_set, _i) \
    { SPRITE_ARANEA_ ## _set ## _BASE,       SPRITE_SOURCE_ARANEA, \
        SOX(_i) + 2,   SOY(_i) + 2,   101, 101, }, \
    { SPRITE_ARANEA_ ## _set ## _FIGHTER,    SPRITE_SOURCE_ARANEA, \
        SOX(_i) + 103, SOY(_i) + 1,  11,  11, }, \
    { SPRITE_ARANEA_ ## _set ## _MISSILE,    SPRITE_SOURCE_ARANEA, \
        SOX(_i) + 115, SOY(_i) + 1,   7,   7, }, \
    { SPRITE_ARANEA_ ## _set ## _POWER_CORE, SPRITE_SOURCE_ARANEA, \
        SOX(_i) + 123, SOY(_i) + 1,   5,   5, }


static const SpriteSpec gSpecs[] = {
    { SPRITE_INVALID,                 SPRITE_SOURCE_INVALID,       0,   0,   0,   0, },

    SPRITE_SPACE(BLUE,     0),
    SPRITE_SPACE(PURPLE,   1),
    SPRITE_SPACE(GREEN,    2),
    SPRITE_SPACE(GREEN2,   3),
    SPRITE_SPACE(GREEN3,   4),
    SPRITE_SPACE(YELLOW,   5),
    SPRITE_SPACE(ORANGE,   6),
    SPRITE_SPACE(RED,      7),
    SPRITE_SPACE(PURPLE2,  8),
    SPRITE_SPACE(RED2,     9),
    SPRITE_SPACE(WHITE,   10),
    SPRITE_SPACE(YELLOW2, 11),
    SPRITE_SPACE(BROWN,   12),
    SPRITE_SPACE(RED3,    13),
    SPRITE_SPACE(PURPLE3, 14),

    { SPRITE_NAJU_BLUE_BASE,          SPRITE_SOURCE_NAJU,         38,  34,  99,  99, },
    { SPRITE_NAJU_BLUE_FIGHTER1,      SPRITE_SOURCE_NAJU,         59, 148,   9,   9, },
    { SPRITE_NAJU_BLUE_FIGHTER2,      SPRITE_SOURCE_NAJU,         75, 148,   9,   9, },
    { SPRITE_NAJU_BLUE_FIGHTER3,      SPRITE_SOURCE_NAJU,         91, 148,   9,   9, },
    { SPRITE_NAJU_BLUE_FIGHTER4,      SPRITE_SOURCE_NAJU,        107, 148,   9,   9, },
    { SPRITE_NAJU_BLUE_POWER_CORE,    SPRITE_SOURCE_NAJU,        126, 150,   4,   4, },
    { SPRITE_NAJU_BLUE_MISSILE,       SPRITE_SOURCE_NAJU,        140, 148,   8,   8, },

    { SPRITE_NAJU_PURPLE_BASE,        SPRITE_SOURCE_NAJU,        198,  34,  99,  99, },
    { SPRITE_NAJU_PURPLE_FIGHTER1,    SPRITE_SOURCE_NAJU,        219, 148,   9,   9, },
    { SPRITE_NAJU_PURPLE_FIGHTER2,    SPRITE_SOURCE_NAJU,        235, 148,   9,   9, },
    { SPRITE_NAJU_PURPLE_FIGHTER3,    SPRITE_SOURCE_NAJU,        251, 148,   9,   9, },
    { SPRITE_NAJU_PURPLE_FIGHTER4,    SPRITE_SOURCE_NAJU,        267, 148,   9,   9, },
    { SPRITE_NAJU_PURPLE_POWER_CORE,  SPRITE_SOURCE_NAJU,        286, 150,   4,   4, },
    { SPRITE_NAJU_PURPLE_MISSILE,     SPRITE_SOURCE_NAJU,        300, 148,   8,   8, },

    { SPRITE_NAJU_GRAY_BASE,          SPRITE_SOURCE_NAJU,        358,  34,  99,  99, },
    { SPRITE_NAJU_GRAY_FIGHTER1,      SPRITE_SOURCE_NAJU,        379, 148,   9,   9, },
    { SPRITE_NAJU_GRAY_FIGHTER2,      SPRITE_SOURCE_NAJU,        395, 148,   9,   9, },
    { SPRITE_NAJU_GRAY_FIGHTER3,      SPRITE_SOURCE_NAJU,        411, 148,   9,   9, },
    { SPRITE_NAJU_GRAY_FIGHTER4,      SPRITE_SOURCE_NAJU,        427, 148,   9,   9, },
    { SPRITE_NAJU_GRAY_POWER_CORE,    SPRITE_SOURCE_NAJU,        446, 150,   4,   4, },
    { SPRITE_NAJU_GRAY_MISSILE,       SPRITE_SOURCE_NAJU,        460, 148,   8,   8, },

    { SPRITE_NAJU_YELLOW_BASE,        SPRITE_SOURCE_NAJU,        518,  34,  99,  99, },
    { SPRITE_NAJU_YELLOW_FIGHTER1,    SPRITE_SOURCE_NAJU,        539, 148,   9,   9, },
    { SPRITE_NAJU_YELLOW_FIGHTER2,    SPRITE_SOURCE_NAJU,        555, 148,   9,   9, },
    { SPRITE_NAJU_YELLOW_FIGHTER3,    SPRITE_SOURCE_NAJU,        571, 148,   9,   9, },
    { SPRITE_NAJU_YELLOW_FIGHTER4,    SPRITE_SOURCE_NAJU,        587, 148,   9,   9, },
    { SPRITE_NAJU_YELLOW_POWER_CORE,  SPRITE_SOURCE_NAJU,        606, 150,   4,   4, },
    { SPRITE_NAJU_YELLOW_MISSILE,     SPRITE_SOURCE_NAJU,        620, 148,   8,   8, },

    { SPRITE_NAJU_GREEN_BASE,         SPRITE_SOURCE_NAJU,         38, 210,  99,  99, },
    { SPRITE_NAJU_GREEN_FIGHTER1,     SPRITE_SOURCE_NAJU,         59, 324,   9,   9, },
    { SPRITE_NAJU_GREEN_FIGHTER2,     SPRITE_SOURCE_NAJU,         75, 324,   9,   9, },
    { SPRITE_NAJU_GREEN_FIGHTER3,     SPRITE_SOURCE_NAJU,         91, 324,   9,   9, },
    { SPRITE_NAJU_GREEN_FIGHTER4,     SPRITE_SOURCE_NAJU,        107, 324,   9,   9, },
    { SPRITE_NAJU_GREEN_POWER_CORE,   SPRITE_SOURCE_NAJU,        126, 326,   4,   4, },
    { SPRITE_NAJU_GREEN_MISSILE,      SPRITE_SOURCE_NAJU,        140, 324,   8,   8, },

    { SPRITE_NAJU_RED_BASE,           SPRITE_SOURCE_NAJU,        198, 210,  99,  99, },
    { SPRITE_NAJU_RED_FIGHTER1,       SPRITE_SOURCE_NAJU,        219, 324,   9,   9, },
    { SPRITE_NAJU_RED_FIGHTER2,       SPRITE_SOURCE_NAJU,        235, 324,   9,   9, },
    { SPRITE_NAJU_RED_FIGHTER3,       SPRITE_SOURCE_NAJU,        251, 324,   9,   9, },
    { SPRITE_NAJU_RED_FIGHTER4,       SPRITE_SOURCE_NAJU,        267, 324,   9,   9, },
    { SPRITE_NAJU_RED_POWER_CORE,     SPRITE_SOURCE_NAJU,        286, 326,   4,   4, },
    { SPRITE_NAJU_RED_MISSILE,        SPRITE_SOURCE_NAJU,        300, 324,   8,   8, },

    { SPRITE_NAJU_BLUE2_BASE,         SPRITE_SOURCE_NAJU,        358, 210,  99,  99, },
    { SPRITE_NAJU_BLUE2_FIGHTER1,     SPRITE_SOURCE_NAJU,        379, 324,   9,   9, },
    { SPRITE_NAJU_BLUE2_FIGHTER2,     SPRITE_SOURCE_NAJU,        395, 324,   9,   9, },
    { SPRITE_NAJU_BLUE2_FIGHTER3,     SPRITE_SOURCE_NAJU,        411, 324,   9,   9, },
    { SPRITE_NAJU_BLUE2_FIGHTER4,     SPRITE_SOURCE_NAJU,        427, 324,   9,   9, },
    { SPRITE_NAJU_BLUE2_POWER_CORE,   SPRITE_SOURCE_NAJU,        446, 326,   4,   4, },
    { SPRITE_NAJU_BLUE2_MISSILE,      SPRITE_SOURCE_NAJU,        460, 324,   8,   8, },

    { SPRITE_NAJU_ORANGE_BASE,        SPRITE_SOURCE_NAJU,        518, 210,  99,  99, },
    { SPRITE_NAJU_ORANGE_FIGHTER1,    SPRITE_SOURCE_NAJU,        539, 324,   9,   9, },
    { SPRITE_NAJU_ORANGE_FIGHTER2,    SPRITE_SOURCE_NAJU,        555, 324,   9,   9, },
    { SPRITE_NAJU_ORANGE_FIGHTER3,    SPRITE_SOURCE_NAJU,        571, 324,   9,   9, },
    { SPRITE_NAJU_ORANGE_FIGHTER4,    SPRITE_SOURCE_NAJU,        587, 324,   9,   9, },
    { SPRITE_NAJU_ORANGE_POWER_CORE,  SPRITE_SOURCE_NAJU,        606, 326,   4,   4, },
    { SPRITE_NAJU_ORANGE_MISSILE,     SPRITE_SOURCE_NAJU,        620, 324,   8,   8, },

    { SPRITE_NAJU_TURQUOISE_BASE,     SPRITE_SOURCE_NAJU,         38, 386,  99,  99, },
    { SPRITE_NAJU_TURQUOISE_FIGHTER1, SPRITE_SOURCE_NAJU,         59, 500,   9,   9, },
    { SPRITE_NAJU_TURQUOISE_FIGHTER2, SPRITE_SOURCE_NAJU,         75, 500,   9,   9, },
    { SPRITE_NAJU_TURQUOISE_FIGHTER3, SPRITE_SOURCE_NAJU,         91, 500,   9,   9, },
    { SPRITE_NAJU_TURQUOISE_FIGHTER4, SPRITE_SOURCE_NAJU,        107, 500,   9,   9, },
    { SPRITE_NAJU_TURQUOISE_POWER_CORE,SPRITE_SOURCE_NAJU,       126, 502,   4,   4, },
    { SPRITE_NAJU_TURQUOISE_MISSILE,  SPRITE_SOURCE_NAJU,        140, 500,   8,   8, },

    { SPRITE_NAJU_PURPLE2_BASE,       SPRITE_SOURCE_NAJU,        198, 386,  99,  99, },
    { SPRITE_NAJU_PURPLE2_FIGHTER1,   SPRITE_SOURCE_NAJU,        219, 500,   9,   9, },
    { SPRITE_NAJU_PURPLE2_FIGHTER2,   SPRITE_SOURCE_NAJU,        235, 500,   9,   9, },
    { SPRITE_NAJU_PURPLE2_FIGHTER3,   SPRITE_SOURCE_NAJU,        251, 500,   9,   9, },
    { SPRITE_NAJU_PURPLE2_FIGHTER4,   SPRITE_SOURCE_NAJU,        267, 500,   9,   9, },
    { SPRITE_NAJU_PURPLE2_POWER_CORE, SPRITE_SOURCE_NAJU,        286, 502,   4,   4, },
    { SPRITE_NAJU_PURPLE2_MISSILE,    SPRITE_SOURCE_NAJU,        300, 500,   8,   8, },

    { SPRITE_NAJU_WHITE_BASE,         SPRITE_SOURCE_NAJU,        358, 386,  99,  99, },
    { SPRITE_NAJU_WHITE_FIGHTER1,     SPRITE_SOURCE_NAJU,        379, 500,   9,   9, },
    { SPRITE_NAJU_WHITE_FIGHTER2,     SPRITE_SOURCE_NAJU,        395, 500,   9,   9, },
    { SPRITE_NAJU_WHITE_FIGHTER3,     SPRITE_SOURCE_NAJU,        411, 500,   9,   9, },
    { SPRITE_NAJU_WHITE_FIGHTER4,     SPRITE_SOURCE_NAJU,        427, 500,   9,   9, },
    { SPRITE_NAJU_WHITE_POWER_CORE,   SPRITE_SOURCE_NAJU,        446, 502,   4,   4, },
    { SPRITE_NAJU_WHITE_MISSILE,      SPRITE_SOURCE_NAJU,        460, 500,   8,   8, },

    { SPRITE_NAJU_RED2_BASE,          SPRITE_SOURCE_NAJU,        518, 386,  99,  99, },
    { SPRITE_NAJU_RED2_FIGHTER1,      SPRITE_SOURCE_NAJU,        539, 500,   9,   9, },
    { SPRITE_NAJU_RED2_FIGHTER2,      SPRITE_SOURCE_NAJU,        555, 500,   9,   9, },
    { SPRITE_NAJU_RED2_FIGHTER3,      SPRITE_SOURCE_NAJU,        571, 500,   9,   9, },
    { SPRITE_NAJU_RED2_FIGHTER4,      SPRITE_SOURCE_NAJU,        587, 500,   9,   9, },
    { SPRITE_NAJU_RED2_POWER_CORE,    SPRITE_SOURCE_NAJU,        606, 502,   4,   4, },
    { SPRITE_NAJU_RED2_MISSILE,       SPRITE_SOURCE_NAJU,        620, 500,   8,   8, },

    { SPRITE_NAJU_YELLOW2_BASE,       SPRITE_SOURCE_NAJU,         38, 562,  99,  99, },
    { SPRITE_NAJU_YELLOW2_FIGHTER1,   SPRITE_SOURCE_NAJU,         59, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW2_FIGHTER2,   SPRITE_SOURCE_NAJU,         75, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW2_FIGHTER3,   SPRITE_SOURCE_NAJU,         91, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW2_FIGHTER4,   SPRITE_SOURCE_NAJU,        107, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW2_POWER_CORE, SPRITE_SOURCE_NAJU,        126, 678,   4,   4, },
    { SPRITE_NAJU_YELLOW2_MISSILE,    SPRITE_SOURCE_NAJU,        140, 676,   8,   8, },

    { SPRITE_NAJU_MAGENTA_BASE,       SPRITE_SOURCE_NAJU,        198, 562,  99,  99, },
    { SPRITE_NAJU_MAGENTA_FIGHTER1,   SPRITE_SOURCE_NAJU,        219, 676,   9,   9, },
    { SPRITE_NAJU_MAGENTA_FIGHTER2,   SPRITE_SOURCE_NAJU,        235, 676,   9,   9, },
    { SPRITE_NAJU_MAGENTA_FIGHTER3,   SPRITE_SOURCE_NAJU,        251, 676,   9,   9, },
    { SPRITE_NAJU_MAGENTA_FIGHTER4,   SPRITE_SOURCE_NAJU,        267, 676,   9,   9, },
    { SPRITE_NAJU_MAGENTA_POWER_CORE, SPRITE_SOURCE_NAJU,        286, 678,   4,   4, },
    { SPRITE_NAJU_MAGENTA_MISSILE,    SPRITE_SOURCE_NAJU,        300, 676,   8,   8, },

    { SPRITE_NAJU_ORANGE2_BASE,       SPRITE_SOURCE_NAJU,        358, 562,  99,  99, },
    { SPRITE_NAJU_ORANGE2_FIGHTER1,   SPRITE_SOURCE_NAJU,        379, 676,   9,   9, },
    { SPRITE_NAJU_ORANGE2_FIGHTER2,   SPRITE_SOURCE_NAJU,        395, 676,   9,   9, },
    { SPRITE_NAJU_ORANGE2_FIGHTER3,   SPRITE_SOURCE_NAJU,        411, 676,   9,   9, },
    { SPRITE_NAJU_ORANGE2_FIGHTER4,   SPRITE_SOURCE_NAJU,        427, 676,   9,   9, },
    { SPRITE_NAJU_ORANGE2_POWER_CORE, SPRITE_SOURCE_NAJU,        446, 678,   4,   4, },
    { SPRITE_NAJU_ORANGE2_MISSILE,    SPRITE_SOURCE_NAJU,        460, 676,   8,   8, },

    { SPRITE_NAJU_YELLOW3_BASE,       SPRITE_SOURCE_NAJU,        518, 562,  99,  99, },
    { SPRITE_NAJU_YELLOW3_FIGHTER1,   SPRITE_SOURCE_NAJU,        539, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW3_FIGHTER2,   SPRITE_SOURCE_NAJU,        555, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW3_FIGHTER3,   SPRITE_SOURCE_NAJU,        571, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW3_FIGHTER4,   SPRITE_SOURCE_NAJU,        587, 676,   9,   9, },
    { SPRITE_NAJU_YELLOW3_POWER_CORE, SPRITE_SOURCE_NAJU,        606, 678,   4,   4, },
    { SPRITE_NAJU_YELLOW3_MISSILE,    SPRITE_SOURCE_NAJU,        620, 676,   8,   8, },

    SPRITE_ALTAIR(PURPLE,   0),
    SPRITE_ALTAIR(PURPLE2,  1),
    SPRITE_ALTAIR(YELLOW,   2),
    SPRITE_ALTAIR(RED2,     3),
    SPRITE_ALTAIR(GREEN,    4),
    SPRITE_ALTAIR(GREEN2,   5),
    SPRITE_ALTAIR(BLUE,     6),
    SPRITE_ALTAIR(BLUE2,    7),
    SPRITE_ALTAIR(MAGENTA,  8),
    SPRITE_ALTAIR(RED,      9),
    SPRITE_ALTAIR(RED3,    10),
    SPRITE_ALTAIR(GREEN3,  11),
    SPRITE_ALTAIR(ORANGE,  12),
    SPRITE_ALTAIR(ORANGE2, 13),
    SPRITE_ALTAIR(YELLOW2, 14),
    SPRITE_ALTAIR(BLUE3,   15),

    SPRITE_URSA(BLUE,     0),
    SPRITE_URSA(BLUE2,    1),
    SPRITE_URSA(PURPLE,   2),
    SPRITE_URSA(PURPLE2,  3),
    SPRITE_URSA(PINK,     4),
    SPRITE_URSA(PINK2,    5),
    SPRITE_URSA(ORANGE,   6),
    SPRITE_URSA(GREEN,    7),
    SPRITE_URSA(GREEN2,   8),
    SPRITE_URSA(GREEN3,   9),
    SPRITE_URSA(BLUE3,   10),
    SPRITE_URSA(MAGENTA, 11),
    SPRITE_URSA(RED,     12),
    SPRITE_URSA(ORANGE2, 13),
    SPRITE_URSA(GREEN4,  14),
    SPRITE_URSA(ORANGE3, 15),

    SPRITE_VEGA(BLUE,     0),
    SPRITE_VEGA(ORANGE,   1),
    SPRITE_VEGA(PURPLE,   2),
    SPRITE_VEGA(GREEN,    3),
    SPRITE_VEGA(ORANGE2,  4),
    SPRITE_VEGA(ORANGE3,  5),
    SPRITE_VEGA(PURPLE2,  6),
    SPRITE_VEGA(BROWN,    7),
    SPRITE_VEGA(BLUE2,    8),
    SPRITE_VEGA(GREEN2,   9),
    SPRITE_VEGA(RED,     10),
    SPRITE_VEGA(ORANGE4, 11),
    SPRITE_VEGA(BLACK,   12),
    SPRITE_VEGA(GREY,    13),
    SPRITE_VEGA(YELLOW,  14),
    SPRITE_VEGA(RED2,    15),

    SPRITE_ARANEA(BLUE,     0),
    SPRITE_ARANEA(ORANGE,   1),
    SPRITE_ARANEA(GREY,     2),
    SPRITE_ARANEA(BROWN,    3),
    SPRITE_ARANEA(YELLOW,   4),
    SPRITE_ARANEA(RED,      5),
    SPRITE_ARANEA(GREEN,    6),
    SPRITE_ARANEA(YELLOW2,  7),
    SPRITE_ARANEA(BLACK,    8),
    SPRITE_ARANEA(WHITE,    9),
    SPRITE_ARANEA(BROWN2,  10),
    SPRITE_ARANEA(BLUE2,   11),
    SPRITE_ARANEA(GREY2,   12),
    SPRITE_ARANEA(BLACK2,  13),
    SPRITE_ARANEA(RED2,    14),
    SPRITE_ARANEA(GREEN2,  15),
};

static const struct {
    FleetAIType aiType;
    SpriteSet spriteSet;
    uint32 color;
} spriteFleetTable[] = {
        //{ FLEET_AI_INVALID,     SPRITE_SET_SPACE_WHITE,   0x000000, }, // 0x(AA)RRGGBB
        { FLEET_AI_NEUTRAL,     SPRITE_SET_SPACE_WHITE,     0x888888, }, // GRAY
        { FLEET_AI_DUMMY,       SPRITE_SET_NAJU_WHITE,      0xFFFFFF, }, // WHITE
        { FLEET_AI_SIMPLE,      SPRITE_SET_NAJU_RED,        0xFF0000, }, // RED
        { FLEET_AI_GATHER,      SPRITE_SET_NAJU_GREEN,      0x00FF00, }, // GREEN
        { FLEET_AI_CLOUD,       SPRITE_SET_NAJU_BLUE,       0x0000FF, }, // BLUE
        { FLEET_AI_MAPPER,      SPRITE_SET_NAJU_YELLOW,     0x808000, }, // YELLOW
        { FLEET_AI_CIRCLE,      SPRITE_SET_NAJU_BLUE2,      0x048488, }, // TEAL-ish
        { FLEET_AI_RUNAWAY,     SPRITE_SET_NAJU_PURPLE2,    0x800080, }, // PURPLE
        { FLEET_AI_COWARD,      SPRITE_SET_NAJU_TURQUOISE,  0x008080, }, // TEAL
        { FLEET_AI_BASIC,       SPRITE_SET_NAJU_YELLOW3,    0x808080, }, // DARK GRAY
        { FLEET_AI_HOLD,        SPRITE_SET_NAJU_PURPLE,     0xF00080, }, // PURPLE
        { FLEET_AI_META,        SPRITE_SET_SPACE_BLUE,      0x80F080, }, // GREENISH-YELLOW
        { FLEET_AI_FLOCK1,      SPRITE_SET_SPACE_RED,       0xFF3322, }, // YELLOWISH-GRAY?
        { FLEET_AI_FLOCK2,      SPRITE_SET_SPACE_RED2,      0xFF3388, }, // BLUEISH-GRAY?
        { FLEET_AI_FLOCK3,      SPRITE_SET_SPACE_PURPLE2,   0xFF3366, }, // GRAYISH?
        { FLEET_AI_FLOCK4,      SPRITE_SET_SPACE_PURPLE,    0xFFFF66, }, // GRAYISH?
        { FLEET_AI_FLOCK5,      SPRITE_SET_SPACE_PURPLE3,   0x2FFF66, }, // GRAYISH?
        { FLEET_AI_FLOCK6,      SPRITE_SET_SPACE_RED3,      0xFF2244, }, // YELLOWISH-GRAY?
        { FLEET_AI_FLOCK7,      SPRITE_SET_SPACE_GREEN,     0x00FF66, }, // BLUEISH-GRAY?
        { FLEET_AI_FLOCK8,      SPRITE_SET_SPACE_GREEN2,    0x101FFF, }, // GRAYISH?
        { FLEET_AI_FLOCK9,      SPRITE_SET_SPACE_GREEN3,    0x123FFF, }, // GRAYISH?
        { FLEET_AI_BUNDLE1,     SPRITE_SET_ALTAIR_PURPLE,   0x883333, },
        { FLEET_AI_BUNDLE2,     SPRITE_SET_ALTAIR_PURPLE2,  0x448333, },
        { FLEET_AI_BUNDLE3,     SPRITE_SET_ALTAIR_YELLOW,   0x443383, },
        { FLEET_AI_BUNDLE4,     SPRITE_SET_ALTAIR_YELLOW2,  0x228383, },
        { FLEET_AI_BUNDLE5,     SPRITE_SET_ALTAIR_RED2,     0x189333, },
        { FLEET_AI_BUNDLE6,     SPRITE_SET_ALTAIR_RED,      0x1833A3, },
        { FLEET_AI_BUNDLE7,     SPRITE_SET_ALTAIR_RED3,     0xA83333, },
        { FLEET_AI_BUNDLE8,     SPRITE_SET_ALTAIR_GREEN,    0xA8F3F3, },
        { FLEET_AI_BUNDLE9,     SPRITE_SET_ALTAIR_GREEN2,   0xC80303, }, // DARK PURPLE
        { FLEET_AI_BUNDLE10,    SPRITE_SET_ALTAIR_GREEN3,   0xA803B3, },
        { FLEET_AI_BUNDLE11,    SPRITE_SET_ALTAIR_BLUE,     0x48B303, },
        { FLEET_AI_BUNDLE12,    SPRITE_SET_ALTAIR_BLUE2,    0xF000F0, }, // DARK PURPLE
        { FLEET_AI_BUNDLE13,    SPRITE_SET_ALTAIR_MAGENTA,  0xF0F020, },
        { FLEET_AI_BUNDLE14,    SPRITE_SET_ALTAIR_ORANGE,   0xF88144, }, // PURPLE
        { FLEET_AI_BUNDLE15,    SPRITE_SET_ALTAIR_ORANGE2,  0xF881F4, },
        { FLEET_AI_BUNDLE16,    SPRITE_SET_ALTAIR_BLUE3,    0x180144, },
        { FLEET_AI_NEURAL1,     SPRITE_SET_URSA_BLUE,       0x4488FF, }, // GRAYISH-BLUE
        { FLEET_AI_NEURAL2,     SPRITE_SET_URSA_PURPLE,     0xFF883F, },
        { FLEET_AI_NEURAL3,     SPRITE_SET_URSA_PURPLE,     0xBEBD7F, },
        { FLEET_AI_NEURAL4,     SPRITE_SET_URSA_ORANGE,     0x587246, },
        { FLEET_AI_NEURAL5,     SPRITE_SET_URSA_GREEN,      0x308446, },
        { FLEET_AI_NEURAL6,     SPRITE_SET_URSA_MAGENTA,    0x474A51, },
        { FLEET_AI_NEURAL7,     SPRITE_SET_URSA_RED,        0xB32428, },
        { FLEET_AI_NEURAL8,     SPRITE_SET_URSA_BLUE2,      0xEDFF21, },
        { FLEET_AI_NEURAL9,     SPRITE_SET_URSA_PURPLE2,    0x47402E, },
        { FLEET_AI_NEURAL10,    SPRITE_SET_URSA_PINK2,      0x063971, },
        { FLEET_AI_NEURAL11,    SPRITE_SET_URSA_GREEN2,     0x6C4675, },
        { FLEET_AI_NEURAL12,    SPRITE_SET_URSA_ORANGE2,    0x3B3C36, },
        { FLEET_AI_NEURAL13,    SPRITE_SET_URSA_PINK,       0x5B3A29, },
        { FLEET_AI_NEURAL14,    SPRITE_SET_VEGA_ORANGE,     0xA03472, },
        { FLEET_AI_BINEURAL1,   SPRITE_SET_URSA_GREEN3,     0x434B4D, },
        { FLEET_AI_BINEURAL2,   SPRITE_SET_URSA_BLUE3,      0x606E8C, },
        { FLEET_AI_BINEURAL3,   SPRITE_SET_URSA_GREEN4,     0x9B111E, },
        { FLEET_AI_BINEURAL4,   SPRITE_SET_URSA_ORANGE3,    0xEFA94A, },
        { FLEET_AI_BINEURAL5,   SPRITE_SET_URSA_PINK,       0x5D9B9B, },
        { FLEET_AI_MATRIX1,     SPRITE_SET_VEGA_BLUE,       0xADCF02, },
};

static const struct {
    SpriteSet ss;
    SpriteType base;
} spriteMappingTable[] = {
        { SPRITE_SET_INVALID,       SPRITE_INVALID, },

        { SPRITE_SET_SPACE_BLUE,    SPRITE_SPACE_BLUE_BASE, },
        { SPRITE_SET_SPACE_PURPLE,  SPRITE_SPACE_PURPLE_BASE, },
        { SPRITE_SET_SPACE_GREEN,   SPRITE_SPACE_GREEN_BASE, },
        { SPRITE_SET_SPACE_GREEN2,  SPRITE_SPACE_GREEN2_BASE, },
        { SPRITE_SET_SPACE_GREEN3,  SPRITE_SPACE_GREEN3_BASE, },
        { SPRITE_SET_SPACE_YELLOW,  SPRITE_SPACE_YELLOW_BASE, },
        { SPRITE_SET_SPACE_ORANGE,  SPRITE_SPACE_ORANGE_BASE, },
        { SPRITE_SET_SPACE_RED,     SPRITE_SPACE_RED_BASE, },
        { SPRITE_SET_SPACE_PURPLE2, SPRITE_SPACE_PURPLE2_BASE, },
        { SPRITE_SET_SPACE_RED2,    SPRITE_SPACE_RED2_BASE, },
        { SPRITE_SET_SPACE_WHITE,   SPRITE_SPACE_WHITE_BASE, },
        { SPRITE_SET_SPACE_YELLOW2, SPRITE_SPACE_YELLOW2_BASE, },
        { SPRITE_SET_SPACE_BROWN,   SPRITE_SPACE_BROWN_BASE, },
        { SPRITE_SET_SPACE_RED3,    SPRITE_SPACE_RED3_BASE, },
        { SPRITE_SET_SPACE_PURPLE3, SPRITE_SPACE_PURPLE3_BASE, },

        { SPRITE_SET_NAJU_BLUE,     SPRITE_NAJU_BLUE_BASE, },
        { SPRITE_SET_NAJU_PURPLE,   SPRITE_NAJU_PURPLE_BASE, },
        { SPRITE_SET_NAJU_GRAY,     SPRITE_NAJU_GRAY_BASE, },
        { SPRITE_SET_NAJU_YELLOW,   SPRITE_NAJU_YELLOW_BASE, },
        { SPRITE_SET_NAJU_GREEN,    SPRITE_NAJU_GREEN_BASE, },
        { SPRITE_SET_NAJU_RED,      SPRITE_NAJU_RED_BASE, },
        { SPRITE_SET_NAJU_BLUE2,    SPRITE_NAJU_BLUE2_BASE, },
        { SPRITE_SET_NAJU_ORANGE,   SPRITE_NAJU_ORANGE_BASE, },
        { SPRITE_SET_NAJU_TURQUOISE,SPRITE_NAJU_TURQUOISE_BASE, },
        { SPRITE_SET_NAJU_PURPLE2,  SPRITE_NAJU_PURPLE2_BASE, },
        { SPRITE_SET_NAJU_WHITE,    SPRITE_NAJU_WHITE_BASE, },
        { SPRITE_SET_NAJU_RED2,     SPRITE_NAJU_RED2_BASE, },
        { SPRITE_SET_NAJU_YELLOW2,  SPRITE_NAJU_YELLOW2_BASE, },
        { SPRITE_SET_NAJU_MAGENTA,  SPRITE_NAJU_MAGENTA_BASE, },
        { SPRITE_SET_NAJU_ORANGE2,  SPRITE_NAJU_ORANGE2_BASE, },
        { SPRITE_SET_NAJU_YELLOW3,  SPRITE_NAJU_YELLOW3_BASE, },

        { SPRITE_SET_ALTAIR_PURPLE, SPRITE_ALTAIR_PURPLE_BASE, },
        { SPRITE_SET_ALTAIR_PURPLE2,SPRITE_ALTAIR_PURPLE2_BASE, },
        { SPRITE_SET_ALTAIR_YELLOW, SPRITE_ALTAIR_YELLOW_BASE, },
        { SPRITE_SET_ALTAIR_RED2,   SPRITE_ALTAIR_RED2_BASE, },
        { SPRITE_SET_ALTAIR_GREEN,  SPRITE_ALTAIR_GREEN_BASE, },
        { SPRITE_SET_ALTAIR_GREEN2, SPRITE_ALTAIR_GREEN2_BASE, },
        { SPRITE_SET_ALTAIR_BLUE,   SPRITE_ALTAIR_BLUE_BASE, },
        { SPRITE_SET_ALTAIR_BLUE2,  SPRITE_ALTAIR_BLUE2_BASE, },
        { SPRITE_SET_ALTAIR_MAGENTA,SPRITE_ALTAIR_MAGENTA_BASE, },
        { SPRITE_SET_ALTAIR_RED,    SPRITE_ALTAIR_RED_BASE, },
        { SPRITE_SET_ALTAIR_RED3,   SPRITE_ALTAIR_RED3_BASE, },
        { SPRITE_SET_ALTAIR_GREEN3, SPRITE_ALTAIR_GREEN3_BASE, },
        { SPRITE_SET_ALTAIR_ORANGE, SPRITE_ALTAIR_ORANGE_BASE, },
        { SPRITE_SET_ALTAIR_ORANGE2,SPRITE_ALTAIR_ORANGE2_BASE, },
        { SPRITE_SET_ALTAIR_YELLOW2,SPRITE_ALTAIR_YELLOW2_BASE, },
        { SPRITE_SET_ALTAIR_BLUE3,  SPRITE_ALTAIR_BLUE3_BASE, },

        { SPRITE_SET_URSA_BLUE,     SPRITE_URSA_BLUE_BASE, },
        { SPRITE_SET_URSA_BLUE2,    SPRITE_URSA_BLUE2_BASE, },
        { SPRITE_SET_URSA_PURPLE,   SPRITE_URSA_PURPLE_BASE, },
        { SPRITE_SET_URSA_PURPLE2,  SPRITE_URSA_PURPLE2_BASE, },
        { SPRITE_SET_URSA_PINK,     SPRITE_URSA_PINK_BASE, },
        { SPRITE_SET_URSA_PINK2,    SPRITE_URSA_PINK2_BASE, },
        { SPRITE_SET_URSA_ORANGE,   SPRITE_URSA_ORANGE_BASE, },
        { SPRITE_SET_URSA_GREEN,    SPRITE_URSA_GREEN_BASE, },
        { SPRITE_SET_URSA_GREEN2,   SPRITE_URSA_GREEN2_BASE, },
        { SPRITE_SET_URSA_GREEN3,   SPRITE_URSA_GREEN3_BASE, },
        { SPRITE_SET_URSA_BLUE3,    SPRITE_URSA_BLUE3_BASE, },
        { SPRITE_SET_URSA_MAGENTA,  SPRITE_URSA_MAGENTA_BASE, },
        { SPRITE_SET_URSA_RED,      SPRITE_URSA_RED_BASE, },
        { SPRITE_SET_URSA_ORANGE2,  SPRITE_URSA_ORANGE2_BASE, },
        { SPRITE_SET_URSA_GREEN4,   SPRITE_URSA_GREEN4_BASE, },
        { SPRITE_SET_URSA_ORANGE3,  SPRITE_URSA_ORANGE3_BASE, },

        { SPRITE_SET_VEGA_BLUE,     SPRITE_VEGA_BLUE_BASE, },
        { SPRITE_SET_VEGA_BLUE2,    SPRITE_VEGA_BLUE2_BASE, },
        { SPRITE_SET_VEGA_ORANGE,   SPRITE_VEGA_ORANGE_BASE, },
        { SPRITE_SET_VEGA_ORANGE2,  SPRITE_VEGA_ORANGE2_BASE, },
        { SPRITE_SET_VEGA_ORANGE3,  SPRITE_VEGA_ORANGE3_BASE, },
        { SPRITE_SET_VEGA_ORANGE4,  SPRITE_VEGA_ORANGE4_BASE, },
        { SPRITE_SET_VEGA_PURPLE,   SPRITE_VEGA_PURPLE_BASE, },
        { SPRITE_SET_VEGA_PURPLE2,  SPRITE_VEGA_PURPLE2_BASE, },
        { SPRITE_SET_VEGA_GREEN,    SPRITE_VEGA_GREEN_BASE, },
        { SPRITE_SET_VEGA_GREEN2,   SPRITE_VEGA_GREEN2_BASE, },
        { SPRITE_SET_VEGA_BROWN,    SPRITE_VEGA_BROWN_BASE, },
        { SPRITE_SET_VEGA_RED,      SPRITE_VEGA_RED_BASE, },
        { SPRITE_SET_VEGA_RED2,     SPRITE_VEGA_RED2_BASE, },
        { SPRITE_SET_VEGA_BLACK,    SPRITE_VEGA_BLACK_BASE, },
        { SPRITE_SET_VEGA_YELLOW,   SPRITE_VEGA_YELLOW_BASE, },
        { SPRITE_SET_VEGA_GREY,     SPRITE_VEGA_GREY_BASE, },

        { SPRITE_SET_ARANEA_BLUE,   SPRITE_ARANEA_BLUE_BASE, },
        { SPRITE_SET_ARANEA_ORANGE, SPRITE_ARANEA_ORANGE_BASE, },
        { SPRITE_SET_ARANEA_GREY,   SPRITE_ARANEA_GREY_BASE, },
        { SPRITE_SET_ARANEA_BROWN,  SPRITE_ARANEA_BROWN_BASE, },
        { SPRITE_SET_ARANEA_YELLOW, SPRITE_ARANEA_YELLOW_BASE, },
        { SPRITE_SET_ARANEA_RED,    SPRITE_ARANEA_RED_BASE, },
        { SPRITE_SET_ARANEA_GREEN,  SPRITE_ARANEA_GREEN_BASE, },
        { SPRITE_SET_ARANEA_YELLOW2,SPRITE_ARANEA_YELLOW2_BASE, },
        { SPRITE_SET_ARANEA_BLACK,  SPRITE_ARANEA_BLACK_BASE, },
        { SPRITE_SET_ARANEA_WHITE,  SPRITE_ARANEA_WHITE_BASE, },
        { SPRITE_SET_ARANEA_BROWN2, SPRITE_ARANEA_BROWN2_BASE, },
        { SPRITE_SET_ARANEA_BLUE2,  SPRITE_ARANEA_BLUE2_BASE, },
        { SPRITE_SET_ARANEA_GREY2,  SPRITE_ARANEA_GREY2_BASE, },
        { SPRITE_SET_ARANEA_BLACK2, SPRITE_ARANEA_BLACK2_BASE, },
        { SPRITE_SET_ARANEA_RED2,   SPRITE_ARANEA_RED2_BASE, },
        { SPRITE_SET_ARANEA_GREEN2, SPRITE_ARANEA_GREEN2_BASE, },
};

typedef struct SpriteBacking {
    uint32 refCount;

    bool active;

    SDL_Surface *sdlSurface;

    /*
     * Textures in SDL are tied to a renderer, so the Sprite module
     * creates them on demand as the blit calls come in.
     */
    SDL_Texture *sdlTexture;
    SDL_Renderer *sdlRenderer;
} SpriteBacking;

typedef struct SpriteGlobalData {
    SDL_Surface *sources[SPRITE_SOURCE_MAX];

    uint32 numBacking;
    SpriteBacking backing[1000];
} SpriteGlobalData;

SpriteGlobalData gSprite;

static void SpriteCalcMobSheetSize(uint32 *sheetWidth,
                                   uint32 *sheetHeight);
static SpriteType SpriteGetMobSpriteType(MobType t, FleetAIType aiType,
                                         uint32 repeatCount);
static void SpriteCalcMobSpriteRect(MobType mobType, SDL_Rect *rect);

static SpriteBacking *SpriteGetBacking(uint32 backingID);
static SpriteBacking *SpriteAllocBacking(uint32 *backingID);
static void SpriteFreeBacking(uint32 backingID);
static void SpriteAcquireBacking(uint32 backingID);
static void SpriteReleaseBacking(uint32 backingID);


void Sprite_Init()
{
    ASSERT(MBUtil_IsZero(&gSprite, sizeof(gSprite)));

    ASSERT(ARRAYSIZE(gSprite.sources) == SPRITE_SOURCE_MAX);
    ASSERT(SPRITE_SOURCE_MAX == 6);
    gSprite.sources[SPRITE_SOURCE_SPACE] = Sprite_LoadPNG("art/space.png", 516, 412);
    gSprite.sources[SPRITE_SOURCE_NAJU]  = Sprite_LoadPNG("art/naju.png",  656, 720);
    gSprite.sources[SPRITE_SOURCE_ALTAIR] = Sprite_LoadPNG("art/altair.png", 516, 412);
    gSprite.sources[SPRITE_SOURCE_URSA] = Sprite_LoadPNG("art/ursa.png", 516, 412);
    gSprite.sources[SPRITE_SOURCE_VEGA] = Sprite_LoadPNG("art/vega.png", 516, 412);
    gSprite.sources[SPRITE_SOURCE_ARANEA] = Sprite_LoadPNG("art/aranea.png", 516, 412);

    for (int x = 0; x < ARRAYSIZE(gSprite.sources); x++) {
        uint32 backingID;
        SpriteBacking *backing;

        VERIFY(gSprite.sources[x] != NULL);

        backing = SpriteAllocBacking(&backingID);
        ASSERT(backingID == x);
        backing->sdlSurface = gSprite.sources[x];
    }
}

void Sprite_Exit()
{
    for (int x = 0; x < ARRAYSIZE(gSprite.sources); x++) {
        SpriteReleaseBacking(x);

        // The surface was freed by the backing.
        gSprite.sources[x] = NULL;
    }

    for (int x = 0; x < ARRAYSIZE(gSprite.backing); x++) {
        ASSERT(gSprite.backing[x].refCount == 0);
        ASSERT(!gSprite.backing[x].active);
    }
}

static SpriteBacking *SpriteGetBacking(uint32 backingID)
{
    SpriteBacking *backing = &gSprite.backing[backingID];
    ASSERT(gSprite.numBacking < ARRAYSIZE(gSprite.backing));
    ASSERT(backing->active);
    ASSERT(backing->refCount > 0);
    return backing;
}

static SpriteBacking *SpriteAllocBacking(uint32 *pBackingID)
{
    uint32 backingID;
    ASSERT(pBackingID != NULL);
    ASSERT(gSprite.numBacking < ARRAYSIZE(gSprite.backing));

    backingID = gSprite.numBacking;
    gSprite.numBacking++;

    ASSERT(!gSprite.backing[backingID].active);
    gSprite.backing[backingID].active = TRUE;

    ASSERT(gSprite.backing[backingID].refCount == 0);
    SpriteAcquireBacking(backingID);

    *pBackingID = backingID;
    return &gSprite.backing[backingID];
}

static void SpriteFreeBacking(uint32 backingID)
{
    ASSERT(backingID < ARRAYSIZE(gSprite.backing));
    ASSERT(backingID < gSprite.numBacking);
    ASSERT(gSprite.backing[backingID].active);
    ASSERT(gSprite.backing[backingID].refCount == 0);

    if (gSprite.backing[backingID].sdlTexture != NULL) {
        SDL_DestroyTexture(gSprite.backing[backingID].sdlTexture);
    }
    ASSERT(gSprite.backing[backingID].sdlSurface != NULL);
    SDL_FreeSurface(gSprite.backing[backingID].sdlSurface);

    MBUtil_Zero(&gSprite.backing[backingID],
                sizeof(gSprite.backing[backingID]));
}

static void SpriteAcquireBacking(uint32 backingID)
{
    ASSERT(backingID < ARRAYSIZE(gSprite.backing));
    ASSERT(backingID < gSprite.numBacking);
    ASSERT(gSprite.backing[backingID].active);
    gSprite.backing[backingID].refCount++;
}

static void SpriteReleaseBacking(uint32 backingID)
{
    ASSERT(backingID < ARRAYSIZE(gSprite.backing));
    ASSERT(backingID < gSprite.numBacking);
    ASSERT(gSprite.backing[backingID].active);
    ASSERT(gSprite.backing[backingID].refCount > 0);
    gSprite.backing[backingID].refCount--;

    if (gSprite.backing[backingID].refCount == 0) {
        SpriteFreeBacking(backingID);
    }
}

Sprite *Sprite_CreateCircle(uint32 radius, uint32 bgraColor)
{
    Sprite *sprite;
    SpriteBacking *backing;
    uint32 d = 2 * radius + 1;
    SDL_Point cPoint;

    sprite = MBUtil_ZAlloc(sizeof(*sprite));

    backing = SpriteAllocBacking(&sprite->backingID);

    sprite->srcx = 0;
    sprite->srcy = 0;
    sprite->w = d;
    sprite->h = d;

    ASSERT(sprite != NULL);
    backing->sdlSurface = SDL_CreateRGBSurfaceWithFormat(0, d, d, 32,
                                                         SDL_PIXELFORMAT_BGRA32);
    cPoint.x = d / 2;
    cPoint.y = d / 2;
    Sprite_DrawCircle(backing->sdlSurface, bgraColor, &cPoint, radius);

    ASSERT(backing->sdlRenderer == NULL);
    ASSERT(backing->sdlTexture == NULL);

    return sprite;
}

void Sprite_DrawCircle(SDL_Surface *sdlSurface, uint32 color,
                       const SDL_Point *center, int radius)
{
    uint8 *pixels;

    int minX = MAX(0, center->x - radius);
    int maxX = MIN(sdlSurface->w, center->x + radius + 1);
    int minY = MAX(0, center->y - radius);
    int maxY = MIN(sdlSurface->h, center->y + radius + 1);

    SDL_LockSurface(sdlSurface);
    pixels = (uint8 *)sdlSurface->pixels;
    pixels += sdlSurface->pitch * minY;

    for (int y = minY; y < maxY; y++) {
        uint32 *row = (uint32 *)pixels;
        int dy = abs(y - center->y);
        for (int x = minX; x < maxX; x++) {
            int dx = abs(x - center->x);
            if (dx * dx + dy * dy <= radius * radius) {
                row[x] = color;
            }
        }
        pixels += sdlSurface->pitch;
    }

    SDL_UnlockSurface(sdlSurface);
}

Sprite *Sprite_CreateType(SpriteType t)
{
    Sprite *sprite;
    SpriteSource source;

    ASSERT(t < SPRITE_TYPE_MAX);
    ASSERT(t < ARRAYSIZE(gSpecs));
    ASSERT(gSpecs[t].type == t);

    sprite = MBUtil_ZAlloc(sizeof(*sprite));

    source = gSpecs[t].source;
    SpriteAcquireBacking(source);
    sprite->backingID = source;
    ASSERT(source < ARRAYSIZE(gSprite.sources));

    sprite->srcx = gSpecs[t].x;
    sprite->srcy = gSpecs[t].y;
    sprite->w = gSpecs[t].w;
    sprite->h = gSpecs[t].h;

    return sprite;
}


Sprite *Sprite_CreateMob(MobType t, FleetAIType aiType, uint32 repeatCount)
{
    SpriteType sType = SpriteGetMobSpriteType(t, aiType, repeatCount);

    if (sType != SPRITE_INVALID) {
        return Sprite_CreateType(sType);
    } else {
        // XXX: Very inefficient!
        uint32 color = Sprite_GetColor(aiType, repeatCount);
        SDL_Surface *mobSheet = Sprite_CreateMobSheet(color);
        Sprite *sprite = Sprite_CreateFromMobSheet(t, mobSheet);
        SDL_FreeSurface(mobSheet);
        return sprite;
    }
}

static SpriteType SpriteGetMobSpriteTypeFromSet(MobType t,
                                                SpriteSet ss)
{
    SpriteType st = SPRITE_INVALID;

    for (uint i = 0; i < ARRAYSIZE(spriteMappingTable); i++) {
        if (spriteMappingTable[i].ss == ss) {
            st = spriteMappingTable[i].base;
            break;
        }
    }

    if (st == SPRITE_INVALID) {
        return SPRITE_INVALID;
    }

    if (ss >= SPRITE_SET_SPACE_BLUE &&
        ss <= SPRITE_SET_SPACE_PURPLE3) {
        ASSERT(MOB_TYPE_BASE == 1);
        ASSERT(MOB_TYPE_FIGHTER == 2);
        ASSERT(MOB_TYPE_MISSILE == 3);
        ASSERT(MOB_TYPE_POWER_CORE == 4);
        ASSERT(t >= 1 && t <= 4);
        return st + (t - 1);
    }

    if (ss >= SPRITE_SET_NAJU_BLUE &&
        ss <= SPRITE_SET_NAJU_YELLOW3) {
        if (t == MOB_TYPE_BASE) {
            return st;
        } else if (t == MOB_TYPE_FIGHTER) {
            return st + Random_Int(1, 4);
        } else if (t == MOB_TYPE_POWER_CORE) {
            return st + 5;
        } else if (t == MOB_TYPE_MISSILE) {
            return st + 6;
        }
    }

    if ((ss >= SPRITE_SET_ALTAIR_PURPLE && ss <= SPRITE_SET_ALTAIR_BLUE3) ||
        (ss >= SPRITE_SET_URSA_BLUE && ss <= SPRITE_SET_URSA_ORANGE3) ||
        (ss >= SPRITE_SET_VEGA_BLUE && ss <= SPRITE_SET_VEGA_GREY) ||
        (ss >= SPRITE_SET_ARANEA_BLUE && ss <= SPRITE_SET_ARANEA_GREEN2)) {
        ASSERT(MOB_TYPE_BASE == 1);
        ASSERT(MOB_TYPE_FIGHTER == 2);
        ASSERT(MOB_TYPE_MISSILE == 3);
        ASSERT(MOB_TYPE_POWER_CORE == 4);
        ASSERT(t >= 1 && t <= 4);
        return st + (t - 1);
    }

    PANIC("Unknown Sprite Requested!\n");
}

static SpriteType SpriteGetMobSpriteType(MobType t,
                                         FleetAIType aiType,
                                         uint32 repeatCount)
{
    // The first instance has a repeatCount of 1.
    ASSERT(repeatCount > 0);
    if (repeatCount > 1) {
        return SPRITE_INVALID;
    }

    for (uint i = 0; i < ARRAYSIZE(spriteFleetTable); i++) {
        if (spriteFleetTable[i].aiType == aiType) {
            SpriteSet set = spriteFleetTable[i].spriteSet;
            return SpriteGetMobSpriteTypeFromSet(t, set);
        }
    }

    return SPRITE_INVALID;
}

void Sprite_Free(Sprite *s)
{
    if (s != NULL) {
        SpriteReleaseBacking(s->backingID);
        free(s);
    }
}


void Sprite_Blit(Sprite *sprite, SDL_Renderer *r, uint32 x, uint32 y)
{
    SDL_Rect srcRect;
    SDL_Rect destRect;
    SpriteBacking *backing = SpriteGetBacking(sprite->backingID);

    Sprite_PrepareTexture(sprite, r);

    srcRect.x = sprite->srcx;
    srcRect.y = sprite->srcy;
    srcRect.w = sprite->w;
    srcRect.h = sprite->h;

    destRect.x = x;
    destRect.y = y;
    destRect.w = sprite->w;
    destRect.h = sprite->h;

    SDL_RenderCopy(r, backing->sdlTexture, &srcRect, &destRect);
}

void Sprite_BlitCentered(Sprite *sprite, SDL_Renderer *r, uint32 cx, uint32 cy)
{
    uint32 dx = cx - (sprite->w / 2);
    uint32 dy = cy - (sprite->h / 2);
    Sprite_Blit(sprite, r, dx, dy);
}

SDL_Surface *Sprite_LoadPNG(const char *fileName,
                            uint32 expectedWidth,
                            uint32 expectedHeight)
{
    SDL_Surface *sdlSurface;

    uint32 pngWidth, pngHeight;
    int colorType;
    int bitDepth;
    int interlaceType;
    int compressionMethod;
    int filterMethod;
    int numPasses;

    FILE *fp = fopen(fileName, "rb");
    VERIFY(fp != NULL);

    png_struct *pngPtr;
    pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    VERIFY(pngPtr != NULL);

    png_info *infoPtr;
    infoPtr = png_create_info_struct(pngPtr);
    VERIFY(infoPtr != NULL);

    if (setjmp(png_jmpbuf(pngPtr))) {
        PANIC("Error reading PNG file\n");
    }

    png_init_io(pngPtr, fp);

    png_read_info(pngPtr, infoPtr);

    png_get_IHDR(pngPtr, infoPtr, &pngWidth, &pngHeight,
                 &bitDepth, &colorType, &interlaceType,
                 &compressionMethod, &filterMethod);

    numPasses = png_set_interlace_handling(pngPtr);
    png_set_bgr(pngPtr);
    png_set_expand(pngPtr);

    png_read_update_info(pngPtr, infoPtr);

    /*
     * XXX: Handle more PNG cases?
     */
    VERIFY(bitDepth == 8);
    VERIFY(expectedWidth == 0 || pngWidth == expectedWidth);
    VERIFY(expectedHeight == 0 || pngHeight == expectedHeight);
    VERIFY(png_get_rowbytes(pngPtr, infoPtr) == sizeof(uint32) * pngWidth);

    sdlSurface = SDL_CreateRGBSurfaceWithFormat(0, pngWidth, pngHeight, 32,
                                                SDL_PIXELFORMAT_BGRA32);

    SDL_LockSurface(sdlSurface);
    for (int i = 0; i < numPasses; i++) {
        uint8 *pixels = sdlSurface->pixels;
        for (int y = 0; y < pngHeight; y++) {
            png_read_row(pngPtr, pixels, NULL);
            pixels += sdlSurface->pitch;
        }
    }
    SDL_UnlockSurface(sdlSurface);

    png_destroy_read_struct(&pngPtr, &infoPtr, NULL);

    fclose(fp);

    return sdlSurface;
}

void Sprite_SavePNG(const char *fileName, SDL_Surface *sdlSurface)
{
    FILE *fp = fopen(fileName, "wb");
    VERIFY(fp != NULL);

    png_struct *pngPtr;

    pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    VERIFY(pngPtr != NULL);

    png_info *infoPtr;
    infoPtr = png_create_info_struct(pngPtr);
    VERIFY(infoPtr != NULL);

    if (setjmp(png_jmpbuf(pngPtr))) {
        PANIC("Error writing PNG file\n");
    }

    png_init_io(pngPtr, fp);

    uint32 bitDepth = 8;
    png_set_IHDR(pngPtr, infoPtr, sdlSurface->w, sdlSurface->h,
                 bitDepth, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_set_bgr(pngPtr);

    png_write_info(pngPtr, infoPtr);

    SDL_LockSurface(sdlSurface);
    uint8 *pixels = (uint8 *)sdlSurface->pixels;

    for (int y = 0; y < sdlSurface->h; y++) {
        png_write_row(pngPtr, pixels);
        pixels += sdlSurface->pitch;
    }

    png_write_end(pngPtr, NULL);
    SDL_UnlockSurface(sdlSurface);

    png_destroy_write_struct(&pngPtr, &infoPtr);

    fclose(fp);
}


static void SpriteCalcMobSheetSize(uint32 *sheetWidth,
                                   uint32 *sheetHeight)
{
    uint32 dw = 0;
    uint32 dh = 0;

    /*
     * Calculate dimensions of sprite-sheet.
     */
    for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
        uint32 radius = (uint32)MobType_GetRadius(t);
        uint32 d = 2 * radius + 2;
        dw += d;
        dh = MAX(dh, d);
    }
    dw++;
    dh++;

    if (sheetWidth != NULL) {
        *sheetWidth = dw;
    }
    if (sheetHeight != NULL) {
        *sheetHeight = dh;
    }
}

static void SpriteCalcMobSpriteRect(MobType mobType,
                                    SDL_Rect *rect)
{
    ASSERT(rect != NULL);
    ASSERT(mobType >= MOB_TYPE_MIN);
    ASSERT(mobType < MOB_TYPE_MAX);

    uint d = 0;
    for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
        SDL_Point cPoint;
        uint32 radius = (uint32)MobType_GetRadius(t);

        d++;

        cPoint.x = d;
        cPoint.y = 1;

        if (mobType == t) {
            rect->x = cPoint.x;
            rect->y = cPoint.y;
            rect->w = 2 * radius + 1;
            rect->h = 2 * radius + 1;
            return;
        }

        d += 1 + (2 * radius);
    }

    NOT_REACHED();
}


SDL_Surface *Sprite_CreateMobSheet(uint32 bgraColor)
{
    SDL_Surface *spriteSheet;
    uint32 dw;
    uint32 dh;
    uint32 transparentBlack = 0x00000000;

    SpriteCalcMobSheetSize(&dw, &dh);

    spriteSheet = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32,
                                                 SDL_PIXELFORMAT_BGRA32);
    VERIFY(spriteSheet != NULL);

    SDL_FillRect(spriteSheet, NULL, transparentBlack);

    /*
     * Draw the circles into the sprite-sheet.
     */
    for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
        SDL_Point cPoint;
        SDL_Rect rect;
        uint32 radius = (uint32)MobType_GetRadius(t);
        SpriteCalcMobSpriteRect(t, &rect);

        ASSERT(2 * radius + 1 == rect.w);
        ASSERT(2 * radius + 1 == rect.h);

        cPoint.x = rect.x + radius;
        cPoint.y = rect.y + radius;

        Sprite_DrawCircle(spriteSheet, bgraColor, &cPoint, radius);
    }

    return spriteSheet;
}


Sprite *Sprite_CreateFromMobSheet(MobType t,
                                  SDL_Surface *mobSheet)
{
    Sprite *sprite;
    SDL_Rect rect;
    SpriteBacking *backing;
    SDL_Surface *sdlSurface;

    ASSERT(mobSheet != NULL);

    sprite = MBUtil_ZAlloc(sizeof(*sprite));
    backing = SpriteAllocBacking(&sprite->backingID);

    SpriteCalcMobSpriteRect(t, &rect);

    // XXX: We make a new surface instead of sharing the mobSheet.
    sprite->srcx = 0;
    sprite->srcy = 0;
    sprite->w = rect.w;
    sprite->h = rect.h;

    sdlSurface = SDL_CreateRGBSurfaceWithFormat(0, rect.w, rect.h, 32,
                                                SDL_PIXELFORMAT_BGRA32);
    VERIFY(sdlSurface != NULL);
    backing->sdlSurface = sdlSurface;

    ASSERT(rect.w == sdlSurface->w);
    ASSERT(rect.h == sdlSurface->h);
    SDL_BlitSurface(mobSheet, &rect, sdlSurface, NULL);
    return sprite;
}

void Sprite_PrepareTexture(Sprite *sprite, SDL_Renderer *r)
{
    ASSERT(sprite != NULL);

    SpriteBacking *backing = SpriteGetBacking(sprite->backingID);
    ASSERT(backing->sdlRenderer == NULL || backing->sdlRenderer == r);

    if (backing->sdlRenderer != r) {
        ASSERT(backing->sdlTexture == NULL);
        backing->sdlTexture = SDL_CreateTextureFromSurface(r, backing->sdlSurface);
        backing->sdlRenderer = r;
    }
}


uint32 Sprite_GetColor(FleetAIType aiType, uint repeatCount)
{
    ASSERT(repeatCount > 0);

    /*
     * Use a random color for any missing fleets.
     */
    uint32 color = Random_Uint32();
    uint i = 0;
    while (i < ARRAYSIZE(spriteFleetTable)) {
        if (spriteFleetTable[i].aiType == aiType) {
            color = spriteFleetTable[i].color;
            break;
        }
        i++;
    }

    uint32 shipAlpha = 0x88;

    ASSERT(repeatCount >= 1);
    if (repeatCount > 1) {
        color = (color << (24 - repeatCount)) |
                (color / (1 + (repeatCount - 1)));
    }
    color &= 0xFFFFFF;
    return color | ((shipAlpha & 0xFF) << 24);
}
