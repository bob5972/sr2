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
#include "MBCompare.h"

#define MOB_FIGHTER_SENSOR_RADIUS (50.0f)

float MobType_GetSpeed(MobType type);
float MobType_GetSensorRadius(MobType type);
float MobType_GetRadius(MobType type);
int MobType_GetMaxFuel(MobType type);
int MobType_GetMaxHealth(MobType type);
int MobType_GetCost(MobType type);

/*
 * Tracks a set of mob pointers indexed by MobID.
 */
void MobPSet_Create(MobPSet *ms);
void MobPSet_Destroy(MobPSet *ms);
void MobPSet_MakeEmpty(MobPSet *ms);
void MobPSet_Add(MobPSet *ms, Mob *mob);
Mob *MobPSet_Get(MobPSet *ms, MobID mobid);
void MobPSet_Remove(MobPSet *ms, MobID mobid);
int MobPSet_Size(MobPSet *ms);
void MobPSet_UnitTest();

void CMobIt_Start(MobPSet *ms, CMobIt *mit);
bool CMobIt_HasNext(CMobIt *mit);
Mob *CMobIt_Next(CMobIt *mit);
void CMobIt_Remove(CMobIt *mit);

/**
 * Construct comparator to sort array of <Mob *> by distance from pos.
 */
void Mob_InitDistanceComparator(CMBComparator *comp, const FPoint *pos);

/**
 * Construct comparator to sort array of <Mob **> by distance from pos.
 */
void MobP_InitDistanceComparator(CMBComparator *comp, const FPoint *pos);

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

static inline bool Mob_CanScanPoint(const Mob *mob, const FPoint *p)
{
    FCircle c;
    Mob_GetSensorCircle(mob, &c);
    return FCircle_ContainsPoint(&c, p);
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
