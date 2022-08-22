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

#include "sensorGrid.hpp"
#include "basicShipAI.hpp"
#include "MBMap.hpp"
#include "MBString.hpp"
#include "ml.hpp"
#include "floatNet.hpp"
#include "textDump.hpp"

#define NEURAL_SCRAMBLE_KEY "neuralFleet.scrambleMutation"

typedef enum NeuralForceType {
    NEURAL_FORCE_ZERO,
    NEURAL_FORCE_HEADING,
    NEURAL_FORCE_ALIGN,
    NEURAL_FORCE_COHERE,
    NEURAL_FORCE_SEPARATE,
    NEURAL_FORCE_NEAREST_FRIEND,
    NEURAL_FORCE_EDGES,
    NEURAL_FORCE_CORNERS,
    NEURAL_FORCE_CENTER,
    NEURAL_FORCE_BASE,
    NEURAL_FORCE_BASE_DEFENSE,
    NEURAL_FORCE_ENEMY,
    NEURAL_FORCE_ENEMY_BASE,
    NEURAL_FORCE_ENEMY_BASE_GUESS,
    NEURAL_FORCE_CORES,

    NEURAL_FORCE_MAX,
} NeuralForceType;

static TextMapEntry tmForces[] = {
    { TMENTRY(NEURAL_FORCE_ZERO),             },
    { TMENTRY(NEURAL_FORCE_HEADING),          },
    { TMENTRY(NEURAL_FORCE_ALIGN),            },
    { TMENTRY(NEURAL_FORCE_COHERE),           },
    { TMENTRY(NEURAL_FORCE_SEPARATE),         },
    { TMENTRY(NEURAL_FORCE_NEAREST_FRIEND),   },
    { TMENTRY(NEURAL_FORCE_EDGES),            },
    { TMENTRY(NEURAL_FORCE_CORNERS),          },
    { TMENTRY(NEURAL_FORCE_CENTER),           },
    { TMENTRY(NEURAL_FORCE_BASE),             },
    { TMENTRY(NEURAL_FORCE_BASE_DEFENSE),     },
    { TMENTRY(NEURAL_FORCE_ENEMY),            },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE),       },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS), },
    { TMENTRY(NEURAL_FORCE_CORES),            },
};

typedef struct NeuralForceDesc {
    NeuralForceType forceType;
    bool useTangent;
    float radius;
} NeuralForceDesc;

typedef struct NeuralCrowdDesc {
    float radius;
} NeuralCrowdDesc;

typedef enum NeuralValueType {
    NEURAL_VALUE_ZERO,
    NEURAL_VALUE_FORCE,
    NEURAL_VALUE_RANGE,
    NEURAL_VALUE_CROWD,
    NEURAL_VALUE_TICK,
    NEURAL_VALUE_MOBID,
    NEURAL_VALUE_MAX,
} NeuralValueType;

static TextMapEntry tmValues[] = {
    { TMENTRY(NEURAL_VALUE_ZERO),  },
    { TMENTRY(NEURAL_VALUE_FORCE), },
    { TMENTRY(NEURAL_VALUE_RANGE), },
    { TMENTRY(NEURAL_VALUE_CROWD), },
    { TMENTRY(NEURAL_VALUE_TICK),  },
    { TMENTRY(NEURAL_VALUE_MOBID), },
};

typedef struct NeuralValueDesc {
    NeuralValueType valueType;
    union {
        NeuralForceDesc forceDesc;
        NeuralForceDesc rangeDesc;
        NeuralCrowdDesc crowdDesc;
    };
} NeuralValueDesc;

typedef struct NeuralConfigValue {
    const char *key;
    const char *value;
} NeuralConfigValue;

static void LoadNeuralValueDesc(MBRegistry *mreg,
                                NeuralValueDesc *desc, const char *prefix);
static void LoadNeuralForceDesc(MBRegistry *mreg,
                                NeuralForceDesc *desc, const char *prefix);
static void LoadNeuralCrowdDesc(MBRegistry *mreg,
                                NeuralCrowdDesc *desc, const char *prefix);
static void MutateNeuralValueDesc(MBRegistry *mreg, NeuralValueDesc *desc,
                                  bool isOutput, float rate,
                                  const char *prefix);

