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

float Mutate_FloatRaw(float value, bool missing, MutationFloatParams *mp)
{
    ASSERT(mp != NULL);

    if (Random_Flip(mp->mutationRate)) {
        if (missing || Random_Flip(mp->jumpRate)) {
            /*
             * Bias jumps slightly towards interesting values.
             */
            if (Random_Flip(0.50)) {
                float f[] = {
                    -1.0f, 0.0f, 1.0f,
                    mp->minValue, mp->maxValue,
                    -mp->minValue, -mp->maxValue,
                    1.0f / mp->minValue, 1.0f / mp->maxValue,
                    mp->minValue / 2.0f, mp->maxValue / 2.0f,
                    (mp->minValue + mp->maxValue) / 2.0f,
                    value / 2.0f, value * 2.0f, -value,
                    1.0f / value, -1.0f / value,
                    (mp->minValue + value) / 2.0f,
                    (mp->maxValue + value) / 2.0f,
                    mp->minValue + value,
                    mp->maxValue - value,
                };
                uint32 r = Random_Uint32() % ARRAYSIZE(f);
                value = f[r];
            } else {
                value = Random_Float(mp->minValue, mp->maxValue);
            }
        } else if (value != 0.0f && Random_Bit()) {
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
    }

    return value;
}

void Mutate_Float(MBRegistry *mreg, MutationFloatParams *mpa, uint32 numParams)
{
    ASSERT(mpa != NULL);

    for (uint32 i = 0; i < numParams; i++) {
        MutationFloatParams *mp = &mpa[i];
        if (Random_Flip(mp->mutationRate)) {
            bool missing = !MBRegistry_ContainsKey(mreg, mp->key);
            float value = MBRegistry_GetFloat(mreg, mp->key);
            value = Mutate_FloatRaw(value, missing, mp);

            char *vStr = NULL;
            int ret = asprintf(&vStr, "%f", value);
            VERIFY(ret > 0);
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


void Mutate_Str(MBRegistry *mreg, MutationStrParams *mpa, uint32 numParams,
                const char **options, uint32 numOptions)
{
    ASSERT(mpa != NULL);
    ASSERT(numOptions > 0);

    for (uint32 i = 0; i < numParams; i++) {
        MutationStrParams *mp = &mpa[i];
        if (Random_Flip(mp->flipRate)) {
            uint choice = Random_Int(0, numOptions - 1);
            MBRegistry_PutCopy(mreg, mp->key, options[choice]);
        }
    }
}

void Mutate_DefaultFloatParams(MutationFloatParams *vf, MutationType type)
{
    if (type == MUTATION_TYPE_ANY) {
        /*
         * Catch-all for unknown value.
         */
        ASSERT(MUTATION_TYPE_ANY == 0);
        type = Random_Int(MUTATION_TYPE_ANY + 1, MUTATION_TYPE_MAX - 1);
        Mutate_DefaultFloatParams(vf, type);
    } else if (type == MUTATION_TYPE_WEIGHT) {
        vf->minValue = -10.0f;
        vf->maxValue = 10.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.15f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_AMPLITUDE ||
               type == MUTATION_TYPE_BOOL ||
               type == MUTATION_TYPE_SUNIT ||
               type == MUTATION_TYPE_MOB_JITTER_SCALE) {
        vf->minValue = -1.0f;
        vf->maxValue = 1.0f;
        vf->magnitude = 0.1f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_PROBABILITY ||
               type == MUTATION_TYPE_UNIT) {
        vf->minValue = 0.0f;
        vf->maxValue = 1.0f;
        vf->magnitude = 0.1f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_SCALE_POW) {
        vf->minValue = 0.0f;
        vf->maxValue = 10.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_SIMPLE_POW) {
        vf->minValue = -5.0f;
        vf->maxValue = 5.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_INVERSE_PROBABILITY) {
        vf->minValue = -1.0f;
        vf->maxValue = 10000.0f;
        vf->magnitude = 0.1f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_RADIUS) {
        vf->minValue = -1.0f;
        vf->maxValue = 3000.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_PERIOD) {
        vf->minValue = -1.0f;
        vf->maxValue = 20000.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_PERIOD_OFFSET) {
        vf->minValue = -10000.0f;
        vf->maxValue = 10000.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_TICKS) {
        vf->minValue = -1.0f;
        vf->maxValue = 10000.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_COUNT) {
        vf->minValue = -1.0f;
        vf->maxValue = 30.0f;
        vf->magnitude = 0.05f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else if (type == MUTATION_TYPE_SPEED) {
        vf->minValue = -1.0f;
        vf->maxValue = 20.0f;
        vf->magnitude = 0.04f;
        vf->jumpRate = 0.10f;
        vf->mutationRate = 0.05f;
    } else {
        NOT_IMPLEMENTED();
    }
}

void Mutate_FloatType(MBRegistry *mreg, const char *key, MutationType type)
{
    MutationFloatParams mp;
    Mutate_DefaultFloatParams(&mp, type);
    mp.key = key;
    Mutate_Float(mreg, &mp, 1);
}

void Mutate_Index(MBRegistry *mreg, const char *key, float rate)
{
    int x, ret;
    char *v = NULL;

    if (!Random_Flip(rate)) {
        return;
    }

    x = MBRegistry_GetInt(mreg, key);

    if (Random_Flip(0.01f)) {
        x = -1;
    } else if (Random_Flip(0.1f)) {
        x = Random_Int(-1, 8);
    } else if (Random_Flip(0.1f)) {
        x = Random_Int(-1, 32);
    } else if (Random_Flip(0.5f)) {
        x = Random_Int(-1, MAX(1, 2 * x));
    } else {
        x = Random_Int(-1, MAX(1, x + 1));
    }

    ret = asprintf(&v, "%d", x);
    VERIFY(ret > 0);
    MBRegistry_PutCopy(mreg, key, v);
    free(v);
}
