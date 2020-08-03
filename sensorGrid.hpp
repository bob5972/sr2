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
    SensorGrid() {
        map.setEmptyValue(-1);
    }

    void updateTick(FleetAI *ai) {
        NOT_IMPLEMENTED();
    }

    Mob *findClosestMob(const FPoint *pos, MobTypeFlags filter);
    MOb *findNthClosestMob(const FPoint *pos, MobTypeFlags filter, int n);
    Mob *findClosestMobInRange(const FPoint *pos, MobTypeFlags filter,
                               float radius);
    Mob *get(MobID mobid);
    void remove(MobID mobid);
    void add(Mob *mob);

    /*
     * Find the enemy base closest to your base.
     */
    Mob *enemyBase();

private:
    struct Target {
        Mob mob;
        uint lastSeenTick;
    };
    IntMap map;
    MBVector<Target> mobs;
}


#endif // _SENSORGRID_H_202008021543
