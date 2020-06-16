/*
 * fleet.c -- part of SpaceRobots2
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

#include "fleet.h"
#include "random.h"
#include "IntMap.h"
#include "battle.h"

typedef struct FleetGlobalData {
    bool initialized;

    FleetAI *ais;
    uint32 numAIs;
    MobVector aiMobs;
    MobVector aiSensors;
} FleetGlobalData;

static FleetGlobalData fleet;

static void FleetGetOps(FleetAI *ai);
static void FleetRunAITick(const BattleStatus *bs, FleetAI *ai);
static void DummyFleetRunAITick(void *aiHandle);

void Fleet_Init()
{
    const BattleParams *bp = Battle_GetParams();
    MBUtil_Zero(&fleet, sizeof(fleet));

    fleet.numAIs = bp->numPlayers;
    fleet.ais = MBUtil_ZAlloc(fleet.numAIs * sizeof(fleet.ais[0]));

    // We need at least neutral and two fleets.
    ASSERT(fleet.numAIs >= 3);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        ASSERT(MBUtil_IsZero(&fleet.ais[i], sizeof(fleet.ais[i])));

        fleet.ais[i].id = i;
        fleet.ais[i].player = bp->players[i];

        ASSERT(bp->players[i].aiType < FLEET_AI_MAX);
        ASSERT(bp->players[i].aiType != FLEET_AI_INVALID);
        ASSERT(bp->players[i].aiType == FLEET_AI_NEUTRAL ||
               i != PLAYER_ID_NEUTRAL);
        ASSERT(bp->players[i].aiType != FLEET_AI_NEUTRAL ||
               i == PLAYER_ID_NEUTRAL);

        MobSet_Create(&fleet.ais[i].mobs);
        MobSet_Create(&fleet.ais[i].sensors);

        FleetGetOps(&fleet.ais[i]);
        if (fleet.ais[i].ops.createFleet != NULL) {
            fleet.ais[i].aiHandle = fleet.ais[i].ops.createFleet(&fleet.ais[i]);
        } else {
            fleet.ais[i].aiHandle = &fleet.ais[i];
        }
    }

    MobVector_CreateEmpty(&fleet.aiMobs);
    MobVector_CreateEmpty(&fleet.aiSensors);

    fleet.initialized = TRUE;
}

void Fleet_Exit()
{
    ASSERT(fleet.initialized);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        if (fleet.ais[i].ops.destroyFleet != NULL) {
            fleet.ais[i].ops.destroyFleet(fleet.ais[i].aiHandle);
        }

        MobSet_Destroy(&fleet.ais[i].mobs);
        MobSet_Destroy(&fleet.ais[i].sensors);
    }

    MobVector_Destroy(&fleet.aiMobs);
    MobVector_Destroy(&fleet.aiSensors);
    free(fleet.ais);
    fleet.initialized = FALSE;
}

static void FleetGetOps(FleetAI *ai)
{
    MBUtil_Zero(&ai->ops, sizeof(ai->ops));

    switch(ai->player.aiType) {
        case FLEET_AI_NEUTRAL:
        case FLEET_AI_DUMMY:
            ai->ops.aiName = "DummyFleet";
            ai->ops.aiAuthor = "Michael Banack";
            ai->ops.runAITick = &DummyFleetRunAITick;
            break;
        case FLEET_AI_SIMPLE:
            SimpleFleet_GetOps(&ai->ops);
            break;
        case FLEET_AI_BOB:
            BobFleet_GetOps(&ai->ops);
            break;
        case FLEET_AI_MAPPER:
            MapperFleet_GetOps(&ai->ops);
            break;
        case FLEET_AI_CLOUD:
            CloudFleet_GetOps(&ai->ops);
            break;
        case FLEET_AI_GATHER:
            GatherFleet_GetOps(&ai->ops);
            break;
        default:
            PANIC("Unknown AI type=%d\n", ai->player.aiType);
    }
}


void Fleet_RunTick(const BattleStatus *bs, Mob *mobs, uint32 numMobs)
{
    const BattleParams *bp = Battle_GetParams();

    /*
     * Make sure the vectors are big enough that we don't
     * resize while filling them up.
     */
    MobVector_MakeEmpty(&fleet.aiMobs);
    MobVector_MakeEmpty(&fleet.aiSensors);
    MobVector_EnsureCapacity(&fleet.aiMobs, numMobs);
    MobVector_EnsureCapacity(&fleet.aiSensors, numMobs * fleet.numAIs);
    MobVector_Pin(&fleet.aiMobs);
    MobVector_Pin(&fleet.aiSensors);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobSet_MakeEmpty(&fleet.ais[i].mobs);
        MobSet_MakeEmpty(&fleet.ais[i].sensors);
        fleet.ais[i].credits = bs->players[i].credits;
    }

    /*
     * Sort the incoming ships by player.
     */
    for (uint32 i = 0; i < numMobs; i++) {
        Mob *mob = &mobs[i];
        Mob *m;
        PlayerID p = mob->playerID;

        ASSERT(Mob_CheckInvariants(mob));

        MobVector_Grow(&fleet.aiMobs);
        m = MobVector_GetLastPtr(&fleet.aiMobs);
        *m = *mob;
        Mob_MaskForAI(m);

        ASSERT(p == PLAYER_ID_NEUTRAL || p < fleet.numAIs);
        if (p != PLAYER_ID_NEUTRAL) {
            MobSet_Add(&fleet.ais[p].mobs, m);
        }

        if (mob->scannedBy != 0) {
            for (PlayerID s = 0; s < fleet.numAIs; s++) {
                if (BitVector_GetRaw(s, &mob->scannedBy)) {
                    MobVector_Grow(&fleet.aiSensors);
                    m = MobVector_GetLastPtr(&fleet.aiSensors);
                    *m = *mob;
                    Mob_MaskForSensor(m);
                    ASSERT(Mob_CheckInvariants(m));
                    MobSet_Add(&fleet.ais[s].sensors, m);
                }
            }
        }
    }

    /*
     * Run the AI for all the players.
     */
    for (uint32 p = 0; p < fleet.numAIs; p++) {
        fleet.ais[p].tick = bs->tick;
        FleetRunAITick(bs, &fleet.ais[p]);
    }

    /*
     * Write the commands back to the original mob array.
     */
    for (uint32 i = 0; i < numMobs; i++) {
        Mob *mob = &mobs[i];
        Mob *m = MobVector_GetPtr(&fleet.aiMobs, i);


        if (mob->mobid != m->mobid) {
            PANIC("Fleet mob list corruption!\n");
        }

        FPoint_Clamp(&m->cmd.target, 0.0f, bp->width, 0.0f, bp->height);
        mob->cmd = m->cmd;
        mob->aiMobHandle = m->aiMobHandle;
    }

    MobVector_Unpin(&fleet.aiMobs);
    MobVector_Unpin(&fleet.aiSensors);
}

