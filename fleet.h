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

void DummyFleet_GetOps(FleetAIOps *ops);
void SimpleFleet_GetOps(FleetAIOps *ops);
void BobFleet_GetOps(FleetAIOps *ops);
void HoldFleet_GetOps(FleetAIOps *ops);
void BasicFleet_GetOps(FleetAIOps *ops);
void MapperFleet_GetOps(FleetAIOps *ops);
void CloudFleet_GetOps(FleetAIOps *ops);
void GatherFleet_GetOps(FleetAIOps *ops);
void CowardFleet_GetOps(FleetAIOps *ops);
void RunAwayFleet_GetOps(FleetAIOps *ops);
void CircleFleet_GetOps(FleetAIOps *ops);
void FlockFleetLite_GetOps(FleetAIOps *ops);
void FlockFleet_GetOps(FleetAIOps *ops);
void FlockFleetHeavy_GetOps(FleetAIOps *ops);

static inline void NeutralFleet_GetOps(FleetAIOps *ops)
{
    DummyFleet_GetOps(ops);
    ops->aiName = "Neutral";
}

static inline void Fleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    struct {
        FleetAIType aiType;
        void (*getOps)(FleetAIOps *ops);
    } fleets[] = {
        { FLEET_AI_NEUTRAL,     NeutralFleet_GetOps     },
        { FLEET_AI_DUMMY,       DummyFleet_GetOps       },
        { FLEET_AI_SIMPLE,      SimpleFleet_GetOps      },
        { FLEET_AI_BOB,         BobFleet_GetOps         },
        { FLEET_AI_MAPPER,      MapperFleet_GetOps      },
        { FLEET_AI_CLOUD,       CloudFleet_GetOps       },
        { FLEET_AI_GATHER,      GatherFleet_GetOps      },
        { FLEET_AI_COWARD,      CowardFleet_GetOps      },
        { FLEET_AI_RUNAWAY,     RunAwayFleet_GetOps     },
        { FLEET_AI_BASIC,       BasicFleet_GetOps       },
        { FLEET_AI_HOLD,        HoldFleet_GetOps        },
        { FLEET_AI_CIRCLE,      CircleFleet_GetOps      },
        { FLEET_AI_FLOCK_LITE,  FlockFleetLite_GetOps   },
        { FLEET_AI_FLOCK,       FlockFleet_GetOps       },
        { FLEET_AI_FLOCK_HEAVY, FlockFleetHeavy_GetOps  },
    };

    ASSERT(aiType != FLEET_AI_INVALID);
    ASSERT(aiType < FLEET_AI_MAX);
    ASSERT(FLEET_AI_MAX == 16);
    MBUtil_Zero(ops, sizeof(*ops));

    for (uint i = 0; i < ARRAYSIZE(fleets); i++ ) {
        if (fleets[i].aiType == aiType) {
            fleets[i].getOps(ops);
            ops->aiType = aiType;
            return;
        }
    }

    PANIC("Unknown AI type=%d\n", aiType);
}


static inline const char *Fleet_GetName(FleetAIType aiType)
{
    FleetAIOps ops;
    Fleet_GetOps(aiType, &ops);
    return ops.aiName;
}

static inline FleetAIType Fleet_GetTypeFromName(const char *name)
{
    uint32 i;

    ASSERT(FLEET_AI_NEUTRAL == 1);
    for (i = FLEET_AI_NEUTRAL; i < FLEET_AI_MAX; i++) {
        const char *fleetName = Fleet_GetName((FleetAIType)i);
        if (fleetName != NULL && strcmp(fleetName, name) == 0) {
            return (FleetAIType)i;
        }
    }

    return FLEET_AI_INVALID;
}

#endif // _FLEET_H_202005311442
