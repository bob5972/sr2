/*
 * mob.h --
 */

#ifndef _MOB_H_202006041753
#define _MOB_H_202006041753

#include "geometry.h"
#include "MBVector.h"

#define MAX_PLAYERS 8
typedef uint32 PlayerID;
#define PLAYER_ID_INVALID ((uint32)-1)

typedef uint32 MobID;
#define MOB_ID_INVALID ((uint32)-1)

typedef enum MobType {
    MOB_TYPE_INVALID = 0,
    MOB_TYPE_BASE    = 1,
    MOB_TYPE_MIN     = 1,
    MOB_TYPE_FIGHTER = 2,
    MOB_TYPE_MISSILE = 3,
    MOB_TYPE_MAX,
} MobType;

typedef struct MobCmd {
    FPoint target;
    MobType spawn;
} MobCmd;

typedef struct Mob {
    MobID id;
    MobType type;
    PlayerID playerID;
    bool alive;
    bool removeMob;
    FPoint pos;
    int fuel;
    uint64 scannedBy;

    MobCmd cmd;
} Mob;

typedef struct SensorMob {
    MobID mobid;
    MobType type;
    PlayerID playerID;
    bool alive;
    FPoint pos;
    int fuel;
} SensorMob;

DECLARE_MBVECTOR_TYPE(Mob, MobVector);
DECLARE_MBVECTOR_TYPE(SensorMob, SensorMobVector);

float MobType_GetSpeed(MobType type);
float MobType_GetRadius(MobType type);
float MobType_GetSensorRadius(MobType type);
int MobType_GetMaxFuel(MobType type);
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
