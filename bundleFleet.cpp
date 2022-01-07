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
    bool resetOnProximity;
    BundleValue proximityRadius;
} BundleMobLocus;

typedef struct BundleSpec {
    bool randomIdle;
    bool nearBaseRandomIdle;
    bool randomizeStoppedVelocity;
    bool simpleAttack;

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

    BundleForce enemy;
    BundleForce enemyBase;

    BundleValue curHeadingWeight;

    BundleFleetLocus fleetLocus;
    BundleMobLocus mobLocus;
} BundleSpec;

#define NUM_MOB_JITTERS (sizeof(BundleSpec) / sizeof(float))

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

            { "nearBaseRandomIdle",          "TRUE"      },
            { "randomIdle",                  "TRUE"      },
            { "randomizeStoppedVelocity",    "TRUE"      },
            { "rotateStartingAngle",         "TRUE",     },
            { "simpleAttack",                "TRUE"      },

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
            { "align.crowd.radius.periodic.amplitude.mobJitterScale", "-0.375753" },
            { "align.crowd.radius.periodic.amplitude.value", "0.168301" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "-0.108591" },
            { "align.crowd.radius.periodic.period.value", "5362.412598" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.381522" },
            { "align.crowd.radius.periodic.tickShift.value", "8914.941406" },
            { "align.crowd.radius.value.mobJitterScale", "0.506334" },
            { "align.crowd.radius.value.value", "-1.000000" },
            { "align.crowd.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.968364" },
            { "align.crowd.size.periodic.amplitude.value", "0.406206" },
            { "align.crowd.size.periodic.period.mobJitterScale", "-0.781509" },
            { "align.crowd.size.periodic.period.value", "7556.751465" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.810000" },
            { "align.crowd.size.periodic.tickShift.value", "3798.598145" },
            { "align.crowd.size.value.mobJitterScale", "0.888799" },
            { "align.crowd.size.value.value", "4.721555" },
            { "align.crowd.size.valueType", "constant" },
            { "align.crowdType", "quadraticUp" },
            { "align.radius.periodic.amplitude.mobJitterScale", "0.285279" },
            { "align.radius.periodic.amplitude.value", "-0.499126" },
            { "align.radius.periodic.period.mobJitterScale", "-0.595802" },
            { "align.radius.periodic.period.value", "8468.488281" },
            { "align.radius.periodic.tickShift.mobJitterScale", "-0.768674" },
            { "align.radius.periodic.tickShift.value", "3499.867920" },
            { "align.radius.value.mobJitterScale", "0.132559" },
            { "align.radius.value.value", "1128.759521" },
            { "align.radius.valueType", "constant" },
            { "align.rangeType", "quadraticDown" },
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
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "0.146617" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "-0.234275" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "1.000000" },
            { "attackSeparate.crowd.radius.periodic.period.value", "1603.346924" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "0.546862" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "2839.928223" },
            { "attackSeparate.crowd.radius.value.mobJitterScale", "0.575492" },
            { "attackSeparate.crowd.radius.value.value", "505.146301" },
            { "attackSeparate.crowd.radius.valueType", "constant" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.513018" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.929315" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.795578" },
            { "attackSeparate.crowd.size.periodic.period.value", "5148.794434" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-0.251742" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-1.000000" },
            { "attackSeparate.crowd.size.value.value", "-0.956097" },
            { "attackSeparate.crowd.size.valueType", "periodic" },
            { "attackSeparate.crowdType", "strictOn" },
            { "attackSeparate.radius.periodic.amplitude.mobJitterScale", "-0.395586" },
            { "attackSeparate.radius.periodic.amplitude.value", "0.083737" },
            { "attackSeparate.radius.periodic.period.mobJitterScale", "0.394375" },
            { "attackSeparate.radius.periodic.period.value", "2825.515381" },
            { "attackSeparate.radius.periodic.tickShift.mobJitterScale", "0.406478" },
            { "attackSeparate.radius.periodic.tickShift.value", "3318.301270" },
            { "attackSeparate.radius.value.mobJitterScale", "0.080423" },
            { "attackSeparate.radius.value.value", "1302.873291" },
            { "attackSeparate.radius.valueType", "constant" },
            { "attackSeparate.rangeType", "quadraticUp" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.400281" },
            { "attackSeparate.weight.periodic.amplitude.value", "-0.828507" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "0.480294" },
            { "attackSeparate.weight.periodic.period.value", "9100.205078" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-0.363912" },
            { "attackSeparate.weight.periodic.tickShift.value", "7603.175293" },
            { "attackSeparate.weight.value.mobJitterScale", "0.023761" },
            { "attackSeparate.weight.value.value", "-8.338675" },
            { "attackSeparate.weight.valueType", "constant" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "-0.405763" },
            { "base.crowd.radius.periodic.amplitude.value", "0.457899" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "0.618923" },
            { "base.crowd.radius.periodic.period.value", "942.138184" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "base.crowd.radius.periodic.tickShift.value", "8911.643555" },
            { "base.crowd.radius.value.mobJitterScale", "-0.674800" },
            { "base.crowd.radius.value.value", "1929.544434" },
            { "base.crowd.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.328041" },
            { "base.crowd.size.periodic.amplitude.value", "-0.869783" },
            { "base.crowd.size.periodic.period.mobJitterScale", "0.395191" },
            { "base.crowd.size.periodic.period.value", "10000.000000" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.597640" },
            { "base.crowd.size.periodic.tickShift.value", "5906.695801" },
            { "base.crowd.size.value.mobJitterScale", "-0.447069" },
            { "base.crowd.size.value.value", "2.891666" },
            { "base.crowd.size.valueType", "constant" },
            { "base.crowdType", "strictOn" },
            { "base.radius.periodic.amplitude.mobJitterScale", "-0.572601" },
            { "base.radius.periodic.amplitude.value", "-1.000000" },
            { "base.radius.periodic.period.mobJitterScale", "-0.316050" },
            { "base.radius.periodic.period.value", "6424.068359" },
            { "base.radius.periodic.tickShift.mobJitterScale", "-0.560312" },
            { "base.radius.periodic.tickShift.value", "2900.507812" },
            { "base.radius.value.mobJitterScale", "0.032384" },
            { "base.radius.value.value", "789.669678" },
            { "base.radius.valueType", "periodic" },
            { "base.rangeType", "quadraticDown" },
            { "base.weight.periodic.amplitude.mobJitterScale", "0.455482" },
            { "base.weight.periodic.amplitude.value", "-0.169507" },
            { "base.weight.periodic.period.mobJitterScale", "0.175880" },
            { "base.weight.periodic.period.value", "4732.085449" },
            { "base.weight.periodic.tickShift.mobJitterScale", "0.415551" },
            { "base.weight.periodic.tickShift.value", "3090.739258" },
            { "base.weight.value.mobJitterScale", "-0.220175" },
            { "base.weight.value.value", "0.607997" },
            { "base.weight.valueType", "periodic" },
            { "baseDefense.crowd.radius.periodic.amplitude.mobJitterScale", "-0.741825" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "-0.694093" },
            { "baseDefense.crowd.radius.periodic.period.mobJitterScale", "0.472881" },
            { "baseDefense.crowd.radius.periodic.period.value", "9000.000000" },
            { "baseDefense.crowd.radius.periodic.tickShift.mobJitterScale", "-0.657454" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "2347.528809" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "baseDefense.crowd.radius.value.value", "673.073914" },
            { "baseDefense.crowd.radius.valueType", "constant" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "0.609885" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "-0.967685" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "0.768096" },
            { "baseDefense.crowd.size.periodic.period.value", "2894.345947" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "-0.663293" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "7780.861328" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.125935" },
            { "baseDefense.crowd.size.value.value", "3.461880" },
            { "baseDefense.crowd.size.valueType", "periodic" },
            { "baseDefense.crowdType", "linearDown" },
            { "baseDefense.radius.periodic.amplitude.mobJitterScale", "0.101941" },
            { "baseDefense.radius.periodic.amplitude.value", "-0.104982" },
            { "baseDefense.radius.periodic.period.mobJitterScale", "-0.348962" },
            { "baseDefense.radius.periodic.period.value", "6473.129395" },
            { "baseDefense.radius.periodic.tickShift.mobJitterScale", "-0.325967" },
            { "baseDefense.radius.periodic.tickShift.value", "-0.900000" },
            { "baseDefense.radius.value.mobJitterScale", "0.282842" },
            { "baseDefense.radius.value.value", "1685.650635" },
            { "baseDefense.radius.valueType", "constant" },
            { "baseDefense.rangeType", "quadraticUp" },
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
            { "center.crowd.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "center.crowd.radius.periodic.amplitude.value", "-0.950215" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "-0.536294" },
            { "center.crowd.radius.periodic.period.value", "2385.005127" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "-0.108328" },
            { "center.crowd.radius.periodic.tickShift.value", "5926.232422" },
            { "center.crowd.radius.value.mobJitterScale", "-0.543911" },
            { "center.crowd.radius.value.value", "2000.000000" },
            { "center.crowd.radius.valueType", "periodic" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.254582" },
            { "center.crowd.size.periodic.amplitude.value", "0.900000" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.860170" },
            { "center.crowd.size.periodic.period.value", "9110.958008" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "0.871353" },
            { "center.crowd.size.periodic.tickShift.value", "1929.109985" },
            { "center.crowd.size.value.mobJitterScale", "-1.000000" },
            { "center.crowd.size.value.value", "10.836775" },
            { "center.crowd.size.valueType", "constant" },
            { "center.crowdType", "linearUp" },
            { "center.radius.periodic.amplitude.mobJitterScale", "-0.042662" },
            { "center.radius.periodic.amplitude.value", "0.475815" },
            { "center.radius.periodic.period.mobJitterScale", "0.597518" },
            { "center.radius.periodic.period.value", "8034.912598" },
            { "center.radius.periodic.tickShift.mobJitterScale", "0.900000" },
            { "center.radius.periodic.tickShift.value", "546.022339" },
            { "center.radius.value.mobJitterScale", "1.000000" },
            { "center.radius.value.value", "1257.381958" },
            { "center.radius.valueType", "constant" },
            { "center.rangeType", "strictOn" },
            { "center.weight.periodic.amplitude.mobJitterScale", "0.468783" },
            { "center.weight.periodic.amplitude.value", "-1.000000" },
            { "center.weight.periodic.period.mobJitterScale", "0.757461" },
            { "center.weight.periodic.period.value", "2054.703857" },
            { "center.weight.periodic.tickShift.mobJitterScale", "0.168499" },
            { "center.weight.periodic.tickShift.value", "8765.216797" },
            { "center.weight.value.mobJitterScale", "-0.260256" },
            { "center.weight.value.value", "9.500000" },
            { "center.weight.valueType", "constant" },
            { "cohere.crowd.radius.periodic.amplitude.mobJitterScale", "0.469702" },
            { "cohere.crowd.radius.periodic.amplitude.value", "-0.536102" },
            { "cohere.crowd.radius.periodic.period.mobJitterScale", "-0.390235" },
            { "cohere.crowd.radius.periodic.period.value", "7617.947754" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "0.721943" },
            { "cohere.crowd.radius.periodic.tickShift.value", "9662.836914" },
            { "cohere.crowd.radius.value.mobJitterScale", "-0.662899" },
            { "cohere.crowd.radius.value.value", "1045.569214" },
            { "cohere.crowd.radius.valueType", "periodic" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.196299" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.147787" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "-0.270201" },
            { "cohere.crowd.size.periodic.period.value", "1010.301331" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.003834" },
            { "cohere.crowd.size.periodic.tickShift.value", "1548.892822" },
            { "cohere.crowd.size.value.mobJitterScale", "0.559478" },
            { "cohere.crowd.size.value.value", "4.463946" },
            { "cohere.crowd.size.valueType", "periodic" },
            { "cohere.crowdType", "linearUp" },
            { "cohere.radius.periodic.amplitude.mobJitterScale", "0.309307" },
            { "cohere.radius.periodic.amplitude.value", "0.562855" },
            { "cohere.radius.periodic.period.mobJitterScale", "-0.082082" },
            { "cohere.radius.periodic.period.value", "10000.000000" },
            { "cohere.radius.periodic.tickShift.mobJitterScale", "0.495175" },
            { "cohere.radius.periodic.tickShift.value", "3194.990479" },
            { "cohere.radius.value.mobJitterScale", "0.743487" },
            { "cohere.radius.value.value", "1184.629150" },
            { "cohere.radius.valueType", "periodic" },
            { "cohere.rangeType", "linearUp" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "0.548071" },
            { "cohere.weight.periodic.amplitude.value", "-1.000000" },
            { "cohere.weight.periodic.period.mobJitterScale", "-0.447586" },
            { "cohere.weight.periodic.period.value", "7854.461914" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.270306" },
            { "cohere.weight.periodic.tickShift.value", "1055.695068" },
            { "cohere.weight.value.mobJitterScale", "0.317044" },
            { "cohere.weight.value.value", "-9.957829" },
            { "cohere.weight.valueType", "periodic" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "cores.crowd.radius.periodic.amplitude.value", "0.240894" },
            { "cores.crowd.radius.periodic.period.mobJitterScale", "1.000000" },
            { "cores.crowd.radius.periodic.period.value", "3527.785156" },
            { "cores.crowd.radius.periodic.tickShift.mobJitterScale", "-0.660024" },
            { "cores.crowd.radius.periodic.tickShift.value", "7455.280273" },
            { "cores.crowd.radius.value.mobJitterScale", "-0.293770" },
            { "cores.crowd.radius.value.value", "1992.707275" },
            { "cores.crowd.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "0.869103" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.899535" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-0.927192" },
            { "cores.crowd.size.periodic.period.value", "2185.385498" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "0.470268" },
            { "cores.crowd.size.periodic.tickShift.value", "4954.479004" },
            { "cores.crowd.size.value.mobJitterScale", "0.836118" },
            { "cores.crowd.size.value.value", "-1.000000" },
            { "cores.crowd.size.valueType", "constant" },
            { "cores.crowdType", "linearDown" },
            { "cores.radius.periodic.amplitude.mobJitterScale", "-0.406865" },
            { "cores.radius.periodic.amplitude.value", "-0.698872" },
            { "cores.radius.periodic.period.mobJitterScale", "-0.990000" },
            { "cores.radius.periodic.period.value", "6957.399414" },
            { "cores.radius.periodic.tickShift.mobJitterScale", "0.288618" },
            { "cores.radius.periodic.tickShift.value", "1621.274536" },
            { "cores.radius.value.mobJitterScale", "0.201092" },
            { "cores.radius.value.value", "1698.850952" },
            { "cores.radius.valueType", "periodic" },
            { "cores.rangeType", "strictOn" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.776193" },
            { "cores.weight.periodic.amplitude.value", "-1.000000" },
            { "cores.weight.periodic.period.mobJitterScale", "0.789824" },
            { "cores.weight.periodic.period.value", "5594.181641" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.518301" },
            { "cores.weight.periodic.tickShift.value", "8047.308594" },
            { "cores.weight.value.mobJitterScale", "-0.881993" },
            { "cores.weight.value.value", "4.127701" },
            { "cores.weight.valueType", "constant" },
            { "corners.crowd.radius.periodic.amplitude.mobJitterScale", "-0.043223" },
            { "corners.crowd.radius.periodic.amplitude.value", "0.763812" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "-0.705491" },
            { "corners.crowd.radius.periodic.period.value", "3311.499023" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "0.740681" },
            { "corners.crowd.radius.periodic.tickShift.value", "9048.750977" },
            { "corners.crowd.radius.value.mobJitterScale", "0.603685" },
            { "corners.crowd.radius.value.value", "1280.113892" },
            { "corners.crowd.radius.valueType", "constant" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "-0.823991" },
            { "corners.crowd.size.periodic.amplitude.value", "-0.229961" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "0.651000" },
            { "corners.crowd.size.periodic.period.value", "4474.759766" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "-0.339310" },
            { "corners.crowd.size.periodic.tickShift.value", "654.322571" },
            { "corners.crowd.size.value.mobJitterScale", "-0.792347" },
            { "corners.crowd.size.value.value", "11.848875" },
            { "corners.crowd.size.valueType", "constant" },
            { "corners.crowdType", "quadraticUp" },
            { "corners.radius.periodic.amplitude.mobJitterScale", "-0.746028" },
            { "corners.radius.periodic.amplitude.value", "0.372991" },
            { "corners.radius.periodic.period.mobJitterScale", "-0.328457" },
            { "corners.radius.periodic.period.value", "8884.138672" },
            { "corners.radius.periodic.tickShift.mobJitterScale", "-0.805783" },
            { "corners.radius.periodic.tickShift.value", "2631.196533" },
            { "corners.radius.value.mobJitterScale", "-0.846008" },
            { "corners.radius.value.value", "1448.135498" },
            { "corners.radius.valueType", "periodic" },
            { "corners.rangeType", "quadraticUp" },
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
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "-0.597216" },
            { "edges.crowd.radius.periodic.amplitude.value", "-0.248807" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "-0.865608" },
            { "edges.crowd.radius.periodic.period.value", "9391.663086" },
            { "edges.crowd.radius.periodic.tickShift.mobJitterScale", "0.607740" },
            { "edges.crowd.radius.periodic.tickShift.value", "7446.548828" },
            { "edges.crowd.radius.value.mobJitterScale", "-0.052765" },
            { "edges.crowd.radius.value.value", "884.600647" },
            { "edges.crowd.radius.valueType", "constant" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "0.314669" },
            { "edges.crowd.size.periodic.amplitude.value", "0.814894" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "-0.920538" },
            { "edges.crowd.size.periodic.period.value", "8100.000000" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.679343" },
            { "edges.crowd.size.periodic.tickShift.value", "5788.603027" },
            { "edges.crowd.size.value.mobJitterScale", "0.160453" },
            { "edges.crowd.size.value.value", "4.934954" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.crowdType", "linearDown" },
            { "edges.radius.periodic.amplitude.mobJitterScale", "-0.076425" },
            { "edges.radius.periodic.amplitude.value", "-0.767398" },
            { "edges.radius.periodic.period.mobJitterScale", "0.846003" },
            { "edges.radius.periodic.period.value", "1087.767334" },
            { "edges.radius.periodic.tickShift.mobJitterScale", "0.628644" },
            { "edges.radius.periodic.tickShift.value", "8555.723633" },
            { "edges.radius.value.mobJitterScale", "-0.400609" },
            { "edges.radius.value.value", "579.894897" },
            { "edges.radius.valueType", "constant" },
            { "edges.rangeType", "quadraticDown" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "-0.626343" },
            { "edges.weight.periodic.amplitude.value", "-1.000000" },
            { "edges.weight.periodic.period.mobJitterScale", "0.779804" },
            { "edges.weight.periodic.period.value", "319.415710" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.514598" },
            { "edges.weight.periodic.tickShift.value", "715.643433" },
            { "edges.weight.value.mobJitterScale", "-0.503806" },
            { "edges.weight.value.value", "7.202097" },
            { "edges.weight.valueType", "periodic" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "0.013939" },
            { "enemy.crowd.radius.periodic.amplitude.value", "-0.101509" },
            { "enemy.crowd.radius.periodic.period.mobJitterScale", "-0.137833" },
            { "enemy.crowd.radius.periodic.period.value", "9023.547852" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "0.357310" },
            { "enemy.crowd.radius.periodic.tickShift.value", "7150.236816" },
            { "enemy.crowd.radius.value.mobJitterScale", "-0.280850" },
            { "enemy.crowd.radius.value.value", "928.333801" },
            { "enemy.crowd.radius.valueType", "periodic" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "0.597647" },
            { "enemy.crowd.size.periodic.amplitude.value", "-0.601186" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.801053" },
            { "enemy.crowd.size.periodic.period.value", "90.217484" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "0.054112" },
            { "enemy.crowd.size.periodic.tickShift.value", "7190.281738" },
            { "enemy.crowd.size.value.mobJitterScale", "0.739930" },
            { "enemy.crowd.size.value.value", "20.000000" },
            { "enemy.crowd.size.valueType", "periodic" },
            { "enemy.crowdType", "quadraticDown" },
            { "enemy.radius.periodic.amplitude.mobJitterScale", "0.643513" },
            { "enemy.radius.periodic.amplitude.value", "0.239239" },
            { "enemy.radius.periodic.period.mobJitterScale", "0.433568" },
            { "enemy.radius.periodic.period.value", "5384.457520" },
            { "enemy.radius.periodic.tickShift.mobJitterScale", "-0.225515" },
            { "enemy.radius.periodic.tickShift.value", "8864.593750" },
            { "enemy.radius.value.mobJitterScale", "-0.964279" },
            { "enemy.radius.value.value", "1187.249878" },
            { "enemy.radius.valueType", "periodic" },
            { "enemy.rangeType", "quadraticDown" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "-0.120304" },
            { "enemy.weight.periodic.amplitude.value", "-0.674164" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.147880" },
            { "enemy.weight.periodic.period.value", "3030.774170" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "0.418172" },
            { "enemy.weight.periodic.tickShift.value", "2754.518066" },
            { "enemy.weight.value.mobJitterScale", "0.495560" },
            { "enemy.weight.value.value", "0.570646" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.crowd.radius.periodic.amplitude.mobJitterScale", "-0.336434" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "-0.616073" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "0.004316" },
            { "enemyBase.crowd.radius.periodic.period.value", "6320.368652" },
            { "enemyBase.crowd.radius.periodic.tickShift.mobJitterScale", "0.307523" },
            { "enemyBase.crowd.radius.periodic.tickShift.value", "8326.301758" },
            { "enemyBase.crowd.radius.value.mobJitterScale", "-0.309541" },
            { "enemyBase.crowd.radius.value.value", "1692.562744" },
            { "enemyBase.crowd.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "-0.789261" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.278390" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.113242" },
            { "enemyBase.crowd.size.periodic.period.value", "4456.736328" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "0.155647" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "819.691101" },
            { "enemyBase.crowd.size.value.mobJitterScale", "0.890448" },
            { "enemyBase.crowd.size.value.value", "3.767771" },
            { "enemyBase.crowd.size.valueType", "constant" },
            { "enemyBase.crowdType", "never" },
            { "enemyBase.radius.periodic.amplitude.mobJitterScale", "-0.335143" },
            { "enemyBase.radius.periodic.amplitude.value", "-0.099600" },
            { "enemyBase.radius.periodic.period.mobJitterScale", "-0.667080" },
            { "enemyBase.radius.periodic.period.value", "8413.723633" },
            { "enemyBase.radius.periodic.tickShift.mobJitterScale", "0.106413" },
            { "enemyBase.radius.periodic.tickShift.value", "-1.000000" },
            { "enemyBase.radius.value.mobJitterScale", "0.725988" },
            { "enemyBase.radius.value.value", "596.981567" },
            { "enemyBase.radius.valueType", "periodic" },
            { "enemyBase.rangeType", "linearUp" },
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
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.018064" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.value", "0.510360" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "1.000000" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "6929.174805" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.295397" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "5405.960938" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "-0.542067" },
            { "fleetLocus.force.crowd.radius.value.value", "1741.319824" },
            { "fleetLocus.force.crowd.radius.valueType", "periodic" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.552794" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "0.566449" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "0.215368" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "7589.274414" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.534149" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "6682.489258" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "-1.000000" },
            { "fleetLocus.force.crowd.size.value.value", "7.021504" },
            { "fleetLocus.force.crowd.size.valueType", "periodic" },
            { "fleetLocus.force.crowdType", "linearUp" },
            { "fleetLocus.force.radius.periodic.amplitude.mobJitterScale", "-0.744871" },
            { "fleetLocus.force.radius.periodic.amplitude.value", "-0.330467" },
            { "fleetLocus.force.radius.periodic.period.mobJitterScale", "0.550250" },
            { "fleetLocus.force.radius.periodic.period.value", "6254.499512" },
            { "fleetLocus.force.radius.periodic.tickShift.mobJitterScale", "0.360480" },
            { "fleetLocus.force.radius.periodic.tickShift.value", "249.562729" },
            { "fleetLocus.force.radius.value.mobJitterScale", "0.191180" },
            { "fleetLocus.force.radius.value.value", "1531.966553" },
            { "fleetLocus.force.radius.valueType", "periodic" },
            { "fleetLocus.force.rangeType", "never" },
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
            { "mobLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "0.780872" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.value", "-0.530915" },
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "0.791142" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "7021.401855" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.997706" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.value", "4838.767578" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "0.965789" },
            { "mobLocus.force.crowd.radius.value.value", "801.931396" },
            { "mobLocus.force.crowd.radius.valueType", "constant" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.029166" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-0.410693" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.918282" },
            { "mobLocus.force.crowd.size.periodic.period.value", "8008.793457" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.602070" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "4823.793945" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.819740" },
            { "mobLocus.force.crowd.size.value.value", "10.828095" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowdType", "quadraticUp" },
            { "mobLocus.force.radius.periodic.amplitude.mobJitterScale", "0.264388" },
            { "mobLocus.force.radius.periodic.amplitude.value", "0.996327" },
            { "mobLocus.force.radius.periodic.period.mobJitterScale", "-0.135688" },
            { "mobLocus.force.radius.periodic.period.value", "4152.555664" },
            { "mobLocus.force.radius.periodic.tickShift.mobJitterScale", "-0.035690" },
            { "mobLocus.force.radius.periodic.tickShift.value", "4395.645020" },
            { "mobLocus.force.radius.value.mobJitterScale", "0.067092" },
            { "mobLocus.force.radius.value.value", "26.127590" },
            { "mobLocus.force.radius.valueType", "periodic" },
            { "mobLocus.force.rangeType", "strictOn" },
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
            { "randomIdle", "TRUE" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "28.837776" },
            { "sensorGrid.staleFighterTime", "11.049438" },
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "separate.crowd.radius.periodic.amplitude.value", "0.750069" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "0.634938" },
            { "separate.crowd.radius.periodic.period.value", "2186.090820" },
            { "separate.crowd.radius.periodic.tickShift.mobJitterScale", "0.080305" },
            { "separate.crowd.radius.periodic.tickShift.value", "1221.492310" },
            { "separate.crowd.radius.value.mobJitterScale", "0.218305" },
            { "separate.crowd.radius.value.value", "704.197815" },
            { "separate.crowd.radius.valueType", "constant" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "-0.923307" },
            { "separate.crowd.size.periodic.amplitude.value", "0.167532" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "-0.502371" },
            { "separate.crowd.size.periodic.period.value", "1920.691528" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.016265" },
            { "separate.crowd.size.periodic.tickShift.value", "568.556396" },
            { "separate.crowd.size.value.mobJitterScale", "0.285721" },
            { "separate.crowd.size.value.value", "4.006861" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowdType", "always" },
            { "separate.radius.periodic.amplitude.mobJitterScale", "0.682200" },
            { "separate.radius.periodic.amplitude.value", "-1.000000" },
            { "separate.radius.periodic.period.mobJitterScale", "-0.788658" },
            { "separate.radius.periodic.period.value", "6011.351074" },
            { "separate.radius.periodic.tickShift.mobJitterScale", "-0.548045" },
            { "separate.radius.periodic.tickShift.value", "4171.840332" },
            { "separate.radius.value.mobJitterScale", "0.788270" },
            { "separate.radius.value.value", "1690.626465" },
            { "separate.radius.valueType", "constant" },
            { "separate.rangeType", "quadraticUp" },
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
            { "align.crowd.radius.periodic.amplitude.value", "-0.051140" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "-0.331735" },
            { "align.crowd.radius.periodic.period.value", "4188.526855" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.977962" },
            { "align.crowd.radius.periodic.tickShift.value", "3184.694824" },
            { "align.crowd.radius.value.mobJitterScale", "0.770306" },
            { "align.crowd.radius.value.value", "85.648621" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "0.439141" },
            { "align.crowd.size.periodic.amplitude.value", "-0.681810" },
            { "align.crowd.size.periodic.period.mobJitterScale", "0.126401" },
            { "align.crowdType", "quadraticDown" },
            { "align.radius.periodic.amplitude.mobJitterScale", "-0.160314" },
            { "align.radius.periodic.period.mobJitterScale", "-0.399551" },
            { "align.radius.periodic.period.value", "6872.918945" },
            { "align.radius.value.value", "76.856781" },
            { "align.weight.periodic.amplitude.mobJitterScale", "-0.209113" },
            { "align.weight.periodic.period.mobJitterScale", "0.430720" },
            { "align.weight.periodic.tickShift.mobJitterScale", "0.910730" },
            { "align.weight.value.mobJitterScale", "0.723799" },
            { "align.weight.value.value", "0.827045" },
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "181.827621" },
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.560170" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "-0.500959" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "0.560655" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.032108" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "9644.429688" },
            { "attackSeparate.crowd.radius.value.value", "171.537613" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "0.185109" },
            { "attackSeparate.crowd.size.periodic.period.value", "4507.496582" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "0.945825" },
            { "attackSeparate.crowd.size.value.value", "3.265365" },
            { "attackSeparate.crowdType", "never" },
            { "attackSeparate.radius.periodic.amplitude.mobJitterScale", "-0.203000" },
            { "attackSeparate.radius.periodic.amplitude.value", "0.035319" },
            { "attackSeparate.radius.periodic.period.mobJitterScale", "0.081711" },
            { "attackSeparate.radius.periodic.period.value", "8198.589844" },
            { "attackSeparate.radius.value.value", "229.267609" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.710210" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "-0.540785" },
            { "attackSeparate.weight.periodic.period.value", "4880.460449" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "0.804290" },
            { "attackSeparate.weight.value.value", "3.091066" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "0.675374" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "0.411808" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "0.502247" },
            { "base.crowd.radius.value.mobJitterScale", "0.306025" },
            { "base.crowd.radius.value.value", "686.184509" },
            { "base.crowd.radius.valueType", "constant" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "0.854254" },
            { "base.crowd.size.periodic.amplitude.value", "0.696899" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.579538" },
            { "base.crowd.size.periodic.tickShift.value", "2133.861816" },
            { "base.crowd.size.value.mobJitterScale", "0.124148" },
            { "base.crowd.size.value.value", "16.296532" },
            { "base.crowd.size.valueType", "constant" },
            { "base.radius.periodic.amplitude.mobJitterScale", "0.757746" },
            { "base.radius.periodic.period.mobJitterScale", "-0.944114" },
            { "base.radius.periodic.period.value", "5422.245117" },
            { "base.radius.periodic.tickShift.value", "9813.756836" },
            { "base.radius.value.mobJitterScale", "0.106048" },
            { "base.radius.value.value", "1405.699829" },
            { "base.weight.periodic.amplitude.value", "-0.619102" },
            { "base.weight.periodic.period.mobJitterScale", "-0.754014" },
            { "base.weight.periodic.period.value", "7403.763184" },
            { "base.weight.periodic.tickShift.mobJitterScale", "0.768387" },
            { "base.weight.periodic.tickShift.value", "5993.753418" },
            { "base.weight.value.value", "7.057205" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "-0.881295" },
            { "baseDefense.crowd.radius.periodic.period.value", "998.405029" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "3993.637695" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "-0.581557" },
            { "baseDefense.crowd.radius.value.value", "515.065125" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "0.348966" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "6131.960938" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.134185" },
            { "baseDefense.crowd.size.value.value", "11.412745" },
            { "baseDefense.radius.periodic.amplitude.mobJitterScale", "-0.738953" },
            { "baseDefense.radius.periodic.period.mobJitterScale", "1.000000" },
            { "baseDefense.radius.periodic.tickShift.value", "6603.688965" },
            { "baseDefense.radius.value.value", "1792.569092" },
            { "baseDefense.radius.valueType", "constant" },
            { "baseDefense.weight.periodic.amplitude.mobJitterScale", "-0.875484" },
            { "baseDefense.weight.periodic.tickShift.mobJitterScale", "0.606333" },
            { "baseDefense.weight.periodic.tickShift.value", "984.566772" },
            { "baseDefense.weight.value.mobJitterScale", "0.805653" },
            { "baseDefense.weight.value.value", "-8.000670" },
            { "baseDefenseRadius", "25.127081" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "-0.391096" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "0.033322" },
            { "center.crowd.radius.periodic.tickShift.value", "4016.315674" },
            { "center.crowd.radius.value.mobJitterScale", "-0.527258" },
            { "center.crowd.radius.value.value", "766.094421" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.348274" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "-0.503560" },
            { "center.crowd.size.periodic.tickShift.value", "356.286469" },
            { "center.crowd.size.value.mobJitterScale", "0.271405" },
            { "center.radius.periodic.amplitude.mobJitterScale", "0.224659" },
            { "center.radius.periodic.amplitude.value", "0.776280" },
            { "center.radius.periodic.period.value", "1152.206299" },
            { "center.radius.periodic.tickShift.mobJitterScale", "0.369279" },
            { "center.radius.value.mobJitterScale", "-0.091729" },
            { "center.radius.value.value", "618.021545" },
            { "center.radius.valueType", "constant" },
            { "center.weight.periodic.amplitude.mobJitterScale", "0.164201" },
            { "center.weight.periodic.period.mobJitterScale", "-0.221119" },
            { "center.weight.periodic.tickShift.mobJitterScale", "-0.160915" },
            { "center.weight.periodic.tickShift.value", "7330.156250" },
            { "center.weight.value.mobJitterScale", "-0.571012" },
            { "center.weight.value.value", "-9.242901" },
            { "cohere.crowd.radius.periodic.amplitude.value", "-0.030119" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "-0.125754" },
            { "cohere.crowd.radius.value.mobJitterScale", "-0.710434" },
            { "cohere.crowd.radius.value.value", "221.215759" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "-0.578827" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.748859" },
            { "cohere.crowd.size.periodic.period.value", "7638.923340" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.723084" },
            { "cohere.crowd.size.periodic.tickShift.value", "5434.939453" },
            { "cohere.crowd.size.value.value", "7.000432" },
            { "cohere.radius.periodic.amplitude.mobJitterScale", "0.296641" },
            { "cohere.radius.periodic.amplitude.value", "-0.894092" },
            { "cohere.radius.value.mobJitterScale", "0.392619" },
            { "cohere.radius.value.value", "1946.445679" },
            { "cohere.weight.periodic.period.mobJitterScale", "-0.293398" },
            { "cohere.weight.periodic.period.value", "3857.262451" },
            { "cohere.weight.value.mobJitterScale", "0.196416" },
            { "cohere.weight.value.value", "-2.525773" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "0.942518" },
            { "cores.crowd.radius.periodic.amplitude.value", "-0.980032" },
            { "cores.crowd.radius.periodic.tickShift.value", "377.282043" },
            { "cores.crowd.radius.value.mobJitterScale", "0.707058" },
            { "cores.crowd.size.periodic.amplitude.value", "0.418095" },
            { "cores.crowd.size.periodic.period.value", "2481.943848" },
            { "cores.crowd.size.periodic.tickShift.value", "10000.000000" },
            { "cores.crowd.size.value.value", "15.346873" },
            { "cores.radius.periodic.amplitude.mobJitterScale", "-0.894492" },
            { "cores.radius.periodic.amplitude.value", "-0.310929" },
            { "cores.radius.periodic.period.value", "5571.164062" },
            { "cores.radius.periodic.tickShift.mobJitterScale", "0.553094" },
            { "cores.radius.periodic.tickShift.value", "5228.984863" },
            { "cores.radius.value.mobJitterScale", "0.578323" },
            { "cores.radius.valueType", "constant" },
            { "cores.weight.periodic.amplitude.value", "0.340708" },
            { "cores.weight.periodic.period.value", "8589.230469" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.981617" },
            { "cores.weight.periodic.tickShift.value", "4228.031250" },
            { "cores.weight.value.value", "4.892436" },
            { "corners.crowd.radius.periodic.amplitude.value", "0.719809" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "0.953544" },
            { "corners.crowd.radius.periodic.period.value", "6423.458496" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "0.990934" },
            { "corners.crowd.radius.periodic.tickShift.value", "4013.902832" },
            { "corners.crowd.radius.value.value", "1816.927856" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "0.885372" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "-0.849573" },
            { "corners.crowd.size.periodic.period.value", "603.696777" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "-0.753211" },
            { "corners.crowd.size.value.mobJitterScale", "0.380151" },
            { "corners.crowd.size.value.value", "4.944045" },
            { "corners.radius.periodic.amplitude.mobJitterScale", "-0.343873" },
            { "corners.radius.periodic.amplitude.value", "-0.277044" },
            { "corners.radius.periodic.period.value", "1110.108154" },
            { "corners.radius.periodic.tickShift.mobJitterScale", "0.041403" },
            { "corners.radius.periodic.tickShift.value", "5325.076660" },
            { "corners.radius.value.mobJitterScale", "-1.000000" },
            { "corners.radius.value.value", "746.597595" },
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
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "0.574534" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "-0.387827" },
            { "edges.crowd.radius.periodic.tickShift.value", "2495.512939" },
            { "edges.crowd.radius.value.mobJitterScale", "-0.534629" },
            { "edges.crowd.radius.value.value", "1349.925659" },
            { "edges.crowd.size.periodic.amplitude.value", "0.512276" },
            { "edges.crowd.size.periodic.period.value", "5108.238770" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "0.053099" },
            { "edges.crowd.size.value.mobJitterScale", "0.001837" },
            { "edges.crowd.size.value.value", "1.599778" },
            { "edges.crowd.size.valueType", "constant" },
            { "edges.radius.periodic.amplitude.value", "-0.320596" },
            { "edges.radius.periodic.period.mobJitterScale", "0.307652" },
            { "edges.radius.periodic.period.value", "9804.218750" },
            { "edges.radius.periodic.tickShift.value", "715.696655" },
            { "edges.radius.value.mobJitterScale", "0.090252" },
            { "edges.radius.value.value", "916.723999" },
            { "edges.weight.periodic.amplitude.value", "-0.164524" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.538755" },
            { "edges.weight.periodic.period.value", "1247.841919" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "-0.578042" },
            { "edges.weight.value.mobJitterScale", "-0.265026" },
            { "edges.weight.value.value", "7.174948" },
            { "edges.weight.valueType", "periodic" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "-0.028456" },
            { "enemy.crowd.radius.periodic.amplitude.value", "-0.169839" },
            { "enemy.crowd.radius.periodic.period.value", "6811.915039" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "0.374059" },
            { "enemy.crowd.radius.periodic.tickShift.value", "7704.856445" },
            { "enemy.crowd.radius.value.mobJitterScale", "-0.157291" },
            { "enemy.crowd.radius.value.value", "1692.328491" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "-0.129000" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "-0.390787" },
            { "enemy.crowd.size.periodic.period.value", "544.119995" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "0.459140" },
            { "enemy.crowd.size.value.mobJitterScale", "0.190205" },
            { "enemy.crowd.size.value.value", "3.786107" },
            { "enemy.crowdType", "always" },
            { "enemy.radius.periodic.amplitude.mobJitterScale", "0.442836" },
            { "enemy.radius.periodic.amplitude.value", "0.971019" },
            { "enemy.radius.periodic.period.mobJitterScale", "-0.162324" },
            { "enemy.radius.periodic.period.value", "2294.790283" },
            { "enemy.radius.value.value", "1262.498047" },
            { "enemy.radius.valueType", "constant" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "-0.126755" },
            { "enemy.weight.periodic.amplitude.value", "0.949832" },
            { "enemy.weight.periodic.period.mobJitterScale", "0.863818" },
            { "enemy.weight.periodic.period.value", "8216.765625" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "-0.019070" },
            { "enemy.weight.periodic.tickShift.value", "5589.622559" },
            { "enemy.weight.value.mobJitterScale", "-0.183337" },
            { "enemy.weight.value.value", "-9.282289" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "-0.869706" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "0.464216" },
            { "enemyBase.crowd.radius.periodic.period.value", "5537.635254" },
            { "enemyBase.crowd.radius.value.value", "220.179703" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "0.632945" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.629738" },
            { "enemyBase.crowd.size.value.value", "2.778095" },
            { "enemyBase.crowdType", "quadraticDown" },
            { "enemyBase.radius.periodic.amplitude.value", "-0.146693" },
            { "enemyBase.radius.periodic.period.value", "1956.951904" },
            { "enemyBase.radius.value.value", "435.686005" },
            { "enemyBase.weight.periodic.amplitude.value", "0.836371" },
            { "enemyBase.weight.periodic.period.value", "9834.474609" },
            { "enemyBase.weight.periodic.tickShift.value", "408.331116" },
            { "enemyBase.weight.value.value", "-6.865941" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "368.761627" },
            { "evadeStrictDistance", "364.574127" },
            { "fleetLocus.circularPeriod", "850.855652" },
            { "fleetLocus.circularWeight", "1.548693" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.736081" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "0.467039" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "8195.372070" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.795574" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "3559.967285" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "-0.682656" },
            { "fleetLocus.force.crowd.radius.value.value", "1163.618286" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.512020" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.905756" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "3938.054443" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.059095" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "6545.076660" },
            { "fleetLocus.force.crowd.size.value.value", "3.513504" },
            { "fleetLocus.force.radius.periodic.amplitude.value", "0.292364" },
            { "fleetLocus.force.radius.periodic.tickShift.mobJitterScale", "-0.541241" },
            { "fleetLocus.force.radius.periodic.tickShift.value", "2335.849121" },
            { "fleetLocus.force.radius.value.mobJitterScale", "-0.327379" },
            { "fleetLocus.force.radius.valueType", "periodic" },
            { "fleetLocus.force.rangeType", "never" },
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
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "-0.812566" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "5997.909668" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "-0.592159" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "mobLocus.force.crowd.radius.value.value", "1786.280518" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.354494" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.365438" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "5586.841309" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.307383" },
            { "mobLocus.force.crowd.size.valueType", "constant" },
            { "mobLocus.force.crowdType", "always" },
            { "mobLocus.force.radius.periodic.amplitude.value", "0.746614" },
            { "mobLocus.force.radius.periodic.tickShift.mobJitterScale", "-0.557284" },
            { "mobLocus.force.radius.value.mobJitterScale", "-0.595792" },
            { "mobLocus.force.radius.value.value", "123.061218" },
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
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "0.978855" },
            { "separate.crowd.radius.periodic.amplitude.value", "0.604296" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "0.081784" },
            { "separate.crowd.radius.periodic.period.value", "6466.973145" },
            { "separate.crowd.radius.periodic.tickShift.value", "7154.783203" },
            { "separate.crowd.radius.value.value", "713.626648" },
            { "separate.crowd.size.periodic.amplitude.value", "-0.361206" },
            { "separate.crowd.size.periodic.period.value", "7077.867676" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.190897" },
            { "separate.crowd.size.value.mobJitterScale", "0.742453" },
            { "separate.crowd.size.value.value", "13.192937" },
            { "separate.radius.periodic.amplitude.value", "0.193831" },
            { "separate.radius.periodic.tickShift.mobJitterScale", "0.277915" },
            { "separate.radius.periodic.tickShift.value", "6534.459473" },
            { "separate.radius.value.mobJitterScale", "-0.716639" },
            { "separate.radius.value.value", "1744.508911" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.645268" },
            { "separate.weight.periodic.amplitude.value", "0.164166" },
            { "separate.weight.periodic.period.mobJitterScale", "-0.902149" },
            { "separate.weight.periodic.tickShift.value", "9153.715820" },
            { "separate.weight.value.value", "7.668723" },
            { "startingMaxRadius", "1729.734253" },
            { "startingMinRadius", "673.257263" },
        };

        BundleConfigValue configs4[] = {
            { "align.crowd.radius.periodic.amplitude.mobJitterScale", "-0.997637" },
            { "align.crowd.radius.periodic.amplitude.value", "-0.580417" },
            { "align.crowd.radius.periodic.period.mobJitterScale", "0.416009" },
            { "align.crowd.radius.periodic.period.value", "4539.239258" },
            { "align.crowd.radius.periodic.tickShift.mobJitterScale", "0.664738" },
            { "align.crowd.radius.periodic.tickShift.value", "1402.517334" },
            { "align.crowd.radius.value.mobJitterScale", "-0.192877" },
            { "align.crowd.radius.value.value", "-1.000000" },
            { "align.crowd.radius.valueType", "constant" },
            { "align.crowd.size.periodic.amplitude.mobJitterScale", "-0.760518" },
            { "align.crowd.size.periodic.amplitude.value", "-0.721088" },
            { "align.crowd.size.periodic.period.mobJitterScale", "0.596016" },
            { "align.crowd.size.periodic.period.value", "5568.675293" },
            { "align.crowd.size.periodic.tickShift.mobJitterScale", "0.990000" },
            { "align.crowd.size.periodic.tickShift.value", "3191.691162" },
            { "align.crowd.size.value.mobJitterScale", "0.261138" },
            { "align.crowd.size.value.value", "14.545824" },
            { "align.crowd.size.valueType", "constant" },
            { "align.crowdType", "strictOn" },
            { "align.radius.periodic.amplitude.mobJitterScale", "1.000000" },
            { "align.radius.periodic.amplitude.value", "-1.000000" },
            { "align.radius.periodic.period.mobJitterScale", "-0.663143" },
            { "align.radius.periodic.period.value", "7195.858887" },
            { "align.radius.periodic.tickShift.mobJitterScale", "0.293900" },
            { "align.radius.periodic.tickShift.value", "4899.379395" },
            { "align.radius.value.mobJitterScale", "0.825958" },
            { "align.radius.value.value", "1611.409424" },
            { "align.radius.valueType", "periodic" },
            { "align.rangeType", "strictOff" },
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
            { "attackSeparate.crowd.radius.periodic.amplitude.mobJitterScale", "-0.160626" },
            { "attackSeparate.crowd.radius.periodic.amplitude.value", "0.729000" },
            { "attackSeparate.crowd.radius.periodic.period.mobJitterScale", "0.807596" },
            { "attackSeparate.crowd.radius.periodic.period.value", "7794.914062" },
            { "attackSeparate.crowd.radius.periodic.tickShift.mobJitterScale", "0.412584" },
            { "attackSeparate.crowd.radius.periodic.tickShift.value", "2446.844727" },
            { "attackSeparate.crowd.radius.value.mobJitterScale", "0.359301" },
            { "attackSeparate.crowd.radius.value.value", "1107.389893" },
            { "attackSeparate.crowd.size.periodic.amplitude.mobJitterScale", "-0.397179" },
            { "attackSeparate.crowd.size.periodic.amplitude.value", "-0.240794" },
            { "attackSeparate.crowd.size.periodic.period.mobJitterScale", "0.759256" },
            { "attackSeparate.crowd.size.periodic.period.value", "7310.659668" },
            { "attackSeparate.crowd.size.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "attackSeparate.crowd.size.periodic.tickShift.value", "4993.658691" },
            { "attackSeparate.crowd.size.value.mobJitterScale", "-1.000000" },
            { "attackSeparate.crowd.size.value.value", "9.217925" },
            { "attackSeparate.crowd.size.valueType", "periodic" },
            { "attackSeparate.crowdType", "quadraticDown" },
            { "attackSeparate.radius.periodic.amplitude.mobJitterScale", "0.815203" },
            { "attackSeparate.radius.periodic.amplitude.value", "-0.235451" },
            { "attackSeparate.radius.periodic.period.mobJitterScale", "0.461347" },
            { "attackSeparate.radius.periodic.period.value", "7895.465820" },
            { "attackSeparate.radius.periodic.tickShift.mobJitterScale", "-0.061523" },
            { "attackSeparate.radius.periodic.tickShift.value", "9595.180664" },
            { "attackSeparate.radius.value.mobJitterScale", "0.358411" },
            { "attackSeparate.radius.value.value", "6.974665" },
            { "attackSeparate.radius.valueType", "periodic" },
            { "attackSeparate.rangeType", "quadraticDown" },
            { "attackSeparate.weight.periodic.amplitude.mobJitterScale", "-0.092663" },
            { "attackSeparate.weight.periodic.amplitude.value", "0.464557" },
            { "attackSeparate.weight.periodic.period.mobJitterScale", "0.742404" },
            { "attackSeparate.weight.periodic.period.value", "6052.318359" },
            { "attackSeparate.weight.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "attackSeparate.weight.periodic.tickShift.value", "-1.000000" },
            { "attackSeparate.weight.value.mobJitterScale", "-0.916499" },
            { "attackSeparate.weight.value.value", "-6.942165" },
            { "attackSeparate.weight.valueType", "periodic" },
            { "base.crowd.radius.periodic.amplitude.mobJitterScale", "0.629757" },
            { "base.crowd.radius.periodic.amplitude.value", "1.000000" },
            { "base.crowd.radius.periodic.period.mobJitterScale", "-0.954232" },
            { "base.crowd.radius.periodic.period.value", "1434.848267" },
            { "base.crowd.radius.periodic.tickShift.mobJitterScale", "0.872867" },
            { "base.crowd.radius.periodic.tickShift.value", "2878.810547" },
            { "base.crowd.radius.value.mobJitterScale", "0.350683" },
            { "base.crowd.radius.value.value", "544.842163" },
            { "base.crowd.radius.valueType", "periodic" },
            { "base.crowd.size.periodic.amplitude.mobJitterScale", "-0.760530" },
            { "base.crowd.size.periodic.amplitude.value", "-0.617358" },
            { "base.crowd.size.periodic.period.mobJitterScale", "0.500071" },
            { "base.crowd.size.periodic.period.value", "7564.628418" },
            { "base.crowd.size.periodic.tickShift.mobJitterScale", "0.365768" },
            { "base.crowd.size.periodic.tickShift.value", "1731.283569" },
            { "base.crowd.size.value.mobJitterScale", "0.567610" },
            { "base.crowd.size.value.value", "10.136998" },
            { "base.crowd.size.valueType", "periodic" },
            { "base.crowdType", "never" },
            { "base.radius.periodic.amplitude.mobJitterScale", "0.503678" },
            { "base.radius.periodic.amplitude.value", "0.357514" },
            { "base.radius.periodic.period.mobJitterScale", "-0.303256" },
            { "base.radius.periodic.period.value", "1714.841187" },
            { "base.radius.periodic.tickShift.mobJitterScale", "0.446971" },
            { "base.radius.periodic.tickShift.value", "7052.154297" },
            { "base.radius.value.mobJitterScale", "0.306412" },
            { "base.radius.value.value", "826.290833" },
            { "base.radius.valueType", "constant" },
            { "base.rangeType", "quadraticDown" },
            { "base.weight.periodic.amplitude.mobJitterScale", "0.195740" },
            { "base.weight.periodic.amplitude.value", "0.241564" },
            { "base.weight.periodic.period.mobJitterScale", "0.423984" },
            { "base.weight.periodic.period.value", "4275.365723" },
            { "base.weight.periodic.tickShift.mobJitterScale", "-0.344272" },
            { "base.weight.periodic.tickShift.value", "6403.570801" },
            { "base.weight.value.mobJitterScale", "-0.900000" },
            { "base.weight.value.value", "-0.552769" },
            { "base.weight.valueType", "constant" },
            { "baseDefense.crowd.radius.periodic.amplitude.mobJitterScale", "-0.006350" },
            { "baseDefense.crowd.radius.periodic.amplitude.value", "0.186222" },
            { "baseDefense.crowd.radius.periodic.period.mobJitterScale", "-0.055209" },
            { "baseDefense.crowd.radius.periodic.period.value", "3771.450928" },
            { "baseDefense.crowd.radius.periodic.tickShift.mobJitterScale", "0.399684" },
            { "baseDefense.crowd.radius.periodic.tickShift.value", "1177.760742" },
            { "baseDefense.crowd.radius.value.mobJitterScale", "-1.000000" },
            { "baseDefense.crowd.radius.value.value", "1064.811401" },
            { "baseDefense.crowd.radius.valueType", "constant" },
            { "baseDefense.crowd.size.periodic.amplitude.mobJitterScale", "-0.063311" },
            { "baseDefense.crowd.size.periodic.amplitude.value", "0.248441" },
            { "baseDefense.crowd.size.periodic.period.mobJitterScale", "0.288933" },
            { "baseDefense.crowd.size.periodic.period.value", "664.790039" },
            { "baseDefense.crowd.size.periodic.tickShift.mobJitterScale", "0.127526" },
            { "baseDefense.crowd.size.periodic.tickShift.value", "5024.023926" },
            { "baseDefense.crowd.size.value.mobJitterScale", "-0.313322" },
            { "baseDefense.crowd.size.value.value", "6.976077" },
            { "baseDefense.crowd.size.valueType", "periodic" },
            { "baseDefense.crowdType", "never" },
            { "baseDefense.radius.periodic.amplitude.mobJitterScale", "-0.900000" },
            { "baseDefense.radius.periodic.amplitude.value", "0.015727" },
            { "baseDefense.radius.periodic.period.mobJitterScale", "-0.568743" },
            { "baseDefense.radius.periodic.period.value", "2136.907227" },
            { "baseDefense.radius.periodic.tickShift.mobJitterScale", "0.459880" },
            { "baseDefense.radius.periodic.tickShift.value", "4744.493652" },
            { "baseDefense.radius.value.mobJitterScale", "0.010407" },
            { "baseDefense.radius.value.value", "1813.240723" },
            { "baseDefense.radius.valueType", "periodic" },
            { "baseDefense.rangeType", "strictOn" },
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
            { "center.crowd.radius.periodic.amplitude.mobJitterScale", "-0.129981" },
            { "center.crowd.radius.periodic.amplitude.value", "-0.808502" },
            { "center.crowd.radius.periodic.period.mobJitterScale", "0.522967" },
            { "center.crowd.radius.periodic.period.value", "4239.490723" },
            { "center.crowd.radius.periodic.tickShift.mobJitterScale", "0.221667" },
            { "center.crowd.radius.periodic.tickShift.value", "3245.848145" },
            { "center.crowd.radius.value.mobJitterScale", "-0.880619" },
            { "center.crowd.radius.value.value", "1296.293945" },
            { "center.crowd.radius.valueType", "periodic" },
            { "center.crowd.size.periodic.amplitude.mobJitterScale", "0.643359" },
            { "center.crowd.size.periodic.amplitude.value", "0.545312" },
            { "center.crowd.size.periodic.period.mobJitterScale", "-0.817902" },
            { "center.crowd.size.periodic.period.value", "4173.515625" },
            { "center.crowd.size.periodic.tickShift.mobJitterScale", "0.562874" },
            { "center.crowd.size.periodic.tickShift.value", "2118.594727" },
            { "center.crowd.size.value.mobJitterScale", "-0.254054" },
            { "center.crowd.size.value.value", "-1.000000" },
            { "center.crowd.size.valueType", "constant" },
            { "center.crowdType", "linearDown" },
            { "center.radius.periodic.amplitude.mobJitterScale", "-0.257824" },
            { "center.radius.periodic.amplitude.value", "-0.778535" },
            { "center.radius.periodic.period.mobJitterScale", "-0.261245" },
            { "center.radius.periodic.period.value", "1270.336792" },
            { "center.radius.periodic.tickShift.mobJitterScale", "0.225704" },
            { "center.radius.periodic.tickShift.value", "5248.995117" },
            { "center.radius.value.mobJitterScale", "0.178743" },
            { "center.radius.value.value", "1376.573120" },
            { "center.radius.valueType", "periodic" },
            { "center.rangeType", "always" },
            { "center.weight.periodic.amplitude.mobJitterScale", "-0.780084" },
            { "center.weight.periodic.amplitude.value", "-0.055858" },
            { "center.weight.periodic.period.mobJitterScale", "0.879264" },
            { "center.weight.periodic.period.value", "3026.286133" },
            { "center.weight.periodic.tickShift.mobJitterScale", "-0.689098" },
            { "center.weight.periodic.tickShift.value", "9663.837891" },
            { "center.weight.value.mobJitterScale", "0.081387" },
            { "center.weight.value.value", "-0.341581" },
            { "center.weight.valueType", "periodic" },
            { "cohere.crowd.radius.periodic.amplitude.mobJitterScale", "0.741092" },
            { "cohere.crowd.radius.periodic.amplitude.value", "-0.347129" },
            { "cohere.crowd.radius.periodic.period.mobJitterScale", "-0.413831" },
            { "cohere.crowd.radius.periodic.period.value", "6253.756836" },
            { "cohere.crowd.radius.periodic.tickShift.mobJitterScale", "0.539022" },
            { "cohere.crowd.radius.periodic.tickShift.value", "9135.519531" },
            { "cohere.crowd.radius.value.mobJitterScale", "-0.855556" },
            { "cohere.crowd.radius.value.value", "353.585327" },
            { "cohere.crowd.radius.valueType", "constant" },
            { "cohere.crowd.size.periodic.amplitude.mobJitterScale", "0.389762" },
            { "cohere.crowd.size.periodic.amplitude.value", "-0.936647" },
            { "cohere.crowd.size.periodic.period.mobJitterScale", "-0.794015" },
            { "cohere.crowd.size.periodic.period.value", "1550.949463" },
            { "cohere.crowd.size.periodic.tickShift.mobJitterScale", "0.264275" },
            { "cohere.crowd.size.periodic.tickShift.value", "196.567032" },
            { "cohere.crowd.size.value.mobJitterScale", "0.900000" },
            { "cohere.crowd.size.value.value", "7.042645" },
            { "cohere.crowd.size.valueType", "periodic" },
            { "cohere.crowdType", "always" },
            { "cohere.radius.periodic.amplitude.mobJitterScale", "0.510163" },
            { "cohere.radius.periodic.amplitude.value", "-0.344642" },
            { "cohere.radius.periodic.period.mobJitterScale", "-0.903820" },
            { "cohere.radius.periodic.period.value", "6761.200195" },
            { "cohere.radius.periodic.tickShift.mobJitterScale", "0.978119" },
            { "cohere.radius.periodic.tickShift.value", "5388.253418" },
            { "cohere.radius.value.mobJitterScale", "0.627164" },
            { "cohere.radius.value.value", "53.364258" },
            { "cohere.radius.valueType", "constant" },
            { "cohere.rangeType", "linearUp" },
            { "cohere.weight.periodic.amplitude.mobJitterScale", "0.380661" },
            { "cohere.weight.periodic.amplitude.value", "-0.967841" },
            { "cohere.weight.periodic.period.mobJitterScale", "0.396756" },
            { "cohere.weight.periodic.period.value", "10000.000000" },
            { "cohere.weight.periodic.tickShift.mobJitterScale", "0.577438" },
            { "cohere.weight.periodic.tickShift.value", "1060.135254" },
            { "cohere.weight.value.mobJitterScale", "-0.540023" },
            { "cohere.weight.value.value", "-4.392778" },
            { "cohere.weight.valueType", "constant" },
            { "cores.crowd.radius.periodic.amplitude.mobJitterScale", "0.054492" },
            { "cores.crowd.radius.periodic.amplitude.value", "-0.058022" },
            { "cores.crowd.radius.periodic.period.mobJitterScale", "-0.665730" },
            { "cores.crowd.radius.periodic.period.value", "1379.847412" },
            { "cores.crowd.radius.periodic.tickShift.mobJitterScale", "-0.088462" },
            { "cores.crowd.radius.periodic.tickShift.value", "3113.915283" },
            { "cores.crowd.radius.value.mobJitterScale", "0.709337" },
            { "cores.crowd.radius.value.value", "572.806946" },
            { "cores.crowd.radius.valueType", "periodic" },
            { "cores.crowd.size.periodic.amplitude.mobJitterScale", "-0.230521" },
            { "cores.crowd.size.periodic.amplitude.value", "-0.407307" },
            { "cores.crowd.size.periodic.period.mobJitterScale", "-0.318837" },
            { "cores.crowd.size.periodic.period.value", "1108.362915" },
            { "cores.crowd.size.periodic.tickShift.mobJitterScale", "-0.951218" },
            { "cores.crowd.size.periodic.tickShift.value", "1688.293213" },
            { "cores.crowd.size.value.mobJitterScale", "-0.103645" },
            { "cores.crowd.size.value.value", "-0.990000" },
            { "cores.crowd.size.valueType", "periodic" },
            { "cores.crowdType", "quadraticUp" },
            { "cores.radius.periodic.amplitude.mobJitterScale", "0.396617" },
            { "cores.radius.periodic.amplitude.value", "0.017514" },
            { "cores.radius.periodic.period.mobJitterScale", "0.659562" },
            { "cores.radius.periodic.period.value", "9954.907227" },
            { "cores.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "cores.radius.periodic.tickShift.value", "5256.877441" },
            { "cores.radius.value.mobJitterScale", "0.668274" },
            { "cores.radius.value.value", "1785.109131" },
            { "cores.radius.valueType", "periodic" },
            { "cores.rangeType", "never" },
            { "cores.weight.periodic.amplitude.mobJitterScale", "0.203537" },
            { "cores.weight.periodic.amplitude.value", "0.547758" },
            { "cores.weight.periodic.period.mobJitterScale", "1.000000" },
            { "cores.weight.periodic.period.value", "1121.036743" },
            { "cores.weight.periodic.tickShift.mobJitterScale", "-0.448087" },
            { "cores.weight.periodic.tickShift.value", "6111.345703" },
            { "cores.weight.value.mobJitterScale", "0.576346" },
            { "cores.weight.value.value", "-8.884531" },
            { "cores.weight.valueType", "constant" },
            { "corners.crowd.radius.periodic.amplitude.mobJitterScale", "-0.974665" },
            { "corners.crowd.radius.periodic.amplitude.value", "0.567519" },
            { "corners.crowd.radius.periodic.period.mobJitterScale", "-0.089344" },
            { "corners.crowd.radius.periodic.period.value", "2946.568848" },
            { "corners.crowd.radius.periodic.tickShift.mobJitterScale", "0.567529" },
            { "corners.crowd.radius.periodic.tickShift.value", "4511.764648" },
            { "corners.crowd.radius.value.mobJitterScale", "-0.794892" },
            { "corners.crowd.radius.value.value", "50.181087" },
            { "corners.crowd.radius.valueType", "periodic" },
            { "corners.crowd.size.periodic.amplitude.mobJitterScale", "-0.641893" },
            { "corners.crowd.size.periodic.amplitude.value", "-0.195778" },
            { "corners.crowd.size.periodic.period.mobJitterScale", "-0.413899" },
            { "corners.crowd.size.periodic.period.value", "8571.804688" },
            { "corners.crowd.size.periodic.tickShift.mobJitterScale", "0.223595" },
            { "corners.crowd.size.periodic.tickShift.value", "4534.453125" },
            { "corners.crowd.size.value.mobJitterScale", "-0.335554" },
            { "corners.crowd.size.value.value", "5.440167" },
            { "corners.crowd.size.valueType", "constant" },
            { "corners.crowdType", "linearDown" },
            { "corners.radius.periodic.amplitude.mobJitterScale", "-0.377146" },
            { "corners.radius.periodic.amplitude.value", "0.307509" },
            { "corners.radius.periodic.period.mobJitterScale", "-0.790672" },
            { "corners.radius.periodic.period.value", "10000.000000" },
            { "corners.radius.periodic.tickShift.mobJitterScale", "0.040308" },
            { "corners.radius.periodic.tickShift.value", "4148.615234" },
            { "corners.radius.value.mobJitterScale", "-0.129875" },
            { "corners.radius.value.value", "-0.900000" },
            { "corners.radius.valueType", "constant" },
            { "corners.rangeType", "strictOn" },
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
            { "edges.crowd.radius.periodic.amplitude.mobJitterScale", "0.428622" },
            { "edges.crowd.radius.periodic.amplitude.value", "-0.478741" },
            { "edges.crowd.radius.periodic.period.mobJitterScale", "-0.039347" },
            { "edges.crowd.radius.periodic.period.value", "-1.000000" },
            { "edges.crowd.radius.periodic.tickShift.mobJitterScale", "0.610549" },
            { "edges.crowd.radius.periodic.tickShift.value", "6925.696289" },
            { "edges.crowd.radius.value.mobJitterScale", "0.704016" },
            { "edges.crowd.radius.value.value", "1486.155273" },
            { "edges.crowd.radius.valueType", "constant" },
            { "edges.crowd.size.periodic.amplitude.mobJitterScale", "-0.369981" },
            { "edges.crowd.size.periodic.amplitude.value", "0.579032" },
            { "edges.crowd.size.periodic.period.mobJitterScale", "-1.000000" },
            { "edges.crowd.size.periodic.period.value", "7911.691406" },
            { "edges.crowd.size.periodic.tickShift.mobJitterScale", "-0.073305" },
            { "edges.crowd.size.periodic.tickShift.value", "7164.106934" },
            { "edges.crowd.size.value.mobJitterScale", "0.303231" },
            { "edges.crowd.size.value.value", "20.000000" },
            { "edges.crowd.size.valueType", "periodic" },
            { "edges.crowdType", "linearUp" },
            { "edges.radius.periodic.amplitude.mobJitterScale", "0.095815" },
            { "edges.radius.periodic.amplitude.value", "-0.524913" },
            { "edges.radius.periodic.period.mobJitterScale", "1.000000" },
            { "edges.radius.periodic.period.value", "2956.822510" },
            { "edges.radius.periodic.tickShift.mobJitterScale", "0.873347" },
            { "edges.radius.periodic.tickShift.value", "8249.994141" },
            { "edges.radius.value.mobJitterScale", "0.124869" },
            { "edges.radius.value.value", "1027.559937" },
            { "edges.radius.valueType", "periodic" },
            { "edges.rangeType", "strictOff" },
            { "edges.weight.periodic.amplitude.mobJitterScale", "0.086834" },
            { "edges.weight.periodic.amplitude.value", "0.069813" },
            { "edges.weight.periodic.period.mobJitterScale", "-0.516740" },
            { "edges.weight.periodic.period.value", "5186.431641" },
            { "edges.weight.periodic.tickShift.mobJitterScale", "0.307494" },
            { "edges.weight.periodic.tickShift.value", "6947.385742" },
            { "edges.weight.value.mobJitterScale", "-0.480400" },
            { "edges.weight.value.value", "1.973997" },
            { "edges.weight.valueType", "constant" },
            { "enemy.crowd.radius.periodic.amplitude.mobJitterScale", "0.318759" },
            { "enemy.crowd.radius.periodic.amplitude.value", "0.887125" },
            { "enemy.crowd.radius.periodic.period.mobJitterScale", "0.424449" },
            { "enemy.crowd.radius.periodic.period.value", "5951.244141" },
            { "enemy.crowd.radius.periodic.tickShift.mobJitterScale", "-1.000000" },
            { "enemy.crowd.radius.periodic.tickShift.value", "3979.839111" },
            { "enemy.crowd.radius.value.mobJitterScale", "0.714816" },
            { "enemy.crowd.radius.value.value", "1175.853882" },
            { "enemy.crowd.radius.valueType", "periodic" },
            { "enemy.crowd.size.periodic.amplitude.mobJitterScale", "0.105315" },
            { "enemy.crowd.size.periodic.amplitude.value", "0.711270" },
            { "enemy.crowd.size.periodic.period.mobJitterScale", "0.209904" },
            { "enemy.crowd.size.periodic.period.value", "4162.410645" },
            { "enemy.crowd.size.periodic.tickShift.mobJitterScale", "-0.297136" },
            { "enemy.crowd.size.periodic.tickShift.value", "2045.583984" },
            { "enemy.crowd.size.value.mobJitterScale", "0.244159" },
            { "enemy.crowd.size.value.value", "20.000000" },
            { "enemy.crowd.size.valueType", "constant" },
            { "enemy.crowdType", "strictOff" },
            { "enemy.radius.periodic.amplitude.mobJitterScale", "0.363027" },
            { "enemy.radius.periodic.amplitude.value", "-0.849631" },
            { "enemy.radius.periodic.period.mobJitterScale", "0.151267" },
            { "enemy.radius.periodic.period.value", "8259.858398" },
            { "enemy.radius.periodic.tickShift.mobJitterScale", "-0.251063" },
            { "enemy.radius.periodic.tickShift.value", "2326.809814" },
            { "enemy.radius.value.mobJitterScale", "1.000000" },
            { "enemy.radius.value.value", "675.515564" },
            { "enemy.radius.valueType", "periodic" },
            { "enemy.rangeType", "linearUp" },
            { "enemy.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "enemy.weight.periodic.amplitude.value", "-0.415855" },
            { "enemy.weight.periodic.period.mobJitterScale", "-0.958802" },
            { "enemy.weight.periodic.period.value", "4443.390137" },
            { "enemy.weight.periodic.tickShift.mobJitterScale", "-0.767862" },
            { "enemy.weight.periodic.tickShift.value", "1665.511475" },
            { "enemy.weight.value.mobJitterScale", "0.302705" },
            { "enemy.weight.value.value", "0.182987" },
            { "enemy.weight.valueType", "constant" },
            { "enemyBase.crowd.radius.periodic.amplitude.mobJitterScale", "-0.150879" },
            { "enemyBase.crowd.radius.periodic.amplitude.value", "0.459352" },
            { "enemyBase.crowd.radius.periodic.period.mobJitterScale", "-0.566621" },
            { "enemyBase.crowd.radius.periodic.period.value", "3987.979004" },
            { "enemyBase.crowd.radius.periodic.tickShift.mobJitterScale", "1.000000" },
            { "enemyBase.crowd.radius.periodic.tickShift.value", "4893.104004" },
            { "enemyBase.crowd.radius.value.mobJitterScale", "0.060784" },
            { "enemyBase.crowd.radius.value.value", "686.847290" },
            { "enemyBase.crowd.radius.valueType", "constant" },
            { "enemyBase.crowd.size.periodic.amplitude.mobJitterScale", "0.342603" },
            { "enemyBase.crowd.size.periodic.amplitude.value", "0.888685" },
            { "enemyBase.crowd.size.periodic.period.mobJitterScale", "-0.354940" },
            { "enemyBase.crowd.size.periodic.period.value", "837.139221" },
            { "enemyBase.crowd.size.periodic.tickShift.mobJitterScale", "-0.313340" },
            { "enemyBase.crowd.size.periodic.tickShift.value", "9557.248047" },
            { "enemyBase.crowd.size.value.mobJitterScale", "-0.603297" },
            { "enemyBase.crowd.size.value.value", "14.142944" },
            { "enemyBase.crowd.size.valueType", "periodic" },
            { "enemyBase.crowdType", "quadraticDown" },
            { "enemyBase.radius.periodic.amplitude.mobJitterScale", "0.734564" },
            { "enemyBase.radius.periodic.amplitude.value", "0.641733" },
            { "enemyBase.radius.periodic.period.mobJitterScale", "0.491955" },
            { "enemyBase.radius.periodic.period.value", "3028.051758" },
            { "enemyBase.radius.periodic.tickShift.mobJitterScale", "0.037870" },
            { "enemyBase.radius.periodic.tickShift.value", "-0.891000" },
            { "enemyBase.radius.value.mobJitterScale", "0.438102" },
            { "enemyBase.radius.value.value", "-0.990000" },
            { "enemyBase.radius.valueType", "constant" },
            { "enemyBase.rangeType", "linearUp" },
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
            { "fleetLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.054109" },
            { "fleetLocus.force.crowd.radius.periodic.amplitude.value", "-0.722894" },
            { "fleetLocus.force.crowd.radius.periodic.period.mobJitterScale", "0.852969" },
            { "fleetLocus.force.crowd.radius.periodic.period.value", "5040.565430" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.194057" },
            { "fleetLocus.force.crowd.radius.periodic.tickShift.value", "5826.963867" },
            { "fleetLocus.force.crowd.radius.value.mobJitterScale", "0.271841" },
            { "fleetLocus.force.crowd.radius.value.value", "1839.255859" },
            { "fleetLocus.force.crowd.radius.valueType", "constant" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "-0.260413" },
            { "fleetLocus.force.crowd.size.periodic.amplitude.value", "-0.890009" },
            { "fleetLocus.force.crowd.size.periodic.period.mobJitterScale", "-0.038311" },
            { "fleetLocus.force.crowd.size.periodic.period.value", "10000.000000" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "0.706192" },
            { "fleetLocus.force.crowd.size.periodic.tickShift.value", "3292.306885" },
            { "fleetLocus.force.crowd.size.value.mobJitterScale", "-0.105208" },
            { "fleetLocus.force.crowd.size.value.value", "7.386487" },
            { "fleetLocus.force.crowd.size.valueType", "constant" },
            { "fleetLocus.force.crowdType", "linearUp" },
            { "fleetLocus.force.radius.periodic.amplitude.mobJitterScale", "-0.737166" },
            { "fleetLocus.force.radius.periodic.amplitude.value", "-0.517972" },
            { "fleetLocus.force.radius.periodic.period.mobJitterScale", "-0.032016" },
            { "fleetLocus.force.radius.periodic.period.value", "4870.444336" },
            { "fleetLocus.force.radius.periodic.tickShift.mobJitterScale", "-0.097485" },
            { "fleetLocus.force.radius.periodic.tickShift.value", "3433.859375" },
            { "fleetLocus.force.radius.value.mobJitterScale", "0.046068" },
            { "fleetLocus.force.radius.value.value", "329.599731" },
            { "fleetLocus.force.radius.valueType", "periodic" },
            { "fleetLocus.force.rangeType", "quadraticDown" },
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
            { "mobLocus.force.crowd.radius.periodic.amplitude.mobJitterScale", "-0.998171" },
            { "mobLocus.force.crowd.radius.periodic.amplitude.value", "0.621639" },
            { "mobLocus.force.crowd.radius.periodic.period.mobJitterScale", "0.090579" },
            { "mobLocus.force.crowd.radius.periodic.period.value", "7565.159180" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.mobJitterScale", "0.235655" },
            { "mobLocus.force.crowd.radius.periodic.tickShift.value", "5741.665039" },
            { "mobLocus.force.crowd.radius.value.mobJitterScale", "-0.684285" },
            { "mobLocus.force.crowd.radius.value.value", "1372.306519" },
            { "mobLocus.force.crowd.radius.valueType", "periodic" },
            { "mobLocus.force.crowd.size.periodic.amplitude.mobJitterScale", "0.256010" },
            { "mobLocus.force.crowd.size.periodic.amplitude.value", "-1.000000" },
            { "mobLocus.force.crowd.size.periodic.period.mobJitterScale", "0.685738" },
            { "mobLocus.force.crowd.size.periodic.period.value", "5590.333008" },
            { "mobLocus.force.crowd.size.periodic.tickShift.mobJitterScale", "-0.063424" },
            { "mobLocus.force.crowd.size.periodic.tickShift.value", "3684.881104" },
            { "mobLocus.force.crowd.size.value.mobJitterScale", "-0.733576" },
            { "mobLocus.force.crowd.size.value.value", "14.643238" },
            { "mobLocus.force.crowd.size.valueType", "periodic" },
            { "mobLocus.force.crowdType", "strictOff" },
            { "mobLocus.force.radius.periodic.amplitude.mobJitterScale", "0.795150" },
            { "mobLocus.force.radius.periodic.amplitude.value", "0.817033" },
            { "mobLocus.force.radius.periodic.period.mobJitterScale", "0.898221" },
            { "mobLocus.force.radius.periodic.period.value", "9658.197266" },
            { "mobLocus.force.radius.periodic.tickShift.mobJitterScale", "-0.621063" },
            { "mobLocus.force.radius.periodic.tickShift.value", "6856.098145" },
            { "mobLocus.force.radius.value.mobJitterScale", "-0.117397" },
            { "mobLocus.force.radius.value.value", "2000.000000" },
            { "mobLocus.force.radius.valueType", "periodic" },
            { "mobLocus.force.rangeType", "linearUp" },
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
            { "nearBaseRandomIdle", "FALSE" },
            { "nearestFriend.crowd.radius.periodic.amplitude.mobJitterScale", "0.656393" },
            { "nearestFriend.crowd.radius.periodic.amplitude.value", "0.213289" },
            { "nearestFriend.crowd.radius.periodic.period.mobJitterScale", "0.877674" },
            { "nearestFriend.crowd.radius.periodic.period.value", "2933.867432" },
            { "nearestFriend.crowd.radius.periodic.tickShift.mobJitterScale", "0.725152" },
            { "nearestFriend.crowd.radius.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.crowd.radius.value.mobJitterScale", "0.478979" },
            { "nearestFriend.crowd.radius.value.value", "813.628235" },
            { "nearestFriend.crowd.radius.valueType", "periodic" },
            { "nearestFriend.crowd.size.periodic.amplitude.mobJitterScale", "-1.000000" },
            { "nearestFriend.crowd.size.periodic.amplitude.value", "-0.082733" },
            { "nearestFriend.crowd.size.periodic.period.mobJitterScale", "-0.325498" },
            { "nearestFriend.crowd.size.periodic.period.value", "6398.181641" },
            { "nearestFriend.crowd.size.periodic.tickShift.mobJitterScale", "0.885306" },
            { "nearestFriend.crowd.size.periodic.tickShift.value", "9000.000000" },
            { "nearestFriend.crowd.size.value.mobJitterScale", "-0.938299" },
            { "nearestFriend.crowd.size.value.value", "15.717419" },
            { "nearestFriend.crowd.size.valueType", "periodic" },
            { "nearestFriend.crowdType", "strictOn" },
            { "nearestFriend.radius.periodic.amplitude.mobJitterScale", "0.510415" },
            { "nearestFriend.radius.periodic.amplitude.value", "0.701764" },
            { "nearestFriend.radius.periodic.period.mobJitterScale", "0.578944" },
            { "nearestFriend.radius.periodic.period.value", "4864.889160" },
            { "nearestFriend.radius.periodic.tickShift.mobJitterScale", "-0.034630" },
            { "nearestFriend.radius.periodic.tickShift.value", "4568.888672" },
            { "nearestFriend.radius.value.mobJitterScale", "0.635803" },
            { "nearestFriend.radius.value.value", "1145.889038" },
            { "nearestFriend.radius.valueType", "periodic" },
            { "nearestFriend.rangeType", "strictOff" },
            { "nearestFriend.weight.periodic.amplitude.mobJitterScale", "1.000000" },
            { "nearestFriend.weight.periodic.amplitude.value", "-0.753387" },
            { "nearestFriend.weight.periodic.period.mobJitterScale", "0.917711" },
            { "nearestFriend.weight.periodic.period.value", "793.138672" },
            { "nearestFriend.weight.periodic.tickShift.mobJitterScale", "-0.133992" },
            { "nearestFriend.weight.periodic.tickShift.value", "10000.000000" },
            { "nearestFriend.weight.value.mobJitterScale", "0.783617" },
            { "nearestFriend.weight.value.value", "0.058244" },
            { "nearestFriend.weight.valueType", "periodic" },
            { "randomIdle", "TRUE" },
            { "randomizeStoppedVelocity", "TRUE" },
            { "rotateStartingAngle", "FALSE" },
            { "sensorGrid.staleCoreTime", "41.629436" },
            { "sensorGrid.staleFighterTime", "0.950000" },
            { "separate.crowd.radius.periodic.amplitude.mobJitterScale", "0.345688" },
            { "separate.crowd.radius.periodic.amplitude.value", "-0.522793" },
            { "separate.crowd.radius.periodic.period.mobJitterScale", "0.338378" },
            { "separate.crowd.radius.periodic.period.value", "10000.000000" },
            { "separate.crowd.radius.periodic.tickShift.mobJitterScale", "-0.192211" },
            { "separate.crowd.radius.periodic.tickShift.value", "8513.941406" },
            { "separate.crowd.radius.value.mobJitterScale", "-0.775946" },
            { "separate.crowd.radius.value.value", "965.350220" },
            { "separate.crowd.radius.valueType", "periodic" },
            { "separate.crowd.size.periodic.amplitude.mobJitterScale", "0.468782" },
            { "separate.crowd.size.periodic.amplitude.value", "-0.408151" },
            { "separate.crowd.size.periodic.period.mobJitterScale", "0.134387" },
            { "separate.crowd.size.periodic.period.value", "3136.231934" },
            { "separate.crowd.size.periodic.tickShift.mobJitterScale", "0.889874" },
            { "separate.crowd.size.periodic.tickShift.value", "-1.000000" },
            { "separate.crowd.size.value.mobJitterScale", "-0.003657" },
            { "separate.crowd.size.value.value", "9.810118" },
            { "separate.crowd.size.valueType", "constant" },
            { "separate.crowdType", "never" },
            { "separate.radius.periodic.amplitude.mobJitterScale", "0.875430" },
            { "separate.radius.periodic.amplitude.value", "0.112632" },
            { "separate.radius.periodic.period.mobJitterScale", "-0.289590" },
            { "separate.radius.periodic.period.value", "5220.742676" },
            { "separate.radius.periodic.tickShift.mobJitterScale", "-0.714845" },
            { "separate.radius.periodic.tickShift.value", "10000.000000" },
            { "separate.radius.value.mobJitterScale", "0.361008" },
            { "separate.radius.value.value", "1016.923523" },
            { "separate.radius.valueType", "periodic" },
            { "separate.rangeType", "quadraticDown" },
            { "separate.weight.periodic.amplitude.mobJitterScale", "-0.748777" },
            { "separate.weight.periodic.amplitude.value", "-0.464512" },
            { "separate.weight.periodic.period.mobJitterScale", "0.029400" },
            { "separate.weight.periodic.period.value", "6154.719727" },
            { "separate.weight.periodic.tickShift.mobJitterScale", "-0.633351" },
            { "separate.weight.periodic.tickShift.value", "7947.900391" },
            { "separate.weight.value.mobJitterScale", "0.277628" },
            { "separate.weight.value.value", "-1.060454" },
            { "separate.weight.valueType", "constant" },
            { "simpleAttack", "TRUE" },
            { "startingMaxRadius", "1994.999878" },
            { "startingMinRadius", "358.747559" },
        };

        struct {
            BundleConfigValue *values;
            uint numValues;
        } configs[] = {
            { defaults, ARRAYSIZE(defaults), },
            { configs1, ARRAYSIZE(configs1), },
            { configs2, ARRAYSIZE(configs2), },
            { configs3, ARRAYSIZE(configs3), },
            { configs4, ARRAYSIZE(configs4), },
        };

        int bundleIndex = aiType - FLEET_AI_BUNDLE1 + 1;
        VERIFY(aiType >= FLEET_AI_BUNDLE1);
        VERIFY(aiType <= FLEET_AI_BUNDLE4);
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
        this->myConfig.randomIdle = MBRegistry_GetBool(mreg, "randomIdle");
        this->myConfig.nearBaseRandomIdle =
            MBRegistry_GetBool(mreg, "nearBaseRandomIdle");
        this->myConfig.randomizeStoppedVelocity =
            MBRegistry_GetBool(mreg, "randomizeStoppedVelocity");
        this->myConfig.simpleAttack = MBRegistry_GetBool(mreg, "simpleAttack");

        loadBundleForce(mreg, &this->myConfig.align, "align");
        loadBundleForce(mreg, &this->myConfig.cohere, "cohere");
        loadBundleForce(mreg, &this->myConfig.separate, "separate");
        loadBundleForce(mreg, &this->myConfig.attackSeparate, "attackSeparate");
        loadBundleForce(mreg, &this->myConfig.nearestFriend, "nearestFriend");

        loadBundleForce(mreg, &this->myConfig.cores, "cores");
        loadBundleForce(mreg, &this->myConfig.enemy, "enemy");
        loadBundleForce(mreg, &this->myConfig.enemyBase, "enemyBase");

        loadBundleForce(mreg, &this->myConfig.center, "center");
        loadBundleForce(mreg, &this->myConfig.edges, "edges");
        loadBundleForce(mreg, &this->myConfig.corners, "corners");
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

    void flockSeparate(Mob *mob, FRPoint *rForce, BundleForce *bundle) {
        ASSERT(mob->type == MOB_TYPE_FIGHTER);
        SensorGrid *sg = mySensorGrid;
        float cweight;

        if (!crowdCheck(mob, bundle, &cweight)) {
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
        SensorGrid *sg = mySensorGrid;
        Mob *m;
        int n = 0;
        float cweight;

        if (!crowdCheck(mob, &myConfig.nearestFriend, &cweight)) {
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

        if (!crowdCheck(mob, bundle, &cweight)) {
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

        if (!crowdCheck(mob, bundle, &cweight)) {
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

    float getMobJitter(Mob *m, float *value) {
        RandomState *rs = &myRandomState;

        BundleShipAI *ship = (BundleShipAI *)getShip(m->mobid);
        ASSERT(ship != NULL);

        if (*value <= 0.0f) {
            return 0.0f;
        }

        int index = getMobJitterIndex(value);
        if (!ship->myShipLive.mobJitters[index].initialized) {
            ship->myShipLive.mobJitters[index].value =
                RandomState_Float(rs, -*value, *value);
        }

        return ship->myShipLive.mobJitters[index].value;
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

    float getCrowdCount(Mob *mob, float crowdRadius) {
        FleetAI *ai = myFleetAI;
        SensorGrid *sg = mySensorGrid;
        uint tick = ai->tick;

        /*
         * Reuse the last value if this is a repeat call.
         */
        if (myCache.crowdCache.mobid == mob->mobid &&
            myCache.crowdCache.tick == tick &&
            myCache.crowdCache.radius == crowdRadius) {
            return myCache.crowdCache.count;
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
    bool crowdCheck(Mob *mob, BundleForce *bundle, float *weight) {
        float crowdTrigger = 0.0f;
        float crowdRadius = 0.0f;
        float crowdValue = 0.0f;

        /*
         * Skip the complicated calculations if the bundle-check was constant.
         */
        if (!isConstantBundleCheck(bundle->crowdCheck)) {
            crowdTrigger = getBundleValue(mob, &bundle->crowd.size);
            crowdRadius = getBundleValue(mob, &bundle->crowd.radius);
            getCrowdCount(mob, crowdRadius);
        }

        return bundleCheck(bundle->crowdCheck, crowdValue, crowdTrigger,
                           weight);
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

        BasicAIGovernor::doAttack(mob, enemyTarget);

        if (myConfig.simpleAttack) {
            return;
        }

        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
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

        if (newlyIdle && myConfig.randomIdle) {
            mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
            mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
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

            if (myConfig.randomizeStoppedVelocity &&
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
            flockCores(mob, &rForce);
            flockFleetLocus(mob, &rForce);
            flockMobLocus(mob, &rForce);

            if (myConfig.randomizeStoppedVelocity &&
                rForce.radius < MICRON) {
                rForce.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
            }

            rForce.radius = speed;

            FRPoint_ToFPoint(&rForce, &mob->pos, &mob->cmd.target);
            ASSERT(!isnanf(mob->cmd.target.x));
            ASSERT(!isnanf(mob->cmd.target.y));
        } else if (newlyIdle) {
            if (myConfig.nearBaseRandomIdle) {
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
            MBVector<Mob *> fv;
            MBVector<Mob *> tv;
            int f = 0;
            int t = 0;

            sg->pushFriends(fv, MOB_FLAG_FIGHTER);
            sg->pushClosestTargetsInRange(tv, MOB_FLAG_SHIP, &base->pos,
                                          myConfig.baseDefenseRadius);

            CMBComparator comp;
            MobP_InitDistanceComparator(&comp, &base->pos);
            fv.sort(MBComparator<Mob *>(&comp));

            Mob *fighter = (f < fv.size()) ? fv[f++] : NULL;
            Mob *target = (t < tv.size()) ? tv[t++] : NULL;

            while (target != NULL && fighter != NULL) {
                BasicShipAI *ship = (BasicShipAI *)getShip(fighter->mobid);

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
    } else if (aiType == FLEET_AI_BUNDLE2) {
        ops->aiName = "BundleFleet2";
    } else if (aiType == FLEET_AI_BUNDLE3) {
        ops->aiName = "BundleFleet3";
    } else if (aiType == FLEET_AI_BUNDLE4) {
        ops->aiName = "BundleFleet4";
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
        { "baseDefenseRadius",     1.0f,   500.0f,  0.05f, 0.15f, 0.01f},

        { "sensorGrid.staleCoreTime",
                                   0.0f,   50.0f,   0.05f, 0.2f, 0.005f},
        { "sensorGrid.staleFighterTime",
                                   0.0f,   20.0f,   0.05f, 0.2f, 0.005f},
        { "creditReserve",       100.0f,  200.0f,   0.05f, 0.1f, 0.005f},
    };

    MutationBoolParams vb[] = {
        // key                       mutation
        { "evadeFighters",            0.05f },
        { "evadeUseStrictDistance",   0.05f },
        { "attackExtendedRange",      0.05f },
        { "rotateStartingAngle",      0.05f },
        { "gatherAbandonStale",       0.05f },
        { "randomIdle",               0.05f },
        { "nearBaseRandomIdle",       0.005f},
        { "randomizeStoppedVelocity", 0.05f },
        { "simpleAttack",             0.05f },
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
    MutateBundleForce(aiType, mreg, "nearestFriend");

    MutateBundleForce(aiType, mreg, "cores");
    MutateBundleForce(aiType, mreg, "enemy");
    MutateBundleForce(aiType, mreg, "enemyBase");

    MutateBundleForce(aiType, mreg, "center");
    MutateBundleForce(aiType, mreg, "edges");
    MutateBundleForce(aiType, mreg, "corners");
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
