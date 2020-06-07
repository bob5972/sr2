/*
 * battle.h --
 */

#ifndef _BATTLE_H_202005310644
#define _BATTLE_H_202005310644

#include "geometry.h"
#include "mob.h"
#include "fleet.h"

#define MAX_PLAYERS 8
typedef uint32 PlayerID;

typedef struct BattlePlayer {
    const char *playerName;
    FleetAIType aiType;
} BattlePlayer;

typedef struct BattleParams {
    BattlePlayer players[MAX_PLAYERS];
    uint32 numPlayers;
    uint32 width;
    uint32 height;
} BattleParams;

typedef struct BattleStatus {
    bool finished;
    uint32 tick;
    uint32 collisions;
    uint32 sensorContacts;
    uint32 spawns;
} BattleStatus;

void Battle_Init(const BattleParams *bp);
void Battle_Exit();
void Battle_RunTick();
const BattleParams *Battle_GetParams();
Mob *Battle_AcquireMobs(uint32 *numMobs);
void Battle_ReleaseMobs();
const BattleStatus *Battle_AcquireStatus();
void Battle_ReleaseStatus();


#endif // _BATTLE_H_202005310644
