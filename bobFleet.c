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

typedef enum BobGovernor {
    BOB_GOV_INVALID = 0,
    BOB_GOV_GUARD   = 1,
    BOB_GOV_MIN     = 1,
    BOB_GOV_SCOUT   = 2,
    BOB_GOV_ATTACK  = 3,
    BOB_GOV_MAX,
} BobGovernor;

typedef struct BobShip {
    MobID mobid;
    BobGovernor gov;
} BobShip;

typedef struct BobFleetData {
    FleetAI *ai;
    FPoint basePos;
    Mob enemyBase;
    uint enemyBaseAge;
} BobFleetData;

static void *BobFleetCreate(FleetAI *ai);
static void BobFleetDestroy(void *aiHandle);
static void BobFleetRunAITick(void *aiHandle);
static void *BobFleetMobSpawned(void *aiHandle, Mob *m);
static void BobFleetMobDestroyed(void *aiHandle, void *aiMobHandle);
static BobShip *BobFleetGetShip(BobFleetData *sf, MobID mobid);

void BobFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "BobFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BobFleetCreate;
    ops->destroyFleet = &BobFleetDestroy;
    ops->runAITick = &BobFleetRunAITick;
    ops->mobSpawned = BobFleetMobSpawned;
    ops->mobDestroyed = BobFleetMobDestroyed;
}

static void *BobFleetCreate(FleetAI *ai)
{
    BobFleetData *sf;
    ASSERT(ai != NULL);

    sf = MBUtil_ZAlloc(sizeof(*sf));
    sf->ai = ai;
    return sf;
}

static void BobFleetDestroy(void *handle)
{
    BobFleetData *sf = handle;
    ASSERT(sf != NULL);
    free(sf);
}

static void *BobFleetMobSpawned(void *aiHandle, Mob *m)
{
    BobFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        BobShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;
        m->cmd.target = sf->basePos;
        ship->gov = Random_Int(BOB_GOV_MIN, BOB_GOV_MAX - 1);
        return ship;
    } else {
        /*
         * We don't track anything else.
         */
        return NULL;
    }
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void BobFleetMobDestroyed(void *aiHandle, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    BobFleetData *sf = aiHandle;
    BobShip *ship = aiMobHandle;

    ASSERT(sf != NULL);
    free(ship);
}

static BobShip *BobFleetGetShip(BobFleetData *sf, MobID mobid)
{
    BobShip *s = FleetUtil_GetMob(sf->ai, mobid)->aiMobHandle;

    if (s != NULL) {
        ASSERT(s->mobid == mobid);
    }

    return s;
}


static void BobFleetRunAITick(void *aiHandle)
{
    BobFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = Battle_GetParams();
    uint targetScanFilter = FLEET_SCAN_SHIP;
    IntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    uint32 numGuard = 0;

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
               sf->enemyBaseAge < 1000) {
        MobVector_Grow(&ai->sensors);
        Mob *sm = MobVector_GetLastPtr(&ai->sensors);
        *sm = sf->enemyBase;
        sf->enemyBaseAge++;
    }

    int groupTargetIndex = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                       targetScanFilter);

    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            BobShip *ship = BobFleetGetShip(sf, mob->mobid);
            int t = -1;

            ASSERT(ship != NULL);
            ASSERT(ship->mobid == mob->mobid);

            if (ship->gov == BOB_GOV_SCOUT) {
                /*
                 * Just run the shared random/loot-box code.
                 */
            } else if (ship->gov == BOB_GOV_ATTACK) {
                t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                targetScanFilter);
            } else if (ship->gov == BOB_GOV_GUARD) {
                Mob *sm;

                numGuard++;
                if (numGuard >= 5) {
                    ship->gov = BOB_GOV_ATTACK;
                }

                t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                targetScanFilter);
                if (t != -1) {
                    sm = MobVector_GetPtr(&ai->sensors, t);
                    if (FPoint_Distance(&sm->pos, &sf->basePos) >
                        MobType_GetSensorRadius(MOB_TYPE_BASE)) {
                        t = -1;
                    }
                }

                t = groupTargetIndex;
                if (t != -1) {
                    sm = MobVector_GetPtr(&ai->sensors, t);
                    if (FPoint_Distance(&sm->pos, &sf->basePos) >
                        MobType_GetSensorRadius(MOB_TYPE_BASE)) {
                        t = -1;
                    }
                }
            }

            if (t == -1) {
                /*
                * Avoid having all the fighters rush to the same loot box.
                */
                t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                FLEET_SCAN_LOOT_BOX);
                if (t != -1 && IntMap_Increment(&targetMap, t) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    t = -1;
                }

                if (ship->gov == BOB_GOV_GUARD && t != -1) {
                    Mob *sm;
                    sm = MobVector_GetPtr(&ai->sensors, t);
                    if (FPoint_Distance(&sm->pos, &sf->basePos) >
                        MobType_GetSensorRadius(MOB_TYPE_BASE)) {
                        t = -1;
                    }
                }
            }

            {
                int ct = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     targetScanFilter);
                if (ct != -1) {
                    Mob *sm;
                    sm = MobVector_GetPtr(&ai->sensors, ct);

                    if (Random_Int(0, 10) == 0 &&
                        FPoint_Distance(&mob->pos, &sm->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
                }
            }

            if (t != -1) {
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, t);
                mob->cmd.target = sm->pos;
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (ship->gov == BOB_GOV_GUARD) {
                    float guardRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
                    mob->cmd.target.x =
                        Random_Float(MAX(0, sf->basePos.x - guardRadius),
                                     sf->basePos.x + guardRadius);
                    mob->cmd.target.y =
                        Random_Float(MAX(0, sf->basePos.y - guardRadius),
                                     sf->basePos.y + guardRadius);
                } else {
                    mob->cmd.target.x = Random_Float(0.0f, bp->width);
                    mob->cmd.target.y = Random_Float(0.0f, bp->height);
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

            ASSERT(MobType_GetSpeed(MOB_TYPE_BASE) == 0.0f);
        } else if (mob->type == MOB_TYPE_LOOT_BOX) {
            Mob *sm;

            mob->cmd.target = sf->basePos;

            /*
             * Add our own loot box to the sensor targets so that we'll
             * steer towards them.
             */
            MobVector_Grow(&ai->sensors);
            sm = MobVector_GetLastPtr(&ai->sensors);
            *sm = *mob;
        }
    }

    IntMap_Destroy(&targetMap);
}
