/*
 * fleet.c --
 */

#include "fleet.h"
#include "random.h"
#include "IntMap.h"
#include "battle.h"

struct FleetAI;

#define FLEET_SCAN_BASE     (1 << MOB_TYPE_BASE)
#define FLEET_SCAN_FIGHTER  (1 << MOB_TYPE_FIGHTER)
#define FLEET_SCAN_MISSILE  (1 << MOB_TYPE_MISSILE)
#define FLEET_SCAN_LOOT_BOX (1 << MOB_TYPE_LOOT_BOX)

#define FLEET_SCAN_SHIP (FLEET_SCAN_BASE | FLEET_SCAN_FIGHTER)
#define FLEET_SCAN_ALL (FLEET_SCAN_SHIP |    \
                        FLEET_SCAN_MISSILE | \
                        FLEET_SCAN_LOOT_BOX)

typedef struct SimpleFleetData {
    FPoint basePos;
    SensorMob enemyBase;
    uint enemyBaseAge;
} SimpleFleetData;

typedef struct FleetAIOps {
    void (*create)(struct FleetAI *ai);
    void (*destroy)(struct FleetAI *ai);
    void (*runAI)(struct FleetAI *ai);
} FleetAIOps;

typedef struct FleetAI {
    PlayerID id;

    /*
     * Filled out by FleetAI controller.
     */
    FleetAIOps ops;
    void *aiHandle;

    BattlePlayerParams player;
    int credits;
    MobVector mobs;
    SensorMobVector sensors;
} FleetAI;

typedef struct FleetGlobalData {
    bool initialized;

    FleetAI *ais;
    uint32 numAIs;
} FleetGlobalData;

static FleetGlobalData fleet;

static void FleetGetOps(FleetAI *ai);
static void FleetRunAI(FleetAI *ai);
static void SimpleFleetCreate(FleetAI *ai);
static void SimpleFleetDestroy(FleetAI *ai);
static void SimpleFleetRunAI(FleetAI *ai);
static void DummyFleetRunAI(FleetAI *ai);

void Fleet_Init()
{
    const BattleParams *bp = Battle_GetParams();
    ASSERT(MBUtil_IsZero(&fleet, sizeof(fleet)));

    fleet.numAIs = bp->numPlayers;
    fleet.ais = malloc(fleet.numAIs * sizeof(fleet.ais[0]));

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MBUtil_Zero(&fleet.ais[i], sizeof(fleet.ais[i]));
        fleet.ais[i].id = i;
        fleet.ais[i].player = bp->players[i];
        MobVector_CreateEmpty(&fleet.ais[i].mobs);
        SensorMobVector_CreateEmpty(&fleet.ais[i].sensors);

        FleetGetOps(&fleet.ais[i]);
        if (fleet.ais[i].ops.create != NULL) {
            fleet.ais[i].ops.create(&fleet.ais[i]);
        }
    }

    fleet.initialized = TRUE;
}

void Fleet_Exit()
{
    ASSERT(fleet.initialized);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_Destroy(&fleet.ais[i].mobs);
        SensorMobVector_Destroy(&fleet.ais[i].sensors);

        if (fleet.ais[i].ops.destroy != NULL) {
            fleet.ais[i].ops.destroy(&fleet.ais[i]);
        }
    }

    free(fleet.ais);
    fleet.initialized = FALSE;
}


static void FleetGetOps(FleetAI *ai)
{
    MBUtil_Zero(&ai->ops, sizeof(ai->ops));

    switch(ai->player.aiType) {
        case FLEET_AI_DUMMY:
            ai->ops.runAI = &DummyFleetRunAI;
            break;
        case FLEET_AI_SIMPLE:
            ai->ops.create = &SimpleFleetCreate;
            ai->ops.destroy = &SimpleFleetDestroy;
            ai->ops.runAI = &SimpleFleetRunAI;
            break;
        default:
            PANIC("Unknown AI type=%d\n", ai->player.aiType);
    }
}


