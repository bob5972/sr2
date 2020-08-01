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

typedef struct Battle {
    bool initialized;

    BattleScenario bsc;

    RandomState rs;

    BattleStatus bs;
    bool statusAcquired;

    Fleet *fleet;

    float lootSpawnBucket;

    MobID lastMobID;
    MobVector mobs;
    bool mobsAcquired;

    MobVector pendingSpawns;
} Battle;

Battle *Battle_Create(const BattleScenario *bsc,
                      uint64 seed)
{
    Battle *battle;

    battle = malloc(sizeof(*battle));
    ASSERT(bsc != NULL);
    MBUtil_Zero(battle, sizeof(*battle));

    RandomState_CreateWithSeed(&battle->rs, seed);

    /*
     * We need Neutral + 2 fleets.
     */
    ASSERT(bsc->bp.numPlayers >= 3);
    battle->bsc = *bsc;

    uint numPlayers = bsc->bp.numPlayers;
    ASSERT(numPlayers < ARRAYSIZE(battle->bs.players));
    battle->bs.numPlayers = numPlayers;
    for (uint i = 0; i < numPlayers; i++) {
        battle->bs.players[i].playerUID = bsc->players[i].playerUID;
        battle->bs.players[i].alive = TRUE;
        battle->bs.players[i].credits = bsc->bp.startingCredits;
    }
    for (uint i = numPlayers; i < ARRAYSIZE(battle->bs.players); i++) {
        battle->bs.players[i].playerUID = PLAYER_ID_INVALID;
    }
    battle->bs.winner = PLAYER_ID_NEUTRAL;
    battle->bs.winnerUID = PLAYER_ID_NEUTRAL;

    ASSERT(bsc->players[PLAYER_ID_NEUTRAL].aiType ==
           FLEET_AI_NEUTRAL);
    MobVector_Create(&battle->mobs, 0, 1024);
    MobVector_CreateEmpty(&battle->pendingSpawns);

    for (uint i = 0; i < numPlayers; i++) {
        if (i == PLAYER_ID_NEUTRAL) {
            continue;
        }

        for (uint s = 0; s < battle->bsc.bp.startingBases + battle->bsc.bp.startingFighters;
             s++) {
            MobVector_Grow(&battle->mobs);
            Mob *mob = MobVector_GetLastPtr(&battle->mobs);

            MobType t = s < battle->bsc.bp.startingBases ?
                        MOB_TYPE_BASE : MOB_TYPE_FIGHTER;
            Mob_Init(mob, t);
            mob->playerID = i;
            mob->mobid = ++battle->lastMobID;
            if (battle->bsc.bp.restrictedStart) {
                // account for NEUTRAL
                uint p = i - 1;
                float slotW = battle->bsc.bp.width / (numPlayers - 1);
                mob->pos.x = RandomState_Float(&battle->rs, p * slotW,
                                            (p + 1) * slotW);
                mob->pos.y = RandomState_Float(&battle->rs, 0.0f,
                                            battle->bsc.bp.height);
            } else {
                mob->pos.x = RandomState_Float(&battle->rs, 0.0f,
                                            battle->bsc.bp.width);
                mob->pos.y = RandomState_Float(&battle->rs, 0.0f,
                                            battle->bsc.bp.height);
            }
            mob->cmd.target = mob->pos;
        }
    }

    battle->fleet = Fleet_Create(bsc, RandomState_Uint64(&battle->rs));

    battle->initialized = TRUE;
    return battle;
}

void Battle_Destroy(Battle *battle)
{
    ASSERT(battle != NULL);
    ASSERT(battle->initialized);

    Fleet_Destroy(battle->fleet);
    battle->fleet = NULL;

    MobVector_Destroy(&battle->mobs);
    MobVector_Destroy(&battle->pendingSpawns);
    RandomState_Destroy(&battle->rs);
    battle->initialized = FALSE;
    free(battle);
}

static bool BattleCheckMobInvariants(Battle *battle, const Mob *mob)
{
    ASSERT(Mob_CheckInvariants(mob));
    ASSERT(mob->pos.x >= 0.0f);
    ASSERT(mob->pos.y >= 0.0f);
    ASSERT(mob->pos.x <= (uint32)battle->bsc.bp.width);
    ASSERT(mob->pos.y <= (uint32)battle->bsc.bp.height);

    ASSERT(mob->cmd.target.x >= 0.0f);
    ASSERT(mob->cmd.target.y >= 0.0f);
    ASSERT(mob->cmd.target.x <= (uint32)battle->bsc.bp.width);
    ASSERT(mob->cmd.target.y <= (uint32)battle->bsc.bp.height);

    return TRUE;
}

static int BattleCalcLootCredits(Battle *battle, const Mob *m)
{
    if (m->type == MOB_TYPE_MISSILE ||
        m->type == MOB_TYPE_LOOT_BOX) {
        return 0;
    }

    int loot = MobType_GetCost(m->type);
    return (int)(battle->bsc.bp.lootDropRate * loot);
}

