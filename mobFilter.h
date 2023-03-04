/*
 * mobFilter.h -- part of SpaceRobots2
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

#ifndef _MOB_FILTER_H_20230104
#define _MOB_FILTER_H_20230104

#ifdef __cplusplus
	extern "C" {
#endif

#include "geometry.h"
#include "MBVector.h"
#include "battleTypes.h"
#include "MBCompare.h"

#define MOB_FILTER_TFLAG_TYPE   (1 << 0)
#define MOB_FILTER_TFLAG_FN     (1 << 1)
#define MOB_FILTER_TFLAG_RANGE  (1 << 2)
#define MOB_FILTER_TFLAG_DIRP   (1 << 3)
#define MOB_FILTER_TFLAG_EMPTY  (1 << 4)
#define MOB_FILTER_TFLAG_MAX    (1 << 5)

typedef bool (*MobFilterFn)(void *cbData, const Mob *m);

typedef struct MobFilter {
    uint filterTypeFlags;

    struct {
        MobTypeFlags flags;
    } typeF;

    struct {
        void *cbData;
        MobFilterFn func;
    } fnF;

    struct {
        FPoint pos;
        float radiusSquared;
    } rangeF;

    /*
     * Filter for mobs forward/backwards from the specified center point
     * and direction (as an FPoint from (0,0)).
     */
    struct {
        FPoint pos;
        FPoint dir;
        bool forward;
    } dirPF;
} MobFilter;

bool MobFilter_Filter(const Mob *m, const MobFilter *f);
bool MobFilter_IsTriviallyEmpty(const MobFilter *filter);
void MobFilter_Batch(Mob **ma, uint *n, const MobFilter *mf);

static inline void MobFilter_Init(MobFilter *mf)
{
    MBUtil_Zero(mf, sizeof(*mf));
    ASSERT(mf->filterTypeFlags < MOB_FILTER_TFLAG_MAX);
}

static inline void MobFilter_UseType(MobFilter *mf, MobTypeFlags flags)
{
    ASSERT(mf->filterTypeFlags < MOB_FILTER_TFLAG_MAX);
    ASSERT((mf->filterTypeFlags & MOB_FILTER_TFLAG_TYPE) == 0);
    ASSERT(mf->typeF.flags == 0);

    mf->filterTypeFlags |= MOB_FILTER_TFLAG_TYPE;
    mf->typeF.flags = flags;
}

static inline void MobFilter_UseFn(MobFilter *mf, MobFilterFn func,
                                   void *cbData)
{
    ASSERT(mf->filterTypeFlags < MOB_FILTER_TFLAG_MAX);
    ASSERT((mf->filterTypeFlags & MOB_FILTER_TFLAG_FN) == 0);
    ASSERT(mf->typeF.flags == 0);

    mf->filterTypeFlags |= MOB_FILTER_TFLAG_FN;
    mf->fnF.cbData = cbData;
    mf->fnF.func = func;
}

static inline void MobFilter_UseRange(MobFilter *mf, const FPoint *pos,
                                      float radius)
{
    ASSERT(mf->filterTypeFlags < MOB_FILTER_TFLAG_MAX);
    ASSERT((mf->filterTypeFlags & MOB_FILTER_TFLAG_RANGE) == 0);

    if (radius <= 0.0f) {
        mf->filterTypeFlags |= MOB_FILTER_TFLAG_EMPTY;
    } else {
        mf->filterTypeFlags |= MOB_FILTER_TFLAG_RANGE;
        mf->rangeF.pos = *pos;
        mf->rangeF.radiusSquared = radius * radius;
    }
}

static inline void MobFilter_UseDirP(MobFilter *mf, const FPoint *pos,
                                     const FPoint *dir,
                                     bool forward)
{
    ASSERT(mf->filterTypeFlags < MOB_FILTER_TFLAG_MAX);
    ASSERT((mf->filterTypeFlags & MOB_FILTER_TFLAG_DIRP) == 0);

    mf->filterTypeFlags |= MOB_FILTER_TFLAG_DIRP;
    mf->dirPF.pos = *pos;
    mf->dirPF.dir = *dir;
    mf->dirPF.forward = forward;
}

static inline void MobFilter_UseDirR(MobFilter *mf, const FPoint *pos,
                                     const FRPoint *dir,
                                     bool forward)
{
    FPoint fdir;
    FRPoint_ToFPoint(dir, pos, &fdir);
    MobFilter_UseDirP(mf, pos, &fdir, forward);
}

#ifdef __cplusplus
    }
#endif

#endif // _MOB_FILTER_H_20230104
