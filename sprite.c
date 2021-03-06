/*
 * sprite.c -- part of SpaceRobots2
 * Copyright (C) 2020 Michael Banack <github@banack.net>
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
#include "random.h"

typedef enum SpriteSource {

    SPRITE_SOURCE_SPACE15,
    SPRITE_SOURCE_SPACE16,
    SPRITE_SOURCE_SPACE17,
    SPRITE_SOURCE_SPACE18,
    SPRITE_SOURCE_SPACE19,
    SPRITE_SOURCE_SPACE20,
    SPRITE_SOURCE_SPACE21,
    SPRITE_SOURCE_SPACE22,
    SPRITE_SOURCE_SPACE23,
    SPRITE_SOURCE_SPACE24,
    SPRITE_SOURCE_SPACE25,
    SPRITE_SOURCE_SPACE26,
    SPRITE_SOURCE_SPACE27,
    SPRITE_SOURCE_SPACE28,
    SPRITE_SOURCE_SPACE29,

    SPRITE_SOURCE_SHEET2,
    SPRITE_SOURCE_MAX,
    SPRITE_SOURCE_INVALID,
} SpriteSource;

typedef struct SpriteSpec {
    SpriteType type;
    SpriteSource source;
    uint32 x, y, w, h;
} SpriteSpec;

static const SpriteSpec gSpecs[] = {
    { SPRITE_INVALID,                 SPRITE_SOURCE_INVALID,       0,   0,   0,   0, },

    { SPRITE_SPACE_BLUE_BASE,         SPRITE_SOURCE_SPACE15,       1,   1, 101, 101, },
    { SPRITE_SPACE_BLUE_FIGHTER,      SPRITE_SOURCE_SPACE15,     103,   1,  11,  11, },
    { SPRITE_SPACE_BLUE_MISSILE,      SPRITE_SOURCE_SPACE15,     115,   1,   7,   7, },
    { SPRITE_SPACE_BLUE_POWER_CORE,   SPRITE_SOURCE_SPACE15,     123,   1,   5,   5, },

    { SPRITE_SPACE_PURPLE_BASE,       SPRITE_SOURCE_SPACE16,       1,   1, 101, 101, },
    { SPRITE_SPACE_PURPLE_FIGHTER,    SPRITE_SOURCE_SPACE16,     103,   1,  11,  11, },
    { SPRITE_SPACE_PURPLE_MISSILE,    SPRITE_SOURCE_SPACE16,     115,   1,   7,   7, },
    { SPRITE_SPACE_PURPLE_POWER_CORE, SPRITE_SOURCE_SPACE16,     123,   1,   5,   5, },

    { SPRITE_SPACE_GREEN_BASE,        SPRITE_SOURCE_SPACE17,       1,   1, 101, 101, },
    { SPRITE_SPACE_GREEN_FIGHTER,     SPRITE_SOURCE_SPACE17,     103,   1,  11,  11, },
    { SPRITE_SPACE_GREEN_MISSILE,     SPRITE_SOURCE_SPACE17,     115,   1,   7,   7, },
    { SPRITE_SPACE_GREEN_POWER_CORE,  SPRITE_SOURCE_SPACE17,     123,   1,   5,   5, },

    { SPRITE_SPACE_GREEN2_BASE,       SPRITE_SOURCE_SPACE18,       1,   1, 101, 101, },
    { SPRITE_SPACE_GREEN2_FIGHTER,    SPRITE_SOURCE_SPACE18,     103,   1,  11,  11, },
    { SPRITE_SPACE_GREEN2_MISSILE,    SPRITE_SOURCE_SPACE18,     115,   1,   7,   7, },
    { SPRITE_SPACE_GREEN2_POWER_CORE, SPRITE_SOURCE_SPACE18,     123,   1,   5,   5, },

    { SPRITE_SPACE_GREEN3_BASE,       SPRITE_SOURCE_SPACE19,       1,   1, 101, 101, },
    { SPRITE_SPACE_GREEN3_FIGHTER,    SPRITE_SOURCE_SPACE19,     103,   1,  11,  11, },
    { SPRITE_SPACE_GREEN3_MISSILE,    SPRITE_SOURCE_SPACE19,     115,   1,   7,   7, },
    { SPRITE_SPACE_GREEN3_POWER_CORE, SPRITE_SOURCE_SPACE19,     123,   1,   5,   5, },

    { SPRITE_SPACE_YELLOW_BASE,       SPRITE_SOURCE_SPACE20,       1,   1, 101, 101, },
    { SPRITE_SPACE_YELLOW_FIGHTER,    SPRITE_SOURCE_SPACE20,     103,   1,  11,  11, },
    { SPRITE_SPACE_YELLOW_MISSILE,    SPRITE_SOURCE_SPACE20,     115,   1,   7,   7, },
    { SPRITE_SPACE_YELLOW_POWER_CORE, SPRITE_SOURCE_SPACE20,     123,   1,   5,   5, },

    { SPRITE_SPACE_ORANGE_BASE,       SPRITE_SOURCE_SPACE21,       1,   1, 101, 101, },
    { SPRITE_SPACE_ORANGE_FIGHTER,    SPRITE_SOURCE_SPACE21,     103,   1,  11,  11, },
    { SPRITE_SPACE_ORANGE_MISSILE,    SPRITE_SOURCE_SPACE21,     115,   1,   7,   7, },
    { SPRITE_SPACE_ORANGE_POWER_CORE, SPRITE_SOURCE_SPACE21,     123,   1,   5,   5, },

    { SPRITE_SPACE_RED_BASE,          SPRITE_SOURCE_SPACE22,       1,   1, 101, 101, },
    { SPRITE_SPACE_RED_FIGHTER,       SPRITE_SOURCE_SPACE22,     103,   1,  11,  11, },
    { SPRITE_SPACE_RED_MISSILE,       SPRITE_SOURCE_SPACE22,     115,   1,   7,   7, },
    { SPRITE_SPACE_RED_POWER_CORE,    SPRITE_SOURCE_SPACE22,     123,   1,   5,   5, },

    { SPRITE_SPACE_PURPLE2_BASE,      SPRITE_SOURCE_SPACE23,       1,   1, 101, 101, },
    { SPRITE_SPACE_PURPLE2_FIGHTER,   SPRITE_SOURCE_SPACE23,     103,   1,  11,  11, },
    { SPRITE_SPACE_PURPLE2_MISSILE,   SPRITE_SOURCE_SPACE23,     115,   1,   7,   7, },
    { SPRITE_SPACE_PURPLE2_POWER_CORE,SPRITE_SOURCE_SPACE23,     123,   1,   5,   5, },

    { SPRITE_SPACE_RED2_BASE,         SPRITE_SOURCE_SPACE24,       1,   1, 101, 101, },
    { SPRITE_SPACE_RED2_FIGHTER,      SPRITE_SOURCE_SPACE24,     103,   1,  11,  11, },
    { SPRITE_SPACE_RED2_MISSILE,      SPRITE_SOURCE_SPACE24,     115,   1,   7,   7, },
    { SPRITE_SPACE_RED2_POWER_CORE,   SPRITE_SOURCE_SPACE24,     123,   1,   5,   5, },

    { SPRITE_SPACE_WHITE_BASE,        SPRITE_SOURCE_SPACE25,       1,   1, 101, 101, },
    { SPRITE_SPACE_WHITE_FIGHTER,     SPRITE_SOURCE_SPACE25,     103,   1,  11,  11, },
    { SPRITE_SPACE_WHITE_MISSILE,     SPRITE_SOURCE_SPACE25,     115,   1,   7,   7, },
    { SPRITE_SPACE_WHITE_POWER_CORE,  SPRITE_SOURCE_SPACE25,     123,   1,   5,   5, },

    { SPRITE_SPACE_YELLOW2_BASE,      SPRITE_SOURCE_SPACE26,       1,   1, 101, 101, },
    { SPRITE_SPACE_YELLOW2_FIGHTER,   SPRITE_SOURCE_SPACE26,     103,   1,  11,  11, },
    { SPRITE_SPACE_YELLOW2_MISSILE,   SPRITE_SOURCE_SPACE26,     115,   1,   7,   7, },
    { SPRITE_SPACE_YELLOW2_POWER_CORE,SPRITE_SOURCE_SPACE26,     123,   1,   5,   5, },

    { SPRITE_SPACE_BROWN_BASE,        SPRITE_SOURCE_SPACE27,       1,   1, 101, 101, },
    { SPRITE_SPACE_BROWN_FIGHTER,     SPRITE_SOURCE_SPACE27,     103,   1,  11,  11, },
    { SPRITE_SPACE_BROWN_MISSILE,     SPRITE_SOURCE_SPACE27,     115,   1,   7,   7, },
    { SPRITE_SPACE_BROWN_POWER_CORE,  SPRITE_SOURCE_SPACE27,     123,   1,   5,   5, },

    { SPRITE_SPACE_RED3_BASE,         SPRITE_SOURCE_SPACE28,       1,   1, 101, 101, },
    { SPRITE_SPACE_RED3_FIGHTER,      SPRITE_SOURCE_SPACE28,     103,   1,  11,  11, },
    { SPRITE_SPACE_RED3_MISSILE,      SPRITE_SOURCE_SPACE28,     115,   1,   7,   7, },
    { SPRITE_SPACE_RED3_POWER_CORE,   SPRITE_SOURCE_SPACE28,     123,   1,   5,   5, },

    { SPRITE_SPACE_PURPLE3_BASE,      SPRITE_SOURCE_SPACE29,       1,   1, 101, 101, },
    { SPRITE_SPACE_PURPLE3_FIGHTER,   SPRITE_SOURCE_SPACE29,     103,   1,  11,  11, },
    { SPRITE_SPACE_PURPLE3_MISSILE,   SPRITE_SOURCE_SPACE29,     115,   1,   7,   7, },
    { SPRITE_SPACE_PURPLE3_POWER_CORE,SPRITE_SOURCE_SPACE29,     123,   1,   5,   5, },

    { SPRITE_BLUE_BASE,               SPRITE_SOURCE_SHEET2,       38,  34,  99,  99, },
    { SPRITE_BLUE_FIGHTER1,           SPRITE_SOURCE_SHEET2,       59, 148,   9,   9, },
    { SPRITE_BLUE_FIGHTER2,           SPRITE_SOURCE_SHEET2,       75, 148,   9,   9, },
    { SPRITE_BLUE_FIGHTER3,           SPRITE_SOURCE_SHEET2,       91, 148,   9,   9, },
    { SPRITE_BLUE_FIGHTER4,           SPRITE_SOURCE_SHEET2,      107, 148,   9,   9, },
    { SPRITE_BLUE_POWER_CORE,         SPRITE_SOURCE_SHEET2,      126, 150,   4,   4, },
    { SPRITE_BLUE_MISSILE,            SPRITE_SOURCE_SHEET2,      140, 148,   8,   8, },

    { SPRITE_PURPLE_BASE,             SPRITE_SOURCE_SHEET2,      198,  34,  99,  99, },
    { SPRITE_PURPLE_FIGHTER1,         SPRITE_SOURCE_SHEET2,      219, 148,   9,   9, },
    { SPRITE_PURPLE_FIGHTER2,         SPRITE_SOURCE_SHEET2,      235, 148,   9,   9, },
    { SPRITE_PURPLE_FIGHTER3,         SPRITE_SOURCE_SHEET2,      251, 148,   9,   9, },
    { SPRITE_PURPLE_FIGHTER4,         SPRITE_SOURCE_SHEET2,      267, 148,   9,   9, },
    { SPRITE_PURPLE_POWER_CORE,       SPRITE_SOURCE_SHEET2,      286, 150,   4,   4, },
    { SPRITE_PURPLE_MISSILE,          SPRITE_SOURCE_SHEET2,      300, 148,   8,   8, },

    { SPRITE_GRAY_BASE,               SPRITE_SOURCE_SHEET2,      358,  34,  99,  99, },
    { SPRITE_GRAY_FIGHTER1,           SPRITE_SOURCE_SHEET2,      379, 148,   9,   9, },
    { SPRITE_GRAY_FIGHTER2,           SPRITE_SOURCE_SHEET2,      395, 148,   9,   9, },
    { SPRITE_GRAY_FIGHTER3,           SPRITE_SOURCE_SHEET2,      411, 148,   9,   9, },
    { SPRITE_GRAY_FIGHTER4,           SPRITE_SOURCE_SHEET2,      427, 148,   9,   9, },
    { SPRITE_GRAY_POWER_CORE,         SPRITE_SOURCE_SHEET2,      446, 150,   4,   4, },
    { SPRITE_GRAY_MISSILE,            SPRITE_SOURCE_SHEET2,      460, 148,   8,   8, },

    { SPRITE_YELLOW_BASE,             SPRITE_SOURCE_SHEET2,      518,  34,  99,  99, },
    { SPRITE_YELLOW_FIGHTER1,         SPRITE_SOURCE_SHEET2,      539, 148,   9,   9, },
    { SPRITE_YELLOW_FIGHTER2,         SPRITE_SOURCE_SHEET2,      555, 148,   9,   9, },
    { SPRITE_YELLOW_FIGHTER3,         SPRITE_SOURCE_SHEET2,      571, 148,   9,   9, },
    { SPRITE_YELLOW_FIGHTER4,         SPRITE_SOURCE_SHEET2,      587, 148,   9,   9, },
    { SPRITE_YELLOW_POWER_CORE,       SPRITE_SOURCE_SHEET2,      606, 150,   4,   4, },
    { SPRITE_YELLOW_MISSILE,          SPRITE_SOURCE_SHEET2,      620, 148,   8,   8, },

    { SPRITE_GREEN_BASE,              SPRITE_SOURCE_SHEET2,       38, 210,  99,  99, },
    { SPRITE_GREEN_FIGHTER1,          SPRITE_SOURCE_SHEET2,       59, 324,   9,   9, },
    { SPRITE_GREEN_FIGHTER2,          SPRITE_SOURCE_SHEET2,       75, 324,   9,   9, },
    { SPRITE_GREEN_FIGHTER3,          SPRITE_SOURCE_SHEET2,       91, 324,   9,   9, },
    { SPRITE_GREEN_FIGHTER4,          SPRITE_SOURCE_SHEET2,      107, 324,   9,   9, },
    { SPRITE_GREEN_POWER_CORE,        SPRITE_SOURCE_SHEET2,      126, 326,   4,   4, },
    { SPRITE_GREEN_MISSILE,           SPRITE_SOURCE_SHEET2,      140, 324,   8,   8, },

    { SPRITE_RED_BASE,                SPRITE_SOURCE_SHEET2,      198, 210,  99,  99, },
    { SPRITE_RED_FIGHTER1,            SPRITE_SOURCE_SHEET2,      219, 324,   9,   9, },
    { SPRITE_RED_FIGHTER2,            SPRITE_SOURCE_SHEET2,      235, 324,   9,   9, },
    { SPRITE_RED_FIGHTER3,            SPRITE_SOURCE_SHEET2,      251, 324,   9,   9, },
    { SPRITE_RED_FIGHTER4,            SPRITE_SOURCE_SHEET2,      267, 324,   9,   9, },
    { SPRITE_RED_POWER_CORE,          SPRITE_SOURCE_SHEET2,      286, 326,   4,   4, },
    { SPRITE_RED_MISSILE,             SPRITE_SOURCE_SHEET2,      300, 324,   8,   8, },

    { SPRITE_BLUE2_BASE,              SPRITE_SOURCE_SHEET2,      358, 210,  99,  99, },
    { SPRITE_BLUE2_FIGHTER1,          SPRITE_SOURCE_SHEET2,      379, 324,   9,   9, },
    { SPRITE_BLUE2_FIGHTER2,          SPRITE_SOURCE_SHEET2,      395, 324,   9,   9, },
    { SPRITE_BLUE2_FIGHTER3,          SPRITE_SOURCE_SHEET2,      411, 324,   9,   9, },
    { SPRITE_BLUE2_FIGHTER4,          SPRITE_SOURCE_SHEET2,      427, 324,   9,   9, },
    { SPRITE_BLUE2_POWER_CORE,        SPRITE_SOURCE_SHEET2,      446, 326,   4,   4, },
    { SPRITE_BLUE2_MISSILE,           SPRITE_SOURCE_SHEET2,      460, 324,   8,   8, },

    { SPRITE_ORANGE_BASE,             SPRITE_SOURCE_SHEET2,      518, 210,  99,  99, },
    { SPRITE_ORANGE_FIGHTER1,         SPRITE_SOURCE_SHEET2,      539, 324,   9,   9, },
    { SPRITE_ORANGE_FIGHTER2,         SPRITE_SOURCE_SHEET2,      555, 324,   9,   9, },
    { SPRITE_ORANGE_FIGHTER3,         SPRITE_SOURCE_SHEET2,      571, 324,   9,   9, },
    { SPRITE_ORANGE_FIGHTER4,         SPRITE_SOURCE_SHEET2,      587, 324,   9,   9, },
    { SPRITE_ORANGE_POWER_CORE,       SPRITE_SOURCE_SHEET2,      606, 326,   4,   4, },
    { SPRITE_ORANGE_MISSILE,          SPRITE_SOURCE_SHEET2,      620, 324,   8,   8, },

    { SPRITE_TURQUOISE_BASE,          SPRITE_SOURCE_SHEET2,       38, 386,  99,  99, },
    { SPRITE_TURQUOISE_FIGHTER1,      SPRITE_SOURCE_SHEET2,       59, 500,   9,   9, },
    { SPRITE_TURQUOISE_FIGHTER2,      SPRITE_SOURCE_SHEET2,       75, 500,   9,   9, },
    { SPRITE_TURQUOISE_FIGHTER3,      SPRITE_SOURCE_SHEET2,       91, 500,   9,   9, },
    { SPRITE_TURQUOISE_FIGHTER4,      SPRITE_SOURCE_SHEET2,      107, 500,   9,   9, },
    { SPRITE_TURQUOISE_POWER_CORE,    SPRITE_SOURCE_SHEET2,      126, 502,   4,   4, },
    { SPRITE_TURQUOISE_MISSILE,       SPRITE_SOURCE_SHEET2,      140, 500,   8,   8, },

    { SPRITE_PURPLE2_BASE,            SPRITE_SOURCE_SHEET2,      198, 386,  99,  99, },
    { SPRITE_PURPLE2_FIGHTER1,        SPRITE_SOURCE_SHEET2,      219, 500,   9,   9, },
    { SPRITE_PURPLE2_FIGHTER2,        SPRITE_SOURCE_SHEET2,      235, 500,   9,   9, },
    { SPRITE_PURPLE2_FIGHTER3,        SPRITE_SOURCE_SHEET2,      251, 500,   9,   9, },
    { SPRITE_PURPLE2_FIGHTER4,        SPRITE_SOURCE_SHEET2,      267, 500,   9,   9, },
    { SPRITE_PURPLE2_POWER_CORE,      SPRITE_SOURCE_SHEET2,      286, 502,   4,   4, },
    { SPRITE_PURPLE2_MISSILE,         SPRITE_SOURCE_SHEET2,      300, 500,   8,   8, },

    { SPRITE_WHITE_BASE,              SPRITE_SOURCE_SHEET2,      358, 386,  99,  99, },
    { SPRITE_WHITE_FIGHTER1,          SPRITE_SOURCE_SHEET2,      379, 500,   9,   9, },
    { SPRITE_WHITE_FIGHTER2,          SPRITE_SOURCE_SHEET2,      395, 500,   9,   9, },
    { SPRITE_WHITE_FIGHTER3,          SPRITE_SOURCE_SHEET2,      411, 500,   9,   9, },
    { SPRITE_WHITE_FIGHTER4,          SPRITE_SOURCE_SHEET2,      427, 500,   9,   9, },
    { SPRITE_WHITE_POWER_CORE,        SPRITE_SOURCE_SHEET2,      446, 502,   4,   4, },
    { SPRITE_WHITE_MISSILE,           SPRITE_SOURCE_SHEET2,      460, 500,   8,   8, },

    { SPRITE_RED2_BASE,               SPRITE_SOURCE_SHEET2,      518, 386,  99,  99, },
    { SPRITE_RED2_FIGHTER1,           SPRITE_SOURCE_SHEET2,      539, 500,   9,   9, },
    { SPRITE_RED2_FIGHTER2,           SPRITE_SOURCE_SHEET2,      555, 500,   9,   9, },
    { SPRITE_RED2_FIGHTER3,           SPRITE_SOURCE_SHEET2,      571, 500,   9,   9, },
    { SPRITE_RED2_FIGHTER4,           SPRITE_SOURCE_SHEET2,      587, 500,   9,   9, },
    { SPRITE_RED2_POWER_CORE,         SPRITE_SOURCE_SHEET2,      606, 502,   4,   4, },
    { SPRITE_RED2_MISSILE,            SPRITE_SOURCE_SHEET2,      620, 500,   8,   8, },

    { SPRITE_YELLOW2_BASE,            SPRITE_SOURCE_SHEET2,       38, 562,  99,  99, },
    { SPRITE_YELLOW2_FIGHTER1,        SPRITE_SOURCE_SHEET2,       59, 676,   9,   9, },
    { SPRITE_YELLOW2_FIGHTER2,        SPRITE_SOURCE_SHEET2,       75, 676,   9,   9, },
    { SPRITE_YELLOW2_FIGHTER3,        SPRITE_SOURCE_SHEET2,       91, 676,   9,   9, },
    { SPRITE_YELLOW2_FIGHTER4,        SPRITE_SOURCE_SHEET2,      107, 676,   9,   9, },
    { SPRITE_YELLOW2_POWER_CORE,      SPRITE_SOURCE_SHEET2,      126, 678,   4,   4, },
    { SPRITE_YELLOW2_MISSILE,         SPRITE_SOURCE_SHEET2,      140, 676,   8,   8, },

    { SPRITE_MAGENTA_BASE,            SPRITE_SOURCE_SHEET2,      198, 562,  99,  99, },
    { SPRITE_MAGENTA_FIGHTER1,        SPRITE_SOURCE_SHEET2,      219, 676,   9,   9, },
    { SPRITE_MAGENTA_FIGHTER2,        SPRITE_SOURCE_SHEET2,      235, 676,   9,   9, },
    { SPRITE_MAGENTA_FIGHTER3,        SPRITE_SOURCE_SHEET2,      251, 676,   9,   9, },
    { SPRITE_MAGENTA_FIGHTER4,        SPRITE_SOURCE_SHEET2,      267, 676,   9,   9, },
    { SPRITE_MAGENTA_POWER_CORE,      SPRITE_SOURCE_SHEET2,      286, 678,   4,   4, },
    { SPRITE_MAGENTA_MISSILE,         SPRITE_SOURCE_SHEET2,      300, 676,   8,   8, },

    { SPRITE_ORANGE2_BASE,            SPRITE_SOURCE_SHEET2,      358, 562,  99,  99, },
    { SPRITE_ORANGE2_FIGHTER1,        SPRITE_SOURCE_SHEET2,      379, 676,   9,   9, },
    { SPRITE_ORANGE2_FIGHTER2,        SPRITE_SOURCE_SHEET2,      395, 676,   9,   9, },
    { SPRITE_ORANGE2_FIGHTER3,        SPRITE_SOURCE_SHEET2,      411, 676,   9,   9, },
    { SPRITE_ORANGE2_FIGHTER4,        SPRITE_SOURCE_SHEET2,      427, 676,   9,   9, },
    { SPRITE_ORANGE2_POWER_CORE,      SPRITE_SOURCE_SHEET2,      446, 678,   4,   4, },
    { SPRITE_ORANGE2_MISSILE,         SPRITE_SOURCE_SHEET2,      460, 676,   8,   8, },

    { SPRITE_YELLOW3_BASE,            SPRITE_SOURCE_SHEET2,      518, 562,  99,  99, },
    { SPRITE_YELLOW3_FIGHTER1,        SPRITE_SOURCE_SHEET2,      539, 676,   9,   9, },
    { SPRITE_YELLOW3_FIGHTER2,        SPRITE_SOURCE_SHEET2,      555, 676,   9,   9, },
    { SPRITE_YELLOW3_FIGHTER3,        SPRITE_SOURCE_SHEET2,      571, 676,   9,   9, },
    { SPRITE_YELLOW3_FIGHTER4,        SPRITE_SOURCE_SHEET2,      587, 676,   9,   9, },
    { SPRITE_YELLOW3_POWER_CORE,      SPRITE_SOURCE_SHEET2,      606, 678,   4,   4, },
    { SPRITE_YELLOW3_MISSILE,         SPRITE_SOURCE_SHEET2,      620, 676,   8,   8, },
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
    ASSERT(SPRITE_SOURCE_MAX == 16);
    gSprite.sources[SPRITE_SOURCE_SPACE15] = Sprite_LoadPNG("art/space15.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE16] = Sprite_LoadPNG("art/space16.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE17] = Sprite_LoadPNG("art/space17.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE18] = Sprite_LoadPNG("art/space18.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE19] = Sprite_LoadPNG("art/space19.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE20] = Sprite_LoadPNG("art/space20.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE21] = Sprite_LoadPNG("art/space21.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE22] = Sprite_LoadPNG("art/space22.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE23] = Sprite_LoadPNG("art/space23.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE24] = Sprite_LoadPNG("art/space24.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE25] = Sprite_LoadPNG("art/space25.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE26] = Sprite_LoadPNG("art/space26.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE27] = Sprite_LoadPNG("art/space27.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE28] = Sprite_LoadPNG("art/space28.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SPACE29] = Sprite_LoadPNG("art/space29.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SHEET2] = Sprite_LoadPNG("art/sheet2.png", 656, 720);

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
    SpriteType st;

    switch (ss) {
        case SPRITE_SET_INVALID:
            st = SPRITE_INVALID;
            break;
        case SPRITE_SET_SPACE_BLUE:
            st = SPRITE_SPACE_BLUE_BASE;
            break;
        case SPRITE_SET_SPACE_PURPLE:
            st = SPRITE_SPACE_PURPLE_BASE;
            break;
        case SPRITE_SET_SPACE_GREEN:
            st = SPRITE_SPACE_GREEN_BASE;
            break;
        case SPRITE_SET_SPACE_GREEN2:
            st = SPRITE_SPACE_GREEN2_BASE;
            break;
        case SPRITE_SET_SPACE_GREEN3:
            st = SPRITE_SPACE_GREEN3_BASE;
            break;
        case SPRITE_SET_SPACE_YELLOW:
            st = SPRITE_SPACE_YELLOW_BASE;
            break;
        case SPRITE_SET_SPACE_ORANGE:
            st = SPRITE_SPACE_ORANGE_BASE;
            break;
        case SPRITE_SET_SPACE_RED:
            st = SPRITE_SPACE_RED_BASE;
            break;
        case SPRITE_SET_SPACE_PURPLE2:
            st = SPRITE_SPACE_PURPLE2_BASE;
            break;
        case SPRITE_SET_SPACE_RED2:
            st = SPRITE_SPACE_RED2_BASE;
            break;
        case SPRITE_SET_SPACE_WHITE:
            st = SPRITE_SPACE_WHITE_BASE;
            break;
        case SPRITE_SET_SPACE_YELLOW2:
            st = SPRITE_SPACE_YELLOW2_BASE;
            break;
        case SPRITE_SET_SPACE_BROWN:
            st = SPRITE_SPACE_BROWN_BASE;
            break;
        case SPRITE_SET_SPACE_RED3:
            st = SPRITE_SPACE_RED3_BASE;
            break;
        case SPRITE_SET_SPACE_PURPLE3:
            st = SPRITE_SPACE_PURPLE3_BASE;
            break;
        case SPRITE_SET_BLUE:
            st = SPRITE_BLUE_BASE;
            break;
        case SPRITE_SET_PURPLE:
            st = SPRITE_PURPLE_BASE;
            break;
        case SPRITE_SET_GRAY:
            st = SPRITE_GRAY_BASE;
            break;
        case SPRITE_SET_YELLOW:
            st = SPRITE_YELLOW_BASE;
            break;
        case SPRITE_SET_GREEN:
            st = SPRITE_GREEN_BASE;
            break;
        case SPRITE_SET_RED:
            st = SPRITE_RED_BASE;
            break;
        case SPRITE_SET_BLUE2:
            st = SPRITE_BLUE2_BASE;
            break;
        case SPRITE_SET_ORANGE:
            st = SPRITE_ORANGE_BASE;
            break;
        case SPRITE_SET_TURQUOISE:
            st = SPRITE_TURQUOISE_BASE;
            break;
        case SPRITE_SET_PURPLE2:
            st = SPRITE_PURPLE2_BASE;
            break;
        case SPRITE_SET_WHITE:
            st = SPRITE_WHITE_BASE;
            break;
        case SPRITE_SET_RED2:
            st = SPRITE_RED2_BASE;
            break;
        case SPRITE_SET_YELLOW2:
            st = SPRITE_YELLOW2_BASE;
            break;
        case SPRITE_SET_MAGENTA:
            st = SPRITE_MAGENTA_BASE;
            break;
        case SPRITE_SET_ORANGE2:
            st = SPRITE_ORANGE2_BASE;
            break;
        case SPRITE_SET_YELLOW3:
            st = SPRITE_YELLOW3_BASE;
            break;
        default:
            NOT_REACHED();
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

    if (t == MOB_TYPE_BASE) {
        return st;
    } else if (t == MOB_TYPE_FIGHTER) {
        return st + Random_Int(1, 4);
    } else if (t == MOB_TYPE_POWER_CORE) {
        return st + 5;
    } else if (t == MOB_TYPE_MISSILE) {
        return st + 6;
    }

    NOT_REACHED();
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

    switch (aiType) {
        case FLEET_AI_NEUTRAL:
            //return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_GRAY);
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_SPACE_WHITE);
        case FLEET_AI_DUMMY:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_WHITE);
        case FLEET_AI_SIMPLE:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_RED);
        case FLEET_AI_GATHER:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_GREEN);
        case FLEET_AI_CLOUD:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_BLUE);
        case FLEET_AI_MAPPER:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_YELLOW);
        case FLEET_AI_RUNAWAY:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_PURPLE2);
        case FLEET_AI_COWARD:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_TURQUOISE);
            //return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_SPACE_GREEN2);
        case FLEET_AI_BASIC:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_YELLOW3);
            //return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_SPACE_YELLOW2);
        case FLEET_AI_HOLD:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_PURPLE);
            //return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_SPACE_PURPLE);
        case FLEET_AI_BOB:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_SPACE_BLUE);
        case FLEET_AI_CIRCLE:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_BLUE2);
        case FLEET_AI_FLOCK:
            return SpriteGetMobSpriteTypeFromSet(t, SPRITE_SET_SPACE_PURPLE);
        default:
            return SPRITE_INVALID;
    }
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

    png_read_update_info(pngPtr, infoPtr);

    /*
     * XXX: Handle more PNG cases?
     */
    VERIFY(bitDepth == 8);
    VERIFY(colorType == PNG_COLOR_TYPE_RGBA);
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
    struct {
        FleetAIType aiType;
        uint32 color;
    } colors[] = {
        { FLEET_AI_INVALID, 0x000000, }, // 0x(AA)RRGGBB
        { FLEET_AI_NEUTRAL, 0x888888, }, // GRAY
        { FLEET_AI_DUMMY,   0xFFFFFF, }, // WHITE
        { FLEET_AI_SIMPLE,  0xFF0000, }, // RED
        { FLEET_AI_GATHER,  0x00FF00, }, // GREEN
        { FLEET_AI_CLOUD,   0x0000FF, }, // BLUE
        { FLEET_AI_MAPPER,  0x808000, }, // YELLOW
        { FLEET_AI_CIRCLE,  0x048488, }, // TEAL-ish
        { FLEET_AI_RUNAWAY, 0x800080, }, // PURPLE
        { FLEET_AI_COWARD,  0x008080, }, // TEAL
        { FLEET_AI_BASIC,   0x808080, }, // DARK GRAY
        { FLEET_AI_HOLD,    0xF00080, }, // PURPLE
        { FLEET_AI_BOB,     0x80F080, }, // GREENISH-YELLOW
        { FLEET_AI_FLOCK,   0xFF3388, },
    };

    ASSERT(repeatCount > 0);

    /*
     * Use WHITE for any missing fleets.
     */
    uint32 color = 0xFFFFFF; // WHITE
    uint i = 0;
    while (i < ARRAYSIZE(colors)) {
        if (colors[i].aiType == aiType) {
            color = colors[i].color;
            break;
        }
        i++;
    }

    uint32 shipAlpha = 0x88;
    color /= (1 + (repeatCount - 1));
    return color | ((shipAlpha & 0xFF) << 24);
}
