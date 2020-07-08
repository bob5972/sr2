/*
 * battle.c -- part of SpaceRobots2
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

#include "battle.h"
#include "mbbasic.h"
#include "random.h"
#include "BitVector.h"
#include "SDL_thread.h"
#include "workQueue.h"

#define SPAWN_RECHARGE_TICKS 5

#define BATTLE_USE_THREADS FALSE
#if BATTLE_USE_THREADS
#define BATTLE_NUM_THREADS 32
#else
#define BATTLE_NUM_THREADS 1
#endif

#define BATTLE_COLLISION_MIN_BATCH 2
#define BATTLE_SCANNING_MIN_BATCH  8

typedef enum BattleWorkType {
    BATTLE_WORK_TYPE_INVALID    = 0,
    BATTLE_WORK_TYPE_NOP        = 1,
    BATTLE_WORK_TYPE_EXIT       = 2,
    BATTLE_WORK_TYPE_SCAN       = 3,
    BATTLE_WORK_TYPE_COLLISION  = 4,
    BATTLE_WORK_TYPE_MAX,
} BattleWorkType;

typedef struct BattleWorkUnit {
    BattleWorkType type;

    union {
        struct {
            uint firstIndex;
            uint lastIndex;
        } scan;
         struct {
            uint firstIndex;
            uint lastIndex;
        } collision;
    };
} BattleWorkUnit;

typedef enum BattleResultType {
    BATTLE_RESULT_TYPE_INVALID   = 0,
    BATTLE_RESULT_TYPE_SCAN      = 1,
    BATTLE_RESULT_TYPE_COLLISION = 2,
    BATTLE_RESULT_TYPE_MAX,
} BattleResultType;

typedef struct BattleWorkResult {
    BattleResultType type;

    union {
        struct {
            uint scanningMobIndex;
            uint targetMobIndex;
        } scan;
        struct {
            uint lhsMobIndex;
            uint rhsMobIndex;
        } collision;
    };
} BattleWorkResult;

typedef struct BattleWorkerThreadData {
    uint id;
    char name[8];
    SDL_Thread *sdlThread;
} BattleWorkerThreadData;

typedef struct BattleGlobalData {
    bool initialized;

    BattleParams bp;
    bool paramsAcquired;

    BattleStatus bs;
    bool statusAcquired;

    float lootSpawnBucket;

    MobID lastMobID;
    MobVector mobs;
    bool mobsAcquired;

    MobVector pendingSpawns;

    BattleWorkerThreadData workerThreads[BATTLE_NUM_THREADS];
    WorkQueue workQueue;
    WorkQueue resultQueue;
} BattleGlobalData;

static BattleGlobalData battle;
static int BattleWorkerThreadMain(void *data);
static void BattleProcessScanning(uint firstIndex, uint lastIndex);
static void BattleProcessCollisions(uint firstIndex, uint lastIndex);

void Battle_Init(const BattleParams *bp)
{
    ASSERT(bp != NULL);
    MBUtil_Zero(&battle, sizeof(battle));

    /*
     * We need Neutral + 2 fleets.
     */
    ASSERT(bp->numPlayers >= 3);
    battle.bp = *bp;

    battle.bs.numPlayers = bp->numPlayers;
    for (uint32 i = 0; i < bp->numPlayers; i++) {
        battle.bs.players[i].playerName = bp->players[i].playerName;
        battle.bs.players[i].alive = TRUE;
        battle.bs.players[i].credits = bp->startingCredits;
    }
    battle.bs.winner = PLAYER_ID_NEUTRAL;

    ASSERT(bp->players[PLAYER_ID_NEUTRAL].aiType == FLEET_AI_NEUTRAL);
    MobVector_Create(&battle.mobs, 0, 1024);
    MobVector_CreateEmpty(&battle.pendingSpawns);

    for (uint i = 0; i < bp->numPlayers; i++) {
        if (i == PLAYER_ID_NEUTRAL) {
            continue;
        }

        MobVector_Grow(&battle.mobs);
        Mob *mob = MobVector_GetLastPtr(&battle.mobs);

        Mob_Init(mob, MOB_TYPE_BASE);
        mob->playerID = i;
        mob->mobid = ++battle.lastMobID;
        if (battle.bp.restrictedStart) {
            // account for NEUTRAL
            uint p = i - 1;
            float slotW = battle.bp.width / (bp->numPlayers - 1);
            mob->pos.x = Random_Float(p * slotW, (p + 1) * slotW);
            mob->pos.y = Random_Float(0.0f, battle.bp.height);
        } else {
            mob->pos.x = Random_Float(0.0f, battle.bp.width);
            mob->pos.y = Random_Float(0.0f, battle.bp.height);
        }
        mob->cmd.target = mob->pos;
    }

    WorkQueue_Create(&battle.resultQueue, sizeof(BattleWorkResult));
    WorkQueue_Create(&battle.workQueue, sizeof(BattleWorkUnit));

    for (uint i = 0; i < ARRAYSIZE(battle.workerThreads); i++) {
        BattleWorkerThreadData *tData = &battle.workerThreads[i];

        MBUtil_Zero(tData, sizeof(*tData));
        tData->id = i;
        snprintf(&tData->name[0], ARRAYSIZE(tData->name), "worker%d", i);
        tData->sdlThread = SDL_CreateThread(BattleWorkerThreadMain,
                                            tData->name, tData);
    }

    battle.initialized = TRUE;
}

