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
#include "battle.h"
}

#include "MBVarMap.h"

#include "mutate.h"
#include "MBUtil.h"

#include "sensorGrid.hpp"
#include "basicShipAI.hpp"
#include "MBMap.hpp"
#include "MBString.hpp"

#define BUNDLE_SCRAMBLE_KEY "bundleFleet.scrambleMutation"

typedef enum BundleCheckType {
    BUNDLE_CHECK_INVALID = 0,
    BUNDLE_CHECK_NEVER,
    BUNDLE_CHECK_ALWAYS,
    BUNDLE_CHECK_STRICT_ON,
    BUNDLE_CHECK_STRICT_OFF,
    BUNDLE_CHECK_LINEAR_UP,
    BUNDLE_CHECK_LINEAR_DOWN,
    BUNDLE_CHECK_QUADRATIC_UP,
    BUNDLE_CHECK_QUADRATIC_DOWN,
} BundleCheckType;

typedef uint32 BundleValueFlags;
#define BUNDLE_VALUE_FLAG_NONE     (0)
#define BUNDLE_VALUE_FLAG_PERIODIC (1 << 0)

typedef struct BundleAtom {
    float value;
    float mobJitterScale;
} BundleAtom;

typedef struct BundlePeriodicParams {
    BundleAtom period;
    BundleAtom amplitude;
    BundleAtom tickShift;
} BundlePeriodicParams;

typedef struct BundleValue {
    BundleValueFlags flags;
    BundleAtom value;

    BundlePeriodicParams periodic;
} BundleValue;

typedef struct BundleCrowd {
    BundleValue size;
    BundleValue radius;
} BundleCrowd;

typedef struct BundleForce {
    BundleCheckType rangeCheck;
    BundleCheckType crowdCheck;
    BundleValue weight;
    BundleValue radius;
    BundleCrowd crowd;
} BundleForce;

typedef struct LiveLocusState {
    FPoint randomPoint;
    uint randomTick;
} LiveLocusState;

typedef struct BundleLocusPointParams {
    float circularPeriod;
    float circularWeight;
    float linearXPeriod;
    float linearYPeriod;
    float linearWeight;
    float randomWeight;
    bool useScaled;
} BundleLocusPointParams;

typedef struct BundleFleetLocus {
    BundleForce force;
    BundleLocusPointParams params;
    float randomPeriod;
} BundleFleetLocus;

typedef struct BundleMobLocus {
    BundleForce force;
    BundleAtom circularPeriod;
    BundleValue circularWeight;
    BundleAtom linearXPeriod;
    BundleAtom linearYPeriod;
    BundleValue linearWeight;
    BundleAtom randomPeriod;
    BundleValue randomWeight;
    bool useScaled;
} BundleMobLocus;

typedef struct BundleSpec {
    bool randomIdle;

    BundleForce align;
    BundleForce cohere;
    BundleForce separate;
    BundleForce attackSeparate;

    BundleForce center;
    BundleForce edges;

    BundleForce cores;
    BundleForce base;
    BundleForce baseDefense;

    float nearBaseRadius;
    float baseDefenseRadius;

    BundleForce enemy;
    BundleForce enemyBase;

    BundleValue curHeadingWeight;

    BundleFleetLocus fleetLocus;
    BundleMobLocus mobLocus;
} BundleSpec;

typedef struct BundleConfigValue {
    const char *key;
    const char *value;
} BundleConfigValue;

