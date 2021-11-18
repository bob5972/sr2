/*
 * bundleFleet.cpp -- part of SpaceRobots2
 * Copyright (C) 2021 Michael Banack <github@banack.net>
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
#include "IntMap.h"
#include "battle.h"
}

#include "mutate.h"
#include "MBUtil.h"

#include "sensorGrid.hpp"
#include "shipAI.hpp"
#include "MBMap.hpp"
#include "MBString.hpp"

typedef uint32 BundleFlags;
#define BUNDLE_FLAG_NONE         (0)
#define BUNDLE_FLAG_STRICT_RANGE (1 << 0)
#define BUNDLE_FLAG_STRICT_CROWD (1 << 1)

typedef enum BundleValueType {
    BUNDLE_VALUE_TYPE_WEIGHT,
    BUNDLE_VALUE_TYPE_RADIUS,
    BUNDLE_VALUE_TYPE_PERIOD,
    BUNDLE_VALUE_TYPE_COUNT,
} BundleValueType;

typedef struct BundleValue {
    float value;
    float period;
    float amplitude;
} BundleValue;

typedef struct BundleCrowd {
    BundleValue size;
    BundleValue radius;
} BundleCrowd;

typedef struct BundleForce {
    BundleFlags flags;
    BundleValue weight;
    BundleValue radius;
    BundleCrowd crowd;
} BundleForce;

typedef struct BundleConfigValue {
    const char *key;
    const char *value;
} BundleConfigValue;

class BundleAIGovernor : public BasicAIGovernor
{
public:
    BundleAIGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    { }

    virtual ~BundleAIGovernor() { }

    virtual void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        BundleConfigValue defaults[] = {
            { "cores.radius.value",          "166.7",    },
            { "cores.weight.value",          "0.1",      },
            { "cores.crowd.radius",          "166.7",    },
            { "cores.crowd.size",            "0",        },

            { "enemy.radius.value",          "166.7",    },
            { "enemy.weight.value",          "0.3",      },
            { "enemy.crowd.radius.value",    "166.7",    },
            { "enemy.crowd.size.value",      "2",        },

            { "enemyBase.radius.value",      "166.7",    },
            { "enemyBase.weight.value",      "0.3",      },

            { "align.radius.value",          "166.7",    },
            { "align.weight.value",          "0.2",      },
            { "aligin.crowd.radius.value",   "166.7",    },
            { "aligin.crowd.size.value",     "3",        },

            { "cohere.radius.value",         "166.7",    },
            { "cohere.weight.value",         "0.1",      },
            { "cohere.crowd.radius.value",   "166.7",    },
            { "cohere.crowd.size.value",     "3",        },

            { "separate.radius.value",       "150.0",    },
            { "separate.weight.value",       "0.8",      },

            { "attackSeparate.radius.value", "166.0",    },
            { "attackSeparate.weight.value", "0.5",      },

            { "curHeadingWeight.value",      "0.5",      },

            { "center.radius.value",         "0.0",      },
            { "center.weight.value",         "0.0",      },

            { "edges.radius.value",          "100.0",    },
            { "edges.weight.value",          "0.9",      },

            { "locus.radius.value",          "1000.0",   },
            { "locus.weight.value",          "0.0",      },

            // Legacy Values
            { "randomIdle",           "TRUE",            },
            { "baseSpawnJitter",        "1",             },

            { "nearBaseRadius",       "250.0",           },
            { "baseDefenseRadius",    "250.0",           },

            { "locusCircularPeriod",  "1000.0",          },
            { "locusCircularWeight",  "0.0",             },
            { "locusLinearXPeriod",   "1000.0",          },
            { "locusLinearYPeriod",   "1000.0",          },
            { "locusLinearWeight",    "0.0",             },
            { "locusRandomWeight",    "0.0",             },
            { "locusRandomPeriod",    "1000.0",          },
            { "useScaledLocus",       "TRUE",            },
        };

        BundleConfigValue configs1[] = {
            { "attackExtendedRange",  "FALSE",           },
            { "attackRange",          "36.357330",       },
            { "creditReserve",        "120.438179",      },
            { "evadeFighters",        "FALSE",           },
            { "evadeRange",           "246.765274",      },
            { "evadeStrictDistance",  "2.582255",        },
            { "evadeUseStrictDistance", "TRUE",          },
            { "gatherAbandonStale",   "TRUE",            },
            { "gatherRange",          "25.859146",       },
            { "guardRange",           "23.338100",       },
            { "locusCircularPeriod",  "9653.471680",     },
            { "locusCircularWeight",  "-0.779813",       },
            { "locusLinearWeight",    "-0.803491",       },
            { "locusLinearXPeriod",   "7472.032227",     },
            { "locusLinearYPeriod",   "8851.404297",     },
            { "locus.radius.value",   "104.198990",      },
            { "locusWeight",          "-0.655256",       },
            { "randomIdle",           "TRUE",            },
            { "rotateStartingAngle",  "FALSE",           },
            { "sensorGrid.staleCoreTime",    "28.385160" },
            { "sensorGrid.staleFighterTime", "16.703636" },
            { "useScaledLocus",       "FALSE",           },
        };

        BundleConfigValue *configDefaults;
        uint configDefaultsSize;

        if (aiType == FLEET_AI_BUNDLE1) {
            configDefaults = configs1;
            configDefaultsSize = ARRAYSIZE(configs1);
        } else {
            PANIC("Unknown aiType: %d\n", aiType);
        }

        for (uint i = 0; i < configDefaultsSize; i++) {
            if (configDefaults[i].value != NULL &&
                !MBRegistry_ContainsKey(mreg, configDefaults[i].key)) {
                MBRegistry_PutConst(mreg, configDefaults[i].key, configDefaults[i].value);
            }
        }

        for (uint i = 0; i < ARRAYSIZE(defaults); i++) {
            if (defaults[i].value != NULL &&
                !MBRegistry_ContainsKey(mreg, defaults[i].key)) {
                MBRegistry_PutConst(mreg, defaults[i].key, defaults[i].value);
            }
        }
    }

    virtual void loadBundleValue(MBRegistry *mreg, BundleValue *bv,
                                 const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".value");
        bv->value = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".period");
        bv->period = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".amplitude");
        bv->amplitude = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    virtual void loadBundleForce(MBRegistry *mreg, BundleForce *b,
                                 const char *prefix) {
        CMBString s;
        const char *cs;
        MBString_Create(&s);

        b->flags = BUNDLE_FLAG_NONE;

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".rangeType");
        cs = MBRegistry_GetCStr(mreg, MBString_GetCStr(&s));
        if (cs == NULL ||
            strcmp(cs, "") == 0 ||
            strcmp(cs, "none") == 0) {
            /* No extra flags. */
        } else if (strcmp(cs, "strict") == 0) {
            b->flags |= BUNDLE_FLAG_STRICT_RANGE;
        } else {
            PANIC("Unknown rangeType = %s\n", cs);
        }

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".crowdType");
        cs = MBRegistry_GetCStr(mreg, MBString_GetCStr(&s));
        if (cs == NULL ||
            strcmp(cs, "") == 0 ||
            strcmp(cs, "none") == 0) {
            /* No extra flags. */
        } else if (strcmp(cs, "strict") == 0) {
            b->flags |= BUNDLE_FLAG_STRICT_RANGE;
        } else {
            PANIC("Unknown crowdType = %s\n", cs);
        }

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".weight");
        loadBundleValue(mreg, &b->weight, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".radius");
        loadBundleValue(mreg, &b->radius, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".crowd.size");
        loadBundleValue(mreg, &b->crowd.size, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".crowd.radius");
        loadBundleValue(mreg, &b->crowd.radius, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }


    virtual void loadRegistry(MBRegistry *mreg) {
        this->myConfig.randomIdle = MBRegistry_GetBool(mreg, "randomIdle");

        loadBundleForce(mreg, &this->myConfig.align, "align");
        loadBundleForce(mreg, &this->myConfig.cohere, "cohere");
        loadBundleForce(mreg, &this->myConfig.separate, "separate");
        loadBundleForce(mreg, &this->myConfig.attackSeparate, "attackSeparate");

        loadBundleForce(mreg, &this->myConfig.cores, "cores");
        loadBundleForce(mreg, &this->myConfig.cores, "enemy");
        loadBundleForce(mreg, &this->myConfig.enemyBase, "enemyBase");

        loadBundleForce(mreg, &this->myConfig.center, "center");
        loadBundleForce(mreg, &this->myConfig.edges, "edges");
        loadBundleForce(mreg, &this->myConfig.base, "base");

        this->myConfig.nearBaseRadius = MBRegistry_GetFloat(mreg, "nearBaseRadius");
        this->myConfig.baseDefenseRadius = MBRegistry_GetFloat(mreg, "baseDefenseRadius");

        loadBundleValue(mreg, &this->myConfig.curHeadingWeight, "curHeadingWeight");

        loadBundleForce(mreg, &this->myConfig.locus, "locus");
        this->myConfig.locusCircularPeriod =
            MBRegistry_GetFloat(mreg, "locusCircularPeriod");
        this->myConfig.locusCircularWeight =
            MBRegistry_GetFloat(mreg, "locusCircularWeight");
        this->myConfig.locusLinearXPeriod =
            MBRegistry_GetFloat(mreg, "locusLinearXPeriod");
        this->myConfig.locusLinearYPeriod =
            MBRegistry_GetFloat(mreg, "locusLinearYPeriod");
        this->myConfig.locusLinearWeight =
            MBRegistry_GetFloat(mreg, "locusLinearWeight");
        this->myConfig.useScaledLocus =
            MBRegistry_GetFloat(mreg, "useScaledLocus");

        this->myConfig.locusRandomWeight =
            MBRegistry_GetFloat(mreg, "locusRandomWeight");
        this->myConfig.locusRandomPeriod =
            (uint)MBRegistry_GetFloat(mreg, "locusRandomPeriod");

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    void flockAlign(Mob *mob, FRPoint *rForce) {
        FPoint avgVel;
        float radius = getBundleValue(&myConfig.align.radius);
        SensorGrid *sg = mySensorGrid;
        sg->friendAvgVelocity(&avgVel, &mob->pos, radius, MOB_FLAG_FIGHTER);
        avgVel.x += mob->pos.x;
        avgVel.y += mob->pos.y;
        applyBundle(mob, rForce, &myConfig.align, &avgVel);
    }

    void flockCohere(Mob *mob, FRPoint *rForce) {
        FPoint avgPos;
        float radius = getBundleValue(&myConfig.cohere.radius);
        SensorGrid *sg = mySensorGrid;
        sg->friendAvgPos(&avgPos, &mob->pos, radius, MOB_FLAG_FIGHTER);
        applyBundle(mob, rForce, &myConfig.cohere, &avgPos);
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

    void flockSeparate(Mob *mob, FRPoint *rForce, BundleForce *bundle) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;

        if (!crowdCheck(mob, bundle)) {
            /* No force. */
            return;
        }

        float radius = getBundleValue(&bundle->radius);
        float weight = getBundleValue(&bundle->weight);

        MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);
        FRPoint repulseVec;

        repulseVec.radius = 0.0f;
        repulseVec.theta = 0.0f;

        while (mit.hasNext()) {
            Mob *f = mit.next();
            ASSERT(f != NULL);

            if (f->mobid != mob->mobid &&
                FPoint_Distance(&f->pos, &mob->pos) <= radius) {
                repulseVector(&repulseVec, &f->pos, &mob->pos, radius);
            }
        }

        repulseVec.radius = weight;
        FRPoint_Add(rForce, &repulseVec, rForce);
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

    void avoidEdges(Mob *mob, FRPoint *rPos) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FleetAI *ai = myFleetAI;
        float radius = getBundleValue(&myConfig.edges.radius);
        float weight = getBundleValue(&myConfig.edges.weight);

        if (edgeDistance(&mob->pos) >= radius) {
            /* No force. */
            return;
        }

        if (!crowdCheck(mob, &myConfig.edges)) {
            /* No force. */
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
        if (FPoint_Distance(&edgePoint, &mob->pos) <= radius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          radius);
        }

        /*
         * Right Edge
         */
        edgePoint = mob->pos;
        edgePoint.x = ai->bp.width;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= radius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          radius);
        }

        /*
         * Top Edge
         */
        edgePoint = mob->pos;
        edgePoint.y = 0.0f;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= radius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          radius);
        }

        /*
         * Bottom edge
         */
        edgePoint = mob->pos;
        edgePoint.y = ai->bp.height;
        if (FPoint_Distance(&edgePoint, &mob->pos) <= radius) {
            repulseVector(&repulseVec, &edgePoint, &mob->pos,
                          radius);
        }

        repulseVec.radius = weight;
        FRPoint_Add(rPos, &repulseVec, rPos);
    }

    float getBundleValue(BundleValue *bv) {
        if (bv->amplitude > 0.0f && bv->period > 0.0f) {
            float p = bv->period;
            float a = bv->amplitude;
            return bv->value + a * sinf(myFleetAI->tick / p);
        } else {
            return bv->value;
        }
    }

    bool crowdCheck(Mob *mob, BundleForce *bundle) {
        SensorGrid *sg = mySensorGrid;

        if ((bundle->flags & BUNDLE_FLAG_STRICT_CROWD) != 0) {
            uint crowdSize = (uint)getBundleValue(&bundle->crowd.size);
            float crowdRadius = getBundleValue(&bundle->crowd.radius);

            if (crowdSize <= 1 || crowdRadius <= 0.0f) {
                return TRUE;
            }

            int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                   &mob->pos, crowdRadius);
            if (numFriends < crowdSize) {
                return FALSE;
            }
        }

        return TRUE;
    }

    void applyBundle(Mob *mob, FRPoint *rForce, BundleForce *bundle,
                     FPoint *focusPos) {
        if (!crowdCheck(mob, bundle)) {
            /* No force. */
            return;
        }

        float radius = getBundleValue(&bundle->radius);

        if ((bundle->flags & BUNDLE_FLAG_STRICT_RANGE) != 0 &&
            FPoint_Distance(&mob->pos, focusPos) > radius) {
            /* No force. */
            return;
        }

        float weight = getBundleValue(&bundle->weight);

        if (weight == 0.0f) {
            /* No force. */
            return;
        }

        FPoint eVec;
        FRPoint reVec;
        FPoint_Subtract(focusPos, &mob->pos, &eVec);
        FPoint_ToFRPoint(&eVec, NULL, &reVec);
        reVec.radius = weight;
        FRPoint_Add(rForce, &reVec, rForce);
    }

    void findCores(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *core = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);

        if (core != NULL) {
            applyBundle(mob, rForce, &myConfig.cores, &core->pos);
        }
    }

    void findEnemies(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *enemy = sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);

        if (enemy != NULL) {
            applyBundle(mob, rForce, &myConfig.enemy, &enemy->pos);
        }
    }

    void findCenter(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FPoint center;
        center.x = myFleetAI->bp.width / 2;
        center.y = myFleetAI->bp.height / 2;
        applyBundle(mob, rForce, &myConfig.center, &center);
    }

    void findLocus(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FPoint circular;
        FPoint linear;
        FPoint locus;
        bool haveCircular = FALSE;
        bool haveLinear = FALSE;
        bool haveRandom = FALSE;
        float width = myFleetAI->bp.width;
        float height = myFleetAI->bp.height;
        float temp;

        if (myConfig.locusCircularPeriod > 0.0f &&
            myConfig.locusCircularWeight != 0.0f) {
            float cwidth = width / 2;
            float cheight = height / 2;
            float ct = myFleetAI->tick / myConfig.locusCircularPeriod;

            /*
             * This isn't actually the circumference of an ellipse,
             * but it's a good approximation.
             */
            ct /= M_PI * (cwidth + cheight);

            circular.x = cwidth + cwidth * cosf(ct);
            circular.y = cheight + cheight * sinf(ct);
            haveCircular = TRUE;
        }

        if (myConfig.locusRandomPeriod > 0.0f &&
            myConfig.locusRandomWeight != 0.0f) {
            /*
             * XXX: Each ship will get a different random locus on the first
             * tick.
             */
            if (myLive.randomLocusTick == 0 ||
                myFleetAI->tick - myLive.randomLocusTick >
                myConfig.locusRandomPeriod) {
                RandomState *rs = &myRandomState;
                myLive.randomLocus.x = RandomState_Float(rs, 0.0f, width);
                myLive.randomLocus.y = RandomState_Float(rs, 0.0f, height);
                myLive.randomLocusTick = myFleetAI->tick;
            }
            haveRandom = TRUE;
        }

        if (myConfig.locusLinearXPeriod > 0.0f &&
            myConfig.locusLinearWeight != 0.0f) {
            float ltx = myFleetAI->tick / myConfig.locusLinearXPeriod;
            ltx /= 2 * width;
            linear.x = width * modff(ltx / width, &temp);
            if (((uint)temp) % 2 == 1) {
                /*
                 * Go backwards for the return trip.
                 */
                linear.x = width - linear.x;
            }
            haveLinear = TRUE;
        } else {
            linear.x = mob->pos.x;
        }

        if (myConfig.locusLinearYPeriod > 0.0f &&
            myConfig.locusLinearWeight != 0.0f) {
            float lty = myFleetAI->tick / myConfig.locusLinearYPeriod;
            lty /= 2 * height;
            linear.y = height * modff(lty / height, &temp);
            if (((uint)temp) % 2 == 1) {
                /*
                 * Go backwards for the return trip.
                 */
                linear.y = height - linear.y;
            }
            haveLinear = TRUE;
        } else {
            linear.y = mob->pos.y;
        }

        if (haveLinear || haveCircular || haveRandom) {
            float scale = 0.0f;
            locus.x = 0.0f;
            locus.y = 0.0f;
            if (haveLinear) {
                locus.x += myConfig.locusLinearWeight * linear.x;
                locus.y += myConfig.locusLinearWeight * linear.y;
                scale += myConfig.locusLinearWeight;
            }
            if (haveCircular) {
                locus.x += myConfig.locusCircularWeight * circular.x;
                locus.y += myConfig.locusCircularWeight * circular.y;
                scale += myConfig.locusCircularWeight;
            }
            if (haveRandom) {
                locus.x += myConfig.locusRandomWeight * myLive.randomLocus.x;
                locus.y += myConfig.locusRandomWeight *  myLive.randomLocus.y;
                scale += myConfig.locusRandomWeight;
            }

            if (myConfig.useScaledLocus) {
                if (scale != 0.0f) {
                    locus.x /= scale;
                    locus.y /= scale;
                }
            }

            applyBundle(mob, rForce, &myConfig.locus, &locus);
        }
    }

    void findBase(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *base = sg->friendBase();
        if (base != NULL) {
            applyBundle(mob, rForce, &myConfig.base, &base->pos);
        }
    }

    void findEnemyBase(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *base = sg->enemyBase();

        if (base != NULL) {
            applyBundle(mob, rForce, &myConfig.enemyBase, &base->pos);
        }
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        BasicAIGovernor::doAttack(mob, enemyTarget);
        FRPoint rPos;
        FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

        flockSeparate(mob, &rPos, &myConfig.attackSeparate);

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
            myConfig.nearBaseRadius > 0.0f &&
            FPoint_Distance(&base->pos, &mob->pos) < myConfig.nearBaseRadius) {
            nearBase = TRUE;
        }

        if (!nearBase) {
            FRPoint rForce, rPos;

            FRPoint_Zero(&rForce);
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

            rForce.theta = rPos.theta;
            rForce.radius = getBundleValue(&myConfig.curHeadingWeight);

            flockAlign(mob, &rForce);
            flockCohere(mob, &rForce);
            flockSeparate(mob, &rForce, &myConfig.separate);

            avoidEdges(mob, &rForce);
            findCenter(mob, &rForce);
            findBase(mob, &rForce);
            findEnemies(mob, &rForce);
            findEnemyBase(mob, &rForce);
            findCores(mob, &rForce);
            findLocus(mob, &rForce);

            rForce.radius = speed;

            FRPoint_ToFPoint(&rForce, &mob->pos, &mob->cmd.target);
            ASSERT(!isnanf(mob->cmd.target.x));
            ASSERT(!isnanf(mob->cmd.target.y));
        } else if (newlyIdle) {
            if (myConfig.randomIdle) {
                mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
                mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
            }
        }

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }

    virtual void runTick() {
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
        bool randomIdle;

        BundleForce align;
        BundleForce cohere;
        BundleForce separate;
        BundleForce attackSeparate;

        BundleForce center;
        BundleForce edges;

        BundleForce cores;
        BundleForce base;

        float nearBaseRadius;
        float baseDefenseRadius;

        BundleForce enemy;
        BundleForce enemyBase;

        BundleValue curHeadingWeight;

        BundleForce locus;
        float locusCircularPeriod;
        float locusCircularWeight;
        float locusLinearXPeriod;
        float locusLinearYPeriod;
        float locusLinearWeight;
        float locusRandomWeight;
        uint  locusRandomPeriod;
        bool  useScaledLocus;
    } myConfig;

    struct {
        FPoint randomLocus;
        uint randomLocusTick;
    } myLive;
};

