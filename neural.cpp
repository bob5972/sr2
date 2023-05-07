/*
 * neural.cpp -- part of SpaceRobots2
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

#include "neural.hpp"
#include "textDump.hpp"
#include "mutate.h"
#include "mobFilter.h"
#include "ml.hpp"

static bool NeuralForceGeneMidway(AIContext *nc,
                                  Mob *mob,
                                  NeuralForceDesc *desc,
                                  FPoint *focusPoint);
static bool NeuralForceGeneEnemyMissile(AIContext *nc,
                                        Mob *mob,
                                        NeuralForceDesc *desc,
                                        FPoint *focusPoint);
static bool NeuralForceGeneRetreatCohere(AIContext *nc,
                                         Mob *mob,
                                         NeuralForceDesc *desc,
                                         FPoint *focusPoint);

static bool NeuralForceGetAdvanceFocusHelper(AIContext *nc,
                                             Mob *mob, FPoint *focusPoint,
                                             bool advance);
static bool NeuralForceGetForwardFocusHelper(AIContext *nc,
                                             Mob *mob, FPoint *focusPoint,
                                             bool forward);
static bool NeuralForceGetBaseControlLimitFocus(AIContext *nc, FPoint *focusPoint);
static void NeuralForceGetHeading(AIContext *nc, Mob *mob, FRPoint *heading);
static bool NeuralForceGetFocusMobPosHelper(Mob *mob, FPoint *focusPoint);
static void NeuralForceGetRepulseFocus(AIContext *nc,
                                       const FPoint *selfPos,
                                       const FPoint *pos, FPoint *force);


static const TextMapEntry tmForces[] = {
    { TMENTRY(NEURAL_FORCE_VOID),                            },
    { TMENTRY(NEURAL_FORCE_ZERO),                            },
    { TMENTRY(NEURAL_FORCE_HEADING),                         },
    { TMENTRY(NEURAL_FORCE_ALIGN),                           },
    { TMENTRY(NEURAL_FORCE_ALIGN2),                          },
    { TMENTRY(NEURAL_FORCE_ALIGN_BIAS_CENTER),               },
    { TMENTRY(NEURAL_FORCE_FORWARD_ALIGN),                   },
    { TMENTRY(NEURAL_FORCE_BACKWARD_ALIGN),                  },
    { TMENTRY(NEURAL_FORCE_ADVANCE_ALIGN),                   },
    { TMENTRY(NEURAL_FORCE_RETREAT_ALIGN),                   },
    { TMENTRY(NEURAL_FORCE_COHERE),                          },
    { TMENTRY(NEURAL_FORCE_FORWARD_COHERE),                  },
    { TMENTRY(NEURAL_FORCE_BACKWARD_COHERE),                 },
    { TMENTRY(NEURAL_FORCE_ADVANCE_COHERE),                  },
    { TMENTRY(NEURAL_FORCE_RETREAT_COHERE),                  },
    { TMENTRY(NEURAL_FORCE_BROKEN_COHERE),                   },
    { TMENTRY(NEURAL_FORCE_SEPARATE),                        },
    { TMENTRY(NEURAL_FORCE_FORWARD_SEPARATE),                },
    { TMENTRY(NEURAL_FORCE_BACKWARD_SEPARATE),               },
    { TMENTRY(NEURAL_FORCE_ADVANCE_SEPARATE),                },
    { TMENTRY(NEURAL_FORCE_RETREAT_SEPARATE),                },
    { TMENTRY(NEURAL_FORCE_NEAREST_FRIEND),                  },
    { TMENTRY(NEURAL_FORCE_NEAREST_FRIEND_MISSILE),          },
    { TMENTRY(NEURAL_FORCE_EDGES),                           },
    { TMENTRY(NEURAL_FORCE_NEAREST_EDGE),                    },
    { TMENTRY(NEURAL_FORCE_FARTHEST_EDGE),                   },
    { TMENTRY(NEURAL_FORCE_CORNERS),                         },
    { TMENTRY(NEURAL_FORCE_NEAREST_CORNER),                  },
    { TMENTRY(NEURAL_FORCE_FARTHEST_CORNER),                 },
    { TMENTRY(NEURAL_FORCE_CENTER),                          },
    { TMENTRY(NEURAL_FORCE_BASE),                            },
    { TMENTRY(NEURAL_FORCE_BASE_LAX),                        },
    { TMENTRY(NEURAL_FORCE_BASE_MIRROR_LAX),                 },
    { TMENTRY(NEURAL_FORCE_BASE_DEFENSE),                    },
    { TMENTRY(NEURAL_FORCE_BASE_SHELL),                      },
    { TMENTRY(NEURAL_FORCE_BASE_FARTHEST_FRIEND),            },
    { TMENTRY(NEURAL_FORCE_BASE_CONTROL_LIMIT),              },
    { TMENTRY(NEURAL_FORCE_BASE_CONTROL_SHELL),              },
    { TMENTRY(NEURAL_FORCE_ENEMY),                           },
    { TMENTRY(NEURAL_FORCE_ENEMY_ALIGN),                     },
    { TMENTRY(NEURAL_FORCE_FORWARD_ENEMY_ALIGN),             },
    { TMENTRY(NEURAL_FORCE_BACKWARD_ENEMY_ALIGN),            },
    { TMENTRY(NEURAL_FORCE_ADVANCE_ENEMY_ALIGN),             },
    { TMENTRY(NEURAL_FORCE_RETREAT_ENEMY_ALIGN),             },
    { TMENTRY(NEURAL_FORCE_ENEMY_COHERE),                    },
    { TMENTRY(NEURAL_FORCE_ENEMY_COHERE2),                   },
    { TMENTRY(NEURAL_FORCE_FORWARD_ENEMY_COHERE),            },
    { TMENTRY(NEURAL_FORCE_BACKWARD_ENEMY_COHERE),           },
    { TMENTRY(NEURAL_FORCE_ADVANCE_ENEMY_COHERE),            },
    { TMENTRY(NEURAL_FORCE_RETREAT_ENEMY_COHERE),            },
    { TMENTRY(NEURAL_FORCE_ENEMY_MISSILE),                   },
    { TMENTRY(NEURAL_FORCE_ENEMY_MISSILE_ALIGN),             },
    { TMENTRY(NEURAL_FORCE_FORWARD_ENEMY_MISSILE_ALIGN),     },
    { TMENTRY(NEURAL_FORCE_BACKWARD_ENEMY_MISSILE_ALIGN),    },
    { TMENTRY(NEURAL_FORCE_ADVANCE_ENEMY_MISSILE_ALIGN),     },
    { TMENTRY(NEURAL_FORCE_RETREAT_ENEMY_MISSILE_ALIGN),     },
    { TMENTRY(NEURAL_FORCE_ENEMY_MISSILE_COHERE),            },
    { TMENTRY(NEURAL_FORCE_FORWARD_ENEMY_MISSILE_COHERE),    },
    { TMENTRY(NEURAL_FORCE_BACKWARD_ENEMY_MISSILE_COHERE),   },
    { TMENTRY(NEURAL_FORCE_ADVANCE_ENEMY_MISSILE_COHERE),    },
    { TMENTRY(NEURAL_FORCE_RETREAT_ENEMY_MISSILE_COHERE),    },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE),                      },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS),                },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS_LAX),            },
    { TMENTRY(NEURAL_FORCE_MIDWAY),                          },
    { TMENTRY(NEURAL_FORCE_MIDWAY_GUESS),                    },
    { TMENTRY(NEURAL_FORCE_MIDWAY_GUESS_LAX),                },
    { TMENTRY(NEURAL_FORCE_CORES),                           },
    { TMENTRY(NEURAL_FORCE_LOCUS),                           },
    { TMENTRY(NEURAL_FORCE_NEXT_LOCUS),                      },
    { TMENTRY(NEURAL_FORCE_UNEXPLORED),                      },
    { TMENTRY(NEURAL_FORCE_CIRCULAR),                        },
    { TMENTRY(NEURAL_FORCE_MOB_ROW),                         },
    { TMENTRY(NEURAL_FORCE_MOB_COLUMN),                      },
    { TMENTRY(NEURAL_FORCE_MOB_SPOT),                        },
    { TMENTRY(NEURAL_FORCE_MOB_BASE_SHELL),                  },
    { TMENTRY(NEURAL_FORCE_MOB_BASE_SECTOR),                 },
    { TMENTRY(NEURAL_FORCE_MOB_CENTER_SHELL),                },
    { TMENTRY(NEURAL_FORCE_MOB_CENTER_SECTOR),               },
    { TMENTRY(NEURAL_FORCE_LAST_TARGET_SHADOW),              },
    { TMENTRY(NEURAL_FORCE_GENE_MIDWAY),                     },
    { TMENTRY(NEURAL_FORCE_GENE_ENEMY_MISSILE),              },
    { TMENTRY(NEURAL_FORCE_GENE_RETREAT_COHERE),             },
};

static const TextMapEntry tmCrowds[] = {
    { TMENTRY(NEURAL_CROWD_FRIEND_FIGHTER),        },
    { TMENTRY(NEURAL_CROWD_FRIEND_MISSILE),        },
    { TMENTRY(NEURAL_CROWD_ENEMY_SHIP),            },
    { TMENTRY(NEURAL_CROWD_ENEMY_MISSILE),         },
    { TMENTRY(NEURAL_CROWD_CORES),                 },
    { TMENTRY(NEURAL_CROWD_FRIEND_CORES),          },
    { TMENTRY(NEURAL_CROWD_BASE_ENEMY_SHIP),       },
    { TMENTRY(NEURAL_CROWD_BASE_FRIEND_SHIP),      },
    { TMENTRY(NEURAL_CROWD_NET_ENEMY_SHIP),        },
    { TMENTRY(NEURAL_CROWD_NET_FRIEND_SHIP),       },
};

static const TextMapEntry tmSquads[] = {
    { TMENTRY(NEURAL_SQUAD_NONE),                  },
    { TMENTRY(NEURAL_SQUAD_MOBID),                 },
    { TMENTRY(NEURAL_SQUAD_EQUAL_PARTITIONS),      },
    { TMENTRY(NEURAL_SQUAD_POWER_UP),              },
    { TMENTRY(NEURAL_SQUAD_POWER_DOWN),            },
};

static const TextMapEntry tmWaves[] = {
    { TMENTRY(NEURAL_WAVE_NONE),        },
    { TMENTRY(NEURAL_WAVE_SINE),        },
    { TMENTRY(NEURAL_WAVE_UNIT_SINE),   },
    { TMENTRY(NEURAL_WAVE_ABS_SINE),    },
    { TMENTRY(NEURAL_WAVE_FMOD),        },
};

static const TextMapEntry tmValues[] = {
    { TMENTRY(NEURAL_VALUE_VOID),            },
    { TMENTRY(NEURAL_VALUE_ZERO),            },
    { TMENTRY(NEURAL_VALUE_FORCE),           },
    { TMENTRY(NEURAL_VALUE_CROWD),           },
    { TMENTRY(NEURAL_VALUE_TICK),            },
    { TMENTRY(NEURAL_VALUE_MOBID),           },
    { TMENTRY(NEURAL_VALUE_SQUAD),           },
    { TMENTRY(NEURAL_VALUE_RANDOM_UNIT),     },
    { TMENTRY(NEURAL_VALUE_CREDITS),         },
    { TMENTRY(NEURAL_VALUE_FRIEND_SHIPS),    },
    { TMENTRY(NEURAL_VALUE_FRIEND_MISSILES), },
    { TMENTRY(NEURAL_VALUE_ENEMY_SHIPS),     },
    { TMENTRY(NEURAL_VALUE_ENEMY_MISSILES),  },
    { TMENTRY(NEURAL_VALUE_SCALAR),          },
};

static const TextMapEntry tmLocus[] = {
    { TMENTRY(NEURAL_LOCUS_VOID),         },
    { TMENTRY(NEURAL_LOCUS_TRACK),        },
    { TMENTRY(NEURAL_LOCUS_ORBIT),        },
    { TMENTRY(NEURAL_LOCUS_PATROL_MAP),   },
    { TMENTRY(NEURAL_LOCUS_PATROL_EDGES), },
};
static const TextMapEntry tmCombiners[] = {
    { TMENTRY(NEURAL_CT_VOID),         },
    { TMENTRY(NEURAL_CT_ASSIGN),       },
    { TMENTRY(NEURAL_CT_MULTIPLY),     },
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

const char *NeuralSquad_ToString(NeuralSquadType nct)
{
    return TextMap_ToString(nct, tmSquads, ARRAYSIZE(tmSquads));
}

const char *NeuralLocus_ToString(NeuralLocusType nlt)
{
    return TextMap_ToString(nlt, tmLocus, ARRAYSIZE(tmLocus));
}

const char *NeuralCombiner_ToString(NeuralCombinerType nct)
{
    return TextMap_ToString(nct, tmCombiners, ARRAYSIZE(tmCombiners));
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

NeuralSquadType NeuralSquad_FromString(const char *str)
{
    return (NeuralSquadType) TextMap_FromString(str, tmSquads,
                                                ARRAYSIZE(tmSquads));
}

NeuralLocusType NeuralLocus_FromString(const char *str)
{
    return (NeuralLocusType) TextMap_FromString(str, tmLocus,
                                                ARRAYSIZE(tmLocus));
}

NeuralCombinerType NeuralCombiner_FromString(const char *str)
{
    return (NeuralCombinerType) TextMap_FromString(str, tmCombiners,
                                                ARRAYSIZE(tmCombiners));
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
        { NEURAL_VALUE_VOID,            0.00f, },
        { NEURAL_VALUE_ZERO,            0.02f, },
        { NEURAL_VALUE_FORCE,           0.30f, },
        { NEURAL_VALUE_CROWD,           0.30f, },
        { NEURAL_VALUE_TICK,            0.04f, },
        { NEURAL_VALUE_MOBID,           0.04f, },
        { NEURAL_VALUE_SQUAD,           0.04f, },
        { NEURAL_VALUE_RANDOM_UNIT,     0.04f, },
        { NEURAL_VALUE_CREDITS,         0.02f, },
        { NEURAL_VALUE_FRIEND_SHIPS,    0.04f, },
        { NEURAL_VALUE_FRIEND_MISSILES, 0.04f, },
        { NEURAL_VALUE_ENEMY_SHIPS,     0.04f, },
        { NEURAL_VALUE_ENEMY_MISSILES,  0.04f, },
        { NEURAL_VALUE_SCALAR,          0.04f, },
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

NeuralSquadType NeuralSquad_Random()
{
    uint i = Random_Int(0, ARRAYSIZE(tmSquads) - 1);
    ASSERT(ARRAYSIZE(tmSquads) == NEURAL_SQUAD_MAX);
    return (NeuralSquadType) tmSquads[i].value;
}

NeuralLocusType NeuralLocus_Random()
{
    uint i = Random_Int(0, ARRAYSIZE(tmLocus) - 1);
    ASSERT(ARRAYSIZE(tmLocus) == NEURAL_LOCUS_MAX);
    return (NeuralLocusType) tmLocus[i].value;
}

NeuralCombinerType NeuralCombiner_Random()
{
    uint i = Random_Int(0, ARRAYSIZE(tmCombiners) - 1);
    ASSERT(ARRAYSIZE(tmCombiners) == NEURAL_CT_MAX);
    return (NeuralCombinerType) tmCombiners[i].value;
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

        case NEURAL_VALUE_SQUAD:
            NeuralSquad_Load(mreg, &desc->squadDesc, s.CStr());
            break;

        case NEURAL_VALUE_TICK:
            NeuralTick_Load(mreg, &desc->tickDesc, s.CStr());
            break;

        case NEURAL_VALUE_SCALAR:
            NeuralScalar_Load(mreg, &desc->scalarDesc, s.CStr());
            break;

        case NEURAL_VALUE_VOID:
        case NEURAL_VALUE_ZERO:
        case NEURAL_VALUE_MOBID:
        case NEURAL_VALUE_RANDOM_UNIT:
        case NEURAL_VALUE_CREDITS:
        case NEURAL_VALUE_FRIEND_SHIPS:
        case NEURAL_VALUE_FRIEND_MISSILES:
        case NEURAL_VALUE_ENEMY_SHIPS:
        case NEURAL_VALUE_ENEMY_MISSILES:
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
    s += "useBase";
    desc->useBase = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "filterForward";
    desc->filterForward = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "filterBackward";
    desc->filterBackward = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "filterAdvance";
    desc->filterAdvance = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "filterRetreat";
    desc->filterRetreat = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "filterRange";
    desc->filterRange = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());

    s = prefix;
    s += "range";
    desc->range = MBRegistry_GetFloat(mreg, s.CStr());

    s = prefix;
    s += "index";
    desc->index = MBRegistry_GetIntD(mreg, s.CStr(), -1);
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
        v = NeuralCrowd_ToString(NEURAL_CROWD_FRIEND_FIGHTER);
    }
    desc->crowdType = NeuralCrowd_FromString(v);
}

void NeuralSquad_Load(MBRegistry *mreg,
                      NeuralSquadDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "numSquads";
    desc->numSquads = MBRegistry_GetInt(mreg, s.CStr());

    s = prefix;
    s += "seed";
    desc->seed = MBRegistry_GetInt(mreg, s.CStr());

    s = prefix;
    s += "squadType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        v = NeuralSquad_ToString(NEURAL_SQUAD_NONE);
    }
    desc->squadType = NeuralSquad_FromString(v);
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


void NeuralLocus_Load(MBRegistry *mreg,
                      NeuralLocusDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    s += "locusType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        v = NeuralLocus_ToString(NEURAL_LOCUS_VOID);
    }
    desc->locusType = NeuralLocus_FromString(v);

    s = prefix;
    s += "speed";
    desc->speed = MBRegistry_GetFloat(mreg, s.CStr());

    s = prefix;
    s += "speedLimited";
    desc->speedLimited = MBRegistry_GetBool(mreg, s.CStr());

    if (desc->locusType == NEURAL_LOCUS_VOID) {
        // Nothing to load.
    } else if (desc->locusType == NEURAL_LOCUS_TRACK) {
        s = prefix;
        s += "focus.";
        NeuralForce_Load(mreg, &desc->orbitDesc.focus, s.CStr());
    } else if (desc->locusType == NEURAL_LOCUS_ORBIT) {
        s = prefix;
        s += "radius";
        desc->orbitDesc.radius = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "period";
        desc->orbitDesc.period = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "focus.";
        NeuralForce_Load(mreg, &desc->orbitDesc.focus, s.CStr());
    } else if (desc->locusType == NEURAL_LOCUS_PATROL_MAP) {
        s = prefix;
        s += "linearPeriod";
        desc->patrolMapDesc.linearPeriod = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "linearXPeriodOffset";
        desc->patrolMapDesc.linearXPeriodOffset = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "linearYPeriodOffset";
        desc->patrolMapDesc.linearYPeriodOffset = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "linearWeight";
        desc->patrolMapDesc.linearWeight = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "circularPeriod";
        desc->patrolMapDesc.circularPeriod = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "circularWeight";
        desc->patrolMapDesc.circularWeight = MBRegistry_GetFloat(mreg, s.CStr());
    } else if (desc->locusType == NEURAL_LOCUS_PATROL_EDGES) {
        s = prefix;
        s += "period";
        desc->patrolEdgesDesc.period = MBRegistry_GetFloat(mreg, s.CStr());
    } else {
        NOT_IMPLEMENTED();
    }
}

void NeuralCondition_Load(MBRegistry *mreg,
                         NeuralConditionDesc *desc, const char *prefix)
{
    MBString s;

    s = prefix;
    s += "squad.active";
    desc->squad.active = MBRegistry_GetBool(mreg, s.CStr());

    s = prefix;
    s += "squad.invert";
    desc->squad.invert = MBRegistry_GetBool(mreg, s.CStr());

    if (desc->squad.active) {
        s = prefix;
        s += "squad.desc.";
        NeuralSquad_Load(mreg, &desc->squad.squadDesc, s.CStr());

        s = prefix;
        s += "squad.limit0";
        desc->squad.limit0 = MBRegistry_GetFloat(mreg, s.CStr());

        s = prefix;
        s += "squad.limit1";
        desc->squad.limit1 = MBRegistry_GetFloat(mreg, s.CStr());
    }
}

void NeuralScalar_Load(MBRegistry *mreg,
                       NeuralScalarDesc *desc, const char *prefix)
{
    MBString s;

    s = prefix;
    s += "scalarID";
    desc->scalarID = MBRegistry_GetInt(mreg, s.CStr());
}

void NeuralOutput_Load(MBRegistry *mreg,
                       NeuralOutputDesc *desc, const char *prefix)
{
    MBString s;
    const char *v;

    s = prefix;
    NeuralValue_Load(mreg, &desc->value, s.CStr());

    if (NN_USE_CONDITIONS) {
        s = prefix;
        s += "condition.";
        NeuralCondition_Load(mreg, &desc->condition, s.CStr());
    } else {
        MBUtil_Zero(&desc->condition, sizeof(desc->condition));
    }

    s = prefix;
    s += "combiner.combinerType";
    v = MBRegistry_GetCStr(mreg, s.CStr());
    if (v == NULL) {
        v = NeuralCombiner_ToString(NEURAL_CT_ASSIGN);
    }
    desc->cType = NeuralCombiner_FromString(v);
}

void NeuralLocus_Mutate(MBRegistry *mreg,
                        float rate, const char *prefix)
{
    MBString s;
    const char *v;
    NeuralLocusDesc desc;
    MutationBoolParams bf;

    NeuralLocus_Load(mreg, &desc, prefix);

    if (Random_Flip(rate)) {
        s = prefix;
        s += "locusType";
        desc.locusType = NeuralLocus_Random();
        v = NeuralLocus_ToString(desc.locusType);
        MBRegistry_PutCopy(mreg, s.CStr(), v);
    }

    s = prefix;
    s += "speed";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_SPEED);

    s = prefix;
    s += "speedLimited";
    bf.key = s.CStr();
    bf.flipRate = MIN(0.5f, rate);
    Mutate_Bool(mreg, &bf, 1);

    /*
     * Mutate all the fields, not just the active ones for the current
     * locus type, to get some genetic drift.
     */
    ASSERT(desc.locusType == NEURAL_LOCUS_VOID ||
           desc.locusType == NEURAL_LOCUS_TRACK ||
           desc.locusType == NEURAL_LOCUS_ORBIT ||
           desc.locusType == NEURAL_LOCUS_PATROL_MAP ||
           desc.locusType == NEURAL_LOCUS_PATROL_EDGES);

    // NEURAL_LOCUS_ORBIT
    s = prefix;
    s += "radius";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_RADIUS);

    // NEURAL_LOCUS_ORBIT || NEURAL_LOCUS_TRACK

    s = prefix;
    s += "focus.";
    NeuralForce_Mutate(mreg, rate, s.CStr());

    // NEURAL_LOCUS_ORBIT || NEURAL_LOCUS_PATROL_EDGES
    s = prefix;
    s += "period";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_PERIOD);

    // NEURAL_LOCUS_PATROL_MAP
    s = prefix;
    s += "linearPeriod";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_PERIOD);

    s = prefix;
    s += "linearXPeriodOffset";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_PERIOD_OFFSET);

    s = prefix;
    s += "linearYPeriodOffset";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_PERIOD_OFFSET);

    s = prefix;
    s += "linearWeight";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_WEIGHT);

    s = prefix;
    s += "circularPeriod";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_PERIOD);

    s = prefix;
    s += "circularWeight";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_WEIGHT);
}