void Battle_Exit()
{
    ASSERT(battle.initialized);

    for (uint i = 0; i < ARRAYSIZE(battle.workerThreads); i++) {
        BattleWorkUnit wu;
        wu.type = BATTLE_WORK_TYPE_EXIT;
        WorkQueue_QueueItem(&battle.workQueue, &wu, sizeof(wu));
    }

    for (uint i = 0; i < ARRAYSIZE(battle.workerThreads); i++) {
        BattleWorkerThreadData *tData = &battle.workerThreads[i];
        SDL_WaitThread(tData->sdlThread, NULL);

    }
    WorkQueue_Destroy(&battle.resultQueue);
    WorkQueue_Destroy(&battle.workQueue);

    MobVector_Destroy(&battle.mobs);
    MobVector_Destroy(&battle.pendingSpawns);
    battle.initialized = FALSE;
}

int BattleWorkerThreadMain(void *data)
{
    //BattleWorkerThreadData *tData = data;

    while (TRUE) {
        BattleWorkUnit wu;

        WorkQueue_WaitForItem(&battle.workQueue, &wu, sizeof(wu));
        ASSERT(wu.type != BATTLE_WORK_TYPE_INVALID);
        ASSERT(wu.type < BATTLE_WORK_TYPE_MAX);

        if (wu.type == BATTLE_WORK_TYPE_SCAN) {
            BattleProcessScanning(wu.scan.firstIndex, wu.scan.lastIndex);
        } else if (wu.type == BATTLE_WORK_TYPE_COLLISION) {
           BattleProcessCollisions(wu.collision.firstIndex,
                                   wu.collision.lastIndex);
        } else if (wu.type == BATTLE_WORK_TYPE_EXIT) {
            return 0;
        } else if (wu.type == BATTLE_WORK_TYPE_NOP) {
            // Do nothing...
        } else {
            NOT_IMPLEMENTED();
        }

        WorkQueue_FinishItem(&battle.workQueue);
    }

    NOT_REACHED();
}

static bool BattleCheckMobInvariants(const Mob *mob)
{
    ASSERT(Mob_CheckInvariants(mob));
    ASSERT(mob->pos.x >= 0.0f);
    ASSERT(mob->pos.y >= 0.0f);
    ASSERT(mob->pos.x <= (uint32)battle.bp.width);
    ASSERT(mob->pos.y <= (uint32)battle.bp.height);

    ASSERT(mob->cmd.target.x >= 0.0f);
    ASSERT(mob->cmd.target.y >= 0.0f);
    ASSERT(mob->cmd.target.x <= (uint32)battle.bp.width);
    ASSERT(mob->cmd.target.y <= (uint32)battle.bp.height);

    return TRUE;
}

