/*
 * display.h -- part of SpaceRobots2
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

#ifndef _DISPLAY_H_202005252017
#define _DISPLAY_H_202005252017

#include "battle.h"
#include "mob.h"

void Display_Init(const BattleParams *bp);
void Display_Exit();
Mob *Display_AcquireMobs(uint32 numMobs, bool frameSkip);
void Display_ReleaseMobs();
void Display_Main();

#endif // _DISPLAY_H_202005252017