void NeuralCondition_Mutate(MBRegistry *mreg,
                            float rate,
                            NeuralNetType nnType,
                            const char *prefix)
{
    MBString s;
    MutationBoolParams bf;

    VERIFY(NN_USE_CONDITIONS);

    if (nnType == NN_TYPE_SCALARS) {
        return;
    }
    ASSERT(nnType == NN_TYPE_FORCES);

    s = prefix;
    s += "squad.active";
    bf.key = s.CStr();
    bf.flipRate = MIN(0.5f, rate);
    Mutate_Bool(mreg, &bf, 1);

    s = prefix;
    s += "squad.invert";
    bf.key = s.CStr();
    bf.flipRate = MIN(0.5f, rate);
    Mutate_Bool(mreg, &bf, 1);

    s = prefix;
    s += "squad.desc.";
    NeuralSquad_Mutate(mreg, rate, prefix);

    s = prefix;
    s += "squad.limit0";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_UNIT);

    s = prefix;
    s += "squad.limit1";
    Mutate_FloatType(mreg, s.CStr(), MUTATION_TYPE_UNIT);
}

void NeuralForce_Mutate(MBRegistry *mreg, float rate, const char *prefix)
{
    MBString s;
    NeuralForceDesc desc;
    MutationFloatParams vf;

    NeuralForce_Load(mreg, &desc, prefix);

    Mutate_DefaultFloatParams(&vf, MUTATION_TYPE_RADIUS);
    s = prefix;
    s += "radius";
    vf.key = s.CStr();
    Mutate_Float(mreg, &vf, 1);

    Mutate_DefaultFloatParams(&vf, MUTATION_TYPE_RADIUS);
    s = prefix;
    s += "range";
    vf.key = s.CStr();
    Mutate_Float(mreg, &vf, 1);

    s = prefix;
    s += "forceType";
    if (Random_Flip(rate)) {
        NeuralForceType ft = NeuralForce_Random();
        const char *v = NeuralForce_ToString(ft);;
        MBRegistry_PutCopy(mreg, s.CStr(), v);
        desc.forceType = ft;
    }

    s = prefix;
    s += "index";
    Mutate_Index(mreg, s.CStr(), rate);

    MutationBoolParams bf;
    const char *strs[] = {
        "useTangent", "useBase", "filterForward", "filterBackward",
        "filterAdvance", "filterRetreat", "filterRange",
    };

    for (uint i = 0; i < ARRAYSIZE(strs); i++) {
        s = prefix;
        s += strs[i];
        bf.key = s.CStr();
        bf.flipRate = rate;
        Mutate_Bool(mreg, &bf, 1);
    }
}

