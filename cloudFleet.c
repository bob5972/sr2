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

typedef struct CloudShip {
    MobID mobid;
    bool initialized;
} CloudShip;

typedef struct CloudFleetData {
    FleetAI *ai;
    bool kamikazeMissiles;

    FPoint basePos;
    uint numGuard;
} CloudFleetData;

static void *CloudFleetCreate(FleetAI *ai);
static void CloudFleetDestroy(void *aiHandle);
static void CloudFleetRunAITick(void *aiHandle);
static void *CloudFleetMobSpawned(void *aiHandle, Mob *m);
static void CloudFleetMobDestroyed(void *aiHandle, void *aiMobHandle);
static CloudShip *CloudFleetGetShip(CloudFleetData *sf, MobID mobid);

void CloudFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "CloudFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &CloudFleetCreate;
    ops->destroyFleet = &CloudFleetDestroy;
    ops->runAITick = &CloudFleetRunAITick;
    ops->mobSpawned = &CloudFleetMobSpawned;
    ops->mobDestroyed = &CloudFleetMobDestroyed;
}

static void *CloudFleetCreate(FleetAI *ai)
{
    CloudFleetData *sf;
    ASSERT(ai != NULL);

    sf = MBUtil_ZAlloc(sizeof(*sf));
    sf->ai = ai;

    if (sf->ai->player.mreg != NULL) {
        if (MBRegistry_GetBoolD(sf->ai->player.mreg, "KamikazeMissiles",
                                FALSE)) {
            sf->kamikazeMissiles = TRUE;
        }
    }

    return sf;
}

static void CloudFleetDestroy(void *aiHandle)
{
    CloudFleetData *sf = aiHandle;
    ASSERT(sf != NULL);

    free(sf);
}

static void *CloudFleetMobSpawned(void *aiHandle, Mob *m)
{
    CloudFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        CloudShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;
        m->cmd.target = sf->basePos;
        ship->initialized = TRUE;
        sf->numGuard++;
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
static void CloudFleetMobDestroyed(void *aiHandle, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    CloudFleetData *sf = aiHandle;
    CloudShip *ship = aiMobHandle;

    ASSERT(sf != NULL);
    ASSERT(sf->numGuard > 0);
    sf->numGuard--;
    free(ship);
}


static CloudShip *CloudFleetGetShip(CloudFleetData *sf, MobID mobid)
{
    CloudShip *s = FleetUtil_GetMob(sf->ai, mobid)->aiMobHandle;

    if (s != NULL) {
        ASSERT(s->mobid == mobid);
    }

    return s;
}


static void CloudFleetRunAITick(void *aiHandle)
{
    CloudFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    uint targetScanFilter = FLEET_SCAN_SHIP;
    IntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE) *
                       (1.0f + sf->numGuard / 10);

    IntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_CLOUD);

    /*
     * Main Mob processing loop.
     */
    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            CloudShip *ship = CloudFleetGetShip(sf, mob->mobid);
            int t = -1;

            ASSERT(ship != NULL);
            ASSERT(ship->mobid == mob->mobid);

            t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                            targetScanFilter);

            if (t == -1) {
                /*
                * Avoid having all the fighters rush to the same loot box.
                */
                t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                FLEET_SCAN_LOOT_BOX);
                if (t != -1) {
                    Mob *sm;
                    sm = MobVector_GetPtr(&ai->sensors, t);
                    if (FPoint_Distance(&sm->pos, &mob->pos) > firingRange) {
                        t = -1;
                    }
                }

                if (t != -1 && IntMap_Increment(&targetMap, t) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    t = -1;
                }
            }

            {
                int ct = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     targetScanFilter);
                if (ct != -1) {
                    Mob *sm;
                    sm = MobVector_GetPtr(&ai->sensors, ct);

                    if (FPoint_Distance(&mob->pos, &sm->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
                }
            }

            if (t != -1) {
                float moveRadius = 2 * MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
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
                float moveRadius = guardRange;
                FPoint moveCenter = sf->basePos;
                FleetUtil_RandomPointInRange(&mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = FLEET_SCAN_SHIP | FLEET_SCAN_MISSILE;
            int s = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (s != -1) {
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, s);
                mob->cmd.target = sm->pos;
            } else if (sf->kamikazeMissiles &&
                       FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                float moveRadius = firingRange;
                FPoint moveCenter = mob->pos;
                FleetUtil_RandomPointInRange(&mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 &&
                Random_Int(0, 20) == 0) {
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
