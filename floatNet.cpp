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

void FloatNet::initialize(uint numInputs, uint numOutputs, uint numNodes)
{
    ASSERT(!myInitialized);
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

    myInitialized = TRUE;
}

void FloatNet::loadZeroNet()
{
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
}

void FloatNet::load(MBRegistry *mreg, const char *prefix)
{
    MBString p;

    p = prefix;
    p += "numInputs";
    myNumInputs = MBRegistry_GetUint(mreg, p.CStr());

    p = prefix;
    p += "numOutputs";
    myNumOutputs = MBRegistry_GetUint(mreg, p.CStr());

    p = prefix;
    p += "numNodes";
    uint numNodes = MBRegistry_GetUint(mreg, p.CStr());

    VERIFY(myNumInputs > 0);
    if (myNumOutputs <= 0) {
        PANIC("Not enough outputs: myNumOutputs=%d\n", myNumOutputs);
    }
    if (myNumOutputs > numNodes) {
        PANIC("Too many outputs: myNumOutputs=%d, numNodes=%d\n",
              myNumOutputs, numNodes);
    }

    initialize(myNumInputs, myNumOutputs, numNodes);
    ASSERT(myNodes.size() == numNodes);

    for (uint i = 0; i < myNodes.size(); i++) {
        char *strp;

        p = prefix;
        int ret = asprintf(&strp, "node[%d].", i + myNumInputs);
        VERIFY(ret > 0);
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
    int ret;

    p = prefix;
    p += "numInputs";
    ret = asprintf(&v, "%d", myNumInputs);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

    p = prefix;
    p += "numOutputs";
    ret = asprintf(&v, "%d", myNumOutputs);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

    p = prefix;
    p += "numNodes";
    ret = asprintf(&v, "%d", myNodes.size());
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, p.CStr(), v);
    free(v);

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

        myNodes[i].save(mreg, p.CStr());
    }
}

// void FloatNet::nodeShiftHelper(uint index, bool shiftUp)
// {
//     for (uint i = index; i < myNodes.size(); i++) {
//         MLFloatNode *n = &myNodes[i];

//         if (shiftUp) {
//             n->index++;
//         } else {
//             n->index--;
//         }

//         for (uint ni = 0; ni < n->inputs.size(); ni++) {
//             if (n->inputs[ni] >= index + myNumInputs) {
//                 if (shiftUp) {
//                     n->inputs[ni]++;
//                 } else {
//                     n->inputs[ni]--;
//                 }
//             }
//         }
//     }
// }

void FloatNet::mutate(float rate, uint maxNodeDegree, uint maxNodes)
{
    // if (Random_Flip(rate)) {
    //     if (myNodes.size() < maxNodes) {
    //         int i;
    //         myNodes.resize(myNodes.size() + 1);
    //         for (i = 0; i < myNumOutputs; i++) {
    //             int n = myNodes.size() - i - 1;
    //             myNodes[n] = myNodes[n - 1];
    //         }
    //         int index = myNodes.size() - myNumOutputs - 1;
    //         myNodes[myNodes.size() - myNumOutputs - 1].makeZero();
    //         nodeShiftHelper(index, TRUE);
    //     }
    // }

    // if (Random_Flip(rate)) {
    //     ASSERT(myNumInputs > 1);
    //     if (myNodes.size() > 1 && myNodes.size() > myNumOutputs) {
    //         int index = Random_Int(0, myNodes.size() - myNumOutputs - 1);
    //         for (uint i = index; i < myNodes.size() - 1; i++) {
    //             myNodes[i] = myNodes[i + 1];
    //         }
    //         myNodes.shrink();
    //         nodeShiftHelper(index, FALSE);
    //     }
    // }

    for (uint i = 0; i < myNodes.size(); i++) {
        if (Random_Flip(rate / 10.0f)) {
            uint n = Random_Int(0, i);
            myNodes[i] = myNodes[n];
        }
        myNodes[i].mutate(rate, maxNodeDegree, maxNodeDegree);
    }
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

    ASSERT(myNodes.size() >= myNumOutputs);
    for (uint i = 0; i < myNumOutputs; i++) {
        uint vi = i + myValues.size() - myNumOutputs;
        outputs[i] = myValues[vi];
    }
}

uint FloatNet::minimize(CPBitVector *inputBV)
{
    CPBitVector bv;
    bool keepGoing = TRUE;
    uint activeCount;
    uint iterations = 0;

    /*
     * Handle all simple reductions.
     */
    for (uint i = 0; i < myNodes.size(); i++) {
        myNodes[i].minimize();
    }

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

    /*
     * Compute reachable nodes.
     */
    bv.resize(myNumInputs + myNodes.size());
    while (keepGoing) {
        activeCount = 0;
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
                activeCount++;
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

    /*
     * Copy reachable nodes out to the caller.
     */
    if (inputBV != NULL) {
        ASSERT(inputBV->size() == myNumInputs);

        for (uint i = 0; i < myNumInputs; i++) {
            inputBV->put(i, bv.get(i));
        }
    }

    return activeCount;

    /*
     * XXX TODO:
     *    Collapse zero nodes?
     */
}