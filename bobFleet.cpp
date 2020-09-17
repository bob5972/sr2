/*
 * bobFleet.cpp -- part of SpaceRobots2
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

#include "sensorGrid.hpp"
#include "shipAI.hpp"

typedef enum BobGovernor {
    BOB_GOV_INVALID = 0,
    BOB_GOV_GUARD   = 1,
    BOB_GOV_MIN     = 1,
    BOB_GOV_SCOUT   = 2,
    BOB_GOV_ATTACK  = 3,
    BOB_GOV_MAX,
} BobGovernor;

class BobShip {
public:
    BobShip(MobID mobid, BobGovernor gov)
    :mobid(mobid), gov(gov)
    {}

    ~BobShip()
    {}

    MobID mobid;
    BobGovernor gov;
};

class BobFleet {
public:
    BobFleet(FleetAI *ai)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        MBUtil_Zero(&this->numGov, sizeof(this->numGov));
    }

    ~BobFleet() {
        RandomState_Destroy(&this->rs);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    uint numGov[BOB_GOV_MAX];
};

static void *BobFleetCreate(FleetAI *ai);
static void BobFleetDestroy(void *aiHandle);
static void BobFleetRunAITick(void *aiHandle);
static void *BobFleetMobSpawned(void *aiHandle, Mob *m);
static void BobFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

void BobFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "BobFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BobFleetCreate;
    ops->destroyFleet = &BobFleetDestroy;
    ops->runAITick = &BobFleetRunAITick;
    ops->mobSpawned = BobFleetMobSpawned;
    ops->mobDestroyed = BobFleetMobDestroyed;
}

static void *BobFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new BobFleet(ai);
}

static void BobFleetDestroy(void *handle)
{
    BobFleet *sf = (BobFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void BobFleetSetGov(BobFleet *sf, BobShip *ship, BobGovernor gov)
{
    ASSERT(sf != NULL);
    ASSERT(ship != NULL);

    ASSERT(ship->gov < BOB_GOV_MAX);
    ASSERT(gov < BOB_GOV_MAX);


    if (ship->gov != BOB_GOV_INVALID) {
        ASSERT(sf->numGov[ship->gov] > 0);
        sf->numGov[ship->gov]--;
    }

    ship->gov = gov;

    if (gov != BOB_GOV_INVALID) {
        sf->numGov[gov]++;
    }
}

static void *BobFleetMobSpawned(void *aiHandle, Mob *m)
{
    BobFleet *sf = (BobFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        BobShip *ship = new BobShip(m->mobid, BOB_GOV_INVALID);
        m->cmd.target = *sf->sg.friendBasePos();

        if (sf->numGov[BOB_GOV_GUARD] < 1) {
            BobFleetSetGov(sf, ship, BOB_GOV_GUARD);
        } else {
            BobFleetSetGov(sf, ship, BOB_GOV_SCOUT);
        }

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
static void BobFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    BobFleet *sf = (BobFleet *)aiHandle;
    BobShip *ship = (BobShip *)aiMobHandle;

    BobFleetSetGov(sf, ship, BOB_GOV_INVALID);

    delete(ship);
}

static void BobFleetRunAITick(void *aiHandle)
{
    BobFleet *sf = (BobFleet *)aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = &sf->ai->bp;
    uint targetScanFilter = MOB_FLAG_SHIP;
    CIntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE);
    CMobIt mit;
    Mob *groupTarget = NULL;
    bool doAttack = FALSE;

    CIntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_BOB);

    sf->sg.updateTick(ai);

    groupTarget = sf->sg.findClosestTarget(sf->sg.friendBasePos(),
                                           targetScanFilter);

    if (sf->numGov[BOB_GOV_SCOUT] > 12) {
        doAttack = TRUE;
    }

    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);

        if (mob->type == MOB_TYPE_FIGHTER) {
            BobShip *ship = (BobShip *)mob->aiMobHandle;
            Mob *target = NULL;

            ASSERT(ship != NULL);
            ASSERT(ship->mobid == mob->mobid);
            ASSERT(ship->gov < BOB_GOV_MAX);

            if (ship->gov == BOB_GOV_SCOUT) {
                /*
                 * Just run the shared random/loot-box code.
                 */
                if (doAttack && sf->numGov[BOB_GOV_SCOUT] > 2) {
                    BobFleetSetGov(sf, ship, BOB_GOV_ATTACK);
                }
            } else if (ship->gov == BOB_GOV_ATTACK) {
                target = sf->sg.findClosestTarget(&mob->pos, targetScanFilter);
            } else if (ship->gov == BOB_GOV_GUARD) {
                target = sf->sg.findClosestTargetInRange(&mob->pos,
                                                         targetScanFilter,
                                                         guardRange);

                if (target == NULL && groupTarget != NULL) {
                    target = groupTarget;

                    if (target != NULL) {
                        if (FPoint_Distance(&target->pos, sf->sg.friendBasePos()) >
                            guardRange) {
                            target = NULL;
                        }
                    }
                }
            }

            if (target == NULL) {
                /*
                * Avoid having all the fighters rush to the same loot box.
                */
                target = sf->sg.findClosestTarget(&mob->pos, MOB_FLAG_LOOT_BOX);
                if (target != NULL &&
                    CIntMap_Increment(&targetMap, target->mobid) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    target = NULL;
                }

                if (ship->gov == BOB_GOV_GUARD && target != NULL) {
                    if (FPoint_Distance(&target->pos, sf->sg.friendBasePos()) >
                        guardRange) {
                        target = NULL;
                    }
                }
            }

            {
                Mob *closeTarget =
                    sf->sg.findClosestTargetInRange(&mob->pos, targetScanFilter,
                                                    firingRange);
                if (closeTarget != NULL) {
                    mob->cmd.spawnType = MOB_TYPE_MISSILE;
                }
            }

            if (target != NULL) {
                mob->cmd.target = target->pos;
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (ship->gov == BOB_GOV_GUARD) {
                    FPoint *basePos = sf->sg.friendBasePos();
                    mob->cmd.target.x =
                        RandomState_Float(&sf->rs,
                                          MAX(0, basePos->x - guardRange),
                                          basePos->x + guardRange);
                    mob->cmd.target.y =
                        RandomState_Float(&sf->rs,
                                          MAX(0, basePos->y - guardRange),
                                          basePos->y + guardRange);
                } else {
                    mob->cmd.target.x =
                        RandomState_Float(&sf->rs, 0.0f, bp->width);
                    mob->cmd.target.y =
                        RandomState_Float(&sf->rs, 0.0f, bp->height);
                }
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            Mob *target = sf->sg.findClosestTarget(&mob->pos, scanFilter);
            if (target != NULL) {
                mob->cmd.target = target->pos;
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            if (ai->credits > 200 && RandomState_Int(&sf->rs, 0, 10) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            ASSERT(MobType_GetSpeed(MOB_TYPE_BASE) == 0.0f);
        } else if (mob->type == MOB_TYPE_LOOT_BOX) {
            Mob *friendMob = sf->sg.findClosestFriend(&mob->pos,
                                                      MOB_FLAG_SHIP);
            if (friendMob != NULL) {
                mob->cmd.target = friendMob->pos;
            }

            /*
             * Add this mob to the sensor list so that we'll
             * steer towards it.
             */
            MobPSet_Add(&ai->sensors, mob);
        }
    }

    CIntMap_Destroy(&targetMap);
}
