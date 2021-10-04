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
        RandomState_CreateWithSeed(&this->rs, ai->seed);
    }

    ~CowardFleet() {
        RandomState_Destroy(&this->rs);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
};

static void *CowardFleetCreate(FleetAI *ai);
static void CowardFleetDestroy(void *aiHandle);
static void CowardFleetRunAITick(void *aiHandle);

void CowardFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "CowardFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &CowardFleetCreate;
    ops->destroyFleet = &CowardFleetDestroy;
    ops->runAITick = &CowardFleetRunAITick;
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

static void CowardFleetRunAITick(void *aiHandle)
{
    CowardFleet *sf = (CowardFleet *)aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = &sf->ai->bp;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float scanningRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE);
    float attackRange = MIN(firingRange, scanningRange) - 1;
    CMobIt mit;
    Mob *friendBase = NULL;
    Mob *threatTarget = NULL;
    uint threatCount = 0;

    ASSERT(ai->player.aiType == FLEET_AI_COWARD);

    sf->sg.updateTick(ai);

    friendBase = sf->sg.friendBase();
    if (friendBase != NULL) {
        FPoint *basePos = &friendBase->pos;
        threatTarget = sf->sg.findClosestTargetInRange(basePos,
                                                       MOB_FLAG_FIGHTER,
                                                       guardRange);
    }

    /*
     * Move Non-Fighters first, since they're simpler.
     */
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);
        if (mob->type == MOB_TYPE_POWER_CORE) {
            Mob *friendMob = sf->sg.findClosestFriend(&mob->pos, MOB_FLAG_SHIP);
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
            if (ai->credits > 200 && RandomState_Int(&sf->rs, 0, 10) == 0) {
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

        Mob *powerCoreTarget = NULL;
        Mob *enemyTarget = NULL;
        Mob *scaredTarget = NULL;

        /*
         * Find powerCore.
         */
        powerCoreTarget = sf->sg.findClosestTargetInRange(&mob->pos,
                                                     MOB_FLAG_POWER_CORE,
                                                     scanningRange);

        /*
         * Shoot if enemies are in range.
         */
        enemyTarget = sf->sg.findClosestTargetInRange(&mob->pos,
                                                      MOB_FLAG_SHIP, firingRange);
        if (enemyTarget != NULL) {
            ASSERT(FPoint_Distance(&mob->pos, &enemyTarget->pos) <= firingRange);
            mob->cmd.spawnType = MOB_TYPE_MISSILE;
        }

        /*
         * Find enemy targets to run away from.
         */
        MobTypeFlags scaredFilter = MOB_FLAG_MISSILE | MOB_FLAG_FIGHTER;
        scaredTarget = sf->sg.findClosestTargetInRange(&mob->pos, scaredFilter,
                                                       firingRange);

        if (scaredTarget != NULL) {
            // Run away!
            float dx = scaredTarget->pos.x - mob->pos.x;
            float dy = scaredTarget->pos.y - mob->pos.y;
            mob->cmd.target.x = mob->pos.x - dx;
            mob->cmd.target.y = mob->pos.y - dy;
        } else if (threatCount < 2 && threatTarget != NULL &&
                   FPoint_Distance(&mob->pos, &friendBase->pos) < guardRange) {
            // Defend the base!
            FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                         &threatTarget->pos, attackRange);
            threatCount++;
        } else if (powerCoreTarget != NULL) {
            mob->cmd.target = powerCoreTarget->pos;
        } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            mob->cmd.target.x = RandomState_Float(&sf->rs, 0.0f, bp->width);
            mob->cmd.target.y = RandomState_Float(&sf->rs, 0.0f, bp->height);

            /*
             * Be more aggressive to bases.
             */
            Mob *enemyTarget =
                sf->sg.findClosestTargetInRange(&mob->pos, MOB_FLAG_BASE,
                                                guardRange);
            if (enemyTarget != NULL) {
                FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                             &enemyTarget->pos, attackRange);
            }
        }
    }
}
