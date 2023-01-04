/*
 * mobFilter.h -- part of SpaceRobots2
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

#ifndef _MOB_FILTER_H_20230104
#define _MOB_FILTER_H_20230104

#ifdef __cplusplus
	extern "C" {
#endif

#include "geometry.h"
#include "MBVector.h"
#include "battleTypes.h"
#include "MBCompare.h"


typedef struct MobFilter {
    struct {
        bool useFlags;
        MobTypeFlags flags;
    } flagsFilter;

    struct {
        void *cbData;
        bool (*func)(void *cbData, const Mob *m);
    } fnFilter;

    struct {
        bool useRange;
        FPoint pos;
        float radius;
    } rangeFilter;

    /*
     * Filter for mobs forward/backwards from the specified center point
     * and direction.
     */
    struct {
        bool useDir;
        FPoint pos;
        FRPoint dir;
        bool forward;
    } dirFilter;

    /*
     * Filter for mobs forward/backwards from the specified center point
     * and direction (as an FPoint from (0,0)).
     */
    struct {
        bool useDir;
        FPoint pos;
        FPoint dir;
        bool forward;
    } dirFPointFilter;
} MobFilter;

bool MobFilter_Filter(const Mob *m, const MobFilter *f);

static inline bool MobFilter_IsTriviallyEmpty(const MobFilter *filter) {
    if (filter->flagsFilter.useFlags &&
        filter->flagsFilter.flags == MOB_FLAG_NONE) {
        return TRUE;
    }
    if (filter->rangeFilter.useRange &&
        filter->rangeFilter.radius <= 0.0f) {
        return TRUE;
    }
    return FALSE;
}

#ifdef __cplusplus
    }
#endif

#endif // _MOB_FILTER_H_20230104
