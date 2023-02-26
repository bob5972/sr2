/*
 * battle.c -- part of SpaceRobots2
 * Copyright (C) 2020-2021 Michael Banack <github@banack.net>
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

#include <immintrin.h>

#include "battle.h"
#include "MBBasic.h"
#include "Random.h"
#include "BitVector.h"
#include <SDL2/SDL_thread.h>
#include "workQueue.h"

typedef struct Battle {
    bool initialized;

    BattleScenario bsc;

    RandomState rs;

    BattleStatus bs;
    bool statusAcquired;

    Fleet *fleet;

    float powerCoreSpawnBucket;

    MobID lastMobID;
    MobVector mobs;
    bool mobsAcquired;

    MobPVec tempMobs[2];

    MobVector pendingSpawns;
} Battle;

static inline __m256 BattleCircleIntersectSSE(__m256 sx, __m256 sy, __m256 sr,
                                              __m256 mx, __m256 my, __m256 mr);


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

    for (uint i = 0; i < ARRAYSIZE(battle->tempMobs); i++) {
        MobPVec_CreateEmpty(&battle->tempMobs[i]);
    }

    uint randomShift = RandomState_Int(&battle->rs, 0, numPlayers - 1);
    for (uint i = 0; i < numPlayers; i++) {
        if (i == PLAYER_ID_NEUTRAL) {
            continue;
        }

        for (uint s = 0;
             s < battle->bsc.bp.startingBases + battle->bsc.bp.startingFighters;
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
                uint p = (i + randomShift) % (numPlayers - 1);
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

    for (uint i = 0; i < ARRAYSIZE(battle->tempMobs); i++) {
        MobPVec_Destroy(&battle->tempMobs[i]);
    }

    MobVector_Destroy(&battle->mobs);
    MobVector_Destroy(&battle->pendingSpawns);
    RandomState_Destroy(&battle->rs);
    battle->initialized = FALSE;
    free(battle);
}

static bool BattleCheckMobInvariants(Battle *battle, const Mob *mob)
{
    ASSERT(Mob_CheckInvariants(mob));
    ASSERT(mob->image == MOB_IMAGE_FULL);
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

static int BattleCalcPowerCoreCredits(Battle *battle, const Mob *m)
{
    if (m->type == MOB_TYPE_MISSILE ||
        m->type == MOB_TYPE_POWER_CORE) {
        return 0;
    }

    int powerCore = MobType_GetCost(m->type);
    return (int)(battle->bsc.bp.powerCoreDropRate * powerCore);
}

static Mob *BattleQueueSpawn(Battle *battle, MobID parentMobid,
                             MobType type,
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
    spawn->lastPos = *pos;
    spawn->cmd.target = *pos;
    spawn->birthTick = battle->bs.tick;
    spawn->parentMobid = parentMobid;

    battle->bs.spawns++;
    if (spawn->type != MOB_TYPE_POWER_CORE &&
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
    spawn = BattleQueueSpawn(battle, mob->mobid,
                             mob->cmd.spawnType,
                             mob->playerID, &mob->pos);
    spawn->cmd.target = mob->cmd.target;
    mob->rechargeTime = MobType_GetRechargeTicks(mob->type);;
    mob->lastSpawnTick = battle->bs.tick;
}

static void BattleRunMobMove(Battle *battle, Mob *mob)
{
    float speed;

    ASSERT(mob->alive);

    if (mob->playerID == PLAYER_ID_NEUTRAL) {
        /*
         * The neutral player never moves today.
         */
        ASSERT(mob->type == MOB_TYPE_POWER_CORE);
        return;
    }

    speed = Mob_GetSpeed(mob);

    mob->lastPos = mob->pos;
    FPoint_MoveToPointAtSpeed(&mob->pos, &mob->cmd.target, speed);
    ASSERT(FPoint_Distance(&mob->lastPos, &mob->pos) <= speed + MICRON);
    ASSERT(BattleCheckMobInvariants(battle, mob));
}


