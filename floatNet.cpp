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

void FloatNet::initialize(uint numInputs, uint numOutputs, uint numInnerNodes)
{
    ASSERT(!myInitialized);
    ASSERT(numInputs > 0);
    ASSERT(numOutputs > 0);
    ASSERT(numInnerNodes > 0);
    ASSERT(numInnerNodes >= numOutputs);

    myNumInputs = numInputs;
    myNumOutputs = numOutputs;
    myNumNodes = numInnerNodes;

    myNodes.resize(myNumNodes);
    myValues.resize(myNumInputs + myNumNodes);
    myUsedInputs.resize(myNumInputs);
    myUsedOutputs.resize(myNumOutputs);

    myUsedInputs.setAll();
    myUsedOutputs.setAll();

    ASSERT(myNumNodes == myNodes.size());
    for (uint i = 0; i < myNodes.size(); i++) {
        myNodes[i].index = i + numInputs;
    }

    myInitialized = TRUE;

    ASSERT(!myHaveOutputOrdering);

    checkInvariants();

    loadZeroNet();
    checkInvariants();
}

void FloatNet::loadZeroNet()
{
    checkInvariants();

    for (uint i = 0; i < myNodes.size(); i++) {
        myNodes[i].op = ML_FOP_0x0_ZERO;

        for (uint k = 0; k < myNodes[i].params.size(); k++) {
            myNodes[i].params[k] = 0.0f;
        }
        for (uint k = 0; k < myNodes[i].inputs.size(); k++) {
            myNodes[i].inputs[k] = 0;
        }
    }

    for (uint i = 0; i < myValues.size(); i++) {
        myValues[i] = 0.0f;
    }

    checkInvariants();
}

void FloatNet::load(MBRegistry *mreg, const char *prefix)
{
    MBString p;

    p = prefix;
    p += "numInputs";
    myNumInputs = MBRegistry_GetUint(mreg, p.CStr());
    if (myNumInputs <= 0) {
        PANIC("Not enough inputs: myNumInputs=%d\n", myNumInputs);
    }

    p = prefix;
    p += "numOutputs";
    myNumOutputs = MBRegistry_GetUint(mreg, p.CStr());
    if (myNumOutputs <= 0) {
        PANIC("Not enough outputs: myNumOutputs=%d\n", myNumOutputs);
    }

    p = prefix;
    p += "numInnerNodes";
    if (MBRegistry_ContainsKey(mreg, p.CStr())) {
        NOT_IMPLEMENTED();
    } else {
        p = prefix;
        p += "numNodes";
        myNumNodes = MBRegistry_GetUint(mreg, p.CStr());
    }

    if (myNumOutputs > myNumNodes) {
        PANIC("Too many outputs: myNumOutputs=%d, myNumNodes=%d\n",
              myNumOutputs, myNumNodes);
    }

    initialize(myNumInputs, myNumOutputs, myNumNodes);
    checkInvariants();

    for (uint i = 0; i < myNodes.size(); i++) {
        char *strp;

        p = prefix;
        int ret = asprintf(&strp, "node[%d].", i + myNumInputs);
        VERIFY(ret > 0);
        p += strp;
        free(strp);
        strp = NULL;

        myNodes[i].load(mreg, p.CStr());
    }

    myValues.resize(myNumInputs + myNumNodes);

    p = prefix;
    p += "haveOutputOrdering";
    if (MBRegistry_GetBoolD(mreg, p.CStr(), FALSE)) {
        for (uint i = 0; i < myNumOutputs; i++) {
            char *k = NULL;

            int ret = asprintf(&k, "%soutput[%d].node", prefix, i);
            VERIFY(ret > 0);

            myOutputOrdering[i] = MBRegistry_GetUint(mreg, k);
            VERIFY(myOutputOrdering[i] < myNodes.size());

            free(k);
        }
    } else {
        myHaveOutputOrdering = FALSE;
        VERIFY(myNumNodes >= myNumOutputs);
    }

    checkInvariants();
}

void FloatNet::save(MBRegistry *mreg, const char *prefix)
{
    MBString p;
    char *v = NULL;
    int ret;

    checkInvariants();

    p = prefix;
    p += "numInputs";
    ret = asprintf(&v, "%d", myNumInputs);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);
    v = NULL;

    p = prefix;
    p += "numOutputs";
    ret = asprintf(&v, "%d", myNumOutputs);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);
    v = NULL;

    p = prefix;
    p += "numNodes";
    ASSERT(myNumNodes == myNodes.size());
    ret = asprintf(&v, "%d", myNumNodes);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);
    v = NULL;

    for (uint i = 0; i < myNodes.size(); i++) {
        char *strp;

        /*
         * Save the node ID offset by myNumInputs to it matches the
         * ID used in the node input references.
         */
        p = prefix;
        ret = asprintf(&strp, "node[%d].", i + myNumInputs);
        VERIFY(ret > 0);
        p += strp;
        free(strp);
        strp = NULL;

        myNodes[i].save(mreg, p.CStr());
    }

    if (myHaveOutputOrdering) {
        p = prefix;
        p += "haveOutputOrdering";
        MBRegistry_PutCopy(mreg, p.CStr(), "TRUE");

        for (uint i = 0; i < myNumOutputs; i++) {
            char *k = NULL;

            ret = asprintf(&k, "%soutput[%d].node", prefix, i);
            VERIFY(ret > 0);

            ASSERT(myOutputOrdering[i] < myNodes.size());
            ret = asprintf(&v, "%d", myOutputOrdering[i]);
            VERIFY(ret > 0);

            MBRegistry_PutCopy(mreg, k, v);

            free(v);
            free(k);
        }
    } else {
        ASSERT(myNumNodes >= myNumOutputs);
    }

    checkInvariants();
}

