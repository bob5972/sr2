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
#include "random.h"

struct Fleet;
typedef struct Fleet Fleet;

const char *Fleet_GetName(FleetAIType fleetAI);
Fleet *Fleet_Create(const BattleScenario *bsc, uint64 seed);
void Fleet_Destroy(Fleet *fleet);
void Fleet_RunTick(Fleet *fleet, const BattleStatus *bs,
                   Mob *mobs, uint32 numMobs);

Mob *FleetUtil_FindClosestMob(MobPSet *ms, const FPoint *pos, uint filter);
Mob *FleetUtil_FindClosestMobInRange(MobPSet *ms, const FPoint *pos, uint filter,
                                     float radius);
Mob *FleetUtil_FindClosestSensor(FleetAI *ai, const FPoint *pos, uint filter);
void FleetUtil_RandomPointInRange(RandomState *rs, FPoint *p,
                                  const FPoint *center, float radius);
void FleetUtil_SortMobPByDistance(MobPVec *mobs, const FPoint *pos);
int FleetUtil_FindNthClosestMobP(MobPVec *mobps, const FPoint *pos, int n);

void DummyFleet_GetOps(FleetAIOps *ops);
void SimpleFleet_GetOps(FleetAIOps *ops);
void BobFleet_GetOps(FleetAIOps *ops);
void BasicFleet_GetOps(FleetAIOps *ops);
void MapperFleet_GetOps(FleetAIOps *ops);
void CloudFleet_GetOps(FleetAIOps *ops);
void GatherFleet_GetOps(FleetAIOps *ops);
void CowardFleet_GetOps(FleetAIOps *ops);
void RunAwayFleet_GetOps(FleetAIOps *ops);

#endif // _FLEET_H_202005311442
