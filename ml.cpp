/*
 * ml.cpp -- part of SpaceRobots2
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

#include <math.h>

#include "ml.hpp"
#include "textDump.hpp"
#include "Random.h"
#include "mutate.h"

static TextMapEntry tmMLFloatOps[] = {
    { TMENTRY(ML_FOP_INVALID), },
    { TMENTRY(ML_FOP_0x0_ZERO), },
    { TMENTRY(ML_FOP_0x0_ONE), },
    { TMENTRY(ML_FOP_0x1_CONSTANT), },
    { TMENTRY(ML_FOP_1x0_IDENTITY), },
    { TMENTRY(ML_FOP_1x0_INVERSE), },
    { TMENTRY(ML_FOP_1x0_NEGATE), },
    { TMENTRY(ML_FOP_1x0_SEEDED_RANDOM_UNIT), },
    { TMENTRY(ML_FOP_1x0_SQUARE), },
    { TMENTRY(ML_FOP_1x0_SQRT), },
    { TMENTRY(ML_FOP_1x0_ARC_COSINE), },
    { TMENTRY(ML_FOP_1x0_ARC_SINE), },
    { TMENTRY(ML_FOP_1x0_ARC_TANGENT), },
    { TMENTRY(ML_FOP_1x0_HYP_COSINE), },
    { TMENTRY(ML_FOP_1x0_HYP_SINE), },
    { TMENTRY(ML_FOP_1x0_HYP_TANGENT), },
    { TMENTRY(ML_FOP_1x0_EXP), },
    { TMENTRY(ML_FOP_1x0_LN), },
    { TMENTRY(ML_FOP_1x0_CEIL), },
    { TMENTRY(ML_FOP_1x0_FLOOR), },
    { TMENTRY(ML_FOP_1x0_ABS), },
    { TMENTRY(ML_FOP_1x0_SIN), },
    { TMENTRY(ML_FOP_1x0_COS), },
    { TMENTRY(ML_FOP_1x0_TAN), },
    { TMENTRY(ML_FOP_1x0_PROB_NOT), },

    { TMENTRY(ML_FOP_1x1_STRICT_ON), },
    { TMENTRY(ML_FOP_1x1_STRICT_OFF), },
    { TMENTRY(ML_FOP_1x1_LINEAR_UP), },
    { TMENTRY(ML_FOP_1x1_LINEAR_DOWN), },
    { TMENTRY(ML_FOP_1x1_QUADRATIC_UP), },
    { TMENTRY(ML_FOP_1x1_QUADRATIC_DOWN), },
    { TMENTRY(ML_FOP_1x1_FMOD), },
    { TMENTRY(ML_FOP_1x1_POW), },
    { TMENTRY(ML_FOP_1x1_GTE), },
    { TMENTRY(ML_FOP_1x1_LTE), },
    { TMENTRY(ML_FOP_1x1_LINEAR_COMBINATION), },

    { TMENTRY(ML_FOP_1x2_CLAMP), },
    { TMENTRY(ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT), },
    { TMENTRY(ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT), },
    { TMENTRY(ML_FOP_1x2_SINE), },
    { TMENTRY(ML_FOP_1x2_COSINE), },
    { TMENTRY(ML_FOP_1x2_INSIDE_RANGE), },
    { TMENTRY(ML_FOP_1x2_OUTSIDE_RANGE), },
    { TMENTRY(ML_FOP_1x2_SEEDED_RANDOM), },

    { TMENTRY(ML_FOP_1x3_IF_GTE_ELSE), },
    { TMENTRY(ML_FOP_1x3_IF_LTE_ELSE), },
    { TMENTRY(ML_FOP_1x3_SQUARE), },
    { TMENTRY(ML_FOP_1x3_SQRT), },
    { TMENTRY(ML_FOP_1x3_ARC_SINE), },
    { TMENTRY(ML_FOP_1x3_ARC_TANGENT), },
    { TMENTRY(ML_FOP_1x3_ARC_COSINE), },
    { TMENTRY(ML_FOP_1x3_HYP_COSINE), },
    { TMENTRY(ML_FOP_1x3_HYP_SINE), },
    { TMENTRY(ML_FOP_1x3_HYP_TANGENT), },
    { TMENTRY(ML_FOP_1x3_EXP), },
    { TMENTRY(ML_FOP_1x3_LN), },
    { TMENTRY(ML_FOP_1x3_SIN), },
    { TMENTRY(ML_FOP_1x3_COS), },
    { TMENTRY(ML_FOP_1x3_TAN), },

    { TMENTRY(ML_FOP_1xN_SELECT_UNIT_INTERVAL_STEP), },
    { TMENTRY(ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP), },

    { TMENTRY(ML_FOP_2x0_POW), },
    { TMENTRY(ML_FOP_2x0_SUM), },
    { TMENTRY(ML_FOP_2x0_SQUARE_SUM), },
    { TMENTRY(ML_FOP_2x0_PRODUCT), },

    { TMENTRY(ML_FOP_2x2_LINEAR_COMBINATION), },
    { TMENTRY(ML_FOP_2x2_IF_GTE_ELSE), },
    { TMENTRY(ML_FOP_2x2_IF_LTE_ELSE), },

    { TMENTRY(ML_FOP_3x0_IF_GTEZ_ELSE), },
    { TMENTRY(ML_FOP_3x0_IF_LTEZ_ELSE), },

    { TMENTRY(ML_FOP_3x2_IF_INSIDE_RANGE_CONST_ELSE), },
    { TMENTRY(ML_FOP_3x2_IF_OUTSIDE_RANGE_CONST_ELSE), },
    { TMENTRY(ML_FOP_3x2_IF_INSIDE_RANGE_ELSE_CONST), },
    { TMENTRY(ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST), },

    { TMENTRY(ML_FOP_3x3_LINEAR_COMBINATION), },

    { TMENTRY(ML_FOP_4x0_IF_GTE_ELSE), },
    { TMENTRY(ML_FOP_4x0_IF_LTE_ELSE), },

    { TMENTRY(ML_FOP_4x4_LINEAR_COMBINATION), },

    { TMENTRY(ML_FOP_5x0_IF_INSIDE_RANGE_ELSE), },
    { TMENTRY(ML_FOP_5x0_IF_OUTSIDE_RANGE_ELSE), },

    { TMENTRY(ML_FOP_Nx0_SUM), },
    { TMENTRY(ML_FOP_Nx0_PRODUCT), },
    { TMENTRY(ML_FOP_Nx0_MIN), },
    { TMENTRY(ML_FOP_Nx0_MAX), },
    { TMENTRY(ML_FOP_Nx0_ARITHMETIC_MEAN), },
    { TMENTRY(ML_FOP_Nx0_GEOMETRIC_MEAN), },
    { TMENTRY(ML_FOP_Nx0_DIV_SUM), },
    { TMENTRY(ML_FOP_Nx0_SELECT_UNIT_INTERVAL_STEP), },
    { TMENTRY(ML_FOP_Nx0_SELECT_UNIT_INTERVAL_LERP), },

    { TMENTRY(ML_FOP_Nx1_DIV_SUM), },

    { TMENTRY(ML_FOP_NxN_LINEAR_COMBINATION), },
    { TMENTRY(ML_FOP_NxN_SCALED_MIN), },
    { TMENTRY(ML_FOP_NxN_SCALED_MAX), },
    { TMENTRY(ML_FOP_NxN_SELECT_GTE), },
    { TMENTRY(ML_FOP_NxN_SELECT_LTE), },
    { TMENTRY(ML_FOP_NxN_POW_SUM), },
    { TMENTRY(ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_STEP), },
};

float MLFloatCheck1x1(MLFloatOp op, float input,
                      float param) {
    if (op == ML_FOP_0x0_ZERO) {
        return 0.0f;
    } else if (op == ML_FOP_0x0_ONE) {
        return 1.0f;
    } else if (isnanf(param) || isnanf(input)) {
        /*
            * Throw out malformed inputs after checking for ALWAYS/NEVER.
            */
        return 0.0f;
    } else if (op == ML_FOP_1x1_STRICT_ON) {
        if (input >= param) {
            return 1.0f;
        } else {
            return 0.0f;
        }
    } else if (op == ML_FOP_1x1_STRICT_OFF) {
        if (input >= param) {
            return 0.0f;
        } else {
            return 1.0f;
        }
    }


    float localWeight;
    if (param <= 0.0f) {
        if (op == ML_FOP_1x1_LINEAR_DOWN ||
            op == ML_FOP_1x1_QUADRATIC_DOWN) {
            /*
             * For the DOWN checks, the force decreases to zero as the param
             * approaches zero, so it makes sense to treat negative numbers
             * as a disabled check.
             */
            return 0.0f;
        } else {
            ASSERT(op == ML_FOP_1x1_LINEAR_UP ||
                   op == ML_FOP_1x1_QUADRATIC_UP);

            /*
             * For the UP checks, the force input should be infinite as the
             * param approaches zero, so use our clamped max force.
             */
            return INFINITY;
        }
    } else if (input <= 0.0f) {
        /*
         * These are the reverse of the param <= 0 checks above.
         */
        if (op == ML_FOP_1x1_LINEAR_DOWN ||
            op == ML_FOP_1x1_QUADRATIC_DOWN) {
            return INFINITY;
        } else {
            ASSERT(op == ML_FOP_1x1_LINEAR_UP ||
                    op == ML_FOP_1x1_QUADRATIC_UP);
            return 0.0f;
        }
    } else if (op == ML_FOP_1x1_LINEAR_UP) {
        localWeight = input / param;
    } else if (op == ML_FOP_1x1_LINEAR_DOWN) {
        localWeight = param / input;
    } else if (op == ML_FOP_1x1_QUADRATIC_UP) {
        float x = input / param;
        localWeight = x * x;
    } else if (op == ML_FOP_1x1_QUADRATIC_DOWN) {
        float x = param / input;
        localWeight = x * x;
    } else {
        PANIC("Unknown 1x1 MLFloatOp: %d\n", op);
    }

    if (localWeight <= 0.0f || isnanf(localWeight)) {
        localWeight = 0.0f;
    }

    return localWeight;
}

