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

    virtual void putLiteDefaults(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE", },
            { "gatherRange",          "100",  },
            { "attackRange",          "250",  },

            // FlockFleet specific options
            { "alignWeight",          "0.2",  },
            { "cohereWeight",         "-0.1", },
            { "separateWeight",       "0.2",  },
            { "edgesWeight",          "0.9",  },
            { "enemyWeight",          "0.3",  },
            { "coresWeight",          "0.1",  },

            { "curHeadingWeight",     "0.5",  },
            { "attackSeparateWeight", "0.5",  },

            { "flockRadius",          "166.7",   }, // baseSensorRadius / 1.5
            { "repulseRadius",        "50.0",    }, // 2 * fighterSensorRadius
            { "edgeRadius",           "100.0",   }, // fighterSensorRadius
        };

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (configs[i].value != NULL &&
                !MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_Put(mreg, configs[i].key, configs[i].value);
            }
        }
    }


    virtual void loadRegistry(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",        },
            { "gatherRange",          "68.465767",   },
            { "attackRange",          "32.886688",   },

            // FlockFleet specific options
            { "alignWeight",          "0.239648",   },
            { "cohereWeight",         "-0.006502",  },
            { "separateWeight",       "0.781240",   },
            { "edgesWeight",          "0.704170",   },
            { "enemyWeight",          "0.556688",   },
            { "coresWeight",          "0.122679",   },

            { "curHeadingWeight",     "0.838760",   },
            { "attackSeparateWeight", "0.188134",   },

            { "flockRadius",          "398.545197", },
            { "repulseRadius",        "121.312904", },
            { "edgeRadius",           "161.593430", },
        };

        mreg = MBRegistry_AllocCopy(mreg);

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (configs[i].value != NULL &&
                !MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_Put(mreg, configs[i].key, configs[i].value);
            }
        }

        this->myConfig.alignWeight = MBRegistry_GetFloat(mreg, "alignWeight");
        this->myConfig.cohereWeight = MBRegistry_GetFloat(mreg, "cohereWeight");
        this->myConfig.separateWeight = MBRegistry_GetFloat(mreg, "separateWeight");
        this->myConfig.edgesWeight = MBRegistry_GetFloat(mreg, "edgesWeight");
        this->myConfig.enemyWeight = MBRegistry_GetFloat(mreg, "enemyWeight");
        this->myConfig.coresWeight = MBRegistry_GetFloat(mreg, "coresWeight");

        this->myConfig.curHeadingWeight =
            MBRegistry_GetFloat(mreg, "curHeadingWeight");
        this->myConfig.attackSeparateWeight =
            MBRegistry_GetFloat(mreg, "attackSeparateWeight");

        this->myConfig.flockRadius = MBRegistry_GetFloat(mreg, "flockRadius");
        this->myConfig.repulseRadius = MBRegistry_GetFloat(mreg, "repulseRadius");
        this->myConfig.edgeRadius = MBRegistry_GetFloat(mreg, "edgeRadius");

        this->BasicAIGovernor::loadRegistry(mreg);

        MBRegistry_Free(mreg);
    }

    void flockAlign(Mob *mob, FRPoint *rPos, float flockRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;

        FPoint avgVel;
        sg->friendAvgVelocity(&avgVel, &mob->pos, flockRadius, MOB_FLAG_FIGHTER);

        FRPoint ravgVel;
        FPoint_ToFRPoint(&avgVel, NULL, &ravgVel);
        ravgVel.radius = weight;

        FRPoint_Add(rPos, &ravgVel, rPos);
    }

    void flockCohere(Mob *mob, FRPoint *rPos, float flockRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;

        FPoint avgPos;
        sg->friendAvgPos(&avgPos, &mob->pos, flockRadius, MOB_FLAG_FIGHTER);

        FRPoint ravgPos;
        FPoint_ToFRPoint(&avgPos, NULL, &ravgPos);
        ravgPos.radius = weight;
        FRPoint_Add(rPos, &ravgPos, rPos);
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

        MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);
        FRPoint repulseVec;

        repulseVec.radius = 0.0f;
        repulseVec.theta = 0.0f;

        while (mit.hasNext()) {
            Mob *f = mit.next();
            ASSERT(f != NULL);


            if (f->mobid != mob->mobid &&
                FPoint_Distance(&f->pos, &mob->pos) <= flockRadius) {
                repulseVector(&repulseVec, &f->pos, &mob->pos,
                              flockRadius);
            }
        }

        repulseVec.radius = weight;
        FRPoint_Add(rPos, &repulseVec, rPos);
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

        if (edgeDistance(&mob->pos) >= repulseRadius) {
            return;
        }

        FRPoint repulseVec;

        repulseVec.radius = 0.0f;
        repulseVec.theta = 0.0f;

        FPoint edgePoint;

        /*
         * Left Edge
         */
        edgePoint = mob->pos;
        edgePoint.x = 0.0f;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          repulseRadius);
        }

        /*
         * Right Edge
         */
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

        /*
         * Bottom edge
         */
        edgePoint = mob->pos;
        edgePoint.y = ai->bp.height;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          repulseRadius);
        }

        repulseVec.radius = weight;
        FRPoint_Add(rPos, &repulseVec, rPos);
    }


    void findEnemies(Mob *mob, FRPoint *rPos, float flockRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *enemy = sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);

        if (enemy != NULL) {
            int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                   &mob->pos, flockRadius);

            if (numFriends >= 5) {
                FPoint eVec;
                FRPoint reVec;
                FPoint_Subtract(&enemy->pos, &mob->pos, &eVec);
                FPoint_ToFRPoint(&eVec, NULL, &reVec);
                reVec.radius = weight;
                FRPoint_Add(rPos, &reVec, rPos);
            }
        }
    }

    void findCores(Mob *mob, FRPoint *rPos, float flockRadius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *core = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);

        if (core != NULL) {
            int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                   &mob->pos, flockRadius);

            if (FPoint_Distance(&mob->pos, &core->pos) <= flockRadius ||
                numFriends >= 5) {
                FPoint eVec;
                FRPoint reVec;
                FPoint_Subtract(&core->pos, &mob->pos, &eVec);
                FPoint_ToFRPoint(&eVec, NULL, &reVec);
                reVec.radius = weight;
                FRPoint_Add(rPos, &reVec, rPos);
            }
        }
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        BasicAIGovernor::doAttack(mob, enemyTarget);
        FRPoint rPos;
        FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

        //flockAlign(mob, &rPos);
        flockSeparate(mob, &rPos, myConfig.flockRadius,
                      myConfig.attackSeparateWeight);
        //flockCohere(mob, &rPos);

        rPos.radius = speed;
        FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        FleetAI *ai = myFleetAI;
        RandomState *rs = &myRandomState;
        SensorGrid *sg = mySensorGrid;
        BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
        Mob *base = sg->friendBase();
        float baseRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        bool nearBase;

        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        nearBase = FALSE;
        if (base != NULL &&
            FPoint_Distance(&base->pos, &mob->pos) < baseRadius) {
            nearBase = TRUE;
        }

        if (!nearBase &&
            sg->numFriendsInRange(MOB_FLAG_FIGHTER, &mob->pos,
                                  myConfig.flockRadius) > 1) {
            FRPoint rForce, rPos;

            FRPoint_Zero(&rForce);
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

            flockAlign(mob, &rForce, myConfig.flockRadius, myConfig.alignWeight);
            flockCohere(mob, &rForce, myConfig.flockRadius, myConfig.cohereWeight);
            flockSeparate(mob, &rForce, myConfig.repulseRadius,
                          myConfig.separateWeight);
            avoidEdges(mob, &rForce, myConfig.edgeRadius, myConfig.edgesWeight);
            findEnemies(mob, &rForce, myConfig.flockRadius, myConfig.enemyWeight);
            findCores(mob, &rForce, myConfig.flockRadius, myConfig.coresWeight);

            rPos.radius = myConfig.curHeadingWeight;
            FRPoint_Add(&rPos, &rForce, &rPos);
            rPos.radius = speed;
            FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
            ASSERT(!isnanf(mob->cmd.target.x));
            ASSERT(!isnanf(mob->cmd.target.y));
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

    struct {
        float alignWeight;
        float cohereWeight;
        float separateWeight;
        float edgesWeight;
        float enemyWeight;
        float coresWeight;

        float curHeadingWeight;
        float attackSeparateWeight;

        float flockRadius;
        float repulseRadius;
        float edgeRadius;
    } myConfig;
};

class FlockFleet {
public:
    FlockFleet(FleetAI *ai, bool lite)
    :sg(), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        gov.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);

        if (lite) {
            this->gov.putLiteDefaults(mreg);
        }

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
static void *FlockFleetLiteCreate(FleetAI *ai);
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


void FlockFleetLite_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "FlockFleetLite";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &FlockFleetLiteCreate;
    ops->destroyFleet = &FlockFleetDestroy;
    ops->runAITick = &FlockFleetRunAITick;
    ops->mobSpawned = FlockFleetMobSpawned;
    ops->mobDestroyed = FlockFleetMobDestroyed;
}

static void *FlockFleetLiteCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new FlockFleet(ai, TRUE);
}

static void *FlockFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new FlockFleet(ai, FALSE);
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
