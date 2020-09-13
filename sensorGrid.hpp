/*
 * sensorGrid.hpp -- part of SpaceRobots2
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

#ifndef _SENSORGRID_H_202008021543
#define _SENSORGRID_H_202008021543

extern "C" {
#include "battleTypes.h"
#include "mob.h"
}

#include "IntMap.hpp"
#include "MBVector.hpp"

class SensorGrid
{
public:
    /**
     * Construct a new SensorGrid.
     */
    SensorGrid() {
        myEnemyBaseDestroyedCount = 0;

        myFriendBasePos.x = 0.0f;
        myFriendBasePos.y = 0.0f;
    }

    /**
     * Destroy this SensorGrid.
     */
    ~SensorGrid() { }

    /**
     * Update this SensorGrid with the new sensor information in the tick.
     *
     * This invalidates any Mob pointers previously obtained from this
     * SensorGrid.
     */
    void updateTick(FleetAI *ai);


    /**
     * Find the friendly mob closest to the specified point.
     */
    Mob *findClosestFriend(const FPoint *pos, MobTypeFlags filter) {
        /*
         * XXX: It's faster to implement this directly to avoid the sort.
         */
        return findNthClosestFriend(pos, filter, 0);
    }

    /**
     * Find the Nth closest friendly mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestFriend(const FPoint *pos, MobTypeFlags filter, int n) {
        return myFriends.findNthClosestMob(pos, filter, n);
    }

    /**
     * Find the target mob closest to the specified point.
     */
    Mob *findClosestTarget(const FPoint *pos, MobTypeFlags filter) {
        /*
         * XXX: It's faster to implement this directly to avoid the sort.
         */
        return findNthClosestTarget(pos, filter, 0);
    }

    /**
     * Find the Nth closest friendly mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestTarget(const FPoint *pos, MobTypeFlags filter, int n) {
        return myTargets.findNthClosestMob(pos, filter, n);
    }

    /**
     * Find the closest mob to the specified point, if it's within
     * the specified range.
     */
    Mob *findClosestTargetInRange(const FPoint *pos, MobTypeFlags filter,
                                  float radius) {
        Mob *m = findClosestTarget(pos, filter);

        if (m != NULL) {
            if (FPoint_Distance(pos, &m->pos) <= radius) {
                return m;
            }
        }

        return NULL;
    }

    /**
     * Look-up a Mob from this SensorGrid.
     */
    Mob *get(MobID mobid) {
        int i;

        i = myFriends.myMap.get(mobid);
        if (i != -1) {
            ASSERT(i < myFriends.myMobs.size());
            return &myFriends.myMobs[i].mob;
        }

        i = myTargets.myMap.get(mobid);
        if (i != -1) {
            ASSERT(i < myTargets.myMobs.size());
            return &myTargets.myMobs[i].mob;
        }

        return NULL;
    }

    /**
     * Find an enemy base.
     */
    Mob *enemyBase() {
        return myTargets.getBase();
    }

    /**
     * How many enemyBases can we confirm were destroyed?
     */
    int enemyBasesDestroyed() {
        return myEnemyBaseDestroyedCount;
    }

    /**
     * Find a friendly base.
     */
    Mob *friendBase() {
        return myFriends.getBase();
    }

    FPoint *friendBasePos() {
        Mob *fbase = friendBase();

        if (fbase != NULL) {
            myFriendBasePos = fbase->pos;
        }

        return &myFriendBasePos;
    }

private:
    struct SensorImage {
        Mob mob;
        uint lastSeenTick;
    };

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

        void updateMob(Mob *m, uint tick);

        void removeMob(MobID badMobid);

        Mob *getBase();

        void makeEmpty() {
            myMobs.makeEmpty();
            myMap.makeEmpty();
            myNumTrackedBases = 0;
            myCachedBase = -1;
        }

        void pin() {
            myMobs.pin();
        }

        void unpin() {
            myMobs.unpin();
        }

        /**
         * Find the Nth closest mob to the specified point.
         * This is 0-based, so the closest mob is found when n=0.
         */
        Mob *findNthClosestMob(const FPoint *pos,
                               MobTypeFlags filter, int n);

        int myNumTrackedBases;
        int myCachedBase;
        IntMap myMap;
        MBVector<SensorImage> myMobs;
    };

    int myEnemyBaseDestroyedCount;
    FPoint myFriendBasePos;
    MobSet myFriends;
    MobSet myTargets;
};


#endif // _SENSORGRID_H_202008021543
