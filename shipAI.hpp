/*
 * shipAI.hpp -- part of SpaceRobots2
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

#ifndef _SHIPAI_H_202009141102
#define _SHIPAI_H_202009141102

extern "C" {
#include "battleTypes.h"
#include "mob.h"
#include "Random.h"
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
        RandomState_CreateWithSeed(&myRandomState, ai->seed);
        myMap.setEmptyValue(-1);
        myAutoAdd = FALSE;
    }

    /**
     * Destroy this ShipAIGovernor.
     */
    virtual ~ShipAIGovernor() {
        int i;
        for (i = 0; i < myAIData.size(); i++) {
            ShipAI *ship = myAIData[i];
            deleteShip(ship);
        }

        RandomState_Destroy(&myRandomState);
    }

    virtual void runTick() {
        FleetAI *ai = myFleetAI;
        CMobIt mit;

        CMobIt_Start(&ai->mobs, &mit);
        runAllMobs(&mit);
    }

    /**
     * Run all the mobs from the iterator
     * that have been added to this ShipAIGovernor.
     */
    void runAllMobs(CMobIt *mit) {
        while (CMobIt_HasNext(mit)) {
            Mob *mob = CMobIt_Next(mit);
            ASSERT(mob != NULL);

            bool haveMob = containsMobid(mob->mobid);

            if (!haveMob && myAutoAdd) {
                addMobid(mob->mobid);
                haveMob = TRUE;
            }

            if (haveMob) {
                runMob(mob);
            }

            if (myAutoAdd && !mob->alive) {
                removeMobid(mob->mobid);
            }
        }
    }

    /**
     * Sets whether the governor should automatically
     * add unfamiliar mobs and run them.
     */
    void setAutoAdd(bool autoAdd) {
        myAutoAdd = autoAdd;
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
        if (!containsMobid(mobid)) {
            ShipAI *sai = newShip(mobid);
            int i = myAIData.push(sai);
            myMap.put(mobid, i);
            Mob *m = getMob(mobid);
            doSpawn(m);
        }
    }

    void removeMobid(MobID mobid) {
        int i = myMap.get(mobid);

        if (i == -1) {
            return;
        }

        doDestroy(getMob(mobid));

        ShipAI *delShip = myAIData[i];

        int lastIndex = myAIData.size() - 1;
        ShipAI *lastShip = myAIData.get(lastIndex);
        MobID lastMobid = lastShip->mobid;

        ASSERT(i != -1);

        myAIData[i] = lastShip;
        myMap.put(lastMobid, i);
        myAIData.shrink();

        myMap.remove(mobid);

        deleteShip(delShip);
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

    virtual ShipAI *newShip(MobID mobid) {
        return new ShipAI(mobid);
    }

    virtual void deleteShip(ShipAI *ship) {
        delete ship;
    }


    /**
     * Fires when a mob is added to the governor.
     */
    virtual void doSpawn(Mob *mob) {
        ASSERT(mob != NULL);
        ASSERT(containsMobid(mob->mobid));

        /*
         * By default, do nothing.
         */
    }

    /**
     * Fires when a mob is removed from the governor.
     */
    virtual void doDestroy(Mob *mob) {
        ASSERT(mob != NULL);
        ASSERT(containsMobid(mob->mobid));

        /*
         * By default, do nothing.
         */
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
    bool myAutoAdd;
};


#endif // _SHIPAI_H_202009141102