static Mob *BattleQueueSpawn(Battle *battle, MobType type,
                             PlayerID p, const FPoint *pos)
{
    Mob *spawn;

    ASSERT(pos != NULL);
    MobVector_Grow(&battle->pendingSpawns);
    spawn = MobVector_GetLastPtr(&battle->pendingSpawns);

    Mob_Init(spawn, type);
    spawn->playerID = p;
    spawn->mobid = ++battle->lastMobID;
    spawn->pos = *pos;
    spawn->cmd.target = *pos;
    spawn->birthTick = battle->bs.tick;

    battle->bs.spawns++;
    if (spawn->type != MOB_TYPE_LOOT_BOX &&
        spawn->type != MOB_TYPE_MISSILE) {
        battle->bs.shipSpawns++;
    }

    return spawn;
}

static void BattleRunMobSpawn(Battle *battle, Mob *mob)
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

    ASSERT(mob->playerID < ARRAYSIZE(battle->bs.players));
    if (battle->bs.players[mob->playerID].credits <
        MobType_GetCost(mob->cmd.spawnType)) {
        return;
    }
    if (mob->rechargeTime > 0) {
        mob->rechargeTime--;
        return;
    }

    battle->bs.players[mob->playerID].credits -=
        MobType_GetCost(mob->cmd.spawnType);
    spawn = BattleQueueSpawn(battle, mob->cmd.spawnType,
                             mob->playerID, &mob->pos);
    spawn->cmd.target = mob->cmd.target;
    mob->rechargeTime = SPAWN_RECHARGE_TICKS;
    mob->cmd.spawnType = MOB_TYPE_INVALID;
}

static void BattleRunMobMove(Battle *battle, Mob *mob)
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
    ASSERT(BattleCheckMobInvariants(battle, mob));
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


static void
BattleRunMobCollision(Battle *battle, Mob *oMob, Mob *iMob)
{
    ASSERT(BattleCheckMobCollision(oMob, iMob));

    battle->bs.collisions++;

    if (oMob->type == MOB_TYPE_LOOT_BOX) {
        ASSERT(iMob->type != MOB_TYPE_LOOT_BOX);
        ASSERT(iMob->playerID < ARRAYSIZE(battle->bs.players));
        battle->bs.players[iMob->playerID].credits += oMob->lootCredits;
        oMob->alive = FALSE;
    } else if (iMob->type == MOB_TYPE_LOOT_BOX) {
        ASSERT(oMob->type != MOB_TYPE_LOOT_BOX);
        ASSERT(oMob->playerID < ARRAYSIZE(battle->bs.players));
        battle->bs.players[oMob->playerID].credits += iMob->lootCredits;
        iMob->alive = FALSE;
    } else {
        oMob->health -= MobType_GetMaxHealth(iMob->type);
        iMob->health -= MobType_GetMaxHealth(oMob->type);

        if (oMob->health <= 0) {
            oMob->alive = FALSE;
            int lootCredits = BattleCalcLootCredits(battle, oMob);
            if (lootCredits > 0) {
                Mob *spawn = BattleQueueSpawn(battle, MOB_TYPE_LOOT_BOX,
                                              oMob->playerID, &oMob->pos);
                spawn->lootCredits = lootCredits;
            }
        }
        if (iMob->health <= 0) {
            iMob->alive = FALSE;
            int lootCredits = BattleCalcLootCredits(battle, iMob);
            if (lootCredits > 0) {
                Mob *spawn;
                spawn = BattleQueueSpawn(battle, MOB_TYPE_LOOT_BOX,
                                         iMob->playerID,
                                         &iMob->pos);
                spawn->lootCredits = lootCredits;
            }
        }
    }
}

