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

void SensorGrid::MobSet::updateMob(Mob *m, uint tick)
{
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

void SensorGrid::MobSet::removeMob(MobID badMobid)
{
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

Mob *SensorGrid::MobSet::findNthClosestMob(const FPoint *pos,
                                           MobTypeFlags filter, int n)
{
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

Mob *SensorGrid::MobSet::getBase()
{
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
