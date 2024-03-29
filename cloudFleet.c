/*
 * cloudFleet.c -- part of SpaceRobots2
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

typedef struct CloudShip {
    MobID mobid;
    bool initialized;
} CloudShip;

typedef struct CloudFleetData {
    FleetAI *ai;
    RandomState rs;
    bool crazyMissiles;

    FPoint basePos;
    uint numGuard;
} CloudFleetData;

static void *CloudFleetCreate(FleetAI *ai);
static void CloudFleetDestroy(void *aiHandle);
static void CloudFleetRunAITick(void *aiHandle);
static void *CloudFleetMobSpawned(void *aiHandle, Mob *m);
static void CloudFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static CloudShip *CloudFleetGetShip(CloudFleetData *sf, MobID mobid);

void CloudFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
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
        if (MBRegistry_GetBoolD(sf->ai->player.mreg, "CrazyMissiles",
                                FALSE)) {
            sf->crazyMissiles = TRUE;
        }
    }

    RandomState_CreateWithSeed(&sf->rs, ai->seed);

    return sf;
}

static void CloudFleetDestroy(void *aiHandle)
{
    CloudFleetData *sf = aiHandle;
    ASSERT(sf != NULL);

    RandomState_Destroy(&sf->rs);
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
static void CloudFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
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
    CloudShip *s = MobPSet_Get(&sf->ai->mobs, mobid)->aiMobHandle;
    ASSERT(s != NULL);
    ASSERT(s->mobid == mobid);
    return s;
}


static void CloudFleetRunAITick(void *aiHandle)
{
    CloudFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    uint targetScanFilter = MOB_FLAG_SHIP;
    CMBIntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE) *
                       (1.0f + sf->numGuard / 10);

    CMBIntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_CLOUD);

    /*
     * Main Mob processing loop.
     */
    CMobIt mit;
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);

        if (mob->type == MOB_TYPE_FIGHTER) {
            CloudShip *ship = CloudFleetGetShip(sf, mob->mobid);
            Mob *target = NULL;

            ASSERT(ship != NULL);
            ASSERT(ship->mobid == mob->mobid);

            target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                 targetScanFilter);

            if (target == NULL) {
                /*
                * Avoid having all the fighters rush to the same power core.
                */
                target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     MOB_FLAG_POWER_CORE);
                if (target != NULL) {
                    if (FPoint_Distance(&target->pos, &mob->pos) > firingRange) {
                        target = NULL;
                    }
                }

                if (target != NULL &&
                    CMBIntMap_Increment(&targetMap, target->mobid) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    target = NULL;
                }
            }

            {
                Mob *ctMob = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                         targetScanFilter);
                if (ctMob != NULL) {
                    if (FPoint_Distance(&mob->pos, &ctMob->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
                }
            }

            if (target != NULL) {
                float moveRadius = 2 * MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
                if (target->type != MOB_TYPE_POWER_CORE) {
                    FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                                 &target->pos, moveRadius);
                } else {
                    mob->cmd.target = target->pos;
                }
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                float moveRadius = guardRange;
                FPoint moveCenter = sf->basePos;
                FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            Mob *target = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (target != NULL) {
                mob->cmd.target = target->pos;
            } else if (sf->crazyMissiles &&
                       FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                float moveRadius = firingRange;
                FPoint moveCenter = mob->pos;
                FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 &&
                RandomState_Int(&sf->rs, 0, 20) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            ASSERT(MobType_GetSpeed(MOB_TYPE_BASE) == 0.0f);
        } else if (mob->type == MOB_TYPE_POWER_CORE) {
            /*
             * Add this mob to the sensor list so that we'll
             * steer towards it.
             */
            mob->cmd.target = sf->basePos;
            MobPSet_Add(&ai->sensors, mob);
        }
    }

    CMBIntMap_Destroy(&targetMap);
}
