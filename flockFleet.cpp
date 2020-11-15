/*
 * flockFleet.cpp -- part of SpaceRobots2
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
#include "MBMap.hpp"

class FlockAIGovernor : public BasicAIGovernor
{
public:
    FlockAIGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    { }

    virtual ~FlockAIGovernor() { }


    virtual void loadRegistry(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            { "gatherAbandonStale",   "TRUE", },
            { "attackRange",          "250",  },
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

    void flockAlign(Mob *mob, FRPoint *rPos) {
        SensorGrid *sg = mySensorGrid;
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        float flockRadius = MobType_GetSensorRadius(MOB_TYPE_BASE) / 1.5f;

        ASSERT(mob->type == MOB_TYPE_FIGHTER);

        FPoint avgVel;
        sg->friendAvgVelocity(&avgVel, &mob->pos, flockRadius, MOB_FLAG_FIGHTER);

        FRPoint ravgVel;
        FPoint_ToFRPoint(&avgVel, NULL, &ravgVel);
        ravgVel.radius = speed;

        FRPoint_WAvg(rPos, 2.0f, &ravgVel, 1.0f, rPos);
        //rPos->radius *= 9.0f;
        //FRPoint_Add(&ravgVel, rPos, rPos);
        //rPos->radius /= 10.0f;
    }

    void flockSeparate(Mob *mob, FRPoint *rPos) {
        SensorGrid *sg = mySensorGrid;
        RandomState *rs = &myRandomState;
        float repulseRadius = 2.0f * MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);

        ASSERT(mob->type == MOB_TYPE_FIGHTER);

        uint n = 0;
        Mob *f = sg->findNthClosestFriend(&mob->pos, MOB_FLAG_FIGHTER, n++);

        while (f != NULL && FPoint_Distance(&f->pos, &mob->pos) <= repulseRadius) {
            if (f->mobid != mob->mobid) {
                FRPoint drp;

                FPoint_ToFRPoint(&f->pos, &mob->pos, &drp);

                float repulsion;

                if (drp.radius <= MICRON) {
                    drp.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
                    drp.radius = speed;
                } else {
                    float k = (drp.radius / repulseRadius) + 1.0f;
                    repulsion = 2.0f / (k * k);
                    drp.radius = -1.0f * speed * repulsion;
                }

                FRPoint_Add(&drp, rPos, rPos);
            }

            f = sg->findNthClosestFriend(&mob->pos, MOB_FLAG_FIGHTER, n++);
        }
    }

   virtual void doAttack(Mob *mob, Mob *enemyTarget) {
       float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
       BasicAIGovernor::doAttack(mob, enemyTarget);
       FRPoint rPos;
       FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);
       //flockAlign(mob, &rPos);
       flockSeparate(mob, &rPos);
       //flockCohere(mob, &rPos);

       rPos.radius = speed;
       FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
   }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        FleetAI *ai = myFleetAI;
        RandomState *rs = &myRandomState;
        SensorGrid *sg = mySensorGrid;
        BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
        //Mob *base = sg->friendBase();
        float baseRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
        float flockRadius = baseRadius;
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);

        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        if (!newlyIdle) {
            return;
        }

        if (sg->numFriendsInRange(MOB_FLAG_FIGHTER, &mob->pos, flockRadius) > 1) {
            FRPoint rPos;
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);
            flockAlign(mob, &rPos);
            flockSeparate(mob, &rPos);
            //flockCohere(mob, &rPos);

            rPos.radius = speed;
            FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
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
                FleetUtil_RandomPointInRange(rs, &mob->cmd.target,&mob->pos,
                                             flockRadius);
            }

            /*
            * If we still can't make enough forward progress, go somewhere random.
            */
            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= speed/4.0f) {
                mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
                mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
            }
        } else if (newlyIdle) {
            mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
            mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
        }

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }

    virtual void runTick() {
        //FleetAI *ai = myFleetAI;
        SensorGrid *sg = mySensorGrid;

        BasicAIGovernor::runTick();

        Mob *base = sg->friendBase();
        float baseRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);

        if (base != NULL) {
            int numEnemies = sg->numTargetsInRange(MOB_FLAG_SHIP, &base->pos,
                                                   baseRadius);
            int f = 0;
            int e = 0;

            Mob *fighter = sg->findNthClosestFriend(&base->pos,
                                                    MOB_FLAG_FIGHTER, f++);
            Mob *enemyTarget = sg->findNthClosestTarget(&base->pos,
                                                        MOB_FLAG_SHIP, e++);

            while (numEnemies > 0 && fighter != NULL) {
                BasicShipAI *ship = (BasicShipAI *)getShip(fighter->mobid);

                if (enemyTarget != NULL) {
                    ship->attack(enemyTarget);
                }

                fighter = sg->findNthClosestFriend(&base->pos,
                                                   MOB_FLAG_FIGHTER, f++);

                enemyTarget = sg->findNthClosestTarget(&base->pos,
                                                       MOB_FLAG_SHIP, e++);

                numEnemies--;
            }
        }
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
    }
};

class FlockFleet {
public:
    FlockFleet(FleetAI *ai)
    :sg(), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        gov.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);
        this->gov.loadRegistry(mreg);
    }

    ~FlockFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    FlockAIGovernor gov;
    MBRegistry *mreg;
};

static void *FlockFleetCreate(FleetAI *ai);
static void FlockFleetDestroy(void *aiHandle);
static void FlockFleetRunAITick(void *aiHandle);
static void *FlockFleetMobSpawned(void *aiHandle, Mob *m);
static void FlockFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

void FlockFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "FlockFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &FlockFleetCreate;
    ops->destroyFleet = &FlockFleetDestroy;
    ops->runAITick = &FlockFleetRunAITick;
    ops->mobSpawned = FlockFleetMobSpawned;
    ops->mobDestroyed = FlockFleetMobDestroyed;
}

static void *FlockFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new FlockFleet(ai);
}

static void FlockFleetDestroy(void *handle)
{
    FlockFleet *sf = (FlockFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *FlockFleetMobSpawned(void *aiHandle, Mob *m)
{
    FlockFleet *sf = (FlockFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void FlockFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    FlockFleet *sf = (FlockFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void FlockFleetRunAITick(void *aiHandle)
{
    FlockFleet *sf = (FlockFleet *)aiHandle;
    sf->gov.runTick();
}
