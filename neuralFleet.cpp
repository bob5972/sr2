/*
 * neuralFleet.cpp -- part of SpaceRobots2
 * Copyright (C) 2022 Michael Banack <github@banack.net>
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
#include "ml.hpp"
#include "floatNet.hpp"
#include "textDump.hpp"

#define NEURAL_MAX_NODE_DEGREE  8
#define NEURAL_MAX_INPUTS      25
#define NEURAL_MAX_OUTPUTS     25
#define NEURAL_MAX_NODES       100

#define NEURAL_SCRAMBLE_KEY "neuralFleet.scrambleMutation"

typedef enum NeuralForceType {
    NEURAL_FORCE_ZERO,
    NEURAL_FORCE_HEADING,
    NEURAL_FORCE_ALIGN,
    NEURAL_FORCE_COHERE,
    NEURAL_FORCE_SEPARATE,
    NEURAL_FORCE_NEAREST_FRIEND,
    NEURAL_FORCE_NEAREST_FRIEND_MISSILE,
    NEURAL_FORCE_EDGES,
    NEURAL_FORCE_CORNERS,
    NEURAL_FORCE_CENTER,
    NEURAL_FORCE_BASE,
    NEURAL_FORCE_BASE_DEFENSE,
    NEURAL_FORCE_ENEMY,
    NEURAL_FORCE_ENEMY_MISSILE,
    NEURAL_FORCE_ENEMY_BASE,
    NEURAL_FORCE_ENEMY_BASE_GUESS,
    NEURAL_FORCE_CORES,

    NEURAL_FORCE_MAX,
} NeuralForceType;

static TextMapEntry tmForces[] = {
    { TMENTRY(NEURAL_FORCE_ZERO),                     },
    { TMENTRY(NEURAL_FORCE_HEADING),                  },
    { TMENTRY(NEURAL_FORCE_ALIGN),                    },
    { TMENTRY(NEURAL_FORCE_COHERE),                   },
    { TMENTRY(NEURAL_FORCE_SEPARATE),                 },
    { TMENTRY(NEURAL_FORCE_NEAREST_FRIEND),           },
    { TMENTRY(NEURAL_FORCE_NEAREST_FRIEND_MISSILE),   },
    { TMENTRY(NEURAL_FORCE_EDGES),                    },
    { TMENTRY(NEURAL_FORCE_CORNERS),                  },
    { TMENTRY(NEURAL_FORCE_CENTER),                   },
    { TMENTRY(NEURAL_FORCE_BASE),                     },
    { TMENTRY(NEURAL_FORCE_BASE_DEFENSE),             },
    { TMENTRY(NEURAL_FORCE_ENEMY),                    },
    { TMENTRY(NEURAL_FORCE_ENEMY_MISSILE),            },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE),               },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS),         },
    { TMENTRY(NEURAL_FORCE_CORES),                    },
};

typedef struct NeuralForceDesc {
    NeuralForceType forceType;
    bool useTangent;
    float radius;
} NeuralForceDesc;

typedef enum NeuralCrowdType {
    NEURAL_CROWD_FRIEND_FIGHTER,
    NEURAL_CROWD_FRIEND_MISSILE,
    NEURAL_CROWD_ENEMY_SHIP,
    NEURAL_CROWD_ENEMY_MISSILE,
    NEURAL_CROWD_CORES,
} NeuralCrowdType;

static TextMapEntry tmCrowds[] = {
    { TMENTRY(NEURAL_CROWD_FRIEND_FIGHTER),        },
    { TMENTRY(NEURAL_CROWD_FRIEND_MISSILE),        },
    { TMENTRY(NEURAL_CROWD_ENEMY_SHIP),            },
    { TMENTRY(NEURAL_CROWD_ENEMY_MISSILE),         },
    { TMENTRY(NEURAL_CROWD_CORES),                 },
};

typedef struct NeuralCrowdDesc {
    NeuralCrowdType crowdType;
    float radius;
} NeuralCrowdDesc;

typedef enum NeuralValueType {
    NEURAL_VALUE_ZERO,
    NEURAL_VALUE_FORCE,
    NEURAL_VALUE_RANGE,
    NEURAL_VALUE_CROWD,
    NEURAL_VALUE_TICK,
    NEURAL_VALUE_MOBID,
    NEURAL_VALUE_RANDOM_UNIT,
    NEURAL_VALUE_CREDITS,
    NEURAL_VALUE_FRIEND_SHIPS,
    NEURAL_VALUE_MAX,
} NeuralValueType;

static TextMapEntry tmValues[] = {
    { TMENTRY(NEURAL_VALUE_ZERO),  },
    { TMENTRY(NEURAL_VALUE_FORCE), },
    { TMENTRY(NEURAL_VALUE_RANGE), },
    { TMENTRY(NEURAL_VALUE_CROWD), },
    { TMENTRY(NEURAL_VALUE_TICK),  },
    { TMENTRY(NEURAL_VALUE_MOBID), },
    { TMENTRY(NEURAL_VALUE_RANDOM_UNIT), },
    { TMENTRY(NEURAL_VALUE_CREDITS), },
    { TMENTRY(NEURAL_VALUE_FRIEND_SHIPS), },
};

typedef struct NeuralValueDesc {
    NeuralValueType valueType;
    union {
        NeuralForceDesc forceDesc;
        NeuralForceDesc rangeDesc;
        NeuralCrowdDesc crowdDesc;
    };
} NeuralValueDesc;

typedef struct NeuralConfigValue {
    const char *key;
    const char *value;
} NeuralConfigValue;

static void LoadNeuralValueDesc(MBRegistry *mreg,
                                NeuralValueDesc *desc, const char *prefix);
static void LoadNeuralForceDesc(MBRegistry *mreg,
                                NeuralForceDesc *desc, const char *prefix);
static void LoadNeuralCrowdDesc(MBRegistry *mreg,
                                NeuralCrowdDesc *desc, const char *prefix);
static void MutateNeuralValueDesc(MBRegistry *mreg, NeuralValueDesc *desc,
                                  bool isOutput, float rate,
                                  const char *prefix);

class NeuralAIGovernor : public BasicAIGovernor
{
private:
    // Members
    FloatNet myNeuralNet;
    MBVector<NeuralValueDesc> myInputDescs;
    MBVector<NeuralValueDesc> myOutputDescs;
    MBVector<float> myInputs;
    MBVector<float> myOutputs;
    uint myNumNodes;

public:
    NeuralAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    { }

    virtual ~NeuralAIGovernor() { }

    class NeuralShipAI : public BasicShipAI
    {
        public:
        NeuralShipAI(MobID mobid, NeuralAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        { }

        virtual ~NeuralShipAI() {}
    };

    virtual NeuralShipAI *newShip(MobID mobid) {
        return new NeuralShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        NeuralConfigValue defaults[] = {
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

        NeuralConfigValue configs1[] = {
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "148.066803" },
            { "creditReserve", "0.000000" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "186.610748" },
            { "evadeStrictDistance", "137.479996" },
            { "evadeUseStrictDistance", "FALSE" },
            { "floatNet.node[20].inputs", "{13, 10, 7, 10, 13, 0, 9, 9, }" },
            { "floatNet.node[20].op", "ML_FOP_1x1_STRICT_OFF" },
            { "floatNet.node[20].params", "{1.050000, 0.167232, 9.493267, 0.000000, 247.688858, 9.984236, 1.000000, 1.000000, }" },
            { "floatNet.node[30].inputs", "{23, 14, 29, 15, 28, 6, 16, 1, }" },
            { "floatNet.node[30].op", "ML_FOP_1x2_SINE" },
            { "floatNet.node[30].params", "{1.000000, 0.940282, 1094.104126, 0.808425, 1060.474121, 10.000000, 0.950000, 1.000000, }" },
            { "floatNet.node[31].inputs", "{28, 8, 26, 16, 23, 2, 20, 30, }" },
            { "floatNet.node[31].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[31].params", "{0.900000, 227.969955, 1291.191284, 1.050000, 0.950000, 0.343430, 0.941271, 1.050000, }" },
            { "floatNet.node[32].inputs", "{3, 25, 12, 12, 16, 23, 5, 4, }" },
            { "floatNet.node[32].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "floatNet.node[32].params", "{981.047180, 1.000000, 1.000000, 0.570663, 0.371041, 0.511033, 1.000000, 0.783566, }" },
            { "floatNet.node[33].inputs", "{13, 20, 16, 28, 24, 19, 26, 26, }" },
            { "floatNet.node[33].op", "ML_FOP_1x0_NEGATE" },
            { "floatNet.node[33].params", "{0.729000, 0.778732, 1.000000, 1.000000, 990.312561, 0.449541, 0.367953, 3000.000000, }" },
            { "floatNet.node[34].inputs", "{13, 30, 25, 14, 20, 24, 19, 0, }" },
            { "floatNet.node[34].op", "ML_FOP_1x1_FMOD" },
            { "floatNet.node[34].params", "{0.950000, 0.182962, 1.000000, 0.950000, 0.482863, 0.018417, 10.000000, 600.039978, }" },
            { "floatNet.node[35].inputs", "{10, 11, 30, 13, 28, 17, 21, 19, }" },
            { "floatNet.node[35].op", "ML_FOP_1x0_INVERSE" },
            { "floatNet.node[35].params", "{0.763107, 1.557925, 1.000000, 1.000000, 0.900000, 0.818662, 20.037409, 1.504580, }" },
            { "floatNet.node[36].inputs", "{30, 14, 13, 12, 17, 32, 12, 3, }" },
            { "floatNet.node[36].op", "ML_FOP_1x1_LINEAR_UP" },
            { "floatNet.node[36].params", "{1.212750, 0.434509, 1.000000, 0.487149, 1.000000, 10.000000, 7.966716, 1.000000, }" },
            { "floatNet.node[37].inputs", "{22, 32, 36, 15, 20, 24, 30, 28, }" },
            { "floatNet.node[37].op", "ML_FOP_NxN_LINEAR_COMBINATION" },
            { "floatNet.node[37].params", "{0.262960, 1.000000, 1.000000, 1423.661255, 0.950000, 1.000000, 0.733408, 1.050000, }" },
            { "floatNet.node[38].inputs", "{20, 3, 22, 15, 29, 16, 33, 4, }" },
            { "floatNet.node[38].op", "ML_FOP_1x1_LINEAR_UP" },
            { "floatNet.node[38].params", "{1.000000, 1.000000, 0.761416, 0.283246, 1.000000, 3005.644287, 10.000000, 1.050000, }" },
            { "floatNet.node[39].inputs", "{32, 13, 27, 14, 35, 12, 32, 17, }" },
            { "floatNet.node[39].op", "ML_FOP_1x1_STRICT_OFF" },
            { "floatNet.node[39].params", "{0.550351, 0.694501, 0.793074, 0.382523, 0.714323, 0.997500, 1.969741, 0.146499, }" },
            { "floatNet.node[21].inputs", "{0, 1, 14, 7, 1, 13, 19, 1, }" },
            { "floatNet.node[21].op", "ML_FOP_NxN_LINEAR_COMBINATION" },
            { "floatNet.node[21].params", "{0.076838, 1.642655, 0.814816, 3.706571, 0.816026, 1.000000, 7.485513, 10.000000, }" },
            { "floatNet.node[22].inputs", "{18, 20, 8, 9, 12, 4, 2, 16, }" },
            { "floatNet.node[22].op", "ML_FOP_NxN_SCALED_MAX" },
            { "floatNet.node[22].params", "{1.000000, 1.000000, 0.850500, 2054.450195, 1381.724854, 157.438812, 1.050000, 184.877609, }" },
            { "floatNet.node[23].inputs", "{3, 18, 3, 0, 6, 5, 13, 11, }" },
            { "floatNet.node[23].op", "ML_FOP_NxN_SCALED_MIN" },
            { "floatNet.node[23].params", "{9.176544, 240.673782, 0.706809, 1.050000, 0.763494, -1.000000, 1.298177, 1.000000, }" },
            { "floatNet.node[24].inputs", "{10, 14, 17, 16, 7, 19, 23, 20, }" },
            { "floatNet.node[24].op", "ML_FOP_NxN_LINEAR_COMBINATION" },
            { "floatNet.node[24].params", "{-1.000000, 10.000000, 0.850500, 0.129628, 1.984586, 3.957525, 6644.178223, 0.594585, }" },
            { "floatNet.node[25].inputs", "{14, 17, 11, 15, 24, 9, 18, 21, }" },
            { "floatNet.node[25].op", "ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT" },
            { "floatNet.node[25].params", "{0.000000, 0.549446, 0.990000, 0.080289, 0.522399, 3467.296143, 0.800638, 1.000000, }" },
            { "floatNet.node[26].inputs", "{7, 25, 10, 0, 5, 1, 14, 7, }" },
            { "floatNet.node[26].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[26].params", "{0.950000, 0.871663, 152.331863, 9.500000, 1.000000, -1.000000, 0.257858, 1.000000, }" },
            { "floatNet.node[27].inputs", "{14, 21, 12, 23, 16, 14, 10, 22, }" },
            { "floatNet.node[27].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[27].params", "{0.538959, 0.695539, 0.330331, 149.907135, 1.050000, 0.211076, 0.557956, 0.465225, }" },
            { "floatNet.node[28].inputs", "{15, 22, 6, 1, 24, 4, 8, 0, }" },
            { "floatNet.node[28].op", "ML_FOP_1x2_SINE" },
            { "floatNet.node[28].params", "{1.050000, 1.000000, 8.814820, 0.313138, 1.000000, 10.500000, 0.120743, 0.950000, }" },
            { "floatNet.node[29].inputs", "{6, 22, 8, 13, 8, 20, 6, 9, }" },
            { "floatNet.node[29].op", "ML_FOP_1x2_CLAMP" },
            { "floatNet.node[29].params", "{239.458893, 0.147459, 1.000000, 0.723297, 1.050000, 0.184670, 997.662720, 27.000000, }" },
            { "floatNet.numInputs", "20" },
            { "floatNet.numNodes", "20" },
            { "floatNet.numOutputs", "20" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "67.846550" },
            { "guardRange", "200.824371" },
            { "input[0].forceType", "NEURAL_FORCE_EDGES" },
            { "input[0].radius", "1379.394165" },
            { "input[0].useTangent", "TRUE" },
            { "input[0].valueType", "NEURAL_VALUE_CROWD" },
            { "input[10].forceType", "NEURAL_FORCE_ALIGN" },
            { "input[10].radius", "26.197947" },
            { "input[10].useTangent", "TRUE" },
            { "input[10].valueType", "NEURAL_VALUE_CROWD" },
            { "input[11].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS" },
            { "input[11].radius", "1410.684570" },
            { "input[11].useTangent", "FALSE" },
            { "input[11].valueType", "NEURAL_VALUE_RANGE" },
            { "input[12].forceType", "NEURAL_FORCE_SEPARATE" },
            { "input[12].radius", "821.751221" },
            { "input[12].useTangent", "FALSE" },
            { "input[12].valueType", "NEURAL_VALUE_TICK" },
            { "input[13].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "input[13].radius", "3000.000000" },
            { "input[13].useTangent", "FALSE" },
            { "input[13].valueType", "NEURAL_VALUE_MOBID" },
            { "input[14].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "input[14].radius", "2820.190674" },
            { "input[14].useTangent", "FALSE" },
            { "input[14].valueType", "NEURAL_VALUE_CROWD" },
            { "input[15].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "input[15].radius", "152.445862" },
            { "input[15].useTangent", "TRUE" },
            { "input[15].valueType", "NEURAL_VALUE_FORCE" },
            { "input[16].forceType", "NEURAL_FORCE_ZERO" },
            { "input[16].radius", "1642.232056" },
            { "input[16].useTangent", "FALSE" },
            { "input[16].valueType", "NEURAL_VALUE_RANGE" },
            { "input[17].forceType", "NEURAL_FORCE_EDGES" },
            { "input[17].radius", "938.246948" },
            { "input[17].useTangent", "TRUE" },
            { "input[17].valueType", "NEURAL_VALUE_RANGE" },
            { "input[18].forceType", "NEURAL_FORCE_SEPARATE" },
            { "input[18].radius", "1578.296631" },
            { "input[18].useTangent", "FALSE" },
            { "input[18].valueType", "NEURAL_VALUE_RANGE" },
            { "input[19].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "input[19].radius", "2423.319824" },
            { "input[19].useTangent", "TRUE" },
            { "input[19].valueType", "NEURAL_VALUE_RANGE" },
            { "input[1].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "input[1].radius", "2067.721191" },
            { "input[1].useTangent", "FALSE" },
            { "input[1].valueType", "NEURAL_VALUE_FORCE" },
            { "input[2].forceType", "NEURAL_FORCE_HEADING" },
            { "input[2].radius", "1371.876465" },
            { "input[2].useTangent", "TRUE" },
            { "input[2].valueType", "NEURAL_VALUE_ZERO" },
            { "input[3].forceType", "NEURAL_FORCE_ALIGN" },
            { "input[3].radius", "1263.489990" },
            { "input[3].useTangent", "FALSE" },
            { "input[3].valueType", "NEURAL_VALUE_CROWD" },
            { "input[4].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "input[4].radius", "2304.703857" },
            { "input[4].useTangent", "TRUE" },
            { "input[4].valueType", "NEURAL_VALUE_MOBID" },
            { "input[5].forceType", "NEURAL_FORCE_SEPARATE" },
            { "input[5].radius", "1718.594727" },
            { "input[5].useTangent", "TRUE" },
            { "input[5].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[6].forceType", "NEURAL_FORCE_ENEMY" },
            { "input[6].radius", "2545.418213" },
            { "input[6].useTangent", "TRUE" },
            { "input[6].valueType", "NEURAL_VALUE_MOBID" },
            { "input[7].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "input[7].radius", "2474.785889" },
            { "input[7].useTangent", "FALSE" },
            { "input[7].valueType", "NEURAL_VALUE_RANGE" },
            { "input[8].forceType", "NEURAL_FORCE_CENTER" },
            { "input[8].radius", "2051.518799" },
            { "input[8].useTangent", "FALSE" },
            { "input[8].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[9].forceType", "NEURAL_FORCE_EDGES" },
            { "input[9].radius", "1191.592407" },
            { "input[9].useTangent", "FALSE" },
            { "input[9].valueType", "NEURAL_VALUE_MOBID" },
            { "output[20].forceType", "NEURAL_FORCE_ENEMY" },
            { "output[20].radius", "2464.956787" },
            { "output[20].useTangent", "FALSE" },
            { "output[20].valueType", "NEURAL_VALUE_FORCE" },
            { "output[30].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[30].radius", "2050.290771" },
            { "output[30].useTangent", "TRUE" },
            { "output[30].valueType", "NEURAL_VALUE_FORCE" },
            { "output[31].forceType", "NEURAL_FORCE_ZERO" },
            { "output[31].radius", "602.595520" },
            { "output[31].useTangent", "TRUE" },
            { "output[31].valueType", "NEURAL_VALUE_FORCE" },
            { "output[32].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "output[32].radius", "1519.014038" },
            { "output[32].useTangent", "FALSE" },
            { "output[32].valueType", "NEURAL_VALUE_FORCE" },
            { "output[33].forceType", "NEURAL_FORCE_COHERE" },
            { "output[33].radius", "3000.000000" },
            { "output[33].useTangent", "TRUE" },
            { "output[33].valueType", "NEURAL_VALUE_FORCE" },
            { "output[34].forceType", "NEURAL_FORCE_ENEMY" },
            { "output[34].radius", "2529.224365" },
            { "output[34].useTangent", "FALSE" },
            { "output[34].valueType", "NEURAL_VALUE_FORCE" },
            { "output[35].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "output[35].radius", "211.022003" },
            { "output[35].useTangent", "TRUE" },
            { "output[35].valueType", "NEURAL_VALUE_FORCE" },
            { "output[36].forceType", "NEURAL_FORCE_EDGES" },
            { "output[36].radius", "1125.137329" },
            { "output[36].useTangent", "TRUE" },
            { "output[36].valueType", "NEURAL_VALUE_FORCE" },
            { "output[37].forceType", "NEURAL_FORCE_HEADING" },
            { "output[37].radius", "1372.211548" },
            { "output[37].useTangent", "FALSE" },
            { "output[37].valueType", "NEURAL_VALUE_FORCE" },
            { "output[38].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "output[38].radius", "812.498596" },
            { "output[38].useTangent", "FALSE" },
            { "output[38].valueType", "NEURAL_VALUE_FORCE" },
            { "output[39].forceType", "NEURAL_FORCE_CORNERS" },
            { "output[39].radius", "2725.875488" },
            { "output[39].useTangent", "FALSE" },
            { "output[39].valueType", "NEURAL_VALUE_FORCE" },
            { "output[21].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[21].radius", "1413.617065" },
            { "output[21].useTangent", "TRUE" },
            { "output[21].valueType", "NEURAL_VALUE_FORCE" },
            { "output[22].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS" },
            { "output[22].radius", "2569.132568" },
            { "output[22].useTangent", "TRUE" },
            { "output[22].valueType", "NEURAL_VALUE_FORCE" },
            { "output[23].forceType", "NEURAL_FORCE_HEADING" },
            { "output[23].radius", "1969.989868" },
            { "output[23].useTangent", "TRUE" },
            { "output[23].valueType", "NEURAL_VALUE_FORCE" },
            { "output[24].forceType", "NEURAL_FORCE_ENEMY" },
            { "output[24].radius", "2281.656494" },
            { "output[24].useTangent", "TRUE" },
            { "output[24].valueType", "NEURAL_VALUE_FORCE" },
            { "output[25].forceType", "NEURAL_FORCE_CORES" },
            { "output[25].radius", "2072.078369" },
            { "output[25].useTangent", "TRUE" },
            { "output[25].valueType", "NEURAL_VALUE_FORCE" },
            { "output[26].forceType", "NEURAL_FORCE_ALIGN" },
            { "output[26].radius", "1009.818115" },
            { "output[26].useTangent", "FALSE" },
            { "output[26].valueType", "NEURAL_VALUE_FORCE" },
            { "output[27].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "output[27].radius", "1700.157471" },
            { "output[27].useTangent", "TRUE" },
            { "output[27].valueType", "NEURAL_VALUE_FORCE" },
            { "output[28].forceType", "NEURAL_FORCE_CENTER" },
            { "output[28].radius", "1027.284424" },
            { "output[28].useTangent", "TRUE" },
            { "output[28].valueType", "NEURAL_VALUE_FORCE" },
            { "output[29].forceType", "NEURAL_FORCE_COHERE" },
            { "output[29].radius", "1428.817993" },
            { "output[29].useTangent", "TRUE" },
            { "output[29].valueType", "NEURAL_VALUE_FORCE" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "0.000000" },
            { "sensorGrid.staleFighterTime", "0.000000" },
            { "startingMaxRadius", "1888.944092" },
            { "startingMinRadius", "460.407959" },
        };

        struct {
            NeuralConfigValue *values;
            uint numValues;
        } configs[] = {
            { defaults,  ARRAYSIZE(defaults), },
            { configs1,  ARRAYSIZE(configs1), },
        };

        int neuralIndex = aiType - FLEET_AI_NEURAL1 + 1;
        VERIFY(aiType >= FLEET_AI_NEURAL1);
        VERIFY(aiType <= FLEET_AI_NEURAL1);
        VERIFY(neuralIndex >= 1 && neuralIndex < ARRAYSIZE(configs));

        for (int i = neuralIndex; i >= 0; i--) {
            NeuralConfigValue *curConfig = configs[i].values;
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

    virtual void loadRegistry(MBRegistry *mreg) {
        if (MBRegistry_ContainsKey(mreg, "floatNet.numInputs")) {
            myNeuralNet.load(mreg, "floatNet.");
        } else {
            myNeuralNet.initialize(1, 1, 1);
            myNeuralNet.loadZeroNet();
        }

        uint numInputs = myNeuralNet.getNumInputs();
        uint numOutputs = myNeuralNet.getNumOutputs();
        myNumNodes = myNeuralNet.getNumNodes();

        myInputs.resize(numInputs);
        myOutputs.resize(numOutputs);

        myInputDescs.resize(numInputs);
        myOutputDescs.resize(numOutputs);

        for (uint i = 0; i < myOutputDescs.size(); i++) {
            char *str = NULL;
            int ret = asprintf(&str, "output[%d].",
                               i + myNeuralNet.getOutputOffset());
            VERIFY(ret > 0);
            LoadNeuralValueDesc(mreg, &myOutputDescs[i], str);
            free(str);

            if (myOutputDescs[i].valueType != NEURAL_VALUE_FORCE) {
                myOutputDescs[i].valueType = NEURAL_VALUE_FORCE;
                myOutputDescs[i].forceDesc.forceType = NEURAL_FORCE_ZERO;
                myOutputDescs[i].forceDesc.useTangent = FALSE;
                myOutputDescs[i].forceDesc.radius = 0.0f;
            }

            if (myOutputDescs[i].forceDesc.forceType == NEURAL_FORCE_ZERO) {
                myNeuralNet.zeroOutputNode(i);
            }
        }

        CPBitVector inputBV;
        inputBV.resize(numInputs);
        myNeuralNet.minimize(&inputBV);

        for (uint i = 0; i < myInputDescs.size(); i++) {
            if (inputBV.get(i)) {
                char *str = NULL;
                int ret = asprintf(&str, "input[%d].", i);
                VERIFY(ret > 0);
                LoadNeuralValueDesc(mreg, &myInputDescs[i], str);
                free(str);
            } else {
                myInputDescs[i].valueType = NEURAL_VALUE_ZERO;
            }
        }

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    float getNeuralValue(Mob *mob, NeuralValueDesc *desc) {
        FRPoint force;
        RandomState *rs = &myRandomState;
        FleetAI *ai = myFleetAI;
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;

        FRPoint_Zero(&force);

        ASSERT(desc != NULL);
        switch (desc->valueType) {
            case NEURAL_VALUE_ZERO:
                return 0.0f;
            case NEURAL_VALUE_FORCE:
                getNeuralForce(mob, &desc->forceDesc, &force);
                return force.radius;
            case NEURAL_VALUE_RANGE:
                return getRangeValue(mob, &desc->rangeDesc);
            case NEURAL_VALUE_CROWD:
                return getCrowdValue(mob, &desc->crowdDesc);
            case NEURAL_VALUE_TICK:
                return (float)myFleetAI->tick;
            case NEURAL_VALUE_MOBID: {
                RandomState lr;
                RandomState_CreateWithSeed(&lr, mob->mobid);
                return RandomState_UnitFloat(&lr);
            }
            case NEURAL_VALUE_RANDOM_UNIT:
                return RandomState_UnitFloat(rs);
            case NEURAL_VALUE_CREDITS:
                return (float)ai->credits;
            case NEURAL_VALUE_FRIEND_SHIPS:
                return (float)sg->numFriends(MOB_FLAG_SHIP);

            default:
                NOT_IMPLEMENTED();
        }
    }

    /*
     * getNeuralForce --
     *    Calculate the specified force.
     *    returns TRUE iff the force is valid.
     */
    bool getNeuralForce(Mob *mob,
                        NeuralForceDesc *desc,
                        FRPoint *rForce) {
        FPoint focusPoint;

        if (getNeuralFocus(mob, desc, &focusPoint)) {
            FPoint_ToFRPoint(&focusPoint, &mob->pos, rForce);
            rForce->radius = 1.0f;

            if (desc->useTangent) {
                rForce->theta += (float)M_PI/2;
            }
            return TRUE;
        } else {
            FRPoint_Zero(rForce);
            return FALSE;
        }
    }

    bool focusMobPosHelper(Mob *mob, FPoint *focusPoint)
    {
        ASSERT(focusPoint != NULL);
        if (mob != NULL) {
            *focusPoint = mob->pos;
            return TRUE;
        }
        return FALSE;
    }

    /*
     * getNeuralFocus --
     *     Get the focus point associated with the specified force.
     *     Returns TRUE if the force is valid.
     *     Returns FALSE if the force is invalid.
     */
    bool getNeuralFocus(Mob *mob,
                        NeuralForceDesc *desc,
                        FPoint *focusPoint) {
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        RandomState *rs = &myRandomState;

        switch(desc->forceType) {
            case NEURAL_FORCE_ZERO:
                return FALSE;

            case NEURAL_FORCE_HEADING: {
                FRPoint rPos;
                FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

                if (rPos.radius < MICRON) {
                    rPos.radius = 1.0f;
                    rPos.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
                }
                FRPoint_ToFPoint(&rPos, &mob->pos, focusPoint);
                return TRUE;
            }
            case NEURAL_FORCE_ALIGN: {
                FPoint avgVel;
                sg->friendAvgVelocity(&avgVel, &mob->pos, desc->radius,
                                      MOB_FLAG_FIGHTER);
                avgVel.x += mob->pos.x;
                avgVel.y += mob->pos.y;
                *focusPoint = avgVel;
                return TRUE;
            }
            case NEURAL_FORCE_COHERE: {
                FPoint avgPos;
                sg->friendAvgPos(&avgPos, &mob->pos, desc->radius,
                                 MOB_FLAG_FIGHTER);
                *focusPoint = avgPos;
                return TRUE;
            }
            case NEURAL_FORCE_SEPARATE:
                return getSeparateFocus(mob, desc, focusPoint);

            case NEURAL_FORCE_NEAREST_FRIEND: {
                Mob *m = sg->findClosestFriend(mob, MOB_FLAG_FIGHTER);
                return focusMobPosHelper(m, focusPoint);
            }
            case NEURAL_FORCE_NEAREST_FRIEND_MISSILE: {
                Mob *m = sg->findClosestFriend(mob, MOB_FLAG_MISSILE);
                return focusMobPosHelper(m, focusPoint);
            }

            case NEURAL_FORCE_EDGES: {
                getEdgeFocus(mob, desc, focusPoint);
                return TRUE;
            }
            case NEURAL_FORCE_CORNERS: {
                flockCorners(mob, desc, focusPoint);
                return TRUE;
            }
            case NEURAL_FORCE_CENTER: {
                focusPoint->x = myFleetAI->bp.width / 2;
                focusPoint->y = myFleetAI->bp.height / 2;
                return TRUE;
            }
            case NEURAL_FORCE_BASE:
                return focusMobPosHelper(sg->friendBase(), focusPoint);

            case NEURAL_FORCE_BASE_DEFENSE: {
                Mob *base = sg->friendBase();
                if (base != NULL) {
                    Mob *enemy = sg->findClosestTarget(&base->pos, MOB_FLAG_SHIP);
                    if (enemy != NULL) {
                        *focusPoint = enemy->pos;
                        return TRUE;
                    }
                }
                return FALSE;
            }
            case NEURAL_FORCE_ENEMY: {
                Mob *m = sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);
                return focusMobPosHelper(m, focusPoint);
            }
            case NEURAL_FORCE_ENEMY_MISSILE: {
                Mob *m = sg->findClosestTarget(&mob->pos, MOB_FLAG_MISSILE);
                return focusMobPosHelper(m, focusPoint);
            }

            case NEURAL_FORCE_ENEMY_BASE:
                return focusMobPosHelper(sg->enemyBase(), focusPoint);

            case NEURAL_FORCE_ENEMY_BASE_GUESS: {
                if (!sg->hasEnemyBase() && sg->hasEnemyBaseGuess()) {
                    *focusPoint = sg->getEnemyBaseGuess();
                    return TRUE;
                }
                return FALSE;
            }

            case NEURAL_FORCE_CORES: {
                Mob *m = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);
                return focusMobPosHelper(m, focusPoint);
            }

            default:
                PANIC("%s: Unhandled forceType: %d\n", __FUNCTION__,
                      desc->forceType);
        }

        NOT_REACHED();
    }

    bool getSeparateFocus(Mob *self, NeuralForceDesc *desc,
                          FPoint *focusPoint) {
        FRPoint force;
        int x = 0;
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);

        ASSERT(self->type == MOB_TYPE_FIGHTER);

        FRPoint_Zero(&force);

        while (mit.hasNext()) {
            Mob *m = mit.next();
            ASSERT(m != NULL);

            if (m->mobid != self->mobid) {
                repulseFocus(&self->pos, &m->pos, &force);
                x++;
            }
        }

        FRPoint_ToFPoint(&force, &self->pos, focusPoint);
        return x > 0;
    }

    void repulseFocus(const FPoint *selfPos, const FPoint *pos, FRPoint *force) {
        FRPoint f;
        RandomState *rs = &myRandomState;

        FPoint_ToFRPoint(pos, selfPos, &f);

        /*
         * Avoid 1/0 => NAN, and then randomize the direction when
         * the point is more or less directly on top of us.
         */
        if (f.radius < MICRON) {
            f.radius = MICRON;
            f.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
        }

        f.radius = 1.0f / (f.radius * f.radius);
        FRPoint_Add(force, &f, force);
    }

    void getEdgeFocus(Mob *self, NeuralForceDesc *desc, FPoint *focusPoint) {
        FleetAI *ai = myFleetAI;
        FPoint edgePoint;
        FRPoint force;

        FRPoint_Zero(&force);

        /*
         * Left Edge
         */
        edgePoint = self->pos;
        edgePoint.x = 0.0f;
        repulseFocus(&self->pos, &edgePoint, &force);

        /*
         * Right Edge
         */
        edgePoint = self->pos;
        edgePoint.x = ai->bp.width;
        repulseFocus(&self->pos, &edgePoint, &force);

        /*
         * Top Edge
         */
        edgePoint = self->pos;
        edgePoint.y = 0.0f;
        repulseFocus(&self->pos, &edgePoint, &force);

        /*
         * Bottom edge
         */
        edgePoint = self->pos;
        edgePoint.y = ai->bp.height;
        repulseFocus(&self->pos, &edgePoint, &force);

        FRPoint_ToFPoint(&force, &self->pos, focusPoint);
    }

    void flockCorners(Mob *self, NeuralForceDesc *desc, FPoint *focusPoint) {
        FleetAI *ai = myFleetAI;
        FPoint cornerPoint;
        FRPoint force;

        FRPoint_Zero(&force);

        cornerPoint.x = 0.0f;
        cornerPoint.y = 0.0f;
        repulseFocus(&self->pos, &cornerPoint, &force);

        cornerPoint.x = ai->bp.width;
        cornerPoint.y = 0.0f;
        repulseFocus(&self->pos, &cornerPoint, &force);

        cornerPoint.x = 0.0f;
        cornerPoint.y = ai->bp.height;
        repulseFocus(&self->pos, &cornerPoint, &force);

        cornerPoint.x = ai->bp.width;
        cornerPoint.y = ai->bp.height;
        repulseFocus(&self->pos, &cornerPoint, &force);

        FRPoint_ToFPoint(&force, &self->pos, focusPoint);
    }

    float getCrowdValue(Mob *mob, NeuralCrowdDesc *desc) {
        //XXX cache?
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        if (desc->crowdType == NEURAL_CROWD_FRIEND_FIGHTER) {
            return sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                         &mob->pos, desc->radius);
        } else if (desc->crowdType == NEURAL_CROWD_ENEMY_SHIP) {
            return sg->numTargetsInRange(MOB_FLAG_SHIP,
                                         &mob->pos, desc->radius);
        } else if (desc->crowdType == NEURAL_CROWD_CORES) {
            return sg->numTargetsInRange(MOB_FLAG_POWER_CORE,
                                         &mob->pos, desc->radius);
        } else if  (desc->crowdType == NEURAL_CROWD_FRIEND_MISSILE) {
            return sg->numFriendsInRange(MOB_FLAG_MISSILE,
                                         &mob->pos, desc->radius);
        } else {
            return sg->numTargetsInRange(MOB_FLAG_MISSILE,
                                         &mob->pos, desc->radius);
        }
    }

    float getRangeValue(Mob *mob, NeuralForceDesc *desc) {
        FPoint focusPoint;
        getNeuralFocus(mob, desc, &focusPoint);
        return FPoint_Distance(&mob->pos, &focusPoint);
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {

        BasicAIGovernor::doAttack(mob, enemyTarget);
    }

    void doForces(Mob *mob, FRPoint *outputForce) {
        uint x;
        float maxV = (1.0f / MICRON);

        ASSERT(myInputs.size() == myInputDescs.size());

        for (uint i = 0; i < myInputDescs.size(); i++) {
            myInputs[i] = getNeuralValue(mob, &myInputDescs[i]);
        }

        ASSERT(myOutputs.size() == myOutputDescs.size());
        myNeuralNet.compute(myInputs, myOutputs);

        for (uint i = 0; i < myOutputs.size(); i++) {
            if (isnan(myOutputs[i])) {
                myOutputs[i] = 0.0f;
            } else if (myOutputs[i] > maxV) {
                myOutputs[i] = maxV;
            } else if (myOutputs[i] < -maxV) {
                myOutputs[i] = -maxV;
            }
        }

        x = 0;
        FRPoint_Zero(outputForce);
        for (uint i = 0; i < myOutputDescs.size(); i++) {
            FRPoint force;
            ASSERT(myOutputDescs[i].valueType == NEURAL_VALUE_FORCE);
            if (myOutputDescs[i].forceDesc.forceType != NEURAL_FORCE_ZERO &&
                myOutputs[x] != 0.0f &&
                getNeuralForce(mob, &myOutputDescs[i].forceDesc,
                               &force)) {
                force.radius = myOutputs[x];
                FRPoint_Add(&force, outputForce, outputForce);
            }
            x++;
        }
        //XXX non-force outputs?
        ASSERT(x <= myOutputs.size());
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        //RandomState *rs = &myRandomState;
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);

        NeuralShipAI *ship = (NeuralShipAI *)mob->aiMobHandle;
        ASSERT(ship == (NeuralShipAI *)getShip(mob->mobid));
        ASSERT(ship != NULL);

        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        FRPoint rForce;
        doForces(mob, &rForce);
        if (rForce.radius < MICRON) {
            /*
             * Continue on the current heading if we didn't get a strong-enough
             * force.
             */
            NeuralForceDesc desc;
            desc.forceType = NEURAL_FORCE_HEADING;
            desc.useTangent = FALSE;
            desc.radius = speed;
            getNeuralForce(mob, &desc, &rForce);
        }

        rForce.radius = speed;
        FRPoint_ToFPoint(&rForce, &mob->pos, &mob->cmd.target);

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }

    virtual void runTick() {
        BasicAIGovernor::runTick();
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
    }
};

