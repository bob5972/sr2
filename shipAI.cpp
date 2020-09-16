/*
 * shipAI.cpp -- part of SpaceRobots2
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
#include "battleTypes.h"
#include "fleet.h"
}

#include "shipAI.hpp"

void BasicAIGovernor::runMob(Mob *mob)
{
    SensorGrid *sg = mySensorGrid;
    FleetAI *ai = myFleetAI;
    RandomState *rs = &myRandomState;

    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float scanningRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);

    if (mob->type == MOB_TYPE_LOOT_BOX) {
        Mob *friendMob = sg->findClosestFriend(&mob->pos, MOB_FLAG_SHIP);
        if (friendMob != NULL) {
            mob->cmd.target = friendMob->pos;
        }
    } else if (mob->type == MOB_TYPE_MISSILE) {
        uint scanFilter = MOB_FLAG_SHIP;
        float range = firingRange + 5;
        Mob *target = sg->findClosestTargetInRange(&mob->pos, scanFilter, range);
        if (target != NULL) {
            mob->cmd.target = target->pos;
        }
    } else if (mob->type == MOB_TYPE_BASE) {
        if (ai->credits > 200 && RandomState_Int(rs, 0, 20) == 0) {
            mob->cmd.spawnType = MOB_TYPE_FIGHTER;
        } else {
            mob->cmd.spawnType = MOB_TYPE_INVALID;
        }
    } else if (mob->type == MOB_TYPE_FIGHTER) {
        BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
        Mob *lootTarget = NULL;
        Mob *enemyTarget = NULL;

        ASSERT(ship != NULL);
        ASSERT(ship->mobid == mob->mobid);

        /*
         * Find loot.
         */
        lootTarget = sg->findClosestTargetInRange(&mob->pos, MOB_FLAG_LOOT_BOX,
                                                  scanningRange);

        /*
         * Find enemy targets to shoot.
         */
        enemyTarget = sg->findClosestTargetInRange(&mob->pos, MOB_FLAG_SHIP,
                                                   firingRange);
        if (enemyTarget != NULL) {
            mob->cmd.spawnType = MOB_TYPE_MISSILE;
            ship->targetPos = enemyTarget->pos;

            if (enemyTarget->type == MOB_TYPE_BASE) {
                /*
                * Be more aggressive to bases.
                */
                float range = MIN(firingRange, scanningRange) - 1;
                FleetUtil_RandomPointInRange(rs, &mob->cmd.target,
                                             &enemyTarget->pos, range);
            }
        }

        /*
         * Find enemy targets to run away from.
         */
        enemyTarget =
            sg->findClosestTargetInRange(&mob->pos,
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
            mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
            mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
        }
    } else {
        NOT_IMPLEMENTED();
    }
}
