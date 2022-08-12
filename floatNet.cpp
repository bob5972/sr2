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
#include "MBRegistry.h"
#include "MBString.hpp"
#include "textDump.hpp"

FloatNet::FloatNet(uint numInputs, uint numOutputs, uint numNodes)
{
    ASSERT(numInputs > 0);
    ASSERT(numOutputs > 0);
    ASSERT(numNodes > 0);
    ASSERT(numNodes >= numOutputs);

    myNumInputs = numInputs;
    myNumOutputs = numOutputs;

    myNodes.resize(numNodes);
    myValues.resize(myNumInputs + numNodes);

    for (uint i = 0; i < myNodes.size(); i++) {
        myNodes[i].myIndex = i + numInputs;
    }
}

void FloatNet::load(MBRegistry *mreg, const char *prefix)
{
    MBString p;

    p = prefix;
    p += ".numInputs";
    myNumInputs = MBRegistry_GetUint(mreg, p.CStr());

    p = prefix;
    p += ".numOutputs";
    myNumOutputs = MBRegistry_GetUint(mreg, p.CStr());

    p = prefix;
    p += ".numNodes";
    uint numNodes = MBRegistry_GetUint(mreg, p.CStr());

    VERIFY(myNumInputs > 0);
    VERIFY(myNumOutputs > 0);
    VERIFY(myNumOutputs <= numNodes);

    myNodes.resize(numNodes);

    for (uint i = 0; i < myNodes.size(); i++) {
        char *strp;

        p = prefix;
        asprintf(&strp, ".node[%d]", i);
        p += strp;
        free(strp);

        myNodes[i].load(mreg, p.CStr());
    }

    myValues.resize(myNumInputs + numNodes);
}

void FloatNet::Node::load(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    MBString str;

    p = prefix;
    p += ".op";
    myOp = (FloatNetOp) MBRegistry_GetUint(mreg, p.CStr());
    VERIFY(myOp != FLOATNET_OP_INVALID);
    VERIFY(myOp < FLOATNET_OP_MAX);

    p = prefix;
    p += ".numInputs";
    uint numInputs = MBRegistry_GetUint(mreg, p.CStr());
    myInputs.resize(numInputs);

    p = prefix;
    p += ".inputs";
    str = MBRegistry_GetCStr(mreg, p.CStr());
    TextDump_Convert(str, myInputs);

    p = prefix;
    p += ".numParams";
    uint numParams = MBRegistry_GetUint(mreg, p.CStr());
    myParams.resize(numParams);

    p = prefix;
    p += ".params";
    str = MBRegistry_GetCStr(mreg, p.CStr());
    TextDump_Convert(str, myParams);
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

        case FLOATNET_OP_TAKE_MIN: {
            float f = getInput(0, values);

            for (uint i = 1; i < myInputs.size(); i++) {
                float nf = getInput(i, values);
                if (nf < f) {
                    f = nf;
                }
            }
            return f;
        }
        case FLOATNET_OP_TAKE_MAX: {
            float f = getInput(0, values);

            for (uint i = 1; i < myInputs.size(); i++) {
                float nf = getInput(i, values);
                if (nf > f) {
                    f = nf;
                }
            }
            return f;
        }

        case FLOATNET_OP_TAKE_SCALED_MIN: {
            float f = getInput(0, values) * getParam(0);

            for (uint i = 1; i < myInputs.size(); i++) {
                float nf = getInput(i, values) * getParam(i);
                if (nf < f) {
                    f = nf;
                }
            }
            return f;
        }
        case FLOATNET_OP_TAKE_SCALED_MAX: {
            float f = getInput(0, values) * getParam(0);

            for (uint i = 1; i < myInputs.size(); i++) {
                float nf = getInput(i, values) * getParam(i);
                if (nf > f) {
                    f = nf;
                }
            }
            return f;
        }

        default:
            NOT_IMPLEMENTED();
    }
}