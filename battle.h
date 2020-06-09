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

void Battle_Init(const BattleParams *bp);
void Battle_Exit();
void Battle_RunTick();
const BattleParams *Battle_GetParams();
Mob *Battle_AcquireMobs(uint32 *numMobs);
void Battle_ReleaseMobs();
const BattleStatus *Battle_AcquireStatus();
void Battle_ReleaseStatus();


#endif // _BATTLE_H_202005310644
