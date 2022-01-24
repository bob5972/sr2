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
    MUTATION_TYPE_WEIGHT,
    MUTATION_TYPE_RADIUS,
    MUTATION_TYPE_PERIOD,
    MUTATION_TYPE_COUNT,
    MUTATION_TYPE_AMPLITUDE,
    MUTATION_TYPE_MOB_JITTER_SCALE,
    MUTATION_TYPE_BOOL,
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

void Mutate_Float(MBRegistry *mreg, MutationFloatParams *mp, uint32 numParams);
void Mutate_Bool(MBRegistry *mreg, MutationBoolParams *mp, uint32 numParams);
void Mutate_Str(MBRegistry *mreg, MutationStrParams *mp, uint32 numParams,
                const char **options, uint32 numOptions);

#ifdef __cplusplus
    }
#endif

#endif // _MUTATE_H_20211117