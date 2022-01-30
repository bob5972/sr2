/*
 * fleet.h -- part of SpaceRobots2
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

#ifndef _FLEET_H_202005311442
#define _FLEET_H_202005311442

#include "mob.h"
#include "battleTypes.h"
#include "Random.h"

struct Fleet;
typedef struct Fleet Fleet;

Fleet *Fleet_Create(const BattleScenario *bsc, uint64 seed);
void Fleet_Destroy(Fleet *fleet);
void Fleet_RunTick(Fleet *fleet, const BattleStatus *bs,
                   Mob *mobs, uint32 numMobs);

void Fleet_CreateAI(FleetAI *ai, FleetAIType aiType,
                    PlayerID id, const BattleParams *bp,
                    const BattlePlayer *player, uint64 seed);
void Fleet_DestroyAI(FleetAI *ai);

Mob *FleetUtil_FindClosestMob(MobPSet *ms, const FPoint *pos, uint filter);
Mob *FleetUtil_FindClosestMobInRange(MobPSet *ms, const FPoint *pos, uint filter,
                                     float radius);
Mob *FleetUtil_FindClosestSensor(FleetAI *ai, const FPoint *pos, uint filter);
void FleetUtil_RandomPointInRange(RandomState *rs, FPoint *p,
                                  const FPoint *center, float radius);
void FleetUtil_SortMobPByDistance(MobPVec *mobs, const FPoint *pos);
int FleetUtil_FindNthClosestMobP(MobPVec *mobps, const FPoint *pos, int n);

void DummyFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void SimpleFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void MetaFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void HoldFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void BasicFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void MapperFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void CloudFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void GatherFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void CowardFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void RunAwayFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void CircleFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void FlockFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
void BundleFleet_GetOps(FleetAIType aiType, FleetAIOps *ops);

bool Fleet_IsFlockFleet(FleetAIType aiType);
bool Fleet_IsBundleFleet(FleetAIType aiType);

void Fleet_GetOps(FleetAIType aiType, FleetAIOps *ops);
int Fleet_GetRanking(FleetAIType aiType);
FleetAIType Fleet_GetTypeFromRanking(int rank);
FleetAIType Fleet_GetTypeFromName(const char *name);


static inline void NeutralFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    DummyFleet_GetOps(aiType, ops);
    ops->aiName = "Neutral";
}

static inline const char *Fleet_GetName(FleetAIType aiType)
{
    FleetAIOps ops;
    Fleet_GetOps(aiType, &ops);
    return ops.aiName;
}


static inline void Fleet_Mutate(FleetAIType aiType, MBRegistry *mreg)
{
    FleetAIOps ops;
    Fleet_GetOps(aiType, &ops);

    if (ops.mutateParams != NULL) {
        ops.mutateParams(aiType, mreg);
    }
}

#endif // _FLEET_H_202005311442