static int BattleCalcLootCredits(const Mob *m)
{
    if (m->type == MOB_TYPE_MISSILE ||
        m->type == MOB_TYPE_LOOT_BOX) {
        return 0;
    }

    int loot = MobType_GetCost(m->type);
    return (int)(battle.bp.lootDropRate * loot);
}

static Mob *BattleQueueSpawn(MobType type, PlayerID p, const FPoint *pos)
{
    Mob *spawn;

    ASSERT(pos != NULL);
    MobVector_Grow(&battle.pendingSpawns);
    spawn = MobVector_GetLastPtr(&battle.pendingSpawns);

    Mob_Init(spawn, type);
    spawn->playerID = p;
    spawn->mobid = ++battle.lastMobID;
    spawn->pos = *pos;
    spawn->cmd.target = *pos;
    spawn->birthTick = battle.bs.tick;

    battle.bs.spawns++;
    if (spawn->type != MOB_TYPE_LOOT_BOX &&
        spawn->type != MOB_TYPE_MISSILE) {
        battle.bs.shipSpawns++;
    }

    return spawn;
}

static void BattleRunMobSpawn(Mob *mob)
{
    Mob *spawn;
    MobType mobType = mob->type;
    MobType spawnType = mob->cmd.spawnType;

    ASSERT(mob != NULL);
    ASSERT(spawnType == MOB_TYPE_INVALID ||
           spawnType >= MOB_TYPE_MIN);
    ASSERT(spawnType < MOB_TYPE_MAX);

    if (spawnType == MOB_TYPE_INVALID) {
        return;
    }

    ASSERT(mobType == MOB_TYPE_BASE ||
           mobType == MOB_TYPE_FIGHTER);

    if (!mob->alive) {
        return;
    }

    if (mobType == MOB_TYPE_BASE) {
        ASSERT(spawnType == MOB_TYPE_FIGHTER);
    }
    if (mobType == MOB_TYPE_FIGHTER) {
        ASSERT(spawnType == MOB_TYPE_MISSILE);
    }

    ASSERT(mob->playerID < ARRAYSIZE(battle.bs.players));
    if (battle.bs.players[mob->playerID].credits <
        MobType_GetCost(mob->cmd.spawnType)) {
        return;
    }
    if (mob->rechargeTime > 0) {
        mob->rechargeTime--;
        return;
    }

    battle.bs.players[mob->playerID].credits -=
        MobType_GetCost(mob->cmd.spawnType);
    spawn = BattleQueueSpawn(mob->cmd.spawnType, mob->playerID, &mob->pos);
    spawn->cmd.target = mob->cmd.target;
    mob->rechargeTime = SPAWN_RECHARGE_TICKS;
    mob->cmd.spawnType = MOB_TYPE_INVALID;
}

static void BattleRunMobMove(Mob *mob)
{
    FPoint origin;
    float distance;
    float speed;

    ASSERT(mob->alive);

    if (mob->playerID == PLAYER_ID_NEUTRAL) {
        /*
         * The neutral player never moves today.
         */
        ASSERT(mob->type == MOB_TYPE_LOOT_BOX);
        return;
    }

    origin.x = mob->pos.x;
    origin.y = mob->pos.y;
    distance = FPoint_Distance(&origin, &mob->cmd.target);

    speed = Mob_GetSpeed(mob);

    if (distance <= speed) {
        mob->pos = mob->cmd.target;
    } else {
        float dx = mob->cmd.target.x - mob->pos.x;
        float dy = mob->cmd.target.y - mob->pos.y;
        float factor = speed / distance;
        FPoint newPos;

        newPos.x = mob->pos.x + dx * factor;
        newPos.y = mob->pos.y + dy * factor;

//         Warning("tPos(%f, %f) pos(%f, %f) newPos(%f, %f)\n",
//                 mob->cmd.target.x, mob->cmd.target.y,
//                 mob->pos.x, mob->pos.y,
//                 newPos.x, newPos.y);
//         Warning("diffPos(%f, %f)\n",
//                 (newPos.x - mob->pos.x),
//                 (newPos.y - mob->pos.y));
//         Warning("distance=%f, speed+micron=%f, error=%f\n",
//                 (float)FPoint_Distance(&newPos, &mob->pos),
//                 (float)(speed + MICRON),
//                 (float)(FPoint_Distance(&newPos, &mob->pos) - (speed + MICRON)));

        //XXX: This ASSERT is hitting for resonable-seeming micron values...?
        ASSERT(FPoint_Distance(&newPos, &origin) <= speed + MICRON);
        mob->pos = newPos;
    }
    ASSERT(BattleCheckMobInvariants(mob));
}


