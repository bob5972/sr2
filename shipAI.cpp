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

BasicAIGovernor::ShipAI *BasicAIGovernor::createShip(MobID mobid)
{
        BasicShipAI *ship = new BasicShipAI(mobid);
        Mob *friendBase = mySensorGrid->friendBase();

        if (friendBase != NULL) {
            ship->enemyPos = friendBase->pos;
        }

        Mob *m = getMob(mobid);
        if (m != NULL) {
            ShipAI *p = getShip(m->parentMobid);
            if (p != NULL) {
                BasicShipAI *pShip = (BasicShipAI *)p;
                ship->enemyPos = pShip->enemyPos;
            }
        }

        return (ShipAI *)ship;
    }

void BasicAIGovernor::runMob(Mob *mob)
{
    SensorGrid *sg = mySensorGrid;
    FleetAI *ai = myFleetAI;
    RandomState *rs = &myRandomState;
    BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);

    ASSERT(ship != NULL);

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
        if (ai->credits > 200 && RandomState_Int(rs, 0, 10) == 0) {
            mob->cmd.spawnType = MOB_TYPE_FIGHTER;
        } else {
            mob->cmd.spawnType = MOB_TYPE_INVALID;
        }
    } else if (mob->type == MOB_TYPE_FIGHTER) {
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
            ship->state = BSAI_STATE_ATTACK;
            mob->cmd.spawnType = MOB_TYPE_MISSILE;
            ship->enemyPos = enemyTarget->pos;

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
        MobTypeFlags evadeFilter = MOB_FLAG_MISSILE;

        if (myConfig.evadeFighters) {
            evadeFilter |= MOB_FLAG_FIGHTER;
        }

        enemyTarget =
            sg->findClosestTargetInRange(&mob->pos, evadeFilter, firingRange);

        if (enemyTarget != NULL) {
            // Run away!
            ship->state = BSAI_STATE_EVADE;

            float dx = enemyTarget->pos.x - mob->pos.x;
            float dy = enemyTarget->pos.y - mob->pos.y;

            if (myConfig.evadeUseStrictDistance) {
                float d = FPoint_Distance(&enemyTarget->pos, &mob->pos);
                dx = dx * myConfig.evadeStrictDistance / d;
                dy = dy * myConfig.evadeStrictDistance / d;
            }

            mob->cmd.target.x = mob->pos.x - dx;
            mob->cmd.target.y = mob->pos.y - dy;
            ship->evadePos = mob->cmd.target;

            if (myConfig.evadeHold) {
                FPoint_Midpoint(&ship->holdPos, &mob->pos, &enemyTarget->pos);
                //ship->holdPos = mob->pos;
                //ship->holdPos = enemyTarget->pos;
            }
        } else if (lootTarget != NULL) {
            ship->state = BSAI_STATE_GATHER;
            mob->cmd.target = lootTarget->pos;
        } else if (ship->state == BSAI_STATE_HOLD) {
            if (ship->holdCount == 0) {
                ship->state = BSAI_STATE_IDLE;
            } else {
                ASSERT(ship->holdCount > 0);
                ship->holdCount--;
            }
        } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {

            if (myConfig.evadeHold && ship->state == BSAI_STATE_EVADE &&
                FPoint_Distance(&mob->cmd.target, &ship->evadePos) <= MICRON) {
                // Hold!
                ship->state = BSAI_STATE_HOLD;
                ship->holdCount = myConfig.holdCount;
                mob->cmd.target = ship->holdPos;
            } else {
                ship->state = BSAI_STATE_IDLE;
                mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
                mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
            }
        }
    } else {
        NOT_IMPLEMENTED();
    }
}
