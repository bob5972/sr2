/*
 * mapperFleet.c -- part of SpaceRobots2
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
#include "MBVarMap.h"
#include "battle.h"

typedef uint8 MapTileFlags;
#define MAP_TILE_EMPTY       (0)
#define MAP_TILE_SCANNED     (1 << 0)
#define MAP_TILE_POWER_CORE  (1 << 0)
#define MAP_TILE_ENEMY       (1 << 1)
#define MAP_TILE_ENEMY_BASE  (1 << 2)

typedef enum MapperGovernor {
    MAPPER_GOV_INVALID = 0,
    MAPPER_GOV_GUARD   = 1,
    MAPPER_GOV_MIN     = 1,
    MAPPER_GOV_SCOUT   = 2,
    MAPPER_GOV_ATTACK  = 3,
    MAPPER_GOV_MAX,
} MapperGovernor;

typedef struct MapperShip {
    MobID mobid;
    MapperGovernor gov;
    int assignedTile;
} MapperShip;

typedef struct MapperFleetData {
    FleetAI *ai;
    RandomState rs;

    FPoint basePos;
    Mob enemyBase;
    FPoint lastShipLost;
    uint lastShipLostTick;

    bool randomWaves;
    uint waveSizeIncrement;
    uint startingWaveSize;
    uint nextWaveSize;
    uint curWaveSize;

    uint numGuard;

    uint mapTileWidth;
    uint mapTileHeight;
    uint mapWidthInTiles;
    uint mapHeightInTiles;
    uint numTiles;
    uint *tileScanTicks;
    MapTileFlags *tileFlags;
    BitVector tileBV;
} MapperFleetData;

static void *MapperFleetCreate(FleetAI *ai);
static void MapperFleetDestroy(void *aiHandle);
static void MapperFleetRunAITick(void *aiHandle);
static void *MapperFleetMobSpawned(void *aiHandle, Mob *m);
static void MapperFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static MapperShip *MapperFleetGetShip(MapperFleetData *sf, MobID mobid);

void MapperFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "MapperFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &MapperFleetCreate;
    ops->destroyFleet = &MapperFleetDestroy;
    ops->runAITick = &MapperFleetRunAITick;
    ops->mobSpawned = &MapperFleetMobSpawned;
    ops->mobDestroyed = &MapperFleetMobDestroyed;
}

static void *MapperFleetCreate(FleetAI *ai)
{
    const BattleParams *bp = &ai->bp;

    MapperFleetData *sf;
    ASSERT(ai != NULL);

    sf = malloc(sizeof(*sf));
    MBUtil_Zero(sf, sizeof(*sf));
    sf->ai = ai;
    RandomState_CreateWithSeed(&sf->rs, ai->seed);

    sf->startingWaveSize = 5;
    sf->waveSizeIncrement = 0;
    sf->randomWaves = FALSE;
    if (sf->ai->player.mreg != NULL) {
        sf->startingWaveSize =
            MBRegistry_GetIntD(sf->ai->player.mreg, "StartingWaveSize",
                               sf->startingWaveSize);
        sf->waveSizeIncrement =
            MBRegistry_GetIntD(sf->ai->player.mreg, "WaveSizeIncrement",
                               sf->waveSizeIncrement);
        sf->randomWaves =
            MBRegistry_GetIntD(sf->ai->player.mreg, "RandomWaves",
                               sf->randomWaves);
    }
    sf->nextWaveSize = sf->startingWaveSize;

    /*
     * Use quarter-circle sized tiles, so that if the ship is
     * anywhere in the tile, we can count it as having scanned
     * most of it.
     */
    sf->mapTileWidth = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
    sf->mapTileHeight = MobType_GetSensorRadius(MOB_TYPE_FIGHTER);
    sf->mapWidthInTiles = bp->width / sf->mapTileWidth;
    sf->mapHeightInTiles = bp->height / sf->mapTileHeight;
    sf->numTiles = sf->mapWidthInTiles * sf->mapHeightInTiles;

    sf->tileScanTicks = malloc(sf->numTiles * sizeof(sf->tileScanTicks[0]));
    sf->tileFlags = malloc(sf->numTiles * sizeof(sf->tileFlags[0]));

    MBUtil_Zero(sf->tileScanTicks, sf->numTiles * sizeof(sf->tileScanTicks[0]));
    MBUtil_Zero(sf->tileFlags, sf->numTiles * sizeof(sf->tileFlags[0]));

    BitVector_CreateWithSize(&sf->tileBV, sf->numTiles);
    BitVector_ResetAll(&sf->tileBV);

    return sf;
}

