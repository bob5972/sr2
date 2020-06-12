/*
 * bobFleet.c -- part of SpaceRobots2
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

typedef struct BobShipData {
    MobID mobid;
} BobShipData;

DECLARE_MBVECTOR_TYPE(BobShipData, ShipVector);

typedef struct BobFleetData {
    FPoint basePos;
    Mob enemyBase;
    uint enemyBaseAge;

    ShipVector ships;
    IntMap shipMap;
} BobFleetData;

static void BobFleetCreate(FleetAI *ai);
static void BobFleetDestroy(FleetAI *ai);
static void BobFleetRunAI(FleetAI *ai);
static BobShipData *BobFleetGetShip(BobFleetData *sf, MobID mobid);
static void BobFleetDestroyShip(BobFleetData *sf, MobID mobid);

void BobFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "BobFleet";
    ops->aiAuthor = "Michael Banack";

    ops->create = &BobFleetCreate;
    ops->destroy = &BobFleetDestroy;
    ops->runAI = &BobFleetRunAI;
}

static void BobFleetCreate(FleetAI *ai)
{
    BobFleetData *sf;
    ASSERT(ai != NULL);

    sf = malloc(sizeof(*sf));
    MBUtil_Zero(sf, sizeof(*sf));
    ai->aiHandle = sf;

    ShipVector_CreateEmpty(&sf->ships);
    IntMap_Create(&sf->shipMap);
    IntMap_SetEmptyValue(&sf->shipMap, MOB_ID_INVALID);
}

static void BobFleetDestroy(FleetAI *ai)
{
    BobFleetData *sf;
    ASSERT(ai != NULL);

    sf = ai->aiHandle;
    ASSERT(sf != NULL);

    IntMap_Destroy(&sf->shipMap);
    ShipVector_Destroy(&sf->ships);

    free(sf);
    ai->aiHandle = NULL;
}

static BobShipData *BobFleetGetShip(BobFleetData *sf, MobID mobid)
{
    int i = IntMap_Get(&sf->shipMap, mobid);

    if (i != MOB_ID_INVALID) {
        return ShipVector_GetPtr(&sf->ships, i);
    } else {
        BobShipData *s;
        ShipVector_Grow(&sf->ships);
        s = ShipVector_GetLastPtr(&sf->ships);

        s->mobid = mobid;

        i = ShipVector_Size(&sf->ships) - 1;
        IntMap_Put(&sf->shipMap, mobid, i);
        ASSERT(IntMap_Get(&sf->shipMap, mobid) == i);
        return s;
    }
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void BobFleetDestroyShip(BobFleetData *sf, MobID mobid)
{
    BobShipData *s;
    int i = IntMap_Get(&sf->shipMap, mobid);

    ASSERT(i != -1);

    IntMap_Remove(&sf->shipMap, mobid);

    if (i != ShipVector_Size(&sf->ships) - 1) {
        ASSERT(i < ShipVector_Size(&sf->ships));
        s = ShipVector_GetLastPtr(&sf->ships);

        ShipVector_PutValue(&sf->ships, i, *s);
        ASSERT(IntMap_Get(&sf->shipMap, s->mobid) ==
               ShipVector_Size(&sf->ships) - 1);

        IntMap_Put(&sf->shipMap, s->mobid, i);
        ASSERT(IntMap_Get(&sf->shipMap, s->mobid) == i);
    }

    ShipVector_Shrink(&sf->ships);
}

static void BobFleetRunAI(FleetAI *ai)
{
    BobFleetData *sf = ai->aiHandle;
    const BattleParams *bp = Battle_GetParams();
    uint targetScanFilter = FLEET_SCAN_SHIP;
    IntMap targetMap;

    IntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_BOB);

    // If we've found the enemy base, assume it's still there.
    int enemyBaseIndex = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                     FLEET_SCAN_BASE);
    if (enemyBaseIndex != -1) {
        Mob *sm = MobVector_GetPtr(&ai->sensors, enemyBaseIndex);
        ASSERT(sm->type == MOB_TYPE_BASE);
        sf->enemyBase = *sm;
        sf->enemyBaseAge = 0;
    } else if (sf->enemyBase.type == MOB_TYPE_BASE &&
               sf->enemyBaseAge < 200) {
        MobVector_Grow(&ai->sensors);
        Mob *sm = MobVector_GetLastPtr(&ai->sensors);
        *sm = sf->enemyBase;
        sf->enemyBaseAge++;
    }

    int targetIndex = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                  targetScanFilter);

    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);
        BobShipData *s = BobFleetGetShip(sf, mob->mobid);
        ASSERT(s != NULL);
        ASSERT(s->mobid == mob->mobid);

        if (!mob->alive) {
            BobFleetDestroyShip(sf, mob->mobid);
        } else if (mob->type == MOB_TYPE_FIGHTER) {
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
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, t);
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
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, s);
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
