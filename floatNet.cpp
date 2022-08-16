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
#include "Random.h"

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
        myNodes[i].index = i + numInputs;
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

void FloatNet::save(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    char *v;

    p = prefix;
    p += ".numInputs";
    asprintf(&v, "%d", myNumInputs);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

    p = prefix;
    p += ".numOutputs";
    asprintf(&v, "%d", myNumOutputs);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

    p = prefix;
    p += ".numNodes";
    asprintf(&v, "%d", myNodes.size());
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

    for (uint i = 0; i < myNodes.size(); i++) {
        char *strp;

        p = prefix;
        asprintf(&strp, ".node[%d]", i);
        p += strp;
        free(strp);

        myNodes[i].save(mreg, p.CStr());
    }
}

void FloatNet::mutate()
{
    for (uint i = 0; i < myNodes.size(); i++) {
        myNodes[i].mutate();
    }
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

