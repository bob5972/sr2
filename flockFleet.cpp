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

    void flockAlign(Mob *mob, FRPoint *rPos, float flockRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;

//         {
//             FPoint vel;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             Warning("%s:%d  IN mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }


        FPoint avgVel;
        sg->friendAvgVelocity(&avgVel, &mob->pos, flockRadius, MOB_FLAG_FIGHTER);

        FRPoint ravgVel;
        FPoint_ToFRPoint(&avgVel, NULL, &ravgVel);
        ravgVel.radius = weight;

        FRPoint_Add(rPos, &ravgVel, rPos);

//         {
//             FPoint vel;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             Warning("%s:%d OUT mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }
    }

    void flockCohere(Mob *mob, FRPoint *rPos, float flockRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;

//         {
//             FPoint vel;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             Warning("%s:%d  IN mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }

        FPoint avgPos;
        sg->friendAvgPos(&avgPos, &mob->pos, flockRadius, MOB_FLAG_FIGHTER);

        FRPoint ravgPos;
        FPoint_ToFRPoint(&avgPos, NULL, &ravgPos);
        ravgPos.radius = weight;
        FRPoint_Add(rPos, &ravgPos, rPos);

//         {
//             FPoint vel;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             Warning("%s:%d OUT mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }
    }

    void repulseVector(FRPoint *repulseVec, FPoint *pos, FPoint *c,
                       float repulseRadius) {
        RandomState *rs = &myRandomState;

        FRPoint drp;

        FPoint_ToFRPoint(pos, c, &drp);

        ASSERT(drp.radius >= 0.0f);
        ASSERT(repulseRadius >= 0.0f);

        if (drp.radius <= MICRON) {
            drp.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
            drp.radius = 1.0f;
        } else {
            float repulsion;
            float k = (drp.radius / repulseRadius) + 1.0f;
            repulsion = 1.0f / (k * k);
            drp.radius = -1.0f * repulsion;
        }

        FRPoint_Add(&drp, repulseVec, repulseVec);
    }

    void flockSeparate(Mob *mob, FRPoint *rPos, float flockRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;

//         {
//             FPoint vel;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             Warning("%s:%d  IN mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }

        uint n = 0;
        FRPoint repulseVec;
        Mob *f = sg->findNthClosestFriend(&mob->pos, MOB_FLAG_FIGHTER, n++);

        repulseVec.radius = 0.0f;
        repulseVec.theta = 0.0f;

        while (f != NULL && FPoint_Distance(&f->pos, &mob->pos) <= flockRadius) {
            if (f->mobid != mob->mobid) {
                repulseVector(&repulseVec, &f->pos, &mob->pos,
                              flockRadius);
            }

            f = sg->findNthClosestFriend(&mob->pos, MOB_FLAG_FIGHTER, n++);
        }

        repulseVec.radius = weight;
        FRPoint_Add(rPos, &repulseVec, rPos);

//         {
//             FPoint vel;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             Warning("%s:%d OUT mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }
    }

    float edgeDistance(FPoint *pos) {
        FleetAI *ai = myFleetAI;
        float edgeDistance;
        FPoint edgePoint;

        edgePoint = *pos;
        edgePoint.x = 0.0f;
        edgeDistance = FPoint_Distance(pos, &edgePoint);

        edgePoint = *pos;
        edgePoint.x = ai->bp.width;
        edgeDistance = MIN(edgeDistance, FPoint_Distance(pos, &edgePoint));

        edgePoint = *pos;
        edgePoint.y = 0.0f;
        edgeDistance = MIN(edgeDistance, FPoint_Distance(pos, &edgePoint));

        edgePoint = *pos;
        edgePoint.y = ai->bp.height;
        edgeDistance = MIN(edgeDistance, FPoint_Distance(pos, &edgePoint));

        return edgeDistance;
    }

    void avoidEdges(Mob *mob, FRPoint *rPos, float repulseRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FleetAI *ai = myFleetAI;

        if (edgeDistance(&mob->pos) > repulseRadius) {
            return;
        }

//         {
//             FPoint vel;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             Warning("%s:%d  IN mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }

        FRPoint repulseVec;

        repulseVec.radius = 0.0f;
        repulseVec.theta = 0.0f;

        FPoint edgePoint;

        edgePoint = mob->pos;
        edgePoint.x = 0.0f;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          repulseRadius);
        }

        edgePoint = mob->pos;
        edgePoint.x = ai->bp.width;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          repulseRadius);
        }

        /*
         * Top Edge
         */
        edgePoint = mob->pos;
        edgePoint.y = 0.0f;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          repulseRadius);
        }

        edgePoint = mob->pos;
        edgePoint.y = ai->bp.height;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          repulseRadius);
        }

        FPoint target;
        FRPoint_ToFPoint(&repulseVec, &mob->pos, &target);
        ASSERT(edgeDistance(&mob->pos) <= edgeDistance(&target));

        repulseVec.radius = weight;
        FRPoint_Add(rPos, &repulseVec, rPos);

