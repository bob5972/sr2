/*
 * simpleFleet.c -- part of SpaceRobots2
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

typedef struct SimpleFleetData {
    FleetAI *ai;
    RandomState rs;
    FPoint basePos;
    Mob enemyBase;
    uint enemyBaseAge;
} SimpleFleetData;

static void *SimpleFleetCreate(FleetAI *ai);
static void SimpleFleetDestroy(void *aiHandle);
static void SimpleFleetRunAI(void *aiHandle);

void SimpleFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "SimpleFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &SimpleFleetCreate;
    ops->destroyFleet = &SimpleFleetDestroy;
    ops->runAITick = &SimpleFleetRunAI;
}

static void *SimpleFleetCreate(FleetAI *ai)
{
    SimpleFleetData *sf;
    ASSERT(ai != NULL);

    sf = malloc(sizeof(*sf));
    MBUtil_Zero(sf, sizeof(*sf));
    sf->ai = ai;
    RandomState_CreateWithSeed(&sf->rs, ai->seed);

    return sf;
}

static void SimpleFleetDestroy(void *handle)
{
    SimpleFleetData *sf = handle;
    ASSERT(sf != NULL);
    RandomState_Destroy(&sf->rs);
    free(sf);
}

static void SimpleFleetRunAI(void *handle)
{
    SimpleFleetData *sf = handle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = &ai->bp;
    uint targetScanFilter = MOB_FLAG_SHIP;
    CIntMap targetMap;

    CIntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_SIMPLE);

    // If we've found the enemy base, assume it's still there.
    Mob *enemyBase = FleetUtil_FindClosestSensor(ai, &sf->basePos, MOB_FLAG_BASE);

    if (enemyBase != NULL) {
        ASSERT(enemyBase->type == MOB_TYPE_BASE);
        sf->enemyBase = *enemyBase;
        sf->enemyBaseAge = 0;
    } else if (sf->enemyBase.type == MOB_TYPE_BASE &&
               sf->enemyBaseAge < 200) {
        MobPSet_Add(&ai->sensors, &sf->enemyBase);
        sf->enemyBaseAge++;
    }

    Mob *target = FleetUtil_FindClosestSensor(ai, &sf->basePos, targetScanFilter);

    CMobIt mit;
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);
        if (mob->type == MOB_TYPE_FIGHTER) {
            if (target == NULL) {
                /*
                 * Avoid having all the fighters rush to the same power core.
                 */
                target = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                     MOB_FLAG_POWER_CORE);
                if (target != NULL) {
                    if (CIntMap_Increment(&targetMap, target->mobid) > 1) {
                        /*
                        * Ideally we find the next best target, but for now just
                        * go back to random movement.
                        */
                        target = NULL;
                    }
                }
            }

            if (target != NULL) {
                mob->cmd.target = target->pos;

                if (target->type != MOB_TYPE_POWER_CORE &&
                    RandomState_Int(&sf->rs, 0, 20) == 0) {
                    mob->cmd.spawnType = MOB_TYPE_MISSILE;
                }
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (RandomState_Bit(&sf->rs)) {
                    mob->cmd.target.x =
                        RandomState_Float(&sf->rs, 0.0f, bp->width);
                    mob->cmd.target.y =
                        RandomState_Float(&sf->rs, 0.0f, bp->height);
                } else {
                    mob->cmd.target = sf->basePos;
                }
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            Mob *target = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (target != NULL) {
                mob->cmd.target = target->pos;
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 &&
                RandomState_Int(&sf->rs, 0, 100) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            ASSERT(MobType_GetSpeed(MOB_TYPE_BASE) == 0.0f);
        } else if (mob->type == MOB_TYPE_POWER_CORE) {
            mob->cmd.target = sf->basePos;
        }
    }

    CIntMap_Destroy(&targetMap);
}
