/*
 * cloudFleet.c -- part of SpaceRobots2
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

#include "fleet.h"
#include "Random.h"
#include "IntMap.h"
#include "battle.h"

typedef struct Contact {
    MobType type;
    FPoint pos;
    uint tick;
} Contact;

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
    uint lostShipTick;
    uint numGuards;
    uint numScouts;
    MobPVec fighters;
    MobPVec targets;
    CMBVector contacts;
    RandomState rs;
} GatherFleetData;

static void *GatherFleetCreate(FleetAI *ai);
static void GatherFleetDestroy(void *aiHandle);
static void GatherFleetRunAITick(void *aiHandle);
static void *GatherFleetMobSpawned(void *aiHandle, Mob *m);
static void GatherFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static GatherShip *GatherFleetGetShip(GatherFleetData *sf, MobID mobid);

void GatherFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
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
    RandomState_CreateWithSeed(&sf->rs, ai->seed);
    MobPVec_CreateEmpty(&sf->fighters);
    MobPVec_CreateEmpty(&sf->targets);
    CMBVector_CreateEmpty(&sf->contacts, sizeof(Contact));

    return sf;
}

static void GatherFleetDestroy(void *aiHandle)
{
    GatherFleetData *sf = aiHandle;
    ASSERT(sf != NULL);
    MobPVec_Destroy(&sf->fighters);
    MobPVec_Destroy(&sf->targets);
    CMBVector_Destroy(&sf->contacts);
    RandomState_Destroy(&sf->rs);
    free(sf);
}

static void *GatherFleetMobSpawned(void *aiHandle, Mob *m)
{
    GatherFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);
    ASSERT(Mob_CheckInvariants(m));

    if (m->type == MOB_TYPE_FIGHTER) {
        GatherShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;

        if (sf->ai->tick - sf->lostShipTick < 200) {
            ship->gov = GATHER_GOV_GUARD;
        } else if (sf->numGuards < 1) {
            ship->gov = GATHER_GOV_GUARD;
        } else if (sf->numScouts < 1) {
            ship->gov = GATHER_GOV_SCOUT;
        } else {
            ship->gov = RandomState_Int(&sf->rs, GATHER_GOV_MIN,
                                        GATHER_GOV_MAX - 1);
        }

        if (ship->gov == GATHER_GOV_GUARD) {
            sf->numGuards++;
        } else {
            ASSERT(ship->gov == GATHER_GOV_SCOUT);
            sf->numScouts++;
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
static void GatherFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    GatherFleetData *sf = aiHandle;
    GatherShip *ship = aiMobHandle;
    ASSERT(sf != NULL);

    if (ship->gov == GATHER_GOV_GUARD) {
        ASSERT(sf->numGuards > 0);
        sf->numGuards--;
        sf->lostShipTick = sf->ai->tick;
    } else {
        ASSERT(ship->gov == GATHER_GOV_SCOUT);
        ASSERT(sf->numScouts > 0);
        sf->numScouts--;
    }

    free(ship);
}

static GatherShip *GatherFleetGetShip(GatherFleetData *sf, MobID mobid)
{
    GatherShip *s = MobPSet_Get(&sf->ai->mobs, mobid)->aiMobHandle;
    ASSERT(s != NULL);
    ASSERT(s->mobid == mobid);
    return s;
}

static void GatherFleetAgeContacts(GatherFleetData *sf)
{
    for (uint i = 0; i < CMBVector_Size(&sf->contacts); i++) {
        uint ageLimit;

        Contact *c = CMBVector_GetPtr(&sf->contacts, i);
        switch (c->type) {
            case MOB_TYPE_BASE:
                ageLimit = 1000;
                break;
            case MOB_TYPE_FIGHTER:
                ageLimit = 500;
                break;
            case MOB_TYPE_MISSILE:
                ageLimit = 100;
                break;
            default:
                NOT_REACHED();
        }

        if (sf->ai->tick - c->tick > ageLimit) {
            Contact *last = CMBVector_GetLastPtr(&sf->contacts);
            *c = *last;
            i--;
            CMBVector_Shrink(&sf->contacts);
        }
    }
}

static void GatherFleetAddContact(GatherFleetData *sf, Mob *sm)
{
    if (sm->type == MOB_TYPE_POWER_CORE) {
        return;
    }

    for (uint i = 0; i < CMBVector_Size(&sf->contacts); i++) {
        Contact *c = CMBVector_GetPtr(&sf->contacts, i);
        if (FPoint_Distance(&c->pos, &sm->pos) < MobType_GetSensorRadius(c->type)) {
            if (MobType_GetSensorRadius(c->type) < MobType_GetSensorRadius(sm->type)) {
                c->type = sm->type;
            }
            c->tick = sf->ai->tick;
            return;
        }
    }

    CMBVector_Grow(&sf->contacts);
    Contact *c = CMBVector_GetLastPtr(&sf->contacts);
    MBUtil_Zero(c, sizeof(*c));
    c->type = sm->type;
    c->pos = sm->pos;
    c->tick = sf->ai->tick;
}

static bool GatherFleetInConflictZone(GatherFleetData *sf, const FPoint *pos,
                                      const FPoint *target)
{
    for (uint i = 0; i < CMBVector_Size(&sf->contacts); i++) {
        Contact *c = CMBVector_GetPtr(&sf->contacts, i);
        if (FPoint_Distance(&c->pos, target) < MobType_GetSensorRadius(c->type)) {
            return TRUE;
        }
    }

    if (FPoint_Distance(target, pos) > MobType_GetSensorRadius(MOB_TYPE_BASE)) {
        FPoint m;
        FPoint_Midpoint(&m, pos, target);
        return GatherFleetInConflictZone(sf, pos, &m) ||
               GatherFleetInConflictZone(sf, &m, target);
    }

    return FALSE;
}

static float GatherFleetBaseDistance(GatherFleetData *sf, const FPoint *pos)
{
    return FPoint_Distance(pos, &sf->basePos);
}

static void GatherFleetRunAITick(void *aiHandle)
{
    GatherFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    const BattleParams *bp = &sf->ai->bp;
    CIntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE) *
                       (1.0f + sf->numGuards / 10.0f + sf->numScouts / 20.0f);
//     float fighterScanRange = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
    float baseScanRange = MobType_GetSensorRadius(MOB_TYPE_BASE);
    float scoutActivationRange = baseScanRange;
    CMobIt mit;

    ASSERT(ai->player.aiType == FLEET_AI_GATHER);
    CIntMap_Create(&targetMap);

    MobPVec_MakeEmpty(&sf->fighters);
    MobPVec_MakeEmpty(&sf->targets);

    /*
     * Initialize mob state
     */
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);
        ASSERT(Mob_CheckInvariants(mob));

        if (mob->type == MOB_TYPE_FIGHTER) {
            MobPVec_Grow(&sf->fighters);
            *MobPVec_GetLastPtr(&sf->fighters) = mob;
        } else if (mob->type == MOB_TYPE_POWER_CORE) {
            MobPVec_Grow(&sf->targets);
            *MobPVec_GetLastPtr(&sf->targets) = mob;
        }
    }

    GatherFleetAgeContacts(sf);

    /*
     * Initialize target state
     */
    CMobIt_Start(&ai->sensors, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *sm = CMobIt_Next(&mit);
        ASSERT(Mob_CheckInvariants(sm));

        if (sm->type != MOB_TYPE_MISSILE) {
            MobPVec_Grow(&sf->targets);
            *MobPVec_GetLastPtr(&sf->targets) = sm;
        }

        GatherFleetAddContact(sf, sm);
    }

    /*
     * Main Mob processing loop.
     */
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);

        if (mob->type == MOB_TYPE_FIGHTER) {
            GatherShip *ship = mob->aiMobHandle;
            Mob *tMob = NULL;
            Mob *ctMob = NULL;

            ASSERT(ship != NULL);
            ASSERT(GatherFleetGetShip(sf, mob->mobid) == ship);

            ctMob = FleetUtil_FindClosestSensor(ai, &mob->pos, MOB_FLAG_SHIP);
            tMob = ctMob;
            if (tMob != NULL) {
                if (ship->gov == GATHER_GOV_SCOUT &&
                    FPoint_Distance(&tMob->pos, &mob->pos) > firingRange) {
                    tMob = NULL;
                }
                if (ship->gov == GATHER_GOV_GUARD &&
                    FPoint_Distance(&tMob->pos, &sf->basePos) > guardRange) {
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

                    if (ship->gov == GATHER_GOV_SCOUT) {
                        if (FPoint_Distance(&tMob->pos, &mob->pos) >
                            scoutActivationRange) {
                            tMob = NULL;
                        }
                    } else {
                        ASSERT(ship->gov == GATHER_GOV_GUARD);
                        if (FPoint_Distance(&tMob->pos, &sf->basePos) >
                            guardRange) {
                            tMob = NULL;
                        }
                    }

                    if (tMob != NULL) {
                        t = FleetUtil_FindNthClosestMobP(&sf->fighters,
                                                         &tMob->pos, 0);
                        if (t != -1) {
                            Mob *f = MobPVec_GetValue(&sf->fighters, t);
                            if (f->mobid == mob->mobid) {
                                // This is the closest mob.... claim it anyway.
                                forceClaim = TRUE;
                            }
                        }
                    }

                    if (tMob != NULL) {
                        uint claimLimit = 1;
                        if (ship->gov == GATHER_GOV_SCOUT) {
                            claimLimit = 1 + (sf->numScouts / 4);
                        }
                        if (CIntMap_Increment(&targetMap, tMob->mobid) <= claimLimit ||
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
                if (ctMob != NULL) {
                    if (FPoint_Distance(&mob->pos, &ctMob->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
                }
            }

            if (tMob != NULL) {
                mob->cmd.target = tMob->pos;
            } else if ((ship->gov == GATHER_GOV_SCOUT &&
                        GatherFleetInConflictZone(sf, &mob->pos, &mob->cmd.target)) ||
                       FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                if (ship->gov == GATHER_GOV_SCOUT) {
                    int i = 0;
                    bool conflict;
                    do {
                        mob->cmd.target.x =
                            RandomState_Float(&sf->rs, 0.0f, bp->width);
                        mob->cmd.target.y =
                            RandomState_Float(&sf->rs, 0.0f, bp->height);
                        i++;
                        conflict = GatherFleetInConflictZone(sf, &mob->pos,
                                                             &mob->cmd.target);
                    } while (i < 10 && conflict);
                    if (i == 10) {
                        /*
                         * If we couldn't find somewhere conflict-free,
                         * become a guard.
                         */
                        ship->gov = GATHER_GOV_GUARD;
                        sf->numGuards++;
                        ASSERT(sf->numScouts > 0);
                        sf->numScouts--;
                    }
                }
                if (ship->gov == GATHER_GOV_GUARD) {
                    float moveRadius = guardRange;
                    FPoint moveCenter = sf->basePos;
                    int i = 0;
                    bool tooClose;
                    do {
                        FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                                     &moveCenter, moveRadius);
                        i++;
                        tooClose = FALSE;
                        if (GatherFleetBaseDistance(sf, &mob->cmd.target) <
                            guardRange / 2.0f) {
                            tooClose = TRUE;
                        }
                    } while (i < 2 && tooClose);
                }
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            Mob *target = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (target != NULL) {
                mob->cmd.target = target->pos;
            } else if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                float moveRadius = firingRange;
                FPoint moveCenter = mob->pos;
                FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;

            if (ai->credits > 200 && RandomState_Int(&sf->rs, 0, 20) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                mob->cmd.target.x =
                    RandomState_Float(&sf->rs, 0.0f, bp->width);
                mob->cmd.target.y =
                    RandomState_Float(&sf->rs, 0.0f, bp->height);
            }
        } else if (mob->type == MOB_TYPE_POWER_CORE) {
            if (FPoint_Distance(&mob->pos, &sf->basePos) <= baseScanRange) {
                mob->cmd.target = sf->basePos;
            } else {
                int t = FleetUtil_FindNthClosestMobP(&sf->fighters, &mob->pos, 0);
                if (t != -1) {
                    Mob *f = MobPVec_GetValue(&sf->fighters, t);
                    mob->cmd.target = f->pos;
                } else {
                    mob->cmd.target = sf->basePos;
                }
            }
        }
    }

    CIntMap_Destroy(&targetMap);
}
