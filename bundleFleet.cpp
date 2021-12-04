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

#define BUNDLE_SCRAMBLE_KEY "bundleFleet.scrambleMutation"

typedef enum BundleCheckType {
    BUNDLE_CHECK_INVALID = 0,
    BUNDLE_CHECK_NEVER,
    BUNDLE_CHECK_ALWAYS,
    BUNDLE_CHECK_STRICT_ON,
    BUNDLE_CHECK_STRICT_OFF,
    BUNDLE_CHECK_LINEAR_UP,
    BUNDLE_CHECK_LINEAR_DOWN,
} BundleCheckType;

typedef uint32 BundleValueFlags;
#define BUNDLE_VALUE_FLAG_NONE     (0)
#define BUNDLE_VALUE_FLAG_PERIODIC (1 << 0)

typedef struct BundleValue {
    BundleValueFlags flags;
    float value;
    float mobJitter;
    float period;
    float periodMobJitter;
    float amplitude;
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
            { "align.crowd.radius.amplitude", "0.321261", },
            { "align.crowd.radius.period", "5953.474121", },
            { "align.crowd.radius.periodMobJitter", "8427.175781", },
            { "align.crowd.radius.value", "1807.180176", },
            { "align.crowd.radius.valueType", "constant", },
            { "align.crowd.size.amplitude", "0.429708", },
            { "align.crowd.size.period", "9243.138672", },
            { "align.crowd.size.periodMobJitter", "1494.291748", },
            { "align.crowd.size.value", "11.233359", },
            { "align.crowd.size.valueType", "periodic", },
            { "align.crowdType", "never", },
            { "align.radius.amplitude", "0.566070", },
            { "align.radius.period", "9900.000000", },
            { "align.radius.periodMobJitter", "7460.959473", },
            { "align.radius.value", "1350.809570", },
            { "align.radius.valueType", "periodic", },
            { "align.rangeType", "linearDown", },
            { "align.weight.amplitude", "0.515353", },
            { "align.weight.period", "9000.000000", },
            { "align.weight.periodMobJitter", "9037.713867", },
            { "align.weight.value", "0.533934", },
            { "align.weight.valueType", "periodic", },
            { "attackExtendedRange", "FALSE", },
            { "attackRange", "117.644791", },
            { "attackSeparate.crowd.radius.amplitude", "0.114193", },
            { "attackSeparate.crowd.radius.period", "7805.395020", },
            { "attackSeparate.crowd.radius.periodMobJitter", "919.714233", },
            { "attackSeparate.crowd.radius.value", "388.137054", },
            { "attackSeparate.crowd.radius.valueType", "periodic", },
            { "attackSeparate.crowd.size.amplitude", "0.095685", },
            { "attackSeparate.crowd.size.period", "8551.041992", },
            { "attackSeparate.crowd.size.periodMobJitter", "6699.266113", },
            { "attackSeparate.crowd.size.value", "8.597579", },
            { "attackSeparate.crowd.size.valueType", "constant", },
            { "attackSeparate.crowdType", "linearDown", },
            { "attackSeparate.radius.amplitude", "1.000000", },
            { "attackSeparate.radius.period", "1345.166748", },
            { "attackSeparate.radius.periodMobJitter", "2289.478760", },
            { "attackSeparate.radius.value", "355.393280", },
            { "attackSeparate.radius.valueType", "periodic", },
            { "attackSeparate.rangeType", "always", },
            { "attackSeparate.weight.amplitude", "0.766508", },
            { "attackSeparate.weight.period", "5715.046875", },
            { "attackSeparate.weight.periodMobJitter", "8829.486328", },
            { "attackSeparate.weight.value", "0.689100", },
            { "attackSeparate.weight.valueType", "periodic", },
            { "base.crowd.radius.amplitude", "0.600368", },
            { "base.crowd.radius.period", "5266.161133", },
            { "base.crowd.radius.periodMobJitter", "7427.318848", },
            { "base.crowd.radius.value", "1078.740356", },
            { "base.crowd.radius.valueType", "periodic", },
            { "base.crowd.size.amplitude", "0.664938", },
            { "base.crowd.size.period", "2936.962402", },
            { "base.crowd.size.periodMobJitter", "5052.449219", },
            { "base.crowd.size.value", "0.521992", },
            { "base.crowd.size.valueType", "constant", },
            { "base.crowdType", "strictOff", },
            { "base.radius.amplitude", "0.407606", },
            { "base.radius.period", "4651.818359", },
            { "base.radius.periodMobJitter", "8930.140625", },
            { "base.radius.value", "970.598145", },
            { "base.radius.valueType", "constant", },
            { "base.rangeType", "strictOff", },
            { "base.weight.amplitude", "0.753670", },
            { "base.weight.period", "-1.000000", },
            { "base.weight.periodMobJitter", "4262.850098", },
            { "base.weight.value", "-1.416888", },
            { "base.weight.valueType", "constant", },
            { "baseDefenseRadius", "143.515045", },
            { "center.crowd.radius.amplitude", "0.733661", },
            { "center.crowd.radius.period", "6119.505371", },
            { "center.crowd.radius.periodMobJitter", "4453.274414", },
            { "center.crowd.radius.value", "564.073486", },
            { "center.crowd.radius.valueType", "periodic", },
            { "center.crowd.size.amplitude", "1.000000", },
            { "center.crowd.size.period", "8173.202148", },
            { "center.crowd.size.periodMobJitter", "2191.400635", },
            { "center.crowd.size.value", "0.074628", },
            { "center.crowd.size.valueType", "periodic", },
            { "center.crowdType", "never", },
            { "center.radius.amplitude", "1.000000", },
            { "center.radius.period", "7462.924316", },
            { "center.radius.periodMobJitter", "6026.039551", },
            { "center.radius.value", "682.307922", },
            { "center.radius.valueType", "constant", },
            { "center.rangeType", "always", },
            { "center.weight.amplitude", "0.806573", },
            { "center.weight.period", "565.521851", },
            { "center.weight.periodMobJitter", "2348.031738", },
            { "center.weight.value", "-1.305155", },
            { "center.weight.valueType", "constant", },
            { "cohere.crowd.radius.amplitude", "0.756902", },
            { "cohere.crowd.radius.period", "7789.553223", },
            { "cohere.crowd.radius.periodMobJitter", "8247.803711", },
            { "cohere.crowd.radius.value", "1782.000000", },
            { "cohere.crowd.radius.valueType", "constant", },
            { "cohere.crowd.size.amplitude", "1.000000", },
            { "cohere.crowd.size.period", "-1.000000", },
            { "cohere.crowd.size.periodMobJitter", "5202.102539", },
            { "cohere.crowd.size.value", "3.687377", },
            { "cohere.crowd.size.valueType", "periodic", },
            { "cohere.crowdType", "linearUp", },
            { "cohere.radius.amplitude", "0.306166", },
            { "cohere.radius.period", "10000.000000", },
            { "cohere.radius.periodMobJitter", "6275.174316", },
            { "cohere.radius.value", "1914.735596", },
            { "cohere.radius.valueType", "periodic", },
            { "cohere.rangeType", "linearDown", },
            { "cohere.weight.amplitude", "0.266461", },
            { "cohere.weight.period", "1867.322510", },
            { "cohere.weight.periodMobJitter", "3501.302979", },
            { "cohere.weight.value", "-0.507121", },
            { "cohere.weight.valueType", "constant", },
            { "cores.crowd.radius.amplitude", "0.525464", },
            { "cores.crowd.radius.period", "6589.460938", },
            { "cores.crowd.radius.periodMobJitter", "3955.157471", },
            { "cores.crowd.radius.value", "174.267288", },
            { "cores.crowd.radius.valueType", "periodic", },
            { "cores.crowd.size.amplitude", "0.055397", },
            { "cores.crowd.size.period", "3110.003174", },
            { "cores.crowd.size.periodMobJitter", "958.940796", },
            { "cores.crowd.size.value", "8.727318", },
            { "cores.crowd.size.valueType", "constant", },
            { "cores.crowdType", "linearDown", },
            { "cores.radius.amplitude", "0.640855", },
            { "cores.radius.period", "9444.480469", },
            { "cores.radius.periodMobJitter", "3977.586182", },
            { "cores.radius.value", "35.896736", },
            { "cores.radius.valueType", "periodic", },
            { "cores.rangeType", "never", },
            { "cores.weight.amplitude", "0.010756", },
            { "cores.weight.period", "2880.781250", },
            { "cores.weight.periodMobJitter", "2923.172607", },
            { "cores.weight.value", "1.0", },
            { "cores.weight.valueType", "constant", },
            { "curHeadingWeight.amplitude", "1.000000", },
            { "curHeadingWeight.period", "2207.250000", },
            { "curHeadingWeight.periodMobJitter", "4729.488281", },
            { "curHeadingWeight.value", "-3.811037", },
            { "curHeadingWeight.valueType", "constant", },
            { "edges.crowd.radius.amplitude", "0.447709", },
            { "edges.crowd.radius.period", "8368.186523", },
            { "edges.crowd.radius.periodMobJitter", "6223.491699", },
            { "edges.crowd.radius.value", "1260.249023", },
            { "edges.crowd.radius.valueType", "periodic", },
            { "edges.crowd.size.amplitude", "0.000000", },
            { "edges.crowd.size.period", "999.697876", },
            { "edges.crowd.size.periodMobJitter", "4573.829590", },
            { "edges.crowd.size.value", "6.702061", },
            { "edges.crowd.size.valueType", "constant", },
            { "edges.crowdType", "never", },
            { "edges.radius.amplitude", "0.389971", },
            { "edges.radius.period", "1560.454834", },
            { "edges.radius.periodMobJitter", "2084.474609", },
            { "edges.radius.value", "50.840942", },
            { "edges.radius.valueType", "constant", },
            { "edges.rangeType", "strictOff", },
            { "edges.weight.amplitude", "0", },
            { "edges.weight.period", "0", },
            { "edges.weight.periodMobJitter", "9518.350586", },
            { "edges.weight.value", "1.0", },
            { "edges.weight.valueType", "constant", },
            { "enemy.crowd.radius.amplitude", "0.798566", },
            { "enemy.crowd.radius.period", "7607.696289", },
            { "enemy.crowd.radius.periodMobJitter", "3514.106201", },
            { "enemy.crowd.radius.value", "203.481049", },
            { "enemy.crowd.radius.valueType", "constant", },
            { "enemy.crowd.size.amplitude", "0.618705", },
            { "enemy.crowd.size.period", "7847.218750", },
            { "enemy.crowd.size.periodMobJitter", "974.588196", },
            { "enemy.crowd.size.value", "20.000000", },
            { "enemy.crowd.size.valueType", "periodic", },
            { "enemy.crowdType", "always", },
            { "enemy.radius.amplitude", "0.090641", },
            { "enemy.radius.period", "3363.753906", },
            { "enemy.radius.periodMobJitter", "1729.533447", },
            { "enemy.radius.value", "611.284424", },
            { "enemy.radius.valueType", "constant", },
            { "enemy.rangeType", "strictOn", },
            { "enemy.weight.amplitude", "0.958385", },
            { "enemy.weight.period", "4596.960449", },
            { "enemy.weight.periodMobJitter", "-1.000000", },
            { "enemy.weight.value", "-1.185188", },
            { "enemy.weight.valueType", "constant", },
            { "enemyBase.crowd.radius.amplitude", "0.736067", },
            { "enemyBase.crowd.radius.period", "6412.087402", },
            { "enemyBase.crowd.radius.periodMobJitter", "3809.063232", },
            { "enemyBase.crowd.radius.value", "1114.323120", },
            { "enemyBase.crowd.radius.valueType", "periodic", },
            { "enemyBase.crowd.size.amplitude", "1.000000", },
            { "enemyBase.crowd.size.period", "909.905334", },
            { "enemyBase.crowd.size.periodMobJitter", "9132.360352", },
            { "enemyBase.crowd.size.value", "10.031953", },
            { "enemyBase.crowd.size.valueType", "periodic", },
            { "enemyBase.crowdType", "always", },
            { "enemyBase.radius.amplitude", "0.559740", },
            { "enemyBase.radius.period", "4657.600586", },
            { "enemyBase.radius.periodMobJitter", "7402.912598", },
            { "enemyBase.radius.value", "693.966919", },
            { "enemyBase.radius.valueType", "periodic", },
            { "enemyBase.rangeType", "never", },
            { "enemyBase.weight.amplitude", "0.121047", },
            { "enemyBase.weight.period", "8157.837891", },
            { "enemyBase.weight.periodMobJitter", "6083.194824", },
            { "enemyBase.weight.value", "0.081705", },
            { "enemyBase.weight.valueType", "constant", },
            { "evadeFighters", "FALSE", },
            { "evadeRange", "283.460571", },
            { "evadeStrictDistance", "87.064606", },
            { "evadeUseStrictDistance", "FALSE", },
            { "gatherAbandonStale", "TRUE", },
            { "gatherRange", "216.282059", },
            { "guardRange", "-0.902500", },
            { "locus.crowd.radius.amplitude", "0.830518", },
            { "locus.crowd.radius.period", "705.356079", },
            { "locus.crowd.radius.periodMobJitter", "7817.947754", },
            { "locus.crowd.radius.value", "1683.359131", },
            { "locus.crowd.radius.valueType", "constant", },
            { "locus.crowd.size.amplitude", "0.807986", },
            { "locus.crowd.size.period", "8092.102051", },
            { "locus.crowd.size.periodMobJitter", "1793.675171", },
            { "locus.crowd.size.value", "10.801899", },
            { "locus.crowd.size.valueType", "constant", },
            { "locus.crowdType", "linearUp", },
            { "locus.radius.amplitude", "0.280220", },
            { "locus.radius.period", "6379.359375", },
            { "locus.radius.periodMobJitter", "4677.827148", },
            { "locus.radius.value", "1326.336304", },
            { "locus.radius.valueType", "periodic", },
            { "locus.rangeType", "always", },
            { "locus.weight.amplitude", "0.000000", },
            { "locus.weight.period", "4181.989746", },
            { "locus.weight.periodMobJitter", "1684.508057", },
            { "locus.weight.value", "4.035198", },
            { "locus.weight.valueType", "constant", },
            { "locusCircularPeriod", "10309.558594", },
            { "locusCircularWeight", "0.856374", },
            { "locusLinearWeight", "1.804331", },
            { "locusLinearXPeriod", "1598.433105", },
            { "locusLinearYPeriod", "9407.249023", },
            { "locusRandomPeriod", "7426.138184", },
            { "locusRandomWeight", "0.471003", },
            { "nearBaseRadius", "423.256439", },
            { "randomIdle", "TRUE", },
            { "rotateStartingAngle", "FALSE", },
            { "separate.crowd.radius.amplitude", "0.141071", },
            { "separate.crowd.radius.period", "2654.302979", },
            { "separate.crowd.radius.periodMobJitter", "5491.818359", },
            { "separate.crowd.radius.value", "981.912476", },
            { "separate.crowd.radius.valueType", "periodic", },
            { "separate.crowd.size.amplitude", "0.782046", },
            { "separate.crowd.size.period", "6823.173828", },
            { "separate.crowd.size.periodMobJitter", "7784.186523", },
            { "separate.crowd.size.value", "11.729516", },
            { "separate.crowd.size.valueType", "constant", },
            { "separate.crowdType", "linearUp", },
            { "separate.radius.amplitude", "0.095367", },
            { "separate.radius.period", "2932.626221", },
            { "separate.radius.periodMobJitter", "6692.702637", },
            { "separate.radius.value", "2000.000000", },
            { "separate.radius.valueType", "periodic", },
            { "separate.rangeType", "strictOff", },
            { "separate.weight.amplitude", "0.694014", },
            { "separate.weight.period", "8443.943359", },
            { "separate.weight.periodMobJitter", "3759.474854", },
            { "separate.weight.value", "1.573738", },
            { "separate.weight.valueType", "constant", },
            { "startingMaxRadius", "1295.414795", },
            { "startingMinRadius", "642.803894", },
            { "useScaledLocus", "TRUE", },
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
        bv->value = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(bv->value));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".value.mobJitter");
        bv->value = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(bv->mobJitter));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".period");
        bv->period = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(bv->period));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".periodMobJitter");
        bv->periodMobJitter = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(bv->periodMobJitter));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".amplitude");
        bv->amplitude = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(bv->amplitude));

        MBString_Destroy(&s);
    }

    virtual void loadBundleCheck(MBRegistry *mreg, BundleCheckType *bc,
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
        } else {
            PANIC("Unknown rangeType = %s\n", cs);
        }
    }

    virtual void loadBundleForce(MBRegistry *mreg, BundleForce *b,
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

    float getMobJitter(Mob *m, float modulo) {
        int mobOffset;
        RandomState *rs = &myRandomState;

        if (modulo <= 0.0f) {
            return 0.0f;
        }

        if (!myMobJitters.lookup(m->mobid, &mobOffset)) {
            // 24-bits for maximum float precision
            uint32 mask = 0xFFFFFF;
            mobOffset = RandomState_Uint32(rs) & mask;
            myMobJitters.put(m->mobid, mobOffset);
        }

        return fmodf((float)mobOffset, modulo);
    }

    float getBundleValue(Mob *m, BundleValue *bv) {
        float value;
        if ((bv->flags & BUNDLE_VALUE_FLAG_PERIODIC) != 0 &&
            bv->amplitude > 0.0f && bv->period > 1.0f) {
            float p = bv->period;
            float t = myFleetAI->tick + getMobJitter(m, bv->periodMobJitter);
            float a = bv->amplitude;
            value = bv->value * (1.0f + a * sinf(t / p));
        } else {
            value = bv->value;
        }

        value += getMobJitter(m, bv->mobJitter);
        return value;
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
        } else if (bc == BUNDLE_CHECK_LINEAR_UP) {
            *weight = value / trigger;
            return TRUE;
        } else if (bc == BUNDLE_CHECK_LINEAR_DOWN) {
            *weight = trigger / value;
            return TRUE;
        } else {
            PANIC("Unknown BundleCheckType: %d\n", bc);
        }

        NOT_REACHED();
    }

    /*
     * Should this force operate given the current crowd size?
     * Returns TRUE iff the force should operate.
     */
    bool crowdCheck(Mob *mob, BundleForce *bundle, float *weight) {
        SensorGrid *sg = mySensorGrid;

        float crowdTrigger = getBundleValue(mob, &bundle->crowd.size);
        float crowdRadius = getBundleValue(mob, &bundle->crowd.radius);

        float crowdValue = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                 &mob->pos, crowdRadius);
        return bundleCheck(bundle->crowdCheck, crowdValue, crowdTrigger, weight);
    }

    void applyBundle(Mob *mob, FRPoint *rForce, BundleForce *bundle,
                     FPoint *focusPos) {
        float cweight;
        if (!crowdCheck(mob, bundle, &cweight)) {
            /* No force. */
            return;
        }

        float radius = getBundleValue(mob, &bundle->radius);

        if (isnanf(radius) || radius <= 0.0f) {
            /* No force. */
            return;
        }

        float distance = FPoint_Distance(&mob->pos, focusPos);
        float rweight;

        if (!bundleCheck(bundle->rangeCheck, distance, radius, &rweight)) {
            /* No force. */
            return;
        }

        float vweight = rweight * cweight *getBundleValue(mob, &bundle->weight);

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
            rForce.radius = getBundleValue(mob, &myConfig.curHeadingWeight);

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

    IntMap myMobJitters;
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

static void MutateBundleValue(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix,
                              MutationType bType)
{
    MutationFloatParams vf;
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
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), bType, mreg);
    Mutate_Float(mreg, &vf, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".value.mobJitter");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), bType, mreg);
    Mutate_Float(mreg, &vf, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".period");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), MUTATION_TYPE_PERIOD,
                           mreg);
    Mutate_Float(mreg, &vf, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".periodMobJitter");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), MUTATION_TYPE_PERIOD,
                           mreg);
    Mutate_Float(mreg, &vf, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".amplitude");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s), MUTATION_TYPE_AMPLITUDE,
                           mreg);
    Mutate_Float(mreg, &vf, 1);

    MBString_Destroy(&s);
}

static void MutateBundleForce(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    MutationStrParams svf;

    const char *checkOptions[] = {
        "never", "always", "strictOn", "strictOff", "linearUp", "linearDown"
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

        { "locusCircularPeriod",  -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { "locusCircularWeight",   0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { "locusLinearXPeriod",   -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { "locusLinearYPeriod",   -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},
        { "locusLinearWeight",     0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { "locusRandomWeight",     0.0f,     2.0f,  0.05f, 0.15f, 0.02f},
        { "locusRandomPeriod",    -1.0f, 12345.0f,  0.05f, 0.15f, 0.02f},

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
        { "useScaledLocus",          0.01f},
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

    MutateBundleValue(aiType, mreg, "curHeadingWeight", MUTATION_TYPE_WEIGHT);

    MutateBundleForce(aiType, mreg, "locus");

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
