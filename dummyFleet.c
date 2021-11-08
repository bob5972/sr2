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

typedef struct DummyFleetData {
    FleetAI *ai;
    RandomState rs;
} DummyFleetData;

static void DummyFleetRunAITick(void *aiHandle);
static void *DummyFleetCreate(FleetAI *ai);
static void DummyFleetDestroy(void *aiHandle);

void DummyFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ops->aiName = "DummyFleet";
    ops->aiAuthor = "Michael Banack";
    ops->createFleet = &DummyFleetCreate;
    ops->destroyFleet = &DummyFleetDestroy;
    ops->runAITick = &DummyFleetRunAITick;
}

static void *DummyFleetCreate(FleetAI *ai)
{
    DummyFleetData *sf;
    ASSERT(ai != NULL);

    sf = malloc(sizeof(*sf));
    MBUtil_Zero(sf, sizeof(*sf));
    sf->ai = ai;
    RandomState_CreateWithSeed(&sf->rs, ai->seed);

    return sf;
}

static void DummyFleetDestroy(void *aiHandle)
{
    DummyFleetData *sf = aiHandle;
    ASSERT(sf != NULL);
    RandomState_Destroy(&sf->rs);
    free(sf);
}

static void DummyFleetRunAITick(void *handle)
{
    DummyFleetData *sf = handle;
    FleetAI *ai = sf->ai;
    CMobIt mit;
    const BattleParams *bp = &ai->bp;

    ASSERT(ai->player.aiType == FLEET_AI_DUMMY ||
           ai->player.aiType == FLEET_AI_NEUTRAL);

    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);
        bool newTarget = FALSE;

        if (mob->type == MOB_TYPE_BASE) {
            if (RandomState_Int(&sf->rs, 0, 100) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            }
        }

        if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            newTarget = TRUE;
        }
        if (mob->type != MOB_TYPE_BASE &&
            RandomState_Int(&sf->rs, 0, 100) == 0) {
            newTarget = TRUE;
        }
        if (mob->birthTick == ai->tick) {
            newTarget = TRUE;
        }

        if (newTarget) {
            if (RandomState_Bit(&sf->rs)) {
                mob->cmd.target.x = RandomState_Float(&sf->rs, 0.0f,
                                                      bp->width);
                mob->cmd.target.y = RandomState_Float(&sf->rs, 0.0f,
                                                      bp->height);
            }
        }
    }
}