static void MapperFleetDestroy(void *aiHandle)
{
    MapperFleetData *sf = aiHandle;
    ASSERT(sf != NULL);

    BitVector_Destroy(&sf->tileBV);
    free(sf->tileScanTicks);
    free(sf->tileFlags);
    RandomState_Destroy(&sf->rs);

    free(sf);
}

static void *MapperFleetMobSpawned(void *aiHandle, Mob *m)
{
    MapperFleetData *sf = aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    if (m->type == MOB_TYPE_FIGHTER) {
        MapperShip *ship;
        ship = MBUtil_ZAlloc(sizeof(*ship));
        ship->mobid = m->mobid;
        ship->assignedTile = -1;

        if (sf->numGuard < 1) {
            ship->gov = MAPPER_GOV_GUARD;
        } else {
            EnumDistribution dist[] = {
                { MAPPER_GOV_SCOUT,  0.50 },
                { MAPPER_GOV_GUARD,  0.50 },
                { MAPPER_GOV_ATTACK, 0.00 },
            };
            ship->gov = RandomState_Enum(&sf->rs, dist, ARRAYSIZE(dist));
        }

        if (ship->gov == MAPPER_GOV_GUARD) {
            sf->numGuard++;
        }

        m->cmd.target = sf->basePos;
        return ship;
    } else {
        /*
         * We don't track anything else.
         */
        return NULL;
    }
}

static void MapperFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    if (aiMobHandle == NULL) {
        return;
    }

    MapperFleetData *sf = aiHandle;
    MapperShip *ship = aiMobHandle;
    ASSERT(sf != NULL);

    if (ship->gov == MAPPER_GOV_GUARD) {
        ASSERT(sf->numGuard > 0);
        sf->numGuard--;
    }

    free(ship);
}


static MapperShip *MapperFleetGetShip(MapperFleetData *sf, MobID mobid)
{
    MapperShip *s = MobPSet_Get(&sf->ai->mobs, mobid)->aiMobHandle;
    ASSERT(s != NULL);
    ASSERT(s->mobid == mobid);
    return s;
}


static void MapperFleetGetTileCoord(MapperFleetData *sf, const FPoint *pos,
                                    uint32 *x, uint32 *y)
{
    ASSERT(sf != NULL);
    ASSERT(pos != NULL);
    ASSERT(x != NULL);
    ASSERT(y != NULL);

    *x = pos->x / sf->mapTileWidth;
    *y = pos->y / sf->mapTileHeight;

    if (*x >= sf->mapWidthInTiles) {
        *x = sf->mapWidthInTiles - 1;
    }
    if (*y >= sf->mapHeightInTiles) {
        *y = sf->mapHeightInTiles - 1;
    }

    ASSERT(*x < sf->mapWidthInTiles);
    ASSERT(*y < sf->mapHeightInTiles);
}

static void MapperFleetGetTileIndex(MapperFleetData *sf, const FPoint *pos,
                                    uint32 *i)
{
    uint32 x, y;

    ASSERT(sf != NULL);
    ASSERT(pos != NULL);
    ASSERT(i != NULL);

    MapperFleetGetTileCoord(sf, pos, &x, &y);
    *i = x + y * sf->mapWidthInTiles;
    ASSERT(*i < sf->numTiles);
}


static void MapperFleetGetPosFromIndex(MapperFleetData *sf, uint32 i, FPoint *pos)
{
    ASSERT(sf != NULL);
    ASSERT(pos != NULL);

    pos->x = (i % sf->mapWidthInTiles) * sf->mapTileWidth;
    pos->y = (i / sf->mapWidthInTiles) * sf->mapTileHeight;
}


static void MapperFleetStartTileSearch(MapperFleetData *sf)
{
    BitVector_ResetAll(&sf->tileBV);
}

