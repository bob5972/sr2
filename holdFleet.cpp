/*
 * holdFleet.cpp -- part of SpaceRobots2
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
#include "fleet.h"
#include "Random.h"
#include "IntMap.h"
#include "battle.h"
}

#include "sensorGrid.hpp"
#include "shipAI.hpp"

class HoldFleetGovernor : public BasicAIGovernor
{
public:
    HoldFleetGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {

    }

    virtual ~HoldFleetGovernor() { }

    virtual void runMob(Mob *mob) {
         BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
         SensorGrid *sg = mySensorGrid;

        this->BasicAIGovernor::runMob(mob);

        if (ship->stateChanged) {
            if (ship->oldState == BSAI_STATE_EVADE &&
                ship->state == BSAI_STATE_IDLE) {
                FPoint holdPos = ship->attackData.pos;
                ship->hold(&holdPos, defaultHoldCount);
            } else if (ship->state == BSAI_STATE_IDLE) {
                Mob *eBase = sg->enemyBase();

                if (eBase != NULL && mob->mobid % 2 == 0) {
                    mob->cmd.target = eBase->pos;
                }
            }
        }
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            // Override BasicFleet defaults
            { "evadeFighters",          "FALSE",  },
            { "evadeUseStrictDistance", "FALSE",  },
            { "evadeStrictDistance",    "397",    },
            { "evadeRange",             "140",    },
            { "attackRange",            "121",    },
            { "attackExtendedRange",    "TRUE",   },
            { "guardRange",             "-1.0",   },
            { "gatherAbandonStale",     "TRUE",   },
            { "gatherRange",            "73.21",  },
            { "rotateStartingAngle",    "TRUE",   },
            { "startingMaxRadius",      "1000",   },
            { "startingMinRadius",      "300",    },

            // HoldFleet-specific options
            { "holdCount",              "42",     },
        };

        mreg = MBRegistry_AllocCopy(mreg);

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (!MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_PutConst(mreg, configs[i].key, configs[i].value);
            }
        }

        defaultHoldCount = MBRegistry_GetUint(mreg, "holdCount");

        this->BasicAIGovernor::loadRegistry(mreg);

        MBRegistry_Free(mreg);
    }

    uint defaultHoldCount;
};


class HoldFleet {
public:
    HoldFleet(FleetAI *ai)
    :sg(), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);

        this->gov.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);
        this->gov.loadRegistry(mreg);
    }

    ~HoldFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    HoldFleetGovernor gov;
    MBRegistry *mreg;
};

static void *HoldFleetCreate(FleetAI *ai);
static void HoldFleetDestroy(void *aiHandle);
static void HoldFleetRunAITick(void *aiHandle);
static void *HoldFleetMobSpawned(void *aiHandle, Mob *m);
static void HoldFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);

void HoldFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "HoldFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &HoldFleetCreate;
    ops->destroyFleet = &HoldFleetDestroy;
    ops->runAITick = &HoldFleetRunAITick;
    ops->mobSpawned = HoldFleetMobSpawned;
    ops->mobDestroyed = HoldFleetMobDestroyed;
}

static void *HoldFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new HoldFleet(ai);
}

static void HoldFleetDestroy(void *handle)
{
    HoldFleet *sf = (HoldFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *HoldFleetMobSpawned(void *aiHandle, Mob *m)
{
    HoldFleet *sf = (HoldFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void HoldFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    HoldFleet *sf = (HoldFleet *)aiHandle;
    sf->gov.removeMobid(m->mobid);
}

static void HoldFleetRunAITick(void *aiHandle)
{
    HoldFleet *sf = (HoldFleet *)aiHandle;
    ASSERT(sf->ai->ops.aiType == FLEET_AI_HOLD);
    sf->gov.runTick();
}