class NeuralFleet {
public:
    NeuralFleet(FleetAI *ai)
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

    ~NeuralFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    MappingSensorGrid sg;
    NeuralAIGovernor gov;
    MBRegistry *mreg;
};

static void *NeuralFleetCreate(FleetAI *ai);
static void NeuralFleetDestroy(void *aiHandle);
static void NeuralFleetRunAITick(void *aiHandle);
static void *NeuralFleetMobSpawned(void *aiHandle, Mob *m);
static void NeuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static void NeuralFleetMutate(FleetAIType aiType, MBRegistry *mreg);

void NeuralFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_NEURAL1) {
        ops->aiName = "NeuralFleet1";
    } else {
        NOT_IMPLEMENTED();
    }

    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &NeuralFleetCreate;
    ops->destroyFleet = &NeuralFleetDestroy;
    ops->runAITick = &NeuralFleetRunAITick;
    ops->mobSpawned = &NeuralFleetMobSpawned;
    ops->mobDestroyed = &NeuralFleetMobDestroyed;
    ops->mutateParams = &NeuralFleetMutate;
}

static void NeuralFleetMutate(FleetAIType aiType, MBRegistry *mreg)
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
    };

    float rate = 0.10;
    MBRegistry_PutCopy(mreg, NEURAL_SCRAMBLE_KEY, "FALSE");
    if (Random_Flip(0.01)) {
        MBRegistry_PutCopy(mreg, NEURAL_SCRAMBLE_KEY, "TRUE");

        for (uint i = 0; i < ARRAYSIZE(vf); i++) {
            vf[i].mutationRate = 1.0f;
            vf[i].jumpRate = 1.0f;
        }
        for (uint i = 0; i < ARRAYSIZE(vb); i++) {
            vb[i].flipRate = 0.5f;
        }
        rate = 1.0f;
    }

    FloatNet fn;
    if (MBRegistry_ContainsKey(mreg, "floatNet.numInputs") &&
        MBRegistry_GetUint(mreg, "floatNet.numInputs") > 0 &&
        !MBRegistry_GetBool(mreg, NEURAL_SCRAMBLE_KEY)) {
        fn.load(mreg, "floatNet.");
    } else {
        fn.initialize(NEURAL_MAX_INPUTS, NEURAL_MAX_OUTPUTS, NEURAL_MAX_NODES);
        fn.loadZeroNet();
    }

    fn.mutate(rate, NEURAL_MAX_NODE_DEGREE, NEURAL_MAX_NODES);
    fn.save(mreg, "floatNet.");

    for (uint i = 0; i < fn.getNumInputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "input[%d].", i);
        VERIFY(ret > 0);
        LoadNeuralValueDesc(mreg, &desc, str);
        MutateNeuralValueDesc(mreg, &desc, FALSE, rate, str);
        free(str);
    }

    for (uint i = 0; i < fn.getNumOutputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "output[%d].", i + fn.getOutputOffset());
        VERIFY(ret > 0);
        LoadNeuralValueDesc(mreg, &desc, str);
        MutateNeuralValueDesc(mreg, &desc, TRUE, rate, str);
        free(str);
    }

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));

    MBRegistry_Remove(mreg, NEURAL_SCRAMBLE_KEY);
}