class BundleFleet {
public:
    BundleFleet(FleetAI *ai)
    :sg(), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        gov.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);

        this->gov.putDefaults(mreg, ai->player.aiType);
        this->gov.loadRegistry(mreg);
    }

    ~BundleFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    BundleAIGovernor gov;
    MBRegistry *mreg;
};

static void *BundleFleetCreate(FleetAI *ai);
static void BundleFleetDestroy(void *aiHandle);
static void BundleFleetRunAITick(void *aiHandle);
static void *BundleFleetMobSpawned(void *aiHandle, Mob *m);
static void BundleFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static void BundleFleetMutate(FleetAIType aiType, MBRegistry *mreg);

void BundleFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_BUNDLE1) {
        ops->aiName = "BundleFleet1";
    } else {
        NOT_IMPLEMENTED();
    }

    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BundleFleetCreate;
    ops->destroyFleet = &BundleFleetDestroy;
    ops->runAITick = &BundleFleetRunAITick;
    ops->mobSpawned = &BundleFleetMobSpawned;
    ops->mobDestroyed = &BundleFleetMobDestroyed;
    ops->mutateParams = &BundleFleetMutate;
}

static void GetMutationFloatParams(MutationFloatParams *vf,
                                   const char *key,
                                   BundleValueType bType)
{
    MBUtil_Zero(vf, sizeof(*vf));

    if (bType == BUNDLE_VALUE_TYPE_WEIGHT) {
        vf->minValue = -1.0f;
        vf->maxValue = 1.0f;
        vf->magnitude = 0.1f;
        vf->jumpRate = 0.1f;
        vf->mutationRate = 0.1f;
    } else if (bType == BUNDLE_VALUE_TYPE_RADIUS ||
               bType == BUNDLE_VALUE_TYPE_PERIOD) {
        vf->minValue = -1.0f;
        vf->maxValue = 10000.0f;
        vf->magnitude = 0.1f;
        vf->jumpRate = 0.1f;
        vf->mutationRate = 0.1f;
    } else if (bType == BUNDLE_VALUE_TYPE_COUNT) {
        vf->minValue = -1.0f;
        vf->maxValue = 20.0f;
        vf->magnitude = 0.1f;
        vf->jumpRate = 0.1f;
        vf->mutationRate = 0.1f;
    }

    vf->key = key;
}

