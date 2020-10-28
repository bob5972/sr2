/*
 * sprite.h -- part of SpaceRobots2
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

#ifndef _SPRITE_H_202010251053
#define _SPRITE_H_202010251053

#include "SDL.h"
#include "mbtypes.h"
#include "mob.h"

typedef enum SpriteType {
    SPRITE_INVALID,

    SPRITE_SIMPLE_RED_BASE,
    SPRITE_SIMPLE_RED_FIGHTER,
    SPRITE_SIMPLE_RED_MISSILE,
    SPRITE_SIMPLE_RED_POWER_CORE,

    SPRITE_SIMPLE_BLUE_BASE,
    SPRITE_SIMPLE_BLUE_FIGHTER,
    SPRITE_SIMPLE_BLUE_MISSILE,
    SPRITE_SIMPLE_BLUE_POWER_CORE,

    SPRITE_SIMPLE_GREEN_BASE,
    SPRITE_SIMPLE_GREEN_FIGHTER,
    SPRITE_SIMPLE_GREEN_MISSILE,
    SPRITE_SIMPLE_GREEN_POWER_CORE,

    SPRITE_SPACE_BASE,
    SPRITE_SPACE_FIGHTER,
    SPRITE_SPACE_MISSILE,
    SPRITE_SPACE_POWER_CORE,

    SPRITE_SPACE_BLUE_BASE,
    SPRITE_SPACE_BLUE_FIGHTER,
    SPRITE_SPACE_BLUE_MISSILE,
    SPRITE_SPACE_BLUE_POWER_CORE,

    SPRITE_FIGHTER_BLUE1,
    SPRITE_FIGHTER_BLUE2,
    SPRITE_FIGHTER_BLUE3,
    SPRITE_FIGHTER_BLUE4,
    SPRITE_FIGHTER_GREEN1,
    SPRITE_FIGHTER_GREEN2,
    SPRITE_FIGHTER_GREEN3,
    SPRITE_FIGHTER_GREEN4,
    SPRITE_FIGHTER_RED1,
    SPRITE_FIGHTER_RED2,
    SPRITE_FIGHTER_RED3,
    SPRITE_FIGHTER_RED4,
    SPRITE_MISSILE1,
    SPRITE_MISSILE2,
    SPRITE_MISSILE3,
    SPRITE_CORE1,
    SPRITE_CORE2,

    SPRITE_BLUE_BASE,
    SPRITE_BLUE_FIGHTER1,
    SPRITE_BLUE_FIGHTER2,
    SPRITE_BLUE_FIGHTER3,
    SPRITE_BLUE_FIGHTER4,

    SPRITE_PURPLE_BASE,
    SPRITE_PURPLE_FIGHTER1,
    SPRITE_PURPLE_FIGHTER2,
    SPRITE_PURPLE_FIGHTER3,
    SPRITE_PURPLE_FIGHTER4,

    SPRITE_GRAY_BASE,
    SPRITE_GRAY_FIGHTER1,
    SPRITE_GRAY_FIGHTER2,
    SPRITE_GRAY_FIGHTER3,
    SPRITE_GRAY_FIGHTER4,

    SPRITE_YELLOW_BASE,
    SPRITE_YELLOW_FIGHTER1,
    SPRITE_YELLOW_FIGHTER2,
    SPRITE_YELLOW_FIGHTER3,
    SPRITE_YELLOW_FIGHTER4,

    SPRITE_GREEN_BASE,
    SPRITE_GREEN_FIGHTER1,
    SPRITE_GREEN_FIGHTER2,
    SPRITE_GREEN_FIGHTER3,
    SPRITE_GREEN_FIGHTER4,

    SPRITE_RED_BASE,
    SPRITE_RED_FIGHTER1,
    SPRITE_RED_FIGHTER2,
    SPRITE_RED_FIGHTER3,
    SPRITE_RED_FIGHTER4,

    SPRITE_BLUE2_BASE,
    SPRITE_BLUE2_FIGHTER1,
    SPRITE_BLUE2_FIGHTER2,
    SPRITE_BLUE2_FIGHTER3,
    SPRITE_BLUE2_FIGHTER4,

    SPRITE_ORANGE_BASE,
    SPRITE_ORANGE_FIGHTER1,
    SPRITE_ORANGE_FIGHTER2,
    SPRITE_ORANGE_FIGHTER3,
    SPRITE_ORANGE_FIGHTER4,

    SPRITE_TURQUOISE_BASE,
    SPRITE_TURQUOISE_FIGHTER1,
    SPRITE_TURQUOISE_FIGHTER2,
    SPRITE_TURQUOISE_FIGHTER3,
    SPRITE_TURQUOISE_FIGHTER4,

    SPRITE_PURPLE2_BASE,
    SPRITE_PURPLE2_FIGHTER1,
    SPRITE_PURPLE2_FIGHTER2,
    SPRITE_PURPLE2_FIGHTER3,
    SPRITE_PURPLE2_FIGHTER4,

    SPRITE_WHITE_BASE,
    SPRITE_WHITE_FIGHTER1,
    SPRITE_WHITE_FIGHTER2,
    SPRITE_WHITE_FIGHTER3,
    SPRITE_WHITE_FIGHTER4,

    SPRITE_RED2_BASE,
    SPRITE_RED2_FIGHTER1,
    SPRITE_RED2_FIGHTER2,
    SPRITE_RED2_FIGHTER3,
    SPRITE_RED2_FIGHTER4,

    SPRITE_YELLOW2_BASE,
    SPRITE_YELLOW2_FIGHTER1,
    SPRITE_YELLOW2_FIGHTER2,
    SPRITE_YELLOW2_FIGHTER3,
    SPRITE_YELLOW2_FIGHTER4,

    SPRITE_MAGENTA_BASE,
    SPRITE_MAGENTA_FIGHTER1,
    SPRITE_MAGENTA_FIGHTER2,
    SPRITE_MAGENTA_FIGHTER3,
    SPRITE_MAGENTA_FIGHTER4,

    SPRITE_ORANGE2_BASE,
    SPRITE_ORANGE2_FIGHTER1,
    SPRITE_ORANGE2_FIGHTER2,
    SPRITE_ORANGE2_FIGHTER3,
    SPRITE_ORANGE2_FIGHTER4,

    SPRITE_YELLOW3_BASE,
    SPRITE_YELLOW3_FIGHTER1,
    SPRITE_YELLOW3_FIGHTER2,
    SPRITE_YELLOW3_FIGHTER3,
    SPRITE_YELLOW3_FIGHTER4,

    SPRITE_TYPE_MAX,
} SpriteType;

typedef struct Sprite {
    uint32 srcx, srcy;
    uint32 w, h;
    uint32 backingID;
} Sprite;

void Sprite_Init();
void Sprite_Exit();

uint32 Sprite_GetColor(FleetAIType aiType, uint repeatCount);

Sprite *Sprite_CreateCircle(uint32 radius, uint32 bgraColor);
Sprite *Sprite_CreateType(SpriteType t);
Sprite *Sprite_CreateMob(MobType t, FleetAIType aiType, uint repeatCount);
Sprite *Sprite_CreateFromMobSheet(MobType t, SDL_Surface *mobSheet);
void Sprite_Free(Sprite *s);

void Sprite_PrepareTexture(Sprite *sprite, SDL_Renderer *r);

SDL_Surface *Sprite_CreateMobSheet(uint32 bgraColor);

SDL_Surface *Sprite_LoadPNG(const char *fileName,
                            uint32 expectedWidth,
                            uint32 expectedHeight);
void Sprite_SavePNG(const char *fileName, SDL_Surface *sdlSurface);

void Sprite_DrawCircle(SDL_Surface *sdlSurface, uint32 color,
                       const SDL_Point *center, int radius);

void Sprite_Blit(Sprite *s, SDL_Renderer *r, uint32 x, uint32 y);
void Sprite_BlitCentered(Sprite *s, SDL_Renderer *r, uint32 x, uint32 y);

#endif // _SPRITE_H_202010251053