static INLINE_ALWAYS bool
BattleCanMobTypesCollide(MobType lhsType, MobType rhsType)
{
    uint lhsFlag = (1 << lhsType);
    uint rhsFlag = (1 << rhsType);
    bool lhsAmmo = (MOB_FLAG_AMMO & lhsFlag) != 0;
    bool rhsAmmo = (MOB_FLAG_AMMO & rhsFlag) != 0;

    if (DEBUG) {
        bool lhsShip = (MOB_FLAG_SHIP & lhsFlag) != 0;
        bool rhsShip = (MOB_FLAG_SHIP & rhsFlag) != 0;
        ASSERT(lhsAmmo == !lhsShip);
        ASSERT(rhsAmmo == !rhsShip);
    }

    if (lhsAmmo) {
        return !rhsAmmo;
    } else {
        ASSERT(!lhsAmmo);
        return rhsAmmo;
    }
}

static INLINE_ALWAYS bool
BattleCheckMobCollision(const Mob *lhs, const Mob *rhs)
{
    FCircle lc, rc;

    if (!BattleCanMobTypesCollide(lhs->type, rhs->type)) {
        return FALSE;
    }
    if (lhs->type != MOB_TYPE_LOOT_BOX &&
        rhs->type != MOB_TYPE_LOOT_BOX &&
        lhs->playerID == rhs->playerID) {
        // Players generally don't collide with themselves...
        return FALSE;
    }
    if (!lhs->alive || !rhs->alive) {
        return FALSE;
    }

    Mob_GetCircle(lhs, &lc);
    Mob_GetCircle(rhs, &rc);
    return FCircle_Intersect(&lc, &rc);
}


static void BattleRunMobCollision(Mob *oMob, Mob *iMob)
{
    ASSERT(BattleCheckMobCollision(oMob, iMob));

    battle.bs.collisions++;

    if (oMob->type == MOB_TYPE_LOOT_BOX) {
        ASSERT(iMob->type != MOB_TYPE_LOOT_BOX);
        ASSERT(iMob->playerID < ARRAYSIZE(battle.bs.players));
        battle.bs.players[iMob->playerID].credits += oMob->lootCredits;
        oMob->alive = FALSE;
    } else if (iMob->type == MOB_TYPE_LOOT_BOX) {
        ASSERT(oMob->type != MOB_TYPE_LOOT_BOX);
        ASSERT(oMob->playerID < ARRAYSIZE(battle.bs.players));
        battle.bs.players[oMob->playerID].credits += iMob->lootCredits;
        iMob->alive = FALSE;
    } else {
        oMob->health -= MobType_GetMaxHealth(iMob->type);
        iMob->health -= MobType_GetMaxHealth(oMob->type);

        if (oMob->health <= 0) {
            oMob->alive = FALSE;
            int lootCredits = BattleCalcLootCredits(oMob);
            if (lootCredits > 0) {
                Mob *spawn = BattleQueueSpawn(MOB_TYPE_LOOT_BOX,
                                                oMob->playerID, &oMob->pos);
                spawn->lootCredits = lootCredits;
            }
        }
        if (iMob->health <= 0) {
            iMob->alive = FALSE;
            int lootCredits = BattleCalcLootCredits(iMob);
            if (lootCredits > 0) {
                Mob *spawn = BattleQueueSpawn(MOB_TYPE_LOOT_BOX,
                                                iMob->playerID, &iMob->pos);
                spawn->lootCredits = lootCredits;
            }
        }
    }
}


