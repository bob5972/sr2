/*
 * neuralFleet.cpp -- part of SpaceRobots2
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
#include "fleet.h"
#include "Random.h"
#include "battle.h"
}

#include "mutate.h"
#include "MBUtil.h"

#include "fleetConfig.h"
#include "sensorGrid.hpp"
#include "basicShipAI.hpp"
#include "MBMap.hpp"
#include "MBString.hpp"
#include "ml.hpp"
#include "floatNet.hpp"
#include "textDump.hpp"

#include "neuralNet.hpp"

#define NEURAL_MAX_NODE_DEGREE  8
#define NEURAL_MAX_INPUTS      25
#define NEURAL_MAX_OUTPUTS     25
#define NEURAL_MAX_NODES       100

#define NEURAL_SCRAMBLE_KEY "neuralFleet.scrambleMutation"

typedef struct NeuralConfigValue {
    const char *key;
    const char *value;
} NeuralConfigValue;

class NeuralAIGovernor : public BasicAIGovernor
{
public:
    // Members
    AIContext myAIC;
    NeuralNet myShipNet;

public:
    NeuralAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        myAIC.rs = &myRandomState;
        myAIC.sg = sg;
        myAIC.ai = myFleetAI;

        myShipNet.aic = myAIC;
    }

    virtual ~NeuralAIGovernor() { }

    class NeuralShipAI : public BasicShipAI
    {
        public:
        NeuralShipAI(MobID mobid, NeuralAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        { }

        virtual ~NeuralShipAI() {}
    };

    void dumpSanitizedParams(MBRegistry *mreg) {
        myShipNet.save(mreg, "shipNet.");
        myShipNet.dumpSanitizedParams(mreg, "shipNet.");
    }

    virtual NeuralShipAI *newShip(MobID mobid) {
        return new NeuralShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        FleetConfig_PushDefaults(mreg, aiType);
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        myShipNet.load(mreg, "shipNet.", NN_TYPE_FORCES);
        this->BasicAIGovernor::loadRegistry(mreg);
    }

    AIContext *getAIContext(void) {
        ASSERT(myAIC.rs != NULL);
        ASSERT(myAIC.sg != NULL);
        ASSERT(myAIC.ai != NULL);
        return &myAIC;
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        //RandomState *rs = &myRandomState;

        NeuralShipAI *ship = (NeuralShipAI *)mob->aiMobHandle;
        ASSERT(ship == (NeuralShipAI *)getShip(mob->mobid));
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
        BasicAIGovernor::runTick();
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
    }
};

class NeuralFleet {
public:
    NeuralFleet(FleetAI *ai)
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

    ~NeuralFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    MappingSensorGrid sg;
    NeuralAIGovernor gov;
    MBRegistry *mreg;
};

static void *NeuralFleetCreate(FleetAI *ai);
static void NeuralFleetDestroy(void *aiHandle);
static void NeuralFleetRunAITick(void *aiHandle);
static void *NeuralFleetMobSpawned(void *aiHandle, Mob *m);
static void NeuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static void NeuralFleetMutate(FleetAIType aiType, MBRegistry *mreg);
static void NeuralFleetDumpSanitizedParams(void *aiHandle, MBRegistry *mreg);

void NeuralFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_NEURAL1) {
        ops->aiName = "NeuralFleet1";
    } else if (aiType == FLEET_AI_NEURAL2) {
        ops->aiName = "NeuralFleet2";
    } else if (aiType == FLEET_AI_NEURAL3) {
        ops->aiName = "NeuralFleet3";
    } else if (aiType == FLEET_AI_NEURAL4) {
        ops->aiName = "NeuralFleet4";
    } else if (aiType == FLEET_AI_NEURAL5) {
        ops->aiName = "NeuralFleet5";
    } else if (aiType == FLEET_AI_NEURAL6) {
        ops->aiName = "NeuralFleet6";
    } else if (aiType == FLEET_AI_NEURAL7) {
        ops->aiName = "NeuralFleet7";
    } else if (aiType == FLEET_AI_NEURAL8) {
        ops->aiName = "NeuralFleet8";
    } else if (aiType == FLEET_AI_NEURAL9) {
        ops->aiName = "NeuralFleet9";
    } else if (aiType == FLEET_AI_NEURAL10) {
        ops->aiName = "NeuralFleet10";
    } else if (aiType == FLEET_AI_NEURAL11) {
        ops->aiName = "NeuralFleet11";
    } else if (aiType == FLEET_AI_NEURAL12) {
        ops->aiName = "NeuralFleet12";
    } else {
        NOT_IMPLEMENTED();
    }

    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &NeuralFleetCreate;
    ops->destroyFleet = &NeuralFleetDestroy;
    ops->runAITick = &NeuralFleetRunAITick;
    ops->mobSpawned = &NeuralFleetMobSpawned;
    ops->mobDestroyed = &NeuralFleetMobDestroyed;
    ops->mutateParams = &NeuralFleetMutate;
    ops->dumpSanitizedParams = &NeuralFleetDumpSanitizedParams;
}

static void NeuralFleetDumpSanitizedParams(void *aiHandle, MBRegistry *mreg)
{
    NeuralFleet *sf = (NeuralFleet *)aiHandle;
    MBRegistry_PutAll(mreg, sf->mreg, "");
    sf->gov.dumpSanitizedParams(mreg);
}

static void NeuralFleetMutate(FleetAIType aiType, MBRegistry *mreg)
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

        { "sensorGrid.staleCoreTime",
                                   0.0f,   50.0f,   0.05f, 0.2f, 0.005f},
        { "sensorGrid.staleFighterTime",
                                   0.0f,   20.0f,   0.05f, 0.2f, 0.005f},
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

    float rate = 0.08;
    MBRegistry_PutCopy(mreg, NEURAL_SCRAMBLE_KEY, "FALSE");
    if (Random_Flip(0.01)) {
        MBRegistry_PutCopy(mreg, NEURAL_SCRAMBLE_KEY, "TRUE");

        for (uint i = 0; i < ARRAYSIZE(vf); i++) {
            vf[i].mutationRate = 1.0f;
            vf[i].jumpRate = 1.0f;
        }
        for (uint i = 0; i < ARRAYSIZE(vb); i++) {
            vb[i].flipRate = 0.5f;
        }
        rate = 1.0f;
    }

    NeuralNet_Mutate(mreg, "shipNet.", rate,
                     NN_TYPE_FORCES,
                     NEURAL_MAX_INPUTS, NEURAL_MAX_OUTPUTS,
                     NEURAL_MAX_NODES, NEURAL_MAX_NODE_DEGREE);

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));

    MBRegistry_Remove(mreg, NEURAL_SCRAMBLE_KEY);
}

static void *NeuralFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new NeuralFleet(ai);
}

static void NeuralFleetDestroy(void *handle)
{
    NeuralFleet *sf = (NeuralFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *NeuralFleetMobSpawned(void *aiHandle, Mob *m)
{
    NeuralFleet *sf = (NeuralFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return sf->gov.getShipHandle(m->mobid);
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void NeuralFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    NeuralFleet *sf = (NeuralFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void NeuralFleetRunAITick(void *aiHandle)
{
    NeuralFleet *sf = (NeuralFleet *)aiHandle;
    sf->gov.runTick();
}
