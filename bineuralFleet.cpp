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
            { "attackRange", "126.199165" },
            { "creditReserve", "0.000000" },
            { "evadeFighters", "FALSE" },
            { "evadeRange", "299.256897" },
            { "evadeStrictDistance", "114.395027" },
            { "evadeUseStrictDistance", "FALSE" },
            { "shipNet.node[100].inputs", "{84, }" },
            { "shipNet.node[100].op", "ML_FOP_Nx3_ACTIVATE_GAUSSIAN" },
            { "shipNet.node[100].params", "{0.000000, 1.050000, 0.276356, 1.000000, 3.157942, 0.000000, }" },
            { "shipNet.node[101].inputs", "{23, 7, 17, }" },
            { "shipNet.node[101].op", "ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST" },
            { "shipNet.node[101].params", "{0.000000, 3.143681, 0.000000, }" },
            { "shipNet.node[102].inputs", "{24, 42, 101, 0, 86, 6, 58, 37, }" },
            { "shipNet.node[102].op", "ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_STEP" },
            { "shipNet.node[102].params", "{1.549154, 0.000000, 2.486770, 17069.486328, 2.520596, }" },
            { "shipNet.node[103].inputs", "{97, 10, 10, }" },
            { "shipNet.node[103].op", "ML_FOP_1x0_FLOOR" },
            { "shipNet.node[103].params", "{1252.525146, 1577.410767, 30.000000, 0.513399, 1.000000, }" },
            { "shipNet.node[104].inputs", "{67, 57, }" },
            { "shipNet.node[104].op", "ML_FOP_5x0_IF_OUTSIDE_RANGE_ELSE" },
            { "shipNet.node[104].params", "{1.000000, 147.082001, 0.000000, 0.000000, 1.000000, 0.000000, }" },
            { "shipNet.node[105].inputs", "{9, 52, 32, 0, 101, }" },
            { "shipNet.node[105].op", "ML_FOP_0x0_ZERO" },
            { "shipNet.node[105].params", "{0.950000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[106].inputs", "{70, 35, }" },
            { "shipNet.node[106].op", "ML_FOP_1x3_IF_LTE_ELSE" },
            { "shipNet.node[106].params", "{}" },
            { "shipNet.node[107].inputs", "{44, 61, 30, 10, }" },
            { "shipNet.node[107].op", "ML_FOP_NxN_WEIGHTED_GEOMETRIC_MEAN" },
            { "shipNet.node[107].params", "{1.505585, 1191.031982, 647.882019, 0.046653, 1.021709, 0.000000, }" },
            { "shipNet.node[108].inputs", "{31, 57, 69, 60, 75, 84, 55, 88, }" },
            { "shipNet.node[108].op", "ML_FOP_2x0_SQUARE_SUM" },
            { "shipNet.node[108].params", "{0.000000, }" },
            { "shipNet.node[109].inputs", "{68, 29, 53, 1, 55, }" },
            { "shipNet.node[109].op", "ML_FOP_1x1_LINEAR_UP" },
            { "shipNet.node[109].params", "{21.620867, 0.000000, 2.713619, 0.108105, 0.000000, 1056.567627, 0.105662, }" },
            { "shipNet.node[110].inputs", "{}" },
            { "shipNet.node[110].op", "ML_FOP_Nx0_DIV_SUM" },
            { "shipNet.node[110].params", "{0.000000, 1.000000, 0.206780, 3.391139, }" },
            { "shipNet.node[111].inputs", "{2, 98, 32, 0, 101, 76, 109, 107, }" },
            { "shipNet.node[111].op", "ML_FOP_3x0_CLAMP" },
            { "shipNet.node[111].params", "{1.000000, 0.000000, 0.898641, 1018.451843, 0.000000, 1.000000, }" },
            { "shipNet.node[112].inputs", "{87, 107, 75, 37, }" },
            { "shipNet.node[112].op", "ML_FOP_1x4_IF_OUTSIDE_RANGE_ELSE" },
            { "shipNet.node[112].params", "{1520.729370, 0.110689, 0.000000, }" },
            { "shipNet.node[113].inputs", "{27, 22, 103, 100, 66, 28, 90, 14, }" },
            { "shipNet.node[113].op", "ML_FOP_Nx1_ACTIVATE_THRESHOLD_UP" },
            { "shipNet.node[113].params", "{1.000000, 19.482994, 1.482340, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[114].inputs", "{37, 94, 89, 113, 91, }" },
            { "shipNet.node[114].op", "ML_FOP_4x0_IF_GTE_ELSE" },
            { "shipNet.node[114].params", "{0.000000, 0.000000, 0.191884, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[115].inputs", "{72, 5, 21, 114, }" },
            { "shipNet.node[115].op", "ML_FOP_1x3_HYP_COSINE" },
            { "shipNet.node[115].params", "{0.000000, 0.000000, 0.106200, 0.000000, 0.234054, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[116].inputs", "{87, 34, }" },
            { "shipNet.node[116].op", "ML_FOP_3x0_IF_GTEZ_ELSE" },
            { "shipNet.node[116].params", "{17151.246094, 0.000000, 0.000000, 0.000000, 0.103045, 0.000000, }" },
            { "shipNet.node[117].inputs", "{5, 11, 84, 53, 35, 94, 64, 38, }" },
            { "shipNet.node[117].op", "ML_FOP_1x2_SINE" },
            { "shipNet.node[117].params", "{}" },
            { "shipNet.node[118].inputs", "{1, 70, 76, 63, }" },
            { "shipNet.node[118].op", "ML_FOP_1x0_CEIL" },
            { "shipNet.node[118].params", "{0.000000, 0.000000, 154.593857, 0.000000, 0.000000, 0.013984, 0.000000, 1.093649, }" },
            { "shipNet.node[119].inputs", "{13, 66, 46, 51, 19, 66, }" },
            { "shipNet.node[119].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "shipNet.node[119].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[120].inputs", "{113, 108, 78, 49, }" },
            { "shipNet.node[120].op", "ML_FOP_1x3_ARC_TANGENT" },
            { "shipNet.node[120].params", "{0.555243, 0.000000, 1036.331543, 0.497635, 1.308342, 1.000000, }" },
            { "shipNet.node[121].inputs", "{31, 109, 42, 118, }" },
            { "shipNet.node[121].op", "ML_FOP_4x0_IF_GTE_ELSE" },
            { "shipNet.node[121].params", "{869.863159, }" },
            { "shipNet.node[122].inputs", "{44, 28, }" },
            { "shipNet.node[122].op", "ML_FOP_NxN_ACTIVATE_POLYNOMIAL" },
            { "shipNet.node[122].params", "{0.000000, 0.000000, }" },
            { "shipNet.node[123].inputs", "{37, 117, 53, 48, 70, 115, 62, }" },
            { "shipNet.node[123].op", "ML_FOP_1x1_LINEAR_COMBINATION" },
            { "shipNet.node[123].params", "{0.000000, }" },
            { "shipNet.node[124].inputs", "{10, 58, 73, }" },
            { "shipNet.node[124].op", "ML_FOP_Nx0_ACTIVATE_LN_UP" },
            { "shipNet.node[124].params", "{158.168335, 2041.309448, 0.000000, 1.000000, 4899.783203, 0.000000, 0.000000, }" },
            { "shipNet.node[25].inputs", "{21, }" },
            { "shipNet.node[25].op", "ML_FOP_Nx0_ACTIVATE_LOGISTIC" },
            { "shipNet.node[25].params", "{1023.721008, 1.598854, 0.000000, 0.000000, 0.000000, 0.001892, 0.000000, }" },
            { "shipNet.node[26].inputs", "{6, 7, 5, 11, 20, 2, }" },
            { "shipNet.node[26].op", "ML_FOP_1x1_QUADRATIC_UP" },
            { "shipNet.node[26].params", "{0.033770, 0.000000, 0.000000, 1.039665, }" },
            { "shipNet.node[27].inputs", "{15, 4, 8, }" },
            { "shipNet.node[27].op", "ML_FOP_1x4_IF_OUTSIDE_RANGE_ELSE" },
            { "shipNet.node[27].params", "{0.180390, 0.000000, 0.950000, 1.008358, 20.911724, 145.154205, 0.000000, 0.000000, }" },
            { "shipNet.node[28].inputs", "{20, 13, 27, 25, 16, 5, 18, }" },
            { "shipNet.node[28].op", "ML_FOP_1x1_SUM" },
            { "shipNet.node[28].params", "{148.639542, 2986.677246, 2.503797, 0.000000, 0.574726, 2.519957, }" },
            { "shipNet.node[29].inputs", "{7, 7, 8, 24, }" },
            { "shipNet.node[29].op", "ML_FOP_Nx0_ACTIVATE_LN_UP" },
            { "shipNet.node[29].params", "{1.100000, 147.068268, 0.000000, 0.000000, }" },
            { "shipNet.node[30].inputs", "{17, }" },
            { "shipNet.node[30].op", "ML_FOP_1x1_STRICT_OFF" },
            { "shipNet.node[30].params", "{10.000000, 942.655945, 898.907593, 0.000000, 0.223819, 0.000000, }" },
            { "shipNet.node[31].inputs", "{28, 17, 29, }" },
            { "shipNet.node[31].op", "ML_FOP_1x1_LTE" },
            { "shipNet.node[31].params", "{}" },
            { "shipNet.node[32].inputs", "{}" },
            { "shipNet.node[32].op", "ML_FOP_2x0_PRODUCT" },
            { "shipNet.node[32].params", "{0.950000, 0.000000, 0.000000, 0.000000, 0.000000, 945.880005, 244.147690, }" },
            { "shipNet.node[33].inputs", "{17, 14, 32, }" },
            { "shipNet.node[33].op", "ML_FOP_1x0_HYP_COSINE" },
            { "shipNet.node[33].params", "{}" },
            { "shipNet.node[34].inputs", "{5, }" },
            { "shipNet.node[34].op", "ML_FOP_Nx2_ACTIVATE_LINEAR_DOWN" },
            { "shipNet.node[34].params", "{143.050323, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[35].inputs", "{}" },
            { "shipNet.node[35].op", "ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT" },
            { "shipNet.node[35].params", "{1.000000, 0.000000, }" },
            { "shipNet.node[36].inputs", "{}" },
            { "shipNet.node[36].op", "ML_FOP_Nx1_ACTIVATE_THRESHOLD_UP" },
            { "shipNet.node[36].params", "{0.000000, 140.136993, 1.000000, }" },
            { "shipNet.node[37].inputs", "{3, 32, 27, 16, }" },
            { "shipNet.node[37].op", "ML_FOP_Nx1_ACTIVATE_DIV_SUM" },
            { "shipNet.node[37].params", "{161.302704, 150.583237, }" },
            { "shipNet.node[38].inputs", "{25, 1, 13, }" },
            { "shipNet.node[38].op", "ML_FOP_1x0_ARC_COSINE" },
            { "shipNet.node[38].params", "{0.000000, 2.555115, 1.000000, }" },
            { "shipNet.node[39].inputs", "{38, 37, }" },
            { "shipNet.node[39].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE" },
            { "shipNet.node[39].params", "{0.898238, 1.000000, 953.236450, 0.864338, 3000.000000, 0.423913, }" },
            { "shipNet.node[40].inputs", "{35, 18, 19, 30, 23, 4, }" },
            { "shipNet.node[40].op", "ML_FOP_NxN_SCALED_DIV_SUM" },
            { "shipNet.node[40].params", "{0.322733, 0.477213, }" },
            { "shipNet.node[41].inputs", "{13, 36, 12, 28, 0, }" },
            { "shipNet.node[41].op", "ML_FOP_1x0_HYP_SINE" },
            { "shipNet.node[41].params", "{1.000000, 0.000000, }" },
            { "shipNet.node[42].inputs", "{5, 34, 21, 4, }" },
            { "shipNet.node[42].op", "ML_FOP_1x0_SQRT" },
            { "shipNet.node[42].params", "{1.000000, 0.096651, 24.953848, 1240.511597, 0.000000, }" },
            { "shipNet.node[43].inputs", "{42, 24, }" },
            { "shipNet.node[43].op", "ML_FOP_1x2_COSINE" },
            { "shipNet.node[43].params", "{0.000000, }" },
            { "shipNet.node[44].inputs", "{30, }" },
            { "shipNet.node[44].op", "ML_FOP_NxN_SELECT_LTE" },
            { "shipNet.node[44].params", "{148.704834, 28.146860, 0.000000, }" },
            { "shipNet.node[45].inputs", "{43, 28, 15, }" },
            { "shipNet.node[45].op", "ML_FOP_0x0_ONE" },
            { "shipNet.node[45].params", "{}" },
            { "shipNet.node[46].inputs", "{}" },
            { "shipNet.node[46].op", "ML_FOP_1x4_IF_OUTSIDE_RANGE_ELSE" },
            { "shipNet.node[46].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[47].inputs", "{21, 16, 2, 33, 11, 21, 17, }" },
            { "shipNet.node[47].op", "ML_FOP_NxN_ACTIVATE_POLYNOMIAL" },
            { "shipNet.node[47].params", "{0.000000, 0.000000, 1.000000, 1.000000, }" },
            { "shipNet.node[48].inputs", "{}" },
            { "shipNet.node[48].op", "ML_FOP_1x1_FMOD" },
            { "shipNet.node[48].params", "{}" },
            { "shipNet.node[49].inputs", "{}" },
            { "shipNet.node[49].op", "ML_FOP_3x0_CLAMP" },
            { "shipNet.node[49].params", "{1.000000, }" },
            { "shipNet.node[50].inputs", "{43, 37, 44, 49, }" },
            { "shipNet.node[50].op", "ML_FOP_Nx0_DIV_SUM" },
            { "shipNet.node[50].params", "{1.000000, 0.258113, 291.728546, 255.701035, 0.000000, }" },
            { "shipNet.node[51].inputs", "{25, 43, 45, 29, }" },
            { "shipNet.node[51].op", "ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP" },
            { "shipNet.node[51].params", "{0.000000, 0.000000, 144.050110, 0.000000, 1.000000, 0.972787, 0.000000, }" },
            { "shipNet.node[52].inputs", "{0, 46, 2, 18, }" },
            { "shipNet.node[52].op", "ML_FOP_1x0_EXP" },
            { "shipNet.node[52].params", "{2700.262939, 1239.031250, 0.094429, 0.000000, 8.878457, 1.000000, 3.889593, 1.000000, }" },
            { "shipNet.node[53].inputs", "{12, 8, 3, 13, }" },
            { "shipNet.node[53].op", "ML_FOP_Nx0_GEOMETRIC_MEAN" },
            { "shipNet.node[53].params", "{20.740566, 1.000000, 29.763569, 13.481079, 0.950000, 0.475516, 2950.105957, 0.000000, }" },
            { "shipNet.node[54].inputs", "{11, 0, 7, 13, 33, 35, 12, 41, }" },
            { "shipNet.node[54].op", "ML_FOP_2x2_IF_GTE_ELSE" },
            { "shipNet.node[54].params", "{1.000000, }" },
            { "shipNet.node[55].inputs", "{31, 11, 21, 25, 54, 29, 25, }" },
            { "shipNet.node[55].op", "ML_FOP_1x3_HYP_TANGENT" },
            { "shipNet.node[55].params", "{0.486350, }" },
            { "shipNet.node[56].inputs", "{0, }" },
            { "shipNet.node[56].op", "ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP" },
            { "shipNet.node[56].params", "{1.000000, 1009.547424, 0.000000, 0.000000, }" },
            { "shipNet.node[57].inputs", "{43, 45, 2, }" },
            { "shipNet.node[57].op", "ML_FOP_Nx0_ACTIVATE_SOFTPLUS" },
            { "shipNet.node[57].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[58].inputs", "{0, 15, 16, 37, 19, 32, 38, }" },
            { "shipNet.node[58].op", "ML_FOP_1x0_EXP" },
            { "shipNet.node[58].params", "{0.000000, 1.019409, 1.000000, 0.000000, 0.000000, 0.097884, 0.231568, }" },
            { "shipNet.node[59].inputs", "{49, 34, 52, 49, 36, }" },
            { "shipNet.node[59].op", "ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST" },
            { "shipNet.node[59].params", "{0.615245, }" },
            { "shipNet.node[60].inputs", "{23, 6, 44, }" },
            { "shipNet.node[60].op", "ML_FOP_1x0_NEGATE" },
            { "shipNet.node[60].params", "{1730.040283, 0.108184, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[61].inputs", "{37, 17, 55, 20, 40, 8, 13, }" },
            { "shipNet.node[61].op", "ML_FOP_1x2_SEEDED_RANDOM" },
            { "shipNet.node[61].params", "{0.000000, 204.888718, 0.000000, 0.000000, 0.000000, 2322.148682, 0.000000, 0.000000, }" },
            { "shipNet.node[62].inputs", "{45, 30, 9, 20, 24, 61, 27, 13, }" },
            { "shipNet.node[62].op", "ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT" },
            { "shipNet.node[62].params", "{1.488505, 1.561505, 147.017166, 0.731341, 1.000000, 0.000000, }" },
            { "shipNet.node[63].inputs", "{36, 9, 11, 62, 47, }" },
            { "shipNet.node[63].op", "ML_FOP_1x0_ABS" },
            { "shipNet.node[63].params", "{1119.231689, 330.688843, }" },
            { "shipNet.node[64].inputs", "{40, 60, 59, }" },
            { "shipNet.node[64].op", "ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_LERP" },
            { "shipNet.node[64].params", "{0.000000, 6479.723145, 144.099915, 5.269822, 0.117956, 294.487152, 1.000000, }" },
            { "shipNet.node[65].inputs", "{31, 29, 34, 7, 13, 54, 29, 29, }" },
            { "shipNet.node[65].op", "ML_FOP_Nx0_ACTIVATE_SQRT_UP" },
            { "shipNet.node[65].params", "{30.000000, }" },
            { "shipNet.node[66].inputs", "{}" },
            { "shipNet.node[66].op", "ML_FOP_1x1_STRICT_ON" },
            { "shipNet.node[66].params", "{0.000000, 0.589762, 0.000000, 17345.072266, 0.000000, 0.000000, 0.000000, 1.000000, }" },
            { "shipNet.node[67].inputs", "{14, 44, 17, 36, 17, }" },
            { "shipNet.node[67].op", "ML_FOP_1x1_LINEAR_COMBINATION" },
            { "shipNet.node[67].params", "{0.000000, 0.937990, 0.000000, 0.900000, }" },
            { "shipNet.node[68].inputs", "{44, 6, 51, 56, 63, 41, 3, }" },
            { "shipNet.node[68].op", "ML_FOP_1x0_CEIL" },
            { "shipNet.node[68].params", "{2960.583740, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[69].inputs", "{30, 51, 25, 62, 55, 37, 20, }" },
            { "shipNet.node[69].op", "ML_FOP_4x4_LINEAR_COMBINATION" },
            { "shipNet.node[69].params", "{1977.453247, }" },
            { "shipNet.node[70].inputs", "{42, 37, 68, 0, 11, }" },
            { "shipNet.node[70].op", "ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP" },
            { "shipNet.node[70].params", "{0.100199, 0.900000, 0.000000, }" },
            { "shipNet.node[71].inputs", "{}" },
            { "shipNet.node[71].op", "ML_FOP_1xN_POLYNOMIAL_CLAMPED_UNIT" },
            { "shipNet.node[71].params", "{}" },
            { "shipNet.node[72].inputs", "{60, 7, 41, }" },
            { "shipNet.node[72].op", "ML_FOP_1x2_OUTSIDE_RANGE" },
            { "shipNet.node[72].params", "{}" },
            { "shipNet.node[73].inputs", "{}" },
            { "shipNet.node[73].op", "ML_FOP_1xN_POLYNOMIAL_CLAMPED_UNIT" },
            { "shipNet.node[73].params", "{0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[74].inputs", "{37, 15, 59, 22, 47, 33, 25, }" },
            { "shipNet.node[74].op", "ML_FOP_1x3_ARC_COSINE" },
            { "shipNet.node[74].params", "{}" },
            { "shipNet.node[75].inputs", "{}" },
            { "shipNet.node[75].op", "ML_FOP_2x0_SUM" },
            { "shipNet.node[75].params", "{24.098963, 1904.422607, 0.000000, 0.798429, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[76].inputs", "{13, 67, 3, 13, }" },
            { "shipNet.node[76].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "shipNet.node[76].params", "{0.109540, 0.000000, 0.000000, 396.382080, 0.000000, }" },
            { "shipNet.node[77].inputs", "{13, }" },
            { "shipNet.node[77].op", "ML_FOP_1x0_HYP_SINE" },
            { "shipNet.node[77].params", "{0.855000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[78].inputs", "{12, }" },
            { "shipNet.node[78].op", "ML_FOP_NxN_SELECT_LTE" },
            { "shipNet.node[78].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[79].inputs", "{54, 70, 4, 75, }" },
            { "shipNet.node[79].op", "ML_FOP_2x0_PRODUCT" },
            { "shipNet.node[79].params", "{0.100938, 0.893583, 0.000000, 0.210219, 1.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[80].inputs", "{48, 57, 75, 40, 73, }" },
            { "shipNet.node[80].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "shipNet.node[80].params", "{3.804961, 257.136505, 1.542052, }" },
            { "shipNet.node[81].inputs", "{59, 65, 55, 49, 3, 30, }" },
            { "shipNet.node[81].op", "ML_FOP_1x3_ARC_COSINE" },
            { "shipNet.node[81].params", "{0.000000, 0.000000, 0.000000, 0.000000, 660.309509, 1.000000, 1.050000, }" },
            { "shipNet.node[82].inputs", "{70, 2, 61, 3, 51, }" },
            { "shipNet.node[82].op", "ML_FOP_Nx1_DIV_SUM" },
            { "shipNet.node[82].params", "{9895.462891, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[83].inputs", "{13, 32, 30, 71, 80, 18, }" },
            { "shipNet.node[83].op", "ML_FOP_1x1_FMOD" },
            { "shipNet.node[83].params", "{}" },
            { "shipNet.node[84].inputs", "{5, 8, 64, 54, 81, }" },
            { "shipNet.node[84].op", "ML_FOP_1x0_PROB_NOT" },
            { "shipNet.node[84].params", "{0.000000, 0.000000, }" },
            { "shipNet.node[85].inputs", "{27, 46, 78, 69, }" },
            { "shipNet.node[85].op", "ML_FOP_1x0_ARC_TANGENT" },
            { "shipNet.node[85].params", "{5684.448730, 0.000000, 1.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[86].inputs", "{84, 75, 73, 75, }" },
            { "shipNet.node[86].op", "ML_FOP_NxN_WEIGHTED_GEOMETRIC_MEAN" },
            { "shipNet.node[86].params", "{0.000000, 0.000000, 24.561216, 1035.908569, }" },
            { "shipNet.node[87].inputs", "{16, 44, }" },
            { "shipNet.node[87].op", "ML_FOP_2x0_CEIL_STEP" },
            { "shipNet.node[87].params", "{1.627296, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[88].inputs", "{2, 56, }" },
            { "shipNet.node[88].op", "ML_FOP_Nx0_SELECT_UNIT_INTERVAL_LERP" },
            { "shipNet.node[88].params", "{12.581761, 2.725593, }" },
            { "shipNet.node[89].inputs", "{69, 63, 15, }" },
            { "shipNet.node[89].op", "ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT" },
            { "shipNet.node[89].params", "{0.000000, 0.000000, 0.000000, 0.000000, 0.000000, }" },
            { "shipNet.node[90].inputs", "{78, 66, 84, 21, 3, 29, 12, }" },
            { "shipNet.node[90].op", "ML_FOP_Nx2_ACTIVATE_QUADRATIC_DOWN" },
            { "shipNet.node[90].params", "{}" },
            { "shipNet.node[91].inputs", "{}" },
            { "shipNet.node[91].op", "ML_FOP_1x0_CEIL" },
            { "shipNet.node[91].params", "{}" },
            { "shipNet.node[92].inputs", "{25, 2, }" },
            { "shipNet.node[92].op", "ML_FOP_1x2_INSIDE_RANGE" },
            { "shipNet.node[92].params", "{0.000000, 0.102678, 1.000000, }" },
            { "shipNet.node[93].inputs", "{31, 27, }" },
            { "shipNet.node[93].op", "ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_LERP" },
            { "shipNet.node[93].params", "{989.335571, }" },
            { "shipNet.node[94].inputs", "{43, 37, }" },
            { "shipNet.node[94].op", "ML_FOP_Nx3_ACTIVATE_GAUSSIAN" },
            { "shipNet.node[94].params", "{0.000000, 0.000000, 5.410668, 1.007700, }" },
            { "shipNet.node[95].inputs", "{6, 39, 81, 52, 20, }" },
            { "shipNet.node[95].op", "ML_FOP_Nx0_SUM" },
            { "shipNet.node[95].params", "{1.340968, 275.882324, 1.000000, 18354.156250, 2.035275, 0.000000, }" },
            { "shipNet.node[96].inputs", "{87, 46, 16, 90, 54, }" },
            { "shipNet.node[96].op", "ML_FOP_Nx0_MAX" },
            { "shipNet.node[96].params", "{2401.708252, 4017.525635, 1.846820, 1.474648, }" },
            { "shipNet.node[97].inputs", "{39, }" },
            { "shipNet.node[97].op", "ML_FOP_Nx0_MAX" },
            { "shipNet.node[97].params", "{}" },
            { "shipNet.node[98].inputs", "{}" },
            { "shipNet.node[98].op", "ML_FOP_2x2_IF_GTE_ELSE" },
            { "shipNet.node[98].params", "{0.000000, 0.000000, 1145.579712, 1385.257324, 0.000000, 0.000000, }" },
            { "shipNet.node[99].inputs", "{1, 30, 35, 62, 47, }" },
            { "shipNet.node[99].op", "ML_FOP_3x2_IF_INSIDE_RANGE_CONST_ELSE" },
            { "shipNet.node[99].params", "{0.000000, 2.573027, 0.000000, 0.688065, 1007.641113, }" },
            { "shipNet.numInputs", "25" },
            { "shipNet.numNodes", "100" },
            { "shipNet.numOutputs", "25" },
            { "gatherAbandonStale", "TRUE" },
            { "gatherRange", "53.190826" },
            { "guardRange", "87.259979" },
            { "shipNet.input[0].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[0].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "shipNet.input[0].radius", "0.000000" },
            { "shipNet.input[0].useTangent", "FALSE" },
            { "shipNet.input[0].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[10].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[10].forceType", "NEURAL_FORCE_ALIGN2" },
            { "shipNet.input[10].radius", "157.201385" },
            { "shipNet.input[10].useTangent", "FALSE" },
            { "shipNet.input[10].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[11].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[11].forceType", "NEURAL_FORCE_BASE" },
            { "shipNet.input[11].radius", "157.342880" },
            { "shipNet.input[11].useTangent", "TRUE" },
            { "shipNet.input[11].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[11].waveType", "NEURAL_WAVE_UNIT_SINE" },
            { "shipNet.input[12].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "shipNet.input[12].forceType", "NEURAL_FORCE_CORES" },
            { "shipNet.input[12].frequency", "0.000000" },
            { "shipNet.input[12].radius", "0.000000" },
            { "shipNet.input[12].useTangent", "TRUE" },
            { "shipNet.input[12].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[12].waveType", "NEURAL_WAVE_SINE" },
            { "shipNet.input[13].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[13].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "shipNet.input[13].frequency", "0.000000" },
            { "shipNet.input[13].radius", "0.000000" },
            { "shipNet.input[13].useTangent", "TRUE" },
            { "shipNet.input[13].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[13].waveType", "NEURAL_WAVE_SINE" },
            { "shipNet.input[14].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[14].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "shipNet.input[14].frequency", "0.000000" },
            { "shipNet.input[14].radius", "164.939926" },
            { "shipNet.input[14].useTangent", "FALSE" },
            { "shipNet.input[14].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[14].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[15].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[15].forceType", "NEURAL_FORCE_COHERE" },
            { "shipNet.input[15].radius", "156.087997" },
            { "shipNet.input[15].useTangent", "FALSE" },
            { "shipNet.input[15].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[16].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[16].forceType", "NEURAL_FORCE_CENTER" },
            { "shipNet.input[16].frequency", "0.000000" },
            { "shipNet.input[16].radius", "0.000000" },
            { "shipNet.input[16].useTangent", "FALSE" },
            { "shipNet.input[16].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "shipNet.input[17].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[17].forceType", "NEURAL_FORCE_BASE" },
            { "shipNet.input[17].frequency", "4093.644043" },
            { "shipNet.input[17].radius", "0.000000" },
            { "shipNet.input[17].useTangent", "FALSE" },
            { "shipNet.input[17].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[17].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[18].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "shipNet.input[18].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "shipNet.input[18].frequency", "0.000000" },
            { "shipNet.input[18].radius", "147.504044" },
            { "shipNet.input[18].useTangent", "FALSE" },
            { "shipNet.input[18].valueType", "NEURAL_VALUE_MOBID" },
            { "shipNet.input[18].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[19].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[19].forceType", "NEURAL_FORCE_ZERO" },
            { "shipNet.input[19].frequency", "0.000000" },
            { "shipNet.input[19].radius", "144.802826" },
            { "shipNet.input[19].useTangent", "FALSE" },
            { "shipNet.input[19].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[19].waveType", "NEURAL_WAVE_UNIT_SINE" },
            { "shipNet.input[1].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[1].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "shipNet.input[1].radius", "301.750824" },
            { "shipNet.input[1].useTangent", "TRUE" },
            { "shipNet.input[1].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[20].crowdType", "NEURAL_CROWD_ENEMY_MISSILE" },
            { "shipNet.input[20].forceType", "NEURAL_FORCE_ENEMY_BASE" },
            { "shipNet.input[20].radius", "0.000000" },
            { "shipNet.input[20].useTangent", "FALSE" },
            { "shipNet.input[20].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "shipNet.input[20].waveType", "NEURAL_WAVE_FMOD" },
            { "shipNet.input[21].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "shipNet.input[21].forceType", "NEURAL_FORCE_ENEMY" },
            { "shipNet.input[21].frequency", "0.000000" },
            { "shipNet.input[21].radius", "-1.000000" },
            { "shipNet.input[21].useTangent", "TRUE" },
            { "shipNet.input[21].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[22].crowdType", "NEURAL_CROWD_CORES" },
            { "shipNet.input[22].forceType", "NEURAL_FORCE_ALIGN2" },
            { "shipNet.input[22].radius", "6.802381" },
            { "shipNet.input[22].useTangent", "FALSE" },
            { "shipNet.input[22].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[22].waveType", "NEURAL_WAVE_NONE" },
            { "shipNet.input[23].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[23].forceType", "NEURAL_FORCE_CENTER" },
            { "shipNet.input[23].frequency", "0.000000" },
            { "shipNet.input[23].radius", "-1.000000" },
            { "shipNet.input[23].useTangent", "FALSE" },
            { "shipNet.input[23].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[23].waveType", "NEURAL_WAVE_NONE" },
            { "shipNet.input[24].crowdType", "NEURAL_CROWD_ENEMY_SHIP" },
            { "shipNet.input[24].forceType", "NEURAL_FORCE_SEPARATE" },
            { "shipNet.input[24].frequency", "0.000000" },
            { "shipNet.input[24].radius", "1086.721436" },
            { "shipNet.input[24].useTangent", "TRUE" },
            { "shipNet.input[24].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.input[2].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[2].forceType", "NEURAL_FORCE_CORES" },
            { "shipNet.input[2].radius", "148.709793" },
            { "shipNet.input[2].useTangent", "FALSE" },
            { "shipNet.input[2].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[3].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[3].forceType", "NEURAL_FORCE_BASE" },
            { "shipNet.input[3].radius", "-1.000000" },
            { "shipNet.input[3].useTangent", "FALSE" },
            { "shipNet.input[3].valueType", "NEURAL_VALUE_CREDITS" },
            { "shipNet.input[4].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[4].forceType", "NEURAL_FORCE_NEAREST_EDGE" },
            { "shipNet.input[4].frequency", "5622.903809" },
            { "shipNet.input[4].radius", "0.000000" },
            { "shipNet.input[4].useTangent", "FALSE" },
            { "shipNet.input[4].valueType", "NEURAL_VALUE_RANDOM_UNIT" },
            { "shipNet.input[4].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "shipNet.input[5].crowdType", "NEURAL_CROWD_FRIEND_FIGHTER" },
            { "shipNet.input[5].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "shipNet.input[5].frequency", "0.000000" },
            { "shipNet.input[5].radius", "0.000000" },
            { "shipNet.input[5].useTangent", "TRUE" },
            { "shipNet.input[5].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[5].waveType", "NEURAL_WAVE_SINE" },
            { "shipNet.input[6].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[6].forceType", "NEURAL_FORCE_HEADING" },
            { "shipNet.input[6].frequency", "0.000000" },
            { "shipNet.input[6].radius", "-0.950000" },
            { "shipNet.input[6].useTangent", "FALSE" },
            { "shipNet.input[6].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[6].waveType", "NEURAL_WAVE_NONE" },
            { "shipNet.input[7].crowdType", "NEURAL_CROWD_BASE_FRIEND_SHIP" },
            { "shipNet.input[7].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "shipNet.input[7].radius", "4.193549" },
            { "shipNet.input[7].useTangent", "TRUE" },
            { "shipNet.input[7].valueType", "NEURAL_VALUE_FRIEND_SHIPS" },
            { "shipNet.input[8].crowdType", "NEURAL_CROWD_BASE_ENEMY_SHIP" },
            { "shipNet.input[8].forceType", "NEURAL_FORCE_ENEMY_BASE_GUESS" },
            { "shipNet.input[8].frequency", "0.000000" },
            { "shipNet.input[8].radius", "0.000000" },
            { "shipNet.input[8].useTangent", "FALSE" },
            { "shipNet.input[8].valueType", "NEURAL_VALUE_CROWD" },
            { "shipNet.input[8].waveType", "NEURAL_WAVE_ABS_SINE" },
            { "shipNet.input[9].crowdType", "NEURAL_CROWD_FRIEND_MISSILE" },
            { "shipNet.input[9].forceType", "NEURAL_FORCE_HEADING" },
            { "shipNet.input[9].radius", "154.979477" },
            { "shipNet.input[9].useTangent", "TRUE" },
            { "shipNet.input[9].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[100].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "shipNet.output[100].radius", "529.224609" },
            { "shipNet.output[100].useTangent", "TRUE" },
            { "shipNet.output[100].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[101].forceType", "NEURAL_FORCE_ZERO" },
            { "shipNet.output[101].radius", "1351.212036" },
            { "shipNet.output[101].useTangent", "FALSE" },
            { "shipNet.output[101].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[102].forceType", "NEURAL_FORCE_SEPARATE" },
            { "shipNet.output[102].radius", "167.162323" },
            { "shipNet.output[102].useTangent", "FALSE" },
            { "shipNet.output[102].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[103].forceType", "NEURAL_FORCE_NEAREST_CORNER" },
            { "shipNet.output[103].radius", "-1.000000" },
            { "shipNet.output[103].useTangent", "FALSE" },
            { "shipNet.output[103].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[104].forceType", "NEURAL_FORCE_ENEMY" },
            { "shipNet.output[104].radius", "1729.684937" },
            { "shipNet.output[104].useTangent", "FALSE" },
            { "shipNet.output[104].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[105].forceType", "NEURAL_FORCE_NEAREST_FRIEND_MISSILE" },
            { "shipNet.output[105].radius", "278.047913" },
            { "shipNet.output[105].useTangent", "FALSE" },
            { "shipNet.output[105].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[106].forceType", "NEURAL_FORCE_ALIGN2" },
            { "shipNet.output[106].radius", "2541.187500" },
            { "shipNet.output[106].useTangent", "FALSE" },
            { "shipNet.output[106].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[107].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "shipNet.output[107].radius", "-1.000000" },
            { "shipNet.output[107].useTangent", "TRUE" },
            { "shipNet.output[107].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[108].forceType", "NEURAL_FORCE_ALIGN2" },
            { "shipNet.output[108].radius", "-0.950000" },
            { "shipNet.output[108].useTangent", "FALSE" },
            { "shipNet.output[108].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[109].forceType", "NEURAL_FORCE_HEADING" },
            { "shipNet.output[109].radius", "1411.446289" },
            { "shipNet.output[109].useTangent", "FALSE" },
            { "shipNet.output[109].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[110].forceType", "NEURAL_FORCE_COHERE" },
            { "shipNet.output[110].radius", "2590.880371" },
            { "shipNet.output[110].useTangent", "FALSE" },
            { "shipNet.output[110].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[111].forceType", "NEURAL_FORCE_ALIGN2" },
            { "shipNet.output[111].radius", "151.967468" },
            { "shipNet.output[111].useTangent", "TRUE" },
            { "shipNet.output[111].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[112].forceType", "NEURAL_FORCE_HEADING" },
            { "shipNet.output[112].radius", "296.518951" },
            { "shipNet.output[112].useTangent", "FALSE" },
            { "shipNet.output[112].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[113].forceType", "NEURAL_FORCE_ZERO" },
            { "shipNet.output[113].radius", "-1.000000" },
            { "shipNet.output[113].useTangent", "TRUE" },
            { "shipNet.output[113].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[114].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "shipNet.output[114].radius", "322.285767" },
            { "shipNet.output[114].useTangent", "FALSE" },
            { "shipNet.output[114].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[115].forceType", "NEURAL_FORCE_HEADING" },
            { "shipNet.output[115].radius", "-1.000000" },
            { "shipNet.output[115].useTangent", "FALSE" },
            { "shipNet.output[115].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[116].forceType", "NEURAL_FORCE_CORES" },
            { "shipNet.output[116].radius", "0.000000" },
            { "shipNet.output[116].useTangent", "TRUE" },
            { "shipNet.output[116].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[117].forceType", "NEURAL_FORCE_COHERE" },
            { "shipNet.output[117].radius", "1002.034180" },
            { "shipNet.output[117].useTangent", "TRUE" },
            { "shipNet.output[117].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[118].forceType", "NEURAL_FORCE_ENEMY_MISSILE" },
            { "shipNet.output[118].radius", "298.878693" },
            { "shipNet.output[118].useTangent", "FALSE" },
            { "shipNet.output[118].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[119].forceType", "NEURAL_FORCE_ALIGN2" },
            { "shipNet.output[119].radius", "2850.000000" },
            { "shipNet.output[119].useTangent", "FALSE" },
            { "shipNet.output[119].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[120].forceType", "NEURAL_FORCE_NEAREST_FRIEND" },
            { "shipNet.output[120].radius", "2963.549072" },
            { "shipNet.output[120].useTangent", "FALSE" },
            { "shipNet.output[120].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[121].forceType", "NEURAL_FORCE_ENEMY" },
            { "shipNet.output[121].radius", "-1.000000" },
            { "shipNet.output[121].useTangent", "FALSE" },
            { "shipNet.output[121].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[122].forceType", "NEURAL_FORCE_ENEMY_COHERE" },
            { "shipNet.output[122].radius", "1693.586426" },
            { "shipNet.output[122].useTangent", "FALSE" },
            { "shipNet.output[122].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[123].forceType", "NEURAL_FORCE_BASE_DEFENSE" },
            { "shipNet.output[123].radius", "1030.571045" },
            { "shipNet.output[123].useTangent", "FALSE" },
            { "shipNet.output[123].valueType", "NEURAL_VALUE_FORCE" },
            { "shipNet.output[124].forceType", "NEURAL_FORCE_ALIGN2" },
            { "shipNet.output[124].radius", "-1.000000" },
            { "shipNet.output[124].useTangent", "FALSE" },
            { "shipNet.output[124].valueType", "NEURAL_VALUE_FORCE" },
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
        myShipNet.load(mreg, "shipNet.", NN_TYPE_FORCES);
        myFleetNet.load(mreg, "fleetNet.", NN_TYPE_SCALARS);
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