static uint32 MapperFleetGetNextTile(MapperFleetData *sf, uint tileFilter)
{
    uint32 offset = RandomState_Int(&sf->rs, 0, sf->numTiles - 1);
    uint32 bestIndex = offset;
    uint32 i;


    i = bestIndex;
    ASSERT(i < sf->numTiles);
    if (!BitVector_Get(&sf->tileBV, i) && sf->tileFlags[i] == MAP_TILE_EMPTY) {
        goto found;
    }


    i = 0;
    while (i < sf->numTiles) {
        uint32 t = (i + offset) % sf->numTiles;
        if (BitVector_Get(&sf->tileBV, t)) {
            // Someone else already claimed this tile...
        } else {
            if (!BitVector_Get(&sf->tileBV, t)) {
                if (sf->tileFlags[t] == MAP_TILE_EMPTY ||
                    (sf->tileFlags[t] & tileFilter) != 0) {
                    bestIndex = t;
                    goto found;
                }
            }
            if (sf->tileScanTicks[t] < sf->tileScanTicks[bestIndex]) {
                bestIndex = t;
            }
        }

        i++;
    }

    if (BitVector_Get(&sf->tileBV, bestIndex)) {
        /*
         * We presumably had everything already assigned.
         */
        BitVector_ResetAll(&sf->tileBV);
    }

found:
    BitVector_Set(&sf->tileBV, bestIndex);
    return bestIndex;
}


