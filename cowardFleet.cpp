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

typedef struct CowardShip {
    MobID mobid;
} CowardShip;

typedef struct CowardTarget {
    Mob mob;
    uint seenTick;
} CowardTarget;

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
    MBVector<CowardTarget> tvec;
};

static void *CowardFleetCreate(FleetAI *ai);
static void CowardFleetDestroy(void *aiHandle);
static void CowardFleetRunAITick(void *aiHandle);
static void *CowardFleetMobSpawned(void *aiHandle, Mob *m);
static void CowardFleetMobDestroyed(void *aiHandle, void *aiMobHandle);
static CowardShip *CowardFleetGetShip(CowardFleet *sf, MobID mobid);

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

static void CowardFleetUpdateTarget(CowardFleet *sf, Mob *m)
{
    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    for (uint i = 0; i < sf->tvec.size(); i++) {
        CowardTarget *cur = &sf->tvec[i];
        if (cur->mob.mobid == m->mobid) {
            cur->mob = *m;
            cur->seenTick = sf->ai->tick;
            return;
        }
    }
}

static void CowardFleetAddTarget(CowardFleet *sf, Mob *m)
{
    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    for (uint i = 0; i < sf->tvec.size(); i++) {
        CowardTarget *cur = &sf->tvec[i];
        if (cur->mob.mobid == m->mobid) {
            /*
             * If it's already here, don't re-add it.
             * We already processed new scan data in UpdateTarget.
             */
            return;
        }
    }

    sf->tvec.grow();
    uint index = sf->tvec.size() - 1;
    CowardTarget *t = &sf->tvec[index];
    t->mob = *m;
    t->seenTick = sf->ai->tick;
}

static void CowardFleetCleanTargets(CowardFleet *sf)
{
    ASSERT(sf != NULL);

    uint i = 0;

    while (i < sf->tvec.size()) {
        CowardTarget *cur = &sf->tvec[i];

        ASSERT(sf->ai->tick >= cur->seenTick);
        if (sf->ai->tick - cur->seenTick > 2) {
            uint lastIndex = sf->tvec.size() - 1;
            CowardTarget *last = &sf->tvec[lastIndex];
            *cur = *last;
            sf->tvec.shrink();
        } else {
            i++;
        }
    }
}

static void *CowardFleetMobSpawned(void *aiHandle, Mob *m)
{
    CowardFleet *sf = (CowardFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        CowardShip *ship;
        ship = (CowardShip *)MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;
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
static void CowardFleetMobDestroyed(void *aiHandle, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    CowardFleet *sf = (CowardFleet *)aiHandle;
    CowardShip *ship = (CowardShip *)aiMobHandle;

    ASSERT(sf != NULL);
    free(ship);
}

static CowardShip *CowardFleetGetShip(CowardFleet *sf, MobID mobid)
{
    CowardShip *s = (CowardShip *)MobPSet_Get(&sf->ai->mobs, mobid)->aiMobHandle;

    ASSERT(s != NULL);
    ASSERT(s->mobid == mobid);
    return s;
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

    CMobIt_Start(&ai->sensors, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *m = CMobIt_Next(&mit);
        CowardFleetUpdateTarget(sf, m);
    }

    uint minVecSize = sf->tvec.size() + MobPSet_Size(&ai->sensors);
    sf->tvec.ensureCapacity(minVecSize);
    sf->tvec.pin();

    for (uint i = 0; i < sf->tvec.size(); i++) {
        CowardTarget *t = &sf->tvec[i];

        /*
         * Add any targets found in the last round that have since
         * moved out of scanning range, and assume they're still there.
         *
         * Since we probably just ran away, this gives the missiles we
         * just shot a place to aim.
         */
        if (MobPSet_Get(&ai->sensors, t->mob.mobid) == NULL) {
            MobPSet_Add(&ai->sensors, &t->mob);
        }
    }

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

        CowardShip *ship = CowardFleetGetShip(sf, mob->mobid);
        Mob *lootTarget = NULL;
        Mob *enemyTarget = NULL;

        ASSERT(ship != NULL);
        ASSERT(ship->mobid == mob->mobid);

        /*
         * Find loot.
         */
        lootTarget = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                 MOB_FLAG_LOOT_BOX);

        if (lootTarget != NULL) {
            if (FPoint_Distance(&mob->pos, &lootTarget->pos) > scanningRange) {
                lootTarget = NULL;
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
                CowardFleetAddTarget(sf, enemyTarget);

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
        } else if (lootTarget != NULL) {
            mob->cmd.target = lootTarget->pos;
        } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
            mob->cmd.target.x = RandomState_Float(&sf->rs, 0.0f, bp->width);
            mob->cmd.target.y = RandomState_Float(&sf->rs, 0.0f, bp->height);
        }
    }

    /*
     * Clear out the old targets.
     */
    sf->tvec.unpin();
    CowardFleetCleanTargets(sf);
}