float MLFloatNode::compute(const MBVector<float> &values)
{
    float f;

    myValues = &values;
    f = computeWork(values);
    myValues = NULL;

    return f;
}


float MLFloatNode::computeWork(const MBVector<float> &values)
{
    if (mb_debug && ML_FOP_MAX != 97) {
        PANIC("ML_FOP_MAX=%d\n", ML_FOP_MAX);
    }

    switch (op) {
        case ML_FOP_0x0_ZERO:
            return 0.0f;
        case ML_FOP_0x0_ONE:
            return 1.0f;

        case ML_FOP_0x1_CONSTANT:
            return getParam(0);

        case ML_FOP_1x0_IDENTITY:
            return getInput(0);

        case ML_FOP_1x0_INVERSE:
            return 1.0f / getInput(0);

        case ML_FOP_1x0_NEGATE:
            return -1.0f * getInput(0);

        case ML_FOP_1x0_SEEDED_RANDOM_UNIT: {
            RandomState lr;
            union {
                float f;
                uint32 u;
            } value;
            value.f = getInput(0);
            RandomState_CreateWithSeed(&lr, value.u);
            return RandomState_UnitFloat(&lr);
        }

        case ML_FOP_1x0_SQUARE: {
            float f = getInput(0);
            return f * f;
        }

        case ML_FOP_1x0_SQRT:
            return sqrtf(getInput(0));
        case ML_FOP_1x0_ARC_COSINE:
            return acosf(getInput(0));
        case ML_FOP_1x0_ARC_SINE:
            return asinf(getInput(0));
        case ML_FOP_1x0_ARC_TANGENT:
            return atanf(getInput(0));
        case ML_FOP_1x0_HYP_COSINE:
            return coshf(getInput(0));
        case ML_FOP_1x0_HYP_SINE:
            return sinhf(getInput(0));
        case ML_FOP_1x0_HYP_TANGENT:
            return tanhf(getInput(0));
        case ML_FOP_1x0_EXP:
            return expf(getInput(0));
        case ML_FOP_1x0_LN:
            return logf(getInput(0));
        case ML_FOP_1x0_CEIL:
            return ceilf(getInput(0));
        case ML_FOP_1x0_FLOOR:
            return floorf(getInput(0));
        case ML_FOP_1x0_ABS:
            return fabsf(getInput(0));
        case ML_FOP_1x0_SIN:
            return sinf(getInput(0));
        case ML_FOP_1x0_COS:
            return cosf(getInput(0));
        case ML_FOP_1x0_TAN:
            return tanf(getInput(0));
        case ML_FOP_1x0_PROB_NOT:
            return (1.0f - getInput(0));

        case ML_FOP_1x1_STRICT_ON:
        case ML_FOP_1x1_STRICT_OFF:
        case ML_FOP_1x1_LINEAR_UP:
        case ML_FOP_1x1_LINEAR_DOWN:
        case ML_FOP_1x1_QUADRATIC_UP:
        case ML_FOP_1x1_QUADRATIC_DOWN:
            return MLFloatCheck1x1(op, getInput(0), getParam(0));

        case ML_FOP_1x1_FMOD:
            return fmodf(getInput(0), getParam(0));

        case ML_FOP_1x1_POW:
            return powf(getInput(0), getParam(0));

        case ML_FOP_1x1_GTE:
            return getInput(0) >= getParam(0) ? 1.0f : 0.0f;
        case ML_FOP_1x1_LTE:
            return getInput(0) <= getParam(0) ? 1.0f : 0.0f;

        case ML_FOP_1x2_CLAMP: {
            float f = getInput(0);
            float min = getParam(0);
            float max = getParam(1);
            f = MAX(f, max);
            f = MIN(f, min);
            return f;
        }

        case ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(p0, p1);
            float min = MIN(p0, p1);
            f = MAX(f, max);
            f = MIN(f, min);

            f = (f - min) / (max - min);
            return f;
        }

        case ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(p0, p1);
            float min = MIN(p0, p1);
            f = MAX(f, 1.0f);
            f = MIN(f, 0.0f);

            f = f * (max - min);
            return f;
        }

        case ML_FOP_1x2_SINE: {
            float p = getParam(0);
            float s = getParam(1);
            float t = getInput(0);
            return sinf(t/p + s);
        }
        case ML_FOP_1x2_COSINE: {
            float p = getParam(0);
            float s = getParam(1);
            float t = getInput(0);
            return cosf(t/p + s);
        }

        case ML_FOP_1x2_INSIDE_RANGE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(p0, p1);
            float min = MIN(p0, p1);
            return (f >= min && f <= max) ? 1.0f : 0.0f;
        }
        case ML_FOP_1x2_OUTSIDE_RANGE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(p0, p1);
            float min = MIN(p0, p1);
            return (f <= min || f >= max) ? 1.0f : 0.0f;
        }
        case ML_FOP_1x2_SEEDED_RANDOM: {
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(p0, p1);
            float min = MIN(p0, p1);
            RandomState lr;
            union {
                float f;
                uint32 u;
            } value;
            value.f = getInput(0);
            RandomState_CreateWithSeed(&lr, value.u);
            return RandomState_Float(&lr, min, max);
        }

        case ML_FOP_1x3_IF_GTE_ELSE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return f >= p0 ? p1 : p2;
        }
        case ML_FOP_1x3_IF_LTE_ELSE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return f <= p0 ? p1 : p2;
        }
        case ML_FOP_1x3_SQUARE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            float s = (p1 * (f + p2));
            return p0 * s * s;
        }
        case ML_FOP_1x3_SQRT: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * sqrtf(p1 * (f + p2));
        }
        case ML_FOP_1x3_ARC_SINE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * asinf(p1 * (f + p2));
        }
        case ML_FOP_1x3_ARC_TANGENT: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * atanf(p1 * (f + p2));
        }
        case ML_FOP_1x3_ARC_COSINE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * acosf(p1 * (f + p2));
        }
        case ML_FOP_1x3_HYP_COSINE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * coshf(p1 * (f + p2));
        }
        case ML_FOP_1x3_HYP_SINE: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * sinhf(p1 * (f + p2));
        }
        case ML_FOP_1x3_HYP_TANGENT: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * tanhf(p1 * (f + p2));
        }
        case ML_FOP_1x3_EXP: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * expf(p1 * (f + p2));
        }
        case ML_FOP_1x3_LN: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * logf(p1 * (f + p2));
        }
        case ML_FOP_1x3_SIN: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * sinf(p1 * (f + p2));
        }
        case ML_FOP_1x3_COS: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * cosf(p1 * (f + p2));
        }
        case ML_FOP_1x3_TAN: {
            float f = getInput(0);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float p2 = getParam(2);
            return p0 * tanf(p1 * (f + p2));
        }

        case ML_FOP_2x0_POW:
            return powf(getInput(0), getInput(1));
        case ML_FOP_2x0_SUM:
            return getInput(0) + getInput(1);
        case ML_FOP_2x0_SQUARE_SUM: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            return (i0 * i0) + (i1 * i1);
        }
        case ML_FOP_2x0_PRODUCT:
            return getInput(0) * getInput(1);

        case ML_FOP_2x2_IF_GTE_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float p0 = getParam(0);
            float p1 = getParam(1);
            return i0 >= i1 ? p0 : p1;
        }
        case ML_FOP_2x2_IF_LTE_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float p0 = getParam(0);
            float p1 = getParam(1);
            return i0 <= i1 ? p0 : p1;
        }

        case ML_FOP_3x0_IF_GTEZ_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            return i0 >= 0.0f ? i1 : i2;
        }
        case ML_FOP_3x0_IF_LTEZ_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            return i0 <= 0.0f ? i1 : i2;
        }

        case ML_FOP_3x2_IF_INSIDE_RANGE_CONST_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(p0, p1);
            float min = MIN(p0, p1);
            return (i0 >= min && i0 <= max) ? i1 : i2;
        }
        case ML_FOP_3x2_IF_OUTSIDE_RANGE_CONST_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(p0, p1);
            float min = MIN(p0, p1);
            return (i0 <= min || i0 >= max) ? i1 : i2;
        }
        case ML_FOP_3x2_IF_INSIDE_RANGE_ELSE_CONST: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(i1, i2);
            float min = MIN(i1, i2);
            return (i0 >= min && i0 <= max) ? p0 : p1;
        }
        case ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float p0 = getParam(0);
            float p1 = getParam(1);
            float max = MAX(i1, i2);
            float min = MIN(i1, i2);
            return (i0 <= min || i0 >= max) ? p0 : p1;
        }

        case ML_FOP_4x0_IF_GTE_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float i3 = getInput(3);
            return i0 >= i1 ? i2 : i3;
        }
        case ML_FOP_4x0_IF_LTE_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float i3 = getInput(3);
            return i0 <= i1 ? i2 : i3;
        }

        case ML_FOP_5x0_IF_INSIDE_RANGE_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float i3 = getInput(3);
            float i4 = getInput(4);
            float max = MAX(i1, i2);
            float min = MIN(i1, i2);
            return (i0 >= min && i0 <= max) ? i3 : i4;
        }
        case ML_FOP_5x0_IF_OUTSIDE_RANGE_ELSE: {
            float i0 = getInput(0);
            float i1 = getInput(1);
            float i2 = getInput(2);
            float i3 = getInput(3);
            float i4 = getInput(4);
            float max = MAX(i1, i2);
            float min = MIN(i1, i2);
            return (i0 <= min || i0 >= max) ? i3 : i4;
        }

        case ML_FOP_Nx0_SUM: {
            float f = 0.0f;

            for (uint i = 0; i < inputs.size(); i++) {
                f += getInput(i);
            }
            return f;
        }
        case ML_FOP_Nx0_PRODUCT: {
            float f = 1.0f;

            for (uint i = 0; i < inputs.size(); i++) {
                f *= getInput(i);
            }
            return f;
        }
        case ML_FOP_Nx0_MIN: {
            float f = getInput(0);

            for (uint i = 1; i < inputs.size(); i++) {
                float nf = getInput(i);
                if (nf < f) {
                    f = nf;
                }
            }
            return f;
        }
        case ML_FOP_Nx0_MAX: {
            float f = getInput(0);

            for (uint i = 1; i < inputs.size(); i++) {
                float nf = getInput(i);
                if (nf > f) {
                    f = nf;
                }
            }
            return f;
        }

        case ML_FOP_Nx0_ARITHMETIC_MEAN: {
            float f = 0.0f;

            if (inputs.size() > 0) {
                for (uint i = 0; i < inputs.size(); i++) {
                    f += getInput(i);
                }
                f /= inputs.size();
            }

            return f;
        }
        case ML_FOP_Nx0_GEOMETRIC_MEAN: {
            float f = 1.0f;

            if (inputs.size() > 0) {
                for (uint i = 0; i < inputs.size(); i++) {
                    f *= getInput(i);
                }
                f = powf(f, 1.0f / inputs.size());
            }

            return f;
        }

        case ML_FOP_Nx0_DIV_SUM: {
            float f = 0.0f;

            for (uint i = 0; i < inputs.size(); i++) {
                f += getInput(i);
            }

            return 1.0f / f;
        }
        case ML_FOP_Nx1_DIV_SUM: {
            float f = 0.0f;
            float c = getParam(0);

            for (uint i = 0; i < inputs.size(); i++) {
                f += getInput(i);
            }

            return c / f;
        }

        case ML_FOP_Nx0_SELECT_UNIT_INTERVAL_STEP: {
            float i0 = getInput(0);
            float s = MAX(0.0f, MIN(1.0f, i0));
            uint n = inputs.size() - 1;
            uint index = 1 + n * s;

            index = MAX(inputs.size(), index);

            return getInput(index);
        }
        case ML_FOP_1xN_SELECT_UNIT_INTERVAL_STEP: {
            float i0 = getInput(0);
            float s = MAX(0.0f, MIN(1.0f, i0));
            uint n = params.size();
            uint index = n * s;

            index = MAX(inputs.size(), index);

            return getParam(index);
        }
        case ML_FOP_NxN_SELECT_UNIT_INTERVAL_WEIGHTED_STEP: {
            float i0 = getInput(0);
            float s = MAX(0.0f, MIN(1.0f, i0));
            float totalW = 0.0f;

            for (uint i = 1; i < inputs.size(); i++) {
                totalW += getParam(i);
            }

            float curW = 0.0f;
            uint index = inputs.size() - 1;
            for (uint i = 1; i < inputs.size(); i++) {
                curW += getParam(i) / totalW;
                if (s <= curW) {
                    index = i;
                    // break loop
                    i = inputs.size();
                }
            }

            return getInput(index);
        }

        case ML_FOP_Nx0_SELECT_UNIT_INTERVAL_LERP: {
            float i0 = getInput(0);
            float s = MAX(0.0f, MIN(1.0f, i0));
            uint n = inputs.size() - 1;
            uint indexLower = 1 + n * s;
            uint indexUpper = indexLower + 1;

            indexLower = MAX(inputs.size(), indexLower);
            indexUpper = MAX(inputs.size(), indexUpper);

            float iL = getInput(indexLower);
            float iU = getInput(indexUpper);
            float slotSize = 1.0f / n;
            float t = (s - (indexLower * slotSize)) / slotSize;

            return iL + ((iU - iL) * t);
        }
        case ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP: {
            float i0 = getInput(0);
            float s = MAX(0.0f, MIN(1.0f, i0));
            uint n = params.size();
            uint indexLower = n * s;
            uint indexUpper = indexLower + 1;

            indexLower = MAX(params.size(), indexLower);
            indexUpper = MAX(params.size(), indexUpper);

            float iL = getParam(indexLower);
            float iU = getParam(indexUpper);
            float slotSize = 1.0f / n;
            float t = (s - (indexLower * slotSize)) / slotSize;

            return iL + ((iU - iL) * t);
        }

        case ML_FOP_1x1_LINEAR_COMBINATION:
        case ML_FOP_2x2_LINEAR_COMBINATION:
        case ML_FOP_3x3_LINEAR_COMBINATION:
        case ML_FOP_4x4_LINEAR_COMBINATION:
        case ML_FOP_NxN_LINEAR_COMBINATION: {
            float f = 0.0;
            uint size = inputs.size();

            // These ASSERTs should be true as long as the node was minimized.
            if (op == ML_FOP_1x1_LINEAR_COMBINATION) {
                ASSERT(size == 1);
            } else if (op == ML_FOP_2x2_LINEAR_COMBINATION) {
                ASSERT(size == 2);
            } else if (op == ML_FOP_3x3_LINEAR_COMBINATION) {
                ASSERT(size == 3);
            } else if (op == ML_FOP_4x4_LINEAR_COMBINATION) {
                ASSERT(size == 4);
            }

            for (uint i = 0; i < size; i++) {
                f += getParam(i) * getInput(i);
            }
            return f;
        }

        case ML_FOP_NxN_SCALED_MIN: {
            float f = getInput(0) * getParam(0);

            for (uint i = 1; i < inputs.size(); i++) {
                float nf = getInput(i) * getParam(i);
                if (nf < f) {
                    f = nf;
                }
            }
            return f;
        }
        case ML_FOP_NxN_SCALED_MAX: {
            float f = getInput(0) * getParam(0);

            for (uint i = 1; i < inputs.size(); i++) {
                float nf = getInput(i) * getParam(i);
                if (nf > f) {
                    f = nf;
                }
            }
            return f;
        }
        case ML_FOP_NxN_SELECT_GTE: {
            float f = getInput(0);

            for (uint i = 1; i < inputs.size(); i++) {
                if (f >= getParam(i)) {
                    return getInput(i);
                }
            }
            return 0.0;
        }
        case ML_FOP_NxN_SELECT_LTE: {
            float f = getInput(0);

            for (uint i = 1; i < inputs.size(); i++) {
                if (f <= getParam(i)) {
                    return getInput(i);
                }
            }
            return 0.0;
        }
        case ML_FOP_NxN_POW_SUM: {
            float f = 0.0;

            for (uint i = 0; i < inputs.size(); i++) {
                f += powf(getInput(i), getParam(0));
            }
            return f;
        }

        case ML_FOP_INVALID:
            PANIC("Unhandled MLFloatOp: ML_FOP_INVALID\n");

        default:
            PANIC("Unknown MLFloatOp: %s(%d)\n", ML_FloatOpToString(op), op);
    }
}

