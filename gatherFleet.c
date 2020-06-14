/*
 * cloudFleet.c -- part of SpaceRobots2
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

typedef enum GatherGovernor {
    GATHER_GOV_INVALID = 0,
    GATHER_GOV_GUARD = 1,
    GATHER_GOV_MIN   = 1,
    GATHER_GOV_SCOUT = 2,
    GATHER_GOV_MAX,
} GatherGovernor;

typedef struct GatherShip {
    MobID mobid;
    MobID targetIndex;
    uint lastFiredTick;
    GatherGovernor gov;
    bool initialized;
} GatherShip;

typedef struct GatherFleetData {
    FleetAI *ai;
    FPoint basePos;
    uint numGuard;
} GatherFleetData;

static void *GatherFleetCreate(FleetAI *ai);
static void GatherFleetDestroy(void *aiHandle);
static void GatherFleetRunAITick(void *aiHandle);
static void *GatherFleetMobSpawned(void *aiHandle, Mob *m);
static void GatherFleetMobDestroyed(void *aiHandle, void *aiMobHandle);
static GatherShip *GatherFleetGetShip(GatherFleetData *sf, MobID mobid);

void GatherFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "GatherFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &GatherFleetCreate;
    ops->destroyFleet = &GatherFleetDestroy;
    ops->runAITick = &GatherFleetRunAITick;
    ops->mobSpawned = &GatherFleetMobSpawned;
    ops->mobDestroyed = &GatherFleetMobDestroyed;
}

static void *GatherFleetCreate(FleetAI *ai)
{
    GatherFleetData *sf;
    ASSERT(ai != NULL);

    sf = MBUtil_ZAlloc(sizeof(*sf));
    sf->ai = ai;

    return sf;
}

static void GatherFleetDestroy(void *aiHandle)
{
    GatherFleetData *sf = aiHandle;
    ASSERT(sf != NULL);
    free(sf);
}

static void *GatherFleetMobSpawned(void *aiHandle, Mob *m)
{
    GatherFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        GatherShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;
        ship->targetIndex = -1;
        ship->gov = Random_Int(GATHER_GOV_MIN, GATHER_GOV_MAX - 1);

        sf->numGuard++;
        m->cmd.target = sf->basePos;
        ship->initialized = TRUE;
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
static void GatherFleetMobDestroyed(void *aiHandle, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    GatherFleetData *sf = aiHandle;
    GatherShip *ship = aiMobHandle;

    ASSERT(sf != NULL);

    ASSERT(sf->numGuard > 0);
    sf->numGuard--;
    free(ship);
}

static GatherShip *GatherFleetGetShip(GatherFleetData *sf, MobID mobid)
{
    GatherShip *s = FleetUtil_GetMob(sf->ai, mobid)->aiMobHandle;

    if (s != NULL) {
        ASSERT(s->mobid == mobid);
    }

    return s;
}

static void GatherFleetRunAITick(void *aiHandle)
{
    GatherFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = Battle_GetParams();
    uint targetScanFilter = FLEET_SCAN_SHIP;
    IntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE) *
                       (1.0f + sf->numGuard / 8);
    float scanRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
    ASSERT(ai->player.aiType == FLEET_AI_GATHER);
    IntMap_Create(&targetMap);

    FleetUtil_SortMobsByDistance(&ai->mobs, &sf->basePos);
    FleetUtil_UpdateMobMap(ai);

    /*
     * Clear mob state
     */
    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            GatherShip *ship = mob->aiMobHandle;
            ASSERT(ship != NULL);
            ASSERT(GatherFleetGetShip(sf, mob->mobid) == ship);
            ship->targetIndex = -1;
        }
    }

    /*
     * Process sensor data.
     */
    for (uint32 si = 0; si < MobVector_Size(&ai->sensors); si++) {
        Mob *sm = MobVector_GetPtr(&ai->sensors, si);
        uint32 mi;
        Mob *m;
        GatherShip *ship;

        if (sm->type != MOB_TYPE_LOOT_BOX) {
            continue;
        }

        mi = FleetUtil_FindClosestMob(&ai->mobs, &sm->pos,
                                      FLEET_SCAN_FIGHTER);
        if (mi == -1) {
            /*
            * We ran out of mobs.
            */
            break;
        }

        m = MobVector_GetPtr(&ai->mobs, mi);
        ship = m->aiMobHandle;

        if (ship->targetIndex == -1) {
            ship->targetIndex = si;
            IntMap_Increment(&targetMap, sm->mobid);
        } else {
            /*
             * Otherwise we'll do a second pass below?
             */
        }
    }

    /*
     * Main Mob processing loop.
     */
    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            GatherShip *ship = mob->aiMobHandle;
            int t = -1;
            int ct;

            ASSERT(ship != NULL);
            ASSERT(GatherFleetGetShip(sf, mob->mobid) == ship);

            ct = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                             targetScanFilter);
            t = ct;
            if (t != -1) {
                Mob *sm = MobVector_GetPtr(&ai->sensors, t);
                if (ship->gov == GATHER_GOV_SCOUT &&
                    FPoint_Distance(&sm->pos, &mob->pos) > firingRange) {
                    t = -1;
                }
            }

            if (t == -1 && ship->targetIndex != -1) {
                t = ship->targetIndex;
            } else if (t == -1) {
                /*
                 * Generally avoid having all the fighters rush to the same
                 * loot box.
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
            }

            {
                if (ct != -1) {
                    Mob *sm;
                    sm = MobVector_GetPtr(&ai->sensors, ct);

                    if ((sf->ai->tick - ship->lastFiredTick) > 20 &&
                        FPoint_Distance(&mob->pos, &sm->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                        ship->lastFiredTick = sf->ai->tick;
                    }
                }
            }

            if (t != -1) {
                float moveRadius = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, t);

                if (sm->type != MOB_TYPE_LOOT_BOX) {
                    FleetUtil_RandomPointInRange(&mob->cmd.target,
                                                 &sm->pos, moveRadius);
                } else {
                    mob->cmd.target = sm->pos;
                }
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (ship->gov == GATHER_GOV_GUARD) {
                    float moveRadius = guardRange;
                    FPoint moveCenter = sf->basePos;
                    FleetUtil_RandomPointInRange(&mob->cmd.target,
                                                    &moveCenter, moveRadius);
                } else {
                    float moveRadius = 3 * scanRange;
                    FPoint moveCenter = mob->pos;
                    do {
                        FleetUtil_RandomPointInRange(&mob->cmd.target,
                                                     &moveCenter, moveRadius);
                    } while (FPoint_Distance(&moveCenter, &mob->cmd.target) <
                             scanRange);
                }
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = FLEET_SCAN_SHIP | FLEET_SCAN_MISSILE;
            int s = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (s != -1) {
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, s);
                mob->cmd.target = sm->pos;
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                float moveRadius = firingRange;
                FPoint moveCenter = mob->pos;
                FleetUtil_RandomPointInRange(&mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 && Random_Int(0, 20) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                mob->cmd.target.x = Random_Float(0.0f, bp->width);
                mob->cmd.target.y = Random_Float(0.0f, bp->height);
            }
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
