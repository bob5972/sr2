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

    ML_FOP_0x0_ZERO = 1,
    ML_FOP_MIN = 1,
    ML_FOP_0x0_ONE,

    ML_FOP_0x1_CONSTANT,

    ML_FOP_1x0_IDENTITY,
    ML_FOP_1x0_INVERSE,
    ML_FOP_1x0_NEGATE,
    ML_FOP_1x0_SEEDED_RANDOM_UNIT,
    ML_FOP_1x0_SQUARE,

    ML_FOP_1x1_STRICT_ON,
    ML_FOP_1x1_STRICT_OFF,
    ML_FOP_1x1_LINEAR_UP,
    ML_FOP_1x1_LINEAR_DOWN,
    ML_FOP_1x1_QUADRATIC_UP,
    ML_FOP_1x1_QUADRATIC_DOWN,
    ML_FOP_1x1_FMOD,

    ML_FOP_1x2_CLAMP,
    ML_FOP_1x2_CLAMPED_SCALE_TO_UNIT,
    ML_FOP_1x2_CLAMPED_SCALE_FROM_UNIT,
    ML_FOP_1x2_SINE,
    ML_FOP_1x2_COSINE,

    ML_FOP_Nx0_SUM,
    ML_FOP_Nx0_PRODUCT,
    ML_FOP_Nx0_MIN,
    ML_FOP_Nx0_MAX,
    ML_FOP_Nx0_ARITHMETIC_MEAN,
    ML_FOP_Nx0_GEOMETRIC_MEAN,

    ML_FOP_NxN_LINEAR_COMBINATION,
    ML_FOP_NxN_SCALED_MIN,
    ML_FOP_NxN_SCALED_MAX,

    ML_FOP_MAX,
} MLFloatOp;

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
        void makeZero();
        bool isZero() { return op == ML_FOP_0x0_ZERO; }

    private:
        const MBVector<float> *myValues;

        float computeWork(const MBVector<float> &values);

        float getInput(uint i) {
            ASSERT(myValues != NULL);
            ASSERT(i < inputs.size());
            ASSERT(index < myValues->size());
            ASSERT(inputs[i] < index);
            return myValues->get(inputs[i]);
        }

        float getParam(uint i) {
            ASSERT(i < params.size());
            return params[i];
        }
};

const char *ML_FloatOpToString(MLFloatOp op);
MLFloatOp ML_StringToFloatOp(const char *opstr);

#endif // _ML_H_202208151722