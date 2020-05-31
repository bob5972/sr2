/*
 * battle.c --
 */

#include "battle.h"
#include "random.h"

#define MOB_SPEED (1.0f)
#define MICRON (0.1f)

typedef struct BattleGlobalData {
    bool initialized;
    BattleParams bp;

    BattleStatus bs;
    bool statusAcquired;

    BattleMob mobs[100];
    bool mobsAcquired;
} BattleGlobalData;

static BattleGlobalData battle;

void Battle_Init(const BattleParams *bp)
{
    ASSERT(bp != NULL);
    ASSERT(Util_IsZero(&battle, sizeof(battle)));
    battle.bp = *bp;

    for (uint32 i = 0; i < ARRAYSIZE(battle.mobs); i++) {
        BattleMob *mob = &battle.mobs[i];
        mob->alive = TRUE;
        if (Random_Bit()) {
            // Force more intra-team collisions.
            mob->playerID = i % 2;
        } else {
            mob->playerID = i % 8;
        }
        mob->pos.x = Random_Float(0.0f, battle.bp.width);
        mob->pos.y = Random_Float(0.0f, battle.bp.height);
        mob->pos.w = Random_Int(10, 80);
        mob->pos.h = Random_Int(10, 80);

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

bool BattleCheckMobInvariants(const BattleMob *mob)
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

void BattleMoveMobToTarget(BattleMob *mob)
{
    FPoint origin;
    float distance;
    ASSERT(BattleCheckMobInvariants(mob));
    ASSERT(mob->alive);

    //XXX: Should we be using the center of the quad?
    origin.x = mob->pos.x;
    origin.y = mob->pos.y;
    distance = FPoint_Distance(&origin, &mob->cmd.target);

    if (distance <= MOB_SPEED) {
        mob->pos.x = mob->cmd.target.x;
        mob->pos.y = mob->cmd.target.y;
    } else {
        float dx = mob->cmd.target.x - mob->pos.x;
        float dy = mob->cmd.target.y - mob->pos.y;
        float factor = MOB_SPEED / distance;
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
//                 (float)(MOB_SPEED + MICRON),
//                 (float)(FPoint_Distance(&newPos, &mob->pos) - (MOB_SPEED + MICRON)));

        //XXX: This ASSERT is hitting for resonable-seeming micron values...?
        ASSERT(FPoint_Distance(&newPos, &origin) <= MOB_SPEED + MICRON);
        mob->pos.x = newPos.x;
        mob->pos.y = newPos.y;
    }
    ASSERT(BattleCheckMobInvariants(mob));
}

bool BattleCheckMobCollision(const BattleMob *lhs, const BattleMob *rhs)
{
    ASSERT(lhs->alive);
    ASSERT(rhs->alive);
    if (lhs->playerID == rhs->playerID) {
        return FALSE;
    }
    return FQuad_Intersect(&lhs->pos, &rhs->pos);
}

void Battle_RunTick()
{
    ASSERT(battle.bs.tick < MAX_UINT32);
    battle.bs.tick++;

    // Run AI
    for (uint32 i = 0; i < ARRAYSIZE(battle.mobs); i++) {
        BattleMob *mob = &battle.mobs[i];
        ASSERT(BattleCheckMobInvariants(mob));
        if (mob->pos.x == mob->cmd.target.x &&
            mob->pos.y == mob->cmd.target.y) {
            battle.bs.targetsReached++;
            //Warning("Mob %d has reached target (%f, %f)\n",
            //        i, mob->cmd.target.x, mob->cmd.target.y);
            if (Random_Bit()) {
                mob->cmd.target.x = Random_Float(0.0f, battle.bp.width);
                mob->cmd.target.y = Random_Float(0.0f, battle.bp.height);
            } else {
                // Head towards the center more frequently to get more
                // cross-team collisions.
                mob->cmd.target.x = battle.bp.width/2;
                mob->cmd.target.y = battle.bp.height/2;
            }
        }
        ASSERT(BattleCheckMobInvariants(mob));
    }

    // Run Physics
    for (uint32 i = 0; i < ARRAYSIZE(battle.mobs); i++) {
        BattleMob *mob = &battle.mobs[i];
        if (mob->alive) {
            BattleMoveMobToTarget(mob);
        }
    }

    // Check for collisions
    for (uint32 outer = 0; outer < ARRAYSIZE(battle.mobs); outer++) {
        BattleMob *oMob = &battle.mobs[outer];

        if (!oMob->alive) {
            continue;
        }

        for (uint32 inner = outer + 1; inner < ARRAYSIZE(battle.mobs);
            inner++) {
            BattleMob *iMob = &battle.mobs[inner];

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
    battle.bs.finished = TRUE;
    for (uint32 i = 0; i < ARRAYSIZE(battle.mobs); i++) {
        BattleMob *mob = &battle.mobs[i];
        if (mob->alive) {
            battle.bs.finished = FALSE;
            break;
        }
    }
}

const BattleMob *Battle_AcquireMobs(uint32 *numMobs)
{
    ASSERT(numMobs != NULL);
    ASSERT(!battle.mobsAcquired);

    battle.mobsAcquired = TRUE;

    *numMobs = ARRAYSIZE(battle.mobs);
    return &battle.mobs[0];
}

void Battle_ReleaseMobs()
{
    ASSERT(battle.mobsAcquired);
    battle.mobsAcquired = FALSE;
}

const BattleStatus *Battle_AcquireStatus()
{
    ASSERT(!battle.statusAcquired);

    battle.statusAcquired = TRUE;
    return &battle.bs;
}

void Battle_ReleaseStatus()
{
    ASSERT(battle.statusAcquired);
    battle.statusAcquired = FALSE;
}
