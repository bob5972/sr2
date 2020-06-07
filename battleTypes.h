/*
 * battleTypes.h --
 */

#ifndef _BATTLE_TYPES_H_202006071525
#define _BATTLE_TYPES_H_202006071525

#include "geometry.h"

typedef enum FleetAIType {
    FLEET_AI_INVALID = 0,
    FLEET_AI_DUMMY   = 1,
    FLEET_AI_SIMPLE  = 2,
    FLEET_AI_MAX,
} FleetAIType;

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
    uint timeLimit;
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

#endif // _BATTLE_TYPES_H_202006071525