class BundleAIGovernor : public BasicAIGovernor
{
public:
    BundleAIGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        MBUtil_Zero(&myLive, sizeof(myLive));
    }

    virtual ~BundleAIGovernor() { }

    class BundleShipAI : public BasicShipAI
    {
        public:
        BundleShipAI(MobID mobid, BundleAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        {
            CMBVarMap_Create(&myMobJitters);
            MBUtil_Zero(&myShipLive, sizeof(myShipLive));
        }

        virtual ~BundleShipAI() {
            CMBVarMap_Destroy(&myMobJitters);
        }

        CMBVarMap myMobJitters;
        struct {
            LiveLocusState mobLocus;
        } myShipLive;
    };

    virtual BundleShipAI *newShip(MobID mobid) {
        return new BundleShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        BundleConfigValue defaults[] = {
            { "creditReserve",               "120.43817",},
            { "sensorGrid.staleCoreTime",    "28.385160" },
            { "sensorGrid.staleFighterTime", "16.703636" },

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

            // Legacy Values
            { "randomIdle",           "TRUE",            },
            { "baseSpawnJitter",        "1",             },

            { "nearBaseRadius",       "250.0",           },
            { "baseDefenseRadius",    "250.0",           },
        };

        BundleConfigValue configs1[] = {
            { "align.crowd.radius.amplitude", "0.321261", },
            { "align.crowd.radius.period", "5953.474121", },
            { "align.crowd.radius.value", "1807.180176", },
            { "align.crowd.radius.valueType", "constant", },
            { "align.crowd.size.amplitude", "0.429708", },
            { "align.crowd.size.period", "9243.138672", },
            { "align.crowd.size.value", "11.233359", },
            { "align.crowd.size.valueType", "periodic", },
            { "align.crowdType", "never", },
            { "align.radius.amplitude", "0.566070", },
            { "align.radius.period", "9900.000000", },
            { "align.radius.value", "1350.809570", },
            { "align.radius.valueType", "periodic", },
            { "align.rangeType", "linearDown", },
            { "align.weight.amplitude", "0.515353", },
            { "align.weight.period", "9000.000000", },
            { "align.weight.value", "0.533934", },
            { "align.weight.valueType", "periodic", },
            { "attackExtendedRange", "FALSE", },
            { "attackRange", "117.644791", },
            { "attackSeparate.crowd.radius.amplitude", "0.114193", },
            { "attackSeparate.crowd.radius.period", "7805.395020", },
            { "attackSeparate.crowd.radius.value", "388.137054", },
            { "attackSeparate.crowd.radius.valueType", "periodic", },
            { "attackSeparate.crowd.size.amplitude", "0.095685", },
            { "attackSeparate.crowd.size.period", "8551.041992", },
            { "attackSeparate.crowd.size.value", "8.597579", },
            { "attackSeparate.crowd.size.valueType", "constant", },
            { "attackSeparate.crowdType", "linearDown", },
            { "attackSeparate.radius.amplitude", "1.000000", },
            { "attackSeparate.radius.period", "1345.166748", },
            { "attackSeparate.radius.value", "355.393280", },
            { "attackSeparate.radius.valueType", "periodic", },
            { "attackSeparate.rangeType", "always", },
            { "attackSeparate.weight.amplitude", "0.766508", },
            { "attackSeparate.weight.period", "5715.046875", },
            { "attackSeparate.weight.value", "0.689100", },
            { "attackSeparate.weight.valueType", "periodic", },
            { "base.crowd.radius.amplitude", "0.600368", },
            { "base.crowd.radius.period", "5266.161133", },
            { "base.crowd.radius.value", "1078.740356", },
            { "base.crowd.radius.valueType", "periodic", },
            { "base.crowd.size.amplitude", "0.664938", },
            { "base.crowd.size.period", "2936.962402", },
            { "base.crowd.size.value", "0.521992", },
            { "base.crowd.size.valueType", "constant", },
            { "base.crowdType", "strictOff", },
            { "base.radius.amplitude", "0.407606", },
            { "base.radius.period", "4651.818359", },
            { "base.radius.value", "970.598145", },
            { "base.radius.valueType", "constant", },
            { "base.rangeType", "strictOff", },
            { "base.weight.amplitude", "0.753670", },
            { "base.weight.period", "-1.000000", },
            { "base.weight.value", "-1.416888", },
            { "base.weight.valueType", "constant", },
            { "baseDefenseRadius", "143.515045", },
            { "center.crowd.radius.amplitude", "0.733661", },
            { "center.crowd.radius.period", "6119.505371", },
            { "center.crowd.radius.value", "564.073486", },
            { "center.crowd.radius.valueType", "periodic", },
            { "center.crowd.size.amplitude", "1.000000", },
            { "center.crowd.size.period", "8173.202148", },
            { "center.crowd.size.value", "0.074628", },
            { "center.crowd.size.valueType", "periodic", },
            { "center.crowdType", "never", },
            { "center.radius.amplitude", "1.000000", },
            { "center.radius.period", "7462.924316", },
            { "center.radius.value", "682.307922", },
            { "center.radius.valueType", "constant", },
            { "center.rangeType", "always", },
            { "center.weight.amplitude", "0.806573", },
            { "center.weight.period", "565.521851", },
            { "center.weight.value", "-1.305155", },
            { "center.weight.valueType", "constant", },
            { "cohere.crowd.radius.amplitude", "0.756902", },
            { "cohere.crowd.radius.period", "7789.553223", },
            { "cohere.crowd.radius.value", "1782.000000", },
            { "cohere.crowd.radius.valueType", "constant", },
            { "cohere.crowd.size.amplitude", "1.000000", },
            { "cohere.crowd.size.period", "-1.000000", },
            { "cohere.crowd.size.value", "3.687377", },
            { "cohere.crowd.size.valueType", "periodic", },
            { "cohere.crowdType", "linearUp", },
            { "cohere.radius.amplitude", "0.306166", },
            { "cohere.radius.period", "10000.000000", },
            { "cohere.radius.value", "1914.735596", },
            { "cohere.radius.valueType", "periodic", },
            { "cohere.rangeType", "linearDown", },
            { "cohere.weight.amplitude", "0.266461", },
            { "cohere.weight.period", "1867.322510", },
            { "cohere.weight.value", "-0.507121", },
            { "cohere.weight.valueType", "constant", },
            { "cores.crowd.radius.amplitude", "0.525464", },
            { "cores.crowd.radius.period", "6589.460938", },
            { "cores.crowd.radius.value", "174.267288", },
            { "cores.crowd.radius.valueType", "periodic", },
            { "cores.crowd.size.amplitude", "0.055397", },
            { "cores.crowd.size.period", "3110.003174", },
            { "cores.crowd.size.value", "8.727318", },
            { "cores.crowd.size.valueType", "constant", },
            { "cores.crowdType", "linearDown", },
            { "cores.radius.amplitude", "0.640855", },
            { "cores.radius.period", "9444.480469", },
            { "cores.radius.value", "35.896736", },
            { "cores.radius.valueType", "periodic", },
            { "cores.rangeType", "never", },
            { "cores.weight.amplitude", "0.010756", },
            { "cores.weight.period", "2880.781250", },
            { "cores.weight.value", "1.0", },
            { "cores.weight.valueType", "constant", },
            { "curHeadingWeight.amplitude", "1.000000", },
            { "curHeadingWeight.period", "2207.250000", },
            { "curHeadingWeight.value", "-3.811037", },
            { "curHeadingWeight.valueType", "constant", },
            { "edges.crowd.radius.amplitude", "0.447709", },
            { "edges.crowd.radius.period", "8368.186523", },
            { "edges.crowd.radius.value", "1260.249023", },
            { "edges.crowd.radius.valueType", "periodic", },
            { "edges.crowd.size.amplitude", "0.000000", },
            { "edges.crowd.size.period", "999.697876", },
            { "edges.crowd.size.value", "6.702061", },
            { "edges.crowd.size.valueType", "constant", },
            { "edges.crowdType", "never", },
            { "edges.radius.amplitude", "0.389971", },
            { "edges.radius.period", "1560.454834", },
            { "edges.radius.value", "50.840942", },
            { "edges.radius.valueType", "constant", },
            { "edges.rangeType", "strictOff", },
            { "edges.weight.amplitude", "0", },
            { "edges.weight.period", "0", },
            { "edges.weight.value", "1.0", },
            { "edges.weight.valueType", "constant", },
            { "enemy.crowd.radius.amplitude", "0.798566", },
            { "enemy.crowd.radius.period", "7607.696289", },
            { "enemy.crowd.radius.value", "203.481049", },
            { "enemy.crowd.radius.valueType", "constant", },
            { "enemy.crowd.size.amplitude", "0.618705", },
            { "enemy.crowd.size.period", "7847.218750", },
            { "enemy.crowd.size.value", "20.000000", },
            { "enemy.crowd.size.valueType", "periodic", },
            { "enemy.crowdType", "always", },
            { "enemy.radius.amplitude", "0.090641", },
            { "enemy.radius.period", "3363.753906", },
            { "enemy.radius.value", "611.284424", },
            { "enemy.radius.valueType", "constant", },
            { "enemy.rangeType", "strictOn", },
            { "enemy.weight.amplitude", "0.958385", },
            { "enemy.weight.period", "4596.960449", },
            { "enemy.weight.value", "-1.185188", },
            { "enemy.weight.valueType", "constant", },
            { "enemyBase.crowd.radius.amplitude", "0.736067", },
            { "enemyBase.crowd.radius.period", "6412.087402", },
            { "enemyBase.crowd.radius.value", "1114.323120", },
            { "enemyBase.crowd.radius.valueType", "periodic", },
            { "enemyBase.crowd.size.amplitude", "1.000000", },
            { "enemyBase.crowd.size.period", "909.905334", },
            { "enemyBase.crowd.size.value", "10.031953", },
            { "enemyBase.crowd.size.valueType", "periodic", },
            { "enemyBase.crowdType", "always", },
            { "enemyBase.radius.amplitude", "0.559740", },
            { "enemyBase.radius.period", "4657.600586", },
            { "enemyBase.radius.value", "693.966919", },
            { "enemyBase.radius.valueType", "periodic", },
            { "enemyBase.rangeType", "never", },
            { "enemyBase.weight.amplitude", "0.121047", },
            { "enemyBase.weight.period", "8157.837891", },
            { "enemyBase.weight.value", "0.081705", },
            { "enemyBase.weight.valueType", "constant", },
            { "evadeFighters", "FALSE", },
            { "evadeRange", "283.460571", },
            { "evadeStrictDistance", "87.064606", },
            { "evadeUseStrictDistance", "FALSE", },
            { "gatherAbandonStale", "TRUE", },
            { "gatherRange", "216.282059", },
            { "guardRange", "-0.902500", },
            { "nearBaseRadius", "423.256439", },
            { "randomIdle", "TRUE", },
            { "rotateStartingAngle", "FALSE", },
            { "separate.crowd.radius.amplitude", "0.141071", },
            { "separate.crowd.radius.period", "2654.302979", },
            { "separate.crowd.radius.value", "981.912476", },
            { "separate.crowd.radius.valueType", "periodic", },
            { "separate.crowd.size.amplitude", "0.782046", },
            { "separate.crowd.size.period", "6823.173828", },
            { "separate.crowd.size.value", "11.729516", },
            { "separate.crowd.size.valueType", "constant", },
            { "separate.crowdType", "linearUp", },
            { "separate.radius.amplitude", "0.095367", },
            { "separate.radius.period", "2932.626221", },
            { "separate.radius.value", "2000.000000", },
            { "separate.radius.valueType", "periodic", },
            { "separate.rangeType", "strictOff", },
            { "separate.weight.amplitude", "0.694014", },
            { "separate.weight.period", "8443.943359", },
            { "separate.weight.value", "1.573738", },
            { "separate.weight.valueType", "constant", },
            { "startingMaxRadius", "1295.414795", },
            { "startingMinRadius", "642.803894", },
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

    void loadBundleAtom(MBRegistry *mreg, BundleAtom *ba,
                        const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".value");
        ba->value = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(ba->value));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".mobJitterScale");
        ba->mobJitterScale = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(ba->mobJitterScale));

        MBString_Destroy(&s);
    }

    void loadBundlePeriodicParams(MBRegistry *mreg,
                                  BundlePeriodicParams *bpp,
                                  const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".period");
        loadBundleAtom(mreg, &bpp->period, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".amplitude");
        loadBundleAtom(mreg, &bpp->amplitude, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".tickShift");
        loadBundleAtom(mreg, &bpp->tickShift, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    void loadBundleValue(MBRegistry *mreg, BundleValue *bv,
                         const char *prefix) {
        CMBString s;
        const char *cs;
        MBString_Create(&s);

        bv->flags = BUNDLE_VALUE_FLAG_NONE;

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".valueType");
        cs = MBRegistry_GetCStr(mreg, MBString_GetCStr(&s));
        if (cs == NULL ||
            strcmp(cs, "") == 0 ||
            strcmp(cs, "constant") == 0 ||
            strcmp(cs, "none") == 0) {
            /* No extra flags. */
        } else if (strcmp(cs, "periodic") == 0) {
            bv->flags |= BUNDLE_VALUE_FLAG_PERIODIC;
        } else {
            PANIC("Unknown valueType = %s\n", cs);
        }

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".value");
        loadBundleAtom(mreg, &bv->value, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".periodic");
        loadBundlePeriodicParams(mreg, &bv->periodic, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    void loadBundleCheck(MBRegistry *mreg, BundleCheckType *bc,
                         const char *prefix) {
        CMBString s;
        const char *cs;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".check");
        cs = MBRegistry_GetCStr(mreg, MBString_GetCStr(&s));
        if (cs == NULL ||
            strcmp(cs, "") == 0 ||
            strcmp(cs, "none") == 0 ||
            strcmp(cs, "nowhere") == 0) {
            *bc = BUNDLE_CHECK_NEVER;
        } else if (strcmp(cs, "strictOn") == 0) {
            *bc = BUNDLE_CHECK_STRICT_ON;
        } else if (strcmp(cs, "strictOff") == 0) {
            *bc = BUNDLE_CHECK_STRICT_OFF;
        } else if (strcmp(cs, "always") == 0) {
            *bc = BUNDLE_CHECK_ALWAYS;
        } else if (strcmp(cs, "linearUp") == 0) {
            *bc = BUNDLE_CHECK_LINEAR_UP;
        } else if (strcmp(cs, "linearDown") == 0) {
            *bc = BUNDLE_CHECK_LINEAR_DOWN;
        } else if (strcmp(cs, "quadraticUp") == 0) {
            *bc = BUNDLE_CHECK_QUADRATIC_UP;
        } else if (strcmp(cs, "quadraticDown") == 0) {
            *bc = BUNDLE_CHECK_QUADRATIC_DOWN;
        } else {
            PANIC("Unknown rangeType = %s\n", cs);
        }
    }

    void loadBundleForce(MBRegistry *mreg, BundleForce *b,
                         const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".rangeType");
        loadBundleCheck(mreg, &b->rangeCheck, MBString_GetCStr(&s));

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

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".crowdType");
        loadBundleCheck(mreg, &b->crowdCheck, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    void loadBundleFleetLocus(MBRegistry *mreg, BundleFleetLocus *lp,
                              const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".force");
        loadBundleForce(mreg, &lp->force, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".circularPeriod");
        lp->params.circularPeriod = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".circularWeight");
        lp->params.circularWeight = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".linearXPeriod");
        lp->params.linearXPeriod = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".linearYPeriod");
        lp->params.linearYPeriod = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".linearWeight");
        lp->params.linearWeight = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".randomWeight");
        lp->params.randomWeight = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".randomPeriod");
        lp->randomPeriod = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".useScaled");
        lp->params.useScaled = MBRegistry_GetBool(mreg, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    void loadBundleMobLocus(MBRegistry *mreg, BundleMobLocus *lp,
                            const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".force");
        loadBundleForce(mreg, &lp->force, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".circularPeriod");
        loadBundleAtom(mreg, &lp->circularPeriod, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".circularWeight");
        loadBundleValue(mreg, &lp->circularWeight, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".linearXPeriod");
        loadBundleAtom(mreg, &lp->linearXPeriod, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".linearYPeriod");
        loadBundleAtom(mreg, &lp->linearYPeriod, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".linearWeight");
        loadBundleValue(mreg, &lp->linearWeight, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".randomWeight");
        loadBundleValue(mreg, &lp->randomWeight, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".randomPeriod");
        loadBundleAtom(mreg, &lp->randomPeriod, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".useScaled");
        lp->useScaled = MBRegistry_GetBool(mreg, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }


    virtual void loadRegistry(MBRegistry *mreg) {
        this->myConfig.randomIdle = MBRegistry_GetBool(mreg, "randomIdle");

        loadBundleForce(mreg, &this->myConfig.align, "align");
        loadBundleForce(mreg, &this->myConfig.cohere, "cohere");
        loadBundleForce(mreg, &this->myConfig.separate, "separate");
        loadBundleForce(mreg, &this->myConfig.attackSeparate, "attackSeparate");

        loadBundleForce(mreg, &this->myConfig.cores, "cores");
        loadBundleForce(mreg, &this->myConfig.enemy, "enemy");
        loadBundleForce(mreg, &this->myConfig.enemyBase, "enemyBase");

        loadBundleForce(mreg, &this->myConfig.center, "center");
        loadBundleForce(mreg, &this->myConfig.edges, "edges");
        loadBundleForce(mreg, &this->myConfig.base, "base");
        loadBundleForce(mreg, &this->myConfig.baseDefense, "baseDefense");

        this->myConfig.nearBaseRadius =
            MBRegistry_GetFloat(mreg, "nearBaseRadius");
        this->myConfig.baseDefenseRadius =
            MBRegistry_GetFloat(mreg, "baseDefenseRadius");

        loadBundleValue(mreg, &this->myConfig.curHeadingWeight,
                        "curHeadingWeight");

        loadBundleFleetLocus(mreg, &this->myConfig.fleetLocus, "fleetLocus");
        loadBundleMobLocus(mreg, &this->myConfig.mobLocus, "mobLocus");

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    void flockAlign(Mob *mob, FRPoint *rForce) {
        FPoint avgVel;
        float radius = getBundleValue(mob, &myConfig.align.radius);
        SensorGrid *sg = mySensorGrid;
        sg->friendAvgVelocity(&avgVel, &mob->pos, radius, MOB_FLAG_FIGHTER);
        avgVel.x += mob->pos.x;
        avgVel.y += mob->pos.y;
        applyBundle(mob, rForce, &myConfig.align, &avgVel);
    }

    void flockCohere(Mob *mob, FRPoint *rForce) {
        FPoint avgPos;
        float radius = getBundleValue(mob, &myConfig.cohere.radius);
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
        float weight;

        if (!crowdCheck(mob, bundle, &weight)) {
            /* No force. */
            return;
        }

        float radius = getBundleValue(mob, &bundle->radius);
        weight *= getBundleValue(mob, &bundle->weight);

        MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);
        FRPoint repulseVec;

        repulseVec.radius = 0.0f;
        repulseVec.theta = 0.0f;

        while (mit.hasNext()) {
            Mob *f = mit.next();
            ASSERT(f != NULL);

            if (f->mobid != mob->mobid &&
                FPoint_DistanceSquared(&f->pos, &mob->pos) <= radius * radius) {
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

    void flockEdges(Mob *mob, FRPoint *rPos) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FleetAI *ai = myFleetAI;
        float radius = getBundleValue(mob, &myConfig.edges.radius);
        float weight;

        if (edgeDistance(&mob->pos) >= radius) {
            /* No force. */
            return;
        }

        if (!crowdCheck(mob, &myConfig.edges, &weight)) {
            /* No force. */
            return;
        }

        weight *= getBundleValue(mob, &myConfig.edges.weight);

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

    float getMobJitter(Mob *m, float *value) {
        RandomState *rs = &myRandomState;
        MBVar key;
        MBVar mj;

        BundleShipAI *ship = (BundleShipAI *)getShip(m->mobid);

        ASSERT(ship != NULL);
        ASSERT(value >= (void *)&myConfig && value < (void *)&(&myConfig)[1]);

        if (*value <= 0.0f) {
            return 0.0f;
        }

        key.all = 0;
        key.vPtr = value;
        if (!CMBVarMap_Lookup(&ship->myMobJitters, key, &mj)) {
            mj.vFloat = RandomState_Float(rs, -*value, *value);
            CMBVarMap_Put(&ship->myMobJitters, key, mj);
        }

        return mj.vFloat;
    }

    /*
     * getBundleAtom --
     *     Compute a bundle atom.
     */
    float getBundleAtom(Mob *m, BundleAtom *ba) {
        float jitter = getMobJitter(m, &ba->mobJitterScale);

        if (jitter == 0.0f) {
            return ba->value;
        } else {
            return ba->value * (1.0f + jitter);
        }
    }

    /*
     * getBundleValue --
     *     Compute a bundle value.
     */
    float getBundleValue(Mob *m, BundleValue *bv) {
        float value = getBundleAtom(m, &bv->value);

        if ((bv->flags & BUNDLE_VALUE_FLAG_PERIODIC) != 0) {
            float p = getBundleAtom(m, &bv->periodic.period);
            float a = getBundleAtom(m, &bv->periodic.amplitude);
            float t = myFleetAI->tick;

            if (a > 0.0f && p > 1.0f) {
                /*
                 * Shift the wave by a constant factor of the period.
                 */
                t += bv->periodic.tickShift.value;
                t += p * getMobJitter(m, &bv->periodic.tickShift.mobJitterScale);
                value *= 1.0f + a * sinf(t / p);
            }
        }

        return value;
    }

    bool isConstantBundleCheck(BundleCheckType bc) {
        return bc == BUNDLE_CHECK_NEVER ||
               bc == BUNDLE_CHECK_ALWAYS;
    }

    /*
     * Should this force operate given the current conditions?
     * Returns TRUE iff the force should operate.
     */
    bool bundleCheck(BundleCheckType bc, float value, float trigger,
                     float *weight) {
        if (bc == BUNDLE_CHECK_NEVER) {
            *weight = 0.0f;
            return FALSE;
        } else if (bc == BUNDLE_CHECK_ALWAYS) {
            *weight = 1.0f;
            return TRUE;
        } else if (isnanf(trigger) || isnanf(value)) {
            /*
             * Throw out malformed values after checking for ALWAYS/NEVER.
             */
            *weight = 0.0f;
            return FALSE;
        } else if (bc == BUNDLE_CHECK_STRICT_ON) {
            if (value >= trigger) {
                *weight = 1.0f;
                return TRUE;
            } else {
                *weight = 0.0f;
                return FALSE;
            }
        } else if (bc == BUNDLE_CHECK_STRICT_OFF) {
            if (value >= trigger) {
                *weight = 0.0f;
                return FALSE;
            } else {
                *weight = 1.0f;
                return TRUE;
            }
        }


        float localWeight;
        float maxWeight = 100.0f;
        if (trigger <= 0.0f) {
            if (bc == BUNDLE_CHECK_LINEAR_DOWN ||
                bc == BUNDLE_CHECK_QUADRATIC_DOWN) {
                /*
                 * For the DOWN checks, the force decreases to zero as the trigger
                 * approaches zero, so it makes sense to treat negative numbers
                 * as a disabled check.
                 */
                *weight = 0.0f;
                return FALSE;
            } else {
                ASSERT(bc == BUNDLE_CHECK_LINEAR_UP ||
                       bc == BUNDLE_CHECK_QUADRATIC_UP);

                /*
                * For the UP checks, the force value should be infinite as the
                * trigger approaches zero, so use our clamped max force.
                */
                *weight = maxWeight;
                return TRUE;
            }
        } else if (value <= 0.0f) {
            /*
             * These are the reverse of the trigger <= 0 checks above.
             */
            if (bc == BUNDLE_CHECK_LINEAR_DOWN ||
                bc == BUNDLE_CHECK_QUADRATIC_DOWN) {
                *weight = maxWeight;
                return TRUE;
            } else {
                ASSERT(bc == BUNDLE_CHECK_LINEAR_UP ||
                       bc == BUNDLE_CHECK_QUADRATIC_UP);
                *weight = 0.0f;
                return FALSE;
            }
        } else if (bc == BUNDLE_CHECK_LINEAR_UP) {
            localWeight = value / trigger;
        } else if (bc == BUNDLE_CHECK_LINEAR_DOWN) {
            localWeight = trigger / value;
        } else if (bc == BUNDLE_CHECK_QUADRATIC_UP) {
            float x = value / trigger;
            localWeight = x * x;
        } else if (bc == BUNDLE_CHECK_QUADRATIC_DOWN) {
            float x = trigger / value;
            localWeight = x * x;
        } else {
            PANIC("Unknown BundleCheckType: %d\n", bc);
        }

        bool retVal = TRUE;
        if (localWeight <= 0.0f || isnanf(localWeight)) {
            localWeight = 0.0f;
            retVal = FALSE;
        } else if (localWeight >= maxWeight) {
            localWeight = maxWeight;
        }

        *weight = localWeight;
        return retVal;
    }

    /*
     * Should this force operate given the current crowd size?
     * Returns TRUE iff the force should operate.
     */
    bool crowdCheck(Mob *mob, BundleForce *bundle, float *weight) {
        SensorGrid *sg = mySensorGrid;

        float crowdTrigger = 0.0f;
        float crowdRadius = 0.0f;
        float crowdValue = 0.0f;

        /*
         * Skip the complicated calculations if the bundle-check was constant.
         */
        if (!isConstantBundleCheck(bundle->crowdCheck)) {
            crowdTrigger = getBundleValue(mob, &bundle->crowd.size);
            crowdRadius = getBundleValue(mob, &bundle->crowd.radius);

            crowdValue = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                               &mob->pos, crowdRadius);
        }

        return bundleCheck(bundle->crowdCheck, crowdValue, crowdTrigger, weight);
    }

    /*
     * applyBundle --
     *      Apply a bundle to a given mob to calculate the force.
     */
    void applyBundle(Mob *mob, FRPoint *rForce, BundleForce *bundle,
                     FPoint *focusPos) {
        float cweight;
        if (!crowdCheck(mob, bundle, &cweight)) {
            /* No force. */
            return;
        }

        float rweight;
        float radius = 0.0f;
        float distance = 0.0f;
        if (!isConstantBundleCheck(bundle->rangeCheck)) {
            distance = FPoint_Distance(&mob->pos, focusPos);
            radius = getBundleValue(mob, &bundle->radius);
        }

        if (!bundleCheck(bundle->rangeCheck, distance, radius, &rweight)) {
            /* No force. */
            return;
        }

        float vweight = rweight * cweight * getBundleValue(mob, &bundle->weight);

        if (vweight == 0.0f) {
            /* No force. */
            return;
        }

        FPoint eVec;
        FRPoint reVec;
        FPoint_Subtract(focusPos, &mob->pos, &eVec);
        FPoint_ToFRPoint(&eVec, NULL, &reVec);
        reVec.radius = vweight;
        FRPoint_Add(rForce, &reVec, rForce);
    }

    void flockCores(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *core = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);

        if (core != NULL) {
            applyBundle(mob, rForce, &myConfig.cores, &core->pos);
        }
    }

    void flockEnemies(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *enemy = sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);

        if (enemy != NULL) {
            applyBundle(mob, rForce, &myConfig.enemy, &enemy->pos);
        }
    }

    void flockCenter(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FPoint center;
        center.x = myFleetAI->bp.width / 2;
        center.y = myFleetAI->bp.height / 2;
        applyBundle(mob, rForce, &myConfig.center, &center);
    }

    void flockMobLocus(Mob *mob, FRPoint *rForce) {
        FPoint mobLocus;
        FPoint *randomPoint = NULL;
        uint tick = myFleetAI->tick;
        BundleLocusPointParams pp;
        BundleShipAI *ship = (BundleShipAI *)getShip(mob->mobid);
        float randomPeriod = getBundleAtom(mob, &myConfig.mobLocus.randomPeriod);
        float randomWeight = getBundleValue(mob, &myConfig.mobLocus.randomWeight);
        LiveLocusState *live;
        float width = myFleetAI->bp.width;
        float height = myFleetAI->bp.height;

        ASSERT(ship != NULL);
        live = &ship->myShipLive.mobLocus;

        if (randomPeriod > 0.0f && randomWeight != 0.0f) {
            if (live->randomTick == 0 ||
                tick - live->randomTick > randomPeriod) {
                RandomState *rs = &myRandomState;
                live->randomPoint.x = RandomState_Float(rs, 0.0f, width);
                live->randomPoint.y = RandomState_Float(rs, 0.0f, height);
                live->randomTick = tick;
            }

            randomPoint = &live->randomPoint;
        }

        pp.circularPeriod = getBundleAtom(mob, &myConfig.mobLocus.circularPeriod);
        pp.circularWeight = getBundleValue(mob, &myConfig.mobLocus.circularWeight);
        pp.linearXPeriod = getBundleAtom(mob, &myConfig.mobLocus.linearXPeriod);
        pp.linearYPeriod = getBundleAtom(mob, &myConfig.mobLocus.linearYPeriod);
        pp.linearWeight = getBundleValue(mob, &myConfig.mobLocus.linearWeight);
        pp.randomWeight = getBundleValue(mob, &myConfig.mobLocus.randomWeight);
        pp.useScaled = myConfig.mobLocus.useScaled;

        if (getLocusPoint(mob, &pp, randomPoint, &mobLocus)) {
            applyBundle(mob, rForce, &myConfig.mobLocus.force, &mobLocus);
        }
    }

    void flockFleetLocus(Mob *mob, FRPoint *rForce) {
        FPoint fleetLocus;
        FPoint *randomPoint = NULL;
        uint tick = myFleetAI->tick;
        float randomPeriod = myConfig.fleetLocus.randomPeriod;
        LiveLocusState *live = &myLive.fleetLocus;
        float width = myFleetAI->bp.width;
        float height = myFleetAI->bp.height;

        if (randomPeriod > 0.0f &&
            myConfig.fleetLocus.params.randomWeight != 0.0f) {
            /*
             * XXX: Each ship will get a different random locus on the first
             * tick.
             */
            if (live->randomTick == 0 ||
                tick - live->randomTick > randomPeriod) {
                RandomState *rs = &myRandomState;
                live->randomPoint.x = RandomState_Float(rs, 0.0f, width);
                live->randomPoint.y = RandomState_Float(rs, 0.0f, height);
                live->randomTick = tick;
            }

            randomPoint = &live->randomPoint;
        }

        if (getLocusPoint(mob, &myConfig.fleetLocus.params, randomPoint,
                          &fleetLocus)) {
            applyBundle(mob, rForce, &myConfig.fleetLocus.force, &fleetLocus);
        }
    }

    /*
     * getLocusPoint --
     *    Calculate the locus point from the provided parameters.
     *    Returns TRUE iff we have a locus point.
     */
    bool getLocusPoint(Mob *mob, BundleLocusPointParams *pp,
                       const FPoint *randomPointIn,
                       FPoint *outPoint) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FPoint circular;
        FPoint linear;
        FPoint randomPoint;
        bool haveCircular = FALSE;
        bool haveLinear = FALSE;
        bool haveRandom = FALSE;
        uint tick = myFleetAI->tick;
        float width = myFleetAI->bp.width;
        float height = myFleetAI->bp.height;
        float temp;

        MBUtil_Zero(&randomPoint, sizeof(randomPoint));
        MBUtil_Zero(&circular, sizeof(circular));
        MBUtil_Zero(&linear, sizeof(linear));

        if (pp->circularPeriod > 0.0f && pp->circularWeight != 0.0f) {
            float cwidth = width / 2;
            float cheight = height / 2;
            float ct = tick / pp->circularPeriod;

            /*
             * This isn't actually the circumference of an ellipse,
             * but it's a good approximation.
             */
            ct /= M_PI * (cwidth + cheight);

            circular.x = cwidth + cwidth * cosf(ct);
            circular.y = cheight + cheight * sinf(ct);
            haveCircular = TRUE;
        }

        if (randomPointIn != NULL) {
            ASSERT(pp->randomWeight != 0.0f);
            randomPoint = *randomPointIn;
            haveRandom = TRUE;
        }

        if (pp->linearXPeriod > 0.0f && pp->linearWeight != 0.0f) {
            float ltx = tick / pp->linearXPeriod;
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

        if (pp->linearYPeriod > 0.0f && pp->linearWeight != 0.0f) {
            float lty = tick / pp->linearYPeriod;
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
            FPoint locusPoint;
            float scale = 0.0f;
            locusPoint.x = 0.0f;
            locusPoint.y = 0.0f;
            if (haveLinear) {
                locusPoint.x += pp->linearWeight * linear.x;
                locusPoint.y += pp->linearWeight * linear.y;
                scale += pp->linearWeight;
            }
            if (haveCircular) {
                locusPoint.x += pp->circularWeight * circular.x;
                locusPoint.y += pp->circularWeight * circular.y;
                scale += pp->circularWeight;
            }
            if (haveRandom) {
                locusPoint.x += pp->randomWeight * randomPoint.x;
                locusPoint.y += pp->randomWeight * randomPoint.y;
                scale += pp->randomWeight;
            }

            if (pp->useScaled) {
                if (scale != 0.0f) {
                    locusPoint.x /= scale;
                    locusPoint.y /= scale;
                }
            }

            *outPoint = locusPoint;
            return TRUE;
        }

        return FALSE;
    }

    void flockBase(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *base = sg->friendBase();
        if (base != NULL) {
            applyBundle(mob, rForce, &myConfig.base, &base->pos);
        }
    }

    void flockBaseDefense(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        Mob *base = sg->friendBase();

        if (base != NULL) {
            Mob *enemy = sg->findClosestTarget(&base->pos, MOB_FLAG_SHIP);

            if (enemy != NULL) {
                applyBundle(mob, rForce, &myConfig.baseDefense, &enemy->pos);
            }
        }
    }

    void flockEnemyBase(Mob *mob, FRPoint *rForce) {
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
            rForce.radius = getBundleValue(mob, &myConfig.curHeadingWeight);

            flockAlign(mob, &rForce);
            flockCohere(mob, &rForce);
            flockSeparate(mob, &rForce, &myConfig.separate);

            flockEdges(mob, &rForce);
            flockCenter(mob, &rForce);
            flockBase(mob, &rForce);
            flockBaseDefense(mob, &rForce);
            flockEnemies(mob, &rForce);
            flockEnemyBase(mob, &rForce);
            flockCores(mob, &rForce);
            flockFleetLocus(mob, &rForce);
            flockMobLocus(mob, &rForce);

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

    BundleSpec myConfig;
    struct {
        LiveLocusState fleetLocus;
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
                                   MutationType bType,
                                   MBRegistry *mreg)
{
    MBUtil_Zero(vf, sizeof(*vf));
    Mutate_DefaultFloatParams(vf, bType);
    vf->key = key;

    if (MBRegistry_GetBool(mreg, BUNDLE_SCRAMBLE_KEY)) {
        vf->mutationRate = 1.0f;
        vf->jumpRate = 1.0f;
    }
}

static void GetMutationStrParams(MutationStrParams *svf,
                                 const char *key,
                                 MBRegistry *mreg)
{
    MBUtil_Zero(svf, sizeof(*svf));
    svf->key = key;
    svf->flipRate = 0.01f;

    if (MBRegistry_GetBool(mreg, BUNDLE_SCRAMBLE_KEY)) {
        svf->flipRate = 0.5f;
    }
}

static void MutateBundleAtom(FleetAIType aiType, MBRegistry *mreg,
                             const char *prefix,
                             MutationType bType)
{
    MutationFloatParams vf;

    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".value");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), bType, mreg);
    Mutate_Float(mreg, &vf, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".mobJitterScale");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s),
                           MUTATION_TYPE_MOB_JITTER_SCALE, mreg);
    Mutate_Float(mreg, &vf, 1);

    MBString_Destroy(&s);
}

