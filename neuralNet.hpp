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

typedef struct NeuralNetContext {
    RandomState *rs;
    MappingSensorGrid *sg;
    FleetAI *ai;
} NeuralNetContext;

typedef enum NeuralForceType {
    NEURAL_FORCE_VOID,
    NEURAL_FORCE_ZERO,
    NEURAL_FORCE_HEADING,
    NEURAL_FORCE_ALIGN,
    NEURAL_FORCE_COHERE,
    NEURAL_FORCE_SEPARATE,
    NEURAL_FORCE_NEAREST_FRIEND,
    NEURAL_FORCE_NEAREST_FRIEND_MISSILE,
    NEURAL_FORCE_EDGES,
    NEURAL_FORCE_CORNERS,
    NEURAL_FORCE_CENTER,
    NEURAL_FORCE_BASE,
    NEURAL_FORCE_BASE_DEFENSE,
    NEURAL_FORCE_ENEMY,
    NEURAL_FORCE_ENEMY_MISSILE,
    NEURAL_FORCE_ENEMY_BASE,
    NEURAL_FORCE_ENEMY_BASE_GUESS,
    NEURAL_FORCE_ENEMY_COHERE,
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

void NeuralValue_Mutate(MBRegistry *mreg, NeuralValueDesc *desc,
                        bool isOutput, float rate,
                        const char *prefix);

bool NeuralForce_GetFocus(NeuralNetContext *nc, Mob *mob,
                          NeuralForceDesc *desc, FPoint *focusPoint);
bool NeuralForce_GetForce(NeuralNetContext *nc, Mob *mob,
                          NeuralForceDesc *desc, FRPoint *rForce);


#endif // _NEURAL_NET_H_202210081813
