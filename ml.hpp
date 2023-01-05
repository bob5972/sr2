/*
 * ml.hpp -- part of SpaceRobots2
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

#ifndef _ML_H_202208151722
#define _ML_H_202208151722

#include "MBRegistry.h"
#include "MBVector.hpp"

/*
 * N Inputs X M Parameters => 1 Output
 */
typedef enum MLFloatOp {
    ML_FOP_INVALID = 0,
    ML_FOP_VOID = 1,
    ML_FOP_INPUT = 2,

    ML_FOP_0x0_ZERO = 3,
    ML_FOP_MIN = 3,
    ML_FOP_0x0_ONE,

    ML_FOP_0x1_CONSTANT,
    ML_FOP_0x1_UNIT_CONSTANT,
    ML_FOP_0x3_CONSTANT_CLAMPED,
    ML_FOP_0x1_CONSTANT_0_10,
    ML_FOP_0x1_CONSTANT_0_100,
    ML_FOP_0x1_CONSTANT_0_1K,
    ML_FOP_0x1_CONSTANT_0_10K,
    ML_FOP_0x1_CONSTANT_N10_0,
    ML_FOP_0x1_CONSTANT_N100_0,
    ML_FOP_0x1_CONSTANT_N1K_0,
    ML_FOP_0x1_CONSTANT_N10K_0,
    ML_FOP_0x1_CONSTANT_N10_10,
    ML_FOP_0x1_CONSTANT_N100_100,
    ML_FOP_0x1_CONSTANT_N1K_1K,
    ML_FOP_0x1_CONSTANT_N10K_10K,

    ML_FOP_1x0_IDENTITY,
    ML_FOP_1x0_NEGATE,
    ML_FOP_1x0_SEEDED_RANDOM_UNIT,
    ML_FOP_1x0_SQRT,
    ML_FOP_1x0_ARC_COSINE,
    ML_FOP_1x0_ARC_SINE,
    ML_FOP_1x0_ARC_TANGENT,
    ML_FOP_1x0_HYP_COSINE,
    ML_FOP_1x0_HYP_SINE,
    ML_FOP_1x0_HYP_TANGENT,
    ML_FOP_1x0_EXP,
    ML_FOP_1x0_LN,
    ML_FOP_1x0_ABS,
    ML_FOP_1x0_SIN,
    ML_FOP_1x0_UNIT_SINE,
    ML_FOP_1x0_ABS_SINE,
    ML_FOP_1x0_COS,
    ML_FOP_1x0_TAN,
    ML_FOP_1x0_PROB_NOT,

    ML_FOP_1x0_INVERSE,
    ML_FOP_1x0_SQUARE,
    ML_FOP_1x0_INVERSE_SQUARE,
    ML_FOP_1x1_WEIGHTED_INVERSE_SQUARE,

    ML_FOP_1x0_CEIL,
    ML_FOP_1x0_FLOOR,
    ML_FOP_1x1_CEIL_STEP,
    ML_FOP_1x1_FLOOR_STEP,
    ML_FOP_2x0_CEIL_STEP,
    ML_FOP_2x0_FLOOR_STEP,

    ML_FOP_1x1_STRICT_ON,
    ML_FOP_1x1_STRICT_OFF,
    ML_FOP_1x1_LINEAR_UP,
    ML_FOP_1x1_LINEAR_DOWN,
    ML_FOP_1x1_QUADRATIC_UP,
    ML_FOP_1x1_QUADRATIC_DOWN,
    ML_FOP_1x1_FMOD,
    ML_FOP_1x1_GTE,
    ML_FOP_1x1_LTE,
    ML_FOP_1x1_PRODUCT,
    ML_FOP_1x1_SUM,

    ML_FOP_1x0_CLAMP_UNIT,
    ML_FOP_1x2_CLAMP,
    ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT,
    ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT,
    ML_FOP_3x0_CLAMP,

    ML_FOP_1x2_SINE,
    ML_FOP_1x2_COSINE,
    ML_FOP_1x2_INSIDE_RANGE,
    ML_FOP_1x2_OUTSIDE_RANGE,
    ML_FOP_1x2_SEEDED_RANDOM,

    ML_FOP_1x3_IF_GTE_ELSE,
    ML_FOP_1x3_IF_LTE_ELSE,
    ML_FOP_1x3_SQUARE,
    ML_FOP_1x3_SQRT,
    ML_FOP_1x3_ARC_SINE,
    ML_FOP_1x3_ARC_TANGENT,
    ML_FOP_1x3_ARC_COSINE,
    ML_FOP_1x3_HYP_COSINE,
    ML_FOP_1x3_HYP_SINE,
    ML_FOP_1x3_HYP_TANGENT,
    ML_FOP_1x3_EXP,
    ML_FOP_1x3_LN,
    ML_FOP_1x3_SIN,
    ML_FOP_1x3_COS,
    ML_FOP_1x3_TAN,

    ML_FOP_1x1_POW,
    ML_FOP_2x0_POW,
    ML_FOP_1x2_POW,
    ML_FOP_3x0_POW,

    ML_FOP_2x0_SUM,
    ML_FOP_2x0_SQUARE_SUM,
    ML_FOP_2x0_PRODUCT,

    ML_FOP_1x4_IF_INSIDE_RANGE_ELSE,
    ML_FOP_1x4_IF_OUTSIDE_RANGE_ELSE,

    ML_FOP_2x2_IF_GTE_ELSE,
    ML_FOP_2x2_IF_LTE_ELSE,

    ML_FOP_3x0_IF_GTEZ_ELSE,
    ML_FOP_3x0_IF_LTEZ_ELSE,

    ML_FOP_3x2_IF_INSIDE_RANGE_CONST_ELSE,
    ML_FOP_3x2_IF_OUTSIDE_RANGE_CONST_ELSE,
    ML_FOP_3x2_IF_INSIDE_RANGE_ELSE_CONST,
    ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST,

    ML_FOP_4x0_IF_GTE_ELSE,
    ML_FOP_4x0_IF_LTE_ELSE,

    ML_FOP_5x0_IF_INSIDE_RANGE_ELSE,
    ML_FOP_5x0_IF_OUTSIDE_RANGE_ELSE,

    ML_FOP_Nx0_SUM,
    ML_FOP_Nx0_PRODUCT,
    ML_FOP_Nx0_MIN,
    ML_FOP_Nx0_MAX,

    ML_FOP_Nx0_ARITHMETIC_MEAN,
    ML_FOP_Nx0_GEOMETRIC_MEAN,
    ML_FOP_NxN_WEIGHTED_ARITHMETIC_MEAN,
    ML_FOP_NxN_WEIGHTED_GEOMETRIC_MEAN,
    ML_FOP_NxN_ANCHORED_ARITHMETIC_MEAN,
    ML_FOP_NxN_ANCHORED_GEOMETRIC_MEAN,

    ML_FOP_Nx0_DIV_SUM,
    ML_FOP_Nx1_DIV_SUM,
    ML_FOP_NxN_SCALED_DIV_SUM,
    ML_FOP_NxN_ANCHORED_DIV_SUM,
    ML_FOP_Nx0_DIV_SUM_SQUARED,
    ML_FOP_Nx1_DIV_SUM_SQUARED,
    ML_FOP_NxN_SCALED_DIV_SUM_SQUARED,
    ML_FOP_NxN_ANCHORED_DIV_SUM_SQUARED,

    ML_FOP_Nx0_INVERSE_SQUARE_SUM,
    ML_FOP_NxN_WEIGHTED_INVERSE_SQUARE_SUM,

    ML_FOP_Nx1_ACTIVATE_THRESHOLD_UP,
    ML_FOP_Nx1_ACTIVATE_THRESHOLD_DOWN,
    ML_FOP_Nx2_ACTIVATE_LINEAR_UP,
    ML_FOP_Nx2_ACTIVATE_LINEAR_DOWN,
    ML_FOP_Nx2_ACTIVATE_QUADRATIC_UP,
    ML_FOP_Nx2_ACTIVATE_QUADRATIC_DOWN,
    ML_FOP_Nx0_ACTIVATE_SQRT_UP,
    ML_FOP_Nx0_ACTIVATE_SQRT_DOWN,
    ML_FOP_Nx0_ACTIVATE_LN_UP,
    ML_FOP_Nx0_ACTIVATE_LN_DOWN,
    ML_FOP_NxN_ACTIVATE_POLYNOMIAL,
    ML_FOP_Nx0_ACTIVATE_DIV_SUM,
    ML_FOP_Nx1_ACTIVATE_DIV_SUM,
    ML_FOP_Nx0_ACTIVATE_HYP_TANGENT,
    ML_FOP_Nx0_ACTIVATE_LOGISTIC,
    ML_FOP_Nx0_ACTIVATE_SOFTPLUS,
    ML_FOP_Nx0_ACTIVATE_GAUSSIAN,
    ML_FOP_Nx0_ACTIVATE_GAUSSIAN_PROB_INVERSE,
    ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP,
    ML_FOP_Nx0_ACTIVATE_GAUSSIAN_UP_PROB_INVERSE,
    ML_FOP_Nx2_ACTIVATE_GAUSSIAN,
    ML_FOP_Nx3_ACTIVATE_GAUSSIAN,
    ML_FOP_Nx0_ACTIVATE_INVERSE_SQUARE,
    ML_FOP_NxN_ACTIVATE_INVERSE_SQUARE,

    ML_FOP_NxN_SCALED_MIN,
    ML_FOP_NxN_SCALED_MAX,
    ML_FOP_Nx1_POW_SUM,
    ML_FOP_NxN_POW_SUM,
    ML_FOP_Nx2N_WEIGHTED_POW_SUM,

    ML_FOP_1xN_POLYNOMIAL,
    ML_FOP_1xN_POLYNOMIAL_CLAMPED_UNIT,

    ML_FOP_1x1_LINEAR_COMBINATION,
    ML_FOP_2x2_LINEAR_COMBINATION,
    ML_FOP_3x3_LINEAR_COMBINATION,
    ML_FOP_4x4_LINEAR_COMBINATION,
    ML_FOP_NxN_LINEAR_COMBINATION,
    ML_FOP_NxN_LINEAR_COMBINATION_CLAMPED_UNIT,

    ML_FOP_NxN_SELECT_GTE,
    ML_FOP_NxN_SELECT_LTE,

    ML_FOP_1xN_SELECT_UNIT_INTERVAL_STEP,
    ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP,
    ML_FOP_Nx0_SELECT_UNIT_INTERVAL_STEP,
    ML_FOP_Nx0_SELECT_UNIT_INTERVAL_LERP,
    ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_STEP,
    ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_LERP,

    ML_FOP_MAX,
} MLFloatOp;

