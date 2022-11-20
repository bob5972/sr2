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

#define NEURAL_ALLOW_ATTACK_FORCES FALSE

typedef enum NeuralForceType {
    NEURAL_FORCE_VOID,
    NEURAL_FORCE_ZERO,
    NEURAL_FORCE_HEADING,
    NEURAL_FORCE_ALIGN,
    NEURAL_FORCE_ALIGN2,
    NEURAL_FORCE_ALIGN_BIAS_CENTER,
    NEURAL_FORCE_FORWARD_ALIGN,
    NEURAL_FORCE_BACKWARD_ALIGN,
    NEURAL_FORCE_ADVANCE_ALIGN,
    NEURAL_FORCE_RETREAT_ALIGN,
    NEURAL_FORCE_COHERE,
    NEURAL_FORCE_FORWARD_COHERE,
    NEURAL_FORCE_BACKWARD_COHERE,
    NEURAL_FORCE_ADVANCE_COHERE,
    NEURAL_FORCE_RETREAT_COHERE,
    NEURAL_FORCE_SEPARATE,
    NEURAL_FORCE_FORWARD_SEPARATE,
    NEURAL_FORCE_BACKWARD_SEPARATE,
    NEURAL_FORCE_ADVANCE_SEPARATE,
    NEURAL_FORCE_RETREAT_SEPARATE,
    NEURAL_FORCE_NEAREST_FRIEND,
    NEURAL_FORCE_NEAREST_FRIEND_MISSILE,
    NEURAL_FORCE_EDGES,
    NEURAL_FORCE_NEAREST_EDGE,
    NEURAL_FORCE_FARTHEST_EDGE,
    NEURAL_FORCE_CORNERS,
    NEURAL_FORCE_NEAREST_CORNER,
    NEURAL_FORCE_FARTHEST_CORNER,
    NEURAL_FORCE_CENTER,
    NEURAL_FORCE_BASE,
    NEURAL_FORCE_BASE_DEFENSE,
    NEURAL_FORCE_BASE_SHELL,
    NEURAL_FORCE_BASE_FARTHEST_FRIEND,
    NEURAL_FORCE_BASE_CONTROL_LIMIT,
    NEURAL_FORCE_BASE_CONTROL_SHELL,
    NEURAL_FORCE_ENEMY,
    NEURAL_FORCE_ENEMY_ALIGN,
    NEURAL_FORCE_FORWARD_ENEMY_ALIGN,
    NEURAL_FORCE_BACKWARD_ENEMY_ALIGN,
    NEURAL_FORCE_ADVANCE_ENEMY_ALIGN,
    NEURAL_FORCE_RETREAT_ENEMY_ALIGN,
    NEURAL_FORCE_ENEMY_COHERE,
    NEURAL_FORCE_ENEMY_COHERE2,
    NEURAL_FORCE_FORWARD_ENEMY_COHERE,
    NEURAL_FORCE_BACKWARD_ENEMY_COHERE,
    NEURAL_FORCE_ADVANCE_ENEMY_COHERE,
    NEURAL_FORCE_RETREAT_ENEMY_COHERE,
    NEURAL_FORCE_ENEMY_MISSILE,
    NEURAL_FORCE_ENEMY_BASE,
    NEURAL_FORCE_ENEMY_BASE_GUESS,
    NEURAL_FORCE_ENEMY_BASE_GUESS_LAX,
    NEURAL_FORCE_MIDWAY,
    NEURAL_FORCE_MIDWAY_GUESS,
    NEURAL_FORCE_MIDWAY_GUESS_LAX,
    NEURAL_FORCE_CORES,

    NEURAL_FORCE_MAX,
} NeuralForceType;

typedef enum NeuralCrowdType {
    NEURAL_CROWD_FRIEND_FIGHTER,
    NEURAL_CROWD_FRIEND_MISSILE,
    NEURAL_CROWD_ENEMY_SHIP,
    NEURAL_CROWD_ENEMY_MISSILE,
    NEURAL_CROWD_CORES,
    NEURAL_CROWD_BASE_ENEMY_SHIP,
    NEURAL_CROWD_BASE_FRIEND_SHIP,
    NEURAL_CROWD_MAX,
} NeuralCrowdType;

typedef enum NeuralWaveType {
    NEURAL_WAVE_NONE,
    NEURAL_WAVE_SINE,
    NEURAL_WAVE_UNIT_SINE,
    NEURAL_WAVE_ABS_SINE,
    NEURAL_WAVE_FMOD,
    NEURAL_WAVE_MAX,
} NeuralWaveType;

typedef enum NeuralValueType {
    NEURAL_VALUE_VOID,
    NEURAL_VALUE_ZERO,
    NEURAL_VALUE_FORCE,
    NEURAL_VALUE_CROWD,
    NEURAL_VALUE_TICK,
    NEURAL_VALUE_MOBID,
    NEURAL_VALUE_RANDOM_UNIT,
    NEURAL_VALUE_CREDITS,
    NEURAL_VALUE_FRIEND_SHIPS,
    NEURAL_VALUE_MAX,
} NeuralValueType;

