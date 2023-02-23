/*
 * mob.h -- part of SpaceRobots2
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

#ifndef _MOB_H_202006041753
#define _MOB_H_202006041753

#ifdef __cplusplus
	extern "C" {
#endif

#include "MBConfig.h"
#include "geometry.h"
#include "MBVector.h"
#include "battleTypes.h"
#include "MBCompare.h"

typedef struct MobTypeData {
    MobType type;
    float radius;
    float sensorRadius;
    float speed;
    int cost;
    int maxFuel;
    int rechargeTicks;
    int maxHealth;
} MobTypeData;

extern const MobTypeData gMobTypeData[MOB_TYPE_MAX];

#define MOB_FIGHTER_SENSOR_RADIUS (50.0f)
#define MOB_FIGHTER_SPEED         (2.5f)

static inline const MobTypeData *MobType_GetData(MobType type)
{
    ASSERT(ARRAYSIZE(gMobTypeData) == MOB_TYPE_MAX);
    ASSERT(type != MOB_TYPE_INVALID);
    ASSERT(type < ARRAYSIZE(gMobTypeData));
    ASSERT(type == gMobTypeData[type].type);
    return &gMobTypeData[type];
}

static inline float MobType_GetRadius(MobType type)
{
    return MobType_GetData(type)->radius;
}

static inline float MobType_GetSensorRadius(MobType type)
{
    ASSERT(MOB_FIGHTER_SENSOR_RADIUS ==
           gMobTypeData[MOB_TYPE_FIGHTER].sensorRadius);
    return MobType_GetData(type)->sensorRadius;
}

static inline float MobType_GetSpeed(MobType type)
{
    ASSERT(MOB_FIGHTER_SPEED ==
           gMobTypeData[MOB_TYPE_FIGHTER].speed);
    return MobType_GetData(type)->speed;
}

static inline int MobType_GetCost(MobType type)
{
    return MobType_GetData(type)->cost;
}

static inline int MobType_GetRechargeTicks(MobType type)
{
    return MobType_GetData(type)->rechargeTicks;
}

static inline int MobType_GetMaxFuel(MobType type)
{
    return MobType_GetData(type)->maxFuel;
}

static inline int MobType_GetMaxHealth(MobType type)
{
    return MobType_GetData(type)->maxHealth;
}

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

static inline Mob *MobPSet_GetIndex(MobPSet *ms, int i)
{
    return MobPVec_GetValue(&ms->pv, i);
}

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

static inline void Mob_MaskForAI(Mob *mob)
{
    ASSERT(mob != NULL);

    ASSERT(mob->image == MOB_IMAGE_FULL);
    mob->image = MOB_IMAGE_AI;

    MBUtil_Zero(&mob->privateFields,
                sizeof(*mob) - OFFSETOF(Mob, privateFields));
    ASSERT(mob->removeMob == 0);
    ASSERT(mob->scannedBy == 0);
}

static inline void Mob_MaskForSensor(Mob *mob)
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

static inline float Mob_GetRadius(const Mob *mob)
{
    ASSERT(mob != NULL);
    return MobType_GetRadius(mob->type);
}

static inline float Mob_GetSensorRadius(const Mob *mob)
{
    ASSERT(mob != NULL);
    return MobType_GetSensorRadius(mob->type);
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

static inline bool Mob_IsAmmo(const Mob *mob)
{
    uint flag = (1 << mob->type);
    return (MOB_FLAG_AMMO & flag) != 0;

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
    if (mb_debug) {
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

#ifdef __cplusplus
    }
#endif

#endif // _MOB_H_202006041753
