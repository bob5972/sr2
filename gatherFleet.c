/*
 * cloudFleet.c -- part of SpaceRobots2
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

typedef enum GatherGovernor {
    GATHER_GOV_INVALID = 0,
    GATHER_GOV_GUARD = 1,
    GATHER_GOV_MIN   = 1,
    GATHER_GOV_SCOUT = 2,
    GATHER_GOV_MAX,
} GatherGovernor;

typedef struct GatherShip {
    MobID mobid;
    GatherGovernor gov;
    bool initialized;
} GatherShip;

typedef struct GatherFleetData {
    FleetAI *ai;
    FPoint basePos;
    uint numGuard;
    uint numScout;
    MobPVec fighters;
    MobPVec targets;
} GatherFleetData;

static void *GatherFleetCreate(FleetAI *ai);
static void GatherFleetDestroy(void *aiHandle);
static void GatherFleetRunAITick(void *aiHandle);
static void *GatherFleetMobSpawned(void *aiHandle, Mob *m);
static void GatherFleetMobDestroyed(void *aiHandle, void *aiMobHandle);
static GatherShip *GatherFleetGetShip(GatherFleetData *sf, MobID mobid);

void GatherFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "GatherFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &GatherFleetCreate;
    ops->destroyFleet = &GatherFleetDestroy;
    ops->runAITick = &GatherFleetRunAITick;
    ops->mobSpawned = &GatherFleetMobSpawned;
    ops->mobDestroyed = &GatherFleetMobDestroyed;
}

static void *GatherFleetCreate(FleetAI *ai)
{
    GatherFleetData *sf;
    ASSERT(ai != NULL);

    sf = MBUtil_ZAlloc(sizeof(*sf));
    sf->ai = ai;
    MobPVec_CreateEmpty(&sf->fighters);
    MobPVec_CreateEmpty(&sf->targets);

    return sf;
}

static void GatherFleetDestroy(void *aiHandle)
{
    GatherFleetData *sf = aiHandle;
    ASSERT(sf != NULL);
    MobPVec_Destroy(&sf->fighters);
    MobPVec_Destroy(&sf->targets);
    free(sf);
}

static void *GatherFleetMobSpawned(void *aiHandle, Mob *m)
{
    GatherFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        GatherShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;

        if (sf->numGuard < 1) {
            ship->gov = GATHER_GOV_GUARD;
        } else if (sf->numScout < 1) {
            ship->gov = GATHER_GOV_SCOUT;
        } else if (sf->numGuard > sf->numScout) {
            ship->gov = GATHER_GOV_SCOUT;
        } else {
            ship->gov = Random_Int(GATHER_GOV_MIN, GATHER_GOV_MAX - 1);
        }

        if (ship->gov == GATHER_GOV_GUARD) {
            sf->numGuard++;
        } else {
            ASSERT(ship->gov == GATHER_GOV_SCOUT);
            sf->numScout++;
        }

        m->cmd.target = sf->basePos;
        ship->initialized = TRUE;
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
static void GatherFleetMobDestroyed(void *aiHandle, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    GatherFleetData *sf = aiHandle;
    GatherShip *ship = aiMobHandle;

    ASSERT(sf != NULL);

    if (ship->gov == GATHER_GOV_GUARD) {
        ASSERT(sf->numGuard > 0);
        sf->numGuard--;
    } else {
        ASSERT(ship->gov == GATHER_GOV_SCOUT);
        ASSERT(sf->numScout > 0);
        sf->numScout--;
    }

    free(ship);
}

static GatherShip *GatherFleetGetShip(GatherFleetData *sf, MobID mobid)
{
    GatherShip *s = FleetUtil_GetMob(sf->ai, mobid)->aiMobHandle;

    if (s != NULL) {
        ASSERT(s->mobid == mobid);
    }

    return s;
}

static void GatherFleetRunAITick(void *aiHandle)
{
    GatherFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = Battle_GetParams();
    IntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE) *
                       (1.0f + sf->numGuard / 10.0f + sf->numScout / 20.0f);
//     float fighterScanRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
//     float baseScanRange = MobType_GetSensorRadius(MOB_TYPE_BASE);

    ASSERT(ai->player.aiType == FLEET_AI_GATHER);
    IntMap_Create(&targetMap);

    FleetUtil_SortMobsByDistance(&ai->mobs, &sf->basePos);
    FleetUtil_UpdateMobMap(ai);

    FleetUtil_SortMobsByDistance(&ai->sensors, &sf->basePos);

    MobPVec_MakeEmpty(&sf->fighters);
    MobPVec_MakeEmpty(&sf->targets);

    /*
     * Initialize mob state
     */
    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            MobPVec_Grow(&sf->fighters);
            *MobPVec_GetLastPtr(&sf->fighters) = mob;
        } else if (mob->type == MOB_TYPE_LOOT_BOX) {
            MobPVec_Grow(&sf->targets);
            *MobPVec_GetLastPtr(&sf->targets) = mob;
        }
    }

    /*
     * Initialize target state
     */
    for (uint32 si = 0; si < MobVector_Size(&ai->sensors); si++) {
        Mob *sm = MobVector_GetPtr(&ai->sensors, si);

        if (sm->type != MOB_TYPE_MISSILE) {
            MobPVec_Grow(&sf->targets);
            *MobPVec_GetLastPtr(&sf->targets) = sm;
        }
    }

    /*
     * Main Mob processing loop.
     */
    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER) {
            GatherShip *ship = mob->aiMobHandle;
            Mob *tMob = NULL;
            int ct;

            ASSERT(ship != NULL);
            ASSERT(GatherFleetGetShip(sf, mob->mobid) == ship);

            ct = FleetUtil_FindClosestSensor(ai, &mob->pos, MOB_FILTER_SHIP);
            if (ct != -1) {
                tMob = MobVector_GetPtr(&ai->sensors, ct);
                if (ship->gov == GATHER_GOV_SCOUT &&
                    FPoint_Distance(&tMob->pos, &mob->pos) > firingRange) {
                    tMob = NULL;
                }
            }

            if (tMob == NULL) {
                int n = 0;
                while (tMob == NULL && n < MobPVec_Size(&sf->targets)) {
                    bool forceClaim = FALSE;
                    int t = FleetUtil_FindNthClosestMobP(&sf->targets,
                                                         &mob->pos, n);
                    ASSERT(t != -1);

                    tMob = MobPVec_GetValue(&sf->targets, t);

                    if (tMob->type != MOB_TYPE_LOOT_BOX &&
                        ship->gov == GATHER_GOV_SCOUT &&
                        FPoint_Distance(&tMob->pos, &mob->pos) > firingRange) {
                        tMob = NULL;
                    }

                    if (tMob != NULL) {
                        t = FleetUtil_FindNthClosestMobP(&sf->fighters,
                                                         &tMob->pos, 0);
                        if (t != -1) {
                            Mob *f = MobPVec_GetValue(&sf->fighters, t);
                            if (f->mobid == mob->mobid) {
                                // This is the cloest mob.... claim it anyway.
                                forceClaim = TRUE;
                            }
                        }

                        if (IntMap_Increment(&targetMap, tMob->mobid) == 1 ||
                            forceClaim) {
                            // Claim the target so nobody else will go there.
                        } else {
                            /*
                            * Otherwise try again.
                            */
                            tMob = NULL;
                        }
                    }
                    n++;
                }
            }

            {
                if (ct != -1) {
                    Mob *sm;
                    sm = MobVector_GetPtr(&ai->sensors, ct);

                    if (FPoint_Distance(&mob->pos, &sm->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
                }
            }

            if (tMob != NULL) {
                float moveRadius = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);

                if (tMob->type != MOB_TYPE_LOOT_BOX) {
                    FleetUtil_RandomPointInRange(&mob->cmd.target,
                                                 &tMob->pos, moveRadius);
                } else {
                    mob->cmd.target = tMob->pos;
                }
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (ship->gov == GATHER_GOV_GUARD) {
                    float moveRadius = guardRange;
                    FPoint moveCenter = sf->basePos;
                    FleetUtil_RandomPointInRange(&mob->cmd.target,
                                                 &moveCenter, moveRadius);
                } else {
                    mob->cmd.target.x = Random_Float(0.0f, bp->width);
                    mob->cmd.target.y = Random_Float(0.0f, bp->height);
                }
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = FLEET_SCAN_SHIP | FLEET_SCAN_MISSILE;
            int s = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (s != -1) {
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, s);
                mob->cmd.target = sm->pos;
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                float moveRadius = firingRange;
                FPoint moveCenter = mob->pos;
                FleetUtil_RandomPointInRange(&mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 && Random_Int(0, 20) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                mob->cmd.target.x = Random_Float(0.0f, bp->width);
                mob->cmd.target.y = Random_Float(0.0f, bp->height);
            }
        } else if (mob->type == MOB_TYPE_LOOT_BOX) {
            int t = FleetUtil_FindNthClosestMobP(&sf->fighters, &mob->pos, 0);
            if (t != -1) {
                Mob *f = MobPVec_GetValue(&sf->fighters, t);
                mob->cmd.target = f->pos;
            } else {
                mob->cmd.target = sf->basePos;
            }
        }
    }

    IntMap_Destroy(&targetMap);
}