extern "C" {
    const char *ML_FloatOpToString(MLFloatOp op);
    MLFloatOp ML_StringToFloatOp(const char *opstr);

    void ML_UnitTest();
};

void MLFloatOp_GetNumParams(MLFloatOp op, uint *numInputsP, uint *numParamsP);

static inline MLFloatOp& operator++(MLFloatOp &op)
{
    int i = static_cast<int>(op);
    return op = static_cast<MLFloatOp>(++i);
}

static inline MLFloatOp operator++(MLFloatOp &op, int) {
    MLFloatOp tmp(op);
    ++op;
    return tmp;
}

class MLFloatNode {
    public:
        MLFloatNode()
        : index(-1), op(ML_FOP_INVALID), myValues(NULL)
        {}

        uint index;
        MLFloatOp op;
        MBVector<float> params;
        MBVector<uint> inputs;

        float compute(const MBVector<float> &values);

        void load(MBRegistry *mreg, const char *prefix);
        void mutate(float rate, uint maxInputs, uint maxParams);
        void save(MBRegistry *mreg, const char *prefix);

        void minimize();
        void makeVoid();
        bool isZero() { return op == ML_FOP_0x0_ZERO; }
        bool isVoid() { return op == ML_FOP_VOID; }

        bool isConstant();
        bool isInput() { return op == ML_FOP_INPUT; }
        void makeConstant(float f) {
            op = ML_FOP_0x1_CONSTANT;
            inputs.resize(0);
            params.resize(1);
            params[0] = f;
        }

    private:
        const MBVector<float> *myValues;

        float computeWork();

        float getInput(uint i) {
            if (mb_debug) {
                if (UNLIKELY(i >= inputs.size())) {
                    PANIC("Input out of range: i=%d, numInputs=%d, op=%s(%d)\n",
                          i, inputs.size(), ML_FloatOpToString(op), op);
                }
            }

            ASSERT(myValues != NULL);
            ASSERT(i < inputs.size());
            ASSERT(index < myValues->size());
            ASSERT(inputs[i] < index);
            return myValues->get(inputs[i]);
        }

        float getParam(uint i) {
            if (mb_debug) {
                if (UNLIKELY(i >= params.size())) {
                    PANIC("Param out of range: i=%d, numParams=%d, op=%s(%d)\n",
                          i, params.size(), ML_FloatOpToString(op), op);
                }
            }
            ASSERT(i < params.size());
            return params[i];
        }
};


#endif // _ML_H_202208151722