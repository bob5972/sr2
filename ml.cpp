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

    { TMENTRY(ML_FOP_1x1_STRICT_ON), },
    { TMENTRY(ML_FOP_1x1_STRICT_OFF), },
    { TMENTRY(ML_FOP_1x1_LINEAR_UP), },
    { TMENTRY(ML_FOP_1x1_LINEAR_DOWN), },
    { TMENTRY(ML_FOP_1x1_QUADRATIC_UP), },
    { TMENTRY(ML_FOP_1x1_QUADRATIC_DOWN), },
    { TMENTRY(ML_FOP_1x1_FMOD), },
    { TMENTRY(ML_FOP_1x1_POW), },

    { TMENTRY(ML_FOP_1x2_CLAMP), },
    { TMENTRY(ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT), },
    { TMENTRY(ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT), },
    { TMENTRY(ML_FOP_1x2_SINE), },
    { TMENTRY(ML_FOP_1x2_COSINE), },

    { TMENTRY(ML_FOP_2x0_POW), },

    { TMENTRY(ML_FOP_Nx0_SUM), },
    { TMENTRY(ML_FOP_Nx0_PRODUCT), },
    { TMENTRY(ML_FOP_Nx0_MIN), },
    { TMENTRY(ML_FOP_Nx0_MAX), },
    { TMENTRY(ML_FOP_Nx0_ARITHMETIC_MEAN), },
    { TMENTRY(ML_FOP_Nx0_GEOMETRIC_MEAN), },

    { TMENTRY(ML_FOP_NxN_LINEAR_COMBINATION), },
    { TMENTRY(ML_FOP_NxN_SCALED_MIN), },
    { TMENTRY(ML_FOP_NxN_SCALED_MAX), },
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
    ASSERT(ML_FOP_MAX == 44);

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

        case ML_FOP_2x0_POW:
            return powf(getInput(0), getInput(1));

        case ML_FOP_Nx0_SUM: {
            float f = 0.0;

            for (uint i = 0; i < inputs.size(); i++) {
                f += getInput(i);
            }
            return f;
        }
        case ML_FOP_Nx0_PRODUCT: {
            float f = 1.0;

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
            float f = 0.0;

            if (inputs.size() > 0) {
                for (uint i = 0; i < inputs.size(); i++) {
                    f += getInput(i);
                }
                f /= inputs.size();
            }

            return f;
        }
        case ML_FOP_Nx0_GEOMETRIC_MEAN: {
            float f = 1.0;

            if (inputs.size() > 0) {
                for (uint i = 0; i < inputs.size(); i++) {
                    f *= getInput(i);
                }
                f = powf(f, 1.0f / inputs.size());
            }

            return f;
        }

        case ML_FOP_NxN_LINEAR_COMBINATION: {
        float f = 0.0;

            for (uint i = 0; i < inputs.size(); i++) {
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

        case ML_FOP_INVALID:
            PANIC("Unhandled MLFloatOp: ML_FOP_INVALID\n");

        default:
            PANIC("Unknown MLFloatOp: %d\n", op);
    }
}

void MLFloatNode::mutate(float rate,
                         uint maxInputs, uint maxParams)
{
    if (!Random_Flip((1.0f + rate) / 2.0f)) {
        return;
    }

    if (inputs.size() < maxInputs) {
        uint oldSize = inputs.size();
        if (Random_Flip(rate)) {
            inputs.resize(oldSize + 1);
        }

        for (uint i = oldSize; i < inputs.size(); i++) {
            inputs[i] = 0;
        }
    }

    if (params.size() < maxParams) {
        uint oldSize = params.size();
        if (Random_Flip(rate)) {
            params.resize(oldSize + 1);
        }

        for (uint i = oldSize; i < params.size(); i++) {
            params[i] = 0.0f;
        }
    }

    if (Random_Flip(rate)) {
        op = (MLFloatOp)Random_Int(ML_FOP_MIN, ML_FOP_MAX - 1);
    }

    for (uint i = 0; i < inputs.size(); i++) {
        if (Random_Flip(rate)) {
            inputs[i] = index > 0 ? Random_Int(0, index - 1) : 0;
        }
    }

    for (uint i = 0; i < params.size(); i++) {
        MutationFloatParams mp;
        int r = Random_Int(0, MUTATION_TYPE_MAX - 1);
        Mutate_DefaultFloatParams(&mp, (MutationType)r);
        mp.mutationRate = (mp.mutationRate + rate) / 2.0f;
        params[i] = Mutate_FloatRaw(params[i], FALSE, &mp);
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
}


void MLFloatNode::save(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    MBString str;

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
}

void MLFloatNode::minimize()
{
    uint numInputs = 0;
    uint numParams = 0;

    ASSERT(ML_FOP_MAX == 44);

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
            numInputs = 1;
            numParams = 1;
            break;

        case ML_FOP_1x2_CLAMP:
        case ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT:
        case ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT:
        case ML_FOP_1x2_SINE:
        case ML_FOP_1x2_COSINE:
            numInputs = 1;
            numParams = 2;
            break;

        case ML_FOP_2x0_POW:
            numInputs = 2;
            numParams = 0;
            break;

        case ML_FOP_Nx0_SUM:
        case ML_FOP_Nx0_PRODUCT:
        case ML_FOP_Nx0_MIN:
        case ML_FOP_Nx0_MAX:
        case ML_FOP_Nx0_ARITHMETIC_MEAN:
        case ML_FOP_Nx0_GEOMETRIC_MEAN:
            numInputs = MAX(1, inputs.size());
            numParams = 0;
            break;

        case ML_FOP_NxN_LINEAR_COMBINATION:
        case ML_FOP_NxN_SCALED_MIN:
        case ML_FOP_NxN_SCALED_MAX:
            numInputs = MIN(inputs.size(), params.size());
            numInputs = MAX(1, numInputs);
            numParams = numInputs;
            break;

        default:
            NOT_IMPLEMENTED();
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
