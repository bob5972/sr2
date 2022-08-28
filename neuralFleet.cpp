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
    NEURAL_CROWD_BASE_ENEMY_SHIP,
    NEURAL_CROWD_BASE_FRIEND_SHIP,
} NeuralCrowdType;

static TextMapEntry tmCrowds[] = {
    { TMENTRY(NEURAL_CROWD_FRIEND_FIGHTER),        },
    { TMENTRY(NEURAL_CROWD_FRIEND_MISSILE),        },
    { TMENTRY(NEURAL_CROWD_ENEMY_SHIP),            },
    { TMENTRY(NEURAL_CROWD_ENEMY_MISSILE),         },
    { TMENTRY(NEURAL_CROWD_CORES),                 },
    { TMENTRY(NEURAL_CROWD_BASE_ENEMY_SHIP),       },
    { TMENTRY(NEURAL_CROWD_BASE_FRIEND_SHIP),       },
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

        NeuralConfigValue configs2[] = {
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "147.381485" },
            { "creditReserve", "0.000000" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "121.319000" },
            { "evadeStrictDistance", "149.183762" },
            { "evadeUseStrictDistance", "TRUE" },
            { "floatNet.node[100].inputs", "{}" },
            { "floatNet.node[100].mutationRate", "0.522987" },
            { "floatNet.node[100].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[100].params", "{2541.818359, 1.000000, 0.000000, 0.023998, }" },
            { "floatNet.node[101].inputs", "{57, 0, 14, 0, }" },
            { "floatNet.node[101].mutationRate", "0.468421" },
            { "floatNet.node[101].op", "ML_FOP_NxN_SCALED_MAX" },
            { "floatNet.node[101].params", "{0.000000, 0.000000, }" },
            { "floatNet.node[102].inputs", "{92, 94, 70, 0, }" },
            { "floatNet.node[102].mutationRate", "0.156351" },
            { "floatNet.node[102].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "floatNet.node[102].params", "{0.000000, 1.000000, 0.000000, }" },
            { "floatNet.node[103].inputs", "{21, 86, 81, }" },
            { "floatNet.node[103].mutationRate", "0.919463" },
            { "floatNet.node[103].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[103].params", "{1.050000, 0.486219, 0.855000, 1.482098, 0.000000, }" },
            { "floatNet.node[104].inputs", "{0, 40, 93, }" },
            { "floatNet.node[104].mutationRate", "0.781845" },
            { "floatNet.node[104].op", "ML_FOP_1x0_ARC_TANGENT" },
            { "floatNet.node[104].params", "{0.000000, 0.000000, }" },
            { "floatNet.node[105].inputs", "{}" },
            { "floatNet.node[105].mutationRate", "0.422708" },
            { "floatNet.node[105].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[105].params", "{}" },
            { "floatNet.node[106].inputs", "{69, 51, 21, 62, }" },
            { "floatNet.node[106].mutationRate", "0.278375" },
            { "floatNet.node[106].op", "ML_FOP_NxN_SCALED_MIN" },
            { "floatNet.node[106].params", "{0.217571, 0.199052, 0.000000, 0.000000, }" },
            { "floatNet.node[107].inputs", "{16, 12, 0, 0, }" },
            { "floatNet.node[107].mutationRate", "0.513428" },
            { "floatNet.node[107].op", "ML_FOP_1x3_IF_LTE_ELSE" },
            { "floatNet.node[107].params", "{}" },
            { "floatNet.node[108].inputs", "{}" },
            { "floatNet.node[108].mutationRate", "0.770410" },
            { "floatNet.node[108].op", "ML_FOP_1x0_ARC_TANGENT" },
            { "floatNet.node[108].params", "{}" },
            { "floatNet.node[109].inputs", "{20, 0, }" },
            { "floatNet.node[109].mutationRate", "0.166146" },
            { "floatNet.node[109].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[109].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[110].inputs", "{9, 24, }" },
            { "floatNet.node[110].mutationRate", "0.727754" },
            { "floatNet.node[110].op", "ML_FOP_NxN_SCALED_MAX" },
            { "floatNet.node[110].params", "{0.000000, }" },
            { "floatNet.node[111].inputs", "{51, 77, 0, }" },
            { "floatNet.node[111].mutationRate", "0.000000" },
            { "floatNet.node[111].op", "ML_FOP_NxN_SELECT_GTE" },
            { "floatNet.node[111].params", "{}" },
            { "floatNet.node[112].inputs", "{4, 45, 0, 0, }" },
            { "floatNet.node[112].mutationRate", "0.412140" },
            { "floatNet.node[112].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "floatNet.node[112].params", "{0.000000, }" },
            { "floatNet.node[113].inputs", "{}" },
            { "floatNet.node[113].mutationRate", "0.325976" },
            { "floatNet.node[113].op", "ML_FOP_1x1_LINEAR_DOWN" },
            { "floatNet.node[113].params", "{0.206842, }" },
            { "floatNet.node[114].inputs", "{66, 0, }" },
            { "floatNet.node[114].mutationRate", "0.225766" },
            { "floatNet.node[114].op", "ML_FOP_Nx0_SUM" },
            { "floatNet.node[114].params", "{0.000000, }" },
            { "floatNet.node[115].inputs", "{28, 46, 25, 30, 21, 46, }" },
            { "floatNet.node[115].mutationRate", "0.774364" },
            { "floatNet.node[115].op", "ML_FOP_0x1_CONSTANT" },
            { "floatNet.node[115].params", "{0.997500, 0.855000, 0.158037, 868.201111, 0.000000, 0.475405, }" },
            { "floatNet.node[116].inputs", "{30, 27, 44, 33, 94, 0, 0, }" },
            { "floatNet.node[116].mutationRate", "0.971939" },
            { "floatNet.node[116].op", "ML_FOP_1x2_SINE" },
            { "floatNet.node[116].params", "{}" },
            { "floatNet.node[117].inputs", "{}" },
            { "floatNet.node[117].mutationRate", "0.454499" },
            { "floatNet.node[117].op", "ML_FOP_1x1_QUADRATIC_DOWN" },
            { "floatNet.node[117].params", "{0.213439, 249.438278, 1.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[118].inputs", "{}" },
            { "floatNet.node[118].mutationRate", "0.111549" },
            { "floatNet.node[118].op", "ML_FOP_1x0_ARC_SINE" },
            { "floatNet.node[118].params", "{}" },
            { "floatNet.node[119].inputs", "{23, 59, 76, 11, }" },
            { "floatNet.node[119].mutationRate", "0.180006" },
            { "floatNet.node[119].op", "ML_FOP_4x0_IF_LTE_ELSE" },
            { "floatNet.node[119].params", "{1.000000, }" },
            { "floatNet.node[120].inputs", "{0, }" },
            { "floatNet.node[120].mutationRate", "0.755632" },
            { "floatNet.node[120].op", "ML_FOP_1x0_HYP_COSINE" },
            { "floatNet.node[120].params", "{1.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[121].inputs", "{0, }" },
            { "floatNet.node[121].mutationRate", "1.000000" },
            { "floatNet.node[121].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[121].params", "{10.000000, 989.854614, 10.000000, 0.000000, 1024.776733, 1.511374, 1.000000, 1.000000, }" },
            { "floatNet.node[122].inputs", "{}" },
            { "floatNet.node[122].mutationRate", "0.370457" },
            { "floatNet.node[122].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[122].params", "{}" },
            { "floatNet.node[123].inputs", "{0, }" },
            { "floatNet.node[123].mutationRate", "0.237435" },
            { "floatNet.node[123].op", "ML_FOP_4x0_IF_GTE_ELSE" },
            { "floatNet.node[123].params", "{1.000000, }" },
            { "floatNet.node[124].inputs", "{}" },
            { "floatNet.node[124].mutationRate", "0.374522" },
            { "floatNet.node[124].op", "ML_FOP_1x0_IDENTITY" },
            { "floatNet.node[124].params", "{1.000000, 1.000000, 1.000000, 0.900000, 30.000000, 0.000000, }" },
            { "floatNet.node[25].inputs", "{0, 20, 19, 3, 0, }" },
            { "floatNet.node[25].mutationRate", "0.547262" },
            { "floatNet.node[25].op", "ML_FOP_1x0_EXP" },
            { "floatNet.node[25].params", "{}" },
            { "floatNet.node[26].inputs", "{}" },
            { "floatNet.node[26].mutationRate", "0.660631" },
            { "floatNet.node[26].op", "ML_FOP_1x0_IDENTITY" },
            { "floatNet.node[26].params", "{9.500000, 1.000000, 0.809990, 1.000000, 239.752808, 2.557461, 0.000000, }" },
            { "floatNet.node[27].inputs", "{15, 9, }" },
            { "floatNet.node[27].mutationRate", "0.288218" },
            { "floatNet.node[27].op", "ML_FOP_4x0_IF_LTE_ELSE" },
            { "floatNet.node[27].params", "{}" },
            { "floatNet.node[28].inputs", "{0, 0, 4, 0, }" },
            { "floatNet.node[28].mutationRate", "0.347537" },
            { "floatNet.node[28].op", "ML_FOP_1x2_SINE" },
            { "floatNet.node[28].params", "{0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[29].inputs", "{}" },
            { "floatNet.node[29].mutationRate", "0.355500" },
            { "floatNet.node[29].op", "ML_FOP_1x1_LTE" },
            { "floatNet.node[29].params", "{1.000000, 0.493627, }" },
            { "floatNet.node[30].inputs", "{12, 6, 1, 1, 0, }" },
            { "floatNet.node[30].mutationRate", "0.990000" },
            { "floatNet.node[30].op", "ML_FOP_1x0_SQRT" },
            { "floatNet.node[30].params", "{0.000000, }" },
            { "floatNet.node[31].inputs", "{16, }" },
            { "floatNet.node[31].mutationRate", "0.259829" },
            { "floatNet.node[31].op", "ML_FOP_1x0_INVERSE" },
            { "floatNet.node[31].params", "{0.000000, }" },
            { "floatNet.node[32].inputs", "{17, 23, }" },
            { "floatNet.node[32].mutationRate", "0.142115" },
            { "floatNet.node[32].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "floatNet.node[32].params", "{0.318496, 2.703238, }" },
            { "floatNet.node[33].inputs", "{0, }" },
            { "floatNet.node[33].mutationRate", "0.876225" },
            { "floatNet.node[33].op", "ML_FOP_1x1_POW" },
            { "floatNet.node[33].params", "{}" },
            { "floatNet.node[34].inputs", "{0, 5, }" },
            { "floatNet.node[34].mutationRate", "0.249436" },
            { "floatNet.node[34].op", "ML_FOP_1x3_IF_LTE_ELSE" },
            { "floatNet.node[34].params", "{9.675898, }" },
            { "floatNet.node[35].inputs", "{27, 24, 10, 8, 3, }" },
            { "floatNet.node[35].mutationRate", "0.308092" },
            { "floatNet.node[35].op", "ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT" },
            { "floatNet.node[35].params", "{0.946803, 0.834379, 0.000000, }" },
            { "floatNet.node[36].inputs", "{17, 0, 23, 18, 0, 0, }" },
            { "floatNet.node[36].mutationRate", "0.000000" },
            { "floatNet.node[36].op", "ML_FOP_1x1_GTE" },
            { "floatNet.node[36].params", "{}" },
            { "floatNet.node[37].inputs", "{25, 4, 0, 0, }" },
            { "floatNet.node[37].mutationRate", "0.735050" },
            { "floatNet.node[37].op", "ML_FOP_1x1_QUADRATIC_DOWN" },
            { "floatNet.node[37].params", "{1.889511, 0.297522, 0.542448, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[38].inputs", "{16, 25, 21, 8, }" },
            { "floatNet.node[38].mutationRate", "0.908786" },
            { "floatNet.node[38].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "floatNet.node[38].params", "{147.677063, 1.000000, -0.180388, }" },
            { "floatNet.node[39].inputs", "{0, 16, }" },
            { "floatNet.node[39].mutationRate", "0.903212" },
            { "floatNet.node[39].op", "ML_FOP_1x0_FLOOR" },
            { "floatNet.node[39].params", "{1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[40].inputs", "{32, 33, 36, 37, 9, 0, }" },
            { "floatNet.node[40].mutationRate", "0.703733" },
            { "floatNet.node[40].op", "ML_FOP_1x1_FMOD" },
            { "floatNet.node[40].params", "{11.000000, 0.022183, 12.594612, 1.000000, 1.000000, 0.000000, 0.480767, }" },
            { "floatNet.node[41].inputs", "{}" },
            { "floatNet.node[41].mutationRate", "0.141356" },
            { "floatNet.node[41].op", "ML_FOP_1x1_LINEAR_UP" },
            { "floatNet.node[41].params", "{1.000000, 0.000000, 1.726690, 0.212026, 969.475098, }" },
            { "floatNet.node[42].inputs", "{4, 2, 30, 26, 1, 9, 3, 0, }" },
            { "floatNet.node[42].mutationRate", "0.542105" },
            { "floatNet.node[42].op", "ML_FOP_Nx0_PRODUCT" },
            { "floatNet.node[42].params", "{230.393173, 0.186008, 0.552235, 0.452753, 0.026679, 2.261700, 1.505523, 0.000000, }" },
            { "floatNet.node[43].inputs", "{36, 0, }" },
            { "floatNet.node[43].mutationRate", "0.320179" },
            { "floatNet.node[43].op", "ML_FOP_1x1_STRICT_ON" },
            { "floatNet.node[43].params", "{0.218088, 0.192591, }" },
            { "floatNet.node[44].inputs", "{}" },
            { "floatNet.node[44].mutationRate", "0.109135" },
            { "floatNet.node[44].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[44].params", "{}" },
            { "floatNet.node[45].inputs", "{9, 5, 35, }" },
            { "floatNet.node[45].mutationRate", "0.810000" },
            { "floatNet.node[45].op", "ML_FOP_2x0_POW" },
            { "floatNet.node[45].params", "{1.000000, 2915.194580, 247.507675, 0.000000, 0.000000, }" },
            { "floatNet.node[46].inputs", "{}" },
            { "floatNet.node[46].mutationRate", "0.109417" },
            { "floatNet.node[46].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[46].params", "{0.994774, 0.000000, }" },
            { "floatNet.node[47].inputs", "{0, }" },
            { "floatNet.node[47].mutationRate", "0.149971" },
            { "floatNet.node[47].op", "ML_FOP_NxN_SELECT_GTE" },
            { "floatNet.node[47].params", "{3.272798, 1.000000, 1.000000, 0.000000, }" },
            { "floatNet.node[48].inputs", "{45, }" },
            { "floatNet.node[48].mutationRate", "0.803098" },
            { "floatNet.node[48].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "floatNet.node[48].params", "{2.569540, 0.982609, 0.208709, 0.000000, }" },
            { "floatNet.node[49].inputs", "{14, 0, }" },
            { "floatNet.node[49].mutationRate", "0.532824" },
            { "floatNet.node[49].op", "ML_FOP_1x2_COSINE" },
            { "floatNet.node[49].params", "{}" },
            { "floatNet.node[50].inputs", "{20, 15, 3, 17, 0, }" },
            { "floatNet.node[50].mutationRate", "0.958871" },
            { "floatNet.node[50].op", "ML_FOP_1x0_FLOOR" },
            { "floatNet.node[50].params", "{151.020752, 0.209279, }" },
            { "floatNet.node[51].inputs", "{47, 0, }" },
            { "floatNet.node[51].mutationRate", "0.825694" },
            { "floatNet.node[51].op", "ML_FOP_1x3_IF_GTE_ELSE" },
            { "floatNet.node[51].params", "{0.000000, }" },
            { "floatNet.node[52].inputs", "{}" },
            { "floatNet.node[52].mutationRate", "0.642046" },
            { "floatNet.node[52].op", "ML_FOP_4x0_IF_GTE_ELSE" },
            { "floatNet.node[52].params", "{0.950000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[53].inputs", "{7, }" },
            { "floatNet.node[53].mutationRate", "0.131893" },
            { "floatNet.node[53].op", "ML_FOP_NxN_LINEAR_COMBINATION" },
            { "floatNet.node[53].params", "{0.000000, 0.000000, }" },
            { "floatNet.node[54].inputs", "{30, 38, 0, }" },
            { "floatNet.node[54].mutationRate", "0.683502" },
            { "floatNet.node[54].op", "ML_FOP_1x0_HYP_SINE" },
            { "floatNet.node[54].params", "{0.000000, }" },
            { "floatNet.node[55].inputs", "{26, 17, 26, 0, 0, }" },
            { "floatNet.node[55].mutationRate", "1.000000" },
            { "floatNet.node[55].op", "ML_FOP_1x1_STRICT_OFF" },
            { "floatNet.node[55].params", "{}" },
            { "floatNet.node[56].inputs", "{0, }" },
            { "floatNet.node[56].mutationRate", "0.000000" },
            { "floatNet.node[56].op", "ML_FOP_1x0_HYP_SINE" },
            { "floatNet.node[56].params", "{0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[57].inputs", "{42, 46, }" },
            { "floatNet.node[57].mutationRate", "0.130578" },
            { "floatNet.node[57].op", "ML_FOP_4x0_IF_LTE_ELSE" },
            { "floatNet.node[57].params", "{1.000000, 1.050000, 0.000000, }" },
            { "floatNet.node[58].inputs", "{}" },
            { "floatNet.node[58].mutationRate", "0.190180" },
            { "floatNet.node[58].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "floatNet.node[58].params", "{0.000000, }" },
            { "floatNet.node[59].inputs", "{0, 18, }" },
            { "floatNet.node[59].mutationRate", "0.505566" },
            { "floatNet.node[59].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[59].params", "{1.000000, 1.721286, 1.479195, 0.164023, 0.000000, 30.000000, 0.000000, }" },
            { "floatNet.node[60].inputs", "{52, }" },
            { "floatNet.node[60].mutationRate", "0.806230" },
            { "floatNet.node[60].op", "ML_FOP_1x2_COSINE" },
            { "floatNet.node[60].params", "{0.881979, 1.000000, 0.000000, 1.007488, 1.723415, }" },
            { "floatNet.node[61].inputs", "{33, }" },
            { "floatNet.node[61].mutationRate", "1.000000" },
            { "floatNet.node[61].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[61].params", "{1.000000, 0.718616, 1.000000, 0.378065, 1.161303, 8.090465, 8.900914, 0.000000, }" },
            { "floatNet.node[62].inputs", "{34, 10, 36, 46, 29, 0, }" },
            { "floatNet.node[62].mutationRate", "0.444333" },
            { "floatNet.node[62].op", "ML_FOP_1x2_SINE" },
            { "floatNet.node[62].params", "{0.214919, 1.000000, 0.816799, 1.604426, 0.803607, 0.000000, 0.000000, }" },
            { "floatNet.node[63].inputs", "{}" },
            { "floatNet.node[63].mutationRate", "0.808283" },
            { "floatNet.node[63].op", "ML_FOP_1x0_HYP_COSINE" },
            { "floatNet.node[63].params", "{1.629984, 0.947625, 1338.149170, 977.586731, 0.000000, 0.000000, }" },
            { "floatNet.node[64].inputs", "{}" },
            { "floatNet.node[64].mutationRate", "0.394210" },
            { "floatNet.node[64].op", "ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT" },
            { "floatNet.node[64].params", "{1.000000, 2922.915283, 1.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[65].inputs", "{60, 0, }" },
            { "floatNet.node[65].mutationRate", "0.369243" },
            { "floatNet.node[65].op", "ML_FOP_1x1_FMOD" },
            { "floatNet.node[65].params", "{}" },
            { "floatNet.node[66].inputs", "{2, 0, }" },
            { "floatNet.node[66].mutationRate", "0.597397" },
            { "floatNet.node[66].op", "ML_FOP_1x2_CLAMP" },
            { "floatNet.node[66].params", "{}" },
            { "floatNet.node[67].inputs", "{}" },
            { "floatNet.node[67].mutationRate", "0.521303" },
            { "floatNet.node[67].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[67].params", "{10.000000, 0.180421, 0.000000, 0.000000, }" },
            { "floatNet.node[68].inputs", "{50, 47, 23, 0, }" },
            { "floatNet.node[68].mutationRate", "0.402062" },
            { "floatNet.node[68].op", "ML_FOP_1x0_EXP" },
            { "floatNet.node[68].params", "{1.000000, 1.476961, 1.542832, 0.000000, }" },
            { "floatNet.node[69].inputs", "{42, 0, }" },
            { "floatNet.node[69].mutationRate", "0.383516" },
            { "floatNet.node[69].op", "ML_FOP_1x0_ABS" },
            { "floatNet.node[69].params", "{1.000000, 0.651541, 1.000000, 1.050000, 0.000000, 0.000000, }" },
            { "floatNet.node[70].inputs", "{5, }" },
            { "floatNet.node[70].mutationRate", "0.750690" },
            { "floatNet.node[70].op", "ML_FOP_Nx0_MIN" },
            { "floatNet.node[70].params", "{}" },
            { "floatNet.node[71].inputs", "{0, }" },
            { "floatNet.node[71].mutationRate", "0.900360" },
            { "floatNet.node[71].op", "ML_FOP_1x3_IF_LTE_ELSE" },
            { "floatNet.node[71].params", "{0.000000, 0.000000, }" },
            { "floatNet.node[72].inputs", "{34, }" },
            { "floatNet.node[72].mutationRate", "0.669021" },
            { "floatNet.node[72].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[72].params", "{1.000000, 0.000000, 8821.981445, 0.000000, }" },
            { "floatNet.node[73].inputs", "{}" },
            { "floatNet.node[73].mutationRate", "0.629911" },
            { "floatNet.node[73].op", "ML_FOP_1x1_POW" },
            { "floatNet.node[73].params", "{0.783255, 9241.718750, 323.068756, 0.000000, 0.000000, }" },
            { "floatNet.node[74].inputs", "{}" },
            { "floatNet.node[74].mutationRate", "0.463259" },
            { "floatNet.node[74].op", "ML_FOP_1x0_EXP" },
            { "floatNet.node[74].params", "{0.202690, 0.000000, 0.000000, }" },
            { "floatNet.node[75].inputs", "{5, 4, 0, 0, }" },
            { "floatNet.node[75].mutationRate", "0.699598" },
            { "floatNet.node[75].op", "ML_FOP_1x1_GTE" },
            { "floatNet.node[75].params", "{5000.000000, 0.118993, 0.000000, 6057.990723, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[76].inputs", "{62, 0, }" },
            { "floatNet.node[76].mutationRate", "0.589950" },
            { "floatNet.node[76].op", "ML_FOP_1x2_COSINE" },
            { "floatNet.node[76].params", "{0.000000, }" },
            { "floatNet.node[77].inputs", "{73, 0, 73, }" },
            { "floatNet.node[77].mutationRate", "0.430054" },
            { "floatNet.node[77].op", "ML_FOP_NxN_SELECT_LTE" },
            { "floatNet.node[77].params", "{0.000000, 1.000000, }" },
            { "floatNet.node[78].inputs", "{}" },
            { "floatNet.node[78].mutationRate", "0.685935" },
            { "floatNet.node[78].op", "ML_FOP_4x0_IF_LTE_ELSE" },
            { "floatNet.node[78].params", "{}" },
            { "floatNet.node[79].inputs", "{17, }" },
            { "floatNet.node[79].mutationRate", "0.196050" },
            { "floatNet.node[79].op", "ML_FOP_1x0_ARC_COSINE" },
            { "floatNet.node[79].params", "{1.000000, 0.000000, 1.000000, 2.540294, }" },
            { "floatNet.node[80].inputs", "{14, 72, 12, 32, }" },
            { "floatNet.node[80].mutationRate", "0.122250" },
            { "floatNet.node[80].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "floatNet.node[80].params", "{0.000000, 0.205995, 0.208297, 10.000000, 0.410165, 0.000000, }" },
            { "floatNet.node[81].inputs", "{0, }" },
            { "floatNet.node[81].mutationRate", "0.603534" },
            { "floatNet.node[81].op", "ML_FOP_1x2_COSINE" },
            { "floatNet.node[81].params", "{1.000000, 0.000000, }" },
            { "floatNet.node[82].inputs", "{0, }" },
            { "floatNet.node[82].mutationRate", "0.246631" },
            { "floatNet.node[82].op", "ML_FOP_1x0_SQRT" },
            { "floatNet.node[82].params", "{255.564316, }" },
            { "floatNet.node[83].inputs", "{0, }" },
            { "floatNet.node[83].mutationRate", "0.189908" },
            { "floatNet.node[83].op", "ML_FOP_1x2_COSINE" },
            { "floatNet.node[83].params", "{996.419739, }" },
            { "floatNet.node[84].inputs", "{0, }" },
            { "floatNet.node[84].mutationRate", "0.200351" },
            { "floatNet.node[84].op", "ML_FOP_0x0_ONE" },
            { "floatNet.node[84].params", "{0.780481, 1.000000, 1671.849487, 0.000000, }" },
            { "floatNet.node[85].inputs", "{0, 82, }" },
            { "floatNet.node[85].mutationRate", "0.619155" },
            { "floatNet.node[85].op", "ML_FOP_Nx0_MIN" },
            { "floatNet.node[85].params", "{1.741242, }" },
            { "floatNet.node[86].inputs", "{40, 7, 0, }" },
            { "floatNet.node[86].mutationRate", "0.507868" },
            { "floatNet.node[86].op", "ML_FOP_1x1_STRICT_OFF" },
            { "floatNet.node[86].params", "{0.000000, 0.520110, 0.205290, 0.000000, }" },
            { "floatNet.node[87].inputs", "{57, 0, }" },
            { "floatNet.node[87].mutationRate", "0.345950" },
            { "floatNet.node[87].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "floatNet.node[87].params", "{2.539918, 0.212144, 990.963989, 0.181965, 0.000000, -0.213307, }" },
            { "floatNet.node[88].inputs", "{79, 0, }" },
            { "floatNet.node[88].mutationRate", "0.150889" },
            { "floatNet.node[88].op", "ML_FOP_1x1_LINEAR_DOWN" },
            { "floatNet.node[88].params", "{}" },
            { "floatNet.node[89].inputs", "{49, }" },
            { "floatNet.node[89].mutationRate", "0.988937" },
            { "floatNet.node[89].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "floatNet.node[89].params", "{1.000000, 0.000000, }" },
            { "floatNet.node[90].inputs", "{0, }" },
            { "floatNet.node[90].mutationRate", "0.194130" },
            { "floatNet.node[90].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "floatNet.node[90].params", "{}" },
            { "floatNet.node[91].inputs", "{76, 53, 0, 0, }" },
            { "floatNet.node[91].mutationRate", "0.049148" },
            { "floatNet.node[91].op", "ML_FOP_1x1_QUADRATIC_DOWN" },
            { "floatNet.node[91].params", "{}" },
            { "floatNet.node[92].inputs", "{}" },
            { "floatNet.node[92].mutationRate", "0.000000" },
            { "floatNet.node[92].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[92].params", "{}" },
            { "floatNet.node[93].inputs", "{0, }" },
            { "floatNet.node[93].mutationRate", "0.312741" },
            { "floatNet.node[93].op", "ML_FOP_1x0_HYP_TANGENT" },
            { "floatNet.node[93].params", "{1.000000, }" },
            { "floatNet.node[94].inputs", "{}" },
            { "floatNet.node[94].mutationRate", "0.000000" },
            { "floatNet.node[94].op", "ML_FOP_NxN_SELECT_LTE" },
            { "floatNet.node[94].params", "{}" },
            { "floatNet.node[95].inputs", "{90, 86, 19, 0, }" },
            { "floatNet.node[95].mutationRate", "0.714157" },
            { "floatNet.node[95].op", "ML_FOP_NxN_SELECT_LTE" },
            { "floatNet.node[95].params", "{0.198811, }" },
            { "floatNet.node[96].inputs", "{61, }" },
            { "floatNet.node[96].mutationRate", "0.275709" },
            { "floatNet.node[96].op", "ML_FOP_1x3_IF_LTE_ELSE" },
            { "floatNet.node[96].params", "{}" },
            { "floatNet.node[97].inputs", "{}" },
            { "floatNet.node[97].mutationRate", "0.209452" },
            { "floatNet.node[97].op", "ML_FOP_1x1_LTE" },
            { "floatNet.node[97].params", "{0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[98].inputs", "{40, 58, 12, 0, 0, }" },
            { "floatNet.node[98].mutationRate", "0.197299" },
            { "floatNet.node[98].op", "ML_FOP_1x0_IDENTITY" },
            { "floatNet.node[98].params", "{0.220758, 0.950000, 0.526828, 0.000000, 0.187072, 978.703247, }" },
            { "floatNet.node[99].inputs", "{70, 29, }" },
            { "floatNet.node[99].mutationRate", "0.237406" },
            { "floatNet.node[99].op", "ML_FOP_1x2_CLAMP" },
            { "floatNet.node[99].params", "{}" },
            { "floatNet.numInputs", "25" },
            { "floatNet.numNodes", "100" },
            { "floatNet.numOutputs", "25" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "47.517754" },
            { "guardRange", "85.632729" },
            { "input[0].crowdType", "NEURAL_CROWD_CORES" },
            { "input[0].radius", "0.000000" },
            { "input[0].useTangent", "TRUE" },
            { "input[0].valueType", "NEURAL_VALUE_MOBID" },
            { "input[10].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[10].forceType", "NEURAL_FORCE_CORNERS" },
            { "input[10].radius", "0.000000" },
            { "input[10].useTangent", "TRUE" },
            { "input[10].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[11].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "input[11].forceType", "NEURAL_FORCE_ZERO" },
            { "input[11].radius", "0.000000" },
            { "input[11].useTangent", "TRUE" },
            { "input[11].valueType", "NEURAL_VALUE_FORCE" },
            { "input[12].forceType", "NEURAL_FORCE_BASE" },
            { "input[12].radius", "0.000000" },
            { "input[12].useTangent", "TRUE" },
            { "input[12].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[13].crowdType", "NEURAL_CROWD_CORES" },
            { "input[13].forceType", "NEURAL_FORCE_ENEMY" },
            { "input[13].radius", "0.000000" },
            { "input[13].valueType", "NEURAL_VALUE_TICK" },
            { "input[14].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[14].forceType", "NEURAL_FORCE_SEPARATE" },
            { "input[14].radius", "0.000000" },
            { "input[14].useTangent", "FALSE" },
            { "input[14].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[15].crowdType", "NEURAL_CROWD_CORES" },
            { "input[15].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "input[15].radius", "0.000000" },
            { "input[15].useTangent", "TRUE" },
            { "input[15].valueType", "NEURAL_VALUE_CROWD" },
            { "input[16].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[16].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "input[16].radius", "0.000000" },
            { "input[16].useTangent", "FALSE" },
            { "input[16].valueType", "NEURAL_VALUE_FORCE" },
            { "input[17].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "input[17].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS" },
            { "input[17].radius", "0.000000" },
            { "input[17].useTangent", "TRUE" },
            { "input[17].valueType", "NEURAL_VALUE_CROWD" },
            { "input[18].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[18].forceType", "NEURAL_FORCE_ALIGN" },
            { "input[18].radius", "0.000000" },
            { "input[18].useTangent", "FALSE" },
            { "input[18].valueType", "NEURAL_VALUE_CREDITS" },
            { "input[19].crowdType", "NEURAL_CROWD_CORES" },
            { "input[19].radius", "0.000000" },
            { "input[19].valueType", "NEURAL_VALUE_MOBID" },
            { "input[1].crowdType", "NEURAL_CROWD_FRIEND_MISSILE" },
            { "input[1].forceType", "NEURAL_FORCE_ALIGN" },
            { "input[1].radius", "0.000000" },
            { "input[1].useTangent", "TRUE" },
            { "input[1].valueType", "NEURAL_VALUE_CREDITS" },
            { "input[20].crowdType", "NEURAL_CROWD_FRIEND_MISSILE" },
            { "input[20].radius", "0.000000" },
            { "input[20].useTangent", "TRUE" },
            { "input[20].valueType", "NEURAL_VALUE_TICK" },
            { "input[21].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[21].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "input[21].radius", "0.000000" },
            { "input[21].useTangent", "TRUE" },
            { "input[21].valueType", "NEURAL_VALUE_CREDITS" },
            { "input[22].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[22].forceType", "NEURAL_FORCE_COHERE" },
            { "input[22].radius", "-1.000000" },
            { "input[22].useTangent", "TRUE" },
            { "input[22].valueType", "NEURAL_VALUE_MOBID" },
            { "input[23].crowdType", "NEURAL_CROWD_FRIEND_MISSILE" },
            { "input[23].forceType", "NEURAL_FORCE_CENTER" },
            { "input[23].radius", "0.000000" },
            { "input[23].useTangent", "FALSE" },
            { "input[23].valueType", "NEURAL_VALUE_RANGE" },
            { "input[24].crowdType", "NEURAL_CROWD_CORES" },
            { "input[24].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "input[24].radius", "1876.846924" },
            { "input[24].useTangent", "FALSE" },
            { "input[24].valueType", "NEURAL_VALUE_CREDITS" },
            { "input[2].crowdType", "NEURAL_CROWD_FRIEND_MISSILE" },
            { "input[2].forceType", "NEURAL_FORCE_CORES" },
            { "input[2].radius", "142.804443" },
            { "input[2].useTangent", "FALSE" },
            { "input[2].valueType", "NEURAL_VALUE_TICK" },
            { "input[3].crowdType", "NEURAL_CROWD_CORES" },
            { "input[3].forceType", "NEURAL_FORCE_CENTER" },
            { "input[3].radius", "0.000000" },
            { "input[3].useTangent", "FALSE" },
            { "input[3].valueType", "NEURAL_VALUE_CROWD" },
            { "input[4].crowdType", "NEURAL_CROWD_CORES" },
            { "input[4].forceType", "NEURAL_FORCE_COHERE" },
            { "input[4].radius", "0.000000" },
            { "input[4].useTangent", "FALSE" },
            { "input[4].valueType", "NEURAL_VALUE_CREDITS" },
            { "input[5].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[5].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "input[5].radius", "0.000000" },
            { "input[5].useTangent", "FALSE" },
            { "input[5].valueType", "NEURAL_VALUE_CROWD" },
            { "input[6].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[6].forceType", "NEURAL_FORCE_EDGES" },
            { "input[6].radius", "0.000000" },
            { "input[6].useTangent", "TRUE" },
            { "input[6].valueType", "NEURAL_VALUE_RANGE" },
            { "input[7].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "input[7].forceType", "NEURAL_FORCE_BASE" },
            { "input[7].radius", "0.000000" },
            { "input[7].useTangent", "TRUE" },
            { "input[7].valueType", "NEURAL_VALUE_FORCE" },
            { "input[8].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[8].forceType", "NEURAL_FORCE_HEADING" },
            { "input[8].radius", "0.000000" },
            { "input[8].useTangent", "TRUE" },
            { "input[8].valueType", "NEURAL_VALUE_CREDITS" },
            { "input[9].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "input[9].radius", "0.000000" },
            { "input[9].useTangent", "FALSE" },
            { "input[9].valueType", "NEURAL_VALUE_MOBID" },
            { "output[100].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "output[100].radius", "-1.000000" },
            { "output[100].useTangent", "FALSE" },
            { "output[100].valueType", "NEURAL_VALUE_FORCE" },
            { "output[101].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "output[101].radius", "0.000000" },
            { "output[101].useTangent", "TRUE" },
            { "output[101].valueType", "NEURAL_VALUE_FORCE" },
            { "output[102].forceType", "NEURAL_FORCE_HEADING" },
            { "output[102].radius", "0.000000" },
            { "output[102].useTangent", "FALSE" },
            { "output[102].valueType", "NEURAL_VALUE_FORCE" },
            { "output[103].forceType", "NEURAL_FORCE_CORES" },
            { "output[103].radius", "-1.000000" },
            { "output[103].useTangent", "FALSE" },
            { "output[103].valueType", "NEURAL_VALUE_FORCE" },
            { "output[104].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "output[104].radius", "1716.954956" },
            { "output[104].useTangent", "TRUE" },
            { "output[104].valueType", "NEURAL_VALUE_FORCE" },
            { "output[105].forceType", "NEURAL_FORCE_ZERO" },
            { "output[105].radius", "0.000000" },
            { "output[105].useTangent", "FALSE" },
            { "output[105].valueType", "NEURAL_VALUE_FORCE" },
            { "output[106].forceType", "NEURAL_FORCE_ALIGN" },
            { "output[106].radius", "-0.857375" },
            { "output[106].useTangent", "TRUE" },
            { "output[106].valueType", "NEURAL_VALUE_FORCE" },
            { "output[107].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[107].radius", "0.000000" },
            { "output[107].useTangent", "TRUE" },
            { "output[107].valueType", "NEURAL_VALUE_FORCE" },
            { "output[108].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS" },
            { "output[108].radius", "0.000000" },
            { "output[108].useTangent", "TRUE" },
            { "output[108].valueType", "NEURAL_VALUE_FORCE" },
            { "output[109].forceType", "NEURAL_FORCE_BASE" },
            { "output[109].radius", "426.598633" },
            { "output[109].useTangent", "TRUE" },
            { "output[109].valueType", "NEURAL_VALUE_FORCE" },
            { "output[110].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[110].radius", "0.000000" },
            { "output[110].useTangent", "TRUE" },
            { "output[110].valueType", "NEURAL_VALUE_FORCE" },
            { "output[111].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[111].radius", "0.000000" },
            { "output[111].useTangent", "TRUE" },
            { "output[111].valueType", "NEURAL_VALUE_FORCE" },
            { "output[112].forceType", "NEURAL_FORCE_CORNERS" },
            { "output[112].radius", "0.000000" },
            { "output[112].useTangent", "TRUE" },
            { "output[112].valueType", "NEURAL_VALUE_FORCE" },
            { "output[113].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "output[113].radius", "0.000000" },
            { "output[113].useTangent", "FALSE" },
            { "output[113].valueType", "NEURAL_VALUE_FORCE" },
            { "output[114].forceType", "NEURAL_FORCE_CORES" },
            { "output[114].radius", "0.000000" },
            { "output[114].useTangent", "TRUE" },
            { "output[114].valueType", "NEURAL_VALUE_FORCE" },
            { "output[115].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "output[115].radius", "0.000000" },
            { "output[115].useTangent", "FALSE" },
            { "output[115].valueType", "NEURAL_VALUE_FORCE" },
            { "output[116].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "output[116].radius", "0.000000" },
            { "output[116].useTangent", "FALSE" },
            { "output[116].valueType", "NEURAL_VALUE_FORCE" },
            { "output[117].forceType", "NEURAL_FORCE_ZERO" },
            { "output[117].radius", "-1.000000" },
            { "output[117].useTangent", "TRUE" },
            { "output[117].valueType", "NEURAL_VALUE_FORCE" },
            { "output[118].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "output[118].radius", "0.000000" },
            { "output[118].useTangent", "FALSE" },
            { "output[118].valueType", "NEURAL_VALUE_FORCE" },
            { "output[119].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "output[119].radius", "0.000000" },
            { "output[119].useTangent", "FALSE" },
            { "output[119].valueType", "NEURAL_VALUE_FORCE" },
            { "output[120].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "output[120].radius", "0.000000" },
            { "output[120].useTangent", "FALSE" },
            { "output[120].valueType", "NEURAL_VALUE_FORCE" },
            { "output[121].forceType", "NEURAL_FORCE_EDGES" },
            { "output[121].radius", "-1.000000" },
            { "output[121].useTangent", "TRUE" },
            { "output[121].valueType", "NEURAL_VALUE_FORCE" },
            { "output[122].forceType", "NEURAL_FORCE_COHERE" },
            { "output[122].radius", "0.000000" },
            { "output[122].useTangent", "FALSE" },
            { "output[122].valueType", "NEURAL_VALUE_FORCE" },
            { "output[123].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[123].radius", "0.000000" },
            { "output[123].useTangent", "FALSE" },
            { "output[123].valueType", "NEURAL_VALUE_FORCE" },
            { "output[124].forceType", "NEURAL_FORCE_CORNERS" },
            { "output[124].radius", "0.000000" },
            { "output[124].useTangent", "FALSE" },
            { "output[124].valueType", "NEURAL_VALUE_FORCE" },
            { "rotateStartingAngle", "TRUE" },
            { "sensorGrid.staleCoreTime", "0.000000" },
            { "sensorGrid.staleFighterTime", "0.000000" },
            { "startingMaxRadius", "1650.026733" },
            { "startingMinRadius", "395.996002" },
        };

        struct {
            NeuralConfigValue *values;
            uint numValues;
        } configs[] = {
            { defaults,  ARRAYSIZE(defaults), },
            { configs1,  ARRAYSIZE(configs1), },
            { configs2,  ARRAYSIZE(configs2), },
        };

        int neuralIndex = aiType - FLEET_AI_NEURAL1 + 1;
        VERIFY(aiType >= FLEET_AI_NEURAL1);
        VERIFY(aiType <= FLEET_AI_NEURAL2);
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
        if (MBRegistry_ContainsKey(mreg, "floatNet.numInputs") &&
            MBRegistry_GetUint(mreg, "floatNet.numInputs") > 0) {
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
        } else if (desc->crowdType == NEURAL_CROWD_ENEMY_MISSILE) {
            return sg->numTargetsInRange(MOB_FLAG_MISSILE,
                                         &mob->pos, desc->radius);
        } else if (desc->crowdType == NEURAL_CROWD_BASE_ENEMY_SHIP) {
            Mob *base = sg->friendBase();
            if (base != NULL) {
                return sg->numTargetsInRange(MOB_FLAG_SHIP,
                                             &base->pos, desc->radius);
            }
            return 0.0f;
        } else if (desc->crowdType == NEURAL_CROWD_BASE_FRIEND_SHIP) {
            Mob *base = sg->friendBase();
            if (base != NULL) {
                return sg->numFriendsInRange(MOB_FLAG_SHIP,
                                             &base->pos, desc->radius);
            }
            return 0.0f;
        } else {
            NOT_IMPLEMENTED();
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
    } else if (aiType == FLEET_AI_NEURAL2) {
        ops->aiName = "NeuralFleet2";
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
