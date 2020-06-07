/*
 * battle.c --
 */

#include "battle.h"
#include "random.h"

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
    ASSERT(Util_IsZero(&battle, sizeof(battle)));

    ASSERT(bp->numPlayers > 0);
    battle.bp = *bp;

    MobVector_Create(&battle.mobs, 100, 1024);
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);

        Util_Zero(mob, sizeof(*mob));

        mob->alive = TRUE;
        if (Random_Bit()) {
            // Force more intra-team collisions for testing.
            ASSERT(battle.bp.numPlayers >= 2);
            mob->playerID = i % 2;
        } else {
            mob->playerID = i % battle.bp.numPlayers;
        }
        mob->id = ++battle.lastMobID;
        mob->type = Random_Int(MOB_TYPE_MIN, MOB_TYPE_MAX - 1);
        mob->fuel = MobType_GetMaxFuel(mob->type);
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
    ASSERT(mob->pos.x >= 0.0f);
    ASSERT(mob->pos.y >= 0.0f);
    ASSERT(mob->pos.x <= (uint32)battle.bp.width);
    ASSERT(mob->pos.y <= (uint32)battle.bp.height);

    ASSERT(mob->cmd.target.x >= 0.0f);
    ASSERT(mob->cmd.target.y >= 0.0f);
    ASSERT(mob->cmd.target.x <= (uint32)battle.bp.width);
    ASSERT(mob->cmd.target.y <= (uint32)battle.bp.height);
    ASSERT(!(mob->removeMob && mob->alive));

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
    ASSERT(mob->cmd.spawn == MOB_TYPE_ROCKET);

    MobVector_Grow(&battle.mobs);
    size = MobVector_Size(&battle.mobs);
    spawn = MobVector_GetPtr(&battle.mobs, size - 1);

    Util_Zero(spawn, sizeof(*spawn));
    spawn->alive = TRUE;
    spawn->playerID = mob->playerID;
    spawn->id = ++battle.lastMobID;
    spawn->type = mob->cmd.spawn;
    spawn->fuel = MobType_GetMaxFuel(spawn->type);
    spawn->pos = mob->pos;
    spawn->cmd.target = mob->cmd.target;

    mob->cmd.spawn = MOB_TYPE_INVALID;
    battle.bs.spawns++;
}

void BattleDoMobMove(Mob *mob)
{
    FPoint origin;
    float distance;
    float speed;
    ASSERT(BattleCheckMobInvariants(mob));
    ASSERT(mob->alive);

    //XXX: Should we be using the center of the quad?
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
    ASSERT(lhs->alive);
    ASSERT(rhs->alive);
    if (lhs->playerID == rhs->playerID) {
        return FALSE;
    }

    Mob_GetCircle(lhs, &lc);
    Mob_GetCircle(rhs, &rc);
    return FCircle_Intersect(&lc, &rc);
}

void Battle_RunTick()
{
    ASSERT(battle.bs.tick < MAX_UINT32);
    battle.bs.tick++;

    // Run Physics
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);
        ASSERT(BattleCheckMobInvariants(mob));

        if (mob->alive && mob->type == MOB_TYPE_ROCKET) {
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

    // Check for collisions
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
                    oMob->alive = FALSE;
                    iMob->alive = FALSE;
                    break;
                }
            }
        }
    }

    // Check for victory!
    int32 livePlayer = -1;
    battle.bs.finished = TRUE;
    for (uint32 i = 0; i < MobVector_Size(&battle.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&battle.mobs, i);
        if (mob->alive) {
            if (livePlayer == -1) {
                livePlayer = mob->playerID;
            } else if (livePlayer != mob->playerID) {
                battle.bs.finished = FALSE;
                break;
            }
        } else {
            /*
             * Keep the mob around for one tick after it dies so the
             * fleet AI's can see that it died.
             */
            if (mob->removeMob) {
                uint32 size = MobVector_Size(&battle.mobs);
                Mob *last = MobVector_GetPtr(&battle.mobs, size - 1);
                *mob = *last;
                MobVector_Shrink(&battle.mobs);

                // Redo the current index
                i--;
            } else {
                mob->removeMob = TRUE;
            }
        }
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
