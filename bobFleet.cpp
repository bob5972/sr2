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

class BobFleetGovernor : public BasicAIGovernor
{
public:
    BobFleetGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg)
    {
        RandomState *rs = &myRandomState;
        this->setAutoAdd(TRUE);
        startingAngle = RandomState_Float(rs, 0.0f, M_PI * 2.0f);
    }

    virtual ~BobFleetGovernor() { }

    virtual void runMob(Mob *mob) {
        BasicShipAI *ship = (BasicShipAI *)getShip(mob->mobid);
        SensorGrid *sg = mySensorGrid;
        //FleetAI *ai = myFleetAI;
        //RandomState *rs = &myRandomState;

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

    virtual void doSpawn(Mob *mob) {
        FleetAI *ai = myFleetAI;

        if (rotateStartingAngle) {
            if (mob->type == MOB_TYPE_FIGHTER) {
                FRPoint p;

                do {
                    startingAngle += M_PI * (3 - sqrtf(5.0f));
                    p.radius = 1000.0f;
                    p.theta = startingAngle;

                    do {
                        p.radius /= 1.1f;
                        FRPoint_ToFPoint(&p, &mob->pos, &mob->cmd.target);
                    } while (p.radius > 300.0f &&
                            FPoint_Clamp(&mob->cmd.target, 0.0f, ai->bp.width,
                                        0.0f, ai->bp.height));
                } while (p.radius <= 300.0f);
            }
        }
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            // Override BasicFleet defaults
            { "evadeFighters",          "FALSE", },
            { "evadeUseStrictDistance", "TRUE",  },
            { "evadeStrictDistance",    "10",    },
            { "evadeRange",             "50",    },
            { "attackRange",            "100",   },
            { "attackExtendedRange",    "TRUE",  },
            { "guardRange",             "200",   },

            // BobFleet-specific options
            { "holdCount",              "10",    },
            { "rotateStartingAngle",    "TRUE",  },
        };

        mreg = MBRegistry_AllocCopy(mreg);

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (!MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_Put(mreg, configs[i].key, configs[i].value);
            }
        }

        this->defaultHoldCount = MBRegistry_GetUint(mreg, "holdCount");
        this->rotateStartingAngle =
            MBRegistry_GetBool(mreg, "rotateStartingAngle");

        this->BasicAIGovernor::loadRegistry(mreg);

        MBRegistry_Free(mreg);
    }

    uint defaultHoldCount;
    float startingAngle;
    bool rotateStartingAngle;
};


class BobFleet {
public:
    BobFleet(FleetAI *ai)
    :sg(), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);

        this->gov.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);
        this->gov.loadRegistry(mreg);
    }

    ~BobFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    BobFleetGovernor gov;
    MBRegistry *mreg;
};

static void *BobFleetCreate(FleetAI *ai);
static void BobFleetDestroy(void *aiHandle);
static void BobFleetRunAITick(void *aiHandle);

void BobFleet_GetOps(FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    ops->aiName = "BobFleet";
    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &BobFleetCreate;
    ops->destroyFleet = &BobFleetDestroy;
    ops->runAITick = &BobFleetRunAITick;
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

static void BobFleetRunAITick(void *aiHandle)
{
    BobFleet *sf = (BobFleet *)aiHandle;

    ASSERT(sf->ai->player.aiType == FLEET_AI_BOB);

    sf->gov.runTick();
}
