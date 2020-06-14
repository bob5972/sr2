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

#define SPEED_SCALE (1.0f)
#define SIZE_SCALE (1.0f)

float MobType_GetRadius(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return 50.0f * SIZE_SCALE;
            break;
        case MOB_TYPE_FIGHTER:
            return 5.0f * SIZE_SCALE;
            break;
        case MOB_TYPE_MISSILE:
            return 3.0f * SIZE_SCALE;
            break;
        case MOB_TYPE_LOOT_BOX:
            return 2.0f * SIZE_SCALE;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}

float MobType_GetSensorRadius(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return 250.0f * SIZE_SCALE;
            break;
        case MOB_TYPE_FIGHTER:
            return 50.0f * SIZE_SCALE;
            break;
        case MOB_TYPE_MISSILE:
            return 30.0f * SIZE_SCALE;
            break;
        case MOB_TYPE_LOOT_BOX:
            return 0.0f * SIZE_SCALE;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }
}

void Mob_GetCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;
    c->radius = MobType_GetRadius(mob->type);
}


void Mob_GetSensorCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;
    c->radius = MobType_GetSensorRadius(mob->type);
}

float MobType_GetSpeed(MobType type)
{
    float speed;

    switch (type) {
        case MOB_TYPE_BASE:
            speed = 0.0f * SPEED_SCALE;
            break;
        case MOB_TYPE_FIGHTER:
            speed = 2.5f * SPEED_SCALE;
            break;
        case MOB_TYPE_MISSILE:
            speed = 5.0f * SPEED_SCALE;
            break;
        case MOB_TYPE_LOOT_BOX:
            speed = 0.5f * SPEED_SCALE;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    ASSERT(MobType_GetRadius(type) >= SPEED_SCALE);
    return speed;
}

int MobType_GetCost(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return -1;
        case MOB_TYPE_FIGHTER:
            return 100;
        case MOB_TYPE_MISSILE:
            return 1;
        case MOB_TYPE_LOOT_BOX:
            return -1;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}

int MobType_GetMaxFuel(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return -1;
        case MOB_TYPE_FIGHTER:
            return -1;
        case MOB_TYPE_MISSILE:
            return 14;
        case MOB_TYPE_LOOT_BOX:
            return 4 * 1000;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}


int MobType_GetMaxHealth(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return 50;
        case MOB_TYPE_FIGHTER:
            return 1;
        case MOB_TYPE_MISSILE:
            return 1;
        case MOB_TYPE_LOOT_BOX:
            return 1;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
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

bool Mob_CheckInvariants(const Mob *m)
{
    ASSERT(m != NULL);
    ASSERT(m->mobid != MOB_ID_INVALID);
    ASSERT(m->type != MOB_TYPE_INVALID);
    ASSERT(m->type >= MOB_TYPE_MIN);
    ASSERT(m->type < MOB_TYPE_MAX);
    ASSERT(m->playerID != PLAYER_ID_INVALID);
    ASSERT(!(m->removeMob && m->alive));
    ASSERT(m->fuel <= MobType_GetMaxFuel(m->type));
    ASSERT(m->health <= MobType_GetMaxHealth(m->type));
    return TRUE;
}

void Mob_MaskForAI(Mob *mob)
{
    ASSERT(mob != NULL);

    ASSERT(mob->image == MOB_IMAGE_FULL);
    mob->image = MOB_IMAGE_AI;
    mob->removeMob = 0;
    mob->scannedBy = 0;
}

void Mob_MaskForSensor(Mob *mob)
{
    ASSERT(mob != NULL);

    Mob_MaskForAI(mob);

    ASSERT(mob->image == MOB_IMAGE_AI);
    mob->image = MOB_IMAGE_SENSOR;

    mob->fuel = 0;
    mob->health = 0;
    mob->birthTick = 0;
    mob->rechargeTime = 0;
    mob->lootCredits = 0;
    MBUtil_Zero(&mob->cmd, sizeof(mob->cmd));
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
