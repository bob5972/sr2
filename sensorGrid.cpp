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
    int trackedEnemyBases = myTargets.getNumTrackedBases();

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
    MobSet::MobIt it = myTargets.iterator();
    while (it.hasNext()) {
        Mob *m = it.next();
        uint staleAge;
        MobID mobid = m->mobid;
        uint lastSeenTick = myTargetLastSeenMap.get(mobid);
        uint scanAge = ai->tick - lastSeenTick;

        ASSERT(lastSeenTick <= ai->tick);

        if (m->type == MOB_TYPE_BASE) {
            staleAge = MAX_UINT;
        } else {
            staleAge = 2;
        }

        if (staleAge < MAX_UINT && scanAge > staleAge) {
            it.remove();
            myTargetLastSeenMap.remove(mobid);
        }
    }

    myFriends.pin();
    myTargets.pin();

    if (myTargets.getNumTrackedBases() < trackedEnemyBases) {
        int baseDelta = trackedEnemyBases - myTargets.getNumTrackedBases();
        myEnemyBaseDestroyedCount += baseDelta;
    }
}

