/*
 * fleet.c --
 */

#include "fleet.h"
#include "random.h"
#include "IntMap.h"

typedef struct FleetAI {
    MobVector mobs;
} FleetAI;

typedef struct FleetGlobalData {
    bool initialized;

    FleetAI *ais;
    uint32 numAIs;
} FleetGlobalData;

static FleetGlobalData fleet;

void Fleet_Init()
{
    const BattleParams *bp = Battle_GetParams();
    ASSERT(MBUtil_IsZero(&fleet, sizeof(fleet)));

    fleet.numAIs = bp->numPlayers;
    fleet.ais = malloc(fleet.numAIs * sizeof(fleet.ais[0]));

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_CreateEmpty(&fleet.ais[i].mobs);
    }

    fleet.initialized = TRUE;
}

void Fleet_Exit()
{
    ASSERT(fleet.initialized);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_Destroy(&fleet.ais[i].mobs);
    }

    free(fleet.ais);
    fleet.initialized = FALSE;
}

void Fleet_RunTick(Mob *mobs, uint32 numMobs)
{
    IntMap mobidMap;
    const BattleParams *bp = Battle_GetParams();

    IntMap_Create(&mobidMap);
    IntMap_SetEmptyValue(&mobidMap, MOB_INVALID_ID);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_MakeEmpty(&fleet.ais[i].mobs);
    }

    /*
     * Sort the incoming ships by player.
     */
    for (uint32 i = 0; i < numMobs; i++) {
        Mob *mob = &mobs[i];
        PlayerID p = mob->playerID;

        IntMap_Put(&mobidMap, mob->id, i);

        ASSERT(p < fleet.numAIs);
        if (mob->alive) {
            Mob *m;
            uint32 oldSize = MobVector_Size(&fleet.ais[p].mobs);
            MobVector_GrowBy(&fleet.ais[p].mobs, 1);
            m = MobVector_GetPtr(&fleet.ais[p].mobs, oldSize);
            *m = *mob;
        }
    }

    /*
     * Run the AI for all the players.
     */
    for (uint32 p = 0; p < fleet.numAIs; p++) {
        for (uint32 m = 0; m < MobVector_Size(&fleet.ais[p].mobs); m++) {
            Mob *mob = MobVector_GetPtr(&fleet.ais[p].mobs, m);

            if (mob->type == MOB_TYPE_FIGHTER) {
                if (Random_Int(0, 200) == 0) {
                    mob->cmd.spawn = MOB_TYPE_MISSILE;
                }
            }

            if (mob->pos.x == mob->cmd.target.x &&
                mob->pos.y == mob->cmd.target.y) {
                if (Random_Bit()) {
                    mob->cmd.target.x = Random_Float(0.0f, bp->width);
                    mob->cmd.target.y = Random_Float(0.0f, bp->height);
                } else {
                    // Head towards the center more frequently to get more
                    // cross-team collisions.
                    mob->cmd.target.x = bp->width/2;
                    mob->cmd.target.y = bp->height/2;
                }
            }
        }
    }

    /*
     * Write the commands back to the original mob array.
     */
    for (uint32 p = 0; p < fleet.numAIs; p++) {
        for (uint32 m = 0; m < MobVector_Size(&fleet.ais[p].mobs); m++) {
            uint32 i;
            Mob *mob = MobVector_GetPtr(&fleet.ais[p].mobs, m);
            ASSERT(mob->playerID == p);

            i = IntMap_Get(&mobidMap, mob->id);
            ASSERT(i != MOB_INVALID_ID);
            mobs[i].cmd = mob->cmd;
        }
    }

    IntMap_Destroy(&mobidMap);
}