static void MutateBundlePeriodicParams(FleetAIType aiType, MBRegistry *mreg,
                                       const char *prefix)
{
    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".period");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s), MUTATION_TYPE_PERIOD);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".amplitude");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s),
                     MUTATION_TYPE_AMPLITUDE);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".tickShift");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s), MUTATION_TYPE_PERIOD);

    MBString_Destroy(&s);
}

static void MutateBundleValue(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix,
                              MutationType bType)
{
    MutationStrParams svf;

    CMBString s;
    MBString_Create(&s);

    const char *options[] = {
        "constant", "periodic",
    };

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".valueType");
    GetMutationStrParams(&svf, MBString_GetCStr(&s), mreg);
    Mutate_Str(mreg, &svf, 1, options, ARRAYSIZE(options));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".value");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s), bType);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".periodic");
    MutateBundlePeriodicParams(aiType, mreg, MBString_GetCStr(&s));

    MBString_Destroy(&s);
}

static void MutateBundleForce(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    MutationStrParams svf;

    const char *checkOptions[] = {
        "never", "always", "strictOn", "strictOff", "linearUp", "linearDown",
        "quadraticUp", "quadraticDown"
    };

    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowdType");
    GetMutationStrParams(&svf, MBString_GetCStr(&s), mreg);
    Mutate_Str(mreg, &svf, 1, checkOptions, ARRAYSIZE(checkOptions));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".rangeType");
    GetMutationStrParams(&svf, MBString_GetCStr(&s), mreg);
    Mutate_Str(mreg, &svf, 1, checkOptions, ARRAYSIZE(checkOptions));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".weight");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_WEIGHT);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".radius");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_RADIUS);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowd.size");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_COUNT);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowd.radius");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_RADIUS);

    MBString_Destroy(&s);
}


