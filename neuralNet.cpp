/*
 * neuralNet.cpp -- part of SpaceRobots2
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

extern "C" {
#include "Random.h"
#include "MBRegistry.h"
}

#include "mutate.h"

#include "neuralNet.hpp"
#include "textDump.hpp"

void NeuralNet::load(MBRegistry *mreg, const char *prefix,
                     NeuralNetType nnTypeIn)
{
    MBString str;
    const char *cstr;

    ASSERT(mreg != NULL);
    ASSERT(prefix != NULL);

    ASSERT(nnTypeIn == NN_TYPE_FORCES || nnTypeIn == NN_TYPE_SCALARS);
    nnType = nnTypeIn;

    str = prefix;
    str += "fn.numInputs";
    cstr = str.CStr();
    if (MBRegistry_ContainsKey(mreg, cstr) &&
        MBRegistry_GetUint(mreg, cstr) > 0) {
        str = prefix;
        str += "fn.";
        floatNet.load(mreg, str.CStr());
    } else {
        floatNet.initialize(1, 1, 1);
        floatNet.loadZeroNet();
    }

    uint numInputs = floatNet.getNumInputs();
    uint numOutputs = floatNet.getNumOutputs();

    inputs.resize(numInputs);
    outputs.resize(numOutputs);

    inputDescs.resize(numInputs);
    outputDescs.resize(numOutputs);

    for (uint i = 0; i < outputDescs.size(); i++) {
        bool voidNode = FALSE;
        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "%soutput[%d].", prefix, i);
        VERIFY(ret > 0);
        NeuralOutput_Load(mreg, &outputDescs[i], lcstr);
        free(lcstr);

        if (nnType == NN_TYPE_SCALARS &&
            outputDescs[i].value.valueType != NEURAL_VALUE_SCALAR) {
            voidNode = TRUE;
        } else if (nnType == NN_TYPE_FORCES &&
            outputDescs[i].value.valueType != NEURAL_VALUE_FORCE) {
            voidNode = TRUE;
        } else if (outputDescs[i].value.forceDesc.filterForward &&
                   outputDescs[i].value.forceDesc.filterBackward) {
            voidNode = TRUE;
        } else if (outputDescs[i].value.forceDesc.filterAdvance &&
                   outputDescs[i].value.forceDesc.filterRetreat) {
            voidNode = TRUE;
        }

        if (outputDescs[i].value.valueType == NEURAL_VALUE_FORCE) {
            if (outputDescs[i].value.forceDesc.forceType == NEURAL_FORCE_ZERO ||
                outputDescs[i].value.forceDesc.forceType == NEURAL_FORCE_VOID) {
                voidNode = TRUE;
            }
        } else if (outputDescs[i].value.valueType == NEURAL_VALUE_VOID) {
            voidNode = TRUE;
        }

        if (voidNode) {
            voidOutputNode(i);
        }
    }

    for (uint i = 0; i < inputDescs.size(); i++) {
        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "%sinput[%d].", prefix, i);
        VERIFY(ret > 0);
        NeuralInput_Load(mreg, &inputDescs[i], lcstr);
        free(lcstr);
    }

    minimize();
}

void NeuralNet::minimize()
{
    CPBitVector inputBV;

    floatNet.minimize();
    floatNet.getUsedInputs(inputBV);

    ASSERT(inputBV.size() == inputs.size());
    ASSERT(inputs.size() == inputDescs.size());
    for (uint i = 0; i < inputDescs.size(); i++) {
        if (!inputBV.get(i)) {
            voidInputNode(i);
        }
    }
}

void NeuralNet::minimizeScalars(NeuralNet &nnConsumer)
{
    CPBitVector outputBV;
    outputBV.resize(outputs.size());
    outputBV.resetAll();

    ASSERT(nnType == NN_TYPE_SCALARS);
    ASSERT(nnConsumer.nnType == NN_TYPE_FORCES);

    for (uint i = 0; i < nnConsumer.inputDescs.size(); i++) {
        NeuralValueDesc *inp = &nnConsumer.inputDescs[i].value;
        if (inp->valueType == NEURAL_VALUE_SCALAR) {
            if (inp->scalarDesc.scalarID > 0 &&
                inp->scalarDesc.scalarID < outputs.size()) {
                outputBV.set(inp->scalarDesc.scalarID);
            } else {
                nnConsumer.voidInputNode(i);
            }
        }
    }

    for (uint i = 0; i < outputs.size(); i++) {
        if (!outputBV.get(i)) {
            voidOutputNode(i);
        }
    }

    minimize();
}

void NeuralNet::dumpSanitizedParams(MBRegistry *mreg, const char *prefix)
{
    /*
     * If we voided out the inputs/outputs when the FloatNet was minimized,
     * reflect that here.
     */
    for (uint i = 0; i < inputDescs.size(); i++) {
        if (inputDescs[i].value.valueType == NEURAL_VALUE_VOID) {
            char *str = NULL;
            const char *value;
            int ret = asprintf(&str, "%sinput[%d].valueType", prefix, i);
            VERIFY(ret > 0);
            value = NeuralValue_ToString(inputDescs[i].value.valueType);
            MBRegistry_PutCopy(mreg, str, value);
            free(str);
        }
    }
    for (uint i = 0; i < outputDescs.size(); i++) {
        if (outputDescs[i].value.valueType == NEURAL_VALUE_FORCE &&
            outputDescs[i].value.forceDesc.forceType == NEURAL_FORCE_VOID) {
            char *str = NULL;
            const char *value;
            int ret = asprintf(&str, "%soutput[%d].forceType", prefix, i);
            VERIFY(ret > 0);
            value = NeuralForce_ToString(outputDescs[i].value.forceDesc.forceType);
            MBRegistry_PutCopy(mreg, str, value);
            free(str);
        }
    }
}