class NeuralAIGovernor : public BasicAIGovernor
{
private:
    // Members
    FloatNet myNeuralNet;
    MBVector<NeuralValueDesc> myInputDescs;
    MBVector<NeuralValueDesc> myOutputDescs;
    MBVector<float> myInputs;
    MBVector<float> myOutputs;
    uint myNumNodes;

public:
    NeuralAIGovernor(FleetAI *ai, MappingSensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    { }

    virtual ~NeuralAIGovernor() { }

    class NeuralShipAI : public BasicShipAI
    {
        public:
        NeuralShipAI(MobID mobid, NeuralAIGovernor *gov)
        :BasicShipAI(mobid, gov)
        { }

        virtual ~NeuralShipAI() {}
    };

    virtual NeuralShipAI *newShip(MobID mobid) {
        return new NeuralShipAI(mobid, this);
    }

    void putDefaults(MBRegistry *mreg, FleetAIType aiType) {
        NeuralConfigValue defaults[] = {
            { "attackExtendedRange",         "TRUE"      },
            { "attackRange",                 "117.644791"},
            { "baseDefenseRadius",           "143.515045"},
            { "baseSpawnJitter",             "1"         },
            { "creditReserve",               "200"       },

            { "evadeFighters",               "FALSE"     },
            { "evadeRange",                  "289.852631"},
            { "evadeStrictDistance",         "105.764320"},
            { "evadeUseStrictDistance",      "TRUE"      },

            { "gatherAbandonStale",          "FALSE"     },
            { "gatherRange",                 "50"        },
            { "guardRange",                  "0"         },

            { "nearBaseRandomIdle.forceOn",  "TRUE"      },
            { "randomIdle.forceOn",          "TRUE"      },
            { "randomizeStoppedVelocity.forceOn", "TRUE" },
            { "rotateStartingAngle",         "TRUE",     },
            { "simpleAttack.forceOn",        "TRUE"      },

            { "nearBaseRadius",              "100.0"     },
            { "baseDefenseRadius",           "250.0"     },

            { "sensorGrid.staleCoreTime",    "28.385160" },
            { "sensorGrid.staleFighterTime", "16.703636" },

            { "startingMaxRadius",           "300"       },
            { "startingMinRadius",           "250"       },
        };

        NeuralConfigValue configs1[] = {
            { "curHeadingWeight.value.value", "1"        },//XXX
            { "curHeadingWeight.valueType",   "constant" },//XXX
        };

        struct {
            NeuralConfigValue *values;
            uint numValues;
        } configs[] = {
            { defaults,  ARRAYSIZE(defaults), },
            { configs1,  ARRAYSIZE(configs1), },
        };

        int neuralIndex = aiType - FLEET_AI_NEURAL1 + 1;
        VERIFY(aiType >= FLEET_AI_NEURAL1);
        VERIFY(aiType <= FLEET_AI_NEURAL1);
        VERIFY(neuralIndex >= 1 && neuralIndex < ARRAYSIZE(configs));

        for (int i = neuralIndex; i >= 0; i--) {
            NeuralConfigValue *curConfig = configs[i].values;
            uint size = configs[i].numValues;
            for (uint k = 0; k < size; k++) {
                if (curConfig[k].value != NULL &&
                    !MBRegistry_ContainsKey(mreg, curConfig[k].key)) {
                    MBRegistry_PutConst(mreg, curConfig[k].key,
                                        curConfig[k].value);
                }
            }
        }
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        if (MBRegistry_ContainsKey(mreg, "floatNet.numInputs")) {
            myNeuralNet.load(mreg, "floatNet.");
        } else {
            myNeuralNet.initialize(1, 1, 1);
            myNeuralNet.loadZeroNet();
        }

        uint numInputs = myNeuralNet.getNumInputs();
        uint numOutputs = myNeuralNet.getNumOutputs();
        myNumNodes = myNeuralNet.getNumNodes();

        myInputs.resize(numInputs);
        myOutputs.resize(numOutputs);

        myInputDescs.resize(numInputs);
        myOutputDescs.resize(numOutputs);

        for (uint i = 0; i < myInputDescs.size(); i++) {
            char *str = NULL;
            int ret = asprintf(&str, "input[%d].", i);
            VERIFY(ret > 0);
            LoadNeuralValueDesc(mreg, &myInputDescs[i], str);
            free(str);
        }

        for (uint i = 0; i < myOutputDescs.size(); i++) {
            char *str = NULL;
            int ret = asprintf(&str, "output[%d].", i);
            VERIFY(ret > 0);
            LoadNeuralValueDesc(mreg, &myOutputDescs[i], str);
            free(str);

            if (myOutputDescs[i].valueType != NEURAL_VALUE_FORCE) {
                myOutputDescs[i].valueType = NEURAL_VALUE_FORCE;
                myOutputDescs[i].forceDesc.forceType = NEURAL_FORCE_ZERO;
                myOutputDescs[i].forceDesc.useTangent = FALSE;
                myOutputDescs[i].forceDesc.radius = 0.0f;
            }
        }

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    float getNeuralValue(Mob *mob, NeuralValueDesc *desc) {
        FRPoint force;

        FRPoint_Zero(&force);

        ASSERT(desc != NULL);
        switch (desc->valueType) {
            case NEURAL_VALUE_ZERO:
                return 0.0f;
            case NEURAL_VALUE_FORCE:
                getNeuralForce(mob, &desc->forceDesc, &force);
                return force.radius;
            case NEURAL_VALUE_RANGE:
                return getRangeValue(mob, &desc->rangeDesc);
            case NEURAL_VALUE_CROWD:
                return getCrowdValue(mob, &desc->crowdDesc);
            case NEURAL_VALUE_TICK:
                return myFleetAI->tick;
            case NEURAL_VALUE_MOBID:
                return (float)mob->mobid;
            default:
                NOT_IMPLEMENTED();
        }
    }

    /*
     * getNeuralForce --
     *    Calculate the specified force.
     *    returns TRUE iff the force is valid.
     */
    bool getNeuralForce(Mob *mob,
                        NeuralForceDesc *desc,
                        FRPoint *rForce) {
        FPoint focusPoint;

        if (getNeuralFocus(mob, desc, &focusPoint)) {
            FPoint_ToFRPoint(&focusPoint, &mob->pos, rForce);
            rForce->radius = 1.0f;

            if (desc->useTangent) {
                rForce->theta += (float)M_PI/2;
            }
            return TRUE;
        } else {
            FRPoint_Zero(rForce);
            return FALSE;
        }
    }

    bool focusMobPosHelper(Mob *mob, FPoint *focusPoint)
    {
        ASSERT(focusPoint != NULL);
        if (mob != NULL) {
            *focusPoint = mob->pos;
            return TRUE;
        }
        return FALSE;
    }

    /*
     * getNeuralFocus --
     *     Get the focus point associated with the specified force.
     *     Returns TRUE if the force is valid.
     *     Returns FALSE if the force is invalid.
     */
    bool getNeuralFocus(Mob *mob,
                        NeuralForceDesc *desc,
                        FPoint *focusPoint) {
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        RandomState *rs = &myRandomState;

        switch(desc->forceType) {
            case NEURAL_FORCE_ZERO:
                return FALSE;

            case NEURAL_FORCE_HEADING: {
                FRPoint rPos;
                FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

                if (rPos.radius < MICRON) {
                    rPos.radius = 1.0f;
                    rPos.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
                }
                FRPoint_ToFPoint(&rPos, &mob->pos, focusPoint);
                return TRUE;
            }
            case NEURAL_FORCE_ALIGN: {
                FPoint avgVel;
                sg->friendAvgVelocity(&avgVel, &mob->pos, desc->radius,
                                      MOB_FLAG_FIGHTER);
                avgVel.x += mob->pos.x;
                avgVel.y += mob->pos.y;
                *focusPoint = avgVel;
                return TRUE;
            }
            case NEURAL_FORCE_COHERE: {
                FPoint avgPos;
                sg->friendAvgPos(&avgPos, &mob->pos, desc->radius,
                                 MOB_FLAG_FIGHTER);
                *focusPoint = avgPos;
                return TRUE;
            }
            case NEURAL_FORCE_SEPARATE:
                return getSeparateFocus(mob, desc, focusPoint);

            case NEURAL_FORCE_NEAREST_FRIEND: {
                Mob *m = sg->findClosestFriend(mob, MOB_FLAG_FIGHTER);
                return focusMobPosHelper(m, focusPoint);
            }

            case NEURAL_FORCE_EDGES: {
                getEdgeFocus(mob, desc, focusPoint);
                return TRUE;
            }
            case NEURAL_FORCE_CORNERS: {
                flockCorners(mob, desc, focusPoint);
                return TRUE;
            }
            case NEURAL_FORCE_CENTER: {
                focusPoint->x = myFleetAI->bp.width / 2;
                focusPoint->y = myFleetAI->bp.height / 2;
                return TRUE;
            }
            case NEURAL_FORCE_BASE:
                return focusMobPosHelper(sg->friendBase(), focusPoint);

            case NEURAL_FORCE_BASE_DEFENSE: {
                Mob *base = sg->friendBase();
                if (base != NULL) {
                    Mob *enemy = sg->findClosestTarget(&base->pos, MOB_FLAG_SHIP);
                    if (enemy != NULL) {
                        *focusPoint = enemy->pos;
                        return TRUE;
                    }
                }
                return FALSE;
            }
            case NEURAL_FORCE_ENEMY: {
                Mob *m = sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);
                return focusMobPosHelper(m, focusPoint);
            }

            case NEURAL_FORCE_ENEMY_BASE:
                return focusMobPosHelper(sg->enemyBase(), focusPoint);

            case NEURAL_FORCE_ENEMY_BASE_GUESS: {
                if (!sg->hasEnemyBase() && sg->hasEnemyBaseGuess()) {
                    *focusPoint = sg->getEnemyBaseGuess();
                    return TRUE;
                }
                return FALSE;
            }

            case NEURAL_FORCE_CORES: {
                Mob *m = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);
                return focusMobPosHelper(m, focusPoint);
            }

            default:
                PANIC("%s: Unhandled forceType: %d\n", __FUNCTION__,
                      desc->forceType);
        }

        NOT_REACHED();
    }

