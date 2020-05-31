/*
 * battle.h --
 */

#ifndef _BATTLE_H_202005310644
#define _BATTLE_H_202005310644

#include "geometry.h"

typedef struct BattleParams {
    uint32 width;
    uint32 height;
} BattleParams;

typedef struct BattleStatus {
    bool finished;
    uint32 tick;
    uint32 targetsReached;
    uint32 collisions;
} BattleStatus;

typedef struct BattleMob {
    bool alive;
    FQuad pos;
    FPoint target;
} BattleMob;

void Battle_Init(const BattleParams *bp);
void Battle_Exit();
void Battle_RunTick();
const BattleMob *Battle_AcquireMobs(uint32 *numMobs);
void Battle_ReleaseMobs();
const BattleStatus *Battle_AcquireStatus();
void Battle_ReleaseStatus();


#endif // _BATTLE_H_202005310644