void NeuralNet_Mutate(MBRegistry *mreg, const char *prefix, float rate,
                      NeuralNetType nnType,
                      uint maxInputs, uint maxOutputs, uint maxNodes,
                      uint maxNodeDegree)
{
    FloatNet fn;
    MBString str;
    const char *cstr;

    str = prefix;
    str += "fn.numInputs";
    cstr = str.CStr();
    if (MBRegistry_ContainsKey(mreg, cstr) &&
        MBRegistry_GetUint(mreg, cstr) > 0 &&
        rate < 1.0f) {
        str = prefix;
        str += "fn.";
        fn.load(mreg, str.CStr());
    } else {
        fn.initialize(maxInputs, maxOutputs, maxNodes);
        fn.loadZeroNet();
    }

    fn.mutate(rate, maxNodeDegree, maxNodes);

    str = prefix;
    str += "fn.";
    MBRegistry_RemoveAllWithPrefix(mreg, str.CStr());
    fn.save(mreg, str.CStr());

    for (uint i = 0; i < fn.getNumInputs(); i++) {
        char *str = NULL;
        int ret = asprintf(&str, "%sinput[%d].", prefix, i);
        VERIFY(ret > 0);
        NeuralInput_Mutate(mreg, rate, nnType, str);
        free(str);
    }

    for (uint i = 0; i < fn.getNumOutputs(); i++) {
        char *str = NULL;
        int ret = asprintf(&str, "%soutput[%d].", prefix, i);
        VERIFY(ret > 0);
        NeuralOutput_Mutate(mreg, rate, nnType, str);
        free(str);
    }
}


void NeuralNet::fillInputs(Mob *mob)
{
    if (mob == NULL) {
        mob = aic.sg->friendBaseShadow();
    }

    ASSERT(inputs.size() == inputDescs.size());

    for (uint i = 0; i < inputDescs.size(); i++) {
        inputs[i] = getInputValue(mob, i);
    }
}


void NeuralNet::compute()
{
    float maxV = (1.0f / MICRON);

    floatNet.compute(inputs, outputs);

    ASSERT(outputs.size() == outputDescs.size());
    for (uint i = 0; i < outputs.size(); i++) {
        if (!isOutputActive(&outputDescs[i])) {
            outputs[i] = 0.0f;
        } else if (isnan(outputs[i])) {
            outputs[i] = 0.0f;
        } else if (outputs[i] > maxV) {
            outputs[i] = maxV;
        } else if (outputs[i] < -maxV) {
            outputs[i] = -maxV;
        }
    }
}


void NeuralNet::doScalars()
{
    ASSERT(nnType == NN_TYPE_SCALARS);

    fillInputs(NULL);
    compute();

    for (uint i = 0; i < outputs.size(); i++) {
        if (mb_debug) {
            if (outputDescs[i].value.valueType != NEURAL_VALUE_SCALAR &&
                outputDescs[i].value.valueType != NEURAL_VALUE_VOID) {
                PANIC("Bad outputDesc valueType=%s(%d)\n",
                      NeuralValue_ToString(outputDescs[i].value.valueType),
                      outputDescs[i].value.valueType);
            }
        }
    }
}



void NeuralNet::doForces(Mob *mob, FRPoint *outputForce)
{
    ASSERT(nnType == NN_TYPE_FORCES);

    fillInputs(mob);
    compute();

    FRPoint_Zero(outputForce);
    ASSERT(outputs.size() == outputDescs.size());
    for (uint i = 0; i < outputDescs.size(); i++) {
        FRPoint force;
        if (outputDescs[i].value.valueType == NEURAL_VALUE_FORCE) {
            ASSERT(outputDescs[i].value.forceDesc.forceType != NEURAL_FORCE_ZERO);
            if (outputs[i] != 0.0f &&
                outputConditionApplies(mob, i) &&
                getOutputForce(mob, i, &force)) {
                NeuralCombiner_ApplyOutput(outputDescs[i].cType, outputs[i], &force);
                FRPoint_Add(&force, outputForce, outputForce);
            }
        } else {
            ASSERT(outputDescs[i].value.valueType == NEURAL_VALUE_VOID);
        }
    }
}
