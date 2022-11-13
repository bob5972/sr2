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
    { TMENTRY(NEURAL_FORCE_FORWARD_ALIGN),                   },
    { TMENTRY(NEURAL_FORCE_BACKWARD_ALIGN),                  },
    { TMENTRY(NEURAL_FORCE_COHERE),                          },
    { TMENTRY(NEURAL_FORCE_FORWARD_COHERE),                  },
    { TMENTRY(NEURAL_FORCE_BACKWARD_COHERE),                 },
    { TMENTRY(NEURAL_FORCE_SEPARATE),                        },
    { TMENTRY(NEURAL_FORCE_FORWARD_SEPARATE),                },
    { TMENTRY(NEURAL_FORCE_BACKWARD_SEPARATE),               },
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
    { TMENTRY(NEURAL_FORCE_ENEMY),                           },
    { TMENTRY(NEURAL_FORCE_ENEMY_ALIGN),                     },
    { TMENTRY(NEURAL_FORCE_ENEMY_COHERE),                    },
    { TMENTRY(NEURAL_FORCE_ENEMY_MISSILE),                   },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE),                      },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS),                },
    { TMENTRY(NEURAL_FORCE_ENEMY_BASE_GUESS_LAX),            },
    { TMENTRY(NEURAL_FORCE_MIDWAY),                          },
    { TMENTRY(NEURAL_FORCE_MIDWAY_GUESS),                    },
    { TMENTRY(NEURAL_FORCE_MIDWAY_GUESS_LAX),                },
    { TMENTRY(NEURAL_FORCE_CORES),                           },
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

    if (NEURAL_ALLOW_ATTACK_FORCES) {
        s = prefix;
        s += "doIdle";
        desc->doIdle = MBRegistry_GetBoolD(mreg, s.CStr(), TRUE);

        s = prefix;
        s += "doAttack";
        desc->doAttack = MBRegistry_GetBoolD(mreg, s.CStr(), FALSE);
    } else {
        desc->doIdle = TRUE;
        desc->doAttack = FALSE;
    }
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
        s = prefix;
        s += "useTangent";
        bf.key = s.CStr();
        bf.flipRate = rate;
        Mutate_Bool(mreg, &bf, 1);

        if (NEURAL_ALLOW_ATTACK_FORCES) {
            s = prefix;
            s += "doIdle";
            bf.key = s.CStr();
            bf.flipRate = rate;
            Mutate_Bool(mreg, &bf, 1);

            s = prefix;
            s += "doAttack";
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
    }
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
    f.rangeFilter.pos = &self->pos;
    f.rangeFilter.radius = desc->radius;
    f.useFlags = FALSE;

    ASSERT(desc->forceType == NEURAL_FORCE_SEPARATE ||
           desc->forceType == NEURAL_FORCE_FORWARD_SEPARATE ||
           desc->forceType == NEURAL_FORCE_BACKWARD_SEPARATE);

    if (desc->forceType != NEURAL_FORCE_SEPARATE) {
        FRPoint dir;
        NeuralForceDesc headDesc;

        MBUtil_Zero(&headDesc, sizeof(headDesc));
        headDesc.forceType = NEURAL_FORCE_HEADING;
        headDesc.useTangent = FALSE;
        headDesc.radius = 1.0f;
        NeuralForce_GetForce(nc, self, &headDesc, &dir);

        f.dirFilter.pos = &self->pos;
        f.dirFilter.forward = desc->forceType == NEURAL_FORCE_FORWARD_SEPARATE ?
                              TRUE : FALSE;
        f.dirFilter.dir = &dir;
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
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

            if (rPos.radius < MICRON) {
                rPos.radius = 1.0f;
                rPos.theta = RandomState_Float(nc->rs, 0, M_PI * 2.0f);
            }
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
        case NEURAL_FORCE_ALIGN2:
        case NEURAL_FORCE_FORWARD_ALIGN:
        case NEURAL_FORCE_BACKWARD_ALIGN: {
            FPoint avgVel;
            MobFilter f;

            MBUtil_Zero(&f, sizeof(f));
            f.rangeFilter.pos = &mob->pos;
            f.rangeFilter.radius = desc->radius;
            f.useFlags = TRUE;
            f.flagsFilter = MOB_FLAG_FIGHTER;

            if (desc->forceType != NEURAL_FORCE_ALIGN2) {
                FRPoint dir;
                NeuralForceDesc headDesc;

                MBUtil_Zero(&headDesc, sizeof(headDesc));
                headDesc.forceType = NEURAL_FORCE_HEADING;
                headDesc.useTangent = FALSE;
                headDesc.radius = 1.0f;
                NeuralForce_GetForce(nc, mob, &headDesc, &dir);

                f.dirFilter.pos = &mob->pos;
                f.dirFilter.forward = desc->forceType == NEURAL_FORCE_FORWARD_ALIGN ?
                                      TRUE : FALSE;
                f.dirFilter.dir = &dir;
            }

            if (nc->sg->friendAvgVel(&avgVel, &f)) {
                if (avgVel.x >= MICRON || avgVel.y >= MICRON) {
                    avgVel.x += mob->pos.x;
                    avgVel.y += mob->pos.y;
                    *focusPoint = avgVel;
                    return TRUE;
                }
             }
             return FALSE;
         }
        case NEURAL_FORCE_COHERE:
        case NEURAL_FORCE_FORWARD_COHERE:
        case NEURAL_FORCE_BACKWARD_COHERE: {
             FPoint avgPos;
            MobFilter f;

            MBUtil_Zero(&f, sizeof(f));
            f.rangeFilter.pos = &mob->pos;
            f.rangeFilter.radius = desc->radius;
            f.useFlags = TRUE;
            f.flagsFilter = MOB_FLAG_FIGHTER;

            if (desc->forceType != NEURAL_FORCE_COHERE) {
                FRPoint dir;
                NeuralForceDesc headDesc;

                MBUtil_Zero(&headDesc, sizeof(headDesc));
                headDesc.forceType = NEURAL_FORCE_HEADING;
                headDesc.useTangent = FALSE;
                headDesc.radius = 1.0f;
                NeuralForce_GetForce(nc, mob, &headDesc, &dir);

                f.dirFilter.pos = &mob->pos;
                f.dirFilter.forward = desc->forceType == NEURAL_FORCE_FORWARD_COHERE ?
                                      TRUE : FALSE;
                f.dirFilter.dir = &dir;
            }

            if (nc->sg->friendAvgPos(&avgPos, &f)) {
                *focusPoint = avgPos;
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_ENEMY_COHERE: {
            FPoint avgPos;
            nc->sg->targetAvgPos(&avgPos, &mob->pos, desc->radius,
                                 MOB_FLAG_SHIP);
            *focusPoint = avgPos;
            return TRUE;
        }
        case NEURAL_FORCE_ENEMY_ALIGN: {
            FPoint avgVel;
            nc->sg->targetAvgVel(&avgVel, &mob->pos, desc->radius,
                                 MOB_FLAG_SHIP);
            if (avgVel.x >= MICRON || avgVel.y >= MICRON) {
                avgVel.x += mob->pos.x;
                avgVel.y += mob->pos.y;
                *focusPoint = avgVel;
                return TRUE;
            }
            return FALSE;
        }
        case NEURAL_FORCE_SEPARATE:
        case NEURAL_FORCE_FORWARD_SEPARATE:
        case NEURAL_FORCE_BACKWARD_SEPARATE:
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
        case NEURAL_FORCE_BASE_SHELL: {
            FRPoint rPoint;
            if (!NeuralForceGetFocusMobPosHelper(nc->sg->friendBase(),
                                                 focusPoint)) {
                return FALSE;
            }
            FPoint_ToFRPoint(&mob->pos, focusPoint, &rPoint);
            FRPoint_SetSpeed(&rPoint, desc->radius);
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

/*
 * NeuralForce_GetForce --
 *    Calculate the specified force.
 *    returns TRUE iff the force is valid.
 */
bool NeuralForce_GetForce(AIContext *nc,
                          Mob *mob,
                          NeuralForceDesc *desc,
                          FRPoint *rForce)
{
    FPoint focusPoint;

    if (NeuralForce_GetFocus(nc, mob, desc, &focusPoint)) {
        FPoint_ToFRPoint(&focusPoint, &mob->pos, rForce);
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


float NeuralForce_GetRange(AIContext *nc,
                           Mob *mob, NeuralForceDesc *desc) {
    FPoint focusPoint;
    if (NeuralForce_GetFocus(nc, mob, desc, &focusPoint)) {
        return FPoint_Distance(&mob->pos, &focusPoint);
    } else {
        return 0.0f;
    }
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
        NeuralForceDesc desc;
        MBUtil_Zero(&desc, sizeof(desc));
        desc.forceType = NEURAL_FORCE_HEADING;
        desc.useTangent = FALSE;
        desc.radius = speed;
        NeuralForce_GetForce(nc, mob, &desc, rForce);
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
                           Mob *mob, NeuralValueDesc *desc, uint i)
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
            seed = (seed << 32) | i;
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