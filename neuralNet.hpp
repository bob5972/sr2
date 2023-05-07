/*
 * neuralNet.hpp -- part of SpaceRobots2
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

#ifndef _NEURAL_NET_H_202210081813
#define _NEURAL_NET_H_202210081813

#include "mob.h"
#include "sensorGrid.hpp"
#include "aiTypes.hpp"
#include "floatNet.hpp"
#include "basicShipAI.hpp"
#include "neural.hpp"
#include "MBString.hpp"

class NeuralNet {
public:
    // Members
    NeuralNetType nnType;
    FloatNet floatNet;
    MBVector<NeuralInputDesc> inputDescs;
    MBVector<NeuralOutputDesc> outputDescs;
    MBVector<float> inputs;
    MBVector<float> outputs;
    AIContext aic;
    MBVector<float> scalarInputs;
    MBVector<NeuralLocusPosition> loci;

    NeuralNet() {
        MBUtil_Zero(&aic, sizeof(aic));
    }

    // XXX: Only saves the FloatNet.
    void save(MBRegistry *mreg, const char *prefix) {
        MBString str = prefix;
        str += "fn.";
        floatNet.save(mreg, str.CStr());
    }

    void dumpSanitizedParams(MBRegistry *mreg, const char *prefix);

    void load(MBRegistry *mreg, const char *prefix, NeuralNetType nnType);

    void fillInputs(Mob *mob);
    void compute();
    void doScalars();
    void doForces(Mob *mob, FRPoint *outputForce);

    void pullScalars(const NeuralNet &nn) {
        scalarInputs.resize(nn.outputs.size());

        for (uint i = 0; i < scalarInputs.size(); i++) {
            scalarInputs[i] = nn.outputs[i];
        }
    }

    void voidInputNode(uint i) {
        inputDescs[i].value.valueType = NEURAL_VALUE_VOID;
    }
    void voidOutputNode(uint i) {
        floatNet.voidOutputNode(i);
        if (outputDescs[i].value.valueType == NEURAL_VALUE_FORCE) {
            outputDescs[i].value.forceDesc.forceType = NEURAL_FORCE_VOID;
        } else {
            outputDescs[i].value.valueType = NEURAL_VALUE_VOID;

        }
    }

    void minimize();
    void minimizeScalars(NeuralNet &nn);

    static bool isOutputActive(const NeuralOutputDesc *outputDesc) {
        if  (outputDesc->value.valueType == NEURAL_VALUE_VOID ||
             outputDesc->value.valueType == NEURAL_VALUE_ZERO) {
            return FALSE;
        } else if (outputDesc->value.valueType == NEURAL_VALUE_SCALAR) {
            return TRUE;
        } else if (outputDesc->value.valueType == NEURAL_VALUE_FORCE) {
            if (outputDesc->value.forceDesc.forceType == NEURAL_FORCE_VOID ||
                outputDesc->value.forceDesc.forceType == NEURAL_FORCE_ZERO) {
                return FALSE;
            }

            return TRUE;
        } else {
            NOT_IMPLEMENTED();
        }
    }

private:
    // Helpers
    bool getFocus(Mob *mob, NeuralForceDesc *desc, FPoint *focusPoint) {
        ASSERT(desc != NULL);
        ASSERT(mob != NULL);
        ASSERT(focusPoint != NULL);

        if (desc->forceType == NEURAL_FORCE_VOID) {
            return FALSE;
        } else if (desc->forceType == NEURAL_FORCE_LOCUS) {
            int index = desc->index;
            if (index < 0 || index >= loci.size()) {
                return FALSE;
            }

            if (!loci[index].active) {
                return FALSE;
            }

            *focusPoint = loci[index].pos;
            return TRUE;
        } else if (desc->forceType == NEURAL_FORCE_NEXT_LOCUS) {
            uint size = loci.size();
            int offset = MAX(0, desc->index);
            for (int i = 0; i < size; i++) {
                int index = (i + offset) % size;
                ASSERT(index >= 0 && index <= size);

                if (loci[index].active) {
                    *focusPoint = loci[index].pos;
                    return TRUE;
                }
            }
            return FALSE;
        } else {
            return NeuralForce_GetFocus(&aic, mob, desc, focusPoint);
        }
    }

    float getInputValue(Mob *mob, uint index) {
        NeuralInputDesc *desc = &inputDescs[index];

        if (desc->value.valueType == NEURAL_VALUE_FORCE &&
            desc->value.forceDesc.forceType == NEURAL_FORCE_LOCUS) {
            FPoint focus;
            bool haveFocus = getFocus(mob, &desc->value.forceDesc, &focus);
            return NeuralForce_FocusToValue(&aic, mob, &desc->value.forceDesc,
                                            &focus, haveFocus);
        } else if (desc->value.valueType == NEURAL_VALUE_SCALAR) {
            if (desc->value.scalarDesc.scalarID < 0 ||
                desc->value.scalarDesc.scalarID >= scalarInputs.size()) {
                return 0.0f;
            }

            return scalarInputs[desc->value.scalarDesc.scalarID];
        } else {
            return NeuralValue_GetValue(&aic, mob, &inputDescs[index].value, index);
        }
    }
    bool getOutputForce(Mob *mob, uint index, FRPoint *rForce) {
        NeuralValueDesc *desc = &outputDescs[index].value;
        FPoint focus;
        bool haveForce;

        ASSERT(desc->valueType == NEURAL_VALUE_FORCE);
        ASSERT(desc->forceDesc.forceType != NEURAL_FORCE_ZERO);

        haveForce = getFocus(mob, &desc->forceDesc, &focus);
        return NeuralForce_FocusToForce(&aic, mob, &desc->forceDesc,
                                        &focus, haveForce, rForce);
    }

    bool outputConditionApplies(Mob *m, uint i) {
        if (!NN_USE_CONDITIONS) {
            return TRUE;
        } else {
            if (i >= outputDescs.size()) {
                return TRUE;
            }

            return NeuralCondition_AppliesToMob(&aic, m,
                                                &outputDescs[i].condition);
        }
    }
};

void NeuralNet_Mutate(MBRegistry *mreg, const char *prefix, float rate,
                      NeuralNetType nnType,
                      uint maxInputs, uint maxOutputs, uint maxNodes,
                      uint maxNodeDegree);

#endif // _NEURAL_NET_H_202210081813
