/*
 * floatNet.cpp -- part of SpaceRobots2
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

#include "floatNet.hpp"
#include "MBAssert.h"
#include "MBDebug.h"

FloatNet::FloatNet(uint numInputs, uint numOutputs, uint numNodes)
{
    ASSERT(numInputs > 0);
    ASSERT(numOutputs > 0);
    ASSERT(numNodes > 0);
    ASSERT(numNodes >= numOutputs);

    myNumInputs = numInputs;
    myNumOutputs = numOutputs;

    myNodes.resize(numNodes);
    myValues.resize(numInputs + numNodes);
}

void FloatNet::compute(const float *inputs, uint numInputs,
                       float *outputs, uint numOutputs)
{
    ASSERT(numInputs == myNumInputs);
    ASSERT(numInputs > 0);
    ASSERT(numOutputs == myNumOutputs);
    ASSERT(numOutputs > 0);

    for (uint i = 0; i < numInputs; i++) {
        myValues[i] = inputs[i];
    }

    for (uint i = 0; i < myNodes.size(); i++) {
        uint vi = i + numInputs;
        myValues[vi] = myNodes[i].compute(myValues);
    }

    ASSERT(myNodes.size() >= numOutputs);

    for (uint i = 0; i < numInputs; i++) {
        uint vi = i + myValues.size() - myNumOutputs;
        outputs[i] = myValues[vi];
    }
}

float FloatNet::Node::compute(MBVector<float> &values)
{
    if (mb_debug) {
        for (uint i = 0; i < myInputs.size(); i++) {
            ASSERT(myInputs[i] < myIndex);
        }
    }

    switch (myOp) {
        case FLOATNET_OP_ZERO:
            return 0.0;
        case FLOATNET_OP_ONE:
            return 1.0;

        case FLOATNET_OP_CONSTANT:
            return getParam(0);
        case FLOATNET_OP_IDENTITY:
            return getInput(0, values);

        case FLOATNET_OP_SUM: {
            float f = 0.0;

            for (uint i = 0; i < myInputs.size(); i++) {
                f += getInput(i, values);
            }
            return f;
        }
        case FLOATNET_OP_PRODUCT: {
            float f = 1.0;

            for (uint i = 0; i < myInputs.size(); i++) {
                f *= getInput(i, values);
            }
            return f;
        }

        case FLOATNET_OP_LINEAR_COMBINATION: {
            float f = 0.0;

            for (uint i = 0; i < myInputs.size(); i++) {
                f += getParam(i) * getInput(i, values);
            }
            return f;
        }

        default:
            NOT_IMPLEMENTED();
    }
}