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
} FleetGlobalData;

static FleetGlobalData fleet;

static void FleetGetOps(FleetAI *ai);
static void FleetRunAITick(FleetAI *ai);
static void DummyFleetRunAITick(void *aiHandle);

void Fleet_Init()
{
    const BattleParams *bp = Battle_GetParams();
    MBUtil_Zero(&fleet, sizeof(fleet));

    fleet.numAIs = bp->numPlayers;
    fleet.ais = malloc(fleet.numAIs * sizeof(fleet.ais[0]));

    // We need at least neutral and two fleets.
    ASSERT(fleet.numAIs >= 3);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MBUtil_Zero(&fleet.ais[i], sizeof(fleet.ais[i]));
        fleet.ais[i].id = i;
        fleet.ais[i].player = bp->players[i];
        ASSERT(bp->players[i].aiType < FLEET_AI_MAX);
        ASSERT(bp->players[i].aiType != FLEET_AI_INVALID);
        ASSERT(bp->players[i].aiType == FLEET_AI_NEUTRAL ||
               i != PLAYER_ID_NEUTRAL);
        ASSERT(bp->players[i].aiType != FLEET_AI_NEUTRAL ||
               i == PLAYER_ID_NEUTRAL);

        IntMap_Create(&fleet.ais[i].mobMap);
        IntMap_SetEmptyValue(&fleet.ais[i].mobMap, -1);

        MobVector_CreateEmpty(&fleet.ais[i].mobs);
        MobVector_CreateEmpty(&fleet.ais[i].sensors);

        FleetGetOps(&fleet.ais[i]);
        if (fleet.ais[i].ops.createFleet != NULL) {
            fleet.ais[i].aiHandle = fleet.ais[i].ops.createFleet(&fleet.ais[i]);
        } else {
            fleet.ais[i].aiHandle = &fleet.ais[i];
        }
    }

    fleet.initialized = TRUE;
}

void Fleet_Exit()
{
    ASSERT(fleet.initialized);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        if (fleet.ais[i].ops.destroyFleet != NULL) {
            fleet.ais[i].ops.destroyFleet(fleet.ais[i].aiHandle);
        }

        IntMap_Destroy(&fleet.ais[i].mobMap);
        MobVector_Destroy(&fleet.ais[i].mobs);
        MobVector_Destroy(&fleet.ais[i].sensors);
    }

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
    IntMap mobidMap;
    const BattleParams *bp = Battle_GetParams();

    IntMap_Create(&mobidMap);
    IntMap_SetEmptyValue(&mobidMap, MOB_ID_INVALID);

    for (uint32 i = 0; i < fleet.numAIs; i++) {
        MobVector_MakeEmpty(&fleet.ais[i].mobs);
        MobVector_MakeEmpty(&fleet.ais[i].sensors);
        fleet.ais[i].credits = bs->players[i].credits;
    }

    /*
     * Sort the incoming ships by player.
     */
    for (uint32 i = 0; i < numMobs; i++) {
        Mob *mob = &mobs[i];
        PlayerID p = mob->playerID;

        IntMap_Put(&mobidMap, mob->mobid, i);

        ASSERT(p == PLAYER_ID_NEUTRAL || p < fleet.numAIs);
        if (p != PLAYER_ID_NEUTRAL) {
            Mob *m;
            uint32 oldSize = MobVector_Size(&fleet.ais[p].mobs);
            MobVector_GrowBy(&fleet.ais[p].mobs, 1);
            m = MobVector_GetPtr(&fleet.ais[p].mobs, oldSize);
            *m = *mob;
            Mob_MaskForAI(m);
        }

        if (mob->scannedBy != 0) {
            for (PlayerID s = 0; s < fleet.numAIs; s++) {
                if (BitVector_GetRaw(s, &mob->scannedBy)) {
                    Mob *sm;
                    MobVector_Grow(&fleet.ais[s].sensors);
                    sm = MobVector_GetLastPtr(&fleet.ais[s].sensors);
                    *sm = *mob;
                    Mob_MaskForSensor(sm);
                }
            }
        }
    }

    /*
     * Run the AI for all the players.
     */
    for (uint32 p = 0; p < fleet.numAIs; p++) {
        fleet.ais[p].tick = bs->tick;
        FleetRunAITick(&fleet.ais[p]);
    }

    /*
     * Write the commands back to the original mob array.
     */
    for (uint32 p = 0; p < fleet.numAIs; p++) {
        for (uint32 m = 0; m < MobVector_Size(&fleet.ais[p].mobs); m++) {
            uint32 i;
            Mob *mob = MobVector_GetPtr(&fleet.ais[p].mobs, m);
            ASSERT(mob->playerID == p);

            i = IntMap_Get(&mobidMap, mob->mobid);
            ASSERT(i != MOB_ID_INVALID);
            ASSERT(mobs[i].mobid == mob->mobid);

            FPoint_Clamp(&mob->cmd.target, 0.0f, bp->width, 0.0f, bp->height);
            mobs[i].cmd = mob->cmd;
            mobs[i].aiMobHandle = mob->aiMobHandle;
        }
    }

    IntMap_Destroy(&mobidMap);
}

