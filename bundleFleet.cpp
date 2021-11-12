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

#include "sensorGrid.hpp"
#include "shipAI.hpp"
#include "MBMap.hpp"
#include "MBString.hpp"

typedef enum BundleLegacyPullType {
    PULL_ALWAYS,
    PULL_RANGE,
} BundleLegacyPullType;

typedef uint32 BundleFlags;
#define BUNDLE_FLAG_NONE         (0)
#define BUNDLE_FLAG_STRICT_RANGE (1 << 0)
#define BUNDLE_FLAG_STRICT_CROWD (1 << 1)

typedef struct BundleValue {
    float baseValue;
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
            { "cores.radius.baseValue", "166.7",    },
            { "cores.weight.baseValue", "0.1",      },
            { "cores.crowd.radius",     "166.7",    },
            { "cores.crowd.size",       "5",        },

            // Legacy Values
            { "randomIdle",           "TRUE",       },
            { "alwaysFlock",          "FALSE",      },
            { "baseSpawnJitter",        "1",        },

            { "flockRadius",          "166.7",      },
            { "flockCrowding",        "2.0",        },
            { "alignWeight",          "0.2",        },
            { "cohereWeight",         "-0.1",       },
            { "brokenCohere",         "FALSE",      },

            { "separateRadius",       "50.0",       },
            { "separatePeriod",       "0.0",        },
            { "separateScale",        "50.0",       },
            { "separateWeight",       "0.2",        },

            { "edgeRadius",           "100.0",      },
            { "edgesWeight",          "0.9",        },
            { "centerRadius",         "0.0",        },
            { "centerWeight",         "0.0",        },

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

            { "locusRadius",          "10000.0",    },
            { "locusWeight",          "0.0",        },
            { "locusCircularPeriod",  "1000.0",     },
            { "locusCircularWeight",  "0.0",        },
            { "locusLinearXPeriod",   "1000.0",     },
            { "locusLinearYPeriod",   "1000.0",     },
            { "locusLinearWeight",    "0.0",        },
            { "locusRandomWeight",    "0.0",        },
            { "locusRandomPeriod",    "1000.0",     },
            { "useScaledLocus",       "TRUE",       },
        };

        BundleConfigValue configs1[] = {
            { "alignWeight",          "1.000000",   },
            { "alwaysFlock",          "TRUE",       },
            { "attackExtendedRange",  "FALSE",      },
            { "attackRange",          "36.357330",  },
            { "attackSeparateRadius", "116.610649", },
            { "attackSeparateWeight", "-0.846049",  },
            { "baseDefenseRadius",    "1.102500",   },
            { "baseRadius",           "292.362305", },
            { "baseSpawnJitter",      "1.000000",   },
            { "baseWeight",           "-0.328720",  },
            { "brokenCohere",         "TRUE",       },
            { "centerRadius",         "761.465576", },
            { "centerWeight",         "-0.048965",  },
            { "cohereWeight",         "0.048618",   },
            { "coresCrowding",        "4.913648",   },
            { "coresCrowdRadius",     "135.280548", },
            { "coresRadius",          "776.426697", },
            { "coresWeight",          "0.197949",   },
            { "creditReserve",        "120.438179", },
            { "curHeadingWeight",     "0.499466",   },
            { "edgeRadius",           "26.930847",  },
            { "edgesWeight",          "0.482821",   },
            { "enemyBaseRadius",      "224.461044", },
            { "enemyBaseWeight",      "0.633770",   },
            { "enemyCrowding",        "9.255432",   },
            { "enemyCrowdRadius",     "728.962708", },
            { "enemyRadius",          "261.936279", },
            { "enemyWeight",          "0.518455",   },
            { "evadeFighters",        "FALSE",      },
            { "evadeRange",           "246.765274", },
            { "evadeStrictDistance",  "2.582255",   },
            { "evadeUseStrictDistance", "TRUE",     },
            { "flockCrowding",        "2.705287",   },
            { "flockRadius",          "105.816391", },
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "25.859146",  },
            { "guardRange",           "23.338100",  },
            { "locusCircularPeriod",  "9653.471680",},
            { "locusCircularWeight",  "-0.779813",  },
            { "locusLinearWeight",    "-0.803491",  },
            { "locusLinearXPeriod",   "7472.032227",},
            { "locusLinearYPeriod",   "8851.404297",},
            { "locusRadius",          "104.198990", },
            { "locusWeight",          "-0.655256",  },
            { "nearBaseRadius",       "10.077254",  },
            { "randomIdle",           "TRUE",       },
            { "rotateStartingAngle",  "FALSE",      },
            { "sensorGrid.staleCoreTime",    "28.385160" },
            { "sensorGrid.staleFighterTime", "16.703636" },
            { "separatePeriod",       "1543.553345",},
            { "separateRadius",       "105.912781", },
            { "separateScale",        "0.000000",   },
            { "separateWeight",       "0.839316",   },
            { "useScaledLocus",       "FALSE",      },
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
        MBString_AppendCStr(&s, ".baseValue");
        bv->baseValue = MBRegistry_GetFloatD(mreg, MBString_GetCStr(&s), 0.0f);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".period");
        bv->period = MBRegistry_GetFloatD(mreg, MBString_GetCStr(&s), 0.0f);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".amplitude");
        bv->amplitude = MBRegistry_GetFloatD(mreg, MBString_GetCStr(&s), 0.0f);

        MBString_Destroy(&s);
    }

    virtual void loadBundle(MBRegistry *mreg, BundleForce *b, const char *prefix) {
        CMBString s;
        MBString_Create(&s);

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
        this->myConfig.alwaysFlock = MBRegistry_GetBool(mreg, "alwaysFlock");

        this->myConfig.flockRadius = MBRegistry_GetFloat(mreg, "flockRadius");
        this->myConfig.flockCrowding = (uint)MBRegistry_GetFloat(mreg, "flockCrowding");
        this->myConfig.alignWeight = MBRegistry_GetFloat(mreg, "alignWeight");
        this->myConfig.cohereWeight = MBRegistry_GetFloat(mreg, "cohereWeight");
        this->myConfig.brokenCohere = MBRegistry_GetBool(mreg, "brokenCohere");

        this->myConfig.separateRadius = MBRegistry_GetFloat(mreg, "separateRadius");
        this->myConfig.separatePeriod = MBRegistry_GetFloat(mreg, "separatePeriod");
        this->myConfig.separateScale = MBRegistry_GetFloat(mreg, "separateScale");
        this->myConfig.separateWeight = MBRegistry_GetFloat(mreg, "separateWeight");

        this->myConfig.edgeRadius = MBRegistry_GetFloat(mreg, "edgeRadius");
        this->myConfig.edgesWeight = MBRegistry_GetFloat(mreg, "edgesWeight");
        this->myConfig.centerRadius = MBRegistry_GetFloat(mreg, "centerRadius");
        this->myConfig.centerWeight = MBRegistry_GetFloat(mreg, "centerWeight");

        loadBundle(mreg, &this->myConfig.cores, "cores");

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

        this->myConfig.locusRadius =
            MBRegistry_GetFloat(mreg, "locusRadius");
        this->myConfig.locusWeight =
            MBRegistry_GetFloat(mreg, "locusWeight");
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

    void flockAlign(const FPoint *avgVel, FRPoint *rPos) {
        float weight = myConfig.alignWeight;
        FRPoint ravgVel;

        FPoint_ToFRPoint(avgVel, NULL, &ravgVel);
        ravgVel.radius = weight;

        FRPoint_Add(rPos, &ravgVel, rPos);
    }

    void brokenCoherePos(FPoint *avgPos, const FPoint *center) {
        SensorGrid *sg = mySensorGrid;
        MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);
        FPoint lAvgPos;
        float flockRadius = myConfig.flockRadius;

        lAvgPos.x = 0.0f;
        lAvgPos.y = 0.0f;

        while (mit.hasNext()) {
            Mob *f = mit.next();
            ASSERT(f != NULL);

            if (FPoint_Distance(&f->pos, center) <= flockRadius) {
                /*
                 * The broken version just sums the positions and doesn't
                 * properly average them.
                 */
                lAvgPos.x += f->pos.x;
                lAvgPos.y += f->pos.y;
            }
        }

        ASSERT(avgPos != NULL);
        *avgPos = lAvgPos;
    }

    void flockCohere(Mob *mob, const FPoint *avgPos, FRPoint *rPos) {
        FPoint lAvgPos;
        float weight = myConfig.cohereWeight;

        if (myConfig.brokenCohere) {
            brokenCoherePos(&lAvgPos, &mob->pos);
        } else {
            lAvgPos = *avgPos;
        }

        FRPoint ravgPos;
        FPoint_ToFRPoint(&lAvgPos, NULL, &ravgPos);
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
                    float radius, float weight, BundleLegacyPullType pType) {
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

    void flockSeparate(Mob *mob, FRPoint *rPos, float radius, float weight) {
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
                FPoint_Distance(&f->pos, &mob->pos) <= radius) {
                repulseVector(&repulseVec, &f->pos, &mob->pos, radius);
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
            BundleLegacyPullType pType = numFriends >= myConfig.enemyCrowding ?
                                  PULL_ALWAYS : PULL_RANGE;
            pullVector(rPos, &mob->pos, &enemy->pos, radius, weight, pType);
        }
    }

    float getBundleValue(BundleValue *bv) {
        if (bv->amplitude > 0.0f && bv->period > 0.0f) {
            float p = bv->period;
            float a = bv->amplitude;
            return bv->baseValue + a * sinf(myFleetAI->tick / p);
        } else {
            return bv->baseValue;
        }
    }

    void applyBundle(Mob *mob, FRPoint *rForce, BundleForce *bundle,
                     FPoint *focusPos) {
        BundleLegacyPullType pType;
        SensorGrid *sg = mySensorGrid;

        if ((bundle->flags & BUNDLE_FLAG_STRICT_CROWD) != 0) {
            uint crowdSize = (uint)getBundleValue(&bundle->crowd.size);
            float crowdRadius = getBundleValue(&bundle->crowd.radius);
            int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                   &mob->pos, crowdRadius);
            if (numFriends < crowdSize) {
                /* No force. */
                return;
            }
        }

        if ((bundle->flags & BUNDLE_FLAG_STRICT_RANGE) != 0) {
            pType = PULL_RANGE;
        } else {
            pType = PULL_ALWAYS;
        }

        float radius = getBundleValue(&bundle->radius);
        float weight = getBundleValue(&bundle->weight);
        pullVector(rForce, &mob->pos, focusPos, radius, weight, pType);
    }

    void findCores(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *core = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);

        if (core != NULL) {
            applyBundle(mob, rForce, &myConfig.cores, &core->pos);
        }
    }

    void findCenter(Mob *mob, FRPoint *rPos, float radius, float weight) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FPoint center;
        center.x = myFleetAI->bp.width / 2;
        center.y = myFleetAI->bp.height / 2;
        pullVector(rPos, &mob->pos, &center, radius, weight, PULL_RANGE);
    }

    void findLocus(Mob *mob, FRPoint *rPos) {
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

            pullVector(rPos, &mob->pos, &locus,
                       myConfig.locusRadius, myConfig.locusWeight, PULL_RANGE);
        }
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

        flockSeparate(mob, &rPos, myConfig.attackSeparateRadius,
                      myConfig.attackSeparateWeight);

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
        bool doFlock;

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

        doFlock = FALSE;
        if (myConfig.flockCrowding <= 1 ||
            sg->numFriendsInRange(MOB_FLAG_FIGHTER, &mob->pos,
                                  myConfig.flockRadius) >= myConfig.flockCrowding) {
            doFlock = TRUE;
        }

        if (!nearBase && (myConfig.alwaysFlock || doFlock)) {
            FRPoint rForce, rPos;

            FRPoint_Zero(&rForce);
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

            if (doFlock) {
                FPoint avgVel;
                FPoint avgPos;
                sg->friendAvgFlock(&avgVel, &avgPos, &mob->pos,
                                   myConfig.flockRadius, MOB_FLAG_FIGHTER);
                flockAlign(&avgVel, &rForce);
                flockCohere(mob, &avgPos, &rForce);

                flockSeparate(mob, &rForce, myLive.separateRadius,
                              myConfig.separateWeight);
            }

            avoidEdges(mob, &rForce, myConfig.edgeRadius, myConfig.edgesWeight);
            findCenter(mob, &rForce, myConfig.centerRadius, myConfig.centerWeight);
            findBase(mob, &rForce, myConfig.baseRadius, myConfig.baseWeight);
            findEnemies(mob, &rForce, myConfig.enemyRadius, myConfig.enemyWeight);
            findEnemyBase(mob, &rForce, myConfig.enemyBaseRadius,
                          myConfig.enemyBaseWeight);
            findCores(mob, &rForce);
            findLocus(mob, &rForce);

            rPos.radius = myConfig.curHeadingWeight;
            FRPoint_Add(&rPos, &rForce, &rPos);
            rPos.radius = speed;

            FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
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

        if (myConfig.separatePeriod > 0.0f &&
            myConfig.separateScale > 0.0f) {
            float p = myConfig.separatePeriod;
            float s = myConfig.separateScale;
            myLive.separateRadius = myConfig.separateRadius +
                                   s * fabs(sinf(myFleetAI->tick / p));
        } else {
            myLive.separateRadius = myConfig.separateRadius;
        }

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
        bool alwaysFlock;

        float flockRadius;
        uint flockCrowding;
        float alignWeight;
        float cohereWeight;
        bool brokenCohere;

        float separateRadius;
        float separatePeriod;
        float separateScale;
        float separateWeight;

        float edgeRadius;
        float edgesWeight;
        float centerRadius;
        float centerWeight;

        BundleForce cores;

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

        float locusRadius;
        float locusWeight;
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
        float separateRadius;
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
    ops->mobSpawned = BundleFleetMobSpawned;
    ops->mobDestroyed = BundleFleetMobDestroyed;
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