void NeuralSquad_Mutate(MBRegistry *mreg,
                        float rate, const char *prefix)
{
    MBString s;

    s = prefix;
    s += "seed";
    Mutate_Index(mreg, s.CStr(), rate);

    s = prefix;
    s += "numSquads";
    Mutate_Index(mreg, s.CStr(), rate);

    s = prefix;
    s += "squadType";
    if (Random_Flip(rate)) {
        NeuralSquadType st = NeuralSquad_Random();
        const char *v = NeuralSquad_ToString(st);
        MBRegistry_PutCopy(mreg, s.CStr(), v);
    }
}

void NeuralOutput_Mutate(MBRegistry *mreg,
                         float rate, NeuralNetType nnType,
                         const char *prefix)
{
    MBString s;

    s = prefix;
    NeuralValue_Mutate(mreg, rate, TRUE, nnType, s.CStr());

    if (NN_USE_CONDITIONS) {
        s = prefix;
        s += "condition.";
        NeuralCondition_Mutate(mreg, rate, nnType, s.CStr());
    }

    s = prefix;
    s += "combiner.combinerType";
    if (Random_Flip(rate)) {
        NeuralCombinerType ct = NeuralCombiner_Random();
        const char *v = NeuralCombiner_ToString(ct);
        MBRegistry_PutCopy(mreg, s.CStr(), v);
    }
}

void NeuralValue_Mutate(MBRegistry *mreg,
                        float rate, bool isOutput, NeuralNetType nnType,
                        const char *prefix)
{
    NeuralValueDesc desc;
    MBString s;

    NeuralValue_Load(mreg, &desc, prefix);

    s = prefix;
    s += "valueType";

    if (isOutput) {
        if (nnType == NN_TYPE_FORCES) {
            desc.valueType = NEURAL_VALUE_FORCE;
        } else {
            ASSERT(nnType == NN_TYPE_SCALARS);
            desc.valueType = NEURAL_VALUE_SCALAR;
        }
    } else if (Random_Flip(rate)) {
        desc.valueType = NeuralValue_Random();
    }
    const char *v = NeuralValue_ToString(desc.valueType);
    MBRegistry_PutCopy(mreg, s.CStr(), v);

    if (desc.valueType == NEURAL_VALUE_FORCE) {
        NeuralForce_Mutate(mreg, rate, prefix);
    } else if (desc.valueType == NEURAL_VALUE_CROWD) {
        MutationFloatParams vf;

        Mutate_DefaultFloatParams(&vf, MUTATION_TYPE_RADIUS);
        s = prefix;
        s += "radius";
        vf.key = s.CStr();
        Mutate_Float(mreg, &vf, 1);

        s = prefix;
        s += "crowdType";
        if (Random_Flip(rate)) {
            NeuralCrowdType ct = NeuralCrowd_Random();
            const char *v = NeuralCrowd_ToString(ct);
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            desc.crowdDesc.crowdType = ct;
        }
    } else if (desc.valueType == NEURAL_VALUE_SQUAD) {
        NeuralSquad_Mutate(mreg, rate, prefix);
    } else if (desc.valueType == NEURAL_VALUE_TICK) {
        MutationFloatParams vf;

        Mutate_DefaultFloatParams(&vf, MUTATION_TYPE_PERIOD);
        s = prefix;
        s += "frequency";
        vf.key = s.CStr();
        Mutate_Float(mreg, &vf, 1);

        s = prefix;
        s += "waveType";
        if (Random_Flip(rate)) {
            NeuralWaveType wi = NeuralWave_Random();
            const char *v = NeuralWave_ToString(wi);
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            desc.tickDesc.waveType = wi;
        }
    } else if (desc.valueType == NEURAL_VALUE_SCALAR) {
        /*
         * scalarID's on outputs are ignored.
         */
        if (!isOutput) {
            s = prefix;
            s += "scalarID";
            Mutate_Index(mreg, s.CStr(), rate);
        }
    } else if (desc.valueType == NEURAL_VALUE_ZERO ||
               desc.valueType == NEURAL_VALUE_FRIEND_SHIPS ||
               desc.valueType == NEURAL_VALUE_FRIEND_MISSILES ||
               desc.valueType == NEURAL_VALUE_ENEMY_SHIPS ||
               desc.valueType == NEURAL_VALUE_ENEMY_MISSILES ||
               desc.valueType == NEURAL_VALUE_MOBID ||
               desc.valueType == NEURAL_VALUE_CREDITS ||
               desc.valueType == NEURAL_VALUE_RANDOM_UNIT) {
        /*
         * No parameters to mutate.
         */
    } else {
        PANIC("Unknown NeuralValueType: %s (%d)\n",
              NeuralValue_ToString(desc.valueType), desc.valueType);
    }
}

static bool NeuralForceGetFlockFocus(AIContext *nc,
                                     Mob *self, NeuralForceDesc *desc,
                                     FPoint *focusPoint)
{
    MobFilter f;
    NeuralForceType forceType = desc->forceType;
    bool useFriends;
    FPoint vel, pos;
    const uint alignF =        (1 << 0);
    const uint cohereF =       (1 << 1);
    const uint enemyF =        (1 << 2);
    const uint advanceF =      (1 << 3);
    const uint forwardF =      (1 << 4);
    const uint backwardF =     (1 << 5);
    const uint retreatF =      (1 << 6);
    const uint enemyMissileF = (1 << 7);

    uint flags = 0;

    MobFilter_Init(&f);
    MobFilter_UseRange(&f, &self->pos, desc->radius);

    switch (forceType) {
        case NEURAL_FORCE_ALIGN2:
            flags = alignF;
            break;
        case NEURAL_FORCE_FORWARD_ALIGN:
            flags = alignF | forwardF;
            break;
        case NEURAL_FORCE_BACKWARD_ALIGN:
            flags = alignF | backwardF;
            break;
        case NEURAL_FORCE_ADVANCE_ALIGN:
            flags = alignF | advanceF;
            break;
        case NEURAL_FORCE_RETREAT_ALIGN:
            flags = alignF | retreatF;
            break;
        case NEURAL_FORCE_COHERE:
            flags = cohereF;
            break;
        case NEURAL_FORCE_FORWARD_COHERE:
            flags = cohereF | forwardF;
            break;
        case NEURAL_FORCE_BACKWARD_COHERE:
            flags = cohereF | backwardF;
            break;
        case NEURAL_FORCE_ADVANCE_COHERE:
            flags = cohereF | advanceF;
            break;
        case NEURAL_FORCE_RETREAT_COHERE:
            flags = cohereF | retreatF;
            break;
        case NEURAL_FORCE_ENEMY_ALIGN:
            flags = alignF | enemyF;
            break;
        case NEURAL_FORCE_FORWARD_ENEMY_ALIGN:
            flags = alignF | enemyF | forwardF;
            break;
        case NEURAL_FORCE_BACKWARD_ENEMY_ALIGN:
            flags = alignF | enemyF | backwardF;
            break;
        case NEURAL_FORCE_ADVANCE_ENEMY_ALIGN:
            flags = alignF | enemyF | advanceF;
            break;
        case NEURAL_FORCE_RETREAT_ENEMY_ALIGN:
            flags = alignF | enemyF | retreatF;
            break;
        case NEURAL_FORCE_ENEMY_COHERE2:
            flags = cohereF | enemyF;
            break;
        case NEURAL_FORCE_FORWARD_ENEMY_COHERE:
            flags = cohereF | enemyF | forwardF;
            break;
        case NEURAL_FORCE_BACKWARD_ENEMY_COHERE:
            flags = cohereF | enemyF | backwardF;
            break;
        case NEURAL_FORCE_ADVANCE_ENEMY_COHERE:
            flags = cohereF | enemyF | advanceF;
            break;
        case NEURAL_FORCE_RETREAT_ENEMY_COHERE:
            flags = cohereF | enemyF | retreatF;
            break;
        case NEURAL_FORCE_ENEMY_MISSILE_ALIGN:
            flags = alignF | enemyMissileF;
            break;
        case NEURAL_FORCE_FORWARD_ENEMY_MISSILE_ALIGN:
            flags = alignF | enemyMissileF | forwardF;
            break;
        case NEURAL_FORCE_BACKWARD_ENEMY_MISSILE_ALIGN:
            flags = alignF | enemyMissileF | backwardF;
            break;
        case NEURAL_FORCE_ADVANCE_ENEMY_MISSILE_ALIGN:
            flags = alignF | enemyMissileF | advanceF;
            break;
        case NEURAL_FORCE_RETREAT_ENEMY_MISSILE_ALIGN:
            flags = alignF | enemyMissileF | retreatF;
            break;
        case NEURAL_FORCE_ENEMY_MISSILE_COHERE:
            flags = cohereF | enemyMissileF;
            break;
        case NEURAL_FORCE_FORWARD_ENEMY_MISSILE_COHERE:
            flags = cohereF | enemyMissileF | forwardF;
            break;
        case NEURAL_FORCE_BACKWARD_ENEMY_MISSILE_COHERE:
            flags = cohereF | enemyMissileF | backwardF;
            break;
        case NEURAL_FORCE_ADVANCE_ENEMY_MISSILE_COHERE:
            flags = cohereF | enemyMissileF | advanceF;
            break;
        case NEURAL_FORCE_RETREAT_ENEMY_MISSILE_COHERE:
            flags = cohereF | enemyMissileF | retreatF;
            break;
        default:
            NOT_IMPLEMENTED();
    }

    if ((flags & enemyMissileF) != 0) {
        ASSERT((flags & enemyF) == 0);
        MobFilter_UseType(&f, MOB_FLAG_MISSILE);
        useFriends = FALSE;
    } else if ((flags & enemyF) != 0) {
        MobFilter_UseType(&f, MOB_FLAG_SHIP);
        useFriends = FALSE;
    } else {
        MobFilter_UseType(&f, MOB_FLAG_FIGHTER);
        useFriends = TRUE;
    }


    if ((flags & forwardF) != 0 || (flags & backwardF) != 0) {
        ASSERT((flags & forwardF) == 0 ||
               (flags & backwardF) == 0);
        ASSERT((flags & advanceF) == 0);
        ASSERT((flags & retreatF) == 0);
        FRPoint dir;
        NeuralForceGetHeading(nc, self, &dir);
        MobFilter_UseDirR(&f, &self->pos, &dir, (flags & forwardF) != 0);
    }

    if ((flags & advanceF) != 0 || (flags & retreatF) != 0) {
        Mob *base = nc->sg->friendBase();
        ASSERT((flags & advanceF) == 0 ||
               (flags & retreatF) == 0);
        ASSERT((flags & forwardF) == 0);
        ASSERT((flags & backwardF) == 0);

        if (base == NULL) {
            return FALSE;
        }

        FPoint dir;
        FPoint_Subtract(&self->pos, &base->pos, &dir);
        MobFilter_UseDirP(&f, &self->pos, &dir, (flags & advanceF) != 0);
    }

    if (!nc->sg->avgFlock(&vel, &pos, &f, useFriends)) {
        return FALSE;
    }

    if ((flags & alignF) != 0) {
        ASSERT((flags & cohereF) == 0);
        if (vel.x >= MICRON || vel.y >= MICRON) {
            vel.x += self->pos.x;
            vel.y += self->pos.y;
            *focusPoint = vel;
            return TRUE;
        }
        return FALSE;
    } else {
        ASSERT((flags & cohereF) != 0);
        *focusPoint = pos;
        return TRUE;
    }

    NOT_REACHED();
}