typedef struct NeuralForceDesc {
    NeuralForceType forceType;
    bool useTangent;
    bool filterForward;
    bool filterBackward;
    bool filterAdvance;
    bool filterRetreat;
    float radius;
    bool doIdle;
    bool doAttack;
} NeuralForceDesc;

typedef struct NeuralTickDesc {
    NeuralWaveType waveType;
    float frequency;
} NeuralTickDesc;

typedef struct NeuralCrowdDesc {
    NeuralCrowdType crowdType;
    float radius;
} NeuralCrowdDesc;

typedef struct NeuralValueDesc {
    NeuralValueType valueType;
    union {
        NeuralForceDesc forceDesc;
        NeuralCrowdDesc crowdDesc;
        NeuralTickDesc tickDesc;
    };
} NeuralValueDesc;

const char *NeuralForce_ToString(NeuralForceType nft);
const char *NeuralValue_ToString(NeuralValueType nvt);
const char *NeuralWave_ToString(NeuralWaveType nwt);
const char *NeuralCrowd_ToString(NeuralCrowdType nct);

NeuralForceType NeuralForce_FromString(const char *str);
NeuralValueType NeuralValue_FromString(const char *str);
NeuralWaveType  NeuralWave_FromString(const char *str);
NeuralCrowdType NeuralCrowd_FromString(const char *str);

NeuralForceType NeuralForce_Random();
NeuralValueType NeuralValue_Random();
NeuralWaveType NeuralWave_Random();
NeuralCrowdType NeuralCrowd_Random();

void NeuralValue_Load(MBRegistry *mreg,
                      NeuralValueDesc *desc, const char *prefix);
void NeuralForce_Load(MBRegistry *mreg,
                      NeuralForceDesc *desc, const char *prefix);
void NeuralCrowd_Load(MBRegistry *mreg,
                      NeuralCrowdDesc *desc, const char *prefix);
void NeuralTick_Load(MBRegistry *mreg,
                     NeuralTickDesc *desc, const char *prefix);

float NeuralValue_GetValue(AIContext *nc, Mob *mob,
                           NeuralValueDesc *desc, uint i);
void NeuralValue_Mutate(MBRegistry *mreg, NeuralValueDesc *desc,
                        bool isOutput, float rate,
                        const char *prefix);

bool NeuralForce_GetFocus(AIContext *nc, Mob *mob,
                          NeuralForceDesc *desc, FPoint *focusPoint);
bool NeuralForce_GetForce(AIContext *nc, Mob *mob,
                          NeuralForceDesc *desc, FRPoint *rForce);
float NeuralForce_GetRange(AIContext *nc, Mob *mob, NeuralForceDesc *desc);
void NeuralForce_ApplyToMob(AIContext *nc, Mob *mob, FRPoint *rForce);

float NeuralCrowd_GetValue(AIContext *nc, Mob *mob, NeuralCrowdDesc *desc);

float NeuralTick_GetValue(AIContext *nc, NeuralTickDesc *desc);

class NeuralNet {
public:
    // Members
    FloatNet floatNet;
    MBVector<NeuralValueDesc> inputDescs;
    MBVector<NeuralValueDesc> outputDescs;
    MBVector<float> inputs;
    MBVector<float> outputs;
    uint numNodes;
    AIContext aic;

    NeuralNet() {
        MBUtil_Zero(&aic, sizeof(aic));
        numNodes = 0;
    }

    void save(MBRegistry *mreg, const char *prefix) {
        floatNet.save(mreg, prefix);
    }

    void dumpSanitizedParams(MBRegistry *mreg, const char *prefix);

    void load(MBRegistry *mreg, const char *prefix);
    void mutate(MBRegistry *mreg);

    void doForces(Mob *mob, BasicShipAIState state, FRPoint *outputForce);


    static bool isOutputActive(BasicShipAIState state,
                               const NeuralValueDesc *outputDesc) {
        ASSERT(outputDesc->valueType == NEURAL_VALUE_FORCE ||
               outputDesc->valueType == NEURAL_VALUE_VOID ||
               outputDesc->valueType == NEURAL_VALUE_ZERO);

        if  (outputDesc->valueType == NEURAL_VALUE_VOID ||
             outputDesc->valueType == NEURAL_VALUE_ZERO) {
            return FALSE;
        }

        ASSERT(outputDesc->valueType == NEURAL_VALUE_FORCE);

        if (outputDesc->forceDesc.forceType == NEURAL_FORCE_VOID ||
            outputDesc->forceDesc.forceType == NEURAL_FORCE_ZERO) {
            return FALSE;
        }

        ASSERT(!NEURAL_ALLOW_ATTACK_FORCES);
        ASSERT((state == BSAI_STATE_IDLE && outputDesc->forceDesc.doIdle) ||
               (state == BSAI_STATE_ATTACK && outputDesc->forceDesc.doAttack));
        return TRUE;
    }

};

void NeuralNet_Mutate(MBRegistry *mreg, const char *prefix, float rate,
                      uint maxInputs, uint maxOutputs, uint maxNodes,
                      uint maxNodeDegree);

#endif // _NEURAL_NET_H_202210081813
