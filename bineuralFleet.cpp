/*
 * bineuralFleet.cpp -- part of SpaceRobots2
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

#include "battle.h"
#include "Random.h"
#include "fleet.h"
#include "mutate.h"
#include "MBUtil.h"

#include "sensorGrid.hpp"
#include "basicShipAI.hpp"
#include "MBMap.hpp"
#include "MBString.hpp"
#include "ml.hpp"
#include "floatNet.hpp"
#include "textDump.hpp"

#include "neuralNet.hpp"

#define BINEURAL_MAX_NODE_DEGREE  8
#define BINEURAL_MAX_INPUTS      30
#define BINEURAL_MAX_OUTPUTS     30
#define BINEURAL_MAX_NODES       130

#define BINEURAL_SCRAMBLE_KEY "bineuralFleet.scrambleMutation"

typedef struct BineuralConfigValue {
    const char *key;
    const char *value;
} BineuralConfigValue;

class BineuralAIGovernor : public BasicAIGovernor
{
public:
    // Members
    AIContext myAIC;
    NeuralNet myShipNet;
    NeuralNet myFleetNet;

public:
    BineuralAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        myAIC.rs = &myRandomState;
        myAIC.sg = sg;
        myAIC.ai = myFleetAI;

        myShipNet.aic = myAIC;
        myFleetNet.aic = myAIC;
    }

    virtual ~BineuralAIGovernor() { }

    class BineuralShipAI : public BasicShipAI
    {
        public:
        BineuralShipAI(MobID mobid, BineuralAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        { }

        virtual ~BineuralShipAI() {}
    };

    //XXX Only saves some of the state.
    void dumpSanitizedParams(MBRegistry *mreg) {
        myShipNet.save(mreg, "shipNet.");
        myShipNet.dumpSanitizedParams(mreg, "shipNet.");

        myFleetNet.save(mreg, "fleetNet.");
        myFleetNet.dumpSanitizedParams(mreg, "fleetNet.");
    }

    virtual BineuralShipAI *newShip(MobID mobid) {
        return new BineuralShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        BineuralConfigValue defaults[] = {
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

            { "useAttackForces",             "FALSE"     },
        };

        BineuralConfigValue configs1[] = {
            { "attackExtendedRange", "TRUE" },
            { "attackRange", "119.889206" },
            { "creditReserve", "0.000000" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "-1.000000" },
            { "evadeStrictDistance", "112.614983" },
            { "evadeUseStrictDistance", "FALSE" },
            { "fleetNet.numInputs", "30" },
            { "fleetNet.numNodes", "130" },
            { "fleetNet.numOutputs", "30" },
            { "shipNet.input[0].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[0].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "shipNet.input[0].radius", "0.000000" },
            { "shipNet.input[0].useTangent", "FALSE" },
            { "shipNet.input[0].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[10].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[10].filterAdvance", "FALSE" },
            { "shipNet.input[10].filterBackward", "FALSE" },
            { "shipNet.input[10].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS_LAX" },
            { "shipNet.input[10].radius", "149.341309" },
            { "shipNet.input[10].useTangent", "TRUE" },
            { "shipNet.input[10].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[11].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[11].filterAdvance", "FALSE" },
            { "shipNet.input[11].filterRetreat", "FALSE" },
            { "shipNet.input[11].forceType", "NEURAL_FORCE_CORNERS" },
            { "shipNet.input[11].frequency", "0.000000" },
            { "shipNet.input[11].radius", "157.342880" },
            { "shipNet.input[11].useTangent", "TRUE" },
            { "shipNet.input[11].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[11].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[12].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[12].filterBackward", "FALSE" },
            { "shipNet.input[12].filterForward", "FALSE" },
            { "shipNet.input[12].forceType", "NEURAL_FORCE_CENTER" },
            { "shipNet.input[12].frequency", "0.000000" },
            { "shipNet.input[12].radius", "0.000000" },
            { "shipNet.input[12].useTangent", "FALSE" },
            { "shipNet.input[12].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[12].waveType", "NEURAL_WAVE_NONE" },
            { "shipNet.input[13].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[13].filterAdvance", "TRUE" },
            { "shipNet.input[13].filterBackward", "FALSE" },
            { "shipNet.input[13].filterForward", "FALSE" },
            { "shipNet.input[13].filterRetreat", "TRUE" },
            { "shipNet.input[13].forceType", "NEURAL_FORCE_CENTER" },
            { "shipNet.input[13].frequency", "0.000000" },
            { "shipNet.input[13].radius", "152.739212" },
            { "shipNet.input[13].useTangent", "TRUE" },
            { "shipNet.input[13].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[13].waveType", "NEURAL_WAVE_SINE" },
            { "shipNet.input[14].crowdType", "NEURAL_CROWD_FRIEND_MISSILE" },
            { "shipNet.input[14].forceType", "NEURAL_FORCE_SEPARATE" },
            { "shipNet.input[14].frequency", "0.000000" },
            { "shipNet.input[14].radius", "164.939926" },
            { "shipNet.input[14].useTangent", "TRUE" },
            { "shipNet.input[14].valueType", "NEURAL_VALUE_ZERO" },
            { "shipNet.input[14].waveType", "NEURAL_WAVE_SINE" },
            { "shipNet.input[15].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[15].filterAdvance", "FALSE" },
            { "shipNet.input[15].filterBackward", "FALSE" },
            { "shipNet.input[15].filterForward", "TRUE" },
            { "shipNet.input[15].filterRetreat", "FALSE" },
            { "shipNet.input[15].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS_LAX" },
            { "shipNet.input[15].radius", "302.953278" },
            { "shipNet.input[15].useTangent", "FALSE" },
            { "shipNet.input[15].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[16].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "shipNet.input[16].forceType", "NEURAL_FORCE_CENTER" },
            { "shipNet.input[16].frequency", "0.000000" },
            { "shipNet.input[16].radius", "0.000000" },
            { "shipNet.input[16].useTangent", "FALSE" },
            { "shipNet.input[16].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[17].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "shipNet.input[17].filterAdvance", "FALSE" },
            { "shipNet.input[17].filterBackward", "TRUE" },
            { "shipNet.input[17].filterForward", "FALSE" },
            { "shipNet.input[17].filterRetreat", "FALSE" },
            { "shipNet.input[17].forceType", "NEURAL_FORCE_FARTHEST_EDGE" },
            { "shipNet.input[17].frequency", "4093.644043" },
            { "shipNet.input[17].radius", "144.868546" },
            { "shipNet.input[17].useTangent", "FALSE" },
            { "shipNet.input[17].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[17].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[18].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "shipNet.input[18].forceType", "NEURAL_FORCE_FORWARD_ENEMY_COHERE" },
            { "shipNet.input[18].frequency", "0.000000" },
            { "shipNet.input[18].radius", "147.504044" },
            { "shipNet.input[18].useTangent", "TRUE" },
            { "shipNet.input[18].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[18].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[19].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "shipNet.input[19].forceType", "NEURAL_FORCE_FARTHEST_CORNER" },
            { "shipNet.input[19].frequency", "0.000000" },
            { "shipNet.input[19].radius", "-0.950000" },
            { "shipNet.input[19].useTangent", "TRUE" },
            { "shipNet.input[19].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[19].waveType", "NEURAL_WAVE_UNIT_SINE" },
            { "shipNet.input[1].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[1].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "shipNet.input[1].radius", "301.750824" },
            { "shipNet.input[1].useTangent", "TRUE" },
            { "shipNet.input[1].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[20].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[20].forceType", "NEURAL_FORCE_FORWARD_SEPARATE" },
            { "shipNet.input[20].radius", "0.000000" },
            { "shipNet.input[20].useTangent", "FALSE" },
            { "shipNet.input[20].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[20].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[21].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[21].filterAdvance", "FALSE" },
            { "shipNet.input[21].filterForward", "TRUE" },
            { "shipNet.input[21].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS_LAX" },
            { "shipNet.input[21].frequency", "0.000000" },
            { "shipNet.input[21].radius", "-1.000000" },
            { "shipNet.input[21].useTangent", "FALSE" },
            { "shipNet.input[21].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[22].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[22].filterAdvance", "TRUE" },
            { "shipNet.input[22].filterBackward", "TRUE" },
            { "shipNet.input[22].filterForward", "TRUE" },
            { "shipNet.input[22].filterRetreat", "TRUE" },
            { "shipNet.input[22].forceType", "NEURAL_FORCE_ENEMY" },
            { "shipNet.input[22].radius", "6.802381" },
            { "shipNet.input[22].useTangent", "FALSE" },
            { "shipNet.input[22].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[22].waveType", "NEURAL_WAVE_NONE" },
            { "shipNet.input[23].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[23].filterRetreat", "FALSE" },
            { "shipNet.input[23].forceType", "NEURAL_FORCE_BACKWARD_COHERE" },
            { "shipNet.input[23].frequency", "0.000000" },
            { "shipNet.input[23].radius", "-1.000000" },
            { "shipNet.input[23].useTangent", "TRUE" },
            { "shipNet.input[23].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[23].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "shipNet.input[24].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "shipNet.input[24].filterAdvance", "FALSE" },
            { "shipNet.input[24].filterBackward", "FALSE" },
            { "shipNet.input[24].filterForward", "TRUE" },
            { "shipNet.input[24].filterRetreat", "FALSE" },
            { "shipNet.input[24].forceType", "NEURAL_FORCE_SEPARATE" },
            { "shipNet.input[24].frequency", "0.000000" },
            { "shipNet.input[24].radius", "1086.721436" },
            { "shipNet.input[24].useTangent", "FALSE" },
            { "shipNet.input[24].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[2].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[2].forceType", "NEURAL_FORCE_CORNERS" },
            { "shipNet.input[2].frequency", "0.000000" },
            { "shipNet.input[2].radius", "148.709793" },
            { "shipNet.input[2].useTangent", "FALSE" },
            { "shipNet.input[2].valueType", "NEURAL_VALUE_MOBID" },
            { "shipNet.input[2].waveType", "NEURAL_WAVE_UNIT_SINE" },
            { "shipNet.input[3].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[3].filterAdvance", "TRUE" },
            { "shipNet.input[3].filterForward", "TRUE" },
            { "shipNet.input[3].filterRetreat", "TRUE" },
            { "shipNet.input[3].forceType", "NEURAL_FORCE_FARTHEST_CORNER" },
            { "shipNet.input[3].radius", "-0.950000" },
            { "shipNet.input[3].useTangent", "FALSE" },
            { "shipNet.input[3].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[4].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[4].filterBackward", "FALSE" },
            { "shipNet.input[4].filterForward", "TRUE" },
            { "shipNet.input[4].forceType", "NEURAL_FORCE_ENEMY_COHERE2" },
            { "shipNet.input[4].frequency", "5622.903809" },
            { "shipNet.input[4].radius", "0.000000" },
            { "shipNet.input[4].useTangent", "TRUE" },
            { "shipNet.input[4].valueType", "NEURAL_VALUE_TICK" },
            { "shipNet.input[4].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "shipNet.input[5].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[5].filterBackward", "FALSE" },
            { "shipNet.input[5].filterForward", "TRUE" },
            { "shipNet.input[5].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "shipNet.input[5].frequency", "0.000000" },
            { "shipNet.input[5].radius", "-1.000000" },
            { "shipNet.input[5].useTangent", "TRUE" },
            { "shipNet.input[5].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[5].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[6].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "shipNet.input[6].filterAdvance", "TRUE" },
            { "shipNet.input[6].filterBackward", "TRUE" },
            { "shipNet.input[6].filterForward", "FALSE" },
            { "shipNet.input[6].forceType", "NEURAL_FORCE_NEAREST_CORNER" },
            { "shipNet.input[6].frequency", "0.000000" },
            { "shipNet.input[6].radius", "-0.950000" },
            { "shipNet.input[6].useTangent", "FALSE" },
            { "shipNet.input[6].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[6].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "shipNet.input[7].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "shipNet.input[7].filterBackward", "TRUE" },
            { "shipNet.input[7].filterForward", "FALSE" },
            { "shipNet.input[7].filterRetreat", "FALSE" },
            { "shipNet.input[7].forceType", "NEURAL_FORCE_ALIGN" },
            { "shipNet.input[7].radius", "4.193549" },
            { "shipNet.input[7].useTangent", "FALSE" },
            { "shipNet.input[7].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[8].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[8].forceType", "NEURAL_FORCE_EDGES" },
            { "shipNet.input[8].frequency", "0.000000" },
            { "shipNet.input[8].radius", "0.000000" },
            { "shipNet.input[8].useTangent", "FALSE" },
            { "shipNet.input[8].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[8].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "shipNet.input[9].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "shipNet.input[9].filterAdvance", "TRUE" },
            { "shipNet.input[9].filterForward", "FALSE" },
            { "shipNet.input[9].filterRetreat", "FALSE" },
            { "shipNet.input[9].forceType", "NEURAL_FORCE_CENTER" },
            { "shipNet.input[9].frequency", "0.000000" },
            { "shipNet.input[9].radius", "154.979477" },
            { "shipNet.input[9].useTangent", "FALSE" },
            { "shipNet.input[9].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[9].waveType", "NEURAL_WAVE_UNIT_SINE" },
            { "shipNet.node[100].inputs", "{31, 31, 71, 51, 65, 86, 73, 31, }" },
            { "shipNet.node[100].op", "ML_FOP_0x1_CONSTANT" },
            { "shipNet.node[100].params", "{}" },
            { "shipNet.node[101].inputs", "{0, 69, 87, 79, 88, 46, }" },
            { "shipNet.node[101].op", "ML_FOP_3x2_IF_INSIDE_RANGE_ELSE_CONST" },
            { "shipNet.node[101].params", "{}" },
            { "shipNet.node[102].inputs", "{24, 53, 101, 0, 37, 74, }" },
            { "shipNet.node[102].op", "ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_STEP" },
            { "shipNet.node[102].params", "{1.626612, 0.995004, 2.244309, 17922.958984, }" },
            { "shipNet.node[103].inputs", "{62, 33, 14, }" },
            { "shipNet.node[103].op", "ML_FOP_1x0_FLOOR" },
            { "shipNet.node[103].params", "{1993.878906, 11.889168, 0.102949, 1.000000, }" },
            { "shipNet.node[104].inputs", "{0, }" },
            { "shipNet.node[104].op", "ML_FOP_Nx1_ACTIVATE_THRESHOLD_DOWN" },
            { "shipNet.node[104].params", "{0.543301, 1293.997192, 1.978794, 989.214783, 493.272369, }" },
            { "shipNet.node[105].inputs", "{104, 84, 6, 16, 91, 41, 10, }" },
            { "shipNet.node[105].op", "ML_FOP_NxN_SELECT_LTE" },
            { "shipNet.node[105].params", "{1.000000, 0.000000, }" },
            { "shipNet.node[106].inputs", "{}" },
            { "shipNet.node[106].op", "ML_FOP_Nx0_ARITHMETIC_MEAN" },
            { "shipNet.node[106].params", "{987.682922, 0.000000, }" },
            { "shipNet.node[107].inputs", "{11, 61, 48, 48, 72, 86, }" },
            { "shipNet.node[107].op", "ML_FOP_Nx1_ACTIVATE_DIV_SUM" },
            { "shipNet.node[107].params", "{1.050000, 712.670227, 0.000000, 0.000000, }" },
            { "shipNet.node[108].inputs", "{33, 45, }" },
            { "shipNet.node[108].op", "ML_FOP_1x2_CLAMP" },
            { "shipNet.node[108].params", "{0.181056, 1.520744, 0.102670, 0.000000, }" },
            { "shipNet.node[109].inputs", "{17, }" },
            { "shipNet.node[109].op", "ML_FOP_3x0_POW" },
            { "shipNet.node[109].params", "{0.000000, 1.000000, 1370.204590, }" },
            { "shipNet.node[110].inputs", "{58, 66, 43, 5, 64, 32, 96, }" },
            { "shipNet.node[110].op", "ML_FOP_NxN_SELECT_LTE" },
            { "shipNet.node[110].params", "{1.000000, 0.000000, 1.489290, 0.000000, }" },
            { "shipNet.node[111].inputs", "{48, 77, 73, 52, 97, 0, 4, 18, }" },
            { "shipNet.node[111].op", "ML_FOP_NxN_SCALED_DIV_SUM_SQUARED" },
            { "shipNet.node[111].params", "{}" },
            { "shipNet.node[112].inputs", "{48, 63, 75, 44, 17, 42, }" },
            { "shipNet.node[112].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "shipNet.node[112].params", "{0.116223, 1.000000, 1372.458252, }" },
            { "shipNet.node[113].inputs", "{64, 22, 57, 78, 62, 94, 95, }" },
            { "shipNet.node[113].op", "ML_FOP_Nx0_ACTIVATE_DIV_SUM" },
            { "shipNet.node[113].params", "{143.244186, 0.000000, 1.482340, 0.513569, 19.756893, 0.000000, }" },
            { "shipNet.node[114].inputs", "{1, }" },
            { "shipNet.node[114].op", "ML_FOP_1x3_SQRT" },
            { "shipNet.node[114].params", "{0.000000, }" },
            { "shipNet.node[115].inputs", "{91, 62, 72, 106, 110, 37, }" },
            { "shipNet.node[115].op", "ML_FOP_0x1_UNIT_CONSTANT" },
            { "shipNet.node[115].params", "{0.099994, 0.000000, 0.000000, 0.000000, 1.596393, 0.000000, 0.203419, 0.000000, }" },
            { "shipNet.node[116].inputs", "{}" },
            { "shipNet.node[116].op", "ML_FOP_1x2_COSINE" },
            { "shipNet.node[116].params", "{0.000000, }" },
            { "shipNet.node[117].inputs", "{70, 90, 79, 72, 62, }" },
            { "shipNet.node[117].op", "ML_FOP_0x1_CONSTANT_N10_0" },
            { "shipNet.node[117].params", "{}" },
            { "shipNet.node[118].inputs", "{1, 45, 76, }" },
            { "shipNet.node[118].op", "ML_FOP_1x0_CEIL" },
            { "shipNet.node[118].params", "{0.000000, 1273.960815, 177.286774, 0.000000, 0.840318, 0.603231, }" },
            { "shipNet.node[119].inputs", "{13, 97, 111, 51, 30, 100, 96, 66, }" },
            { "shipNet.node[119].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "shipNet.node[119].params", "{0.000000, 1.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[120].inputs", "{117, 93, 76, 47, 93, 36, }" },
            { "shipNet.node[120].op", "ML_FOP_1x0_ARC_TANGENT" },
            { "shipNet.node[120].params", "{1047.382690, 0.517285, 0.000000, 0.000000, }" },
            { "shipNet.node[121].inputs", "{4, 8, 42, 114, }" },
            { "shipNet.node[121].op", "ML_FOP_3x0_IF_GTEZ_ELSE" },
            { "shipNet.node[121].params", "{}" },
            { "shipNet.node[122].inputs", "{98, 88, 13, 54, 93, }" },
            { "shipNet.node[122].op", "ML_FOP_1x0_UNIT_SINE" },
            { "shipNet.node[122].params", "{0.000000, 150.159256, 0.000000, }" },
            { "shipNet.node[123].inputs", "{44, 17, 55, 96, }" },
            { "shipNet.node[123].op", "ML_FOP_1x1_LINEAR_COMBINATION" },
            { "shipNet.node[123].params", "{0.184680, 0.000000, 0.000000, 155.073166, 981.998840, 0.000000, 0.000000, }" },
            { "shipNet.node[124].inputs", "{11, 2, 87, 19, }" },
            { "shipNet.node[124].op", "ML_FOP_1x0_UNIT_SINE" },
            { "shipNet.node[124].params", "{0.000000, 0.000000, 0.000000, 0.000000, 1.000000, 0.000000, 1.000000, }" },
            { "shipNet.node[25].inputs", "{5, 11, 15, 8, 14, 22, }" },
            { "shipNet.node[25].op", "ML_FOP_0x1_CONSTANT_0_100" },
            { "shipNet.node[25].params", "{0.000000, 1.000000, }" },
            { "shipNet.node[26].inputs", "{7, 9, 10, }" },
            { "shipNet.node[26].op", "ML_FOP_0x1_CONSTANT" },
            { "shipNet.node[26].params", "{0.000000, 0.000000, }" },
            { "shipNet.node[27].inputs", "{}" },
            { "shipNet.node[27].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "shipNet.node[27].params", "{1.535408, }" },
            { "shipNet.node[28].inputs", "{15, 20, 9, 16, 25, 20, 24, }" },
            { "shipNet.node[28].op", "ML_FOP_Nx2_ACTIVATE_QUADRATIC_UP" },
            { "shipNet.node[28].params", "{0.642530, 7505.147461, 868.092957, 1.000000, 0.508775, 1.000000, 0.094623, 0.000000, }" },
            { "shipNet.node[29].inputs", "{27, 16, 23, }" },
            { "shipNet.node[29].op", "ML_FOP_1x1_GTE" },
            { "shipNet.node[29].params", "{0.090702, 0.000000, 0.000000, 9535.729492, 0.000000, 0.000000, 239.734283, }" },
            { "shipNet.node[30].inputs", "{}" },
            { "shipNet.node[30].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "shipNet.node[30].params", "{0.000000, 1149.891479, 0.000000, 1761.745850, 0.000000, }" },
            { "shipNet.node[31].inputs", "{11, 7, 24, 14, 6, 21, 1, }" },
            { "shipNet.node[31].op", "ML_FOP_1x0_CLAMP_UNIT" },
            { "shipNet.node[31].params", "{1.484436, 2056.047607, 0.000000, 145.182266, 0.000000, 1.000000, }" },
            { "shipNet.node[32].inputs", "{12, 18, 31, }" },
            { "shipNet.node[32].op", "ML_FOP_Nx0_ACTIVATE_SQRT_UP" },
            { "shipNet.node[32].params", "{8.834229, 1.000000, 1540.079956, 1.000000, 0.000000, 0.769500, 0.000000, 0.000000, }" },
            { "shipNet.node[33].inputs", "{22, 29, 21, 2, 7, 32, 27, 21, }" },
            { "shipNet.node[33].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "shipNet.node[33].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 6.296649, 1.000000, 0.000000, }" },
            { "shipNet.node[34].inputs", "{7, 14, 29, 10, 30, }" },
            { "shipNet.node[34].op", "ML_FOP_0x1_CONSTANT_N1K_0" },
            { "shipNet.node[34].params", "{0.069093, 1.000000, 0.000000, 1060.549072, 0.097707, }" },
            { "shipNet.node[35].inputs", "{17, 33, 18, 16, }" },
            { "shipNet.node[35].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "shipNet.node[35].params", "{1123.579956, 0.000000, 0.425565, 1.050000, 9747.679688, 0.000000, 1.002265, 10237.089844, }" },
            { "shipNet.node[36].inputs", "{14, 1, 27, 32, }" },
            { "shipNet.node[36].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP" },
            { "shipNet.node[36].params", "{4.920677, }" },
            { "shipNet.node[37].inputs", "{26, }" },
            { "shipNet.node[37].op", "ML_FOP_Nx0_ACTIVATE_DIV_SUM" },
            { "shipNet.node[37].params", "{1019.083862, 0.340067, }" },
            { "shipNet.node[38].inputs", "{}" },
            { "shipNet.node[38].op", "ML_FOP_Nx0_MAX" },
            { "shipNet.node[38].params", "{0.950000, 0.182452, }" },
            { "shipNet.node[39].inputs", "{33, 2, 14, }" },
            { "shipNet.node[39].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "shipNet.node[39].params", "{27.074999, 2.256364, 0.895679, 24.541735, 1.000000, 0.291301, 2500.483643, }" },
            { "shipNet.node[40].inputs", "{27, 9, 19, 19, 3, 15, 21, }" },
            { "shipNet.node[40].op", "ML_FOP_0x0_ZERO" },
            { "shipNet.node[40].params", "{0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[41].inputs", "{}" },
            { "shipNet.node[41].op", "ML_FOP_Nx0_MAX" },
            { "shipNet.node[41].params", "{0.000000, 5.276439, 1.485161, 0.000000, }" },
            { "shipNet.node[42].inputs", "{16, }" },
            { "shipNet.node[42].op", "ML_FOP_1x3_ARC_TANGENT" },
            { "shipNet.node[42].params", "{163.507523, }" },
            { "shipNet.node[43].inputs", "{}" },
            { "shipNet.node[43].op", "ML_FOP_1x0_TAN" },
            { "shipNet.node[43].params", "{1777.616699, }" },
            { "shipNet.node[44].inputs", "{28, }" },
            { "shipNet.node[44].op", "ML_FOP_5x0_IF_OUTSIDE_RANGE_ELSE" },
            { "shipNet.node[44].params", "{29.554201, }" },
            { "shipNet.node[45].inputs", "{12, }" },
            { "shipNet.node[45].op", "ML_FOP_2x0_POW" },
            { "shipNet.node[45].params", "{0.000000, 0.000000, 243.633469, 584.325378, 0.090686, 2.019671, 0.000000, }" },
            { "shipNet.node[46].inputs", "{21, 28, 18, 4, 37, }" },
            { "shipNet.node[46].op", "ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST" },
            { "shipNet.node[46].params", "{40.355164, 156.498688, 0.000000, }" },
            { "shipNet.node[47].inputs", "{41, }" },
            { "shipNet.node[47].op", "ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP" },
            { "shipNet.node[47].params", "{0.000000, 1.000000, }" },
            { "shipNet.node[48].inputs", "{}" },
            { "shipNet.node[48].op", "ML_FOP_NxN_WEIGHTED_ARITHMETIC_MEAN" },
            { "shipNet.node[48].params", "{1.586385, 0.000000, 0.000000, 1759.740967, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[49].inputs", "{20, }" },
            { "shipNet.node[49].op", "ML_FOP_1x1_LINEAR_DOWN" },
            { "shipNet.node[49].params", "{1136.950073, 0.000000, 0.000000, 1.490049, }" },
            { "shipNet.node[50].inputs", "{22, 2, 4, 21, 19, 12, 20, }" },
            { "shipNet.node[50].op", "ML_FOP_Nx2_ACTIVATE_LINEAR_DOWN" },
            { "shipNet.node[50].params", "{0.095780, 0.000000, 0.499307, 230.770187, 1.770261, 0.590019, 0.000000, }" },
            { "shipNet.node[51].inputs", "{32, }" },
            { "shipNet.node[51].op", "ML_FOP_1x0_ARC_COSINE" },
            { "shipNet.node[51].params", "{1.034716, 0.997500, 0.000000, }" },
            { "shipNet.node[52].inputs", "{8, }" },
            { "shipNet.node[52].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "shipNet.node[52].params", "{3723.401611, 260.550598, 0.000000, }" },
            { "shipNet.node[53].inputs", "{31, 9, 41, 29, }" },
            { "shipNet.node[53].op", "ML_FOP_Nx1_ACTIVATE_THRESHOLD_UP" },
            { "shipNet.node[53].params", "{31.251745, 0.950000, 1.000000, 0.499292, 21.777594, 2950.105957, 1.000000, }" },
            { "shipNet.node[54].inputs", "{23, 38, }" },
            { "shipNet.node[54].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "shipNet.node[54].params", "{0.000000, 0.000000, 1013.443298, 0.201231, 1.155000, 0.000000, 0.000000, }" },
            { "shipNet.node[55].inputs", "{25, 53, 48, 28, 27, 12, }" },
            { "shipNet.node[55].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_PROB_INVERSE" },
            { "shipNet.node[55].params", "{1.000000, 0.000000, 0.134443, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[56].inputs", "{1, }" },
            { "shipNet.node[56].op", "ML_FOP_1x2_POW" },
            { "shipNet.node[56].params", "{1.000000, 1.000000, 1880.100464, }" },
            { "shipNet.node[57].inputs", "{53, 19, 16, 29, 48, 27, }" },
            { "shipNet.node[57].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP" },
            { "shipNet.node[57].params", "{911.902893, 0.000000, 1.000000, }" },
            { "shipNet.node[58].inputs", "{19, 17, 46, 51, 35, 16, 46, 23, }" },
            { "shipNet.node[58].op", "ML_FOP_1x1_PRODUCT" },
            { "shipNet.node[58].params", "{1.000000, 2.444058, 0.945000, 0.950000, }" },
            { "shipNet.node[59].inputs", "{38, 56, 41, 39, 9, 7, }" },
            { "shipNet.node[59].op", "ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP" },
            { "shipNet.node[59].params", "{0.000000, 0.216434, 1.774954, 0.000000, 974.963074, 1.000000, 0.000000, }" },
            { "shipNet.node[60].inputs", "{9, 28, 42, 56, 39, }" },
            { "shipNet.node[60].op", "ML_FOP_1x2_CLAMP" },
            { "shipNet.node[60].params", "{290.582825, 1049.949585, }" },
            { "shipNet.node[61].inputs", "{33, }" },
            { "shipNet.node[61].op", "ML_FOP_1x0_HYP_COSINE" },
            { "shipNet.node[61].params", "{4283.847168, 0.094453, 1.000000, 1065.806641, 2322.148682, }" },
            { "shipNet.node[62].inputs", "{14, 43, }" },
            { "shipNet.node[62].op", "ML_FOP_0x1_CONSTANT" },
            { "shipNet.node[62].params", "{0.590880, 0.950000, 0.000000, 150.629379, 1.557601, 265.239502, 0.000000, }" },
            { "shipNet.node[63].inputs", "{54, 46, }" },
            { "shipNet.node[63].op", "ML_FOP_1x0_PROB_NOT" },
            { "shipNet.node[63].params", "{}" },
            { "shipNet.node[64].inputs", "{}" },
            { "shipNet.node[64].op", "ML_FOP_NxN_LINEAR_COMBINATION" },
            { "shipNet.node[64].params", "{0.122032, }" },
            { "shipNet.node[65].inputs", "{29, 31, 33, 11, 2, 20, 36, }" },
            { "shipNet.node[65].op", "ML_FOP_NxN_WEIGHTED_GEOMETRIC_MEAN" },
            { "shipNet.node[65].params", "{}" },
            { "shipNet.node[66].inputs", "{}" },
            { "shipNet.node[66].op", "ML_FOP_0x1_CONSTANT_N100_100" },
            { "shipNet.node[66].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[67].inputs", "{}" },
            { "shipNet.node[67].op", "ML_FOP_1x3_HYP_SINE" },
            { "shipNet.node[67].params", "{}" },
            { "shipNet.node[68].inputs", "{10, 30, 14, 0, 31, }" },
            { "shipNet.node[68].op", "ML_FOP_1x0_ARC_COSINE" },
            { "shipNet.node[68].params", "{0.610892, 1968.812378, }" },
            { "shipNet.node[69].inputs", "{56, 15, 59, 24, 8, 50, 44, }" },
            { "shipNet.node[69].op", "ML_FOP_0x3_CONSTANT_CLAMPED" },
            { "shipNet.node[69].params", "{0.000000, 2388.428223, }" },
            { "shipNet.node[70].inputs", "{}" },
            { "shipNet.node[70].op", "ML_FOP_1x3_SQRT" },
            { "shipNet.node[70].params", "{13.927458, 150.720612, }" },
            { "shipNet.node[71].inputs", "{60, 1, 70, }" },
            { "shipNet.node[71].op", "ML_FOP_Nx0_ACTIVATE_LN_DOWN" },
            { "shipNet.node[71].params", "{1.000000, 0.000000, 8.900069, 0.000000, 0.000000, }" },
            { "shipNet.node[72].inputs", "{69, }" },
            { "shipNet.node[72].op", "ML_FOP_1x0_HYP_SINE" },
            { "shipNet.node[72].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[73].inputs", "{9, 18, }" },
            { "shipNet.node[73].op", "ML_FOP_NxN_ANCHORED_DIV_SUM_SQUARED" },
            { "shipNet.node[73].params", "{0.000000, }" },
            { "shipNet.node[74].inputs", "{43, 38, 66, 27, 73, 28, }" },
            { "shipNet.node[74].op", "ML_FOP_Nx1_ACTIVATE_THRESHOLD_UP" },
            { "shipNet.node[74].params", "{0.000000, 1113.128052, 1086.244385, 0.000000, }" },
            { "shipNet.node[75].inputs", "{}" },
            { "shipNet.node[75].op", "ML_FOP_4x0_IF_GTE_ELSE" },
            { "shipNet.node[75].params", "{0.000000, 0.000000, 0.000000, 1.491193, 1.682133, }" },
            { "shipNet.node[76].inputs", "{66, 21, 15, 32, }" },
            { "shipNet.node[76].op", "ML_FOP_1x0_SQRT" },
            { "shipNet.node[76].params", "{0.791190, 0.000000, 0.000000, }" },
            { "shipNet.node[77].inputs", "{24, 18, 12, 38, 67, 57, }" },
            { "shipNet.node[77].op", "ML_FOP_NxN_LINEAR_COMBINATION" },
            { "shipNet.node[77].params", "{1.000000, 0.749966, 0.000000, }" },
            { "shipNet.node[78].inputs", "{77, }" },
            { "shipNet.node[78].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "shipNet.node[78].params", "{0.000000, 0.000000, }" },
            { "shipNet.node[79].inputs", "{30, 42, 42, 47, 10, 41, }" },
            { "shipNet.node[79].op", "ML_FOP_1x1_GTE" },
            { "shipNet.node[79].params", "{1.607831, 0.000000, }" },
            { "shipNet.node[80].inputs", "{60, 68, 62, 6, }" },
            { "shipNet.node[80].op", "ML_FOP_3x0_IF_GTEZ_ELSE" },
            { "shipNet.node[80].params", "{0.000000, 2.473642, 0.000000, 1.000000, 0.508492, 1391.570557, }" },
            { "shipNet.node[81].inputs", "{1, 32, 4, }" },
            { "shipNet.node[81].op", "ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST" },
            { "shipNet.node[81].params", "{1.611180, 2.407777, }" },
            { "shipNet.node[82].inputs", "{31, }" },
            { "shipNet.node[82].op", "ML_FOP_1x3_HYP_TANGENT" },
            { "shipNet.node[82].params", "{386.799255, 0.789887, }" },
            { "shipNet.node[83].inputs", "{67, 72, 58, 61, }" },
            { "shipNet.node[83].op", "ML_FOP_1x2_SEEDED_RANDOM" },
            { "shipNet.node[83].params", "{0.000000, 0.000000, }" },
            { "shipNet.node[84].inputs", "{16, 11, 4, }" },
            { "shipNet.node[84].op", "ML_FOP_1x0_HYP_COSINE" },
            { "shipNet.node[84].params", "{}" },
            { "shipNet.node[85].inputs", "{2, 4, 10, 25, 66, 2, 18, 75, }" },
            { "shipNet.node[85].op", "ML_FOP_4x0_IF_LTE_ELSE" },
            { "shipNet.node[85].params", "{}" },
            { "shipNet.node[86].inputs", "{60, 30, }" },
            { "shipNet.node[86].op", "ML_FOP_5x0_IF_INSIDE_RANGE_ELSE" },
            { "shipNet.node[86].params", "{0.000000, 0.108999, 24.561216, 766.009766, }" },
            { "shipNet.node[87].inputs", "{41, 48, 74, 39, 37, 13, }" },
            { "shipNet.node[87].op", "ML_FOP_NxN_ACTIVATE_POLYNOMIAL" },
            { "shipNet.node[87].params", "{0.000000, 0.000000, 154.382095, 29.470863, 0.000000, 283.685974, 0.000000, }" },
            { "shipNet.node[88].inputs", "{50, 33, 13, 80, 65, 32, 32, }" },
            { "shipNet.node[88].op", "ML_FOP_0x1_CONSTANT_N1K_0" },
            { "shipNet.node[88].params", "{}" },
            { "shipNet.node[89].inputs", "{0, 60, 45, 78, }" },
            { "shipNet.node[89].op", "ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT" },
            { "shipNet.node[89].params", "{0.091052, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[90].inputs", "{21, 55, 37, 57, 8, 59, 70, 59, }" },
            { "shipNet.node[90].op", "ML_FOP_Nx0_ACTIVATE_LN_DOWN" },
            { "shipNet.node[90].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[91].inputs", "{40, 38, 90, 53, 21, 17, 15, 32, }" },
            { "shipNet.node[91].op", "ML_FOP_1x3_HYP_SINE" },
            { "shipNet.node[91].params", "{757.178406, }" },
            { "shipNet.node[92].inputs", "{55, 0, 87, 66, 47, 78, }" },
            { "shipNet.node[92].op", "ML_FOP_1x0_SIN" },
            { "shipNet.node[92].params", "{219.802856, 0.194203, 0.000000, 0.159984, 0.000000, }" },
            { "shipNet.node[93].inputs", "{}" },
            { "shipNet.node[93].op", "ML_FOP_4x0_IF_LTE_ELSE" },
            { "shipNet.node[93].params", "{0.997500, 0.895488, 411.156036, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[94].inputs", "{78, 36, 72, 69, }" },
            { "shipNet.node[94].op", "ML_FOP_1x1_FMOD" },
            { "shipNet.node[94].params", "{0.097396, 2053.246582, 27.440088, }" },
            { "shipNet.node[95].inputs", "{39, 79, 31, 48, 58, 91, }" },
            { "shipNet.node[95].op", "ML_FOP_1x0_HYP_TANGENT" },
            { "shipNet.node[95].params", "{2670.252441, 1.100000, 20000.000000, 425.691711, }" },
            { "shipNet.node[96].inputs", "{84, 24, 56, 50, }" },
            { "shipNet.node[96].op", "ML_FOP_1x0_IDENTITY" },
            { "shipNet.node[96].params", "{143.490097, 0.000000, 58.757553, 0.000000, }" },
            { "shipNet.node[97].inputs", "{66, 14, 57, 8, 21, 40, 2, 58, }" },
            { "shipNet.node[97].op", "ML_FOP_3x0_POW" },
            { "shipNet.node[97].params", "{0.000000, 148.431381, 0.000000, }" },
            { "shipNet.node[98].inputs", "{43, 19, 6, 36, 3, 1, }" },
            { "shipNet.node[98].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "shipNet.node[98].params", "{0.099385, 1.000000, 0.098355, 2142.184082, 0.000000, 0.503039, 0.000000, 0.000000, }" },
            { "shipNet.node[99].inputs", "{32, 41, 23, 2, 2, 26, 42, }" },
            { "shipNet.node[99].op", "ML_FOP_0x1_CONSTANT_N10_10" },
            { "shipNet.node[99].params", "{1871.464966, 0.000000, 0.000000, 30.000000, }" },
            { "shipNet.numInputs", "25" },
            { "shipNet.numNodes", "100" },
            { "shipNet.numOutputs", "25" },
            { "shipNet.output[100].filterAdvance", "TRUE" },
            { "shipNet.output[100].filterForward", "FALSE" },
            { "shipNet.output[100].filterRetreat", "FALSE" },
            { "shipNet.output[100].forceType", "NEURAL_FORCE_BASE_FARTHEST_FRIEND" },
            { "shipNet.output[100].radius", "638.955017" },
            { "shipNet.output[100].useTangent", "FALSE" },
            { "shipNet.output[100].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[101].filterAdvance", "FALSE" },
            { "shipNet.output[101].filterBackward", "TRUE" },
            { "shipNet.output[101].filterForward", "TRUE" },
            { "shipNet.output[101].filterRetreat", "FALSE" },
            { "shipNet.output[101].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "shipNet.output[101].radius", "2893.590332" },
            { "shipNet.output[101].useTangent", "TRUE" },
            { "shipNet.output[101].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[102].filterAdvance", "FALSE" },
            { "shipNet.output[102].filterBackward", "FALSE" },
            { "shipNet.output[102].filterForward", "FALSE" },
            { "shipNet.output[102].forceType", "NEURAL_FORCE_SEPARATE" },
            { "shipNet.output[102].radius", "322.205200" },
            { "shipNet.output[102].useTangent", "FALSE" },
            { "shipNet.output[102].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[103].filterAdvance", "FALSE" },
            { "shipNet.output[103].filterBackward", "TRUE" },
            { "shipNet.output[103].filterForward", "FALSE" },
            { "shipNet.output[103].filterRetreat", "TRUE" },
            { "shipNet.output[103].forceType", "NEURAL_FORCE_ENEMY" },
            { "shipNet.output[103].radius", "-0.950000" },
            { "shipNet.output[103].useTangent", "TRUE" },
            { "shipNet.output[103].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[104].filterAdvance", "FALSE" },
            { "shipNet.output[104].filterRetreat", "FALSE" },
            { "shipNet.output[104].forceType", "NEURAL_FORCE_COHERE" },
            { "shipNet.output[104].radius", "1729.684937" },
            { "shipNet.output[104].useTangent", "TRUE" },
            { "shipNet.output[104].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[105].filterAdvance", "TRUE" },
            { "shipNet.output[105].filterForward", "TRUE" },
            { "shipNet.output[105].forceType", "NEURAL_FORCE_SEPARATE" },
            { "shipNet.output[105].radius", "119.356041" },
            { "shipNet.output[105].useTangent", "FALSE" },
            { "shipNet.output[105].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[106].filterAdvance", "TRUE" },
            { "shipNet.output[106].filterBackward", "FALSE" },
            { "shipNet.output[106].filterForward", "FALSE" },
            { "shipNet.output[106].filterRetreat", "TRUE" },
            { "shipNet.output[106].forceType", "NEURAL_FORCE_BASE_CONTROL_LIMIT" },
            { "shipNet.output[106].radius", "2541.187500" },
            { "shipNet.output[106].useTangent", "TRUE" },
            { "shipNet.output[106].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[107].filterAdvance", "FALSE" },
            { "shipNet.output[107].filterBackward", "TRUE" },
            { "shipNet.output[107].filterForward", "TRUE" },
            { "shipNet.output[107].filterRetreat", "FALSE" },
            { "shipNet.output[107].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "shipNet.output[107].radius", "151.735733" },
            { "shipNet.output[107].useTangent", "TRUE" },
            { "shipNet.output[107].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[108].filterAdvance", "FALSE" },
            { "shipNet.output[108].filterBackward", "TRUE" },
            { "shipNet.output[108].filterRetreat", "TRUE" },
            { "shipNet.output[108].forceType", "NEURAL_FORCE_BACKWARD_COHERE" },
            { "shipNet.output[108].radius", "-0.950000" },
            { "shipNet.output[108].useTangent", "TRUE" },
            { "shipNet.output[108].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[109].filterBackward", "FALSE" },
            { "shipNet.output[109].filterForward", "FALSE" },
            { "shipNet.output[109].filterRetreat", "TRUE" },
            { "shipNet.output[109].forceType", "NEURAL_FORCE_NEAREST_EDGE" },
            { "shipNet.output[109].radius", "1562.723389" },
            { "shipNet.output[109].useTangent", "FALSE" },
            { "shipNet.output[109].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[110].filterAdvance", "FALSE" },
            { "shipNet.output[110].filterBackward", "FALSE" },
            { "shipNet.output[110].filterForward", "TRUE" },
            { "shipNet.output[110].filterRetreat", "FALSE" },
            { "shipNet.output[110].forceType", "NEURAL_FORCE_RETREAT_ALIGN" },
            { "shipNet.output[110].radius", "2720.424316" },
            { "shipNet.output[110].useTangent", "TRUE" },
            { "shipNet.output[110].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[111].filterAdvance", "FALSE" },
            { "shipNet.output[111].filterForward", "TRUE" },
            { "shipNet.output[111].filterRetreat", "TRUE" },
            { "shipNet.output[111].forceType", "NEURAL_FORCE_ENEMY" },
            { "shipNet.output[111].radius", "151.967468" },
            { "shipNet.output[111].useTangent", "TRUE" },
            { "shipNet.output[111].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[112].filterAdvance", "TRUE" },
            { "shipNet.output[112].filterForward", "TRUE" },
            { "shipNet.output[112].filterRetreat", "TRUE" },
            { "shipNet.output[112].forceType", "NEURAL_FORCE_FORWARD_ENEMY_ALIGN" },
            { "shipNet.output[112].radius", "296.518951" },
            { "shipNet.output[112].useTangent", "FALSE" },
            { "shipNet.output[112].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[113].filterAdvance", "FALSE" },
            { "shipNet.output[113].filterForward", "TRUE" },
            { "shipNet.output[113].filterRetreat", "FALSE" },
            { "shipNet.output[113].forceType", "NEURAL_FORCE_ALIGN" },
            { "shipNet.output[113].radius", "-1.000000" },
            { "shipNet.output[113].useTangent", "FALSE" },
            { "shipNet.output[113].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[114].filterBackward", "TRUE" },
            { "shipNet.output[114].filterForward", "FALSE" },
            { "shipNet.output[114].filterRetreat", "TRUE" },
            { "shipNet.output[114].forceType", "NEURAL_FORCE_ENEMY_ALIGN" },
            { "shipNet.output[114].radius", "338.400024" },
            { "shipNet.output[114].useTangent", "FALSE" },
            { "shipNet.output[114].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[115].filterAdvance", "FALSE" },
            { "shipNet.output[115].filterBackward", "FALSE" },
            { "shipNet.output[115].forceType", "NEURAL_FORCE_FARTHEST_EDGE" },
            { "shipNet.output[115].radius", "-1.000000" },
            { "shipNet.output[115].useTangent", "FALSE" },
            { "shipNet.output[115].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[116].filterAdvance", "TRUE" },
            { "shipNet.output[116].filterBackward", "TRUE" },
            { "shipNet.output[116].filterForward", "TRUE" },
            { "shipNet.output[116].forceType", "NEURAL_FORCE_FORWARD_ALIGN" },
            { "shipNet.output[116].radius", "0.000000" },
            { "shipNet.output[116].useTangent", "TRUE" },
            { "shipNet.output[116].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[117].filterAdvance", "TRUE" },
            { "shipNet.output[117].filterBackward", "TRUE" },
            { "shipNet.output[117].filterForward", "TRUE" },
            { "shipNet.output[117].filterRetreat", "TRUE" },
            { "shipNet.output[117].forceType", "NEURAL_FORCE_ADVANCE_SEPARATE" },
            { "shipNet.output[117].radius", "1052.135864" },
            { "shipNet.output[117].useTangent", "FALSE" },
            { "shipNet.output[117].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[118].filterForward", "FALSE" },
            { "shipNet.output[118].filterRetreat", "FALSE" },
            { "shipNet.output[118].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "shipNet.output[118].radius", "313.822601" },
            { "shipNet.output[118].useTangent", "FALSE" },
            { "shipNet.output[118].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[119].filterRetreat", "FALSE" },
            { "shipNet.output[119].forceType", "NEURAL_FORCE_ALIGN" },
            { "shipNet.output[119].radius", "2850.000000" },
            { "shipNet.output[119].useTangent", "FALSE" },
            { "shipNet.output[119].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[120].filterAdvance", "FALSE" },
            { "shipNet.output[120].filterBackward", "TRUE" },
            { "shipNet.output[120].filterForward", "TRUE" },
            { "shipNet.output[120].filterRetreat", "FALSE" },
            { "shipNet.output[120].forceType", "NEURAL_FORCE_MIDWAY_GUESS_LAX" },
            { "shipNet.output[120].radius", "2963.549072" },
            { "shipNet.output[120].useTangent", "FALSE" },
            { "shipNet.output[120].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[121].filterAdvance", "TRUE" },
            { "shipNet.output[121].filterBackward", "TRUE" },
            { "shipNet.output[121].filterForward", "TRUE" },
            { "shipNet.output[121].filterRetreat", "TRUE" },
            { "shipNet.output[121].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "shipNet.output[121].radius", "147.077377" },
            { "shipNet.output[121].useTangent", "TRUE" },
            { "shipNet.output[121].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[122].filterAdvance", "FALSE" },
            { "shipNet.output[122].filterBackward", "TRUE" },
            { "shipNet.output[122].filterForward", "TRUE" },
            { "shipNet.output[122].filterRetreat", "TRUE" },
            { "shipNet.output[122].forceType", "NEURAL_FORCE_ADVANCE_ALIGN" },
            { "shipNet.output[122].radius", "1778.265625" },
            { "shipNet.output[122].useTangent", "TRUE" },
            { "shipNet.output[122].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[123].filterAdvance", "TRUE" },
            { "shipNet.output[123].filterBackward", "TRUE" },
            { "shipNet.output[123].filterForward", "FALSE" },
            { "shipNet.output[123].filterRetreat", "TRUE" },
            { "shipNet.output[123].forceType", "NEURAL_FORCE_BASE_CONTROL_SHELL" },
            { "shipNet.output[123].radius", "1030.571045" },
            { "shipNet.output[123].useTangent", "FALSE" },
            { "shipNet.output[123].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[124].filterAdvance", "FALSE" },
            { "shipNet.output[124].forceType", "NEURAL_FORCE_BASE_CONTROL_SHELL" },
            { "shipNet.output[124].radius", "-1.000000" },
            { "shipNet.output[124].useTangent", "FALSE" },
            { "shipNet.output[124].valueType", "NEURAL_VALUE_FORCE" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "30.651253" },
            { "guardRange", "112.419868" },
            { "rotateStartingAngle", "FALSE" },
            { "sensorGrid.staleCoreTime", "0.000000" },
            { "sensorGrid.staleFighterTime", "0.000000" },
            { "startingMaxRadius", "1434.236328" },
            { "startingMinRadius", "391.281525" },
        };
        //BineuralConfigValue configs2[] = {
        //    { "void", "void" },
        //};

        struct {
            BineuralConfigValue *values;
            uint numValues;
        } configs[] = {
            { defaults,  ARRAYSIZE(defaults), },
            { configs1,  ARRAYSIZE(configs1), },
            //{ configs2,  ARRAYSIZE(configs2), },
        };

        int bineuralIndex = aiType - FLEET_AI_BINEURAL1 + 1;
        VERIFY(aiType >= FLEET_AI_BINEURAL1);
        VERIFY(aiType <= FLEET_AI_BINEURAL1);
        VERIFY(bineuralIndex >= 1 && bineuralIndex < ARRAYSIZE(configs));

        int i = bineuralIndex;
        BineuralConfigValue *curConfig = configs[i].values;
        uint size = configs[i].numValues;
        for (uint k = 0; k < size; k++) {
            if (curConfig[k].value != NULL &&
                !MBRegistry_ContainsKey(mreg, curConfig[k].key)) {
                MBRegistry_PutConst(mreg, curConfig[k].key,
                                    curConfig[k].value);
            }
        }

        i = 0;
        curConfig = configs[i].values;
        size = configs[i].numValues;
        for (uint k = 0; k < size; k++) {
            if (curConfig[k].value != NULL &&
                !MBRegistry_ContainsKey(mreg, curConfig[k].key)) {
                MBRegistry_PutConst(mreg, curConfig[k].key,
                                    curConfig[k].value);
            }
        }

        /*
         * Don't add all the earlier configs by default.
         */

        // for (int i = bineuralIndex; i >= 0; i--) {
        //     BineuralConfigValue *curConfig = configs[i].values;
        //     uint size = configs[i].numValues;
        //     for (uint k = 0; k < size; k++) {
        //         if (curConfig[k].value != NULL &&
        //             !MBRegistry_ContainsKey(mreg, curConfig[k].key)) {
        //             MBRegistry_PutConst(mreg, curConfig[k].key,
        //                                 curConfig[k].value);
        //         }
        //     }
        // }
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        myShipNet.load(mreg, "shipNet.", NN_TYPE_FORCES);
        myFleetNet.load(mreg, "fleetNet.", NN_TYPE_SCALARS);
        myFleetNet.minimizeScalars(myShipNet);

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    AIContext *getAIContext(void) {
        ASSERT(myAIC.rs != NULL);
        ASSERT(myAIC.sg != NULL);
        ASSERT(myAIC.ai != NULL);
        return &myAIC;
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {
        BasicAIGovernor::doAttack(mob, enemyTarget);
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        BineuralShipAI *ship = (BineuralShipAI *)mob->aiMobHandle;
        ASSERT(ship == (BineuralShipAI *)getShip(mob->mobid));
        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        FRPoint rForce;
        myShipNet.doForces(mob, &rForce);
        NeuralForce_ApplyToMob(getAIContext(), mob, &rForce);

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }

    virtual void runTick() {
        myFleetNet.doScalars();
        myShipNet.pullScalars(myFleetNet);
        BasicAIGovernor::runTick();
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
    }
};

