/*
 * mob.c -- part of SpaceRobots2
 * Copyright (C) 2020 Michael Banack <github@banack.net>
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
    static struct {
        MobType type;
        float radius;
    } v[] = {
        { MOB_TYPE_INVALID,    0.0f, },
        { MOB_TYPE_BASE,      50.0f, },
        { MOB_TYPE_FIGHTER,    5.0f, },
        { MOB_TYPE_MISSILE,    3.0f, },
        { MOB_TYPE_LOOT_BOX ,  2.0f, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].radius;
}

float MobType_GetSensorRadius(MobType type)
{
    static struct {
        MobType type;
        float sensorRadius;
    } v[] = {
        { MOB_TYPE_INVALID,    0.0f, },
        { MOB_TYPE_BASE,     250.0f, },
        { MOB_TYPE_FIGHTER,   50.0f, },
        { MOB_TYPE_MISSILE,   30.0f, },
        { MOB_TYPE_LOOT_BOX ,  0.0f, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].sensorRadius;
}

float MobType_GetSpeed(MobType type)
{
    static struct {
        MobType type;
        float speed;
    } v[] = {
        { MOB_TYPE_INVALID,    0.0f, },
        { MOB_TYPE_BASE,       0.0f, },
        { MOB_TYPE_FIGHTER,    2.5f, },
        { MOB_TYPE_MISSILE,    5.0f, },
        { MOB_TYPE_LOOT_BOX ,  0.5f, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].speed;
}

int MobType_GetCost(MobType type)
{
    static struct {
        MobType type;
        int cost;
    } v[] = {
        { MOB_TYPE_INVALID,    -1, },
        { MOB_TYPE_BASE,       -1, },
        { MOB_TYPE_FIGHTER,   100, },
        { MOB_TYPE_MISSILE,     1, },
        { MOB_TYPE_LOOT_BOX ,  -1, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].cost;
}

int MobType_GetMaxFuel(MobType type)
{
    static struct {
        MobType type;
        int fuel;
    } v[] = {
        { MOB_TYPE_INVALID,      -1, },
        { MOB_TYPE_BASE,         -1, },
        { MOB_TYPE_FIGHTER,      -1, },
        { MOB_TYPE_MISSILE,      14, },
        { MOB_TYPE_LOOT_BOX ,  4000, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].type);
    return v[type].fuel;
}


int MobType_GetMaxHealth(MobType type)
{
    static struct {
        MobType type;
        int health;
    } v[] = {
        { MOB_TYPE_INVALID,      -1, },
        { MOB_TYPE_BASE,         50, },
        { MOB_TYPE_FIGHTER,       1, },
        { MOB_TYPE_MISSILE,       1, },
        { MOB_TYPE_LOOT_BOX ,     1, },
    };

    ASSERT(ARRAYSIZE(v) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(v));
    ASSERT(type == v[type].fuel);
    return v[type].health;
}

void Mob_Init(Mob *mob, MobType t)
{
    MBUtil_Zero(mob, sizeof(*mob));
    mob->image = MOB_IMAGE_FULL;
    mob->alive = TRUE;
    mob->playerID = PLAYER_ID_INVALID;
    mob->mobid = MOB_ID_INVALID;
    mob->type = t;
    mob->fuel = MobType_GetMaxFuel(t);
    mob->health = MobType_GetMaxHealth(t);
    mob->cmd.spawnType = MOB_TYPE_INVALID;
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
    ASSERT(mob->health == 0);
    ASSERT(mob->birthTick == 0);
    ASSERT(mob->rechargeTime == 0);
    ASSERT(mob->lootCredits == 0);
    ASSERT(MBUtil_IsZero(&mob->cmd, sizeof(mob->cmd)));
}

void MobSet_Create(MobSet *ms)
{
    ASSERT(ms != NULL);
    IntMap_Create(&ms->map);
    IntMap_SetEmptyValue(&ms->map, -1);
    MobPVec_CreateEmpty(&ms->pv);
}

void MobSet_Destroy(MobSet *ms)
{
    ASSERT(ms != NULL);
    IntMap_Destroy(&ms->map);
    MobPVec_Destroy(&ms->pv);
}

void MobSet_MakeEmpty(MobSet *ms)
{
    ASSERT(ms != NULL);
    IntMap_MakeEmpty(&ms->map);
    MobPVec_MakeEmpty(&ms->pv);
}

void MobSet_Add(MobSet *ms, Mob *mob)
{
    ASSERT(mob != NULL);

    int oldIndex = IntMap_Get(&ms->map, mob->mobid);
    if (oldIndex == -1) {
        int oldSize = MobPVec_Size(&ms->pv);
        MobPVec_Grow(&ms->pv);
        oldIndex = oldSize;
        IntMap_Put(&ms->map, mob->mobid, oldIndex);
    }
    MobPVec_PutValue(&ms->pv, oldIndex, mob);
}

Mob *MobSet_Get(MobSet *ms, MobID mobid)
{
    int index = IntMap_Get(&ms->map, mobid);
    if (index == -1) {
        return NULL;
    }
    return MobPVec_GetValue(&ms->pv, index);
}

void MobSet_Remove(MobSet *ms, MobID mobid)
{
    int index = IntMap_Get(&ms->map, mobid);
    if (index == -1) {
        return;
    }

    int size = MobPVec_Size(&ms->pv);
    Mob *last = MobPVec_GetValue(&ms->pv, size - 1);
    MobPVec_PutValue(&ms->pv, index, last);
    IntMap_Remove(&ms->map, mobid);
}

int MobSet_Size(MobSet *ms)
{
    return MobPVec_Size(&ms->pv);
}

void MobIt_Start(MobSet *ms, MobIt *mit)
{
    MBUtil_Zero(mit, sizeof(*mit));
    mit->ms = ms;
    mit->i = 0;
}

bool MobIt_HasNext(MobIt *mit)
{
    ASSERT(mit != NULL);
    return mit->i < MobPVec_Size(&mit->ms->pv);
}

Mob *MobIt_Next(MobIt *mit)
{
    ASSERT(MobIt_HasNext(mit));
    return MobPVec_GetValue(&mit->ms->pv, mit->i++);
}
