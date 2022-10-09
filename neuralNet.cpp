/*
 * neuralNet.cpp -- part of SpaceRobots2
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
#include "Random.h"
#include "MBRegistry.h"
}

#include "neuralNet.hpp"
#include "textDump.hpp"

static TextMapEntry tmForces[] = {
    { TMENTRY(NEURAL_FORCE_VOID),                     },
    { TMENTRY(NEURAL_FORCE_ZERO),                     },
    { TMENTRY(NEURAL_FORCE_HEADING),                  },
    { TMENTRY(NEURAL_FORCE_ALIGN),                    },
    { TMENTRY(NEURAL_FORCE_COHERE),                   },
    { TMENTRY(NEURAL_FORCE_SEPARATE),                 },
    { TMENTRY(NEURAL_FORCE_NEAREST_FRIEND),           },
    { TMENTRY(NEURAL_FORCE_NEAREST_FRIEND_MISSILE),   },
    { TMENTRY(NEURAL_FORCE_EDGES),                    },
    { TMENTRY(NEURAL_FORCE_CORNERS),                  },
    { TMENTRY(NEURAL_FORCE_CENTER),                   },
    { TMENTRY(NEURAL_FORCE_BASE),                     },
    { TMENTRY(NEURAL_FORCE_BASE_DEFENSE),             },
    { TMENTRY(NEURAL_FORCE_ENEMY),                    },
    { TMENTRY(NEURAL_FORCE_ENEMY_MISSILE),            },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE),               },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS),         },
    { TMENTRY(NEURAL_FORCE_CORES),                    },
    { TMENTRY(NEURAL_FORCE_ENEMY_COHERE),             },
};

static TextMapEntry tmCrowds[] = {
    { TMENTRY(NEURAL_CROWD_FRIEND_FIGHTER),        },
    { TMENTRY(NEURAL_CROWD_FRIEND_MISSILE),        },
    { TMENTRY(NEURAL_CROWD_ENEMY_SHIP),            },
    { TMENTRY(NEURAL_CROWD_ENEMY_MISSILE),         },
    { TMENTRY(NEURAL_CROWD_CORES),                 },
    { TMENTRY(NEURAL_CROWD_BASE_ENEMY_SHIP),       },
    { TMENTRY(NEURAL_CROWD_BASE_FRIEND_SHIP),       },
};

static TextMapEntry tmWaves[] = {
    { TMENTRY(NEURAL_WAVE_NONE),        },
    { TMENTRY(NEURAL_WAVE_SINE),        },
    { TMENTRY(NEURAL_WAVE_UNIT_SINE),   },
    { TMENTRY(NEURAL_WAVE_ABS_SINE),    },
    { TMENTRY(NEURAL_WAVE_FMOD),        },
};

static TextMapEntry tmValues[] = {
    { TMENTRY(NEURAL_VALUE_VOID),  },
    { TMENTRY(NEURAL_VALUE_ZERO),  },
    { TMENTRY(NEURAL_VALUE_FORCE), },
    { TMENTRY(NEURAL_VALUE_CROWD), },
    { TMENTRY(NEURAL_VALUE_TICK),  },
    { TMENTRY(NEURAL_VALUE_MOBID), },
    { TMENTRY(NEURAL_VALUE_RANDOM_UNIT), },
    { TMENTRY(NEURAL_VALUE_CREDITS), },
    { TMENTRY(NEURAL_VALUE_FRIEND_SHIPS), },
};

const char *NeuralForce_ToString(NeuralForceType nft)
{
    return TextMap_ToString(nft, tmForces, ARRAYSIZE(tmForces));
}

const char *NeuralValue_ToString(NeuralValueType nvt)
{
    return TextMap_ToString(nvt, tmValues, ARRAYSIZE(tmValues));
}

const char *NeuralWave_ToString(NeuralWaveType nwt)
{
    return TextMap_ToString(nwt, tmWaves, ARRAYSIZE(tmWaves));
}

const char *NeuralCrowd_ToString(NeuralCrowdType nct)
{
    return TextMap_ToString(nct, tmCrowds, ARRAYSIZE(tmCrowds));
}

NeuralForceType NeuralForce_FromString(const char *str)
{
    return (NeuralForceType) TextMap_FromString(str, tmForces,
                                                ARRAYSIZE(tmForces));
}

NeuralValueType NeuralValue_FromString(const char *str)
{
    return (NeuralValueType) TextMap_FromString(str, tmValues,
                                                ARRAYSIZE(tmValues));
}

NeuralWaveType NeuralWave_FromString(const char *str)
{
    return (NeuralWaveType) TextMap_FromString(str, tmWaves,
                                               ARRAYSIZE(tmWaves));
}

NeuralCrowdType NeuralCrowd_FromString(const char *str)
{
    return (NeuralCrowdType) TextMap_FromString(str, tmCrowds,
                                                ARRAYSIZE(tmCrowds));
}

NeuralForceType NeuralForce_Random()
{
    uint i = Random_Int(1, ARRAYSIZE(tmForces) - 1);
    ASSERT(ARRAYSIZE(tmForces) == NEURAL_FORCE_MAX);
    ASSERT(tmForces[0].value == NEURAL_FORCE_VOID);
    return (NeuralForceType) tmForces[i].value;
}

NeuralValueType NeuralValue_Random()
{
    EnumDistribution vts[] = {
        { NEURAL_VALUE_VOID,         0.00f, },
        { NEURAL_VALUE_ZERO,         0.02f, },
        { NEURAL_VALUE_FORCE,        0.40f, },
        { NEURAL_VALUE_CROWD,        0.40f, },
        { NEURAL_VALUE_TICK,         0.04f, },
        { NEURAL_VALUE_MOBID,        0.04f, },
        { NEURAL_VALUE_RANDOM_UNIT,  0.04f, },
        { NEURAL_VALUE_CREDITS,      0.02f, },
        { NEURAL_VALUE_FRIEND_SHIPS, 0.04f, },
    };

    ASSERT(ARRAYSIZE(vts) == NEURAL_VALUE_MAX);
    return (NeuralValueType) Random_Enum(vts, ARRAYSIZE(vts));
}

NeuralWaveType NeuralWave_Random()
{
    uint i = Random_Int(0, ARRAYSIZE(tmWaves) - 1);
    ASSERT(ARRAYSIZE(tmWaves) == NEURAL_WAVE_MAX);
    return (NeuralWaveType) tmWaves[i].value;
}

NeuralCrowdType NeuralCrowd_Random()
{
    uint i = Random_Int(0, ARRAYSIZE(tmCrowds) - 1);
    ASSERT(ARRAYSIZE(tmCrowds) == NEURAL_CROWD_MAX);
    return (NeuralCrowdType) tmCrowds[i].value;
}

void NeuralValue_Load(MBRegistry *mreg,
                      NeuralValueDesc *desc, const char *prefix)
{
    MBString s;
    const char *cstr;

    s = prefix;
    s += "valueType";
    cstr = MBRegistry_GetCStr(mreg, s.CStr());
    desc->valueType = NEURAL_VALUE_MAX;

    if (cstr == NULL) {
        cstr = NeuralValue_ToString(NEURAL_VALUE_ZERO);
    }

    desc->valueType = NeuralValue_FromString(cstr);
    VERIFY(desc->valueType < NEURAL_VALUE_MAX);

    s = prefix;
    switch (desc->valueType) {
        case NEURAL_VALUE_FORCE:
            NeuralForce_Load(mreg, &desc->forceDesc, s.CStr());
            break;

        case NEURAL_VALUE_CROWD:
            NeuralCrowd_Load(mreg, &desc->crowdDesc, s.CStr());
            break;

        case NEURAL_VALUE_TICK:
            NeuralTick_Load(mreg, &desc->tickDesc, s.CStr());
            break;

        case NEURAL_VALUE_VOID:
        case NEURAL_VALUE_ZERO:
        case NEURAL_VALUE_MOBID:
        case NEURAL_VALUE_RANDOM_UNIT:
        case NEURAL_VALUE_CREDITS:
        case NEURAL_VALUE_FRIEND_SHIPS:
            break;

        default:
            NOT_IMPLEMENTED();
    }
}

void NeuralForce_Load(MBRegistry *mreg,
                      NeuralForceDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "forceType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        v = NeuralForce_ToString(NEURAL_FORCE_ZERO);
    }
    desc->forceType = NeuralForce_FromString(v);

    s = prefix;
    s += "useTangent";
    desc->useTangent = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());

    s = prefix;
    s += "doIdle";
    desc->doIdle = MBRegistry_GetBoolD(mreg, s.CStr(), TRUE);

    s = prefix;
    s += "doAttack";
    desc->doAttack = MBRegistry_GetBoolD(mreg, s.CStr(), FALSE);
}

void NeuralCrowd_Load(MBRegistry *mreg,
                      NeuralCrowdDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());

    s = prefix;
    s += "crowdType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        NeuralCrowd_ToString(NEURAL_CROWD_FRIEND_FIGHTER);
    }
    desc->crowdType = NeuralCrowd_FromString(v);
}

void NeuralTick_Load(MBRegistry *mreg,
                     NeuralTickDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "frequency";
    desc->frequency = MBRegistry_GetFloat(mreg, s.CStr());

    s = prefix;
    s += "waveType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        v = NeuralWave_ToString(NEURAL_WAVE_NONE);
    }
    desc->waveType = NeuralWave_FromString(v);
}