static void BattleProcessCollisions(uint firstIndex, uint lastIndex)
{
    uint size = MobVector_Size(&battle.mobs);
    ASSERT(lastIndex < size);
    for (uint32 outer = firstIndex; outer <= lastIndex ; outer++) {
        Mob *oMob = MobVector_GetPtr(&battle.mobs, outer);

        for (uint32 inner = outer + 1; inner < size; inner++) {
            Mob *iMob = MobVector_GetPtr(&battle.mobs, inner);
            if (BattleCheckMobCollision(oMob, iMob)) {
                BattleWorkResult wr;
                wr.type = BATTLE_RESULT_TYPE_COLLISION;
                wr.collision.lhsMobIndex = outer;
                wr.collision.rhsMobIndex = inner;
                WorkQueue_QueueItem(&battle.resultQueue, &wr, sizeof(wr));
            }
        }
    }
}

static void BattleRunCollisions(void)
{
    int size = MobVector_Size(&battle.mobs);

    if (BATTLE_USE_THREADS) {
        uint i = 0;
        int batches;
        int unitSize;

        batches = 2 * ARRAYSIZE(battle.workerThreads);
        unitSize = 1 + (size / batches);
        unitSize = MAX(BATTLE_COLLISION_MIN_BATCH, unitSize);

        WorkQueue_Lock(&battle.workQueue);
        while (i < size) {
            BattleWorkUnit wu;
            wu.type = BATTLE_WORK_TYPE_COLLISION;
            wu.collision.firstIndex = i;
            wu.collision.lastIndex = MIN(i + unitSize, size - 1);

            WorkQueue_QueueItemLocked(&battle.workQueue, &wu, sizeof(wu));

            i = wu.collision.lastIndex + 1;
        }
        WorkQueue_Unlock(&battle.workQueue);
        WorkQueue_WaitForAllFinished(&battle.workQueue);
    } else {
        BattleProcessCollisions(0, size - 1);
    }

    WorkQueue_Lock(&battle.resultQueue);
    size = WorkQueue_QueueSizeLocked(&battle.resultQueue);
    for (uint i = 0; i < size; i++) {
        BattleWorkResult wr;
        WorkQueue_GetItemLocked(&battle.resultQueue, &wr, sizeof(wr));
        ASSERT(wr.type == BATTLE_RESULT_TYPE_COLLISION);

        Mob *oMob = MobVector_GetPtr(&battle.mobs, wr.collision.lhsMobIndex);
        Mob *iMob = MobVector_GetPtr(&battle.mobs, wr.collision.rhsMobIndex);

        /*
         * The death of an earlier mob might make this collision no longer
         * happen.
         */
        if (BattleCheckMobCollision(oMob, iMob)) {
            BattleRunMobCollision(oMob, iMob);
        }
    }
    WorkQueue_Unlock(&battle.resultQueue);
    ASSERT(WorkQueue_IsEmpty(&battle.resultQueue));
}

// Is the scanning mob allowed to scan anything?
static bool BattleCanMobScan(const Mob *scanning)
{
    if (scanning->type == MOB_TYPE_LOOT_BOX) {
        ASSERT(MobType_GetSensorRadius(MOB_TYPE_LOOT_BOX) == 0.0f);
        return FALSE;
    }
    ASSERT(scanning->playerID != PLAYER_ID_NEUTRAL);
    if (!scanning->alive) {
        return FALSE;
    }
    return TRUE;
}

// Can the scanning mob see the target mob?
static bool BattleCheckMobScan(const Mob *scanning, const FCircle *sc, const Mob *target)
{
    FCircle tc;

    ASSERT(BattleCanMobScan(scanning));

    if (scanning->playerID == target->playerID) {
        // Players don't scan themselves...
        return FALSE;
    }
    if (BitVector_GetRaw32(scanning->playerID, target->scannedBy)) {
        // This target was already seen by the player, so this isn't
        // a new scan.
        return FALSE;
    }

    Mob_GetCircle(target, &tc);

    if (FCircle_Intersect(sc, &tc)) {
        return TRUE;
    }
    return FALSE;
}