static void MutateBundleFleetLocus(FleetAIType aiType, MBRegistry *mreg,
                                   const char *prefix)
{
    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".force");
    MutateBundleForce(aiType, mreg, MBString_GetCStr(&s));

    MutationFloatParams vf[] = {
        // key                min     max       mag   jump   mutation
        { ".circularPeriod",  -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { ".circularWeight",   0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { ".linearXPeriod",   -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { ".linearYPeriod",   -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { ".linearWeight",     0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { ".randomWeight",     0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { ".randomPeriod",    -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
    };

    MutationBoolParams vb[] = {
        // key                       mutation
        { ".useScaledLocus",          0.01f},
    };

    for (uint i = 0; i < ARRAYSIZE(vf); i++) {
        MutationFloatParams mfp = vf[i];

        if (MBRegistry_GetBool(mreg, BUNDLE_SCRAMBLE_KEY)) {
            mfp.mutationRate = 1.0f;
            mfp.jumpRate = 1.0f;
        }

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, mfp.key);
        mfp.key = MBString_GetCStr(&s);

        Mutate_Float(mreg, &mfp, 1);
    }

    for (uint i = 0; i < ARRAYSIZE(vb); i++) {
        MutationBoolParams mbp = vb[i];

        if (MBRegistry_GetBool(mreg, BUNDLE_SCRAMBLE_KEY)) {
            mbp.flipRate = 0.5f;
        }

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, mbp.key);
        mbp.key = MBString_GetCStr(&s);

        Mutate_Bool(mreg, &mbp, 1);
    }

    MBString_Destroy(&s);
}

static void MutateBundleMobLocus(FleetAIType aiType, MBRegistry *mreg,
                                 const char *prefix)
{
    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".force");
    MutateBundleForce(aiType, mreg, MBString_GetCStr(&s));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".circularPeriod");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s),
                     MUTATION_TYPE_PERIOD);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".circularWeight");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_WEIGHT);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".linearXPeriod");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s),
                     MUTATION_TYPE_PERIOD);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".linearYPeriod");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s),
                     MUTATION_TYPE_PERIOD);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".linearWeight");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_WEIGHT);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".randomPeriod");
    MutateBundleAtom(aiType, mreg, MBString_GetCStr(&s),
                     MUTATION_TYPE_PERIOD);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".randomWeight");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_WEIGHT);

    MutationBoolParams vb[] = {
        // key                       mutation
        { ".useScaledLocus",          0.01f},
    };

    for (uint i = 0; i < ARRAYSIZE(vb); i++) {
        MutationBoolParams mbp = vb[i];

        if (MBRegistry_GetBool(mreg, BUNDLE_SCRAMBLE_KEY)) {
            mbp.flipRate = 0.5f;
        }

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, mbp.key);
        mbp.key = MBString_GetCStr(&s);

        Mutate_Bool(mreg, &mbp, 1);
    }

    MBString_Destroy(&s);
}

