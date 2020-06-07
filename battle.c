/*
 * battle.c --
 */

#include "battle.h"
#include "random.h"
#include "BitVector.h"

#define MICRON (0.1f)

typedef struct BattleGlobalData {
    bool initialized;

    BattleParams bp;
    bool paramsAcquired;

    BattleStatus bs;
    bool statusAcquired;

    MobID lastMobID;
    MobVector mobs;
    bool mobsAcquired;
} BattleGlobalData;

static BattleGlobalData battle;

void Battle_Init(const BattleParams *bp)
{
    ASSERT(bp != NULL);
    ASSERT(MBUtil_IsZero(&battle, sizeof(battle)));

    ASSERT(bp->numPlayers > 0);
    battle.bp = *bp;

    battle.bs.numPlayers = bp->numPlayers;
    for (uint32 i = 0; i < bp->numPlayers; i++) {
        battle.bs.players[i].alive = TRUE;
        battle.bs.players[i].credits = bp->startingCredits;
    }

    MobVector_Create(&battle.mobs, bp->numPlayers, 1024);

    for (uint32 i = 0; i < bp->numPlayers; i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);

        Mob_Init(mob, MOB_TYPE_BASE);
        mob->playerID = i;
        mob->id = ++battle.lastMobID;
        mob->pos.x = Random_Float(0.0f, battle.bp.width);
        mob->pos.y = Random_Float(0.0f, battle.bp.height);

        mob->cmd.target.x = Random_Float(0.0f, battle.bp.width);
        mob->cmd.target.y = Random_Float(0.0f, battle.bp.height);
    }

    battle.initialized = TRUE;
}

void Battle_Exit()
{
    ASSERT(battle.initialized);
    MobVector_Destroy(&battle.mobs);
    battle.initialized = FALSE;
}

bool BattleCheckMobInvariants(const Mob *mob)
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

void BattleDoMobSpawn(Mob *mob)
{
    uint32 size;
    Mob *spawn;

    ASSERT(mob != NULL);
    ASSERT(mob->alive);
    ASSERT(mob->type == MOB_TYPE_BASE ||
           mob->type == MOB_TYPE_FIGHTER);
    ASSERT(mob->cmd.spawn == MOB_TYPE_INVALID ||
           mob->cmd.spawn >= MOB_TYPE_MIN);
    ASSERT(mob->cmd.spawn < MOB_TYPE_MAX);
    ASSERT(mob->cmd.spawn != MOB_TYPE_BASE);
    ASSERT(mob->cmd.spawn == MOB_TYPE_MISSILE ||
           mob->type == MOB_TYPE_BASE);

    ASSERT(mob->playerID < ARRAYSIZE(battle.bs.players));
    if (battle.bs.players[mob->playerID].credits >=
        MobType_GetCost(mob->cmd.spawn)) {
        battle.bs.players[mob->playerID].credits -=
            MobType_GetCost(mob->cmd.spawn);
        MobVector_Grow(&battle.mobs);
        size = MobVector_Size(&battle.mobs);
        spawn = MobVector_GetPtr(&battle.mobs, size - 1);

        Mob_Init(spawn, mob->cmd.spawn);
        spawn->playerID = mob->playerID;
        spawn->id = ++battle.lastMobID;
        spawn->pos = mob->pos;
        spawn->cmd.target = mob->cmd.target;

        mob->cmd.spawn = MOB_TYPE_INVALID;
        battle.bs.spawns++;
    }
}

void BattleDoMobMove(Mob *mob)
{
    FPoint origin;
    float distance;
    float speed;
    ASSERT(BattleCheckMobInvariants(mob));
    ASSERT(mob->alive);

    origin.x = mob->pos.x;
    origin.y = mob->pos.y;
    distance = FPoint_Distance(&origin, &mob->cmd.target);

    speed = Mob_GetSpeed(mob);

    if (distance <= speed) {
        mob->pos.x = mob->cmd.target.x;
        mob->pos.y = mob->cmd.target.y;
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
        mob->pos.x = newPos.x;
        mob->pos.y = newPos.y;
    }
    ASSERT(BattleCheckMobInvariants(mob));
}

