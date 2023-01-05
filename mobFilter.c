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


bool MobFilter_IsTriviallyEmpty(const MobFilter *mf)
{
    ASSERT(mf != NULL);
    ASSERT(mf->filterTypeFlags < MOB_FILTER_TFLAG_MAX);

    if (mf->filterTypeFlags == 0) {
        return TRUE;
    }

    if ((mf->filterTypeFlags & MOB_FILTER_TFLAG_TYPE) != 0 &&
        mf->typeF.flags == MOB_FLAG_NONE) {
        return TRUE;
    }

    if ((mf->filterTypeFlags & MOB_FILTER_TFLAG_RANGE) != 0 &&
        mf->rangeF.radius <= 0.0f) {
        return TRUE;
    }

    return FALSE;
}

bool MobFilter_Filter(const Mob *m, const MobFilter *mf)
{
    uint32 flags;
    ASSERT(m != NULL);
    ASSERT(mf != NULL);

    flags = mf->filterTypeFlags;

    if (mb_debug) {
        ASSERT(flags < MOB_FILTER_TFLAG_MAX);
        if ((flags & MOB_FILTER_TFLAG_FN) == 0) {
            ASSERT(mf->fnF.func == NULL);
        } else {
            ASSERT(mf->fnF.func != NULL);
        }
    }

    while (flags != 0) {
        uint32 index = MBUtil_FFS(flags);
        ASSERT(index > 0);
        uint32 bit = 1 << (index - 1);
        flags &= ~bit;

        switch (bit) {
            case MOB_FILTER_TFLAG_TYPE:
                if (((1 << m->type) & mf->typeF.flags) == 0) {
                    return FALSE;
                }
                break;
            case MOB_FILTER_TFLAG_RANGE:
                if (mf->rangeF.radius <= 0.0f) {
                    return FALSE;
                }
                if (FPoint_DistanceSquared(&mf->rangeF.pos, &m->pos) >
                    mf->rangeF.radius * mf->rangeF.radius) {
                    return FALSE;
                }
                break;
            case MOB_FILTER_TFLAG_FN:
                ASSERT(mf->fnF.func != NULL);
                if (!mf->fnF.func(mf->fnF.cbData, m)) {
                    return FALSE;
                }
                break;
            case MOB_FILTER_TFLAG_DIRP:
                if (!FPoint_IsFacingFPointVec(&m->pos, &mf->dirPF.pos,
                                              &mf->dirPF.dir,
                                              mf->dirPF.forward)) {
                    return FALSE;
                }
                break;
            default:
                NOT_REACHED();
        }
    }

    return TRUE;
}
