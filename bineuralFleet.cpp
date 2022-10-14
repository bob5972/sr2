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

#include "neuralNet.hpp"

#define BINEURAL_MAX_NODE_DEGREE  8
#define BINEURAL_MAX_INPUTS      25
#define BINEURAL_MAX_OUTPUTS     25
#define BINEURAL_MAX_NODES       100

#define BINEURAL_SCRAMBLE_KEY "bineuralFleet.scrambleMutation"

typedef struct BineuralConfigValue {
    const char *key;
    const char *value;
} BineuralConfigValue;

class BineuralAIGovernor : public BasicAIGovernor
{
public:
    // Members
    FloatNet myNeuralNet;
    MBVector<NeuralValueDesc> myInputDescs;
    MBVector<NeuralValueDesc> myOutputDescs;
    MBVector<float> myInputs;
    MBVector<float> myOutputs;
    uint myNumNodes;
    bool myUseAttackForces;
    NeuralNetContext myNNC;

public:
    BineuralAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        myNNC.rs = &myRandomState;
        myNNC.sg = sg;
        myNNC.ai = myFleetAI;
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

    void dumpSanitizedParams(MBRegistry *mreg) {
        myNeuralNet.save(mreg, "floatNet.");
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
            { "attackRange", "126.199165" },
            { "creditReserve", "0.000000" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "299.256897" },
            { "evadeStrictDistance", "114.395027" },
            { "evadeUseStrictDistance", "FALSE" },
            { "floatNet.node[100].inputs", "{84, }" },
            { "floatNet.node[100].op", "ML_FOP_Nx3_ACTIVATE_GAUSSIAN" },
            { "floatNet.node[100].params", "{0.000000, 1.050000, 0.276356, 1.000000, 3.157942, 0.000000, }" },
            { "floatNet.node[101].inputs", "{23, 7, 17, }" },
            { "floatNet.node[101].op", "ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST" },
            { "floatNet.node[101].params", "{0.000000, 3.143681, 0.000000, }" },
            { "floatNet.node[102].inputs", "{24, 42, 101, 0, 86, 6, 58, 37, }" },
            { "floatNet.node[102].op", "ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_STEP" },
            { "floatNet.node[102].params", "{1.549154, 0.000000, 2.486770, 17069.486328, 2.520596, }" },
            { "floatNet.node[103].inputs", "{97, 10, 10, }" },
            { "floatNet.node[103].op", "ML_FOP_1x0_FLOOR" },
            { "floatNet.node[103].params", "{1252.525146, 1577.410767, 30.000000, 0.513399, 1.000000, }" },
            { "floatNet.node[104].inputs", "{67, 57, }" },
            { "floatNet.node[104].op", "ML_FOP_5x0_IF_OUTSIDE_RANGE_ELSE" },
            { "floatNet.node[104].params", "{1.000000, 147.082001, 0.000000, 0.000000, 1.000000, 0.000000, }" },
            { "floatNet.node[105].inputs", "{9, 52, 32, 0, 101, }" },
            { "floatNet.node[105].op", "ML_FOP_0x0_ZERO" },
            { "floatNet.node[105].params", "{0.950000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[106].inputs", "{70, 35, }" },
            { "floatNet.node[106].op", "ML_FOP_1x3_IF_LTE_ELSE" },
            { "floatNet.node[106].params", "{}" },
            { "floatNet.node[107].inputs", "{44, 61, 30, 10, }" },
            { "floatNet.node[107].op", "ML_FOP_NxN_WEIGHTED_GEOMETRIC_MEAN" },
            { "floatNet.node[107].params", "{1.505585, 1191.031982, 647.882019, 0.046653, 1.021709, 0.000000, }" },
            { "floatNet.node[108].inputs", "{31, 57, 69, 60, 75, 84, 55, 88, }" },
            { "floatNet.node[108].op", "ML_FOP_2x0_SQUARE_SUM" },
            { "floatNet.node[108].params", "{0.000000, }" },
            { "floatNet.node[109].inputs", "{68, 29, 53, 1, 55, }" },
            { "floatNet.node[109].op", "ML_FOP_1x1_LINEAR_UP" },
            { "floatNet.node[109].params", "{21.620867, 0.000000, 2.713619, 0.108105, 0.000000, 1056.567627, 0.105662, }" },
            { "floatNet.node[110].inputs", "{}" },
            { "floatNet.node[110].op", "ML_FOP_Nx0_DIV_SUM" },
            { "floatNet.node[110].params", "{0.000000, 1.000000, 0.206780, 3.391139, }" },
            { "floatNet.node[111].inputs", "{2, 98, 32, 0, 101, 76, 109, 107, }" },
            { "floatNet.node[111].op", "ML_FOP_3x0_CLAMP" },
            { "floatNet.node[111].params", "{1.000000, 0.000000, 0.898641, 1018.451843, 0.000000, 1.000000, }" },
            { "floatNet.node[112].inputs", "{87, 107, 75, 37, }" },
            { "floatNet.node[112].op", "ML_FOP_1x4_IF_OUTSIDE_RANGE_ELSE" },
            { "floatNet.node[112].params", "{1520.729370, 0.110689, 0.000000, }" },
            { "floatNet.node[113].inputs", "{27, 22, 103, 100, 66, 28, 90, 14, }" },
            { "floatNet.node[113].op", "ML_FOP_Nx1_ACTIVATE_THRESHOLD_UP" },
            { "floatNet.node[113].params", "{1.000000, 19.482994, 1.482340, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[114].inputs", "{37, 94, 89, 113, 91, }" },
            { "floatNet.node[114].op", "ML_FOP_4x0_IF_GTE_ELSE" },
            { "floatNet.node[114].params", "{0.000000, 0.000000, 0.191884, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[115].inputs", "{72, 5, 21, 114, }" },
            { "floatNet.node[115].op", "ML_FOP_1x3_HYP_COSINE" },
            { "floatNet.node[115].params", "{0.000000, 0.000000, 0.106200, 0.000000, 0.234054, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[116].inputs", "{87, 34, }" },
            { "floatNet.node[116].op", "ML_FOP_3x0_IF_GTEZ_ELSE" },
            { "floatNet.node[116].params", "{17151.246094, 0.000000, 0.000000, 0.000000, 0.103045, 0.000000, }" },
            { "floatNet.node[117].inputs", "{5, 11, 84, 53, 35, 94, 64, 38, }" },
            { "floatNet.node[117].op", "ML_FOP_1x2_SINE" },
            { "floatNet.node[117].params", "{}" },
            { "floatNet.node[118].inputs", "{1, 70, 76, 63, }" },
            { "floatNet.node[118].op", "ML_FOP_1x0_CEIL" },
            { "floatNet.node[118].params", "{0.000000, 0.000000, 154.593857, 0.000000, 0.000000, 0.013984, 0.000000, 1.093649, }" },
            { "floatNet.node[119].inputs", "{13, 66, 46, 51, 19, 66, }" },
            { "floatNet.node[119].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "floatNet.node[119].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[120].inputs", "{113, 108, 78, 49, }" },
            { "floatNet.node[120].op", "ML_FOP_1x3_ARC_TANGENT" },
            { "floatNet.node[120].params", "{0.555243, 0.000000, 1036.331543, 0.497635, 1.308342, 1.000000, }" },
            { "floatNet.node[121].inputs", "{31, 109, 42, 118, }" },
            { "floatNet.node[121].op", "ML_FOP_4x0_IF_GTE_ELSE" },
            { "floatNet.node[121].params", "{869.863159, }" },
            { "floatNet.node[122].inputs", "{44, 28, }" },
            { "floatNet.node[122].op", "ML_FOP_NxN_ACTIVATE_POLYNOMIAL" },
            { "floatNet.node[122].params", "{0.000000, 0.000000, }" },
            { "floatNet.node[123].inputs", "{37, 117, 53, 48, 70, 115, 62, }" },
            { "floatNet.node[123].op", "ML_FOP_1x1_LINEAR_COMBINATION" },
            { "floatNet.node[123].params", "{0.000000, }" },
            { "floatNet.node[124].inputs", "{10, 58, 73, }" },
            { "floatNet.node[124].op", "ML_FOP_Nx0_ACTIVATE_LN_UP" },
            { "floatNet.node[124].params", "{158.168335, 2041.309448, 0.000000, 1.000000, 4899.783203, 0.000000, 0.000000, }" },
            { "floatNet.node[25].inputs", "{21, }" },
            { "floatNet.node[25].op", "ML_FOP_Nx0_ACTIVATE_LOGISTIC" },
            { "floatNet.node[25].params", "{1023.721008, 1.598854, 0.000000, 0.000000, 0.000000, 0.001892, 0.000000, }" },
            { "floatNet.node[26].inputs", "{6, 7, 5, 11, 20, 2, }" },
            { "floatNet.node[26].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "floatNet.node[26].params", "{0.033770, 0.000000, 0.000000, 1.039665, }" },
            { "floatNet.node[27].inputs", "{15, 4, 8, }" },
            { "floatNet.node[27].op", "ML_FOP_1x4_IF_OUTSIDE_RANGE_ELSE" },
            { "floatNet.node[27].params", "{0.180390, 0.000000, 0.950000, 1.008358, 20.911724, 145.154205, 0.000000, 0.000000, }" },
            { "floatNet.node[28].inputs", "{20, 13, 27, 25, 16, 5, 18, }" },
            { "floatNet.node[28].op", "ML_FOP_1x1_SUM" },
            { "floatNet.node[28].params", "{148.639542, 2986.677246, 2.503797, 0.000000, 0.574726, 2.519957, }" },
            { "floatNet.node[29].inputs", "{7, 7, 8, 24, }" },
            { "floatNet.node[29].op", "ML_FOP_Nx0_ACTIVATE_LN_UP" },
            { "floatNet.node[29].params", "{1.100000, 147.068268, 0.000000, 0.000000, }" },
            { "floatNet.node[30].inputs", "{17, }" },
            { "floatNet.node[30].op", "ML_FOP_1x1_STRICT_OFF" },
            { "floatNet.node[30].params", "{10.000000, 942.655945, 898.907593, 0.000000, 0.223819, 0.000000, }" },
            { "floatNet.node[31].inputs", "{28, 17, 29, }" },
            { "floatNet.node[31].op", "ML_FOP_1x1_LTE" },
            { "floatNet.node[31].params", "{}" },
            { "floatNet.node[32].inputs", "{}" },
            { "floatNet.node[32].op", "ML_FOP_2x0_PRODUCT" },
            { "floatNet.node[32].params", "{0.950000, 0.000000, 0.000000, 0.000000, 0.000000, 945.880005, 244.147690, }" },
            { "floatNet.node[33].inputs", "{17, 14, 32, }" },
            { "floatNet.node[33].op", "ML_FOP_1x0_HYP_COSINE" },
            { "floatNet.node[33].params", "{}" },
            { "floatNet.node[34].inputs", "{5, }" },
            { "floatNet.node[34].op", "ML_FOP_Nx2_ACTIVATE_LINEAR_DOWN" },
            { "floatNet.node[34].params", "{143.050323, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[35].inputs", "{}" },
            { "floatNet.node[35].op", "ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT" },
            { "floatNet.node[35].params", "{1.000000, 0.000000, }" },
            { "floatNet.node[36].inputs", "{}" },
            { "floatNet.node[36].op", "ML_FOP_Nx1_ACTIVATE_THRESHOLD_UP" },
            { "floatNet.node[36].params", "{0.000000, 140.136993, 1.000000, }" },
            { "floatNet.node[37].inputs", "{3, 32, 27, 16, }" },
            { "floatNet.node[37].op", "ML_FOP_Nx1_ACTIVATE_DIV_SUM" },
            { "floatNet.node[37].params", "{161.302704, 150.583237, }" },
            { "floatNet.node[38].inputs", "{25, 1, 13, }" },
            { "floatNet.node[38].op", "ML_FOP_1x0_ARC_COSINE" },
            { "floatNet.node[38].params", "{0.000000, 2.555115, 1.000000, }" },
            { "floatNet.node[39].inputs", "{38, 37, }" },
            { "floatNet.node[39].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "floatNet.node[39].params", "{0.898238, 1.000000, 953.236450, 0.864338, 3000.000000, 0.423913, }" },
            { "floatNet.node[40].inputs", "{35, 18, 19, 30, 23, 4, }" },
            { "floatNet.node[40].op", "ML_FOP_NxN_SCALED_DIV_SUM" },
            { "floatNet.node[40].params", "{0.322733, 0.477213, }" },
            { "floatNet.node[41].inputs", "{13, 36, 12, 28, 0, }" },
            { "floatNet.node[41].op", "ML_FOP_1x0_HYP_SINE" },
            { "floatNet.node[41].params", "{1.000000, 0.000000, }" },
            { "floatNet.node[42].inputs", "{5, 34, 21, 4, }" },
            { "floatNet.node[42].op", "ML_FOP_1x0_SQRT" },
            { "floatNet.node[42].params", "{1.000000, 0.096651, 24.953848, 1240.511597, 0.000000, }" },
            { "floatNet.node[43].inputs", "{42, 24, }" },
            { "floatNet.node[43].op", "ML_FOP_1x2_COSINE" },
            { "floatNet.node[43].params", "{0.000000, }" },
            { "floatNet.node[44].inputs", "{30, }" },
            { "floatNet.node[44].op", "ML_FOP_NxN_SELECT_LTE" },
            { "floatNet.node[44].params", "{148.704834, 28.146860, 0.000000, }" },
            { "floatNet.node[45].inputs", "{43, 28, 15, }" },
            { "floatNet.node[45].op", "ML_FOP_0x0_ONE" },
            { "floatNet.node[45].params", "{}" },
            { "floatNet.node[46].inputs", "{}" },
            { "floatNet.node[46].op", "ML_FOP_1x4_IF_OUTSIDE_RANGE_ELSE" },
            { "floatNet.node[46].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[47].inputs", "{21, 16, 2, 33, 11, 21, 17, }" },
            { "floatNet.node[47].op", "ML_FOP_NxN_ACTIVATE_POLYNOMIAL" },
            { "floatNet.node[47].params", "{0.000000, 0.000000, 1.000000, 1.000000, }" },
            { "floatNet.node[48].inputs", "{}" },
            { "floatNet.node[48].op", "ML_FOP_1x1_FMOD" },
            { "floatNet.node[48].params", "{}" },
            { "floatNet.node[49].inputs", "{}" },
            { "floatNet.node[49].op", "ML_FOP_3x0_CLAMP" },
            { "floatNet.node[49].params", "{1.000000, }" },
            { "floatNet.node[50].inputs", "{43, 37, 44, 49, }" },
            { "floatNet.node[50].op", "ML_FOP_Nx0_DIV_SUM" },
            { "floatNet.node[50].params", "{1.000000, 0.258113, 291.728546, 255.701035, 0.000000, }" },
            { "floatNet.node[51].inputs", "{25, 43, 45, 29, }" },
            { "floatNet.node[51].op", "ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP" },
            { "floatNet.node[51].params", "{0.000000, 0.000000, 144.050110, 0.000000, 1.000000, 0.972787, 0.000000, }" },
            { "floatNet.node[52].inputs", "{0, 46, 2, 18, }" },
            { "floatNet.node[52].op", "ML_FOP_1x0_EXP" },
            { "floatNet.node[52].params", "{2700.262939, 1239.031250, 0.094429, 0.000000, 8.878457, 1.000000, 3.889593, 1.000000, }" },
            { "floatNet.node[53].inputs", "{12, 8, 3, 13, }" },
            { "floatNet.node[53].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "floatNet.node[53].params", "{20.740566, 1.000000, 29.763569, 13.481079, 0.950000, 0.475516, 2950.105957, 0.000000, }" },
            { "floatNet.node[54].inputs", "{11, 0, 7, 13, 33, 35, 12, 41, }" },
            { "floatNet.node[54].op", "ML_FOP_2x2_IF_GTE_ELSE" },
            { "floatNet.node[54].params", "{1.000000, }" },
            { "floatNet.node[55].inputs", "{31, 11, 21, 25, 54, 29, 25, }" },
            { "floatNet.node[55].op", "ML_FOP_1x3_HYP_TANGENT" },
            { "floatNet.node[55].params", "{0.486350, }" },
            { "floatNet.node[56].inputs", "{0, }" },
            { "floatNet.node[56].op", "ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP" },
            { "floatNet.node[56].params", "{1.000000, 1009.547424, 0.000000, 0.000000, }" },
            { "floatNet.node[57].inputs", "{43, 45, 2, }" },
            { "floatNet.node[57].op", "ML_FOP_Nx0_ACTIVATE_SOFTPLUS" },
            { "floatNet.node[57].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[58].inputs", "{0, 15, 16, 37, 19, 32, 38, }" },
            { "floatNet.node[58].op", "ML_FOP_1x0_EXP" },
            { "floatNet.node[58].params", "{0.000000, 1.019409, 1.000000, 0.000000, 0.000000, 0.097884, 0.231568, }" },
            { "floatNet.node[59].inputs", "{49, 34, 52, 49, 36, }" },
            { "floatNet.node[59].op", "ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST" },
            { "floatNet.node[59].params", "{0.615245, }" },
            { "floatNet.node[60].inputs", "{23, 6, 44, }" },
            { "floatNet.node[60].op", "ML_FOP_1x0_NEGATE" },
            { "floatNet.node[60].params", "{1730.040283, 0.108184, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[61].inputs", "{37, 17, 55, 20, 40, 8, 13, }" },
            { "floatNet.node[61].op", "ML_FOP_1x2_SEEDED_RANDOM" },
            { "floatNet.node[61].params", "{0.000000, 204.888718, 0.000000, 0.000000, 0.000000, 2322.148682, 0.000000, 0.000000, }" },
            { "floatNet.node[62].inputs", "{45, 30, 9, 20, 24, 61, 27, 13, }" },
            { "floatNet.node[62].op", "ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT" },
            { "floatNet.node[62].params", "{1.488505, 1.561505, 147.017166, 0.731341, 1.000000, 0.000000, }" },
            { "floatNet.node[63].inputs", "{36, 9, 11, 62, 47, }" },
            { "floatNet.node[63].op", "ML_FOP_1x0_ABS" },
            { "floatNet.node[63].params", "{1119.231689, 330.688843, }" },
            { "floatNet.node[64].inputs", "{40, 60, 59, }" },
            { "floatNet.node[64].op", "ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_LERP" },
            { "floatNet.node[64].params", "{0.000000, 6479.723145, 144.099915, 5.269822, 0.117956, 294.487152, 1.000000, }" },
            { "floatNet.node[65].inputs", "{31, 29, 34, 7, 13, 54, 29, 29, }" },
            { "floatNet.node[65].op", "ML_FOP_Nx0_ACTIVATE_SQRT_UP" },
            { "floatNet.node[65].params", "{30.000000, }" },
            { "floatNet.node[66].inputs", "{}" },
            { "floatNet.node[66].op", "ML_FOP_1x1_STRICT_ON" },
            { "floatNet.node[66].params", "{0.000000, 0.589762, 0.000000, 17345.072266, 0.000000, 0.000000, 0.000000, 1.000000, }" },
            { "floatNet.node[67].inputs", "{14, 44, 17, 36, 17, }" },
            { "floatNet.node[67].op", "ML_FOP_1x1_LINEAR_COMBINATION" },
            { "floatNet.node[67].params", "{0.000000, 0.937990, 0.000000, 0.900000, }" },
            { "floatNet.node[68].inputs", "{44, 6, 51, 56, 63, 41, 3, }" },
            { "floatNet.node[68].op", "ML_FOP_1x0_CEIL" },
            { "floatNet.node[68].params", "{2960.583740, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[69].inputs", "{30, 51, 25, 62, 55, 37, 20, }" },
            { "floatNet.node[69].op", "ML_FOP_4x4_LINEAR_COMBINATION" },
            { "floatNet.node[69].params", "{1977.453247, }" },
            { "floatNet.node[70].inputs", "{42, 37, 68, 0, 11, }" },
            { "floatNet.node[70].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP" },
            { "floatNet.node[70].params", "{0.100199, 0.900000, 0.000000, }" },
            { "floatNet.node[71].inputs", "{}" },
            { "floatNet.node[71].op", "ML_FOP_1xN_POLYNOMIAL_CLAMPED_UNIT" },
            { "floatNet.node[71].params", "{}" },
            { "floatNet.node[72].inputs", "{60, 7, 41, }" },
            { "floatNet.node[72].op", "ML_FOP_1x2_OUTSIDE_RANGE" },
            { "floatNet.node[72].params", "{}" },
            { "floatNet.node[73].inputs", "{}" },
            { "floatNet.node[73].op", "ML_FOP_1xN_POLYNOMIAL_CLAMPED_UNIT" },
            { "floatNet.node[73].params", "{0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[74].inputs", "{37, 15, 59, 22, 47, 33, 25, }" },
            { "floatNet.node[74].op", "ML_FOP_1x3_ARC_COSINE" },
            { "floatNet.node[74].params", "{}" },
            { "floatNet.node[75].inputs", "{}" },
            { "floatNet.node[75].op", "ML_FOP_2x0_SUM" },
            { "floatNet.node[75].params", "{24.098963, 1904.422607, 0.000000, 0.798429, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[76].inputs", "{13, 67, 3, 13, }" },
            { "floatNet.node[76].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "floatNet.node[76].params", "{0.109540, 0.000000, 0.000000, 396.382080, 0.000000, }" },
            { "floatNet.node[77].inputs", "{13, }" },
            { "floatNet.node[77].op", "ML_FOP_1x0_HYP_SINE" },
            { "floatNet.node[77].params", "{0.855000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[78].inputs", "{12, }" },
            { "floatNet.node[78].op", "ML_FOP_NxN_SELECT_LTE" },
            { "floatNet.node[78].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[79].inputs", "{54, 70, 4, 75, }" },
            { "floatNet.node[79].op", "ML_FOP_2x0_PRODUCT" },
            { "floatNet.node[79].params", "{0.100938, 0.893583, 0.000000, 0.210219, 1.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[80].inputs", "{48, 57, 75, 40, 73, }" },
            { "floatNet.node[80].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "floatNet.node[80].params", "{3.804961, 257.136505, 1.542052, }" },
            { "floatNet.node[81].inputs", "{59, 65, 55, 49, 3, 30, }" },
            { "floatNet.node[81].op", "ML_FOP_1x3_ARC_COSINE" },
            { "floatNet.node[81].params", "{0.000000, 0.000000, 0.000000, 0.000000, 660.309509, 1.000000, 1.050000, }" },
            { "floatNet.node[82].inputs", "{70, 2, 61, 3, 51, }" },
            { "floatNet.node[82].op", "ML_FOP_Nx1_DIV_SUM" },
            { "floatNet.node[82].params", "{9895.462891, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[83].inputs", "{13, 32, 30, 71, 80, 18, }" },
            { "floatNet.node[83].op", "ML_FOP_1x1_FMOD" },
            { "floatNet.node[83].params", "{}" },
            { "floatNet.node[84].inputs", "{5, 8, 64, 54, 81, }" },
            { "floatNet.node[84].op", "ML_FOP_1x0_PROB_NOT" },
            { "floatNet.node[84].params", "{0.000000, 0.000000, }" },
            { "floatNet.node[85].inputs", "{27, 46, 78, 69, }" },
            { "floatNet.node[85].op", "ML_FOP_1x0_ARC_TANGENT" },
            { "floatNet.node[85].params", "{5684.448730, 0.000000, 1.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[86].inputs", "{84, 75, 73, 75, }" },
            { "floatNet.node[86].op", "ML_FOP_NxN_WEIGHTED_GEOMETRIC_MEAN" },
            { "floatNet.node[86].params", "{0.000000, 0.000000, 24.561216, 1035.908569, }" },
            { "floatNet.node[87].inputs", "{16, 44, }" },
            { "floatNet.node[87].op", "ML_FOP_2x0_CEIL_STEP" },
            { "floatNet.node[87].params", "{1.627296, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[88].inputs", "{2, 56, }" },
            { "floatNet.node[88].op", "ML_FOP_Nx0_SELECT_UNIT_INTERVAL_LERP" },
            { "floatNet.node[88].params", "{12.581761, 2.725593, }" },
            { "floatNet.node[89].inputs", "{69, 63, 15, }" },
            { "floatNet.node[89].op", "ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT" },
            { "floatNet.node[89].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "floatNet.node[90].inputs", "{78, 66, 84, 21, 3, 29, 12, }" },
            { "floatNet.node[90].op", "ML_FOP_Nx2_ACTIVATE_QUADRATIC_DOWN" },
            { "floatNet.node[90].params", "{}" },
            { "floatNet.node[91].inputs", "{}" },
            { "floatNet.node[91].op", "ML_FOP_1x0_CEIL" },
            { "floatNet.node[91].params", "{}" },
            { "floatNet.node[92].inputs", "{25, 2, }" },
            { "floatNet.node[92].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "floatNet.node[92].params", "{0.000000, 0.102678, 1.000000, }" },
            { "floatNet.node[93].inputs", "{31, 27, }" },
            { "floatNet.node[93].op", "ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_LERP" },
            { "floatNet.node[93].params", "{989.335571, }" },
            { "floatNet.node[94].inputs", "{43, 37, }" },
            { "floatNet.node[94].op", "ML_FOP_Nx3_ACTIVATE_GAUSSIAN" },
            { "floatNet.node[94].params", "{0.000000, 0.000000, 5.410668, 1.007700, }" },
            { "floatNet.node[95].inputs", "{6, 39, 81, 52, 20, }" },
            { "floatNet.node[95].op", "ML_FOP_Nx0_SUM" },
            { "floatNet.node[95].params", "{1.340968, 275.882324, 1.000000, 18354.156250, 2.035275, 0.000000, }" },
            { "floatNet.node[96].inputs", "{87, 46, 16, 90, 54, }" },
            { "floatNet.node[96].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[96].params", "{2401.708252, 4017.525635, 1.846820, 1.474648, }" },
            { "floatNet.node[97].inputs", "{39, }" },
            { "floatNet.node[97].op", "ML_FOP_Nx0_MAX" },
            { "floatNet.node[97].params", "{}" },
            { "floatNet.node[98].inputs", "{}" },
            { "floatNet.node[98].op", "ML_FOP_2x2_IF_GTE_ELSE" },
            { "floatNet.node[98].params", "{0.000000, 0.000000, 1145.579712, 1385.257324, 0.000000, 0.000000, }" },
            { "floatNet.node[99].inputs", "{1, 30, 35, 62, 47, }" },
            { "floatNet.node[99].op", "ML_FOP_3x2_IF_INSIDE_RANGE_CONST_ELSE" },
            { "floatNet.node[99].params", "{0.000000, 2.573027, 0.000000, 0.688065, 1007.641113, }" },
            { "floatNet.numInputs", "25" },
            { "floatNet.numNodes", "100" },
            { "floatNet.numOutputs", "25" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "53.190826" },
            { "guardRange", "87.259979" },
            { "input[0].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[0].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "input[0].radius", "0.000000" },
            { "input[0].useTangent", "FALSE" },
            { "input[0].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "input[10].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "input[10].forceType", "NEURAL_FORCE_ALIGN2" },
            { "input[10].radius", "157.201385" },
            { "input[10].useTangent", "FALSE" },
            { "input[10].valueType", "NEURAL_VALUE_CROWD" },
            { "input[11].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "input[11].forceType", "NEURAL_FORCE_BASE" },
            { "input[11].radius", "157.342880" },
            { "input[11].useTangent", "TRUE" },
            { "input[11].valueType", "NEURAL_VALUE_FORCE" },
            { "input[11].waveType", "NEURAL_WAVE_UNIT_SINE" },
            { "input[12].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[12].forceType", "NEURAL_FORCE_CORES" },
            { "input[12].frequency", "0.000000" },
            { "input[12].radius", "0.000000" },
            { "input[12].useTangent", "TRUE" },
            { "input[12].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "input[12].waveType", "NEURAL_WAVE_SINE" },
            { "input[13].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[13].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "input[13].frequency", "0.000000" },
            { "input[13].radius", "0.000000" },
            { "input[13].useTangent", "TRUE" },
            { "input[13].valueType", "NEURAL_VALUE_FORCE" },
            { "input[13].waveType", "NEURAL_WAVE_SINE" },
            { "input[14].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "input[14].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "input[14].frequency", "0.000000" },
            { "input[14].radius", "164.939926" },
            { "input[14].useTangent", "FALSE" },
            { "input[14].valueType", "NEURAL_VALUE_FORCE" },
            { "input[14].waveType", "NEURAL_WAVE_FMOD" },
            { "input[15].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "input[15].forceType", "NEURAL_FORCE_COHERE" },
            { "input[15].radius", "156.087997" },
            { "input[15].useTangent", "FALSE" },
            { "input[15].valueType", "NEURAL_VALUE_FORCE" },
            { "input[16].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "input[16].forceType", "NEURAL_FORCE_CENTER" },
            { "input[16].frequency", "0.000000" },
            { "input[16].radius", "0.000000" },
            { "input[16].useTangent", "FALSE" },
            { "input[16].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[17].crowdType", "NEURAL_CROWD_CORES" },
            { "input[17].forceType", "NEURAL_FORCE_BASE" },
            { "input[17].frequency", "4093.644043" },
            { "input[17].radius", "0.000000" },
            { "input[17].useTangent", "FALSE" },
            { "input[17].valueType", "NEURAL_VALUE_CROWD" },
            { "input[17].waveType", "NEURAL_WAVE_FMOD" },
            { "input[18].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "input[18].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "input[18].frequency", "0.000000" },
            { "input[18].radius", "147.504044" },
            { "input[18].useTangent", "FALSE" },
            { "input[18].valueType", "NEURAL_VALUE_MOBID" },
            { "input[18].waveType", "NEURAL_WAVE_FMOD" },
            { "input[19].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "input[19].forceType", "NEURAL_FORCE_ZERO" },
            { "input[19].frequency", "0.000000" },
            { "input[19].radius", "144.802826" },
            { "input[19].useTangent", "FALSE" },
            { "input[19].valueType", "NEURAL_VALUE_CROWD" },
            { "input[19].waveType", "NEURAL_WAVE_UNIT_SINE" },
            { "input[1].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "input[1].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "input[1].radius", "301.750824" },
            { "input[1].useTangent", "TRUE" },
            { "input[1].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "input[20].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "input[20].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "input[20].radius", "0.000000" },
            { "input[20].useTangent", "FALSE" },
            { "input[20].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[20].waveType", "NEURAL_WAVE_FMOD" },
            { "input[21].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[21].forceType", "NEURAL_FORCE_ENEMY" },
            { "input[21].frequency", "0.000000" },
            { "input[21].radius", "-1.000000" },
            { "input[21].useTangent", "TRUE" },
            { "input[21].valueType", "NEURAL_VALUE_CROWD" },
            { "input[22].crowdType", "NEURAL_CROWD_CORES" },
            { "input[22].forceType", "NEURAL_FORCE_ALIGN2" },
            { "input[22].radius", "6.802381" },
            { "input[22].useTangent", "FALSE" },
            { "input[22].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "input[22].waveType", "NEURAL_WAVE_NONE" },
            { "input[23].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[23].forceType", "NEURAL_FORCE_CENTER" },
            { "input[23].frequency", "0.000000" },
            { "input[23].radius", "-1.000000" },
            { "input[23].useTangent", "FALSE" },
            { "input[23].valueType", "NEURAL_VALUE_CROWD" },
            { "input[23].waveType", "NEURAL_WAVE_NONE" },
            { "input[24].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "input[24].forceType", "NEURAL_FORCE_SEPARATE" },
            { "input[24].frequency", "0.000000" },
            { "input[24].radius", "1086.721436" },
            { "input[24].useTangent", "TRUE" },
            { "input[24].valueType", "NEURAL_VALUE_FORCE" },
            { "input[2].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "input[2].forceType", "NEURAL_FORCE_CORES" },
            { "input[2].radius", "148.709793" },
            { "input[2].useTangent", "FALSE" },
            { "input[2].valueType", "NEURAL_VALUE_CROWD" },
            { "input[3].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[3].forceType", "NEURAL_FORCE_BASE" },
            { "input[3].radius", "-1.000000" },
            { "input[3].useTangent", "FALSE" },
            { "input[3].valueType", "NEURAL_VALUE_CREDITS" },
            { "input[4].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "input[4].forceType", "NEURAL_FORCE_NEAREST_EDGE" },
            { "input[4].frequency", "5622.903809" },
            { "input[4].radius", "0.000000" },
            { "input[4].useTangent", "FALSE" },
            { "input[4].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "input[4].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "input[5].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "input[5].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "input[5].frequency", "0.000000" },
            { "input[5].radius", "0.000000" },
            { "input[5].useTangent", "TRUE" },
            { "input[5].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "input[5].waveType", "NEURAL_WAVE_SINE" },
            { "input[6].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "input[6].forceType", "NEURAL_FORCE_HEADING" },
            { "input[6].frequency", "0.000000" },
            { "input[6].radius", "-0.950000" },
            { "input[6].useTangent", "FALSE" },
            { "input[6].valueType", "NEURAL_VALUE_CROWD" },
            { "input[6].waveType", "NEURAL_WAVE_NONE" },
            { "input[7].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "input[7].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "input[7].radius", "4.193549" },
            { "input[7].useTangent", "TRUE" },
            { "input[7].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "input[8].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "input[8].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS" },
            { "input[8].frequency", "0.000000" },
            { "input[8].radius", "0.000000" },
            { "input[8].useTangent", "FALSE" },
            { "input[8].valueType", "NEURAL_VALUE_CROWD" },
            { "input[8].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "input[9].crowdType", "NEURAL_CROWD_FRIEND_MISSILE" },
            { "input[9].forceType", "NEURAL_FORCE_HEADING" },
            { "input[9].radius", "154.979477" },
            { "input[9].useTangent", "TRUE" },
            { "input[9].valueType", "NEURAL_VALUE_FORCE" },
            { "output[100].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "output[100].radius", "529.224609" },
            { "output[100].useTangent", "TRUE" },
            { "output[100].valueType", "NEURAL_VALUE_FORCE" },
            { "output[101].forceType", "NEURAL_FORCE_ZERO" },
            { "output[101].radius", "1351.212036" },
            { "output[101].useTangent", "FALSE" },
            { "output[101].valueType", "NEURAL_VALUE_FORCE" },
            { "output[102].forceType", "NEURAL_FORCE_SEPARATE" },
            { "output[102].radius", "167.162323" },
            { "output[102].useTangent", "FALSE" },
            { "output[102].valueType", "NEURAL_VALUE_FORCE" },
            { "output[103].forceType", "NEURAL_FORCE_NEAREST_CORNER" },
            { "output[103].radius", "-1.000000" },
            { "output[103].useTangent", "FALSE" },
            { "output[103].valueType", "NEURAL_VALUE_FORCE" },
            { "output[104].forceType", "NEURAL_FORCE_ENEMY" },
            { "output[104].radius", "1729.684937" },
            { "output[104].useTangent", "FALSE" },
            { "output[104].valueType", "NEURAL_VALUE_FORCE" },
            { "output[105].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "output[105].radius", "278.047913" },
            { "output[105].useTangent", "FALSE" },
            { "output[105].valueType", "NEURAL_VALUE_FORCE" },
            { "output[106].forceType", "NEURAL_FORCE_ALIGN2" },
            { "output[106].radius", "2541.187500" },
            { "output[106].useTangent", "FALSE" },
            { "output[106].valueType", "NEURAL_VALUE_FORCE" },
            { "output[107].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[107].radius", "-1.000000" },
            { "output[107].useTangent", "TRUE" },
            { "output[107].valueType", "NEURAL_VALUE_FORCE" },
            { "output[108].forceType", "NEURAL_FORCE_ALIGN2" },
            { "output[108].radius", "-0.950000" },
            { "output[108].useTangent", "FALSE" },
            { "output[108].valueType", "NEURAL_VALUE_FORCE" },
            { "output[109].forceType", "NEURAL_FORCE_HEADING" },
            { "output[109].radius", "1411.446289" },
            { "output[109].useTangent", "FALSE" },
            { "output[109].valueType", "NEURAL_VALUE_FORCE" },
            { "output[110].forceType", "NEURAL_FORCE_COHERE" },
            { "output[110].radius", "2590.880371" },
            { "output[110].useTangent", "FALSE" },
            { "output[110].valueType", "NEURAL_VALUE_FORCE" },
            { "output[111].forceType", "NEURAL_FORCE_ALIGN2" },
            { "output[111].radius", "151.967468" },
            { "output[111].useTangent", "TRUE" },
            { "output[111].valueType", "NEURAL_VALUE_FORCE" },
            { "output[112].forceType", "NEURAL_FORCE_HEADING" },
            { "output[112].radius", "296.518951" },
            { "output[112].useTangent", "FALSE" },
            { "output[112].valueType", "NEURAL_VALUE_FORCE" },
            { "output[113].forceType", "NEURAL_FORCE_ZERO" },
            { "output[113].radius", "-1.000000" },
            { "output[113].useTangent", "TRUE" },
            { "output[113].valueType", "NEURAL_VALUE_FORCE" },
            { "output[114].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "output[114].radius", "322.285767" },
            { "output[114].useTangent", "FALSE" },
            { "output[114].valueType", "NEURAL_VALUE_FORCE" },
            { "output[115].forceType", "NEURAL_FORCE_HEADING" },
            { "output[115].radius", "-1.000000" },
            { "output[115].useTangent", "FALSE" },
            { "output[115].valueType", "NEURAL_VALUE_FORCE" },
            { "output[116].forceType", "NEURAL_FORCE_CORES" },
            { "output[116].radius", "0.000000" },
            { "output[116].useTangent", "TRUE" },
            { "output[116].valueType", "NEURAL_VALUE_FORCE" },
            { "output[117].forceType", "NEURAL_FORCE_COHERE" },
            { "output[117].radius", "1002.034180" },
            { "output[117].useTangent", "TRUE" },
            { "output[117].valueType", "NEURAL_VALUE_FORCE" },
            { "output[118].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "output[118].radius", "298.878693" },
            { "output[118].useTangent", "FALSE" },
            { "output[118].valueType", "NEURAL_VALUE_FORCE" },
            { "output[119].forceType", "NEURAL_FORCE_ALIGN2" },
            { "output[119].radius", "2850.000000" },
            { "output[119].useTangent", "FALSE" },
            { "output[119].valueType", "NEURAL_VALUE_FORCE" },
            { "output[120].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "output[120].radius", "2963.549072" },
            { "output[120].useTangent", "FALSE" },
            { "output[120].valueType", "NEURAL_VALUE_FORCE" },
            { "output[121].forceType", "NEURAL_FORCE_ENEMY" },
            { "output[121].radius", "-1.000000" },
            { "output[121].useTangent", "FALSE" },
            { "output[121].valueType", "NEURAL_VALUE_FORCE" },
            { "output[122].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "output[122].radius", "1693.586426" },
            { "output[122].useTangent", "FALSE" },
            { "output[122].valueType", "NEURAL_VALUE_FORCE" },
            { "output[123].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "output[123].radius", "1030.571045" },
            { "output[123].useTangent", "FALSE" },
            { "output[123].valueType", "NEURAL_VALUE_FORCE" },
            { "output[124].forceType", "NEURAL_FORCE_ALIGN2" },
            { "output[124].radius", "-1.000000" },
            { "output[124].useTangent", "FALSE" },
            { "output[124].valueType", "NEURAL_VALUE_FORCE" },
            { "sensorGrid.staleCoreTime", "0.000000" },
            { "sensorGrid.staleFighterTime", "0.000000" },
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
        myUseAttackForces = MBRegistry_GetBoolD(mreg, "useAttackForces", FALSE);

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
            NeuralValue_Load(mreg, &myOutputDescs[i], str);
            free(str);

            if (myOutputDescs[i].valueType != NEURAL_VALUE_FORCE) {
                myOutputDescs[i].valueType = NEURAL_VALUE_FORCE;
                myOutputDescs[i].forceDesc.forceType = NEURAL_FORCE_VOID;
                myOutputDescs[i].forceDesc.useTangent = FALSE;
                myOutputDescs[i].forceDesc.radius = 0.0f;
                myOutputDescs[i].forceDesc.doIdle = FALSE;
                myOutputDescs[i].forceDesc.doAttack = FALSE;
            }

            if (myOutputDescs[i].forceDesc.forceType == NEURAL_FORCE_ZERO ||
                myOutputDescs[i].forceDesc.forceType == NEURAL_FORCE_VOID) {
                myNeuralNet.voidOutputNode(i);
                myOutputDescs[i].forceDesc.forceType = NEURAL_FORCE_VOID;
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
                NeuralValue_Load(mreg, &myInputDescs[i], str);
                free(str);
            } else {
                myInputDescs[i].valueType = NEURAL_VALUE_VOID;
            }
        }

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    NeuralNetContext *getNeuralNetContext(void) {
        ASSERT(myNNC.rs != NULL);
        ASSERT(myNNC.sg != NULL);
        ASSERT(myNNC.ai != NULL);
        return &myNNC;
    }

    void doForces(Mob *mob, BasicShipAIState state, FRPoint *outputForce) {
        uint x;
        float maxV = (1.0f / MICRON);
        NeuralNetContext *nc = getNeuralNetContext();

        ASSERT(myInputs.size() == myInputDescs.size());

        for (uint i = 0; i < myInputDescs.size(); i++) {
            myInputs[i] = NeuralValue_GetValue(nc, mob, &myInputDescs[i], i);
        }

        ASSERT(myOutputs.size() == myOutputDescs.size());
        myNeuralNet.compute(myInputs, myOutputs);

        for (uint i = 0; i < myOutputs.size(); i++) {
            ASSERT(myOutputDescs[i].valueType == NEURAL_VALUE_FORCE);
            if ((state == BSAI_STATE_IDLE && !myOutputDescs[i].forceDesc.doIdle) ||
                (state == BSAI_STATE_ATTACK && !myOutputDescs[i].forceDesc.doAttack)) {
                myOutputs[i] = 0.0f;
            } else if (isnan(myOutputs[i])) {
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
            ASSERT(myOutputDescs[i].forceDesc.forceType != NEURAL_FORCE_ZERO);
            if (myOutputDescs[i].forceDesc.forceType != NEURAL_FORCE_VOID &&
                myOutputs[x] != 0.0f &&
                NeuralForce_GetForce(getNeuralNetContext(), mob,
                                     &myOutputDescs[i].forceDesc, &force)) {
                FRPoint_SetSpeed(&force, myOutputs[x]);
                FRPoint_Add(&force, outputForce, outputForce);
            }
            x++;
        }
        //XXX non-force outputs?
        ASSERT(x <= myOutputs.size());
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {
        if (myUseAttackForces) {
            BineuralShipAI *ship = (BineuralShipAI *)mob->aiMobHandle;
            ASSERT(ship == (BineuralShipAI *)getShip(mob->mobid));
            ASSERT(ship != NULL);

            FPoint origTarget = mob->cmd.target;
            BasicAIGovernor::doAttack(mob, enemyTarget);

            mob->cmd.target = origTarget;

            FRPoint rForce;
            doForces(mob, ship->state, &rForce);
            NeuralForce_ApplyToMob(getNeuralNetContext(), mob, &rForce);
        } else {
            BasicAIGovernor::doAttack(mob, enemyTarget);
        }
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        //RandomState *rs = &myRandomState;

        BineuralShipAI *ship = (BineuralShipAI *)mob->aiMobHandle;
        ASSERT(ship == (BineuralShipAI *)getShip(mob->mobid));
        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        FRPoint rForce;
        doForces(mob, ship->state, &rForce);
        NeuralForce_ApplyToMob(getNeuralNetContext(), mob, &rForce);

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

    /*
     * If we voided out the inputs/outputs when the FloatNet was minimized,
     * reflect that here.
     */
    for (uint i = 0; i < sf->gov.myInputDescs.size(); i++) {
        if (sf->gov.myInputDescs[i].valueType == NEURAL_VALUE_VOID) {
            char *str = NULL;
            const char *value;
            int ret = asprintf(&str, "input[%d].valueType", i);
            VERIFY(ret > 0);
            value = NeuralValue_ToString(sf->gov.myInputDescs[i].valueType);
            MBRegistry_PutCopy(mreg, str, value);
            free(str);
        }
    }
    for (uint i = 0; i < sf->gov.myOutputDescs.size(); i++) {
        if (sf->gov.myOutputDescs[i].valueType == NEURAL_VALUE_FORCE &&
            sf->gov.myOutputDescs[i].forceDesc.forceType == NEURAL_FORCE_VOID) {
            char *str = NULL;
            const char *value;
            int ret = asprintf(&str, "output[%d].forceType", i);
            VERIFY(ret > 0);
            value = NeuralForce_ToString(sf->gov.myOutputDescs[i].forceDesc.forceType);
            MBRegistry_PutCopy(mreg, str, value);
            free(str);
        }
    }
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
        { "useAttackForces",             0.05f },
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

    FloatNet fn;
    if (MBRegistry_ContainsKey(mreg, "floatNet.numInputs") &&
        MBRegistry_GetUint(mreg, "floatNet.numInputs") > 0 &&
        !MBRegistry_GetBool(mreg, BINEURAL_SCRAMBLE_KEY)) {
        fn.load(mreg, "floatNet.");
    } else {
        fn.initialize(BINEURAL_MAX_INPUTS, BINEURAL_MAX_OUTPUTS,
                      BINEURAL_MAX_NODES);
        fn.loadZeroNet();
    }

    fn.mutate(rate, BINEURAL_MAX_NODE_DEGREE, BINEURAL_MAX_NODES);
    fn.save(mreg, "floatNet.");

    for (uint i = 0; i < fn.getNumInputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "input[%d].", i);
        VERIFY(ret > 0);
        NeuralValue_Load(mreg, &desc, str);
        NeuralValue_Mutate(mreg, &desc, FALSE, rate, str);
        free(str);
    }

    for (uint i = 0; i < fn.getNumOutputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "output[%d].", i + fn.getOutputOffset());
        VERIFY(ret > 0);
        NeuralValue_Load(mreg, &desc, str);
        NeuralValue_Mutate(mreg, &desc, TRUE, rate, str);
        free(str);
    }

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