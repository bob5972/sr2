/*
 * runAwayFleet.c -- part of SpaceRobots2
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

typedef struct RunAwayShip {
    MobID mobid;
    FPoint targetPos;
} RunAwayShip;

typedef struct RunAwayFleetData {
    FleetAI *ai;
    RandomState rs;
} RunAwayFleetData;

static void *RunAwayFleetCreate(FleetAI *ai);
static void RunAwayFleetDestroy(void *aiHandle);
static void RunAwayFleetRunAITick(void *aiHandle);
static void *RunAwayFleetMobSpawned(void *aiHandle, Mob *m);
static void RunAwayFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static RunAwayShip *RunAwayFleetGetShip(RunAwayFleetData *sf, MobID mobid);

void RunAwayFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "RunAwayFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &RunAwayFleetCreate;
    ops->destroyFleet = &RunAwayFleetDestroy;
    ops->runAITick = &RunAwayFleetRunAITick;
    ops->mobSpawned = RunAwayFleetMobSpawned;
    ops->mobDestroyed = RunAwayFleetMobDestroyed;
}

static void *RunAwayFleetCreate(FleetAI *ai)
{
    RunAwayFleetData *sf;
    ASSERT(ai != NULL);

    sf = MBUtil_ZAlloc(sizeof(*sf));
    sf->ai = ai;

    RandomState_CreateWithSeed(&sf->rs, ai->seed);
    return sf;
}

static void RunAwayFleetDestroy(void *handle)
{
    RunAwayFleetData *sf = handle;
    ASSERT(sf != NULL);
    RandomState_Destroy(&sf->rs);
    free(sf);
}

static void *RunAwayFleetMobSpawned(void *aiHandle, Mob *m)
{
    RunAwayFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        RunAwayShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;
        return ship;
    } else if (m->type == MOB_TYPE_MISSILE) {
        MobID parentMobid = m->parentMobid;
        RunAwayShip *parent = RunAwayFleetGetShip(sf, parentMobid);
        if (parent != NULL) {
            m->cmd.target = parent->targetPos;
        }

        return NULL;
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
static void RunAwayFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    RunAwayFleetData *sf = aiHandle;
    RunAwayShip *ship = aiMobHandle;

    ASSERT(sf != NULL);
    free(ship);
}

static RunAwayShip *RunAwayFleetGetShip(RunAwayFleetData *sf, MobID mobid)
{
    RunAwayShip *s = MobPSet_Get(&sf->ai->mobs, mobid)->aiMobHandle;

    if (s != NULL) {
        ASSERT(s->mobid == mobid);
    }

    return s;
}


static void RunAwayFleetRunAITick(void *aiHandle)
{
    RunAwayFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = &sf->ai->bp;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float scanningRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
    CMobIt mit;

    ASSERT(ai->player.aiType == FLEET_AI_RUNAWAY);

    /*
     * Move Non-Fighters first, since they're simpler and modify
     * the sensor state.
     */
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);
        if (mob->type == MOB_TYPE_POWER_CORE) {
            Mob *friend = FleetUtil_FindClosestMob(&sf->ai->mobs, &mob->pos,
                                                   MOB_FLAG_SHIP);
            if (friend != NULL) {
                mob->cmd.target = friend->pos;
            }

            /*
             * Add this mob to the sensor list so that we'll
             * steer towards it.
             */
            MobPSet_Add(&ai->sensors, mob);
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            float range = firingRange + 5;
            Mob *target = FleetUtil_FindClosestMobInRange(&ai->sensors, &mob->pos,
                                                          scanFilter, range);
            if (target != NULL) {
                mob->cmd.target = target->pos;
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            if (ai->credits > 200 && RandomState_Int(&sf->rs, 0, 20) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }
        } else {
            ASSERT(mob->type == MOB_TYPE_FIGHTER);
        }
    }

    /*
     * Move Fighters
     */
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);

        if (mob->type != MOB_TYPE_FIGHTER) {
            continue;
        }

        RunAwayShip *ship = RunAwayFleetGetShip(sf, mob->mobid);
        Mob *powerCoreTarget = NULL;
        Mob *enemyTarget = NULL;

        ASSERT(ship != NULL);
        ASSERT(ship->mobid == mob->mobid);

        /*
         * Find powerCore.
         */
        powerCoreTarget = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                 MOB_FLAG_POWER_CORE);

        if (powerCoreTarget != NULL) {
            if (FPoint_Distance(&mob->pos, &powerCoreTarget->pos) > scanningRange) {
                powerCoreTarget = NULL;
            }
        }

        /*
         * Find enemy targets to shoot.
         */
        enemyTarget = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                  MOB_FLAG_SHIP);
        if (enemyTarget != NULL) {
            if (FPoint_Distance(&mob->pos, &enemyTarget->pos) < firingRange) {
                mob->cmd.spawnType = MOB_TYPE_MISSILE;
                ship->targetPos = enemyTarget->pos;

                if (enemyTarget->type == MOB_TYPE_BASE) {
                    /*
                    * Be more aggressive to bases.
                    */
                    float range = MIN(firingRange, scanningRange) - 1;
                    FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                                 &enemyTarget->pos, range);
                }
            }
        }

        /*
         * Find enemy targets to run away from.
         */
        enemyTarget = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                  MOB_FLAG_FIGHTER | MOB_FLAG_MISSILE);
        if (enemyTarget != NULL) {
            if (FPoint_Distance(&mob->pos, &enemyTarget->pos) >= firingRange) {
                enemyTarget = NULL;
            }
        }

        if (enemyTarget != NULL) {
            // Run away!
            float dx = enemyTarget->pos.x - mob->pos.x;
            float dy = enemyTarget->pos.y - mob->pos.y;
            mob->cmd.target.x = mob->pos.x - dx;
            mob->cmd.target.y = mob->pos.y - dy;
        } else if (powerCoreTarget != NULL) {
            mob->cmd.target = powerCoreTarget->pos;
        } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            mob->cmd.target.x = RandomState_Float(&sf->rs, 0.0f, bp->width);
            mob->cmd.target.y = RandomState_Float(&sf->rs, 0.0f, bp->height);
        }
    }
}
