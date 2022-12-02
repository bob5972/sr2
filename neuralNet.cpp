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

#include "mutate.h"

#include "neuralNet.hpp"
#include "textDump.hpp"

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
                                       const FPoint *pos, FRPoint *force);

static TextMapEntry tmForces[] = {
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
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE),                      },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS),                },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS_LAX),            },
    { TMENTRY(NEURAL_FORCE_MIDWAY),                          },
    { TMENTRY(NEURAL_FORCE_MIDWAY_GUESS),                    },
    { TMENTRY(NEURAL_FORCE_MIDWAY_GUESS_LAX),                },
    { TMENTRY(NEURAL_FORCE_CORES),                           },
    { TMENTRY(NEURAL_FORCE_LOCUS),                           },
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
    { TMENTRY(NEURAL_VALUE_SCALAR), },
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
        { NEURAL_VALUE_FORCE,        0.38f, },
        { NEURAL_VALUE_CROWD,        0.38f, },
        { NEURAL_VALUE_TICK,         0.04f, },
        { NEURAL_VALUE_MOBID,        0.04f, },
        { NEURAL_VALUE_RANDOM_UNIT,  0.04f, },
        { NEURAL_VALUE_CREDITS,      0.02f, },
        { NEURAL_VALUE_FRIEND_SHIPS, 0.04f, },
        { NEURAL_VALUE_SCALAR,       0.04f, },
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

        case NEURAL_VALUE_SCALAR:
            NeuralScalar_Load(mreg, &desc->scalarDesc, s.CStr());
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
    s += "radius";
    desc->radius = MBRegistry_GetFloat(mreg, s.CStr());
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


void NeuralScalar_Load(MBRegistry *mreg,
                       NeuralScalarDesc *desc, const char *prefix)
{
    MBString s;

    s = prefix;
    s += "scalarID";
    desc->scalarID = MBRegistry_GetInt(mreg, s.CStr());
}