static INLINE_ALWAYS bool
BattleCanMobTypesCollide(MobType lhsType, MobType rhsType)
{
    uint lhsFlag = (1 << lhsType);
    uint rhsFlag = (1 << rhsType);
    bool lhsAmmo = (MOB_FLAG_AMMO & lhsFlag) != 0;
    bool rhsAmmo = (MOB_FLAG_AMMO & rhsFlag) != 0;

    if (mb_debug) {
        bool lhsShip = (MOB_FLAG_SHIP & lhsFlag) != 0;
        bool rhsShip = (MOB_FLAG_SHIP & rhsFlag) != 0;
        ASSERT(lhsAmmo == !lhsShip);
        ASSERT(rhsAmmo == !rhsShip);
    }

    return lhsAmmo ^ rhsAmmo;
}

static INLINE_ALWAYS bool
BattleCheckMobCollision(const Mob *oMob, const FCircle *oc, const Mob *iMob)
{
    FCircle ic;

    ASSERT(BattleCanMobTypesCollide(oMob->type, iMob->type));
    ASSERT(Mob_IsAmmo(oMob));
    ASSERT(!Mob_IsAmmo(iMob));

    if (oMob->type != MOB_TYPE_POWER_CORE &&
        oMob->playerID == iMob->playerID) {
        // Players generally don't collide with themselves...
        ASSERT(iMob->type != MOB_TYPE_POWER_CORE);
        return FALSE;
    }

    ASSERT(oMob->alive);
    ASSERT(iMob->alive == TRUE || iMob->alive == FALSE);
    if (!iMob->alive) {
        return FALSE;
    }

    Mob_GetCircle(iMob, &ic);
    return FCircle_Intersect(oc, &ic);
}


static void
BattleRunMobCollision(Battle *battle, Mob *oMob, Mob *iMob)
{
    battle->bs.collisions++;

    if (oMob->type == MOB_TYPE_POWER_CORE) {
        ASSERT(iMob->type != MOB_TYPE_POWER_CORE);
        ASSERT(iMob->playerID < ARRAYSIZE(battle->bs.players));
        battle->bs.players[iMob->playerID].credits += oMob->powerCoreCredits;
        oMob->alive = FALSE;
    } else if (iMob->type == MOB_TYPE_POWER_CORE) {
        ASSERT(oMob->type != MOB_TYPE_POWER_CORE);
        ASSERT(oMob->playerID < ARRAYSIZE(battle->bs.players));
        battle->bs.players[oMob->playerID].credits += iMob->powerCoreCredits;
        iMob->alive = FALSE;
    } else {
        oMob->health -= MobType_GetMaxHealth(iMob->type);
        iMob->health -= MobType_GetMaxHealth(oMob->type);

        if (oMob->health <= 0) {
            oMob->alive = FALSE;
            int powerCoreCredits = BattleCalcPowerCoreCredits(battle, oMob);
            if (powerCoreCredits > 0) {
                Mob *spawn = BattleQueueSpawn(battle, oMob->mobid,
                                              MOB_TYPE_POWER_CORE,
                                              oMob->playerID, &oMob->pos);
                spawn->powerCoreCredits = powerCoreCredits;
            }
        }
        if (iMob->health <= 0) {
            iMob->alive = FALSE;
            int powerCoreCredits = BattleCalcPowerCoreCredits(battle, iMob);
            if (powerCoreCredits > 0) {
                Mob *spawn;
                spawn = BattleQueueSpawn(battle, iMob->mobid,
                                         MOB_TYPE_POWER_CORE,
                                         iMob->playerID,
                                         &iMob->pos);
                spawn->powerCoreCredits = powerCoreCredits;
            }
        }
    }
}


