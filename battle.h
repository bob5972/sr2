/*
 * battle.h -- part of SpaceRobots2
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
#ifndef _BATTLE_H_202005310644
#define _BATTLE_H_202005310644

#include "geometry.h"
#include "mob.h"
#include "fleet.h"
#include "battleTypes.h"

struct Battle;
typedef struct Battle Battle;

Battle *Battle_Create(const BattleParams *bp, uint64 seed);
void Battle_Destroy(Battle *battle);
void Battle_RunTick(Battle *battle);
const BattleParams *Battle_GetParams(Battle *battle);
Mob *Battle_AcquireMobs(Battle *battle, uint32 *numMobs);
void Battle_ReleaseMobs(Battle *battle);
const BattleStatus *Battle_AcquireStatus(Battle *battle);
void Battle_ReleaseStatus(Battle *battle);

#endif // _BATTLE_H_202005310644
