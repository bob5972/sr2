/*
 * mutate.c -- part of SpaceRobots2
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

#include <math.h>
#include <stdio.h>

#include "mutate.h"
#include "MBAssert.h"
#include "Random.h"

void Mutate_Float(MBRegistry *mreg, MutationFloatParams *mpa, uint32 numParams)
{
    ASSERT(mpa != NULL);

    for (uint32 i = 0; i < numParams; i++) {
        MutationFloatParams *mp = &mpa[i];
        if (Random_Flip(mp->mutationRate)) {
            float value = MBRegistry_GetFloat(mreg, mp->key);

            if (!MBRegistry_ContainsKey(mreg, mp->key) ||
                Random_Flip(mp->jumpRate)) {
                value = Random_Float(mp->minValue, mp->maxValue);
            } else if (Random_Bit()) {
                if (Random_Bit()) {
                    value *= 1.0f - mp->magnitude;
                } else {
                    value *= 1.0f + mp->magnitude;
                }
            } else {
                float range = fabsf(mp->maxValue - mp->minValue);
                range = Random_Float(range * (1.0f - mp->magnitude),
                                    range * (1.0f + mp->magnitude));
                if (Random_Bit()) {
                    value += mp->magnitude * range;
                } else {
                    value -= mp->magnitude * range;
                }
            }

            value = MAX(mp->minValue, value);
            value = MIN(mp->maxValue, value);

            char *vStr = NULL;
            asprintf(&vStr, "%f", value);
            ASSERT(vStr != NULL);
            MBRegistry_PutCopy(mreg, mp->key, vStr);
            free(vStr);
        }
    }
}


void Mutate_Bool(MBRegistry *mreg, MutationBoolParams *mpa, uint32 numParams)
{
    ASSERT(mpa != NULL);

    for (uint32 i = 0; i < numParams; i++) {
        MutationBoolParams *mp = &mpa[i];
        if (Random_Flip(mp->flipRate)) {
            bool value;

            if (MBRegistry_ContainsKey(mreg, mp->key)) {
                value = !MBRegistry_GetBool(mreg, mp->key);
            } else {
                value = Random_Bit();
            }
            MBRegistry_PutCopy(mreg, mp->key, value ? "TRUE" : "FALSE");
        }
    }
}
