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

        struct {
            BundleConfigValue *values;
            uint numValues;
        } configs[] = {
            { defaults, ARRAYSIZE(defaults), },
            { configs1, ARRAYSIZE(configs1), },
            { configs2, ARRAYSIZE(configs2), },
        };

        int bundleIndex = aiType - FLEET_AI_BUNDLE1 + 1;
        VERIFY(aiType >= FLEET_AI_BUNDLE1);
        VERIFY(aiType <= FLEET_AI_BUNDLE2);
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
