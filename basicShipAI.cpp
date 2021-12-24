/*
 * basicShipAI.cpp -- part of SpaceRobots2
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

extern "C" {
#include "battleTypes.h"
#include "fleet.h"
}

#include "basicShipAI.hpp"

void BasicAIGovernor::doSpawn(Mob *mob)
{
    FleetAI *ai = myFleetAI;

    BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
    if (ship != NULL) {
        BasicShipAI *p = (BasicShipAI *)getShip(mob->parentMobid);
        if (p != NULL) {
            BasicShipAI *pShip = (BasicShipAI *)p;
            ship->attackData.pos = pShip->attackData.pos;
        }
    }

    if (myConfig.rotateStartingAngle &&
        mob->type == MOB_TYPE_FIGHTER) {
        FRPoint p;
        uint i = 0;
        bool keepGoing = TRUE;

        do {
            // Rotate by the Golden Angle
            myStartingAngle += M_PI * (3 - sqrtf(5.0f));
            p.radius = myConfig.startingMaxRadius;
            p.theta = myStartingAngle;

            do {
                FRPoint_ToFPoint(&p, &mob->pos, &mob->cmd.target);
                p.radius /= 1.1f;

                i++;
                if (i >= 10 * 1000) {
                    /*
                        * If the min/max radius are set wrong,
                        * we could otherwise loop here forever.
                        */
                    mob->cmd.target = mob->pos;
                    keepGoing = FALSE;
                }
            } while (keepGoing &&
                        p.radius >= myConfig.startingMinRadius &&
                        FPoint_Clamp(&mob->cmd.target, 0.0f, ai->bp.width,
                                    0.0f, ai->bp.height));
        } while (keepGoing &&
                    p.radius < myConfig.startingMinRadius);
    }
}

void BasicAIGovernor::doIdle(Mob *mob, bool newlyIdle)
{
    FleetAI *ai = myFleetAI;
    RandomState *rs = &myRandomState;
    BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);

    ship->state = BSAI_STATE_IDLE;

    if (newlyIdle) {
        mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
        mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
    }
}

void BasicAIGovernor::doAttack(Mob *mob, Mob *enemyTarget)
{
    RandomState *rs = &myRandomState;
    BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
    SensorGrid *sg = mySensorGrid;
    Mob *friendBase = sg->friendBase();

    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float scanningRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);

    bool beAggressive = FALSE;

    ship->state = BSAI_STATE_ATTACK;
    ship->attackData.pos = enemyTarget->pos;

    if (RandomState_Int(rs, 0, myConfig.fighterFireJitter) == 0 &&
        FPoint_Distance(&mob->pos, &enemyTarget->pos) <= firingRange) {
        mob->cmd.spawnType = MOB_TYPE_MISSILE;
    }

    if (myConfig.attackRange > 0 &&
        FPoint_Distance(&mob->pos, &enemyTarget->pos) <
        myConfig.attackRange) {
        beAggressive = TRUE;
    } else if (enemyTarget->type == MOB_TYPE_BASE) {
        beAggressive = TRUE;
    } else if (friendBase != NULL && myConfig.guardRange > 0 &&
                FPoint_Distance(&enemyTarget->pos, &friendBase->pos) <=
                myConfig.guardRange) {
        beAggressive = TRUE;
    }


    if (beAggressive) {
        float range = MIN(firingRange, scanningRange) - 1;
        FleetUtil_RandomPointInRange(rs, &mob->cmd.target,
                                        &enemyTarget->pos, range);
    }
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
        float baseRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
        FPoint *friendBasePos = sg->friendBasePos();
        Mob *friendMob = sg->findClosestFriend(&mob->pos, MOB_FLAG_SHIP);
        float baseD = INFINITY;
        float friendD = INFINITY;
        float friendFromBaseD = INFINITY;
        bool friendMovingCloser = FALSE;

        if (friendBasePos != NULL) {
            baseD = FPoint_Distance(friendBasePos, &mob->pos);
        }

        if (friendMob != NULL) {
            friendD = FPoint_Distance(&friendMob->pos, &mob->pos);

            if (friendD <= FPoint_Distance(&mob->pos, &friendMob->lastPos)) {
                friendMovingCloser = TRUE;
            }
        }

        if (friendBasePos != NULL && friendMob != NULL) {
            friendFromBaseD = FPoint_Distance(friendBasePos, &friendMob->pos);
        }

        if (friendFromBaseD <= baseD) {
            mob->cmd.target = friendMob->pos;
        } else if (baseD <= friendD || baseD <= baseRadius) {
            mob->cmd.target = *friendBasePos;
        } else if (friendD <= myConfig.gatherRange && friendMovingCloser) {
            mob->cmd.target = friendMob->pos;
        } else if (friendBasePos != NULL) {
            mob->cmd.target = *friendBasePos;
        } else if (friendMob != NULL) {
            mob->cmd.target = friendMob->pos;
        }
    } else if (mob->type == MOB_TYPE_MISSILE) {
        uint scanFilter = MOB_FLAG_SHIP;
        float range = firingRange + 5;
        Mob *target = sg->findClosestTargetInRange(&mob->pos, scanFilter, range);
        if (target != NULL) {
            mob->cmd.target = target->pos;
        } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            /*
             * We have no target, and we've reached our last known point.
             * Continue motion in the same direction.
             */
            FRPoint pr;
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &pr);
            if (pr.radius <= MICRON) {
                /*
                 * If we're too close to our last point, then go off in a
                 * random direction.
                 */
                pr.theta = RandomState_Float(&myRandomState, 0, 2 * M_PI);
            }
            pr.radius += MobType_GetSpeed(MOB_TYPE_MISSILE);
            FRPoint_ToFPoint(&pr, &mob->lastPos, &mob->cmd.target);
        }
    } else if (mob->type == MOB_TYPE_BASE) {
        if (ai->credits > myConfig.creditReserve &&
            RandomState_Int(rs, 0, myConfig.baseSpawnJitter) == 0) {
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
