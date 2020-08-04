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
#include "IntMap.h"
#include "MBRegistry.h"

#define MICRON (0.1f)

/*
 * PlayerID's are relative to a single scenario.
 * PlayerUID's are consistent across multiple scenarios in a single run.
 */
#define MAX_PLAYERS (16)
typedef uint32 PlayerID;
typedef uint32 PlayerUID;
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

#define MOB_FLAG_BASE     (1 << MOB_TYPE_BASE)
#define MOB_FLAG_FIGHTER  (1 << MOB_TYPE_FIGHTER)
#define MOB_FLAG_MISSILE  (1 << MOB_TYPE_MISSILE)
#define MOB_FLAG_LOOT_BOX (1 << MOB_TYPE_LOOT_BOX)
#define MOB_FLAG_AMMO     (MOB_FLAG_MISSILE | MOB_FLAG_LOOT_BOX)
#define MOB_FLAG_SHIP     (MOB_FLAG_BASE | MOB_FLAG_FIGHTER)
#define MOB_FLAG_ALL      (MOB_FLAG_SHIP | MOB_FLAG_MISSILE | MOB_FLAG_LOOT_BOX)
typedef uint MobTypeFlags;

typedef struct MobCmd {
    FPoint target;
    MobType spawnType;
} MobCmd;

typedef enum MobImageType {
    MOB_IMAGE_INVALID = 0,
    MOB_IMAGE_FULL    = 1,
    MOB_IMAGE_MIN     = 1,
    MOB_IMAGE_AI      = 2,
    MOB_IMAGE_SENSOR  = 3,
    MOB_IMAGE_MAX,
} MobImageType;

typedef struct Mob {
    /*
     * Public fields that show up when a ship is scanned.
     */
    MobID mobid;
    MobType type;
    MobImageType image;
    PlayerID playerID;
    bool alive;
    FPoint pos;
    float radius;
    float sensorRadius;

    /*
     * Protected fields that are also used by the Fleet AIs.
     * (See Mob_MaskForSensor)
     */
    char protectedFields;
    void *aiMobHandle;
    int fuel;
    int health;
    uint birthTick;
    int rechargeTime;
    int lootCredits;
    MobID parentMobid;
    MobCmd cmd;

    /*
     * Private fields that are used only by the Battle engine.
     * (See Mob_MaskForAI)
     */
    char privateFields;
    bool removeMob;
    uint32 scannedBy;
} Mob;

DECLARE_CMBVECTOR_TYPE(Mob, MobVector);
DECLARE_CMBVECTOR_TYPE(Mob *, MobPVec);

typedef struct MobPSet {
    CIntMap map;
    MobPVec pv;
} MobPSet;

typedef struct MobIt {
    MobPSet *ms;
    int i;
    MobID lastMobid;
} MobIt;

typedef enum FleetAIType {
    FLEET_AI_INVALID = 0,
    FLEET_AI_NEUTRAL = 1,
    FLEET_AI_DUMMY   = 2,
    FLEET_AI_SIMPLE  = 3,
    FLEET_AI_BOB     = 4,
    FLEET_AI_MAPPER  = 5,
    FLEET_AI_CLOUD   = 6,
    FLEET_AI_GATHER  = 7,
    FLEET_AI_COWARD  = 8,
    FLEET_AI_FF      = 9,
    FLEET_AI_MAX,
} FleetAIType;

struct FleetAI;

typedef struct BattlePlayer {
    uint playerUID;
    const char *playerName;
    FleetAIType aiType;
    MBRegistry *mreg;
} BattlePlayer;

typedef struct BattleParams {
    uint numPlayers;
    uint width;
    uint height;
    uint startingCredits;
    uint creditsPerTick;
    uint tickLimit;
    bool restrictedStart;

    // Factor of cost that gets dropped when a mob is destroyed.
    float lootDropRate;

    // How quickly neutral loot spawns, in credits / tick.
    float lootSpawnRate;
    int minLootSpawn;
    int maxLootSpawn;

    uint startingBases;
    uint startingFighters;
} BattleParams;

typedef struct BattleScenario {
    BattleParams bp;
    BattlePlayer players[MAX_PLAYERS];
} BattleScenario;

typedef struct BattlePlayerStatus {
    uint playerUID;
    bool alive;
    int credits;
    uint numMobs;
} BattlePlayerStatus;

typedef struct BattleStatus {
    bool finished;
    uint tick;

    BattlePlayerStatus players[MAX_PLAYERS];
    uint numPlayers;
    PlayerID winner;
    PlayerUID winnerUID;

    int collisions;
    int sensorContacts;
    int spawns;
    int shipSpawns;
} BattleStatus;

typedef struct FleetAIOps {
    const char *aiName;
    const char *aiAuthor;
    void *(*createFleet)(struct FleetAI *ai);
    void (*destroyFleet)(void *aiHandle);
    void *(*mobSpawned)(void *aiHandle, Mob *m);
    void (*mobDestroyed)(void *aiHandle, void *aiMobHandle);
    void (*runAITick)(void *aiHandle);
    void (*runAIMob)(void *aiHandle, void *aiMobHandle);
} FleetAIOps;

typedef struct FleetAI {
    FleetAIOps ops;
    void *aiHandle;

    uint tick;
    PlayerID id;
    BattleParams bp;
    BattlePlayer player;
    uint64 seed;
    int credits;
    MobPSet mobs;
    MobPSet sensors;
} FleetAI;

#endif // _BATTLE_TYPES_H_202006071525
