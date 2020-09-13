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
    void updateTick(FleetAI *ai) {
        CMobIt mit;
        Mob *enemyBase = myTargets.getBase();
        int trackedEnemyBases = myTargets.myNumTrackedBases;

        myFriends.unpin();
        myTargets.unpin();

        /*
         * Process friendly mobs.
         */
        myFriends.makeEmpty();
        CMobIt_Start(&ai->mobs, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);

            myFriends.updateMob(m, ai->tick);

            if (m->type == MOB_TYPE_LOOT_BOX) {
                /*
                 * Also add LootBoxes to the targets list, since fleets
                 * collect their own boxes as loot.
                 */
                myTargets.updateMob(m, ai->tick);
            }

            if (enemyBase != NULL) {
                ASSERT(enemyBase->type == MOB_TYPE_BASE);
                if (Mob_CanScanPoint(m, &enemyBase->pos)) {
                    /*
                     * If we can scan where the enemyBase was, remove it,
                     * since it's either gone now, or we'll re-add it below
                     * if it shows up in the scan.
                     *
                     * XXX: If there's more than one enemyBase, we'll only
                     * clear one at a time...
                     */
                    myTargets.removeMob(enemyBase->mobid);
                    enemyBase = myTargets.getBase();
                }
            }
        }

        /*
         * Update existing targets.
         */
        CMobIt_Start(&ai->sensors, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);

            if (m->alive) {
                myTargets.updateMob(m, ai->tick);
            } else {
                myTargets.removeMob(m->mobid);
            }
        }

        /*
         * Clear out stale targets.
         */
        uint i = 0;
        while (i < myTargets.myMobs.size()) {
            uint staleAge;

            ASSERT(myTargets.myMap.get(myTargets.myMobs[i].mob.mobid) == i);
            ASSERT(myTargets.myMobs[i].lastSeenTick <= ai->tick);

            uint scanAge = ai->tick - myTargets.myMobs[i].lastSeenTick;

            if (myTargets.myMobs[i].mob.type == MOB_TYPE_BASE) {
                staleAge = MAX_UINT;
            } else {
                staleAge = 2;
            }

            if (staleAge < MAX_UINT && scanAge > staleAge) {
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

        if (myTargets.myNumTrackedBases < trackedEnemyBases) {
            int baseDelta = trackedEnemyBases - myTargets.myNumTrackedBases;
            myEnemyBaseDestroyedCount += baseDelta;
        }
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

        void updateMob(Mob *m, uint tick) {
            int i = myMap.get(m->mobid);

            if (i == -1) {
                myMobs.grow();
                i = myMobs.size() - 1;
                myMap.put(m->mobid, i);

                if (m->type == MOB_TYPE_BASE) {
                    myCachedBase = i;
                    myNumTrackedBases++;
                }
            }

            ASSERT(i < myMobs.size());
            SensorImage *t = &myMobs[i];
            t->mob = *m;
            t->lastSeenTick = tick;
        }

        void removeMob(MobID badMobid) {
            int i = myMap.get(badMobid);

            if (i == -1) {
                return;
            }

            if (myMobs[i].mob.type == MOB_TYPE_BASE) {
                ASSERT(myNumTrackedBases > 0);
                myNumTrackedBases--;
            }


            if (i == myCachedBase) {
                myCachedBase = -1;
            }

            int last = myMobs.size() - 1;
            if (last != -1) {
                myMobs[i] = myMobs[last];
                myMap.put(myMobs[i].mob.mobid, i);
                myMobs.shrink();

                if (myCachedBase == last) {
                    myCachedBase = i;
                }
            }

            myMap.remove(badMobid);
        }

        Mob *getBase() {
            if (myCachedBase != -1) {
                ASSERT(myNumTrackedBases > 0);
                ASSERT(myCachedBase >= 0 && myCachedBase < myMobs.size());
                return &myMobs[myCachedBase].mob;
            }

            if (myNumTrackedBases > 0) {
                for (uint i = 0; i < myMobs.size(); i++) {
                    if (myMobs[i].mob.type == MOB_TYPE_BASE) {
                        myCachedBase = i;
                        return &myMobs[i].mob;
                    }
                }

                NOT_REACHED();
            }

            return NULL;
        }

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
                               MobTypeFlags filter, int n) {
            ASSERT(n >= 0);
            ASSERT(filter != 0);

            if (n >= myMobs.size()) {
                return NULL;
            }

            MBVector<Mob *> v;
            v.ensureCapacity(myMobs.size());

            for (uint i = 0; i < myMobs.size(); i++) {
                Mob *m = &myMobs[i].mob;
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
