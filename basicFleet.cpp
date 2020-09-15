/*
 * basicFleet.cpp -- part of SpaceRobots2
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

class BasicFleet {
public:
    BasicFleet(FleetAI *ai)
    :sg(), basicGov(ai, &rs, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
    }

    ~BasicFleet() {
        RandomState_Destroy(&this->rs);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    BasicAIGovernor basicGov;
};

static void *BasicFleetCreate(FleetAI *ai);
static void BasicFleetDestroy(void *aiHandle);
static void BasicFleetRunAITick(void *aiHandle);
static void *BasicFleetMobSpawned(void *aiHandle, Mob *m);
static void BasicFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

#define ALLBASIC TRUE

void BasicFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "BasicFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BasicFleetCreate;
    ops->destroyFleet = &BasicFleetDestroy;
    ops->runAITick = &BasicFleetRunAITick;
    ops->mobSpawned = BasicFleetMobSpawned;
    ops->mobDestroyed = BasicFleetMobDestroyed;
}

static void *BasicFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new BasicFleet(ai);
}

static void BasicFleetDestroy(void *handle)
{
    BasicFleet *sf = (BasicFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *BasicFleetMobSpawned(void *aiHandle, Mob *m)
{
    BasicFleet *sf = (BasicFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->basicGov.addMobid(m->mobid);

    /*
    * We don't track anything else.
    */
    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void BasicFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    BasicFleet *sf = (BasicFleet *)aiHandle;

    sf->basicGov.removeMobid(m->mobid);
}

static void BasicFleetRunAITick(void *aiHandle)
{
    BasicFleet *sf = (BasicFleet *)aiHandle;
    FleetAI *ai = sf->ai;
    CMobIt mit;

    sf->sg.updateTick(ai);
    CMobIt_Start(&ai->mobs, &mit);
    sf->basicGov.runAllMobs(&mit);
}
