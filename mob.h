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

typedef struct MobCmd {
    FPoint target;
    MobType spawnType;
} MobCmd;

typedef struct Mob {
    MobID id;
    MobType type;
    PlayerID playerID;
    bool alive;
    bool removeMob;
    FPoint pos;
    int fuel;
    int health;
    int age;
    int rechargeTime;
    int lootCredits;
    uint64 scannedBy;

    MobCmd cmd;
} Mob;

typedef struct SensorMob {
    MobID mobid;
    MobType type;
    PlayerID playerID;
    bool alive;
    FPoint pos;
} SensorMob;

DECLARE_MBVECTOR_TYPE(Mob, MobVector);
DECLARE_MBVECTOR_TYPE(SensorMob, SensorMobVector);

float MobType_GetSpeed(MobType type);
float MobType_GetRadius(MobType type);
float MobType_GetSensorRadius(MobType type);
int MobType_GetMaxFuel(MobType type);
int MobType_GetMaxHealth(MobType type);
int MobType_GetCost(MobType type);

void Mob_Init(Mob *mob, MobType t);
bool Mob_CheckInvariants(const Mob *mob);
void Mob_GetSensorCircle(const Mob *mob, FCircle *c);
void Mob_GetCircle(const Mob *mob, FCircle *c);

void SensorMob_InitFromMob(SensorMob *sm, const Mob *mob);

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

#endif // _MOB_H_202006041753
