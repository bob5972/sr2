/*
 * floatNet.hpp -- part of SpaceRobots2
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

#ifndef _FLOATNET_H_202208121158
#define _FLOATNET_H_202208121158

#include "MBTypes.h"
#include "MBVector.hpp"
#include "MBRegistry.h"
#include "ml.hpp"

typedef enum FloatNetOp {
    FLOATNET_OP_INVALID = 0,
    FLOATNET_OP_MIN = 1,
    FLOATNET_OP_ZERO = 1,
    FLOATNET_OP_ONE,

    FLOATNET_OP_CONSTANT,
    FLOATNET_OP_IDENTITY,

    FLOATNET_OP_CLAMP,

    FLOATNET_OP_SUM,
    FLOATNET_OP_PRODUCT,

    FLOATNET_OP_LINEAR_COMBINATION,

    FLOATNET_OP_TAKE_MIN,
    FLOATNET_OP_TAKE_MAX,

    FLOATNET_OP_TAKE_SCALED_MIN,
    FLOATNET_OP_TAKE_SCALED_MAX,

    FLOATNET_OP_MAX,
} FloatNetOp;

class FloatNet
{
    public:
        FloatNet(uint numInputs, uint numOutputs, uint numNodes);

        void compute(const float *inputs, uint numInputs,
                     float *outputs, uint numOutputs);

        void load(MBRegistry *mreg, const char *prefix);
        void mutate();
        void save(MBRegistry *mreg, const char *prefix);

    private:
        uint myNumInputs;
        uint myNumOutputs;

        MBVector<MLFloatNode> myNodes;
        MBVector<float> myValues;
};

#endif // _FLOATNET_H_202208121158