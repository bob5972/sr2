/*
 * circleFleet.cpp -- part of SpaceRobots2
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

class CircleAIGovernor : public BasicAIGovernor
{
public:
    CircleAIGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {

    }

    virtual ~CircleAIGovernor() { }

    virtual void runMob(Mob *mob) {
        BasicShipAI *ship;
        BasicAIGovernor::runMob(mob);

        ship = (BasicShipAI *)getShip(mob->mobid);
        ASSERT(ship != NULL);

        if (mob->type == MOB_TYPE_FIGHTER &&
            ship->state == BSAI_STATE_IDLE) {
            Mob *base = mySensorGrid->friendBase();

            if (base != NULL) {
                FPoint idlePos;
                float radius = MobType_GetSensorRadius(MOB_TYPE_BASE);
                float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
                //float t = myFleetAI->tick * (2 * M_PI * radius) / speed;
                uint id = mob->mobid * mob->mobid;
                float t = (id + myFleetAI->tick) / (30 * speed);
                idlePos.x = base->pos.x + radius * cosf(t);
                idlePos.y = base->pos.y + radius * sinf(t);
                mob->cmd.target = idlePos;
            }
        }
    }
};

class CircleFleet {
public:
    CircleFleet(FleetAI *ai)
    :sg(), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        gov.setSeed(RandomState_Uint64(&this->rs));
    }

    ~CircleFleet() {
        RandomState_Destroy(&this->rs);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    CircleAIGovernor gov;
};

static void *CircleFleetCreate(FleetAI *ai);
static void CircleFleetDestroy(void *aiHandle);
static void CircleFleetRunAITick(void *aiHandle);
static void *CircleFleetMobSpawned(void *aiHandle, Mob *m);
static void CircleFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

void CircleFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "CircleFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &CircleFleetCreate;
    ops->destroyFleet = &CircleFleetDestroy;
    ops->runAITick = &CircleFleetRunAITick;
    ops->mobSpawned = CircleFleetMobSpawned;
    ops->mobDestroyed = CircleFleetMobDestroyed;
}

static void *CircleFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new CircleFleet(ai);
}

static void CircleFleetDestroy(void *handle)
{
    CircleFleet *sf = (CircleFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *CircleFleetMobSpawned(void *aiHandle, Mob *m)
{
    CircleFleet *sf = (CircleFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void CircleFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    CircleFleet *sf = (CircleFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void CircleFleetRunAITick(void *aiHandle)
{
    CircleFleet *sf = (CircleFleet *)aiHandle;
    FleetAI *ai = sf->ai;
    CMobIt mit;

    sf->sg.updateTick(ai);
    CMobIt_Start(&ai->mobs, &mit);
    sf->gov.runAllMobs(&mit);
}
