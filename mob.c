/*
 * mob.c -- part of SpaceRobots2
 * Copyright (C) 2020-2021 Michael Banack <github@banack.net>
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

#include "mob.h"

float MobType_GetRadius(MobType type)
{
    static const struct {
        MobType type;
        float radius;
    } v[] = {
        { MOB_TYPE_INVALID,     0.0f, },
        { MOB_TYPE_BASE,       50.0f, },
        { MOB_TYPE_FIGHTER,     5.0f, },
        { MOB_TYPE_MISSILE,     3.0f, },
        { MOB_TYPE_POWER_CORE,  2.0f, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].radius;
}

float MobType_GetSensorRadius(MobType type)
{
    static const struct {
        MobType type;
        float sensorRadius;
    } v[] = {
        { MOB_TYPE_INVALID,      0.0f, },
        { MOB_TYPE_BASE,       250.0f, },
        { MOB_TYPE_FIGHTER,    MOB_FIGHTER_SENSOR_RADIUS, },
        { MOB_TYPE_MISSILE,     30.0f, },
        { MOB_TYPE_POWER_CORE ,  0.0f, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].sensorRadius;
}

float MobType_GetSpeed(MobType type)
{
    static const struct {
        MobType type;
        float speed;
    } v[] = {
        { MOB_TYPE_INVALID,      0.0f, },
        { MOB_TYPE_BASE,         0.0f, },
        { MOB_TYPE_FIGHTER,      2.5f, },
        { MOB_TYPE_MISSILE,      5.0f, },
        { MOB_TYPE_POWER_CORE ,  0.5f, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].speed;
}

int MobType_GetCost(MobType type)
{
    static const struct {
        MobType type;
        int cost;
    } v[] = {
        { MOB_TYPE_INVALID,      -1, },
        { MOB_TYPE_BASE,         -1, },
        { MOB_TYPE_FIGHTER,     100, },
        { MOB_TYPE_MISSILE,       5, },
        { MOB_TYPE_POWER_CORE ,  -1, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].cost;
}

int MobType_GetRechargeTicks(MobType type)
{
    if (type == MOB_TYPE_BASE) {
        return 50;
    }

    return 5;
}

int MobType_GetMaxFuel(MobType type)
{
    static const struct {
        MobType type;
        int fuel;
    } v[] = {
        { MOB_TYPE_INVALID,        -1, },
        { MOB_TYPE_BASE,           -1, },
        { MOB_TYPE_FIGHTER,        -1, },
        { MOB_TYPE_MISSILE,        14, },
        { MOB_TYPE_POWER_CORE ,  4000, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].fuel;
}


int MobType_GetMaxHealth(MobType type)
{
    static const struct {
        MobType type;
        int health;
    } v[] = {
        { MOB_TYPE_INVALID,      -1, },
        { MOB_TYPE_BASE,         50, },
        { MOB_TYPE_FIGHTER,       1, },
        { MOB_TYPE_MISSILE,       1, },
        { MOB_TYPE_POWER_CORE ,   1, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].health;
}

void Mob_Init(Mob *mob, MobType t)
{
    MBUtil_Zero(mob, sizeof(*mob));
    mob->image = MOB_IMAGE_FULL;
    mob->alive = TRUE;
    mob->playerID = PLAYER_ID_INVALID;
    mob->mobid = MOB_ID_INVALID;
    mob->parentMobid = MOB_ID_INVALID;
    mob->type = t;
    mob->fuel = MobType_GetMaxFuel(t);
    mob->health = MobType_GetMaxHealth(t);
    mob->cmd.spawnType = MOB_TYPE_INVALID;
    mob->radius = MobType_GetRadius(t);
    mob->sensorRadius = MobType_GetSensorRadius(t);
}

void Mob_MaskForAI(Mob *mob)
{
    ASSERT(mob != NULL);

    ASSERT(mob->image == MOB_IMAGE_FULL);
    mob->image = MOB_IMAGE_AI;

    MBUtil_Zero(&mob->privateFields,
                sizeof(*mob) - OFFSETOF(Mob, privateFields));
    ASSERT(mob->removeMob == 0);
    ASSERT(mob->scannedBy == 0);
}

void Mob_MaskForSensor(Mob *mob)
{
    ASSERT(mob != NULL);

    Mob_MaskForAI(mob);

    ASSERT(mob->image == MOB_IMAGE_AI);
    mob->image = MOB_IMAGE_SENSOR;

    MBUtil_Zero(&mob->protectedFields,
                sizeof(*mob) - OFFSETOF(Mob, protectedFields));

    ASSERT(mob->fuel == 0);
    ASSERT(mob->birthTick == 0);
    ASSERT(mob->rechargeTime == 0);
    ASSERT(mob->powerCoreCredits == 0);
    ASSERT(MBUtil_IsZero(&mob->cmd, sizeof(mob->cmd)));
}

void MobPSet_Create(MobPSet *ms)
{
    ASSERT(ms != NULL);
    CMBIntMap_Create(&ms->map);
    CMBIntMap_SetEmptyValue(&ms->map, -1);
    MobPVec_CreateEmpty(&ms->pv);
}

void MobPSet_Destroy(MobPSet *ms)
{
    ASSERT(ms != NULL);
    CMBIntMap_Destroy(&ms->map);
    MobPVec_Destroy(&ms->pv);
}

void MobPSet_MakeEmpty(MobPSet *ms)
{
    ASSERT(ms != NULL);
    CMBIntMap_MakeEmpty(&ms->map);
    MobPVec_MakeEmpty(&ms->pv);
}

void MobPSet_Add(MobPSet *ms, Mob *mob)
{
    ASSERT(mob != NULL);

    int oldIndex = CMBIntMap_Get(&ms->map, mob->mobid);
    if (oldIndex == -1) {
        int oldSize = MobPVec_Size(&ms->pv);
        MobPVec_Grow(&ms->pv);
        oldIndex = oldSize;
        CMBIntMap_Put(&ms->map, mob->mobid, oldIndex);
    }
    MobPVec_PutValue(&ms->pv, oldIndex, mob);
}

Mob *MobPSet_Get(MobPSet *ms, MobID mobid)
{
    int index = CMBIntMap_Get(&ms->map, mobid);
    if (index == -1) {
        return NULL;
    }
    return MobPVec_GetValue(&ms->pv, index);
}

void MobPSet_Remove(MobPSet *ms, MobID mobid)
{
    int index = CMBIntMap_Get(&ms->map, mobid);
    if (index == -1) {
        return;
    }

    int size = MobPVec_Size(&ms->pv);
    if (size > 1) {
        Mob *last = MobPVec_GetValue(&ms->pv, size - 1);
        MobPVec_PutValue(&ms->pv, index, last);
        CMBIntMap_Put(&ms->map, last->mobid, index);
    }
    MobPVec_Shrink(&ms->pv);

    CMBIntMap_Remove(&ms->map, mobid);
}

int MobPSet_Size(MobPSet *ms)
{
    return MobPVec_Size(&ms->pv);
}

void CMobIt_Start(MobPSet *ms, CMobIt *mit)
{
    MBUtil_Zero(mit, sizeof(*mit));
    mit->ms = ms;
    mit->i = 0;
    mit->lastMobid = MOB_ID_INVALID;
}

bool CMobIt_HasNext(CMobIt *mit)
{
    ASSERT(mit != NULL);
    ASSERT(mit->i >= 0);
    return mit->i < MobPVec_Size(&mit->ms->pv);
}

Mob *CMobIt_Next(CMobIt *mit)
{
    Mob *mob;
    ASSERT(CMobIt_HasNext(mit));

    mob = MobPVec_GetValue(&mit->ms->pv, mit->i);
    mit->i++;

    ASSERT(mob != NULL);
    mit->lastMobid = mob->mobid;

    ASSERT(MobPSet_Get(mit->ms, mob->mobid) == mob);
    return mob;
}

void CMobIt_Remove(CMobIt *mit)
{
    MobID mobid = mit->lastMobid;
    ASSERT(mit->i > 0);
    ASSERT(mobid != MOB_ID_INVALID);
    ASSERT(MobPSet_Get(mit->ms, mobid) != NULL);

    mit->i--;
    MobPSet_Remove(mit->ms, mobid);
    mit->lastMobid = MOB_ID_INVALID;

    ASSERT(MobPSet_Get(mit->ms, mobid) == NULL);
}


void MobPSet_UnitTest()
{
    Mob mobs[100];
    MobPSet ms;
    CMobIt mit;

    for (uint i = 0; i < ARRAYSIZE(mobs); i++) {
        mobs[i].mobid = i;
    }

    MobPSet_Create(&ms);
    MobPSet_Destroy(&ms);

    MobPSet_Create(&ms);
    MobPSet_Add(&ms, &mobs[1]);
    ASSERT(MobPSet_Get(&ms, 1) != NULL);
    ASSERT(MobPSet_Get(&ms, 1) != NULL);
    MobPSet_Add(&ms, &mobs[1]);
    ASSERT(MobPSet_Get(&ms, 1) != NULL);

    CMobIt_Start(&ms, &mit);
    while (CMobIt_HasNext(&mit)) {
        CMobIt_Next(&mit);
        CMobIt_Remove(&mit);
    }
    ASSERT(!CMobIt_HasNext(&mit));
    MobPSet_Destroy(&ms);

    MobPSet_Create(&ms);
    MobPSet_Add(&ms, &mobs[0]);
    ASSERT(MobPSet_Get(&ms, 0) != NULL);
    MobPSet_Remove(&ms, 0);
    ASSERT(MobPSet_Get(&ms, 0) == NULL);
    MobPSet_Add(&ms, &mobs[1]);
    ASSERT(MobPSet_Get(&ms, 1) != NULL);
    MobPSet_Add(&ms, &mobs[2]);
    ASSERT(MobPSet_Get(&ms, 1) != NULL);
    ASSERT(MobPSet_Get(&ms, 2) != NULL);
    MobPSet_Remove(&ms, 2);
    ASSERT(MobPSet_Get(&ms, 2) == NULL);
    MobPSet_Destroy(&ms);

    MobPSet_Create(&ms);
    for (uint i = 0; i < ARRAYSIZE(mobs); i++) {
        MobPSet_Add(&ms, &mobs[i]);

        CMobIt_Start(&ms, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);
            if (m->mobid % 2 == 0) {
                CMobIt_Remove(&mit);
            }
        }
        ASSERT(!CMobIt_HasNext(&mit));
    }
    MobPSet_Destroy(&ms);
}


static int MobDistanceComparatorFn(const void *lhs, const void *rhs,
                                   void *cbData)
{
    const Mob *l = lhs;
    const Mob *r = rhs;
    const FPoint *pos = cbData;

    ASSERT(Mob_CheckInvariants(l));
    ASSERT(Mob_CheckInvariants(r));

    float lDistance = FPoint_DistanceSquared(pos, &l->pos);
    float rDistance = FPoint_DistanceSquared(pos, &r->pos);

    if (lDistance < rDistance) {
        return -1;
    } else if (lDistance > rDistance) {
        return 1;
    } else {
        ASSERT(lDistance == rDistance);
        return 0;
    }
}

static int MobPDistanceComparatorFn(const void *lhs, const void *rhs,
                                    void *cbData)
{
    const Mob **l = (void *)lhs;
    const Mob **r = (void *)rhs;
    const FPoint *pos = cbData;

    return MobDistanceComparatorFn(*l, *r, (void *)pos);
}


void Mob_InitDistanceComparator(CMBComparator *comp, const FPoint *pos)
{
    ASSERT(comp != NULL);
    comp->compareFn = MobDistanceComparatorFn;
    comp->cbData = (void *)pos;
    comp->itemSize = sizeof(Mob);
}

void MobP_InitDistanceComparator(CMBComparator *comp, const FPoint *pos)
{
    ASSERT(comp != NULL);
    comp->compareFn = MobPDistanceComparatorFn;
    comp->cbData = (void *)pos;
    comp->itemSize = sizeof(Mob *);
}

bool Mob_Filter(const Mob *m, const MobFilter *f)
{
    ASSERT(m != NULL);
    ASSERT(f != NULL);

    if (f->flagsFilter.useFlags) {
        if (((1 << m->type) & f->flagsFilter.flags) == 0) {
            return FALSE;
        }
    }
    if (f->rangeFilter.useRange) {
        if (f->rangeFilter.radius <= 0.0f) {
            return FALSE;
        }
        if (FPoint_DistanceSquared(&f->rangeFilter.pos, &m->pos) >
            f->rangeFilter.radius * f->rangeFilter.radius) {
            return FALSE;
        }
    }
    if (f->fnFilter.func != NULL) {
        if (!f->fnFilter.func(f->fnFilter.cbData, m)) {
            return FALSE;
        }
    }
    if (f->dirFilter.useDir) {
        if (!FPoint_IsFacing(&m->pos, &f->dirFilter.pos, &f->dirFilter.dir,
                             f->dirFilter.forward)) {
            return FALSE;
        }
    }
    if (f->dirFPointFilter.useDir) {
        if (!FPoint_IsFacingFPointVec(&m->pos, &f->dirFPointFilter.pos,
                                      &f->dirFPointFilter.dir,
                                      f->dirFPointFilter.forward)) {
            return FALSE;
        }
    }

    return TRUE;
}
