/*
 * fleet.h -- part of SpaceRobots2
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

#ifndef _FLEET_H_202005311442
#define _FLEET_H_202005311442

#include "mob.h"
#include "battleTypes.h"

void Fleet_Init();
void Fleet_Exit();
void Fleet_RunTick(const BattleStatus *bs, Mob *mobs, uint32 numMobs);


#define MOB_FILTER_BASE     (1 << MOB_TYPE_BASE)
#define MOB_FILTER_FIGHTER  (1 << MOB_TYPE_FIGHTER)
#define MOB_FILTER_MISSILE  (1 << MOB_TYPE_MISSILE)
#define MOB_FILTER_LOOT_BOX (1 << MOB_TYPE_LOOT_BOX)
#define MOB_FILTER_SHIP (MOB_FILTER_BASE | MOB_FILTER_FIGHTER)
#define MOB_FILTER_ALL (MOB_FILTER_SHIP |    \
                        MOB_FILTER_MISSILE | \
                        MOB_FILTER_LOOT_BOX)

// Deprecated: use MOB_FILTER defines
#define FLEET_SCAN_BASE     MOB_FILTER_BASE
#define FLEET_SCAN_FIGHTER  MOB_FILTER_FIGHTER
#define FLEET_SCAN_MISSILE  MOB_FILTER_MISSILE
#define FLEET_SCAN_LOOT_BOX MOB_FILTER_LOOT_BOX
#define FLEET_SCAN_SHIP     MOB_FILTER_SHIP
#define FLEET_SCAN_ALL      MOB_FILTER_ALL

int FleetUtil_FindClosestMob(MobVector *mobs, const FPoint *pos, uint filter);
int FleetUtil_FindClosestSensor(FleetAI *ai, const FPoint *pos, uint filter);
void FleetUtil_SortMobsByDistance(MobVector *mobs, const FPoint *pos);
void FleetUtil_RandomPointInRange(FPoint *p, const FPoint *center, float radius);
Mob *FleetUtil_GetMob(FleetAI *ai, MobID mobid);
void FleetUtil_UpdateMobMap(FleetAI *ai);
MobPVec *FleetUtil_AllocMobPVec(MobVector *mobs);
void FleetUtil_FreeMobPVec(MobPVec *mobps);
void FleetUtil_SortMobPByDistance(MobPVec *mobs, const FPoint *pos);
int FleetUtil_FindNthClosestMobP(MobPVec *mobps, const FPoint *pos, int n);

void SimpleFleet_GetOps(FleetAIOps *ops);
void BobFleet_GetOps(FleetAIOps *ops);
void MapperFleet_GetOps(FleetAIOps *ops);
void CloudFleet_GetOps(FleetAIOps *ops);
void GatherFleet_GetOps(FleetAIOps *ops);

#endif // _FLEET_H_202005311442