    bool getSeparateFocus(Mob *self, NeuralForceDesc *desc,
                          FPoint *focusPoint) {
        FRPoint force;
        int x = 0;
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);

        ASSERT(self->type == MOB_TYPE_FIGHTER);

        FRPoint_Zero(&force);

        while (mit.hasNext()) {
            Mob *m = mit.next();
            ASSERT(m != NULL);

            if (m->mobid != self->mobid) {
                repulseFocus(&self->pos, &m->pos, &force);
                x++;
            }
        }

        FRPoint_ToFPoint(&force, &self->pos, focusPoint);
        return x > 0;
    }

    void repulseFocus(const FPoint *selfPos, const FPoint *pos, FRPoint *force) {
        FRPoint f;
        RandomState *rs = &myRandomState;

        FPoint_ToFRPoint(pos, selfPos, &f);

        /*
         * Avoid 1/0 => NAN, and then randomize the direction when
         * the point is more or less directly on top of us.
         */
        if (f.radius < MICRON) {
            f.radius = MICRON;
            f.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
        }

        f.radius = 1.0f / (f.radius * f.radius);
        FRPoint_Add(force, &f, force);
    }

    void getEdgeFocus(Mob *self, NeuralForceDesc *desc, FPoint *focusPoint) {
        FleetAI *ai = myFleetAI;
        FPoint edgePoint;
        FRPoint force;

        FRPoint_Zero(&force);

        /*
         * Left Edge
         */
        edgePoint = self->pos;
        edgePoint.x = 0.0f;
        repulseFocus(&self->pos, &edgePoint, &force);

        /*
         * Right Edge
         */
        edgePoint = self->pos;
        edgePoint.x = ai->bp.width;
        repulseFocus(&self->pos, &edgePoint, &force);

        /*
         * Top Edge
         */
        edgePoint = self->pos;
        edgePoint.y = 0.0f;
        repulseFocus(&self->pos, &edgePoint, &force);

        /*
         * Bottom edge
         */
        edgePoint = self->pos;
        edgePoint.y = ai->bp.height;
        repulseFocus(&self->pos, &edgePoint, &force);

        FRPoint_ToFPoint(&force, &self->pos, focusPoint);
    }

    void flockCorners(Mob *self, NeuralForceDesc *desc, FPoint *focusPoint) {
        FleetAI *ai = myFleetAI;
        FPoint cornerPoint;
        FRPoint force;

        FRPoint_Zero(&force);

        cornerPoint.x = 0.0f;
        cornerPoint.y = 0.0f;
        repulseFocus(&self->pos, &cornerPoint, &force);

        cornerPoint.x = ai->bp.width;
        cornerPoint.y = 0.0f;
        repulseFocus(&self->pos, &cornerPoint, &force);

        cornerPoint.x = 0.0f;
        cornerPoint.y = ai->bp.height;
        repulseFocus(&self->pos, &cornerPoint, &force);

        cornerPoint.x = ai->bp.width;
        cornerPoint.y = ai->bp.height;
        repulseFocus(&self->pos, &cornerPoint, &force);

        FRPoint_ToFPoint(&force, &self->pos, focusPoint);
    }

    float getCrowdValue(Mob *mob, NeuralCrowdDesc *desc) {
        //XXX cache?
        MappingSensorGrid *sg = (MappingSensorGrid *)mySensorGrid;
        return sg->numFriendsInRange(MOB_FLAG_FIGHTER, &mob->pos, desc->radius);
    }

    float getRangeValue(Mob *mob, NeuralForceDesc *desc) {
        FPoint focusPoint;
        getNeuralFocus(mob, desc, &focusPoint);
        return FPoint_Distance(&mob->pos, &focusPoint);
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {

        BasicAIGovernor::doAttack(mob, enemyTarget);
    }

    void doForces(Mob *mob, FRPoint *outputForce) {
        uint x;
        float maxV = (1.0f / MICRON);

        ASSERT(myInputs.size() == myInputDescs.size());

        for (uint i = 0; i < myInputDescs.size(); i++) {
            myInputs[i] = getNeuralValue(mob, &myInputDescs[i]);
        }

        ASSERT(myOutputs.size() == myOutputDescs.size());
        myNeuralNet.compute(myInputs, myOutputs);

        for (uint i = 0; i < myOutputs.size(); i++) {
            if (isnan(myOutputs[i])) {
                myOutputs[i] = 0.0f;
            } else if (myOutputs[i] > maxV) {
                myOutputs[i] = maxV;
            } else if (myOutputs[i] < -maxV) {
                myOutputs[i] = -maxV;
            }
        }

        x = 0;
        FRPoint_Zero(outputForce);
        for (uint i = 0; i < myOutputDescs.size(); i++) {
            FRPoint force;
            ASSERT(myOutputDescs[i].valueType == NEURAL_VALUE_FORCE);
            if (getNeuralForce(mob, &myOutputDescs[i].forceDesc,
                               &force)) {
                force.radius = myOutputs[x];
                FRPoint_Add(outputForce, outputForce, &force);
            }
            x++;
        }
        //XXX non-force outputs?
        ASSERT(x <= myOutputs.size());
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        //RandomState *rs = &myRandomState;
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);

        NeuralShipAI *ship = (NeuralShipAI *)mob->aiMobHandle;
        ASSERT(ship == (NeuralShipAI *)getShip(mob->mobid));
        ASSERT(ship != NULL);

        ASSERT(ship != NULL);

        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        FRPoint rForce;
        doForces(mob, &rForce);
        if (rForce.radius < MICRON) {
            /*
             * Continue on the current heading if we didn't get a strong-enough
             * force.
             */
            NeuralForceDesc desc;
            desc.forceType = NEURAL_FORCE_HEADING;
            desc.useTangent = FALSE;
            desc.radius = speed;
            getNeuralForce(mob, &desc, &rForce);
        }

        rForce.radius = speed;
        FRPoint_ToFPoint(&rForce, &mob->pos, &mob->cmd.target);

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

void NeuralFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_NEURAL1) {
        ops->aiName = "NeuralFleet1";
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
}

