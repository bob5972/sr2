/*
 * geometry.h -- part of SpaceRobots2
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

#ifndef _GEOMETRY_H_202005310649
#define _GEOMETRY_H_202005310649

#include <math.h>
#include "mbassert.h"

typedef struct UPoint {
    uint x;
    uint y;
} UPoint;

typedef struct IPoint {
    int x;
    int y;
} IPoint;

typedef struct FPoint {
    float x;
    float y;
} FPoint;

typedef struct FCircle {
    FPoint center;
    float radius;
} FCircle;

typedef struct FQuad {
    float x;
    float y;
    float w;
    float h;
} FQuad;

static inline void FPoint_Clamp(FPoint *p, float xMin, float xMax,
                                float yMin, float yMax)
{
    ASSERT(xMin <= xMax);
    ASSERT(yMin <= yMax);

    if (p->x < xMin) {
        p->x = xMin;
    } else if (p->x > xMax) {
        p->x = xMax;
    }

    if (p->y < yMin) {
        p->y = yMin;
    } else if (p->y > yMax) {
        p->y = yMax;
    }
}

static inline float FPoint_Distance(const FPoint *a, const FPoint *b)
{
    float dx = (b->x - a->x);
    float dy = (b->y - a->y);
    float d = dx * dx + dy * dy;

    ASSERT(d >= 0);
    return sqrt(d);
}

static inline void FPoint_Midpoint(FPoint *m, const FPoint *a, const FPoint *b)
{
    m->x = (a->x + b->x) / 2.0f;
    m->y = (a->y + b->y) / 2.0f;
}

static inline bool FQuad_Intersect(const FQuad *a, const FQuad *b)
{
    if (a->x + a->w <= b->x) {
        return FALSE;
    }
    if (a->y + a->h <= b->y) {
        return FALSE;
    }
    if (a->x > b->x + b->w) {
        return FALSE;
    }
    if (a->y > b->y + b->h) {
        return FALSE;
    }
    return TRUE;
}

static inline bool FCircle_Intersect(const FCircle *a, const FCircle *b)
{
    float dx = a->center.x - b->center.x;
    float dy = a->center.y - b->center.y;
    float dr = a->radius + b->radius;

    if (dx * dx + dy * dy <= dr * dr) {
        if (UNLIKELY(a->radius == 0.0f || b->radius == 0.0f)) {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

#endif // _GEOMETRY_H_202005310649
