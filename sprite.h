/*
 * sprite.h -- part of SpaceRobots2
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

#ifndef _SPRITE_H_202010251053
#define _SPRITE_H_202010251053

#include <SDL2/SDL.h>
#include "MBTypes.h"
#include "mob.h"

typedef enum SpriteSet {
    SPRITE_SET_INVALID,
    SPRITE_SET_SPACE_BLUE,          // Meta
    SPRITE_SET_SPACE_PURPLE,        // Flock4
    SPRITE_SET_SPACE_GREEN,         // Flock7
    SPRITE_SET_SPACE_GREEN2,        // Flock8
    SPRITE_SET_SPACE_GREEN3,        // Flock9
    SPRITE_SET_SPACE_YELLOW,
    SPRITE_SET_SPACE_ORANGE,
    SPRITE_SET_SPACE_RED,           // Flock1
    SPRITE_SET_SPACE_PURPLE2,       // Flock3
    SPRITE_SET_SPACE_RED2,          // Flock2
    SPRITE_SET_SPACE_WHITE,         // Neutral
    SPRITE_SET_SPACE_YELLOW2,
    SPRITE_SET_SPACE_BROWN,
    SPRITE_SET_SPACE_RED3,          // Flock6
    SPRITE_SET_SPACE_PURPLE3,       // Flock5

    SPRITE_SET_NAJU_BLUE,           // Cloud
    SPRITE_SET_NAJU_PURPLE,         // Hold
    SPRITE_SET_NAJU_GRAY,
    SPRITE_SET_NAJU_YELLOW,         // Mapper
    SPRITE_SET_NAJU_GREEN,          // Gather
    SPRITE_SET_NAJU_RED,            // Simple
    SPRITE_SET_NAJU_BLUE2,          // Circle
    SPRITE_SET_NAJU_ORANGE,
    SPRITE_SET_NAJU_TURQUOISE,      // Coward
    SPRITE_SET_NAJU_PURPLE2,        // RunAway
    SPRITE_SET_NAJU_WHITE,          // Dummy
    SPRITE_SET_NAJU_RED2,
    SPRITE_SET_NAJU_YELLOW2,
    SPRITE_SET_NAJU_MAGENTA,
    SPRITE_SET_NAJU_ORANGE2,
    SPRITE_SET_NAJU_YELLOW3,        // Basic

    SPRITE_SET_ALTAIR_PURPLE,       // Bundle1
    SPRITE_SET_ALTAIR_PURPLE2,      // Bundle2
    SPRITE_SET_ALTAIR_YELLOW,       // Bundle3
    SPRITE_SET_ALTAIR_RED2,         // Bundle5
    SPRITE_SET_ALTAIR_GREEN,        // Bundle8
    SPRITE_SET_ALTAIR_GREEN2,       // Bundle9
    SPRITE_SET_ALTAIR_BLUE,         // Bundle11
    SPRITE_SET_ALTAIR_BLUE2,        // Bundle12
    SPRITE_SET_ALTAIR_MAGENTA,      // Bundle13
    SPRITE_SET_ALTAIR_RED,          // Bundle6
    SPRITE_SET_ALTAIR_RED3,         // Bundle7
    SPRITE_SET_ALTAIR_GREEN3,       // Bundle10
    SPRITE_SET_ALTAIR_ORANGE,       // Bundle14
    SPRITE_SET_ALTAIR_ORANGE2,      // Bundle15
    SPRITE_SET_ALTAIR_YELLOW2,      // Bundle4
    SPRITE_SET_ALTAIR_BLUE3,        // Bundle16

    SPRITE_SET_URSA_BLUE,           // Neural1
    SPRITE_SET_URSA_BLUE2,          // Neural8
    SPRITE_SET_URSA_PURPLE,         // Neural2 / Neural3
    SPRITE_SET_URSA_PURPLE2,        // Neural9
    SPRITE_SET_URSA_PINK,           // Bineural5 / Neural13
    SPRITE_SET_URSA_PINK2,          // Neural10
    SPRITE_SET_URSA_ORANGE,         // Neural4
    SPRITE_SET_URSA_GREEN,          // Neural5
    SPRITE_SET_URSA_GREEN2,         // Neural11
    SPRITE_SET_URSA_GREEN3,         // Bineural1
    SPRITE_SET_URSA_BLUE3,          // Bineural2
    SPRITE_SET_URSA_MAGENTA,        // Neural6
    SPRITE_SET_URSA_RED,            // Neural7
    SPRITE_SET_URSA_ORANGE2,        // Neural12
    SPRITE_SET_URSA_GREEN4,         // Bineural3
    SPRITE_SET_URSA_ORANGE3,        // Bineural4

    SPRITE_SET_VEGA_BLUE,           // Matrix1
    SPRITE_SET_VEGA_ORANGE,
    SPRITE_SET_VEGA_PURPLE,
    SPRITE_SET_VEGA_GREEN,
    SPRITE_SET_VEGA_ORANGE2,
    SPRITE_SET_VEGA_ORANGE3,
    SPRITE_SET_VEGA_PURPLE2,
    SPRITE_SET_VEGA_BROWN,
    SPRITE_SET_VEGA_BLUE2,
    SPRITE_SET_VEGA_GREEN2,
    SPRITE_SET_VEGA_RED,
    SPRITE_SET_VEGA_ORANGE4,
    SPRITE_SET_VEGA_BLACK,
    SPRITE_SET_VEGA_GREY,
    SPRITE_SET_VEGA_YELLOW,
    SPRITE_SET_VEGA_RED2,

    SPRITE_SET_ARANEA_BLUE,
    SPRITE_SET_ARANEA_ORANGE,
    SPRITE_SET_ARANEA_GREY,
    SPRITE_SET_ARANEA_BROWN,
    SPRITE_SET_ARANEA_YELLOW,
    SPRITE_SET_ARANEA_RED,
    SPRITE_SET_ARANEA_GREEN,
    SPRITE_SET_ARANEA_YELLOW2,
    SPRITE_SET_ARANEA_BLACK,
    SPRITE_SET_ARANEA_WHITE,
    SPRITE_SET_ARANEA_BROWN2,
    SPRITE_SET_ARANEA_BLUE2,
    SPRITE_SET_ARANEA_GREY2,
    SPRITE_SET_ARANEA_BLACK2,
    SPRITE_SET_ARANEA_RED2,
    SPRITE_SET_ARANEA_GREEN2,
} SpriteSet;

typedef enum SpriteType {
    SPRITE_INVALID,

    SPRITE_SPACE_BLUE_BASE,
    SPRITE_SPACE_BLUE_FIGHTER,
    SPRITE_SPACE_BLUE_MISSILE,
    SPRITE_SPACE_BLUE_POWER_CORE,

    SPRITE_SPACE_PURPLE_BASE,
    SPRITE_SPACE_PURPLE_FIGHTER,
    SPRITE_SPACE_PURPLE_MISSILE,
    SPRITE_SPACE_PURPLE_POWER_CORE,

    SPRITE_SPACE_GREEN_BASE,
    SPRITE_SPACE_GREEN_FIGHTER,
    SPRITE_SPACE_GREEN_MISSILE,
    SPRITE_SPACE_GREEN_POWER_CORE,

    SPRITE_SPACE_GREEN2_BASE,
    SPRITE_SPACE_GREEN2_FIGHTER,
    SPRITE_SPACE_GREEN2_MISSILE,
    SPRITE_SPACE_GREEN2_POWER_CORE,

    SPRITE_SPACE_GREEN3_BASE,
    SPRITE_SPACE_GREEN3_FIGHTER,
    SPRITE_SPACE_GREEN3_MISSILE,
    SPRITE_SPACE_GREEN3_POWER_CORE,

    SPRITE_SPACE_YELLOW_BASE,
    SPRITE_SPACE_YELLOW_FIGHTER,
    SPRITE_SPACE_YELLOW_MISSILE,
    SPRITE_SPACE_YELLOW_POWER_CORE,

    SPRITE_SPACE_ORANGE_BASE,
    SPRITE_SPACE_ORANGE_FIGHTER,
    SPRITE_SPACE_ORANGE_MISSILE,
    SPRITE_SPACE_ORANGE_POWER_CORE,

    SPRITE_SPACE_RED_BASE,
    SPRITE_SPACE_RED_FIGHTER,
    SPRITE_SPACE_RED_MISSILE,
    SPRITE_SPACE_RED_POWER_CORE,

    SPRITE_SPACE_PURPLE2_BASE,
    SPRITE_SPACE_PURPLE2_FIGHTER,
    SPRITE_SPACE_PURPLE2_MISSILE,
    SPRITE_SPACE_PURPLE2_POWER_CORE,

    SPRITE_SPACE_RED2_BASE,
    SPRITE_SPACE_RED2_FIGHTER,
    SPRITE_SPACE_RED2_MISSILE,
    SPRITE_SPACE_RED2_POWER_CORE,

    SPRITE_SPACE_WHITE_BASE,
    SPRITE_SPACE_WHITE_FIGHTER,
    SPRITE_SPACE_WHITE_MISSILE,
    SPRITE_SPACE_WHITE_POWER_CORE,

    SPRITE_SPACE_YELLOW2_BASE,
    SPRITE_SPACE_YELLOW2_FIGHTER,
    SPRITE_SPACE_YELLOW2_MISSILE,
    SPRITE_SPACE_YELLOW2_POWER_CORE,

    SPRITE_SPACE_BROWN_BASE,
    SPRITE_SPACE_BROWN_FIGHTER,
    SPRITE_SPACE_BROWN_MISSILE,
    SPRITE_SPACE_BROWN_POWER_CORE,

    SPRITE_SPACE_RED3_BASE,
    SPRITE_SPACE_RED3_FIGHTER,
    SPRITE_SPACE_RED3_MISSILE,
    SPRITE_SPACE_RED3_POWER_CORE,

    SPRITE_SPACE_PURPLE3_BASE,
    SPRITE_SPACE_PURPLE3_FIGHTER,
    SPRITE_SPACE_PURPLE3_MISSILE,
    SPRITE_SPACE_PURPLE3_POWER_CORE,

    SPRITE_NAJU_BLUE_BASE,
    SPRITE_NAJU_BLUE_FIGHTER1,
    SPRITE_NAJU_BLUE_FIGHTER2,
    SPRITE_NAJU_BLUE_FIGHTER3,
    SPRITE_NAJU_BLUE_FIGHTER4,
    SPRITE_NAJU_BLUE_POWER_CORE,
    SPRITE_NAJU_BLUE_MISSILE,

    SPRITE_NAJU_PURPLE_BASE,
    SPRITE_NAJU_PURPLE_FIGHTER1,
    SPRITE_NAJU_PURPLE_FIGHTER2,
    SPRITE_NAJU_PURPLE_FIGHTER3,
    SPRITE_NAJU_PURPLE_FIGHTER4,
    SPRITE_NAJU_PURPLE_POWER_CORE,
    SPRITE_NAJU_PURPLE_MISSILE,

    SPRITE_NAJU_GRAY_BASE,
    SPRITE_NAJU_GRAY_FIGHTER1,
    SPRITE_NAJU_GRAY_FIGHTER2,
    SPRITE_NAJU_GRAY_FIGHTER3,
    SPRITE_NAJU_GRAY_FIGHTER4,
    SPRITE_NAJU_GRAY_POWER_CORE,
    SPRITE_NAJU_GRAY_MISSILE,

    SPRITE_NAJU_YELLOW_BASE,
    SPRITE_NAJU_YELLOW_FIGHTER1,
    SPRITE_NAJU_YELLOW_FIGHTER2,
    SPRITE_NAJU_YELLOW_FIGHTER3,
    SPRITE_NAJU_YELLOW_FIGHTER4,
    SPRITE_NAJU_YELLOW_POWER_CORE,
    SPRITE_NAJU_YELLOW_MISSILE,

    SPRITE_NAJU_GREEN_BASE,
    SPRITE_NAJU_GREEN_FIGHTER1,
    SPRITE_NAJU_GREEN_FIGHTER2,
    SPRITE_NAJU_GREEN_FIGHTER3,
    SPRITE_NAJU_GREEN_FIGHTER4,
    SPRITE_NAJU_GREEN_POWER_CORE,
    SPRITE_NAJU_GREEN_MISSILE,

    SPRITE_NAJU_RED_BASE,
    SPRITE_NAJU_RED_FIGHTER1,
    SPRITE_NAJU_RED_FIGHTER2,
    SPRITE_NAJU_RED_FIGHTER3,
    SPRITE_NAJU_RED_FIGHTER4,
    SPRITE_NAJU_RED_POWER_CORE,
    SPRITE_NAJU_RED_MISSILE,

    SPRITE_NAJU_BLUE2_BASE,
    SPRITE_NAJU_BLUE2_FIGHTER1,
    SPRITE_NAJU_BLUE2_FIGHTER2,
    SPRITE_NAJU_BLUE2_FIGHTER3,
    SPRITE_NAJU_BLUE2_FIGHTER4,
    SPRITE_NAJU_BLUE2_POWER_CORE,
    SPRITE_NAJU_BLUE2_MISSILE,

    SPRITE_NAJU_ORANGE_BASE,
    SPRITE_NAJU_ORANGE_FIGHTER1,
    SPRITE_NAJU_ORANGE_FIGHTER2,
    SPRITE_NAJU_ORANGE_FIGHTER3,
    SPRITE_NAJU_ORANGE_FIGHTER4,
    SPRITE_NAJU_ORANGE_POWER_CORE,
    SPRITE_NAJU_ORANGE_MISSILE,

    SPRITE_NAJU_TURQUOISE_BASE,
    SPRITE_NAJU_TURQUOISE_FIGHTER1,
    SPRITE_NAJU_TURQUOISE_FIGHTER2,
    SPRITE_NAJU_TURQUOISE_FIGHTER3,
    SPRITE_NAJU_TURQUOISE_FIGHTER4,
    SPRITE_NAJU_TURQUOISE_POWER_CORE,
    SPRITE_NAJU_TURQUOISE_MISSILE,

    SPRITE_NAJU_PURPLE2_BASE,
    SPRITE_NAJU_PURPLE2_FIGHTER1,
    SPRITE_NAJU_PURPLE2_FIGHTER2,
    SPRITE_NAJU_PURPLE2_FIGHTER3,
    SPRITE_NAJU_PURPLE2_FIGHTER4,
    SPRITE_NAJU_PURPLE2_POWER_CORE,
    SPRITE_NAJU_PURPLE2_MISSILE,

    SPRITE_NAJU_WHITE_BASE,
    SPRITE_NAJU_WHITE_FIGHTER1,
    SPRITE_NAJU_WHITE_FIGHTER2,
    SPRITE_NAJU_WHITE_FIGHTER3,
    SPRITE_NAJU_WHITE_FIGHTER4,
    SPRITE_NAJU_WHITE_POWER_CORE,
    SPRITE_NAJU_WHITE_MISSILE,

    SPRITE_NAJU_RED2_BASE,
    SPRITE_NAJU_RED2_FIGHTER1,
    SPRITE_NAJU_RED2_FIGHTER2,
    SPRITE_NAJU_RED2_FIGHTER3,
    SPRITE_NAJU_RED2_FIGHTER4,
    SPRITE_NAJU_RED2_POWER_CORE,
    SPRITE_NAJU_RED2_MISSILE,

    SPRITE_NAJU_YELLOW2_BASE,
    SPRITE_NAJU_YELLOW2_FIGHTER1,
    SPRITE_NAJU_YELLOW2_FIGHTER2,
    SPRITE_NAJU_YELLOW2_FIGHTER3,
    SPRITE_NAJU_YELLOW2_FIGHTER4,
    SPRITE_NAJU_YELLOW2_POWER_CORE,
    SPRITE_NAJU_YELLOW2_MISSILE,

    SPRITE_NAJU_MAGENTA_BASE,
    SPRITE_NAJU_MAGENTA_FIGHTER1,
    SPRITE_NAJU_MAGENTA_FIGHTER2,
    SPRITE_NAJU_MAGENTA_FIGHTER3,
    SPRITE_NAJU_MAGENTA_FIGHTER4,
    SPRITE_NAJU_MAGENTA_POWER_CORE,
    SPRITE_NAJU_MAGENTA_MISSILE,

    SPRITE_NAJU_ORANGE2_BASE,
    SPRITE_NAJU_ORANGE2_FIGHTER1,
    SPRITE_NAJU_ORANGE2_FIGHTER2,
    SPRITE_NAJU_ORANGE2_FIGHTER3,
    SPRITE_NAJU_ORANGE2_FIGHTER4,
    SPRITE_NAJU_ORANGE2_POWER_CORE,
    SPRITE_NAJU_ORANGE2_MISSILE,

    SPRITE_NAJU_YELLOW3_BASE,
    SPRITE_NAJU_YELLOW3_FIGHTER1,
    SPRITE_NAJU_YELLOW3_FIGHTER2,
    SPRITE_NAJU_YELLOW3_FIGHTER3,
    SPRITE_NAJU_YELLOW3_FIGHTER4,
    SPRITE_NAJU_YELLOW3_POWER_CORE,
    SPRITE_NAJU_YELLOW3_MISSILE,

    SPRITE_ALTAIR_PURPLE_BASE,
    SPRITE_ALTAIR_PURPLE_FIGHTER,
    SPRITE_ALTAIR_PURPLE_MISSILE,
    SPRITE_ALTAIR_PURPLE_POWER_CORE,

    SPRITE_ALTAIR_PURPLE2_BASE,
    SPRITE_ALTAIR_PURPLE2_FIGHTER,
    SPRITE_ALTAIR_PURPLE2_MISSILE,
    SPRITE_ALTAIR_PURPLE2_POWER_CORE,

    SPRITE_ALTAIR_YELLOW_BASE,
    SPRITE_ALTAIR_YELLOW_FIGHTER,
    SPRITE_ALTAIR_YELLOW_MISSILE,
    SPRITE_ALTAIR_YELLOW_POWER_CORE,

    SPRITE_ALTAIR_RED2_BASE,
    SPRITE_ALTAIR_RED2_FIGHTER,
    SPRITE_ALTAIR_RED2_MISSILE,
    SPRITE_ALTAIR_RED2_POWER_CORE,

    SPRITE_ALTAIR_GREEN_BASE,
    SPRITE_ALTAIR_GREEN_FIGHTER,
    SPRITE_ALTAIR_GREEN_MISSILE,
    SPRITE_ALTAIR_GREEN_POWER_CORE,

    SPRITE_ALTAIR_GREEN2_BASE,
    SPRITE_ALTAIR_GREEN2_FIGHTER,
    SPRITE_ALTAIR_GREEN2_MISSILE,
    SPRITE_ALTAIR_GREEN2_POWER_CORE,

    SPRITE_ALTAIR_BLUE_BASE,
    SPRITE_ALTAIR_BLUE_FIGHTER,
    SPRITE_ALTAIR_BLUE_MISSILE,
    SPRITE_ALTAIR_BLUE_POWER_CORE,

    SPRITE_ALTAIR_BLUE2_BASE,
    SPRITE_ALTAIR_BLUE2_FIGHTER,
    SPRITE_ALTAIR_BLUE2_MISSILE,
    SPRITE_ALTAIR_BLUE2_POWER_CORE,

    SPRITE_ALTAIR_MAGENTA_BASE,
    SPRITE_ALTAIR_MAGENTA_FIGHTER,
    SPRITE_ALTAIR_MAGENTA_MISSILE,
    SPRITE_ALTAIR_MAGENTA_POWER_CORE,

    SPRITE_ALTAIR_RED_BASE,
    SPRITE_ALTAIR_RED_FIGHTER,
    SPRITE_ALTAIR_RED_MISSILE,
    SPRITE_ALTAIR_RED_POWER_CORE,

    SPRITE_ALTAIR_RED3_BASE,
    SPRITE_ALTAIR_RED3_FIGHTER,
    SPRITE_ALTAIR_RED3_MISSILE,
    SPRITE_ALTAIR_RED3_POWER_CORE,

    SPRITE_ALTAIR_GREEN3_BASE,
    SPRITE_ALTAIR_GREEN3_FIGHTER,
    SPRITE_ALTAIR_GREEN3_MISSILE,
    SPRITE_ALTAIR_GREEN3_POWER_CORE,

    SPRITE_ALTAIR_ORANGE_BASE,
    SPRITE_ALTAIR_ORANGE_FIGHTER,
    SPRITE_ALTAIR_ORANGE_MISSILE,
    SPRITE_ALTAIR_ORANGE_POWER_CORE,

    SPRITE_ALTAIR_ORANGE2_BASE,
    SPRITE_ALTAIR_ORANGE2_FIGHTER,
    SPRITE_ALTAIR_ORANGE2_MISSILE,
    SPRITE_ALTAIR_ORANGE2_POWER_CORE,

    SPRITE_ALTAIR_YELLOW2_BASE,
    SPRITE_ALTAIR_YELLOW2_FIGHTER,
    SPRITE_ALTAIR_YELLOW2_MISSILE,
    SPRITE_ALTAIR_YELLOW2_POWER_CORE,

    SPRITE_ALTAIR_BLUE3_BASE,
    SPRITE_ALTAIR_BLUE3_FIGHTER,
    SPRITE_ALTAIR_BLUE3_MISSILE,
    SPRITE_ALTAIR_BLUE3_POWER_CORE,

    SPRITE_URSA_BLUE_BASE,
    SPRITE_URSA_BLUE_FIGHTER,
    SPRITE_URSA_BLUE_MISSILE,
    SPRITE_URSA_BLUE_POWER_CORE,

    SPRITE_URSA_BLUE2_BASE,
    SPRITE_URSA_BLUE2_FIGHTER,
    SPRITE_URSA_BLUE2_MISSILE,
    SPRITE_URSA_BLUE2_POWER_CORE,

    SPRITE_URSA_PURPLE_BASE,
    SPRITE_URSA_PURPLE_FIGHTER,
    SPRITE_URSA_PURPLE_MISSILE,
    SPRITE_URSA_PURPLE_POWER_CORE,

    SPRITE_URSA_PURPLE2_BASE,
    SPRITE_URSA_PURPLE2_FIGHTER,
    SPRITE_URSA_PURPLE2_MISSILE,
    SPRITE_URSA_PURPLE2_POWER_CORE,

    SPRITE_URSA_PINK_BASE,
    SPRITE_URSA_PINK_FIGHTER,
    SPRITE_URSA_PINK_MISSILE,
    SPRITE_URSA_PINK_POWER_CORE,

    SPRITE_URSA_PINK2_BASE,
    SPRITE_URSA_PINK2_FIGHTER,
    SPRITE_URSA_PINK2_MISSILE,
    SPRITE_URSA_PINK2_POWER_CORE,

    SPRITE_URSA_ORANGE_BASE,
    SPRITE_URSA_ORANGE_FIGHTER,
    SPRITE_URSA_ORANGE_MISSILE,
    SPRITE_URSA_ORANGE_POWER_CORE,

    SPRITE_URSA_GREEN_BASE,
    SPRITE_URSA_GREEN_FIGHTER,
    SPRITE_URSA_GREEN_MISSILE,
    SPRITE_URSA_GREEN_POWER_CORE,

    SPRITE_URSA_GREEN2_BASE,
    SPRITE_URSA_GREEN2_FIGHTER,
    SPRITE_URSA_GREEN2_MISSILE,
    SPRITE_URSA_GREEN2_POWER_CORE,

    SPRITE_URSA_GREEN3_BASE,
    SPRITE_URSA_GREEN3_FIGHTER,
    SPRITE_URSA_GREEN3_MISSILE,
    SPRITE_URSA_GREEN3_POWER_CORE,

    SPRITE_URSA_BLUE3_BASE,
    SPRITE_URSA_BLUE3_FIGHTER,
    SPRITE_URSA_BLUE3_MISSILE,
    SPRITE_URSA_BLUE3_POWER_CORE,

    SPRITE_URSA_MAGENTA_BASE,
    SPRITE_URSA_MAGENTA_FIGHTER,
    SPRITE_URSA_MAGENTA_MISSILE,
    SPRITE_URSA_MAGENTA_POWER_CORE,

    SPRITE_URSA_RED_BASE,
    SPRITE_URSA_RED_FIGHTER,
    SPRITE_URSA_RED_MISSILE,
    SPRITE_URSA_RED_POWER_CORE,

    SPRITE_URSA_ORANGE2_BASE,
    SPRITE_URSA_ORANGE2_FIGHTER,
    SPRITE_URSA_ORANGE2_MISSILE,
    SPRITE_URSA_ORANGE2_POWER_CORE,

    SPRITE_URSA_GREEN4_BASE,
    SPRITE_URSA_GREEN4_FIGHTER,
    SPRITE_URSA_GREEN4_MISSILE,
    SPRITE_URSA_GREEN4_POWER_CORE,

    SPRITE_URSA_ORANGE3_BASE,
    SPRITE_URSA_ORANGE3_FIGHTER,
    SPRITE_URSA_ORANGE3_MISSILE,
    SPRITE_URSA_ORANGE3_POWER_CORE,

    SPRITE_VEGA_BLUE_BASE,
    SPRITE_VEGA_BLUE_FIGHTER,
    SPRITE_VEGA_BLUE_MISSILE,
    SPRITE_VEGA_BLUE_POWER_CORE,

    SPRITE_VEGA_ORANGE_BASE,
    SPRITE_VEGA_ORANGE_FIGHTER,
    SPRITE_VEGA_ORANGE_MISSILE,
    SPRITE_VEGA_ORANGE_POWER_CORE,

    SPRITE_VEGA_PURPLE_BASE,
    SPRITE_VEGA_PURPLE_FIGHTER,
    SPRITE_VEGA_PURPLE_MISSILE,
    SPRITE_VEGA_PURPLE_POWER_CORE,

    SPRITE_VEGA_GREEN_BASE,
    SPRITE_VEGA_GREEN_FIGHTER,
    SPRITE_VEGA_GREEN_MISSILE,
    SPRITE_VEGA_GREEN_POWER_CORE,

    SPRITE_VEGA_ORANGE2_BASE,
    SPRITE_VEGA_ORANGE2_FIGHTER,
    SPRITE_VEGA_ORANGE2_MISSILE,
    SPRITE_VEGA_ORANGE2_POWER_CORE,

    SPRITE_VEGA_ORANGE3_BASE,
    SPRITE_VEGA_ORANGE3_FIGHTER,
    SPRITE_VEGA_ORANGE3_MISSILE,
    SPRITE_VEGA_ORANGE3_POWER_CORE,

    SPRITE_VEGA_PURPLE2_BASE,
    SPRITE_VEGA_PURPLE2_FIGHTER,
    SPRITE_VEGA_PURPLE2_MISSILE,
    SPRITE_VEGA_PURPLE2_POWER_CORE,

    SPRITE_VEGA_BROWN_BASE,
    SPRITE_VEGA_BROWN_FIGHTER,
    SPRITE_VEGA_BROWN_MISSILE,
    SPRITE_VEGA_BROWN_POWER_CORE,

    SPRITE_VEGA_BLUE2_BASE,
    SPRITE_VEGA_BLUE2_FIGHTER,
    SPRITE_VEGA_BLUE2_MISSILE,
    SPRITE_VEGA_BLUE2_POWER_CORE,

    SPRITE_VEGA_GREEN2_BASE,
    SPRITE_VEGA_GREEN2_FIGHTER,
    SPRITE_VEGA_GREEN2_MISSILE,
    SPRITE_VEGA_GREEN2_POWER_CORE,

    SPRITE_VEGA_RED_BASE,
    SPRITE_VEGA_RED_FIGHTER,
    SPRITE_VEGA_RED_MISSILE,
    SPRITE_VEGA_RED_POWER_CORE,

    SPRITE_VEGA_ORANGE4_BASE,
    SPRITE_VEGA_ORANGE4_FIGHTER,
    SPRITE_VEGA_ORANGE4_MISSILE,
    SPRITE_VEGA_ORANGE4_POWER_CORE,

    SPRITE_VEGA_BLACK_BASE,
    SPRITE_VEGA_BLACK_FIGHTER,
    SPRITE_VEGA_BLACK_MISSILE,
    SPRITE_VEGA_BLACK_POWER_CORE,

    SPRITE_VEGA_GREY_BASE,
    SPRITE_VEGA_GREY_FIGHTER,
    SPRITE_VEGA_GREY_MISSILE,
    SPRITE_VEGA_GREY_POWER_CORE,

    SPRITE_VEGA_YELLOW_BASE,
    SPRITE_VEGA_YELLOW_FIGHTER,
    SPRITE_VEGA_YELLOW_MISSILE,
    SPRITE_VEGA_YELLOW_POWER_CORE,

    SPRITE_VEGA_RED2_BASE,
    SPRITE_VEGA_RED2_FIGHTER,
    SPRITE_VEGA_RED2_MISSILE,
    SPRITE_VEGA_RED2_POWER_CORE,

    SPRITE_ARANEA_BLUE_BASE,
    SPRITE_ARANEA_BLUE_FIGHTER,
    SPRITE_ARANEA_BLUE_MISSILE,
    SPRITE_ARANEA_BLUE_POWER_CORE,

    SPRITE_ARANEA_ORANGE_BASE,
    SPRITE_ARANEA_ORANGE_FIGHTER,
    SPRITE_ARANEA_ORANGE_MISSILE,
    SPRITE_ARANEA_ORANGE_POWER_CORE,

    SPRITE_ARANEA_GREY_BASE,
    SPRITE_ARANEA_GREY_FIGHTER,
    SPRITE_ARANEA_GREY_MISSILE,
    SPRITE_ARANEA_GREY_POWER_CORE,

    SPRITE_ARANEA_BROWN_BASE,
    SPRITE_ARANEA_BROWN_FIGHTER,
    SPRITE_ARANEA_BROWN_MISSILE,
    SPRITE_ARANEA_BROWN_POWER_CORE,

    SPRITE_ARANEA_YELLOW_BASE,
    SPRITE_ARANEA_YELLOW_FIGHTER,
    SPRITE_ARANEA_YELLOW_MISSILE,
    SPRITE_ARANEA_YELLOW_POWER_CORE,

    SPRITE_ARANEA_RED_BASE,
    SPRITE_ARANEA_RED_FIGHTER,
    SPRITE_ARANEA_RED_MISSILE,
    SPRITE_ARANEA_RED_POWER_CORE,

    SPRITE_ARANEA_GREEN_BASE,
    SPRITE_ARANEA_GREEN_FIGHTER,
    SPRITE_ARANEA_GREEN_MISSILE,
    SPRITE_ARANEA_GREEN_POWER_CORE,

    SPRITE_ARANEA_YELLOW2_BASE,
    SPRITE_ARANEA_YELLOW2_FIGHTER,
    SPRITE_ARANEA_YELLOW2_MISSILE,
    SPRITE_ARANEA_YELLOW2_POWER_CORE,

    SPRITE_ARANEA_BLACK_BASE,
    SPRITE_ARANEA_BLACK_FIGHTER,
    SPRITE_ARANEA_BLACK_MISSILE,
    SPRITE_ARANEA_BLACK_POWER_CORE,

    SPRITE_ARANEA_WHITE_BASE,
    SPRITE_ARANEA_WHITE_FIGHTER,
    SPRITE_ARANEA_WHITE_MISSILE,
    SPRITE_ARANEA_WHITE_POWER_CORE,

    SPRITE_ARANEA_BROWN2_BASE,
    SPRITE_ARANEA_BROWN2_FIGHTER,
    SPRITE_ARANEA_BROWN2_MISSILE,
    SPRITE_ARANEA_BROWN2_POWER_CORE,

    SPRITE_ARANEA_BLUE2_BASE,
    SPRITE_ARANEA_BLUE2_FIGHTER,
    SPRITE_ARANEA_BLUE2_MISSILE,
    SPRITE_ARANEA_BLUE2_POWER_CORE,

    SPRITE_ARANEA_GREY2_BASE,
    SPRITE_ARANEA_GREY2_FIGHTER,
    SPRITE_ARANEA_GREY2_MISSILE,
    SPRITE_ARANEA_GREY2_POWER_CORE,

    SPRITE_ARANEA_BLACK2_BASE,
    SPRITE_ARANEA_BLACK2_FIGHTER,
    SPRITE_ARANEA_BLACK2_MISSILE,
    SPRITE_ARANEA_BLACK2_POWER_CORE,

    SPRITE_ARANEA_RED2_BASE,
    SPRITE_ARANEA_RED2_FIGHTER,
    SPRITE_ARANEA_RED2_MISSILE,
    SPRITE_ARANEA_RED2_POWER_CORE,

    SPRITE_ARANEA_GREEN2_BASE,
    SPRITE_ARANEA_GREEN2_FIGHTER,
    SPRITE_ARANEA_GREEN2_MISSILE,
    SPRITE_ARANEA_GREEN2_POWER_CORE,

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