#ifdef __AVX__
#define VSIZE 8
static void BattleCollideBatch(Battle *battle, Mob *oMob,
                               float *x, float *y, float *r,
                               Mob **innerMobs, uint32 innerSize)
{
    FCircle oc;
    PlayerID oMobPlayerID = oMob->playerID;
    uint32 inner = 0;

    Mob_GetCircle(oMob, &oc);

    union {
        float f[VSIZE];
        uint32 u[VSIZE];
    } result;

    __m256 sx, sy, sr;

    sx = _mm256_broadcast_ss(&oc.center.x);
    sy = _mm256_broadcast_ss(&oc.center.y);
    sr = _mm256_broadcast_ss(&oc.radius);

    while (inner + VSIZE < innerSize) {
        __m256 mx = _mm256_load_ps(&x[inner]);
        __m256 my = _mm256_load_ps(&y[inner]);
        __m256 mr = _mm256_load_ps(&r[inner]);
        __m256 cmp = BattleCircleIntersectSSE(sx, sy, sr, mx, my, mr);
        _mm256_storeu_ps(&result.f[0], cmp);

        for (uint32 i = 0; i < VSIZE; i++) {
            Mob *iMob = innerMobs[inner + i];
            if (result.u[i] != 0 && iMob->alive &&
                (oMob->type == MOB_TYPE_POWER_CORE || oMobPlayerID != iMob->playerID)) {
                ASSERT(BattleCheckMobCollision(oMob, &oc, iMob));
                BattleRunMobCollision(battle, oMob, iMob);
                if (!oMob->alive) {
                    /*
                    * If the outer mob is no longer alive, it can't collide
                    * with anything else.
                    */
                   return;
                }
            } else {
                ASSERT(!BattleCheckMobCollision(oMob, &oc, iMob));
            }
        }

        inner += VSIZE;
    }

    while (inner < innerSize) {
        Mob *iMob = innerMobs[inner];
        if (BattleCheckMobCollision(oMob, &oc, iMob)) {
            BattleRunMobCollision(battle, oMob, iMob);
            if (!oMob->alive) {
                /*
                 * If the outer mob is no longer alive, it can't collide
                 * with anything else.
                 */
                return;
            }
        }
        inner++;
    }
}
#undef VSIZE
#endif // __AVX__


static void BattleRunCollisions(Battle *battle)
{
    uint size = MobVector_Size(&battle->mobs);

    /*
     * Partition the mobs into ammo/non-ammo, and then check for
     * collisions between the two groups.
     */

#ifdef __AVX__
#define BSIZE 256

    MobVector_Pin(&battle->mobs);
    Mob *mobs = MobVector_GetCArray(&battle->mobs);

    uint32 i = 0;
    while (i < size) {
        float x[BSIZE];
        float y[BSIZE];
        float r[BSIZE];
        Mob *m[BSIZE];
        uint32 n = 0;

        while (n < ARRAYSIZE(x) && i < size) {
            Mob *iMob = &mobs[i];
            if (!Mob_IsAmmo(iMob)) {
                x[n] = iMob->pos.x;
                y[n] = iMob->pos.y;
                r[n] = Mob_GetRadius(iMob);
                m[n] = iMob;
                n++;
            }
            i++;
        }

        for (uint32 outer = 0; outer < size; outer++) {
            Mob *oMob = &mobs[outer];

            if (!Mob_IsAmmo(oMob) || !oMob->alive) {
                continue;
            }

            BattleCollideBatch(battle, oMob, &x[0], &y[0], &r[0], &m[0], n);
        }
    }

    MobVector_Unpin(&battle->mobs);
#undef BSIZE
#else
    ASSERT(ARRAYSIZE(battle->tempMobs) >= 2);
    MobPVec_Resize(&battle->tempMobs[0], mobSize);
    MobPVec_Resize(&battle->tempMobs[1], mobSize);

    MobVector_Pin(&battle->mobs);
    Mob *mobs = MobVector_GetCArray(&battle->mobs);

    uint ammoSize = 0;
    uint shipSize = 0;

    for (uint32 x = 0; x < mobSize; x++) {
        Mob *oMob = &mobs[x];

        if (oMob->alive) {
            if (Mob_IsAmmo(oMob)) {
                MobPVec_PutValue(&battle->tempMobs[0], ammoSize++, oMob);
            } else {
                MobPVec_PutValue(&battle->tempMobs[1], shipSize++, oMob);
            }
        }
    }

    MobVector_Unpin(&battle->mobs);

    DEBUG_ONLY(
        MobPVec_Resize(&battle->tempMobs[0], ammoSize);
        MobPVec_Resize(&battle->tempMobs[1], shipSize);
    );

    for (uint32 outer = 0; outer < ammoSize; outer++) {
        Mob *oMob = MobPVec_GetValue(&battle->tempMobs[0], outer);

        for (uint32 inner = 0; inner < shipSize; inner++) {
            Mob *iMob = MobPVec_GetValue(&battle->tempMobs[1], inner);
            if (BattleCheckMobCollision(oMob, iMob)) {
                BattleRunMobCollision(battle, oMob, iMob);
                if (!oMob->alive) {
                    break;
                }
            }
        }
    }
#endif // __AVX__
}