void NeuralValue_Mutate(MBRegistry *mreg, NeuralValueDesc *desc,
                        bool isOutput, float rate,
                        const char *prefix)
{
    MBString s;

    s = prefix;
    s += "valueType";

    if (isOutput) {
        desc->valueType = NEURAL_VALUE_FORCE;
    } else if (Random_Flip(rate)) {
        desc->valueType = NeuralValue_Random();
    }
    const char *v = NeuralValue_ToString(desc->valueType);
    MBRegistry_PutCopy(mreg, s.CStr(), v);

    if (desc->valueType == NEURAL_VALUE_FORCE ||
        desc->valueType == NEURAL_VALUE_CROWD) {
        MutationFloatParams vf;

        Mutate_DefaultFloatParams(&vf, MUTATION_TYPE_RADIUS);
        s = prefix;
        s += "radius";
        vf.key = s.CStr();
        Mutate_Float(mreg, &vf, 1);
    }

    if (desc->valueType == NEURAL_VALUE_CROWD) {
        s = prefix;
        s += "crowdType";
        if (Random_Flip(rate)) {
            NeuralCrowdType ct = NeuralCrowd_Random();
            const char *v = NeuralCrowd_ToString(ct);
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            desc->crowdDesc.crowdType = ct;
        }
    } else if (desc->valueType == NEURAL_VALUE_FORCE) {
        s = prefix;
        s += "forceType";
        if (Random_Flip(rate)) {
            NeuralForceType ft = NeuralForce_Random();
            const char *v = NeuralForce_ToString(ft);;
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            desc->forceDesc.forceType = ft;
        }

        MutationBoolParams bf;
        const char *strs[] = {
            "useTangent", "filterForward", "filterBackward",
            "filterAdvance", "filterRetreat",
        };

        for (uint i = 0; i < ARRAYSIZE(strs); i++) {
            s = prefix;
            s += strs[i];
            bf.key = s.CStr();
            bf.flipRate = rate;
            Mutate_Bool(mreg, &bf, 1);
        }
    } else if (desc->valueType == NEURAL_VALUE_TICK) {
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
            desc->tickDesc.waveType = wi;
        }
    } else if (desc->valueType == NEURAL_VALUE_SCALAR) {
        /*
         * scalarID's on outputs are ignored.
         */
        if (!isOutput && Random_Flip(rate)) {
            char *v = NULL;
            int x, ret;

            s = prefix;
            s += "scalarID";
            x = MBRegistry_GetInt(mreg, s.CStr());
            x = Random_Int(-1, x + 1);
            ret = asprintf(&v, "%d", x);
            VERIFY(ret > 0);
            MBRegistry_PutCopy(mreg, s.CStr(), v);
            free(v);
        }
    } else if (desc->valueType == NEURAL_VALUE_ZERO ||
               desc->valueType == NEURAL_VALUE_FRIEND_SHIPS ||
               desc->valueType == NEURAL_VALUE_MOBID ||
               desc->valueType == NEURAL_VALUE_CREDITS ||
               desc->valueType == NEURAL_VALUE_RANDOM_UNIT) {
        /*
         * No parameters to mutate.
         */
    } else {
        PANIC("Unknown NeuralValueType: %s (%d)\n",
              NeuralValue_ToString(desc->valueType), desc->valueType);
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
    bool align = FALSE;
    bool cohere = FALSE;
    bool enemy = FALSE;
    bool advance = FALSE;
    bool forward = FALSE;
    bool backward = FALSE;
    bool retreat = FALSE;

    MBUtil_Zero(&f, sizeof(f));
    f.rangeFilter.useRange = TRUE;
    f.rangeFilter.pos = self->pos;
    f.rangeFilter.radius = desc->radius;
    f.flagsFilter.useFlags = TRUE;

    switch (forceType) {
        case NEURAL_FORCE_ALIGN2:
            align = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_FORWARD_ALIGN:
            forward = TRUE;
            align = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_BACKWARD_ALIGN:
            backward = TRUE;
            align = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_ADVANCE_ALIGN:
            advance = TRUE;
            align = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_RETREAT_ALIGN:
            retreat = TRUE;
            align = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_COHERE:
            cohere = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_FORWARD_COHERE:
            forward = TRUE;
            cohere = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_BACKWARD_COHERE:
            backward = TRUE;
            cohere = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_ADVANCE_COHERE:
            advance = TRUE;
            cohere = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_RETREAT_COHERE:
            retreat = TRUE;
            cohere = TRUE;
            enemy = FALSE;
            break;
        case NEURAL_FORCE_ENEMY_ALIGN:
            align = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_FORWARD_ENEMY_ALIGN:
            forward = TRUE;
            align = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_BACKWARD_ENEMY_ALIGN:
            backward = TRUE;
            align = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_ADVANCE_ENEMY_ALIGN:
            advance = TRUE;
            align = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_RETREAT_ENEMY_ALIGN:
            retreat = TRUE;
            align = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_ENEMY_COHERE2:
            cohere = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_FORWARD_ENEMY_COHERE:
            forward = TRUE;
            cohere = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_BACKWARD_ENEMY_COHERE:
            backward = TRUE;
            cohere = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_ADVANCE_ENEMY_COHERE:
            advance = TRUE;
            cohere = TRUE;
            enemy = TRUE;
            break;
        case NEURAL_FORCE_RETREAT_ENEMY_COHERE:
            retreat = TRUE;
            cohere = TRUE;
            enemy = TRUE;
            break;
        default:
            NOT_IMPLEMENTED();
    }

    if (enemy) {
        f.flagsFilter.flags = MOB_FLAG_SHIP;
        useFriends = FALSE;
    } else {
        f.flagsFilter.flags = MOB_FLAG_FIGHTER;
        useFriends = TRUE;
    }


    if (forward || backward) {
        ASSERT(!forward || !backward);
        ASSERT(!advance);
        ASSERT(!retreat);
        f.dirFilter.useDir = TRUE;
        f.dirFilter.forward = forward;
        NeuralForceGetHeading(nc, self, &f.dirFilter.dir);
        f.dirFilter.pos = self->pos;
    }

    if (advance || retreat) {
        Mob *base = nc->sg->friendBase();
        ASSERT(!advance || !retreat);
        ASSERT(!forward);
        ASSERT(!backward);

        if (base == NULL) {
            return FALSE;
        }

        f.dirFilter.useDir = TRUE;
        f.dirFilter.forward = advance;
        FPoint_ToFRPoint(&self->pos, &base->pos, &f.dirFilter.dir);
        f.dirFilter.pos = self->pos;
    }

    if (!nc->sg->avgFlock(&vel, &pos, &f, useFriends)) {
        return FALSE;
    }

    if (align) {
        ASSERT(!cohere);
        if (vel.x >= MICRON || vel.y >= MICRON) {
            vel.x += self->pos.x;
            vel.y += self->pos.y;
            *focusPoint = vel;
            return TRUE;
        }
        return FALSE;
    } else if (cohere) {
        *focusPoint = pos;
        return TRUE;
    }

    NOT_REACHED();
}