static void NeuralFleetMutate(FleetAIType aiType, MBRegistry *mreg)
{
    MutationFloatParams vf[] = {
        // key                     min     max       mag   jump   mutation
        { "evadeStrictDistance",  -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "evadeRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "attackRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "guardRange",           -1.0f,   500.0f,  0.05f, 0.10f, 0.10f},
        { "gatherRange",          -1.0f,   500.0f,  0.05f, 0.10f, 0.20f},
        { "startingMaxRadius",    1000.0f, 2000.0f, 0.05f, 0.10f, 0.20f},
        { "startingMinRadius",    300.0f,  800.0f,  0.05f, 0.10f, 0.20f},

        { "sensorGrid.staleCoreTime",
                                   0.0f,   50.0f,   0.05f, 0.2f, 0.005f},
        { "sensorGrid.staleFighterTime",
                                   0.0f,   20.0f,   0.05f, 0.2f, 0.005f},
        { "creditReserve",       100.0f,  200.0f,   0.05f, 0.1f, 0.005f},
    };

    MutationBoolParams vb[] = {
        // key                          mutation
        { "evadeFighters",               0.05f },
        { "evadeUseStrictDistance",      0.05f },
        { "attackExtendedRange",         0.05f },
        { "rotateStartingAngle",         0.05f },
        { "gatherAbandonStale",          0.05f },
    };

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
    }

    FloatNet fn;
    if (MBRegistry_ContainsKey(mreg, "floatNet.numInputs")) {
        fn.load(mreg, "floatNet.");
    } else {
        fn.initialize(20, 20, 20);
        fn.loadZeroNet();
    }

    //XXX resize better?

    float rate = 0.75f;
    fn.mutate(rate);
    fn.save(mreg, "floatNet.");

    for (uint i = 0; i < fn.getNumInputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "input[%d].", i);
        VERIFY(ret > 0);
        LoadNeuralValueDesc(mreg, &desc, str);
        MutateNeuralValueDesc(mreg, &desc, FALSE, rate, str);
        free(str);
    }

    for (uint i = 0; i < fn.getNumOutputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "output[%d].", i);
        VERIFY(ret > 0);
        LoadNeuralValueDesc(mreg, &desc, str);
        MutateNeuralValueDesc(mreg, &desc, TRUE, rate, str);
        free(str);
    }

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));

    MBRegistry_Remove(mreg, NEURAL_SCRAMBLE_KEY);
}