// Is the scanning mob allowed to scan anything?
static bool BattleCanMobScan(const Mob *scanning)
{
    if (scanning->type == MOB_TYPE_POWER_CORE) {
        ASSERT(MobType_GetSensorRadius(MOB_TYPE_POWER_CORE) == 0.0f);
        return FALSE;
    }
    ASSERT(scanning->playerID != PLAYER_ID_NEUTRAL);
    if (!scanning->alive) {
        return FALSE;
    }
    return TRUE;
}

// Can the scanning mob see the target mob?
static bool BattleCheckMobScan(const Mob *scanning, const FCircle *sc,
                               const Mob *target, bool assertUsage)
{
    FCircle tc;

    // Caller should've checked these already.
    ASSERT(BattleCanMobScan(scanning));

    if (!assertUsage) {
        if (BitVector_GetRaw32(scanning->playerID, target->scannedBy)) {
            // This target was already seen by the player, so this isn't
            // a new scan.
            return FALSE;
        }
    }

    Mob_GetCircle(target, &tc);
    if (FCircle_Intersect(sc, &tc)) {
        return TRUE;
    }
    return FALSE;
}

#ifdef __AVX__
static inline __m256 BattleCircleIntersectSSE(__m256 sx, __m256 sy, __m256 sr,
                                              __m256 mx, __m256 my, __m256 mr)
{
    __m256 dx = _mm256_sub_ps(sx, mx);
    __m256 dy = _mm256_sub_ps(sy, my);
    __m256 dr = _mm256_add_ps(sr, mr);

    __m256 dx2 = _mm256_mul_ps(dx, dx);
    __m256 dy2 = _mm256_mul_ps(dy, dy);
    __m256 dr2 = _mm256_mul_ps(dr, dr);

    __m256 dd = _mm256_add_ps(dx2, dy2);

    return _mm256_cmp_ps(dd, dr2, _CMP_LE_OS);
}
#endif // __AVX__

#ifdef __AVX__
#define VSIZE 8
static void BattleScanBatch(Battle *battle, Mob *oMob,
                            float *x, float *y, float *r,
                            Mob *innerMobs, uint32 innerSize)
{
    FCircle sc;
    PlayerID oMobPlayerID = oMob->playerID;
    uint32 inner = 0;

    Mob_GetSensorCircle(oMob, &sc);

    union {
        float f[VSIZE];
        uint32 u[VSIZE];
    } result;

    __m256 sx, sy, sr;

    sx = _mm256_broadcast_ss(&sc.center.x);
    sy = _mm256_broadcast_ss(&sc.center.y);
    sr = _mm256_broadcast_ss(&sc.radius);

    while (inner + VSIZE < innerSize) {
        __m256 mx = _mm256_load_ps(&x[inner]);
        __m256 my = _mm256_load_ps(&y[inner]);
        __m256 mr = _mm256_load_ps(&r[inner]);
        __m256 cmp = BattleCircleIntersectSSE(sx, sy, sr, mx, my, mr);
        _mm256_storeu_ps(&result.f[0], cmp);

        for (uint32 i = 0; i < VSIZE; i++) {
            if (result.u[i] != 0) {
                Mob *iMob = &innerMobs[inner + i];
                ASSERT(BattleCheckMobScan(oMob, &sc, iMob, TRUE));
                ASSERT(oMobPlayerID < sizeof(iMob->scannedBy) * 8);
                BitVector_SetRaw32(oMobPlayerID, &iMob->scannedBy);
                battle->bs.sensorContacts++;
            } else {
                Mob *iMob = &innerMobs[inner + i];
                ASSERT(!BattleCheckMobScan(oMob, &sc, iMob, TRUE));
            }
        }

        inner += VSIZE;
    }

    while (inner < innerSize) {
        Mob *iMob = &innerMobs[inner];
        if (BattleCheckMobScan(oMob, &sc, iMob, FALSE)) {
            ASSERT(oMobPlayerID < sizeof(iMob->scannedBy) * 8);
            BitVector_SetRaw32(oMobPlayerID, &iMob->scannedBy);
            battle->bs.sensorContacts++;
        }
        inner++;
    }
}
#undef VSIZE
#endif // __AVX__

