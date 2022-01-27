/*
 * sensorGrid.cpp -- part of SpaceRobots2
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

#include "sensorGrid.hpp"
#include "mobSet.hpp"

void SensorGrid::updateTick(FleetAI *ai)
{
    CMobIt mit;

    ASSERT(myLastTick <= ai->tick);
    if (myLastTick == ai->tick) {
        /*
         * Assume we already updated this tick.
         */
        return;
    }

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

        MobSet::MobIt tmit = myTargets.iterator();
        while (tmit.hasNext()) {
            Mob *tMob = tmit.next();
            if (Mob_CanScanPoint(m, &tMob->pos)) {
                /*
                 * If we can scan where the target was, remove it,
                 * since it's either gone now, or we'll re-add it below
                 * if it shows up in the scan.
                 */
                myTargets.removeMob(tMob->mobid);
                myTargetLastSeenMap.remove(tMob->mobid);
            }
        }

        if (m->type == MOB_TYPE_POWER_CORE) {
            /*
             * Also add PowerCores to the targets list, since fleets
             * collect their own boxes as powerCore.
             */
            myTargets.updateMob(m);
            myTargetLastSeenMap.put(m->mobid, ai->tick);
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
        } else if (m->type == MOB_TYPE_POWER_CORE) {
            staleAge = myStaleCoreTime;
        } else {
            staleAge = myStaleFighterTime;
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


void MappingSensorGrid::updateTick(FleetAI *ai)
{
    /*
     * Do the base-class processing.
     */
    SensorGrid::updateTick(ai);

    /*
     * Load the new sensor updates.
     */
    CMobIt mit;
    CMobIt_Start(&ai->mobs, &mit);
    while (CMobIt_HasNext(&mit)) {
        Mob *mob = CMobIt_Next(&mit);

        /*
         * XXX: We ignore missiles scanning because they don't scan a full
         * fighter radius.  We similarly ignore bases because they'll spawn
         * fighters all the time anyway.
         */
        if (mob->type == MOB_TYPE_FIGHTER) {
            uint32 i = GetTileIndex(&mob->pos);
            myData.scannedBV.set(i);
        }
    }

    /*
     * Calculate the enemy base guess.
     */
    generateGuess();
}

void MappingSensorGrid::generateGuess()
{
    Mob *eBase = enemyBase();
    if (eBase != NULL) {
        /*
         * Use the current enemy base as the guess.
         */
        myData.enemyBaseGuessPos = eBase->pos;
        myData.enemyBaseGuessIndex = GetTileIndex(&myData.enemyBaseGuessPos);
        myData.hasEnemyBaseGuess = TRUE;
        return;
    } else if (myData.hasEnemyBaseGuess &&
               myData.scannedBV.get(myData.enemyBaseGuessIndex)) {
        /*
         * We already have a valid one... don't change it.
         */
        return;
    } else if (myData.noMoreEnemyBaseGuess) {
        /*
         * We ran out of places to look.
         */
        ASSERT(!myData.hasEnemyBaseGuess);
        return;
     } else if (!myData.hasEnemyBaseGuess ||
                myData.enemyBaseGuessIndex == -1 ||
                myData.scannedBV.get(myData.enemyBaseGuessIndex)) {
        /*
         * XXX: Generate guess randomly?
         */
        for (int x = 0; x < myData.bvWidth; x++) {
            for (int y = 0; y < myData.bvHeight; y++) {
                int i = x + y * myData.bvHeight;
                if (!myData.scannedBV.get(i)) {
                    myData.enemyBaseGuessIndex = i;
                    myData.enemyBaseGuessPos.x = (x + 0.5f) * TILE_SIZE;
                    myData.enemyBaseGuessPos.y = (y + 0.5f) * TILE_SIZE;
                    myData.hasEnemyBaseGuess = TRUE;
                    return;
                }
            }
        }
    }

    /*
     * No more guesses.
     */
    myData.noMoreEnemyBaseGuess = TRUE;
    myData.hasEnemyBaseGuess = FALSE;
    myData.enemyBaseGuessIndex = -1;
    myData.enemyBaseGuessPos.x = 0.0f;
    myData.enemyBaseGuessPos.y = 0.0f;
}