class BineuralFleet {
public:
    BineuralFleet(FleetAI *ai)
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

    ~BineuralFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    MappingSensorGrid sg;
    BineuralAIGovernor gov;
    MBRegistry *mreg;
};

static void *BineuralFleetCreate(FleetAI *ai);
static void BineuralFleetDestroy(void *aiHandle);
static void BineuralFleetRunAITick(void *aiHandle);
static void *BineuralFleetMobSpawned(void *aiHandle, Mob *m);
static void BineuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static void BineuralFleetMutate(FleetAIType aiType, MBRegistry *mreg);
static void BineuralFleetDumpSanitizedParams(void *aiHandle, MBRegistry *mreg);

void BineuralFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_BINEURAL1) {
        ops->aiName = "BineuralFleet1";
    } else {
        NOT_IMPLEMENTED();
    }

    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BineuralFleetCreate;
    ops->destroyFleet = &BineuralFleetDestroy;
    ops->runAITick = &BineuralFleetRunAITick;
    ops->mobSpawned = &BineuralFleetMobSpawned;
    ops->mobDestroyed = &BineuralFleetMobDestroyed;
    ops->mutateParams = &BineuralFleetMutate;
    ops->dumpSanitizedParams = &BineuralFleetDumpSanitizedParams;
}

