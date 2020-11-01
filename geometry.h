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

typedef struct FRPoint {
    float radius;
    float theta;
} FRPoint;

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


static INLINE bool
Float_Compare(float lhs, float rhs, float tolerance)
{
    float d = lhs - rhs;
    if (fabsf(d) <= tolerance) {
        return TRUE;
    }
    return FALSE;
}

static inline bool FPoint_Clamp(FPoint *p, float xMin, float xMax,
                                float yMin, float yMax)
{
    bool clamped = FALSE;

    ASSERT(xMin <= xMax);
    ASSERT(yMin <= yMax);

    if (p->x < xMin) {
        p->x = xMin;
        clamped = TRUE;
    } else if (p->x > xMax) {
        p->x = xMax;
        clamped = TRUE;
    }

    if (p->y < yMin) {
        p->y = yMin;
        clamped = TRUE;
    } else if (p->y > yMax) {
        p->y = yMax;
        clamped = TRUE;
    }

    return clamped;
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


static inline void FPoint_ToFRPoint(const FPoint *p, const FPoint *c, FRPoint *rp)
{
    ASSERT(p != NULL);
    ASSERT(c != NULL);
    ASSERT(rp != NULL);

    FPoint temp = *p;
    temp.x -= c->x;
    temp.y -= c->y;

    rp->radius = sqrt((temp.x * temp.x) + (temp.y * temp.y));
    rp->theta = atanf(temp.y / temp.x);

    if (isnanf(rp->theta)) {
        rp->theta = 0.0f;
    } else if (temp.x <= 0.0f) {
        rp->theta += M_PI;
    }
}

static inline void FRPoint_ToFPoint(const FRPoint *rp, const FPoint *c, FPoint *p)
{
    ASSERT(p != NULL);
    ASSERT(c != NULL);
    ASSERT(rp != NULL);

    FPoint temp;

    temp.x = rp->radius * cosf(rp->theta);
    temp.y = rp->radius * sinf(rp->theta);
    temp.x += c->x;
    temp.y += c->y;

    *p = temp;
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

static inline bool FCircle_ContainsPoint(const FCircle *a, const FPoint *p)
{
    float dx = a->center.x - p->x;
    float dy = a->center.y - p->y;
    float dr = a->radius;

    if (dx * dx + dy * dy <= dr * dr) {
        if (UNLIKELY(a->radius == 0.0f)) {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

static inline void FCircle_CenterToIPoint(const FCircle *c, IPoint *p)
{
    //p->x = lrintf(c->center.x);
    //p->y = lrintf(c->center.y);

    ASSERT(c->center.x >= 0);
    ASSERT(c->center.y >= 0);

    p->x = (int)(c->center.x + 0.5f);
    p->y = (int)(c->center.y + 0.5f);
}

#endif // _GEOMETRY_H_202005310649