static void BundleFleetMutate(FleetAIType aiType, MBRegistry *mreg)
{
    MutationFloatParams vf[] = {
        // key                     min     max       mag   jump   mutation
        { "evadeStrictDistance",  -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "evadeRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "attackRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "guardRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.10f},
        { "gatherRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "startingMaxRadius",    1000.0f, 2000.0f, 0.05f, 0.10f, 0.20f},
        { "startingMinRadius",    300.0f,  800.0f,  0.05f, 0.10f, 0.20f},

        { "nearBaseRadius",        1.0f,   500.0f,  0.05f, 0.15f, 0.01f},
        { "baseDefenseRadius",     1.0f,   500.0f,  0.05f, 0.15f, 0.01f},

        /*
         * Not mutated:
         *    creditReserve
         *    sensorGrid.staleCoreTime
         *    sensorGrid.staleFighterTime
         */
    };

    MutationBoolParams vb[] = {
        // key                       mutation
        { "evadeFighters",           0.05f},
        { "evadeUseStrictDistance",  0.05f},
        { "attackExtendedRange",     0.05f},
        { "rotateStartingAngle",     0.05f},
        { "gatherAbandonStale",      0.05f},
        { "randomIdle",              0.01f},
    };

    MBRegistry_PutCopy(mreg, BUNDLE_SCRAMBLE_KEY, "FALSE");
    if (Random_Flip(0.01)) {
        MBRegistry_PutCopy(mreg, BUNDLE_SCRAMBLE_KEY, "TRUE");

        for (uint i = 0; i < ARRAYSIZE(vf); i++) {
            vf[i].mutationRate = 1.0f;
            vf[i].jumpRate = 1.0f;
        }
        for (uint i = 0; i < ARRAYSIZE(vb); i++) {
            vb[i].flipRate = 0.5f;
        }
    }

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
    MutateBundleForce(aiType, mreg, "baseDefense");

    MutateBundleValue(aiType, mreg, "curHeadingWeight", MUTATION_TYPE_WEIGHT);

    MutateBundleFleetLocus(aiType, mreg, "fleetLocus");
    MutateBundleMobLocus(aiType, mreg, "mobLocus");

    MBRegistry_Remove(mreg, BUNDLE_SCRAMBLE_KEY);
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
