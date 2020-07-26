/*
 * mob.h -- part of SpaceRobots2
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

#ifndef _MOB_H_202006041753
#define _MOB_H_202006041753

#include "geometry.h"
#include "MBVector.h"
#include "battleTypes.h"

float MobType_GetSpeed(MobType type);
float MobType_GetSensorRadius(MobType type);
float MobType_GetRadius(MobType type);
int MobType_GetMaxFuel(MobType type);
int MobType_GetMaxHealth(MobType type);
int MobType_GetCost(MobType type);

void MobSet_Create(MobSet *ms);
void MobSet_Destroy(MobSet *ms);
void MobSet_MakeEmpty(MobSet *ms);
void MobSet_Add(MobSet *ms, Mob *mob);
Mob *MobSet_Get(MobSet *ms, MobID mobid);
void MobSet_Remove(MobSet *ms, MobID mobid);
int MobSet_Size(MobSet *ms);

void MobIt_Start(MobSet *ms, MobIt *mit);
bool MobIt_HasNext(MobIt *mit);
Mob *MobIt_Next(MobIt *mit);
void MobIt_Remove(MobIt *mit);

void Mob_Init(Mob *mob, MobType t);

void Mob_MaskForSensor(Mob *mob);
void Mob_MaskForAI(Mob *mob);

static inline float Mob_GetRadius(const Mob *mob)
{
    ASSERT(mob != NULL);
    ASSERT(mob->radius == MobType_GetRadius(mob->type));
    return mob->radius;
}

static inline float Mob_GetSensorRadius(const Mob *mob)
{
    ASSERT(mob != NULL);
    ASSERT(mob->sensorRadius == MobType_GetSensorRadius(mob->type));
    return mob->sensorRadius;
}

static inline float Mob_GetSpeed(const Mob *mob)
{
    ASSERT(mob != NULL);
    return MobType_GetSpeed(mob->type);
}

static inline uint Mob_GetMaxFuel(const Mob *mob)
{
    ASSERT(mob != NULL);
    return MobType_GetMaxFuel(mob->type);
}

static inline void Mob_GetCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;
    c->radius = Mob_GetRadius(mob);
}


static inline void Mob_GetSensorCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;
    c->radius = MobType_GetSensorRadius(mob->type);
}


static inline bool Mob_CheckInvariants(const Mob *m)
{
    if (DEBUG) {
        ASSERT(m != NULL);

        ASSERT(m->mobid != MOB_ID_INVALID);

        ASSERT(m->type != MOB_TYPE_INVALID);
        ASSERT(m->type >= MOB_TYPE_MIN);
        ASSERT(m->type < MOB_TYPE_MAX);

        ASSERT(m->image != MOB_IMAGE_INVALID);
        ASSERT(m->image >= MOB_IMAGE_MIN);
        ASSERT(m->image < MOB_IMAGE_MAX);

        ASSERT(m->playerID != PLAYER_ID_INVALID);

        if (m->image == MOB_IMAGE_FULL) {
            ASSERT(!(m->removeMob && m->alive));
        }

        if (m->image == MOB_IMAGE_FULL ||
            m->image == MOB_IMAGE_AI) {
            ASSERT(m->fuel <= MobType_GetMaxFuel(m->type));
            ASSERT(m->health <= MobType_GetMaxHealth(m->type));
        }
    }

    return TRUE;
}

#endif // _MOB_H_202006041753