static bool NeuralForceGetSeparateFocus(AIContext *nc,
                                        Mob *self, NeuralForceDesc *desc,
                                        FPoint *focusPoint)
{
    FRPoint force;
    int x = 0;
    MobSet::MobIt mit = nc->sg->friendsIterator(MOB_FLAG_FIGHTER);

    MobFilter f;
    MBUtil_Zero(&f, sizeof(f));
    f.rangeFilter.useRange = TRUE;
    f.rangeFilter.pos = self->pos;
    f.rangeFilter.radius = desc->radius;
    f.flagsFilter.useFlags = FALSE;

    if (desc->forceType == NEURAL_FORCE_FORWARD_SEPARATE ||
        desc->forceType == NEURAL_FORCE_BACKWARD_SEPARATE) {
        f.dirFilter.useDir = TRUE;
        NeuralForceGetHeading(nc, self, &f.dirFilter.dir);
        f.dirFilter.pos = self->pos;
        f.dirFilter.forward = desc->forceType == NEURAL_FORCE_FORWARD_SEPARATE ?
                              TRUE : FALSE;
    } else if (desc->forceType == NEURAL_FORCE_ADVANCE_SEPARATE ||
               desc->forceType == NEURAL_FORCE_RETREAT_SEPARATE) {
        Mob *base = nc->sg->friendBase();

        if (base == NULL) {
            return FALSE;
        }

        f.dirFilter.useDir = TRUE;
        FPoint_ToFRPoint(&self->pos, &base->pos, &f.dirFilter.dir);
        f.dirFilter.pos = self->pos;

        f.dirFilter.forward = desc->forceType == NEURAL_FORCE_ADVANCE_SEPARATE ?
                              TRUE : FALSE;
    } else {
        ASSERT(desc->forceType == NEURAL_FORCE_SEPARATE);
    }

    ASSERT(self->type == MOB_TYPE_FIGHTER);

    FRPoint_Zero(&force);

    if (!Mob_IsFilterEmpty(&f)) {
        while (mit.hasNext()) {
            Mob *m = mit.next();
            ASSERT(m != NULL);

            if (m->mobid != self->mobid && Mob_Filter(m, &f)) {
                NeuralForceGetRepulseFocus(nc, &self->pos, &m->pos, &force);
                x++;
            }
        }
    }

    FRPoint_ToFPoint(&force, &self->pos, focusPoint);
    return x > 0;
}

