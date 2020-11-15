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
        myCachedBase = -1;
        myMap.setEmptyValue(-1);
        myTypeCounts.setEmptyValue(0);
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
        return myTypeCounts.get(MOB_TYPE_BASE);
    }

    /**
     * Returns the number of mobs in this MobSet.
     */
    int size() {
        return myMobs.size();
    }

    int numMobs(MobTypeFlags filter) {
        int mobs = 0;
        int i;

        for (i = MOB_TYPE_MIN; i < MOB_TYPE_MAX; i++) {
            MobTypeFlags f = (1 << i);
            if ((f & filter) != 0) {
                mobs += myTypeCounts.get(i);
            }
        }

        return mobs;
    }

    int numMobsInRange(MobTypeFlags filter, const FPoint *pos, float range) {
        int mobs = 0;

        MobIt mit = iterator();
        while (mit.hasNext()) {
            Mob *m = mit.next();

            if (((1 << m->type) & filter) != 0) {
                if (FPoint_Distance(&m->pos, pos) <= range) {
                    mobs++;
                }
            }
        }

        return mobs;
    }



    /**
     * Find the Nth closest mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestMob(const FPoint *pos,
                            MobTypeFlags filter, int n);
    class MobIt {
    public:
        MobIt(MobSet *ms, MobTypeFlags filter) {
            myMobSet = ms;
            i = 0;
            myLastMobid = MOB_ID_INVALID;
            myFilter = filter;
        }

        MobIt(MobSet *ms) {
            myMobSet = ms;
            i = 0;
            myLastMobid = MOB_ID_INVALID;
            myFilter = MOB_FLAG_ALL;
        }

        bool hasNext() {
            return i < myMobSet->numMobs(myFilter);
        }

        Mob *next() {
            Mob *m;
            do {
                m = &myMobSet->myMobs[i++];
            } while (((1 << m->type) & myFilter) == 0);
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
        MobTypeFlags myFilter;
        int i;
    };


    MobIt iterator() {
        return MobIt(this);
    }

    MobIt iterator(MobTypeFlags filter) {
        return MobIt(this, filter);
    }

private:
    int myCachedBase;
    IntMap myMap;
    IntMap myTypeCounts;
    MBVector<Mob> myMobs;
};

#endif //_MOBSET_H_202009132004