void FloatNet::mutate(float rate, uint maxNodeDegree, uint maxNodes)
{
    checkInvariants();

    for (uint i = 0; i < myNodes.size(); i++) {
        if (Random_Flip(rate / 10.0f)) {
            uint n = Random_Int(0, i);
            myNodes[i] = myNodes[n];
        }
        myNodes[i].mutate(rate, maxNodeDegree, maxNodeDegree);
    }

    checkInvariants();

    if (!myHaveOutputOrdering) {
        ASSERT(myNumNodes >= myNumOutputs);
        myHaveOutputOrdering = TRUE;
        myOutputOrdering.resize(myNumOutputs);
        for (uint i = 0; i < myNumOutputs; i++) {
            myOutputOrdering[i] = i + myNodes.size() - myNumOutputs;
        }
    }

    checkInvariants();

    ASSERT(myHaveOutputOrdering);
    for (uint i = 0; i < myNumOutputs; i++) {
        if (Random_Flip(rate)) {
            myOutputOrdering[i] = Random_Int(0, myNodes.size() - 1);
        }
    }

    checkInvariants();
}


void FloatNet::compute(const MBVector<float> &inputs,
                       MBVector<float> &outputs)
{
    ASSERT(inputs.size() == myNumInputs);
    ASSERT(outputs.size() == myNumOutputs);
    ASSERT(myValues.size() >= myNumInputs);

    for (uint i = 0; i < myNumInputs; i++) {
        myValues[i] = inputs[i];
    }

    for (uint i = 0; i < myNodes.size(); i++) {
        uint vi = i + myNumInputs;
        myValues[vi] = myNodes[i].compute(myValues);
    }

    if (!myHaveOutputOrdering) {
        ASSERT(myNodes.size() >= myNumOutputs);
        for (uint i = 0; i < myNumOutputs; i++) {
            uint vi = i + myValues.size() - myNumOutputs;
            outputs[i] = myValues[vi];
        }
    } else {
        ASSERT(myOutputOrdering.size() == myNumOutputs);
        for (uint i = 0; i < myNumOutputs; i++) {
            uint vi = myOutputOrdering[i];
            ASSERT(vi < myValues.size());
            outputs[i] = myValues[vi];
        }
    }
}

void FloatNet::minimize()
{
    CPBitVector bv;
    bool keepGoing = TRUE;
    uint iterations = 0;

    checkInvariants();

    /*
     * Handle all simple reductions.
     */
    for (uint i = 0; i < myNodes.size(); i++) {
        myNodes[i].minimize();
    }

    checkInvariants();

    /*
     * Constant folding.
     */
    CPBitVector nodesBV;
    nodesBV.resize(myNodes.size() + myNumInputs);
    nodesBV.resetAll();
    for (uint i = 0; i < myValues.size(); i++) {
        myValues[i] = 0.0f;
    }
    for (uint i = 0; i < myNodes.size(); i++) {
        MLFloatNode *n = &myNodes[i];
        uint vindex = i + myNumInputs;
        ASSERT(myNodes[i].index == vindex);

        bool allConstant = TRUE;
        if (!n->isConstant()) {
            for (uint ii = 0; ii < n->inputs.size(); ii++) {
                if (n->inputs[ii] >= myNumInputs) {
                    uint indexi = n->inputs[ii] - myNumInputs;
                    MLFloatNode *ni = &myNodes[indexi];
                    ASSERT(ni->index == indexi + myNumInputs);
                    if (!nodesBV.get(n->inputs[ii])) {
                        ASSERT(!ni->isConstant());
                        allConstant = FALSE;
                        break;
                    } else {
                        ASSERT(ni->isConstant());
                    }
                } else {
                    allConstant = FALSE;
                    break;
                }
            }
        }

        if (allConstant) {
            myValues[vindex] = n->compute(myValues);
            n->makeConstant(myValues[vindex]);
            nodesBV.set(vindex);
        }
    }

    checkInvariants();

    /*
     * Compute reachable nodes.
     */
    bv.resize(myNumInputs + myNodes.size());
    while (keepGoing) {
        keepGoing = FALSE;
        bv.resetAll();
        for (uint i = 0; i < myNodes.size(); i++) {
            MLFloatNode *n = &myNodes[i];
            ASSERT(myNodes[i].index == i + myNumInputs);
            if (n->isVoid()) {
                /*
                 * Treat already voided nodes as "referenced" for now,
                 * because we don't need to void them again.
                 */
                bv.set(i + myNumInputs);
            } else {
                for (uint in = 0; in < n->inputs.size(); in++) {
                    bv.set(n->inputs[in]);
                }
            }
        }

        uint numInnerNodes = myNodes.size() - myNumOutputs;
        ASSERT(myNodes.size() >= myNumOutputs);
        for (uint i = 0; i < numInnerNodes; i++) {
            uint ni = i + myNumInputs;
            ASSERT(myNodes[i].index == ni);
            if (!bv.get(ni)) {
                myNodes[i].makeVoid();
                keepGoing = TRUE;
            }
        }

        VERIFY(iterations < 1 + myNumInputs + myNodes.size());
        iterations++;
    }

    checkInvariants();

    ASSERT(myUsedInputs.size() == myNumInputs);
    for (uint i = 0; i < myNumInputs; i++) {
        myUsedInputs.put(i, bv.get(i));
    }

    checkInvariants();

    /*
     * XXX TODO:
     *    Collapse zero nodes?
     */
}