static void NeuralForceGetRepulseFocus(AIContext *nc,
                                       const FPoint *selfPos,
                                       const FPoint *pos, FRPoint *force)
{
    FRPoint f;

    FPoint_ToFRPoint(selfPos, pos, &f);

    /*
     * Avoid 1/0 => NAN, and then randomize the direction when
     * the point is more or less directly on top of us.
     */
    if (f.radius < MICRON) {
        f.radius = MICRON;
        f.theta = RandomState_Float(nc->rs, 0, M_PI * 2.0f);
    }

    f.radius = 1.0f / (f.radius * f.radius);
    FRPoint_Add(force, &f, force);
}
static void NeuralForceGetEdgeFocus(AIContext *nc,
                                    Mob *self, NeuralForceDesc *desc,
                                    FPoint *focusPoint)
{
    FPoint edgePoint;
    FRPoint force;

    FRPoint_Zero(&force);

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

    FRPoint_ToFPoint(&force, &self->pos, focusPoint);
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
    FRPoint force;

    FRPoint_Zero(&force);

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

    FRPoint_ToFPoint(&force, &self->pos, focusPoint);
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
        return NeuralForceGetFocusMobPosHelper(farthestFriend,
                                                focusPoint);
    } else if (farthestFriend == NULL) {
        return NeuralForceGetFocusMobPosHelper(nearestEnemy,
                                                focusPoint);
    }

    if (FPoint_DistanceSquared(&base->pos, &nearestEnemy->pos) <=
        FPoint_DistanceSquared(&base->pos,&farthestFriend->pos)) {
        return NeuralForceGetFocusMobPosHelper(nearestEnemy,
                                                focusPoint);
    }
    return NeuralForceGetFocusMobPosHelper(farthestFriend,
                                            focusPoint);
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

    FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

    if (rPos.radius < MICRON) {
        rPos.radius = 1.0f;
        rPos.theta = RandomState_Float(nc->rs, 0, M_PI * 2.0f);
    }

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
        case NEURAL_FORCE_RETREAT_ENEMY_COHERE: {
            return NeuralForceGetFlockFocus(nc, mob, desc, focusPoint);
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

            FPoint_ToFRPoint(&mob->pos, &base->pos, &rPoint);
            rPoint.radius = limitDistance;
            FRPoint_ToFPoint(&rPoint, &base->pos, focusPoint);
            return TRUE;
        }
        case NEURAL_FORCE_BASE_SHELL: {
            FRPoint rPoint;
            if (!NeuralForceGetFocusMobPosHelper(nc->sg->friendBase(),
                                                 focusPoint)) {
                return FALSE;
            }
            FPoint_ToFRPoint(&mob->pos, focusPoint, &rPoint);
            rPoint.radius = desc->radius;
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

        // case NEURAL_FORCE_LOCUS: {
        //     NOT_IMPLEMENTED();
        // }

        default:
            PANIC("%s: Unhandled forceType: %d\n", __FUNCTION__,
                    desc->forceType);
    }

    NOT_REACHED();
}


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
    FRPoint dir;
    Mob *base = nc->sg->friendBase();

    if (base == NULL) {
        return FALSE;
    }

    FPoint_ToFRPoint(&mob->pos, &base->pos, &dir);
    return FPoint_IsFacing(focusPoint, &mob->pos, &dir, advance);
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

    if (haveForce) {
        FPoint_ToFRPoint(focusPoint, &mob->pos, rForce);
        FRPoint_SetSpeed(rForce, 1.0f);

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
    } else {
        NOT_IMPLEMENTED();
    }
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
            RandomState lr;
            uint64 seed = mob->mobid;
            seed = (seed << 32) | index;
            RandomState_CreateWithSeed(&lr, seed);
            return RandomState_UnitFloat(&lr);
        }
        case NEURAL_VALUE_RANDOM_UNIT:
            return RandomState_UnitFloat(rs);
        case NEURAL_VALUE_CREDITS:
            return (float)ai->credits;
        case NEURAL_VALUE_FRIEND_SHIPS:
            return (float)sg->numFriends(MOB_FLAG_SHIP);

        default:
            NOT_IMPLEMENTED();
    }
}