static void BattleProcessScanning(uint firstIndex, uint lastIndex)
{
    ASSERT(lastIndex < MobVector_Size(&battle.mobs));
    for (uint32 outer = firstIndex; outer <= lastIndex ; outer++) {
        Mob *oMob = MobVector_GetPtr(&battle.mobs, outer);
        FCircle sc;
        if (!BattleCanMobScan(oMob)) {
            continue;
        }

        Mob_GetSensorCircle(oMob, &sc);

        for (uint32 inner = 0; inner < MobVector_Size(&battle.mobs); inner++) {
            Mob *iMob = MobVector_GetPtr(&battle.mobs, inner);

            if (BattleCheckMobScan(oMob, &sc, iMob)) {
                BattleWorkResult wr;
                wr.type = BATTLE_RESULT_TYPE_SCAN;
                wr.scan.scanningMobIndex = outer;
                wr.scan.targetMobIndex = inner;
                WorkQueue_QueueItem(&battle.resultQueue, &wr, sizeof(wr));
            }
        }
    }
}

static void BattleRunScanning(void)
{
    int size = MobVector_Size(&battle.mobs);

    if (BATTLE_USE_THREADS) {
        uint i = 0;
        int batches;
        int unitSize;

        batches = 2 * ARRAYSIZE(battle.workerThreads);
        unitSize = 1 + (size / batches);
        unitSize = MAX(BATTLE_SCANNING_MIN_BATCH, unitSize);

        WorkQueue_Lock(&battle.workQueue);
        while (i < size) {
            BattleWorkUnit wu;
            wu.type = BATTLE_WORK_TYPE_SCAN;
            wu.scan.firstIndex = i;
            wu.scan.lastIndex = MIN(i + unitSize, size - 1);

            WorkQueue_QueueItemLocked(&battle.workQueue, &wu, sizeof(wu));

            i = wu.scan.lastIndex + 1;
        }
        WorkQueue_Unlock(&battle.workQueue);
        WorkQueue_WaitForAllFinished(&battle.workQueue);
    } else {
        BattleProcessScanning(0, size - 1);
    }

    WorkQueue_Lock(&battle.resultQueue);
    size = WorkQueue_QueueSizeLocked(&battle.resultQueue);
    for (uint i = 0; i < size; i++) {
        BattleWorkResult wr;
        WorkQueue_GetItemLocked(&battle.resultQueue, &wr, sizeof(wr));
        ASSERT(wr.type == BATTLE_RESULT_TYPE_SCAN);

        Mob *oMob = MobVector_GetPtr(&battle.mobs, wr.scan.scanningMobIndex);
        Mob *iMob = MobVector_GetPtr(&battle.mobs, wr.scan.targetMobIndex);
        ASSERT(oMob->playerID < sizeof(iMob->scannedBy) * 8);
        BitVector_SetRaw32(oMob->playerID, &iMob->scannedBy);
        battle.bs.sensorContacts++;
    }
    WorkQueue_Unlock(&battle.resultQueue);
    ASSERT(WorkQueue_IsEmpty(&battle.resultQueue));
}


