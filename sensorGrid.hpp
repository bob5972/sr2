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
     * Update the SensorGrid with the new sensor information in the tick.
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
                int last = myTargets.size() - 1;
                if (last != -1) {
                    myTargets[i] = myTargets[last];
                    myTargets.shrink();
                }
                myMap.put(myTargets[i].mob.mobid, i);
            }
        }
    }

    Mob *findClosestMob(const FPoint *pos, MobTypeFlags filter);
    Mob *findNthClosestMob(const FPoint *pos, MobTypeFlags filter, int n);
    Mob *findClosestMobInRange(const FPoint *pos, MobTypeFlags filter,
                               float radius);
    Mob *get(MobID mobid);

    /*
     * Find the enemy base closest to your base.
     */
    Mob *enemyBase();

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
