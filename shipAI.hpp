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
        loadRegistry(ai->player.mreg);
    }

    void loadRegistry(MBRegistry *mreg) {
        struct {
            const char *key;
            const char *value;
        } configs[] = {
            { "evadeFighters", "FALSE", },
        };

        mreg = MBRegistry_AllocCopy(mreg);

        for (uint i = 0; i < ARRAYSIZE(configs); i++) {
            MBRegistry_Put(mreg, configs[i].key, configs[i].value);
        }

        myEvadeFighters = MBRegistry_GetBool(mreg, "evadeFighters");

        MBRegistry_Free(mreg);
    }

    /**
     * Destroy this BasicAIGovernor.
     */
    virtual ~BasicAIGovernor() { }

//     FPoint *getTargetPos(MobID mobid) {
//         BasicShipAI *ship = (BasicShipAI *)getShip(mobid);
//         if (ship == NULL) {
//             return NULL;
//         }
//
//         return &ship->targetPos;
//     }

    virtual void runMob(Mob *mob);

protected:
    class BasicShipAI : public ShipAI
    {
    public:
        BasicShipAI(MobID mobid)
        :ShipAI(mobid)
        {
            MBUtil_Zero(&targetPos, sizeof(targetPos));
        }

        ~BasicShipAI() { }

        FPoint targetPos;
    };

    virtual ShipAI *createShip(MobID mobid) {
        BasicShipAI *ship = new BasicShipAI(mobid);
        Mob *friendBase = mySensorGrid->friendBase();

        if (friendBase != NULL) {
            ship->targetPos = friendBase->pos;
        }

        Mob *m = getMob(mobid);
        if (m != NULL) {
            ShipAI *p = getShip(m->parentMobid);
            if (p != NULL) {
                BasicShipAI *pShip = (BasicShipAI *)p;
                ship->targetPos = pShip->targetPos;
            }
        }

        return (ShipAI *)ship;
    }

    SensorGrid *mySensorGrid;
    bool myEvadeFighters;
};


#endif // _FIGHTERAI_H_202009141102