void MLFloatNode::mutate(float rate,
                         uint maxInputs, uint maxParams)
{
    MutationFloatParams mp;
    uint oldSize;
    uint newSize;

    /*
     * Mutate the mutationRate itself with a base probability.
     */
    Mutate_DefaultFloatParams(&mp, MUTATION_TYPE_PROBABILITY);
    mp.mutationRate = (mp.mutationRate + rate) / 2.0f;
    mp.minValue = 0.0f;
    mp.maxValue = 1.0f;
    mutationRate = Mutate_FloatRaw(mutationRate, FALSE, &mp);

    if (!Random_Flip(mutationRate)) {
        return;
    }

    /* Op */
    if (Random_Flip(rate)) {
        op = (MLFloatOp)Random_Int(ML_FOP_MIN, ML_FOP_MAX - 1);
    }

    /* Inputs */
    oldSize = inputs.size();
    newSize = oldSize;
    if (oldSize > 1 && Random_Flip(rate / 2.0f)) {
        for (uint i = 0; i < oldSize; i++) {
            uint i0 = Random_Int(0, oldSize - 1);
            uint i1 = Random_Int(0, oldSize - 1);
            if (i0 != i1) {
                uint t = inputs[i0];
                inputs[i0] = inputs[i1];
                inputs[i1] = t;
            }
        }
    }

    if (oldSize < maxInputs && Random_Flip(rate)) {
        newSize = oldSize + 1;
    }
    if (oldSize > 0 && Random_Flip(rate)) {
        newSize = oldSize - 1;
    }
    if (Random_Flip(rate / 2.0f)) {
        newSize = Random_Int(0, maxInputs);
    }

    inputs.resize(newSize);
    for (uint i = oldSize; i < newSize; i++) {
        inputs[i] = index > 0 ? Random_Int(0, index - 1) : 0;
    }
    for (uint i = 0; i < inputs.size(); i++) {
        if (Random_Flip(rate)) {
            inputs[i] = index > 0 ? Random_Int(0, index - 1) : 0;
        }
    }
    if (newSize > 1 && Random_Flip(rate / 2.0f)) {
        for (uint i = 0; i < newSize; i++) {
            uint i0 = Random_Int(0, newSize - 1);
            uint i1 = Random_Int(0, newSize - 1);
            if (i0 != i1) {
                uint t = inputs[i0];
                inputs[i0] = inputs[i1];
                inputs[i1] = t;
            }
        }
    }


    /* Params */
    oldSize = params.size();
    newSize = oldSize;
    if (oldSize > 1 && Random_Flip(rate / 2.0f)) {
        for (uint i = 0; i < oldSize; i++) {
            uint i0 = Random_Int(0, oldSize - 1);
            uint i1 = Random_Int(0, oldSize - 1);
            if (i0 != i1) {
                float t = params[i0];
                params[i0] = params[i1];
                params[i1] = t;
            }
        }
    }

    if (oldSize < maxParams && Random_Flip(rate)) {
        newSize = oldSize + 1;
    }
    if (oldSize > 0 && Random_Flip(rate)) {
        newSize = oldSize - 1;
    }
    if (Random_Flip(rate / 2.0f)) {
        newSize = Random_Int(0, maxParams);
    }

    params.resize(newSize);
    for (uint i = oldSize; i < newSize; i++) {
        params[i] = 0.0f;
    }
    for (uint i = 0; i < params.size(); i++) {
        int r = Random_Int(0, MUTATION_TYPE_MAX - 1);
        Mutate_DefaultFloatParams(&mp, (MutationType)r);
        mp.mutationRate = (mp.mutationRate + rate) / 2.0f;
        params[i] = Mutate_FloatRaw(params[i], FALSE, &mp);
    }
    if (newSize > 1 && Random_Flip(rate / 2.0f)) {
        for (uint i = 0; i < newSize; i++) {
            uint i0 = Random_Int(0, newSize - 1);
            uint i1 = Random_Int(0, newSize - 1);
            if (i0 != i1) {
                float t = params[i0];
                params[i0] = params[i1];
                params[i1] = t;
            }
        }
    }
}



