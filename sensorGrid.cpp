/*
 * sensorGrid.cpp -- part of SpaceRobots2
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

#include "sensorGrid.hpp"

void SensorGrid::updateTick(FleetAI *ai)
{
    CMobIt mit;
    Mob *enemyBase = myTargets.getBase();
    int trackedEnemyBases = myTargets.myNumTrackedBases;

    myLastTick = ai->tick;

    myFriends.unpin();
    myTargets.unpin();

    /*
     * Process friendly mobs.
     */
    myFriends.makeEmpty();
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *m = CMobIt_Next(&mit);

        myFriends.updateMob(m);

        if (m->type == MOB_TYPE_LOOT_BOX) {
            /*
                * Also add LootBoxes to the targets list, since fleets
                * collect their own boxes as loot.
                */
            myTargets.updateMob(m);
            myTargetLastSeenMap.put(m->mobid, ai->tick);
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
                myTargetLastSeenMap.remove(enemyBase->mobid);
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
            myTargets.updateMob(m);
            myTargetLastSeenMap.put(m->mobid, ai->tick);
        } else {
            myTargets.removeMob(m->mobid);
            myTargetLastSeenMap.remove(m->mobid);
        }
    }

    /*
     * Clear out stale targets.
     */
    uint i = 0;
    while (i < myTargets.myMobs.size()) {
        uint staleAge;
        MobID mobid = myTargets.myMobs[i].mobid;
        uint lastSeenTick = getLastSeenTick(mobid);

        ASSERT(myTargets.myMap.get(mobid) == i);
        ASSERT(lastSeenTick <= ai->tick);

        uint scanAge = ai->tick - lastSeenTick;

        if (myTargets.myMobs[i].type == MOB_TYPE_BASE) {
            staleAge = MAX_UINT;
        } else {
            staleAge = 2;
        }

        if (staleAge < MAX_UINT && scanAge > staleAge) {
            myTargets.removeMob(mobid);
            myTargetLastSeenMap.remove(mobid);
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