bool BattleCheckMobCollision(const Mob *lhs, const Mob *rhs)
{
    FCircle lc, rc;
    bool canCollide;

    ASSERT(lhs->alive);
    ASSERT(rhs->alive);

    if (lhs->playerID == rhs->playerID) {
        // Players don't collide with themselves...
        return FALSE;
    }

    canCollide = FALSE;
    if (lhs->type == MOB_TYPE_BASE ||
        rhs->type == MOB_TYPE_BASE) {
        canCollide = TRUE;
    }
    if (lhs->type == MOB_TYPE_MISSILE ||
        rhs->type == MOB_TYPE_MISSILE) {
        canCollide = TRUE;
    }
    if (!canCollide) {
        return FALSE;
    }

    Mob_GetCircle(lhs, &lc);
    Mob_GetCircle(rhs, &rc);
    return FCircle_Intersect(&lc, &rc);
}

// Can the scanning mob see the target mob?
bool BattleCheckMobScan(const Mob *scanning, const Mob *target)
{
    FCircle sc, tc;

    ASSERT(scanning->alive);

    if (scanning->playerID == target->playerID) {
        // Players don't scan themselves...
        return FALSE;
    }

    Mob_GetSensorCircle(scanning, &sc);
    Mob_GetCircle(target, &tc);
    return FCircle_Intersect(&sc, &tc);
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

        if (mob->alive && mob->type == MOB_TYPE_MISSILE) {
            mob->fuel--;

            if (mob->fuel <= 0) {
                mob->alive = FALSE;
            }
        }

        if (mob->alive) {
            BattleDoMobMove(mob);
        }
    }

    // Spawn things
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);
        if (mob->alive && mob->cmd.spawn != MOB_TYPE_INVALID) {
            BattleDoMobSpawn(mob);
        }
    }

    // Process collisions
    for (uint32 outer = 0; outer < MobVector_Size(&battle.mobs); outer++) {
        Mob *oMob = MobVector_GetPtr(&battle.mobs, outer);

        if (!oMob->alive) {
            continue;
        }

        for (uint32 inner = outer + 1; inner < MobVector_Size(&battle.mobs);
            inner++) {
            Mob *iMob = MobVector_GetPtr(&battle.mobs, inner);

            if (iMob->alive) {
                if (BattleCheckMobCollision(oMob, iMob)) {
                    battle.bs.collisions++;

                    oMob->health -= MobType_GetMaxHealth(iMob->type);
                    iMob->health -= MobType_GetMaxHealth(oMob->type);

                    if (oMob->health <= 0) {
                        oMob->alive = FALSE;
                    }
                    if (iMob->health <= 0) {
                        iMob->alive = FALSE;
                    }
                    break;
                }
            }
        }
    }

    // Process scanning
    for (uint32 outer = 0; outer < MobVector_Size(&battle.mobs); outer++) {
        Mob *oMob = MobVector_GetPtr(&battle.mobs, outer);

        for (uint32 inner = outer + 1; inner < MobVector_Size(&battle.mobs);
            inner++) {
            Mob *iMob = MobVector_GetPtr(&battle.mobs, inner);

            if (oMob->alive &&
                !BitVector_GetRaw(oMob->playerID, &iMob->scannedBy)) {

                if (BattleCheckMobScan(oMob, iMob)) {
                    ASSERT(oMob->playerID < sizeof(iMob->scannedBy) * 8);
                    BitVector_SetRaw(oMob->playerID, &iMob->scannedBy);
                    battle.bs.sensorContacts++;
                }
            }

            if (iMob->alive &&
                !BitVector_GetRaw(iMob->playerID, &oMob->scannedBy)) {
                if (BattleCheckMobScan(iMob, oMob)) {
                    ASSERT(iMob->playerID < sizeof(oMob->scannedBy) * 8);
                    BitVector_SetRaw(iMob->playerID, &oMob->scannedBy);
                    battle.bs.sensorContacts++;
                }
            }
        }
    }

    // Destroy mobs and track player liveness
    for (uint32 i = 0; i < battle.bs.numPlayers; i++) {
        battle.bs.players[i].alive = FALSE;
    }
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);
        if (mob->alive) {
            PlayerID p = mob->playerID;
            battle.bs.players[p].alive = TRUE;
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
    if (livePlayers <= 1 ||
        battle.bs.tick >= battle.bp.timeLimit) {
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
    return MobVector_GetCArray(&battle.mobs);
}

void Battle_ReleaseMobs()
{
    ASSERT(battle.initialized);
    ASSERT(battle.mobsAcquired);
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
