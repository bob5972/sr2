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

#include <immintrin.h>

#include "mobFilter.h"


bool MobFilter_IsTriviallyEmpty(const MobFilter *mf)
{
    ASSERT(mf != NULL);
    ASSERT(mf->filterTypeFlags < MOB_FILTER_TFLAG_MAX);

    if ((mf->filterTypeFlags & MOB_FILTER_TFLAG_EMPTY) != 0) {
        return TRUE;
    }

    if ((mf->filterTypeFlags & MOB_FILTER_TFLAG_TYPE) != 0 &&
        mf->typeF.flags == MOB_FLAG_NONE) {
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
            case MOB_FILTER_TFLAG_EMPTY:
                return FALSE;
            case MOB_FILTER_TFLAG_TYPE:
                if (((1 << m->type) & mf->typeF.flags) == 0) {
                    return FALSE;
                }
                break;
            case MOB_FILTER_TFLAG_RANGE:
                if (FPoint_DistanceSquared(&mf->rangeF.pos, &m->pos) >
                    mf->rangeF.radiusSquared) {
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

    ASSERT(!MobFilter_IsTriviallyEmpty(mf));
    return TRUE;
}

#ifdef __AVX__
static inline __m256 MobFilterRangeAVX(__m256 sx, __m256 sy, __m256 sr2,
                                       __m256 mx, __m256 my)
{
    __m256 dx = _mm256_sub_ps(mx, sx);
    __m256 dy = _mm256_sub_ps(my, sy);
    __m256 dx2 = _mm256_mul_ps(dx, dx);
    __m256 dy2 = _mm256_mul_ps(dy, dy);
    __m256 dd = _mm256_add_ps(dx2, dy2);
    return _mm256_cmp_ps(dd, sr2, _CMP_LE_OS);
}
#endif // __AVX__


#ifdef __AVX__
#define VSIZE 8
static void MobFilterRangeBatch(const FPoint *pos, float radiusSquared,
                                float *x, float *y,
                                Mob **maIn, uint size,
                                Mob **maOut, uint *outSize)
{
    uint maI = 0;
    uint goodN = 0;

    union {
        float f[VSIZE];
        uint32 u[VSIZE];
    } result;

    __m256 sx, sy, sr2;

    sx = _mm256_broadcast_ss(&pos->x);
    sy = _mm256_broadcast_ss(&pos->y);
    sr2 = _mm256_broadcast_ss(&radiusSquared);

    while (maI + VSIZE < size) {
        __m256 mx = _mm256_load_ps(&x[maI]);
        __m256 my = _mm256_load_ps(&y[maI]);
        __m256 cmp = MobFilterRangeAVX(sx, sy, sr2, mx, my);
        _mm256_storeu_ps(&result.f[0], cmp);

        for (uint32 i = 0; i < VSIZE; i++) {
            if (result.u[i] != 0) {
                ASSERT(maI + i >= goodN);
                Mob *m = maIn[maI + i];
                maOut[goodN] = m;
                goodN++;
            }
        }

        maI += VSIZE;
    }

    while (maI < size) {
        Mob *m = maIn[maI];
        if (FPoint_DistanceSquared(pos, &m->pos) <= radiusSquared) {
            ASSERT(maI >= goodN);
            maOut[goodN] = m;
            goodN++;
        }
        maI++;
    }

    *outSize += goodN;
}
#undef VSIZE
#endif // __AVX__

void MobFilter_Batch(Mob **ma, uint *n, const MobFilter *mf)
{
    uint goodN = 0;
    uint ln = *n;
    MobFilter noRStack;
    const MobFilter *lmf = mf;

#ifdef __AVX__
    if ((mf->filterTypeFlags & MOB_FILTER_TFLAG_RANGE) != 0) {
        noRStack = *mf;
        noRStack.filterTypeFlags &= ~MOB_FILTER_TFLAG_RANGE;
        lmf = &noRStack;
    }
#endif // __AVX__

    if (!MobFilter_IsTriviallyEmpty(lmf)) {
        for (uint x = 0; x < ln; x++) {
            if (MobFilter_Filter(ma[x], lmf)) {
                ma[goodN] = ma[x];
                goodN++;
            }
        }
    }

#ifdef __AVX__
#define BSIZE 256
    if ((mf->filterTypeFlags & MOB_FILTER_TFLAG_RANGE) != 0) {
        FPoint pos = mf->rangeF.pos;
        float radiusSquared = mf->rangeF.radiusSquared;
        uint i = 0;
        ln = goodN;
        goodN = 0;
        while (i < ln) {
            float x[256] __attribute__ ((aligned (32)));
            float y[256] __attribute__ ((aligned (32)));
            uint an = 0;
            uint32 iStart = i;

            while (an < ARRAYSIZE(x) && i < ln) {
                x[an] = ma[i]->pos.x;
                y[an] = ma[i]->pos.y;
                i++;
                an++;
            }

            MobFilterRangeBatch(&pos, radiusSquared,
                                x, y, &ma[iStart], an, &ma[goodN], &goodN);
        }
    }
#undef BSIZE
#endif // __AVX__

    *n = goodN;
}
