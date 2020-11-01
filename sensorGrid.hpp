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

#include "mobSet.hpp"
#include "IntMap.hpp"

class SensorGrid
{
public:
    /**
     * Construct a new SensorGrid.
     */
    SensorGrid() {
        myTargetLastSeenMap.setEmptyValue(0);
        myLastTick = 0;

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
     * How many friends do we have?
     */
    int numFriends() {
        return myFriends.size();
    }


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
     * Look-up a friendly Mob from this SensorGrid.
     */
    Mob *getFriend(MobID mobid) {
        return myFriends.get(mobid);
    }

    /**
     * Look-up an enemy Mob from this SensorGrid.
     */
    Mob *getEnemy(MobID mobid) {
        return myTargets.get(mobid);
    }

    /**
     * Look-up a Mob from this SensorGrid.
     */
    Mob *get(MobID mobid) {
        Mob *m = getFriend(mobid);
        if (m != NULL) {
            return m;
        }

        m = getEnemy(mobid);
        if (m != NULL) {
            return m;
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

    /**
     * Return the tick we last scanned the specified mob at, if it's
     * still tracked on the SensorGrid.
     */
    uint getLastSeenTick(MobID mobid) {
        if (getFriend(mobid) != NULL) {
            return myLastTick;
        }

        return myTargetLastSeenMap.get(mobid);
    }

private:
    int myEnemyBaseDestroyedCount;
    FPoint myFriendBasePos;

    uint myLastTick;
    IntMap myTargetLastSeenMap;

    MobSet myFriends;
    MobSet myTargets;
};


#endif // _SENSORGRID_H_202008021543
