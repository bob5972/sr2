/*
 * mobFilter.c -- part of SpaceRobots2
 * Copyright (C) 2020-2023 Michael Banack <github@banack.net>
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

#include "mobFilter.h"

bool MobFilter_Filter(const Mob *m, const MobFilter *f)
{
    ASSERT(m != NULL);
    ASSERT(f != NULL);

    if (f->flagsFilter.useFlags) {
        if (((1 << m->type) & f->flagsFilter.flags) == 0) {
            return FALSE;
        }
    }
    if (f->rangeFilter.useRange) {
        if (f->rangeFilter.radius <= 0.0f) {
            return FALSE;
        }
        if (FPoint_DistanceSquared(&f->rangeFilter.pos, &m->pos) >
            f->rangeFilter.radius * f->rangeFilter.radius) {
            return FALSE;
        }
    }
    if (f->fnFilter.func != NULL) {
        if (!f->fnFilter.func(f->fnFilter.cbData, m)) {
            return FALSE;
        }
    }
    if (f->dirFilter.useDir) {
        if (!FPoint_IsFacing(&m->pos, &f->dirFilter.pos, &f->dirFilter.dir,
                             f->dirFilter.forward)) {
            return FALSE;
        }
    }
    if (f->dirFPointFilter.useDir) {
        if (!FPoint_IsFacingFPointVec(&m->pos, &f->dirFPointFilter.pos,
                                      &f->dirFPointFilter.dir,
                                      f->dirFPointFilter.forward)) {
            return FALSE;
        }
    }

    return TRUE;
}
