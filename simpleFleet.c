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
    FPoint basePos;
    SensorMob enemyBase;
    uint enemyBaseAge;
} SimpleFleetData;

static void SimpleFleetCreate(FleetAI *ai);
static void SimpleFleetDestroy(FleetAI *ai);
static void SimpleFleetRunAI(FleetAI *ai);

void SimpleFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "SimpleFleet";
    ops->aiAuthor = "Michael Banack";

    ops->create = &SimpleFleetCreate;
    ops->destroy = &SimpleFleetDestroy;
    ops->runAI = &SimpleFleetRunAI;
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
    uint targetScanFilter = FLEET_SCAN_SHIP;
    IntMap targetMap;

    IntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_SIMPLE);

    // If we've found the enemy base, assume it's still there.
    int enemyBaseIndex = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                     FLEET_SCAN_BASE);
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

    int targetIndex = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                  targetScanFilter);

    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            int t = targetIndex;

            if (t == -1) {
                /*
                 * Avoid having all the fighters rush to the same loot box.
                 */
                t = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                FLEET_SCAN_LOOT_BOX);
                if (IntMap_Increment(&targetMap, t) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    t = -1;
                }
            }

            if (t != -1) {
                SensorMob *sm;
                sm = SensorMobVector_GetPtr(&ai->sensors, t);
                mob->cmd.target = sm->pos;

                if (sm->type != MOB_TYPE_LOOT_BOX &&
                    Random_Int(0, 20) == 0) {
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
            int s = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
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

    IntMap_Destroy(&targetMap);
}
