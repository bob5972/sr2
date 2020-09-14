/*
 * mobSet.hpp -- part of SpaceRobots2
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

#ifndef _MOBSET_H_202009132004
#define _MOBSET_H_202009132004

extern "C" {
#include "battleTypes.h"
#include "mob.h"
}

#include "IntMap.hpp"
#include "MBVector.hpp"

class MobSet {
public:
    MobSet() {
        myNumTrackedBases = 0;
        myCachedBase = -1;
        myMap.setEmptyValue(-1);
        myMobs.pin();
    }

    ~MobSet() {
        myMobs.unpin();
    }

    void updateMob(Mob *m);

    void removeMob(MobID badMobid);

    Mob *getBase();

    void makeEmpty();

    void pin() {
        myMobs.pin();
    }

    void unpin() {
        myMobs.unpin();
    }

    /**
        * Find the Nth closest mob to the specified point.
        * This is 0-based, so the closest mob is found when n=0.
        */
    Mob *findNthClosestMob(const FPoint *pos,
                            MobTypeFlags filter, int n);

    int myNumTrackedBases;
    int myCachedBase;
    IntMap myMap;
    MBVector<Mob> myMobs;
};

#endif //_MOBSET_H_202009132004
