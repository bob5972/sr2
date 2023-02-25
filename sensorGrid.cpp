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
#include "mutate.h"

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
                tmit.remove();
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


bool SensorGrid::avgFlock(FPoint *avgVel, FPoint *avgPos,
                          const MobFilter *f, bool useFriends)
{
    uint n = 0;
    MobSet::MobIt mit;
    FPoint lAvgVel;
    FPoint lAvgPos;

    ASSERT(f != NULL);

    if (useFriends) {
        mit = myFriends.iterator();
    } else {
        mit = myTargets.iterator();
    }

    lAvgVel.x = 0.0f;
    lAvgVel.y = 0.0f;

    lAvgPos.x = 0.0f;
    lAvgPos.y = 0.0f;

    if (!MobFilter_IsTriviallyEmpty(f)) {
        while (mit.hasNext()) {
            Mob *m = mit.next();
            ASSERT(m != NULL);

            if (MobFilter_Filter(m, f)) {
                n++;
                lAvgVel.x += (m->pos.x - m->lastPos.x);
                lAvgVel.y += (m->pos.y - m->lastPos.y);

                lAvgPos.x += m->pos.x;
                lAvgPos.y += m->pos.y;
            }
        }
    }

    if (n != 0) {
        lAvgVel.x /= n;
        lAvgVel.y /= n;

        lAvgPos.x /= n;
        lAvgPos.y /= n;
    }

    if (avgVel != NULL) {
        *avgVel = lAvgVel;
    }
    if (avgPos != NULL) {
        *avgPos = lAvgPos;
    }

    return n > 0;
}

void MappingSensorGrid::updateTick(FleetAI *ai)
{
    /*
     * Do the base-class processing.
     */
    SensorGrid::updateTick(ai);

    generateScannedMap(ai);
    generateEnemyBaseGuess();
    generateUnexploredFocus();
    generateFarthestTargetShadow();
}


void MappingSensorGrid::generateScannedMap(FleetAI *ai)
{
    CMobIt mit;

    if (myData.recentlyScannedResetTicks > 1 &&
        ai->tick % myData.recentlyScannedResetTicks == 0) {
        myData.recentlyScannedBV.resetAll();
        myData.haveUnexploredFocus = TRUE;
    }
    if (myData.recentlyScannedMoveFocusTicks > 1 &&
        ai->tick % myData.recentlyScannedMoveFocusTicks == 0) {
        myData.forceUnexploredFocusMove = TRUE;
    }

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
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x - (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y;
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x + (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y;
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x;
            pos.y = mob->pos.y - (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x;
            pos.y = mob->pos.y + (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x - (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y - (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x + (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y - (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x - (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y + (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);

            pos.x = mob->pos.x + (TILE_SIZE / 2.0f);
            pos.y = mob->pos.y + (TILE_SIZE / 2.0f);
            i = GetTileIndex(&pos);
            myData.scannedBV.set(i);
            myData.recentlyScannedBV.set(i);
        }
    }
}

void MappingSensorGrid::generateFarthestTargetShadow(void)
{
    Mob *fbase = friendBaseShadow();
    FPoint pos;

    if (fbase != NULL) {
        pos = fbase->pos;
    } else {
        pos.x = 0;
        pos.y = 0;
    }

    Mob *ft = findFarthestTarget(&pos, MOB_FLAG_SHIP);
    if (ft != NULL) {
        myData.farthestTargetShadow = *ft;
        myData.haveFarthestTargetShadow = TRUE;
    }
}

void MappingSensorGrid::generateUnexploredFocus()
{
    if (!myData.forceUnexploredFocusMove &&
        !hasBeenRecentlyScanned(&myData.unexploredFocusPos)) {
        return;
    }

    myData.forceUnexploredFocusMove = FALSE;

    int ys = RandomState_Int(&myData.rs, 0, myData.bvHeight - 1);
    int xs = RandomState_Int(&myData.rs, 0, myData.bvWidth - 1);
    int yc = 0;

    while (yc < myData.bvHeight) {
        int y = (yc + ys) % myData.bvHeight;
        int xc = 0;

        while (xc < myData.bvWidth) {
            int x = (xc + xs) % myData.bvWidth;
            int i = x + y * myData.bvWidth;
            if (!myData.recentlyScannedBV.get(i)) {
                myData.unexploredFocusPos.x = x * TILE_SIZE;
                myData.unexploredFocusPos.y = y * TILE_SIZE;
                myData.haveUnexploredFocus = TRUE;
                return;
            }

            xc++;
        }

        yc++;
    }

    /*
     * No more guesses... Turn it off until the next reset.
     */
    myData.haveUnexploredFocus = FALSE;
}

void MappingSensorGrid::generateEnemyBaseGuess()
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


void SensorGrid_Mutate(MBRegistry *mreg, float rate, const char *prefix)
{
    MutationFloatParams vf[] = {
        // key                     min     max       mag   jump   mutation
        { "sensorGrid.staleCoreTime",
                                   0.0f,   50.0f,   0.05f, 0.2f, 0.005f},
        { "sensorGrid.staleFighterTime",
                                   0.0f,   20.0f,   0.05f, 0.2f, 0.005f},
    };

    VERIFY(strcmp(prefix, "") == 0);

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));

    Mutate_FloatType(mreg, "sensorGrid.mapping.recentlyScannedResetTicks",
                     MUTATION_TYPE_TICKS);
    Mutate_FloatType(mreg, "sensorGrid.mapping.recentlyScannedMoveFocusTicks",
                     MUTATION_TYPE_TICKS);
}