static void MutateNeuralValueDesc(MBRegistry *mreg, NeuralValueDesc *desc,
                                  bool isOutput, float rate,
                                  const char *prefix)
{
    MBString s;

    s = prefix;
    s += "valueType";

    if (isOutput) {
        desc->valueType = NEURAL_VALUE_FORCE;
    } else if (Random_Flip(rate)) {
        uint i = Random_Int(0, ARRAYSIZE(tmValues) - 1);
        desc->valueType = (NeuralValueType) tmValues[i].value;
    }
    const char *v = TextMap_ToString(desc->valueType, tmValues, ARRAYSIZE(tmValues));
    MBRegistry_PutCopy(mreg, s.CStr(), v);

    if (desc->valueType == NEURAL_VALUE_FORCE ||
        desc->valueType == NEURAL_VALUE_RANGE ||
        desc->valueType == NEURAL_VALUE_CROWD) {
        MutationFloatParams vf;

        Mutate_DefaultFloatParams(&vf, MUTATION_TYPE_RADIUS);
        s = prefix;
        s += "radius";
        vf.key = s.CStr();
        Mutate_Float(mreg, &vf, 1);
    }

    if (desc->valueType == NEURAL_VALUE_FORCE) {
        s = prefix;
        s += "forceType";
        if (Random_Flip(rate)) {
            uint i = Random_Int(0, ARRAYSIZE(tmForces) - 1);
            const char *v = tmForces[i].str;
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            desc->forceDesc.forceType = (NeuralForceType) tmForces[i].value;
        }

        MutationBoolParams bf;
        s = prefix;
        s += "useTangent";
        bf.key = s.CStr();
        bf.flipRate = rate;
        Mutate_Bool(mreg, &bf, 1);
    }
}


