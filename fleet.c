/*
 * fleet.c -- part of SpaceRobots2
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

#include "fleet.h"
#include "Random.h"
#include "MBVarMap.h"
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

static const FleetAIType gRankings[] = {
                        // Full, Strong
    FLEET_AI_SIMPLE,    //  0.0%
    FLEET_AI_GATHER,    //  2.0%
    FLEET_AI_CLOUD,     //  2.0%
    FLEET_AI_MAPPER,    //  2.2%
    FLEET_AI_RUNAWAY,   //  7.7%
    FLEET_AI_COWARD,    // 10.1%
    FLEET_AI_BUNDLE1,   // 17.4%
    FLEET_AI_BUNDLE8,   // 20.5%
    FLEET_AI_BUNDLE7,   // 21.0%
    FLEET_AI_BUNDLE4,   // 21.1%
    FLEET_AI_CIRCLE,    // 23.5%
    FLEET_AI_FLOCK1,    // 23.5%
    FLEET_AI_BASIC,     // 26.9%
    FLEET_AI_BUNDLE6,   // 27.3%
    FLEET_AI_BUNDLE5,   // 30.9%
    FLEET_AI_NEURAL4,   // 31.9%
    FLEET_AI_BUNDLE2,   // 32.6%
    FLEET_AI_NEURAL2,   // 35.4%
    FLEET_AI_NEURAL3,   // 36.2%
    FLEET_AI_FLOCK2,    // 37.0%
    FLEET_AI_BUNDLE9,   // 40.0%
    FLEET_AI_BUNDLE3,   // 43.5%
    FLEET_AI_HOLD,      // 45.4%
    FLEET_AI_NEURAL1,   // 45.8%
    FLEET_AI_BUNDLE10,  // 49.6%
    FLEET_AI_META,      // 53.3%
    FLEET_AI_BUNDLE11,  // 56.3%
    FLEET_AI_BUNDLE13,  // 58.1%
    FLEET_AI_NEURAL6,   // 58.7%
    FLEET_AI_NEURAL5,   // 58.8%, 24.3%
    FLEET_AI_BUNDLE14,  // 58.9%, 24.3%
    FLEET_AI_FLOCK4,    // 62.2%, 23.9%
    FLEET_AI_NEURAL8,   // 65.7%, 27.2%
    FLEET_AI_NEURAL10,  // 67.2%, 32.6%
    FLEET_AI_NEURAL9,   // 69.4%, 33.6%
    FLEET_AI_FLOCK5,    // 68.9%, 33.8%
    FLEET_AI_NEURAL7,   // 70.3%, 34.5%
    FLEET_AI_BUNDLE12,  // 73.1%, 35.1%
    FLEET_AI_FLOCK3,    // 69.1%, 37.4%
    FLEET_AI_FLOCK6,    // 74.2%, 41.7%
    FLEET_AI_NEURAL12,  // 76.2%, 43.2%
    FLEET_AI_FLOCK7,    // 76.9%, 46.4%
    FLEET_AI_NEURAL11,  // 78.1%, 49.4%
    FLEET_AI_BUNDLE16,  // 79.9%, 51.1%
    FLEET_AI_BINEURAL2, // 82.7%, 55.0%
    FLEET_AI_BINEURAL1, // 80.1%, 56.0%
    FLEET_AI_BUNDLE15,  // 80.7%, 57.2%
    FLEET_AI_FLOCK8,    // 81.4%, 57.4%
    FLEET_AI_BINEURAL3, // 83.0%, 61.9%
    FLEET_AI_BINEURAL4, // 83.2%, 63.2%
    FLEET_AI_NEURAL13,  // 83.4%, 64.1%
    FLEET_AI_NEURAL14,  // 89.4%, 76.7%
    FLEET_AI_FLOCK9,    // 90.8%, 77.8%
};

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
        uint64 seed = RandomState_Uint64(&fleet->rs);
        ASSERT(MBUtil_IsZero(&fleet->ais[i], sizeof(fleet->ais[i])));

        ASSERT(bsc->players[i].aiType < FLEET_AI_MAX);
        ASSERT(bsc->players[i].aiType != FLEET_AI_INVALID);
        ASSERT(bsc->players[i].aiType == FLEET_AI_NEUTRAL ||
               i != PLAYER_ID_NEUTRAL);
        ASSERT(bsc->players[i].aiType != FLEET_AI_NEUTRAL ||
               i == PLAYER_ID_NEUTRAL);

        Fleet_CreateAI(&fleet->ais[i], bsc->players[i].aiType,
                       i, &fleet->bsc.bp, &bsc->players[i], seed);
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

    for (uint i = 0; i < fleet->numAIs; i++) {
        Fleet_DestroyAI(&fleet->ais[i]);
    }

    MobVector_Destroy(&fleet->aiMobs);
    MobVector_Destroy(&fleet->aiSensors);
    free(fleet->ais);
    fleet->initialized = FALSE;
    free(fleet);
}

void Fleet_CreateAI(FleetAI *ai, FleetAIType aiType,
                    PlayerID id, const BattleParams *bp,
                    const BattlePlayer *player,
                    uint64 seed)
{
    Fleet_GetOps(aiType, &ai->ops);

    ai->id = id;
    ai->bp = *bp;
    ai->player = *player;
    ai->player.aiType = aiType;

    if (ai->player.mreg != NULL) {
        ai->player.mreg =
            MBRegistry_AllocCopy(ai->player.mreg);
    }

    ai->seed = seed;
    ai->tick = 0;
    ai->credits = 0;

    MobPSet_Create(&ai->mobs);
    MobPSet_Create(&ai->sensors);

    if (ai->ops.createFleet != NULL) {
        ai->aiHandle = ai->ops.createFleet(ai);
    } else {
        ai->aiHandle = ai;
    }
}

void Fleet_DestroyAI(FleetAI *ai)
{
    if (ai->ops.mobDestroyed != NULL) {
        CMobIt mit;
        CMobIt_Start(&ai->mobs, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);
            ai->ops.mobDestroyed(ai->aiHandle, m, m->aiMobHandle);
        }
    }

    if (ai->ops.destroyFleet != NULL) {
        ai->ops.destroyFleet(ai->aiHandle);
    }

    MobPSet_Destroy(&ai->mobs);
    MobPSet_Destroy(&ai->sensors);

    if (ai->player.mreg != NULL) {
        MBRegistry_Free(ai->player.mreg);
        ai->player.mreg = NULL;
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

    for (uint i = 0; i < fleet->numAIs; i++) {
        MobPSet_MakeEmpty(&fleet->ais[i].mobs);
        MobPSet_MakeEmpty(&fleet->ais[i].sensors);
        fleet->ais[i].credits = bs->players[i].credits;
    }

    /*
     * Sort the incoming ships by player.
     */
    for (uint i = 0; i < numMobs; i++) {
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


        ASSERT(mob->mobid == m->mobid);

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


FleetAIType Fleet_GetTypeFromName(const char *name)
{
    uint32 i;

    ASSERT(FLEET_AI_NEUTRAL == 1);
    for (i = FLEET_AI_NEUTRAL; i < FLEET_AI_MAX; i++) {
        const char *fleetName = Fleet_GetName((FleetAIType)i);
        if (fleetName != NULL && strcmp(fleetName, name) == 0) {
            return (FleetAIType)i;
        }
    }

    return FLEET_AI_INVALID;
}


bool Fleet_IsFlockFleet(FleetAIType aiType)
{
    if (aiType >= FLEET_AI_FLOCK1 && aiType <= FLEET_AI_FLOCK9) {
        return TRUE;
    }
    return FALSE;
}

bool Fleet_IsBundleFleet(FleetAIType aiType)
{
    if (aiType >= FLEET_AI_BUNDLE1 && aiType <= FLEET_AI_BUNDLE16) {
        return TRUE;
    }
    return FALSE;
}

bool Fleet_IsNeuralFleet(FleetAIType aiType)
{
    if (aiType >= FLEET_AI_NEURAL1 && aiType <= FLEET_AI_NEURAL14) {
        return TRUE;
    }
    return FALSE;
}

bool Fleet_IsBineuralFleet(FleetAIType aiType)
{
    if (aiType >= FLEET_AI_BINEURAL1 && aiType <= FLEET_AI_BINEURAL5) {
        return TRUE;
    }
    return FALSE;
}

void Fleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    struct {
        FleetAIType aiType;
        void (*getOps)(FleetAIType aiType, FleetAIOps *ops);
    } fleets[] = {
        { FLEET_AI_NEUTRAL,     NeutralFleet_GetOps     },
        { FLEET_AI_DUMMY,       DummyFleet_GetOps       },
        { FLEET_AI_SIMPLE,      SimpleFleet_GetOps      },
        { FLEET_AI_META,        MetaFleet_GetOps        },
        { FLEET_AI_MAPPER,      MapperFleet_GetOps      },
        { FLEET_AI_CLOUD,       CloudFleet_GetOps       },
        { FLEET_AI_GATHER,      GatherFleet_GetOps      },
        { FLEET_AI_COWARD,      CowardFleet_GetOps      },
        { FLEET_AI_RUNAWAY,     RunAwayFleet_GetOps     },
        { FLEET_AI_BASIC,       BasicFleet_GetOps       },
        { FLEET_AI_HOLD,        HoldFleet_GetOps        },
        { FLEET_AI_CIRCLE,      CircleFleet_GetOps      },
        { FLEET_AI_MATRIX1,     MatrixFleet_GetOps      },
    };

    ASSERT(aiType != FLEET_AI_INVALID);
    ASSERT(aiType < FLEET_AI_MAX);
    MBUtil_Zero(ops, sizeof(*ops));

    if (Fleet_IsFlockFleet(aiType)) {
        FlockFleet_GetOps(aiType, ops);
        ops->aiType = aiType;
        return;
    } else if (Fleet_IsBundleFleet(aiType)) {
        BundleFleet_GetOps(aiType, ops);
        ops->aiType = aiType;
        return;
    } else if (Fleet_IsNeuralFleet(aiType)) {
        NeuralFleet_GetOps(aiType, ops);
        ops->aiType = aiType;
        return;
    } else if (Fleet_IsBineuralFleet(aiType)) {
        BineuralFleet_GetOps(aiType, ops);
        ops->aiType = aiType;
        return;
    }

    for (uint i = 0; i < ARRAYSIZE(fleets); i++ ) {
        if (fleets[i].aiType == aiType) {
            fleets[i].getOps(aiType, ops);
            ops->aiType = aiType;
            return;
        }
    }

    PANIC("Unknown AI type=%d\n", aiType);
}

/*
 * Get the approximate ranking of the fleet.
 * (ie increasing rank means the fleet wins more often)
 */
int Fleet_GetRanking(FleetAIType aiType) {
    ASSERT(FLEET_AI_INVALID == 0);
    ASSERT(FLEET_AI_NEUTRAL == 1);
    ASSERT(FLEET_AI_DUMMY == 2);
    ASSERT(ARRAYSIZE(gRankings) + 3 == FLEET_AI_MAX);

    for (int i = 0; i < ARRAYSIZE(gRankings); i++) {
        if (aiType == gRankings[i]) {
            return i;
        }
    }

    return ARRAYSIZE(gRankings);
}

FleetAIType Fleet_GetTypeFromRanking(int rank)
{
    if (rank < 0 || rank >= ARRAYSIZE(gRankings)) {
        return FLEET_AI_INVALID;
    }

    return gRankings[rank];
}
