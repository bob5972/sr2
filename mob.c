/*
 * mob.c -- part of SpaceRobots2
 * Copyright (C) 2020-2023 Michael Banack <github@banack.net>
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

const MobTypeData gMobTypeData[MOB_TYPE_MAX] = {
    // type               radius sensor  speed cost fuel  recharge health
    { MOB_TYPE_INVALID,    0.0f,   0.0f, 0.0f,  -1,   -1,  -1,      -1,  },
    { MOB_TYPE_BASE,      50.0f, 250.0f, 0.0f,  -1,   -1,  50,      50,  },
    { MOB_TYPE_FIGHTER,    5.0f,  50.0f, 2.5f, 100,   -1,   5,       1,  },
    { MOB_TYPE_MISSILE,    3.0f,  30.0f, 5.0f,   5,   14,  -1,       1,  },
    { MOB_TYPE_POWER_CORE, 2.0f,   0.0f, 0.5f,  -1, 4000,  -1,       1,  },
};

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