static void MutateNeuralValueDesc(MBRegistry *mreg, NeuralValueDesc *desc,
                                  bool isOutput, float rate,
                                  const char *prefix)
{
    MBString s;

    s = prefix;
    s += "valueType";

    if (isOutput) {
        desc->valueType = NEURAL_VALUE_FORCE;
    } else if (Random_Flip(rate)) {
        uint i = Random_Int(0, ARRAYSIZE(tmValues) - 1);
        desc->valueType = (NeuralValueType) tmValues[i].value;
    }
    const char *v = TextMap_ToString(desc->valueType, tmValues, ARRAYSIZE(tmValues));
    MBRegistry_PutCopy(mreg, s.CStr(), v);

    if (desc->valueType == NEURAL_VALUE_FORCE ||
        desc->valueType == NEURAL_VALUE_RANGE ||
        desc->valueType == NEURAL_VALUE_CROWD) {
        MutationFloatParams vf;

        Mutate_DefaultFloatParams(&vf, MUTATION_TYPE_RADIUS);
        s = prefix;
        s += "radius";
        vf.key = s.CStr();
        Mutate_Float(mreg, &vf, 1);
    }

    if (desc->valueType == NEURAL_VALUE_CROWD) {
        s = prefix;
        s += "crowdType";
        if (Random_Flip(rate)) {
            uint i = Random_Int(0, ARRAYSIZE(tmCrowds) - 1);
            const char *v = tmCrowds[i].str;
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            desc->crowdDesc.crowdType = (NeuralCrowdType) tmCrowds[i].value;
        }
    }

    if (desc->valueType == NEURAL_VALUE_FORCE) {
        s = prefix;
        s += "forceType";
        if (Random_Flip(rate)) {
            uint i = Random_Int(0, ARRAYSIZE(tmForces) - 1);
            const char *v = tmForces[i].str;
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            desc->forceDesc.forceType = (NeuralForceType) tmForces[i].value;
        }

        MutationBoolParams bf;
        s = prefix;
        s += "useTangent";
        bf.key = s.CStr();
        bf.flipRate = rate;
        Mutate_Bool(mreg, &bf, 1);
    }
}


