/*
 * battleTypes.h --
 */

#ifndef _BATTLE_TYPES_H_202006071525
#define _BATTLE_TYPES_H_202006071525

#include "geometry.h"

#define MICRON (0.1f)

#define MAX_PLAYERS 8
typedef uint32 PlayerID;
#define PLAYER_ID_INVALID ((uint32)-1)
#define PLAYER_ID_NEUTRAL (0)

typedef uint32 MobID;
#define MOB_ID_INVALID ((uint32)-1)

typedef enum MobType {
    MOB_TYPE_INVALID  = 0,
    MOB_TYPE_BASE     = 1,
    MOB_TYPE_MIN      = 1,
    MOB_TYPE_FIGHTER  = 2,
    MOB_TYPE_MISSILE  = 3,
    MOB_TYPE_LOOT_BOX = 4,
    MOB_TYPE_MAX,
} MobType;

typedef enum FleetAIType {
    FLEET_AI_INVALID = 0,
    FLEET_AI_NEUTRAL = 1,
    FLEET_AI_DUMMY   = 2,
    FLEET_AI_SIMPLE  = 3,
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

    // Factor of cost that gets dropped when a mob is destroyed.
    float lootDropRate;

    // How quickly neutral loot spawns, in credits / tick.
    float lootSpawnRate;
    int minLootSpawn;
    int maxLootSpawn;
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
