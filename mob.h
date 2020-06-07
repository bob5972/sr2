/*
 * mob.h --
 */

#ifndef _MOB_H_202006041753
#define _MOB_H_202006041753

#include "geometry.h"

typedef uint32 PlayerID;

typedef uint32 MobID;
#define MOB_INVALID_ID ((uint32)-1)

typedef enum MobType {
    MOB_TYPE_INVALID = 0,
    MOB_TYPE_BASE    = 1,
    MOB_TYPE_MIN     = 1,
    MOB_TYPE_FIGHTER = 2,
    MOB_TYPE_ROCKET  = 3,
    MOB_TYPE_MAX,
} MobType;

typedef struct MobCmd {
    FPoint target;
} MobCmd;

typedef struct Mob {
    MobID id;
    MobType type;
    PlayerID playerID;
    bool alive;
    FPoint pos;
    MobCmd cmd;
    int fuel;
} Mob;

void Mob_GetCircle(const Mob *mob, FCircle *q);
float Mob_GetSpeed(const Mob *mob);
float MobType_GetRadius(MobType type);
uint Mob_GetMaxFuel(const Mob *mob);
uint MobType_GetMaxFuel(MobType type);

#endif // _MOB_H_202006041753
