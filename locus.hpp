/*
 * locus.hpp -- part of SpaceRobots2
 * Copyright (C) 2022 Michael Banack <github@banack.net>
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

#ifndef _LOCUS_HPP_202210231659
#define _LOCUS_HPP_202210231659

#include "sensorGrid.hpp"

typedef enum LocusType {
    LOCUS_TYPE_ORBIT,
    // LOCUS_TYPE_PATROL_MAP,
    // LOCUS_TYPE_PATROL_POINTS,
} LocusType;

typedef enum LocusPoint {
    LOCUS_POINT_BASE,
    LOCUS_POINT_CENTER,
    // LOCUS_POINT_ENEMY_BASE,
    // LOCUS_POINT_ENEMY_BASE_GUESS,
    // LOCUS_POINT_MIDWAY,
    // LOCUS_POINT_MIDWAY_GUESS,
} LocusPoint;

typedef struct Locus {
    LocusType type;
    LocusPoint focusPoint;

    FPoint pos;
    float speed;
} Locus;

//void Locus_RunTick(Locus *locus

#endif // _LOCUS_HPP_202210231659