static void LoadNeuralValueDesc(MBRegistry *mreg,
                                NeuralValueDesc *desc, const char *prefix)
{
    MBString s;
    const char *cstr;

    s = prefix;
    s += "valueType";
    cstr = MBRegistry_GetCStr(mreg, s.CStr());
    desc->valueType = NEURAL_VALUE_MAX;

    if (cstr == NULL) {
        ASSERT(tmValues[0].value == NEURAL_VALUE_ZERO);
        cstr = tmValues[0].str;
    }

    desc->valueType = (NeuralValueType)
        TextMap_FromString(cstr, tmValues, ARRAYSIZE(tmValues));
    VERIFY(desc->valueType < NEURAL_VALUE_MAX);

    s = prefix;
    switch (desc->valueType) {
        case NEURAL_VALUE_FORCE:
        case NEURAL_VALUE_RANGE:
            ASSERT(&desc->forceDesc == &desc->rangeDesc);
            ASSERT(sizeof(desc->forceDesc) == sizeof(desc->rangeDesc));
            LoadNeuralForceDesc(mreg, &desc->forceDesc, s.CStr());
            break;

        case NEURAL_VALUE_CROWD:
            LoadNeuralCrowdDesc(mreg, &desc->crowdDesc, s.CStr());
            break;

        case NEURAL_VALUE_ZERO:
        case NEURAL_VALUE_TICK:
        case NEURAL_VALUE_MOBID:
        case NEURAL_VALUE_RANDOM_UNIT:
        case NEURAL_VALUE_CREDITS:
        case NEURAL_VALUE_FRIEND_SHIPS:
            break;

        default:
            NOT_IMPLEMENTED();
    }
}

