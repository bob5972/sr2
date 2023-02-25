/*
 * bineuralFleet.cpp -- part of SpaceRobots2
 * Copyright (C) 2022-2023 Michael Banack <github@banack.net>
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

#include "battle.h"
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

#define BINEURAL_MAX_NODE_DEGREE   8
#define BINEURAL_MAX_INPUTS       40
#define BINEURAL_MAX_OUTPUTS      40
#define BINEURAL_MAX_NODES       150

#define BINEURAL_MAX_LOCUS         8

#define BINEURAL_SCRAMBLE_KEY "bineuralFleet.scrambleMutation"

typedef struct BineuralConfigValue {
    const char *key;
    const char *value;
} BineuralConfigValue;

typedef struct BineuralLocus {
    NeuralLocusDesc desc;
    NeuralLocusPosition pos;
} BineuralLocus;

class BineuralAIGovernor : public BasicAIGovernor
{
public:
    // Members
    AIContext myAIC;
    NeuralNet myShipNet;
    NeuralNet myFleetNet;
    BineuralLocus myLoci[BINEURAL_MAX_LOCUS];

public:
    BineuralAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        myAIC.rs = &myRandomState;
        myAIC.sg = sg;
        myAIC.ai = myFleetAI;

        myShipNet.aic = myAIC;
        myFleetNet.aic = myAIC;
    }

    virtual ~BineuralAIGovernor() { }

    class BineuralShipAI : public BasicShipAI
    {
        public:
        BineuralShipAI(MobID mobid, BineuralAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        { }

        virtual ~BineuralShipAI() {}
    };

    //XXX Only saves some of the state.
    void dumpSanitizedParams(MBRegistry *mreg) {
        myShipNet.save(mreg, "shipNet.");
        myShipNet.dumpSanitizedParams(mreg, "shipNet.");

        myFleetNet.save(mreg, "fleetNet.");
        myFleetNet.dumpSanitizedParams(mreg, "fleetNet.");
    }

    virtual BineuralShipAI *newShip(MobID mobid) {
        return new BineuralShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        FleetConfig_PushDefaults(mreg, aiType);
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        myShipNet.load(mreg, "shipNet.", NN_TYPE_FORCES);
        myFleetNet.load(mreg, "fleetNet.", NN_TYPE_SCALARS);
        myFleetNet.minimizeScalars(myShipNet);

        uint numLoci = MBRegistry_GetUint(mreg, "numLoci");
        VERIFY(numLoci <= ARRAYSIZE(myLoci));
        myShipNet.loci.resize(ARRAYSIZE(myLoci));
        for (uint i = 0; i < ARRAYSIZE(myLoci); i++) {
            MBUtil_Zero(&myLoci[i], sizeof(myLoci[i]));
            MBUtil_Zero(&myShipNet.loci[i], sizeof(myShipNet.loci[i]));

            if (i < numLoci) {
                char *k = NULL;
                int ret;
                ret = asprintf(&k, "locus[%d].", i);
                VERIFY(ret > 0);
                NeuralLocus_Load(mreg, &myLoci[i].desc, k);
                free(k);
            } else {
                myLoci[i].desc.locusType = NEURAL_LOCUS_VOID;
            }


        }



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
        BineuralShipAI *ship = (BineuralShipAI *)mob->aiMobHandle;
        ASSERT(ship == (BineuralShipAI *)getShip(mob->mobid));
        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        FRPoint rForce;
        myShipNet.doForces(mob, &rForce);
        NeuralForce_ApplyToMob(getAIContext(), mob, &rForce);

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }

    virtual void runTick() {
        myFleetNet.doScalars();

        ASSERT(myShipNet.loci.size() == ARRAYSIZE(myLoci));
        for (uint i = 0; i < ARRAYSIZE(myLoci); i++) {
            NeuralLocus_RunTick(&myAIC, &myLoci[i].desc, &myLoci[i].pos);
            myShipNet.loci[i] = myLoci[i].pos;
        }

        myShipNet.pullScalars(myFleetNet);
        BasicAIGovernor::runTick();
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
    }
};

class BineuralFleet {
public:
    BineuralFleet(FleetAI *ai)
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

    ~BineuralFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    MappingSensorGrid sg;
    BineuralAIGovernor gov;
    MBRegistry *mreg;
};

static void *BineuralFleetCreate(FleetAI *ai);
static void BineuralFleetDestroy(void *aiHandle);
static void BineuralFleetRunAITick(void *aiHandle);
static void *BineuralFleetMobSpawned(void *aiHandle, Mob *m);
static void BineuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static void BineuralFleetMutate(FleetAIType aiType, MBRegistry *mreg);
static void BineuralFleetDumpSanitizedParams(void *aiHandle, MBRegistry *mreg);

void BineuralFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_BINEURAL1) {
        ops->aiName = "BineuralFleet1";
    } else if (aiType == FLEET_AI_BINEURAL2) {
        ops->aiName = "BineuralFleet2";
    } else if (aiType == FLEET_AI_BINEURAL3) {
        ops->aiName = "BineuralFleet3";
    } else if (aiType == FLEET_AI_BINEURAL4) {
        ops->aiName = "BineuralFleet4";
    } else {
        NOT_IMPLEMENTED();
    }

    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BineuralFleetCreate;
    ops->destroyFleet = &BineuralFleetDestroy;
    ops->runAITick = &BineuralFleetRunAITick;
    ops->mobSpawned = &BineuralFleetMobSpawned;
    ops->mobDestroyed = &BineuralFleetMobDestroyed;
    ops->mutateParams = &BineuralFleetMutate;
    ops->dumpSanitizedParams = &BineuralFleetDumpSanitizedParams;
}

static void BineuralFleetDumpSanitizedParams(void *aiHandle, MBRegistry *mreg)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;
    MBRegistry_PutAll(mreg, sf->mreg, "");
    sf->gov.dumpSanitizedParams(mreg);
}

static void BineuralFleetMutate(FleetAIType aiType, MBRegistry *mreg)
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

    float rate = 0.01;
    MBRegistry_PutCopy(mreg, BINEURAL_SCRAMBLE_KEY, "FALSE");

    if (Random_Flip(0.10)) {
        rate *= 10.0f;

        if (Random_Flip(0.01)) {
            rate = 1.0f;
            MBRegistry_PutCopy(mreg, BINEURAL_SCRAMBLE_KEY, "TRUE");
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

    NeuralNet_Mutate(mreg, "shipNet.", rate,
                     NN_TYPE_FORCES,
                     BINEURAL_MAX_INPUTS, BINEURAL_MAX_OUTPUTS,
                     BINEURAL_MAX_NODES, BINEURAL_MAX_NODE_DEGREE);
    NeuralNet_Mutate(mreg, "fleetNet.", rate,
                     NN_TYPE_SCALARS,
                     BINEURAL_MAX_INPUTS, BINEURAL_MAX_OUTPUTS,
                     BINEURAL_MAX_NODES, BINEURAL_MAX_NODE_DEGREE);

    ASSERT(BINEURAL_MAX_LOCUS == 8);
    MBRegistry_PutCopy(mreg, "numLoci", "8");
    for (uint i = 0; i < BINEURAL_MAX_LOCUS; i++) {
        char *k = NULL;
        int ret;
        ret = asprintf(&k, "locus[%d].", i);
        VERIFY(ret > 0);
        NeuralLocus_Mutate(mreg, rate, k);
        free(k);
    }

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));

    MBRegistry_Remove(mreg, BINEURAL_SCRAMBLE_KEY);
}

static void *BineuralFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new BineuralFleet(ai);
}

static void BineuralFleetDestroy(void *handle)
{
    BineuralFleet *sf = (BineuralFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *BineuralFleetMobSpawned(void *aiHandle, Mob *m)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return sf->gov.getShipHandle(m->mobid);
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void BineuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void BineuralFleetRunAITick(void *aiHandle)
{
    BineuralFleet *sf = (BineuralFleet *)aiHandle;
    sf->gov.runTick();
}