static bool NeuralForceGetSeparateFocus(AIContext *nc,
                                        Mob *self, NeuralForceDesc *desc,
                                        FPoint *focusPoint)
{
    FPoint force;
    uint x = 0;
    MobSet::MobIt mit = nc->sg->friendsIterator(MOB_FLAG_FIGHTER);

    MobFilter f;
    MobFilter_Init(&f);
    MobFilter_UseRange(&f, &self->pos, desc->radius);

    if (desc->forceType == NEURAL_FORCE_FORWARD_SEPARATE ||
        desc->forceType == NEURAL_FORCE_BACKWARD_SEPARATE) {
        bool forward = desc->forceType == NEURAL_FORCE_FORWARD_SEPARATE ?
                        TRUE : FALSE;
        FRPoint dir;
        NeuralForceGetHeading(nc, self, &dir);
        MobFilter_UseDirR(&f, &self->pos, &dir, forward);
    } else if (desc->forceType == NEURAL_FORCE_ADVANCE_SEPARATE ||
               desc->forceType == NEURAL_FORCE_RETREAT_SEPARATE) {
        Mob *base = nc->sg->friendBase();
        bool forward = desc->forceType == NEURAL_FORCE_ADVANCE_SEPARATE ?
                       TRUE : FALSE;
        FPoint dir;

        if (base == NULL) {
            return FALSE;
        }

        FPoint_Subtract(&self->pos, &base->pos, &dir);
        MobFilter_UseDirP(&f, &self->pos, &dir, forward);
    } else {
        ASSERT(desc->forceType == NEURAL_FORCE_SEPARATE);
    }

    FPoint_Zero(&force);

    if (!MobFilter_IsTriviallyEmpty(&f)) {
        Mob *ma[512];
        uint mn = 0;
        mit.nextBatch(ma, &mn, ARRAYSIZE(ma));
        MobFilter_Batch(ma, &mn, &f);

        while (mn > 0) {
            mn--;
            Mob *m = ma[mn];
            ASSERT(m != NULL);

            if (m->mobid != self->mobid) {
                NeuralForceGetRepulseFocus(nc, &self->pos, &m->pos, &force);
                x++;
            }
        }
    }

    FPoint_Add(&force, &self->pos, focusPoint);
    return x > 0;
}

static void NeuralForceGetRepulseFocus(AIContext *nc,
                                       const FPoint *selfPos,
                                       const FPoint *pos, FPoint *force)
{
    FRPoint f;
    FPoint p;
    float radiusSquared;

    ASSERT(pos != NULL);
    ASSERT(selfPos != NULL);
    ASSERT(force != NULL);

    p.x = selfPos->x - pos->x;
    p.y = selfPos->y - pos->y;

    radiusSquared = (p.x * p.x) + (p.y * p.y);

    if (radiusSquared < MICRON * MICRON) {
        /*
         * Avoid 1/0 => NAN, and then randomize the direction when
         * the point is more or less directly on top of us.
         */
        f.radius = 1.0f / (MICRON * MICRON);
        f.theta = RandomState_Float(nc->rs, 0, M_PI * 2.0f);
    } else {
        ASSERT(fabs(FPoint_ToRadius(&p) - sqrtf(radiusSquared)) <= MICRON);
        f.radius = 1.0f / (radiusSquared);
        f.theta = FPoint_ToTheta(&p);
    }

    FRPoint_ToFPoint(&f, NULL, &p);
    FPoint_Add(force, &p, force);
}

static void NeuralForceGetEdgeFocus(AIContext *nc,
                                    Mob *self, NeuralForceDesc *desc,
                                    FPoint *focusPoint)
{
    FPoint edgePoint;
    FPoint force;

    FPoint_Zero(&force);

    /*
     * Left Edge
     */
    edgePoint = self->pos;
    edgePoint.x = 0.0f;
    NeuralForceGetRepulseFocus(nc, &self->pos, &edgePoint, &force);

    /*
     * Right Edge
     */
    edgePoint = self->pos;
    edgePoint.x = nc->ai->bp.width;
    NeuralForceGetRepulseFocus(nc, &self->pos, &edgePoint, &force);

    /*
     * Top Edge
     */
    edgePoint = self->pos;
    edgePoint.y = 0.0f;
    NeuralForceGetRepulseFocus(nc, &self->pos, &edgePoint, &force);

    /*
     * Bottom edge
     */
    edgePoint = self->pos;
    edgePoint.y = nc->ai->bp.height;
    NeuralForceGetRepulseFocus(nc, &self->pos, &edgePoint, &force);

    FPoint_Add(&force, &self->pos, focusPoint);
}


static bool NeuralForceGetCloseEdgeFocus(AIContext *nc,
                                         Mob *self, NeuralForceDesc *desc,
                                         FPoint *focusPoint,
                                         bool nearest)
{
    FPoint edgePoints[4];
    float edgeDistances[4];

    /*
     * Left Edge
     */
    edgePoints[0] = self->pos;
    edgePoints[0].x = 0.0f;
    edgeDistances[0] = fabs(self->pos.x);

    /*
     * Right Edge
     */
    edgePoints[1] = self->pos;
    edgePoints[1].x = nc->ai->bp.width;
    edgeDistances[1] = FPoint_Distance(&self->pos, &edgePoints[1]);

    /*
     * Top Edge
     */
    edgePoints[2] = self->pos;
    edgePoints[2].y = 0.0f;
    edgeDistances[2] = fabs(self->pos.y);

    /*
     * Bottom edge
     */
    edgePoints[3] = self->pos;
    edgePoints[3].y = nc->ai->bp.height;
    edgeDistances[3] = FPoint_Distance(&self->pos, &edgePoints[3]);

    uint minI = ARRAYSIZE(edgePoints);
    for (uint i = 0; i < ARRAYSIZE(edgePoints); i++) {
        if (edgeDistances[i] <= desc->radius) {
            if (minI >= ARRAYSIZE(edgePoints) ||
                (nearest && edgeDistances[i] < edgeDistances[minI]) ||
                (!nearest && edgeDistances[i] > edgeDistances[minI])) {
                minI = i;
            }
        }
    }

    if (minI < ARRAYSIZE(edgePoints)) {
        *focusPoint = edgePoints[minI];
        return TRUE;
    } else {
        return FALSE;
    }
}


void NeuralForceGetCornersFocus(AIContext *nc,
                                Mob *self, NeuralForceDesc *desc,
                                FPoint *focusPoint) {
    FPoint cornerPoint;
    FPoint force;

    FPoint_Zero(&force);

    cornerPoint.x = 0.0f;
    cornerPoint.y = 0.0f;
    NeuralForceGetRepulseFocus(nc, &self->pos, &cornerPoint, &force);

    cornerPoint.x = nc->ai->bp.width;
    cornerPoint.y = 0.0f;
    NeuralForceGetRepulseFocus(nc, &self->pos, &cornerPoint, &force);

    cornerPoint.x = 0.0f;
    cornerPoint.y = nc->ai->bp.height;
    NeuralForceGetRepulseFocus(nc, &self->pos, &cornerPoint, &force);

    cornerPoint.x = nc->ai->bp.width;
    cornerPoint.y = nc->ai->bp.height;
    NeuralForceGetRepulseFocus(nc, &self->pos, &cornerPoint, &force);

    FPoint_Add(&force, &self->pos, focusPoint);
}

bool NeuralForceGetCloseCornerFocus(AIContext *nc,
                                    Mob *self, NeuralForceDesc *desc,
                                    FPoint *focusPoint,
                                    bool nearest) {
    FPoint cornerPoints[4];
    float cornerDistances[4];

    cornerPoints[0].x = 0.0f;
    cornerPoints[0].y = 0.0f;
    cornerDistances[0] = FPoint_Distance(&self->pos, &cornerPoints[0]);

    cornerPoints[1].x = nc->ai->bp.width;
    cornerPoints[1].y = 0.0f;
    cornerDistances[1] = FPoint_Distance(&self->pos, &cornerPoints[1]);

    cornerPoints[2].x = 0.0f;
    cornerPoints[2].y = nc->ai->bp.height;
    cornerDistances[2] = FPoint_Distance(&self->pos, &cornerPoints[2]);

    cornerPoints[3].x = nc->ai->bp.width;
    cornerPoints[3].y = nc->ai->bp.height;
    cornerDistances[3] = FPoint_Distance(&self->pos, &cornerPoints[3]);

    uint minI = ARRAYSIZE(cornerPoints);
    for (uint i = 0; i < ARRAYSIZE(cornerPoints); i++) {
        if (cornerDistances[i] <= desc->radius) {
            if (minI >= ARRAYSIZE(cornerPoints) ||
                (nearest && cornerDistances[i] < cornerDistances[minI]) ||
                (!nearest && cornerDistances[i] > cornerDistances[minI])) {
                minI = i;
            }
        }
    }

    if (minI < ARRAYSIZE(cornerPoints)) {
        *focusPoint = cornerPoints[minI];
        return TRUE;
    } else {
        return FALSE;
    }
}