void Battle_RunTick()
{
    ASSERT(battle.bs.tick < MAX_UINT32);
    battle.bs.tick++;

    // Run Physics
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);
        ASSERT(BattleCheckMobInvariants(mob));

        mob->scannedBy = 0;

        if (mob->alive) {
            if (mob->type == MOB_TYPE_MISSILE ||
                mob->type == MOB_TYPE_LOOT_BOX) {
                mob->fuel--;

                if (mob->fuel <= 0) {
                    mob->alive = FALSE;
                }
            }
        }

        if (mob->alive) {
            BattleRunMobMove(mob);
        }
    }

    // Spawn loot
    battle.lootSpawnBucket += battle.bp.lootSpawnRate;
    while (battle.lootSpawnBucket > battle.bp.minLootSpawn) {
        FPoint pos;
        int loot = Random_Int(battle.bp.minLootSpawn, battle.bp.maxLootSpawn);
        battle.lootSpawnBucket -= loot;

        pos.x = Random_Float(0.0f, battle.bp.width);
        pos.y = Random_Float(0.0f, battle.bp.height);
        Mob *spawn = BattleQueueSpawn(MOB_TYPE_LOOT_BOX, PLAYER_ID_NEUTRAL, &pos);
        spawn->lootCredits = loot;
    }

    // Queue spawned things
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);
        BattleRunMobSpawn(mob);
    }

    // Process collisions
    BattleRunCollisions();

    // Create spawned things (after collisions)
    for (uint32 i = 0; i < MobVector_Size(&battle.pendingSpawns); i++) {
        Mob *spawn = MobVector_GetPtr(&battle.pendingSpawns, i);
        MobVector_GrowBy(&battle.mobs, 1);
        Mob *newMob = MobVector_GetLastPtr(&battle.mobs);
        *newMob = *spawn;
    }
    MobVector_MakeEmpty(&battle.pendingSpawns);

    // Process Scanning
    BattleRunScanning();

    // Destroy mobs and track player liveness
    for (uint32 i = 0; i < battle.bs.numPlayers; i++) {
        battle.bs.players[i].alive = FALSE;
    }
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);
        if (mob->alive) {
            if (mob->type != MOB_TYPE_LOOT_BOX) {
                PlayerID p = mob->playerID;
                battle.bs.players[p].alive = TRUE;
            }
        } else {
            /*
             * Keep the mob around for one tick after it dies so the
             * fleet AI's can see that it died.
             */
            if (mob->removeMob) {
                Mob *last = MobVector_GetLastPtr(&battle.mobs);
                *mob = *last;
                MobVector_Shrink(&battle.mobs);

                // Redo the current index
                i--;
            } else {
                mob->removeMob = TRUE;
            }
        }
    }

    // Check for victory, pay the players
    uint32 livePlayers = 0;
    for (uint32 i = 0; i < battle.bs.numPlayers; i++) {
        if (battle.bs.players[i].alive) {
            livePlayers++;
            battle.bs.players[i].credits += battle.bp.creditsPerTick;
        }
    }
    if (livePlayers <= 1) {
        battle.bs.finished = TRUE;

        for (uint32 i = 0; i < battle.bs.numPlayers; i++) {
            if (battle.bs.players[i].alive) {
                battle.bs.winner = i;
            }
        }
    }

    if(battle.bs.tick >= battle.bp.timeLimit) {
        battle.bs.finished = TRUE;
    }
}

Mob *Battle_AcquireMobs(uint32 *numMobs)
{
    ASSERT(battle.initialized);
    ASSERT(numMobs != NULL);
    ASSERT(!battle.mobsAcquired);

    battle.mobsAcquired = TRUE;

    *numMobs = MobVector_Size(&battle.mobs);
    MobVector_Pin(&battle.mobs);
    return MobVector_GetCArray(&battle.mobs);
}

void Battle_ReleaseMobs()
{
    ASSERT(battle.initialized);
    ASSERT(battle.mobsAcquired);
    MobVector_Unpin(&battle.mobs);
    battle.mobsAcquired = FALSE;
}

const BattleStatus *Battle_AcquireStatus()
{
    ASSERT(battle.initialized);
    ASSERT(!battle.statusAcquired);

    battle.statusAcquired = TRUE;
    return &battle.bs;
}

void Battle_ReleaseStatus()
{
    ASSERT(battle.initialized);
    ASSERT(battle.statusAcquired);
    battle.statusAcquired = FALSE;
}

const BattleParams *Battle_GetParams()
{
    /*
     * This is called from multiple threads,and is
     * safe safe only because this never changes.
     */
    ASSERT(battle.initialized);
    return &battle.bp;
}
