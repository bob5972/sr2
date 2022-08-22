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

static TextMapEntry tmMLFloatOps[] = {
    { TMENTRY(ML_FOP_INVALID), },
    { TMENTRY(ML_FOP_0x0_ZERO), },
    { TMENTRY(ML_FOP_0x0_ONE), },
    { TMENTRY(ML_FOP_0x1_CONSTANT), },
    { TMENTRY(ML_FOP_1x0_IDENTITY), },
    { TMENTRY(ML_FOP_1x1_STRICT_ON), },
    { TMENTRY(ML_FOP_1x1_STRICT_OFF), },
    { TMENTRY(ML_FOP_1x1_LINEAR_UP), },
    { TMENTRY(ML_FOP_1x1_LINEAR_DOWN), },
    { TMENTRY(ML_FOP_1x1_QUADRATIC_UP), },
    { TMENTRY(ML_FOP_1x1_QUADRATIC_DOWN), },
    { TMENTRY(ML_FOP_1x2_CLAMP), },
    { TMENTRY(ML_FOP_1x2_SINE), },
    { TMENTRY(ML_FOP_Nx0_SUM), },
    { TMENTRY(ML_FOP_Nx0_PRODUCT), },
    { TMENTRY(ML_FOP_Nx0_MIN), },
    { TMENTRY(ML_FOP_Nx0_MAX), },
    { TMENTRY(ML_FOP_NxN_LINEAR_COMBINATION), },
    { TMENTRY(ML_FOP_NxN_SCALED_MIN), },
    { TMENTRY(ML_FOP_NxN_SCALED_MAX), },
};

float ML_TransformFloat1x1(MLFloatOp op, float input,
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
    ASSERT(ML_FOP_MAX == 20);

    switch (op) {
        case ML_FOP_0x0_ZERO:
            return 0.0f;
        case ML_FOP_0x0_ONE:
            return 1.0f;

        case ML_FOP_0x1_CONSTANT:
            return getParam(0);

        case ML_FOP_1x0_IDENTITY:
            return getInput(0);

        case ML_FOP_1x1_STRICT_ON:
        case ML_FOP_1x1_STRICT_OFF:
        case ML_FOP_1x1_LINEAR_UP:
        case ML_FOP_1x1_LINEAR_DOWN:
        case ML_FOP_1x1_QUADRATIC_UP:
        case ML_FOP_1x1_QUADRATIC_DOWN:
            return ML_TransformFloat1x1(op, getInput(0), getParam(0));

        case ML_FOP_1x2_CLAMP: {
            float f = getInput(0);
            float min = getParam(0);
            float max = getParam(1);
            f = MAX(f, max);
            f = MIN(f, min);
            return f;
        }

        case ML_FOP_1x2_SINE: {
            float p = getParam(0);
            float s = getParam(1);
            float t = getInput(0);
            return sinf(t/p + s);
        }

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
    if (!Random_Flip(rate)) {
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

    // XXX: Better mutations?
    for (uint i = 0; i < params.size(); i++) {
        if (Random_Flip(rate)) {
            params[i] = Random_Float(-1.0f, 1.0f);
        }
    }
}



void MLFloatNode::load(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    MBString str;

    p = prefix;
    p += ".op";
    op = ML_StringToFloatOp(MBRegistry_GetCStr(mreg, p.CStr()));
    if (op == ML_FOP_INVALID) {
        op = ML_FOP_0x0_ZERO;
    }
    VERIFY(op < ML_FOP_MAX);

    p = prefix;
    p += ".numInputs";
    uint numInputs = MBRegistry_GetUint(mreg, p.CStr());

    p = prefix;
    p += ".inputs";
    str = MBRegistry_GetCStr(mreg, p.CStr());
    TextDump_Convert(str, inputs);
    VERIFY(inputs.size() == numInputs);

    p = prefix;
    p += ".numParams";
    uint numParams = MBRegistry_GetUint(mreg, p.CStr());

    p = prefix;
    p += ".params";
    str = MBRegistry_GetCStr(mreg, p.CStr());
    TextDump_Convert(str, params);
    VERIFY(inputs.size() == numParams);
}


void MLFloatNode::save(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    MBString str;
    char *v;
    int ret;

    p = prefix;
    p += "op";
    MBRegistry_PutCopy(mreg, p.CStr(), ML_FloatOpToString(op));

    p = prefix;
    p += "numInputs";
    ret = asprintf(&v, "%d", inputs.size());
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

    p = prefix;
    p += "inputs";
    TextDump_Convert(inputs, str);
    MBRegistry_PutCopy(mreg, p.CStr(), str.CStr());

    p = prefix;
    p += "numParams";
    ret = asprintf(&v, "%d", params.size());
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

    p = prefix;
    p += "params";
    TextDump_Convert(params, str);
    MBRegistry_PutCopy(mreg, p.CStr(), str.CStr());
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


