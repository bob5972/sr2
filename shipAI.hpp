/*
 * shipAI.hpp -- part of SpaceRobots2
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

#ifndef _FIGHTERAI_H_202009141102
#define _FIGHTERAI_H_202009141102

extern "C" {
#include "battleTypes.h"
#include "mob.h"
#include "random.h"
}

#include "mobSet.hpp"
#include "sensorGrid.hpp"

class ShipAIGovernor
{
public:
    /**
     * Construct a new ShipAIGovernor.
     */
    ShipAIGovernor(FleetAI *ai)
    {
        myFleetAI = ai;
        RandomState_Create(&myRandomState);
        myMap.setEmptyValue(-1);
    }

    /**
     * Destroy this ShipAIGovernor.
     */
    ~ShipAIGovernor() {
        int i;
        for (i = 0; i < myAIData.size(); i++) {
            ShipAI *ship = myAIData[i];
            destroyShip(ship);
        }

        RandomState_Destroy(&myRandomState);
    }

    /**
     * Run all the mobs from the iterator
     * that have been added to this ShipAIGovernor.
     */
    void runAllMobs(CMobIt *mit) {
        while (CMobIt_HasNext(mit)) {
            Mob *mob = CMobIt_Next(mit);
            ASSERT(mob != NULL);

            if (containsMobid(mob->mobid)) {
                runMob(mob);
            }
        }
    }

    /**
     * Sets the random seed used by this ShipAIGovernor.
     */
    void setSeed(uint64 seed) {
        RandomState_SetSeed(&myRandomState, seed);
    }

    /**
     * Run a single mob.  This is overridden by sub-classes.
     */
    virtual void runMob(Mob *mob) {
        ASSERT(mob != NULL);
        ASSERT(containsMobid(mob->mobid));

        /*
         * By default, do nothing.
         */
    }

    bool containsMobid(MobID mobid) {
        return myMap.get(mobid) != -1;
    }

    void addMobid(MobID mobid) {
        int i = myAIData.push(createShip(mobid));
        myMap.put(mobid, i);
    }

    void removeMobid(MobID mobid) {
        int i = myMap.get(mobid);

        if (i == -1) {
            return;
        }

        ShipAI *delShip = myAIData[i];

        int lastIndex = myAIData.size() - 1;
        ShipAI *lastShip = myAIData.get(lastIndex);
        MobID lastMobid = lastShip->mobid;

        ASSERT(i != -1);

        myAIData[i] = lastShip;
        myMap.put(lastMobid, i);
        myAIData.shrink();

        myMap.remove(mobid);

        destroyShip(delShip);
    }

protected:
    FleetAI *myFleetAI;
    RandomState myRandomState;

    class ShipAI
    {
    public:
        ShipAI(MobID mobid)
        :mobid(mobid) { }

        virtual ~ShipAI() { }

        MobID mobid;
    };

    virtual ShipAI *createShip(MobID mobid) {
        return new ShipAI(mobid);
    }

    virtual void destroyShip(ShipAI *ship) {
        delete ship;
    }

    Mob *getMob(MobID mobid) {
        return MobPSet_Get(&myFleetAI->mobs, mobid);
    }

    ShipAI *getShip(MobID mobid) {
        int i = myMap.get(mobid);
        if (i == -1) {
            return NULL;
        }
        return myAIData[i];
    }

private:
    IntMap myMap;
    MBVector<ShipAI *> myAIData;
};

class BasicAIGovernor : public ShipAIGovernor
{
public:
    /**
     * Construct a new BasicAIGovernor.
     */
    BasicAIGovernor(FleetAI *ai, SensorGrid *sg)
    :ShipAIGovernor(ai)
    {
        mySensorGrid = sg;

        MBUtil_Zero(&myConfig, sizeof(myConfig));
        loadRegistry(ai->player.mreg);
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
            { "gatherRange",            "50",    },
        };

        mreg = MBRegistry_AllocCopy(mreg);

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            if (!MBRegistry_ContainsKey(mreg, configs[i].key)) {
                MBRegistry_Put(mreg, configs[i].key, configs[i].value);
            }
        }

        myConfig.evadeFighters = MBRegistry_GetBool(mreg, "evadeFighters");
        myConfig.evadeUseStrictDistance =
            MBRegistry_GetBool(mreg, "evadeUseStrictDistance");
        myConfig.evadeStrictDistance =
            MBRegistry_GetFloat(mreg, "evadeStrictDistance");
        myConfig.attackRange =
            MBRegistry_GetFloat(mreg, "attackRange");
        myConfig.gatherRange =
            MBRegistry_GetFloat(mreg, "gatherRange");

        MBRegistry_Free(mreg);
    }

    /**
     * Destroy this BasicAIGovernor.
     */
    virtual ~BasicAIGovernor() { }

    virtual void runMob(Mob *mob);


    virtual void doIdle(Mob *mob, bool newlyIdle) {
        FleetAI *ai = myFleetAI;
        RandomState *rs = &myRandomState;

        if (newlyIdle) {
            mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
            mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
        }
    }

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
        BasicShipAI(MobID mobid)
        :ShipAI(mobid)
        {
            MBUtil_Zero(&attackData, sizeof(attackData));
            MBUtil_Zero(&evadeData, sizeof(evadeData));
            MBUtil_Zero(&holdData, sizeof(holdData));

            state = BSAI_STATE_IDLE;
            oldState = BSAI_STATE_IDLE;
            stateChanged = FALSE;
        }

        ~BasicShipAI() { }

        void hold(const FPoint *holdPos, uint holdCount) {
            state = BSAI_STATE_HOLD;
            holdData.pos = *holdPos;
            holdData.count = holdCount;
        }

//         void idleMove(const FPoint *movePos) {
//             idleData.newPos = TRUE;
//             idleData.pos = *movePos;
//         }

        BasicShipAIState oldState;
        BasicShipAIState state;
        bool stateChanged;

//         struct {
//             bool newPos;
//             FPoint pos;
//         } idleData;

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

    virtual ShipAI *createShip(MobID mobid);

    SensorGrid *mySensorGrid;

    struct {
        bool evadeFighters;
        bool evadeUseStrictDistance;
        float evadeStrictDistance;
        float attackRange;
        float gatherRange;
    } myConfig;
};


#endif // _FIGHTERAI_H_202009141102
