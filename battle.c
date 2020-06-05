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
    Mob mobs[100];
    bool mobsAcquired;
} BattleGlobalData;

static BattleGlobalData battle;

void Battle_Init(const BattleParams *bp)
{
    ASSERT(bp != NULL);
    ASSERT(Util_IsZero(&battle, sizeof(battle)));

    ASSERT(bp->numPlayers > 0);
    battle.bp = *bp;

    for (uint32 i = 0; i < ARRAYSIZE(battle.mobs); i++) {
        Mob *mob = &battle.mobs[i];
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

    return TRUE;
}

void BattleMoveMobToTarget(Mob *mob)
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
    FQuad lq, rq;
    ASSERT(lhs->alive);
    ASSERT(rhs->alive);
    if (lhs->playerID == rhs->playerID) {
        return FALSE;
    }

    Mob_GetQuad(lhs, &lq);
    Mob_GetQuad(rhs, &rq);
    return FQuad_Intersect(&lq, &rq);
}

void Battle_RunTick()
{
    ASSERT(battle.bs.tick < MAX_UINT32);
    battle.bs.tick++;

    // Run Physics
    for (uint32 i = 0; i < ARRAYSIZE(battle.mobs); i++) {
        Mob *mob = &battle.mobs[i];
        ASSERT(BattleCheckMobInvariants(mob));

        if (mob->alive) {
            BattleMoveMobToTarget(mob);
        }
    }

    // Check for collisions
    for (uint32 outer = 0; outer < ARRAYSIZE(battle.mobs); outer++) {
        Mob *oMob = &battle.mobs[outer];

        if (!oMob->alive) {
            continue;
        }

        for (uint32 inner = outer + 1; inner < ARRAYSIZE(battle.mobs);
            inner++) {
            Mob *iMob = &battle.mobs[inner];

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
    for (uint32 i = 0; i < ARRAYSIZE(battle.mobs); i++) {
        Mob *mob = &battle.mobs[i];
        if (mob->alive) {
            if (livePlayer == -1) {
                livePlayer = mob->playerID;
            } else if (livePlayer != mob->playerID) {
                battle.bs.finished = FALSE;
                break;
            }
        }
    }
    if (livePlayer == -1) {
        battle.bs.finished = FALSE;
    }
}

Mob *Battle_AcquireMobs(uint32 *numMobs)
{
    ASSERT(battle.initialized);
    ASSERT(numMobs != NULL);
    ASSERT(!battle.mobsAcquired);

    battle.mobsAcquired = TRUE;

    *numMobs = ARRAYSIZE(battle.mobs);
    return &battle.mobs[0];
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
