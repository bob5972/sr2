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



void SensorGrid::avgVelocityHelper(FPoint *avgVel,
                                   const FPoint *p, float radius,
                                   MobTypeFlags filter,
                                   bool useFriends) {
    uint n = 0;
    MobSet::MobIt mit;
    FPoint lAvgVel;

    if (useFriends) {
        mit = myFriends.iterator(filter);
    } else {
        mit = myTargets.iterator(filter);
    }

    lAvgVel.x = 0.0f;
    lAvgVel.y = 0.0f;

    while (radius > 0.0f && mit.hasNext()) {
        Mob *f = mit.next();
        ASSERT(f != NULL);

        if (FPoint_DistanceSquared(&f->pos, p) <= radius * radius) {
            n++;
            lAvgVel.x += (f->pos.x - f->lastPos.x);
            lAvgVel.y += (f->pos.y - f->lastPos.y);
        }
    }

    if (n != 0) {
        lAvgVel.x /= n;
        lAvgVel.y /= n;
    }

    ASSERT(avgVel != NULL);
    *avgVel = lAvgVel;
}

void SensorGrid::avgPosHelper(FPoint *avgPos,
                              const FPoint *p, float radius,
                              MobTypeFlags filter,
                              bool useFriends) {
    uint n = 0;
    MobSet::MobIt mit;
    FPoint lAvgPos;

    if (useFriends) {
        mit = myFriends.iterator(filter);
    } else {
        mit = myTargets.iterator(filter);
    }

    lAvgPos.x = 0.0f;
    lAvgPos.y = 0.0f;

    while (radius > 0.0f && mit.hasNext()) {
        Mob *f = mit.next();
        ASSERT(f != NULL);

        if (FPoint_DistanceSquared(&f->pos, p) <= radius * radius) {
            n++;

            lAvgPos.x += f->pos.x;
            lAvgPos.y += f->pos.y;
        }
    }

    if (n != 0) {
        lAvgPos.x /= n;
        lAvgPos.y /= n;
    }

    ASSERT(avgPos != NULL);
    *avgPos = lAvgPos;
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
         *
         * XXX: The TILE_SIZE is also way under-estimating the actual scanned
         * area, and we might be scanning up to 16 tiles on a given tick.
         * This should hopefully wash out as the fighters move over several
         * ticks though.
         *
         * But there has to be a better way to do this...
         */
        if (mob->type == MOB_TYPE_FIGHTER) {
            FPoint pos;
            uint32 i;

            pos.x = mob->pos.x;
            pos.y = mob->pos.y;
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x - (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y;
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x + (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y;
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x;
            pos.y = mob->pos.y - (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x;
            pos.y = mob->pos.y + (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x - (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y - (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x + (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y - (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x - (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y + (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);

            pos.x = mob->pos.x + (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y + (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
        }
    }

    /*
     * Calculate the enemy base guess.
     */
    generateGuess();

    // Warning("%s:%d enemyBaseGuess has=%d, noMore=%d, i=%d, xy(%f, %f)\n", __FUNCTION__, __LINE__,
    //         myData.hasEnemyBaseGuess, myData.noMoreEnemyBaseGuess,
    //         myData.enemyBaseGuessIndex,
    //         myData.enemyBaseGuessPos.x, myData.enemyBaseGuessPos.y);
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
               !myData.scannedBV.get(myData.enemyBaseGuessIndex)) {
        /*
         * We already have a valid one... don't change it.
         */
        ASSERT(myData.enemyBaseGuessIndex != -1);
        return;
    } else if (myData.noMoreEnemyBaseGuess) {
        /*
         * We ran out of places to look.
         */
        ASSERT(!myData.hasEnemyBaseGuess);
        return;
     } else if (!myData.hasEnemyBaseGuess ||
                myData.scannedBV.get(myData.enemyBaseGuessIndex)) {
        int ys = RandomState_Int(&myData.rs, 0, myData.bvHeight - 1);
        int xs = RandomState_Int(&myData.rs, 0, myData.bvWidth - 1);
        int yc = 0;

        while (yc < myData.bvHeight) {
            int y = (yc + ys) % myData.bvHeight;
            int xc = 0;

            while (xc < myData.bvWidth) {
                int x = (xc + xs) % myData.bvWidth;
                int i = x + y * myData.bvWidth;
                if (!myData.scannedBV.get(i)) {
                    myData.enemyBaseGuessIndex = i;
                    myData.enemyBaseGuessPos.x = x * TILE_SIZE;
                    myData.enemyBaseGuessPos.y = y * TILE_SIZE;
                    myData.hasEnemyBaseGuess = TRUE;
                    return;
                }

                xc++;
            }

            yc++;
        }
    }

    /*
     * No more guesses.
     */
    myData.noMoreEnemyBaseGuess = TRUE;
    myData.hasEnemyBaseGuess = FALSE;
    myData.enemyBaseGuessIndex = -1;
}