static bool
NeuralForceGetBaseControlLimitFocus(AIContext *nc,
                                    FPoint *focusPoint)
{
    Mob *nearestEnemy;
    Mob *farthestFriend;

    Mob *base = nc->sg->friendBase();
    if (base == NULL) {
        return FALSE;
    }

    nearestEnemy = nc->sg->findClosestTarget(&base->pos, MOB_FLAG_SHIP);
    farthestFriend = nc->sg->findFarthestFriend(&base->pos, MOB_FLAG_FIGHTER);

    if (nearestEnemy == NULL) {
        return NeuralForceGetFocusMobPosHelper(farthestFriend, focusPoint);
    } else if (farthestFriend == NULL) {
        return NeuralForceGetFocusMobPosHelper(nearestEnemy, focusPoint);
    }

    if (FPoint_DistanceSquared(&base->pos, &nearestEnemy->pos) <=
        FPoint_DistanceSquared(&base->pos,&farthestFriend->pos)) {
        return NeuralForceGetFocusMobPosHelper(nearestEnemy, focusPoint);
    }
    return NeuralForceGetFocusMobPosHelper(farthestFriend, focusPoint);
}

static
void NeuralForceGetHeading(AIContext *nc,
                           Mob *mob,
                           FRPoint *heading)
{
    FRPoint rPos;

    ASSERT(nc != NULL);
    ASSERT(mob != NULL);
    ASSERT(heading != NULL);

    if (FPoint_DistanceSquared(&mob->pos, &mob->lastPos) <= MICRON * MICRON) {
        rPos.theta = RandomState_Float(nc->rs, 0, M_PI * 2.0f);
    } else {
        rPos.theta = FPoint_ToFRPointTheta(&mob->pos, &mob->lastPos);
    }

    rPos.radius = 1.0f;
    FRPoint_SetSpeed(&rPos, 1.0f);

    *heading = rPos;
}

/*
 * NeuralForce_GetFocus --
 *     Get the focus point associated with the specified force.
 *     Returns TRUE if the force is valid.
 *     Returns FALSE if the force is invalid.
 */