static void FleetRunAITick(const BattleStatus *bs, FleetAI *ai)
{
    MobIt mit;

    if (ai->ops.mobSpawned != NULL ||
        ai->ops.mobDestroyed != NULL ||
        ai->ops.runAIMob != NULL) {
        MobIt_Start(&ai->mobs, &mit);
        while (MobIt_HasNext(&mit)) {
            Mob *m = MobIt_Next(&mit);
            ASSERT(Mob_CheckInvariants(m));
            if (m->birthTick == bs->tick) {
                if (ai->ops.mobSpawned != NULL) {
                    m->aiMobHandle = ai->ops.mobSpawned(ai->aiHandle, m);
                } else {
                    m->aiMobHandle = m;
                }
            }
        }
    }

    if (ai->ops.runAITick != NULL) {
        ai->ops.runAITick(ai->aiHandle);
    }

    if (ai->ops.runAIMob != NULL) {
        MobIt_Start(&ai->mobs, &mit);
        while (MobIt_HasNext(&mit)) {
            Mob *m = MobIt_Next(&mit);
            ai->ops.runAIMob(ai->aiHandle, m->aiMobHandle);
        }
    }

    if (ai->ops.mobDestroyed != NULL) {
        MobIt_Start(&ai->mobs, &mit);
        while (MobIt_HasNext(&mit)) {
            Mob *m = MobIt_Next(&mit);
            if (!m->alive) {
                ai->ops.mobDestroyed(ai->aiHandle, m->aiMobHandle);
            }
        }
    }
}

static void DummyFleetRunAITick(void *handle)
{
    FleetAI *ai = handle;
    MobIt mit;
    const BattleParams *bp = Battle_GetParams();

    /*
     * We use this function for the neutral player, but don't actually queue any
     * mobs for them to process.
     */
    ASSERT(ai->player.aiType == FLEET_AI_DUMMY ||
           ai->player.aiType == FLEET_AI_NEUTRAL);

    MobIt_Start(&ai->mobs, &mit);
    while (MobIt_HasNext(&mit)) {
        Mob *mob = MobIt_Next(&mit);
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
        if (mob->birthTick == ai->tick) {
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
