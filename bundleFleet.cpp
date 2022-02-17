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
    float mobJitterScalePow;
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
    BundleCheckType check;
    BundleValue size;
    BundleValue radius;
} BundleCrowd;

typedef struct BundleRange {
    BundleCheckType check;
    BundleValue radius;
} BundleRange;

typedef struct BundleForce {
    BundleValue weight;
    BundleRange range;
    BundleCrowd crowd;
} BundleForce;

typedef struct BundleBool {
    bool forceOn;
    BundleCrowd crowd;
    BundleRange range;
    BundleValue value;
} BundleBool;

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
    bool resetOnProximity;
    BundleValue proximityRadius;
} BundleMobLocus;

typedef struct BundleSpec {
    BundleBool randomIdle;
    BundleBool nearBaseRandomIdle;
    BundleBool randomizeStoppedVelocity;
    BundleBool simpleAttack;
    BundleBool flockDuringAttack;

    BundleForce align;
    BundleForce cohere;
    BundleForce separate;
    BundleForce attackSeparate;
    BundleForce nearestFriend;

    BundleForce center;
    BundleForce edges;
    BundleForce corners;

    BundleForce cores;
    BundleForce base;
    BundleForce baseDefense;

    float nearBaseRadius;
    float baseDefenseRadius;
    bool fighterBaseDefenseUseRadius;
    bool brokenCrowdChecks;
    float maxWeight;

    BundleForce enemy;
    BundleForce enemyBase;
    BundleForce enemyBaseGuess;

    BundleValue curHeadingWeight;

    BundleFleetLocus fleetLocus;
    BundleMobLocus mobLocus;

    BundleValue holdInvProbability;
    BundleValue holdTicks;
} BundleSpec;

#define NUM_MOB_JITTERS (sizeof(BundleSpec) / sizeof(float))

typedef struct BundleConfigValue {
    const char *key;
    const char *value;
} BundleConfigValue;

class BundleAIGovernor : public BasicAIGovernor
{
public:
    BundleAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        MBUtil_Zero(&myLive, sizeof(myLive));
        MBUtil_Zero(&myCache, sizeof(myCache));
    }

    virtual ~BundleAIGovernor() { }

    class BundleShipAI : public BasicShipAI
    {
        public:
        BundleShipAI(MobID mobid, BundleAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        {
            MBUtil_Zero(&myShipLive, sizeof(myShipLive));
        }

        virtual ~BundleShipAI() {}

        struct {
            struct {
                float value;
                bool initialized;
            } mobJitters[NUM_MOB_JITTERS];
            LiveLocusState mobLocus;
        } myShipLive;
    };

    virtual BundleShipAI *newShip(MobID mobid) {
        return new BundleShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        BundleConfigValue defaults[] = {
            { "attackExtendedRange",         "TRUE"      },
            { "attackRange",                 "117.644791"},
            { "baseDefenseRadius",           "143.515045"},
            { "baseSpawnJitter",             "1"         },
            { "creditReserve",               "200"       },

            { "evadeFighters",               "FALSE"     },
            { "evadeRange",                  "289.852631"},
            { "evadeStrictDistance",         "105.764320"},
            { "evadeUseStrictDistance",      "TRUE"      },

            { "gatherAbandonStale",          "FALSE"     },
            { "gatherRange",                 "50"        },
            { "guardRange",                  "0"         },

            { "maxWeight",                   "100.0",    },

            { "nearBaseRandomIdle.forceOn",  "TRUE"      },
            { "randomIdle.forceOn",          "TRUE"      },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "rotateStartingAngle",         "TRUE",     },
            { "simpleAttack.forceOn",        "TRUE"      },

            { "nearBaseRadius",              "100.0"     },
            { "baseDefenseRadius",           "250.0"     },

            { "sensorGrid.staleCoreTime",    "28.385160" },
            { "sensorGrid.staleFighterTime", "16.703636" },

            { "startingMaxRadius",           "300"       },
            { "startingMinRadius",           "250"       },
        };

        BundleConfigValue configs1[] = {
            { "curHeadingWeight.value.value", "1"        },
            { "curHeadingWeight.valueType",   "constant" },
        };

        BundleConfigValue configs2[] = {
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.375753" },
            { "align.range.radius.periodic.amplitude.value", "0.168301" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.108591" },
            { "align.range.radius.periodic.period.value", "5362.412598" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.381522" },
            { "align.range.radius.periodic.tickShift.value", "8914.941406" },
            { "align.range.radius.value.mobJitterScale", "0.506334" },
            { "align.range.radius.value.value", "-1.000000" },
            { "align.range.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.968364" },
            { "align.crowd.size.periodic.amplitude.value", "0.406206" },
            { "align.crowd.size.periodic.period.mobJitterScale", "-0.781509" },
            { "align.crowd.size.periodic.period.value", "7556.751465" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.810000" },
            { "align.crowd.size.periodic.tickShift.value", "3798.598145" },
            { "align.crowd.size.value.mobJitterScale", "0.888799" },
            { "align.crowd.size.value.value", "4.721555" },
            { "align.crowd.size.valueType", "constant" },
            { "align.crowd.type.check", "never" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "0.285279" },
            { "align.range.radius.periodic.amplitude.value", "-0.499126" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.595802" },
            { "align.range.radius.periodic.period.value", "8468.488281" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "-0.768674" },
            { "align.range.radius.periodic.tickShift.value", "3499.867920" },
            { "align.range.radius.value.mobJitterScale", "0.132559" },
            { "align.range.radius.value.value", "1128.759521" },
            { "align.range.radius.valueType", "constant" },
            { "align.range.type.check", "never" },
            { "align.weight.periodic.amplitude.mobJitterScale", "-0.083757" },
            { "align.weight.periodic.amplitude.value", "0.979589" },
            { "align.weight.periodic.period.mobJitterScale", "-0.300993" },
            { "align.weight.periodic.period.value", "8662.593750" },
            { "align.weight.periodic.tickShift.mobJitterScale", "-0.598240" },
            { "align.weight.periodic.tickShift.value", "1096.382080" },
            { "align.weight.value.mobJitterScale", "0.254999" },
            { "align.weight.value.value", "7.067549" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "181.408646" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.146617" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.234275" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "attackSeparate.range.radius.periodic.period.value", "1603.346924" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.546862" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "2839.928223" },
            { "attackSeparate.range.radius.value.mobJitterScale", "0.575492" },
            { "attackSeparate.range.radius.value.value", "505.146301" },
            { "attackSeparate.range.radius.valueType", "constant" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.513018" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.929315" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.795578" },
            { "attackSeparate.crowd.size.periodic.period.value", "5148.794434" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-0.251742" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-1.000000" },
            { "attackSeparate.crowd.size.value.value", "-0.956097" },
            { "attackSeparate.crowd.size.valueType", "periodic" },
            { "attackSeparate.crowd.type.check", "never" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.395586" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.083737" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.394375" },
            { "attackSeparate.range.radius.periodic.period.value", "2825.515381" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.406478" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "3318.301270" },
            { "attackSeparate.range.radius.value.mobJitterScale", "0.080423" },
            { "attackSeparate.range.radius.value.value", "1302.873291" },
            { "attackSeparate.range.radius.valueType", "constant" },
            { "attackSeparate.range.type.check", "never" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.400281" },
            { "attackSeparate.weight.periodic.amplitude.value", "-0.828507" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "0.480294" },
            { "attackSeparate.weight.periodic.period.value", "9100.205078" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-0.363912" },
            { "attackSeparate.weight.periodic.tickShift.value", "7603.175293" },
            { "attackSeparate.weight.value.mobJitterScale", "0.023761" },
            { "attackSeparate.weight.value.value", "-8.338675" },
            { "attackSeparate.weight.valueType", "constant" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "-0.405763" },
            { "base.range.radius.periodic.amplitude.value", "0.457899" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.618923" },
            { "base.range.radius.periodic.period.value", "942.138184" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "base.range.radius.periodic.tickShift.value", "8911.643555" },
            { "base.range.radius.value.mobJitterScale", "-0.674800" },
            { "base.range.radius.value.value", "1929.544434" },
            { "base.range.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.328041" },
            { "base.crowd.size.periodic.amplitude.value", "-0.869783" },
            { "base.crowd.size.periodic.period.mobJitterScale", "0.395191" },
            { "base.crowd.size.periodic.period.value", "10000.000000" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.597640" },
            { "base.crowd.size.periodic.tickShift.value", "5906.695801" },
            { "base.crowd.size.value.mobJitterScale", "-0.447069" },
            { "base.crowd.size.value.value", "2.891666" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "never" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "-0.572601" },
            { "base.range.radius.periodic.amplitude.value", "-1.000000" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.316050" },
            { "base.range.radius.periodic.period.value", "6424.068359" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "-0.560312" },
            { "base.range.radius.periodic.tickShift.value", "2900.507812" },
            { "base.range.radius.value.mobJitterScale", "0.032384" },
            { "base.range.radius.value.value", "789.669678" },
            { "base.range.radius.valueType", "periodic" },
            { "base.range.type.check", "never" },
            { "base.weight.periodic.amplitude.mobJitterScale", "0.455482" },
            { "base.weight.periodic.amplitude.value", "-0.169507" },
            { "base.weight.periodic.period.mobJitterScale", "0.175880" },
            { "base.weight.periodic.period.value", "4732.085449" },
            { "base.weight.periodic.tickShift.mobJitterScale", "0.415551" },
            { "base.weight.periodic.tickShift.value", "3090.739258" },
            { "base.weight.value.mobJitterScale", "-0.220175" },
            { "base.weight.value.value", "0.607997" },
            { "base.weight.valueType", "periodic" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.741825" },
            { "baseDefense.range.radius.periodic.amplitude.value", "-0.694093" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.472881" },
            { "baseDefense.range.radius.periodic.period.value", "9000.000000" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.657454" },
            { "baseDefense.range.radius.periodic.tickShift.value", "2347.528809" },
            { "baseDefense.range.radius.value.mobJitterScale", "-1.000000" },
            { "baseDefense.range.radius.value.value", "673.073914" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "0.609885" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.967685" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "0.768096" },
            { "baseDefense.crowd.size.periodic.period.value", "2894.345947" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "-0.663293" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "7780.861328" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.125935" },
            { "baseDefense.crowd.size.value.value", "3.461880" },
            { "baseDefense.crowd.size.valueType", "periodic" },
            { "baseDefense.crowd.type.check", "never" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "0.101941" },
            { "baseDefense.range.radius.periodic.amplitude.value", "-0.104982" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "-0.348962" },
            { "baseDefense.range.radius.periodic.period.value", "6473.129395" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.325967" },
            { "baseDefense.range.radius.periodic.tickShift.value", "-0.900000" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.282842" },
            { "baseDefense.range.radius.value.value", "1685.650635" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "never" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.320436" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "-0.200933" },
            { "baseDefense.weight.periodic.period.value", "1011.942322" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "-0.917186" },
            { "baseDefense.weight.periodic.tickShift.value", "5695.613281" },
            { "baseDefense.weight.value.mobJitterScale", "-0.663346" },
            { "baseDefense.weight.value.value", "-7.393560" },
            { "baseDefense.weight.valueType", "periodic" },
            { "baseDefenseRadius", "178.805420" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "center.range.radius.periodic.amplitude.value", "-0.950215" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.536294" },
            { "center.range.radius.periodic.period.value", "2385.005127" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "-0.108328" },
            { "center.range.radius.periodic.tickShift.value", "5926.232422" },
            { "center.range.radius.value.mobJitterScale", "-0.543911" },
            { "center.range.radius.value.value", "2000.000000" },
            { "center.range.radius.valueType", "periodic" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.254582" },
            { "center.crowd.size.periodic.amplitude.value", "0.900000" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.860170" },
            { "center.crowd.size.periodic.period.value", "9110.958008" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "0.871353" },
            { "center.crowd.size.periodic.tickShift.value", "1929.109985" },
            { "center.crowd.size.value.mobJitterScale", "-1.000000" },
            { "center.crowd.size.value.value", "10.836775" },
            { "center.crowd.size.valueType", "constant" },
            { "center.crowd.type.check", "never" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.042662" },
            { "center.range.radius.periodic.amplitude.value", "0.475815" },
            { "center.range.radius.periodic.period.mobJitterScale", "0.597518" },
            { "center.range.radius.periodic.period.value", "8034.912598" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.900000" },
            { "center.range.radius.periodic.tickShift.value", "546.022339" },
            { "center.range.radius.value.mobJitterScale", "1.000000" },
            { "center.range.radius.value.value", "1257.381958" },
            { "center.range.radius.valueType", "constant" },
            { "center.range.type.check", "never" },
            { "center.weight.periodic.amplitude.mobJitterScale", "0.468783" },
            { "center.weight.periodic.amplitude.value", "-1.000000" },
            { "center.weight.periodic.period.mobJitterScale", "0.757461" },
            { "center.weight.periodic.period.value", "2054.703857" },
            { "center.weight.periodic.tickShift.mobJitterScale", "0.168499" },
            { "center.weight.periodic.tickShift.value", "8765.216797" },
            { "center.weight.value.mobJitterScale", "-0.260256" },
            { "center.weight.value.value", "9.500000" },
            { "center.weight.valueType", "constant" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.469702" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.536102" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.390235" },
            { "cohere.range.radius.periodic.period.value", "7617.947754" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "0.721943" },
            { "cohere.range.radius.periodic.tickShift.value", "9662.836914" },
            { "cohere.range.radius.value.mobJitterScale", "-0.662899" },
            { "cohere.range.radius.value.value", "1045.569214" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.196299" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.147787" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "-0.270201" },
            { "cohere.crowd.size.periodic.period.value", "1010.301331" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.003834" },
            { "cohere.crowd.size.periodic.tickShift.value", "1548.892822" },
            { "cohere.crowd.size.value.mobJitterScale", "0.559478" },
            { "cohere.crowd.size.value.value", "4.463946" },
            { "cohere.crowd.size.valueType", "periodic" },
            { "cohere.crowd.type.check", "never" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.309307" },
            { "cohere.range.radius.periodic.amplitude.value", "0.562855" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.082082" },
            { "cohere.range.radius.periodic.period.value", "10000.000000" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "0.495175" },
            { "cohere.range.radius.periodic.tickShift.value", "3194.990479" },
            { "cohere.range.radius.value.mobJitterScale", "0.743487" },
            { "cohere.range.radius.value.value", "1184.629150" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "never" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "0.548071" },
            { "cohere.weight.periodic.amplitude.value", "-1.000000" },
            { "cohere.weight.periodic.period.mobJitterScale", "-0.447586" },
            { "cohere.weight.periodic.period.value", "7854.461914" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.270306" },
            { "cohere.weight.periodic.tickShift.value", "1055.695068" },
            { "cohere.weight.value.mobJitterScale", "0.317044" },
            { "cohere.weight.value.value", "-9.957829" },
            { "cohere.weight.valueType", "periodic" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "cores.range.radius.periodic.amplitude.value", "0.240894" },
            { "cores.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "cores.range.radius.periodic.period.value", "3527.785156" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "-0.660024" },
            { "cores.range.radius.periodic.tickShift.value", "7455.280273" },
            { "cores.range.radius.value.mobJitterScale", "-0.293770" },
            { "cores.range.radius.value.value", "1992.707275" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "0.869103" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.899535" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-0.927192" },
            { "cores.crowd.size.periodic.period.value", "2185.385498" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.470268" },
            { "cores.crowd.size.periodic.tickShift.value", "4954.479004" },
            { "cores.crowd.size.value.mobJitterScale", "0.836118" },
            { "cores.crowd.size.value.value", "-1.000000" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowd.type.check", "never" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "-0.406865" },
            { "cores.range.radius.periodic.amplitude.value", "-0.698872" },
            { "cores.range.radius.periodic.period.mobJitterScale", "-0.990000" },
            { "cores.range.radius.periodic.period.value", "6957.399414" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.288618" },
            { "cores.range.radius.periodic.tickShift.value", "1621.274536" },
            { "cores.range.radius.value.mobJitterScale", "0.201092" },
            { "cores.range.radius.value.value", "1698.850952" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.range.type.check", "never" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.776193" },
            { "cores.weight.periodic.amplitude.value", "-1.000000" },
            { "cores.weight.periodic.period.mobJitterScale", "0.789824" },
            { "cores.weight.periodic.period.value", "5594.181641" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.518301" },
            { "cores.weight.periodic.tickShift.value", "8047.308594" },
            { "cores.weight.value.mobJitterScale", "-0.881993" },
            { "cores.weight.value.value", "4.127701" },
            { "cores.weight.valueType", "constant" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.043223" },
            { "corners.range.radius.periodic.amplitude.value", "0.763812" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-0.705491" },
            { "corners.range.radius.periodic.period.value", "3311.499023" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "0.740681" },
            { "corners.range.radius.periodic.tickShift.value", "9048.750977" },
            { "corners.range.radius.value.mobJitterScale", "0.603685" },
            { "corners.range.radius.value.value", "1280.113892" },
            { "corners.range.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "-0.823991" },
            { "corners.crowd.size.periodic.amplitude.value", "-0.229961" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "0.651000" },
            { "corners.crowd.size.periodic.period.value", "4474.759766" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "-0.339310" },
            { "corners.crowd.size.periodic.tickShift.value", "654.322571" },
            { "corners.crowd.size.value.mobJitterScale", "-0.792347" },
            { "corners.crowd.size.value.value", "11.848875" },
            { "corners.crowd.size.valueType", "constant" },
            { "corners.crowd.type.check", "never" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.746028" },
            { "corners.range.radius.periodic.amplitude.value", "0.372991" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-0.328457" },
            { "corners.range.radius.periodic.period.value", "8884.138672" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-0.805783" },
            { "corners.range.radius.periodic.tickShift.value", "2631.196533" },
            { "corners.range.radius.value.mobJitterScale", "-0.846008" },
            { "corners.range.radius.value.value", "1448.135498" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.range.type.check", "never" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "-0.792189" },
            { "corners.weight.periodic.amplitude.value", "0.900000" },
            { "corners.weight.periodic.period.mobJitterScale", "0.609460" },
            { "corners.weight.periodic.period.value", "1473.460571" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "-0.654149" },
            { "corners.weight.periodic.tickShift.value", "-0.900000" },
            { "corners.weight.value.mobJitterScale", "-0.226356" },
            { "corners.weight.value.value", "-1.915910" },
            { "corners.weight.valueType", "periodic" },
            { "creditReserve", "185.292099" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.105169" },
            { "curHeadingWeight.periodic.amplitude.value", "0.218135" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "-0.009044" },
            { "curHeadingWeight.periodic.period.value", "6961.687988" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "-0.517378" },
            { "curHeadingWeight.periodic.tickShift.value", "9763.986328" },
            { "curHeadingWeight.value.mobJitterScale", "0.841872" },
            { "curHeadingWeight.value.value", "2.101316" },
            { "curHeadingWeight.valueType", "periodic" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.597216" },
            { "edges.range.radius.periodic.amplitude.value", "-0.248807" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.865608" },
            { "edges.range.radius.periodic.period.value", "9391.663086" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.607740" },
            { "edges.range.radius.periodic.tickShift.value", "7446.548828" },
            { "edges.range.radius.value.mobJitterScale", "-0.052765" },
            { "edges.range.radius.value.value", "884.600647" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "0.314669" },
            { "edges.crowd.size.periodic.amplitude.value", "0.814894" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "-0.920538" },
            { "edges.crowd.size.periodic.period.value", "8100.000000" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.679343" },
            { "edges.crowd.size.periodic.tickShift.value", "5788.603027" },
            { "edges.crowd.size.value.mobJitterScale", "0.160453" },
            { "edges.crowd.size.value.value", "4.934954" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.crowd.type.check", "never" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.076425" },
            { "edges.range.radius.periodic.amplitude.value", "-0.767398" },
            { "edges.range.radius.periodic.period.mobJitterScale", "0.846003" },
            { "edges.range.radius.periodic.period.value", "1087.767334" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.628644" },
            { "edges.range.radius.periodic.tickShift.value", "8555.723633" },
            { "edges.range.radius.value.mobJitterScale", "-0.400609" },
            { "edges.range.radius.value.value", "579.894897" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.range.type.check", "never" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.626343" },
            { "edges.weight.periodic.amplitude.value", "-1.000000" },
            { "edges.weight.periodic.period.mobJitterScale", "0.779804" },
            { "edges.weight.periodic.period.value", "319.415710" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.514598" },
            { "edges.weight.periodic.tickShift.value", "715.643433" },
            { "edges.weight.value.mobJitterScale", "-0.503806" },
            { "edges.weight.value.value", "7.202097" },
            { "edges.weight.valueType", "periodic" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.013939" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.101509" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "-0.137833" },
            { "enemy.range.radius.periodic.period.value", "9023.547852" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "0.357310" },
            { "enemy.range.radius.periodic.tickShift.value", "7150.236816" },
            { "enemy.range.radius.value.mobJitterScale", "-0.280850" },
            { "enemy.range.radius.value.value", "928.333801" },
            { "enemy.range.radius.valueType", "periodic" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "0.597647" },
            { "enemy.crowd.size.periodic.amplitude.value", "-0.601186" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.801053" },
            { "enemy.crowd.size.periodic.period.value", "90.217484" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "0.054112" },
            { "enemy.crowd.size.periodic.tickShift.value", "7190.281738" },
            { "enemy.crowd.size.value.mobJitterScale", "0.739930" },
            { "enemy.crowd.size.value.value", "20.000000" },
            { "enemy.crowd.size.valueType", "periodic" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.643513" },
            { "enemy.range.radius.periodic.amplitude.value", "0.239239" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.433568" },
            { "enemy.range.radius.periodic.period.value", "5384.457520" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.225515" },
            { "enemy.range.radius.periodic.tickShift.value", "8864.593750" },
            { "enemy.range.radius.value.mobJitterScale", "-0.964279" },
            { "enemy.range.radius.value.value", "1187.249878" },
            { "enemy.range.radius.valueType", "periodic" },
            { "enemy.range.type.check", "never" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "-0.120304" },
            { "enemy.weight.periodic.amplitude.value", "-0.674164" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.147880" },
            { "enemy.weight.periodic.period.value", "3030.774170" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.418172" },
            { "enemy.weight.periodic.tickShift.value", "2754.518066" },
            { "enemy.weight.value.mobJitterScale", "0.495560" },
            { "enemy.weight.value.value", "0.570646" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.336434" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.616073" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "0.004316" },
            { "enemyBase.range.radius.periodic.period.value", "6320.368652" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.307523" },
            { "enemyBase.range.radius.periodic.tickShift.value", "8326.301758" },
            { "enemyBase.range.radius.value.mobJitterScale", "-0.309541" },
            { "enemyBase.range.radius.value.value", "1692.562744" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "-0.789261" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.278390" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.113242" },
            { "enemyBase.crowd.size.periodic.period.value", "4456.736328" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.155647" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "819.691101" },
            { "enemyBase.crowd.size.value.mobJitterScale", "0.890448" },
            { "enemyBase.crowd.size.value.value", "3.767771" },
            { "enemyBase.crowd.size.valueType", "constant" },
            { "enemyBase.crowd.type.check", "never" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.335143" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.099600" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.667080" },
            { "enemyBase.range.radius.periodic.period.value", "8413.723633" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.106413" },
            { "enemyBase.range.radius.periodic.tickShift.value", "-1.000000" },
            { "enemyBase.range.radius.value.mobJitterScale", "0.725988" },
            { "enemyBase.range.radius.value.value", "596.981567" },
            { "enemyBase.range.radius.valueType", "periodic" },
            { "enemyBase.range.type.check", "never" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "-0.115270" },
            { "enemyBase.weight.periodic.amplitude.value", "-0.507521" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "0.367729" },
            { "enemyBase.weight.periodic.period.value", "4402.262695" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "-0.803203" },
            { "enemyBase.weight.periodic.tickShift.value", "850.273560" },
            { "enemyBase.weight.value.mobJitterScale", "-0.045299" },
            { "enemyBase.weight.value.value", "-1.313796" },
            { "enemyBase.weight.valueType", "constant" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "147.974152" },
            { "evadeStrictDistance", "59.282166" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fleetLocus.circularPeriod", "8169.303223" },
            { "fleetLocus.circularWeight", "1.123024" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.018064" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.510360" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "fleetLocus.force.range.radius.periodic.period.value", "6929.174805" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.295397" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "5405.960938" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "-0.542067" },
            { "fleetLocus.force.range.radius.value.value", "1741.319824" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.552794" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "0.566449" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "0.215368" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "7589.274414" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.534149" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "6682.489258" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "-1.000000" },
            { "fleetLocus.force.crowd.size.value.value", "7.021504" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "never" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.744871" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "-0.330467" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "0.550250" },
            { "fleetLocus.force.range.radius.periodic.period.value", "6254.499512" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.360480" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "249.562729" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.191180" },
            { "fleetLocus.force.range.radius.value.value", "1531.966553" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "0.067932" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.859751" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "0.903232" },
            { "fleetLocus.force.weight.periodic.period.value", "4965.840820" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.489218" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "3464.338379" },
            { "fleetLocus.force.weight.value.mobJitterScale", "-0.056222" },
            { "fleetLocus.force.weight.value.value", "-7.408298" },
            { "fleetLocus.force.weight.valueType", "periodic" },
            { "fleetLocus.linearWeight", "0.817746" },
            { "fleetLocus.linearXPeriod", "613.892395" },
            { "fleetLocus.linearYPeriod", "3889.190674" },
            { "fleetLocus.randomPeriod", "6167.006836" },
            { "fleetLocus.randomWeight", "1.954951" },
            { "fleetLocus.useScaled", "TRUE" },
            { "fleetLocus.useScaledLocus", "TRUE" },
            { "gatherAbandonStale", "FALSE" },
            { "gatherRange", "110.597687" },
            { "guardRange", "109.514572" },
            { "mobLocus.circularPeriod.mobJitterScale", "0.050878" },
            { "mobLocus.circularPeriod.value", "2393.599854" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "-0.297951" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "0.550027" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "0.758695" },
            { "mobLocus.circularWeight.periodic.period.value", "714.238342" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "-0.180130" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "9414.339844" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.719698" },
            { "mobLocus.circularWeight.value.value", "-2.020376" },
            { "mobLocus.circularWeight.valueType", "periodic" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.780872" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "-0.530915" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "0.791142" },
            { "mobLocus.force.range.radius.periodic.period.value", "7021.401855" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.997706" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "4838.767578" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.965789" },
            { "mobLocus.force.range.radius.value.value", "801.931396" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.029166" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-0.410693" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.918282" },
            { "mobLocus.force.crowd.size.periodic.period.value", "8008.793457" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.602070" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "4823.793945" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.819740" },
            { "mobLocus.force.crowd.size.value.value", "10.828095" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowd.type.check", "never" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.264388" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.996327" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.135688" },
            { "mobLocus.force.range.radius.periodic.period.value", "4152.555664" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.035690" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "4395.645020" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.067092" },
            { "mobLocus.force.range.radius.value.value", "26.127590" },
            { "mobLocus.force.range.radius.valueType", "periodic" },
            { "mobLocus.force.range.type.check", "never" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "0.963901" },
            { "mobLocus.force.weight.periodic.amplitude.value", "0.945270" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.465821" },
            { "mobLocus.force.weight.periodic.period.value", "1154.283691" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.986619" },
            { "mobLocus.force.weight.periodic.tickShift.value", "5798.237305" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.518762" },
            { "mobLocus.force.weight.value.value", "8.187295" },
            { "mobLocus.force.weight.valueType", "periodic" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "-0.356338" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "-0.688575" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.784860" },
            { "mobLocus.linearWeight.periodic.period.value", "9000.000000" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "0.211532" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "2989.916016" },
            { "mobLocus.linearWeight.value.mobJitterScale", "0.942406" },
            { "mobLocus.linearWeight.value.value", "4.669368" },
            { "mobLocus.linearWeight.valueType", "periodic" },
            { "mobLocus.linearXPeriod.mobJitterScale", "-0.246770" },
            { "mobLocus.linearXPeriod.value", "4464.055664" },
            { "mobLocus.linearYPeriod.mobJitterScale", "-0.722319" },
            { "mobLocus.linearYPeriod.value", "3697.949219" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "-0.719303" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.494496" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "-0.768334" },
            { "mobLocus.proximityRadius.periodic.period.value", "2212.907959" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "-0.121491" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "1246.053589" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "0.852295" },
            { "mobLocus.proximityRadius.value.value", "-1.000000" },
            { "mobLocus.proximityRadius.valueType", "periodic" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.437693" },
            { "mobLocus.randomPeriod.value", "1892.151001" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "-0.612294" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "-0.072323" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "0.278113" },
            { "mobLocus.randomWeight.periodic.period.value", "10000.000000" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-0.209524" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "6570.347168" },
            { "mobLocus.randomWeight.value.mobJitterScale", "-0.834555" },
            { "mobLocus.randomWeight.value.value", "-1.566723" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "FALSE" },
            { "mobLocus.useScaled", "FALSE" },
            { "mobLocus.useScaledLocus", "TRUE" },
            { "nearBaseRadius", "344.840485" },
            { "randomIdle.forceOn", "TRUE" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "28.837776" },
            { "sensorGrid.staleFighterTime", "11.049438" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "separate.range.radius.periodic.amplitude.value", "0.750069" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.634938" },
            { "separate.range.radius.periodic.period.value", "2186.090820" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "0.080305" },
            { "separate.range.radius.periodic.tickShift.value", "1221.492310" },
            { "separate.range.radius.value.mobJitterScale", "0.218305" },
            { "separate.range.radius.value.value", "704.197815" },
            { "separate.range.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "-0.923307" },
            { "separate.crowd.size.periodic.amplitude.value", "0.167532" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "-0.502371" },
            { "separate.crowd.size.periodic.period.value", "1920.691528" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.016265" },
            { "separate.crowd.size.periodic.tickShift.value", "568.556396" },
            { "separate.crowd.size.value.mobJitterScale", "0.285721" },
            { "separate.crowd.size.value.value", "4.006861" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowd.type.check", "never" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.682200" },
            { "separate.range.radius.periodic.amplitude.value", "-1.000000" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.788658" },
            { "separate.range.radius.periodic.period.value", "6011.351074" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.548045" },
            { "separate.range.radius.periodic.tickShift.value", "4171.840332" },
            { "separate.range.radius.value.mobJitterScale", "0.788270" },
            { "separate.range.radius.value.value", "1690.626465" },
            { "separate.range.radius.valueType", "constant" },
            { "separate.range.type.check", "never" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.437641" },
            { "separate.weight.periodic.amplitude.value", "0.277970" },
            { "separate.weight.periodic.period.mobJitterScale", "-0.784862" },
            { "separate.weight.periodic.period.value", "10000.000000" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "0.694047" },
            { "separate.weight.periodic.tickShift.value", "10000.000000" },
            { "separate.weight.value.mobJitterScale", "1.000000" },
            { "separate.weight.value.value", "-3.706197" },
            { "separate.weight.valueType", "constant" },
            { "startingMaxRadius", "1810.858765" },
            { "startingMinRadius", "674.267029" },
        };

        BundleConfigValue configs3[] = {
            { "align.range.radius.periodic.amplitude.value", "-0.051140" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.331735" },
            { "align.range.radius.periodic.period.value", "4188.526855" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.977962" },
            { "align.range.radius.periodic.tickShift.value", "3184.694824" },
            { "align.range.radius.value.mobJitterScale", "0.770306" },
            { "align.range.radius.value.value", "85.648621" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.439141" },
            { "align.crowd.size.periodic.amplitude.value", "-0.681810" },
            { "align.crowd.size.periodic.period.mobJitterScale", "0.126401" },
            { "align.crowd.type.check", "never" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.160314" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.399551" },
            { "align.range.radius.periodic.period.value", "6872.918945" },
            { "align.range.radius.value.value", "76.856781" },
            { "align.weight.periodic.amplitude.mobJitterScale", "-0.209113" },
            { "align.weight.periodic.period.mobJitterScale", "0.430720" },
            { "align.weight.periodic.tickShift.mobJitterScale", "0.910730" },
            { "align.weight.value.mobJitterScale", "0.723799" },
            { "align.weight.value.value", "0.827045" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "181.827621" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.560170" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.500959" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.560655" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "-0.032108" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "9644.429688" },
            { "attackSeparate.range.radius.value.value", "171.537613" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.185109" },
            { "attackSeparate.crowd.size.periodic.period.value", "4507.496582" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "0.945825" },
            { "attackSeparate.crowd.size.value.value", "3.265365" },
            { "attackSeparate.crowd.type.check", "never" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.203000" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.035319" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.081711" },
            { "attackSeparate.range.radius.periodic.period.value", "8198.589844" },
            { "attackSeparate.range.radius.value.value", "229.267609" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.710210" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.540785" },
            { "attackSeparate.weight.periodic.period.value", "4880.460449" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "0.804290" },
            { "attackSeparate.weight.value.value", "3.091066" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.675374" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.411808" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.502247" },
            { "base.range.radius.value.mobJitterScale", "0.306025" },
            { "base.range.radius.value.value", "686.184509" },
            { "base.range.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "0.854254" },
            { "base.crowd.size.periodic.amplitude.value", "0.696899" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.579538" },
            { "base.crowd.size.periodic.tickShift.value", "2133.861816" },
            { "base.crowd.size.value.mobJitterScale", "0.124148" },
            { "base.crowd.size.value.value", "16.296532" },
            { "base.crowd.size.valueType", "constant" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.757746" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.944114" },
            { "base.range.radius.periodic.period.value", "5422.245117" },
            { "base.range.radius.periodic.tickShift.value", "9813.756836" },
            { "base.range.radius.value.mobJitterScale", "0.106048" },
            { "base.range.radius.value.value", "1405.699829" },
            { "base.weight.periodic.amplitude.value", "-0.619102" },
            { "base.weight.periodic.period.mobJitterScale", "-0.754014" },
            { "base.weight.periodic.period.value", "7403.763184" },
            { "base.weight.periodic.tickShift.mobJitterScale", "0.768387" },
            { "base.weight.periodic.tickShift.value", "5993.753418" },
            { "base.weight.value.value", "7.057205" },
            { "baseDefense.range.radius.periodic.amplitude.value", "-0.881295" },
            { "baseDefense.range.radius.periodic.period.value", "998.405029" },
            { "baseDefense.range.radius.periodic.tickShift.value", "3993.637695" },
            { "baseDefense.range.radius.value.mobJitterScale", "-0.581557" },
            { "baseDefense.range.radius.value.value", "515.065125" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "0.348966" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "6131.960938" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.134185" },
            { "baseDefense.crowd.size.value.value", "11.412745" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.738953" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "baseDefense.range.radius.periodic.tickShift.value", "6603.688965" },
            { "baseDefense.range.radius.value.value", "1792.569092" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-0.875484" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.606333" },
            { "baseDefense.weight.periodic.tickShift.value", "984.566772" },
            { "baseDefense.weight.value.mobJitterScale", "0.805653" },
            { "baseDefense.weight.value.value", "-8.000670" },
            { "baseDefenseRadius", "25.127081" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.391096" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.033322" },
            { "center.range.radius.periodic.tickShift.value", "4016.315674" },
            { "center.range.radius.value.mobJitterScale", "-0.527258" },
            { "center.range.radius.value.value", "766.094421" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.348274" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "-0.503560" },
            { "center.crowd.size.periodic.tickShift.value", "356.286469" },
            { "center.crowd.size.value.mobJitterScale", "0.271405" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "0.224659" },
            { "center.range.radius.periodic.amplitude.value", "0.776280" },
            { "center.range.radius.periodic.period.value", "1152.206299" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.369279" },
            { "center.range.radius.value.mobJitterScale", "-0.091729" },
            { "center.range.radius.value.value", "618.021545" },
            { "center.range.radius.valueType", "constant" },
            { "center.weight.periodic.amplitude.mobJitterScale", "0.164201" },
            { "center.weight.periodic.period.mobJitterScale", "-0.221119" },
            { "center.weight.periodic.tickShift.mobJitterScale", "-0.160915" },
            { "center.weight.periodic.tickShift.value", "7330.156250" },
            { "center.weight.value.mobJitterScale", "-0.571012" },
            { "center.weight.value.value", "-9.242901" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.030119" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.125754" },
            { "cohere.range.radius.value.mobJitterScale", "-0.710434" },
            { "cohere.range.radius.value.value", "221.215759" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "-0.578827" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.748859" },
            { "cohere.crowd.size.periodic.period.value", "7638.923340" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.723084" },
            { "cohere.crowd.size.periodic.tickShift.value", "5434.939453" },
            { "cohere.crowd.size.value.value", "7.000432" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.296641" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.894092" },
            { "cohere.range.radius.value.mobJitterScale", "0.392619" },
            { "cohere.range.radius.value.value", "1946.445679" },
            { "cohere.weight.periodic.period.mobJitterScale", "-0.293398" },
            { "cohere.weight.periodic.period.value", "3857.262451" },
            { "cohere.weight.value.mobJitterScale", "0.196416" },
            { "cohere.weight.value.value", "-2.525773" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.942518" },
            { "cores.range.radius.periodic.amplitude.value", "-0.980032" },
            { "cores.range.radius.periodic.tickShift.value", "377.282043" },
            { "cores.range.radius.value.mobJitterScale", "0.707058" },
            { "cores.crowd.size.periodic.amplitude.value", "0.418095" },
            { "cores.crowd.size.periodic.period.value", "2481.943848" },
            { "cores.crowd.size.periodic.tickShift.value", "10000.000000" },
            { "cores.crowd.size.value.value", "15.346873" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "-0.894492" },
            { "cores.range.radius.periodic.amplitude.value", "-0.310929" },
            { "cores.range.radius.periodic.period.value", "5571.164062" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.553094" },
            { "cores.range.radius.periodic.tickShift.value", "5228.984863" },
            { "cores.range.radius.value.mobJitterScale", "0.578323" },
            { "cores.range.radius.valueType", "constant" },
            { "cores.weight.periodic.amplitude.value", "0.340708" },
            { "cores.weight.periodic.period.value", "8589.230469" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.981617" },
            { "cores.weight.periodic.tickShift.value", "4228.031250" },
            { "cores.weight.value.value", "4.892436" },
            { "corners.range.radius.periodic.amplitude.value", "0.719809" },
            { "corners.range.radius.periodic.period.mobJitterScale", "0.953544" },
            { "corners.range.radius.periodic.period.value", "6423.458496" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "0.990934" },
            { "corners.range.radius.periodic.tickShift.value", "4013.902832" },
            { "corners.range.radius.value.value", "1816.927856" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "0.885372" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "-0.849573" },
            { "corners.crowd.size.periodic.period.value", "603.696777" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "-0.753211" },
            { "corners.crowd.size.value.mobJitterScale", "0.380151" },
            { "corners.crowd.size.value.value", "4.944045" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.343873" },
            { "corners.range.radius.periodic.amplitude.value", "-0.277044" },
            { "corners.range.radius.periodic.period.value", "1110.108154" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "0.041403" },
            { "corners.range.radius.periodic.tickShift.value", "5325.076660" },
            { "corners.range.radius.value.mobJitterScale", "-1.000000" },
            { "corners.range.radius.value.value", "746.597595" },
            { "corners.weight.periodic.amplitude.value", "-0.779431" },
            { "corners.weight.periodic.period.mobJitterScale", "-0.159484" },
            { "corners.weight.periodic.period.value", "10000.000000" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "-0.764566" },
            { "corners.weight.value.mobJitterScale", "0.266177" },
            { "corners.weight.valueType", "periodic" },
            { "curHeadingWeight.periodic.amplitude.value", "0.162215" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "-0.538298" },
            { "curHeadingWeight.periodic.period.value", "580.295410" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "-0.831925" },
            { "curHeadingWeight.value.value", "8.921747" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "0.574534" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.387827" },
            { "edges.range.radius.periodic.tickShift.value", "2495.512939" },
            { "edges.range.radius.value.mobJitterScale", "-0.534629" },
            { "edges.range.radius.value.value", "1349.925659" },
            { "edges.crowd.size.periodic.amplitude.value", "0.512276" },
            { "edges.crowd.size.periodic.period.value", "5108.238770" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.053099" },
            { "edges.crowd.size.value.mobJitterScale", "0.001837" },
            { "edges.crowd.size.value.value", "1.599778" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.range.radius.periodic.amplitude.value", "-0.320596" },
            { "edges.range.radius.periodic.period.mobJitterScale", "0.307652" },
            { "edges.range.radius.periodic.period.value", "9804.218750" },
            { "edges.range.radius.periodic.tickShift.value", "715.696655" },
            { "edges.range.radius.value.mobJitterScale", "0.090252" },
            { "edges.range.radius.value.value", "916.723999" },
            { "edges.weight.periodic.amplitude.value", "-0.164524" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.538755" },
            { "edges.weight.periodic.period.value", "1247.841919" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "-0.578042" },
            { "edges.weight.value.mobJitterScale", "-0.265026" },
            { "edges.weight.value.value", "7.174948" },
            { "edges.weight.valueType", "periodic" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "-0.028456" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.169839" },
            { "enemy.range.radius.periodic.period.value", "6811.915039" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "0.374059" },
            { "enemy.range.radius.periodic.tickShift.value", "7704.856445" },
            { "enemy.range.radius.value.mobJitterScale", "-0.157291" },
            { "enemy.range.radius.value.value", "1692.328491" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.129000" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "-0.390787" },
            { "enemy.crowd.size.periodic.period.value", "544.119995" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "0.459140" },
            { "enemy.crowd.size.value.mobJitterScale", "0.190205" },
            { "enemy.crowd.size.value.value", "3.786107" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.442836" },
            { "enemy.range.radius.periodic.amplitude.value", "0.971019" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "-0.162324" },
            { "enemy.range.radius.periodic.period.value", "2294.790283" },
            { "enemy.range.radius.value.value", "1262.498047" },
            { "enemy.range.radius.valueType", "constant" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "-0.126755" },
            { "enemy.weight.periodic.amplitude.value", "0.949832" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.863818" },
            { "enemy.weight.periodic.period.value", "8216.765625" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "-0.019070" },
            { "enemy.weight.periodic.tickShift.value", "5589.622559" },
            { "enemy.weight.value.mobJitterScale", "-0.183337" },
            { "enemy.weight.value.value", "-9.282289" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.869706" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "0.464216" },
            { "enemyBase.range.radius.periodic.period.value", "5537.635254" },
            { "enemyBase.range.radius.value.value", "220.179703" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "0.632945" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.629738" },
            { "enemyBase.crowd.size.value.value", "2.778095" },
            { "enemyBase.crowd.type.check", "never" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.146693" },
            { "enemyBase.range.radius.periodic.period.value", "1956.951904" },
            { "enemyBase.range.radius.value.value", "435.686005" },
            { "enemyBase.weight.periodic.amplitude.value", "0.836371" },
            { "enemyBase.weight.periodic.period.value", "9834.474609" },
            { "enemyBase.weight.periodic.tickShift.value", "408.331116" },
            { "enemyBase.weight.value.value", "-6.865941" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "368.761627" },
            { "evadeStrictDistance", "364.574127" },
            { "fleetLocus.circularPeriod", "850.855652" },
            { "fleetLocus.circularWeight", "1.548693" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.736081" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "0.467039" },
            { "fleetLocus.force.range.radius.periodic.period.value", "8195.372070" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.795574" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "3559.967285" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "-0.682656" },
            { "fleetLocus.force.range.radius.value.value", "1163.618286" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.512020" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.905756" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "3938.054443" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.059095" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "6545.076660" },
            { "fleetLocus.force.crowd.size.value.value", "3.513504" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.292364" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.541241" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "2335.849121" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "-0.327379" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "0.230153" },
            { "fleetLocus.force.weight.periodic.period.value", "7991.845703" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.965561" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "4928.279785" },
            { "fleetLocus.force.weight.value.value", "-1.029253" },
            { "fleetLocus.linearYPeriod", "5408.588379" },
            { "fleetLocus.randomPeriod", "3801.106201" },
            { "gatherAbandonStale", "FALSE" },
            { "gatherRange", "55.696205" },
            { "guardRange", "164.575317" },
            { "mobLocus.circularPeriod.value", "2265.280029" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "0.048998" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "0.603349" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "0.549604" },
            { "mobLocus.circularWeight.periodic.period.value", "4821.490234" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "-0.929719" },
            { "mobLocus.circularWeight.value.value", "-8.179069" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.812566" },
            { "mobLocus.force.range.radius.periodic.period.value", "5997.909668" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.592159" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "-1.000000" },
            { "mobLocus.force.range.radius.value.value", "1786.280518" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.354494" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.365438" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "5586.841309" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.307383" },
            { "mobLocus.force.crowd.size.valueType", "constant" },
            { "mobLocus.force.crowd.type.check", "never" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.746614" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.557284" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "-0.595792" },
            { "mobLocus.force.range.radius.value.value", "123.061218" },
            { "mobLocus.force.weight.periodic.amplitude.value", "0.238803" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "-0.812091" },
            { "mobLocus.force.weight.periodic.period.value", "8111.791992" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.943180" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.976057" },
            { "mobLocus.force.weight.value.value", "4.029823" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.051991" },
            { "mobLocus.linearWeight.periodic.period.value", "8302.616211" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "-0.302894" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "5847.866211" },
            { "mobLocus.linearWeight.value.value", "-5.909348" },
            { "mobLocus.linearXPeriod.mobJitterScale", "0.703716" },
            { "mobLocus.linearYPeriod.mobJitterScale", "-0.945227" },
            { "mobLocus.linearYPeriod.value", "7445.639160" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "-0.762719" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "0.223341" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "0.301430" },
            { "mobLocus.proximityRadius.periodic.period.value", "6634.515625" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "0.052768" },
            { "mobLocus.proximityRadius.value.value", "1712.720581" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.444012" },
            { "mobLocus.randomPeriod.value", "2439.498779" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "-0.787409" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "-0.730384" },
            { "mobLocus.randomWeight.periodic.period.value", "1047.329712" },
            { "mobLocus.randomWeight.value.value", "-2.671097" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "rotateStartingAngle", "TRUE" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.978855" },
            { "separate.range.radius.periodic.amplitude.value", "0.604296" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.081784" },
            { "separate.range.radius.periodic.period.value", "6466.973145" },
            { "separate.range.radius.periodic.tickShift.value", "7154.783203" },
            { "separate.range.radius.value.value", "713.626648" },
            { "separate.crowd.size.periodic.amplitude.value", "-0.361206" },
            { "separate.crowd.size.periodic.period.value", "7077.867676" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.190897" },
            { "separate.crowd.size.value.mobJitterScale", "0.742453" },
            { "separate.crowd.size.value.value", "13.192937" },
            { "separate.range.radius.periodic.amplitude.value", "0.193831" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "0.277915" },
            { "separate.range.radius.periodic.tickShift.value", "6534.459473" },
            { "separate.range.radius.value.mobJitterScale", "-0.716639" },
            { "separate.range.radius.value.value", "1744.508911" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.645268" },
            { "separate.weight.periodic.amplitude.value", "0.164166" },
            { "separate.weight.periodic.period.mobJitterScale", "-0.902149" },
            { "separate.weight.periodic.tickShift.value", "9153.715820" },
            { "separate.weight.value.value", "7.668723" },
            { "startingMaxRadius", "1729.734253" },
            { "startingMinRadius", "673.257263" },
        };

        BundleConfigValue configs4[] = {
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.997637" },
            { "align.range.radius.periodic.amplitude.value", "-0.580417" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.416009" },
            { "align.range.radius.periodic.period.value", "4539.239258" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.664738" },
            { "align.range.radius.periodic.tickShift.value", "1402.517334" },
            { "align.range.radius.value.mobJitterScale", "-0.192877" },
            { "align.range.radius.value.value", "-1.000000" },
            { "align.range.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "-0.760518" },
            { "align.crowd.size.periodic.amplitude.value", "-0.721088" },
            { "align.crowd.size.periodic.period.mobJitterScale", "0.596016" },
            { "align.crowd.size.periodic.period.value", "5568.675293" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.990000" },
            { "align.crowd.size.periodic.tickShift.value", "3191.691162" },
            { "align.crowd.size.value.mobJitterScale", "0.261138" },
            { "align.crowd.size.value.value", "14.545824" },
            { "align.crowd.size.valueType", "constant" },
            { "align.crowd.type.check", "never" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "align.range.radius.periodic.amplitude.value", "-1.000000" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.663143" },
            { "align.range.radius.periodic.period.value", "7195.858887" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.293900" },
            { "align.range.radius.periodic.tickShift.value", "4899.379395" },
            { "align.range.radius.value.mobJitterScale", "0.825958" },
            { "align.range.radius.value.value", "1611.409424" },
            { "align.range.radius.valueType", "periodic" },
            { "align.range.type.check", "never" },
            { "align.weight.periodic.amplitude.mobJitterScale", "0.837901" },
            { "align.weight.periodic.amplitude.value", "0.305668" },
            { "align.weight.periodic.period.mobJitterScale", "-0.599944" },
            { "align.weight.periodic.period.value", "5701.847168" },
            { "align.weight.periodic.tickShift.mobJitterScale", "0.875103" },
            { "align.weight.periodic.tickShift.value", "9712.392578" },
            { "align.weight.value.mobJitterScale", "-0.156086" },
            { "align.weight.value.value", "6.890658" },
            { "align.weight.valueType", "periodic" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "141.527267" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.160626" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.729000" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.807596" },
            { "attackSeparate.range.radius.periodic.period.value", "7794.914062" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.412584" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "2446.844727" },
            { "attackSeparate.range.radius.value.mobJitterScale", "0.359301" },
            { "attackSeparate.range.radius.value.value", "1107.389893" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.397179" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "-0.240794" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.759256" },
            { "attackSeparate.crowd.size.periodic.period.value", "7310.659668" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "4993.658691" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-1.000000" },
            { "attackSeparate.crowd.size.value.value", "9.217925" },
            { "attackSeparate.crowd.size.valueType", "periodic" },
            { "attackSeparate.crowd.type.check", "never" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.815203" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.235451" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.461347" },
            { "attackSeparate.range.radius.periodic.period.value", "7895.465820" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "-0.061523" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "9595.180664" },
            { "attackSeparate.range.radius.value.mobJitterScale", "0.358411" },
            { "attackSeparate.range.radius.value.value", "6.974665" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.range.type.check", "never" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.092663" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.464557" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "0.742404" },
            { "attackSeparate.weight.periodic.period.value", "6052.318359" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "attackSeparate.weight.periodic.tickShift.value", "-1.000000" },
            { "attackSeparate.weight.value.mobJitterScale", "-0.916499" },
            { "attackSeparate.weight.value.value", "-6.942165" },
            { "attackSeparate.weight.valueType", "periodic" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.629757" },
            { "base.range.radius.periodic.amplitude.value", "1.000000" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.954232" },
            { "base.range.radius.periodic.period.value", "1434.848267" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.872867" },
            { "base.range.radius.periodic.tickShift.value", "2878.810547" },
            { "base.range.radius.value.mobJitterScale", "0.350683" },
            { "base.range.radius.value.value", "544.842163" },
            { "base.range.radius.valueType", "periodic" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.760530" },
            { "base.crowd.size.periodic.amplitude.value", "-0.617358" },
            { "base.crowd.size.periodic.period.mobJitterScale", "0.500071" },
            { "base.crowd.size.periodic.period.value", "7564.628418" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.365768" },
            { "base.crowd.size.periodic.tickShift.value", "1731.283569" },
            { "base.crowd.size.value.mobJitterScale", "0.567610" },
            { "base.crowd.size.value.value", "10.136998" },
            { "base.crowd.size.valueType", "periodic" },
            { "base.crowd.type.check", "never" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.503678" },
            { "base.range.radius.periodic.amplitude.value", "0.357514" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.303256" },
            { "base.range.radius.periodic.period.value", "1714.841187" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.446971" },
            { "base.range.radius.periodic.tickShift.value", "7052.154297" },
            { "base.range.radius.value.mobJitterScale", "0.306412" },
            { "base.range.radius.value.value", "826.290833" },
            { "base.range.radius.valueType", "constant" },
            { "base.range.type.check", "never" },
            { "base.weight.periodic.amplitude.mobJitterScale", "0.195740" },
            { "base.weight.periodic.amplitude.value", "0.241564" },
            { "base.weight.periodic.period.mobJitterScale", "0.423984" },
            { "base.weight.periodic.period.value", "4275.365723" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.344272" },
            { "base.weight.periodic.tickShift.value", "6403.570801" },
            { "base.weight.value.mobJitterScale", "-0.900000" },
            { "base.weight.value.value", "-0.552769" },
            { "base.weight.valueType", "constant" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.006350" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.186222" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "-0.055209" },
            { "baseDefense.range.radius.periodic.period.value", "3771.450928" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "0.399684" },
            { "baseDefense.range.radius.periodic.tickShift.value", "1177.760742" },
            { "baseDefense.range.radius.value.mobJitterScale", "-1.000000" },
            { "baseDefense.range.radius.value.value", "1064.811401" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "-0.063311" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "0.248441" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "0.288933" },
            { "baseDefense.crowd.size.periodic.period.value", "664.790039" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "0.127526" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "5024.023926" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.313322" },
            { "baseDefense.crowd.size.value.value", "6.976077" },
            { "baseDefense.crowd.size.valueType", "periodic" },
            { "baseDefense.crowd.type.check", "never" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.015727" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "-0.568743" },
            { "baseDefense.range.radius.periodic.period.value", "2136.907227" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "0.459880" },
            { "baseDefense.range.radius.periodic.tickShift.value", "4744.493652" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.010407" },
            { "baseDefense.range.radius.value.value", "1813.240723" },
            { "baseDefense.range.radius.valueType", "periodic" },
            { "baseDefense.range.type.check", "never" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "0.585415" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.877266" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "0.609404" },
            { "baseDefense.weight.periodic.period.value", "5497.967773" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "baseDefense.weight.periodic.tickShift.value", "7490.540527" },
            { "baseDefense.weight.value.mobJitterScale", "-0.807073" },
            { "baseDefense.weight.value.value", "5.179328" },
            { "baseDefense.weight.valueType", "periodic" },
            { "baseDefenseRadius", "249.662384" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.129981" },
            { "center.range.radius.periodic.amplitude.value", "-0.808502" },
            { "center.range.radius.periodic.period.mobJitterScale", "0.522967" },
            { "center.range.radius.periodic.period.value", "4239.490723" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.221667" },
            { "center.range.radius.periodic.tickShift.value", "3245.848145" },
            { "center.range.radius.value.mobJitterScale", "-0.880619" },
            { "center.range.radius.value.value", "1296.293945" },
            { "center.range.radius.valueType", "periodic" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.643359" },
            { "center.crowd.size.periodic.amplitude.value", "0.545312" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.817902" },
            { "center.crowd.size.periodic.period.value", "4173.515625" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "0.562874" },
            { "center.crowd.size.periodic.tickShift.value", "2118.594727" },
            { "center.crowd.size.value.mobJitterScale", "-0.254054" },
            { "center.crowd.size.value.value", "-1.000000" },
            { "center.crowd.size.valueType", "constant" },
            { "center.crowd.type.check", "never" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.257824" },
            { "center.range.radius.periodic.amplitude.value", "-0.778535" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.261245" },
            { "center.range.radius.periodic.period.value", "1270.336792" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.225704" },
            { "center.range.radius.periodic.tickShift.value", "5248.995117" },
            { "center.range.radius.value.mobJitterScale", "0.178743" },
            { "center.range.radius.value.value", "1376.573120" },
            { "center.range.radius.valueType", "periodic" },
            { "center.range.type.check", "never" },
            { "center.weight.periodic.amplitude.mobJitterScale", "-0.780084" },
            { "center.weight.periodic.amplitude.value", "-0.055858" },
            { "center.weight.periodic.period.mobJitterScale", "0.879264" },
            { "center.weight.periodic.period.value", "3026.286133" },
            { "center.weight.periodic.tickShift.mobJitterScale", "-0.689098" },
            { "center.weight.periodic.tickShift.value", "9663.837891" },
            { "center.weight.value.mobJitterScale", "0.081387" },
            { "center.weight.value.value", "-0.341581" },
            { "center.weight.valueType", "periodic" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.741092" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.347129" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.413831" },
            { "cohere.range.radius.periodic.period.value", "6253.756836" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "0.539022" },
            { "cohere.range.radius.periodic.tickShift.value", "9135.519531" },
            { "cohere.range.radius.value.mobJitterScale", "-0.855556" },
            { "cohere.range.radius.value.value", "353.585327" },
            { "cohere.range.radius.valueType", "constant" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.389762" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.936647" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "-0.794015" },
            { "cohere.crowd.size.periodic.period.value", "1550.949463" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.264275" },
            { "cohere.crowd.size.periodic.tickShift.value", "196.567032" },
            { "cohere.crowd.size.value.mobJitterScale", "0.900000" },
            { "cohere.crowd.size.value.value", "7.042645" },
            { "cohere.crowd.size.valueType", "periodic" },
            { "cohere.crowd.type.check", "never" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.510163" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.344642" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.903820" },
            { "cohere.range.radius.periodic.period.value", "6761.200195" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "0.978119" },
            { "cohere.range.radius.periodic.tickShift.value", "5388.253418" },
            { "cohere.range.radius.value.mobJitterScale", "0.627164" },
            { "cohere.range.radius.value.value", "53.364258" },
            { "cohere.range.radius.valueType", "constant" },
            { "cohere.range.type.check", "never" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "0.380661" },
            { "cohere.weight.periodic.amplitude.value", "-0.967841" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.396756" },
            { "cohere.weight.periodic.period.value", "10000.000000" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.577438" },
            { "cohere.weight.periodic.tickShift.value", "1060.135254" },
            { "cohere.weight.value.mobJitterScale", "-0.540023" },
            { "cohere.weight.value.value", "-4.392778" },
            { "cohere.weight.valueType", "constant" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.054492" },
            { "cores.range.radius.periodic.amplitude.value", "-0.058022" },
            { "cores.range.radius.periodic.period.mobJitterScale", "-0.665730" },
            { "cores.range.radius.periodic.period.value", "1379.847412" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "-0.088462" },
            { "cores.range.radius.periodic.tickShift.value", "3113.915283" },
            { "cores.range.radius.value.mobJitterScale", "0.709337" },
            { "cores.range.radius.value.value", "572.806946" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "-0.230521" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.407307" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-0.318837" },
            { "cores.crowd.size.periodic.period.value", "1108.362915" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "-0.951218" },
            { "cores.crowd.size.periodic.tickShift.value", "1688.293213" },
            { "cores.crowd.size.value.mobJitterScale", "-0.103645" },
            { "cores.crowd.size.value.value", "-0.990000" },
            { "cores.crowd.size.valueType", "periodic" },
            { "cores.crowd.type.check", "never" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.396617" },
            { "cores.range.radius.periodic.amplitude.value", "0.017514" },
            { "cores.range.radius.periodic.period.mobJitterScale", "0.659562" },
            { "cores.range.radius.periodic.period.value", "9954.907227" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "cores.range.radius.periodic.tickShift.value", "5256.877441" },
            { "cores.range.radius.value.mobJitterScale", "0.668274" },
            { "cores.range.radius.value.value", "1785.109131" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.range.type.check", "never" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.203537" },
            { "cores.weight.periodic.amplitude.value", "0.547758" },
            { "cores.weight.periodic.period.mobJitterScale", "1.000000" },
            { "cores.weight.periodic.period.value", "1121.036743" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.448087" },
            { "cores.weight.periodic.tickShift.value", "6111.345703" },
            { "cores.weight.value.mobJitterScale", "0.576346" },
            { "cores.weight.value.value", "-8.884531" },
            { "cores.weight.valueType", "constant" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.974665" },
            { "corners.range.radius.periodic.amplitude.value", "0.567519" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-0.089344" },
            { "corners.range.radius.periodic.period.value", "2946.568848" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "0.567529" },
            { "corners.range.radius.periodic.tickShift.value", "4511.764648" },
            { "corners.range.radius.value.mobJitterScale", "-0.794892" },
            { "corners.range.radius.value.value", "50.181087" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "-0.641893" },
            { "corners.crowd.size.periodic.amplitude.value", "-0.195778" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "-0.413899" },
            { "corners.crowd.size.periodic.period.value", "8571.804688" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.223595" },
            { "corners.crowd.size.periodic.tickShift.value", "4534.453125" },
            { "corners.crowd.size.value.mobJitterScale", "-0.335554" },
            { "corners.crowd.size.value.value", "5.440167" },
            { "corners.crowd.size.valueType", "constant" },
            { "corners.crowd.type.check", "never" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.377146" },
            { "corners.range.radius.periodic.amplitude.value", "0.307509" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-0.790672" },
            { "corners.range.radius.periodic.period.value", "10000.000000" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "0.040308" },
            { "corners.range.radius.periodic.tickShift.value", "4148.615234" },
            { "corners.range.radius.value.mobJitterScale", "-0.129875" },
            { "corners.range.radius.value.value", "-0.900000" },
            { "corners.range.radius.valueType", "constant" },
            { "corners.range.type.check", "never" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "-0.093849" },
            { "corners.weight.periodic.amplitude.value", "-0.010627" },
            { "corners.weight.periodic.period.mobJitterScale", "-0.561346" },
            { "corners.weight.periodic.period.value", "-1.000000" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "0.900000" },
            { "corners.weight.periodic.tickShift.value", "5510.197266" },
            { "corners.weight.value.mobJitterScale", "-0.497885" },
            { "corners.weight.value.value", "-9.374656" },
            { "corners.weight.valueType", "periodic" },
            { "creditReserve", "169.037186" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.788548" },
            { "curHeadingWeight.periodic.amplitude.value", "0.485165" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "1.000000" },
            { "curHeadingWeight.periodic.period.value", "2778.397217" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "0.716105" },
            { "curHeadingWeight.periodic.tickShift.value", "6849.835449" },
            { "curHeadingWeight.value.mobJitterScale", "0.723601" },
            { "curHeadingWeight.value.value", "0.706990" },
            { "curHeadingWeight.valueType", "periodic" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "0.428622" },
            { "edges.range.radius.periodic.amplitude.value", "-0.478741" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.039347" },
            { "edges.range.radius.periodic.period.value", "-1.000000" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.610549" },
            { "edges.range.radius.periodic.tickShift.value", "6925.696289" },
            { "edges.range.radius.value.mobJitterScale", "0.704016" },
            { "edges.range.radius.value.value", "1486.155273" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.369981" },
            { "edges.crowd.size.periodic.amplitude.value", "0.579032" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "edges.crowd.size.periodic.period.value", "7911.691406" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "-0.073305" },
            { "edges.crowd.size.periodic.tickShift.value", "7164.106934" },
            { "edges.crowd.size.value.mobJitterScale", "0.303231" },
            { "edges.crowd.size.value.value", "20.000000" },
            { "edges.crowd.size.valueType", "periodic" },
            { "edges.crowd.type.check", "never" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "0.095815" },
            { "edges.range.radius.periodic.amplitude.value", "-0.524913" },
            { "edges.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "edges.range.radius.periodic.period.value", "2956.822510" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.873347" },
            { "edges.range.radius.periodic.tickShift.value", "8249.994141" },
            { "edges.range.radius.value.mobJitterScale", "0.124869" },
            { "edges.range.radius.value.value", "1027.559937" },
            { "edges.range.radius.valueType", "periodic" },
            { "edges.range.type.check", "never" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "0.086834" },
            { "edges.weight.periodic.amplitude.value", "0.069813" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.516740" },
            { "edges.weight.periodic.period.value", "5186.431641" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.307494" },
            { "edges.weight.periodic.tickShift.value", "6947.385742" },
            { "edges.weight.value.mobJitterScale", "-0.480400" },
            { "edges.weight.value.value", "1.973997" },
            { "edges.weight.valueType", "constant" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.318759" },
            { "enemy.range.radius.periodic.amplitude.value", "0.887125" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.424449" },
            { "enemy.range.radius.periodic.period.value", "5951.244141" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "enemy.range.radius.periodic.tickShift.value", "3979.839111" },
            { "enemy.range.radius.value.mobJitterScale", "0.714816" },
            { "enemy.range.radius.value.value", "1175.853882" },
            { "enemy.range.radius.valueType", "periodic" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "0.105315" },
            { "enemy.crowd.size.periodic.amplitude.value", "0.711270" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.209904" },
            { "enemy.crowd.size.periodic.period.value", "4162.410645" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "-0.297136" },
            { "enemy.crowd.size.periodic.tickShift.value", "2045.583984" },
            { "enemy.crowd.size.value.mobJitterScale", "0.244159" },
            { "enemy.crowd.size.value.value", "20.000000" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.363027" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.849631" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.151267" },
            { "enemy.range.radius.periodic.period.value", "8259.858398" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.251063" },
            { "enemy.range.radius.periodic.tickShift.value", "2326.809814" },
            { "enemy.range.radius.value.mobJitterScale", "1.000000" },
            { "enemy.range.radius.value.value", "675.515564" },
            { "enemy.range.radius.valueType", "periodic" },
            { "enemy.range.type.check", "never" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemy.weight.periodic.amplitude.value", "-0.415855" },
            { "enemy.weight.periodic.period.mobJitterScale", "-0.958802" },
            { "enemy.weight.periodic.period.value", "4443.390137" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "-0.767862" },
            { "enemy.weight.periodic.tickShift.value", "1665.511475" },
            { "enemy.weight.value.mobJitterScale", "0.302705" },
            { "enemy.weight.value.value", "0.182987" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.150879" },
            { "enemyBase.range.radius.periodic.amplitude.value", "0.459352" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.566621" },
            { "enemyBase.range.radius.periodic.period.value", "3987.979004" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "enemyBase.range.radius.periodic.tickShift.value", "4893.104004" },
            { "enemyBase.range.radius.value.mobJitterScale", "0.060784" },
            { "enemyBase.range.radius.value.value", "686.847290" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "0.342603" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.888685" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.354940" },
            { "enemyBase.crowd.size.periodic.period.value", "837.139221" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "-0.313340" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "9557.248047" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.603297" },
            { "enemyBase.crowd.size.value.value", "14.142944" },
            { "enemyBase.crowd.size.valueType", "periodic" },
            { "enemyBase.crowd.type.check", "never" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "0.734564" },
            { "enemyBase.range.radius.periodic.amplitude.value", "0.641733" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "0.491955" },
            { "enemyBase.range.radius.periodic.period.value", "3028.051758" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.037870" },
            { "enemyBase.range.radius.periodic.tickShift.value", "-0.891000" },
            { "enemyBase.range.radius.value.mobJitterScale", "0.438102" },
            { "enemyBase.range.radius.value.value", "-0.990000" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.range.type.check", "never" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemyBase.weight.periodic.amplitude.value", "0.199772" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "-0.455554" },
            { "enemyBase.weight.periodic.period.value", "5646.392090" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "-0.149552" },
            { "enemyBase.weight.periodic.tickShift.value", "5676.587891" },
            { "enemyBase.weight.value.mobJitterScale", "-1.000000" },
            { "enemyBase.weight.value.value", "-6.014412" },
            { "enemyBase.weight.valueType", "constant" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "393.371948" },
            { "evadeStrictDistance", "57.199680" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fleetLocus.circularPeriod", "2948.727783" },
            { "fleetLocus.circularWeight", "1.900000" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.054109" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "-0.722894" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "0.852969" },
            { "fleetLocus.force.range.radius.periodic.period.value", "5040.565430" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.194057" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "5826.963867" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.271841" },
            { "fleetLocus.force.range.radius.value.value", "1839.255859" },
            { "fleetLocus.force.range.radius.valueType", "constant" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.260413" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.890009" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.038311" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "10000.000000" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.706192" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "3292.306885" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "-0.105208" },
            { "fleetLocus.force.crowd.size.value.value", "7.386487" },
            { "fleetLocus.force.crowd.size.valueType", "constant" },
            { "fleetLocus.force.crowd.type.check", "never" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.737166" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "-0.517972" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.032016" },
            { "fleetLocus.force.range.radius.periodic.period.value", "4870.444336" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.097485" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "3433.859375" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.046068" },
            { "fleetLocus.force.range.radius.value.value", "329.599731" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.939289" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "0.114300" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-0.035231" },
            { "fleetLocus.force.weight.periodic.period.value", "8707.219727" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.162717" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "9900.000000" },
            { "fleetLocus.force.weight.value.mobJitterScale", "0.214938" },
            { "fleetLocus.force.weight.value.value", "5.710650" },
            { "fleetLocus.force.weight.valueType", "periodic" },
            { "fleetLocus.linearWeight", "1.278957" },
            { "fleetLocus.linearXPeriod", "8415.854492" },
            { "fleetLocus.linearYPeriod", "12345.000000" },
            { "fleetLocus.randomPeriod", "4342.521973" },
            { "fleetLocus.randomWeight", "1.686487" },
            { "fleetLocus.useScaled", "FALSE" },
            { "gatherAbandonStale", "FALSE" },
            { "gatherRange", "52.850559" },
            { "guardRange", "200.562622" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.182473" },
            { "mobLocus.circularPeriod.value", "7075.960938" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "0.359235" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "0.331540" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "0.800632" },
            { "mobLocus.circularWeight.periodic.period.value", "518.070496" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.758455" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "971.241943" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.278737" },
            { "mobLocus.circularWeight.value.value", "2.435009" },
            { "mobLocus.circularWeight.valueType", "periodic" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.998171" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.621639" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "0.090579" },
            { "mobLocus.force.range.radius.periodic.period.value", "7565.159180" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.235655" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "5741.665039" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "-0.684285" },
            { "mobLocus.force.range.radius.value.value", "1372.306519" },
            { "mobLocus.force.range.radius.valueType", "periodic" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.256010" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.685738" },
            { "mobLocus.force.crowd.size.periodic.period.value", "5590.333008" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.063424" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "3684.881104" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.733576" },
            { "mobLocus.force.crowd.size.value.value", "14.643238" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowd.type.check", "never" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.795150" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.817033" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "0.898221" },
            { "mobLocus.force.range.radius.periodic.period.value", "9658.197266" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.621063" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "6856.098145" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "-0.117397" },
            { "mobLocus.force.range.radius.value.value", "2000.000000" },
            { "mobLocus.force.range.radius.valueType", "periodic" },
            { "mobLocus.force.range.type.check", "never" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "0.666675" },
            { "mobLocus.force.weight.periodic.amplitude.value", "-0.563960" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "-1.000000" },
            { "mobLocus.force.weight.periodic.period.value", "10000.000000" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "mobLocus.force.weight.periodic.tickShift.value", "1296.970947" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.353412" },
            { "mobLocus.force.weight.value.value", "-0.315948" },
            { "mobLocus.force.weight.valueType", "constant" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "0.797679" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.437860" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.780965" },
            { "mobLocus.linearWeight.periodic.period.value", "2755.506348" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "0.907684" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "6077.991211" },
            { "mobLocus.linearWeight.value.mobJitterScale", "0.271331" },
            { "mobLocus.linearWeight.value.value", "-4.088938" },
            { "mobLocus.linearWeight.valueType", "constant" },
            { "mobLocus.linearXPeriod.mobJitterScale", "-0.547205" },
            { "mobLocus.linearXPeriod.value", "2055.839844" },
            { "mobLocus.linearYPeriod.mobJitterScale", "-0.347451" },
            { "mobLocus.linearYPeriod.value", "10000.000000" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "0.422239" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.583711" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "-0.606503" },
            { "mobLocus.proximityRadius.periodic.period.value", "7181.252441" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "-0.324248" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "2801.618164" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "-0.524231" },
            { "mobLocus.proximityRadius.value.value", "730.915100" },
            { "mobLocus.proximityRadius.valueType", "constant" },
            { "mobLocus.randomPeriod.mobJitterScale", "-0.396322" },
            { "mobLocus.randomPeriod.value", "3842.960449" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "0.561786" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "0.545125" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "0.167213" },
            { "mobLocus.randomWeight.periodic.period.value", "-0.900000" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-0.884398" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "-1.000000" },
            { "mobLocus.randomWeight.value.mobJitterScale", "-0.474759" },
            { "mobLocus.randomWeight.value.value", "1.808278" },
            { "mobLocus.randomWeight.valueType", "periodic" },
            { "mobLocus.resetOnProximity", "TRUE" },
            { "mobLocus.useScaled", "TRUE" },
            { "nearBaseRadius", "91.025482" },
            { "nearBaseRandomIdle.forceOn", "FALSE" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.656393" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.213289" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "0.877674" },
            { "nearestFriend.range.radius.periodic.period.value", "2933.867432" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "0.725152" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.range.radius.value.mobJitterScale", "0.478979" },
            { "nearestFriend.range.radius.value.value", "813.628235" },
            { "nearestFriend.range.radius.valueType", "periodic" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "-0.082733" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "-0.325498" },
            { "nearestFriend.crowd.size.periodic.period.value", "6398.181641" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "0.885306" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "9000.000000" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "-0.938299" },
            { "nearestFriend.crowd.size.value.value", "15.717419" },
            { "nearestFriend.crowd.size.valueType", "periodic" },
            { "nearestFriend.crowd.type.check", "never" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.510415" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.701764" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "0.578944" },
            { "nearestFriend.range.radius.periodic.period.value", "4864.889160" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.034630" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "4568.888672" },
            { "nearestFriend.range.radius.value.mobJitterScale", "0.635803" },
            { "nearestFriend.range.radius.value.value", "1145.889038" },
            { "nearestFriend.range.radius.valueType", "periodic" },
            { "nearestFriend.range.type.check", "never" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.753387" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "0.917711" },
            { "nearestFriend.weight.periodic.period.value", "793.138672" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "-0.133992" },
            { "nearestFriend.weight.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.weight.value.mobJitterScale", "0.783617" },
            { "nearestFriend.weight.value.value", "0.058244" },
            { "nearestFriend.weight.valueType", "periodic" },
            { "randomIdle.forceOn", "TRUE" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "rotateStartingAngle", "FALSE" },
            { "sensorGrid.staleCoreTime", "41.629436" },
            { "sensorGrid.staleFighterTime", "0.950000" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.345688" },
            { "separate.range.radius.periodic.amplitude.value", "-0.522793" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.338378" },
            { "separate.range.radius.periodic.period.value", "10000.000000" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.192211" },
            { "separate.range.radius.periodic.tickShift.value", "8513.941406" },
            { "separate.range.radius.value.mobJitterScale", "-0.775946" },
            { "separate.range.radius.value.value", "965.350220" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "0.468782" },
            { "separate.crowd.size.periodic.amplitude.value", "-0.408151" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "0.134387" },
            { "separate.crowd.size.periodic.period.value", "3136.231934" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.889874" },
            { "separate.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "separate.crowd.size.value.mobJitterScale", "-0.003657" },
            { "separate.crowd.size.value.value", "9.810118" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowd.type.check", "never" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.875430" },
            { "separate.range.radius.periodic.amplitude.value", "0.112632" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.289590" },
            { "separate.range.radius.periodic.period.value", "5220.742676" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.714845" },
            { "separate.range.radius.periodic.tickShift.value", "10000.000000" },
            { "separate.range.radius.value.mobJitterScale", "0.361008" },
            { "separate.range.radius.value.value", "1016.923523" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.range.type.check", "never" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.748777" },
            { "separate.weight.periodic.amplitude.value", "-0.464512" },
            { "separate.weight.periodic.period.mobJitterScale", "0.029400" },
            { "separate.weight.periodic.period.value", "6154.719727" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.633351" },
            { "separate.weight.periodic.tickShift.value", "7947.900391" },
            { "separate.weight.value.mobJitterScale", "0.277628" },
            { "separate.weight.value.value", "-1.060454" },
            { "separate.weight.valueType", "constant" },
            { "simpleAttack.forceOn", "TRUE" },
            { "startingMaxRadius", "1994.999878" },
            { "startingMinRadius", "358.747559" },
        };

        BundleConfigValue configs5[] = {
            { "align.range.radius.periodic.amplitude.mobJitterScale", "0.773816" },
            { "align.range.radius.periodic.amplitude.value", "0.531932" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.872063" },
            { "align.range.radius.periodic.period.value", "6809.119629" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.548249" },
            { "align.range.radius.periodic.tickShift.value", "1093.369629" },
            { "align.range.radius.value.mobJitterScale", "-1.000000" },
            { "align.range.radius.value.value", "330.432159" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "-0.663701" },
            { "align.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "align.crowd.size.periodic.period.mobJitterScale", "-0.785155" },
            { "align.crowd.size.periodic.period.value", "9066.664062" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "-0.204497" },
            { "align.crowd.size.periodic.tickShift.value", "1817.129517" },
            { "align.crowd.size.value.mobJitterScale", "0.474505" },
            { "align.crowd.size.value.value", "0.326203" },
            { "align.crowd.size.valueType", "periodic" },
            { "align.crowd.type.check", "never" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.103650" },
            { "align.range.radius.periodic.amplitude.value", "-0.215587" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.504149" },
            { "align.range.radius.periodic.period.value", "2593.607178" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "-0.627764" },
            { "align.range.radius.periodic.tickShift.value", "4706.968262" },
            { "align.range.radius.value.mobJitterScale", "0.769160" },
            { "align.range.radius.value.value", "-1.000000" },
            { "align.range.radius.valueType", "periodic" },
            { "align.range.type.check", "never" },
            { "align.weight.periodic.amplitude.mobJitterScale", "0.875352" },
            { "align.weight.periodic.amplitude.value", "0.655263" },
            { "align.weight.periodic.period.mobJitterScale", "-1.000000" },
            { "align.weight.periodic.period.value", "8135.162109" },
            { "align.weight.periodic.tickShift.mobJitterScale", "0.530720" },
            { "align.weight.periodic.tickShift.value", "1806.654541" },
            { "align.weight.value.mobJitterScale", "0.048118" },
            { "align.weight.value.value", "0.650391" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "185.254288" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.127337" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.867969" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "-0.650682" },
            { "attackSeparate.range.radius.periodic.period.value", "-0.990000" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.792535" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "3070.885010" },
            { "attackSeparate.range.radius.value.mobJitterScale", "0.069181" },
            { "attackSeparate.range.radius.value.value", "1373.903198" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.414377" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.777303" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.653349" },
            { "attackSeparate.crowd.size.periodic.period.value", "8451.583008" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-0.117829" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "1322.139404" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-0.999978" },
            { "attackSeparate.crowd.size.value.value", "16.245770" },
            { "attackSeparate.crowd.size.valueType", "constant" },
            { "attackSeparate.crowd.type.check", "never" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.371149" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.435201" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.730247" },
            { "attackSeparate.range.radius.periodic.period.value", "3255.769531" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.782087" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "1119.629150" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.483809" },
            { "attackSeparate.range.radius.value.value", "1928.739868" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.range.type.check", "never" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.141387" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.370526" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.572546" },
            { "attackSeparate.weight.periodic.period.value", "3096.504639" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "0.782674" },
            { "attackSeparate.weight.periodic.tickShift.value", "10000.000000" },
            { "attackSeparate.weight.value.mobJitterScale", "0.562192" },
            { "attackSeparate.weight.value.value", "8.094960" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "-0.143468" },
            { "base.range.radius.periodic.amplitude.value", "1.000000" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.033025" },
            { "base.range.radius.periodic.period.value", "2163.767090" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "base.range.radius.periodic.tickShift.value", "1676.326050" },
            { "base.range.radius.value.mobJitterScale", "-0.648948" },
            { "base.range.radius.value.value", "827.924133" },
            { "base.range.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.267228" },
            { "base.crowd.size.periodic.amplitude.value", "0.609871" },
            { "base.crowd.size.periodic.period.mobJitterScale", "-0.201008" },
            { "base.crowd.size.periodic.period.value", "8583.216797" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.667400" },
            { "base.crowd.size.periodic.tickShift.value", "6222.792969" },
            { "base.crowd.size.value.mobJitterScale", "-0.503096" },
            { "base.crowd.size.value.value", "-0.866832" },
            { "base.crowd.size.valueType", "periodic" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "-0.067425" },
            { "base.range.radius.periodic.amplitude.value", "0.706420" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.527732" },
            { "base.range.radius.periodic.period.value", "5201.663086" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.557585" },
            { "base.range.radius.periodic.tickShift.value", "5289.015137" },
            { "base.range.radius.value.mobJitterScale", "-0.272672" },
            { "base.range.radius.value.value", "1037.696167" },
            { "base.range.radius.valueType", "constant" },
            { "base.range.type.check", "never" },
            { "base.weight.periodic.amplitude.mobJitterScale", "-0.788972" },
            { "base.weight.periodic.amplitude.value", "-0.450654" },
            { "base.weight.periodic.period.mobJitterScale", "-0.968389" },
            { "base.weight.periodic.period.value", "8952.544922" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.518093" },
            { "base.weight.periodic.tickShift.value", "865.104248" },
            { "base.weight.value.mobJitterScale", "0.210789" },
            { "base.weight.value.value", "1.327885" },
            { "base.weight.valueType", "periodic" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "0.139872" },
            { "baseDefense.range.radius.periodic.amplitude.value", "-0.794197" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.217053" },
            { "baseDefense.range.radius.periodic.period.value", "-1.000000" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.294163" },
            { "baseDefense.range.radius.periodic.tickShift.value", "5248.515137" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.439425" },
            { "baseDefense.range.radius.value.value", "-0.900000" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "0.775622" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.516851" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "-0.867945" },
            { "baseDefense.crowd.size.periodic.period.value", "7598.124023" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "-0.794069" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "5602.407227" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.349934" },
            { "baseDefense.crowd.size.value.value", "2.904340" },
            { "baseDefense.crowd.size.valueType", "constant" },
            { "baseDefense.crowd.type.check", "never" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.898434" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.503716" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.158888" },
            { "baseDefense.range.radius.periodic.period.value", "6628.075684" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.663771" },
            { "baseDefense.range.radius.periodic.tickShift.value", "2282.360352" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.237997" },
            { "baseDefense.range.radius.value.value", "47.057972" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "never" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "0.307280" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.047181" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "-0.898617" },
            { "baseDefense.weight.periodic.period.value", "5971.128906" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "baseDefense.weight.periodic.tickShift.value", "10000.000000" },
            { "baseDefense.weight.value.mobJitterScale", "0.682046" },
            { "baseDefense.weight.value.value", "-4.005088" },
            { "baseDefense.weight.valueType", "constant" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.777972" },
            { "center.range.radius.periodic.amplitude.value", "0.798949" },
            { "center.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "center.range.radius.periodic.period.value", "7877.466309" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.371853" },
            { "center.range.radius.periodic.tickShift.value", "3341.117676" },
            { "center.range.radius.value.mobJitterScale", "0.525610" },
            { "center.range.radius.value.value", "1544.166138" },
            { "center.range.radius.valueType", "constant" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.686237" },
            { "center.crowd.size.periodic.amplitude.value", "-0.748714" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.463095" },
            { "center.crowd.size.periodic.period.value", "6657.798828" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "0.369272" },
            { "center.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "center.crowd.size.value.mobJitterScale", "0.608883" },
            { "center.crowd.size.value.value", "12.643256" },
            { "center.crowd.size.valueType", "constant" },
            { "center.crowd.type.check", "never" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "0.789043" },
            { "center.range.radius.periodic.amplitude.value", "0.451824" },
            { "center.range.radius.periodic.period.mobJitterScale", "0.122213" },
            { "center.range.radius.periodic.period.value", "3185.374023" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "-0.323503" },
            { "center.range.radius.periodic.tickShift.value", "3245.539307" },
            { "center.range.radius.value.mobJitterScale", "-0.377821" },
            { "center.range.radius.value.value", "1374.222168" },
            { "center.range.radius.valueType", "constant" },
            { "center.range.type.check", "never" },
            { "center.weight.periodic.amplitude.mobJitterScale", "-0.313885" },
            { "center.weight.periodic.amplitude.value", "0.581380" },
            { "center.weight.periodic.period.mobJitterScale", "0.258521" },
            { "center.weight.periodic.period.value", "3850.435547" },
            { "center.weight.periodic.tickShift.mobJitterScale", "-0.183710" },
            { "center.weight.periodic.tickShift.value", "79.234589" },
            { "center.weight.value.mobJitterScale", "1.000000" },
            { "center.weight.value.value", "8.907995" },
            { "center.weight.valueType", "periodic" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.793762" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.200905" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.671177" },
            { "cohere.range.radius.periodic.period.value", "3256.916260" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.082508" },
            { "cohere.range.radius.periodic.tickShift.value", "2163.927002" },
            { "cohere.range.radius.value.mobJitterScale", "-0.562042" },
            { "cohere.range.radius.value.value", "1780.552246" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.481264" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.583685" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "0.779526" },
            { "cohere.crowd.size.periodic.period.value", "6981.572754" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "-0.770761" },
            { "cohere.crowd.size.periodic.tickShift.value", "1402.559692" },
            { "cohere.crowd.size.value.mobJitterScale", "-0.160711" },
            { "cohere.crowd.size.value.value", "15.596762" },
            { "cohere.crowd.type.check", "never" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.810000" },
            { "cohere.range.radius.periodic.amplitude.value", "0.129742" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "0.404731" },
            { "cohere.range.radius.periodic.period.value", "4454.511719" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.265320" },
            { "cohere.range.radius.periodic.tickShift.value", "6569.726074" },
            { "cohere.range.radius.value.mobJitterScale", "0.097655" },
            { "cohere.range.radius.value.value", "960.147583" },
            { "cohere.range.radius.valueType", "constant" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "-0.608981" },
            { "cohere.weight.periodic.amplitude.value", "-0.095188" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.454763" },
            { "cohere.weight.periodic.period.value", "5881.180176" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.574161" },
            { "cohere.weight.periodic.tickShift.value", "2238.082764" },
            { "cohere.weight.value.mobJitterScale", "0.195610" },
            { "cohere.weight.value.value", "-3.071281" },
            { "cohere.weight.valueType", "periodic" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "-0.220961" },
            { "cores.range.radius.periodic.amplitude.value", "0.111419" },
            { "cores.range.radius.periodic.period.mobJitterScale", "0.356439" },
            { "cores.range.radius.periodic.period.value", "7393.668457" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "-0.562008" },
            { "cores.range.radius.periodic.tickShift.value", "4944.792969" },
            { "cores.range.radius.value.mobJitterScale", "-0.733639" },
            { "cores.range.radius.value.value", "48.773293" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "0.232942" },
            { "cores.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-0.810000" },
            { "cores.crowd.size.periodic.period.value", "-1.000000" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "-0.183926" },
            { "cores.crowd.size.periodic.tickShift.value", "5863.881348" },
            { "cores.crowd.size.value.mobJitterScale", "-0.076045" },
            { "cores.crowd.size.value.value", "2.463980" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowd.type.check", "never" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "cores.range.radius.periodic.amplitude.value", "1.000000" },
            { "cores.range.radius.periodic.period.mobJitterScale", "0.562891" },
            { "cores.range.radius.periodic.period.value", "3728.972412" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.783805" },
            { "cores.range.radius.periodic.tickShift.value", "6189.655762" },
            { "cores.range.radius.value.mobJitterScale", "0.320882" },
            { "cores.range.radius.value.value", "338.741577" },
            { "cores.range.radius.valueType", "constant" },
            { "cores.range.type.check", "never" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.482375" },
            { "cores.weight.periodic.amplitude.value", "0.724672" },
            { "cores.weight.periodic.period.mobJitterScale", "-0.833502" },
            { "cores.weight.periodic.period.value", "1859.984009" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "0.021778" },
            { "cores.weight.periodic.tickShift.value", "4533.100586" },
            { "cores.weight.value.mobJitterScale", "-0.775033" },
            { "cores.weight.value.value", "-6.447917" },
            { "cores.weight.valueType", "constant" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.614196" },
            { "corners.range.radius.periodic.amplitude.value", "0.146834" },
            { "corners.range.radius.periodic.period.mobJitterScale", "0.295635" },
            { "corners.range.radius.periodic.period.value", "2405.400635" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "0.538336" },
            { "corners.range.radius.periodic.tickShift.value", "-1.000000" },
            { "corners.range.radius.value.mobJitterScale", "-1.000000" },
            { "corners.range.radius.value.value", "1273.083618" },
            { "corners.range.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "-0.777530" },
            { "corners.crowd.size.periodic.amplitude.value", "0.180582" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "-0.492333" },
            { "corners.crowd.size.periodic.period.value", "6131.944824" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.241881" },
            { "corners.crowd.size.periodic.tickShift.value", "2165.073730" },
            { "corners.crowd.size.value.mobJitterScale", "-0.226080" },
            { "corners.crowd.size.value.value", "3.197161" },
            { "corners.crowd.size.valueType", "periodic" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.162802" },
            { "corners.range.radius.periodic.amplitude.value", "0.205700" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "corners.range.radius.periodic.period.value", "8513.180664" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-0.902629" },
            { "corners.range.radius.periodic.tickShift.value", "428.440582" },
            { "corners.range.radius.value.mobJitterScale", "-0.489866" },
            { "corners.range.radius.value.value", "1737.401855" },
            { "corners.range.radius.valueType", "constant" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "-0.673179" },
            { "corners.weight.periodic.amplitude.value", "-0.257665" },
            { "corners.weight.periodic.period.mobJitterScale", "-0.523889" },
            { "corners.weight.periodic.period.value", "7460.093750" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "0.790126" },
            { "corners.weight.periodic.tickShift.value", "613.216797" },
            { "corners.weight.value.mobJitterScale", "-0.549651" },
            { "corners.weight.value.value", "-4.739366" },
            { "creditReserve", "199.446793" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.734859" },
            { "curHeadingWeight.periodic.amplitude.value", "0.614133" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "0.438706" },
            { "curHeadingWeight.periodic.period.value", "2694.040527" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "-0.608658" },
            { "curHeadingWeight.periodic.tickShift.value", "7904.550293" },
            { "curHeadingWeight.value.mobJitterScale", "0.519899" },
            { "curHeadingWeight.value.value", "0.870777" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.468820" },
            { "edges.range.radius.periodic.amplitude.value", "1.000000" },
            { "edges.range.radius.periodic.period.mobJitterScale", "0.500206" },
            { "edges.range.radius.periodic.period.value", "1895.671143" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.900000" },
            { "edges.range.radius.periodic.tickShift.value", "9928.017578" },
            { "edges.range.radius.value.mobJitterScale", "-0.482913" },
            { "edges.range.radius.value.value", "182.700409" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.668091" },
            { "edges.crowd.size.periodic.amplitude.value", "-0.675569" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "0.450548" },
            { "edges.crowd.size.periodic.period.value", "6614.987793" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "1.000000" },
            { "edges.crowd.size.periodic.tickShift.value", "5014.099121" },
            { "edges.crowd.size.value.mobJitterScale", "-0.247955" },
            { "edges.crowd.size.value.value", "17.280859" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.706828" },
            { "edges.range.radius.periodic.amplitude.value", "0.068807" },
            { "edges.range.radius.periodic.period.mobJitterScale", "0.648850" },
            { "edges.range.radius.periodic.period.value", "5369.750977" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "edges.range.radius.periodic.tickShift.value", "1020.260742" },
            { "edges.range.radius.value.mobJitterScale", "-0.774045" },
            { "edges.range.radius.value.value", "1190.647827" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.836452" },
            { "edges.weight.periodic.amplitude.value", "-0.709553" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.360581" },
            { "edges.weight.periodic.period.value", "2934.161621" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.308657" },
            { "edges.weight.periodic.tickShift.value", "4227.692871" },
            { "edges.weight.value.mobJitterScale", "0.517211" },
            { "edges.weight.value.value", "2.220076" },
            { "edges.weight.valueType", "periodic" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.778036" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.275185" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.846396" },
            { "enemy.range.radius.periodic.period.value", "9062.518555" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.252552" },
            { "enemy.range.radius.periodic.tickShift.value", "-1.000000" },
            { "enemy.range.radius.value.mobJitterScale", "0.071094" },
            { "enemy.range.radius.value.value", "2000.000000" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.576239" },
            { "enemy.crowd.size.periodic.amplitude.value", "-0.429896" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.593857" },
            { "enemy.crowd.size.periodic.period.value", "8189.122070" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "-0.093932" },
            { "enemy.crowd.size.periodic.tickShift.value", "5808.433105" },
            { "enemy.crowd.size.value.mobJitterScale", "0.187990" },
            { "enemy.crowd.size.value.value", "12.986944" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "-0.124516" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.279534" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "-0.485041" },
            { "enemy.range.radius.periodic.period.value", "1062.841309" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.097877" },
            { "enemy.range.radius.periodic.tickShift.value", "3565.766602" },
            { "enemy.range.radius.value.mobJitterScale", "-0.534148" },
            { "enemy.range.radius.value.value", "322.774628" },
            { "enemy.range.radius.valueType", "constant" },
            { "enemy.range.type.check", "never" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "-0.338877" },
            { "enemy.weight.periodic.amplitude.value", "-0.203866" },
            { "enemy.weight.periodic.period.mobJitterScale", "-0.153307" },
            { "enemy.weight.periodic.period.value", "7109.623535" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.758874" },
            { "enemy.weight.periodic.tickShift.value", "5699.695801" },
            { "enemy.weight.value.mobJitterScale", "0.214018" },
            { "enemy.weight.value.value", "4.428343" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.500281" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.494311" },
            { "enemyBase.range.radius.periodic.period.value", "6070.160156" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "-0.881831" },
            { "enemyBase.range.radius.periodic.tickShift.value", "7549.542969" },
            { "enemyBase.range.radius.value.mobJitterScale", "-0.647612" },
            { "enemyBase.range.radius.value.value", "1173.403442" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.001773" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "0.132705" },
            { "enemyBase.crowd.size.periodic.period.value", "4529.246582" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.043878" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "3573.961914" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.678228" },
            { "enemyBase.crowd.size.value.value", "13.380767" },
            { "enemyBase.crowd.type.check", "never" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "enemyBase.range.radius.periodic.amplitude.value", "0.892146" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.762241" },
            { "enemyBase.range.radius.periodic.period.value", "5765.630859" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "-0.050843" },
            { "enemyBase.range.radius.periodic.tickShift.value", "4790.399902" },
            { "enemyBase.range.radius.value.mobJitterScale", "0.184679" },
            { "enemyBase.range.radius.value.value", "530.059814" },
            { "enemyBase.range.radius.valueType", "periodic" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.641205" },
            { "enemyBase.weight.periodic.amplitude.value", "-0.473076" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "0.956191" },
            { "enemyBase.weight.periodic.period.value", "4746.312012" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "0.375240" },
            { "enemyBase.weight.periodic.tickShift.value", "8100.000000" },
            { "enemyBase.weight.value.mobJitterScale", "0.393788" },
            { "enemyBase.weight.value.value", "3.454714" },
            { "enemyBase.weight.valueType", "periodic" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "359.532593" },
            { "evadeStrictDistance", "355.788940" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fleetLocus.circularPeriod", "4831.017090" },
            { "fleetLocus.circularWeight", "1.466555" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.139218" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.970357" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "fleetLocus.force.range.radius.periodic.period.value", "7711.861816" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.966953" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "8292.224609" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.800082" },
            { "fleetLocus.force.range.radius.value.value", "1501.743896" },
            { "fleetLocus.force.range.radius.valueType", "constant" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.056530" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.421574" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "0.026090" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "6202.354492" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.268360" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "2289.572998" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "-0.302817" },
            { "fleetLocus.force.crowd.size.value.value", "13.779195" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "never" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.194120" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.386690" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "fleetLocus.force.range.radius.periodic.period.value", "1032.267456" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.422855" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "5188.332031" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.868949" },
            { "fleetLocus.force.range.radius.value.value", "1827.765259" },
            { "fleetLocus.force.range.radius.valueType", "constant" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.431565" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.834771" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-0.491313" },
            { "fleetLocus.force.weight.periodic.period.value", "3707.137939" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.350996" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "5968.701172" },
            { "fleetLocus.force.weight.value.mobJitterScale", "0.158934" },
            { "fleetLocus.force.weight.value.value", "-7.459723" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "1.528537" },
            { "fleetLocus.linearXPeriod", "3935.301025" },
            { "fleetLocus.linearYPeriod", "575.660706" },
            { "fleetLocus.randomPeriod", "8751.974609" },
            { "fleetLocus.randomWeight", "1.901301" },
            { "fleetLocus.useScaled", "TRUE" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "65.696503" },
            { "guardRange", "134.828064" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.119480" },
            { "mobLocus.circularPeriod.value", "6583.940430" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "-0.380331" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "0.100596" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "0.195609" },
            { "mobLocus.circularWeight.periodic.period.value", "267.512787" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.071787" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "4902.237305" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.405023" },
            { "mobLocus.circularWeight.value.value", "-7.299368" },
            { "mobLocus.circularWeight.valueType", "constant" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.165504" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.247477" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "mobLocus.force.range.radius.periodic.period.value", "7382.423340" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.519529" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "9915.421875" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "-0.781897" },
            { "mobLocus.force.range.radius.value.value", "243.272263" },
            { "mobLocus.force.range.radius.valueType", "periodic" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.790946" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-0.785212" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.881101" },
            { "mobLocus.force.crowd.size.periodic.period.value", "2802.677490" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.374478" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "7871.564941" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "0.173263" },
            { "mobLocus.force.crowd.size.value.value", "10.225864" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowd.type.check", "never" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.286870" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.794056" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "0.996017" },
            { "mobLocus.force.range.radius.periodic.period.value", "5794.513672" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.212289" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "2314.237061" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.499106" },
            { "mobLocus.force.range.radius.value.value", "1496.998535" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.range.type.check", "never" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.835072" },
            { "mobLocus.force.weight.periodic.amplitude.value", "0.232473" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.034322" },
            { "mobLocus.force.weight.periodic.period.value", "2632.268799" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.900000" },
            { "mobLocus.force.weight.periodic.tickShift.value", "1326.583374" },
            { "mobLocus.force.weight.value.mobJitterScale", "-0.466181" },
            { "mobLocus.force.weight.value.value", "-7.970978" },
            { "mobLocus.force.weight.valueType", "periodic" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "0.605482" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "1.000000" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "-0.839933" },
            { "mobLocus.linearWeight.periodic.period.value", "4025.183594" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "-0.098022" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "3862.218262" },
            { "mobLocus.linearWeight.value.mobJitterScale", "0.795993" },
            { "mobLocus.linearWeight.value.value", "-0.141514" },
            { "mobLocus.linearWeight.valueType", "periodic" },
            { "mobLocus.linearXPeriod.mobJitterScale", "-0.149883" },
            { "mobLocus.linearXPeriod.value", "489.058014" },
            { "mobLocus.linearYPeriod.mobJitterScale", "0.230032" },
            { "mobLocus.linearYPeriod.value", "584.972351" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "0.078539" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.682326" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "0.849688" },
            { "mobLocus.proximityRadius.periodic.period.value", "8019.000000" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "7140.731934" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "-0.892326" },
            { "mobLocus.proximityRadius.value.value", "1360.677979" },
            { "mobLocus.proximityRadius.valueType", "periodic" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.273299" },
            { "mobLocus.randomPeriod.value", "472.677795" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "0.471519" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "0.156658" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "-0.792849" },
            { "mobLocus.randomWeight.periodic.period.value", "5967.139648" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "4776.847168" },
            { "mobLocus.randomWeight.value.mobJitterScale", "-0.549930" },
            { "mobLocus.randomWeight.value.value", "-1.080561" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "FALSE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "466.211639" },
            { "nearBaseRandomIdle.forceOn", "TRUE" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "-0.079872" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "-0.011918" },
            { "nearestFriend.range.radius.periodic.period.value", "6181.295410" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.270052" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "4150.705078" },
            { "nearestFriend.range.radius.value.mobJitterScale", "0.043697" },
            { "nearestFriend.range.radius.value.value", "785.779053" },
            { "nearestFriend.range.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "-0.031213" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "-0.820430" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "0.522729" },
            { "nearestFriend.crowd.size.periodic.period.value", "3796.199219" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "-0.213395" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "1636.398315" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "0.618949" },
            { "nearestFriend.crowd.size.value.value", "1.045862" },
            { "nearestFriend.crowd.size.valueType", "periodic" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.335377" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.362549" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "-0.611512" },
            { "nearestFriend.range.radius.periodic.period.value", "7575.432617" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.367669" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "7958.236816" },
            { "nearestFriend.range.radius.value.mobJitterScale", "-0.434431" },
            { "nearestFriend.range.radius.value.value", "1183.872437" },
            { "nearestFriend.range.radius.valueType", "periodic" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "0.543940" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.667710" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "0.410157" },
            { "nearestFriend.weight.periodic.period.value", "10000.000000" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "-0.001351" },
            { "nearestFriend.weight.periodic.tickShift.value", "1093.079102" },
            { "nearestFriend.weight.value.mobJitterScale", "-0.601265" },
            { "nearestFriend.weight.value.value", "-2.827474" },
            { "randomIdle.forceOn", "TRUE" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "15.258034" },
            { "sensorGrid.staleFighterTime", "1.707165" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.114589" },
            { "separate.range.radius.periodic.amplitude.value", "0.083034" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.697423" },
            { "separate.range.radius.periodic.period.value", "2721.856445" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.168553" },
            { "separate.range.radius.periodic.tickShift.value", "6264.737305" },
            { "separate.range.radius.value.mobJitterScale", "-0.545271" },
            { "separate.range.radius.value.value", "909.844849" },
            { "separate.range.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "0.461186" },
            { "separate.crowd.size.periodic.amplitude.value", "1.000000" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "-0.265448" },
            { "separate.crowd.size.periodic.period.value", "3613.857422" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "-0.761493" },
            { "separate.crowd.size.periodic.tickShift.value", "7793.289062" },
            { "separate.crowd.size.value.mobJitterScale", "0.671655" },
            { "separate.crowd.size.value.value", "11.438020" },
            { "separate.crowd.type.check", "never" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.237986" },
            { "separate.range.radius.periodic.amplitude.value", "-0.203410" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.078867" },
            { "separate.range.radius.periodic.period.value", "2530.513184" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.412025" },
            { "separate.range.radius.periodic.tickShift.value", "4656.739746" },
            { "separate.range.radius.value.mobJitterScale", "-0.310538" },
            { "separate.range.radius.value.value", "542.744324" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.range.type.check", "never" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.518334" },
            { "separate.weight.periodic.amplitude.value", "-0.487461" },
            { "separate.weight.periodic.period.mobJitterScale", "0.127719" },
            { "separate.weight.periodic.period.value", "8897.419922" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.827311" },
            { "separate.weight.periodic.tickShift.value", "4081.031982" },
            { "separate.weight.value.mobJitterScale", "0.296379" },
            { "separate.weight.value.value", "10.000000" },
            { "separate.weight.valueType", "constant" },
            { "simpleAttack.forceOn", "TRUE" },
            { "startingMaxRadius", "1131.782349" },
            { "startingMinRadius", "566.272705" },
        };

        BundleConfigValue configs6[] = {
            { "align.range.radius.periodic.amplitude.mobJitterScale", "0.303767"},
            { "align.range.radius.periodic.amplitude.value", "-0.028980" },
            { "align.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "align.range.radius.periodic.period.value", "3396.909424" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.313868" },
            { "align.range.radius.periodic.tickShift.value", "4040.127930" },
            { "align.range.radius.value.mobJitterScale", "0.843717" },
            { "align.range.radius.value.value", "1462.830566" },
            { "align.range.radius.valueType", "periodic" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "-0.349837" },
            { "align.crowd.size.periodic.amplitude.value", "-0.690101" },
            { "align.crowd.size.periodic.period.mobJitterScale", "-0.230328" },
            { "align.crowd.size.periodic.period.value", "8951.242188" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "-0.319363" },
            { "align.crowd.size.periodic.tickShift.value", "3732.774658" },
            { "align.crowd.size.value.mobJitterScale", "0.409335" },
            { "align.crowd.size.value.value", "0.287741" },
            { "align.crowd.size.valueType", "periodic" },
            { "align.crowd.type.check", "never" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.590458" },
            { "align.range.radius.periodic.amplitude.value", "0.265961" },
            { "align.range.radius.periodic.period.mobJitterScale", "-0.620556" },
            { "align.range.radius.periodic.period.value", "5049.148438" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.356558" },
            { "align.range.radius.periodic.tickShift.value", "2067.238525" },
            { "align.range.radius.value.mobJitterScale", "1.000000" },
            { "align.range.radius.value.value", "653.465942" },
            { "align.range.radius.valueType", "periodic" },
            { "align.range.type.check", "never" },
            { "align.weight.periodic.amplitude.mobJitterScale", "0.914211" },
            { "align.weight.periodic.amplitude.value", "0.442087" },
            { "align.weight.periodic.period.mobJitterScale", "-1.000000" },
            { "align.weight.periodic.period.value", "-0.900000" },
            { "align.weight.periodic.tickShift.mobJitterScale", "0.261621" },
            { "align.weight.periodic.tickShift.value", "1806.654541" },
            { "align.weight.value.mobJitterScale", "-0.128512" },
            { "align.weight.value.value", "7.215507" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "174.674911" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.408364" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.680756" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "-0.404524" },
            { "attackSeparate.range.radius.periodic.period.value", "7870.269043" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "-0.590382" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "6012.278320" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.349955" },
            { "attackSeparate.range.radius.value.value", "836.828186" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.003222" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.699573" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.867885" },
            { "attackSeparate.crowd.size.periodic.period.value", "6063.754395" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "0.386739" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "1189.925415" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-0.253843" },
            { "attackSeparate.crowd.size.value.value", "11.676046" },
            { "attackSeparate.crowd.size.valueType", "constant" },
            { "attackSeparate.crowd.type.check", "never" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.785959" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.650087" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.748814" },
            { "attackSeparate.range.radius.periodic.period.value", "4984.351562" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "3720.978271" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.928496" },
            { "attackSeparate.range.radius.value.value", "388.012268" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.range.type.check", "never" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.545036" },
            { "attackSeparate.weight.periodic.amplitude.value", "-0.388967" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.119417" },
            { "attackSeparate.weight.periodic.period.value", "5106.089355" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "0.722358" },
            { "attackSeparate.weight.periodic.tickShift.value", "8611.406250" },
            { "attackSeparate.weight.value.mobJitterScale", "-1.000000" },
            { "attackSeparate.weight.value.value", "6.145157" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "-0.308537" },
            { "base.range.radius.periodic.amplitude.value", "0.878051" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.373826" },
            { "base.range.radius.periodic.period.value", "1131.804688" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "-0.694907" },
            { "base.range.radius.periodic.tickShift.value", "586.410522" },
            { "base.range.radius.value.mobJitterScale", "-0.460821" },
            { "base.range.radius.value.value", "235.429932" },
            { "base.range.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.272250" },
            { "base.crowd.size.periodic.amplitude.value", "0.724059" },
            { "base.crowd.size.periodic.period.mobJitterScale", "-0.263019" },
            { "base.crowd.size.periodic.period.value", "5192.633301" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "-0.085008" },
            { "base.crowd.size.periodic.tickShift.value", "10000.000000" },
            { "base.crowd.size.value.mobJitterScale", "0.684186" },
            { "base.crowd.size.value.value", "1.382313" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "never" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "-0.733963" },
            { "base.range.radius.periodic.amplitude.value", "-0.918238" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.743692" },
            { "base.range.radius.periodic.period.value", "7371.557617" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.542223" },
            { "base.range.radius.periodic.tickShift.value", "4797.590332" },
            { "base.range.radius.value.mobJitterScale", "-0.233012" },
            { "base.range.radius.value.value", "1342.211548" },
            { "base.range.radius.valueType", "constant" },
            { "base.range.type.check", "never" },
            { "base.weight.periodic.amplitude.mobJitterScale", "-0.632677" },
            { "base.weight.periodic.amplitude.value", "-0.944716" },
            { "base.weight.periodic.period.mobJitterScale", "-0.051014" },
            { "base.weight.periodic.period.value", "5432.127930" },
            { "base.weight.periodic.tickShift.mobJitterScale", "0.382693" },
            { "base.weight.periodic.tickShift.value", "6099.957520" },
            { "base.weight.value.mobJitterScale", "0.672680" },
            { "base.weight.value.value", "-9.033925" },
            { "base.weight.valueType", "periodic" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.345357" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.119525" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "-0.801793" },
            { "baseDefense.range.radius.periodic.period.value", "1869.819214" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.109900" },
            { "baseDefense.range.radius.periodic.tickShift.value", "4723.663574" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.509529" },
            { "baseDefense.range.radius.value.value", "-0.900000" },
            { "baseDefense.range.radius.valueType", "periodic" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "-0.568318" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.468610" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "-0.419971" },
            { "baseDefense.crowd.size.periodic.period.value", "7888.048340" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "-0.550325" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "5819.075684" },
            { "baseDefense.crowd.size.value.mobJitterScale", "0.550619" },
            { "baseDefense.crowd.size.value.value", "9.474373" },
            { "baseDefense.crowd.size.valueType", "constant" },
            { "baseDefense.crowd.type.check", "never" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "0.355040" },
            { "baseDefense.range.radius.periodic.amplitude.value", "-0.809205" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "-0.054079" },
            { "baseDefense.range.radius.periodic.period.value", "7848.520508" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.367415" },
            { "baseDefense.range.radius.periodic.tickShift.value", "2458.569580" },
            { "baseDefense.range.radius.value.mobJitterScale", "-0.900000" },
            { "baseDefense.range.radius.value.value", "338.759857" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "never" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-0.866812" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.193060" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "-0.868267" },
            { "baseDefense.weight.periodic.period.value", "4553.452637" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.452516" },
            { "baseDefense.weight.periodic.tickShift.value", "8953.754883" },
            { "baseDefense.weight.value.mobJitterScale", "0.620134" },
            { "baseDefense.weight.value.value", "0.315018" },
            { "baseDefense.weight.valueType", "constant" },
            { "baseDefenseRadius", "469.343994" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.777972" },
            { "center.range.radius.periodic.amplitude.value", "0.033675" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.074787" },
            { "center.range.radius.periodic.period.value", "73.988731" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.737212" },
            { "center.range.radius.periodic.tickShift.value", "3361.212891" },
            { "center.range.radius.value.mobJitterScale", "0.843037" },
            { "center.range.radius.value.value", "978.496460" },
            { "center.range.radius.valueType", "periodic" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.793935" },
            { "center.crowd.size.periodic.amplitude.value", "-0.673843" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.308655" },
            { "center.crowd.size.periodic.period.value", "7959.890625" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "0.195491" },
            { "center.crowd.size.periodic.tickShift.value", "6333.053223" },
            { "center.crowd.size.value.mobJitterScale", "-0.401140" },
            { "center.crowd.size.value.value", "-0.547282" },
            { "center.crowd.size.valueType", "constant" },
            { "center.crowd.type.check", "never" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.470269" },
            { "center.range.radius.periodic.amplitude.value", "0.406305" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.630931" },
            { "center.range.radius.periodic.period.value", "3108.676514" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "-0.300984" },
            { "center.range.radius.periodic.tickShift.value", "6037.275879" },
            { "center.range.radius.value.mobJitterScale", "-0.534004" },
            { "center.range.radius.value.value", "421.806915" },
            { "center.range.radius.valueType", "constant" },
            { "center.range.type.check", "never" },
            { "center.weight.periodic.amplitude.mobJitterScale", "0.001618" },
            { "center.weight.periodic.amplitude.value", "0.325504" },
            { "center.weight.periodic.period.mobJitterScale", "0.815193" },
            { "center.weight.periodic.period.value", "1334.674438" },
            { "center.weight.periodic.tickShift.mobJitterScale", "-0.150182" },
            { "center.weight.periodic.tickShift.value", "71.311127" },
            { "center.weight.value.mobJitterScale", "0.822143" },
            { "center.weight.value.value", "9.040425" },
            { "center.weight.valueType", "periodic" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "-0.716312" },
            { "cohere.range.radius.periodic.amplitude.value", "0.371293" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.186474" },
            { "cohere.range.radius.periodic.period.value", "8007.612793" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.769986" },
            { "cohere.range.radius.periodic.tickShift.value", "1878.314697" },
            { "cohere.range.radius.value.mobJitterScale", "-0.796296" },
            { "cohere.range.radius.value.value", "1571.860718" },
            { "cohere.range.radius.valueType", "constant" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.494403" },
            { "cohere.crowd.size.periodic.amplitude.value", "0.637176" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "0.877634" },
            { "cohere.crowd.size.periodic.period.value", "7904.680664" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.413920" },
            { "cohere.crowd.size.periodic.tickShift.value", "1388.534058" },
            { "cohere.crowd.size.value.mobJitterScale", "-0.200903" },
            { "cohere.crowd.size.value.value", "9.918414" },
            { "cohere.crowd.type.check", "never" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "-0.206370" },
            { "cohere.range.radius.periodic.amplitude.value", "0.805316" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.983127" },
            { "cohere.range.radius.periodic.period.value", "4315.744629" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.385872" },
            { "cohere.range.radius.periodic.tickShift.value", "3923.043457" },
            { "cohere.range.radius.value.mobJitterScale", "0.532986" },
            { "cohere.range.radius.value.value", "1256.103394" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "never" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "-0.030385" },
            { "cohere.weight.periodic.amplitude.value", "-0.182153" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.718743" },
            { "cohere.weight.periodic.period.value", "612.333313" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.774143" },
            { "cohere.weight.periodic.tickShift.value", "2129.974609" },
            { "cohere.weight.value.mobJitterScale", "-0.892445" },
            { "cohere.weight.value.value", "4.191203" },
            { "cohere.weight.valueType", "constant" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.477978" },
            { "cores.range.radius.periodic.amplitude.value", "-0.948364" },
            { "cores.range.radius.periodic.period.mobJitterScale", "0.442191" },
            { "cores.range.radius.periodic.period.value", "9811.847656" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "cores.range.radius.periodic.tickShift.value", "584.104919" },
            { "cores.range.radius.value.mobJitterScale", "-0.619411" },
            { "cores.range.radius.value.value", "415.072845" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "0.002229" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.900000" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "cores.crowd.size.periodic.period.value", "2615.280273" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "-0.168984" },
            { "cores.crowd.size.periodic.tickShift.value", "9535.759766" },
            { "cores.crowd.size.value.mobJitterScale", "0.116836" },
            { "cores.crowd.size.value.value", "10.824501" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowd.type.check", "never" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.900000" },
            { "cores.range.radius.periodic.amplitude.value", "0.649809" },
            { "cores.range.radius.periodic.period.mobJitterScale", "0.596434" },
            { "cores.range.radius.periodic.period.value", "4720.613770" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.900000" },
            { "cores.range.radius.periodic.tickShift.value", "7878.717773" },
            { "cores.range.radius.value.mobJitterScale", "0.101870" },
            { "cores.range.radius.value.value", "1826.412354" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.range.type.check", "never" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.637208" },
            { "cores.weight.periodic.amplitude.value", "-0.672954" },
            { "cores.weight.periodic.period.mobJitterScale", "-0.551225" },
            { "cores.weight.periodic.period.value", "3916.143066" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.810285" },
            { "cores.weight.periodic.tickShift.value", "2341.936523" },
            { "cores.weight.value.mobJitterScale", "0.514697" },
            { "cores.weight.value.value", "7.921634" },
            { "cores.weight.valueType", "constant" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.403897" },
            { "corners.range.radius.periodic.amplitude.value", "-0.256576" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-0.066142" },
            { "corners.range.radius.periodic.period.value", "3351.063965" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "corners.range.radius.periodic.tickShift.value", "2015.971313" },
            { "corners.range.radius.value.mobJitterScale", "0.466270" },
            { "corners.range.radius.value.value", "1218.303101" },
            { "corners.range.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "corners.crowd.size.periodic.amplitude.value", "-0.196087" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "-0.100772" },
            { "corners.crowd.size.periodic.period.value", "2148.500488" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.024168" },
            { "corners.crowd.size.periodic.tickShift.value", "2952.057373" },
            { "corners.crowd.size.value.mobJitterScale", "-0.881677" },
            { "corners.crowd.size.value.value", "-1.000000" },
            { "corners.crowd.size.valueType", "constant" },
            { "corners.crowd.type.check", "never" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.825848" },
            { "corners.range.radius.periodic.amplitude.value", "0.226270" },
            { "corners.range.radius.periodic.period.mobJitterScale", "0.267180" },
            { "corners.range.radius.periodic.period.value", "10000.000000" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "0.473145" },
            { "corners.range.radius.periodic.tickShift.value", "4363.056641" },
            { "corners.range.radius.value.mobJitterScale", "-0.678814" },
            { "corners.range.radius.value.value", "1079.862793" },
            { "corners.range.radius.valueType", "constant" },
            { "corners.range.type.check", "never" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "0.173626" },
            { "corners.weight.periodic.amplitude.value", "0.099821" },
            { "corners.weight.periodic.period.mobJitterScale", "0.277355" },
            { "corners.weight.periodic.period.value", "3182.676758" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "0.584403" },
            { "corners.weight.periodic.tickShift.value", "3489.030273" },
            { "corners.weight.value.mobJitterScale", "-0.519206" },
            { "corners.weight.value.value", "7.690554" },
            { "corners.weight.valueType", "periodic" },
            { "creditReserve", "199.446793" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.612410" },
            { "curHeadingWeight.periodic.amplitude.value", "0.743101" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "0.790032" },
            { "curHeadingWeight.periodic.period.value", "3399.420898" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "0.341551" },
            { "curHeadingWeight.periodic.tickShift.value", "10000.000000" },
            { "curHeadingWeight.value.mobJitterScale", "0.166851" },
            { "curHeadingWeight.value.value", "8.312990" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.210496" },
            { "edges.range.radius.periodic.amplitude.value", "-0.674202" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.675476" },
            { "edges.range.radius.periodic.period.value", "380.741638" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.039908" },
            { "edges.range.radius.periodic.tickShift.value", "9209.983398" },
            { "edges.range.radius.value.mobJitterScale", "-0.804072" },
            { "edges.range.radius.value.value", "1540.808594" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.641830" },
            { "edges.crowd.size.periodic.amplitude.value", "0.254569" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "0.254831" },
            { "edges.crowd.size.periodic.period.value", "8265.339844" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.615189" },
            { "edges.crowd.size.periodic.tickShift.value", "6029.361816" },
            { "edges.crowd.size.value.mobJitterScale", "-0.110585" },
            { "edges.crowd.size.value.value", "12.660626" },
            { "edges.crowd.size.valueType", "periodic" },
            { "edges.crowd.type.check", "never" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "0.352999" },
            { "edges.range.radius.periodic.amplitude.value", "0.709915" },
            { "edges.range.radius.periodic.period.mobJitterScale", "0.670024" },
            { "edges.range.radius.periodic.period.value", "8719.189453" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "edges.range.radius.periodic.tickShift.value", "4223.023438" },
            { "edges.range.radius.value.mobJitterScale", "-0.564561" },
            { "edges.range.radius.value.value", "1884.610596" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.range.type.check", "never" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.689310" },
            { "edges.weight.periodic.amplitude.value", "0.786079" },
            { "edges.weight.periodic.period.mobJitterScale", "0.406660" },
            { "edges.weight.periodic.period.value", "9614.454102" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.320217" },
            { "edges.weight.periodic.tickShift.value", "8521.816406" },
            { "edges.weight.value.mobJitterScale", "0.226021" },
            { "edges.weight.value.value", "-7.079600" },
            { "edges.weight.valueType", "periodic" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemy.range.radius.periodic.amplitude.value", "1.000000" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "enemy.range.radius.periodic.period.value", "1835.988281" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.325229" },
            { "enemy.range.radius.periodic.tickShift.value", "9834.126953" },
            { "enemy.range.radius.value.mobJitterScale", "-1.000000" },
            { "enemy.range.radius.value.value", "1055.827515" },
            { "enemy.range.radius.valueType", "periodic" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.690162" },
            { "enemy.crowd.size.periodic.amplitude.value", "0.590042" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.007458" },
            { "enemy.crowd.size.periodic.period.value", "4560.625977" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "0.469067" },
            { "enemy.crowd.size.periodic.tickShift.value", "8191.900391" },
            { "enemy.crowd.size.value.mobJitterScale", "-0.013048" },
            { "enemy.crowd.size.value.value", "16.547121" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "-0.441839" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.886941" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "-0.475719" },
            { "enemy.range.radius.periodic.period.value", "8166.400391" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.802857" },
            { "enemy.range.radius.periodic.tickShift.value", "3639.149902" },
            { "enemy.range.radius.value.mobJitterScale", "-0.622471" },
            { "enemy.range.radius.value.value", "1427.624023" },
            { "enemy.range.radius.valueType", "constant" },
            { "enemy.range.type.check", "never" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "-0.578616" },
            { "enemy.weight.periodic.amplitude.value", "-0.794101" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.248940" },
            { "enemy.weight.periodic.period.value", "7986.520020" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.990605" },
            { "enemy.weight.periodic.tickShift.value", "6584.183105" },
            { "enemy.weight.value.mobJitterScale", "0.387888" },
            { "enemy.weight.value.value", "-10.000000" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "0.355722" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.201086" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.653379" },
            { "enemyBase.range.radius.periodic.period.value", "5540.330078" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.297608" },
            { "enemyBase.range.radius.periodic.tickShift.value", "10000.000000" },
            { "enemyBase.range.radius.value.mobJitterScale", "-0.853353" },
            { "enemyBase.range.radius.value.value", "108.650070" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "0.808495" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.282810" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "0.525005" },
            { "enemyBase.crowd.size.periodic.period.value", "6333.028809" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.086719" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "3438.636719" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.044494" },
            { "enemyBase.crowd.size.value.value", "3.257570" },
            { "enemyBase.crowd.size.valueType", "periodic" },
            { "enemyBase.crowd.type.check", "never" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.800307" },
            { "enemyBase.range.radius.periodic.amplitude.value", "0.122007" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "0.843012" },
            { "enemyBase.range.radius.periodic.period.value", "5151.595215" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.101504" },
            { "enemyBase.range.radius.periodic.tickShift.value", "2373.663086" },
            { "enemyBase.range.radius.value.mobJitterScale", "0.352570" },
            { "enemyBase.range.radius.value.value", "1291.818726" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.range.type.check", "never" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.955141" },
            { "enemyBase.weight.periodic.amplitude.value", "0.902921" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "0.699600" },
            { "enemyBase.weight.periodic.period.value", "4698.849121" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "0.433197" },
            { "enemyBase.weight.periodic.tickShift.value", "8822.717773" },
            { "enemyBase.weight.value.mobJitterScale", "-0.881702" },
            { "enemyBase.weight.value.value", "-7.397503" },
            { "enemyBase.weight.valueType", "constant" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "374.007111" },
            { "evadeStrictDistance", "474.731445" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fleetLocus.circularPeriod", "4729.422852" },
            { "fleetLocus.circularWeight", "1.467787" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.049298" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.023914" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "0.577136" },
            { "fleetLocus.force.range.radius.periodic.period.value", "4158.054688" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.840494" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "3528.776367" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.641586" },
            { "fleetLocus.force.range.radius.value.value", "1556.019409" },
            { "fleetLocus.force.range.radius.valueType", "constant" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.084173" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.598220" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.136517" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "9573.745117" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.020576" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "999.033630" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "-0.109607" },
            { "fleetLocus.force.crowd.size.value.value", "9.762760" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "never" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.282212" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "-0.508336" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.216779" },
            { "fleetLocus.force.range.radius.periodic.period.value", "8087.954102" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.418626" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "5650.093750" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "-0.482937" },
            { "fleetLocus.force.range.radius.value.value", "1421.532227" },
            { "fleetLocus.force.range.radius.valueType", "constant" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.241385" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.826423" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-0.982074" },
            { "fleetLocus.force.weight.periodic.period.value", "467.154114" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.350996" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "3463.577881" },
            { "fleetLocus.force.weight.value.mobJitterScale", "-0.238768" },
            { "fleetLocus.force.weight.value.value", "4.056369" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "1.229860" },
            { "fleetLocus.linearXPeriod", "3738.535889" },
            { "fleetLocus.linearYPeriod", "574.221558" },
            { "fleetLocus.randomPeriod", "1665.636719" },
            { "fleetLocus.randomWeight", "1.909164" },
            { "fleetLocus.useScaled", "TRUE" },
            { "flockDuringAttack.forceOn", "FALSE" },
            { "gatherAbandonStale", "FALSE" },
            { "gatherRange", "50.508949" },
            { "guardRange", "114.359146" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.109069" },
            { "mobLocus.circularPeriod.value", "7716.700195" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "-0.329960" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "0.122753" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "-0.977058" },
            { "mobLocus.circularWeight.periodic.period.value", "-0.900000" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.933708" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "1756.586548" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.337608" },
            { "mobLocus.circularWeight.value.value", "-7.789859" },
            { "mobLocus.circularWeight.valueType", "constant" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.883554" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "-0.352387" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.877645" },
            { "mobLocus.force.range.radius.periodic.period.value", "2471.635498" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.325888" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "3006.935547" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "-1.000000" },
            { "mobLocus.force.range.radius.value.value", "1633.973999" },
            { "mobLocus.force.range.radius.valueType", "periodic" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.673646" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-0.113365" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.302516" },
            { "mobLocus.force.crowd.size.periodic.period.value", "3886.288818" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.706614" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "7871.564941" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "0.742196" },
            { "mobLocus.force.crowd.size.value.value", "13.149846" },
            { "mobLocus.force.crowd.size.valueType", "constant" },
            { "mobLocus.force.crowd.type.check", "never" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.783651" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "-0.128708" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.439087" },
            { "mobLocus.force.range.radius.periodic.period.value", "7645.979980" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.016458" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "-0.900000" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.810088" },
            { "mobLocus.force.range.radius.value.value", "1343.021606" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.range.type.check", "never" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.596578" },
            { "mobLocus.force.weight.periodic.amplitude.value", "0.569913" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "-0.875495" },
            { "mobLocus.force.weight.periodic.period.value", "2167.951172" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "mobLocus.force.weight.periodic.tickShift.value", "-1.000000" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.049641" },
            { "mobLocus.force.weight.value.value", "-5.225343" },
            { "mobLocus.force.weight.valueType", "periodic" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "0.484137" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.900000" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.542633" },
            { "mobLocus.linearWeight.periodic.period.value", "4279.568359" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "-0.651945" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "2276.423584" },
            { "mobLocus.linearWeight.value.mobJitterScale", "0.013693" },
            { "mobLocus.linearWeight.value.value", "-1.879192" },
            { "mobLocus.linearWeight.valueType", "periodic" },
            { "mobLocus.linearXPeriod.mobJitterScale", "-0.566414" },
            { "mobLocus.linearXPeriod.value", "-1.000000" },
            { "mobLocus.linearYPeriod.mobJitterScale", "-0.403415" },
            { "mobLocus.linearYPeriod.value", "643.469604" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "0.359918" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.657007" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "-0.741747" },
            { "mobLocus.proximityRadius.periodic.period.value", "6303.981445" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "-0.706177" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "6290.159180" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "-0.990000" },
            { "mobLocus.proximityRadius.value.value", "436.962189" },
            { "mobLocus.proximityRadius.valueType", "constant" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.045224" },
            { "mobLocus.randomPeriod.value", "1575.671387" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "-0.341122" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "-0.567966" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "0.850672" },
            { "mobLocus.randomWeight.periodic.period.value", "4014.914551" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "6081.969727" },
            { "mobLocus.randomWeight.value.mobJitterScale", "0.684562" },
            { "mobLocus.randomWeight.value.value", "8.373062" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "FALSE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "419.229462" },
            { "nearBaseRandomIdle.forceOn", "TRUE" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.894288" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "-0.900000" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "-0.193002" },
            { "nearestFriend.range.radius.periodic.period.value", "8103.985840" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.941075" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "752.274658" },
            { "nearestFriend.range.radius.value.mobJitterScale", "-0.153269" },
            { "nearestFriend.range.radius.value.value", "490.943604" },
            { "nearestFriend.range.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "0.778496" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "0.636534" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "0.188343" },
            { "nearestFriend.crowd.size.periodic.period.value", "6677.261719" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "-0.232387" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "0.748928" },
            { "nearestFriend.crowd.size.value.value", "3.021099" },
            { "nearestFriend.crowd.size.valueType", "periodic" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.281290" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "-0.618121" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "-0.472305" },
            { "nearestFriend.range.radius.periodic.period.value", "5906.472168" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.818305" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "1229.078247" },
            { "nearestFriend.range.radius.value.mobJitterScale", "0.317248" },
            { "nearestFriend.range.radius.value.value", "-0.980100" },
            { "nearestFriend.range.radius.valueType", "periodic" },
            { "nearestFriend.range.type.check", "never" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "0.895023" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.093568" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "-0.257354" },
            { "nearestFriend.weight.periodic.period.value", "9885.971680" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "0.801804" },
            { "nearestFriend.weight.periodic.tickShift.value", "3769.436035" },
            { "nearestFriend.weight.value.mobJitterScale", "-0.104346" },
            { "nearestFriend.weight.value.value", "-8.210824" },
            { "nearestFriend.weight.valueType", "constant" },
            { "randomIdle.forceOn", "FALSE" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "16.061089" },
            { "sensorGrid.staleFighterTime", "1.707165" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.860380" },
            { "separate.range.radius.periodic.amplitude.value", "-0.306259" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.265697" },
            { "separate.range.radius.periodic.period.value", "8421.583984" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.169477" },
            { "separate.range.radius.periodic.tickShift.value", "4562.610840" },
            { "separate.range.radius.value.mobJitterScale", "0.011466" },
            { "separate.range.radius.value.value", "180.239548" },
            { "separate.range.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "0.178016" },
            { "separate.crowd.size.periodic.amplitude.value", "-0.492456" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "0.309855" },
            { "separate.crowd.size.periodic.period.value", "2450.952637" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "-0.243912" },
            { "separate.crowd.size.periodic.tickShift.value", "10000.000000" },
            { "separate.crowd.size.value.mobJitterScale", "0.683274" },
            { "separate.crowd.size.value.value", "14.971319" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowd.type.check", "never" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.261785" },
            { "separate.range.radius.periodic.amplitude.value", "-0.675898" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.024811" },
            { "separate.range.radius.periodic.period.value", "1811.048096" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "0.374472" },
            { "separate.range.radius.periodic.tickShift.value", "4564.070801" },
            { "separate.range.radius.value.mobJitterScale", "0.876786" },
            { "separate.range.radius.value.value", "844.060608" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.range.type.check", "never" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "0.253169" },
            { "separate.weight.periodic.amplitude.value", "-0.243346" },
            { "separate.weight.periodic.period.mobJitterScale", "-0.368770" },
            { "separate.weight.periodic.period.value", "6887.109863" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.737134" },
            { "separate.weight.periodic.tickShift.value", "8153.114258" },
            { "separate.weight.value.mobJitterScale", "-0.116910" },
            { "separate.weight.value.value", "1.235031" },
            { "separate.weight.valueType", "constant" },
            { "simpleAttack.forceOn", "TRUE" },
            { "startingMaxRadius", "1551.391479" },
            { "startingMinRadius", "416.119781" },
        };

        BundleConfigValue configs7[] = {
            { "align.range.radius.periodic.amplitude.mobJitterScale", "0.653555" },
            { "align.range.radius.periodic.amplitude.value", "0.652492" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.596374" },
            { "align.range.radius.periodic.period.value", "9913.634766" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.740750" },
            { "align.range.radius.periodic.tickShift.value", "7700.037598" },
            { "align.range.radius.value.mobJitterScale", "0.329074" },
            { "align.range.radius.value.value", "2000.000000" },
            { "align.range.radius.valueType", "periodic" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "-0.086237" },
            { "align.crowd.size.periodic.amplitude.value", "-0.606958" },
            { "align.crowd.size.periodic.period.mobJitterScale", "0.834362" },
            { "align.crowd.size.periodic.period.value", "8910.000000" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.410857" },
            { "align.crowd.size.periodic.tickShift.value", "2184.366455" },
            { "align.crowd.size.value.mobJitterScale", "-0.703907" },
            { "align.crowd.size.value.value", "6.262674" },
            { "align.crowd.size.valueType", "constant" },
            { "align.crowd.type.check", "never" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.375990" },
            { "align.range.radius.periodic.amplitude.value", "-0.390739" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.130139" },
            { "align.range.radius.periodic.period.value", "10000.000000" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "-0.776108" },
            { "align.range.radius.periodic.tickShift.value", "7148.439941" },
            { "align.range.radius.value.mobJitterScale", "-0.246045" },
            { "align.range.radius.value.value", "1046.518433" },
            { "align.range.radius.valueType", "constant" },
            { "align.range.type.check", "never" },
            { "align.weight.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "align.weight.periodic.amplitude.value", "-0.280895" },
            { "align.weight.periodic.period.mobJitterScale", "-0.522718" },
            { "align.weight.periodic.period.value", "6564.090820" },
            { "align.weight.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "align.weight.periodic.tickShift.value", "1652.486084" },
            { "align.weight.value.mobJitterScale", "0.302895" },
            { "align.weight.value.value", "5.632702" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "140.791916" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.149401" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.951722" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "-0.632135" },
            { "attackSeparate.range.radius.periodic.period.value", "5261.521484" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.804713" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "1196.707153" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.371070" },
            { "attackSeparate.range.radius.value.value", "1587.109619" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "0.635782" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.125812" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.233674" },
            { "attackSeparate.crowd.size.periodic.period.value", "5305.370605" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "0.310014" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "7329.817871" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "0.817303" },
            { "attackSeparate.crowd.size.value.value", "20.000000" },
            { "attackSeparate.crowd.size.valueType", "periodic" },
            { "attackSeparate.crowd.type.check", "never" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.615626" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.900000" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.093444" },
            { "attackSeparate.range.radius.periodic.period.value", "4610.033691" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "-0.099879" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "8042.772949" },
            { "attackSeparate.range.radius.value.mobJitterScale", "0.203820" },
            { "attackSeparate.range.radius.value.value", "809.848755" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.range.type.check", "never" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.977177" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.005607" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.720948" },
            { "attackSeparate.weight.periodic.period.value", "2085.546631" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-0.900000" },
            { "attackSeparate.weight.periodic.tickShift.value", "3566.423096" },
            { "attackSeparate.weight.value.mobJitterScale", "-0.213618" },
            { "attackSeparate.weight.value.value", "3.635406" },
            { "attackSeparate.weight.valueType", "constant" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.741534" },
            { "base.range.radius.periodic.amplitude.value", "0.582580" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.516584" },
            { "base.range.radius.periodic.period.value", "8186.723633" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "-0.875177" },
            { "base.range.radius.periodic.tickShift.value", "5995.842773" },
            { "base.range.radius.value.mobJitterScale", "-0.452220" },
            { "base.range.radius.value.value", "1740.667725" },
            { "base.range.radius.valueType", "periodic" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.085635" },
            { "base.crowd.size.periodic.amplitude.value", "-0.699359" },
            { "base.crowd.size.periodic.period.mobJitterScale", "-0.536972" },
            { "base.crowd.size.periodic.period.value", "8054.147949" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "-0.473837" },
            { "base.crowd.size.periodic.tickShift.value", "4646.277344" },
            { "base.crowd.size.value.mobJitterScale", "-0.022306" },
            { "base.crowd.size.value.value", "6.174621" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "never" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.510039" },
            { "base.range.radius.periodic.amplitude.value", "0.746824" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.767744" },
            { "base.range.radius.periodic.period.value", "-1.000000" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "-0.127995" },
            { "base.range.radius.periodic.tickShift.value", "2971.962402" },
            { "base.range.radius.value.mobJitterScale", "-0.602608" },
            { "base.range.radius.value.value", "894.427368" },
            { "base.range.radius.valueType", "constant" },
            { "base.range.type.check", "never" },
            { "base.weight.periodic.amplitude.mobJitterScale", "-0.118068" },
            { "base.weight.periodic.amplitude.value", "-1.000000" },
            { "base.weight.periodic.period.mobJitterScale", "-0.595249" },
            { "base.weight.periodic.period.value", "3581.316650" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.767415" },
            { "base.weight.periodic.tickShift.value", "-1.000000" },
            { "base.weight.value.mobJitterScale", "0.160237" },
            { "base.weight.value.value", "-9.562931" },
            { "base.weight.valueType", "constant" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "0.446866" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.689858" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.960214" },
            { "baseDefense.range.radius.periodic.period.value", "4945.483887" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.492756" },
            { "baseDefense.range.radius.periodic.tickShift.value", "1323.097290" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.221905" },
            { "baseDefense.range.radius.value.value", "190.752914" },
            { "baseDefense.range.radius.valueType", "periodic" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.992570" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "-0.760573" },
            { "baseDefense.crowd.size.periodic.period.value", "9096.071289" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "-0.547265" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "10000.000000" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.438710" },
            { "baseDefense.crowd.size.value.value", "14.477304" },
            { "baseDefense.crowd.size.valueType", "constant" },
            { "baseDefense.crowd.type.check", "never" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "0.365004" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.256863" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.087917" },
            { "baseDefense.range.radius.periodic.period.value", "5346.260254" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "0.900000" },
            { "baseDefense.range.radius.periodic.tickShift.value", "4674.336914" },
            { "baseDefense.range.radius.value.mobJitterScale", "-0.900000" },
            { "baseDefense.range.radius.value.value", "894.895386" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "never" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-0.883355" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.973474" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "-0.957773" },
            { "baseDefense.weight.periodic.period.value", "5836.812012" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "-0.326712" },
            { "baseDefense.weight.periodic.tickShift.value", "5917.968262" },
            { "baseDefense.weight.value.mobJitterScale", "0.493242" },
            { "baseDefense.weight.value.value", "-5.789010" },
            { "baseDefense.weight.valueType", "constant" },
            { "baseDefenseRadius", "492.155304" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.618629" },
            { "center.range.radius.periodic.amplitude.value", "-0.824285" },
            { "center.range.radius.periodic.period.mobJitterScale", "0.672028" },
            { "center.range.radius.periodic.period.value", "1875.173950" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.412306" },
            { "center.range.radius.periodic.tickShift.value", "7709.014160" },
            { "center.range.radius.value.mobJitterScale", "-0.276397" },
            { "center.range.radius.value.value", "2000.000000" },
            { "center.range.radius.valueType", "constant" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.900000" },
            { "center.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "center.crowd.size.periodic.period.value", "7373.351562" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "-0.251719" },
            { "center.crowd.size.periodic.tickShift.value", "3245.986816" },
            { "center.crowd.size.value.mobJitterScale", "0.651465" },
            { "center.crowd.size.value.value", "3.384829" },
            { "center.crowd.size.valueType", "constant" },
            { "center.crowd.type.check", "never" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "0.445817" },
            { "center.range.radius.periodic.amplitude.value", "-0.529666" },
            { "center.range.radius.periodic.period.mobJitterScale", "0.649338" },
            { "center.range.radius.periodic.period.value", "5419.169922" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.522481" },
            { "center.range.radius.periodic.tickShift.value", "3851.185059" },
            { "center.range.radius.value.mobJitterScale", "0.809273" },
            { "center.range.radius.value.value", "796.595825" },
            { "center.range.radius.valueType", "constant" },
            { "center.range.type.check", "never" },
            { "center.weight.periodic.amplitude.mobJitterScale", "-0.157502" },
            { "center.weight.periodic.amplitude.value", "0.494549" },
            { "center.weight.periodic.period.mobJitterScale", "-0.794225" },
            { "center.weight.periodic.period.value", "3198.065918" },
            { "center.weight.periodic.tickShift.mobJitterScale", "0.106912" },
            { "center.weight.periodic.tickShift.value", "356.957245" },
            { "center.weight.value.mobJitterScale", "-0.052648" },
            { "center.weight.value.value", "-10.000000" },
            { "center.weight.valueType", "periodic" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "cohere.range.radius.periodic.amplitude.value", "0.419012" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "0.378379" },
            { "cohere.range.radius.periodic.period.value", "-1.000000" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "0.002584" },
            { "cohere.range.radius.periodic.tickShift.value", "1043.144409" },
            { "cohere.range.radius.value.mobJitterScale", "-0.087265" },
            { "cohere.range.radius.value.value", "778.665344" },
            { "cohere.range.radius.valueType", "constant" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.876736" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.956534" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "-0.977475" },
            { "cohere.crowd.size.periodic.period.value", "1213.261230" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "-0.217514" },
            { "cohere.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "cohere.crowd.size.value.mobJitterScale", "0.498172" },
            { "cohere.crowd.size.value.value", "-0.900000" },
            { "cohere.crowd.size.valueType", "constant" },
            { "cohere.crowd.type.check", "never" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.773034" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.473515" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "0.618839" },
            { "cohere.range.radius.periodic.period.value", "989.001953" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.040459" },
            { "cohere.range.radius.periodic.tickShift.value", "7662.532227" },
            { "cohere.range.radius.value.mobJitterScale", "0.105630" },
            { "cohere.range.radius.value.value", "1517.630005" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "never" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "0.224473" },
            { "cohere.weight.periodic.amplitude.value", "0.415524" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.402602" },
            { "cohere.weight.periodic.period.value", "3358.028564" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "cohere.weight.periodic.tickShift.value", "4971.724609" },
            { "cohere.weight.value.mobJitterScale", "0.456072" },
            { "cohere.weight.value.value", "5.179855" },
            { "cohere.weight.valueType", "constant" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "-0.297693" },
            { "cores.range.radius.periodic.amplitude.value", "0.181148" },
            { "cores.range.radius.periodic.period.mobJitterScale", "0.057994" },
            { "cores.range.radius.periodic.period.value", "6757.915039" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "-0.973643" },
            { "cores.range.radius.periodic.tickShift.value", "10000.000000" },
            { "cores.range.radius.value.mobJitterScale", "-0.534257" },
            { "cores.range.radius.value.value", "572.910583" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "-0.465735" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.512510" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-0.392788" },
            { "cores.crowd.size.periodic.period.value", "3356.451172" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.356109" },
            { "cores.crowd.size.periodic.tickShift.value", "1.000000" },
            { "cores.crowd.size.value.mobJitterScale", "-1.000000" },
            { "cores.crowd.size.value.value", "20.000000" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowd.type.check", "never" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.561953" },
            { "cores.range.radius.periodic.amplitude.value", "-0.459170" },
            { "cores.range.radius.periodic.period.mobJitterScale", "0.120379" },
            { "cores.range.radius.periodic.period.value", "6655.360352" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "-0.579671" },
            { "cores.range.radius.periodic.tickShift.value", "5619.370117" },
            { "cores.range.radius.value.mobJitterScale", "0.839069" },
            { "cores.range.radius.value.value", "1802.905029" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.range.type.check", "never" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "cores.weight.periodic.amplitude.value", "-0.286850" },
            { "cores.weight.periodic.period.mobJitterScale", "0.841076" },
            { "cores.weight.periodic.period.value", "9976.324219" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.227018" },
            { "cores.weight.periodic.tickShift.value", "1175.575806" },
            { "cores.weight.value.mobJitterScale", "-0.278818" },
            { "cores.weight.value.value", "-7.040056" },
            { "cores.weight.valueType", "periodic" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.545276" },
            { "corners.range.radius.periodic.amplitude.value", "-0.667573" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-0.494348" },
            { "corners.range.radius.periodic.period.value", "3729.355225" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "corners.range.radius.periodic.tickShift.value", "9298.952148" },
            { "corners.range.radius.value.mobJitterScale", "-0.023283" },
            { "corners.range.radius.value.value", "881.891785" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "0.405042" },
            { "corners.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "-0.107933" },
            { "corners.crowd.size.periodic.period.value", "4559.743164" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "-0.397402" },
            { "corners.crowd.size.periodic.tickShift.value", "2908.010010" },
            { "corners.crowd.size.value.mobJitterScale", "0.145480" },
            { "corners.crowd.size.value.value", "1.631990" },
            { "corners.crowd.size.valueType", "periodic" },
            { "corners.crowd.type.check", "never" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.292849" },
            { "corners.range.radius.periodic.amplitude.value", "0.575863" },
            { "corners.range.radius.periodic.period.mobJitterScale", "-0.237989" },
            { "corners.range.radius.periodic.period.value", "9375.089844" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "corners.range.radius.periodic.tickShift.value", "10000.000000" },
            { "corners.range.radius.value.mobJitterScale", "0.351557" },
            { "corners.range.radius.value.value", "1190.916992" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.range.type.check", "never" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "0.724457" },
            { "corners.weight.periodic.amplitude.value", "0.867358" },
            { "corners.weight.periodic.period.mobJitterScale", "-0.600776" },
            { "corners.weight.periodic.period.value", "9607.842773" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "-0.836337" },
            { "corners.weight.periodic.tickShift.value", "518.583557" },
            { "corners.weight.value.mobJitterScale", "-1.000000" },
            { "corners.weight.value.value", "-1.026620" },
            { "corners.weight.valueType", "periodic" },
            { "creditReserve", "199.446793" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "-0.161010" },
            { "curHeadingWeight.periodic.amplitude.value", "1.000000" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "-0.196846" },
            { "curHeadingWeight.periodic.period.value", "6990.240723" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "-0.160748" },
            { "curHeadingWeight.periodic.tickShift.value", "3711.736084" },
            { "curHeadingWeight.value.mobJitterScale", "0.467929" },
            { "curHeadingWeight.value.value", "1.224227" },
            { "curHeadingWeight.valueType", "constant" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.264377" },
            { "edges.range.radius.periodic.amplitude.value", "-0.328272" },
            { "edges.range.radius.periodic.period.mobJitterScale", "0.073687" },
            { "edges.range.radius.periodic.period.value", "7369.365723" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.399379" },
            { "edges.range.radius.periodic.tickShift.value", "1881.367310" },
            { "edges.range.radius.value.mobJitterScale", "0.243162" },
            { "edges.range.radius.value.value", "1011.215576" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "0.292605" },
            { "edges.crowd.size.periodic.amplitude.value", "0.204823" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "-0.477338" },
            { "edges.crowd.size.periodic.period.value", "4172.226562" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.797890" },
            { "edges.crowd.size.periodic.tickShift.value", "5712.184082" },
            { "edges.crowd.size.value.mobJitterScale", "0.433648" },
            { "edges.crowd.size.value.value", "4.759602" },
            { "edges.crowd.size.valueType", "periodic" },
            { "edges.crowd.type.check", "never" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "0.179700" },
            { "edges.range.radius.periodic.amplitude.value", "0.426636" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.398081" },
            { "edges.range.radius.periodic.period.value", "8295.972656" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "-0.900000" },
            { "edges.range.radius.periodic.tickShift.value", "76.200035" },
            { "edges.range.radius.value.mobJitterScale", "-0.564461" },
            { "edges.range.radius.value.value", "316.399292" },
            { "edges.range.radius.valueType", "periodic" },
            { "edges.range.type.check", "never" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "0.181350" },
            { "edges.weight.periodic.amplitude.value", "0.320682" },
            { "edges.weight.periodic.period.mobJitterScale", "1.000000" },
            { "edges.weight.periodic.period.value", "5401.647949" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "-0.176578" },
            { "edges.weight.periodic.tickShift.value", "5226.207520" },
            { "edges.weight.value.mobJitterScale", "0.310295" },
            { "edges.weight.value.value", "-0.503198" },
            { "edges.weight.valueType", "periodic" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.487030" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.557261" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "-0.327608" },
            { "enemy.range.radius.periodic.period.value", "1618.205078" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.558650" },
            { "enemy.range.radius.periodic.tickShift.value", "8219.541992" },
            { "enemy.range.radius.value.mobJitterScale", "-0.344907" },
            { "enemy.range.radius.value.value", "302.855103" },
            { "enemy.range.radius.valueType", "constant" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "0.378997" },
            { "enemy.crowd.size.periodic.amplitude.value", "0.810000" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "-0.731052" },
            { "enemy.crowd.size.periodic.period.value", "4392.489258" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "0.026471" },
            { "enemy.crowd.size.periodic.tickShift.value", "5983.848145" },
            { "enemy.crowd.size.value.mobJitterScale", "-0.221733" },
            { "enemy.crowd.size.value.value", "12.785927" },
            { "enemy.crowd.size.valueType", "periodic" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.015732" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.643457" },
            { "enemy.range.radius.periodic.period.value", "2120.450928" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "0.155305" },
            { "enemy.range.radius.periodic.tickShift.value", "6946.404785" },
            { "enemy.range.radius.value.mobJitterScale", "1.000000" },
            { "enemy.range.radius.value.value", "-1.000000" },
            { "enemy.range.radius.valueType", "constant" },
            { "enemy.range.type.check", "never" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "-0.555589" },
            { "enemy.weight.periodic.amplitude.value", "0.822338" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.150284" },
            { "enemy.weight.periodic.period.value", "5666.407715" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.499106" },
            { "enemy.weight.periodic.tickShift.value", "9570.649414" },
            { "enemy.weight.value.mobJitterScale", "-1.000000" },
            { "enemy.weight.value.value", "1.406536" },
            { "enemy.weight.valueType", "periodic" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.061686" },
            { "enemyBase.range.radius.periodic.amplitude.value", "0.619051" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.838136" },
            { "enemyBase.range.radius.periodic.period.value", "6577.403809" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.105226" },
            { "enemyBase.range.radius.periodic.tickShift.value", "6348.147461" },
            { "enemyBase.range.radius.value.mobJitterScale", "-0.920971" },
            { "enemyBase.range.radius.value.value", "1237.082153" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "0.764764" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.679928" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.144929" },
            { "enemyBase.crowd.size.periodic.period.value", "9315.338867" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.069022" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "9000.000000" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.005402" },
            { "enemyBase.crowd.size.value.value", "5.717134" },
            { "enemyBase.crowd.size.valueType", "constant" },
            { "enemyBase.crowd.type.check", "never" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.503440" },
            { "enemyBase.range.radius.periodic.amplitude.value", "0.540863" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.289706" },
            { "enemyBase.range.radius.periodic.period.value", "10000.000000" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "-0.298801" },
            { "enemyBase.range.radius.periodic.tickShift.value", "2433.526123" },
            { "enemyBase.range.radius.value.mobJitterScale", "0.538608" },
            { "enemyBase.range.radius.value.value", "160.581238" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.range.type.check", "never" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.596198" },
            { "enemyBase.weight.periodic.amplitude.value", "-0.049614" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "-0.890727" },
            { "enemyBase.weight.periodic.period.value", "8871.845703" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "-0.490103" },
            { "enemyBase.weight.periodic.tickShift.value", "8997.929688" },
            { "enemyBase.weight.value.mobJitterScale", "0.462898" },
            { "enemyBase.weight.value.value", "5.287519" },
            { "enemyBase.weight.valueType", "constant" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "454.318359" },
            { "evadeStrictDistance", "476.019104" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fleetLocus.circularPeriod", "3045.315674" },
            { "fleetLocus.circularWeight", "2.000000" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.198688" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.334394" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.370963" },
            { "fleetLocus.force.range.radius.periodic.period.value", "7651.486816" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.379651" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "8593.720703" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.745147" },
            { "fleetLocus.force.range.radius.value.value", "1264.364746" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.466474" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.821173" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.171943" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "1798.440796" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.892271" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "6694.617188" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "-0.993288" },
            { "fleetLocus.force.crowd.size.value.value", "2.672786" },
            { "fleetLocus.force.crowd.size.valueType", "constant" },
            { "fleetLocus.force.crowd.type.check", "never" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.804319" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.060413" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.618225" },
            { "fleetLocus.force.range.radius.periodic.period.value", "8286.911133" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.059195" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "8401.980469" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "-0.794933" },
            { "fleetLocus.force.range.radius.value.value", "1631.495483" },
            { "fleetLocus.force.range.radius.valueType", "constant" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "0.533270" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "0.665126" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "0.900000" },
            { "fleetLocus.force.weight.periodic.period.value", "3659.553955" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.613735" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "10000.000000" },
            { "fleetLocus.force.weight.value.mobJitterScale", "-0.152931" },
            { "fleetLocus.force.weight.value.value", "-4.020475" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "0.636558" },
            { "fleetLocus.linearXPeriod", "3074.656494" },
            { "fleetLocus.linearYPeriod", "11079.780273" },
            { "fleetLocus.randomPeriod", "483.338409" },
            { "fleetLocus.randomWeight", "2.000000" },
            { "fleetLocus.useScaled", "FALSE" },
            { "flockDuringAttack.forceOn", "FALSE" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "51.465023" },
            { "guardRange", "93.715591" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.812572" },
            { "mobLocus.circularPeriod.value", "9074.688477" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "-0.363630" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "-0.906784" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "-0.661723" },
            { "mobLocus.circularWeight.periodic.period.value", "8967.759766" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.891130" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "1102.419922" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.248533" },
            { "mobLocus.circularWeight.value.value", "2.330949" },
            { "mobLocus.circularWeight.valueType", "periodic" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.416112" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.350509" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.342188" },
            { "mobLocus.force.range.radius.periodic.period.value", "3973.057861" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.022501" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "8258.509766" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "-0.052231" },
            { "mobLocus.force.range.radius.value.value", "749.637512" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.698844" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "0.651082" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.323917" },
            { "mobLocus.force.crowd.size.periodic.period.value", "8562.939453" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.853180" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "7616.838379" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.479161" },
            { "mobLocus.force.crowd.size.value.value", "15.933333" },
            { "mobLocus.force.crowd.size.valueType", "constant" },
            { "mobLocus.force.crowd.type.check", "never" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.158173" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "-0.791974" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "0.183857" },
            { "mobLocus.force.range.radius.periodic.period.value", "2880.281250" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "8523.562500" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.650785" },
            { "mobLocus.force.range.radius.value.value", "407.114441" },
            { "mobLocus.force.range.radius.valueType", "periodic" },
            { "mobLocus.force.range.type.check", "never" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.460919" },
            { "mobLocus.force.weight.periodic.amplitude.value", "-0.705624" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.394636" },
            { "mobLocus.force.weight.periodic.period.value", "3886.536865" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.206026" },
            { "mobLocus.force.weight.periodic.tickShift.value", "6408.808105" },
            { "mobLocus.force.weight.value.mobJitterScale", "-0.474051" },
            { "mobLocus.force.weight.value.value", "7.363282" },
            { "mobLocus.force.weight.valueType", "constant" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "0.149191" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.403242" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "1.000000" },
            { "mobLocus.linearWeight.periodic.period.value", "4178.346680" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "-0.543533" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "4172.028809" },
            { "mobLocus.linearWeight.value.mobJitterScale", "0.425209" },
            { "mobLocus.linearWeight.value.value", "-10.000000" },
            { "mobLocus.linearWeight.valueType", "constant" },
            { "mobLocus.linearXPeriod.mobJitterScale", "-1.000000" },
            { "mobLocus.linearXPeriod.value", "5294.217773" },
            { "mobLocus.linearYPeriod.mobJitterScale", "0.855229" },
            { "mobLocus.linearYPeriod.value", "1.000000" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "-0.314947" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "0.292024" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "-0.900000" },
            { "mobLocus.proximityRadius.periodic.period.value", "2317.701416" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "0.622622" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "4486.188965" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "-1.000000" },
            { "mobLocus.proximityRadius.value.value", "1222.230957" },
            { "mobLocus.proximityRadius.valueType", "constant" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.630176" },
            { "mobLocus.randomPeriod.value", "2670.529053" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "-0.013516" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "-0.943511" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "-1.000000" },
            { "mobLocus.randomWeight.periodic.period.value", "7765.522461" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "0.886394" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "1206.838989" },
            { "mobLocus.randomWeight.value.mobJitterScale", "-1.000000" },
            { "mobLocus.randomWeight.value.value", "9.038343" },
            { "mobLocus.randomWeight.valueType", "periodic" },
            { "mobLocus.resetOnProximity", "FALSE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "300.801270" },
            { "nearBaseRandomIdle.forceOn", "FALSE" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.475914" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.263599" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "0.377700" },
            { "nearestFriend.range.radius.periodic.period.value", "10000.000000" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.588540" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.range.radius.value.mobJitterScale", "-0.438807" },
            { "nearestFriend.range.radius.value.value", "208.059341" },
            { "nearestFriend.range.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "0.685140" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "0.426429" },
            { "nearestFriend.crowd.size.periodic.period.value", "5029.460938" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "-0.561473" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "20.144215" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "0.444308" },
            { "nearestFriend.crowd.size.value.value", "4.566016" },
            { "nearestFriend.crowd.size.valueType", "periodic" },
            { "nearestFriend.crowd.type.check", "never" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "-0.501191" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "0.465727" },
            { "nearestFriend.range.radius.periodic.period.value", "5543.703613" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.787756" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "5758.913574" },
            { "nearestFriend.range.radius.value.mobJitterScale", "-1.000000" },
            { "nearestFriend.range.radius.value.value", "1267.734497" },
            { "nearestFriend.range.radius.valueType", "constant" },
            { "nearestFriend.range.type.check", "never" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "0.329950" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.784646" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "-0.578832" },
            { "nearestFriend.weight.periodic.period.value", "1809.160889" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "0.566433" },
            { "nearestFriend.weight.periodic.tickShift.value", "875.554382" },
            { "nearestFriend.weight.value.mobJitterScale", "0.095431" },
            { "nearestFriend.weight.value.value", "0.470555" },
            { "nearestFriend.weight.valueType", "periodic" },
            { "randomIdle.forceOn", "FALSE" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "40.025291" },
            { "sensorGrid.staleFighterTime", "0.668698" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "-0.088320" },
            { "separate.range.radius.periodic.amplitude.value", "-0.076864" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.422677" },
            { "separate.range.radius.periodic.period.value", "3548.283936" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.494341" },
            { "separate.range.radius.periodic.tickShift.value", "673.003906" },
            { "separate.range.radius.value.mobJitterScale", "-0.721141" },
            { "separate.range.radius.value.value", "596.943848" },
            { "separate.range.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "0.563459" },
            { "separate.crowd.size.periodic.amplitude.value", "0.145833" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "0.631504" },
            { "separate.crowd.size.periodic.period.value", "4378.684082" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "-0.081295" },
            { "separate.crowd.size.periodic.tickShift.value", "4538.283203" },
            { "separate.crowd.size.value.mobJitterScale", "0.658728" },
            { "separate.crowd.size.value.value", "5.539504" },
            { "separate.crowd.size.valueType", "periodic" },
            { "separate.crowd.type.check", "never" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "separate.range.radius.periodic.amplitude.value", "0.936464" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.174253" },
            { "separate.range.radius.periodic.period.value", "8491.372070" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "separate.range.radius.periodic.tickShift.value", "5407.598633" },
            { "separate.range.radius.value.mobJitterScale", "0.834946" },
            { "separate.range.radius.value.value", "968.393311" },
            { "separate.range.radius.valueType", "constant" },
            { "separate.range.type.check", "never" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "0.693811" },
            { "separate.weight.periodic.amplitude.value", "-0.025236" },
            { "separate.weight.periodic.period.mobJitterScale", "-0.347077" },
            { "separate.weight.periodic.period.value", "9780.836914" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "0.612534" },
            { "separate.weight.periodic.tickShift.value", "10000.000000" },
            { "separate.weight.value.mobJitterScale", "0.563146" },
            { "separate.weight.value.value", "9.006775" },
            { "separate.weight.valueType", "constant" },
            { "simpleAttack.forceOn", "TRUE" },
            { "startingMaxRadius", "1717.478638" },
            { "startingMinRadius", "353.757050" },
         };

        BundleConfigValue configs8[] = {
            { "align.crowd.radius.periodic.amplitude.mobJitterScale", "-0.723838" },
            { "align.crowd.radius.periodic.amplitude.value", "-1.000000" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "-0.357471" },
            { "align.crowd.radius.periodic.period.value", "8536.285156" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.510174" },
            { "align.crowd.radius.periodic.tickShift.value", "1230.857788" },
            { "align.crowd.radius.value.mobJitterScale", "0.832103" },
            { "align.crowd.radius.value.value", "1034.234253" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.573010" },
            { "align.crowd.size.periodic.amplitude.value", "-0.303864" },
            { "align.crowd.size.periodic.period.mobJitterScale", "0.075461" },
            { "align.crowd.size.periodic.period.value", "7009.017578" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.250006" },
            { "align.crowd.size.periodic.tickShift.value", "6240.475586" },
            { "align.crowd.size.value.mobJitterScale", "0.474770" },
            { "align.crowd.size.value.value", "9.806349" },
            { "align.crowd.type.check", "linearUp" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.437430" },
            { "align.range.radius.periodic.amplitude.value", "-0.493559" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.363674" },
            { "align.range.radius.periodic.period.value", "2891.146240" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.874177" },
            { "align.range.radius.periodic.tickShift.value", "-1.000000" },
            { "align.range.radius.value.mobJitterScale", "0.216686" },
            { "align.range.radius.value.value", "736.540527" },
            { "align.range.radius.valueType", "constant" },
            { "align.range.type.check", "linearDown" },
            { "align.weight.periodic.amplitude.mobJitterScale", "-0.397002" },
            { "align.weight.periodic.amplitude.value", "-0.053077" },
            { "align.weight.periodic.period.mobJitterScale", "0.329414" },
            { "align.weight.periodic.period.value", "8311.178711" },
            { "align.weight.periodic.tickShift.mobJitterScale", "-0.954250" },
            { "align.weight.periodic.tickShift.value", "1734.257568" },
            { "align.weight.value.mobJitterScale", "0.497409" },
            { "align.weight.value.value", "7.437058" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "131.643738" },
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.283124" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "-0.391581" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "-0.214266" },
            { "attackSeparate.crowd.radius.periodic.period.value", "2803.751709" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "0.498763" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "3888.679932" },
            { "attackSeparate.crowd.radius.value.mobJitterScale", "-0.388556" },
            { "attackSeparate.crowd.radius.value.value", "1229.007080" },
            { "attackSeparate.crowd.radius.valueType", "constant" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.965053" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "-0.209362" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.341653" },
            { "attackSeparate.crowd.size.periodic.period.value", "5279.211426" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "0.641336" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "5077.349609" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "0.100784" },
            { "attackSeparate.crowd.size.value.value", "4.534397" },
            { "attackSeparate.crowd.size.valueType", "constant" },
            { "attackSeparate.crowd.type.check", "linearUp" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.626002" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.463879" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "attackSeparate.range.radius.periodic.period.value", "8978.243164" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "-0.518193" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "6764.099609" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.383298" },
            { "attackSeparate.range.radius.value.value", "1681.031494" },
            { "attackSeparate.range.type.check", "strictOn" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.879020" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.610999" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.001349" },
            { "attackSeparate.weight.periodic.period.value", "7086.646484" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-0.756862" },
            { "attackSeparate.weight.periodic.tickShift.value", "6099.344238" },
            { "attackSeparate.weight.value.mobJitterScale", "-0.202954" },
            { "attackSeparate.weight.value.value", "-1.954010" },
            { "attackSeparate.weight.valueType", "periodic" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "0.272416" },
            { "base.crowd.radius.periodic.amplitude.value", "0.381149" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "0.980005" },
            { "base.crowd.radius.periodic.period.value", "3473.620850" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "0.337139" },
            { "base.crowd.radius.periodic.tickShift.value", "6855.192871" },
            { "base.crowd.radius.value.mobJitterScale", "0.706092" },
            { "base.crowd.radius.value.value", "2000.000000" },
            { "base.crowd.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.321340" },
            { "base.crowd.size.periodic.amplitude.value", "0.719819" },
            { "base.crowd.size.periodic.period.mobJitterScale", "-0.810000" },
            { "base.crowd.size.periodic.period.value", "1781.792847" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.065591" },
            { "base.crowd.size.periodic.tickShift.value", "7971.676758" },
            { "base.crowd.size.value.mobJitterScale", "0.797722" },
            { "base.crowd.size.value.value", "-0.801900" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "quadraticDown" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.261353" },
            { "base.range.radius.periodic.amplitude.value", "0.545270" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.809965" },
            { "base.range.radius.periodic.period.value", "3747.532715" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.503971" },
            { "base.range.radius.periodic.tickShift.value", "8982.763672" },
            { "base.range.radius.value.mobJitterScale", "-0.509305" },
            { "base.range.radius.value.value", "684.124268" },
            { "base.range.radius.valueType", "constant" },
            { "base.range.type.check", "linearDown" },
            { "base.weight.periodic.amplitude.mobJitterScale", "-0.768362" },
            { "base.weight.periodic.amplitude.value", "-0.189072" },
            { "base.weight.periodic.period.mobJitterScale", "-0.571513" },
            { "base.weight.periodic.period.value", "7032.903809" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.004466" },
            { "base.weight.periodic.tickShift.value", "2164.944336" },
            { "base.weight.value.mobJitterScale", "-0.627066" },
            { "base.weight.value.value", "7.606221" },
            { "baseDefense.crowd.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "-0.449350" },
            { "baseDefense.crowd.radius.periodic.period.mobJitterScale", "-0.664525" },
            { "baseDefense.crowd.radius.periodic.period.value", "4369.157227" },
            { "baseDefense.crowd.radius.periodic.tickShift.mobJitterScale", "0.325719" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "6633.802246" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "0.591428" },
            { "baseDefense.crowd.radius.value.value", "1480.089355" },
            { "baseDefense.crowd.radius.valueType", "periodic" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "0.248689" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.146714" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "0.312365" },
            { "baseDefense.crowd.size.periodic.period.value", "1787.456787" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "0.483481" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "2555.091797" },
            { "baseDefense.crowd.size.value.mobJitterScale", "0.011442" },
            { "baseDefense.crowd.size.value.value", "2.032178" },
            { "baseDefense.crowd.type.check", "quadraticUp" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.331933" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.528757" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.587932" },
            { "baseDefense.range.radius.periodic.period.value", "4977.439453" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "baseDefense.range.radius.periodic.tickShift.value", "8625.263672" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.221529" },
            { "baseDefense.range.radius.value.value", "587.852173" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "always" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.652336" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "0.868550" },
            { "baseDefense.weight.periodic.period.value", "9000.000000" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.778877" },
            { "baseDefense.weight.periodic.tickShift.value", "1850.280762" },
            { "baseDefense.weight.value.mobJitterScale", "0.118065" },
            { "baseDefense.weight.value.value", "-5.985843" },
            { "baseDefense.weight.valueType", "constant" },
            { "brokenCrowdChecks", "TRUE" },
            { "center.crowd.radius.periodic.amplitude.mobJitterScale", "-0.613740" },
            { "center.crowd.radius.periodic.amplitude.value", "-0.710882" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "-0.046714" },
            { "center.crowd.radius.periodic.period.value", "7206.285156" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "0.193257" },
            { "center.crowd.radius.periodic.tickShift.value", "9694.068359" },
            { "center.crowd.radius.value.mobJitterScale", "-0.428313" },
            { "center.crowd.radius.value.value", "1492.635620" },
            { "center.crowd.radius.valueType", "constant" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.819499" },
            { "center.crowd.size.periodic.amplitude.value", "0.127825" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.021288" },
            { "center.crowd.size.periodic.period.value", "3988.431152" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "-0.580340" },
            { "center.crowd.size.periodic.tickShift.value", "9204.295898" },
            { "center.crowd.size.value.mobJitterScale", "-0.117129" },
            { "center.crowd.size.value.value", "4.685698" },
            { "center.crowd.size.valueType", "constant" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.556016" },
            { "center.range.radius.periodic.amplitude.value", "0.950442" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.895122" },
            { "center.range.radius.periodic.period.value", "1689.869873" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "-0.032997" },
            { "center.range.radius.periodic.tickShift.value", "-0.900000" },
            { "center.range.radius.value.mobJitterScale", "-1.000000" },
            { "center.range.radius.value.value", "419.654205" },
            { "center.range.radius.valueType", "constant" },
            { "center.range.type.check", "quadraticDown" },
            { "center.weight.periodic.amplitude.mobJitterScale", "-0.411168" },
            { "center.weight.periodic.amplitude.value", "-0.731794" },
            { "center.weight.periodic.period.mobJitterScale", "1.000000" },
            { "center.weight.periodic.period.value", "6059.680176" },
            { "center.weight.periodic.tickShift.mobJitterScale", "0.501683" },
            { "center.weight.periodic.tickShift.value", "1477.837158" },
            { "center.weight.value.mobJitterScale", "-0.330763" },
            { "center.weight.value.value", "5.543721" },
            { "center.weight.valueType", "periodic" },
            { "cohere.crowd.radius.periodic.amplitude.mobJitterScale", "-0.300208" },
            { "cohere.crowd.radius.periodic.amplitude.value", "0.014810" },
            { "cohere.crowd.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "cohere.crowd.radius.periodic.period.value", "4306.573730" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "0.531593" },
            { "cohere.crowd.radius.periodic.tickShift.value", "1.000000" },
            { "cohere.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "cohere.crowd.radius.value.value", "1485.363525" },
            { "cohere.crowd.radius.valueType", "periodic" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.345668" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "0.582845" },
            { "cohere.crowd.size.periodic.period.value", "5856.032227" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.143444" },
            { "cohere.crowd.size.periodic.tickShift.value", "8624.766602" },
            { "cohere.crowd.size.value.mobJitterScale", "-1.000000" },
            { "cohere.crowd.size.value.value", "7.441362" },
            { "cohere.crowd.size.valueType", "constant" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "-0.717139" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.666347" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.377740" },
            { "cohere.range.radius.periodic.period.value", "8888.746094" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.761479" },
            { "cohere.range.radius.periodic.tickShift.value", "-1.000000" },
            { "cohere.range.radius.value.mobJitterScale", "0.798356" },
            { "cohere.range.radius.value.value", "330.986176" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "linearDown" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "0.430057" },
            { "cohere.weight.periodic.amplitude.value", "-0.285235" },
            { "cohere.weight.periodic.period.mobJitterScale", "-0.562540" },
            { "cohere.weight.periodic.period.value", "7751.362305" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.044512" },
            { "cohere.weight.periodic.tickShift.value", "-1.000000" },
            { "cohere.weight.value.mobJitterScale", "0.570678" },
            { "cohere.weight.value.value", "-2.153038" },
            { "cohere.weight.valueType", "constant" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "-0.514429" },
            { "cores.crowd.radius.periodic.amplitude.value", "0.414499" },
            { "cores.crowd.radius.periodic.period.mobJitterScale", "1.000000" },
            { "cores.crowd.radius.periodic.period.value", "9634.734375" },
            { "cores.crowd.radius.periodic.tickShift.mobJitterScale", "0.011175" },
            { "cores.crowd.radius.periodic.tickShift.value", "501.588928" },
            { "cores.crowd.radius.value.mobJitterScale", "-0.784407" },
            { "cores.crowd.radius.value.value", "532.678650" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "0.473059" },
            { "cores.crowd.size.periodic.amplitude.value", "0.040520" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "cores.crowd.size.periodic.period.value", "6761.712402" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.979099" },
            { "cores.crowd.size.periodic.tickShift.value", "1922.351562" },
            { "cores.crowd.size.value.mobJitterScale", "0.136214" },
            { "cores.crowd.size.value.value", "15.960786" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowd.type.check", "linearUp" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.396378" },
            { "cores.range.radius.periodic.amplitude.value", "0.124734" },
            { "cores.range.radius.periodic.period.mobJitterScale", "-0.980641" },
            { "cores.range.radius.periodic.period.value", "292.823242" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.609326" },
            { "cores.range.radius.periodic.tickShift.value", "5974.928711" },
            { "cores.range.radius.value.mobJitterScale", "-0.493390" },
            { "cores.range.radius.value.value", "1155.311157" },
            { "cores.range.radius.valueType", "constant" },
            { "cores.range.type.check", "linearDown" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.997572" },
            { "cores.weight.periodic.amplitude.value", "0.385718" },
            { "cores.weight.periodic.period.mobJitterScale", "-0.029844" },
            { "cores.weight.periodic.period.value", "1777.452271" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.537829" },
            { "cores.weight.periodic.tickShift.value", "7219.061523" },
            { "cores.weight.value.mobJitterScale", "-0.512606" },
            { "cores.weight.value.value", "4.701284" },
            { "cores.weight.valueType", "constant" },
            { "corners.crowd.radius.periodic.amplitude.mobJitterScale", "-0.544753" },
            { "corners.crowd.radius.periodic.amplitude.value", "-0.013377" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "0.676946" },
            { "corners.crowd.radius.periodic.period.value", "3058.092285" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "-0.242267" },
            { "corners.crowd.radius.periodic.tickShift.value", "641.512878" },
            { "corners.crowd.radius.value.mobJitterScale", "0.674417" },
            { "corners.crowd.radius.value.value", "38.534962" },
            { "corners.crowd.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "0.511584" },
            { "corners.crowd.size.periodic.amplitude.value", "-0.013610" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "0.808718" },
            { "corners.crowd.size.periodic.period.value", "7585.468750" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.962805" },
            { "corners.crowd.size.periodic.tickShift.value", "664.637634" },
            { "corners.crowd.size.value.mobJitterScale", "0.172321" },
            { "corners.crowd.size.value.value", "-0.207910" },
            { "corners.crowd.type.check", "strictOff" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.758218" },
            { "corners.range.radius.periodic.amplitude.value", "0.929145" },
            { "corners.range.radius.periodic.period.mobJitterScale", "0.969885" },
            { "corners.range.radius.periodic.period.value", "342.181335" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-0.456807" },
            { "corners.range.radius.periodic.tickShift.value", "8909.400391" },
            { "corners.range.radius.value.mobJitterScale", "0.653003" },
            { "corners.range.radius.value.value", "1924.797607" },
            { "corners.range.type.check", "quadraticDown" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "0.128603" },
            { "corners.weight.periodic.amplitude.value", "0.250734" },
            { "corners.weight.periodic.period.mobJitterScale", "-0.830751" },
            { "corners.weight.periodic.period.value", "7888.131348" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "0.389372" },
            { "corners.weight.periodic.tickShift.value", "9696.195312" },
            { "corners.weight.value.mobJitterScale", "-0.889577" },
            { "corners.weight.value.value", "0.597972" },
            { "corners.weight.valueType", "constant" },
            { "creditReserve", "102.333519" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.579442" },
            { "curHeadingWeight.periodic.amplitude.value", "-0.189152" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "-0.416844" },
            { "curHeadingWeight.periodic.period.value", "8540.330078" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "0.577196" },
            { "curHeadingWeight.periodic.tickShift.value", "2460.511230" },
            { "curHeadingWeight.value.mobJitterScale", "-0.480068" },
            { "curHeadingWeight.value.value", "6.813936" },
            { "curHeadingWeight.valueType", "periodic" },
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "-0.994516" },
            { "edges.crowd.radius.periodic.amplitude.value", "-0.378261" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "-0.074269" },
            { "edges.crowd.radius.periodic.period.value", "3354.915771" },
            { "edges.crowd.radius.periodic.tickShift.mobJitterScale", "-0.619426" },
            { "edges.crowd.radius.periodic.tickShift.value", "3648.832764" },
            { "edges.crowd.radius.value.mobJitterScale", "1.000000" },
            { "edges.crowd.radius.value.value", "756.695984" },
            { "edges.crowd.radius.valueType", "periodic" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.089937" },
            { "edges.crowd.size.periodic.amplitude.value", "-0.148265" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "-0.406571" },
            { "edges.crowd.size.periodic.period.value", "3180.369141" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.347137" },
            { "edges.crowd.size.periodic.tickShift.value", "2897.321289" },
            { "edges.crowd.size.value.mobJitterScale", "0.160988" },
            { "edges.crowd.size.value.value", "18.209734" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.529615" },
            { "edges.range.radius.periodic.amplitude.value", "-0.568274" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.147722" },
            { "edges.range.radius.periodic.period.value", "2397.120361" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.316319" },
            { "edges.range.radius.periodic.tickShift.value", "7746.920898" },
            { "edges.range.radius.value.mobJitterScale", "-0.166437" },
            { "edges.range.radius.value.value", "105.715218" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.range.type.check", "quadraticDown" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.260742" },
            { "edges.weight.periodic.amplitude.value", "-0.782359" },
            { "edges.weight.periodic.period.mobJitter", "le = 0.211810" },
            { "edges.weight.periodic.period.value", "2228.156006" },
            { "edges.weight.periodic.tickShift.mo", "tterScale = -0.628409" },
            { "edges.weight.periodic.tick", "ft.value = 5620.700195" },
            { "edges.weight.value.mobJitt", "cale = 0.376197" },
            { "edges.weight.value.value", "-9.761374" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "-0.113565" },
            { "enemy.crowd.radius.periodic.amplitude.value", "0.797273" },
            { "enemy.crowd.radius.periodic.period.mobJitterScale", "0.612839" },
            { "enemy.crowd.radius.periodic.period.value", "7205.943848" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "0.537484" },
            { "enemy.crowd.radius.periodic.tickShift.value", "2852.082031" },
            { "enemy.crowd.radius.value.mobJitterScale", "0.246272" },
            { "enemy.crowd.radius.value.value", "1753.000244" },
            { "enemy.crowd.radius.valueType", "constant" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.719163" },
            { "enemy.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.435277" },
            { "enemy.crowd.size.periodic.period.value", "8627.128906" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "enemy.crowd.size.periodic.tickShift.value", "3536.032959" },
            { "enemy.crowd.size.value.mobJitterScale", "-0.034810" },
            { "enemy.crowd.size.value.value", "4.881328" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.358426" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.991542" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.222314" },
            { "enemy.range.radius.periodic.period.value", "3640.694092" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.534312" },
            { "enemy.range.radius.periodic.tickShift.value", "2044.963135" },
            { "enemy.range.radius.value.mobJitterScale", "-0.513448" },
            { "enemy.range.radius.value.value", "627.409363" },
            { "enemy.range.type.check", "always" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "0.360322" },
            { "enemy.weight.periodic.amplitude.value", "-0.694687" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.342379" },
            { "enemy.weight.periodic.period.value", "3183.064697" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.509380" },
            { "enemy.weight.periodic.tickShift.value", "7158.720703" },
            { "enemy.weight.value.mobJitterScale", "1.000000" },
            { "enemy.weight.value.value", "-4.875552" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.crowd.radius.periodic.amplitude.mobJitterScale", "0.666059" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "-0.892442" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "enemyBase.crowd.radius.periodic.period.value", "5049.244141" },
            { "enemyBase.crowd.radius.periodic.tickShift.mobJitterScale", "-0.224162" },
            { "enemyBase.crowd.radius.periodic.tickShift.value", "4023.808350" },
            { "enemyBase.crowd.radius.value.mobJitterScale", "0.802474" },
            { "enemyBase.crowd.radius.value.value", "-1.000000" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "-0.263916" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.204752" },
            { "enemyBase.crowd.size.periodic.period.value", "-1.000000" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.604551" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "5171.812500" },
            { "enemyBase.crowd.size.value.mobJitterScale", "0.503575" },
            { "enemyBase.crowd.size.value.value", "8.090740" },
            { "enemyBase.crowd.size.valueType", "constant" },
            { "enemyBase.crowd.type.check", "strictOff" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.384846" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.488288" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "enemyBase.range.radius.periodic.period.value", "2707.473877" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.469393" },
            { "enemyBase.range.radius.periodic.tickShift.value", "2827.049805" },
            { "enemyBase.range.radius.value.mobJitterScale", "-1.000000" },
            { "enemyBase.range.radius.value.value", "1299.019775" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.385827" },
            { "enemyBase.weight.periodic.amplitude.value", "-0.976131" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "0.146087" },
            { "enemyBase.weight.periodic.period.value", "7883.161133" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "-0.816430" },
            { "enemyBase.weight.periodic.tickShift.value", "4527.190918" },
            { "enemyBase.weight.value.mobJitterScale", "-0.301163" },
            { "enemyBase.weight.value.value", "5.789040" },
            { "enemyBase.weight.valueType", "constant" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.mobJitterScale", "-0.745041" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.value", "-0.077830" },
            { "enemyBaseGuess.crowd.radius.periodic.period.mobJitterScale", "-0.142308" },
            { "enemyBaseGuess.crowd.radius.periodic.period.value", "8708.490234" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.mobJitterScale", "-0.479243" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.value", "4289.472168" },
            { "enemyBaseGuess.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "enemyBaseGuess.crowd.radius.value.value", "1453.562134" },
            { "enemyBaseGuess.crowd.radius.valueType", "periodic" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.mobJitterScale", "0.248863" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "enemyBaseGuess.crowd.size.periodic.period.mobJitterScale", "-0.104922" },
            { "enemyBaseGuess.crowd.size.periodic.period.value", "965.213562" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.mobJitterScale", "-0.451154" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.value", "3639.852783" },
            { "enemyBaseGuess.crowd.size.value.mobJitterScale", "-0.700633" },
            { "enemyBaseGuess.crowd.size.value.value", "-1.000000" },
            { "enemyBaseGuess.crowd.size.valueType", "periodic" },
            { "enemyBaseGuess.crowd.type.check", "linearDown" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.mobJitterScale", "-0.873219" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.value", "-0.560818" },
            { "enemyBaseGuess.range.radius.periodic.period.mobJitterScale", "-0.779006" },
            { "enemyBaseGuess.range.radius.periodic.period.value", "3905.405518" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.mobJitterScale", "-0.607495" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.value", "3773.683105" },
            { "enemyBaseGuess.range.radius.value.mobJitterScale", "-0.786919" },
            { "enemyBaseGuess.range.radius.value.value", "1552.246216" },
            { "enemyBaseGuess.range.radius.valueType", "constant" },
            { "enemyBaseGuess.range.type.check", "never" },
            { "enemyBaseGuess.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemyBaseGuess.weight.periodic.amplitude.value", "0.505701" },
            { "enemyBaseGuess.weight.periodic.period.mobJitterScale", "0.242758" },
            { "enemyBaseGuess.weight.periodic.period.value", "5884.085938" },
            { "enemyBaseGuess.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "enemyBaseGuess.weight.periodic.tickShift.value", "1530.359863" },
            { "enemyBaseGuess.weight.value.mobJitterScale", "0.824522" },
            { "enemyBaseGuess.weight.value.value", "9.500000" },
            { "enemyBaseGuess.weight.valueType", "periodic" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "253.553192" },
            { "evadeStrictDistance", "343.531586" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fighterBaseDefenseUseRadius", "FALSE" },
            { "fleetLocus.circularPeriod", "6876.850098" },
            { "fleetLocus.circularWeight", "2.000000" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.383634" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.value", "-0.328060" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "0.617076" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "2803.064941" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "-0.727701" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "5636.786621" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "-0.318254" },
            { "fleetLocus.force.crowd.radius.value.value", "26.244011" },
            { "fleetLocus.force.crowd.radius.valueType", "periodic" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.027272" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "0.205372" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.739863" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "4643.184082" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.318560" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "5600.111816" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "0.409048" },
            { "fleetLocus.force.crowd.size.value.value", "11.628789" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "always" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.459694" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.826412" },
            { "fleetLocus.force.range.radius.periodic.period.value", "4565.895996" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.809695" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "2509.600098" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "-0.670140" },
            { "fleetLocus.force.range.radius.value.value", "1727.792236" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.488759" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-0.858352" },
            { "fleetLocus.force.weight.periodic.period.value", "2993.643311" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.800718" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "7792.906738" },
            { "fleetLocus.force.weight.value.mobJitterScale", "0.797047" },
            { "fleetLocus.force.weight.value.value", "7.610662" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "1.119370" },
            { "fleetLocus.linearXPeriod", "10872.433594" },
            { "fleetLocus.linearYPeriod", "8879.106445" },
            { "fleetLocus.randomPeriod", "4618.925293" },
            { "fleetLocus.randomWeight", "1.031988" },
            { "fleetLocus.useScaled", "TRUE" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.mobJitterScale", "-0.508431" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.value", "-0.946075" },
            { "flockDuringAttack.crowd.radius.periodic.period.mobJitterScale", "0.599983" },
            { "flockDuringAttack.crowd.radius.periodic.period.value", "2646.490967" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.846164" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.value", "5980.806641" },
            { "flockDuringAttack.crowd.radius.value.mobJitterScale", "-0.816748" },
            { "flockDuringAttack.crowd.radius.value.value", "760.381042" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.535740" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.value", "-0.634508" },
            { "flockDuringAttack.crowd.size.periodic.period.mobJitterScale", "0.953574" },
            { "flockDuringAttack.crowd.size.periodic.period.value", "7693.864258" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.mobJitterScale", "0.415498" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.value", "4906.399902" },
            { "flockDuringAttack.crowd.size.value.mobJitterScale", "0.417545" },
            { "flockDuringAttack.crowd.size.value.value", "14.666368" },
            { "flockDuringAttack.crowd.size.valueType", "constant" },
            { "flockDuringAttack.crowd.type.check", "never" },
            { "flockDuringAttack.forceOn", "FALSE" },
            { "flockDuringAttack.range.radius.periodic.amplitude.mobJitterScale", "0.282039" },
            { "flockDuringAttack.range.radius.periodic.amplitude.value", "0.623702" },
            { "flockDuringAttack.range.radius.periodic.period.mobJitterScale", "0.288751" },
            { "flockDuringAttack.range.radius.periodic.period.value", "6076.941895" },
            { "flockDuringAttack.range.radius.periodic.tickShift.mobJitterScale", "-0.728538" },
            { "flockDuringAttack.range.radius.periodic.tickShift.value", "9011.491211" },
            { "flockDuringAttack.range.radius.value.mobJitterScale", "-0.858978" },
            { "flockDuringAttack.range.radius.value.value", "494.490753" },
            { "flockDuringAttack.range.radius.valueType", "constant" },
            { "flockDuringAttack.range.type.check", "strictOn" },
            { "flockDuringAttack.value.periodic.amplitude.mobJitterScale", "0.501248" },
            { "flockDuringAttack.value.periodic.amplitude.value", "0.582870" },
            { "flockDuringAttack.value.periodic.period.mobJitterScale", "-0.308088" },
            { "flockDuringAttack.value.periodic.period.value", "8560.545898" },
            { "flockDuringAttack.value.periodic.tickShift.mobJitterScale", "-0.292355" },
            { "flockDuringAttack.value.periodic.tickShift.value", "9065.378906" },
            { "flockDuringAttack.value.value.mobJitterScale", "-0.577563" },
            { "flockDuringAttack.value.value.value", "0.028214" },
            { "flockDuringAttack.value.valueType", "constant" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "67.097183" },
            { "guardRange", "168.764328" },
            { "hold.invProbability.periodic.amplitude.mobJitterScale", "-0.778385" },
            { "hold.invProbability.periodic.amplitude.value", "0.525451" },
            { "hold.invProbability.periodic.period.mobJitterScale", "0.670797" },
            { "hold.invProbability.periodic.period.value", "2214.884521" },
            { "hold.invProbability.periodic.tickShift.mobJitterScale", "0.128211" },
            { "hold.invProbability.periodic.tickShift.value", "-1.000000" },
            { "hold.invProbability.value.mobJitterScale", "0.848250" },
            { "hold.invProbability.value.valu", " 10000.000000" },
            { "hold.invProbability.valueType", "constant" },
            { "hold.ticks.periodic.amplitude.mobJitt", "cale = 1.000000" },
            { "hold.ticks.periodic.amplitude.value", "-0.709669" },
            { "hold.ticks.periodic.period.mobJitterScale", "-0.990000" },
            { "hold.ticks.periodic.period.value", "4627.131836" },
            { "hold.ticks.periodic.tickShift.mobJitterScale", "0.517764" },
            { "hold.ticks.periodic.tickShift.value", "609.913147" },
            { "hold.ticks.value.mobJitterScale", "0.259767" },
            { "hold.ticks.value.value", "612.029785" },
            { "hold.ticks.valueType", "periodic" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.753332" },
            { "mobLocus.circularPeriod.value", "7529.633301" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "-0.691168" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "-1.000000" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "-0.241660" },
            { "mobLocus.circularWeight.periodic.period.value", "3202.818604" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "-0.508412" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "1040.052612" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.550883" },
            { "mobLocus.circularWeight.value.value", "-1.303414" },
            { "mobLocus.circularWeight.valueType", "constant" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.270795" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.value", "1.000000" },
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.182999" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "2599.939697" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.601894" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.value", "1385.453369" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "-0.093159" },
            { "mobLocus.force.crowd.radius.value.value", "558.492920" },
            { "mobLocus.force.crowd.radius.valueType", "constant" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.029949" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "0.515706" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "mobLocus.force.crowd.size.periodic.period.value", "5288.679199" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.792523" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "6140.027344" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "0.727316" },
            { "mobLocus.force.crowd.size.value.value", "16.199251" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowd.type.check", "linearUp" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.332333" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.076052" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.045041" },
            { "mobLocus.force.range.radius.periodic.period.value", "3621.787109" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.899542" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "3671.809814" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.595580" },
            { "mobLocus.force.range.radius.value.value", "574.746765" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.range.type.check", "always" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.575783" },
            { "mobLocus.force.weight.periodic.amplitude.value", "0.229083" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.736707" },
            { "mobLocus.force.weight.periodic.period.value", "5973.848145" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.377396" },
            { "mobLocus.force.weight.periodic.tickShift.value", "2128.088135" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.264655" },
            { "mobLocus.force.weight.value.value", "7.928903" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "0.793202" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "-0.810000" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.427251" },
            { "mobLocus.linearWeight.periodic.period.value", "2000.073120" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "0.751062" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "10000.000000" },
            { "mobLocus.linearWeight.value.mobJitterScale", "0.161089" },
            { "mobLocus.linearWeight.value.value", "1.215708" },
            { "mobLocus.linearWeight.valueType", "periodic" },
            { "mobLocus.linearXPeriod.mobJitterScale", "0.395199" },
            { "mobLocus.linearXPeriod.value", "608.558655" },
            { "mobLocus.linearYPeriod.mobJitterScale", "0.090457" },
            { "mobLocus.linearYPeriod.value", "10000.000000" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "-0.907524" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.891000" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "0.027127" },
            { "mobLocus.proximityRadius.periodic.period.value", "6554.017578" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "0.115264" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "1810.788574" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "-0.010009" },
            { "mobLocus.proximityRadius.value.value", "1292.208374" },
            { "mobLocus.proximityRadius.valueType", "periodic" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.240360" },
            { "mobLocus.randomPeriod.value", "7683.451660" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "-0.670905" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "0.221727" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "0.298932" },
            { "mobLocus.randomWeight.periodic.period.value", "1297.879761" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-0.278469" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "8172.792969" },
            { "mobLocus.randomWeight.value.mobJitterScale", "0.727619" },
            { "mobLocus.randomWeight.value.value", "8.315613" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "TRUE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "419.863647" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.543370" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.value", "0.411116" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.mobJitterScale", "-0.492742" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.value", "6107.423340" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.912923" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.value", "-1.000000" },
            { "nearBaseRandomIdle.crowd.radius.value.mobJitterScale", "1.000000" },
            { "nearBaseRandomIdle.crowd.radius.value.value", "1447.652954" },
            { "nearBaseRandomIdle.crowd.radius.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.mobJitterScale", "0.130832" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.value", "0.212115" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.value", "6790.902832" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.mobJitterScale", "-0.695923" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.value", "2097.240234" },
            { "nearBaseRandomIdle.crowd.size.value.mobJitterScale", "0.548683" },
            { "nearBaseRandomIdle.crowd.size.value.value", "3.007458" },
            { "nearBaseRandomIdle.crowd.size.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.type.check", "quadraticUp" },
            { "nearBaseRandomIdle.forceOn", "FALSE" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.mobJitterScale", "-0.362255" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.value", "-0.286453" },
            { "nearBaseRandomIdle.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "nearBaseRandomIdle.range.radius.periodic.period.value", "8106.854492" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.346995" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.value", "10000.000000" },
            { "nearBaseRandomIdle.range.radius.value.mobJitterScale", "-0.650520" },
            { "nearBaseRandomIdle.range.radius.value.value", "1017.373413" },
            { "nearBaseRandomIdle.range.radius.valueType", "periodic" },
            { "nearBaseRandomIdle.range.type.check", "strictOn" },
            { "nearBaseRandomIdle.value.periodic.amplitude.mobJitterScale", "0.778834" },
            { "nearBaseRandomIdle.value.periodic.amplitude.value", "-0.636085" },
            { "nearBaseRandomIdle.value.periodic.period.mobJitterScale", "-0.090463" },
            { "nearBaseRandomIdle.value.periodic.period.value", "6860.064453" },
            { "nearBaseRandomIdle.value.periodic.tickShift.mobJitterScale", "-0.093920" },
            { "nearBaseRandomIdle.value.periodic.tickShift.value", "3928.133789" },
            { "nearBaseRandomIdle.value.value.mobJitterScale", "-0.516429" },
            { "nearBaseRandomIdle.value.value.value", "0.237624" },
            { "nearBaseRandomIdle.value.valueType", "constant" },
            { "nearestFriend.crowd.radius.periodic.amplitude.mobJitterScale", "-0.582280" },
            { "nearestFriend.crowd.radius.periodic.amplitude.value", "-0.900000" },
            { "nearestFriend.crowd.radius.periodic.period.mobJitterScale", "-0.367045" },
            { "nearestFriend.crowd.radius.periodic.period.value", "4686.005371" },
            { "nearestFriend.crowd.radius.periodic.tickShift.mobJitterScale", "-0.686911" },
            { "nearestFriend.crowd.radius.periodic.tickShift.value", "5033.226074" },
            { "nearestFriend.crowd.radius.value.mobJitterScale", "0.893143" },
            { "nearestFriend.crowd.radius.value.value", "2000.000000" },
            { "nearestFriend.crowd.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "1.000000" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "-0.812341" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "-0.399262" },
            { "nearestFriend.crowd.size.periodic.period.value", "6853.668457" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "-0.029875" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "6080.995605" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "-0.445233" },
            { "nearestFriend.crowd.size.value.value", "-0.900000" },
            { "nearestFriend.crowd.size.valueType", "constant" },
            { "nearestFriend.crowd.type.check", "quadraticUp" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "-0.711729" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.882090" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "nearestFriend.range.radius.periodic.period.value", "5146.727539" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "0.160676" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "2388.345947" },
            { "nearestFriend.range.radius.value.mobJitterScale", "0.080157" },
            { "nearestFriend.range.radius.value.value", "1942.090088" },
            { "nearestFriend.range.radius.valueType", "periodic" },
            { "nearestFriend.range.type.check", "quadraticUp" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "-0.737894" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.004223" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "-0.718433" },
            { "nearestFriend.weight.periodic.period.value", "3760.571289" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "nearestFriend.weight.periodic.tickShift.value", "3934.461426" },
            { "nearestFriend.weight.value.mobJitterScale", "0.128424" },
            { "nearestFriend.weight.value.value", "8.104838" },
            { "nearestFriend.weight.valueType", "constant" },
            { "randomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.799510" },
            { "randomIdle.crowd.radius.periodic.amplitude.value", "0.412066" },
            { "randomIdle.crowd.radius.periodic.period.mobJitterScale", "-0.510820" },
            { "randomIdle.crowd.radius.periodic.period.value", "9676.550781" },
            { "randomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.788418" },
            { "randomIdle.crowd.radius.periodic.tickShift.value", "-1.000000" },
            { "randomIdle.crowd.radius.value.mobJitterScale", "-0.874008" },
            { "randomIdle.crowd.radius.value.value", "926.073120" },
            { "randomIdle.crowd.radius.valueType", "constant" },
            { "randomIdle.crowd.size.periodic.amplitude.mobJitterScale", "0.861914" },
            { "randomIdle.crowd.size.periodic.amplitude.value", "-0.513057" },
            { "randomIdle.crowd.size.periodic.period.mobJitterScale", "-0.972196" },
            { "randomIdle.crowd.size.periodic.period.value", "3069.881836" },
            { "randomIdle.crowd.size.periodic.tickShift.mobJitterScale", "-0.453907" },
            { "randomIdle.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "randomIdle.crowd.size.value.mobJitterScale", "-0.401251" },
            { "randomIdle.crowd.size.value.value", "19.255051" },
            { "randomIdle.crowd.type.check", "never" },
            { "randomIdle.forceOn", "TRUE" },
            { "randomIdle.range.radius.periodic.amplitude.mobJitterScale", "0.805501" },
            { "randomIdle.range.radius.periodic.amplitude.value", "-0.548472" },
            { "randomIdle.range.radius.periodic.period.mobJitterScale", "0.411097" },
            { "randomIdle.range.radius.periodic.period.value", "3288.202148" },
            { "randomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.512191" },
            { "randomIdle.range.radius.periodic.tickShift.value", "10000.000000" },
            { "randomIdle.range.radius.value.mobJitterScale", "-0.418782" },
            { "randomIdle.range.radius.value.value", "727.185364" },
            { "randomIdle.range.radius.valueType", "periodic" },
            { "randomIdle.value.periodic.amplitude.mobJitterScale", "-0.369186" },
            { "randomIdle.value.periodic.amplitude.value", "-0.648493" },
            { "randomIdle.value.periodic.period.mobJitterScale", "0.259165" },
            { "randomIdle.value.periodic.period.value", "8518.525391" },
            { "randomIdle.value.periodic.tickShift.mobJitterScale", "-0.102290" },
            { "randomIdle.value.periodic.tickShift.value", "2557.845459" },
            { "randomIdle.value.value.mobJitterScale", "0.790335" },
            { "randomIdle.value.value.value", "0.580560" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.mobJitterScale", "0.349510" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.value", "-0.638881" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.mobJitterScale", "0.520063" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.value", "5396.080566" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.mobJitterScale", "-0.077485" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tic", "ift.value = 6875.250000" },
            { "randomizeStoppedVelocity.crowd.radius.value.mobJ", "erScale = 0.680414" },
            { "randomizeStoppedVelocity.crowd.radius.value.valu", " 1.100000" },
            { "randomizeStoppedVelocity.crowd.radius.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.mobJit", "Scale = -0.627701" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.value", "0.414475" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.mobJitter", "le = -0.515798" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.value", "8119.947266" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.mob", "terScale = -0.549884" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tic", "ift.value = 3997.842041" },
            { "randomizeStoppedVelocity.crowd.size.value.mobJ", "erScale = -0.826159" },
            { "randomizeStoppedVelocity.crowd.size.value.v", "e = 4.874429" },
            { "randomizeStoppedVelocity.crow", "ize.valueType = periodic" },
            { "randomizeStoppedVelocity.crow", "ype.check = strictOff" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.mobJitt", "cale = 0.075649" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.value", "-0.523120" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.mobJitter", "le = 0.727619" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.value", "2763.763184" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.mobJitterScale", "0.201098" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.value", "5204.114258" },
            { "randomizeStoppedVelocity.range.radius.value.mobJitterScale", "0.643894" },
            { "randomizeStoppedVelocity.range.radius.value.value", "822.083557" },
            { "randomizeStoppedVelocity.range.radius.valueType", "constant" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.mobJitterScale", "-0.440185" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.value", "0.262571" },
            { "randomizeStoppedVelocity.value.periodic.period.mobJitterScale", "1.000000" },
            { "randomizeStoppedVelocity.value.periodic.period.value", "6798.182617" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.mobJitterScale", "0.835883" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.value", "7667.679199" },
            { "randomizeStoppedVelocity.value.value.mobJitterScale", "0.423201" },
            { "randomizeStoppedVelocity.value.value.value", "-0.723449" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "11.820508" },
            { "sensorGrid.staleFighterTime", "2.004606" },
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.646217" },
            { "separate.crowd.radius.periodic.amplitude.value", "0.244054" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "-0.499717" },
            { "separate.crowd.radius.periodic.period.value", "4672.717285" },
            { "separate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.476654" },
            { "separate.crowd.radius.periodic.tickShift.value", "-1.000000" },
            { "separate.crowd.radius.value.mobJitterScale", "-0.714451" },
            { "separate.crowd.radius.value.value", "842.759888" },
            { "separate.crowd.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "-0.133370" },
            { "separate.crowd.size.periodic.amplitude.value", "0.715134" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "-0.174565" },
            { "separate.crowd.size.periodic.period.value", "3520.023438" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "-0.053919" },
            { "separate.crowd.size.periodic.tickShift.value", "2200.878662" },
            { "separate.crowd.size.value.mobJitterScale", "-0.632803" },
            { "separate.crowd.size.value.value", "4.618165" },
            { "separate.crowd.type.check", "always" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "separate.range.radius.periodic.amplitude.value", "1.000000" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.643755" },
            { "separate.range.radius.periodic.period.value", "3237.110107" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "0.517396" },
            { "separate.range.radius.periodic.tickShift.value", "7642.424805" },
            { "separate.range.radius.value.mobJitterScale", "-0.407831" },
            { "separate.range.radius.value.value", "1770.331421" },
            { "separate.range.radius.valueType", "constant" },
            { "separate.range.type.check", "never" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.625088" },
            { "separate.weight.periodic.amplitude.value", "0.180118" },
            { "separate.weight.periodic.period.mobJitterScale", "0.526422" },
            { "separate.weight.periodic.period.value", "3805.281250" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.351306" },
            { "separate.weight.periodic.tickShift.value", "3270.706299" },
            { "separate.weight.value.mobJitterScale", "0.955835" },
            { "separate.weight.value.value", "-3.128856" },
            { "separate.weight.valueType", "periodic" },
            { "simpleAttack.crowd.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "simpleAttack.crowd.radius.periodic.amplitude.value", "-0.914550" },
            { "simpleAttack.crowd.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "simpleAttack.crowd.radius.periodic.period.value", "1609.376587" },
            { "simpleAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.996069" },
            { "simpleAttack.crowd.radius.periodic.tickShift.value", "-1.000000" },
            { "simpleAttack.crowd.radius.value.mobJitterScale", "0.565166" },
            { "simpleAttack.crowd.radius.value.value", "1596.848755" },
            { "simpleAttack.crowd.radius.valueType", "periodic" },
            { "simpleAttack.crowd.size.periodic.amplitude.mobJitterScale", "-0.449368" },
            { "simpleAttack.crowd.size.periodic.amplitude.value", "-0.734516" },
            { "simpleAttack.crowd.size.periodic.period.mobJitterScale", "0.624710" },
            { "simpleAttack.crowd.size.periodic.period.value", "6451.492676" },
            { "simpleAttack.crowd.size.periodic.tickShift.mobJitterScale", "-0.028501" },
            { "simpleAttack.crowd.size.periodic.tickShift.value", "2763.728760" },
            { "simpleAttack.crowd.size.value.mobJitterScale", "-0.319268" },
            { "simpleAttack.crowd.size.value.value", "18.085287" },
            { "simpleAttack.crowd.size.valueType", "periodic" },
            { "simpleAttack.crowd.type.check", "strictOn" },
            { "simpleAttack.forceOn", "TRUE" },
            { "simpleAttack.range.radius.periodic.amplitude.mobJitterScale", "0.668171" },
            { "simpleAttack.range.radius.periodic.amplitude.value", "-0.900000" },
            { "simpleAttack.range.radius.periodic.period.mobJitterScale", "-0.319821" },
            { "simpleAttack.range.radius.periodic.period.value", "1.100000" },
            { "simpleAttack.range.radius.periodic.tickShift.mobJitterScale", "0.109491" },
            { "simpleAttack.range.radius.periodic.tickShift.value", "10000.000000" },
            { "simpleAttack.range.radius.value.mobJitterScale", "-0.146252" },
            { "simpleAttack.range.radius.value.value", "1080.440063" },
            { "simpleAttack.range.radius.valueType", "constant" },
            { "simpleAttack.range.type.check", "strictOn" },
            { "simpleAttack.value.periodic.amplitude.mobJitterScale", "0.785823" },
            { "simpleAttack.value.periodic.amplitude.value", "-0.581230" },
            { "simpleAttack.value.periodic.period.mobJitterScale", "0.399060" },
            { "simpleAttack.value.periodic.period.value", "624.282288" },
            { "simpleAttack.value.periodic.tickShift.mobJitterScale", "0.543145" },
            { "simpleAttack.value.periodic.tickShift.value", "6145.979980" },
            { "simpleAttack.value.value.mobJitterScale", "-0.273594" },
            { "simpleAttack.value.value.value", "0.065305" },
            { "simpleAttack.value.valueType", "periodic" },
            { "startingMaxRadius", "1355.716675" },
            { "startingMinRadius", "374.166687" },
        };
        BundleConfigValue configs9[] = {
            { "brokenCrowdChecks", "FALSE" },
            { "align.crowd.radius.periodic.amplitude.mobJitterScale", "-0.936558" },
            { "align.crowd.radius.periodic.amplitude.value", "-1.000000" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "-0.571749" },
            { "align.crowd.radius.periodic.period.value", "8342.918945" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.326934" },
            { "align.crowd.radius.periodic.tickShift.value", "8688.698242" },
            { "align.crowd.radius.value.mobJitterScale", "0.686854" },
            { "align.crowd.radius.value.value", "1127.169922" },
            { "align.crowd.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.573010" },
            { "align.crowd.size.periodic.amplitude.value", "-0.144680" },
            { "align.crowd.size.periodic.period.mobJitterScale", "-0.131483" },
            { "align.crowd.size.periodic.period.value", "1271.343018" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.044981" },
            { "align.crowd.size.periodic.tickShift.value", "4742.378418" },
            { "align.crowd.size.value.mobJitterScale", "-0.829904" },
            { "align.crowd.size.value.value", "7.371715" },
            { "align.crowd.size.valueType", "constant" },
            { "align.crowd.type.check", "linearUp" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.719630" },
            { "align.range.radius.periodic.amplitude.value", "-0.293821" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.197922" },
            { "align.range.radius.periodic.period.value", "2834.085449" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.661814" },
            { "align.range.radius.periodic.tickShift.value", "-0.900000" },
            { "align.range.radius.value.mobJitterScale", "1.000000" },
            { "align.range.radius.value.value", "570.482056" },
            { "align.range.radius.valueType", "constant" },
            { "align.range.type.check", "linearDown" },
            { "align.weight.periodic.amplitude.mobJitterScale", "-0.599080" },
            { "align.weight.periodic.amplitude.value", "-0.267171" },
            { "align.weight.periodic.period.mobJitterScale", "-0.426342" },
            { "align.weight.periodic.period.value", "7235.197754" },
            { "align.weight.periodic.tickShift.mobJitterScale", "-0.452226" },
            { "align.weight.periodic.tickShift.value", "833.588074" },
            { "align.weight.value.mobJitterScale", "0.601865" },
            { "align.weight.value.value", "9.763093" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "145.034424" },
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.254812" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "-0.391581" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "0.705347" },
            { "attackSeparate.crowd.radius.periodic.period.value", "7016.529785" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "0.548639" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "2901.904785" },
            { "attackSeparate.crowd.radius.value.mobJitterScale", "-0.388556" },
            { "attackSeparate.crowd.radius.value.value", "1564.831177" },
            { "attackSeparate.crowd.radius.valueType", "constant" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.877321" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.682726" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.574715" },
            { "attackSeparate.crowd.size.periodic.period.value", "5783.020996" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-0.393970" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "4187.182617" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "0.298921" },
            { "attackSeparate.crowd.size.value.value", "11.400879" },
            { "attackSeparate.crowd.size.valueType", "constant" },
            { "attackSeparate.crowd.type.check", "quadraticUp" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.698653" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.704380" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.424997" },
            { "attackSeparate.range.radius.periodic.period.value", "10000.000000" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "-0.060889" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "7440.509766" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.623141" },
            { "attackSeparate.range.radius.value.value", "1787.425293" },
            { "attackSeparate.range.type.check", "strictOn" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.675586" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.555454" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "0.096847" },
            { "attackSeparate.weight.periodic.period.value", "6033.578613" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "0.906987" },
            { "attackSeparate.weight.periodic.tickShift.value", "5694.170410" },
            { "attackSeparate.weight.value.mobJitterScale", "-0.212936" },
            { "attackSeparate.weight.value.value", "-7.099125" },
            { "attackSeparate.weight.valueType", "periodic" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "0.070309" },
            { "base.crowd.radius.periodic.amplitude.value", "0.739891" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "-0.337289" },
            { "base.crowd.radius.periodic.period.value", "1862.039429" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "0.520252" },
            { "base.crowd.radius.periodic.tickShift.value", "2785.013916" },
            { "base.crowd.radius.value.mobJitterScale", "0.706092" },
            { "base.crowd.radius.value.value", "424.302948" },
            { "base.crowd.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "0.603067" },
            { "base.crowd.size.periodic.amplitude.value", "0.719819" },
            { "base.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "base.crowd.size.periodic.period.value", "1603.613525" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "-0.137816" },
            { "base.crowd.size.periodic.tickShift.value", "4273.619629" },
            { "base.crowd.size.value.mobJitterScale", "0.797722" },
            { "base.crowd.size.value.value", "14.891645" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "quadraticDown" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.251652" },
            { "base.range.radius.periodic.amplitude.value", "0.721609" },
            { "base.range.radius.periodic.period.mobJitterScale", "0.656072" },
            { "base.range.radius.periodic.period.value", "9000.000000" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.315240" },
            { "base.range.radius.periodic.tickShift.value", "10000.000000" },
            { "base.range.radius.value.mobJitterScale", "0.140637" },
            { "base.range.radius.value.value", "1435.833740" },
            { "base.range.radius.valueType", "periodic" },
            { "base.range.type.check", "linearDown" },
            { "base.weight.periodic.amplitude.mobJitterScale", "0.116760" },
            { "base.weight.periodic.amplitude.value", "-0.189072" },
            { "base.weight.periodic.period.mobJitterScale", "-0.093351" },
            { "base.weight.periodic.period.value", "7032.903809" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.004466" },
            { "base.weight.periodic.tickShift.value", "8633.225586" },
            { "base.weight.value.mobJitterScale", "1.000000" },
            { "base.weight.value.value", "3.554574" },
            { "base.weight.valueType", "constant" },
            { "baseDefense.crowd.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "-0.724179" },
            { "baseDefense.crowd.radius.periodic.period.mobJitterScale", "-0.664525" },
            { "baseDefense.crowd.radius.periodic.period.value", "2702.732910" },
            { "baseDefense.crowd.radius.periodic.tickShift.mobJitterScale", "0.398101" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "9145.913086" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "1.000000" },
            { "baseDefense.crowd.radius.value.value", "506.106598" },
            { "baseDefense.crowd.radius.valueType", "periodic" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "0.397853" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.161385" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "-0.100871" },
            { "baseDefense.crowd.size.periodic.period.value", "4035.528564" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "0.483481" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "2458.634521" },
            { "baseDefense.crowd.size.value.mobJitterScale", "0.011799" },
            { "baseDefense.crowd.size.value.value", "2.235396" },
            { "baseDefense.crowd.type.check", "quadraticUp" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.328613" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.713191" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.529139" },
            { "baseDefense.range.radius.periodic.period.value", "4479.695312" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "baseDefense.range.radius.periodic.tickShift.value", "2455.943359" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.649100" },
            { "baseDefense.range.radius.value.value", "-1.000000" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "always" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.865216" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "0.789591" },
            { "baseDefense.weight.periodic.period.value", "9000.000000" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.533038" },
            { "baseDefense.weight.periodic.tickShift.value", "1012.555969" },
            { "baseDefense.weight.value.mobJitterScale", "-0.076487" },
            { "baseDefense.weight.value.value", "8.779205" },
            { "baseDefense.weight.valueType", "periodic" },
            { "baseDefenseRadius", "464.254028" },
            { "center.crowd.radius.periodic.amplitude.mobJitterScale", "-0.699774" },
            { "center.crowd.radius.periodic.amplitude.value", "-0.250589" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "0.160638" },
            { "center.crowd.radius.periodic.period.value", "7321.457520" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "0.380995" },
            { "center.crowd.radius.periodic.tickShift.value", "4776.438965" },
            { "center.crowd.radius.value.mobJitterScale", "-0.401291" },
            { "center.crowd.radius.value.value", "786.217712" },
            { "center.crowd.radius.valueType", "constant" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.819499" },
            { "center.crowd.size.periodic.amplitude.value", "-0.536075" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.208695" },
            { "center.crowd.size.periodic.period.value", "2964.288086" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "-0.788116" },
            { "center.crowd.size.periodic.tickShift.value", "9148.484375" },
            { "center.crowd.size.value.mobJitterScale", "-0.306157" },
            { "center.crowd.size.value.value", "19.585342" },
            { "center.crowd.size.valueType", "periodic" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.398021" },
            { "center.range.radius.periodic.amplitude.value", "-0.662367" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.895122" },
            { "center.range.radius.periodic.period.value", "1672.971191" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "-0.240982" },
            { "center.range.radius.periodic.tickShift.value", "-1.000000" },
            { "center.range.radius.value.mobJitterScale", "-0.810000" },
            { "center.range.radius.value.value", "360.544434" },
            { "center.range.radius.valueType", "constant" },
            { "center.range.type.check", "quadraticDown" },
            { "center.weight.periodic.amplitude.mobJitterScale", "-0.407056" },
            { "center.weight.periodic.amplitude.value", "-0.731794" },
            { "center.weight.periodic.period.mobJitterScale", "0.668480" },
            { "center.weight.periodic.period.value", "6375.853516" },
            { "center.weight.periodic.tickShift.mobJitterScale", "0.557425" },
            { "center.weight.periodic.tickShift.value", "1477.837158" },
            { "center.weight.value.mobJitterScale", "-0.821289" },
            { "center.weight.value.value", "-2.848799" },
            { "center.weight.valueType", "periodic" },
            { "cohere.crowd.radius.periodic.amplitude.mobJitterScale", "-0.112237" },
            { "cohere.crowd.radius.periodic.amplitude.value", "0.011996" },
            { "cohere.crowd.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "cohere.crowd.radius.periodic.period.value", "4737.231445" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "0.156480" },
            { "cohere.crowd.radius.periodic.tickShift.value", "6994.729980" },
            { "cohere.crowd.radius.value.mobJitterScale", "-0.238718" },
            { "cohere.crowd.radius.value.value", "1381.295898" },
            { "cohere.crowd.radius.valueType", "periodic" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.327156" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "0.195715" },
            { "cohere.crowd.size.periodic.period.value", "8653.084961" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "-0.058796" },
            { "cohere.crowd.size.periodic.tickShift.value", "1132.748657" },
            { "cohere.crowd.size.value.mobJitterScale", "-1.000000" },
            { "cohere.crowd.size.value.value", "8.268181" },
            { "cohere.crowd.size.valueType", "constant" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "-0.580882" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.385874" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-0.377740" },
            { "cohere.range.radius.periodic.period.value", "9017.322266" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.575184" },
            { "cohere.range.radius.periodic.tickShift.value", "1764.521606" },
            { "cohere.range.radius.value.mobJitterScale", "0.592162" },
            { "cohere.range.radius.value.value", "192.383194" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "strictOff" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "-0.137575" },
            { "cohere.weight.periodic.amplitude.value", "-0.606619" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.106516" },
            { "cohere.weight.periodic.period.value", "7508.795898" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.044512" },
            { "cohere.weight.periodic.tickShift.value", "-1.000000" },
            { "cohere.weight.value.mobJitterScale", "0.564345" },
            { "cohere.weight.value.value", "-1.852564" },
            { "cohere.weight.valueType", "constant" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "-0.703737" },
            { "cores.crowd.radius.periodic.amplitude.value", "-0.768270" },
            { "cores.crowd.radius.periodic.period.mobJitterScale", "-0.854934" },
            { "cores.crowd.radius.periodic.period.value", "8344.077148" },
            { "cores.crowd.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "cores.crowd.radius.periodic.tickShift.value", "1506.614014" },
            { "cores.crowd.radius.value.mobJitterScale", "-0.882498" },
            { "cores.crowd.radius.value.value", "1343.628662" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "0.090915" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.398707" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "0.900000" },
            { "cores.crowd.size.periodic.period.value", "6141.595215" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.979099" },
            { "cores.crowd.size.periodic.tickShift.value", "891.589172" },
            { "cores.crowd.size.value.mobJitterScale", "0.407817" },
            { "cores.crowd.size.value.value", "17.931684" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowd.type.check", "linearDown" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.648159" },
            { "cores.range.radius.periodic.amplitude.value", "-0.076442" },
            { "cores.range.radius.periodic.period.mobJitterScale", "-0.588563" },
            { "cores.range.radius.periodic.period.value", "6356.158691" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.283639" },
            { "cores.range.radius.periodic.tickShift.value", "2862.066895" },
            { "cores.range.radius.value.mobJitterScale", "0.597000" },
            { "cores.range.radius.value.value", "564.370422" },
            { "cores.range.radius.valueType", "constant" },
            { "cores.range.type.check", "linearDown" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.997572" },
            { "cores.weight.periodic.amplitude.value", "0.385718" },
            { "cores.weight.periodic.period.mobJitterScale", "0.777109" },
            { "cores.weight.periodic.period.value", "9213.272461" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.532451" },
            { "cores.weight.periodic.tickShift.value", "8119.938477" },
            { "cores.weight.value.mobJitterScale", "-0.117907" },
            { "cores.weight.value.value", "-5.917153" },
            { "cores.weight.valueType", "constant" },
            { "corners.crowd.radius.periodic.amplitude.mobJitterScale", "0.553150" },
            { "corners.crowd.radius.periodic.amplitude.value", "0.652389" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "0.875955" },
            { "corners.crowd.radius.periodic.period.value", "649.068726" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "-0.474977" },
            { "corners.crowd.radius.periodic.tickShift.value", "1176.818726" },
            { "corners.crowd.radius.value.mobJitterScale", "-0.210728" },
            { "corners.crowd.radius.value.value", "272.695282" },
            { "corners.crowd.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "0.318163" },
            { "corners.crowd.size.periodic.amplitude.value", "0.342840" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "0.627164" },
            { "corners.crowd.size.periodic.period.value", "6256.838867" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.247364" },
            { "corners.crowd.size.periodic.tickShift.value", "604.216003" },
            { "corners.crowd.size.value.mobJitterScale", "0.155296" },
            { "corners.crowd.size.value.value", "-0.189009" },
            { "corners.crowd.type.check", "strictOff" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.673963" },
            { "corners.range.radius.periodic.amplitude.value", "-0.461757" },
            { "corners.range.radius.periodic.period.mobJitterScale", "0.969885" },
            { "corners.range.radius.periodic.period.value", "1311.569580" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-0.224893" },
            { "corners.range.radius.periodic.tickShift.value", "7519.286133" },
            { "corners.range.radius.value.mobJitterScale", "0.577286" },
            { "corners.range.radius.value.value", "466.789673" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.range.type.check", "quadraticDown" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "0.127317" },
            { "corners.weight.periodic.amplitude.value", "-0.990000" },
            { "corners.weight.periodic.period.mobJitterScale", "-1.000000" },
            { "corners.weight.periodic.period.value", "8124.818359" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "-0.256004" },
            { "corners.weight.periodic.tickShift.value", "8496.961914" },
            { "corners.weight.value.mobJitterScale", "-1.000000" },
            { "corners.weight.value.value", "-2.304240" },
            { "corners.weight.valueType", "constant" },
            { "creditReserve", "102.333519" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.579442" },
            { "curHeadingWeight.periodic.amplitude.value", "-0.034518" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "-0.621471" },
            { "curHeadingWeight.periodic.period.value", "8540.330078" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "0.634916" },
            { "curHeadingWeight.periodic.tickShift.value", "5759.041016" },
            { "curHeadingWeight.value.mobJitterScale", "-0.270508" },
            { "curHeadingWeight.value.value", "6.995586" },
            { "curHeadingWeight.valueType", "periodic" },
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "-0.796763" },
            { "edges.crowd.radius.periodic.amplitude.value", "-0.226011" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "-0.273614" },
            { "edges.crowd.radius.periodic.period.value", "4059.448242" },
            { "edges.crowd.radius.periodic.tickShift.mobJitterScale", "-0.897725" },
            { "edges.crowd.radius.periodic.tickShift.value", "3111.920410" },
            { "edges.crowd.radius.value.mobJitterScale", "1.000000" },
            { "edges.crowd.radius.value.value", "63.942524" },
            { "edges.crowd.radius.valueType", "constant" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.418199" },
            { "edges.crowd.size.periodic.amplitude.value", "0.041107" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "0.678531" },
            { "edges.crowd.size.periodic.period.value", "133.518463" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.312423" },
            { "edges.crowd.size.periodic.tickShift.value", "1855.255615" },
            { "edges.crowd.size.value.mobJitterScale", "-0.242224" },
            { "edges.crowd.size.value.value", "-1.000000" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "edges.range.radius.periodic.amplitude.value", "-0.631416" },
            { "edges.range.radius.periodic.period.mobJitterScale", "0.850425" },
            { "edges.range.radius.periodic.period.value", "3429.769043" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "0.347951" },
            { "edges.range.radius.periodic.tickShift.value", "5081.300293" },
            { "edges.range.radius.value.mobJitterScale", "-0.370566" },
            { "edges.range.radius.value.value", "1284.435791" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.range.type.check", "quadraticUp" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.131227" },
            { "edges.weight.periodic.amplitude.value", "0.621259" },
            { "edges.weight.periodic.period.mobJitterScale", "0.275306" },
            { "edges.weight.periodic.period.value", "7642.448242" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "-0.446422" },
            { "edges.weight.periodic.tickShift.value", "2727.331787" },
            { "edges.weight.value.mobJitterScale", "0.570557" },
            { "edges.weight.value.value", "-5.029550" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "-0.126183" },
            { "enemy.crowd.radius.periodic.amplitude.value", "0.877000" },
            { "enemy.crowd.radius.periodic.period.mobJitterScale", "0.501413" },
            { "enemy.crowd.radius.periodic.period.value", "7981.263184" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "0.800650" },
            { "enemy.crowd.radius.periodic.tickShift.value", "3005.793945" },
            { "enemy.crowd.radius.value.mobJitterScale", "0.459543" },
            { "enemy.crowd.radius.value.value", "526.802795" },
            { "enemy.crowd.radius.valueType", "constant" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.533786" },
            { "enemy.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "-0.604804" },
            { "enemy.crowd.size.periodic.period.value", "4469.407227" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "enemy.crowd.size.periodic.tickShift.value", "1817.149048" },
            { "enemy.crowd.size.value.mobJitterScale", "-0.034810" },
            { "enemy.crowd.size.value.value", "6.895956" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "never" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "-0.769930" },
            { "enemy.range.radius.periodic.amplitude.value", "-1.000000" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.017302" },
            { "enemy.range.radius.periodic.period.value", "992.563538" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.443909" },
            { "enemy.range.radius.periodic.tickShift.value", "2151.506836" },
            { "enemy.range.radius.value.mobJitterScale", "0.885478" },
            { "enemy.range.radius.value.value", "1512.450562" },
            { "enemy.range.type.check", "always" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "0.169535" },
            { "enemy.weight.periodic.amplitude.value", "-0.766993" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.536723" },
            { "enemy.weight.periodic.period.value", "2163.511719" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.331217" },
            { "enemy.weight.periodic.tickShift.value", "7087.133301" },
            { "enemy.weight.value.mobJitterScale", "1.000000" },
            { "enemy.weight.value.value", "-4.428475" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.crowd.radius.periodic.amplitude.mobJitterScale", "-0.455858" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "-0.441138" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "-0.603236" },
            { "enemyBase.crowd.radius.periodic.period.value", "8973.761719" },
            { "enemyBase.crowd.radius.periodic.tickShift.mobJitterScale", "-0.224162" },
            { "enemyBase.crowd.radius.periodic.tickShift.value", "4381.927734" },
            { "enemyBase.crowd.radius.value.mobJitterScale", "1.000000" },
            { "enemyBase.crowd.radius.value.value", "1051.522217" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "-0.041961" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.810740" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.254377" },
            { "enemyBase.crowd.size.periodic.period.value", "-1.000000" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.544096" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "4105.221191" },
            { "enemyBase.crowd.size.value.mobJitterScale", "0.303805" },
            { "enemyBase.crowd.size.value.value", "6.826741" },
            { "enemyBase.crowd.size.valueType", "constant" },
            { "enemyBase.crowd.type.check", "strictOff" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.153826" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.349173" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "0.207602" },
            { "enemyBase.range.radius.periodic.period.value", "1497.065308" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "-0.313450" },
            { "enemyBase.range.radius.periodic.tickShift.value", "1500.840210" },
            { "enemyBase.range.radius.value.mobJitterScale", "-1.000000" },
            { "enemyBase.range.radius.value.value", "1294.632690" },
            { "enemyBase.range.radius.valueType", "periodic" },
            { "enemyBase.range.type.check", "quadraticDown" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.529993" },
            { "enemyBase.weight.periodic.amplitude.value", "-0.626495" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "-0.315608" },
            { "enemyBase.weight.periodic.period.value", "4610.589355" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "-0.869555" },
            { "enemyBase.weight.periodic.tickShift.value", "5051.045410" },
            { "enemyBase.weight.value.mobJitterScale", "-0.095536" },
            { "enemyBase.weight.value.value", "4.874373" },
            { "enemyBase.weight.valueType", "periodic" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.mobJitterScale", "-0.670537" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.value", "0.104519" },
            { "enemyBaseGuess.crowd.radius.periodic.period.mobJitterScale", "-0.156539" },
            { "enemyBaseGuess.crowd.radius.periodic.period.value", "9000.000000" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.mobJitterScale", "-0.237027" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.value", "4267.643555" },
            { "enemyBaseGuess.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "enemyBaseGuess.crowd.radius.value.value", "938.033203" },
            { "enemyBaseGuess.crowd.radius.valueType", "constant" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.mobJitterScale", "-0.156069" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.value", "-0.900000" },
            { "enemyBaseGuess.crowd.size.periodic.period.mobJitterScale", "0.056107" },
            { "enemyBaseGuess.crowd.size.periodic.period.value", "955.561462" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.mobJitterScale", "0.510856" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.value", "8726.671875" },
            { "enemyBaseGuess.crowd.size.value.mobJitterScale", "-0.900000" },
            { "enemyBaseGuess.crowd.size.value.value", "-1.000000" },
            { "enemyBaseGuess.crowd.size.valueType", "periodic" },
            { "enemyBaseGuess.crowd.type.check", "linearDown" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.mobJitterScale", "-0.535163" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.value", "-0.371971" },
            { "enemyBaseGuess.range.radius.periodic.period.mobJitterScale", "-0.585214" },
            { "enemyBaseGuess.range.radius.periodic.period.value", "1272.536255" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.mobJitterScale", "-0.236566" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.value", "4192.981445" },
            { "enemyBaseGuess.range.radius.value.mobJitterScale", "-0.708227" },
            { "enemyBaseGuess.range.radius.value.value", "-1.000000" },
            { "enemyBaseGuess.range.radius.valueType", "constant" },
            { "enemyBaseGuess.range.type.check", "strictOff" },
            { "enemyBaseGuess.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemyBaseGuess.weight.periodic.amplitude.value", "0.556271" },
            { "enemyBaseGuess.weight.periodic.period.mobJitterScale", "0.242758" },
            { "enemyBaseGuess.weight.periodic.period.value", "5865.492676" },
            { "enemyBaseGuess.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "enemyBaseGuess.weight.periodic.tickShift.value", "5929.748535" },
            { "enemyBaseGuess.weight.value.mobJitterScale", "0.221247" },
            { "enemyBaseGuess.weight.value.value", "3.508280" },
            { "enemyBaseGuess.weight.valueType", "periodic" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "273.744202" },
            { "evadeStrictDistance", "204.355988" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fighterBaseDefenseUseRadius", "TRUE" },
            { "fleetLocus.circularPeriod", "7220.692383" },
            { "fleetLocus.circularWeight", "2.000000" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.387509" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.value", "-0.141640" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.841339" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "2522.758301" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.910717" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "5198.553223" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "-0.289322" },
            { "fleetLocus.force.crowd.radius.value.value", "146.716248" },
            { "fleetLocus.force.crowd.radius.valueType", "periodic" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.230153" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.207075" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.275646" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "5203.303711" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.296285" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "6685.870605" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "0.217984" },
            { "fleetLocus.force.crowd.size.value.value", "18.000000" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "always" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.212241" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.070531" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.743771" },
            { "fleetLocus.force.range.radius.periodic.period.value", "4565.895996" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.592336" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "6397.895996" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.526366" },
            { "fleetLocus.force.range.radius.value.value", "1335.173462" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "0.411705" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.723752" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-0.982031" },
            { "fleetLocus.force.weight.periodic.period.value", "3354.437744" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "-0.987040" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "7103.181641" },
            { "fleetLocus.force.weight.value.mobJitterScale", "0.805098" },
            { "fleetLocus.force.weight.value.value", "9.753067" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "1.012764" },
            { "fleetLocus.linearXPeriod", "10838.901367" },
            { "fleetLocus.linearYPeriod", "9323.061523" },
            { "fleetLocus.randomPeriod", "4013.977539" },
            { "fleetLocus.randomWeight", "1.031988" },
            { "fleetLocus.useScaled", "TRUE" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.mobJitterScale", "-0.374500" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.value", "-0.253732" },
            { "flockDuringAttack.crowd.radius.periodic.period.mobJitterScale", "0.769222" },
            { "flockDuringAttack.crowd.radius.periodic.period.value", "1603.981445" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.692316" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.value", "5382.726074" },
            { "flockDuringAttack.crowd.radius.value.mobJitterScale", "-0.816748" },
            { "flockDuringAttack.crowd.radius.value.value", "1458.427002" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.242109" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.value", "-0.641132" },
            { "flockDuringAttack.crowd.size.periodic.period.mobJitterScale", "0.447730" },
            { "flockDuringAttack.crowd.size.periodic.period.value", "8499.778320" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.mobJitterScale", "0.594410" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.value", "6033.678223" },
            { "flockDuringAttack.crowd.size.value.mobJitterScale", "-0.048157" },
            { "flockDuringAttack.crowd.size.value.value", "16.133005" },
            { "flockDuringAttack.crowd.size.valueType", "constant" },
            { "flockDuringAttack.crowd.type.check", "strictOff" },
            { "flockDuringAttack.forceOn", "FALSE" },
            { "flockDuringAttack.range.radius.periodic.amplitude.mobJitterScale", "0.253835" },
            { "flockDuringAttack.range.radius.periodic.amplitude.value", "0.997190" },
            { "flockDuringAttack.range.radius.periodic.period.mobJitterScale", "-0.333530" },
            { "flockDuringAttack.range.radius.periodic.period.value", "6076.941895" },
            { "flockDuringAttack.range.radius.periodic.tickShift.mobJitterScale", "-0.451883" },
            { "flockDuringAttack.range.radius.periodic.tickShift.value", "9011.491211" },
            { "flockDuringAttack.range.radius.value.mobJitterScale", "-0.556256" },
            { "flockDuringAttack.range.radius.value.value", "1290.422852" },
            { "flockDuringAttack.range.radius.valueType", "constant" },
            { "flockDuringAttack.range.type.check", "strictOn" },
            { "flockDuringAttack.value.periodic.amplitude.mobJitterScale", "0.558330" },
            { "flockDuringAttack.value.periodic.amplitude.value", "-0.008028" },
            { "flockDuringAttack.value.periodic.period.mobJitterScale", "-0.226865" },
            { "flockDuringAttack.value.periodic.period.value", "8560.545898" },
            { "flockDuringAttack.value.periodic.tickShift.mobJitterScale", "-0.292355" },
            { "flockDuringAttack.value.periodic.tickShift.value", "4011.650146" },
            { "flockDuringAttack.value.value.mobJitterScale", "-0.577563" },
            { "flockDuringAttack.value.value.value", "1.000000" },
            { "flockDuringAttack.value.valueType", "constant" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "39.644287" },
            { "guardRange", "153.589127" },
            { "hold.invProbability.periodic.amplitude.mobJitterScale", "0.492477" },
            { "hold.invProbability.periodic.amplitude.value", "0.282124" },
            { "hold.invProbability.periodic.period.mobJitterScale", "0.603717" },
            { "hold.invProbability.periodic.period.value", "3496.318359" },
            { "hold.invProbability.periodic.tickShift.mobJitterScale", "0.128211" },
            { "hold.invProbability.periodic.tickShift.value", "-1.000000" },
            { "hold.invProbability.value.mobJitterScale", "0.194941" },
            { "hold.invProbability.value.value", "7210.160645" },
            { "hold.invProbability.valueType", "constant" },
            { "hold.ticks.periodic.amplitude.mobJitterScale", "0.801253" },
            { "hold.ticks.periodic.amplitude.value", "-1.000000" },
            { "hold.ticks.periodic.period.mobJitterScale", "-0.900000" },
            { "hold.ticks.periodic.period.value", "4206.483398" },
            { "hold.ticks.periodic.tickShift.mobJitterScale", "0.763603" },
            { "hold.ticks.periodic.tickShift.value", "494.029633" },
            { "hold.ticks.value.mobJitterScale", "-0.180226" },
            { "hold.ticks.value.value", "509.596100" },
            { "hold.ticks.valueType", "constant" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.677999" },
            { "mobLocus.circularPeriod.value", "4730.083984" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "-0.803102" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "-0.331682" },
            { "mobLocus.circularWeight.periodic.period.value", "2278.373535" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.738049" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "1040.052612" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.605971" },
            { "mobLocus.circularWeight.value.value", "-5.761037" },
            { "mobLocus.circularWeight.valueType", "constant" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "0.094001" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.value", "0.816917" },
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.569871" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "1397.909546" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.807384" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.value", "1259.503052" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "-0.248055" },
            { "mobLocus.force.crowd.radius.value.value", "1105.378052" },
            { "mobLocus.force.crowd.radius.valueType", "periodic" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.020258" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "0.573007" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "mobLocus.force.crowd.size.periodic.period.value", "6399.302246" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.853440" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "4095.908936" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "0.635465" },
            { "mobLocus.force.crowd.size.value.value", "9.681067" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowd.type.check", "linearUp" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.350633" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "-0.124795" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.045041" },
            { "mobLocus.force.range.radius.periodic.period.value", "2690.346680" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.348108" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "6104.679199" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.589624" },
            { "mobLocus.force.range.radius.value.value", "469.136139" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.525825" },
            { "mobLocus.force.weight.periodic.amplitude.value", "0.251991" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.946954" },
            { "mobLocus.force.weight.periodic.period.value", "5465.742676" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.411560" },
            { "mobLocus.force.weight.periodic.tickShift.value", "8219.847656" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.291120" },
            { "mobLocus.force.weight.value.value", "-3.884734" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "-0.734809" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.509231" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.575336" },
            { "mobLocus.linearWeight.periodic.period.value", "2000.073120" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "0.751062" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "7228.217773" },
            { "mobLocus.linearWeight.value.mobJitterScale", "-0.032391" },
            { "mobLocus.linearWeight.value.value", "-0.390756" },
            { "mobLocus.linearWeight.valueType", "periodic" },
            { "mobLocus.linearXPeriod.mobJitterScale", "0.391247" },
            { "mobLocus.linearXPeriod.value", "2563.834473" },
            { "mobLocus.linearYPeriod.mobJitterScale", "0.772242" },
            { "mobLocus.linearYPeriod.value", "8950.356445" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "0.945055" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.617076" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "-0.174233" },
            { "mobLocus.proximityRadius.periodic.period.value", "6507.999023" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "-0.137600" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "6226.518066" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "-0.231112" },
            { "mobLocus.proximityRadius.value.value", "1841.532959" },
            { "mobLocus.proximityRadius.valueType", "constant" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.218509" },
            { "mobLocus.randomPeriod.value", "6845.955078" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "-0.314998" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "0.029263" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "0.732232" },
            { "mobLocus.randomWeight.periodic.period.value", "489.365662" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-0.183890" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "7318.515137" },
            { "mobLocus.randomWeight.value.mobJitterScale", "-0.566317" },
            { "mobLocus.randomWeight.value.value", "-2.884803" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "TRUE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "419.863647" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.543370" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.value", "-1.000000" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.mobJitterScale", "0.754791" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.value", "6169.114746" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.292043" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.value", "4220.091309" },
            { "nearBaseRandomIdle.crowd.radius.value.mobJitterScale", "-0.617957" },
            { "nearBaseRandomIdle.crowd.radius.value.value", "357.662598" },
            { "nearBaseRandomIdle.crowd.radius.valueType", "periodic" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.mobJitterScale", "0.573385" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.value", "-0.001929" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.value", "5881.708496" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.mobJitterScale", "0.000277" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.value", "2097.240234" },
            { "nearBaseRandomIdle.crowd.size.value.mobJitterScale", "0.548683" },
            { "nearBaseRandomIdle.crowd.size.value.value", "14.661386" },
            { "nearBaseRandomIdle.crowd.size.valueType", "periodic" },
            { "nearBaseRandomIdle.crowd.type.check", "quadraticUp" },
            { "nearBaseRandomIdle.forceOn", "FALSE" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.mobJitterScale", "-0.137507" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.value", "0.366586" },
            { "nearBaseRandomIdle.range.radius.periodic.period.mobJitterScale", "0.694100" },
            { "nearBaseRandomIdle.range.radius.periodic.period.value", "5231.827637" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.682309" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.value", "9000.000000" },
            { "nearBaseRandomIdle.range.radius.value.mobJitterScale", "-0.656586" },
            { "nearBaseRandomIdle.range.radius.value.value", "189.456436" },
            { "nearBaseRandomIdle.range.radius.valueType", "periodic" },
            { "nearBaseRandomIdle.range.type.check", "linearUp" },
            { "nearBaseRandomIdle.value.periodic.amplitude.mobJitterScale", "-0.928547" },
            { "nearBaseRandomIdle.value.periodic.amplitude.value", "-0.312219" },
            { "nearBaseRandomIdle.value.periodic.period.mobJitterScale", "0.115528" },
            { "nearBaseRandomIdle.value.periodic.period.value", "5145.938477" },
            { "nearBaseRandomIdle.value.periodic.tickShift.mobJitterScale", "-0.283370" },
            { "nearBaseRandomIdle.value.periodic.tickShift.value", "3928.133789" },
            { "nearBaseRandomIdle.value.value.mobJitterScale", "-0.516429" },
            { "nearBaseRandomIdle.value.value.value", "0.417113" },
            { "nearBaseRandomIdle.value.valueType", "periodic" },
            { "nearestFriend.crowd.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "nearestFriend.crowd.radius.periodic.amplitude.value", "-1.000000" },
            { "nearestFriend.crowd.radius.periodic.period.mobJitterScale", "-0.568690" },
            { "nearestFriend.crowd.radius.periodic.period.value", "5154.605957" },
            { "nearestFriend.crowd.radius.periodic.tickShift.mobJitterScale", "-0.471245" },
            { "nearestFriend.crowd.radius.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.crowd.radius.value.mobJitterScale", "0.994417" },
            { "nearestFriend.crowd.radius.value.value", "1240.379395" },
            { "nearestFriend.crowd.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "-0.836207" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "-0.893575" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "-0.810000" },
            { "nearestFriend.crowd.size.periodic.period.value", "5798.069336" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "0.172499" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "8629.155273" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "-0.241560" },
            { "nearestFriend.crowd.size.value.value", "-0.900000" },
            { "nearestFriend.crowd.size.valueType", "constant" },
            { "nearestFriend.crowd.type.check", "quadraticUp" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.727034" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.882090" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "nearestFriend.range.radius.periodic.period.value", "5182.228516" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "0.144608" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "1070.603760" },
            { "nearestFriend.range.radius.value.mobJitterScale", "-0.101394" },
            { "nearestFriend.range.radius.value.value", "1049.323364" },
            { "nearestFriend.range.radius.valueType", "constant" },
            { "nearestFriend.range.type.check", "quadraticUp" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "-0.808005" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.457324" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "-0.554452" },
            { "nearestFriend.weight.periodic.period.value", "3722.965576" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "nearestFriend.weight.periodic.tickShift.value", "3934.461426" },
            { "nearestFriend.weight.value.mobJitterScale", "-0.088786" },
            { "nearestFriend.weight.value.value", "9.500000" },
            { "nearestFriend.weight.valueType", "constant" },
            { "randomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.799510" },
            { "randomIdle.crowd.radius.periodic.amplitude.value", "0.057795" },
            { "randomIdle.crowd.radius.periodic.period.mobJitterScale", "-0.691812" },
            { "randomIdle.crowd.radius.periodic.period.value", "952.289062" },
            { "randomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "randomIdle.crowd.radius.periodic.tickShift.value", "8266.105469" },
            { "randomIdle.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "randomIdle.crowd.radius.value.value", "805.062378" },
            { "randomIdle.crowd.radius.valueType", "constant" },
            { "randomIdle.crowd.size.periodic.amplitude.mobJitterScale", "-0.916631" },
            { "randomIdle.crowd.size.periodic.amplitude.value", "-0.208831" },
            { "randomIdle.crowd.size.periodic.period.mobJitterScale", "-0.173030" },
            { "randomIdle.crowd.size.periodic.period.value", "9356.440430" },
            { "randomIdle.crowd.size.periodic.tickShift.mobJitterScale", "-0.846715" },
            { "randomIdle.crowd.size.periodic.tickShift.value", "865.506165" },
            { "randomIdle.crowd.size.value.mobJitterScale", "-0.788593" },
            { "randomIdle.crowd.size.value.value", "17.264877" },
            { "randomIdle.crowd.size.valueType", "constant" },
            { "randomIdle.crowd.type.check", "never" },
            { "randomIdle.forceOn", "FALSE" },
            { "randomIdle.range.radius.periodic.amplitude.mobJitterScale", "0.616091" },
            { "randomIdle.range.radius.periodic.amplitude.value", "-0.338591" },
            { "randomIdle.range.radius.periodic.period.mobJitterScale", "-0.739726" },
            { "randomIdle.range.radius.periodic.period.value", "4059.509033" },
            { "randomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.512191" },
            { "randomIdle.range.radius.periodic.tickShift.value", "9000.000000" },
            { "randomIdle.range.radius.value.mobJitterScale", "-0.460660" },
            { "randomIdle.range.radius.value.value", "141.254730" },
            { "randomIdle.range.radius.valueType", "periodic" },
            { "randomIdle.value.periodic.amplitude.mobJitterScale", "-0.332267" },
            { "randomIdle.value.periodic.amplitude.value", "-0.458042" },
            { "randomIdle.value.periodic.period.mobJitterScale", "0.374727" },
            { "randomIdle.value.periodic.period.value", "7474.683105" },
            { "randomIdle.value.periodic.tickShift.mobJitterScale", "-0.113656" },
            { "randomIdle.value.periodic.tickShift.value", "1706.542114" },
            { "randomIdle.value.value.mobJitterScale", "0.654549" },
            { "randomIdle.value.value.value", "-0.550370" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.mobJitterScale", "0.257366" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.value", "-0.891000" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.mobJitterScale", "0.254993" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.value", "7390.003418" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.mobJitterScale", "0.222650" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.value", "8726.643555" },
            { "randomizeStoppedVelocity.crowd.radius.value.mobJitterScale", "0.445493" },
            { "randomizeStoppedVelocity.crowd.radius.value.value", "1428.234131" },
            { "randomizeStoppedVelocity.crowd.radius.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.mobJitterScale", "-0.578646" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.value", "0.414475" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.mobJitterScale", "-0.515798" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.value", "7381.770020" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.mobJitterScale", "-0.549884" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.value", "5734.664062" },
            { "randomizeStoppedVelocity.crowd.size.value.mobJitterScale", "-0.283496" },
            { "randomizeStoppedVelocity.crowd.size.value.value", "2.215379" },
            { "randomizeStoppedVelocity.crowd.size.valueType", "constant" },
            { "randomizeStoppedVelocity.crowd.type.check", "strictOff" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.mobJitterScale", "0.075649" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.value", "-0.575432" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.mobJitterScale", "0.792350" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.value", "1887.948120" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.mobJitterScale", "0.180988" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.value", "5820.470215" },
            { "randomizeStoppedVelocity.range.radius.value.mobJitterScale", "0.643894" },
            { "randomizeStoppedVelocity.range.radius.value.value", "1125.777344" },
            { "randomizeStoppedVelocity.range.radius.valueType", "constant" },
            { "randomizeStoppedVelocity.range.type.check", "strictOn" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.mobJitterScale", "0.307468" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.value", "0.236314" },
            { "randomizeStoppedVelocity.value.periodic.period.mobJitterScale", "1.000000" },
            { "randomizeStoppedVelocity.value.periodic.period.value", "4497.551270" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.mobJitterScale", "0.677065" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.value", "6900.911133" },
            { "randomizeStoppedVelocity.value.value.mobJitterScale", "0.384728" },
            { "randomizeStoppedVelocity.value.value.value", "-0.658036" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "11.820508" },
            { "sensorGrid.staleFighterTime", "0.981725" },
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.437849" },
            { "separate.crowd.radius.periodic.amplitude.value", "0.209670" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "-0.760054" },
            { "separate.crowd.radius.periodic.period.value", "2413.002686" },
            { "separate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.904279" },
            { "separate.crowd.radius.periodic.tickShift.value", "-1.000000" },
            { "separate.crowd.radius.value.mobJitterScale", "-0.643006" },
            { "separate.crowd.radius.value.value", "889.271790" },
            { "separate.crowd.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "-0.120033" },
            { "separate.crowd.size.periodic.amplitude.value", "0.993655" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "0.166268" },
            { "separate.crowd.size.periodic.period.value", "4436.318848" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.076504" },
            { "separate.crowd.size.periodic.tickShift.value", "8151.985352" },
            { "separate.crowd.size.value.mobJitterScale", "0.529263" },
            { "separate.crowd.size.value.value", "2.305166" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowd.type.check", "always" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.810000" },
            { "separate.range.radius.periodic.amplitude.value", "-0.882092" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.345343" },
            { "separate.range.radius.periodic.period.value", "502.389557" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "separate.range.radius.periodic.tickShift.value", "3098.444092" },
            { "separate.range.radius.value.mobJitterScale", "0.415732" },
            { "separate.range.radius.value.value", "690.273804" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.range.type.check", "never" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "0.693834" },
            { "separate.weight.periodic.amplitude.value", "0.198130" },
            { "separate.weight.periodic.period.mobJitterScale", "0.526422" },
            { "separate.weight.periodic.period.value", "4799.893066" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.938787" },
            { "separate.weight.periodic.tickShift.value", "2242.375000" },
            { "separate.weight.value.mobJitterScale", "0.873005" },
            { "separate.weight.value.value", "-7.571142" },
            { "separate.weight.valueType", "periodic" },
            { "simpleAttack.crowd.radius.periodic.amplitude.mobJitterScale", "-0.596540" },
            { "simpleAttack.crowd.radius.periodic.amplitude.value", "-0.744557" },
            { "simpleAttack.crowd.radius.periodic.period.mobJitterScale", "-0.458995" },
            { "simpleAttack.crowd.radius.periodic.period.value", "2733.369629" },
            { "simpleAttack.crowd.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "simpleAttack.crowd.radius.periodic.tickShift.value", "1068.301270" },
            { "simpleAttack.crowd.radius.value.mobJitterScale", "0.972673" },
            { "simpleAttack.crowd.radius.value.value", "1381.970459" },
            { "simpleAttack.crowd.radius.valueType", "periodic" },
            { "simpleAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.414414" },
            { "simpleAttack.crowd.size.periodic.amplitude.value", "0.289823" },
            { "simpleAttack.crowd.size.periodic.period.mobJitterScale", "0.687181" },
            { "simpleAttack.crowd.size.periodic.period.value", "7096.642090" },
            { "simpleAttack.crowd.size.periodic.tickShift.mobJitterScale", "-0.210048" },
            { "simpleAttack.crowd.size.periodic.tickShift.value", "2487.355713" },
            { "simpleAttack.crowd.size.value.mobJitterScale", "-0.127022" },
            { "simpleAttack.crowd.size.value.value", "12.383728" },
            { "simpleAttack.crowd.size.valueType", "periodic" },
            { "simpleAttack.crowd.type.check", "strictOn" },
            { "simpleAttack.forceOn", "TRUE" },
            { "simpleAttack.range.radius.periodic.amplitude.mobJitterScale", "0.884639" },
            { "simpleAttack.range.radius.periodic.amplitude.value", "-0.900000" },
            { "simpleAttack.range.radius.periodic.period.mobJitterScale", "-0.507467" },
            { "simpleAttack.range.radius.periodic.period.value", "1.210000" },
            { "simpleAttack.range.radius.periodic.tickShift.mobJitterScale", "0.150215" },
            { "simpleAttack.range.radius.periodic.tickShift.value", "10000.000000" },
            { "simpleAttack.range.radius.value.mobJitterScale", "-0.144790" },
            { "simpleAttack.range.radius.value.value", "1297.833984" },
            { "simpleAttack.range.radius.valueType", "constant" },
            { "simpleAttack.range.type.check", "quadraticDown" },
            { "simpleAttack.value.periodic.amplitude.mobJitterScale", "1.000000" },
            { "simpleAttack.value.periodic.amplitude.value", "-0.581230" },
            { "simpleAttack.value.periodic.period.mobJitterScale", "0.311004" },
            { "simpleAttack.value.periodic.period.value", "8602.618164" },
            { "simpleAttack.value.periodic.tickShift.mobJitterScale", "-0.685915" },
            { "simpleAttack.value.periodic.tickShift.value", "6025.565430" },
            { "simpleAttack.value.value.mobJitterScale", "0.724662" },
            { "simpleAttack.value.value.value", "0.083469" },
            { "simpleAttack.value.valueType", "constant" },
            { "startingMaxRadius", "1298.302246" },
            { "startingMinRadius", "491.606903" },
        };
        BundleConfigValue configs10[] = {
            { "align.crowd.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "align.crowd.radius.periodic.amplitude.value", "-0.900000" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "-0.691816" },
            { "align.crowd.radius.periodic.period.value", "1798.796875" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.729568" },
            { "align.crowd.radius.periodic.tickShift.value", "231.923737" },
            { "align.crowd.radius.value.mobJitterScale", "0.470928" },
            { "align.crowd.radius.value.value", "632.786682" },
            { "align.crowd.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.810330" },
            { "align.crowd.size.periodic.amplitude.value", "-0.369636" },
            { "align.crowd.size.periodic.period.mobJitterScale", "-0.324168" },
            { "align.crowd.size.periodic.period.value", "2966.139160" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.044981" },
            { "align.crowd.size.periodic.tickShift.value", "5687.129395" },
            { "align.crowd.size.value.mobJitterScale", "-0.829904" },
            { "align.crowd.size.value.value", "4.202988" },
            { "align.crowd.size.valueType", "constant" },
            { "align.crowd.type.check", "linearUp" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.294180" },
            { "align.range.radius.periodic.amplitude.value", "-0.475352" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.197922" },
            { "align.range.radius.periodic.period.value", "10000.000000" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.732726" },
            { "align.range.radius.periodic.tickShift.value", "2145.731445" },
            { "align.range.radius.value.mobJitterScale", "0.810000" },
            { "align.range.radius.value.value", "308.747650" },
            { "align.range.radius.valueType", "constant" },
            { "align.range.type.check", "linearDown" },
            { "align.weight.periodic.amplitude.mobJitterScale", "-0.508970" },
            { "align.weight.periodic.amplitude.value", "-0.468956" },
            { "align.weight.periodic.period.mobJitterScale", "-0.640049" },
            { "align.weight.periodic.period.value", "5989.284180" },
            { "align.weight.periodic.tickShift.mobJitterScale", "-0.418389" },
            { "align.weight.periodic.tickShift.value", "1992.849365" },
            { "align.weight.value.mobJitterScale", "1.000000" },
            { "align.weight.value.value", "8.221008" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "124.299881" },
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.283124" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "-0.392328" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "-0.991106" },
            { "attackSeparate.crowd.radius.periodic.period.value", "6389.703125" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "0.840854" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "3192.095215" },
            { "attackSeparate.crowd.radius.value.mobJitterScale", "0.682592" },
            { "attackSeparate.crowd.radius.value.value", "2000.000000" },
            { "attackSeparate.crowd.radius.valueType", "constant" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.613588" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.368364" },
            { "attackSeparate.crowd.size.periodic.period.value", "6300.551270" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-0.155625" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "6720.401855" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "0.298921" },
            { "attackSeparate.crowd.size.value.value", "15.635868" },
            { "attackSeparate.crowd.size.valueType", "constant" },
            { "attackSeparate.crowd.type.check", "quadraticUp" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "-0.811765" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.519085" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.424997" },
            { "attackSeparate.range.radius.periodic.period.value", "7989.664551" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "-0.060889" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "6487.900879" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.399910" },
            { "attackSeparate.range.radius.value.value", "1144.927368" },
            { "attackSeparate.range.type.check", "strictOn" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.445871" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.880321" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.106112" },
            { "attackSeparate.weight.periodic.period.value", "1840.722412" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-0.769784" },
            { "attackSeparate.weight.periodic.tickShift.value", "6223.354492" },
            { "attackSeparate.weight.value.mobJitterScale", "-0.234230" },
            { "attackSeparate.weight.value.value", "-0.918193" },
            { "attackSeparate.weight.valueType", "constant" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "-0.099353" },
            { "base.crowd.radius.periodic.amplitude.value", "0.959933" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "-0.576180" },
            { "base.crowd.radius.periodic.period.value", "978.798157" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "0.020125" },
            { "base.crowd.radius.periodic.tickShift.value", "2848.047119" },
            { "base.crowd.radius.value.mobJitterScale", "0.569233" },
            { "base.crowd.radius.value.value", "142.953690" },
            { "base.crowd.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "0.463663" },
            { "base.crowd.size.periodic.amplitude.value", "1.000000" },
            { "base.crowd.size.periodic.period.mobJitterScale", "0.867146" },
            { "base.crowd.size.periodic.period.value", "5257.515137" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "-0.581489" },
            { "base.crowd.size.periodic.tickShift.value", "4430.978516" },
            { "base.crowd.size.value.mobJitterScale", "-0.422955" },
            { "base.crowd.size.value.value", "7.941950" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "quadraticDown" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.049186" },
            { "base.range.radius.periodic.amplitude.value", "0.793770" },
            { "base.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "base.range.radius.periodic.period.value", "7980.264648" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.381440" },
            { "base.range.radius.periodic.tickShift.value", "1936.666992" },
            { "base.range.radius.value.mobJitterScale", "-0.062679" },
            { "base.range.radius.value.value", "2000.000000" },
            { "base.range.radius.valueType", "periodic" },
            { "base.range.type.check", "linearDown" },
            { "base.weight.periodic.amplitude.mobJitterScale", "-0.877844" },
            { "base.weight.periodic.amplitude.value", "-0.137834" },
            { "base.weight.periodic.period.mobJitterScale", "-0.429423" },
            { "base.weight.periodic.period.value", "6329.613281" },
            { "base.weight.periodic.tickShift.mobJitterScale", "0.183000" },
            { "base.weight.periodic.tickShift.value", "10000.000000" },
            { "base.weight.value.mobJitterScale", "-0.451899" },
            { "base.weight.value.value", "9.407269" },
            { "base.weight.valueType", "constant" },
            { "baseDefense.crowd.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "-0.796597" },
            { "baseDefense.crowd.radius.periodic.period.mobJitterScale", "0.670054" },
            { "baseDefense.crowd.radius.periodic.period.value", "2973.006348" },
            { "baseDefense.crowd.radius.periodic.tickShift.mobJitterScale", "0.450159" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "10000.000000" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "1.000000" },
            { "baseDefense.crowd.radius.value.value", "482.100220" },
            { "baseDefense.crowd.radius.valueType", "periodic" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "-0.659387" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.140207" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "-0.127713" },
            { "baseDefense.crowd.size.periodic.period.value", "5562.273438" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "0.203827" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "2458.634521" },
            { "baseDefense.crowd.size.value.mobJitterScale", "0.042389" },
            { "baseDefense.crowd.size.value.value", "1.991738" },
            { "baseDefense.crowd.size.valueType", "periodic" },
            { "baseDefense.crowd.type.check", "quadraticUp" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.469328" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.706059" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "0.141152" },
            { "baseDefense.range.radius.periodic.period.value", "5349.272461" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "baseDefense.range.radius.periodic.tickShift.value", "1837.093384" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.472613" },
            { "baseDefense.range.radius.value.value", "1800.000000" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "linearDown" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.778694" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "0.781695" },
            { "baseDefense.weight.periodic.period.value", "3751.967285" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.810006" },
            { "baseDefense.weight.periodic.tickShift.value", "902.187317" },
            { "baseDefense.weight.value.mobJitterScale", "0.705692" },
            { "baseDefense.weight.value.value", "3.582699" },
            { "baseDefense.weight.valueType", "periodic" },
            { "baseDefenseRadius", "464.254028" },
            { "center.crowd.radius.periodic.amplitude.mobJitterScale", "-0.364490" },
            { "center.crowd.radius.periodic.amplitude.value", "-0.250589" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "1.000000" },
            { "center.crowd.radius.periodic.period.value", "8367.868164" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "0.101893" },
            { "center.crowd.radius.periodic.tickShift.value", "4205.507812" },
            { "center.crowd.radius.value.mobJitterScale", "0.306647" },
            { "center.crowd.radius.value.value", "266.801758" },
            { "center.crowd.radius.valueType", "constant" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.737549" },
            { "center.crowd.size.periodic.amplitude.value", "-0.529379" },
            { "center.crowd.size.periodic.period.mobJitterScale", "0.821016" },
            { "center.crowd.size.periodic.period.value", "5231.730957" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "-0.780234" },
            { "center.crowd.size.periodic.tickShift.value", "4946.688965" },
            { "center.crowd.size.value.mobJitterScale", "0.675888" },
            { "center.crowd.size.value.value", "16.150549" },
            { "center.crowd.size.valueType", "periodic" },
            { "center.crowd.type.check", "linearUp" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.481605" },
            { "center.range.radius.periodic.amplitude.value", "-0.662367" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.461906" },
            { "center.range.radius.periodic.period.value", "8131.180176" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "-0.396484" },
            { "center.range.radius.periodic.tickShift.value", "-1.000000" },
            { "center.range.radius.value.mobJitterScale", "-0.730175" },
            { "center.range.radius.value.value", "517.262146" },
            { "center.range.radius.valueType", "periodic" },
            { "center.range.type.check", "quadraticDown" },
            { "center.weight.periodic.amplitude.mobJitterScale", "0.760360" },
            { "center.weight.periodic.amplitude.value", "-1.000000" },
            { "center.weight.periodic.period.mobJitterScale", "0.970897" },
            { "center.weight.periodic.period.value", "5315.549805" },
            { "center.weight.periodic.tickShift.mobJitterScale", "0.501683" },
            { "center.weight.periodic.tickShift.value", "5566.495605" },
            { "center.weight.value.mobJitterScale", "-1.000000" },
            { "center.weight.value.value", "-4.862716" },
            { "center.weight.valueType", "constant" },
            { "cohere.crowd.radius.periodic.amplitude.mobJitterScale", "-0.149388" },
            { "cohere.crowd.radius.periodic.amplitude.value", "0.164435" },
            { "cohere.crowd.radius.periodic.period.mobJitterScale", "-0.175335" },
            { "cohere.crowd.radius.periodic.period.value", "6190.380371" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "-0.023282" },
            { "cohere.crowd.radius.periodic.tickShift.value", "7150.455566" },
            { "cohere.crowd.radius.value.mobJitterScale", "-0.200967" },
            { "cohere.crowd.radius.value.value", "1214.944946" },
            { "cohere.crowd.radius.valueType", "periodic" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.310836" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.097434" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "-0.913731" },
            { "cohere.crowd.size.periodic.period.value", "1793.753662" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "-0.226249" },
            { "cohere.crowd.size.periodic.tickShift.value", "1132.748657" },
            { "cohere.crowd.size.value.mobJitterScale", "-1.000000" },
            { "cohere.crowd.size.value.value", "4.382195" },
            { "cohere.crowd.size.valueType", "constant" },
            { "cohere.crowd.type.check", "strictOff" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "-0.862973" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.410542" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "cohere.range.radius.periodic.period.value", "9017.322266" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "0.780676" },
            { "cohere.range.radius.periodic.tickShift.value", "4879.970703" },
            { "cohere.range.radius.value.mobJitterScale", "0.350038" },
            { "cohere.range.radius.value.value", "194.093262" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "linearDown" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "-0.228821" },
            { "cohere.weight.periodic.amplitude.value", "-0.608268" },
            { "cohere.weight.periodic.period.mobJitterScale", "-0.835344" },
            { "cohere.weight.periodic.period.value", "9267.500977" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.214946" },
            { "cohere.weight.periodic.tickShift.value", "4365.213867" },
            { "cohere.weight.value.mobJitterScale", "0.774157" },
            { "cohere.weight.value.value", "3.392372" },
            { "cohere.weight.valueType", "constant" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "-0.679230" },
            { "cores.crowd.radius.periodic.amplitude.value", "-0.698427" },
            { "cores.crowd.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "cores.crowd.radius.periodic.period.value", "6261.778320" },
            { "cores.crowd.radius.periodic.tickShift.mobJitterScale", "-0.603218" },
            { "cores.crowd.radius.periodic.tickShift.value", "2143.066162" },
            { "cores.crowd.radius.value.mobJitterScale", "-0.883714" },
            { "cores.crowd.radius.value.value", "1817.128784" },
            { "cores.crowd.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "-0.423732" },
            { "cores.crowd.size.periodic.amplitude.value", "1.000000" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "0.990000" },
            { "cores.crowd.size.periodic.period.value", "5381.442871" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.583181" },
            { "cores.crowd.size.periodic.tickShift.value", "4713.194336" },
            { "cores.crowd.size.value.mobJitterScale", "0.530307" },
            { "cores.crowd.size.value.value", "19.724854" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowd.type.check", "linearDown" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.349700" },
            { "cores.range.radius.periodic.amplitude.value", "-0.056963" },
            { "cores.range.radius.periodic.period.mobJitterScale", "-0.588563" },
            { "cores.range.radius.periodic.period.value", "-1.000000" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.280803" },
            { "cores.range.radius.periodic.tickShift.value", "2862.066895" },
            { "cores.range.radius.value.mobJitterScale", "0.960637" },
            { "cores.range.radius.value.value", "1521.486206" },
            { "cores.range.radius.valueType", "constant" },
            { "cores.range.type.check", "linearDown" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.974489" },
            { "cores.weight.periodic.amplitude.value", "0.751996" },
            { "cores.weight.periodic.period.mobJitterScale", "0.240747" },
            { "cores.weight.periodic.period.value", "2743.106934" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.644266" },
            { "cores.weight.periodic.tickShift.value", "5772.048340" },
            { "cores.weight.value.mobJitterScale", "-0.300920" },
            { "cores.weight.value.value", "-9.646600" },
            { "cores.weight.valueType", "periodic" },
            { "corners.crowd.radius.periodic.amplitude.mobJitterScale", "0.667311" },
            { "corners.crowd.radius.periodic.amplitude.value", "0.491800" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "0.878670" },
            { "corners.crowd.radius.periodic.period.value", "10000.000000" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "-0.735246" },
            { "corners.crowd.radius.periodic.tickShift.value", "1294.500610" },
            { "corners.crowd.radius.value.mobJitterScale", "-0.059881" },
            { "corners.crowd.radius.value.value", "1559.275635" },
            { "corners.crowd.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "0.286347" },
            { "corners.crowd.size.periodic.amplitude.value", "0.065476" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "corners.crowd.size.periodic.period.value", "8437.597656" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.474595" },
            { "corners.crowd.size.periodic.tickShift.value", "9880.130859" },
            { "corners.crowd.size.value.mobJitterScale", "-0.050492" },
            { "corners.crowd.size.value.value", "-0.900000" },
            { "corners.crowd.type.check", "strictOff" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "-0.551995" },
            { "corners.range.radius.periodic.amplitude.value", "-0.310372" },
            { "corners.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "corners.range.radius.periodic.period.value", "944.186584" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-0.383130" },
            { "corners.range.radius.periodic.tickShift.value", "5785.993164" },
            { "corners.range.radius.value.mobJitterScale", "0.571513" },
            { "corners.range.radius.value.value", "1772.390381" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.range.type.check", "quadraticDown" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "0.536297" },
            { "corners.weight.periodic.amplitude.value", "-0.799886" },
            { "corners.weight.periodic.period.mobJitterScale", "0.469667" },
            { "corners.weight.periodic.period.value", "7715.052734" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "-0.281604" },
            { "corners.weight.periodic.tickShift.value", "8695.443359" },
            { "corners.weight.value.mobJitterScale", "-0.221811" },
            { "corners.weight.value.value", "5.590372" },
            { "corners.weight.valueType", "constant" },
            { "creditReserve", "102.333519" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.174617" },
            { "curHeadingWeight.periodic.amplitude.value", "0.147980" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "-0.621471" },
            { "curHeadingWeight.periodic.period.value", "7468.846191" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "-0.400198" },
            { "curHeadingWeight.periodic.tickShift.value", "2122.291748" },
            { "curHeadingWeight.value.mobJitterScale", "-0.519141" },
            { "curHeadingWeight.value.value", "-2.750601" },
            { "curHeadingWeight.valueType", "periodic" },
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "-0.134105" },
            { "edges.crowd.radius.periodic.amplitude.value", "0.158105" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "-0.841231" },
            { "edges.crowd.radius.periodic.period.value", "4465.393066" },
            { "edges.crowd.radius.periodic.tickShift.mobJitterScale", "0.251655" },
            { "edges.crowd.radius.periodic.tickShift.value", "3110.500977" },
            { "edges.crowd.radius.value.mobJitterScale", "0.078667" },
            { "edges.crowd.radius.value.value", "1845.644165" },
            { "edges.crowd.radius.valueType", "periodic" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.414017" },
            { "edges.crowd.size.periodic.amplitude.value", "-0.168098" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "0.687331" },
            { "edges.crowd.size.periodic.period.value", "1243.765991" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "-0.119820" },
            { "edges.crowd.size.periodic.tickShift.value", "4742.781250" },
            { "edges.crowd.size.value.mobJitterScale", "-0.926846" },
            { "edges.crowd.size.value.value", "-0.552850" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.crowd.type.check", "linearDown" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.810000" },
            { "edges.range.radius.periodic.amplitude.value", "-0.627248" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.016586" },
            { "edges.range.radius.periodic.period.value", "3429.769043" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "edges.range.radius.periodic.tickShift.value", "5030.486816" },
            { "edges.range.radius.value.mobJitterScale", "0.087751" },
            { "edges.range.radius.value.value", "1546.610962" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.range.type.check", "quadraticUp" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.113825" },
            { "edges.weight.periodic.amplitude.value", "0.727313" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.784871" },
            { "edges.weight.periodic.period.value", "8067.824219" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.167448" },
            { "edges.weight.periodic.tickShift.value", "1734.529541" },
            { "edges.weight.value.mobJitterScale", "0.424132" },
            { "edges.weight.value.value", "-9.107150" },
            { "edges.weight.valueType", "constant" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "0.059779" },
            { "enemy.crowd.radius.periodic.amplitude.value", "-0.580612" },
            { "enemy.crowd.radius.periodic.period.mobJitterScale", "0.557126" },
            { "enemy.crowd.radius.periodic.period.value", "6273.198242" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "0.800650" },
            { "enemy.crowd.radius.periodic.tickShift.value", "8975.989258" },
            { "enemy.crowd.radius.value.mobJitterScale", "0.413589" },
            { "enemy.crowd.radius.value.value", "869.415649" },
            { "enemy.crowd.radius.valueType", "periodic" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.546004" },
            { "enemy.crowd.size.periodic.amplitude.value", "0.415611" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.960799" },
            { "enemy.crowd.size.periodic.period.value", "3322.522461" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "-0.900000" },
            { "enemy.crowd.size.periodic.tickShift.value", "9985.978516" },
            { "enemy.crowd.size.value.mobJitterScale", "-0.034810" },
            { "enemy.crowd.size.value.value", "6.908399" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "strictOn" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "-0.308143" },
            { "enemy.range.radius.periodic.amplitude.value", "-1.000000" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "0.015572" },
            { "enemy.range.radius.periodic.period.value", "1360.034790" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.299372" },
            { "enemy.range.radius.periodic.tickShift.value", "3672.141113" },
            { "enemy.range.radius.value.mobJitterScale", "0.949890" },
            { "enemy.range.radius.value.value", "1630.439087" },
            { "enemy.range.radius.valueType", "periodic" },
            { "enemy.range.type.check", "linearDown" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "0.115470" },
            { "enemy.weight.periodic.amplitude.value", "0.639919" },
            { "enemy.weight.periodic.period.mobJitterScale", "-0.286482" },
            { "enemy.weight.periodic.period.value", "2736.425049" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.550962" },
            { "enemy.weight.periodic.tickShift.value", "9848.288086" },
            { "enemy.weight.value.mobJitterScale", "1.000000" },
            { "enemy.weight.value.value", "3.631045" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.crowd.radius.periodic.amplitude.mobJitterScale", "-0.354788" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "-0.441138" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "0.793888" },
            { "enemyBase.crowd.radius.periodic.period.value", "7806.381348" },
            { "enemyBase.crowd.radius.periodic.tickShift.mobJitterScale", "-0.244113" },
            { "enemyBase.crowd.radius.periodic.tickShift.value", "4381.927734" },
            { "enemyBase.crowd.radius.value.mobJitterScale", "0.840908" },
            { "enemyBase.crowd.radius.value.value", "1935.165649" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "-0.050773" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.151414" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "0.303969" },
            { "enemyBase.crowd.size.periodic.period.value", "-0.990000" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.191543" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "1864.172729" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.055287" },
            { "enemyBase.crowd.size.value.value", "4.233061" },
            { "enemyBase.crowd.size.valueType", "periodic" },
            { "enemyBase.crowd.type.check", "strictOff" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "-0.086430" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.782476" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "0.173767" },
            { "enemyBase.range.radius.periodic.period.value", "7776.569336" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "-0.212447" },
            { "enemyBase.range.radius.periodic.tickShift.value", "1500.840210" },
            { "enemyBase.range.radius.value.mobJitterScale", "0.713539" },
            { "enemyBase.range.radius.value.value", "645.297119" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.range.type.check", "quadraticDown" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.582992" },
            { "enemyBase.weight.periodic.amplitude.value", "-0.430087" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "-0.383820" },
            { "enemyBase.weight.periodic.period.value", "7823.189941" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "enemyBase.weight.periodic.tickShift.value", "5556.149902" },
            { "enemyBase.weight.value.mobJitterScale", "-0.502794" },
            { "enemyBase.weight.value.value", "-7.757640" },
            { "enemyBase.weight.valueType", "periodic" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.mobJitterScale", "-0.242825" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.value", "-0.283187" },
            { "enemyBaseGuess.crowd.radius.periodic.period.mobJitterScale", "-0.158584" },
            { "enemyBaseGuess.crowd.radius.periodic.period.value", "5243.994141" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.mobJitterScale", "0.571204" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.value", "3268.607910" },
            { "enemyBaseGuess.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "enemyBaseGuess.crowd.radius.value.value", "1193.494141" },
            { "enemyBaseGuess.crowd.radius.valueType", "periodic" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.mobJitterScale", "-0.171676" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.value", "0.521818" },
            { "enemyBaseGuess.crowd.size.periodic.period.mobJitterScale", "0.567339" },
            { "enemyBaseGuess.crowd.size.periodic.period.value", "4931.866211" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.mobJitterScale", "0.459770" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.value", "10000.000000" },
            { "enemyBaseGuess.crowd.size.value.mobJitterScale", "-0.971985" },
            { "enemyBaseGuess.crowd.size.value.value", "-1.000000" },
            { "enemyBaseGuess.crowd.size.valueType", "periodic" },
            { "enemyBaseGuess.crowd.type.check", "linearDown" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.mobJitterScale", "-0.591465" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.value", "0.682658" },
            { "enemyBaseGuess.range.radius.periodic.period.mobJitterScale", "-0.438287" },
            { "enemyBaseGuess.range.radius.periodic.period.value", "2348.483398" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.mobJitterScale", "-0.019503" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.value", "4192.981445" },
            { "enemyBaseGuess.range.radius.value.mobJitterScale", "0.960322" },
            { "enemyBaseGuess.range.radius.value.value", "388.181244" },
            { "enemyBaseGuess.range.radius.valueType", "constant" },
            { "enemyBaseGuess.range.type.check", "strictOff" },
            { "enemyBaseGuess.weight.periodic.amplitude.mobJitterScale", "0.816787" },
            { "enemyBaseGuess.weight.periodic.amplitude.value", "-0.277056" },
            { "enemyBaseGuess.weight.periodic.period.mobJitterScale", "0.398847" },
            { "enemyBaseGuess.weight.periodic.period.value", "5768.286621" },
            { "enemyBaseGuess.weight.periodic.tickShift.mobJitterScale", "0.333191" },
            { "enemyBaseGuess.weight.periodic.tickShift.value", "6739.786133" },
            { "enemyBaseGuess.weight.value.mobJitterScale", "-0.400666" },
            { "enemyBaseGuess.weight.value.value", "0.479086" },
            { "enemyBaseGuess.weight.valueType", "periodic" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "14.496975" },
            { "evadeStrictDistance", "266.458405" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fighterBaseDefenseUseRadius", "FALSE" },
            { "fleetLocus.circularPeriod", "7200.106445" },
            { "fleetLocus.circularWeight", "2.000000" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.665166" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.value", "-0.091571" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.841339" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "9364.166992" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "4173.372070" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "0.749074" },
            { "fleetLocus.force.crowd.radius.value.value", "960.142029" },
            { "fleetLocus.force.crowd.radius.valueType", "constant" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.491635" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.207075" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.473046" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "4711.096191" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.090672" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "6725.057617" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "0.360093" },
            { "fleetLocus.force.crowd.size.value.value", "17.862526" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "never" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.069907" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.279355" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.818148" },
            { "fleetLocus.force.range.radius.periodic.period.value", "3133.237305" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.794109" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "6333.916992" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.124718" },
            { "fleetLocus.force.range.radius.value.value", "1633.325928" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "0.142034" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.525882" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-1.000000" },
            { "fleetLocus.force.weight.periodic.period.value", "388.063812" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "0.761114" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "6457.437500" },
            { "fleetLocus.force.weight.value.mobJitterScale", "0.990000" },
            { "fleetLocus.force.weight.value.value", "-3.953883" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "1.012764" },
            { "fleetLocus.linearXPeriod", "11478.919922" },
            { "fleetLocus.linearYPeriod", "8414.062500" },
            { "fleetLocus.randomPeriod", "4214.676270" },
            { "fleetLocus.randomWeight", "1.137766" },
            { "fleetLocus.useScaled", "TRUE" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.mobJitterScale", "-0.457588" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.value", "-0.180419" },
            { "flockDuringAttack.crowd.radius.periodic.period.mobJitterScale", "0.846144" },
            { "flockDuringAttack.crowd.radius.periodic.period.value", "586.807983" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.156394" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.value", "5956.625488" },
            { "flockDuringAttack.crowd.radius.value.mobJitterScale", "0.259931" },
            { "flockDuringAttack.crowd.radius.value.value", "503.304413" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.242109" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.value", "-0.791837" },
            { "flockDuringAttack.crowd.size.periodic.period.mobJitterScale", "0.402957" },
            { "flockDuringAttack.crowd.size.periodic.period.value", "7997.995117" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.mobJitterScale", "0.838810" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.value", "7004.553223" },
            { "flockDuringAttack.crowd.size.value.mobJitterScale", "-0.322685" },
            { "flockDuringAttack.crowd.size.value.value", "18.360819" },
            { "flockDuringAttack.crowd.size.valueType", "periodic" },
            { "flockDuringAttack.crowd.type.check", "strictOn" },
            { "flockDuringAttack.forceOn", "FALSE" },
            { "flockDuringAttack.range.radius.periodic.amplitude.mobJitterScale", "0.279218" },
            { "flockDuringAttack.range.radius.periodic.amplitude.value", "1.000000" },
            { "flockDuringAttack.range.radius.periodic.period.mobJitterScale", "0.474327" },
            { "flockDuringAttack.range.radius.periodic.period.value", "8361.037109" },
            { "flockDuringAttack.range.radius.periodic.tickShift.mobJitterScale", "-0.926288" },
            { "flockDuringAttack.range.radius.periodic.tickShift.value", "8110.341797" },
            { "flockDuringAttack.range.radius.value.mobJitterScale", "-0.292470" },
            { "flockDuringAttack.range.radius.value.value", "1494.420898" },
            { "flockDuringAttack.range.radius.valueType", "constant" },
            { "flockDuringAttack.range.type.check", "strictOn" },
            { "flockDuringAttack.value.periodic.amplitude.mobJitterScale", "0.584826" },
            { "flockDuringAttack.value.periodic.amplitude.value", "-0.211422" },
            { "flockDuringAttack.value.periodic.period.mobJitterScale", "-0.226865" },
            { "flockDuringAttack.value.periodic.period.value", "9416.600586" },
            { "flockDuringAttack.value.periodic.tickShift.mobJitterScale", "-0.081237" },
            { "flockDuringAttack.value.periodic.tickShift.value", "4011.650146" },
            { "flockDuringAttack.value.value.mobJitterScale", "-0.136604" },
            { "flockDuringAttack.value.value.value", "0.789725" },
            { "flockDuringAttack.value.valueType", "constant" },
            { "gatherAbandonStale", "FALSE" },
            { "gatherRange", "59.900517" },
            { "guardRange", "123.226936" },
            { "hold.invProbability.periodic.amplitude.mobJitterScale", "0.672811" },
            { "hold.invProbability.periodic.amplitude.value", "1.000000" },
            { "hold.invProbability.periodic.period.mobJitterScale", "0.900000" },
            { "hold.invProbability.periodic.period.value", "3532.209961" },
            { "hold.invProbability.periodic.tickShift.mobJitterScale", "-0.057532" },
            { "hold.invProbability.periodic.tickShift.value", "7279.289062" },
            { "hold.invProbability.value.mobJitterScale", "-0.205668" },
            { "hold.invProbability.value.value", "6192.603516" },
            { "hold.invProbability.valueType", "constant" },
            { "hold.ticks.periodic.amplitude.mobJitterScale", "-0.886542" },
            { "hold.ticks.periodic.amplitude.value", "-0.575824" },
            { "hold.ticks.periodic.period.mobJitterScale", "-0.002287" },
            { "hold.ticks.periodic.period.value", "79.585701" },
            { "hold.ticks.periodic.tickShift.mobJitterScale", "0.467819" },
            { "hold.ticks.periodic.tickShift.value", "7055.355957" },
            { "hold.ticks.value.mobJitterScale", "0.514314" },
            { "hold.ticks.value.value", "458.636475" },
            { "hold.ticks.valueType", "constant" },
            { "mobLocus.circularPeriod.mobJitterScale", "0.698262" },
            { "mobLocus.circularPeriod.value", "5520.089355" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "-1.000000" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "-0.168818" },
            { "mobLocus.circularWeight.periodic.period.value", "4423.695801" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.659540" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "1040.052612" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.545375" },
            { "mobLocus.circularWeight.value.value", "-6.545949" },
            { "mobLocus.circularWeight.valueType", "periodic" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.151700" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.value", "0.808748" },
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.569871" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "2125.688965" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.799311" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.value", "1133.552734" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "-0.045709" },
            { "mobLocus.force.crowd.radius.value.value", "2000.000000" },
            { "mobLocus.force.crowd.radius.valueType", "periodic" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.163672" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "0.313566" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.900000" },
            { "mobLocus.force.crowd.size.periodic.period.value", "3883.383301" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.955086" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "3695.439697" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "0.465830" },
            { "mobLocus.force.crowd.size.value.value", "11.714092" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowd.type.check", "linearUp" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.779045" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.293748" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.403937" },
            { "mobLocus.force.range.radius.periodic.period.value", "1732.351318" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.582875" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "6104.679199" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.836546" },
            { "mobLocus.force.range.radius.value.value", "-1.000000" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "0.738715" },
            { "mobLocus.force.weight.periodic.amplitude.value", "0.441620" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.946954" },
            { "mobLocus.force.weight.periodic.period.value", "6479.931152" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.205459" },
            { "mobLocus.force.weight.periodic.tickShift.value", "6151.847656" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.274283" },
            { "mobLocus.force.weight.value.value", "-7.759153" },
            { "mobLocus.force.weight.valueType", "periodic" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "-0.517797" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.292005" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.393355" },
            { "mobLocus.linearWeight.periodic.period.value", "4845.236328" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "0.826168" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "6206.320801" },
            { "mobLocus.linearWeight.value.mobJitterScale", "-0.141062" },
            { "mobLocus.linearWeight.value.value", "-1.208004" },
            { "mobLocus.linearWeight.valueType", "periodic" },
            { "mobLocus.linearXPeriod.mobJitterScale", "0.635377" },
            { "mobLocus.linearXPeriod.value", "1388.244263" },
            { "mobLocus.linearYPeriod.mobJitterScale", "0.603713" },
            { "mobLocus.linearYPeriod.value", "9.297001" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "0.686090" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.433751" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "-0.189180" },
            { "mobLocus.proximityRadius.periodic.period.value", "4944.527344" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "-0.798153" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "6226.518066" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "0.138794" },
            { "mobLocus.proximityRadius.value.value", "1321.906860" },
            { "mobLocus.proximityRadius.valueType", "constant" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.184929" },
            { "mobLocus.randomPeriod.value", "9540.189453" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "-0.543336" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "-0.365342" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "0.782198" },
            { "mobLocus.randomWeight.periodic.period.value", "2076.366943" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-0.366500" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "2281.714111" },
            { "mobLocus.randomWeight.value.mobJitterScale", "-0.271931" },
            { "mobLocus.randomWeight.value.value", "9.419860" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "TRUE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "418.236877" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.579455" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.value", "-1.000000" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.mobJitterScale", "0.830270" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.value", "768.227600" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.734242" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.value", "3756.891846" },
            { "nearBaseRandomIdle.crowd.radius.value.mobJitterScale", "-0.350491" },
            { "nearBaseRandomIdle.crowd.radius.value.value", "1362.444336" },
            { "nearBaseRandomIdle.crowd.radius.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.mobJitterScale", "-0.346098" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.value", "-0.006268" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.value", "2561.712646" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.mobJitterScale", "0.184783" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.value", "56.242050" },
            { "nearBaseRandomIdle.crowd.size.value.mobJitterScale", "-0.976901" },
            { "nearBaseRandomIdle.crowd.size.value.value", "18.717873" },
            { "nearBaseRandomIdle.crowd.size.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.type.check", "never" },
            { "nearBaseRandomIdle.forceOn", "TRUE" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.mobJitterScale", "-0.534877" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.value", "0.553609" },
            { "nearBaseRandomIdle.range.radius.periodic.period.mobJitterScale", "0.909678" },
            { "nearBaseRandomIdle.range.radius.periodic.period.value", "-1.000000" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.mobJitterScale", "-0.524995" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.value", "7961.419434" },
            { "nearBaseRandomIdle.range.radius.value.mobJitterScale", "-0.656586" },
            { "nearBaseRandomIdle.range.radius.value.value", "1180.982666" },
            { "nearBaseRandomIdle.range.radius.valueType", "periodic" },
            { "nearBaseRandomIdle.range.type.check", "linearDown" },
            { "nearBaseRandomIdle.value.periodic.amplitude.mobJitterScale", "-0.203664" },
            { "nearBaseRandomIdle.value.periodic.amplitude.value", "0.109426" },
            { "nearBaseRandomIdle.value.periodic.period.mobJitterScale", "0.750148" },
            { "nearBaseRandomIdle.value.periodic.period.value", "3720.608154" },
            { "nearBaseRandomIdle.value.periodic.tickShift.mobJitterScale", "-0.255033" },
            { "nearBaseRandomIdle.value.periodic.tickShift.value", "3888.852539" },
            { "nearBaseRandomIdle.value.value.mobJitterScale", "-0.629861" },
            { "nearBaseRandomIdle.value.value.value", "0.203218" },
            { "nearBaseRandomIdle.value.valueType", "periodic" },
            { "nearestFriend.crowd.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "nearestFriend.crowd.radius.periodic.amplitude.value", "0.897347" },
            { "nearestFriend.crowd.radius.periodic.period.mobJitterScale", "-0.511821" },
            { "nearestFriend.crowd.radius.periodic.period.value", "4757.238770" },
            { "nearestFriend.crowd.radius.periodic.tickShift.mobJitterScale", "-0.339013" },
            { "nearestFriend.crowd.radius.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.crowd.radius.value.mobJitterScale", "1.000000" },
            { "nearestFriend.crowd.radius.value.value", "248.670761" },
            { "nearestFriend.crowd.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "-0.836207" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "-0.893575" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "nearestFriend.crowd.size.periodic.period.value", "-1.000000" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "-0.065194" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "8012.853516" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "0.151762" },
            { "nearestFriend.crowd.size.value.value", "5.010695" },
            { "nearestFriend.crowd.size.valueType", "periodic" },
            { "nearestFriend.crowd.type.check", "strictOff" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.502473" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.674670" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "nearestFriend.range.radius.periodic.period.value", "1313.188354" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "0.159069" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "1177.664185" },
            { "nearestFriend.range.radius.value.mobJitterScale", "-0.582810" },
            { "nearestFriend.range.radius.value.value", "551.695923" },
            { "nearestFriend.range.radius.valueType", "periodic" },
            { "nearestFriend.range.type.check", "quadraticUp" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.457324" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "0.391237" },
            { "nearestFriend.weight.periodic.period.value", "1907.174805" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "0.791203" },
            { "nearestFriend.weight.periodic.tickShift.value", "6720.436035" },
            { "nearestFriend.weight.value.mobJitterScale", "0.330642" },
            { "nearestFriend.weight.value.value", "-3.562106" },
            { "nearestFriend.weight.valueType", "constant" },
            { "randomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.307059" },
            { "randomIdle.crowd.radius.periodic.amplitude.value", "-0.162896" },
            { "randomIdle.crowd.radius.periodic.period.mobJitterScale", "-0.365779" },
            { "randomIdle.crowd.radius.periodic.period.value", "1441.625122" },
            { "randomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.900000" },
            { "randomIdle.crowd.radius.periodic.tickShift.value", "6444.324707" },
            { "randomIdle.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "randomIdle.crowd.radius.value.value", "1502.009521" },
            { "randomIdle.crowd.radius.valueType", "constant" },
            { "randomIdle.crowd.size.periodic.amplitude.mobJitterScale", "-0.884856" },
            { "randomIdle.crowd.size.periodic.amplitude.value", "0.189858" },
            { "randomIdle.crowd.size.periodic.period.mobJitterScale", "-0.291235" },
            { "randomIdle.crowd.size.periodic.period.value", "10000.000000" },
            { "randomIdle.crowd.size.periodic.tickShift.mobJitterScale", "-0.918040" },
            { "randomIdle.crowd.size.periodic.tickShift.value", "1047.262573" },
            { "randomIdle.crowd.size.value.mobJitterScale", "-0.671293" },
            { "randomIdle.crowd.size.value.value", "11.045941" },
            { "randomIdle.crowd.size.valueType", "constant" },
            { "randomIdle.crowd.type.check", "never" },
            { "randomIdle.forceOn", "FALSE" },
            { "randomIdle.range.radius.periodic.amplitude.mobJitterScale", "0.247853" },
            { "randomIdle.range.radius.periodic.amplitude.value", "0.350139" },
            { "randomIdle.range.radius.periodic.period.mobJitterScale", "0.933285" },
            { "randomIdle.range.radius.periodic.period.value", "5362.529785" },
            { "randomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.735377" },
            { "randomIdle.range.radius.periodic.tickShift.value", "7796.413086" },
            { "randomIdle.range.radius.value.mobJitterScale", "-0.414594" },
            { "randomIdle.range.radius.value.value", "1686.581055" },
            { "randomIdle.range.radius.valueType", "periodic" },
            { "randomIdle.range.type.check", "quadraticUp" },
            { "randomIdle.value.periodic.amplitude.mobJitterScale", "0.315928" },
            { "randomIdle.value.periodic.amplitude.value", "-0.864422" },
            { "randomIdle.value.periodic.period.mobJitterScale", "0.620510" },
            { "randomIdle.value.periodic.period.value", "4138.049316" },
            { "randomIdle.value.periodic.tickShift.mobJitterScale", "-0.285183" },
            { "randomIdle.value.periodic.tickShift.value", "2989.547852" },
            { "randomIdle.value.value.mobJitterScale", "0.964903" },
            { "randomIdle.value.value.value", "0.233979" },
            { "randomIdle.value.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.mobJitterScale", "-0.437236" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.value", "-1.000000" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.mobJitterScale", "0.277194" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.value", "5838.173340" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.mobJitterScale", "-0.170985" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.value", "8726.643555" },
            { "randomizeStoppedVelocity.crowd.radius.value.mobJitterScale", "0.579740" },
            { "randomizeStoppedVelocity.crowd.radius.value.value", "1831.464600" },
            { "randomizeStoppedVelocity.crowd.radius.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.mobJitterScale", "-0.927757" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.value", "0.660361" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.mobJitterScale", "-0.868219" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.value", "3727.939209" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.mobJitterScale", "-0.675153" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.value", "7136.440430" },
            { "randomizeStoppedVelocity.crowd.size.value.mobJitterScale", "-1.000000" },
            { "randomizeStoppedVelocity.crowd.size.value.value", "5.669626" },
            { "randomizeStoppedVelocity.crowd.size.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.type.check", "strictOff" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.mobJitterScale", "0.290846" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.value", "0.194707" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.mobJitterScale", "-0.043114" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.value", "2781.989258" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.mobJitterScale", "-0.484730" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.value", "1604.584717" },
            { "randomizeStoppedVelocity.range.radius.value.mobJitterScale", "0.549376" },
            { "randomizeStoppedVelocity.range.radius.value.value", "199.033676" },
            { "randomizeStoppedVelocity.range.radius.valueType", "constant" },
            { "randomizeStoppedVelocity.range.type.check", "strictOn" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.mobJitterScale", "0.340522" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.value", "0.233950" },
            { "randomizeStoppedVelocity.value.periodic.period.mobJitterScale", "1.000000" },
            { "randomizeStoppedVelocity.value.periodic.period.value", "7496.895996" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.mobJitterScale", "0.677065" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.value", "7515.092285" },
            { "randomizeStoppedVelocity.value.value.mobJitterScale", "0.423201" },
            { "randomizeStoppedVelocity.value.value.value", "-0.651456" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "11.820508" },
            { "sensorGrid.staleFighterTime", "1.049959" },
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.433471" },
            { "separate.crowd.radius.periodic.amplitude.value", "-0.383797" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "-0.519649" },
            { "separate.crowd.radius.periodic.period.value", "1262.466919" },
            { "separate.crowd.radius.periodic.tickShift.mobJitterScale", "0.660162" },
            { "separate.crowd.radius.periodic.tickShift.value", "-1.000000" },
            { "separate.crowd.radius.value.mobJitterScale", "-0.713536" },
            { "separate.crowd.radius.value.value", "813.972351" },
            { "separate.crowd.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "0.058213" },
            { "separate.crowd.size.periodic.amplitude.value", "0.810000" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "0.144464" },
            { "separate.crowd.size.periodic.period.value", "4605.658691" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.075739" },
            { "separate.crowd.size.periodic.tickShift.value", "7761.130371" },
            { "separate.crowd.size.value.mobJitterScale", "-0.066165" },
            { "separate.crowd.size.value.value", "3.740974" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowd.type.check", "always" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.001009" },
            { "separate.range.radius.periodic.amplitude.value", "-0.793883" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.483876" },
            { "separate.range.radius.periodic.period.value", "1714.172974" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "separate.range.radius.periodic.tickShift.value", "1875.857056" },
            { "separate.range.radius.value.mobJitterScale", "0.622203" },
            { "separate.range.radius.value.value", "945.052246" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.range.type.check", "quadraticDown" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.099326" },
            { "separate.weight.periodic.amplitude.value", "0.196149" },
            { "separate.weight.periodic.period.mobJitterScale", "0.487400" },
            { "separate.weight.periodic.period.value", "8264.724609" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.799375" },
            { "separate.weight.periodic.tickShift.value", "6818.379883" },
            { "separate.weight.value.mobJitterScale", "-0.280199" },
            { "separate.weight.value.value", "-2.949627" },
            { "separate.weight.valueType", "constant" },
            { "simpleAttack.crowd.radius.periodic.amplitude.mobJitterScale", "0.929708" },
            { "simpleAttack.crowd.radius.periodic.amplitude.value", "-0.757758" },
            { "simpleAttack.crowd.radius.periodic.period.mobJitterScale", "-0.709540" },
            { "simpleAttack.crowd.radius.periodic.period.value", "3868.532959" },
            { "simpleAttack.crowd.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "simpleAttack.crowd.radius.periodic.tickShift.value", "4702.195801" },
            { "simpleAttack.crowd.radius.value.mobJitterScale", "0.717424" },
            { "simpleAttack.crowd.radius.value.value", "1315.532593" },
            { "simpleAttack.crowd.radius.valueType", "constant" },
            { "simpleAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.117608" },
            { "simpleAttack.crowd.size.periodic.amplitude.value", "0.289823" },
            { "simpleAttack.crowd.size.periodic.period.mobJitterScale", "0.556617" },
            { "simpleAttack.crowd.size.periodic.period.value", "5331.951660" },
            { "simpleAttack.crowd.size.periodic.tickShift.mobJitterScale", "-0.207947" },
            { "simpleAttack.crowd.size.periodic.tickShift.value", "0.000000" },
            { "simpleAttack.crowd.size.value.mobJitterScale", "-0.129735" },
            { "simpleAttack.crowd.size.value.value", "12.383728" },
            { "simpleAttack.crowd.size.valueType", "periodic" },
            { "simpleAttack.crowd.type.check", "strictOn" },
            { "simpleAttack.forceOn", "TRUE" },
            { "simpleAttack.range.radius.periodic.amplitude.mobJitterScale", "0.587728" },
            { "simpleAttack.range.radius.periodic.amplitude.value", "-0.987937" },
            { "simpleAttack.range.radius.periodic.period.mobJitterScale", "-0.374820" },
            { "simpleAttack.range.radius.periodic.period.value", "-0.900000" },
            { "simpleAttack.range.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "simpleAttack.range.radius.periodic.tickShift.value", "9030.994141" },
            { "simpleAttack.range.radius.value.mobJitterScale", "0.558191" },
            { "simpleAttack.range.radius.value.value", "1714.531494" },
            { "simpleAttack.range.radius.valueType", "constant" },
            { "simpleAttack.range.type.check", "quadraticDown" },
            { "simpleAttack.value.periodic.amplitude.mobJitterScale", "0.810000" },
            { "simpleAttack.value.periodic.amplitude.value", "-0.639353" },
            { "simpleAttack.value.periodic.period.mobJitterScale", "-0.059316" },
            { "simpleAttack.value.periodic.period.value", "8368.158203" },
            { "simpleAttack.value.periodic.tickShift.mobJitterScale", "-0.427471" },
            { "simpleAttack.value.periodic.tickShift.value", "7758.612305" },
            { "simpleAttack.value.value.mobJitterScale", "0.467440" },
            { "simpleAttack.value.value.value", "-0.710388" },
            { "simpleAttack.value.valueType", "constant" },
            { "startingMaxRadius", "1761.484863" },
            { "startingMinRadius", "415.870300" },
        };
        BundleConfigValue configs11[] = {
            { "align.crowd.radius.periodic.amplitude.mobJitterScale", "-0.819771" },
            { "align.crowd.radius.periodic.amplitude.value", "-0.729000" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "align.crowd.radius.periodic.period.value", "-0.801900" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.726125" },
            { "align.crowd.radius.periodic.tickShift.value", "153.236725" },
            { "align.crowd.radius.value.mobJitterScale", "0.626805" },
            { "align.crowd.radius.value.value", "1410.391113" },
            { "align.crowd.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "1.000000" },
            { "align.crowd.size.periodic.amplitude.value", "-0.200416" },
            { "align.crowd.size.periodic.period.mobJitterScale", "-0.130796" },
            { "align.crowd.size.periodic.period.value", "5229.239746" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "-0.172068" },
            { "align.crowd.size.periodic.tickShift.value", "5630.258301" },
            { "align.crowd.size.value.mobJitterScale", "-0.413246" },
            { "align.crowd.size.value.value", "1.979929" },
            { "align.crowd.size.valueType", "periodic" },
            { "align.crowd.type.check", "linearUp" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "align.range.radius.periodic.amplitude.value", "-0.670292" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.197922" },
            { "align.range.radius.periodic.period.value", "10000.000000" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "0.608095" },
            { "align.range.radius.periodic.tickShift.value", "1582.818726" },
            { "align.range.radius.value.mobJitterScale", "-0.539245" },
            { "align.range.radius.value.value", "387.341797" },
            { "align.range.radius.valueType", "constant" },
            { "align.range.type.check", "linearDown" },
            { "align.weight.periodic.amplitude.mobJitterScale", "0.003184" },
            { "align.weight.periodic.amplitude.value", "-0.670339" },
            { "align.weight.periodic.period.mobJitterScale", "-0.518440" },
            { "align.weight.periodic.period.value", "10000.000000" },
            { "align.weight.periodic.tickShift.mobJitterScale", "0.039679" },
            { "align.weight.periodic.tickShift.value", "1992.849365" },
            { "align.weight.value.mobJitterScale", "0.362236" },
            { "align.weight.value.value", "1.271327" },
            { "align.weight.valueType", "constant" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "111.900185" },
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.875568" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "-0.669958" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "-0.122437" },
            { "attackSeparate.crowd.radius.periodic.period.value", "9223.905273" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.778816" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "4773.978516" },
            { "attackSeparate.crowd.radius.value.mobJitterScale", "0.869325" },
            { "attackSeparate.crowd.radius.value.value", "1400.136963" },
            { "attackSeparate.crowd.radius.valueType", "constant" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.513674" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.239886" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.526383" },
            { "attackSeparate.crowd.size.periodic.period.value", "7404.877930" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-0.188307" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "7228.548340" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-0.099430" },
            { "attackSeparate.crowd.size.value.value", "4.063056" },
            { "attackSeparate.crowd.size.valueType", "constant" },
            { "attackSeparate.crowd.type.check", "quadraticUp" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.869644" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "0.139481" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.225323" },
            { "attackSeparate.range.radius.periodic.period.value", "1471.238037" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.951696" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "2363.289307" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.476507" },
            { "attackSeparate.range.radius.value.value", "216.533432" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.range.type.check", "strictOn" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.524374" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.029362" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.317718" },
            { "attackSeparate.weight.periodic.period.value", "5479.040039" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-0.658642" },
            { "attackSeparate.weight.periodic.tickShift.value", "6765.426270" },
            { "attackSeparate.weight.value.mobJitterScale", "-0.030056" },
            { "attackSeparate.weight.value.value", "-5.601905" },
            { "attackSeparate.weight.valueType", "constant" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "-0.062456" },
            { "base.crowd.radius.periodic.amplitude.value", "0.949918" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "-0.601827" },
            { "base.crowd.radius.periodic.period.value", "12.408356" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "0.621513" },
            { "base.crowd.radius.periodic.tickShift.value", "7725.146973" },
            { "base.crowd.radius.value.mobJitterScale", "0.306347" },
            { "base.crowd.radius.value.value", "171.244232" },
            { "base.crowd.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.687612" },
            { "base.crowd.size.periodic.amplitude.value", "0.555572" },
            { "base.crowd.size.periodic.period.mobJitterScale", "0.786869" },
            { "base.crowd.size.periodic.period.value", "2912.433105" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.525738" },
            { "base.crowd.size.periodic.tickShift.value", "4107.798340" },
            { "base.crowd.size.value.mobJitterScale", "-0.643398" },
            { "base.crowd.size.value.value", "6.213152" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "quadraticDown" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "0.092729" },
            { "base.range.radius.periodic.amplitude.value", "0.793770" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.720088" },
            { "base.range.radius.periodic.period.value", "7753.876953" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.803068" },
            { "base.range.radius.periodic.tickShift.value", "2347.092285" },
            { "base.range.radius.value.mobJitterScale", "0.801857" },
            { "base.range.radius.value.value", "1252.200073" },
            { "base.range.radius.valueType", "periodic" },
            { "base.range.type.check", "quadraticDown" },
            { "base.weight.periodic.amplitude.mobJitterScale", "-0.383339" },
            { "base.weight.periodic.amplitude.value", "-0.315177" },
            { "base.weight.periodic.period.mobJitterScale", "0.204819" },
            { "base.weight.periodic.period.value", "2848.225586" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.670303" },
            { "base.weight.periodic.tickShift.value", "5547.875977" },
            { "base.weight.value.mobJitterScale", "1.000000" },
            { "base.weight.value.value", "8.055808" },
            { "base.weight.valueType", "constant" },
            { "baseDefense.crowd.radius.periodic.amplitude.mobJitterScale", "0.212569" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "0.615239" },
            { "baseDefense.crowd.radius.periodic.period.mobJitterScale", "0.545004" },
            { "baseDefense.crowd.radius.periodic.period.value", "3634.731934" },
            { "baseDefense.crowd.radius.periodic.tickShift.mobJitterScale", "0.568814" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "5943.337402" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "0.821121" },
            { "baseDefense.crowd.radius.value.value", "1440.565430" },
            { "baseDefense.crowd.radius.valueType", "constant" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "0.525797" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "0.047127" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "-0.489586" },
            { "baseDefense.crowd.size.periodic.period.value", "4528.967285" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "0.190003" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "3626.541748" },
            { "baseDefense.crowd.size.value.mobJitterScale", "0.218374" },
            { "baseDefense.crowd.size.value.value", "1.971821" },
            { "baseDefense.crowd.size.valueType", "periodic" },
            { "baseDefense.crowd.type.check", "quadraticUp" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.222102" },
            { "baseDefense.range.radius.periodic.amplitude.value", "0.678535" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "baseDefense.range.radius.periodic.period.value", "6273.352051" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "-0.887741" },
            { "baseDefense.range.radius.periodic.tickShift.value", "3157.719482" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.044910" },
            { "baseDefense.range.radius.value.value", "1182.588257" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "linearDown" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-0.258431" },
            { "baseDefense.weight.periodic.amplitude.value", "-0.556198" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "-0.401772" },
            { "baseDefense.weight.periodic.period.value", "2474.467773" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.383214" },
            { "baseDefense.weight.periodic.tickShift.value", "1779.157715" },
            { "baseDefense.weight.value.mobJitterScale", "0.888813" },
            { "baseDefense.weight.value.value", "7.298647" },
            { "baseDefense.weight.valueType", "constant" },
            { "baseDefenseRadius", "282.482269" },
            { "center.crowd.radius.periodic.amplitude.mobJitterScale", "-0.782533" },
            { "center.crowd.radius.periodic.amplitude.value", "0.059328" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "0.727819" },
            { "center.crowd.radius.periodic.period.value", "1875.677612" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "0.117905" },
            { "center.crowd.radius.periodic.tickShift.value", "4779.875488" },
            { "center.crowd.radius.value.mobJitterScale", "-0.160953" },
            { "center.crowd.radius.value.value", "418.904297" },
            { "center.crowd.radius.valueType", "constant" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "-0.900939" },
            { "center.crowd.size.periodic.amplitude.value", "0.434200" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.239185" },
            { "center.crowd.size.periodic.period.value", "5179.413574" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "-0.702211" },
            { "center.crowd.size.periodic.tickShift.value", "8126.266113" },
            { "center.crowd.size.value.mobJitterScale", "0.241111" },
            { "center.crowd.size.value.value", "9.676625" },
            { "center.crowd.size.valueType", "periodic" },
            { "center.crowd.type.check", "linearUp" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-0.785928" },
            { "center.range.radius.periodic.amplitude.value", "-1.000000" },
            { "center.range.radius.periodic.period.mobJitterScale", "-0.313603" },
            { "center.range.radius.periodic.period.value", "140.181885" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "-0.581404" },
            { "center.range.radius.periodic.tickShift.value", "7804.536133" },
            { "center.range.radius.value.mobJitterScale", "0.679470" },
            { "center.range.radius.value.value", "1964.968018" },
            { "center.range.radius.valueType", "periodic" },
            { "center.range.type.check", "never" },
            { "center.weight.periodic.amplitude.mobJitterScale", "-0.022085" },
            { "center.weight.periodic.amplitude.value", "-0.242707" },
            { "center.weight.periodic.period.mobJitterScale", "0.778562" },
            { "center.weight.periodic.period.value", "996.592896" },
            { "center.weight.periodic.tickShift.mobJitterScale", "0.641720" },
            { "center.weight.periodic.tickShift.value", "3004.574951" },
            { "center.weight.value.mobJitterScale", "0.406051" },
            { "center.weight.value.value", "-0.442811" },
            { "center.weight.valueType", "constant" },
            { "cohere.crowd.radius.periodic.amplitude.mobJitterScale", "-0.275188" },
            { "cohere.crowd.radius.periodic.amplitude.value", "0.417690" },
            { "cohere.crowd.radius.periodic.period.mobJitterScale", "-0.200751" },
            { "cohere.crowd.radius.periodic.period.value", "3867.982910" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "-0.027889" },
            { "cohere.crowd.radius.periodic.tickShift.value", "7865.501465" },
            { "cohere.crowd.radius.value.mobJitterScale", "-0.230327" },
            { "cohere.crowd.radius.value.value", "1155.596924" },
            { "cohere.crowd.radius.valueType", "constant" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.905773" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.170696" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "cohere.crowd.size.periodic.period.value", "-1.000000" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.040970" },
            { "cohere.crowd.size.periodic.tickShift.value", "2915.221680" },
            { "cohere.crowd.size.value.mobJitterScale", "-0.792594" },
            { "cohere.crowd.size.value.value", "7.319818" },
            { "cohere.crowd.size.valueType", "constant" },
            { "cohere.crowd.type.check", "strictOff" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "0.778351" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.369488" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "cohere.range.radius.periodic.period.value", "808.239929" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.544713" },
            { "cohere.range.radius.periodic.tickShift.value", "4391.973633" },
            { "cohere.range.radius.value.mobJitterScale", "-0.518871" },
            { "cohere.range.radius.value.value", "1441.126709" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "linearDown" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "0.861473" },
            { "cohere.weight.periodic.amplitude.value", "-0.652809" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.664519" },
            { "cohere.weight.periodic.period.value", "9289.973633" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "-0.729069" },
            { "cohere.weight.periodic.tickShift.value", "8513.070312" },
            { "cohere.weight.value.mobJitterScale", "0.053888" },
            { "cohere.weight.value.value", "-5.070666" },
            { "cohere.weight.valueType", "constant" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "-0.496615" },
            { "cores.crowd.radius.periodic.amplitude.value", "1.000000" },
            { "cores.crowd.radius.periodic.period.mobJitterScale", "-0.581605" },
            { "cores.crowd.radius.periodic.period.value", "6444.197754" },
            { "cores.crowd.radius.periodic.tickShift.mobJitterScale", "-0.542896" },
            { "cores.crowd.radius.periodic.tickShift.value", "6964.546387" },
            { "cores.crowd.radius.value.mobJitterScale", "-0.978124" },
            { "cores.crowd.radius.value.value", "213.491867" },
            { "cores.crowd.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "0.090357" },
            { "cores.crowd.size.periodic.amplitude.value", "1.000000" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "0.449014" },
            { "cores.crowd.size.periodic.period.value", "5381.442871" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.697909" },
            { "cores.crowd.size.periodic.tickShift.value", "3806.275146" },
            { "cores.crowd.size.value.mobJitterScale", "0.388744" },
            { "cores.crowd.size.value.value", "11.988400" },
            { "cores.crowd.size.valueType", "periodic" },
            { "cores.crowd.type.check", "linearDown" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "0.283257" },
            { "cores.range.radius.periodic.amplitude.value", "-0.256688" },
            { "cores.range.radius.periodic.period.mobJitterScale", "-0.159104" },
            { "cores.range.radius.periodic.period.value", "2064.132080" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "0.484144" },
            { "cores.range.radius.periodic.tickShift.value", "3923.685547" },
            { "cores.range.radius.value.mobJitterScale", "0.898177" },
            { "cores.range.radius.value.value", "1800.000000" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.range.type.check", "linearDown" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.788166" },
            { "cores.weight.periodic.amplitude.value", "0.763693" },
            { "cores.weight.periodic.period.mobJitterScale", "-0.590063" },
            { "cores.weight.periodic.period.value", "6841.812500" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.706069" },
            { "cores.weight.periodic.tickShift.value", "7496.139648" },
            { "cores.weight.value.mobJitterScale", "-0.720235" },
            { "cores.weight.value.value", "6.374467" },
            { "cores.weight.valueType", "constant" },
            { "corners.crowd.radius.periodic.amplitude.mobJitterScale", "-0.541653" },
            { "corners.crowd.radius.periodic.amplitude.value", "0.057707" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "0.675197" },
            { "corners.crowd.radius.periodic.period.value", "2181.965332" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "-0.818327" },
            { "corners.crowd.radius.periodic.tickShift.value", "7734.260742" },
            { "corners.crowd.radius.value.mobJitterScale", "1.000000" },
            { "corners.crowd.radius.value.value", "770.147644" },
            { "corners.crowd.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "0.471145" },
            { "corners.crowd.size.periodic.amplitude.value", "0.368691" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "0.900000" },
            { "corners.crowd.size.periodic.period.value", "2400.486816" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.407768" },
            { "corners.crowd.size.periodic.tickShift.value", "3040.158936" },
            { "corners.crowd.size.value.mobJitterScale", "0.835803" },
            { "corners.crowd.size.value.value", "-1.000000" },
            { "corners.crowd.type.check", "never" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.575806" },
            { "corners.range.radius.periodic.amplitude.value", "0.129784" },
            { "corners.range.radius.periodic.period.mobJitterScale", "0.900000" },
            { "corners.range.radius.periodic.period.value", "2112.503174" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-0.385697" },
            { "corners.range.radius.periodic.tickShift.value", "6650.602051" },
            { "corners.range.radius.value.mobJitterScale", "0.628664" },
            { "corners.range.radius.value.value", "-1.000000" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.range.type.check", "quadraticDown" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "-0.323836" },
            { "corners.weight.periodic.amplitude.value", "-1.000000" },
            { "corners.weight.periodic.period.mobJitterScale", "0.625127" },
            { "corners.weight.periodic.period.value", "8788.801758" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "0.523067" },
            { "corners.weight.periodic.tickShift.value", "7767.361816" },
            { "corners.weight.value.mobJitterScale", "-0.905706" },
            { "corners.weight.value.value", "-6.405005" },
            { "corners.weight.valueType", "constant" },
            { "creditReserve", "102.333519" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "curHeadingWeight.periodic.amplitude.value", "-0.070335" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "0.566482" },
            { "curHeadingWeight.periodic.period.value", "8404.708008" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "-0.644453" },
            { "curHeadingWeight.periodic.tickShift.value", "6634.063965" },
            { "curHeadingWeight.value.mobJitterScale", "-0.237486" },
            { "curHeadingWeight.value.value", "-6.013883" },
            { "curHeadingWeight.valueType", "periodic" },
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "-0.157037" },
            { "edges.crowd.radius.periodic.amplitude.value", "-0.026422" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "0.307209" },
            { "edges.crowd.radius.periodic.period.value", "7709.430176" },
            { "edges.crowd.radius.periodic.tickShift.mobJitterScale", "0.226489" },
            { "edges.crowd.radius.periodic.tickShift.value", "9561.449219" },
            { "edges.crowd.radius.value.mobJitterScale", "-0.121041" },
            { "edges.crowd.radius.value.value", "816.299500" },
            { "edges.crowd.radius.valueType", "periodic" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.414017" },
            { "edges.crowd.size.periodic.amplitude.value", "-0.213887" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "edges.crowd.size.periodic.period.value", "1197.715454" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "-0.058456" },
            { "edges.crowd.size.periodic.tickShift.value", "8352.010742" },
            { "edges.crowd.size.value.mobJitterScale", "1.000000" },
            { "edges.crowd.size.value.value", "-1.000000" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.crowd.type.check", "strictOff" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.026607" },
            { "edges.range.radius.periodic.amplitude.value", "-0.827558" },
            { "edges.range.radius.periodic.period.mobJitterScale", "-0.937547" },
            { "edges.range.radius.periodic.period.value", "3772.746094" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "-0.092791" },
            { "edges.range.radius.periodic.tickShift.value", "2663.460938" },
            { "edges.range.radius.value.mobJitterScale", "-0.581471" },
            { "edges.range.radius.value.value", "1847.365479" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.range.type.check", "strictOn" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.962634" },
            { "edges.weight.periodic.amplitude.value", "0.727313" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.711619" },
            { "edges.weight.periodic.period.value", "7987.145508" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "edges.weight.periodic.tickShift.value", "1540.922119" },
            { "edges.weight.value.mobJitterScale", "0.323172" },
            { "edges.weight.value.value", "3.144239" },
            { "edges.weight.valueType", "constant" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "-0.328602" },
            { "enemy.crowd.radius.periodic.amplitude.value", "-0.716494" },
            { "enemy.crowd.radius.periodic.period.mobJitterScale", "0.713715" },
            { "enemy.crowd.radius.periodic.period.value", "4491.053711" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "-0.603556" },
            { "enemy.crowd.radius.periodic.tickShift.value", "2869.923584" },
            { "enemy.crowd.radius.value.mobJitterScale", "0.808888" },
            { "enemy.crowd.radius.value.value", "1902.734863" },
            { "enemy.crowd.radius.valueType", "constant" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.559342" },
            { "enemy.crowd.size.periodic.amplitude.value", "0.615300" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.112917" },
            { "enemy.crowd.size.periodic.period.value", "2829.017822" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "0.810648" },
            { "enemy.crowd.size.periodic.tickShift.value", "4967.693359" },
            { "enemy.crowd.size.value.mobJitterScale", "-0.031329" },
            { "enemy.crowd.size.value.value", "2.958930" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "strictOn" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.074287" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.990000" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "-0.202738" },
            { "enemy.range.radius.periodic.period.value", "526.310852" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.589944" },
            { "enemy.range.radius.periodic.tickShift.value", "2969.172363" },
            { "enemy.range.radius.value.mobJitterScale", "-0.813138" },
            { "enemy.range.radius.value.value", "1084.891479" },
            { "enemy.range.radius.valueType", "constant" },
            { "enemy.range.type.check", "linearDown" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "0.348317" },
            { "enemy.weight.periodic.amplitude.value", "1.000000" },
            { "enemy.weight.periodic.period.mobJitterScale", "-0.003385" },
            { "enemy.weight.periodic.period.value", "5208.307617" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.738410" },
            { "enemy.weight.periodic.tickShift.value", "903.412476" },
            { "enemy.weight.value.mobJitterScale", "0.857762" },
            { "enemy.weight.value.value", "-6.787437" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.crowd.radius.periodic.amplitude.mobJitterScale", "-0.134198" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "0.300472" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "0.041597" },
            { "enemyBase.crowd.radius.periodic.period.value", "4471.371094" },
            { "enemyBase.crowd.radius.periodic.tickShift.mobJitterScale", "0.299046" },
            { "enemyBase.crowd.radius.periodic.tickShift.value", "5424.190430" },
            { "enemyBase.crowd.radius.value.mobJitterScale", "0.639339" },
            { "enemyBase.crowd.radius.value.value", "1261.009766" },
            { "enemyBase.crowd.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "-0.050265" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "1.000000" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.894724" },
            { "enemyBase.crowd.size.periodic.period.value", "2119.526367" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.633405" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "8821.654297" },
            { "enemyBase.crowd.size.value.mobJitterScale", "0.654388" },
            { "enemyBase.crowd.size.value.value", "5.096158" },
            { "enemyBase.crowd.size.valueType", "periodic" },
            { "enemyBase.crowd.type.check", "strictOff" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "0.359198" },
            { "enemyBase.range.radius.periodic.amplitude.value", "0.411279" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "-0.008250" },
            { "enemyBase.range.radius.periodic.period.value", "9173.635742" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "-0.528992" },
            { "enemyBase.range.radius.periodic.tickShift.value", "313.641327" },
            { "enemyBase.range.radius.value.mobJitterScale", "-0.520180" },
            { "enemyBase.range.radius.value.value", "861.602051" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.range.type.check", "quadraticDown" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.634878" },
            { "enemyBase.weight.periodic.amplitude.value", "0.207085" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "-0.422089" },
            { "enemyBase.weight.periodic.period.value", "8961.458984" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "enemyBase.weight.periodic.tickShift.value", "7467.031738" },
            { "enemyBase.weight.value.mobJitterScale", "-0.318814" },
            { "enemyBase.weight.value.value", "-5.518919" },
            { "enemyBase.weight.valueType", "constant" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.mobJitterScale", "-0.455792" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.value", "0.103011" },
            { "enemyBaseGuess.crowd.radius.periodic.period.mobJitterScale", "0.037047" },
            { "enemyBaseGuess.crowd.radius.periodic.period.value", "8633.642578" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.mobJitterScale", "0.347818" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.value", "2641.698486" },
            { "enemyBaseGuess.crowd.radius.value.mobJitterScale", "-0.428070" },
            { "enemyBaseGuess.crowd.radius.value.value", "-1.000000" },
            { "enemyBaseGuess.crowd.radius.valueType", "periodic" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.mobJitterScale", "-0.879495" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.value", "0.719863" },
            { "enemyBaseGuess.crowd.size.periodic.period.mobJitterScale", "0.266465" },
            { "enemyBaseGuess.crowd.size.periodic.period.value", "4901.932129" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.mobJitterScale", "0.911387" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.value", "7364.159668" },
            { "enemyBaseGuess.crowd.size.value.mobJitterScale", "-0.757547" },
            { "enemyBaseGuess.crowd.size.value.value", "-1.000000" },
            { "enemyBaseGuess.crowd.size.valueType", "constant" },
            { "enemyBaseGuess.crowd.type.check", "linearDown" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.mobJitterScale", "-0.294997" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.value", "-0.857649" },
            { "enemyBaseGuess.range.radius.periodic.period.mobJitterScale", "0.574136" },
            { "enemyBaseGuess.range.radius.periodic.period.value", "1182.964966" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.mobJitterScale", "-0.986518" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.value", "5260.275879" },
            { "enemyBaseGuess.range.radius.value.mobJitterScale", "-0.706336" },
            { "enemyBaseGuess.range.radius.value.value", "1129.586792" },
            { "enemyBaseGuess.range.radius.valueType", "constant" },
            { "enemyBaseGuess.range.type.check", "strictOn" },
            { "enemyBaseGuess.weight.periodic.amplitude.mobJitterScale", "0.663793" },
            { "enemyBaseGuess.weight.periodic.amplitude.value", "-0.224415" },
            { "enemyBaseGuess.weight.periodic.period.mobJitterScale", "0.047774" },
            { "enemyBaseGuess.weight.periodic.period.value", "3131.945312" },
            { "enemyBaseGuess.weight.periodic.tickShift.mobJitterScale", "0.300388" },
            { "enemyBaseGuess.weight.periodic.tickShift.value", "2950.289551" },
            { "enemyBaseGuess.weight.value.mobJitterScale", "0.103579" },
            { "enemyBaseGuess.weight.value.value", "1.981095" },
            { "enemyBaseGuess.weight.valueType", "constant" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "7.847223" },
            { "evadeStrictDistance", "417.628876" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fighterBaseDefenseUseRadius", "FALSE" },
            { "fleetLocus.circularPeriod", "6840.101074" },
            { "fleetLocus.circularWeight", "2.000000" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.363766" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.value", "0.616772" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.832926" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "4811.583008" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.657213" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "1076.331665" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "0.316467" },
            { "fleetLocus.force.crowd.radius.value.value", "406.651398" },
            { "fleetLocus.force.crowd.radius.valueType", "constant" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.237047" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "0.208878" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.723136" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "4723.593262" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "1.000000" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "2046.541626" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "0.548237" },
            { "fleetLocus.force.crowd.size.value.value", "8.065153" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "linearDown" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.326488" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "-0.214219" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "-0.989959" },
            { "fleetLocus.force.range.radius.periodic.period.value", "2368.047119" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.526800" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "6377.476562" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.656215" },
            { "fleetLocus.force.range.radius.value.value", "241.470123" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "0.788562" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.578470" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-0.726762" },
            { "fleetLocus.force.weight.periodic.period.value", "-1.000000" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "0.502272" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "1289.908936" },
            { "fleetLocus.force.weight.value.mobJitterScale", "0.900000" },
            { "fleetLocus.force.weight.value.value", "5.559298" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "1.012764" },
            { "fleetLocus.linearXPeriod", "11478.919922" },
            { "fleetLocus.linearYPeriod", "8856.908203" },
            { "fleetLocus.randomPeriod", "4198.898438" },
            { "fleetLocus.randomWeight", "1.611552" },
            { "fleetLocus.useScaled", "TRUE" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.mobJitterScale", "-0.497352" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.value", "-0.724258" },
            { "flockDuringAttack.crowd.radius.periodic.period.mobJitterScale", "0.861923" },
            { "flockDuringAttack.crowd.radius.periodic.period.value", "4277.556641" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.475218" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.value", "6848.419434" },
            { "flockDuringAttack.crowd.radius.value.mobJitterScale", "0.672987" },
            { "flockDuringAttack.crowd.radius.value.value", "1263.230469" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.092128" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.value", "0.583326" },
            { "flockDuringAttack.crowd.size.periodic.period.mobJitterScale", "0.377886" },
            { "flockDuringAttack.crowd.size.periodic.period.value", "4408.037109" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.mobJitterScale", "0.555186" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.value", "3338.882324" },
            { "flockDuringAttack.crowd.size.value.mobJitterScale", "-0.038029" },
            { "flockDuringAttack.crowd.size.value.value", "17.281216" },
            { "flockDuringAttack.crowd.size.valueType", "periodic" },
            { "flockDuringAttack.crowd.type.check", "quadraticUp" },
            { "flockDuringAttack.forceOn", "FALSE" },
            { "flockDuringAttack.range.radius.periodic.amplitude.mobJitterScale", "0.307140" },
            { "flockDuringAttack.range.radius.periodic.amplitude.value", "0.632684" },
            { "flockDuringAttack.range.radius.periodic.period.mobJitterScale", "-0.071228" },
            { "flockDuringAttack.range.radius.periodic.period.value", "7432.625977" },
            { "flockDuringAttack.range.radius.periodic.tickShift.mobJitterScale", "-0.578873" },
            { "flockDuringAttack.range.radius.periodic.tickShift.value", "9875.075195" },
            { "flockDuringAttack.range.radius.value.mobJitterScale", "-0.102930" },
            { "flockDuringAttack.range.radius.value.value", "2000.000000" },
            { "flockDuringAttack.range.radius.valueType", "periodic" },
            { "flockDuringAttack.range.type.check", "strictOn" },
            { "flockDuringAttack.value.periodic.amplitude.mobJitterScale", "0.373777" },
            { "flockDuringAttack.value.periodic.amplitude.value", "-0.019057" },
            { "flockDuringAttack.value.periodic.period.mobJitterScale", "-0.625051" },
            { "flockDuringAttack.value.periodic.period.value", "8560.545898" },
            { "flockDuringAttack.value.periodic.tickShift.mobJitterScale", "0.328205" },
            { "flockDuringAttack.value.periodic.tickShift.value", "5565.298340" },
            { "flockDuringAttack.value.value.mobJitterScale", "-0.356785" },
            { "flockDuringAttack.value.value.value", "0.658926" },
            { "flockDuringAttack.value.valueType", "constant" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "54.079327" },
            { "guardRange", "195.753860" },
            { "hold.invProbability.periodic.amplitude.mobJitterScale", "0.468178" },
            { "hold.invProbability.periodic.amplitude.value", "0.900000" },
            { "hold.invProbability.periodic.period.mobJitterScale", "1.000000" },
            { "hold.invProbability.periodic.period.value", "2545.372559" },
            { "hold.invProbability.periodic.tickShift.mobJitterScale", "0.365938" },
            { "hold.invProbability.periodic.tickShift.value", "4820.290039" },
            { "hold.invProbability.value.mobJitterScale", "-0.409977" },
            { "hold.invProbability.value.value", "6130.677246" },
            { "hold.invProbability.valueType", "constant" },
            { "hold.ticks.periodic.amplitude.mobJitterScale", "-0.776314" },
            { "hold.ticks.periodic.amplitude.value", "-0.035563" },
            { "hold.ticks.periodic.period.mobJitterScale", "-0.023227" },
            { "hold.ticks.periodic.period.value", "78.789841" },
            { "hold.ticks.periodic.tickShift.mobJitterScale", "0.708051" },
            { "hold.ticks.periodic.tickShift.value", "2146.982422" },
            { "hold.ticks.value.mobJitterScale", "-0.416029" },
            { "hold.ticks.value.value", "373.583893" },
            { "hold.ticks.valueType", "periodic" },
            { "maxWeight", "3528.958984" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.742331" },
            { "mobLocus.circularPeriod.value", "4968.080078" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "-0.719175" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "-1.000000" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "0.037584" },
            { "mobLocus.circularWeight.periodic.period.value", "2437.267334" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.826070" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "1214.122559" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.704569" },
            { "mobLocus.circularWeight.value.value", "7.257419" },
            { "mobLocus.circularWeight.valueType", "constant" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "0.264988" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.value", "0.863288" },
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.661356" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "2125.688965" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.523525" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.value", "1468.017090" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "-0.041138" },
            { "mobLocus.force.crowd.radius.value.value", "370.713623" },
            { "mobLocus.force.crowd.radius.valueType", "periodic" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.146104" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-0.741707" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "mobLocus.force.crowd.size.periodic.period.value", "5752.655762" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "777.351868" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.562858" },
            { "mobLocus.force.crowd.size.value.value", "20.000000" },
            { "mobLocus.force.crowd.size.valueType", "constant" },
            { "mobLocus.force.crowd.type.check", "linearUp" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.863276" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.355435" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "-0.113542" },
            { "mobLocus.force.range.radius.periodic.period.value", "5696.458008" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.842416" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "0.000000" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.436304" },
            { "mobLocus.force.range.radius.value.value", "253.536194" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.range.type.check", "never" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "0.936513" },
            { "mobLocus.force.weight.periodic.amplitude.value", "-0.075880" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.200321" },
            { "mobLocus.force.weight.periodic.period.value", "1264.847778" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.001041" },
            { "mobLocus.force.weight.periodic.tickShift.value", "3479.290527" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.437262" },
            { "mobLocus.force.weight.value.value", "3.349775" },
            { "mobLocus.force.weight.valueType", "periodic" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "-0.236484" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.099368" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.620240" },
            { "mobLocus.linearWeight.periodic.period.value", "3528.148926" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "1.000000" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "5230.199219" },
            { "mobLocus.linearWeight.value.mobJitterScale", "-0.511209" },
            { "mobLocus.linearWeight.value.value", "0.868857" },
            { "mobLocus.linearWeight.valueType", "constant" },
            { "mobLocus.linearXPeriod.mobJitterScale", "0.373794" },
            { "mobLocus.linearXPeriod.value", "1844.605103" },
            { "mobLocus.linearYPeriod.mobJitterScale", "-0.037627" },
            { "mobLocus.linearYPeriod.value", "7045.197754" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "0.491404" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-1.000000" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "-0.317774" },
            { "mobLocus.proximityRadius.periodic.period.value", "3241.004395" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "-0.728917" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "9832.228516" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "-0.387890" },
            { "mobLocus.proximityRadius.value.value", "909.891541" },
            { "mobLocus.proximityRadius.valueType", "constant" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.343327" },
            { "mobLocus.randomPeriod.value", "3369.397217" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "-0.745631" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "0.146858" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "-0.211177" },
            { "mobLocus.randomWeight.periodic.period.value", "1538.200317" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "0.408540" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "1362.582275" },
            { "mobLocus.randomWeight.value.mobJitterScale", "-0.034673" },
            { "mobLocus.randomWeight.value.value", "-3.847646" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "TRUE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "363.001312" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.637401" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.value", "-0.810000" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.mobJitterScale", "-0.728027" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.value", "702.850220" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.791594" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.value", "8534.826172" },
            { "nearBaseRandomIdle.crowd.radius.value.mobJitterScale", "-0.315442" },
            { "nearBaseRandomIdle.crowd.radius.value.value", "119.681351" },
            { "nearBaseRandomIdle.crowd.radius.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.mobJitterScale", "-0.390490" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.value", "-0.006895" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.value", "6695.746094" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.mobJitterScale", "0.531038" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.value", "991.876404" },
            { "nearBaseRandomIdle.crowd.size.value.mobJitterScale", "-0.620768" },
            { "nearBaseRandomIdle.crowd.size.value.value", "13.547216" },
            { "nearBaseRandomIdle.crowd.size.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.type.check", "quadraticDown" },
            { "nearBaseRandomIdle.forceOn", "FALSE" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.mobJitterScale", "-0.686189" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.value", "0.540779" },
            { "nearBaseRandomIdle.range.radius.periodic.period.mobJitterScale", "-0.243522" },
            { "nearBaseRandomIdle.range.radius.periodic.period.value", "1065.890015" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.216485" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.value", "1666.357056" },
            { "nearBaseRandomIdle.range.radius.value.mobJitterScale", "-0.707038" },
            { "nearBaseRandomIdle.range.radius.value.value", "681.666321" },
            { "nearBaseRandomIdle.range.radius.valueType", "constant" },
            { "nearBaseRandomIdle.range.type.check", "linearDown" },
            { "nearBaseRandomIdle.value.periodic.amplitude.mobJitterScale", "-0.010811" },
            { "nearBaseRandomIdle.value.periodic.amplitude.value", "0.353513" },
            { "nearBaseRandomIdle.value.periodic.period.mobJitterScale", "1.000000" },
            { "nearBaseRandomIdle.value.periodic.period.value", "-1.000000" },
            { "nearBaseRandomIdle.value.periodic.tickShift.mobJitterScale", "-0.229530" },
            { "nearBaseRandomIdle.value.periodic.tickShift.value", "1166.170044" },
            { "nearBaseRandomIdle.value.value.mobJitterScale", "-0.028878" },
            { "nearBaseRandomIdle.value.value.value", "0.182896" },
            { "nearBaseRandomIdle.value.valueType", "periodic" },
            { "nearestFriend.crowd.radius.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "nearestFriend.crowd.radius.periodic.amplitude.value", "0.701819" },
            { "nearestFriend.crowd.radius.periodic.period.mobJitterScale", "-0.452693" },
            { "nearestFriend.crowd.radius.periodic.period.value", "3740.101318" },
            { "nearestFriend.crowd.radius.periodic.tickShift.mobJitterScale", "0.885120" },
            { "nearestFriend.crowd.radius.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.crowd.radius.value.mobJitterScale", "0.243332" },
            { "nearestFriend.crowd.radius.value.value", "622.961426" },
            { "nearestFriend.crowd.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "0.037150" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "0.145201" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "-0.817117" },
            { "nearestFriend.crowd.size.periodic.period.value", "-1.000000" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "-0.646088" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "0.032559" },
            { "nearestFriend.crowd.size.value.value", "9.775476" },
            { "nearestFriend.crowd.size.valueType", "constant" },
            { "nearestFriend.crowd.type.check", "quadraticUp" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "0.369789" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.882211" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "0.520451" },
            { "nearestFriend.range.radius.periodic.period.value", "4213.134766" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "-0.239058" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "9667.213867" },
            { "nearestFriend.range.radius.value.mobJitterScale", "0.157404" },
            { "nearestFriend.range.radius.value.value", "428.499390" },
            { "nearestFriend.range.radius.valueType", "constant" },
            { "nearestFriend.range.type.check", "quadraticUp" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "-0.868417" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.747403" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "0.062212" },
            { "nearestFriend.weight.periodic.period.value", "3233.041992" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "-0.739887" },
            { "nearestFriend.weight.periodic.tickShift.value", "6632.024414" },
            { "nearestFriend.weight.value.mobJitterScale", "-0.767819" },
            { "nearestFriend.weight.value.value", "-1.079656" },
            { "nearestFriend.weight.valueType", "constant" },
            { "randomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.245232" },
            { "randomIdle.crowd.radius.periodic.amplitude.value", "0.004185" },
            { "randomIdle.crowd.radius.periodic.period.mobJitterScale", "0.108074" },
            { "randomIdle.crowd.radius.periodic.period.value", "2432.576660" },
            { "randomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.446941" },
            { "randomIdle.crowd.radius.periodic.tickShift.value", "1349.302856" },
            { "randomIdle.crowd.radius.value.mobJitterScale", "-0.900000" },
            { "randomIdle.crowd.radius.value.value", "41.022137" },
            { "randomIdle.crowd.radius.valueType", "constant" },
            { "randomIdle.crowd.size.periodic.amplitude.mobJitterScale", "-0.420703" },
            { "randomIdle.crowd.size.periodic.amplitude.value", "0.391206" },
            { "randomIdle.crowd.size.periodic.period.mobJitterScale", "-0.263563" },
            { "randomIdle.crowd.size.periodic.period.value", "8069.421875" },
            { "randomIdle.crowd.size.periodic.tickShift.mobJitterScale", "0.170960" },
            { "randomIdle.crowd.size.periodic.tickShift.value", "86.564560" },
            { "randomIdle.crowd.size.value.mobJitterScale", "-0.638390" },
            { "randomIdle.crowd.size.value.value", "11.045941" },
            { "randomIdle.crowd.size.valueType", "constant" },
            { "randomIdle.crowd.type.check", "always" },
            { "randomIdle.forceOn", "TRUE" },
            { "randomIdle.range.radius.periodic.amplitude.mobJitterScale", "0.247853" },
            { "randomIdle.range.radius.periodic.amplitude.value", "0.134241" },
            { "randomIdle.range.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "randomIdle.range.radius.periodic.period.value", "1279.787476" },
            { "randomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.557063" },
            { "randomIdle.range.radius.periodic.tickShift.value", "9779.170898" },
            { "randomIdle.range.radius.value.mobJitterScale", "-0.382629" },
            { "randomIdle.range.radius.value.value", "1213.602783" },
            { "randomIdle.range.radius.valueType", "periodic" },
            { "randomIdle.range.type.check", "quadraticUp" },
            { "randomIdle.value.periodic.amplitude.mobJitterScale", "-0.716580" },
            { "randomIdle.value.periodic.amplitude.value", "0.327020" },
            { "randomIdle.value.periodic.period.mobJitterScale", "-1.000000" },
            { "randomIdle.value.periodic.period.value", "2753.216553" },
            { "randomIdle.value.periodic.tickShift.mobJitterScale", "-0.955826" },
            { "randomIdle.value.periodic.tickShift.value", "7720.690430" },
            { "randomIdle.value.value.mobJitterScale", "1.000000" },
            { "randomIdle.value.value.value", "-0.148807" },
            { "randomIdle.value.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.value", "-0.900000" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.mobJitterScale", "0.277194" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.value", "10000.000000" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.mobJitterScale", "-0.314652" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.value", "10000.000000" },
            { "randomizeStoppedVelocity.crowd.radius.value.mobJitterScale", "0.458255" },
            { "randomizeStoppedVelocity.crowd.radius.value.value", "1622.954590" },
            { "randomizeStoppedVelocity.crowd.radius.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.mobJitterScale", "-0.581633" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.value", "0.426083" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.mobJitterScale", "0.713029" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.value", "7802.493164" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.mobJitterScale", "-0.709124" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.value", "5038.222656" },
            { "randomizeStoppedVelocity.crowd.size.value.mobJitterScale", "-1.000000" },
            { "randomizeStoppedVelocity.crowd.size.value.value", "5.037300" },
            { "randomizeStoppedVelocity.crowd.size.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.type.check", "strictOff" },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.mobJitterScale", "0.270872" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.value", "1.000000" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.mobJitterScale", "0.147392" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.value", "1921.421143" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.mobJitterScale", "0.490620" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.value", "-1.000000" },
            { "randomizeStoppedVelocity.range.radius.value.mobJitterScale", "0.549376" },
            { "randomizeStoppedVelocity.range.radius.value.value", "5.007942" },
            { "randomizeStoppedVelocity.range.radius.valueType", "constant" },
            { "randomizeStoppedVelocity.range.type.check", "strictOn" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.mobJitterScale", "0.039870" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.value", "0.268655" },
            { "randomizeStoppedVelocity.value.periodic.period.mobJitterScale", "0.712508" },
            { "randomizeStoppedVelocity.value.periodic.period.value", "5182.304199" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.mobJitterScale", "-0.725569" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.value", "4651.704590" },
            { "randomizeStoppedVelocity.value.value.mobJitterScale", "0.839962" },
            { "randomizeStoppedVelocity.value.value.value", "-0.580448" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "11.820508" },
            { "sensorGrid.staleFighterTime", "1.049959" },
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.524058" },
            { "separate.crowd.radius.periodic.amplitude.value", "-0.793002" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "-0.829125" },
            { "separate.crowd.radius.periodic.period.value", "992.216675" },
            { "separate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.699645" },
            { "separate.crowd.radius.periodic.tickShift.value", "928.554382" },
            { "separate.crowd.radius.value.mobJitterScale", "-0.642182" },
            { "separate.crowd.radius.value.value", "1662.215088" },
            { "separate.crowd.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "0.679230" },
            { "separate.crowd.size.periodic.amplitude.value", "-0.206211" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "-0.225051" },
            { "separate.crowd.size.periodic.period.value", "4559.602051" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "-0.224794" },
            { "separate.crowd.size.periodic.tickShift.value", "8812.683594" },
            { "separate.crowd.size.value.mobJitterScale", "0.835303" },
            { "separate.crowd.size.value.value", "-0.206483" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowd.type.check", "always" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "0.210955" },
            { "separate.range.radius.periodic.amplitude.value", "-0.988720" },
            { "separate.range.radius.periodic.period.mobJitterScale", "0.158374" },
            { "separate.range.radius.periodic.period.value", "2688.514893" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "separate.range.radius.periodic.tickShift.value", "1640.423462" },
            { "separate.range.radius.value.mobJitterScale", "0.684423" },
            { "separate.range.radius.value.value", "794.725037" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.range.type.check", "quadraticDown" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.545320" },
            { "separate.weight.periodic.amplitude.value", "-0.836968" },
            { "separate.weight.periodic.period.mobJitterScale", "0.275892" },
            { "separate.weight.periodic.period.value", "8066.585449" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.870520" },
            { "separate.weight.periodic.tickShift.value", "8762.026367" },
            { "separate.weight.value.mobJitterScale", "-0.353404" },
            { "separate.weight.value.value", "-5.940841" },
            { "separate.weight.valueType", "periodic" },
            { "simpleAttack.crowd.radius.periodic.amplitude.mobJitterScale", "0.578568" },
            { "simpleAttack.crowd.radius.periodic.amplitude.value", "0.930455" },
            { "simpleAttack.crowd.radius.periodic.period.mobJitterScale", "-0.858543" },
            { "simpleAttack.crowd.radius.periodic.period.value", "5253.363770" },
            { "simpleAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.704180" },
            { "simpleAttack.crowd.radius.periodic.tickShift.value", "-1.000000" },
            { "simpleAttack.crowd.radius.value.mobJitterScale", "1.000000" },
            { "simpleAttack.crowd.radius.value.value", "1911.948730" },
            { "simpleAttack.crowd.radius.valueType", "constant" },
            { "simpleAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.117608" },
            { "simpleAttack.crowd.size.periodic.amplitude.value", "0.090021" },
            { "simpleAttack.crowd.size.periodic.period.mobJitterScale", "-0.048841" },
            { "simpleAttack.crowd.size.periodic.period.value", "5225.845703" },
            { "simpleAttack.crowd.size.periodic.tickShift.mobJitterScale", "-0.931866" },
            { "simpleAttack.crowd.size.periodic.tickShift.value", "2604.073975" },
            { "simpleAttack.crowd.size.value.mobJitterScale", "-0.129735" },
            { "simpleAttack.crowd.size.value.value", "3.027120" },
            { "simpleAttack.crowd.size.valueType", "constant" },
            { "simpleAttack.crowd.type.check", "always" },
            { "simpleAttack.forceOn", "TRUE" },
            { "simpleAttack.range.radius.periodic.amplitude.mobJitterScale", "0.289819" },
            { "simpleAttack.range.radius.periodic.amplitude.value", "-0.641704" },
            { "simpleAttack.range.radius.periodic.period.mobJitterScale", "0.105543" },
            { "simpleAttack.range.radius.periodic.period.value", "10000.000000" },
            { "simpleAttack.range.radius.periodic.tickShift.mobJitterScale", "-0.480097" },
            { "simpleAttack.range.radius.periodic.tickShift.value", "581.202759" },
            { "simpleAttack.range.radius.value.mobJitterScale", "0.402433" },
            { "simpleAttack.range.radius.value.value", "1547.803955" },
            { "simpleAttack.range.radius.valueType", "constant" },
            { "simpleAttack.range.type.check", "quadraticDown" },
            { "simpleAttack.value.periodic.amplitude.mobJitterScale", "0.801900" },
            { "simpleAttack.value.periodic.amplitude.value", "-1.000000" },
            { "simpleAttack.value.periodic.period.mobJitterScale", "-0.259054" },
            { "simpleAttack.value.periodic.period.value", "6556.747559" },
            { "simpleAttack.value.periodic.tickShift.mobJitterScale", "-0.624234" },
            { "simpleAttack.value.periodic.tickShift.value", "7758.612305" },
            { "simpleAttack.value.value.mobJitterScale", "0.344925" },
            { "simpleAttack.value.value.value", "-1.000000" },
            { "simpleAttack.value.valueType", "periodic" },
            { "startingMaxRadius", "1587.389893" },
            { "startingMinRadius", "334.176880" },
        };
        BundleConfigValue configs12[] = {
            { "align.crowd.radius.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "align.crowd.radius.periodic.amplitude.value", "-0.723512" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "-0.982345" },
            { "align.crowd.radius.periodic.period.value", "6117.970703" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.238703" },
            { "align.crowd.radius.periodic.tickShift.value", "1008.125671" },
            { "align.crowd.radius.value.mobJitterScale", "0.283142" },
            { "align.crowd.radius.value.value", "914.980896" },
            { "align.crowd.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.718776" },
            { "align.crowd.size.periodic.amplitude.value", "0.143807" },
            { "align.crowd.size.periodic.period.mobJitterScale", "0.054472" },
            { "align.crowd.size.periodic.period.value", "693.845276" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.950657" },
            { "align.crowd.size.periodic.tickShift.value", "5118.416504" },
            { "align.crowd.size.value.mobJitterScale", "-0.454571" },
            { "align.crowd.size.value.value", "3.795100" },
            { "align.crowd.size.valueType", "periodic" },
            { "align.crowd.type.check", "linearUp" },
            { "align.range.radius.periodic.amplitude.mobJitterScale", "-0.810000" },
            { "align.range.radius.periodic.amplitude.value", "-0.574481" },
            { "align.range.radius.periodic.period.mobJitterScale", "0.014282" },
            { "align.range.radius.periodic.period.value", "8965.770508" },
            { "align.range.radius.periodic.tickShift.mobJitterScale", "-0.077372" },
            { "align.range.radius.periodic.tickShift.value", "-1.000000" },
            { "align.range.radius.value.mobJitterScale", "-0.190631" },
            { "align.range.radius.value.value", "182.229248" },
            { "align.range.radius.valueType", "constant" },
            { "align.range.type.check", "linearDown" },
            { "align.weight.periodic.amplitude.mobJitterScale", "0.751395" },
            { "align.weight.periodic.amplitude.value", "-0.798077" },
            { "align.weight.periodic.period.mobJitterScale", "-0.579688" },
            { "align.weight.periodic.period.value", "9000.000000" },
            { "align.weight.periodic.tickShift.mobJitterScale", "0.368785" },
            { "align.weight.periodic.tickShift.value", "984.835632" },
            { "align.weight.value.mobJitterScale", "0.745434" },
            { "align.weight.value.value", "6.397413" },
            { "align.weight.valueType", "periodic" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "126.390282" },
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.990000" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "-0.756808" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "-0.074184" },
            { "attackSeparate.crowd.radius.periodic.period.value", "6622.686035" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.990000" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "8845.212891" },
            { "attackSeparate.crowd.radius.value.mobJitterScale", "0.455973" },
            { "attackSeparate.crowd.radius.value.value", "188.202271" },
            { "attackSeparate.crowd.radius.valueType", "periodic" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "0.890491" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.420029" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.777744" },
            { "attackSeparate.crowd.size.periodic.period.value", "8271.318359" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-0.167781" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "3622.276123" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-0.097030" },
            { "attackSeparate.crowd.size.value.value", "17.172640" },
            { "attackSeparate.crowd.size.valueType", "constant" },
            { "attackSeparate.crowd.type.check", "strictOn" },
            { "attackSeparate.range.radius.periodic.amplitude.mobJitterScale", "0.764994" },
            { "attackSeparate.range.radius.periodic.amplitude.value", "-0.643894" },
            { "attackSeparate.range.radius.periodic.period.mobJitterScale", "0.900000" },
            { "attackSeparate.range.radius.periodic.period.value", "9189.702148" },
            { "attackSeparate.range.radius.periodic.tickShift.mobJitterScale", "0.066657" },
            { "attackSeparate.range.radius.periodic.tickShift.value", "6389.921387" },
            { "attackSeparate.range.radius.value.mobJitterScale", "-0.471742" },
            { "attackSeparate.range.radius.value.value", "-1.000000" },
            { "attackSeparate.range.radius.valueType", "periodic" },
            { "attackSeparate.range.type.check", "strictOn" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "0.186659" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.212437" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "1.000000" },
            { "attackSeparate.weight.periodic.period.value", "1758.452515" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-0.692035" },
            { "attackSeparate.weight.periodic.tickShift.value", "1471.582642" },
            { "attackSeparate.weight.value.mobJitterScale", "0.209023" },
            { "attackSeparate.weight.value.value", "6.451889" },
            { "attackSeparate.weight.valueType", "periodic" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "0.442958" },
            { "base.crowd.radius.periodic.amplitude.value", "0.784517" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "-0.723544" },
            { "base.crowd.radius.periodic.period.value", "3064.789551" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "-0.550621" },
            { "base.crowd.radius.periodic.tickShift.value", "2749.638184" },
            { "base.crowd.radius.value.mobJitterScale", "0.451903" },
            { "base.crowd.radius.value.value", "1154.008301" },
            { "base.crowd.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "0.602942" },
            { "base.crowd.size.periodic.amplitude.value", "0.615263" },
            { "base.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "base.crowd.size.periodic.period.value", "3837.684082" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "-0.529747" },
            { "base.crowd.size.periodic.tickShift.value", "4149.291504" },
            { "base.crowd.size.value.mobJitterScale", "-0.359445" },
            { "base.crowd.size.value.value", "0.409724" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowd.type.check", "linearDown" },
            { "base.range.radius.periodic.amplitude.mobJitterScale", "-0.194140" },
            { "base.range.radius.periodic.amplitude.value", "-0.453953" },
            { "base.range.radius.periodic.period.mobJitterScale", "-0.638212" },
            { "base.range.radius.periodic.period.value", "2993.852051" },
            { "base.range.radius.periodic.tickShift.mobJitterScale", "0.994896" },
            { "base.range.radius.periodic.tickShift.value", "3085.046387" },
            { "base.range.radius.value.mobJitterScale", "-0.156886" },
            { "base.range.radius.value.value", "1590.793701" },
            { "base.range.radius.valueType", "periodic" },
            { "base.range.type.check", "quadraticDown" },
            { "base.weight.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "base.weight.periodic.amplitude.value", "-0.248654" },
            { "base.weight.periodic.period.mobJitterScale", "-0.401350" },
            { "base.weight.periodic.period.value", "4502.991211" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.816306" },
            { "base.weight.periodic.tickShift.value", "2512.044434" },
            { "base.weight.value.mobJitterScale", "0.799296" },
            { "base.weight.value.value", "2.842355" },
            { "base.weight.valueType", "constant" },
            { "baseDefense.crowd.radius.periodic.amplitude.mobJitterScale", "0.285788" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "0.757987" },
            { "baseDefense.crowd.radius.periodic.period.mobJitterScale", "-1.000000" },
            { "baseDefense.crowd.radius.periodic.period.value", "1476.886719" },
            { "baseDefense.crowd.radius.periodic.tickShift.mobJitterScale", "-0.123665" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "3099.970703" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "-0.106633" },
            { "baseDefense.crowd.radius.value.value", "1722.770264" },
            { "baseDefense.crowd.radius.valueType", "periodic" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "0.565412" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "0.252985" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "-0.396564" },
            { "baseDefense.crowd.size.periodic.period.value", "4981.864258" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "0.766820" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "7079.573242" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.868215" },
            { "baseDefense.crowd.size.value.value", "2.190912" },
            { "baseDefense.crowd.size.valueType", "periodic" },
            { "baseDefense.crowd.type.check", "quadraticUp" },
            { "baseDefense.range.radius.periodic.amplitude.mobJitterScale", "-0.222102" },
            { "baseDefense.range.radius.periodic.amplitude.value", "-0.430022" },
            { "baseDefense.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "baseDefense.range.radius.periodic.period.value", "6928.268066" },
            { "baseDefense.range.radius.periodic.tickShift.mobJitterScale", "0.475312" },
            { "baseDefense.range.radius.periodic.tickShift.value", "331.950409" },
            { "baseDefense.range.radius.value.mobJitterScale", "0.920556" },
            { "baseDefense.range.radius.value.value", "866.982849" },
            { "baseDefense.range.radius.valueType", "constant" },
            { "baseDefense.range.type.check", "linearDown" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "0.884360" },
            { "baseDefense.weight.periodic.amplitude.value", "0.021473" },
            { "baseDefense.weight.periodic.period.mobJitterScale", "-0.017738" },
            { "baseDefense.weight.periodic.period.value", "2690.639404" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.585810" },
            { "baseDefense.weight.periodic.tickShift.value", "4322.806152" },
            { "baseDefense.weight.value.mobJitterScale", "0.715160" },
            { "baseDefense.weight.value.value", "6.575263" },
            { "baseDefense.weight.valueType", "constant" },
            { "baseDefenseRadius", "128.028442" },
            { "center.crowd.radius.periodic.amplitude.mobJitterScale", "0.696289" },
            { "center.crowd.radius.periodic.amplitude.value", "0.609851" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "1.000000" },
            { "center.crowd.radius.periodic.period.value", "5847.261230" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "0.120959" },
            { "center.crowd.radius.periodic.tickShift.value", "6988.603516" },
            { "center.crowd.radius.value.mobJitterScale", "0.033704" },
            { "center.crowd.radius.value.value", "1187.036377" },
            { "center.crowd.radius.valueType", "constant" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "-0.537975" },
            { "center.crowd.size.periodic.amplitude.value", "1.000000" },
            { "center.crowd.size.periodic.period.mobJitterScale", "0.441396" },
            { "center.crowd.size.periodic.period.value", "4614.857422" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "0.748221" },
            { "center.crowd.size.periodic.tickShift.value", "5120.527344" },
            { "center.crowd.size.value.mobJitterScale", "0.763667" },
            { "center.crowd.size.value.value", "4.826708" },
            { "center.crowd.size.valueType", "periodic" },
            { "center.crowd.type.check", "always" },
            { "center.range.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "center.range.radius.periodic.amplitude.value", "-0.711256" },
            { "center.range.radius.periodic.period.mobJitterScale", "0.143567" },
            { "center.range.radius.periodic.period.value", "1046.018066" },
            { "center.range.radius.periodic.tickShift.mobJitterScale", "0.396711" },
            { "center.range.radius.periodic.tickShift.value", "1963.267334" },
            { "center.range.radius.value.mobJitterScale", "0.096661" },
            { "center.range.radius.value.value", "1280.593628" },
            { "center.range.radius.valueType", "constant" },
            { "center.range.type.check", "linearUp" },
            { "center.weight.periodic.amplitude.mobJitterScale", "0.947022" },
            { "center.weight.periodic.amplitude.value", "-0.469493" },
            { "center.weight.periodic.period.mobJitterScale", "-0.691841" },
            { "center.weight.periodic.period.value", "9966.955078" },
            { "center.weight.periodic.tickShift.mobJitterScale", "-0.954208" },
            { "center.weight.periodic.tickShift.value", "5428.716797" },
            { "center.weight.value.mobJitterScale", "0.491322" },
            { "center.weight.value.value", "-5.843178" },
            { "center.weight.valueType", "periodic" },
            { "cohere.crowd.radius.periodic.amplitude.mobJitterScale", "-0.299680" },
            { "cohere.crowd.radius.periodic.amplitude.value", "0.568501" },
            { "cohere.crowd.radius.periodic.period.mobJitterScale", "0.209370" },
            { "cohere.crowd.radius.periodic.period.value", "774.078552" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "-0.027889" },
            { "cohere.crowd.radius.periodic.tickShift.value", "9936.387695" },
            { "cohere.crowd.radius.value.mobJitterScale", "0.471226" },
            { "cohere.crowd.radius.value.value", "228.865448" },
            { "cohere.crowd.radius.valueType", "periodic" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "-0.444553" },
            { "cohere.crowd.size.periodic.amplitude.value", "0.232472" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "cohere.crowd.size.periodic.period.value", "1059.192261" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "-0.186381" },
            { "cohere.crowd.size.periodic.tickShift.value", "2944.507324" },
            { "cohere.crowd.size.value.mobJitterScale", "-0.792594" },
            { "cohere.crowd.size.value.value", "7.961658" },
            { "cohere.crowd.size.valueType", "periodic" },
            { "cohere.crowd.type.check", "quadraticDown" },
            { "cohere.range.radius.periodic.amplitude.mobJitterScale", "-0.162244" },
            { "cohere.range.radius.periodic.amplitude.value", "-0.866484" },
            { "cohere.range.radius.periodic.period.mobJitterScale", "0.277039" },
            { "cohere.range.radius.periodic.period.value", "10000.000000" },
            { "cohere.range.radius.periodic.tickShift.mobJitterScale", "-0.628869" },
            { "cohere.range.radius.periodic.tickShift.value", "4785.343750" },
            { "cohere.range.radius.value.mobJitterScale", "-0.068806" },
            { "cohere.range.radius.value.value", "1286.923706" },
            { "cohere.range.radius.valueType", "periodic" },
            { "cohere.range.type.check", "linearDown" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "cohere.weight.periodic.amplitude.value", "-0.293358" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.100429" },
            { "cohere.weight.periodic.period.value", "10000.000000" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "-0.108536" },
            { "cohere.weight.periodic.tickShift.value", "8152.316406" },
            { "cohere.weight.value.mobJitterScale", "0.794773" },
            { "cohere.weight.value.value", "8.948562" },
            { "cohere.weight.valueType", "constant" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "0.670772" },
            { "cores.crowd.radius.periodic.amplitude.value", "-0.625437" },
            { "cores.crowd.radius.periodic.period.mobJitterScale", "-0.952234" },
            { "cores.crowd.radius.periodic.period.value", "5799.777832" },
            { "cores.crowd.radius.periodic.tickShift.mobJitterScale", "0.942014" },
            { "cores.crowd.radius.periodic.tickShift.value", "5613.993652" },
            { "cores.crowd.radius.value.mobJitterScale", "-0.754212" },
            { "cores.crowd.radius.value.value", "118.257202" },
            { "cores.crowd.radius.valueType", "constant" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "-0.934696" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.966898" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-0.390224" },
            { "cores.crowd.size.periodic.period.value", "6323.150391" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.236885" },
            { "cores.crowd.size.periodic.tickShift.value", "7693.854980" },
            { "cores.crowd.size.value.mobJitterScale", "0.346371" },
            { "cores.crowd.size.value.value", "6.987986" },
            { "cores.crowd.size.valueType", "periodic" },
            { "cores.crowd.type.check", "linearDown" },
            { "cores.range.radius.periodic.amplitude.mobJitterScale", "-0.604353" },
            { "cores.range.radius.periodic.amplitude.value", "-0.252468" },
            { "cores.range.radius.periodic.period.mobJitterScale", "-0.175014" },
            { "cores.range.radius.periodic.period.value", "3819.492188" },
            { "cores.range.radius.periodic.tickShift.mobJitterScale", "-0.219337" },
            { "cores.range.radius.periodic.tickShift.value", "2755.559570" },
            { "cores.range.radius.value.mobJitterScale", "0.543138" },
            { "cores.range.radius.value.value", "1245.317871" },
            { "cores.range.radius.valueType", "periodic" },
            { "cores.range.type.check", "linearDown" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "-0.416402" },
            { "cores.weight.periodic.amplitude.value", "-0.058276" },
            { "cores.weight.periodic.period.mobJitterScale", "-0.063266" },
            { "cores.weight.periodic.period.value", "1514.070435" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.721043" },
            { "cores.weight.periodic.tickShift.value", "6451.700684" },
            { "cores.weight.value.mobJitterScale", "-0.523700" },
            { "cores.weight.value.value", "8.336347" },
            { "cores.weight.valueType", "constant" },
            { "corners.crowd.radius.periodic.amplitude.mobJitterScale", "-0.811172" },
            { "corners.crowd.radius.periodic.amplitude.value", "0.555427" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "1.000000" },
            { "corners.crowd.radius.periodic.period.value", "1186.942383" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "-0.598717" },
            { "corners.crowd.radius.periodic.tickShift.value", "4404.385254" },
            { "corners.crowd.radius.value.mobJitterScale", "1.000000" },
            { "corners.crowd.radius.value.value", "1639.207397" },
            { "corners.crowd.radius.valueType", "periodic" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "-0.260853" },
            { "corners.crowd.size.periodic.amplitude.value", "-0.712034" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "0.990000" },
            { "corners.crowd.size.periodic.period.value", "-1.000000" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "1.000000" },
            { "corners.crowd.size.periodic.tickShift.value", "4439.605957" },
            { "corners.crowd.size.value.mobJitterScale", "-0.332281" },
            { "corners.crowd.size.value.value", "15.156243" },
            { "corners.crowd.size.valueType", "constant" },
            { "corners.crowd.type.check", "never" },
            { "corners.range.radius.periodic.amplitude.mobJitterScale", "0.858555" },
            { "corners.range.radius.periodic.amplitude.value", "0.786470" },
            { "corners.range.radius.periodic.period.mobJitterScale", "0.558819" },
            { "corners.range.radius.periodic.period.value", "118.172691" },
            { "corners.range.radius.periodic.tickShift.mobJitterScale", "-0.184481" },
            { "corners.range.radius.periodic.tickShift.value", "3746.594727" },
            { "corners.range.radius.value.mobJitterScale", "0.298147" },
            { "corners.range.radius.value.value", "501.392426" },
            { "corners.range.radius.valueType", "periodic" },
            { "corners.range.type.check", "quadraticDown" },
            { "corners.weight.periodic.amplitude.mobJitterScale", "0.075228" },
            { "corners.weight.periodic.amplitude.value", "-1.000000" },
            { "corners.weight.periodic.period.mobJitterScale", "0.618875" },
            { "corners.weight.periodic.period.value", "8958.683594" },
            { "corners.weight.periodic.tickShift.mobJitterScale", "0.509094" },
            { "corners.weight.periodic.tickShift.value", "8918.312500" },
            { "corners.weight.value.mobJitterScale", "0.342676" },
            { "corners.weight.value.value", "-4.917222" },
            { "corners.weight.valueType", "constant" },
            { "creditReserve", "100.000000" },
            { "curHeadingWeight.periodic.amplitude.mobJitterScale", "0.356755" },
            { "curHeadingWeight.periodic.amplitude.value", "0.330972" },
            { "curHeadingWeight.periodic.period.mobJitterScale", "-1.000000" },
            { "curHeadingWeight.periodic.period.value", "8670.484375" },
            { "curHeadingWeight.periodic.tickShift.mobJitterScale", "-0.658739" },
            { "curHeadingWeight.periodic.tickShift.value", "8101.745117" },
            { "curHeadingWeight.value.mobJitterScale", "-0.340547" },
            { "curHeadingWeight.value.value", "-5.525331" },
            { "curHeadingWeight.valueType", "constant" },
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "-0.480210" },
            { "edges.crowd.radius.periodic.amplitude.value", "0.142294" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "-0.763249" },
            { "edges.crowd.radius.periodic.period.value", "8288.979492" },
            { "edges.crowd.radius.periodic.tickShift.mobJitterScale", "0.891718" },
            { "edges.crowd.radius.periodic.tickShift.value", "8626.647461" },
            { "edges.crowd.radius.value.mobJitterScale", "0.041461" },
            { "edges.crowd.radius.value.value", "1623.985596" },
            { "edges.crowd.radius.valueType", "periodic" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "0.045571" },
            { "edges.crowd.size.periodic.amplitude.value", "-0.194131" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "-0.474647" },
            { "edges.crowd.size.periodic.period.value", "2428.942139" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "1.000000" },
            { "edges.crowd.size.periodic.tickShift.value", "6125.841797" },
            { "edges.crowd.size.value.mobJitterScale", "0.799308" },
            { "edges.crowd.size.value.value", "-0.900000" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.crowd.type.check", "linearDown" },
            { "edges.range.radius.periodic.amplitude.mobJitterScale", "-0.649874" },
            { "edges.range.radius.periodic.amplitude.value", "-0.595534" },
            { "edges.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "edges.range.radius.periodic.period.value", "6409.429199" },
            { "edges.range.radius.periodic.tickShift.mobJitterScale", "-0.091878" },
            { "edges.range.radius.periodic.tickShift.value", "3627.947021" },
            { "edges.range.radius.value.mobJitterScale", "-0.919582" },
            { "edges.range.radius.value.value", "197.747635" },
            { "edges.range.radius.valueType", "constant" },
            { "edges.range.type.check", "strictOn" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.848610" },
            { "edges.weight.periodic.amplitude.value", "0.786939" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.861314" },
            { "edges.weight.periodic.period.value", "4236.356445" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.845323" },
            { "edges.weight.periodic.tickShift.value", "8811.240234" },
            { "edges.weight.value.mobJitterScale", "0.806770" },
            { "edges.weight.value.value", "2.326709" },
            { "edges.weight.valueType", "constant" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "0.010399" },
            { "enemy.crowd.radius.periodic.amplitude.value", "-0.709330" },
            { "enemy.crowd.radius.periodic.period.mobJitterScale", "0.785087" },
            { "enemy.crowd.radius.periodic.period.value", "5393.927246" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "-0.404342" },
            { "enemy.crowd.radius.periodic.tickShift.value", "10000.000000" },
            { "enemy.crowd.radius.value.mobJitterScale", "-0.827424" },
            { "enemy.crowd.radius.value.value", "443.737427" },
            { "enemy.crowd.radius.valueType", "constant" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.051699" },
            { "enemy.crowd.size.periodic.amplitude.value", "0.358980" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.165026" },
            { "enemy.crowd.size.periodic.period.value", "5764.772949" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "1.000000" },
            { "enemy.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "enemy.crowd.size.value.mobJitterScale", "0.415849" },
            { "enemy.crowd.size.value.value", "-0.778737" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowd.type.check", "strictOn" },
            { "enemy.range.radius.periodic.amplitude.mobJitterScale", "0.070196" },
            { "enemy.range.radius.periodic.amplitude.value", "-0.553197" },
            { "enemy.range.radius.periodic.period.mobJitterScale", "1.000000" },
            { "enemy.range.radius.periodic.period.value", "3240.600586" },
            { "enemy.range.radius.periodic.tickShift.mobJitterScale", "-0.368124" },
            { "enemy.range.radius.periodic.tickShift.value", "2933.653564" },
            { "enemy.range.radius.value.mobJitterScale", "1.000000" },
            { "enemy.range.radius.value.value", "1238.138306" },
            { "enemy.range.radius.valueType", "periodic" },
            { "enemy.range.type.check", "linearDown" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "0.532482" },
            { "enemy.weight.periodic.amplitude.value", "-0.764496" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.186991" },
            { "enemy.weight.periodic.period.value", "4648.356934" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.941752" },
            { "enemy.weight.periodic.tickShift.value", "804.940491" },
            { "enemy.weight.value.mobJitterScale", "-0.072978" },
            { "enemy.weight.value.value", "2.462042" },
            { "enemy.weight.valueType", "periodic" },
            { "enemyBase.crowd.radius.periodic.amplitude.mobJitterScale", "-0.709131" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "0.316801" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "0.063136" },
            { "enemyBase.crowd.radius.periodic.period.value", "7927.173340" },
            { "enemyBase.crowd.radius.periodic.tickShift.mobJitterScale", "0.136007" },
            { "enemyBase.crowd.radius.periodic.tickShift.value", "3544.970703" },
            { "enemyBase.crowd.radius.value.mobJitterScale", "0.575405" },
            { "enemyBase.crowd.radius.value.value", "1011.343018" },
            { "enemyBase.crowd.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "-0.095005" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "-0.372385" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "0.805413" },
            { "enemyBase.crowd.size.periodic.period.value", "1681.290649" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.921788" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "4468.683594" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.363864" },
            { "enemyBase.crowd.size.value.value", "-1.000000" },
            { "enemyBase.crowd.size.valueType", "periodic" },
            { "enemyBase.crowd.type.check", "strictOff" },
            { "enemyBase.range.radius.periodic.amplitude.mobJitterScale", "0.323381" },
            { "enemyBase.range.radius.periodic.amplitude.value", "-0.164748" },
            { "enemyBase.range.radius.periodic.period.mobJitterScale", "0.029263" },
            { "enemyBase.range.radius.periodic.period.value", "3113.116943" },
            { "enemyBase.range.radius.periodic.tickShift.mobJitterScale", "0.472799" },
            { "enemyBase.range.radius.periodic.tickShift.value", "2174.445312" },
            { "enemyBase.range.radius.value.mobJitterScale", "-0.177846" },
            { "enemyBase.range.radius.value.value", "727.765442" },
            { "enemyBase.range.radius.valueType", "constant" },
            { "enemyBase.range.type.check", "quadraticDown" },
            { "enemyBase.weight.periodic.amplitude.mobJitterScale", "0.826723" },
            { "enemyBase.weight.periodic.amplitude.value", "0.028908" },
            { "enemyBase.weight.periodic.period.mobJitterScale", "-0.459222" },
            { "enemyBase.weight.periodic.period.value", "9581.925781" },
            { "enemyBase.weight.periodic.tickShift.mobJitterScale", "0.039936" },
            { "enemyBase.weight.periodic.tickShift.value", "10000.000000" },
            { "enemyBase.weight.value.mobJitterScale", "0.846529" },
            { "enemyBase.weight.value.value", "2.456953" },
            { "enemyBase.weight.valueType", "periodic" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.mobJitterScale", "-0.336719" },
            { "enemyBaseGuess.crowd.radius.periodic.amplitude.value", "0.317861" },
            { "enemyBaseGuess.crowd.radius.periodic.period.mobJitterScale", "0.773975" },
            { "enemyBaseGuess.crowd.radius.periodic.period.value", "125.151894" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.mobJitterScale", "-0.553654" },
            { "enemyBaseGuess.crowd.radius.periodic.tickShift.value", "4522.884277" },
            { "enemyBaseGuess.crowd.radius.value.mobJitterScale", "-0.685604" },
            { "enemyBaseGuess.crowd.radius.value.value", "1975.779297" },
            { "enemyBaseGuess.crowd.radius.valueType", "periodic" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.mobJitterScale", "0.321797" },
            { "enemyBaseGuess.crowd.size.periodic.amplitude.value", "-0.469148" },
            { "enemyBaseGuess.crowd.size.periodic.period.mobJitterScale", "0.082773" },
            { "enemyBaseGuess.crowd.size.periodic.period.value", "4445.586426" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.mobJitterScale", "1.000000" },
            { "enemyBaseGuess.crowd.size.periodic.tickShift.value", "8100.575684" },
            { "enemyBaseGuess.crowd.size.value.mobJitterScale", "-0.784789" },
            { "enemyBaseGuess.crowd.size.value.value", "1.524315" },
            { "enemyBaseGuess.crowd.size.valueType", "constant" },
            { "enemyBaseGuess.crowd.type.check", "strictOn" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.mobJitterScale", "0.122593" },
            { "enemyBaseGuess.range.radius.periodic.amplitude.value", "-1.000000" },
            { "enemyBaseGuess.range.radius.periodic.period.mobJitterScale", "0.900000" },
            { "enemyBaseGuess.range.radius.periodic.period.value", "852.377075" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.mobJitterScale", "0.616380" },
            { "enemyBaseGuess.range.radius.periodic.tickShift.value", "-1.000000" },
            { "enemyBaseGuess.range.radius.value.mobJitterScale", "-0.167669" },
            { "enemyBaseGuess.range.radius.value.value", "479.364502" },
            { "enemyBaseGuess.range.radius.valueType", "constant" },
            { "enemyBaseGuess.range.type.check", "strictOn" },
            { "enemyBaseGuess.weight.periodic.amplitude.mobJitterScale", "0.244743" },
            { "enemyBaseGuess.weight.periodic.amplitude.value", "-0.471580" },
            { "enemyBaseGuess.weight.periodic.period.mobJitterScale", "0.761772" },
            { "enemyBaseGuess.weight.periodic.period.value", "6439.380371" },
            { "enemyBaseGuess.weight.periodic.tickShift.mobJitterScale", "-0.699112" },
            { "enemyBaseGuess.weight.periodic.tickShift.value", "2565.931641" },
            { "enemyBaseGuess.weight.value.mobJitterScale", "0.479500" },
            { "enemyBaseGuess.weight.value.value", "3.772515" },
            { "enemyBaseGuess.weight.valueType", "periodic" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "360.088043" },
            { "evadeStrictDistance", "193.313187" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fighterBaseDefenseUseRadius", "TRUE" },
            { "fleetLocus.circularPeriod", "7200.106445" },
            { "fleetLocus.circularWeight", "2.000000" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.363766" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.value", "0.340568" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.746236" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "6694.945801" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.294756" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "6656.172363" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "-0.477067" },
            { "fleetLocus.force.crowd.radius.value.value", "2000.000000" },
            { "fleetLocus.force.crowd.radius.valueType", "constant" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.173135" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.387623" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "0.416278" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "3261.852051" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.418702" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "2728.879639" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "0.097967" },
            { "fleetLocus.force.crowd.size.value.value", "7.146707" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowd.type.check", "linearUp" },
            { "fleetLocus.force.range.radius.periodic.amplitude.mobJitterScale", "-0.444761" },
            { "fleetLocus.force.range.radius.periodic.amplitude.value", "0.606728" },
            { "fleetLocus.force.range.radius.periodic.period.mobJitterScale", "0.355969" },
            { "fleetLocus.force.range.radius.periodic.period.value", "2344.366699" },
            { "fleetLocus.force.range.radius.periodic.tickShift.mobJitterScale", "-0.010457" },
            { "fleetLocus.force.range.radius.periodic.tickShift.value", "9754.738281" },
            { "fleetLocus.force.range.radius.value.mobJitterScale", "0.721837" },
            { "fleetLocus.force.range.radius.value.value", "1099.753296" },
            { "fleetLocus.force.range.radius.valueType", "periodic" },
            { "fleetLocus.force.range.type.check", "never" },
            { "fleetLocus.force.weight.periodic.amplitude.mobJitterScale", "-0.687651" },
            { "fleetLocus.force.weight.periodic.amplitude.value", "-0.520623" },
            { "fleetLocus.force.weight.periodic.period.mobJitterScale", "-0.064584" },
            { "fleetLocus.force.weight.periodic.period.value", "6728.394531" },
            { "fleetLocus.force.weight.periodic.tickShift.mobJitterScale", "0.208793" },
            { "fleetLocus.force.weight.periodic.tickShift.value", "7239.492188" },
            { "fleetLocus.force.weight.value.mobJitterScale", "-0.468573" },
            { "fleetLocus.force.weight.value.value", "-3.794740" },
            { "fleetLocus.force.weight.valueType", "constant" },
            { "fleetLocus.linearWeight", "1.056830" },
            { "fleetLocus.linearXPeriod", "3329.145508" },
            { "fleetLocus.linearYPeriod", "8856.908203" },
            { "fleetLocus.randomPeriod", "4403.295898" },
            { "fleetLocus.randomWeight", "0.681881" },
            { "fleetLocus.useScaled", "FALSE" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.mobJitterScale", "-0.811663" },
            { "flockDuringAttack.crowd.radius.periodic.amplitude.value", "-0.744393" },
            { "flockDuringAttack.crowd.radius.periodic.period.mobJitterScale", "-0.221764" },
            { "flockDuringAttack.crowd.radius.periodic.period.value", "5883.249512" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.537773" },
            { "flockDuringAttack.crowd.radius.periodic.tickShift.value", "9609.983398" },
            { "flockDuringAttack.crowd.radius.value.mobJitterScale", "-0.414012" },
            { "flockDuringAttack.crowd.radius.value.value", "1002.098267" },
            { "flockDuringAttack.crowd.radius.valueType", "constant" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.mobJitterScale", "0.281305" },
            { "flockDuringAttack.crowd.size.periodic.amplitude.value", "0.519744" },
            { "flockDuringAttack.crowd.size.periodic.period.mobJitterScale", "0.544820" },
            { "flockDuringAttack.crowd.size.periodic.period.value", "6419.064453" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.mobJitterScale", "0.303526" },
            { "flockDuringAttack.crowd.size.periodic.tickShift.value", "1225.631104" },
            { "flockDuringAttack.crowd.size.value.mobJitterScale", "-0.250628" },
            { "flockDuringAttack.crowd.size.value.value", "5.292718" },
            { "flockDuringAttack.crowd.size.valueType", "periodic" },
            { "flockDuringAttack.crowd.type.check", "always" },
            { "flockDuringAttack.forceOn", "FALSE" },
            { "flockDuringAttack.range.radius.periodic.amplitude.mobJitterScale", "0.095261" },
            { "flockDuringAttack.range.radius.periodic.amplitude.value", "-0.345019" },
            { "flockDuringAttack.range.radius.periodic.period.mobJitterScale", "-0.104286" },
            { "flockDuringAttack.range.radius.periodic.period.value", "9000.000000" },
            { "flockDuringAttack.range.radius.periodic.tickShift.mobJitterScale", "-0.117440" },
            { "flockDuringAttack.range.radius.periodic.tickShift.value", "2289.199707" },
            { "flockDuringAttack.range.radius.value.mobJitterScale", "0.283804" },
            { "flockDuringAttack.range.radius.value.value", "1741.565796" },
            { "flockDuringAttack.range.radius.valueType", "periodic" },
            { "flockDuringAttack.range.type.check", "strictOn" },
            { "flockDuringAttack.value.periodic.amplitude.mobJitterScale", "0.411155" },
            { "flockDuringAttack.value.periodic.amplitude.value", "0.539501" },
            { "flockDuringAttack.value.periodic.period.mobJitterScale", "-0.859912" },
            { "flockDuringAttack.value.periodic.period.value", "3534.926025" },
            { "flockDuringAttack.value.periodic.tickShift.mobJitterScale", "-0.166487" },
            { "flockDuringAttack.value.periodic.tickShift.value", "6057.622070" },
            { "flockDuringAttack.value.value.mobJitterScale", "-0.356785" },
            { "flockDuringAttack.value.value.value", "0.538760" },
            { "flockDuringAttack.value.valueType", "constant" },
            { "gatherAbandonStale", "FALSE" },
            { "gatherRange", "-0.832233" },
            { "guardRange", "185.221115" },
            { "hold.invProbability.periodic.amplitude.mobJitterScale", "0.318362" },
            { "hold.invProbability.periodic.amplitude.value", "-0.053504" },
            { "hold.invProbability.periodic.period.mobJitterScale", "-0.743678" },
            { "hold.invProbability.periodic.period.value", "1045.213623" },
            { "hold.invProbability.periodic.tickShift.mobJitterScale", "0.612287" },
            { "hold.invProbability.periodic.tickShift.value", "455.389221" },
            { "hold.invProbability.value.mobJitterScale", "0.608235" },
            { "hold.invProbability.value.value", "3530.888184" },
            { "hold.invProbability.valueType", "periodic" },
            { "hold.ticks.periodic.amplitude.mobJitterScale", "-0.676691" },
            { "hold.ticks.periodic.amplitude.value", "0.327262" },
            { "hold.ticks.periodic.period.mobJitterScale", "0.203408" },
            { "hold.ticks.periodic.period.value", "-1.000000" },
            { "hold.ticks.periodic.tickShift.mobJitterScale", "1.000000" },
            { "hold.ticks.periodic.tickShift.value", "3093.785889" },
            { "hold.ticks.value.mobJitterScale", "-0.498543" },
            { "hold.ticks.value.value", "624.359924" },
            { "hold.ticks.valueType", "periodic" },
            { "maxWeight", "3629.565186" },
            { "mobLocus.circularPeriod.mobJitterScale", "-0.297314" },
            { "mobLocus.circularPeriod.value", "3192.555176" },
            { "mobLocus.circularWeight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "mobLocus.circularWeight.periodic.amplitude.value", "-0.830082" },
            { "mobLocus.circularWeight.periodic.period.mobJitterScale", "0.140692" },
            { "mobLocus.circularWeight.periodic.period.value", "1741.119385" },
            { "mobLocus.circularWeight.periodic.tickShift.mobJitterScale", "0.685835" },
            { "mobLocus.circularWeight.periodic.tickShift.value", "3827.332031" },
            { "mobLocus.circularWeight.value.mobJitterScale", "-0.469187" },
            { "mobLocus.circularWeight.value.value", "5.932658" },
            { "mobLocus.circularWeight.valueType", "constant" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.664722" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.value", "0.828682" },
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.609245" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "2608.378906" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.value", "7995.970215" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "-0.027143" },
            { "mobLocus.force.crowd.radius.value.value", "2000.000000" },
            { "mobLocus.force.crowd.radius.valueType", "constant" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.060932" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-0.727189" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "1.000000" },
            { "mobLocus.force.crowd.size.periodic.period.value", "4822.961914" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.861368" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "1919.164062" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.233443" },
            { "mobLocus.force.crowd.size.value.value", "18.000000" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowd.type.check", "linearUp" },
            { "mobLocus.force.range.radius.periodic.amplitude.mobJitterScale", "0.475362" },
            { "mobLocus.force.range.radius.periodic.amplitude.value", "0.781773" },
            { "mobLocus.force.range.radius.periodic.period.mobJitterScale", "0.078016" },
            { "mobLocus.force.range.radius.periodic.period.value", "5415.509766" },
            { "mobLocus.force.range.radius.periodic.tickShift.mobJitterScale", "0.501703" },
            { "mobLocus.force.range.radius.periodic.tickShift.value", "880.215820" },
            { "mobLocus.force.range.radius.value.mobJitterScale", "0.681131" },
            { "mobLocus.force.range.radius.value.value", "112.687370" },
            { "mobLocus.force.range.radius.valueType", "constant" },
            { "mobLocus.force.range.type.check", "never" },
            { "mobLocus.force.weight.periodic.amplitude.mobJitterScale", "0.584449" },
            { "mobLocus.force.weight.periodic.amplitude.value", "-0.732650" },
            { "mobLocus.force.weight.periodic.period.mobJitterScale", "0.198318" },
            { "mobLocus.force.weight.periodic.period.value", "4663.208496" },
            { "mobLocus.force.weight.periodic.tickShift.mobJitterScale", "0.403765" },
            { "mobLocus.force.weight.periodic.tickShift.value", "-0.900000" },
            { "mobLocus.force.weight.value.mobJitterScale", "0.490125" },
            { "mobLocus.force.weight.value.value", "-0.524109" },
            { "mobLocus.force.weight.valueType", "periodic" },
            { "mobLocus.linearWeight.periodic.amplitude.mobJitterScale", "0.620864" },
            { "mobLocus.linearWeight.periodic.amplitude.value", "0.115564" },
            { "mobLocus.linearWeight.periodic.period.mobJitterScale", "0.664325" },
            { "mobLocus.linearWeight.periodic.period.value", "3259.634766" },
            { "mobLocus.linearWeight.periodic.tickShift.mobJitterScale", "0.309229" },
            { "mobLocus.linearWeight.periodic.tickShift.value", "4737.322266" },
            { "mobLocus.linearWeight.value.mobJitterScale", "0.927468" },
            { "mobLocus.linearWeight.value.value", "-1.347305" },
            { "mobLocus.linearWeight.valueType", "constant" },
            { "mobLocus.linearXPeriod.mobJitterScale", "0.531187" },
            { "mobLocus.linearXPeriod.value", "2982.587402" },
            { "mobLocus.linearYPeriod.mobJitterScale", "-0.240665" },
            { "mobLocus.linearYPeriod.value", "9817.230469" },
            { "mobLocus.proximityRadius.periodic.amplitude.mobJitterScale", "0.529849" },
            { "mobLocus.proximityRadius.periodic.amplitude.value", "-0.249275" },
            { "mobLocus.proximityRadius.periodic.period.mobJitterScale", "0.578319" },
            { "mobLocus.proximityRadius.periodic.period.value", "8083.446777" },
            { "mobLocus.proximityRadius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "mobLocus.proximityRadius.periodic.tickShift.value", "7825.845215" },
            { "mobLocus.proximityRadius.value.mobJitterScale", "1.000000" },
            { "mobLocus.proximityRadius.value.value", "804.822388" },
            { "mobLocus.proximityRadius.valueType", "constant" },
            { "mobLocus.randomPeriod.mobJitterScale", "0.171420" },
            { "mobLocus.randomPeriod.value", "9743.434570" },
            { "mobLocus.randomWeight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "mobLocus.randomWeight.periodic.amplitude.value", "-0.186027" },
            { "mobLocus.randomWeight.periodic.period.mobJitterScale", "0.164212" },
            { "mobLocus.randomWeight.periodic.period.value", "2044.068604" },
            { "mobLocus.randomWeight.periodic.tickShift.mobJitterScale", "-0.408530" },
            { "mobLocus.randomWeight.periodic.tickShift.value", "2595.378906" },
            { "mobLocus.randomWeight.value.mobJitterScale", "0.400760" },
            { "mobLocus.randomWeight.value.value", "8.476021" },
            { "mobLocus.randomWeight.valueType", "constant" },
            { "mobLocus.resetOnProximity", "TRUE" },
            { "mobLocus.useScaled", "FALSE" },
            { "nearBaseRadius", "339.137360" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "0.543061" },
            { "nearBaseRandomIdle.crowd.radius.periodic.amplitude.value", "0.552661" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.mobJitterScale", "0.908919" },
            { "nearBaseRandomIdle.crowd.radius.periodic.period.value", "4981.272461" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.488005" },
            { "nearBaseRandomIdle.crowd.radius.periodic.tickShift.value", "5503.406738" },
            { "nearBaseRandomIdle.crowd.radius.value.mobJitterScale", "-0.283898" },
            { "nearBaseRandomIdle.crowd.radius.value.value", "1810.856323" },
            { "nearBaseRandomIdle.crowd.radius.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.mobJitterScale", "-0.429539" },
            { "nearBaseRandomIdle.crowd.size.periodic.amplitude.value", "1.000000" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.mobJitterScale", "-0.690233" },
            { "nearBaseRandomIdle.crowd.size.periodic.period.value", "5566.616699" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.mobJitterScale", "0.642556" },
            { "nearBaseRandomIdle.crowd.size.periodic.tickShift.value", "2300.637695" },
            { "nearBaseRandomIdle.crowd.size.value.mobJitterScale", "0.254720" },
            { "nearBaseRandomIdle.crowd.size.value.value", "8.118486" },
            { "nearBaseRandomIdle.crowd.size.valueType", "constant" },
            { "nearBaseRandomIdle.crowd.type.check", "always" },
            { "nearBaseRandomIdle.forceOn", "FALSE" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "nearBaseRandomIdle.range.radius.periodic.amplitude.value", "0.146949" },
            { "nearBaseRandomIdle.range.radius.periodic.period.mobJitterScale", "0.660582" },
            { "nearBaseRandomIdle.range.radius.periodic.period.value", "2824.787598" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.543646" },
            { "nearBaseRandomIdle.range.radius.periodic.tickShift.value", "10000.000000" },
            { "nearBaseRandomIdle.range.radius.value.mobJitterScale", "-0.432970" },
            { "nearBaseRandomIdle.range.radius.value.value", "1285.594849" },
            { "nearBaseRandomIdle.range.radius.valueType", "constant" },
            { "nearBaseRandomIdle.range.type.check", "linearDown" },
            { "nearBaseRandomIdle.value.periodic.amplitude.mobJitterScale", "0.180797" },
            { "nearBaseRandomIdle.value.periodic.amplitude.value", "0.554815" },
            { "nearBaseRandomIdle.value.periodic.period.mobJitterScale", "-0.163976" },
            { "nearBaseRandomIdle.value.periodic.period.value", "6648.952637" },
            { "nearBaseRandomIdle.value.periodic.tickShift.mobJitterScale", "-0.930608" },
            { "nearBaseRandomIdle.value.periodic.tickShift.value", "6743.094238" },
            { "nearBaseRandomIdle.value.value.mobJitterScale", "0.505891" },
            { "nearBaseRandomIdle.value.value.value", "0.440395" },
            { "nearBaseRandomIdle.value.valueType", "periodic" },
            { "nearestFriend.crowd.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "nearestFriend.crowd.radius.periodic.amplitude.value", "1.000000" },
            { "nearestFriend.crowd.radius.periodic.period.mobJitterScale", "-0.104158" },
            { "nearestFriend.crowd.radius.periodic.period.value", "5180.632812" },
            { "nearestFriend.crowd.radius.periodic.tickShift.mobJitterScale", "0.552689" },
            { "nearestFriend.crowd.radius.periodic.tickShift.value", "8807.058594" },
            { "nearestFriend.crowd.radius.value.mobJitterScale", "0.469307" },
            { "nearestFriend.crowd.radius.value.value", "1826.774658" },
            { "nearestFriend.crowd.radius.valueType", "constant" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "-0.489218" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "0.005182" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "-0.399793" },
            { "nearestFriend.crowd.size.periodic.period.value", "-0.900000" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "-0.581479" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "0.097649" },
            { "nearestFriend.crowd.size.value.value", "7.881464" },
            { "nearestFriend.crowd.size.valueType", "periodic" },
            { "nearestFriend.crowd.type.check", "quadraticUp" },
            { "nearestFriend.range.radius.periodic.amplitude.mobJitterScale", "-0.108594" },
            { "nearestFriend.range.radius.periodic.amplitude.value", "0.900000" },
            { "nearestFriend.range.radius.periodic.period.mobJitterScale", "-0.081017" },
            { "nearestFriend.range.radius.periodic.period.value", "4171.003418" },
            { "nearestFriend.range.radius.periodic.tickShift.mobJitterScale", "0.900000" },
            { "nearestFriend.range.radius.periodic.tickShift.value", "1167.001343" },
            { "nearestFriend.range.radius.value.mobJitterScale", "-0.524529" },
            { "nearestFriend.range.radius.value.value", "2000.000000" },
            { "nearestFriend.range.radius.valueType", "periodic" },
            { "nearestFriend.range.type.check", "always" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "0.538562" },
            { "nearestFriend.weight.periodic.amplitude.value", "0.927548" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "-0.285779" },
            { "nearestFriend.weight.periodic.period.value", "1295.037354" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "-0.675014" },
            { "nearestFriend.weight.periodic.tickShift.value", "7606.952148" },
            { "nearestFriend.weight.value.mobJitterScale", "0.499382" },
            { "nearestFriend.weight.value.value", "-7.055893" },
            { "nearestFriend.weight.valueType", "constant" },
            { "randomIdle.crowd.radius.periodic.amplitude.mobJitterScale", "-0.349655" },
            { "randomIdle.crowd.radius.periodic.amplitude.value", "0.181293" },
            { "randomIdle.crowd.radius.periodic.period.mobJitterScale", "-0.610381" },
            { "randomIdle.crowd.radius.periodic.period.value", "1653.672974" },
            { "randomIdle.crowd.radius.periodic.tickShift.mobJitterScale", "-0.362949" },
            { "randomIdle.crowd.radius.periodic.tickShift.value", "8409.475586" },
            { "randomIdle.crowd.radius.value.mobJitterScale", "-0.900000" },
            { "randomIdle.crowd.radius.value.value", "1272.007202" },
            { "randomIdle.crowd.radius.valueType", "constant" },
            { "randomIdle.crowd.size.periodic.amplitude.mobJitterScale", "-0.458146" },
            { "randomIdle.crowd.size.periodic.amplitude.value", "0.473360" },
            { "randomIdle.crowd.size.periodic.period.mobJitterScale", "-0.500502" },
            { "randomIdle.crowd.size.periodic.period.value", "8924.709961" },
            { "randomIdle.crowd.size.periodic.tickShift.mobJitterScale", "0.341293" },
            { "randomIdle.crowd.size.periodic.tickShift.value", "1060.740112" },
            { "randomIdle.crowd.size.value.mobJitterScale", "1.000000" },
            { "randomIdle.crowd.size.value.value", "2.116166" },
            { "randomIdle.crowd.size.valueType", "constant" },
            { "randomIdle.crowd.type.check", "never" },
            { "randomIdle.forceOn", "FALSE" },
            { "randomIdle.range.radius.periodic.amplitude.mobJitterScale", "-0.094619" },
            { "randomIdle.range.radius.periodic.amplitude.value", "0.119609" },
            { "randomIdle.range.radius.periodic.period.mobJitterScale", "0.279987" },
            { "randomIdle.range.radius.periodic.period.value", "4106.283691" },
            { "randomIdle.range.radius.periodic.tickShift.mobJitterScale", "0.502013" },
            { "randomIdle.range.radius.periodic.tickShift.value", "10000.000000" },
            { "randomIdle.range.radius.value.mobJitterScale", "-0.609735" },
            { "randomIdle.range.radius.value.value", "1222.541992" },
            { "randomIdle.range.radius.valueType", "periodic" },
            { "randomIdle.range.type.check", "quadraticDown" },
            { "randomIdle.value.periodic.amplitude.mobJitterScale", "-0.580430" },
            { "randomIdle.value.periodic.amplitude.value", "0.359722" },
            { "randomIdle.value.periodic.period.mobJitterScale", "1.000000" },
            { "randomIdle.value.periodic.period.value", "5054.708008" },
            { "randomIdle.value.periodic.tickShift.mobJitterScale", "-0.118220" },
            { "randomIdle.value.periodic.tickShift.value", "2107.932373" },
            { "randomIdle.value.value.mobJitterScale", "0.811275" },
            { "randomIdle.value.value.value", "0.661717" },
            { "randomIdle.value.valueType", "constant" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.mobJitterScale", "0.803281" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.amplitude.value", "-0.136580" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.mobJitterScale", "0.080398" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.period.value", "9682.730469" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.mobJitterScale", "-0.106544" },
            { "randomizeStoppedVelocity.crowd.radius.periodic.tickShift.value", "2996.027100" },
            { "randomizeStoppedVelocity.crowd.radius.value.mobJitterScale", "-0.073465" },
            { "randomizeStoppedVelocity.crowd.radius.value.value", "-0.810000" },
            { "randomizeStoppedVelocity.crowd.radius.valueType", "constant" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.mobJitterScale", "-0.507826" },
            { "randomizeStoppedVelocity.crowd.size.periodic.amplitude.value", "1.000000" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.mobJitterScale", "0.776489" },
            { "randomizeStoppedVelocity.crowd.size.periodic.period.value", "2690.810303" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.mobJitterScale", "-0.081538" },
            { "randomizeStoppedVelocity.crowd.size.periodic.tickShift.value", "6096.250000" },
            { "randomizeStoppedVelocity.crowd.size.value.mobJitterScale", "-0.430324" },
            { "randomizeStoppedVelocity.crowd.size.value.value", "13.399354" },
            { "randomizeStoppedVelocity.crowd.size.valueType", "periodic" },
            { "randomizeStoppedVelocity.crowd.type.check", "strictOff" },
            { "randomizeStoppedVelocity.forceOn", "FALSE" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.mobJitterScale", "0.608389" },
            { "randomizeStoppedVelocity.range.radius.periodic.amplitude.value", "-0.318620" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.mobJitterScale", "0.761119" },
            { "randomizeStoppedVelocity.range.radius.periodic.period.value", "7762.241211" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.mobJitterScale", "-0.750283" },
            { "randomizeStoppedVelocity.range.radius.periodic.tickShift.value", "4700.399414" },
            { "randomizeStoppedVelocity.range.radius.value.mobJitterScale", "-0.173849" },
            { "randomizeStoppedVelocity.range.radius.value.value", "869.773987" },
            { "randomizeStoppedVelocity.range.radius.valueType", "constant" },
            { "randomizeStoppedVelocity.range.type.check", "quadraticDown" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.mobJitterScale", "0.219595" },
            { "randomizeStoppedVelocity.value.periodic.amplitude.value", "-1.000000" },
            { "randomizeStoppedVelocity.value.periodic.period.mobJitterScale", "1.000000" },
            { "randomizeStoppedVelocity.value.periodic.period.value", "4190.914062" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.mobJitterScale", "-0.725569" },
            { "randomizeStoppedVelocity.value.periodic.tickShift.value", "5065.706055" },
            { "randomizeStoppedVelocity.value.value.mobJitterScale", "0.217063" },
            { "randomizeStoppedVelocity.value.value.value", "0.868241" },
            { "randomizeStoppedVelocity.value.valueType", "periodic" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "9.305043" },
            { "sensorGrid.staleFighterTime", "1.049959" },
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.363382" },
            { "separate.crowd.radius.periodic.amplitude.value", "0.259256" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "-0.628775" },
            { "separate.crowd.radius.periodic.period.value", "4992.346191" },
            { "separate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.260836" },
            { "separate.crowd.radius.periodic.tickShift.value", "-0.810000" },
            { "separate.crowd.radius.value.mobJitterScale", "0.146112" },
            { "separate.crowd.radius.value.value", "1444.304077" },
            { "separate.crowd.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "-0.801360" },
            { "separate.crowd.size.periodic.amplitude.value", "0.197410" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "0.098937" },
            { "separate.crowd.size.periodic.period.value", "3907.248779" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "-0.968204" },
            { "separate.crowd.size.periodic.tickShift.value", "9811.442383" },
            { "separate.crowd.size.value.mobJitterScale", "-1.000000" },
            { "separate.crowd.size.value.value", "16.846748" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowd.type.check", "always" },
            { "separate.range.radius.periodic.amplitude.mobJitterScale", "-0.197067" },
            { "separate.range.radius.periodic.amplitude.value", "-0.048890" },
            { "separate.range.radius.periodic.period.mobJitterScale", "-0.670855" },
            { "separate.range.radius.periodic.period.value", "5883.599609" },
            { "separate.range.radius.periodic.tickShift.mobJitterScale", "-0.144719" },
            { "separate.range.radius.periodic.tickShift.value", "4283.576660" },
            { "separate.range.radius.value.mobJitterScale", "0.441629" },
            { "separate.range.radius.value.value", "885.321716" },
            { "separate.range.radius.valueType", "periodic" },
            { "separate.range.type.check", "quadraticDown" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "0.565715" },
            { "separate.weight.periodic.amplitude.value", "-0.572895" },
            { "separate.weight.periodic.period.mobJitterScale", "0.274284" },
            { "separate.weight.periodic.period.value", "8201.265625" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.775633" },
            { "separate.weight.periodic.tickShift.value", "6891.218750" },
            { "separate.weight.value.mobJitterScale", "-0.620263" },
            { "separate.weight.value.value", "-5.140159" },
            { "separate.weight.valueType", "periodic" },
            { "simpleAttack.crowd.radius.periodic.amplitude.mobJitterScale", "0.143816" },
            { "simpleAttack.crowd.radius.periodic.amplitude.value", "0.829035" },
            { "simpleAttack.crowd.radius.periodic.period.mobJitterScale", "0.241336" },
            { "simpleAttack.crowd.radius.periodic.period.value", "7431.102051" },
            { "simpleAttack.crowd.radius.periodic.tickShift.mobJitterScale", "0.922069" },
            { "simpleAttack.crowd.radius.periodic.tickShift.value", "4407.379883" },
            { "simpleAttack.crowd.radius.value.mobJitterScale", "0.804361" },
            { "simpleAttack.crowd.radius.value.value", "1192.699341" },
            { "simpleAttack.crowd.radius.valueType", "constant" },
            { "simpleAttack.crowd.size.periodic.amplitude.mobJitterScale", "-0.327236" },
            { "simpleAttack.crowd.size.periodic.amplitude.value", "0.121028" },
            { "simpleAttack.crowd.size.periodic.period.mobJitterScale", "0.170843" },
            { "simpleAttack.crowd.size.periodic.period.value", "6771.612305" },
            { "simpleAttack.crowd.size.periodic.tickShift.mobJitterScale", "0.049026" },
            { "simpleAttack.crowd.size.periodic.tickShift.value", "255.226700" },
            { "simpleAttack.crowd.size.value.mobJitterScale", "0.434253" },
            { "simpleAttack.crowd.size.value.value", "10.947847" },
            { "simpleAttack.crowd.size.valueType", "constant" },
            { "simpleAttack.crowd.type.check", "quadraticUp" },
            { "simpleAttack.forceOn", "TRUE" },
            { "simpleAttack.range.radius.periodic.amplitude.mobJitterScale", "-0.649143" },
            { "simpleAttack.range.radius.periodic.amplitude.value", "-0.322237" },
            { "simpleAttack.range.radius.periodic.period.mobJitterScale", "-0.651443" },
            { "simpleAttack.range.radius.periodic.period.value", "3533.778076" },
            { "simpleAttack.range.radius.periodic.tickShift.mobJitterScale", "-0.528107" },
            { "simpleAttack.range.radius.periodic.tickShift.value", "1401.240723" },
            { "simpleAttack.range.radius.value.mobJitterScale", "-0.079376" },
            { "simpleAttack.range.radius.value.value", "1610.969360" },
            { "simpleAttack.range.radius.valueType", "constant" },
            { "simpleAttack.range.type.check", "quadraticDown" },
            { "simpleAttack.value.periodic.amplitude.mobJitterScale", "-0.733821" },
            { "simpleAttack.value.periodic.amplitude.value", "-1.000000" },
            { "simpleAttack.value.periodic.period.mobJitterScale", "-0.259054" },
            { "simpleAttack.value.periodic.period.value", "7195.044434" },
            { "simpleAttack.value.periodic.tickShift.mobJitterScale", "0.406757" },
            { "simpleAttack.value.periodic.tickShift.value", "2876.858154" },
            { "simpleAttack.value.value.mobJitterScale", "-0.064954" },
            { "simpleAttack.value.value.value", "-1.000000" },
            { "simpleAttack.value.valueType", "constant" },
            { "startingMaxRadius", "1845.642090" },
            { "startingMinRadius", "427.468964" },
        };

        struct {
            BundleConfigValue *values;
            uint numValues;
        } configs[] = {
            { defaults,  ARRAYSIZE(defaults), },
            { configs1,  ARRAYSIZE(configs1), },
            { configs2,  ARRAYSIZE(configs2), },
            { configs3,  ARRAYSIZE(configs3), },
            { configs4,  ARRAYSIZE(configs4), },
            { configs5,  ARRAYSIZE(configs5), },
            { configs6,  ARRAYSIZE(configs6), },
            { configs7,  ARRAYSIZE(configs7), },
            { configs8,  ARRAYSIZE(configs8), },
            { configs9,  ARRAYSIZE(configs9), },
            { configs10, ARRAYSIZE(configs10), },
            { configs11, ARRAYSIZE(configs11), },
            { configs12, ARRAYSIZE(configs12), },
        };

        int bundleIndex = aiType - FLEET_AI_BUNDLE1 + 1;
        VERIFY(aiType >= FLEET_AI_BUNDLE1);
        VERIFY(aiType <= FLEET_AI_BUNDLE12);
        VERIFY(bundleIndex >= 1 && bundleIndex < ARRAYSIZE(configs));

        for (int i = bundleIndex; i >= 0; i--) {
            BundleConfigValue *curConfig = configs[i].values;
            uint size = configs[i].numValues;
            for (uint k = 0; k < size; k++) {
                if (curConfig[k].value != NULL &&
                    !MBRegistry_ContainsKey(mreg, curConfig[k].key)) {
                    MBRegistry_PutConst(mreg, curConfig[k].key,
                                        curConfig[k].value);
                }
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

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".mobJitterScalePow");
        ba->mobJitterScalePow = MBRegistry_GetFloat(mreg, MBString_GetCStr(&s));
        ASSERT(!isnanf(ba->mobJitterScalePow));

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
            strcmp(cs, "never") == 0 ||
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
            PANIC("Unknown BundleCheck type = %s\n", cs);
        }
    }

    void loadBundleRange(MBRegistry *mreg, BundleRange *br,
                         const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".type");
        loadBundleCheck(mreg, &br->check, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".radius");
        loadBundleValue(mreg, &br->radius, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    void loadBundleForce(MBRegistry *mreg, BundleForce *b,
                         const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".range");
        loadBundleRange(mreg, &b->range, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".weight");
        loadBundleValue(mreg, &b->weight, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".crowd");
        loadBundleCrowd(mreg, &b->crowd, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    void loadBundleBool(MBRegistry *mreg, BundleBool *b,
                         const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".forceOn");
        b->forceOn = MBRegistry_GetBool(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".range");
        loadBundleRange(mreg, &b->range, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".value");
        loadBundleValue(mreg, &b->value, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".crowd");
        loadBundleCrowd(mreg, &b->crowd, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }

    void loadBundleCrowd(MBRegistry *mreg, BundleCrowd *bc,
                         const char *prefix) {
        CMBString s;
        MBString_Create(&s);

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".size");
        loadBundleValue(mreg, &bc->size, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".radius");
        loadBundleValue(mreg, &bc->radius, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".type");
        loadBundleCheck(mreg, &bc->check, MBString_GetCStr(&s));

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

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".resetOnProximity");
        lp->resetOnProximity =
            MBRegistry_GetBool(mreg, MBString_GetCStr(&s));

        MBString_MakeEmpty(&s);
        MBString_AppendCStr(&s, prefix);
        MBString_AppendCStr(&s, ".proximityRadius");
        loadBundleValue(mreg, &lp->proximityRadius, MBString_GetCStr(&s));

        MBString_Destroy(&s);
    }


    virtual void loadRegistry(MBRegistry *mreg) {
        loadBundleBool(mreg, &this->myConfig.randomIdle, "randomIdle");
        loadBundleBool(mreg, &this->myConfig.nearBaseRandomIdle, "nearBaseRandomIdle");
        loadBundleBool(mreg, &this->myConfig.randomizeStoppedVelocity, "randomizeStoppedVelocity");
        loadBundleBool(mreg, &this->myConfig.simpleAttack, "simpleAttack");
        loadBundleBool(mreg, &this->myConfig.flockDuringAttack, "flockDuringAttack");

        loadBundleForce(mreg, &this->myConfig.align, "align");
        loadBundleForce(mreg, &this->myConfig.cohere, "cohere");
        loadBundleForce(mreg, &this->myConfig.separate, "separate");
        loadBundleForce(mreg, &this->myConfig.attackSeparate, "attackSeparate");
        loadBundleForce(mreg, &this->myConfig.nearestFriend, "nearestFriend");

        loadBundleForce(mreg, &this->myConfig.cores, "cores");
        loadBundleForce(mreg, &this->myConfig.enemy, "enemy");
        loadBundleForce(mreg, &this->myConfig.enemyBase, "enemyBase");
        loadBundleForce(mreg, &this->myConfig.enemyBaseGuess, "enemyBaseGuess");

        loadBundleForce(mreg, &this->myConfig.center, "center");
        loadBundleForce(mreg, &this->myConfig.edges, "edges");
        loadBundleForce(mreg, &this->myConfig.corners, "corners");
        loadBundleForce(mreg, &this->myConfig.base, "base");
        loadBundleForce(mreg, &this->myConfig.baseDefense, "baseDefense");

        this->myConfig.nearBaseRadius =
            MBRegistry_GetFloat(mreg, "nearBaseRadius");
        this->myConfig.baseDefenseRadius =
            MBRegistry_GetFloat(mreg, "baseDefenseRadius");
        this->myConfig.fighterBaseDefenseUseRadius =
            MBRegistry_GetBool(mreg, "fighterBaseDefenseUseRadius");
        this->myConfig.brokenCrowdChecks =
            MBRegistry_GetBool(mreg, "brokenCrowdChecks");
        this->myConfig.maxWeight =
            MBRegistry_GetFloat(mreg, "maxWeight");

        loadBundleValue(mreg, &this->myConfig.curHeadingWeight,
                        "curHeadingWeight");

        loadBundleFleetLocus(mreg, &this->myConfig.fleetLocus, "fleetLocus");
        loadBundleMobLocus(mreg, &this->myConfig.mobLocus, "mobLocus");

        loadBundleValue(mreg, &this->myConfig.holdInvProbability,
                        "hold.invProbability");
        loadBundleValue(mreg, &this->myConfig.holdTicks, "hold.ticks");

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    void flockAlign(Mob *mob, FRPoint *rForce) {
        FPoint avgVel;
        float radius = getBundleValue(mob, &myConfig.align.range.radius);
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        sg->friendAvgVelocity(&avgVel, &mob->pos, radius, MOB_FLAG_FIGHTER);
        avgVel.x += mob->pos.x;
        avgVel.y += mob->pos.y;
        applyBundle(mob, rForce, &myConfig.align, &avgVel);
    }

    void flockCohere(Mob *mob, FRPoint *rForce) {
        FPoint avgPos;
        float radius = getBundleValue(mob, &myConfig.cohere.range.radius);
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        sg->friendAvgPos(&avgPos, &mob->pos, radius, MOB_FLAG_FIGHTER);
        applyBundle(mob, rForce, &myConfig.cohere, &avgPos);
    }

    void flockSeparate(Mob *mob, FRPoint *rForce, BundleForce *bundle) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        float cweight;

        if (!crowdCheck(mob, &bundle->crowd, &cweight)) {
            /* No force. */
            return;
        }

        if (bundle->range.check == BUNDLE_CHECK_NEVER) {
            /* No force. */
            return;
        }

        MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);

        while (mit.hasNext()) {
            Mob *f = mit.next();
            ASSERT(f != NULL);

            if (f->mobid != mob->mobid) {
                applyBundle(mob, rForce, bundle, &f->pos);
            }
        }
    }

    void flockNearestFriend(Mob *mob, FRPoint *rForce) {
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        Mob *m;
        int n = 0;
        float cweight;

        if (!crowdCheck(mob, &myConfig.nearestFriend.crowd, &cweight)) {
            /* No force. */
            return;
        }

        do {
            m = sg->findNthClosestFriend(&mob->pos, MOB_FLAG_FIGHTER, n++);

            if (m != NULL && m->mobid != mob->mobid) {
                applyBundle(mob, rForce, &myConfig.nearestFriend, &m->pos);
                return;
            }
        } while (m != NULL);
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

    void flockEdges(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FleetAI *ai = myFleetAI;
        BundleForce *bundle = &myConfig.edges;
        FPoint edgePoint;
        float cweight;

        if (!crowdCheck(mob, &bundle->crowd, &cweight)) {
            /* No force. */
            return;
        }

        /*
         * Left Edge
         */
        edgePoint = mob->pos;
        edgePoint.x = 0.0f;
        applyBundle(mob, rForce, bundle, &edgePoint);

        /*
         * Right Edge
         */
        edgePoint = mob->pos;
        edgePoint.x = ai->bp.width;
        applyBundle(mob, rForce, bundle, &edgePoint);

        /*
         * Top Edge
         */
        edgePoint = mob->pos;
        edgePoint.y = 0.0f;
        applyBundle(mob, rForce, bundle, &edgePoint);

        /*
         * Bottom edge
         */
        edgePoint = mob->pos;
        edgePoint.y = ai->bp.height;
        applyBundle(mob, rForce, bundle, &edgePoint);
    }

    void flockCorners(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        FleetAI *ai = myFleetAI;
        BundleForce *bundle = &myConfig.edges;
        FPoint cornerPoint;
        float cweight;

        if (!crowdCheck(mob, &bundle->crowd, &cweight)) {
            /* No force. */
            return;
        }

        cornerPoint.x = 0.0f;
        cornerPoint.y = 0.0f;
        applyBundle(mob, rForce, bundle, &cornerPoint);

        cornerPoint.x = ai->bp.width;
        cornerPoint.y = 0.0f;
        applyBundle(mob, rForce, bundle, &cornerPoint);

        cornerPoint.x = 0.0f;
        cornerPoint.y = ai->bp.height;
        applyBundle(mob, rForce, bundle, &cornerPoint);

        cornerPoint.x = ai->bp.width;
        cornerPoint.y = ai->bp.height;
        applyBundle(mob, rForce, bundle, &cornerPoint);
    }

    int getMobJitterIndex(float *valuePtr) {
        uintptr_t value = (uintptr_t)valuePtr;
        uintptr_t configStart = (uintptr_t)&myConfig;
        uintptr_t configEnd = (uintptr_t)&((&myConfig)[1]);
        ASSERT(value >= configStart && value + sizeof(float) <= configEnd);
        ASSERT(value % sizeof(float) == 0);
        int index = (value - configStart) / sizeof(float);
        ASSERT(index >= 0);
        ASSERT(index < NUM_MOB_JITTERS);
        return index;
    }

    float getMobJitter(Mob *m, float *value, float pow) {
        RandomState *rs = &myRandomState;

        BundleShipAI *ship = (BundleShipAI *)m->aiMobHandle;
        ASSERT(ship == (BundleShipAI *)getShip(m->mobid));
        ASSERT(ship != NULL);

        if (*value <= 0.0f) {
            return 0.0f;
        }

        int index = getMobJitterIndex(value);
        if (!ship->myShipLive.mobJitters[index].initialized) {
            float mv = RandomState_Float(rs, -*value, *value);
            if (pow >= 2.0f) {
                pow = truncf(pow);
                mv = powf(mv, 1.0f / pow);
            }
            ship->myShipLive.mobJitters[index].value = mv;
        }

        return ship->myShipLive.mobJitters[index].value;
    }

    /*
     * getBundleAtom --
     *     Compute a bundle atom.
     */
    float getBundleAtom(Mob *m, BundleAtom *ba) {
        float jitter = getMobJitter(m, &ba->mobJitterScale,
                                    ba->mobJitterScalePow);

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
                t += p * getMobJitter(m, &bv->periodic.tickShift.mobJitterScale,
                                      1.0f);
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
     * Is this BundleCheck a constant weight value after the trigger
     * is exceeded?  If TRUE, then we don't necessarily need to know the
     * exact value if we know the trigger is met.
     */
    bool isInvariantBundleCheck(BundleCheckType bc) {
        return isConstantBundleCheck(bc) ||
               bc == BUNDLE_CHECK_STRICT_ON ||
               bc == BUNDLE_CHECK_STRICT_OFF;
    }

    /*
     * bundleCheck --
     *    Should this force operate given the current conditions?
     *    Returns TRUE iff the force should operate.
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
        float maxWeight = myConfig.maxWeight;
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

    float getCrowdCount(Mob *mob, float crowdRadius, BundleCheckType bc,
                        float crowdTrigger) {
        FleetAI *ai = myFleetAI;
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        uint tick = ai->tick;

        /*
         * Reuse the last value, if safe.
         */
        if (myCache.crowdCache.mobid == mob->mobid &&
            myCache.crowdCache.tick == tick) {
            if (myCache.crowdCache.radius == crowdRadius) {
                return myCache.crowdCache.count;
            }
            if (isInvariantBundleCheck(bc)) {
                if (myCache.crowdCache.radius <= crowdRadius &&
                    myCache.crowdCache.count >= crowdTrigger) {
                    return myCache.crowdCache.count;
                }
            }
        }

        myCache.crowdCache.mobid = mob->mobid;
        myCache.crowdCache.tick = tick;
        myCache.crowdCache.radius = crowdRadius;
        myCache.crowdCache.count =
            sg->numFriendsInRange(MOB_FLAG_FIGHTER, &mob->pos, crowdRadius);

        return myCache.crowdCache.count;
    }

    /*
     * crowdCheck --
     * Should this force operate given the current crowd size?
     * Returns TRUE iff the force should operate.
     */
    bool crowdCheck(Mob *mob, BundleCrowd *crowd, float *weight) {
        float crowdTrigger = 0.0f;
        float crowdValue = 0.0f;

        /*
         * Skip the complicated calculations if the bundle-check was constant,
         * or we're otherwise in a simple case.
         */
        if (!isConstantBundleCheck(crowd->check)) {
            crowdTrigger = getBundleValue(mob, &crowd->size);

            if (crowdTrigger > 0.0f) {
                if (!myConfig.brokenCrowdChecks) {
                    float crowdRadius = getBundleValue(mob, &crowd->radius);
                    crowdValue = getCrowdCount(mob, crowdRadius, crowd->check,
                                               crowdTrigger);
                }
            }
        }

        return bundleCheck(crowd->check, crowdValue, crowdTrigger,
                           weight);
    }

    /*
     * getBundleBool --
     *    Is this per-mob BundleBool TRUE or FALSE right now?
     */
    bool getBundleBool(Mob *mob, BundleBool *bb) {
        float radius = 0.0f;
        float distance = 0.0f;
        float deadWeight;

        if (bb->forceOn) {
            return TRUE;
        }

        if (!crowdCheck(mob, &bb->crowd, &deadWeight)) {
            return FALSE;
        }

        if (!isConstantBundleCheck(bb->range.check)) {
            MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
            Mob *base = sg->friendBase();

            /*
             * The range check might do weird things once the base is destroyed.
             */
            if (base != NULL) {
                distance = FPoint_Distance(&mob->pos, &base->pos);
                radius = getBundleValue(mob, &bb->range.radius);
            }
        }

        if (!bundleCheck(bb->range.check, distance, radius, &deadWeight)) {
            return FALSE;
        }

        float value = getBundleValue(mob, &bb->value);
        return value > 0.0f;
    }

    /*
     * applyBundle --
     *      Apply a bundle to a given mob to calculate the force.
     */
    void applyBundle(Mob *mob, FRPoint *rForce, BundleForce *bundle,
                     FPoint *focusPos) {
        float cweight;
        if (!crowdCheck(mob, &bundle->crowd, &cweight)) {
            /* No force. */
            return;
        }

        float rweight;
        float radius = 0.0f;
        float distance = 0.0f;
        if (!isConstantBundleCheck(bundle->range.check)) {
            distance = FPoint_Distance(&mob->pos, focusPos);
            radius = getBundleValue(mob, &bundle->range.radius);
        }

        if (!bundleCheck(bundle->range.check, distance, radius, &rweight)) {
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
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        Mob *core = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);

        if (core != NULL) {
            applyBundle(mob, rForce, &myConfig.cores, &core->pos);
        }
    }

    void flockEnemies(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
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
        float randomPeriod = getBundleAtom(mob, &myConfig.mobLocus.randomPeriod);
        float randomWeight = getBundleValue(mob, &myConfig.mobLocus.randomWeight);
        LiveLocusState *live;
        float width = myFleetAI->bp.width;
        float height = myFleetAI->bp.height;

        BundleShipAI *ship = (BundleShipAI *)mob->aiMobHandle;
        ASSERT(ship == (BundleShipAI *)getShip(mob->mobid));
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

            float proximityRadius =
                getBundleValue(mob, &myConfig.mobLocus.proximityRadius);

            if (myConfig.mobLocus.resetOnProximity &&
                proximityRadius > 0.0f &&
                FPoint_Distance(&mobLocus, &mob->pos) <= proximityRadius) {
                /*
                 * If we're within the proximity radius, reset the random point
                 * on the next tick.
                 */
                live->randomTick = 0;
            }
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
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        Mob *base = sg->friendBase();
        if (base != NULL) {
            applyBundle(mob, rForce, &myConfig.base, &base->pos);
        }
    }

    void flockBaseDefense(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
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
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        Mob *base = sg->enemyBase();

        if (base != NULL) {
            applyBundle(mob, rForce, &myConfig.enemyBase, &base->pos);
        }
    }

    void flockEnemyBaseGuess(Mob *mob, FRPoint *rForce) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        if (!sg->hasEnemyBase() && sg->hasEnemyBaseGuess()) {
            FPoint pos = sg->getEnemyBaseGuess();
            applyBundle(mob, rForce, &myConfig.enemyBaseGuess, &pos);
        }
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {

        BasicAIGovernor::doAttack(mob, enemyTarget);

        if (!getBundleBool(mob, &myConfig.simpleAttack)) {
            float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
            FRPoint rPos;
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

            flockSeparate(mob, &rPos, &myConfig.attackSeparate);

            rPos.radius = speed;
            FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
        }

        if (getBundleBool(mob, &myConfig.flockDuringAttack)) {
            doIdle(mob, FALSE);
        }
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        FleetAI *ai = myFleetAI;
        RandomState *rs = &myRandomState;
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        Mob *base = sg->friendBase();
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        bool nearBase;

        BundleShipAI *ship = (BundleShipAI *)mob->aiMobHandle;
        ASSERT(ship == (BundleShipAI *)getShip(mob->mobid));
        ASSERT(ship != NULL);

        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        if (newlyIdle) {
            if (getBundleBool(mob, &myConfig.randomIdle)) {
                mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
                mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
            }
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

            if (getBundleBool(mob, &myConfig.randomizeStoppedVelocity) &&
                rPos.radius < MICRON) {
                rPos.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
            }

            rForce.theta = rPos.theta;
            rForce.radius = getBundleValue(mob, &myConfig.curHeadingWeight);

            flockAlign(mob, &rForce);
            flockCohere(mob, &rForce);
            flockSeparate(mob, &rForce, &myConfig.separate);
            flockNearestFriend(mob, &rForce);

            flockEdges(mob, &rForce);
            flockCorners(mob, &rForce);
            flockCenter(mob, &rForce);
            flockBase(mob, &rForce);
            flockBaseDefense(mob, &rForce);
            flockEnemies(mob, &rForce);
            flockEnemyBase(mob, &rForce);
            flockEnemyBaseGuess(mob, &rForce);
            flockCores(mob, &rForce);
            flockFleetLocus(mob, &rForce);
            flockMobLocus(mob, &rForce);

            if (getBundleBool(mob, &myConfig.randomizeStoppedVelocity) &&
                rForce.radius < MICRON) {
                rForce.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
            }

            rForce.radius = speed;

            FRPoint_ToFPoint(&rForce, &mob->pos, &mob->cmd.target);
            ASSERT(!isnanf(mob->cmd.target.x));
            ASSERT(!isnanf(mob->cmd.target.y));
        } else if (newlyIdle) {
            if (getBundleBool(mob, &myConfig.nearBaseRandomIdle)) {
                mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
                mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
            }
        }

        {
            float hProbInv = getBundleValue(mob, &myConfig.holdInvProbability);
            if (hProbInv >= 1.0f) {
                float hProb = 1.0f / hProbInv;
                if (RandomState_Flip(rs, hProb)) {
                    float tickLength = getBundleValue(mob, &myConfig.holdTicks);
                    if (tickLength >= 1.0f) {
                        ship->hold(&mob->pos, (uint)tickLength);
                    }
                }
            }
        }

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }

    virtual void runTick() {
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;

        BasicAIGovernor::runTick();

        Mob *base = sg->friendBase();

        if (base != NULL && myConfig.baseDefenseRadius > 0.0f) {
            MBVector<Mob *> fv;
            MBVector<Mob *> tv;
            int f = 0;
            int t = 0;

            sg->pushClosestTargetsInRange(tv, MOB_FLAG_SHIP, &base->pos,
                                          myConfig.baseDefenseRadius);

            if (myConfig.fighterBaseDefenseUseRadius) {
                sg->pushClosestFriendsInRange(fv, MOB_FLAG_FIGHTER, &base->pos,
                                              myConfig.baseDefenseRadius);
            } else {
                CMBComparator comp;
                MobP_InitDistanceComparator(&comp, &base->pos);
                sg->pushFriends(fv, MOB_FLAG_FIGHTER);
                fv.sort(MBComparator<Mob *>(&comp));
            }

            Mob *fighter = (f < fv.size()) ? fv[f++] : NULL;
            Mob *target = (t < tv.size()) ? tv[t++] : NULL;

            while (target != NULL && fighter != NULL) {
                BundleShipAI *ship = (BundleShipAI *)fighter->aiMobHandle;
                ASSERT(ship == (BundleShipAI *)getShip(fighter->mobid));
                ASSERT(ship != NULL);

                ship->attack(target);

                fighter = (f < fv.size()) ? fv[f++] : NULL;
                target = (t < tv.size()) ? tv[t++] : NULL;
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
    struct {
        struct {
            MobID mobid;
            uint tick;
            float radius;
            float count;
        } crowdCache;
    } myCache;
};

class BundleFleet {
public:
    BundleFleet(FleetAI *ai)
    :sg(ai->bp.width, ai->bp.height, 0), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        gov.setSeed(RandomState_Uint64(&this->rs));
        sg.setSeed(RandomState_Uint64(&this->rs));

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
    MappingSensorGrid sg;
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
    } else if (aiType == FLEET_AI_BUNDLE2) {
        ops->aiName = "BundleFleet2";
    } else if (aiType == FLEET_AI_BUNDLE3) {
        ops->aiName = "BundleFleet3";
    } else if (aiType == FLEET_AI_BUNDLE4) {
        ops->aiName = "BundleFleet4";
    } else if (aiType == FLEET_AI_BUNDLE5) {
        ops->aiName = "BundleFleet5";
    } else if (aiType == FLEET_AI_BUNDLE6) {
        ops->aiName = "BundleFleet6";
    } else if (aiType == FLEET_AI_BUNDLE7) {
        ops->aiName = "BundleFleet7";
    } else if (aiType == FLEET_AI_BUNDLE8) {
        ops->aiName = "BundleFleet8";
    } else if (aiType == FLEET_AI_BUNDLE9) {
        ops->aiName = "BundleFleet9";
    } else if (aiType == FLEET_AI_BUNDLE10) {
        ops->aiName = "BundleFleet10";
    } else if (aiType == FLEET_AI_BUNDLE11) {
        ops->aiName = "BundleFleet11";
    } else if (aiType == FLEET_AI_BUNDLE12) {
        ops->aiName = "BundleFleet12";
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

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".mobJitterScalePow");
    GetMutationFloatParams(&vf, MBString_GetCStr(&s),
                           MUTATION_TYPE_SCALE_POW, mreg);
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

static void MutateBundleCheck(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    MutationStrParams svf;
    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".check");

    const char *checkOptions[] = {
        "never", "always", "strictOn", "strictOff", "linearUp", "linearDown",
        "quadraticUp", "quadraticDown"
    };

    GetMutationStrParams(&svf, MBString_GetCStr(&s), mreg);
    Mutate_Str(mreg, &svf, 1, checkOptions, ARRAYSIZE(checkOptions));

    MBString_Destroy(&s);
}

static void MutateBundleCrowd(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".type");
    MutateBundleCheck(aiType, mreg, MBString_GetCStr(&s));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".size");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_COUNT);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".radius");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_RADIUS);

    MBString_Destroy(&s);
}

static void MutateBundleRange(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".type");
    MutateBundleCheck(aiType, mreg, MBString_GetCStr(&s));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".radius");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_RADIUS);

    MBString_Destroy(&s);
}

static void MutateBundleForce(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    CMBString s;
    MBString_Create(&s);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowd");
    MutateBundleCrowd(aiType, mreg, MBString_GetCStr(&s));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".range");
    MutateBundleRange(aiType, mreg, MBString_GetCStr(&s));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".weight");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_WEIGHT);

    MBString_Destroy(&s);
}


static void MutateBundleBool(FleetAIType aiType, MBRegistry *mreg,
                              const char *prefix)
{
    CMBString s;
    MBString_Create(&s);

    MutationBoolParams vb;

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".forceOn");
    MBUtil_Zero(&vb, sizeof(vb));
    vb.key = MBString_GetCStr(&s);
    vb.flipRate = 0.05f;
    if (MBRegistry_GetBool(mreg, BUNDLE_SCRAMBLE_KEY)) {
        vb.flipRate = 0.5f;
    }
    Mutate_Bool(mreg, &vb, 1);

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".crowd");
    MutateBundleCrowd(aiType, mreg, MBString_GetCStr(&s));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".range");
    MutateBundleRange(aiType, mreg, MBString_GetCStr(&s));

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".value");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_BOOL);

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
        { ".useScaled",          0.01f},
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

    MBString_MakeEmpty(&s);
    MBString_AppendCStr(&s, prefix);
    MBString_AppendCStr(&s, ".proximityRadius");
    MutateBundleValue(aiType, mreg, MBString_GetCStr(&s),
                      MUTATION_TYPE_RADIUS);

    MutationBoolParams vb[] = {
        // key                  mutation
        { ".useScaled",          0.01f},
        { ".resetOnProximity",   0.01f},
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
        { "baseDefenseRadius",    -1.0f,   500.0f,  0.05f, 0.15f, 0.01f},

        { "maxWeight",             1.0f, 10000.0f,  0.01f, 0.10f, 0.01f},

        { "sensorGrid.staleCoreTime",
                                   0.0f,   50.0f,   0.05f, 0.2f, 0.005f},
        { "sensorGrid.staleFighterTime",
                                   0.0f,   20.0f,   0.05f, 0.2f, 0.005f},
        { "creditReserve",       100.0f,  200.0f,   0.05f, 0.1f, 0.005f},
    };

    MutationBoolParams vb[] = {
        // key                          mutation
        { "evadeFighters",               0.05f },
        { "evadeUseStrictDistance",      0.05f },
        { "attackExtendedRange",         0.05f },
        { "rotateStartingAngle",         0.05f },
        { "gatherAbandonStale",          0.05f },
        { "fighterBaseDefenseUseRadius", 0.05f },
        //{ "brokenCrowdChecks",           0.05f },
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

    MutateBundleBool(aiType, mreg, "randomIdle");
    MutateBundleBool(aiType, mreg, "nearBaseRandomIdle");
    MutateBundleBool(aiType, mreg, "randomizeStoppedVelocity");
    MutateBundleBool(aiType, mreg, "simpleAttack");
    MutateBundleBool(aiType, mreg, "flockDuringAttack");

    MutateBundleForce(aiType, mreg, "align");
    MutateBundleForce(aiType, mreg, "cohere");
    MutateBundleForce(aiType, mreg, "separate");
    MutateBundleForce(aiType, mreg, "attackSeparate");
    MutateBundleForce(aiType, mreg, "nearestFriend");

    MutateBundleForce(aiType, mreg, "cores");
    MutateBundleForce(aiType, mreg, "enemy");
    MutateBundleForce(aiType, mreg, "enemyBase");
    MutateBundleForce(aiType, mreg, "enemyBaseGuess");

    MutateBundleForce(aiType, mreg, "center");
    MutateBundleForce(aiType, mreg, "edges");
    MutateBundleForce(aiType, mreg, "corners");
    MutateBundleForce(aiType, mreg, "base");
    MutateBundleForce(aiType, mreg, "baseDefense");

    MutateBundleValue(aiType, mreg, "curHeadingWeight", MUTATION_TYPE_WEIGHT);

    MutateBundleFleetLocus(aiType, mreg, "fleetLocus");
    MutateBundleMobLocus(aiType, mreg, "mobLocus");

    MutateBundleValue(aiType, mreg, "hold.invProbability",
                      MUTATION_TYPE_INVERSE_PROBABILITY);
    MutateBundleValue(aiType, mreg, "hold.ticks", MUTATION_TYPE_TICKS);

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
    return sf->gov.getShipHandle(m->mobid);
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
