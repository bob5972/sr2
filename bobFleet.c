/*
 * bobFleet.c -- part of SpaceRobots2
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

typedef enum BobGovernor {
    BOB_GOV_INVALID = 0,
    BOB_GOV_GUARD   = 1,
    BOB_GOV_MIN     = 1,
    BOB_GOV_SCOUT   = 2,
    BOB_GOV_ATTACK  = 3,
    BOB_GOV_MAX,
} BobGovernor;

typedef struct BobShip {
    MobID mobid;
    BobGovernor gov;
} BobShip;

typedef struct BobFleetData {
    FleetAI *ai;
    FPoint basePos;
    Mob enemyBase;
    uint enemyBaseAge;
    RandomState rs;
    uint numGov[BOB_GOV_MAX];
} BobFleetData;

static void *BobFleetCreate(FleetAI *ai);
static void BobFleetDestroy(void *aiHandle);
static void BobFleetRunAITick(void *aiHandle);
static void *BobFleetMobSpawned(void *aiHandle, Mob *m);
static void BobFleetMobDestroyed(void *aiHandle, void *aiMobHandle);
static BobShip *BobFleetGetShip(BobFleetData *sf, MobID mobid);

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
    BobFleetData *sf;
    ASSERT(ai != NULL);

    sf = MBUtil_ZAlloc(sizeof(*sf));
    sf->ai = ai;

    RandomState_CreateWithSeed(&sf->rs, ai->seed);
    return sf;
}

static void BobFleetDestroy(void *handle)
{
    BobFleetData *sf = handle;
    ASSERT(sf != NULL);
    RandomState_Destroy(&sf->rs);
    free(sf);
}

static void *BobFleetMobSpawned(void *aiHandle, Mob *m)
{
    BobFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        BobShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;
        m->cmd.target = sf->basePos;

        if (sf->numGov[BOB_GOV_GUARD] < 1) {
            ship->gov = BOB_GOV_GUARD;
        } else {
            ship->gov = BOB_GOV_SCOUT;
        }
        sf->numGov[ship->gov]++;
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
static void BobFleetMobDestroyed(void *aiHandle, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    BobFleetData *sf = aiHandle;
    BobShip *ship = aiMobHandle;

    ASSERT(ship->gov < ARRAYSIZE(sf->numGov));
    ASSERT(sf->numGov[ship->gov] > 0);
    sf->numGov[ship->gov]--;

    ASSERT(sf != NULL);
    free(ship);
}

static BobShip *BobFleetGetShip(BobFleetData *sf, MobID mobid)
{
    BobShip *s = MobSet_Get(&sf->ai->mobs, mobid)->aiMobHandle;

    ASSERT(s != NULL);
    ASSERT(s->mobid == mobid);
    return s;
}


static void BobFleetRunAITick(void *aiHandle)
{
    BobFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = &sf->ai->bp;
    uint targetScanFilter = MOB_FLAG_SHIP;
    IntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    MobIt mit;

    IntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_BOB);

    /*
     * Save the last enemy base we've seen.
     */
    Mob *enemyBase = FleetUtil_FindClosestSensor(ai, &sf->basePos, MOB_FLAG_BASE);
    if (enemyBase != NULL) {
        ASSERT(enemyBase->type == MOB_TYPE_BASE);
        sf->enemyBase = *enemyBase;
        sf->enemyBaseAge = 0;
    } else {
        MobIt_Start(&ai->mobs, &mit);
        while (MobIt_HasNext(&mit)) {
            Mob *mob = MobIt_Next(&mit);
            if (Mob_CanScanPoint(mob, &sf->enemyBase.pos)) {
                /*
                 * We can confirm there's no base there any more.
                 */
                sf->enemyBase.type = MOB_TYPE_INVALID;
            }
        }

        /*
         * Assume it's still there if we haven't confirmed destruction.
         */
        if (sf->enemyBase.type == MOB_TYPE_BASE) {
            MobSet_Add(&ai->sensors, &sf->enemyBase);
            sf->enemyBaseAge++;
        }
    }

    Mob *groupTarget = FleetUtil_FindClosestSensor(ai, &sf->basePos,
                                                   targetScanFilter);

    bool doAttack = FALSE;

    if (sf->numGov[BOB_GOV_SCOUT] > 12) {
        doAttack = TRUE;
    }

    MobIt_Start(&ai->mobs, &mit);
    while (MobIt_HasNext(&mit)) {
        Mob *mob = MobIt_Next(&mit);

        if (mob->type == MOB_TYPE_FIGHTER) {
            BobShip *ship = BobFleetGetShip(sf, mob->mobid);
            Mob *target = NULL;

            ASSERT(ship != NULL);
            ASSERT(ship->mobid == mob->mobid);
            ASSERT(ship->gov < BOB_GOV_MAX);

            if (ship->gov == BOB_GOV_SCOUT) {
                /*
                 * Just run the shared random/loot-box code.
                 */
                if (doAttack && sf->numGov[BOB_GOV_SCOUT] > 2) {
                    ship->gov = BOB_GOV_ATTACK;
                    ASSERT(sf->numGov[BOB_GOV_SCOUT] > 0);
                    sf->numGov[BOB_GOV_SCOUT]--;
                    sf->numGov[BOB_GOV_ATTACK]++;
                }
            } else if (ship->gov == BOB_GOV_ATTACK) {
                target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     targetScanFilter);
            } else if (ship->gov == BOB_GOV_GUARD) {
                target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     targetScanFilter);
                if (target != NULL) {
                    if (FPoint_Distance(&target->pos, &sf->basePos) >
                        MobType_GetSensorRadius(MOB_TYPE_BASE)) {
                        target = NULL;
                    }
                }

                target = groupTarget;
                if (target != NULL) {
                    if (FPoint_Distance(&target->pos, &sf->basePos) >
                        MobType_GetSensorRadius(MOB_TYPE_BASE)) {
                        target = NULL;
                    }
                }
            }

            if (target == NULL) {
                /*
                * Avoid having all the fighters rush to the same loot box.
                */
                target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     MOB_FLAG_LOOT_BOX);
                if (target != NULL &&
                    IntMap_Increment(&targetMap, target->mobid) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    target = NULL;
                }

                if (ship->gov == BOB_GOV_GUARD && target != NULL) {
                    if (FPoint_Distance(&target->pos, &sf->basePos) >
                        MobType_GetSensorRadius(MOB_TYPE_BASE)) {
                        target = NULL;
                    }
                }
            }

            {
                Mob *closeTarget = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                               targetScanFilter);
                if (closeTarget != NULL) {
                    if (FPoint_Distance(&mob->pos, &closeTarget->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
                }
            }

            if (target != NULL) {
                mob->cmd.target = target->pos;
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (ship->gov == BOB_GOV_GUARD) {
                    float guardRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
                    mob->cmd.target.x =
                        RandomState_Float(&sf->rs,
                                          MAX(0, sf->basePos.x - guardRadius),
                                          sf->basePos.x + guardRadius);
                    mob->cmd.target.y =
                        RandomState_Float(&sf->rs,
                                          MAX(0, sf->basePos.y - guardRadius),
                                          sf->basePos.y + guardRadius);
                } else {
                    mob->cmd.target.x =
                        RandomState_Float(&sf->rs, 0.0f, bp->width);
                    mob->cmd.target.y =
                        RandomState_Float(&sf->rs, 0.0f, bp->height);
                }
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            Mob *target = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (target != NULL) {
                mob->cmd.target = target->pos;
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 && RandomState_Int(&sf->rs, 0, 10) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            ASSERT(MobType_GetSpeed(MOB_TYPE_BASE) == 0.0f);
        } else if (mob->type == MOB_TYPE_LOOT_BOX) {
            Mob *friend = FleetUtil_FindClosestMob(&sf->ai->mobs, &mob->pos,
                                                   MOB_FLAG_FIGHTER);
            if (friend != NULL) {
                mob->cmd.target = friend->pos;
            } else {
                mob->cmd.target = sf->basePos;
            }

            /*
             * Add this mob to the sensor list so that we'll
             * steer towards it.
             */
            MobSet_Add(&ai->sensors, mob);
        }
    }

    IntMap_Destroy(&targetMap);
}