void MLFloatNode::load(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    MBString str;

    p = prefix;
    p += "op";
    op = ML_StringToFloatOp(MBRegistry_GetCStr(mreg, p.CStr()));
    if (op == ML_FOP_INVALID) {
        op = ML_FOP_0x0_ZERO;
    }
    VERIFY(op < ML_FOP_MAX);
    p = prefix;
    p += "inputs";
    str = MBRegistry_GetCStr(mreg, p.CStr());
    TextDump_Convert(str, inputs);

    p = prefix;
    p += "params";
    str = MBRegistry_GetCStr(mreg, p.CStr());
    TextDump_Convert(str, params);

    p = prefix;
    p += "mutationRate";
    mutationRate = MBRegistry_GetFloat(mreg, p.CStr());
}


void MLFloatNode::save(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    MBString str;
    char *cs;
    int ret;

    p = prefix;
    p += "op";
    MBRegistry_PutCopy(mreg, p.CStr(), ML_FloatOpToString(op));

    p = prefix;
    p += "inputs";
    TextDump_Convert(inputs, str);
    MBRegistry_PutCopy(mreg, p.CStr(), str.CStr());

    p = prefix;
    p += "params";
    TextDump_Convert(params, str);
    MBRegistry_PutCopy(mreg, p.CStr(), str.CStr());

    p = prefix;
    p += "mutationRate";
    ret = asprintf(&cs, "%f", mutationRate);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), cs);
    free(cs);
}

