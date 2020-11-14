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

BasicAIGovernor::ShipAI *BasicAIGovernor::newShip(MobID mobid)
{
        BasicShipAI *ship = new BasicShipAI(mobid, this);

        Mob *m = getMob(mobid);
        if (m != NULL) {
            ShipAI *p = getShip(m->parentMobid);
            if (p != NULL) {
                BasicShipAI *pShip = (BasicShipAI *)p;
                ship->attackData.pos = pShip->attackData.pos;
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

    ship->oldState = ship->state;
    ship->stateChanged = FALSE;

    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    //float scanningRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);

    if (mob->type == MOB_TYPE_POWER_CORE) {
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
        Mob *powerCoreTarget = NULL;
        Mob *enemyTarget = NULL;
        bool redoIdle = FALSE;
        Mob *friendBase = sg->friendBase();

        float attackRange = firingRange;

        if (myConfig.attackRange > 0 && myConfig.attackExtendedRange) {
            attackRange = MAX(firingRange, myConfig.attackRange);
        }


        if (friendBase != NULL && myConfig.guardRange > 0 &&
            FPoint_Distance(&mob->pos, &friendBase->pos) <= myConfig.guardRange) {
            attackRange = MAX(attackRange, myConfig.guardRange);
        }

        ASSERT(ship != NULL);
        ASSERT(ship->mobid == mob->mobid);

        /*
         * Find powerCore.
         */
        powerCoreTarget = sg->findClosestTargetInRange(&mob->pos,
                                                       MOB_FLAG_POWER_CORE,
                                                       myConfig.gatherRange);

        if (powerCoreTarget == NULL && ship->state == BSAI_STATE_GATHER) {
            ship->state = BSAI_STATE_IDLE;
            redoIdle = TRUE;
        }

        /*
         * Find enemy targets to shoot.
         */
        enemyTarget = sg->findClosestTargetInRange(&mob->pos, MOB_FLAG_SHIP,
                                                   attackRange);
        if (enemyTarget != NULL) {
            ship->state = BSAI_STATE_ATTACK;
            doAttack(mob, enemyTarget);
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
            ship->evadeData.pos = mob->cmd.target;
        } else if (ship->state == BSAI_STATE_HOLD) {
            if (ship->holdData.count == 0) {
                ship->state = BSAI_STATE_IDLE;
            } else {
                mob->cmd.target = ship->holdData.pos;
                ASSERT(ship->holdData.count > 0);
                ship->holdData.count--;
            }
        } else if (powerCoreTarget != NULL) {
            ship->state = BSAI_STATE_GATHER;
            mob->cmd.target = powerCoreTarget->pos;
        } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            ship->state = BSAI_STATE_IDLE;
            redoIdle = TRUE;
        }

        if (ship->state == BSAI_STATE_IDLE) {
            doIdle(mob, redoIdle || ship->oldState != BSAI_STATE_IDLE);
        }
    } else {
        NOT_IMPLEMENTED();
    }

    if (ship->state != ship->oldState) {
        ship->stateChanged = TRUE;
    }
}