static void BineuralFleetDumpSanitizedParams(void *aiHandle, MBRegistry *mreg)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;
    MBRegistry_PutAll(mreg, sf->mreg, "");
    sf->gov.dumpSanitizedParams(mreg);
}

static void BineuralFleetMutate(FleetAIType aiType, MBRegistry *mreg)
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

    float rate = 0.12;
    MBRegistry_PutCopy(mreg, BINEURAL_SCRAMBLE_KEY, "FALSE");
    if (Random_Flip(0.01)) {
        MBRegistry_PutCopy(mreg, BINEURAL_SCRAMBLE_KEY, "TRUE");

        for (uint i = 0; i < ARRAYSIZE(vf); i++) {
            vf[i].mutationRate = 1.0f;
            vf[i].jumpRate = 1.0f;
        }
        for (uint i = 0; i < ARRAYSIZE(vb); i++) {
            vb[i].flipRate = 0.5f;
        }
        rate = 1.0f;
    }

    NeuralNet_Mutate(mreg, "shipNet.", rate,
                     NN_TYPE_FORCES,
                     BINEURAL_MAX_INPUTS, BINEURAL_MAX_OUTPUTS,
                     BINEURAL_MAX_NODES, BINEURAL_MAX_NODE_DEGREE);
    NeuralNet_Mutate(mreg, "fleetNet.", rate,
                     NN_TYPE_SCALARS,
                     BINEURAL_MAX_INPUTS, BINEURAL_MAX_OUTPUTS,
                     BINEURAL_MAX_NODES, BINEURAL_MAX_NODE_DEGREE);

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));

    MBRegistry_Remove(mreg, BINEURAL_SCRAMBLE_KEY);
}

static void *BineuralFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new BineuralFleet(ai);
}

static void BineuralFleetDestroy(void *handle)
{
    BineuralFleet *sf = (BineuralFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *BineuralFleetMobSpawned(void *aiHandle, Mob *m)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return sf->gov.getShipHandle(m->mobid);
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void BineuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void BineuralFleetRunAITick(void *aiHandle)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;
    sf->gov.runTick();
}
