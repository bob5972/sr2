/*
 * metaFleet.cpp -- part of SpaceRobots2
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

extern "C" {
#include "fleet.h"
#include "Random.h"
#include "battle.h"
}

#include "MBVarMap.h"

#include "mutate.h"

#include "sensorGrid.hpp"
#include "shipAI.hpp"

static void *MetaFleetMobSpawned(void *aiHandle, Mob *m);
static void MetaFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

class MetaFleet {
public:
    MetaFleet(FleetAI *ai)
    :sg()
    {
        this->myAI = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        mreg = MBRegistry_AllocCopy(ai->player.mreg);
        loadRegistry();

        this->holdFleetSpawnRate =
            MBRegistry_GetFloat(mreg, "holdFleetSpawnRate");

        // XXX: Should match Mutate.
        MBUtil_Zero(&this->squadAI, sizeof(this->squadAI));
        ASSERT(ARRAYSIZE(this->squadAI) == 2);
        this->squadAI[0].ops.aiType = FLEET_AI_HOLD;
        this->squadAI[1].ops.aiType = FLEET_AI_FLOCK4;

        for (uint i = 0; i < ARRAYSIZE(this->squadAI); i++) {
            FleetAI *squadAI = &this->squadAI[i];
            uint64 seed = RandomState_Uint64(&this->rs);
            Fleet_CreateAI(squadAI, squadAI->ops.aiType,
                           ai->id, &ai->bp, &ai->player, seed);
        }
    }

    ~MetaFleet() {
        for (uint i = 0; i <ARRAYSIZE(this->squadAI); i++) {
            Fleet_DestroyAI(&this->squadAI[i]);
        }

        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    void loadRegistry(void) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            // MetaFleet-specific options
            { "holdFleetSpawnRate",  "0.25", },
        };

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (!MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_PutConst(mreg, configs[i].key, configs[i].value);
            }
        }
    }

    FleetAI *myAI;
    RandomState rs;
    SensorGrid sg;
    IntMap mobMap;

    FleetAI squadAI[2];


    float holdFleetSpawnRate;
    MBRegistry *mreg;
};

static void *MetaFleetCreate(FleetAI *ai);
static void MetaFleetDestroy(void *aiHandle);
static void MetaFleetRunAITick(void *aiHandle);
static void MetaFleetMutate(FleetAIType aiType, MBRegistry *mreg);

void MetaFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "MetaFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &MetaFleetCreate;
    ops->destroyFleet = &MetaFleetDestroy;
    ops->runAITick = &MetaFleetRunAITick;
    ops->mobSpawned = &MetaFleetMobSpawned;
    ops->mobDestroyed = &MetaFleetMobDestroyed;
    ops->mutateParams = &MetaFleetMutate;
}

static void MetaFleetMutate(FleetAIType aiType, MBRegistry *mreg)
{
    MutationFloatParams bvf[] = {
        // key                     min    max      mag   jump   mutation
        { "holdFleetSpawnRate",   0.01f,   1.0f,  0.05f, 0.15f, 0.02f},
    };

    Mutate_Float(mreg, bvf, ARRAYSIZE(bvf));

    // XXX: Should match the constructor.
    Fleet_Mutate(FLEET_AI_FLOCK4, mreg);
    Fleet_Mutate(FLEET_AI_HOLD, mreg);
}

static void *MetaFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new MetaFleet(ai);
}

static void MetaFleetDestroy(void *handle)
{
    MetaFleet *sf = (MetaFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *MetaFleetMobSpawned(void *aiHandle, Mob *m)
{
    MetaFleet *sf = (MetaFleet *)aiHandle;
    uint i;
    FleetAI *squadAI;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    ASSERT(ARRAYSIZE(sf->squadAI) == 2);
    if (RandomState_Flip(&sf->rs, sf->holdFleetSpawnRate)) {
        i = 0;
        ASSERT(sf->squadAI[i].ops.aiType == FLEET_AI_HOLD);
    } else {
        i = 1;
    }

    ASSERT(!sf->mobMap.containsKey(m->mobid));
    sf->mobMap.put(m->mobid, i);

    squadAI = &sf->squadAI[i];
    MobPSet_Add(&squadAI->mobs, m);

    if (squadAI->ops.mobSpawned != NULL) {
        void *aiMobHandle;
        aiMobHandle = squadAI->ops.mobSpawned(squadAI->aiHandle, m);
        ASSERT(aiMobHandle == NULL);
        UNUSED_VARIABLE(aiMobHandle);
    }

    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void MetaFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    MetaFleet *sf = (MetaFleet *)aiHandle;

    ASSERT(sf->mobMap.containsKey(m->mobid));

    uint i = sf->mobMap.get(m->mobid);
    FleetAI *squadAI = &sf->squadAI[i];
    if (squadAI->ops.mobDestroyed != NULL) {
        squadAI->ops.mobDestroyed(squadAI->aiHandle, m, aiMobHandle);
    }
    sf->mobMap.remove(m->mobid);
}

static void MetaFleetRunAITick(void *aiHandle)
{
    MetaFleet *sf = (MetaFleet *)aiHandle;
    CMobIt mit;

    ASSERT(sf->myAI->player.aiType == FLEET_AI_META);

    for (uint i = 0; i < ARRAYSIZE(sf->squadAI); i++) {
        FleetAI *squadAI = &sf->squadAI[i];
        MobPSet_MakeEmpty(&squadAI->mobs);
        MobPSet_MakeEmpty(&squadAI->sensors);
        squadAI->credits = sf->myAI->credits;
        squadAI->tick = sf->myAI->tick;
    }

    CMobIt_Start(&sf->myAI->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *m = CMobIt_Next(&mit);
        ASSERT(sf->mobMap.containsKey(m->mobid));
        uint i = sf->mobMap.get(m->mobid);
        FleetAI *squadAI = &sf->squadAI[i];
        MobPSet_Add(&squadAI->mobs, m);
    }

    CMobIt_Start(&sf->myAI->sensors, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *m = CMobIt_Next(&mit);
        for (uint i = 0; i < ARRAYSIZE(sf->squadAI); i++) {
            FleetAI *squadAI = &sf->squadAI[i];
            MobPSet_Add(&squadAI->sensors, m);
        }
    }

    for (uint i = 0; i <ARRAYSIZE(sf->squadAI); i++) {
        FleetAI *squadAI = &sf->squadAI[i];
        if (squadAI->ops.runAITick != NULL) {
            squadAI->ops.runAITick(squadAI->aiHandle);
        }
    }
}
