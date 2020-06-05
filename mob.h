/*
 * mob.h --
 */

#ifndef _MOB_H_202006041753
#define _MOB_H_202006041753

#include "geometry.h"

typedef uint32 PlayerID;
typedef uint32 MobID;

typedef enum MobType {
    MOB_TYPE_INVALID = 0,
    MOB_TYPE_BASE    = 1,
    MOB_TYPE_MIN     = 1,
    MOB_TYPE_FIGHTER = 2,
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
} Mob;

void Mob_GetQuad(const Mob *mob, FQuad *q);
float Mob_GetSpeed(const Mob *mob);

#endif // _MOB_H_202006041753
