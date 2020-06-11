/*
 * battleTypes.h -- part of SpaceRobots2
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

typedef struct MobCmd {
    FPoint target;
    MobType spawnType;
} MobCmd;

typedef enum MobImageType {
    MOB_IMAGE_INVALID = 0,
    MOB_IMAGE_FULL    = 1,
    MOB_IMAGE_AI      = 2,
    MOB_IMAGE_SENSOR  = 3,
    MOB_IMAGE_MAX,
} MobImageType;

typedef struct Mob {
    /*
     * Public fields that show up when a ship is scanned.
     */
    MobID id;
    MobType type;
    MobImageType image;
    PlayerID playerID;
    bool alive;
    FPoint pos;

    /*
     * Protected fields that are also used by the Fleet AIs.
     * (See Mob_MaskForSensor)
     */
    int fuel;
    int health;
    int age;
    int rechargeTime;
    int lootCredits;
    MobCmd cmd;

    /*
     * Private fields that are used only by the Battle engine.
     * (See Mob_MaskForAI)
     */
    bool removeMob;
    uint64 scannedBy;
} Mob;

DECLARE_MBVECTOR_TYPE(Mob, MobVector);

typedef enum FleetAIType {
    FLEET_AI_INVALID = 0,
    FLEET_AI_NEUTRAL = 1,
    FLEET_AI_DUMMY   = 2,
    FLEET_AI_SIMPLE  = 3,
    FLEET_AI_MAX,
} FleetAIType;

struct FleetAI;

typedef struct BattlePlayerParams {
    const char *playerName;
    FleetAIType aiType;
} BattlePlayerParams;

typedef struct FleetAIOps {
    const char *aiName;
    const char *aiAuthor;
    void (*create)(struct FleetAI *ai);
    void (*destroy)(struct FleetAI *ai);
    void (*runAI)(struct FleetAI *ai);
} FleetAIOps;

typedef struct FleetAI {
    /*
     * Filled out by FleetAI controller.
     */
    FleetAIOps ops;
    void *aiHandle;

    /*
     * Filled out by Fleet
     */
    PlayerID id;
    BattlePlayerParams player;
    int credits;
    MobVector mobs;
    MobVector sensors;
} FleetAI;

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
