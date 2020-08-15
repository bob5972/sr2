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
        myMap.setEmptyValue(-1);
    }

    /**
     * Update this SensorGrid with the new sensor information in the tick.
     *
     * This invalidates any Mob pointers previously obtained from this
     * SensorGrid.
     */
    void updateTick(FleetAI *ai) {
        CMobIt mit;

        /*
         * Update existing mobs.
         */
        CMobIt_Start(&ai->sensors, &mit);
        while (CMobIt_HasNext(&mit)) {
            Mob *m = CMobIt_Next(&mit);
            int i = myMap.get(m->mobid);

            if (i == -1) {
                myTargets.grow();
                i = myTargets.size() - 1;
                myMap.put(m->mobid, i);
            }

            Target *t = &myTargets[i];
            t->mob = *m;
            t->lastSeenTick = ai->tick;
        }

        /*
         * Clear out stale targets.
         */
        for (uint i = 0; i < myTargets.size(); i++) {
            uint staleAge;
            ASSERT(myTargets[i].lastSeenTick <= ai->tick);

            if (myTargets[i].mob.type == MOB_TYPE_BASE) {
                staleAge = MAX_UINT;
            } else {
                staleAge = 2;
            }

            if (myTargets[i].lastSeenTick - ai->tick > staleAge) {
                myMap.remove(myTargets[i].mob.mobid);
                int last = myTargets.size() - 1;
                if (last != -1) {
                    myTargets[i] = myTargets[last];
                    myTargets.shrink();
                }
                myMap.put(myTargets[i].mob.mobid, i);
            }
        }
    }

    /**
     * Find the closest mob to the specified point.
     */
    Mob *findClosestMob(const FPoint *pos, MobTypeFlags filter) {
        return findNthClosestMob(pos, filter, 0);
    }

    /**
     * Find the Nth closest mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestMob(const FPoint *pos, MobTypeFlags filter, int n) {
        ASSERT(n >= 0);

        if (n > myTargets.size()) {
            return NULL;
        }

        MBVector<Mob *> v;
        v.ensureCapacity(myTargets.size());

        for (uint i = 0; i < myTargets.size(); i++) {
            v.push(&myTargets[i].mob);
        }

        NOT_IMPLEMENTED();
    }

    /**
     * Find the closest mob to the specified point, if it's within
     * the specified range.
     */
    Mob *findClosestMobInRange(const FPoint *pos, MobTypeFlags filter,
                               float radius) {
        Mob *m = findClosestMob(pos, filter);

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
    Mob *get(MobID mobid)
    {
        int i = myMap.get(mobid);
        if (i == -1) {
            return NULL;
        }
        ASSERT(i < myTargets.size());
        return &myTargets[i].mob;
    }

    /**
     * Find the enemy base closest to your base.
     */
    Mob *enemyBase() {
        NOT_IMPLEMENTED();
    }

private:
    struct Target {
        Mob mob;
        uint lastSeenTick;
    };

    /*
     * Map mobid to index in the mobs vector.
     */
    IntMap myMap;

    MBVector<Target> myTargets;
};


#endif // _SENSORGRID_H_202008021543