//         {
//             FPoint vel;
//             FPoint linearRepulse;
//             FRPoint_ToFPoint(rPos, NULL, &vel);
//             FRPoint_ToFPoint(&repulseVec, NULL, &linearRepulse);
//             Warning("%s:%d repulseVec(%0.1f, %0.1f) linear(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__,
//                     repulseVec.radius, repulseVec.theta,
//                     linearRepulse.x, linearRepulse.y);
//             Warning("%s:%d OUT mobid=%d, rPos(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                     __FUNCTION__, __LINE__, mob->mobid,
//                     rPos->radius, rPos->theta, vel.x, vel.y);
//         }
    }

//  virtual void doAttack(Mob *mob, Mob *enemyTarget) {
//      float baseRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
//      float flockRadius = baseRadius / 1.5f;
//      float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
//      BasicAIGovernor::doAttack(mob, enemyTarget);
//      FRPoint rPos;
//      FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);
//      //flockAlign(mob, &rPos);
//      flockSeparate(mob, &rPos, flockRadius, 0.5f);
//      //flockCohere(mob, &rPos);
//
//      rPos.radius = speed;
//      FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
//  }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        FleetAI *ai = myFleetAI;
        RandomState *rs = &myRandomState;
        SensorGrid *sg = mySensorGrid;
        BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
        Mob *base = sg->friendBase();
        float baseRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
        float fighterRadius = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
        float repulseRadius = 2 * fighterRadius;
        float flockRadius = baseRadius / 1.5f;
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        bool nearBase;

        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

//         if (!newlyIdle) {
//             return;
//         }

        nearBase = FALSE;
        if (base != NULL &&
            FPoint_Distance(&base->pos, &mob->pos) < baseRadius) {
            nearBase = TRUE;
        }

        if (!nearBase &&
            sg->numFriendsInRange(MOB_FLAG_FIGHTER, &mob->pos, flockRadius) > 1) {
            FRPoint rForce, rPos;

            FRPoint_Zero(&rForce);
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

//             {
//                 FPoint vel;
//                 FRPoint_ToFPoint(&rForce, NULL, &vel);
//                 Warning("\n%s:%d  IN mobid=%d, rForce(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                         __FUNCTION__, __LINE__, mob->mobid,
//                         rForce.radius, rForce.theta, vel.x, vel.y);
//                 Warning("%s:%d  pos(%0.1f, %0.1f) lastPos(%0.1f,%0.1f)\n",
//                         __FUNCTION__, __LINE__, mob->mobid,
//                         mob->pos.x, mob->pos.y, mob->lastPos.x, mob->lastPos.y);
//             }

            flockAlign(mob, &rForce, flockRadius, 0.2f);
            flockCohere(mob, &rForce, flockRadius, 0.1f);
            flockSeparate(mob, &rForce, repulseRadius, 0.2f);
            avoidEdges(mob, &rForce, repulseRadius, 0.9f);

            rPos.radius = 0.5f;
            FRPoint_Add(&rPos, &rForce, &rPos);
            rPos.radius = speed;
            FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
            ASSERT(!isnanf(mob->cmd.target.x));
            ASSERT(!isnanf(mob->cmd.target.y));

//             {
//                 FPoint vel;
//                 FRPoint_ToFPoint(&rForce, NULL, &vel);
//                 Warning("%s:%d OUT mobid=%d, rForce(%0.1f, %0.1f) vel(%0.1f,%0.1f)\n",
//                         __FUNCTION__, __LINE__, mob->mobid,
//                         rForce.radius, rForce.theta, vel.x, vel.y);
//                 Warning("%s:%d  pos(%0.1f, %0.1f) lastPos(%0.1f,%0.1f) "
//                         "target(%0.1f, %0.1f)\n\n",
//                         __FUNCTION__, __LINE__, mob->mobid,
//                         mob->pos.x, mob->pos.y, mob->lastPos.x, mob->lastPos.y,
//                         mob->cmd.target.x, mob->cmd.target.y);
//             }
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
