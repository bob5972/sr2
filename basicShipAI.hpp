/*
 * basicShipAI.hpp -- part of SpaceRobots2
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

#ifndef _BASICSHIPAI_H_20211224
#define _BASICSHIPAI_H_20211224

#include "shipAI.hpp"

class BasicAIGovernor : public ShipAIGovernor
{
public:
    /**
     * Construct a new BasicAIGovernor.
     */
    BasicAIGovernor(FleetAI *ai, SensorGrid *sg)
    :ShipAIGovernor(ai)
    {
        RandomState *rs = &myRandomState;
        mySensorGrid = sg;

        MBUtil_Zero(&myConfig, sizeof(myConfig));
        loadRegistry(ai->player.mreg);

        sg->loadRegistry(ai->player.mreg);

        myStartingAngle = RandomState_Float(rs, 0.0f, M_PI * 2.0f);
    }

    virtual void loadRegistry(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            { "evadeFighters",          "FALSE", },
            { "evadeUseStrictDistance", "FALSE", },
            { "evadeStrictDistance",    "50",    },
            { "attackRange",            "100",   },
            { "attackExtendedRange",    "TRUE",  },
            { "guardRange",             "0",     },
            { "gatherRange",            "50",    },
            { "gatherAbandonStale",     "FALSE", },
            { "rotateStartingAngle",    "TRUE",  },
            { "startingMaxRadius",      "300",   },
            { "startingMinRadius",      "250",   },
            { "creditReserve",          "200",   },
            { "baseSpawnJitter",        "1",     },
            { "fighterFireJitter",      "0",     },
        };

        mreg = MBRegistry_AllocCopy(mreg);

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (!MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_PutConst(mreg, configs[i].key, configs[i].value);
            }
        }

        myConfig.evadeFighters = MBRegistry_GetBool(mreg, "evadeFighters");
        myConfig.evadeUseStrictDistance =
            MBRegistry_GetBool(mreg, "evadeUseStrictDistance");
        myConfig.evadeStrictDistance =
            MBRegistry_GetFloat(mreg, "evadeStrictDistance");
        myConfig.attackRange = MBRegistry_GetFloat(mreg, "attackRange");
        myConfig.attackExtendedRange =
            MBRegistry_GetBool(mreg, "attackExtendedRange");
        myConfig.guardRange =
            MBRegistry_GetFloat(mreg, "guardRange");
        myConfig.gatherRange =
            MBRegistry_GetFloat(mreg, "gatherRange");
        myConfig.gatherAbandonStale =
            MBRegistry_GetBool(mreg, "gatherAbandonStale");

        myConfig.rotateStartingAngle =
            MBRegistry_GetBool(mreg, "rotateStartingAngle");

        myConfig.startingMaxRadius =
            MBRegistry_GetFloat(mreg, "startingMaxRadius");
        if (myConfig.startingMaxRadius < 0.0f) {
            myConfig.startingMaxRadius = 0.0f;
        }
        myConfig.startingMinRadius =
            MBRegistry_GetFloat(mreg, "startingMinRadius");
        if (myConfig.startingMinRadius < 0.0f) {
            myConfig.startingMinRadius = 0.0f;
        }

        if (myConfig.startingMinRadius >= myConfig.startingMaxRadius) {
            myConfig.startingMaxRadius = myConfig.startingMinRadius;
        }
        ASSERT(myConfig.startingMinRadius <= myConfig.startingMaxRadius);

        myConfig.creditReserve =
            (uint)MBRegistry_GetFloat(mreg, "creditReserve");
        myConfig.baseSpawnJitter =
            (uint)MBRegistry_GetFloat(mreg, "baseSpawnJitter");
        myConfig.fighterFireJitter =
            (uint)MBRegistry_GetFloat(mreg, "fighterFireJitter");

        MBRegistry_Free(mreg);
    }

    /**
     * Destroy this BasicAIGovernor.
     */
    virtual ~BasicAIGovernor() { }

    virtual void runMob(Mob *mob);

    virtual void runTick() {
        FleetAI *ai = myFleetAI;
        SensorGrid *sg = mySensorGrid;

        sg->updateTick(ai);
        ShipAIGovernor::runTick();
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget);
    virtual void doIdle(Mob *mob, bool newlyIdle);
    virtual void doSpawn(Mob *mob);

protected:
    typedef enum BasicShipAIState {
        BSAI_STATE_IDLE,
        BSAI_STATE_GATHER,
        BSAI_STATE_ATTACK,
        BSAI_STATE_EVADE,
        BSAI_STATE_HOLD,
    } BasicShipAIState;

    class BasicShipAI : public ShipAI
    {
        public:
        BasicShipAI(MobID mobid, BasicAIGovernor *gov)
        :ShipAI(mobid), myGov(gov)
        {
            MBUtil_Zero(&attackData, sizeof(attackData));
            MBUtil_Zero(&evadeData, sizeof(evadeData));
            MBUtil_Zero(&holdData, sizeof(holdData));

            state = BSAI_STATE_IDLE;
            oldState = BSAI_STATE_IDLE;
            stateChanged = FALSE;
        }

        virtual ~BasicShipAI() { }

        void hold(const FPoint *holdPos, uint holdCount) {
            state = BSAI_STATE_HOLD;
            holdData.pos = *holdPos;
            holdData.count = holdCount;
        }

        void attack(Mob *enemyTarget) {
            Mob *mob = myGov->getMob(mobid);
            state = BSAI_STATE_ATTACK;
            mob->cmd.target = enemyTarget->pos;
            myGov->doAttack(mob, enemyTarget);
        }

        BasicAIGovernor *myGov;
        BasicShipAIState oldState;
        BasicShipAIState state;
        bool stateChanged;

        struct {
            FPoint pos;
        } attackData;

        struct {
            FPoint pos;
        } evadeData;

        struct {
            uint count;
            FPoint pos;
        } holdData;
    };

public:
    virtual BasicShipAI *newShip(MobID mobid) {
        return new BasicShipAI(mobid, this);
    }

protected:
    /*
     * BasicAIGovernor data
     */
    SensorGrid *mySensorGrid;

    struct {
        bool evadeFighters;
        bool evadeUseStrictDistance;
        float evadeStrictDistance;
        float attackRange;
        bool attackExtendedRange;
        float guardRange;
        float gatherRange;
        bool gatherAbandonStale;

        bool rotateStartingAngle;
        float startingMaxRadius;
        float startingMinRadius;

        uint creditReserve;
        uint baseSpawnJitter;
        uint fighterFireJitter;
    } myConfig;

    float myStartingAngle;
};

#endif // _BASICSHIPAI_H_20211224