/*
 * fleet.c --
 */

#include "fleet.h"
#include "random.h"
#include "IntMap.h"
#include "battle.h"

typedef struct FleetAI {
    BattlePlayerParams player;
    MobVector mobs;
    SensorMobVector sensors;
} FleetAI;

typedef struct FleetGlobalData {
    bool initialized;

    FleetAI *ais;
    uint32 numAIs;
} FleetGlobalData;

static FleetGlobalData fleet;

static void FleetRunAI(const BattleStatus *bs, FleetAI *ai);

void Fleet_Init()
{
    const BattleParams *bp = Battle_GetParams();
    ASSERT(MBUtil_IsZero(&fleet, sizeof(fleet)));

    fleet.numAIs = bp->numPlayers;
    fleet.ais = malloc(fleet.numAIs * sizeof(fleet.ais[0]));

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MBUtil_Zero(&fleet.ais[i], sizeof(fleet.ais[i]));
        fleet.ais[i].player = bp->players[i];
        MobVector_CreateEmpty(&fleet.ais[i].mobs);
        SensorMobVector_CreateEmpty(&fleet.ais[i].sensors);
    }

    fleet.initialized = TRUE;
}

void Fleet_Exit()
{
    ASSERT(fleet.initialized);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_Destroy(&fleet.ais[i].mobs);
        SensorMobVector_Destroy(&fleet.ais[i].sensors);
    }

    free(fleet.ais);
    fleet.initialized = FALSE;
}

void Fleet_RunTick(const BattleStatus *bs, Mob *mobs, uint32 numMobs)
{
    IntMap mobidMap;

    IntMap_Create(&mobidMap);
    IntMap_SetEmptyValue(&mobidMap, MOB_ID_INVALID);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_MakeEmpty(&fleet.ais[i].mobs);
        SensorMobVector_MakeEmpty(&fleet.ais[i].sensors);
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

        if (mob->scannedBy != 0) {
            for (PlayerID s = 0; s < fleet.numAIs; s++) {
                if (BitVector_GetRaw(s, &mob->scannedBy)) {
                    SensorMob *sm;
                    SensorMobVector_Grow(&fleet.ais[s].sensors);
                    sm = SensorMobVector_GetLastPtr(&fleet.ais[s].sensors);
                    SensorMob_InitFromMob(sm, mob);
                }
            }
        }
    }

    /*
     * Run the AI for all the players.
     */
    for (uint32 p = 0; p < fleet.numAIs; p++) {
        FleetRunAI(bs, &fleet.ais[p]);
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
            ASSERT(i != MOB_ID_INVALID);
            ASSERT(mobs[i].id == mob->id);
            mobs[i].cmd = mob->cmd;
        }
    }

    IntMap_Destroy(&mobidMap);
}

static int FleetFindClosestSensor(FleetAI *ai, Mob *m)
{
    float distance;
    int index;
    SensorMob *sm;

    ASSERT(SensorMobVector_Size(&ai->sensors) > 0);

    index = 0;
    sm = SensorMobVector_GetPtr(&ai->sensors, index);
    distance = FPoint_Distance(&m->pos, &sm->pos);

    for (uint i = 1; i < SensorMobVector_Size(&ai->sensors); i++) {
        sm = SensorMobVector_GetPtr(&ai->sensors, index);
        float curDistance = FPoint_Distance(&m->pos, &sm->pos);
        if (curDistance < distance) {
            distance = curDistance;
            index = i;
        }
    }

    return index;
}

static void FleetRunAI(const BattleStatus *bs, FleetAI *ai)
{
    const BattleParams *bp = Battle_GetParams();

    ASSERT(ai->player.aiType == FLEET_AI_DUMMY ||
           ai->player.aiType == FLEET_AI_SIMPLE);

    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        // Dummy fleet doesn't shoot.
        if (ai->player.aiType == FLEET_AI_SIMPLE &&
            SensorMobVector_Size(&ai->sensors) > 0) {
            if (mob->type == MOB_TYPE_FIGHTER) {
                SensorMob *sm;
                int s = FleetFindClosestSensor(ai, mob);

                sm = SensorMobVector_GetPtr(&ai->sensors, s);
                mob->cmd.target = sm->pos;
                if (Random_Int(0, 20) == 0) {
                    mob->cmd.spawn = MOB_TYPE_MISSILE;
                }
            }
        }

        if (mob->type == MOB_TYPE_BASE) {
            if (Random_Int(0, 100) == 0) {
                mob->cmd.spawn = MOB_TYPE_FIGHTER;
            }
        }

        if (mob->pos.x == mob->cmd.target.x &&
            mob->pos.y == mob->cmd.target.y) {
            if (Random_Bit()) {
                mob->cmd.target.x = Random_Float(0.0f, bp->width);
                mob->cmd.target.y = Random_Float(0.0f, bp->height);
            } else {
                // Head towards the center more frequently to get more
                // cross-team collisions for testing.
                mob->cmd.target.x = bp->width / 2;
                mob->cmd.target.y = bp->height / 2;
            }
        }
    }
}
