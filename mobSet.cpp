/*
 * mobSet.cpp -- part of SpaceRobots2
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

#include "mobSet.hpp"

void MobSet::makeEmpty()
{
    myMobs.makeEmpty();
    myMap.makeEmpty();
    myTypeCounts.makeEmpty();
    myCachedBase = -1;
}

void MobSet::updateMob(Mob *m)
{
    int i = myMap.get(m->mobid);

    if (i == -1) {
        myMobs.grow();
        i = myMobs.size() - 1;
        myMap.put(m->mobid, i);

        if (m->type == MOB_TYPE_BASE) {
            myCachedBase = i;
        }
        myTypeCounts.increment(m->type);
    } else {
        /*
        * Otherwise we need to update myTypeCounts.
        */
        ASSERT(i < myMobs.size());
        ASSERT(myMobs[i].type == m->type);
        ASSERT(myTypeCounts.get(myMobs[i].type) > 0);
    }

    ASSERT(i < myMobs.size());
    myMobs[i] = *m;
}

void MobSet::removeMob(MobID badMobid)
{
    int i = myMap.get(badMobid);

    if (i == -1) {
        return;
    }

    ASSERT(myTypeCounts.get(myMobs[i].type) > 0);
    myTypeCounts.decrement(myMobs[i].type);

    if (i == myCachedBase) {
        myCachedBase = -1;
    }

    int last = myMobs.size() - 1;
    if (last != -1) {
        myMobs[i] = myMobs[last];
        myMap.put(myMobs[i].mobid, i);
        myMobs.shrink();

        if (myCachedBase == last) {
            myCachedBase = i;
        }
    }

    myMap.remove(badMobid);
}

Mob *MobSet::findNthClosestMob(const FPoint *pos,
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
        Mob *m = &myMobs[i];
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

Mob *MobSet::getBase()
{
    if (myCachedBase != -1) {
        ASSERT(myTypeCounts.get(MOB_TYPE_BASE) > 0);
        ASSERT(myCachedBase >= 0 && myCachedBase < myMobs.size());
        return &myMobs[myCachedBase];
    }

    if (myTypeCounts.get(MOB_TYPE_BASE) > 0) {
        for (uint i = 0; i < myMobs.size(); i++) {
            if (myMobs[i].type == MOB_TYPE_BASE) {
                myCachedBase = i;
                return &myMobs[i];
            }
        }

        NOT_REACHED();
    }

    return NULL;
}