static void BattleRunScanning(Battle *battle)
{
    uint size = MobVector_Size(&battle->mobs);

    MobVector_Pin(&battle->mobs);

    Mob *mobs = MobVector_GetCArray(&battle->mobs);

#ifdef __AVX__
#define BSIZE 256
    uint32 i = 0;
    while (i < size) {
        float x[BSIZE];
        float y[BSIZE];
        float r[BSIZE];
        uint32 iStart = i;
        uint32 n = 0;

        while (n < ARRAYSIZE(x) && i < size) {
            Mob *iMob = &mobs[i];
            x[n] = mobs[i].pos.x;
            y[n] = mobs[i].pos.y;
            r[n] = Mob_GetRadius(iMob);

            i++;
            n++;
        }

        for (uint32 outer = 0; outer < size; outer++) {
            Mob *oMob = &mobs[outer];

            if (!BattleCanMobScan(oMob)) {
                continue;
            }

            BattleScanBatch(battle, oMob, &x[0], &y[0], &r[0], &mobs[iStart], n);
        }
    }
#undef BSIZE
#else
    /*
     * If we're taking the scalar path, pre-marking all the mobs
     * as scanned by their own player helps.
     */
    for (uint32 outer = 0; outer < size; outer++) {
        Mob *oMob = &mobs[outer];
        BitVector_SetRaw32(oMob->playerID, &oMob->scannedBy);
    }

    for (uint32 outer = 0; outer < size; outer++) {
        Mob *oMob = &mobs[outer];
        if (!BattleCanMobScan(oMob)) {
            continue;
        }

        for (uint32 inner = 0; inner < size; inner++) {
            Mob *iMob = &innerMobs[inner];
            if (BattleCheckMobScan(oMob, &sc, iMob, FALSE)) {
                ASSERT(oMobPlayerID < sizeof(iMob->scannedBy) * 8);
                BitVector_SetRaw32(oMobPlayerID, &iMob->scannedBy);
                battle->bs.sensorContacts++;
            }
        }
    }
#endif // __AVX__

    /*
     * Clear out the scan bits so that players don't scan themselves.
     *
     * This is arguably not useful, but keeps it consistent across the
     * scalar/AVX path, and means that fleet.c doesn't have to check for it.
     */
    for (uint32 outer = 0; outer < size; outer++) {
        Mob *oMob = &mobs[outer];
        BitVector_ResetRaw32(oMob->playerID, &oMob->scannedBy);
    }

    MobVector_Unpin(&battle->mobs);
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
                mob->type == MOB_TYPE_POWER_CORE) {
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

    // Spawn powerCore
    battle->powerCoreSpawnBucket += battle->bsc.bp.powerCoreSpawnRate;
    while (battle->powerCoreSpawnBucket > battle->bsc.bp.minPowerCoreSpawn) {
        FPoint pos;
        int powerCore = RandomState_Int(&battle->rs,
                                   battle->bsc.bp.minPowerCoreSpawn,
                                   battle->bsc.bp.maxPowerCoreSpawn);
        battle->powerCoreSpawnBucket -= powerCore;

        pos.x = RandomState_Float(&battle->rs, 0.0f,
                                  battle->bsc.bp.width);
        pos.y = RandomState_Float(&battle->rs, 0.0f,
                                  battle->bsc.bp.height);
        Mob *spawn = BattleQueueSpawn(battle, MOB_ID_INVALID,
                                      MOB_TYPE_POWER_CORE,
                                      PLAYER_ID_NEUTRAL, &pos);
        spawn->powerCoreCredits = powerCore;
    }

    // Queue spawned things
    for (uint32 i = 0; i < MobVector_Size(&battle->mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle->mobs, i);
        BattleRunMobSpawn(battle, mob);
        mob->cmd.spawnType = MOB_TYPE_INVALID;
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

            if ((mob->type != MOB_TYPE_POWER_CORE &&
                 !battle->bsc.bp.baseVictory) ||
                mob->type == MOB_TYPE_BASE) {
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
