/*
 * mobSet.cpp -- part of SpaceRobots2
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

#include "mobSet.hpp"

void MobSet::makeEmpty()
{
    myMobs.makeEmpty();
    myMap.makeEmpty();
    myCachedBase = -1;

    for (uint i = 0; i < myTypeCounts.size(); i++) {
        myTypeCounts[i] = 0;
    }
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
        myTypeCounts[m->type]++;
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
    myTypeCounts[myMobs[i].type]--;

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

Mob *MobSet::findClosestMob(const FPoint *pos,
                            MobTypeFlags filter)
{
    Mob *best = NULL;
    float bestDistance;
    uint32 size = myMobs.size();

    ASSERT(filter != 0);

    for (uint i = 0; i < size; i++) {
        Mob *m = &myMobs[i];
        if (((1 << m->type) & filter) != 0) {
            float curDistance = FPoint_DistanceSquared(pos, &m->pos);
            if (best == NULL || curDistance < bestDistance) {
                best = m;
                bestDistance = curDistance;
            }
        }
    }

    return best;
}

void MobSet::pushMobs(MBVector<Mob *> &v, MobTypeFlags filter) {
    v.ensureCapacity(v.size() + myMobs.size());

    for (uint i = 0; i < myMobs.size(); i++) {
        Mob *m = &myMobs[i];
        if (((1 << m->type) & filter) != 0) {
            v.push(m);
        }
    }
}

Mob *MobSet::findNthClosestMob(const FPoint *pos,
                               MobTypeFlags filter, int n)
{
    uint32 size = myMobs.size();

    ASSERT(n >= 0);
    ASSERT(filter != 0);

    if (n == 0) {
        return findClosestMob(pos, filter);
    } else if (n >= size) {
        return NULL;
    }

    MBVector<Mob *> v;
    v.ensureCapacity(size);

    for (uint i = 0; i < size; i++) {
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
