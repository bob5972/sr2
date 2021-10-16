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

typedef struct FlockConfigValue {
    const char *key;
    const char *value;
} FlockConfigValue;

typedef enum FlockPullType {
    PULL_ALWAYS,
    PULL_RANGE,
} FlockPullType;

class FlockAIGovernor : public BasicAIGovernor
{
public:
    FlockAIGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    { }

    virtual ~FlockAIGovernor() { }

    virtual void putDefaults(MBRegistry *mreg, FleetAIType flockType) {
        FlockConfigValue configs1[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE", },
            { "gatherRange",          "100",  },
            { "attackRange",          "250",  },

            // FlockFleet specific options
            { "flockRadius",          "166.7",      }, // baseSensorRadius / 1.5
            { "alignWeight",          "0.2",        },
            { "cohereWeight",         "-0.1",       },

            { "repulseRadius",        "50.0",       }, // 2 * fighterSensorRadius
            { "separateWeight",       "0.2",        },

            { "edgeRadius",           "100.0",      }, // fighterSensorRadius
            { "edgesWeight",          "0.9",        },
            { "centerRadius",         "0.0",        },
            { "centerWeight",         "0.0",        },

            { "coresRadius",          "166.7",      },
            { "coresWeight",          "0.1",        },
            { "coresCrowdRadius",     "166.7",      },
            { "coresCrowding",        "5",          },

            { "baseRadius",           "100",        },
            { "baseWeight",           "0.0",        },
            { "nearBaseRadius",       "250.0",      },
            { "baseDefenseRadius",    "250.0",      },

            { "enemyRadius",          "166.7",      },
            { "enemyWeight",          "0.3",        },
            { "enemyCrowdRadius",     "166.7",      },
            { "enemyCrowding",        "5",          },

            { "enemyBaseRadius",      "100",        },
            { "enemyBaseWeight",      "0.0",        },

            { "curHeadingWeight",     "0.5",        },

            { "attackSeparateRadius", "166.7",      },
            { "attackSeparateWeight", "0.5",        },
        };

        FlockConfigValue configs2[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "68.465767",  },
            { "attackRange",          "32.886688",  },

            // FlockFleet specific options
            { "flockRadius",          "398.545197", },
            { "alignWeight",          "0.239648",   },
            { "cohereWeight",         "-0.006502",  },

            { "repulseRadius",        "121.312904", },
            { "separateWeight",       "0.781240",   },

            { "edgeRadius",           "161.593430", },
            { "edgesWeight",          "0.704170",   },
            { "centerRadius",         "0.0",        },
            { "centerWeight",         "0.0",        },

            { "coresRadius",          "398.545197", },
            { "coresWeight",          "0.122679",   },
            { "coresCrowdRadius",     "398.545197", },
            { "coresCrowding",        "5.0",        },

            { "baseRadius",           "100",        },
            { "baseWeight",           "0.0",        },
            { "nearBaseRadius",       "250.0",      },
            { "baseDefenseRadius",    "250.0",      },

            { "enemyRadius",          "398.545197", },
            { "enemyWeight",          "0.556688",   },
            { "enemyCrowdRadius",     "398.545197", },
            { "enemyCrowding",        "5",          },

            { "enemyBaseRadius",      "100",        },
            { "enemyBaseWeight",      "0.0",        },

            { "curHeadingWeight",     "0.838760",   },

            { "attackSeparateRadius", "398.545197", },
            { "attackSeparateWeight", "0.188134",   },
        };

        FlockConfigValue configs3[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "61",         },
            { "attackRange",          "13.183991",  },
            { "guardRange",           "82.598732",  },
            { "evadeStrictDistance",  "25",         },
            { "attackExtendedRange",  "TRUE",       },
            { "evadeRange",           "485",        },
            { "evadeUseStrictDistance", "TRUE",     },
            { "rotateStartingAngle", "TRUE",        },

