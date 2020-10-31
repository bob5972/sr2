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

typedef struct Fleet {
    bool initialized;

    FleetAI *ais;
    uint32 numAIs;
    MobVector aiMobs;
    MobVector aiSensors;

    BattleScenario bsc;
    RandomState rs;
} Fleet;

static void FleetGetOps(FleetAIType aiType, FleetAIOps *ops);
static void FleetRunAITick(const BattleStatus *bs, FleetAI *ai);

Fleet *Fleet_Create(const BattleScenario *bsc,
                    uint64 seed)
{
    Fleet *fleet;

    fleet = malloc(sizeof(*fleet));
    MBUtil_Zero(fleet, sizeof(*fleet));

    RandomState_CreateWithSeed(&fleet->rs, seed);

    fleet->bsc = *bsc;

    fleet->numAIs = bsc->bp.numPlayers;
    fleet->ais = MBUtil_ZAlloc(fleet->numAIs * sizeof(fleet->ais[0]));

    // We need at least neutral and two fleets.
    ASSERT(fleet->numAIs >= 3);

    for (uint32 i = 0; i < fleet->numAIs; i++) {
        ASSERT(MBUtil_IsZero(&fleet->ais[i], sizeof(fleet->ais[i])));

        fleet->ais[i].id = i;

        fleet->ais[i].player = bsc->players[i];
        if (bsc->players[i].mreg != NULL) {
            fleet->ais[i].player.mreg =
                MBRegistry_AllocCopy(bsc->players[i].mreg);
        }

        ASSERT(bsc->players[i].aiType < FLEET_AI_MAX);
        ASSERT(bsc->players[i].aiType != FLEET_AI_INVALID);
        ASSERT(bsc->players[i].aiType == FLEET_AI_NEUTRAL ||
               i != PLAYER_ID_NEUTRAL);
        ASSERT(bsc->players[i].aiType != FLEET_AI_NEUTRAL ||
               i == PLAYER_ID_NEUTRAL);

        MobPSet_Create(&fleet->ais[i].mobs);
        MobPSet_Create(&fleet->ais[i].sensors);

        fleet->ais[i].bp = fleet->bsc.bp;
        fleet->ais[i].seed = RandomState_Uint64(&fleet->rs);

        FleetGetOps(fleet->ais[i].player.aiType, &fleet->ais[i].ops);
        if (fleet->ais[i].ops.createFleet != NULL) {
            fleet->ais[i].aiHandle = fleet->ais[i].ops.createFleet(&fleet->ais[i]);
        } else {
            fleet->ais[i].aiHandle = &fleet->ais[i];
        }
    }

    MobVector_CreateEmpty(&fleet->aiMobs);
    MobVector_CreateEmpty(&fleet->aiSensors);

    fleet->initialized = TRUE;
    return fleet;
}

void Fleet_Destroy(Fleet *fleet)
{
    ASSERT(fleet != NULL);
    ASSERT(fleet->initialized);

    for (uint32 i = 0; i < fleet->numAIs; i++) {
        FleetAI *ai = &fleet->ais[i];

        if (ai->ops.mobDestroyed != NULL) {
            CMobIt mit;
            CMobIt_Start(&ai->mobs, &mit);
            while (CMobIt_HasNext(&mit)) {
                Mob *m = CMobIt_Next(&mit);
                ai->ops.mobDestroyed(ai->aiHandle, m, m->aiMobHandle);
            }
        }

        if (ai->ops.destroyFleet != NULL) {
            ai->ops.destroyFleet(fleet->ais[i].aiHandle);
        }

        MobPSet_Destroy(&ai->mobs);
        MobPSet_Destroy(&ai->sensors);

        if (ai->player.mreg != NULL) {
            MBRegistry_Free(ai->player.mreg);
            ai->player.mreg = NULL;
        }
    }

    MobVector_Destroy(&fleet->aiMobs);
    MobVector_Destroy(&fleet->aiSensors);
    free(fleet->ais);
    fleet->initialized = FALSE;
    free(fleet);
}

const char *Fleet_GetName(FleetAIType aiType)
{
    FleetAIOps ops;
    FleetGetOps(aiType, &ops);
    return ops.aiName;
}


