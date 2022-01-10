/*
 * battle.h -- part of SpaceRobots2
 * Copyright (C) 2020-2021 Michael Banack <github@banack.net>
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
#ifndef _BATTLE_H_202005310644
#define _BATTLE_H_202005310644

#include "geometry.h"
#include "mob.h"
#include "fleet.h"
#include "battleTypes.h"

struct Battle;
typedef struct Battle Battle;

Battle *Battle_Create(const BattleScenario *bsc, uint64 seed);
void Battle_Destroy(Battle *battle);
void Battle_RunTick(Battle *battle);
Mob *Battle_AcquireMobs(Battle *battle, uint32 *numMobs);
void Battle_ReleaseMobs(Battle *battle);
const BattleStatus *Battle_AcquireStatus(Battle *battle);
void Battle_ReleaseStatus(Battle *battle);

static INLINE const char *
PlayerType_ToString(PlayerType type)
{
    switch (type) {
        case PLAYER_TYPE_INVALID:
            return "PlayerTypeInvalid";
        case PLAYER_TYPE_NEUTRAL:
            return "Neutral";
        case PLAYER_TYPE_CONTROL:
            return "Control";
        case PLAYER_TYPE_TARGET:
            return "Target";
        default:
            NOT_IMPLEMENTED();
    }
}

static INLINE PlayerType
PlayerType_FromString(const char *str)
{
    if (str == NULL ||
        strcmp(str, "PlayerTypeInvalid") == 0) {
        return PLAYER_TYPE_INVALID;
    } else if (strcmp(str, "Neutral") == 0) {
        return PLAYER_TYPE_NEUTRAL;
    } else if (strcmp(str, "Control") == 0) {
        return PLAYER_TYPE_CONTROL;
    } else if (strcmp(str, "Target") == 0) {
        return PLAYER_TYPE_TARGET;
    } else {
        NOT_IMPLEMENTED();
    }
}

#endif // _BATTLE_H_202005310644