            // FlockFleet specific options
            { "flockRadius",          "338",        },
            { "alignWeight",          "0.000000",   },
            { "cohereWeight",         "-0.233058",  },

            { "repulseRadius",        "121.312904", },
            { "separateWeight",       "0.781240",   },

            { "edgeRadius",           "10.0",       },
            { "edgesWeight",          "0.10",       },
            { "centerRadius",         "0.0",        },
            { "centerWeight",         "0.0",        },

            { "coresRadius",          "1.000000",   },
            { "coresWeight",          "0.0",        },
            { "coresCrowdRadius",     "1.000000",   },
            { "coresCrowding",        "2.0",        },

            { "baseRadius",           "54.0",       },
            { "baseWeight",           "-0.589485",  },
            { "nearBaseRadius",       "8.000000",   },
            { "baseDefenseRadius",    "64.0",       },

            { "enemyRadius",          "398.545197", },
            { "enemyWeight",          "0.931404",   },
            { "enemyCrowdRadius",     "398.545197", },
            { "enemyCrowding",        "5",          },

            { "enemyBaseRadius",      "103",        },
            { "enemyBaseWeight",      "0.000000",   },

            { "curHeadingWeight",     "0.838760",   },

            { "attackSeparateRadius", "8.000000",   },
            { "attackSeparateWeight", "0.0",        },
        };

        FlockConfigValue configs4[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "61",         },
            { "attackRange",          "50.625603",  },
            { "guardRange",           "2.148767",   },
            { "evadeStrictDistance",  "20.359625",  },
            { "attackExtendedRange",  "TRUE",       },
            { "evadeRange",           "25.040209",  },
            { "evadeUseStrictDistance", "TRUE",     },
            { "rotateStartingAngle", "TRUE",        },

            // FlockFleet specific options
            { "flockRadius",          "129.883743", },
            { "alignWeight",          "0.295573",   },
            { "cohereWeight",         "-0.097492",  },

            { "repulseRadius",        "121.312904", },
            { "separateWeight",       "0.781240",   },

            { "edgeRadius",           "23.606379",  },
            { "edgesWeight",          "0.958569",   },
            { "centerRadius",         "0.0",        },
            { "centerWeight",         "0.0",        },

            { "coresRadius",          "93.769035",  },
            { "coresWeight",          "0.210546",   },
            { "coresCrowdRadius",     "93.769035",  },
            { "coresCrowding",        "7.429844",   },

            { "baseRadius",           "38.207771",  },
            { "baseWeight",           "0.181976",   },
            { "nearBaseRadius",       "53.931396",  },
            { "baseDefenseRadius",    "49.061054",  },

            { "enemyRadius",          "398.545197", },
            { "enemyWeight",          "0.931404",   },
            { "enemyCrowdRadius",     "398.545197", },
            { "enemyCrowding",        "5",          },

            { "enemyBaseRadius",      "10.000000",  },
            { "enemyBaseWeight",      "-0.950000",  },

            { "curHeadingWeight",     "0.215320",   },

            { "attackSeparateRadius", "26.184313",  },
            { "attackSeparateWeight", "-0.942996",  },
        };

        FlockConfigValue *configDefaults;
        uint configDefaultsSize;

        if (flockType == FLEET_AI_FLOCK1) {
            configDefaults = configs1;
            configDefaultsSize = ARRAYSIZE(configs1);
        } else if (flockType == FLEET_AI_FLOCK2) {
            configDefaults = configs2;
            configDefaultsSize = ARRAYSIZE(configs2);
        } else if (flockType == FLEET_AI_FLOCK3) {
            configDefaults = configs3;
            configDefaultsSize = ARRAYSIZE(configs3);
        } else if (flockType == FLEET_AI_FLOCK4) {
            configDefaults = configs4;
            configDefaultsSize = ARRAYSIZE(configs4);
        } else {
            PANIC("Unknown aiType: %d\n", flockType);
        }

        for (uint i = 0; i < configDefaultsSize; i++) {
            if (configDefaults[i].value != NULL &&
                !MBRegistry_ContainsKey(mreg, configDefaults[i].key)) {
                MBRegistry_Put(mreg, configDefaults[i].key, configDefaults[i].value);
            }
        }
    }


    virtual void loadRegistry(MBRegistry *mreg) {
        this->myConfig.flockRadius = MBRegistry_GetFloat(mreg, "flockRadius");
        this->myConfig.alignWeight = MBRegistry_GetFloat(mreg, "alignWeight");
        this->myConfig.cohereWeight = MBRegistry_GetFloat(mreg, "cohereWeight");

        this->myConfig.repulseRadius = MBRegistry_GetFloat(mreg, "repulseRadius");
        this->myConfig.separateWeight = MBRegistry_GetFloat(mreg, "separateWeight");

        this->myConfig.edgeRadius = MBRegistry_GetFloat(mreg, "edgeRadius");
        this->myConfig.edgesWeight = MBRegistry_GetFloat(mreg, "edgesWeight");
        this->myConfig.centerRadius = MBRegistry_GetFloat(mreg, "centerRadius");
        this->myConfig.centerWeight = MBRegistry_GetFloat(mreg, "centerWeight");

        this->myConfig.coresRadius = MBRegistry_GetFloat(mreg, "coresRadius");
        this->myConfig.coresWeight = MBRegistry_GetFloat(mreg, "coresWeight");
        this->myConfig.coresCrowdRadius = MBRegistry_GetFloat(mreg, "coresCrowdRadius");
        this->myConfig.coresCrowding = (uint)MBRegistry_GetFloat(mreg, "coresCrowding");

        this->myConfig.baseRadius = MBRegistry_GetFloat(mreg, "baseRadius");
        this->myConfig.baseWeight = MBRegistry_GetFloat(mreg, "baseWeight");
        this->myConfig.nearBaseRadius = MBRegistry_GetFloat(mreg, "nearBaseRadius");
        this->myConfig.baseDefenseRadius = MBRegistry_GetFloat(mreg, "baseDefenseRadius");

        this->myConfig.enemyRadius = MBRegistry_GetFloat(mreg, "enemyRadius");
        this->myConfig.enemyWeight = MBRegistry_GetFloat(mreg, "enemyWeight");
        this->myConfig.enemyCrowdRadius = MBRegistry_GetFloat(mreg, "enemyCrowdRadius");
        this->myConfig.enemyCrowding = (uint)MBRegistry_GetFloat(mreg, "enemyCrowding");

        this->myConfig.enemyBaseRadius = MBRegistry_GetFloat(mreg, "enemyBaseRadius");
        this->myConfig.enemyBaseWeight = MBRegistry_GetFloat(mreg, "enemyBaseWeight");

        this->myConfig.curHeadingWeight =
            MBRegistry_GetFloat(mreg, "curHeadingWeight");

        this->myConfig.attackSeparateRadius =
            MBRegistry_GetFloat(mreg, "attackSeparateRadius");
        this->myConfig.attackSeparateWeight =
            MBRegistry_GetFloat(mreg, "attackSeparateWeight");

        this->BasicAIGovernor::loadRegistry(mreg);
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

    void pullVector(FRPoint *curForce,
                    const FPoint *cPos, const FPoint *tPos,
                    float radius, float weight, FlockPullType pType) {
        ASSERT(pType == PULL_ALWAYS ||
               pType == PULL_RANGE);

        if (pType == PULL_RANGE &&
            FPoint_Distance(cPos, tPos) > radius) {
            return;
        } else if (weight == 0.0f) {
            return;
        }

        FPoint eVec;
        FRPoint reVec;
        FPoint_Subtract(tPos, cPos, &eVec);
        FPoint_ToFRPoint(&eVec, NULL, &reVec);
        reVec.radius = weight;
        FRPoint_Add(curForce, &reVec, curForce);
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


    void findEnemies(Mob *mob, FRPoint *rPos, float radius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *enemy = sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);

        if (enemy != NULL) {
            int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                   &mob->pos, myConfig.enemyCrowdRadius);
            FlockPullType pType = numFriends >= myConfig.enemyCrowding ?
                                  PULL_ALWAYS : PULL_RANGE;
            pullVector(rPos, &mob->pos, &enemy->pos, radius, weight, pType);
        }
    }

    void findCores(Mob *mob, FRPoint *rPos, float radius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *core = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);

        if (core != NULL) {
            int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                   &mob->pos, myConfig.coresCrowdRadius);
            FlockPullType pType = numFriends >= myConfig.coresCrowding ?
                                  PULL_ALWAYS : PULL_RANGE;
            pullVector(rPos, &mob->pos, &core->pos, radius, weight, pType);
        }
    }

    void findCenter(Mob *mob, FRPoint *rPos, float radius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FPoint center;
        center.x = myFleetAI->bp.width / 2;
        center.y = myFleetAI->bp.height / 2;
        pullVector(rPos, &mob->pos, &center, radius, weight, PULL_RANGE);
    }

    void findBase(Mob *mob, FRPoint *rPos, float radius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *base = sg->friendBase();

        if (base != NULL) {
            pullVector(rPos, &mob->pos, &base->pos, radius, weight, PULL_RANGE);
        }
    }

    void findEnemyBase(Mob *mob, FRPoint *rPos, float radius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *base = sg->enemyBase();

        if (base != NULL) {
            pullVector(rPos, &mob->pos, &base->pos, radius, weight, PULL_RANGE);
        }
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        BasicAIGovernor::doAttack(mob, enemyTarget);
        FRPoint rPos;
        FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

        //flockAlign(mob, &rPos);
        flockSeparate(mob, &rPos, myConfig.attackSeparateRadius,
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
            FPoint_Distance(&base->pos, &mob->pos) < myConfig.nearBaseRadius) {
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
            findCenter(mob, &rForce, myConfig.centerRadius, myConfig.centerWeight);
            findBase(mob, &rForce, myConfig.baseRadius, myConfig.baseWeight);
            findEnemies(mob, &rForce, myConfig.enemyRadius, myConfig.enemyWeight);
            findEnemyBase(mob, &rForce, myConfig.enemyBaseRadius,
                          myConfig.enemyBaseWeight);
            findCores(mob, &rForce, myConfig.coresRadius, myConfig.coresWeight);

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

        if (base != NULL) {
            int numEnemies = sg->numTargetsInRange(MOB_FLAG_SHIP, &base->pos,
                                                   myConfig.baseDefenseRadius);
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
        float flockRadius;
        float alignWeight;
        float cohereWeight;

        float repulseRadius;
        float separateWeight;

        float edgeRadius;
        float edgesWeight;
        float centerRadius;
        float centerWeight;

        float coresRadius;
        float coresWeight;
        float coresCrowdRadius;
        uint  coresCrowding;

        float baseRadius;
        float baseWeight;
        float nearBaseRadius;
        float baseDefenseRadius;

        float enemyRadius;
        float enemyWeight;
        float enemyCrowdRadius;
        uint  enemyCrowding;

        float enemyBaseRadius;
        float enemyBaseWeight;

        float curHeadingWeight;

        float attackSeparateRadius;
        float attackSeparateWeight;

    } myConfig;
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

        this->gov.putDefaults(mreg, ai->player.aiType);
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

void FlockFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_FLOCK1) {
        ops->aiName = "FlockFleet1";
    } else if (aiType == FLEET_AI_FLOCK2) {
        ops->aiName = "FlockFleet2";
    } else if (aiType == FLEET_AI_FLOCK3) {
        ops->aiName = "FlockFleet3";
    } else if (aiType == FLEET_AI_FLOCK4) {
        ops->aiName = "FlockFleet4";
    } else {
        NOT_IMPLEMENTED();
    }

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
