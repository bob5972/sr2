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
static void FleetRunAI(FleetAI *ai);
static void DummyFleetRunAI(FleetAI *ai);

void Fleet_Init()
{
    const BattleParams *bp = Battle_GetParams();
    ASSERT(MBUtil_IsZero(&fleet, sizeof(fleet)));

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
        case FLEET_AI_NEUTRAL:
        case FLEET_AI_DUMMY:
            ai->ops.runAI = &DummyFleetRunAI;
            break;
        case FLEET_AI_SIMPLE:
            SimpleFleet_GetOps(&ai->ops);
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

        ASSERT(p == PLAYER_ID_NEUTRAL || p < fleet.numAIs);
        if (mob->alive && p != PLAYER_ID_NEUTRAL) {
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

int FleetUtil_FindClosestSensor(FleetAI *ai, const FPoint *pos, uint scanFilter)
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

static void DummyFleetRunAI(FleetAI *ai)
{
    const BattleParams *bp = Battle_GetParams();

    /*
     * XXX: We use this function for the neutral player, but actually queue any
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
