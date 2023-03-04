/*
 * mobSet.hpp -- part of SpaceRobots2
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

#ifndef _MOBSET_H_202009132004
#define _MOBSET_H_202009132004

extern "C" {
#include "battleTypes.h"
#include "mob.h"
}

#include "IntMap.hpp"
#include "MBVector.hpp"
#include "mobFilter.h"

class MobSet {
public:
    MobSet() {
        myCachedBase = -1;
        myMap.setEmptyValue(-1);

        for (uint i = 0; i < ARRAYSIZE(myTypeCounts); i++) {
            myTypeCounts[i] = 0;
        }

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
        return myTypeCounts[MOB_TYPE_BASE];
    }

    /**
     * Returns the number of mobs in this MobSet.
     */
    int size() {
        return myMobs.size();
    }

    int numMobs(MobTypeFlags filter) {
        int mobs = 0;

        while (filter != 0) {
            uint32 index = MBUtil_FFS(filter);
            ASSERT(index > 0);
            ASSERT(index - 1 < ARRAYSIZE(myTypeCounts));
            uint32 bit = 1 << (index - 1);
            filter &= ~bit;
            mobs += myTypeCounts[index - 1];
        }

        return mobs;
    }

    int numMobsInRange(MobTypeFlags filter, const FPoint *pos, float range) {
        int mobs = 0;

        if (range <= 0.0f) {
            return 0;
        }

        float rs = range * range;

        MobIt mit = iterator();
        while (mit.hasNext()) {
            Mob *m = mit.next();

            if (((1 << m->type) & filter) != 0) {
                if (FPoint_DistanceSquared(&m->pos, pos) <= rs) {
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
    Mob *findNthClosestMob(const FPoint *pos, MobTypeFlags filter, int n);
    Mob *findClosestMob(const FPoint *pos, MobTypeFlags filter);
    Mob *findFarthestMob(const FPoint *pos, MobTypeFlags filter);

    void pushMobs(MBVector<Mob *> &v, MobTypeFlags filter);

    /*
     * Push all the mobs that match filter and are in the specified range,
     * and sort them ascending by distance.
     */
    void pushClosestMobsInRange(MBVector<Mob *> &v, MobTypeFlags filter,
                                const FPoint *pos, float range);

    void pushMobsInRange(MBVector<Mob *> &v, MobTypeFlags filter,
                         const FPoint *pos, float range);

    void pushMobs(MBVector<Mob *>&v, const MobFilter *f);

    class MobIt {
    public:
        MobIt() {
            myMobSet = NULL;
            myFilter = MOB_FLAG_NONE;
            i = 0;
            numReturned = 0;
            myLastMobid = MOB_ID_INVALID;
            numMobs = 0;
        }
        MobIt(MobSet *ms, MobTypeFlags filter) {
            myMobSet = ms;
            i = 0;
            numReturned = 0;
            myLastMobid = MOB_ID_INVALID;
            myFilter = filter;
            numMobs = myMobSet->numMobs(myFilter);
        }

        MobIt(MobSet *ms) {
            myMobSet = ms;
            i = 0;
            numReturned = 0;
            myLastMobid = MOB_ID_INVALID;
            myFilter = MOB_FLAG_ALL;
            numMobs = myMobSet->numMobs(myFilter);
        }

        bool hasNext() {
            return numReturned < numMobs;
        }

        Mob *next() {
            Mob *m;
            do {
                m = &myMobSet->myMobs[i++];
            } while (((1 << m->type) & myFilter) == 0);
            myLastMobid = m->mobid;
            numReturned++;
            return m;
        }

        void nextBatch(Mob **ma, uint *n, uint size) {
            uint ln = *n;
            ma += ln;
            while (ln < size && hasNext()) {
                ma[0] = next();
                ma++;
                ln++;
            }
            *n = ln;
        }

        void remove() {
            ASSERT(myLastMobid != MOB_ID_INVALID);
            myMobSet->removeMob(myLastMobid);
            myLastMobid = MOB_ID_INVALID;

            ASSERT(i > 0);
            i--;

            ASSERT(numReturned > 0);
            numReturned--;
            numMobs--;
        }

    private:
        MobSet *myMobSet;
        MobID myLastMobid;
        MobTypeFlags myFilter;
        int i;
        uint numReturned;
        uint numMobs;
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
    uint myTypeCounts[MOB_TYPE_MAX];
    MBVector<Mob> myMobs;
};

#endif //_MOBSET_H_202009132004
