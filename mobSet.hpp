/*
 * mobSet.hpp -- part of SpaceRobots2
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

#ifndef _MOBSET_H_202009132004
#define _MOBSET_H_202009132004

extern "C" {
#include "battleTypes.h"
#include "mob.h"
}

#include "IntMap.hpp"
#include "MBVector.hpp"

class MobSet {
public:
    MobSet() {
        myNumTrackedBases = 0;
        myCachedBase = -1;
        myMap.setEmptyValue(-1);
        myMobs.pin();
    }

    ~MobSet() {
        myMobs.unpin();
    }

    Mob *get(MobID mobid) {
        int i = myMap.get(mobid);
        if (i == -1) {
            return NULL;
        }

        ASSERT(i < myMobs.size());
        ASSERT(myMobs[i].mobid == mobid);

        return &myMobs[i];
    }

    void updateMob(Mob *m);

    void removeMob(MobID badMobid);

    Mob *getBase();

    void makeEmpty();

    void pin() {
        myMobs.pin();
    }

    void unpin() {
        myMobs.unpin();
    }

    int getNumTrackedBases() {
        return myNumTrackedBases;
    }

    /**
     * Returns the number of mobs in this MobSet.
     */
    int size() {
        return myMobs.size();
    }

    /**
     * Find the Nth closest mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestMob(const FPoint *pos,
                            MobTypeFlags filter, int n);
    class MobIt {
    public:
        MobIt(MobSet *ms) {
            myMobSet = ms;
            i = 0;
            myLastMobid = MOB_ID_INVALID;
        }

        bool hasNext() {
            return i < myMobSet->size();
        }

        Mob *next() {
            Mob *m = &myMobSet->myMobs[i++];
            myLastMobid = m->mobid;
            return m;
        }

        void remove() {
            ASSERT(myLastMobid != MOB_ID_INVALID);
            myMobSet->removeMob(myLastMobid);
            myLastMobid = MOB_ID_INVALID;

            ASSERT(i > 0);
            i--;
        }

    private:
        MobSet *myMobSet;
        MobID myLastMobid;
        int i;
    };


    MobIt iterator() {
        return MobIt(this);
    }

private:
    int myNumTrackedBases;
    int myCachedBase;
    IntMap myMap;
    MBVector<Mob> myMobs;
};

#endif //_MOBSET_H_202009132004