static void LoadNeuralForceDesc(MBRegistry *mreg,
                                NeuralForceDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "forceType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        ASSERT(tmForces[0].value == NEURAL_FORCE_ZERO);
        v = tmForces[0].str;
    }
    desc->forceType = (NeuralForceType)
        TextMap_FromString(v, tmForces, ARRAYSIZE(tmForces));

    s = prefix;
    s += "useTangent";
    desc->useTangent = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());
}

static void LoadNeuralCrowdDesc(MBRegistry *mreg,
                                NeuralCrowdDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());

    s = prefix;
    s += "crowdType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        ASSERT(tmCrowds[0].value == NEURAL_CROWD_FRIEND_FIGHTER);
        v = tmCrowds[0].str;
    }
    desc->crowdType = (NeuralCrowdType)
        TextMap_FromString(v, tmCrowds, ARRAYSIZE(tmCrowds));
}

static void *NeuralFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new NeuralFleet(ai);
}

static void NeuralFleetDestroy(void *handle)
{
    NeuralFleet *sf = (NeuralFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *NeuralFleetMobSpawned(void *aiHandle, Mob *m)
{
    NeuralFleet *sf = (NeuralFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return sf->gov.getShipHandle(m->mobid);
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void NeuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    NeuralFleet *sf = (NeuralFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void NeuralFleetRunAITick(void *aiHandle)
{
    NeuralFleet *sf = (NeuralFleet *)aiHandle;
    sf->gov.runTick();
}
