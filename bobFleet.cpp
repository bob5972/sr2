/*
 * bobFleet.cpp -- part of SpaceRobots2
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

extern "C" {
#include "fleet.h"
#include "random.h"
#include "IntMap.h"
#include "battle.h"
}

#include "sensorGrid.hpp"
#include "shipAI.hpp"

class BobFleet {
public:
    BobFleet(FleetAI *ai)
    :sg(), basicGov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);

        this->basicGov.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);

        struct {
            const char *key;
            const char *value;
        } configs[] = {
            { "evadeFighters",          "FALSE", },
            { "evadeUseStrictDistance", "TRUE",  },
            { "evadeStrictDistance",    "10",    },
            { "evadeHold",              "TRUE",  },
            { "holdCount",              "100",   },
        };

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (!MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_Put(mreg, configs[i].key, configs[i].value);
            }
        }

        this->basicGov.loadRegistry(mreg);
    }

    ~BobFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    BasicAIGovernor basicGov;
    MBRegistry *mreg;
};

static void *BobFleetCreate(FleetAI *ai);
static void BobFleetDestroy(void *aiHandle);
static void BobFleetRunAITick(void *aiHandle);
static void *BobFleetMobSpawned(void *aiHandle, Mob *m);
static void BobFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

void BobFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "BobFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BobFleetCreate;
    ops->destroyFleet = &BobFleetDestroy;
    ops->runAITick = &BobFleetRunAITick;
    ops->mobSpawned = BobFleetMobSpawned;
    ops->mobDestroyed = BobFleetMobDestroyed;
}

static void *BobFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new BobFleet(ai);
}

static void BobFleetDestroy(void *handle)
{
    BobFleet *sf = (BobFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *BobFleetMobSpawned(void *aiHandle, Mob *m)
{
    BobFleet *sf = (BobFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->basicGov.addMobid(m->mobid);
    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void BobFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    BobFleet *sf = (BobFleet *)aiHandle;
    sf->basicGov.removeMobid(m->mobid);
}

static void BobFleetRunAITick(void *aiHandle)
{
    BobFleet *sf = (BobFleet *)aiHandle;
    FleetAI *ai = sf->ai;
    //const BattleParams *bp = &sf->ai->bp;
    //uint targetScanFilter = MOB_FLAG_SHIP;
    //float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
    //                    MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    //float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE);
    CMobIt mit;

    ASSERT(ai->player.aiType == FLEET_AI_BOB);

    sf->sg.updateTick(ai);

    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);
        sf->basicGov.runMob(mob);
    }
}