void Fleet_RunTick(const BattleStatus *bs, Mob *mobs, uint32 numMobs)
{
    IntMap mobidMap;

    IntMap_Create(&mobidMap);
    IntMap_SetEmptyValue(&mobidMap, MOB_ID_INVALID);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_MakeEmpty(&fleet.ais[i].mobs);
        SensorMobVector_MakeEmpty(&fleet.ais[i].sensors);
        fleet.ais[i].credits = bs->players[i].credits;
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
        FleetRunAI(&fleet.ais[p]);
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

static int FleetFindClosestSensor(FleetAI *ai, const FPoint *pos, uint scanFilter)
{
    float distance;
    int index = -1;
    SensorMob *sm;

    for (uint i = 0; i < SensorMobVector_Size(&ai->sensors); i++) {
        sm = SensorMobVector_GetPtr(&ai->sensors, i);
        if (!sm->alive) {
            continue;
        }
        if (((1 << sm->type) & scanFilter) != 0) {
            float curDistance = FPoint_Distance(pos, &sm->pos);
            if (index == -1 || curDistance < distance) {
                distance = curDistance;
                index = i;
            }
        }
    }

    return index;
}


static void FleetRunAI(FleetAI *ai)
{
    ASSERT(ai->ops.runAI != NULL);
    ai->ops.runAI(ai);
}

static void SimpleFleetCreate(FleetAI *ai)
{
    SimpleFleetData *sf;
    ASSERT(ai != NULL);

    sf = malloc(sizeof(*sf));
    MBUtil_Zero(sf, sizeof(*sf));
    ai->aiHandle = sf;
}

static void SimpleFleetDestroy(FleetAI *ai)
{
    SimpleFleetData *sf;
    ASSERT(ai != NULL);

    sf = ai->aiHandle;
    ASSERT(sf != NULL);
    free(sf);
    ai->aiHandle = NULL;
}

static void SimpleFleetRunAI(FleetAI *ai)
{
    SimpleFleetData *sf = ai->aiHandle;
    const BattleParams *bp = Battle_GetParams();
    uint targetScanFilter = FLEET_SCAN_SHIP | FLEET_SCAN_LOOT_BOX;

    ASSERT(ai->player.aiType == FLEET_AI_SIMPLE);

    // If we've found the enemy base, assume it's still there.
    int enemyBaseIndex = FleetFindClosestSensor(ai, &sf->basePos, FLEET_SCAN_BASE);
    if (enemyBaseIndex != -1) {
        SensorMob *sm = SensorMobVector_GetPtr(&ai->sensors, enemyBaseIndex);
        ASSERT(sm->type == MOB_TYPE_BASE);
        sf->enemyBase = *sm;
        sf->enemyBaseAge = 0;
    } else if (sf->enemyBase.type == MOB_TYPE_BASE &&
               sf->enemyBaseAge < 200) {
        SensorMobVector_Grow(&ai->sensors);
        SensorMob *sm = SensorMobVector_GetLastPtr(&ai->sensors);
        *sm = sf->enemyBase;
        sf->enemyBaseAge++;
    }

    int targetIndex = FleetFindClosestSensor(ai, &sf->basePos, targetScanFilter);

    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            if (targetIndex != -1) {
                    SensorMob *sm;
                    sm = SensorMobVector_GetPtr(&ai->sensors, targetIndex);
                    mob->cmd.target = sm->pos;

                    if (Random_Int(0, 20) == 0) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (Random_Bit()) {
                    mob->cmd.target.x = Random_Float(0.0f, bp->width);
                    mob->cmd.target.y = Random_Float(0.0f, bp->height);
                } else {
                    mob->cmd.target = sf->basePos;
                }
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = FLEET_SCAN_SHIP | FLEET_SCAN_MISSILE;
            int s = FleetFindClosestSensor(ai, &mob->pos, scanFilter);
            if (s != -1) {
                SensorMob *sm;
                sm = SensorMobVector_GetPtr(&ai->sensors, s);
                mob->cmd.target = sm->pos;
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 &&
                Random_Int(0, 100) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                mob->cmd.target.x = Random_Float(0.0f, bp->width);
                mob->cmd.target.y = Random_Float(0.0f, bp->height);
            }
        } else if (mob->type == MOB_TYPE_LOOT_BOX) {
            mob->cmd.target = sf->basePos;
        }
    }
}


static void DummyFleetRunAI(FleetAI *ai)
{
    const BattleParams *bp = Battle_GetParams();

    ASSERT(ai->player.aiType == FLEET_AI_DUMMY);

    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);
        bool newTarget = FALSE;

        if (mob->type == MOB_TYPE_BASE) {
            if (Random_Int(0, 100) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            }
        }

        if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            newTarget = TRUE;
        }
        if (mob->type != MOB_TYPE_BASE &&
            Random_Int(0, 100) == 0) {
            newTarget = TRUE;
        }
        if (mob->age == 0) {
            newTarget = TRUE;
        }

        if (newTarget) {
            if (Random_Bit()) {
                mob->cmd.target.x = Random_Float(0.0f, bp->width);
                mob->cmd.target.y = Random_Float(0.0f, bp->height);
            }
        }
    }
}