bool NeuralForce_GetFocus(AIContext *nc,
                          Mob *mob,
                          NeuralForceDesc *desc,
                          FPoint *focusPoint)
{
    if (desc->useBase) {
        mob = nc->sg->friendBaseShadow();
    }

    switch(desc->forceType) {
        case NEURAL_FORCE_VOID:
        case NEURAL_FORCE_ZERO:
            return FALSE;

        case NEURAL_FORCE_HEADING: {
            FRPoint rPos;
            NeuralForceGetHeading(nc, mob, &rPos);
            FRPoint_ToFPoint(&rPos, &mob->pos, focusPoint);
            return TRUE;
        }
        case NEURAL_FORCE_ALIGN: {
            FPoint avgVel;
            nc->sg->friendAvgVel(&avgVel, &mob->pos, desc->radius,
                                 MOB_FLAG_FIGHTER);
            avgVel.x += mob->pos.x;
            avgVel.y += mob->pos.y;
            *focusPoint = avgVel;
            return TRUE;
        }
        case NEURAL_FORCE_ALIGN_BIAS_CENTER: {
            FPoint avgVel;
            bool success;
            success = nc->sg->friendAvgVel(&avgVel, &mob->pos, desc->radius,
                                           MOB_FLAG_FIGHTER);
            if (!success || (avgVel.x < MICRON && avgVel.y < MICRON)) {
                focusPoint->x = nc->ai->bp.width / 2;
                focusPoint->y = nc->ai->bp.height / 2;
            } else {
                avgVel.x += mob->pos.x;
                avgVel.y += mob->pos.y;
                *focusPoint = avgVel;
            }
            return TRUE;
        }
        case NEURAL_FORCE_ALIGN2:
        case NEURAL_FORCE_FORWARD_ALIGN:
        case NEURAL_FORCE_BACKWARD_ALIGN:
        case NEURAL_FORCE_ADVANCE_ALIGN:
        case NEURAL_FORCE_RETREAT_ALIGN:
        case NEURAL_FORCE_COHERE:
        case NEURAL_FORCE_FORWARD_COHERE:
        case NEURAL_FORCE_BACKWARD_COHERE:
        case NEURAL_FORCE_ADVANCE_COHERE:
        case NEURAL_FORCE_RETREAT_COHERE:
        case NEURAL_FORCE_ENEMY_ALIGN:
        case NEURAL_FORCE_FORWARD_ENEMY_ALIGN:
        case NEURAL_FORCE_BACKWARD_ENEMY_ALIGN:
        case NEURAL_FORCE_ADVANCE_ENEMY_ALIGN:
        case NEURAL_FORCE_RETREAT_ENEMY_ALIGN:
        case NEURAL_FORCE_ENEMY_COHERE2:
        case NEURAL_FORCE_FORWARD_ENEMY_COHERE:
        case NEURAL_FORCE_BACKWARD_ENEMY_COHERE:
        case NEURAL_FORCE_ADVANCE_ENEMY_COHERE:
        case NEURAL_FORCE_RETREAT_ENEMY_COHERE:
        case NEURAL_FORCE_ENEMY_MISSILE_COHERE:
        case NEURAL_FORCE_FORWARD_ENEMY_MISSILE_COHERE:
        case NEURAL_FORCE_BACKWARD_ENEMY_MISSILE_COHERE:
        case NEURAL_FORCE_ADVANCE_ENEMY_MISSILE_COHERE:
        case NEURAL_FORCE_RETREAT_ENEMY_MISSILE_COHERE:
        case NEURAL_FORCE_ENEMY_MISSILE_ALIGN:
        case NEURAL_FORCE_FORWARD_ENEMY_MISSILE_ALIGN:
        case NEURAL_FORCE_BACKWARD_ENEMY_MISSILE_ALIGN:
        case NEURAL_FORCE_ADVANCE_ENEMY_MISSILE_ALIGN:
        case NEURAL_FORCE_RETREAT_ENEMY_MISSILE_ALIGN: {
            return NeuralForceGetFlockFocus(nc, mob, desc, focusPoint);
        }
        case NEURAL_FORCE_BROKEN_COHERE: {
            MobSet::MobIt mit = nc->sg->friendsIterator(MOB_FLAG_FIGHTER);
            FPoint lAvgPos;
            float flockRadius = desc->radius;

            lAvgPos.x = 0.0f;
            lAvgPos.y = 0.0f;

            while (mit.hasNext()) {
                Mob *f = mit.next();
                ASSERT(f != NULL);

                if (FPoint_Distance(&f->pos, &mob->pos) <= flockRadius) {
                    /*
                    * The broken version just sums the positions and doesn't
                    * properly average them.
                    */
                    lAvgPos.x += f->pos.x;
                    lAvgPos.y += f->pos.y;
                }
            }

            *focusPoint = lAvgPos;
            return TRUE;
        }
        case NEURAL_FORCE_ENEMY_COHERE: {
            FPoint avgPos;
            nc->sg->targetAvgPos(&avgPos, &mob->pos, desc->radius,
                                 MOB_FLAG_SHIP);
            *focusPoint = avgPos;
            return TRUE;
        }
        case NEURAL_FORCE_SEPARATE:
        case NEURAL_FORCE_FORWARD_SEPARATE:
        case NEURAL_FORCE_BACKWARD_SEPARATE:
        case NEURAL_FORCE_ADVANCE_SEPARATE:
        case NEURAL_FORCE_RETREAT_SEPARATE:
            return NeuralForceGetSeparateFocus(nc, mob, desc, focusPoint);

        case NEURAL_FORCE_NEAREST_FRIEND: {
            Mob *m = nc->sg->findClosestFriend(mob, MOB_FLAG_FIGHTER);
            return NeuralForceGetFocusMobPosHelper(m, focusPoint);
        }
        case NEURAL_FORCE_NEAREST_FRIEND_MISSILE: {
            Mob *m = nc->sg->findClosestFriend(mob, MOB_FLAG_MISSILE);
            return NeuralForceGetFocusMobPosHelper(m, focusPoint);
        }

        case NEURAL_FORCE_EDGES: {
            NeuralForceGetEdgeFocus(nc, mob, desc, focusPoint);
            return TRUE;
        }
        case NEURAL_FORCE_NEAREST_EDGE: {
            return NeuralForceGetCloseEdgeFocus(nc, mob, desc, focusPoint, TRUE);
        }
        case NEURAL_FORCE_FARTHEST_EDGE: {
            return NeuralForceGetCloseEdgeFocus(nc, mob, desc, focusPoint, FALSE);
        }
        case NEURAL_FORCE_CORNERS: {
            NeuralForceGetCornersFocus(nc, mob, desc, focusPoint);
            return TRUE;
        }
        case NEURAL_FORCE_NEAREST_CORNER: {
            return NeuralForceGetCloseCornerFocus(nc, mob, desc, focusPoint, TRUE);
        }
        case NEURAL_FORCE_FARTHEST_CORNER: {
            return NeuralForceGetCloseCornerFocus(nc, mob, desc, focusPoint, FALSE);
        }
        case NEURAL_FORCE_CENTER: {
            focusPoint->x = nc->ai->bp.width / 2;
            focusPoint->y = nc->ai->bp.height / 2;
            return TRUE;
        }
        case NEURAL_FORCE_BASE:
            return NeuralForceGetFocusMobPosHelper(nc->sg->friendBase(), focusPoint);
        case NEURAL_FORCE_BASE_LAX:
            return NeuralForceGetFocusMobPosHelper(nc->sg->friendBaseShadow(), focusPoint);
        case NEURAL_FORCE_BASE_MIRROR_LAX: {
            FPoint *pos = nc->sg->friendBaseShadowPos();
            focusPoint->x = nc->ai->bp.width - pos->x;
            focusPoint->y = nc->ai->bp.height - pos->y;
            return TRUE;
        }

        case NEURAL_FORCE_BASE_DEFENSE: {
            Mob *base = nc->sg->friendBase();
            if (base != NULL) {
                Mob *enemy = nc->sg->findClosestTarget(&base->pos, MOB_FLAG_SHIP);
                if (enemy != NULL) {
                    *focusPoint = enemy->pos;
                    return TRUE;
                }
            }
            return FALSE;
        }
        case NEURAL_FORCE_BASE_FARTHEST_FRIEND: {
            Mob *base = nc->sg->friendBase();
            if (base != NULL) {
                Mob *friendS = nc->sg->findFarthestFriend(&base->pos, MOB_FLAG_FIGHTER);
                return NeuralForceGetFocusMobPosHelper(friendS, focusPoint);
            }
            return FALSE;
        }
        case NEURAL_FORCE_BASE_CONTROL_LIMIT:
            return NeuralForceGetBaseControlLimitFocus(nc, focusPoint);
        case NEURAL_FORCE_BASE_CONTROL_SHELL: {
            FRPoint rPoint;
            Mob *base = nc->sg->friendBase();
            float limitDistance;

            if (base == NULL) {
                return FALSE;
            }

            if (!NeuralForceGetBaseControlLimitFocus(nc, focusPoint)) {
                return FALSE;
            }

            limitDistance = FPoint_Distance(focusPoint, &base->pos);

            FPoint_ToFRPointWithRadius(&mob->pos, &base->pos, limitDistance,
                                       &rPoint);
            FRPoint_ToFPoint(&rPoint, &base->pos, focusPoint);
            return TRUE;
        }
        case NEURAL_FORCE_BASE_SHELL: {
            FRPoint rPoint;
            if (!NeuralForceGetFocusMobPosHelper(nc->sg->friendBase(),
                                                 focusPoint)) {
                return FALSE;
            }
            FPoint_ToFRPointWithRadius(&mob->pos, focusPoint, desc->radius,
                                       &rPoint);
            FRPoint_ToFPoint(&rPoint, focusPoint, focusPoint);
            return TRUE;
        }

        case NEURAL_FORCE_ENEMY: {
            Mob *m = nc->sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);
            return NeuralForceGetFocusMobPosHelper(m, focusPoint);
        }
        case NEURAL_FORCE_ENEMY_MISSILE: {
            Mob *m = nc->sg->findClosestTarget(&mob->pos, MOB_FLAG_MISSILE);
            return NeuralForceGetFocusMobPosHelper(m, focusPoint);
        }

        case NEURAL_FORCE_ENEMY_BASE:
            return NeuralForceGetFocusMobPosHelper(nc->sg->enemyBase(), focusPoint);

        case NEURAL_FORCE_ENEMY_BASE_GUESS: {
            if (!nc->sg->hasEnemyBase() && nc->sg->hasEnemyBaseGuess()) {
                *focusPoint = nc->sg->getEnemyBaseGuess();
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_ENEMY_BASE_GUESS_LAX: {
            if (nc->sg->hasEnemyBaseGuess()) {
                *focusPoint = nc->sg->getEnemyBaseGuess();
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_MIDWAY: {
            if (nc->sg->hasMidway()) {
                *focusPoint = nc->sg->getMidway();
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_MIDWAY_GUESS: {
            if (!nc->sg->hasMidway() && nc->sg->hasMidwayGuess()) {
                *focusPoint = nc->sg->getMidwayGuess();
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_MIDWAY_GUESS_LAX: {
            if (nc->sg->hasMidwayGuess()) {
                *focusPoint = nc->sg->getMidwayGuess();
                return TRUE;
            }
            return FALSE;
        }

        case NEURAL_FORCE_CORES: {
            Mob *m = nc->sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);
            return NeuralForceGetFocusMobPosHelper(m, focusPoint);
        }

        case NEURAL_FORCE_LOCUS:
        case NEURAL_FORCE_NEXT_LOCUS: {
            /*
             * Locus forces don't formally have a focus, they're handled
             * by the NeuralNet in NeuralNet::getFocus.
             */
            return FALSE;
        }

        case NEURAL_FORCE_UNEXPLORED: {
            if (nc->sg->hasUnexploredFocus()) {
                *focusPoint = nc->sg->getUnexploredFocus();
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_CIRCULAR: {
            float speed = MOB_FIGHTER_SPEED;
            ASSERT(mob->type == MOB_TYPE_BASE || mob->type == MOB_TYPE_FIGHTER);
            float period = desc->radius / speed;
            float t = ((float)nc->ai->tick) / period;
            focusPoint->x = mob->pos.x + cosf(t);
            focusPoint->y = mob->pos.y + sinf(t);
            return TRUE;
        }
        case NEURAL_FORCE_MOB_ROW: {
            float fmobid = Random_UnitFloatFromSeed(mob->mobid);
            focusPoint->x = mob->pos.x;
            focusPoint->y = fmobid * nc->ai->bp.height;
            return TRUE;
        }
        case NEURAL_FORCE_MOB_COLUMN: {
            float fmobid = Random_UnitFloatFromSeed(mob->mobid);
            focusPoint->x = fmobid * nc->ai->bp.width;
            focusPoint->y = mob->pos.y;
            return TRUE;
        }
        case NEURAL_FORCE_MOB_SPOT: {
            uint64 mobid = mob->mobid;
            uint64 radix1 = 0x1234567812345678;
            uint64 radix2 = 0x9876543298765432;
            float fmobid1 = Random_UnitFloatFromSeed(mobid ^ radix1);
            float fmobid2 = Random_UnitFloatFromSeed(mobid ^ radix2);
            focusPoint->x = fmobid1 * nc->ai->bp.width;
            focusPoint->y = fmobid2 * nc->ai->bp.height;
            return TRUE;
        }
        case NEURAL_FORCE_MOB_BASE_SECTOR:
        case NEURAL_FORCE_MOB_CENTER_SECTOR: {
            FRPoint rfocus;
            FPoint center;
            FPoint *pos;

            if (desc->forceType == NEURAL_FORCE_MOB_CENTER_SECTOR) {
                center.x = nc->ai->bp.width / 2.0f;
                center.y = nc->ai->bp.height / 2.0f;
                pos = &center;
            } else {
                ASSERT(desc->forceType == NEURAL_FORCE_MOB_BASE_SECTOR);
                pos = nc->sg->friendBaseShadowPos();
            }
            float fmobid = Random_UnitFloatFromSeed(mob->mobid);
            rfocus.radius = FPoint_ToFRPointRadius(&mob->pos, pos);
            rfocus.theta = fmobid * 2.0f * M_PI;
            FRPoint_ToFPoint(&rfocus, pos, focusPoint);

            if (focusPoint->x >= 0.0f && focusPoint->x <= nc->ai->bp.width &&
                focusPoint->y >= 0.0f && focusPoint->y <= nc->ai->bp.height) {
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_MOB_BASE_SHELL:
        case NEURAL_FORCE_MOB_CENTER_SHELL: {
            FRPoint rfocus;
            FPoint center;
            FPoint *pos;
            float d = sqrtf(nc->ai->bp.width * nc->ai->bp.width +
                            nc->ai->bp.height * nc->ai->bp.height);

            if (desc->forceType == NEURAL_FORCE_MOB_CENTER_SHELL) {
                center.x = nc->ai->bp.width / 2.0f;
                center.y = nc->ai->bp.height / 2.0f;
                pos = &center;
                d /= 2.0f;
            } else {
                ASSERT(desc->forceType == NEURAL_FORCE_MOB_BASE_SHELL);
                pos = nc->sg->friendBaseShadowPos();
            }
            float fmobid = Random_UnitFloatFromSeed(mob->mobid);

            rfocus.radius = fmobid * d;
            rfocus.theta = FPoint_ToFRPointTheta(&mob->pos, pos);
            FRPoint_ToFPoint(&rfocus, pos, focusPoint);

            if (focusPoint->x >= 0.0f && focusPoint->x <= nc->ai->bp.width &&
                focusPoint->y >= 0.0f && focusPoint->y <= nc->ai->bp.height) {
                return TRUE;
            }
            return FALSE;
        }

        case NEURAL_FORCE_LAST_TARGET_SHADOW: {
            Mob *m = nc->sg->farthestTargetShadow();
            return NeuralForceGetFocusMobPosHelper(m, focusPoint);
        }

        case NEURAL_FORCE_GENE_MIDWAY: {
            return NeuralForceGeneMidway(nc, mob, desc, focusPoint);
        }
        case NEURAL_FORCE_GENE_ENEMY_MISSILE: {
            return NeuralForceGeneEnemyMissile(nc, mob, desc, focusPoint);
        }
        case NEURAL_FORCE_GENE_RETREAT_COHERE: {
            return NeuralForceGeneRetreatCohere(nc, mob, desc, focusPoint);
        }

        default:
            PANIC("%s: Unhandled forceType: %d\n", __FUNCTION__,
                    desc->forceType);
    }

    NOT_REACHED();
}

/*
 * NeuralForceGetFocusMobPosHelper --
 */
static bool NeuralForceGetFocusMobPosHelper(Mob *mob, FPoint *focusPoint)
{
    ASSERT(focusPoint != NULL);
    if (mob != NULL) {
        *focusPoint = mob->pos;
        return TRUE;
    }
    return FALSE;
}

static bool NeuralForceGetForwardFocusHelper(AIContext *nc,
                                             Mob *mob, FPoint *focusPoint,
                                             bool forward)
{
    FRPoint dir;

    ASSERT(mob != NULL);
    ASSERT(focusPoint != NULL);

    NeuralForceGetHeading(nc, mob, &dir);
    return FPoint_IsFacing(focusPoint, &mob->pos, &dir, forward);
}

static bool NeuralForceGetAdvanceFocusHelper(AIContext *nc,
                                             Mob *mob, FPoint *focusPoint,
                                             bool advance)
{
    Mob *base = nc->sg->friendBase();

    if (base == NULL) {
        return FALSE;
    }

    return FPoint_IsFacingFPoint(focusPoint, &mob->pos,
                                 &mob->pos, &base->pos, advance);
}


/*
 * NeuralForce_FocusToForce --
 *    Convert a focus point to a force.
 *    Returns TRUE iff the force is valid after conversion.
 */
bool NeuralForce_FocusToForce(AIContext *nc,
                              Mob *mob,
                              NeuralForceDesc *desc,
                              FPoint *focusPoint,
                              bool haveForce,
                              FRPoint *rForce)
{
    ASSERT(rForce != NULL);
    ASSERT(focusPoint != NULL);

    ASSERT(!desc->filterForward || !desc->filterBackward);
    ASSERT(!desc->filterAdvance || !desc->filterRetreat);

    if (haveForce && (desc->filterForward || desc->filterBackward)) {
        haveForce = NeuralForceGetForwardFocusHelper(nc, mob, focusPoint,
                                                     desc->filterForward);
    }
    if (haveForce && (desc->filterAdvance || desc->filterRetreat)) {
        haveForce = NeuralForceGetAdvanceFocusHelper(nc, mob, focusPoint,
                                                     desc->filterAdvance);
    }
    if (haveForce && desc->filterRange) {
        if (FPoint_DistanceSquared(&mob->pos, focusPoint) >
            desc->range * desc->range) {
            haveForce = FALSE;
        }
    }

    if (haveForce) {
        FPoint_ToFRPointWithRadius(focusPoint, &mob->pos, 1.0f, rForce);

        if (desc->useTangent) {
            rForce->theta += (float)M_PI/2;
        }
        return TRUE;
    } else {
        FRPoint_Zero(rForce);
        return FALSE;
    }
}

/*
 * NeuralForce_GetForce --
 *    Calculate the specified force.
 *    Returns TRUE iff the force is valid.
 */
bool NeuralForce_GetForce(AIContext *nc,
                          Mob *mob,
                          NeuralForceDesc *desc,
                          FRPoint *rForce)
{
    FPoint focusPoint;
    bool haveForce;
    haveForce = NeuralForce_GetFocus(nc, mob, desc, &focusPoint);
    return NeuralForce_FocusToForce(nc, mob, desc, &focusPoint, haveForce,
                                    rForce);
}

float NeuralForce_FocusToRange(Mob *mob,
                               FPoint *focusPoint,
                               bool haveFocus)
{
    ASSERT(mob != NULL);
    ASSERT(focusPoint != NULL);

    if (haveFocus) {
        return FPoint_Distance(&mob->pos, focusPoint);
    } else {
        return 0.0f;
    }
}

float NeuralForce_GetRange(AIContext *nc,
                           Mob *mob, NeuralForceDesc *desc)
{
    FPoint focusPoint;
    bool haveFocus = NeuralForce_GetFocus(nc, mob, desc, &focusPoint);
    return NeuralForce_FocusToRange(mob, &focusPoint, haveFocus);
}


/*
 * NeuralForce_ApplyToMob --
 *   Applies a force to a mob, taking speed into account.
 */
void NeuralForce_ApplyToMob(AIContext *nc,
                            Mob *mob, FRPoint *rForce) {
    float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
    ASSERT(mob->type == MOB_TYPE_FIGHTER);

    if (rForce->radius < MICRON) {
        /*
         * Continue on the current heading if we didn't get a strong-enough
         * force.
         */

        NeuralForceGetHeading(nc, mob, rForce);
    }
    FRPoint_SetSpeed(rForce, speed);

    FRPoint_ToFPoint(rForce, &mob->pos, &mob->cmd.target);
}


float NeuralCrowd_GetValue(AIContext *nc,
                           Mob *mob, NeuralCrowdDesc *desc)
{
    //XXX cache?
    MappingSensorGrid *sg = nc->sg;

    if (desc->radius <= 0.0f) {
        return 0.0f;
    }

    if (desc->crowdType == NEURAL_CROWD_FRIEND_FIGHTER) {
        return sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                     &mob->pos, desc->radius);
    } else if (desc->crowdType == NEURAL_CROWD_ENEMY_SHIP) {
        return sg->numTargetsInRange(MOB_FLAG_SHIP,
                                     &mob->pos, desc->radius);
    } else if (desc->crowdType == NEURAL_CROWD_CORES) {
        return sg->numTargetsInRange(MOB_FLAG_POWER_CORE,
                                     &mob->pos, desc->radius);
    } else if (desc->crowdType == NEURAL_CROWD_FRIEND_CORES) {
        return sg->numFriendsInRange(MOB_FLAG_POWER_CORE,
                                     &mob->pos, desc->radius);
    } else if  (desc->crowdType == NEURAL_CROWD_FRIEND_MISSILE) {
        return sg->numFriendsInRange(MOB_FLAG_MISSILE,
                                     &mob->pos, desc->radius);
    } else if (desc->crowdType == NEURAL_CROWD_ENEMY_MISSILE) {
        return sg->numTargetsInRange(MOB_FLAG_MISSILE,
                                     &mob->pos, desc->radius);
    } else if (desc->crowdType == NEURAL_CROWD_BASE_ENEMY_SHIP) {
        Mob *base = sg->friendBase();
        if (base != NULL) {
            return sg->numTargetsInRange(MOB_FLAG_SHIP,
                                         &base->pos, desc->radius);
        }
        return 0.0f;
    } else if (desc->crowdType == NEURAL_CROWD_BASE_FRIEND_SHIP) {
        Mob *base = sg->friendBase();
        if (base != NULL) {
            return sg->numFriendsInRange(MOB_FLAG_SHIP,
                                         &base->pos, desc->radius);
        }
        return 0.0f;
    } else if (desc->crowdType == NEURAL_CROWD_NET_ENEMY_SHIP ||
               desc->crowdType == NEURAL_CROWD_NET_FRIEND_SHIP) {
        int ec = sg->numTargetsInRange(MOB_FLAG_SHIP, &mob->pos, desc->radius);
        int fc = sg->numFriendsInRange(MOB_FLAG_SHIP, &mob->pos, desc->radius);

        if (desc->crowdType == NEURAL_CROWD_NET_ENEMY_SHIP) {
            return ec - fc;
        }

        return fc - ec;
    } else {
        NOT_IMPLEMENTED();
    }
}

float NeuralSquad_GetValue(AIContext *nc, Mob *mob, NeuralSquadDesc *squadDesc)
{
    NeuralSquadType squadType = squadDesc->squadType;
    int numSquads = squadDesc->numSquads;

    if (squadType == NEURAL_SQUAD_NONE) {
        return 0.0f;
    }

    if (numSquads <= 1 || numSquads >= MAX_INT32) {
        return 0.0f;
    }

    uint64 seed = mob->mobid;
    seed = (seed << 32) | squadDesc->seed;
    float fmobid = Random_UnitFloatFromSeed(seed);

    if (squadType == NEURAL_SQUAD_MOBID) {
        return fmobid;
    } else if (squadType == NEURAL_SQUAD_EQUAL_PARTITIONS) {
        /*
        * Should replicate ML_FOP_1x1_SQUAD_SELECT on a
        * NEURAL_VALUE_MOBID.
        */
        ASSERT(squadType == NEURAL_SQUAD_EQUAL_PARTITIONS);

        if (fmobid == 1.0f) {
            return 1.0f - (1.0f / numSquads);
        }
        return floorf(fmobid / (1.0f / numSquads));
    } else if (squadType == NEURAL_SQUAD_POWER_UP ||
               squadType == NEURAL_SQUAD_POWER_DOWN) {
        int i;
        float base = powf(2.0f, numSquads);
        float topHigh = base / 2.0f;
        float bottom = base - 1.0f;
        float curSquad;
        float curFraction = topHigh / bottom;
        float squadSize = 1.0f / numSquads;

        curSquad = squadType == NEURAL_SQUAD_POWER_DOWN ?
                   0.0f : 1.0f - (1.0f / numSquads);

        ASSERT(numSquads > 1);
        for (i = 0; i < numSquads - 1; i++) {
            if (fmobid <= curFraction) {
                return curSquad;
            }

            curSquad = squadType == NEURAL_SQUAD_POWER_DOWN ?
                       (curSquad + squadSize) :
                       (curSquad - squadSize);
            curFraction /= 2.0f;
        }

        return curSquad;
    } else {
        NOT_IMPLEMENTED();
    }
}

bool NeuralCondition_AppliesToMob(AIContext *nc, Mob *mob,
                                  NeuralConditionDesc *condDesc)
{
    if (condDesc->squad.active) {
        float squad = NeuralSquad_GetValue(nc, mob,  &condDesc->squad.squadDesc);
        float min = MIN(condDesc->squad.limit0, condDesc->squad.limit1);
        float max = MAX(condDesc->squad.limit0, condDesc->squad.limit1);

        if (!condDesc->squad.invert) {
            if (squad < min || squad > max) {
                return FALSE;
            }
        } else {
            if (squad >= min && squad <= max) {
                return FALSE;
            }
        }
    }

    return TRUE;
}


float NeuralTick_GetValue(AIContext *nc, NeuralTickDesc *desc)
{
    FleetAI *ai = nc->ai;

    //XXX cache?

    if (desc->waveType != NEURAL_WAVE_NONE &&
        desc->frequency == 0.0f) {
        return 0.0f;
    }

    float t = (float)ai->tick;

    if (desc->waveType == NEURAL_WAVE_NONE) {
        return t;
    } else if (desc->waveType == NEURAL_WAVE_SINE) {
        return sinf(t / desc->frequency);
    } else if (desc->waveType == NEURAL_WAVE_UNIT_SINE) {
        return 0.5f * sinf(t / desc->frequency) + 0.5f;
    } else if (desc->waveType == NEURAL_WAVE_ABS_SINE) {
        return fabsf(sinf(t / desc->frequency));
    } else if (desc->waveType == NEURAL_WAVE_FMOD) {
        return fmodf(t, desc->frequency);
    } else {
        NOT_IMPLEMENTED();
    }
}


float NeuralValue_GetValue(AIContext *nc,
                           Mob *mob, NeuralValueDesc *desc, uint index)
{
    FRPoint force;
    RandomState *rs = nc->rs;
    FleetAI *ai = nc->ai;
    MappingSensorGrid *sg = nc->sg;

    FRPoint_Zero(&force);

    ASSERT(desc != NULL);
    switch (desc->valueType) {
        case NEURAL_VALUE_ZERO:
        case NEURAL_VALUE_VOID:
            return 0.0f;
        case NEURAL_VALUE_FORCE:
            return NeuralForce_GetRange(nc, mob, &desc->forceDesc);
        case NEURAL_VALUE_CROWD:
            return NeuralCrowd_GetValue(nc, mob, &desc->crowdDesc);
        case NEURAL_VALUE_TICK:
            return NeuralTick_GetValue(nc, &desc->tickDesc);
        case NEURAL_VALUE_MOBID: {
            NeuralSquadDesc squadDesc;
            MBUtil_Zero(&squadDesc, sizeof(squadDesc));
            squadDesc.squadType = NEURAL_SQUAD_MOBID;
            squadDesc.seed = index;
            return NeuralSquad_GetValue(nc, mob, &squadDesc);
        }
        case NEURAL_VALUE_SQUAD: {
            return NeuralSquad_GetValue(nc, mob, &desc->squadDesc);
        }
        case NEURAL_VALUE_RANDOM_UNIT:
            return RandomState_UnitFloat(rs);
        case NEURAL_VALUE_CREDITS:
            return (float)ai->credits;
        case NEURAL_VALUE_FRIEND_SHIPS:
            return (float)sg->numFriends(MOB_FLAG_SHIP);
        case NEURAL_VALUE_FRIEND_MISSILES:
            return (float)sg->numFriends(MOB_FLAG_MISSILE);
        case NEURAL_VALUE_ENEMY_SHIPS:
            return (float)sg->numTargets(MOB_FLAG_SHIP);
        case NEURAL_VALUE_ENEMY_MISSILES:
            return (float)sg->numTargets(MOB_FLAG_MISSILE);

        default:
            NOT_IMPLEMENTED();
    }
}


void NeuralLocus_RunTick(AIContext *aic, NeuralLocusDesc *desc,
                         NeuralLocusPosition *lpos)
{
    FPoint newPoint;
    bool wasActive = lpos->active;
    Mob *base = aic->sg->friendBaseShadow();

    ASSERT(desc != NULL);
    ASSERT(lpos != NULL);

    if (desc->locusType == NEURAL_LOCUS_VOID) {
        lpos->active = FALSE;
    } else if (desc->locusType == NEURAL_LOCUS_TRACK) {
        lpos->active = NeuralForce_GetFocus(aic, base, &desc->trackDesc.focus,
                                            &newPoint);
    } else if (desc->locusType == NEURAL_LOCUS_ORBIT) {
        FRPoint rp;
        FPoint focusPoint;

        if (desc->orbitDesc.radius < MICRON ||
            desc->orbitDesc.period < MICRON) {
            lpos->active = FALSE;
            return;
        }


        lpos->active = NeuralForce_GetFocus(aic, base, &desc->orbitDesc.focus,
                                            &focusPoint);

        if (lpos->active) {
            if (!wasActive) {
                rp.theta = RandomState_Float(aic->rs, 0, M_PI * 2.0f);
            } else {
                FPoint_ToFRPoint(&lpos->pos, &focusPoint, &rp);
            }
            rp.radius = desc->orbitDesc.radius;

            rp.theta += M_PI * 2.0f / desc->orbitDesc.period;
            rp.theta = fmodf(rp.theta, M_PI * 2.0f);

            FRPoint_ToFPoint(&rp, &focusPoint, &newPoint);
        }
    } else if (desc->locusType == NEURAL_LOCUS_PATROL_MAP) {
        FPoint circular;
        FPoint linear;
        FPoint locus;
        bool haveCircular = FALSE;
        bool haveLinear = FALSE;
        float width = aic->ai->bp.width;
        float height = aic->ai->bp.height;

        MBUtil_Zero(&circular, sizeof(circular));
        MBUtil_Zero(&linear, sizeof(linear));
        MBUtil_Zero(&locus, sizeof(locus));

        if (desc->patrolMapDesc.circularPeriod > 0.0f &&
            desc->patrolMapDesc.circularWeight > 0.0f) {
            float cwidth = width / 2;
            float cheight = height / 2;
            float ct = 2.0f * M_PI * (aic->ai->tick / desc->patrolMapDesc.circularPeriod);

            circular.x = cwidth + cwidth * cosf(ct);
            circular.y = cheight + cheight * sinf(ct);
            haveCircular = TRUE;
        }

        if (desc->patrolMapDesc.linearPeriod > 0.0f &&
            desc->patrolMapDesc.linearWeight > 0.0f) {
            float temp;
            float xPeriod = desc->patrolMapDesc.linearPeriod +
                            desc->patrolMapDesc.linearXPeriodOffset;
            xPeriod = MAX(1.0f, xPeriod);
            float xp = aic->ai->tick / xPeriod;

            xp = modff(xp, &temp);
            ASSERT(xp >= 0.0f && xp <= 1.0f);
            if (xp <= 0.5f) {
                linear.x = width * 2.0f * xp;
            } else {
                linear.x = width * (2.0f - (2.0f * xp));
            }

            float yPeriod = desc->patrolMapDesc.linearPeriod +
                            desc->patrolMapDesc.linearYPeriodOffset;
            yPeriod = MAX(1.0f, yPeriod);
            float yp = aic->ai->tick / yPeriod;
            yp = modff(yp, &temp);
            ASSERT(yp >= 0.0f && yp <= 1.0f);
            if (yp <= 0.5f) {
                linear.y = height * 2.0f * yp;
            } else {
                linear.y = height * (2.0f - (2.0f * yp));
            }

            haveLinear = TRUE;
        }

        if (haveLinear || haveCircular) {
            float scale = 0.0f;
            locus.x = 0.0f;
            locus.y = 0.0f;
            if (haveLinear) {
                locus.x += desc->patrolMapDesc.linearWeight * linear.x;
                locus.y += desc->patrolMapDesc.linearWeight * linear.y;
                scale += desc->patrolMapDesc.linearWeight;
            }
            if (haveCircular) {
                locus.x += desc->patrolMapDesc.circularWeight * circular.x;
                locus.y += desc->patrolMapDesc.circularWeight * circular.y;
                scale += desc->patrolMapDesc.circularWeight;
            }

            ASSERT(scale > 0.0f);
            locus.x /= scale;
            locus.y /= scale;

            newPoint = locus;
            lpos->active = TRUE;
        } else {
            lpos->active = FALSE;
        }
    } else if (desc->locusType == NEURAL_LOCUS_PATROL_EDGES) {
        float width = aic->ai->bp.width;
        float height = aic->ai->bp.height;

        if (desc->patrolEdgesDesc.period < MICRON) {
            lpos->active = FALSE;
            return;
        }

        float temp;
        float period = desc->patrolEdgesDesc.period;
        float p = aic->ai->tick / period;
        p = modff(p, &temp);
        ASSERT(p >= 0.0f && p <= 1.0f);

        float wp = width / (2.0f * width + 2.0f * height);
        float hp = height / (2.0f * width + 2.0f * height);

        if (p <= wp) {
            newPoint.x = width * (p / wp);
            newPoint.y = 0;
        } else if (p >= wp && p <= (wp + hp)) {
            newPoint.x = width;
            newPoint.y = height * ((p - wp) / hp);
        } else if (p >= (wp + hp) && p <= (2.0f * wp + hp)) {
            newPoint.x = width * (1.0f - ((p - (wp + hp)) / wp));
            newPoint.y = height;
        } else {
            newPoint.x = 0;
            newPoint.y = height * (1.0f - ((p - (2.0f * wp + hp)) / hp));
        }

        lpos->active = TRUE;
    } else {
        NOT_IMPLEMENTED();
    }

    if (lpos->active) {
        if (!wasActive) {
            lpos->pos = newPoint;
        } else if (!desc->speedLimited ||
                FPoint_Distance(&lpos->pos, &newPoint) <= desc->speed) {
            lpos->pos = newPoint;
        } else {
            FPoint_MoveToPointAtSpeed(&lpos->pos, &newPoint, desc->speed);
        }
    }
}

void NeuralCombiner_ApplyOutput(NeuralCombinerType cType,
                                float inputValue,
                                FRPoint *force)
{
    switch (cType) {
        case NEURAL_CT_VOID:
            FRPoint_SetSpeed(force, 0.0f);
            break;
        case NEURAL_CT_ASSIGN:
            FRPoint_SetSpeed(force, inputValue);
            break;
        case NEURAL_CT_MULTIPLY:
            FRPoint_Multiply(force, inputValue);
            break;
        default:
            NOT_IMPLEMENTED();
    }
}

static bool NeuralForceGeneMidway(AIContext *aic,
                               Mob *mob,
                               NeuralForceDesc *desc,
                               FPoint *focusPoint)
{
    NeuralForceDesc o0;
    NeuralValueDesc i0, i1;

    /*
     * Copy the base desc to ensure we initialize any new
     * parameters.
     */

    o0 = *desc;
    o0.filterAdvance = TRUE;
    o0.filterBackward = FALSE;
    o0.filterForward = TRUE;
    o0.filterRetreat = FALSE;
    o0.forceType = NEURAL_FORCE_MIDWAY;
    o0.radius = 1729.684937;
    o0.range = 0.0f;
    o0.useBase = TRUE;
    o0.useTangent = TRUE;

    MBUtil_Zero(&i0, sizeof(i0));
    i0.forceDesc = *desc;
    i0.valueType = NEURAL_VALUE_FORCE;
    i0.forceDesc.forceType = NEURAL_FORCE_ALIGN;
    i0.forceDesc.filterAdvance = FALSE;
    i0.forceDesc.filterBackward = TRUE;
    i0.forceDesc.filterForward = TRUE;
    i0.forceDesc.filterRetreat = TRUE;
    i0.forceDesc.radius = 156.808365;
    i0.forceDesc.useBase = TRUE;
    i0.forceDesc.useTangent = FALSE;

    MBUtil_Zero(&i1, sizeof(i1));
    i1.forceDesc = *desc;
    i1.valueType = NEURAL_VALUE_FORCE;
    i1.forceDesc.forceType = NEURAL_FORCE_RETREAT_ENEMY_COHERE;
    i1.forceDesc.filterAdvance = FALSE;
    i1.forceDesc.filterBackward = TRUE;
    i1.forceDesc.filterForward = TRUE;
    i1.forceDesc.filterRetreat = FALSE;
    i1.forceDesc.radius = 0.0f;
    i1.forceDesc.useBase = FALSE;
    i1.forceDesc.useTangent = FALSE;

    FPoint focusI0, focusI1, focusO0;
    FRPoint rForce;
    bool haveO0 = NeuralForce_GetFocus(aic, mob, &o0, &focusO0);

    if (!haveO0) {
        return FALSE;
    }

    bool haveI0 = NeuralForce_GetFocus(aic, mob, &i0.forceDesc, &focusI0);
    bool haveI1 = NeuralForce_GetFocus(aic, mob, &i1.forceDesc, &focusI1);

    float vI0 = NeuralForce_FocusToRange(mob, &focusI0, haveI0);
    float vI1 = NeuralForce_FocusToRange(mob, &focusI1, haveI1);

    float f = 1.0f;
    f *= vI0;
    f *= vI1;
    f = powf(f, 1.0f / 2);

    ASSERT(haveO0);
    haveO0 = NeuralForce_FocusToForce(aic, mob, &o0, &focusO0, TRUE, &rForce);

    if (haveO0) {
        FRPoint_SetSpeed(&rForce, f);
        FRPoint_ToFPoint(&rForce, &mob->pos, focusPoint);
        return TRUE;
    }

    return FALSE;
}


static bool NeuralForceGeneEnemyMissile(AIContext *aic,
                                        Mob *mob,
                                        NeuralForceDesc *desc,
                                        FPoint *focusPoint)
{
    NeuralForceDesc o0;
    NeuralValueDesc i0;

    /*
     * Copy the base desc to ensure we initialize any new
     * parameters.
     */

    o0 = *desc;
    o0.filterAdvance = FALSE;
    o0.filterBackward = FALSE;
    o0.filterForward = FALSE;
    o0.filterRetreat = FALSE;
    o0.forceType = NEURAL_FORCE_ENEMY_MISSILE;
    o0.radius = 313.822601;
    o0.range = 0.0f;
    o0.useBase = FALSE;
    o0.useTangent = FALSE;

    MBUtil_Zero(&i0, sizeof(i0));
    i0.valueType = NEURAL_VALUE_FRIEND_SHIPS;

    FPoint focusO0;
    FRPoint rForce;
    bool haveO0 = NeuralForce_GetFocus(aic, mob, &o0, &focusO0);

    if (!haveO0) {
        return FALSE;
    }

    float vI0 = NeuralValue_GetValue(aic, mob, &i0, 0);

    ASSERT(haveO0);
    haveO0 = NeuralForce_FocusToForce(aic, mob, &o0, &focusO0, TRUE, &rForce);

    if (haveO0) {
        FRPoint_SetSpeed(&rForce, vI0);
        FRPoint_ToFPoint(&rForce, &mob->pos, focusPoint);
        return TRUE;
    }

    return FALSE;
}


static bool NeuralForceGeneRetreatCohere(AIContext *aic,
                                         Mob *mob,
                                         NeuralForceDesc *desc,
                                         FPoint *focusPoint)
{
    NeuralForceDesc o0;
    NeuralValueDesc i1, i6, i14;

    /*
     * Copy the base desc to ensure we initialize any new
     * parameters.
     */

    o0 = *desc;
    o0.filterAdvance = FALSE;
    o0.filterBackward = FALSE;
    o0.filterForward = TRUE;
    o0.filterRange = FALSE;
    o0.filterRetreat = FALSE;
    o0.forceType = NEURAL_FORCE_RETREAT_COHERE;
    o0.radius = 2728.651611;
    o0.useBase = TRUE;
    o0.useTangent = TRUE;

    MBUtil_Zero(&i1, sizeof(i1));
    i1.valueType = NEURAL_VALUE_FRIEND_SHIPS;

    MBUtil_Zero(&i6, sizeof(i6));
    i6.valueType = NEURAL_VALUE_MOBID;

    MBUtil_Zero(&i14, sizeof(i14));
    i14.forceDesc = *desc;
    i14.valueType = NEURAL_VALUE_FORCE;
    i14.forceDesc.forceType = NEURAL_FORCE_ENEMY_BASE_GUESS;
    i14.forceDesc.filterAdvance = TRUE;
    i14.forceDesc.filterBackward = FALSE;
    i14.forceDesc.filterForward = FALSE;
    i14.forceDesc.filterRetreat = TRUE;
    i14.forceDesc.radius = 309.636841;
    i14.forceDesc.useBase = FALSE;
    i14.forceDesc.useTangent = TRUE;

    FPoint focusI14, focusO0;
    FRPoint rForce;
    bool haveO0 = NeuralForce_GetFocus(aic, mob, &o0, &focusO0);

    if (!haveO0) {
        return FALSE;
    }

    bool haveI14 = NeuralForce_GetFocus(aic, mob, &i14.forceDesc, &focusI14);
    float vI1 = NeuralValue_GetValue(aic, mob, &i1, 0);
    float vI6 = NeuralValue_GetValue(aic, mob, &i6, 0);

    float vI14 = NeuralForce_FocusToRange(mob, &focusI14, haveI14);

    float vN33 = logf(vI6);

    float vN39 = vN33 + vI1 + vI14;
    vN39 = ML_ClampUnit(1.0f - expf(-(vN39 * vN39)));

    ASSERT(haveO0);
    haveO0 = NeuralForce_FocusToForce(aic, mob, &o0, &focusO0, TRUE, &rForce);

    if (haveO0) {
        FRPoint_SetSpeed(&rForce, vN39);
        FRPoint_ToFPoint(&rForce, &mob->pos, focusPoint);
        return TRUE;
    }

    return FALSE;
}