void NeuralNet::load(MBRegistry *mreg, const char *prefix,
                     NeuralNetType nnTypeIn)
{
    MBString str;
    const char *cstr;

    ASSERT(mreg != NULL);
    ASSERT(prefix != NULL);

    ASSERT(nnTypeIn == NN_TYPE_FORCES || nnTypeIn == NN_TYPE_SCALARS);
    nnType = nnTypeIn;

    str = prefix;
    str += "numInputs";
    cstr = str.CStr();
    if (MBRegistry_ContainsKey(mreg, cstr) &&
        MBRegistry_GetUint(mreg, cstr) > 0) {
        floatNet.load(mreg, prefix);
    } else {
        floatNet.initialize(1, 1, 1);
        floatNet.loadZeroNet();
    }

    uint numInputs = floatNet.getNumInputs();
    uint numOutputs = floatNet.getNumOutputs();
    numNodes = floatNet.getNumNodes();

    inputs.resize(numInputs);
    outputs.resize(numOutputs);

    inputDescs.resize(numInputs);
    outputDescs.resize(numOutputs);

    for (uint i = 0; i < outputDescs.size(); i++) {
        bool voidNode = FALSE;
        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "%soutput[%d].", prefix,
                            i + floatNet.getOutputOffset());
        VERIFY(ret > 0);
        NeuralValue_Load(mreg, &outputDescs[i], lcstr);
        free(lcstr);

        if (nnType == NN_TYPE_SCALARS &&
            outputDescs[i].valueType != NEURAL_VALUE_SCALAR) {
            voidNode = TRUE;
        } else if (nnType == NN_TYPE_FORCES &&
            outputDescs[i].valueType != NEURAL_VALUE_FORCE) {
            voidNode = TRUE;
        } else if (outputDescs[i].forceDesc.filterForward &&
                   outputDescs[i].forceDesc.filterBackward) {
            voidNode = TRUE;
        } else if (outputDescs[i].forceDesc.filterAdvance &&
                   outputDescs[i].forceDesc.filterRetreat) {
            voidNode = TRUE;
        }

        if (outputDescs[i].valueType == NEURAL_VALUE_FORCE) {
            if (outputDescs[i].forceDesc.forceType == NEURAL_FORCE_ZERO ||
                outputDescs[i].forceDesc.forceType == NEURAL_FORCE_VOID) {
                voidNode = TRUE;
            }
        } else if (outputDescs[i].valueType == NEURAL_VALUE_VOID) {
            voidNode = TRUE;
        }

        if (voidNode) {
            voidOutputNode(i);
        }
    }

    for (uint i = 0; i < inputDescs.size(); i++) {
        char *lcstr = NULL;
        int ret = asprintf(&lcstr, "%sinput[%d].", prefix, i);
        VERIFY(ret > 0);
        NeuralValue_Load(mreg, &inputDescs[i], lcstr);
        free(lcstr);
    }

    minimize();
}

void NeuralNet::minimize()
{
    CPBitVector inputBV;
    inputBV.resize(inputs.size());
    floatNet.minimize(&inputBV);

    for (uint i = 0; i < inputDescs.size(); i++) {
        if (!inputBV.get(i)) {
            voidInputNode(i);
        }
    }
}

void NeuralNet::minimizeScalars(NeuralNet &nnConsumer)
{
    CPBitVector outputBV;
    outputBV.resize(outputs.size());
    outputBV.resetAll();

    ASSERT(nnType == NN_TYPE_SCALARS);
    ASSERT(nnConsumer.nnType == NN_TYPE_FORCES);

    for (uint i = 0; i < nnConsumer.inputDescs.size(); i++) {
        NeuralValueDesc *inp = &nnConsumer.inputDescs[i];
        if (inp->valueType == NEURAL_VALUE_SCALAR) {
            if (inp->scalarDesc.scalarID > 0 &&
                inp->scalarDesc.scalarID < outputs.size()) {
                outputBV.set(inp->scalarDesc.scalarID);
            } else {
                nnConsumer.voidInputNode(i);
            }
        }
    }

    for (uint i = 0; i < outputs.size(); i++) {
        if (!outputBV.get(i)) {
            voidOutputNode(i);
        }
    }

    minimize();
}

void NeuralNet::dumpSanitizedParams(MBRegistry *mreg, const char *prefix)
{
    /*
     * If we voided out the inputs/outputs when the FloatNet was minimized,
     * reflect that here.
     */
    for (uint i = 0; i < inputDescs.size(); i++) {
        if (inputDescs[i].valueType == NEURAL_VALUE_VOID) {
            char *str = NULL;
            const char *value;
            int ret = asprintf(&str, "%sinput[%d].valueType", prefix, i);
            VERIFY(ret > 0);
            value = NeuralValue_ToString(inputDescs[i].valueType);
            MBRegistry_PutCopy(mreg, str, value);
            free(str);
        }
    }
    for (uint i = 0; i < outputDescs.size(); i++) {
        if (outputDescs[i].valueType == NEURAL_VALUE_FORCE &&
            outputDescs[i].forceDesc.forceType == NEURAL_FORCE_VOID) {
            char *str = NULL;
            const char *value;
            int ret = asprintf(&str, "%soutput[%d].forceType", prefix, i);
            VERIFY(ret > 0);
            value = NeuralForce_ToString(outputDescs[i].forceDesc.forceType);
            MBRegistry_PutCopy(mreg, str, value);
            free(str);
        }
    }
}


