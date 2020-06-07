/*
 * battle.h --
 */

#ifndef _BATTLE_H_202005310644
#define _BATTLE_H_202005310644

#include "geometry.h"
#include "mob.h"
#include "fleet.h"

typedef struct BattlePlayerParams {
    const char *playerName;
    FleetAIType aiType;
} BattlePlayerParams;

typedef struct BattleParams {
    BattlePlayerParams players[MAX_PLAYERS];
    uint numPlayers;
    uint width;
    uint height;
    uint startingCredits;
    uint creditsPerTick;
} BattleParams;

typedef struct BattlePlayerStatus {
    bool alive;
    int credits;
} BattlePlayerStatus;

typedef struct BattleStatus {
    bool finished;
    uint tick;

    BattlePlayerStatus players[MAX_PLAYERS];
    uint numPlayers;

    int collisions;
    int sensorContacts;
    int spawns;
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
