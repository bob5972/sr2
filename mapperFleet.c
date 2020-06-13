/*
 * mapperFleet.c -- part of SpaceRobots2
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

typedef uint8 MapTileFlags;
#define MAP_TILE_EMPTY       (0)
#define MAP_TILE_SCANNED     (1 << 0)
#define MAP_TILE_LOOT        (1 << 0)
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

typedef struct MapperShipData {
    MobID mobid;
    MapperGovernor gov;
    int assignedTile;
    uint lastFiredTick;
} MapperShipData;

DECLARE_MBVECTOR_TYPE(MapperShipData, ShipVector);

typedef struct MapperFleetData {
    FPoint basePos;
    Mob enemyBase;
    FPoint lastShipLost;
    uint lastShipLostTick;

    uint tick;
    uint spawnCount;
    uint numGuard;

    uint mapTileWidth;
    uint mapTileHeight;
    uint mapWidthInTiles;
    uint mapHeightInTiles;
    uint numTiles;
    uint *tileScanTicks;
    MapTileFlags *tileFlags;
    BitVector tileBV;

    ShipVector ships;
    IntMap shipMap;
} MapperFleetData;

static void MapperFleetCreate(FleetAI *ai);
static void MapperFleetDestroy(FleetAI *ai);
static void MapperFleetRunAI(FleetAI *ai);
static MapperShipData *MapperFleetGetShip(MapperFleetData *sf, MobID mobid);
static void MapperFleetDestroyShip(MapperFleetData *sf, MobID mobid);

void MapperFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "MapperFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &MapperFleetCreate;
    ops->destroyFleet = &MapperFleetDestroy;
    ops->runAITick = &MapperFleetRunAI;
}

static void MapperFleetCreate(FleetAI *ai)
{
    const BattleParams *bp = Battle_GetParams();

    MapperFleetData *sf;
    ASSERT(ai != NULL);

    sf = malloc(sizeof(*sf));
    MBUtil_Zero(sf, sizeof(*sf));
    ai->aiHandle = sf;

    ShipVector_CreateEmpty(&sf->ships);
    IntMap_Create(&sf->shipMap);
    IntMap_SetEmptyValue(&sf->shipMap, MOB_ID_INVALID);

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
}

static void MapperFleetDestroy(FleetAI *ai)
{
    MapperFleetData *sf;
    ASSERT(ai != NULL);

    sf = ai->aiHandle;
    ASSERT(sf != NULL);

    BitVector_Destroy(&sf->tileBV);
    free(sf->tileScanTicks);
    free(sf->tileFlags);

    IntMap_Destroy(&sf->shipMap);
    ShipVector_Destroy(&sf->ships);

    free(sf);
    ai->aiHandle = NULL;
}

static void MapperFleetInitShip(MapperFleetData *sf,
                                MapperShipData *ship, MobID mobid)
{
    MBUtil_Zero(ship, sizeof(*ship));
    ship->mobid = mobid;
    ship->assignedTile = -1;
    ship->gov = MAPPER_GOV_INVALID;
}

static MapperShipData *MapperFleetGetShip(MapperFleetData *sf, MobID mobid)
{
    int i = IntMap_Get(&sf->shipMap, mobid);

    if (i != MOB_ID_INVALID) {
        return ShipVector_GetPtr(&sf->ships, i);
    } else {
        MapperShipData *s;
        ShipVector_Grow(&sf->ships);
        s = ShipVector_GetLastPtr(&sf->ships);

        MapperFleetInitShip(sf, s, mobid);

        i = ShipVector_Size(&sf->ships) - 1;
        IntMap_Put(&sf->shipMap, mobid, i);
        ASSERT(IntMap_Get(&sf->shipMap, mobid) == i);
        return s;
    }
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void MapperFleetDestroyShip(MapperFleetData *sf, MobID mobid)
{
    MapperShipData *s;
    int i = IntMap_Get(&sf->shipMap, mobid);

    ASSERT(i != -1);

    IntMap_Remove(&sf->shipMap, mobid);

    if (i != ShipVector_Size(&sf->ships) - 1) {
        ASSERT(i < ShipVector_Size(&sf->ships));
        s = ShipVector_GetPtr(&sf->ships, i);
        if (s->gov == MAPPER_GOV_GUARD) {
            ASSERT(sf->numGuard > 0);
            sf->numGuard--;
        }

        s = ShipVector_GetLastPtr(&sf->ships);

        ShipVector_PutValue(&sf->ships, i, *s);
        ASSERT(IntMap_Get(&sf->shipMap, s->mobid) ==
               ShipVector_Size(&sf->ships) - 1);

        IntMap_Put(&sf->shipMap, s->mobid, i);
        ASSERT(IntMap_Get(&sf->shipMap, s->mobid) == i);
    }

    ShipVector_Shrink(&sf->ships);
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

static uint32 MapperFleetGetNextTile(MapperFleetData *sf)
{
    uint32 bestIndex = Random_Int(0, sf->numTiles - 1);
    uint32 i;

    i = bestIndex;
    ASSERT(i < sf->numTiles);
    if (!BitVector_Get(&sf->tileBV, i) && sf->tileFlags[i] == MAP_TILE_EMPTY) {
        goto found;
    }

    i = 0;
    while (i < sf->numTiles) {
        if (BitVector_Get(&sf->tileBV, i)) {
            // Someone else already claimed this tile...
        } else {
            if (!BitVector_Get(&sf->tileBV, i) &&
                sf->tileFlags[i] == MAP_TILE_EMPTY) {
                bestIndex = i;
                goto found;
            }
            if (sf->tileScanTicks[i] < sf->tileScanTicks[bestIndex]) {
                if ((sf->tileFlags[bestIndex] & ~MAP_TILE_SCANNED) == 0) {
                    bestIndex = i;
                }
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


static void MapperFleetRunAI(FleetAI *ai)
{
    MapperFleetData *sf = ai->aiHandle;
    const BattleParams *bp = Battle_GetParams();
    uint targetScanFilter = FLEET_SCAN_SHIP;
    IntMap targetMap;
    float firingRange = MobType_GetSpeed(MOB_TYPE_MISSILE) *
                        MobType_GetMaxFuel(MOB_TYPE_MISSILE);
    float guardRange = MobType_GetSensorRadius(MOB_TYPE_BASE) *
                       (1.0f + sf->numGuard / 10);

    IntMap_Create(&targetMap);

    ASSERT(ai->player.aiType == FLEET_AI_MAPPER);
    sf->tick++;

    /*
     * Analyze our sensor data.
     */
    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);

        if (mob->type == MOB_TYPE_FIGHTER ||
            mob->type == MOB_TYPE_BASE) {
            uint32 i;
            MapperFleetGetTileIndex(sf, &mob->pos, &i);
            ASSERT(i < sf->numTiles);
            sf->tileScanTicks[i] = sf->tick;
            sf->tileFlags[i] = MAP_TILE_SCANNED;
        }

        if (mob->type == MOB_TYPE_BASE) {
            sf->basePos = mob->pos;
        }
    }
    for (uint32 s = 0; s < MobVector_Size(&ai->sensors); s++) {
        Mob *sm = MobVector_GetPtr(&ai->sensors, s);
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
            ASSERT(sm->type == MOB_TYPE_LOOT_BOX);
            f |= MAP_TILE_LOOT;
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
            MobVector_Grow(&ai->sensors);
            Mob *sm = MobVector_GetLastPtr(&ai->sensors);
            *sm = sf->enemyBase;
        }
    }

    /*
     * Assign tiles to scouts.
     */
    {
        MapperFleetStartTileSearch(sf);
        FleetUtil_SortMobsByDistance(&ai->mobs, &sf->basePos);
        int i = MobVector_Size(&ai->mobs) - 1;
        while (i >= 0) {
            Mob *mob = MobVector_GetPtr(&ai->mobs, i);
            if (mob->type == MOB_TYPE_FIGHTER) {
                MapperShipData *s = MapperFleetGetShip(sf, mob->mobid);
                ASSERT(s != NULL);
                ASSERT(s->mobid == mob->mobid);

                if (s->gov == MAPPER_GOV_SCOUT &&
                    s->assignedTile == -1) {
                    uint32 tileIndex = MapperFleetGetNextTile(sf);
                    s->assignedTile = tileIndex;
                }
            }
            i--;
        }
    }

    /*
     * Map Mob processing loop.
     */
    for (uint32 m = 0; m < MobVector_Size(&ai->mobs); m++) {
        Mob *mob = MobVector_GetPtr(&ai->mobs, m);
        MapperShipData *s = MapperFleetGetShip(sf, mob->mobid);
        ASSERT(s != NULL);
        ASSERT(s->mobid == mob->mobid);

        if (mob->type == MOB_TYPE_FIGHTER && s->gov == MAPPER_GOV_INVALID) {
            sf->spawnCount++;
            mob->cmd.target = sf->basePos;

            if (sf->numGuard < 1) {
                s->gov = MAPPER_GOV_GUARD;
            } else {
                EnumDistribution dist[] = {
                    { MAPPER_GOV_SCOUT,  0.50 },
                    { MAPPER_GOV_GUARD,  0.25 },
                    { MAPPER_GOV_ATTACK, 0.25 },
                };
                s->gov = Random_Enum(dist, ARRAYSIZE(dist));
            }
            if (s->gov == MAPPER_GOV_GUARD) {
                sf->numGuard++;
            }
        }

        if (!mob->alive) {
            if (mob->type == MOB_TYPE_FIGHTER) {
                sf->lastShipLost = mob->pos;
                sf->lastShipLostTick = sf->tick;
            }
            MapperFleetDestroyShip(sf, mob->mobid);
        } else if (mob->type == MOB_TYPE_FIGHTER) {
            int t = -1;

            if (s->gov == MAPPER_GOV_SCOUT) {
                /*
                 * Just run the shared random/loot-box code.
                 */
            } else if (s->gov == MAPPER_GOV_ATTACK) {
                t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                targetScanFilter);
            } else if (s->gov == MAPPER_GOV_GUARD) {
                Mob *sm;

                t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                targetScanFilter);
                if (t != -1) {
                    sm = MobVector_GetPtr(&ai->sensors, t);
                    if (FPoint_Distance(&sm->pos, &sf->basePos) > guardRange) {
                        t = -1;
                    }
                }
            }

            if (t == -1) {
                /*
                * Avoid having all the fighters rush to the same loot box.
                */
                t = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                FLEET_SCAN_LOOT_BOX);
                if (t != -1) {
                    if (s->gov == MAPPER_GOV_GUARD) {
                        Mob *sm;
                        sm = MobVector_GetPtr(&ai->sensors, t);
                        if (FPoint_Distance(&sm->pos, &sf->basePos) > guardRange) {
                            t = -1;
                        }
                    } else {
                        Mob *sm;
                        sm = MobVector_GetPtr(&ai->sensors, t);
                        if (FPoint_Distance(&sm->pos, &mob->pos) > firingRange) {
                            t = -1;
                        }
                    }
                }

                if (t != -1 && IntMap_Increment(&targetMap, t) > 1) {
                    /*
                     * Ideally we find the next best target, but for now just
                     * go back to random movement.
                     */
                    t = -1;
                }
            }

            {
                int ct = FleetUtil_FindClosestSensor(ai, &mob->pos,
                                                     targetScanFilter);
                if (ct != -1) {
                    Mob *sm;
                    sm = MobVector_GetPtr(&ai->sensors, ct);

                    if ((sf->tick - s->lastFiredTick) > 20 &&
                        FPoint_Distance(&mob->pos, &sm->pos) < firingRange) {
                        mob->cmd.spawnType = MOB_TYPE_MISSILE;
                        s->lastFiredTick = sf->tick;
                    }
                }
            }

            if (t != -1) {
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, t);
                mob->cmd.target = sm->pos;
            } else if (s->gov == MAPPER_GOV_SCOUT &&
                       s->assignedTile != -1) {
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
                        if (sf->tick - sf->lastShipLostTick < 1000) {
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

                FleetUtil_RandomPointInRange(&mob->cmd.target,
                                             &moveCenter, moveRadius);
            }
        } else if (mob->type == MOB_TYPE_MISSILE) {
            uint scanFilter = FLEET_SCAN_SHIP | FLEET_SCAN_MISSILE;
            int s = FleetUtil_FindClosestSensor(ai, &mob->pos, scanFilter);
            if (s != -1) {
                Mob *sm;
                sm = MobVector_GetPtr(&ai->sensors, s);
                mob->cmd.target = sm->pos;
            }
        } else if (mob->type == MOB_TYPE_BASE) {
            if (ai->credits > 200 &&
                Random_Int(0, 20) == 0) {
                mob->cmd.spawnType = MOB_TYPE_FIGHTER;
            } else {
                mob->cmd.spawnType = MOB_TYPE_INVALID;
            }

            if (FPoint_Distance(&mob->pos, &mob->cmd.target) <= MICRON) {
                mob->cmd.target.x = Random_Float(0.0f, bp->width);
                mob->cmd.target.y = Random_Float(0.0f, bp->height);
            }
        } else if (mob->type == MOB_TYPE_LOOT_BOX) {
            Mob *sm;

            mob->cmd.target = sf->basePos;

            /*
             * Add our own loot box to the sensor targets so that we'll
             * steer towards them.
             */
            MobVector_Grow(&ai->sensors);
            sm = MobVector_GetLastPtr(&ai->sensors);
            *sm = *mob;
        }
    }

    IntMap_Destroy(&targetMap);
}