static void GetMutationStrParams(MutationStrParams *svf,
                                 const char *key)
{

    MBUtil_Zero(svf, sizeof(*svf));
    svf->key = key;
    svf->flipRate = 0.01;
}

static void MutateBundleValue(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix,
                              BundleValueType bType)
{
    MutationFloatParams vf;

    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".value");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), bType);
    Mutate_Float(mreg, &vf, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".period");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), BUNDLE_VALUE_TYPE_PERIOD);
    Mutate_Float(mreg, &vf, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".amplitude");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), bType);
    Mutate_Float(mreg, &vf, 1);

    MBString_Destroy(&s);
}

static void MutateBundleForce(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    MutationStrParams svf;

    const char *options[] = {
        "none", "strict",
    };

    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowdType");
    GetMutationStrParams(&svf, MBString_GetCStr(&s));
    Mutate_Str(mreg, &svf, 1, options, ARRAYSIZE(options));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".rangeType");
    GetMutationStrParams(&svf, MBString_GetCStr(&s));
    Mutate_Str(mreg, &svf, 1, options, ARRAYSIZE(options));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".weight");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      BUNDLE_VALUE_TYPE_WEIGHT);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".radius");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      BUNDLE_VALUE_TYPE_RADIUS);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowd.size");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      BUNDLE_VALUE_TYPE_COUNT);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowd.radius");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      BUNDLE_VALUE_TYPE_RADIUS);

    MBString_Destroy(&s);
}

