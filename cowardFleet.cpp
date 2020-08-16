/*
 * cowardFleet.cpp -- part of SpaceRobots2
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

extern "C" {
#include "fleet.h"
#include "random.h"
#include "IntMap.h"
#include "battle.h"
}

#include "MBVector.hpp"
#include "sensorGrid.hpp"

class CowardFleet {
public:
    CowardFleet(FleetAI *ai) {
        this->ai = ai;
        this->fighterCount = 0;
        this->waveSize = 0;

        RandomState_CreateWithSeed(&this->rs, ai->seed);

        if (ai->player.mreg != NULL) {
            this->waveSize = MBRegistry_GetUintD(ai->player.mreg, "WaveSize", 0);
        }
    }

    ~CowardFleet() {
        RandomState_Destroy(&this->rs);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    uint waveSize;
    int fighterCount;
};

static void *CowardFleetCreate(FleetAI *ai);
static void CowardFleetDestroy(void *aiHandle);
static void CowardFleetRunAITick(void *aiHandle);
static void *CowardFleetMobSpawned(void *aiHandle, Mob *m);
static void CowardFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

void CowardFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "CowardFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &CowardFleetCreate;
    ops->destroyFleet = &CowardFleetDestroy;
    ops->runAITick = &CowardFleetRunAITick;
    ops->mobSpawned = CowardFleetMobSpawned;
    ops->mobDestroyed = CowardFleetMobDestroyed;
}

static void *CowardFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new CowardFleet(ai);
}

static void CowardFleetDestroy(void *handle)
{
    CowardFleet *sf = (CowardFleet *)handle;
    ASSERT(sf != NULL);
    delete sf;
}

static void *CowardFleetMobSpawned(void *aiHandle, Mob *m)
{
    CowardFleet *sf = (CowardFleet *)aiHandle;

    if (m->type == MOB_TYPE_FIGHTER) {
        sf->fighterCount++;
    }

    return NULL;
}
static void CowardFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    CowardFleet *sf = (CowardFleet *)aiHandle;

    if (m->type == MOB_TYPE_FIGHTER) {
        ASSERT(sf->fighterCount > 0);
        sf->fighterCount--;
    }
}

static void CowardFleetRunAITick(void *aiHandle)
{
    CowardFleet *sf = (CowardFleet *)aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = &sf->ai->bp;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float scanningRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
    CMobIt mit;

    ASSERT(ai->player.aiType == FLEET_AI_COWARD);

    sf->sg.updateTick(ai);

    /*
     * Move Non-Fighters first, since they're simpler and modify
     * the sensor state.
     */
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);
        if (mob->type == MOB_TYPE_LOOT_BOX) {
            Mob *friendMob = FleetUtil_FindClosestMob(&sf->ai->mobs, &mob->pos,
                                                      MOB_FLAG_SHIP);
            if (friendMob != NULL) {
                mob->cmd.target = friendMob->pos;
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            float range = firingRange + 5;
            Mob *target;
            target = sf->sg.findClosestTargetInRange(&mob->pos, scanFilter, range);
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

        Mob *lootTarget = NULL;
        Mob *enemyTarget = NULL;

        /*
         * Find loot.
         */
        lootTarget = sf->sg.findClosestTargetInRange(&mob->pos,
                                                     MOB_FLAG_LOOT_BOX,
                                                     scanningRange);

        /*
         * Find enemy targets to shoot.
         */
        enemyTarget = sf->sg.findClosestTargetInRange(&mob->pos,
                                                      MOB_FLAG_SHIP, firingRange);
        if (enemyTarget != NULL) {
            ASSERT(FPoint_Distance(&mob->pos, &enemyTarget->pos) < firingRange);
            mob->cmd.spawnType = MOB_TYPE_MISSILE;

            if (enemyTarget->type == MOB_TYPE_BASE) {
                /*
                * Be more aggressive to bases.
                */
                float range = MIN(firingRange, scanningRange) - 1;
                FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                             &enemyTarget->pos, range);
            }
        }

        /*
         * Find enemy targets to run away from.
         */
        enemyTarget =
            sf->sg.findClosestTargetInRange(&mob->pos,
                                            MOB_FLAG_FIGHTER | MOB_FLAG_MISSILE,
                                            firingRange);


        if (enemyTarget != NULL) {
            // Run away!
            float dx = enemyTarget->pos.x - mob->pos.x;
            float dy = enemyTarget->pos.y - mob->pos.y;
            mob->cmd.target.x = mob->pos.x - dx;
            mob->cmd.target.y = mob->pos.y - dy;
        } else if (lootTarget != NULL) {
            mob->cmd.target = lootTarget->pos;
        } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            mob->cmd.target.x = RandomState_Float(&sf->rs, 0.0f, bp->width);
            mob->cmd.target.y = RandomState_Float(&sf->rs, 0.0f, bp->height);

            if (sf->waveSize > 0 && sf->fighterCount > sf->waveSize) {
                Mob *enemyBase = sf->sg.enemyBase();
                if (enemyBase != NULL) {
                    mob->cmd.target = enemyBase->pos;
                }
            }
        }
    }
}
