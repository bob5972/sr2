/*
 * mutate.h -- part of SpaceRobots2
 * Copyright (C) 2021 Michael Banack <github@banack.net>
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

#ifndef _MUTATE_H_20211117
#define _MUTATE_H_20211117

#include "MBRegistry.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum MutationType {
    MUTATION_TYPE_ANY,
    MUTATION_TYPE_WEIGHT,
    MUTATION_TYPE_RADIUS,
    MUTATION_TYPE_PERIOD,
    MUTATION_TYPE_PERIOD_OFFSET,
    MUTATION_TYPE_COUNT,
    MUTATION_TYPE_AMPLITUDE,
    MUTATION_TYPE_MOB_JITTER_SCALE,
    MUTATION_TYPE_SCALE_POW,
    MUTATION_TYPE_SIMPLE_POW,
    MUTATION_TYPE_BOOL,
    MUTATION_TYPE_PROBABILITY,
    MUTATION_TYPE_INVERSE_PROBABILITY,
    MUTATION_TYPE_TICKS,
    MUTATION_TYPE_UNIT,
    MUTATION_TYPE_SUNIT,
    MUTATION_TYPE_SPEED,
    MUTATION_TYPE_MAX,
} MutationType;

typedef struct MutationFloatParams {
    const char *key;
    float minValue;
    float maxValue;
    float magnitude;
    float jumpRate;
    float mutationRate;
} MutationFloatParams;

typedef struct MutationBoolParams {
    const char *key;
    float flipRate;
} MutationBoolParams;

typedef struct MutationStrParams {
    const char *key;
    float flipRate;
} MutationStrParams;

void Mutate_DefaultFloatParams(MutationFloatParams *mp, MutationType type);

void Mutate_FloatType(MBRegistry *mreg, const char *key, MutationType type);
void Mutate_Float(MBRegistry *mreg, MutationFloatParams *mp, uint32 numParams);
float Mutate_FloatRaw(float value, bool missing, MutationFloatParams *mp);
void Mutate_Index(MBRegistry *mreg, const char *key, float rate);
void Mutate_Bool(MBRegistry *mreg, MutationBoolParams *mp, uint32 numParams);
void Mutate_Str(MBRegistry *mreg, MutationStrParams *mp, uint32 numParams,
                const char **options, uint32 numOptions);

#ifdef __cplusplus
    }
#endif

#endif // _MUTATE_H_20211117