int FleetUtil_FindClosestSensor(FleetAI *ai, const FPoint *pos, uint scanFilter)
{
    return FleetUtil_FindClosestMob(&ai->sensors, pos, scanFilter);
}

int FleetUtil_FindClosestMob(MobVector *mobs, const FPoint *pos, uint scanFilter)
{
    float distance;
    int index = -1;

    for (uint i = 0; i < MobVector_Size(mobs); i++) {
        Mob *m = MobVector_GetPtr(mobs, i);
        if (!m->alive) {
            continue;
        }
        if (((1 << m->type) & scanFilter) != 0) {
            float curDistance = FPoint_Distance(pos, &m->pos);
            if (index == -1 || curDistance < distance) {
                distance = curDistance;
                index = i;
            }
        }
    }

    return index;
}

void FleetUtil_SortMobsByDistance(MobVector *mobs, const FPoint *pos)
{
    if (MobVector_Size(mobs) <= 1) {
        return;
    }

    for (int i = 1; i < MobVector_Size(mobs); i++) {
        Mob *iMob = MobVector_GetPtr(mobs, i);
        float iDistance = FPoint_Distance(&iMob->pos, pos);

        for (int n = i - 1; n >= 0; n--) {
            Mob *nMob = MobVector_GetPtr(mobs, n);
            float nDistance = FPoint_Distance(&nMob->pos, pos);

            if (nDistance > iDistance) {
                Mob tMob = *iMob;
                *iMob = *nMob;
                *nMob = tMob;

                iMob = nMob;
                iDistance = nDistance;
            } else {
                break;
            }
        }
    }
}

void FleetUtil_RandomPointInRange(FPoint *p, const FPoint *center, float radius)
{
    ASSERT(p != NULL);
    ASSERT(center != NULL);

    p->x = Random_Float(MAX(0, center->x - radius), center->x + radius);
    p->y = Random_Float(MAX(0, center->y - radius), center->y + radius);
}

Mob *FleetUtil_GetMob(FleetAI *ai, MobID mobid)
{
    Mob *m;
    ASSERT(ai != NULL);

    int i = IntMap_Get(&ai->mobMap, mobid);
    if (i == -1) {
        return NULL;
    }

    m = MobVector_GetPtr(&ai->mobs, i);
    if (m->mobid == mobid) {
        return m;
    }

    /*
     * XXX: If the fleet sorted their mob vector, then the mobMap is
     * all mis-aligned... There isn't an obviously better way to handle
     * this than using pointers, or some other abstraction?
     */
    for (i = 0; i < MobVector_Size(&ai->mobs); i++) {
        m = MobVector_GetPtr(&ai->mobs, i);
        if (m->mobid == mobid) {
            return m;
        }
    }

    return NULL;
}

static void FleetRunAITick(FleetAI *ai)
{
    for (uint32 i = 0; i < MobVector_Size(&ai->mobs); i++) {
        Mob *m = MobVector_GetPtr(&ai->mobs, i);
        if (ai->ops.mobSpawned != NULL) {
            if (!IntMap_ContainsKey(&ai->mobMap, m->mobid)) {
                m->aiMobHandle = ai->ops.mobSpawned(ai->aiHandle, m);
            }
        } else {
            m->aiMobHandle = m;
        }
        IntMap_Put(&ai->mobMap, m->mobid, i);
        ASSERT(IntMap_ContainsKey(&ai->mobMap, m->mobid));
    }

    ASSERT(ai->ops.runAITick != NULL);
    ai->ops.runAITick(ai->aiHandle);

    if (ai->ops.runAIMob != NULL) {
        for (uint32 i = 0; i < MobVector_Size(&ai->mobs); i++) {
            Mob *m = MobVector_GetPtr(&ai->mobs, i);
            ai->ops.runAIMob(ai->aiHandle, m->aiMobHandle);
        }
    }

    for (uint32 i = 0; i < MobVector_Size(&ai->mobs); i++) {
        Mob *m = MobVector_GetPtr(&ai->mobs, i);

        if (!m->alive) {
            if (ai->ops.mobDestroyed != NULL) {
                ai->ops.mobDestroyed(ai->aiHandle, m->aiMobHandle);
            }

            IntMap_Remove(&ai->mobMap, m->mobid);
            ASSERT(!IntMap_ContainsKey(&ai->mobMap, m->mobid));

            m->aiMobHandle = NULL;
        }
    }
}

static void DummyFleetRunAITick(void *handle)
{
    FleetAI *ai = handle;
    const BattleParams *bp = Battle_GetParams();

    /*
     * We use this function for the neutral player, but don't actually queue any
     * mobs for them to process.
     */
    ASSERT(ai->player.aiType == FLEET_AI_DUMMY ||
           ai->player.aiType == FLEET_AI_NEUTRAL);

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