static void BundleFleetMutate(FleetAIType aiType, MBRegistry *mreg)
{
    MutationFloatParams vf[] = {
        // key                     min    max       mag   jump   mutation
        { "evadeStrictDistance",  -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "evadeRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "attackRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "guardRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.10f},
        { "gatherRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "startingMaxRadius",    1000.0f, 2000.0f, 0.05f, 0.10f, 0.20f},
        { "startingMinRadius",    300.0f,  800.0f,  0.05f, 0.10f, 0.20f},

        { "nearBaseRadius",        1.0f,   500.0f,  0.05f, 0.15f, 0.01f},
        { "baseDefenseRadius",     1.0f,   500.0f,  0.05f, 0.15f, 0.01f},

        { "locusCircularPeriod",  -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { "locusCircularWeight",   0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { "locusLinearXPeriod",   -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { "locusLinearYPeriod",   -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { "locusLinearWeight",     0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { "locusRandomWeight",     0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { "locusRandomPeriod",    -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
    };

    MutationBoolParams vb[] = {
        // key                       mutation
        { "evadeFighters",           0.05f},
        { "evadeUseStrictDistance",  0.05f},
        { "attackExtendedRange",     0.05f},
        { "rotateStartingAngle",     0.05f},
        { "gatherAbandonStale",      0.05f},
        { "useScaledLocus",          0.01f},
        { "randomIdle",              0.01f},
    };

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));

    MutateBundleForce(aiType, mreg, "align");
    MutateBundleForce(aiType, mreg, "cohere");
    MutateBundleForce(aiType, mreg, "separate");
    MutateBundleForce(aiType, mreg, "attackSeparate");

    MutateBundleForce(aiType, mreg, "cores");
    MutateBundleForce(aiType, mreg, "enemy");
    MutateBundleForce(aiType, mreg, "enemyBase");

    MutateBundleForce(aiType, mreg, "center");
    MutateBundleForce(aiType, mreg, "edges");
    MutateBundleForce(aiType, mreg, "base");

    MutateBundleValue(aiType, mreg, "curHeadingWeight", BUNDLE_VALUE_TYPE_WEIGHT);

    MutateBundleForce(aiType, mreg, "locus");
}

static void *BundleFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new BundleFleet(ai);
}

static void BundleFleetDestroy(void *handle)
{
    BundleFleet *sf = (BundleFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *BundleFleetMobSpawned(void *aiHandle, Mob *m)
{
    BundleFleet *sf = (BundleFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void BundleFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    BundleFleet *sf = (BundleFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void BundleFleetRunAITick(void *aiHandle)
{
    BundleFleet *sf = (BundleFleet *)aiHandle;
    sf->gov.runTick();
}
