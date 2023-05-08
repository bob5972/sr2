/*
 * matrixFleet.cpp -- part of SpaceRobots2
 * Copyright (C) 2023 Michael Banack <github@banack.net>
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

#include "Random.h"
#include "fleet.h"
#include "mutate.h"
#include "MBUtil.h"

#include "sensorGrid.hpp"
#include "basicShipAI.hpp"
#include "MBMap.hpp"
#include "MBString.hpp"
#include "ml.hpp"
#include "floatNet.hpp"
#include "textDump.hpp"

#include "neuralNet.hpp"
#include "fleetConfig.h"

#define MATRIX_SCRAMBLE_KEY "matrix.scrambleMutation"

#define MATRIX_DEFAULT_NODES 8


class MatrixAIGovernor : public BasicAIGovernor
{
public:
    // Members
    AIContext myAIC;

    MBVector<NeuralInputDesc> inputDescs;
    MBVector<NeuralOutputDesc> outputDescs;
    MBVector<float> weights;
    MBVector<float> inputs;
    MBVector<float> outputs;

public:
    MatrixAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        myAIC.rs = &myRandomState;
        myAIC.sg = sg;
        myAIC.ai = myFleetAI;
    }

    virtual ~MatrixAIGovernor() { }

    class MatrixShipAI : public BasicShipAI
    {
        public:
        MatrixShipAI(MobID mobid, MatrixAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        { }

        virtual ~MatrixShipAI() {}
    };

    virtual MatrixShipAI *newShip(MobID mobid) {
        return new MatrixShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        FleetConfig_PushDefaults(mreg, aiType);
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        MBString s;
        uint i;
        uint numInputs, numOutputs;

        s = "numInputs";
        numInputs = MBRegistry_GetUint(mreg, s.CStr());
        if (numInputs == 0) {
            numInputs = MATRIX_DEFAULT_NODES;
        }
        inputs.resize(numInputs);
        inputDescs.resize(numInputs);

        s = "numOutputs";
        numOutputs = MBRegistry_GetUint(mreg, s.CStr());
        if (numOutputs == 0) {
            numOutputs = MATRIX_DEFAULT_NODES;
        }
        outputs.resize(numOutputs);
        outputDescs.resize(numOutputs);

        weights.resize(inputs.size() * outputs.size());

        for (i = 0; i < inputs.size(); i++) {
            char *lcstr = NULL;
            int ret = asprintf(&lcstr, "input[%d].", i);
            VERIFY(ret > 0);
            NeuralInput_Load(mreg, &inputDescs[i], lcstr);
            free(lcstr);
        }

        for (i = 0; i < outputs.size(); i++) {
            char *lcstr = NULL;
            int ret = asprintf(&lcstr, "output[%d].", i);
            VERIFY(ret > 0);
            NeuralOutput_Load(mreg, &outputDescs[i], lcstr);
            free(lcstr);

            if (outputDescs[i].value.valueType != NEURAL_VALUE_FORCE) {
                outputDescs[i].value.valueType = NEURAL_VALUE_VOID;
            }
        }

        MBVector<float> row;
        row.resize(inputs.size());
        for (i = 0; i < outputs.size(); i++) {
            MBString rowStr;
            uint j;

            char *lcstr = NULL;
            int ret = asprintf(&lcstr, "weight[%d].", i);
            VERIFY(ret > 0);
            rowStr = MBRegistry_GetCStr(mreg, lcstr);
            TextDump_Convert(rowStr, row);
            free(lcstr);

            for (j = 0; j < inputs.size(); j++) {
                uint w = i * inputs.size() + j;

                if (j < row.size()) {
                    weights[w] = row[j];
                } else {
                    weights[w] = 0.0f;
                }
            }
        }

        // XXX: Void invalid inputs/outputs.

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    AIContext *getAIContext(void) {
        ASSERT(myAIC.rs != NULL);
        ASSERT(myAIC.sg != NULL);
        ASSERT(myAIC.ai != NULL);
        return &myAIC;
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {
        BasicAIGovernor::doAttack(mob, enemyTarget);
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        MatrixShipAI *ship = (MatrixShipAI *)mob->aiMobHandle;
        ASSERT(ship == (MatrixShipAI *)getShip(mob->mobid));
        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        uint i;

        uint numOutputs = outputs.size();
        uint numInputs = inputs.size();
        ASSERT(inputDescs.size() == numInputs);
        ASSERT(outputDescs.size() == numOutputs);

        for (i = 0; i < numInputs; i++) {
            inputs[i] = NeuralValue_GetValue(&myAIC, mob, &inputDescs[i].value,
                                             i);
        }

        ASSERT(weights.size() == numInputs * numOutputs);

        uint w = 0;
        for (i = 0; i < numOutputs; i++) {
            uint j = 0;
            outputs[i] = 0.0f;

            while (j < numInputs) {
                outputs[i] += weights[w] * inputs[j];
                w++;
                j++;
            }
        }

        FRPoint rForce;
        FRPoint_Zero(&rForce);

        for (i = 0; i < numOutputs; i++) {
            FRPoint curForce;
            ASSERT(outputDescs[i].value.valueType == NEURAL_VALUE_FORCE ||
                   outputDescs[i].value.valueType == NEURAL_VALUE_VOID);

            if (outputDescs[i].value.valueType == NEURAL_VALUE_FORCE &&
                outputDescs[i].value.forceDesc.forceType != NEURAL_FORCE_VOID &&
                outputs[i] != 0.0f) {
                bool haveForce;
                haveForce =
                    NeuralForce_GetForce(&myAIC, mob,
                                         &outputDescs[i].value.forceDesc,
                                         &curForce);
                if (haveForce) {
                    NeuralCombiner_ApplyOutput(outputDescs[i].cType,
                                               outputs[i],
                                               &curForce);
                    FRPoint_Add(&curForce, &rForce, &rForce);
                }
            }
        }

        NeuralForce_ApplyToMob(getAIContext(), mob, &rForce);

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }

    virtual void runTick() {
        BasicAIGovernor::runTick();
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
    }
};

class MatrixFleet {
public:
    MatrixFleet(FleetAI *ai)
    :sg(ai->bp.width, ai->bp.height, 0), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        gov.setSeed(RandomState_Uint64(&this->rs));
        sg.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);

        this->gov.putDefaults(mreg, ai->player.aiType);
        this->gov.loadRegistry(mreg);
    }

    ~MatrixFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    MappingSensorGrid sg;
    MatrixAIGovernor gov;
    MBRegistry *mreg;
};

static void *MatrixFleetCreate(FleetAI *ai);
static void MatrixFleetDestroy(void *aiHandle);
static void MatrixFleetRunAITick(void *aiHandle);
static void *MatrixFleetMobSpawned(void *aiHandle, Mob *m);
static void MatrixFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static void MatrixFleetMutate(FleetAIType aiType, MBRegistry *mreg);

void MatrixFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_MATRIX1) {
        ops->aiName = "MatrixFleet1";
    } else {
        NOT_IMPLEMENTED();
    }

    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &MatrixFleetCreate;
    ops->destroyFleet = &MatrixFleetDestroy;
    ops->runAITick = &MatrixFleetRunAITick;
    ops->mobSpawned = &MatrixFleetMobSpawned;
    ops->mobDestroyed = &MatrixFleetMobDestroyed;
    ops->mutateParams = &MatrixFleetMutate;
    //ops->dumpSanitizedParams = &MatrixFleetDumpSanitizedParams;
}

static void MatrixFleetMutate(FleetAIType aiType, MBRegistry *mreg)
{
    MutationFloatParams vf[] = {
        // key                     min     max       mag   jump   mutation
        { "evadeStrictDistance",  -1.0f,   500.0f,  0.05f, 0.10f, 0.10f},
        { "evadeRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.10f},
        { "attackRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.10f},
        { "guardRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.05f},
        { "gatherRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.10f},
        { "startingMaxRadius",    1000.0f, 2000.0f, 0.05f, 0.10f, 0.10f},
        { "startingMinRadius",    300.0f,  800.0f,  0.05f, 0.10f, 0.10f},

        { "creditReserve",       100.0f,  200.0f,   0.05f, 0.1f, 0.005f},
    };

    MutationBoolParams vb[] = {
        // key                          mutation
        { "evadeFighters",               0.02f },
        { "evadeUseStrictDistance",      0.02f },
        { "attackExtendedRange",         0.02f },
        { "rotateStartingAngle",         0.02f },
        { "gatherAbandonStale",          0.02f },
    };

    float rate = 0.10;
    MBRegistry_PutCopy(mreg, MATRIX_SCRAMBLE_KEY, "FALSE");

    if (Random_Flip(0.10)) {
        rate *= 10.0f;

        if (Random_Flip(0.01)) {
            rate = 1.0f;
            MBRegistry_PutCopy(mreg, MATRIX_SCRAMBLE_KEY, "TRUE");
        }

        if (rate >= 1.0f) {
            rate = 1.0f;
        }
    }

    for (uint i = 0; i < ARRAYSIZE(vf); i++) {
        vf[i].mutationRate = MIN(vf[i].mutationRate, rate);
    }
    for (uint i = 0; i < ARRAYSIZE(vb); i++) {
        vb[i].flipRate = MIN(vb[i].flipRate, rate);
        vb[i].flipRate = MAX(vb[i].flipRate, 0.5f);
    }

    SensorGrid_Mutate(mreg, rate, "");

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));

    MBString s;
    uint i;
    uint numInputs, numOutputs;

    s = "numInputs";
    numInputs = MBRegistry_GetUint(mreg, s.CStr());
    if (numInputs == 0) {
        numInputs = MATRIX_DEFAULT_NODES;
    }
    for (i = 0; i < numInputs; i++) {
        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "input[%d].", i);
        VERIFY(ret > 0);
        NeuralInput_Mutate(mreg, rate, NN_TYPE_FORCES, lcstr);
        free(lcstr);
    }

    s = "numOutputs";
    numOutputs = MBRegistry_GetUint(mreg, s.CStr());
    if (numOutputs == 0) {
        numOutputs = MATRIX_DEFAULT_NODES;
    }
    for (i = 0; i < numOutputs; i++) {
        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "output[%d].", i);
        VERIFY(ret > 0);
        NeuralOutput_Mutate(mreg, rate, NN_TYPE_FORCES, lcstr);
        free(lcstr);
    }

    MBVector<float> row;
    MBVector<float> weights;
    weights.resize(numInputs * numOutputs);
    row.resize(numInputs);

    MutationFloatParams mfp;
    Mutate_DefaultFloatParams(&mfp, MUTATION_TYPE_WEIGHT);
    mfp.mutationRate = (mfp.mutationRate + rate) / 2.0f;

    for (i = 0; i < numOutputs; i++) {
        MBString rowStr;
        uint j;

        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "weight[%d]", i);
        VERIFY(ret > 0);
        rowStr = MBRegistry_GetCStr(mreg, lcstr);
        TextDump_Convert(rowStr, row);
        free(lcstr);

        for (j = 0; j < numInputs; j++) {
            uint w = i * numInputs + j;

            if (j < row.size()) {
                weights[w] = row[j];
            } else {
                weights[w] = Mutate_FloatRaw(0.0f, TRUE, &mfp);
            }
        }
    }

    for (uint w = 0; w < weights.size(); w++) {
        weights[w] = Mutate_FloatRaw(weights[w], FALSE, &mfp);
    }

    row.resize(numInputs);
    for (i = 0; i < numOutputs; i++) {
        MBString rowStr;
        uint j;
        for (j = 0; j < numInputs; j++) {
            uint w = i * numInputs + j;
            row[j] = weights[w];
        }

        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "weight[%d]", i);
        VERIFY(ret > 0);
        TextDump_Convert(row, rowStr);
        MBRegistry_PutCopy(mreg, lcstr, rowStr.CStr());
        free(lcstr);
    }

    MBRegistry_Remove(mreg, MATRIX_SCRAMBLE_KEY);
}

static void *MatrixFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new MatrixFleet(ai);
}

static void MatrixFleetDestroy(void *handle)
{
    MatrixFleet *sf = (MatrixFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *MatrixFleetMobSpawned(void *aiHandle, Mob *m)
{
    MatrixFleet *sf = (MatrixFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return sf->gov.getShipHandle(m->mobid);
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void MatrixFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    MatrixFleet *sf = (MatrixFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void MatrixFleetRunAITick(void *aiHandle)
{
    MatrixFleet *sf = (MatrixFleet *)aiHandle;
    sf->gov.runTick();
}