static void MapperFleetRunAITick(void *aiHandle)
{
    MapperFleetData *sf = aiHandle;
    FleetAI *ai = sf->ai;
    uint targetScanFilter = MOB_FLAG_SHIP;
    CMBIntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE);
    float baseScanRange = MobType_GetSensorRadius(MOB_TYPE_BASE);

    CMBIntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_MAPPER);

    /*
     * Analyze our sensor data.
     */
    CMobIt mit;
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);

        if (mob->type == MOB_TYPE_FIGHTER) {
            uint32 i;
            MapperFleetGetTileIndex(sf, &mob->pos, &i);
            ASSERT(i < sf->numTiles);
            sf->tileScanTicks[i] = sf->ai->tick;
            sf->tileFlags[i] = MAP_TILE_SCANNED;
        } else if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;
        } else if (mob->type == MOB_TYPE_POWER_CORE) {
            /*
             * Add this mob to the sensor list so that we'll
             * steer towards it.
             */
            MobPSet_Add(&ai->sensors, mob);
        }
    }

    CMobIt_Start(&ai->sensors, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *sm = CMobIt_Next(&mit);
        uint32 i;
        MapTileFlags f = MAP_TILE_SCANNED;

        if (sm->type == MOB_TYPE_BASE) {
            f |= MAP_TILE_ENEMY;
            f |= MAP_TILE_ENEMY_BASE;
            sf->enemyBase = *sm;
        } else if (sm->type == MOB_TYPE_FIGHTER ||
                   sm->type == MOB_TYPE_MISSILE) {
            f |= MAP_TILE_ENEMY;
        } else {
            ASSERT(sm->type == MOB_TYPE_POWER_CORE);
            f |= MAP_TILE_POWER_CORE;
        }

        MapperFleetGetTileIndex(sf, &sm->pos, &i);
        ASSERT(i < sf->numTiles);
        sf->tileFlags[i] |= f;
    }

    /*
     * Add the last seen enemy base to valid targets.
     */
    if (sf->enemyBase.type == MOB_TYPE_BASE) {
        uint32 tileIndex;
        MapperFleetGetTileIndex(sf, &sf->enemyBase.pos, &tileIndex);

        ASSERT(tileIndex < sf->numTiles);
        if ((sf->tileFlags[tileIndex] & MAP_TILE_ENEMY_BASE) == 0) {
            sf->enemyBase.type = MOB_TYPE_INVALID;
        } else {
            MobPSet_Add(&ai->sensors, &sf->enemyBase);
        }
    }

    /*
     * Assign tiles to fighters.
     */
    {
        if (sf->numGuard >= sf->nextWaveSize * 2) {
            sf->curWaveSize = sf->nextWaveSize;
            sf->nextWaveSize += sf->waveSizeIncrement;
        }

        MapperFleetStartTileSearch(sf);
        CMobIt mit;
        CMobIt_Start(&ai->mobs, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *mob = CMobIt_Next(&mit);
            if (mob->type == MOB_TYPE_FIGHTER) {
                MapperShip *s = MapperFleetGetShip(sf, mob->mobid);
                ASSERT(s != NULL);
                ASSERT(s->mobid == mob->mobid);

                if (s->gov == MAPPER_GOV_GUARD) {
                    if (sf->curWaveSize > 0) {
                        if (!sf->randomWaves || RandomState_Bit(&sf->rs)) {
                            ASSERT(sf->numGuard > 0);
                            sf->numGuard--;
                            sf->curWaveSize--;
                            s->gov = MAPPER_GOV_ATTACK;
                        }
                    }
                }

                if (s->assignedTile == -1) {
                    uint tileFilter = 0;
                    if (s->gov == MAPPER_GOV_SCOUT) {
                        tileFilter = MAP_TILE_POWER_CORE;
                    } else if (s->gov == MAPPER_GOV_ATTACK) {
                        tileFilter = MAP_TILE_ENEMY | MAP_TILE_ENEMY_BASE;
                    }

                    if (tileFilter != 0) {
                        uint32 tileIndex = MapperFleetGetNextTile(sf, tileFilter);
                        s->assignedTile = tileIndex;
                    }
                }
            }
        }
    }

    /*
     * Main Mob processing loop.
     */
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);

        if (mob->type == MOB_TYPE_FIGHTER) {
            MapperShip *s = MapperFleetGetShip(sf, mob->mobid);
            Mob *target = NULL;

            ASSERT(s != NULL);
            ASSERT(s->mobid == mob->mobid);

            if (!mob->alive) {
                sf->lastShipLost = mob->pos;
                sf->lastShipLostTick = sf->ai->tick;
            }

            if (s->gov == MAPPER_GOV_SCOUT) {
                /*
                 * Just run the shared random/power-core code.
                 */
            } else if (s->gov == MAPPER_GOV_ATTACK) {
                target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     targetScanFilter);
            } else if (s->gov == MAPPER_GOV_GUARD) {
                target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     targetScanFilter);
                if (target != NULL) {
                    if (FPoint_Distance(&target->pos, &sf->basePos) > guardRange) {
                        target = NULL;
                    }
                }
            }

            if (target == NULL) {
                /*
                * Avoid having all the fighters rush to the same power core.
                */
                target = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     MOB_FLAG_POWER_CORE);
                if (target != NULL) {
                    if (s->gov == MAPPER_GOV_GUARD) {
                        if (FPoint_Distance(&target->pos, &sf->basePos) > guardRange) {
                            target = NULL;
                        }
                    } else if (s->gov == MAPPER_GOV_SCOUT) {
                        if (FPoint_Distance(&target->pos, &mob->pos) > firingRange) {
                            target =  NULL;
                        }
                    }
                }

                if (target != NULL &&
                    CMBIntMap_Increment(&targetMap, target->mobid) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    target = NULL;
                }
            }

            {
                Mob *ctMob = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                         targetScanFilter);
                if (ctMob != NULL) {
                    if (FPoint_Distance(&mob->pos, &ctMob->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                    }
                }
            }

            if (target != NULL) {
                mob->cmd.target = target->pos;
            } else if (s->assignedTile != -1) {
                MapperFleetGetPosFromIndex(sf, s->assignedTile,
                                           &mob->cmd.target);
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                float moveRadius;
                FPoint moveCenter;

                s->assignedTile = -1;

                switch(s->gov) {
                    case MAPPER_GOV_GUARD:
                        moveRadius = guardRange;
                        moveCenter = sf->basePos;
                        break;
                    case MAPPER_GOV_SCOUT:
                        moveRadius = MobType_GetSensorRadius(MOB_TYPE_BASE);
                        moveCenter = mob->pos;
                        break;
                    case MAPPER_GOV_ATTACK:
                        moveRadius = firingRange;
                        if (sf->ai->tick - sf->lastShipLostTick < 1000) {
                            moveRadius *= 3.0f;
                            moveCenter = sf->lastShipLost;
                        } else if (sf->enemyBase.type == MOB_TYPE_BASE) {
                            moveCenter = sf->enemyBase.pos;
                        } else {
                            moveCenter = mob->pos;
                        }
                        break;
                    default:
                        NOT_REACHED();
                }

                FleetUtil_RandomPointInRange(&sf->rs, &mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = MOB_FLAG_SHIP;
            Mob *target = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (target != NULL) {
                mob->cmd.target = target->pos;
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            if (ai->credits > 200 && RandomState_Int(&sf->rs, 0, 20) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            ASSERT(MobType_GetSpeed(MOB_TYPE_BASE) == 0.0f);
        } else if (mob->type == MOB_TYPE_POWER_CORE) {
            if (FPoint_Distance(&mob->pos, &sf->basePos) <= baseScanRange) {
                mob->cmd.target = sf->basePos;
            } else {
                Mob *friend = FleetUtil_FindClosestMob(&sf->ai->mobs, &mob->pos,
                                                       MOB_FLAG_FIGHTER);
                if (friend != NULL) {
                    mob->cmd.target = friend->pos;
                } else {
                    mob->cmd.target = sf->basePos;
                }
            }
        }
    }

    CMBIntMap_Destroy(&targetMap);
}