void MLFloatNode::minimize()
{
    uint numInputs = 0;
    uint numParams = 0;

    if (mb_debug && ML_FOP_MAX != 97) {
        PANIC("ML_FOP_MAX=%d\n", ML_FOP_MAX);
    }

    switch (op) {
        case ML_FOP_0x0_ZERO:
        case ML_FOP_0x0_ONE:
            numInputs = 0;
            numParams = 0;
            break;

        case ML_FOP_0x1_CONSTANT:
            numInputs = 0;
            numParams = 1;
            break;

        case ML_FOP_1x0_IDENTITY:
        case ML_FOP_1x0_INVERSE:
        case ML_FOP_1x0_NEGATE:
        case ML_FOP_1x0_SEEDED_RANDOM_UNIT:
        case ML_FOP_1x0_SQUARE:
        case ML_FOP_1x0_SQRT:
        case ML_FOP_1x0_ARC_COSINE:
        case ML_FOP_1x0_ARC_SINE:
        case ML_FOP_1x0_ARC_TANGENT:
        case ML_FOP_1x0_HYP_COSINE:
        case ML_FOP_1x0_HYP_SINE:
        case ML_FOP_1x0_HYP_TANGENT:
        case ML_FOP_1x0_EXP:
        case ML_FOP_1x0_LN:
        case ML_FOP_1x0_CEIL:
        case ML_FOP_1x0_FLOOR:
        case ML_FOP_1x0_ABS:
        case ML_FOP_1x0_SIN:
        case ML_FOP_1x0_COS:
        case ML_FOP_1x0_TAN:
        case ML_FOP_1x0_PROB_NOT:
            numInputs = 1;
            numParams = 0;
            break;

        case ML_FOP_1x1_STRICT_ON:
        case ML_FOP_1x1_STRICT_OFF:
        case ML_FOP_1x1_LINEAR_UP:
        case ML_FOP_1x1_LINEAR_DOWN:
        case ML_FOP_1x1_QUADRATIC_UP:
        case ML_FOP_1x1_QUADRATIC_DOWN:
        case ML_FOP_1x1_FMOD:
        case ML_FOP_1x1_POW:
        case ML_FOP_1x1_GTE:
        case ML_FOP_1x1_LTE:
        case ML_FOP_1x1_LINEAR_COMBINATION:
            numInputs = 1;
            numParams = 1;
            break;

        case ML_FOP_1x2_CLAMP:
        case ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT:
        case ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT:
        case ML_FOP_1x2_SINE:
        case ML_FOP_1x2_COSINE:
        case ML_FOP_1x2_INSIDE_RANGE:
        case ML_FOP_1x2_OUTSIDE_RANGE:
        case ML_FOP_1x2_SEEDED_RANDOM:
            numInputs = 1;
            numParams = 2;
            break;

        case ML_FOP_1x3_IF_GTE_ELSE:
        case ML_FOP_1x3_IF_LTE_ELSE:
        case ML_FOP_1x3_SQUARE:
        case ML_FOP_1x3_SQRT:
        case ML_FOP_1x3_ARC_SINE:
        case ML_FOP_1x3_ARC_TANGENT:
        case ML_FOP_1x3_ARC_COSINE:
        case ML_FOP_1x3_HYP_COSINE:
        case ML_FOP_1x3_HYP_SINE:
        case ML_FOP_1x3_HYP_TANGENT:
        case ML_FOP_1x3_EXP:
        case ML_FOP_1x3_LN:
        case ML_FOP_1x3_SIN:
        case ML_FOP_1x3_COS:
        case ML_FOP_1x3_TAN:
            numInputs = 1;
            numParams = 3;
            break;

        case ML_FOP_1xN_SELECT_UNIT_INTERVAL_STEP:
        case ML_FOP_1xN_SELECT_UNIT_INTERVAL_LERP:
            numInputs = 1;
            numParams = MAX(1, inputs.size());
            break;

        case ML_FOP_2x0_POW:
        case ML_FOP_2x0_SUM:
        case ML_FOP_2x0_SQUARE_SUM:
        case ML_FOP_2x0_PRODUCT:
            numInputs = 2;
            numParams = 0;
            break;

        case ML_FOP_2x2_LINEAR_COMBINATION:
        case ML_FOP_2x2_IF_GTE_ELSE:
        case ML_FOP_2x2_IF_LTE_ELSE:
            numInputs = 2;
            numParams = 2;
            break;

        case ML_FOP_3x0_IF_GTEZ_ELSE:
        case ML_FOP_3x0_IF_LTEZ_ELSE:
            numInputs = 3;
            numParams = 0;
            break;

        case ML_FOP_3x2_IF_INSIDE_RANGE_CONST_ELSE:
        case ML_FOP_3x2_IF_OUTSIDE_RANGE_CONST_ELSE:
        case ML_FOP_3x2_IF_INSIDE_RANGE_ELSE_CONST:
        case ML_FOP_3x2_IF_OUTSIDE_RANGE_ELSE_CONST:
            numInputs = 3;
            numParams = 2;
            break;

        case ML_FOP_3x3_LINEAR_COMBINATION:
            numInputs = 3;
            numParams = 3;
            break;

        case ML_FOP_4x0_IF_GTE_ELSE:
        case ML_FOP_4x0_IF_LTE_ELSE:
            numInputs = 4;
            numParams = 0;
            break;

        case ML_FOP_5x0_IF_INSIDE_RANGE_ELSE:
        case ML_FOP_5x0_IF_OUTSIDE_RANGE_ELSE:
            numInputs = 5;
            numParams = 0;
            break;

        case ML_FOP_4x4_LINEAR_COMBINATION:
            numInputs = 4;
            numParams = 4;
            break;

        case ML_FOP_Nx0_SUM:
        case ML_FOP_Nx0_PRODUCT:
        case ML_FOP_Nx0_ARITHMETIC_MEAN:
        case ML_FOP_Nx0_GEOMETRIC_MEAN:
            numInputs = inputs.size();
            numParams = 0;
            break;

        case ML_FOP_Nx0_MIN:
        case ML_FOP_Nx0_MAX:
        case ML_FOP_Nx0_DIV_SUM:
            numInputs = MAX(1, inputs.size());
            numParams = 0;
            break;

        case ML_FOP_Nx0_SELECT_UNIT_INTERVAL_STEP:
        case ML_FOP_Nx0_SELECT_UNIT_INTERVAL_LERP:
            numInputs = MAX(2, inputs.size());
            numParams = 0;
            break;

        case ML_FOP_Nx1_DIV_SUM:
            numInputs = MAX(1, inputs.size());
            numParams = 1;
            break;

        case ML_FOP_NxN_LINEAR_COMBINATION:
        case ML_FOP_NxN_SCALED_MIN:
        case ML_FOP_NxN_SCALED_MAX:
        case ML_FOP_NxN_SELECT_GTE:
        case ML_FOP_NxN_SELECT_LTE:
        case ML_FOP_NxN_POW_SUM:
            numInputs = MIN(inputs.size(), params.size());
            numInputs = MAX(1, numInputs);
            numParams = numInputs;
            break;

        default:
            PANIC("Unknown MLFloatOp: %s(%d)\n", ML_FloatOpToString(op), op);
    }

    if (numInputs > inputs.size() ||
        numParams > params.size()) {
        /*
         * If we don't have enough parameters,
         * treat this as a ZERO op.
         */
        makeZero();
    } else {
        if (numInputs < inputs.size()) {
            inputs.resize(numInputs);
        }
        if (numParams < params.size()) {
            params.resize(numParams);
        }
    }
}

void MLFloatNode::makeZero()
{
    op = ML_FOP_0x0_ZERO;
    params.resize(0);
    inputs.resize(0);
    mutationRate = 0.25f;
}

const char *ML_FloatOpToString(MLFloatOp op)
{
    return TextMap_ToString(op, tmMLFloatOps, ARRAYSIZE(tmMLFloatOps));
}

MLFloatOp ML_StringToFloatOp(const char *opstr)
{
    if (opstr == NULL) {
        return ML_FOP_INVALID;
    }

    return (MLFloatOp)TextMap_FromString(opstr, tmMLFloatOps, ARRAYSIZE(tmMLFloatOps));
}

void ML_UnitTest()
{
    MLFloatOp op;

    VERIFY(ARRAYSIZE(tmMLFloatOps) == ML_FOP_MAX);

    for (op = ML_FOP_INVALID; op < ML_FOP_MAX; op++) {
        VERIFY(ML_StringToFloatOp(ML_FloatOpToString(op)) == op);
    }
}