void NeuralNet_Mutate(MBRegistry *mreg, const char *prefix, float rate,
                      uint maxInputs, uint maxOutputs, uint maxNodes,
                      uint maxNodeDegree)
{
    FloatNet fn;
    MBString str;
    const char *cstr;

    str = prefix;
    str += "numInputs";
    cstr = str.CStr();
    if (MBRegistry_ContainsKey(mreg, cstr) &&
        MBRegistry_GetUint(mreg, cstr) > 0 &&
        rate < 1.0f) {
        fn.load(mreg, prefix);
    } else {
        fn.initialize(maxInputs, maxOutputs, maxNodes);
        fn.loadZeroNet();
    }

    fn.mutate(rate, maxNodeDegree, maxNodes);
    fn.save(mreg, prefix);

    for (uint i = 0; i < fn.getNumInputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "%sinput[%d].", prefix, i);
        VERIFY(ret > 0);
        NeuralValue_Load(mreg, &desc, str);
        NeuralValue_Mutate(mreg, &desc, FALSE, rate, str);
        free(str);
    }

    for (uint i = 0; i < fn.getNumOutputs(); i++) {
        NeuralValueDesc desc;
        char *str = NULL;
        int ret = asprintf(&str, "%soutput[%d].", prefix, i + fn.getOutputOffset());
        VERIFY(ret > 0);
        NeuralValue_Load(mreg, &desc, str);
        NeuralValue_Mutate(mreg, &desc, TRUE, rate, str);
        free(str);
    }
}


void NeuralNet::fillInputs(Mob *mob)
{
    if (mob == NULL) {
        mob = aic.sg->friendBaseShadow();
    }

    ASSERT(inputs.size() == inputDescs.size());

    for (uint i = 0; i < inputDescs.size(); i++) {
        inputs[i] = getInputValue(mob, i);
    }
}


void NeuralNet::compute()
{
    float maxV = (1.0f / MICRON);

    floatNet.compute(inputs, outputs);

    ASSERT(outputs.size() == outputDescs.size());
    for (uint i = 0; i < outputs.size(); i++) {
        if (!isOutputActive(&outputDescs[i])) {
            outputs[i] = 0.0f;
        } else if (isnan(outputs[i])) {
            outputs[i] = 0.0f;
        } else if (outputs[i] > maxV) {
            outputs[i] = maxV;
        } else if (outputs[i] < -maxV) {
            outputs[i] = -maxV;
        }
    }
}


void NeuralNet::doScalars()
{
    fillInputs(NULL);
    compute();

    for (uint i = 0; i < outputs.size(); i++) {
        ASSERT(outputDescs[i].valueType == NEURAL_VALUE_SCALAR ||
               outputDescs[i].valueType == NEURAL_VALUE_VOID);
    }
}



void NeuralNet::doForces(Mob *mob, FRPoint *outputForce)
{
    fillInputs(mob);
    compute();

    FRPoint_Zero(outputForce);
    ASSERT(outputs.size() == outputDescs.size());
    for (uint i = 0; i < outputDescs.size(); i++) {
        FRPoint force;
        ASSERT(outputDescs[i].valueType == NEURAL_VALUE_FORCE);
        ASSERT(outputDescs[i].forceDesc.forceType != NEURAL_FORCE_ZERO);
        if (outputs[i] != 0.0f &&
            getOutputForce(mob, i, &force)) {
            FRPoint_SetSpeed(&force, outputs[i]);
            FRPoint_Add(&force, outputForce, outputForce);
        }
    }
}
