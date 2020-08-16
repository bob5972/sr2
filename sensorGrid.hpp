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

#include "IntMap.hpp"
#include "MBVector.hpp"

class SensorGrid
{
public:
    /**
     * Construct a new SensorGrid.
     */
    SensorGrid() { }

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
    void updateTick(FleetAI *ai) {
        CMobIt mit;

        myFriends.unpin();
        myTargets.unpin();

        /*
         * Process friendly mobs.
         */
        CMobIt_Start(&ai->mobs, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);

            myFriends.updateMob(m, ai->tick);

            if (m->type == MOB_TYPE_BASE) {
                myBasePos = m->pos;
            } else if (m->type == MOB_TYPE_LOOT_BOX) {
                myTargets.updateMob(m, ai->tick);
            }
        }

        /*
         * Update existing targets.
         */
        CMobIt_Start(&ai->sensors, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);
            myTargets.updateMob(m, ai->tick);
        }

        /*
         * Clear out stale targets.
         */
        uint i = 0;
        while (i < myTargets.myMobs.size()) {
            uint staleAge;

            ASSERT(myTargets.myMap.get(myTargets.myMobs[i].mob.mobid) == i);
            ASSERT(myTargets.myMobs[i].lastSeenTick <= ai->tick);

            if (myTargets.myMobs[i].mob.type == MOB_TYPE_BASE) {
                staleAge = MAX_UINT;
            } else {
                staleAge = 2;
            }

            if (myTargets.myMobs[i].lastSeenTick - ai->tick > staleAge) {
                myTargets.removeMob(myTargets.myMobs[i].mob.mobid);
            } else {
                /*
                 * Only move onto the next one if we didn't swap it out.
                 */
                i++;
            }
        }

        myFriends.pin();
        myTargets.pin();
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
        return findNthClosestMob(myFriends, pos, filter, n);
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
        return findNthClosestMob(myTargets, pos, filter, n);
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
        int i = myTargets.myMap.get(mobid);
        if (i != -1) {
            ASSERT(i < myTargets.myMobs.size());
            return &myTargets.myMobs[i].mob;
        }

        i = myFriends.myMap.get(mobid);
        if (i != -1) {
            ASSERT(i < myFriends.myMobs.size());
            return &myFriends.myMobs[i].mob;
        }

        return NULL;
    }

    /**
     * Find the enemy base closest to your base.
     */
    Mob *enemyBase() {
        return findClosestTarget(&myBasePos, MOB_FLAG_BASE);
    }

private:
    struct SensorImage {
        Mob mob;
        uint lastSeenTick;
    };

    class MobSet {
    public:
        MobSet() {
            myMap.setEmptyValue(-1);
            myMobs.pin();
        }

        ~MobSet() {
            myMobs.unpin();
        }

        void updateMob(Mob *m, uint tick) {
            int i = myMap.get(m->mobid);

            if (i == -1) {
                myMobs.grow();
                i = myMobs.size() - 1;
                myMap.put(m->mobid, i);
            }

            ASSERT(i < myMobs.size());
            SensorImage *t = &myMobs[i];
            t->mob = *m;
            t->lastSeenTick = tick;
        }

        void removeMob(MobID badMobid) {
            int i = myMap.get(badMobid);
            ASSERT(i != -1);

            int last = myMobs.size() - 1;
            if (last != -1) {
                myMobs[i] = myMobs[last];
                myMap.put(myMobs[i].mob.mobid, i);
                myMobs.shrink();
            }

            myMap.remove(badMobid);
        }

        void pin() {
            myMobs.pin();
        }

        void unpin() {
            myMobs.unpin();
        }

        IntMap myMap;
        MBVector<SensorImage> myMobs;
    };


    /**
     * Find the Nth closest mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestMob(MobSet &ms, const FPoint *pos,
                           MobTypeFlags filter, int n) {
        ASSERT(n >= 0);

        if (n >= ms.myMobs.size()) {
            return NULL;
        }

        MBVector<Mob *> v;
        v.ensureCapacity(ms.myMobs.size());

        for (uint i = 0; i < ms.myMobs.size(); i++) {
            Mob *m = &ms.myMobs[i].mob;
            if (((1 << m->type) & filter) != 0) {
                v.push(m);
            }
        }

        if (n >= v.size()) {
            return NULL;
        }

        CMBComparator comp;
        MobP_InitDistanceComparator(&comp, pos);
        v.sort(MBComparator<Mob *>(&comp));

        return v[n];
    }

    FPoint myBasePos;

    MobSet myFriends;
    MobSet myTargets;
};


#endif // _SENSORGRID_H_202008021543
