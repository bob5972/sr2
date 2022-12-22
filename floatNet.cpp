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

    myNumInputs = numInputs;
    myNumOutputs = numOutputs;
    myNumNodes = myNumInputs + numInnerNodes;

    myNodes.resize(myNumNodes);
    myValues.resize(myNumNodes);
    myUsedInputs.resize(myNumInputs);
    myUsedOutputs.resize(myNumOutputs);

    myUsedInputs.setAll();
    myUsedOutputs.setAll();

     ASSERT(!myHaveOutputOrdering);

     loadZeroNet();
     myInitialized = TRUE;
 }

 void FloatNet::loadZeroNet()
 {
    if (myInitialized) {
        checkInvariants();
    }

    ASSERT(myNumNodes == myNodes.size());
    for (uint i = 0; i < myNodes.size(); i++) {
        myNodes[i].index = i;

        if (i < myNumInputs) {
            myNodes[i].op = ML_FOP_INPUT;

            ASSERT(myNodes[i].params.size() == 0);
            ASSERT(myNodes[i].inputs.size() == 0);
        } else {
            myNodes[i].op = ML_FOP_0x0_ZERO;

            for (uint k = 0; k < myNodes[i].params.size(); k++) {
                myNodes[i].params[k] = 0.0f;
            }
            for (uint k = 0; k < myNodes[i].inputs.size(); k++) {
                myNodes[i].inputs[k] = 0;
            }
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
    uint numInnerNodes;

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
        numInnerNodes = MBRegistry_GetUint(mreg, p.CStr());
        VERIFY(numInnerNodes > 1);
        myNumNodes = myNumInputs + numInnerNodes;
    } else {
        p = prefix;
        p += "numNodes";
        uint numNodes = MBRegistry_GetUint(mreg, p.CStr());
        VERIFY(numNodes >= myNumOutputs);
        myNumNodes = myNumInputs + numNodes;
    }

    initialize(myNumInputs, myNumOutputs, myNumNodes - myNumInputs);
    checkInvariants();

    for (uint i = myNumInputs; i < myNodes.size(); i++) {
        char *strp;

        p = prefix;
        int ret = asprintf(&strp, "node[%d].", i);
        VERIFY(ret > 0);
        p += strp;
        free(strp);
        strp = NULL;

        myNodes[i].load(mreg, p.CStr());
    }

    myValues.resize(myNumNodes);

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
    p += "numInnerNodes";
    ASSERT(myNodes.size() > myNumInputs);
    ret = asprintf(&v, "%d", myNodes.size() - myNumInputs);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);
    v = NULL;

    checkInvariants();

    for (uint i = myNumInputs; i < myNodes.size(); i++) {
        char *strp = NULL;

        p = prefix;
        ret = asprintf(&strp, "node[%d].", i);
        VERIFY(ret > 0);
        p += strp;
        free(strp);
        strp = NULL;

        ASSERT(myNodes[i].index == i);
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
}

void FloatNet::mutate(float rate, uint maxNodeDegree, uint maxNodes)
{
    checkInvariants();

    for (uint i = 0; i < myNodes.size(); i++) {
        if (i < myNumInputs) {
            ASSERT(myNodes[i].op == ML_FOP_INPUT ||
                   myNodes[i].op == ML_FOP_VOID);
        } else {
            ASSERT(myNodes[i].op != ML_FOP_INPUT);

            if (Random_Flip(rate / 10.0f)) {
                uint n = Random_Int(0, i);
                if (n >= myNumInputs) {
                    myNodes[i].op = myNodes[n].op;
                    myNodes[i].params = myNodes[n].params;
                    myNodes[i].inputs = myNodes[n].inputs;
                }
            }
            myNodes[i].mutate(rate, maxNodeDegree, maxNodeDegree);
        }

        checkInvariants();
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
    ASSERT(myValues.size() == myNodes.size());

    for (uint i = 0; i < myNumInputs; i++) {
        ASSERT(myNodes[i].op == ML_FOP_INPUT ||
               myNodes[i].op == ML_FOP_VOID);
        myValues[i] = inputs[i];
    }

    for (uint i = myNumInputs; i < myNodes.size(); i++) {
        myValues[i] = myNodes[i].compute(myValues);
    }

    ASSERT(myNodes.size() == myValues.size());
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
    nodesBV.resize(myNodes.size());
    nodesBV.resetAll();
    for (uint i = 0; i < myValues.size(); i++) {
        myValues[i] = 0.0f;
    }
    for (uint i = myNumInputs; i < myNodes.size(); i++) {
        MLFloatNode *n = &myNodes[i];
        ASSERT(myNodes[i].index == i);

        bool allConstant;
        if (n->isInput()) {
            allConstant = FALSE;
        } else if (n->isConstant()) {
            allConstant = TRUE;
        } else {
            allConstant = TRUE;
            for (uint ii = 0; ii < n->inputs.size(); ii++) {
                if (n->inputs[ii] >= myNumInputs) {
                    uint indexi = n->inputs[ii];
                    MLFloatNode *ni = &myNodes[indexi];
                    ASSERT(ni->index == indexi);
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
            ASSERT(myNodes[i].op != ML_FOP_INPUT);
            myValues[i] = n->compute(myValues);
            n->makeConstant(myValues[i]);
            nodesBV.set(i);
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

        for (uint i = 0; i < myNumOutputs; i++) {
            if (myUsedOutputs.get(i)) {
                if (myHaveOutputOrdering) {
                    ASSERT(myOutputOrdering.size() == myNumOutputs);
                    bv.set(myOutputOrdering[i]);
                } else {
                    bv.set(i + myNodes.size() - myNumOutputs);
                }
            }
        }

        checkInvariants();

        for (uint i = 0; i < myNodes.size(); i++) {
            MLFloatNode *n = &myNodes[i];
            ASSERT(myNodes[i].index == i);
            if (n->isVoid()) {
                /*
                 * Treat already voided nodes as "referenced" for now,
                 * because we don't need to void them again.
                 */
                bv.set(i);
            } else {
                for (uint in = 0; in < n->inputs.size(); in++) {
                    bv.set(n->inputs[in]);
                }
            }
        }

        for (uint i = 0; i < myNodes.size(); i++) {
            ASSERT(myNodes[i].index == i);
            if (!bv.get(i)) {
                myNodes[i].makeVoid();
                keepGoing = TRUE;
            }
        }

        VERIFY(iterations < 1 + myNodes.size());
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