static void BattleRunCollisions(Battle *battle)
{
    uint size = MobVector_Size(&battle->mobs);

    for (uint32 outer = 0; outer < size; outer++) {
        Mob *oMob = MobVector_GetPtr(&battle->mobs, outer);

        for (uint32 inner = outer + 1; inner < size; inner++) {
            Mob *iMob = MobVector_GetPtr(&battle->mobs, inner);
            if (BattleCheckMobCollision(oMob, iMob)) {
                BattleRunMobCollision(battle, oMob, iMob);
            }
        }
    }
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

static void BattleRunScanning(Battle *battle)
{
    uint size = MobVector_Size(&battle->mobs);

    for (uint32 outer = 0; outer < size; outer++) {
        Mob *oMob = MobVector_GetPtr(&battle->mobs, outer);
        FCircle sc;
        if (!BattleCanMobScan(oMob)) {
            continue;
        }

        Mob_GetSensorCircle(oMob, &sc);

        for (uint32 inner = 0; inner < size; inner++) {
            Mob *iMob = MobVector_GetPtr(&battle->mobs, inner);

            if (BattleCheckMobScan(oMob, &sc, iMob)) {
                ASSERT(oMob->playerID < sizeof(iMob->scannedBy) * 8);
                BitVector_SetRaw32(oMob->playerID, &iMob->scannedBy);
                battle->bs.sensorContacts++;
            }
        }
    }
}


void Battle_RunTick(Battle *battle)
{
    ASSERT(battle->bs.tick < MAX_UINT32);

    // Run the AI
    uint32 numMobs;
    Mob *bMobs = Battle_AcquireMobs(battle, &numMobs);
    Fleet_RunTick(battle->fleet, &battle->bs, bMobs, numMobs);
    Battle_ReleaseMobs(battle);
    bMobs = NULL;

    // Increment the tick after the AI
    battle->bs.tick++;

    // Run Physics
    for (uint32 i = 0; i < MobVector_Size(&battle->mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle->mobs, i);
        ASSERT(BattleCheckMobInvariants(battle, mob));

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
            BattleRunMobMove(battle, mob);
        }
    }

    // Spawn loot
    battle->lootSpawnBucket += battle->bsc.bp.lootSpawnRate;
    while (battle->lootSpawnBucket > battle->bsc.bp.minLootSpawn) {
        FPoint pos;
        int loot = RandomState_Int(&battle->rs,
                                   battle->bsc.bp.minLootSpawn,
                                   battle->bsc.bp.maxLootSpawn);
        battle->lootSpawnBucket -= loot;

        pos.x = RandomState_Float(&battle->rs, 0.0f,
                                  battle->bsc.bp.width);
        pos.y = RandomState_Float(&battle->rs, 0.0f,
                                  battle->bsc.bp.height);
        Mob *spawn = BattleQueueSpawn(battle, MOB_TYPE_LOOT_BOX,
                                      PLAYER_ID_NEUTRAL, &pos);
        spawn->lootCredits = loot;
    }

    // Queue spawned things
    for (uint32 i = 0; i < MobVector_Size(&battle->mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle->mobs, i);
        BattleRunMobSpawn(battle, mob);
    }

    // Process collisions
    BattleRunCollisions(battle);

    // Create spawned things (after collisions)
    for (uint32 i = 0; i < MobVector_Size(&battle->pendingSpawns); i++) {
        Mob *spawn = MobVector_GetPtr(&battle->pendingSpawns, i);
        MobVector_GrowBy(&battle->mobs, 1);
        Mob *newMob = MobVector_GetLastPtr(&battle->mobs);
        *newMob = *spawn;
    }
    MobVector_MakeEmpty(&battle->pendingSpawns);

    // Process Scanning
    BattleRunScanning(battle);

    // Destroy mobs and track player liveness
    for (uint32 i = 0; i < battle->bs.numPlayers; i++) {
        battle->bs.players[i].alive = FALSE;
        battle->bs.players[i].numMobs = 0;
    }
    for (uint32 i = 0; i < MobVector_Size(&battle->mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle->mobs, i);
        if (mob->alive) {
            PlayerID p = mob->playerID;
            battle->bs.players[p].numMobs++;
            if (mob->type != MOB_TYPE_LOOT_BOX) {

                battle->bs.players[p].alive = TRUE;
            }
        } else {
            /*
             * Keep the mob around for one tick after it dies so the
             * fleet AI's can see that it died.
             */
            if (mob->removeMob) {
                Mob *last = MobVector_GetLastPtr(&battle->mobs);
                *mob = *last;
                MobVector_Shrink(&battle->mobs);

                // Redo the current index
                i--;
            } else {
                mob->removeMob = TRUE;
            }
        }
    }

    // Check for victory, pay the players
    uint32 livePlayers = 0;
    for (uint32 i = 0; i < battle->bs.numPlayers; i++) {
        if (battle->bs.players[i].alive) {
            livePlayers++;
            battle->bs.players[i].credits += battle->bsc.bp.creditsPerTick;
        }
    }
    if (livePlayers <= 1) {
        battle->bs.finished = TRUE;

        for (uint32 i = 0; i < battle->bs.numPlayers; i++) {
            if (battle->bs.players[i].alive) {
                battle->bs.winner = i;
                battle->bs.winnerUID = battle->bs.players[i].playerUID;
            }
        }
    }

    if(battle->bs.tick >= battle->bsc.bp.tickLimit) {
        battle->bs.finished = TRUE;
    }
}

Mob *Battle_AcquireMobs(Battle *battle, uint32 *numMobs)
{
    ASSERT(battle->initialized);
    ASSERT(numMobs != NULL);
    ASSERT(!battle->mobsAcquired);

    battle->mobsAcquired = TRUE;

    *numMobs = MobVector_Size(&battle->mobs);
    MobVector_Pin(&battle->mobs);
    return MobVector_GetCArray(&battle->mobs);
}

void Battle_ReleaseMobs(Battle *battle)
{
    ASSERT(battle->initialized);
    ASSERT(battle->mobsAcquired);
    MobVector_Unpin(&battle->mobs);
    battle->mobsAcquired = FALSE;
}

const BattleStatus *Battle_AcquireStatus(Battle *battle)
{
    ASSERT(battle->initialized);
    ASSERT(!battle->statusAcquired);

    battle->statusAcquired = TRUE;
    return &battle->bs;
}

void Battle_ReleaseStatus(Battle *battle)
{
    ASSERT(battle->initialized);
    ASSERT(battle->statusAcquired);
    battle->statusAcquired = FALSE;
}
