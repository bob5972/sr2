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
    { }

    virtual ~CircleAIGovernor() { }


    virtual void loadRegistry(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            { "gatherRange",          "100", },
            { "attackRange",          "250", },
        };

        mreg = MBRegistry_AllocCopy(mreg);

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (!MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_Put(mreg, configs[i].key, configs[i].value);
            }
        }

        this->BasicAIGovernor::loadRegistry(mreg);

        MBRegistry_Free(mreg);
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        FleetAI *ai = myFleetAI;
        RandomState *rs = &myRandomState;
        BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
        Mob *base = mySensorGrid->friendBase();

        ASSERT(ship != NULL);
        ASSERT(ship->state == BSAI_STATE_IDLE);

        if (mob->type != MOB_TYPE_FIGHTER || base == NULL) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        if (!newlyIdle) {
            return;
        }

        float baseRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
        float radius = baseRadius;

        //XXX: Per-ship radius?
        int numFriends = mySensorGrid->numFriends();
        uint maxDim = sqrtf(myFleetAI->bp.width * myFleetAI->bp.width +
                            myFleetAI->bp.height * myFleetAI->bp.height);
        radius *= expf(logf(1.01f) * (1 + numFriends));
        radius = MAX(50.0f, radius);
        radius = MIN(radius, maxDim);

        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        float angularSpeed = Float_AngularSpeed(radius, speed);

        FRPoint rPos;
        FPoint_ToFRPoint(&mob->pos, &base->pos, &rPos);

        if (FPoint_Distance(&base->pos, &mob->pos) <= 10.0f) {
            FleetUtil_RandomPointInRange(rs, &mob->cmd.target, &base->pos, baseRadius);
        } else if (!Float_Compare(rPos.radius, radius, MICRON)) {
            rPos.radius = radius;
            FRPoint_ToFPoint(&rPos, &base->pos, &mob->cmd.target);
        } else {
            rPos.theta += angularSpeed;
            FRPoint_ToFPoint(&rPos, &base->pos, &mob->cmd.target);
        }

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));

        /*
        * Deal with edge-cases so we can keep making forward progress.
        */
        bool clamped;
        clamped = FPoint_Clamp(&mob->cmd.target,
                               0.0f, ai->bp.width,
                               0.0f, ai->bp.height);
        if (clamped) {
            FPoint_ToFRPoint(&mob->cmd.target, &base->pos, &rPos);
            angularSpeed = Float_AngularSpeed(rPos.radius, speed);
            rPos.theta += MAX(0.5f, angularSpeed);
            FRPoint_ToFPoint(&rPos, &base->pos, &mob->cmd.target);

            clamped = FPoint_Clamp(&mob->cmd.target,
                                   0.0f, ai->bp.width,
                                   0.0f, ai->bp.height);
        }

        if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= speed/4.0f) {
            mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
            mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
        }
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
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

        mreg = MBRegistry_AllocCopy(ai->player.mreg);
        this->gov.loadRegistry(mreg);

    }

    ~CircleFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    CircleAIGovernor gov;
    MBRegistry *mreg;
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