static void FleetGetOps(FleetAIType aiType, FleetAIOps *ops)
{
    MBUtil_Zero(ops, sizeof(*ops));

    switch(aiType) {
        case FLEET_AI_NEUTRAL:
            DummyFleet_GetOps(ops);
            ops->aiName = "Neutral";
            break;
        case FLEET_AI_DUMMY:
            DummyFleet_GetOps(ops);
            break;
        case FLEET_AI_SIMPLE:
            SimpleFleet_GetOps(ops);
            break;
        case FLEET_AI_BOB:
            BobFleet_GetOps(ops);
            break;
        case FLEET_AI_MAPPER:
            MapperFleet_GetOps(ops);
            break;
        case FLEET_AI_CLOUD:
            CloudFleet_GetOps(ops);
            break;
        case FLEET_AI_GATHER:
            GatherFleet_GetOps(ops);
            break;
        case FLEET_AI_COWARD:
            CowardFleet_GetOps(ops);
            break;
        case FLEET_AI_RUNAWAY:
            RunAwayFleet_GetOps(ops);
            break;
        case FLEET_AI_BASIC:
            BasicFleet_GetOps(ops);
            break;
        case FLEET_AI_HOLD:
            HoldFleet_GetOps(ops);
            break;
        case FLEET_AI_CIRCLE:
            CircleFleet_GetOps(ops);
            break;
        default:
            PANIC("Unknown AI type=%d\n", aiType);
    }
}


void Fleet_RunTick(Fleet *fleet, const BattleStatus *bs,
                   Mob *mobs, uint32 numMobs)
{
    const BattleParams *bp = &fleet->bsc.bp;

    /*
     * Make sure the vectors are big enough that we don't
     * resize while filling them up.
     */
    MobVector_MakeEmpty(&fleet->aiMobs);
    MobVector_MakeEmpty(&fleet->aiSensors);
    MobVector_EnsureCapacity(&fleet->aiMobs, numMobs);
    MobVector_EnsureCapacity(&fleet->aiSensors, numMobs * fleet->numAIs);
    MobVector_Pin(&fleet->aiMobs);
    MobVector_Pin(&fleet->aiSensors);

    for (uint32 i = 0; i < fleet->numAIs; i++) {
        MobPSet_MakeEmpty(&fleet->ais[i].mobs);
        MobPSet_MakeEmpty(&fleet->ais[i].sensors);
        fleet->ais[i].credits = bs->players[i].credits;
    }

    /*
     * Sort the incoming ships by player.
     */
    for (uint32 i = 0; i < numMobs; i++) {
        Mob *mob = &mobs[i];
        Mob *m;
        PlayerID p = mob->playerID;

        ASSERT(Mob_CheckInvariants(mob));

        MobVector_Grow(&fleet->aiMobs);
        m = MobVector_GetLastPtr(&fleet->aiMobs);
        *m = *mob;
        Mob_MaskForAI(m);

        ASSERT(p == PLAYER_ID_NEUTRAL || p < fleet->numAIs);
        if (p != PLAYER_ID_NEUTRAL) {
            MobPSet_Add(&fleet->ais[p].mobs, m);
        }

        if (mob->scannedBy != 0) {
            for (PlayerID s = 0; s < fleet->numAIs; s++) {
                if (BitVector_GetRaw32(s, mob->scannedBy)) {
                    MobVector_Grow(&fleet->aiSensors);
                    m = MobVector_GetLastPtr(&fleet->aiSensors);
                    *m = *mob;
                    Mob_MaskForSensor(m);
                    ASSERT(Mob_CheckInvariants(m));
                    MobPSet_Add(&fleet->ais[s].sensors, m);
                }
            }
        }
    }

    /*
     * Run the AI for all the players.
     */
    for (uint32 p = 0; p < fleet->numAIs; p++) {
        fleet->ais[p].tick = bs->tick;
        FleetRunAITick(bs, &fleet->ais[p]);
    }

    /*
     * Write the commands back to the original mob array.
     */
    for (uint32 i = 0; i < numMobs; i++) {
        Mob *mob = &mobs[i];
        Mob *m = MobVector_GetPtr(&fleet->aiMobs, i);


        if (mob->mobid != m->mobid) {
            PANIC("Fleet mob list corruption!\n");
        }

        FPoint_Clamp(&m->cmd.target, 0.0f, bp->width, 0.0f, bp->height);
        mob->cmd = m->cmd;
        mob->aiMobHandle = m->aiMobHandle;
    }

    MobVector_Unpin(&fleet->aiMobs);
    MobVector_Unpin(&fleet->aiSensors);
}

static void FleetRunAITick(const BattleStatus *bs, FleetAI *ai)
{
    CMobIt mit;

    if (ai->ops.mobSpawned != NULL) {
        CMobIt_Start(&ai->mobs, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);
            ASSERT(Mob_CheckInvariants(m));
            if (m->birthTick == bs->tick) {
                if (ai->ops.mobSpawned != NULL) {
                    m->aiMobHandle = ai->ops.mobSpawned(ai->aiHandle, m);
                } else {
                    m->aiMobHandle = NULL;
                }
            }
        }
    }

    if (ai->ops.runAITick != NULL) {
        ai->ops.runAITick(ai->aiHandle);
    }

    if (ai->ops.mobDestroyed != NULL) {
        CMobIt_Start(&ai->mobs, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);
            if (!m->alive) {
                ai->ops.mobDestroyed(ai->aiHandle, m, m->aiMobHandle);
                CMobIt_Remove(&mit);
            }
        }
    }
}