static void LoadNeuralValueDesc(MBRegistry *mreg,
                                NeuralValueDesc *desc, const char *prefix)
{
    MBString s;
    const char *cstr;

    s = prefix;
    s += "valueType";
    cstr = MBRegistry_GetCStr(mreg, s.CStr());
    desc->valueType = NEURAL_VALUE_MAX;

    if (cstr == NULL) {
        ASSERT(tmValues[0].value == NEURAL_VALUE_ZERO);
        cstr = tmValues[0].str;
    }

    desc->valueType = (NeuralValueType)
        TextMap_FromString(cstr, tmValues, ARRAYSIZE(tmValues));
    VERIFY(desc->valueType < NEURAL_VALUE_MAX);

    s = prefix;
    switch (desc->valueType) {
        case NEURAL_VALUE_FORCE:
        case NEURAL_VALUE_RANGE:
            ASSERT(&desc->forceDesc == &desc->rangeDesc);
            ASSERT(sizeof(desc->forceDesc) == sizeof(desc->rangeDesc));
            LoadNeuralForceDesc(mreg, &desc->forceDesc, s.CStr());
            break;

        case NEURAL_VALUE_CROWD:
            LoadNeuralCrowdDesc(mreg, &desc->crowdDesc, s.CStr());
            break;

        case NEURAL_VALUE_ZERO:
        case NEURAL_VALUE_TICK:
        case NEURAL_VALUE_MOBID:
            break;

        default:
            NOT_IMPLEMENTED();
    }
}

static void LoadNeuralForceDesc(MBRegistry *mreg,
                                NeuralForceDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "forceType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        ASSERT(tmForces[0].value == NEURAL_FORCE_ZERO);
        v = tmForces[0].str;
    }
    desc->forceType = (NeuralForceType)
        TextMap_FromString(v, tmForces, ARRAYSIZE(tmForces));

    s = prefix;
    s += "useTangent";
    desc->useTangent = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());
}

static void LoadNeuralCrowdDesc(MBRegistry *mreg,
                                NeuralCrowdDesc *desc, const char *prefix)
{
    MBString s;

    s = prefix;
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());
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
