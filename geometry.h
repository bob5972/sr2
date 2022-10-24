/*
 * geometry.h -- part of SpaceRobots2
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

#ifndef _GEOMETRY_H_202005310649
#define _GEOMETRY_H_202005310649

#include <math.h>
#include "MBAssert.h"

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


static inline bool
Float_Compare(float lhs, float rhs, float tolerance)
{
    float d = lhs - rhs;
    if (fabsf(d) <= tolerance) {
        return TRUE;
    }
    return FALSE;
}

/*
 * Returns the speed in radians of the angle change of a point moving at the
 * specified linear speed along the circumference of a circle of the specified
 * radius.
 */
static inline float Float_AngularSpeed(float radius, float speed)
{
    return speed / radius;
}

static inline bool FPoint_Clamp(FPoint *p, float xMin, float xMax,
                                float yMin, float yMax)
{
    bool clamped = FALSE;

    ASSERT(xMin <= xMax);
    ASSERT(yMin <= yMax);

    if (isnan(p->x) || p->x < xMin) {
        p->x = xMin;
        clamped = TRUE;
    } else if (p->x > xMax) {
        p->x = xMax;
        clamped = TRUE;
    }

    if (isnan(p->y) || p->y < yMin) {
        p->y = yMin;
        clamped = TRUE;
    } else if (p->y > yMax) {
        p->y = yMax;
        clamped = TRUE;
    }

    return clamped;
}

static inline float FPoint_DistanceSquared(const FPoint *a, const FPoint *b)
{
    float dx = (b->x - a->x);
    float dy = (b->y - a->y);
    return dx * dx + dy * dy;
}

static inline float FPoint_Distance(const FPoint *a, const FPoint *b)
{
    return sqrtf(FPoint_DistanceSquared(a, b));
}

static inline void
FPoint_MoveToPointAtSpeed(FPoint *pos, const FPoint *target, float speed)
{
    float distance = FPoint_Distance(pos, target);

    if (distance <= speed) {
        *pos = *target;
    } else {
        float dx = target->x - pos->x;
        float dy = target->y - pos->y;
        float factor = speed / distance;

        pos->x = pos->x + dx * factor;
        pos->y = pos->y + dy * factor;
    }
}

static inline void FPoint_Midpoint(FPoint *m, const FPoint *a, const FPoint *b)
{
    m->x = (a->x + b->x) / 2.0f;
    m->y = (a->y + b->y) / 2.0f;
}

static inline void FPoint_Subtract(const FPoint *a, const FPoint *b,
                                   FPoint *result)
{
    result->x = a->x - b->x;
    result->y = a->y - b->y;
}

static inline void FRPoint_Zero(FRPoint *rp)
{
    ASSERT(rp != NULL);
    rp->radius = 0.0f;
    rp->theta = 0.0f;
}

static inline void FPoint_Zero(FPoint *p)
{
    ASSERT(p != NULL);
    p->x = 0.0f;
    p->y = 0.0f;
}

static inline void FRPoint_SetSpeed(FRPoint *p, float s)
{
    ASSERT(p->radius >= 0.0f);

    if (s >= 0.0f) {
        p->radius = s;
    } else {
        p->radius = -s;
        p->theta += M_PI;
    }
}

static inline void FPoint_ToFRPoint(const FPoint *p, const FPoint *c, FRPoint *rp)
{
    FPoint zero;

    ASSERT(p != NULL);
    ASSERT(rp != NULL);

    if (c == NULL) {
        FPoint_Zero(&zero);
        c = &zero;
    }

    FPoint temp = *p;
    temp.x -= c->x;
    temp.y -= c->y;

    rp->radius = sqrtf((temp.x * temp.x) + (temp.y * temp.y));

    /*
     * We could use atan2f here to have it deal with the signs,
     * but then it gives negative angles.
     */
    rp->theta = atanf(temp.y / temp.x);

    if (isnanf(rp->theta)) {
        rp->theta = 0.0f;
    } else if (temp.x < 0.0f) {
        rp->theta += M_PI;
    } else if (rp->theta < 0.0f) {
        rp->theta += 2.0f * M_PI;
    }

    ASSERT(rp->theta <= 2.0f * (float)M_PI);
    ASSERT(rp->theta >= -2.0f * (float)M_PI);
    ASSERT(rp->theta >= 0.0f);
    ASSERT(rp->radius >= 0.0f);
}

static inline void FRPoint_ToFPoint(const FRPoint *rp, const FPoint *c, FPoint *p)
{
    FPoint zero;

    ASSERT(p != NULL);
    ASSERT(rp != NULL);

    if (c == NULL) {
        FPoint_Zero(&zero);
        c = &zero;
    }

    FPoint temp;

    temp.x = rp->radius * cosf(rp->theta);
    temp.y = rp->radius * sinf(rp->theta);
    temp.x += c->x;
    temp.y += c->y;

    *p = temp;
}

static inline void FRPoint_Add(const FRPoint *lhs, const FRPoint *rhs,
                               FRPoint *result)
{
    FPoint vl, vr;
    FPoint vs;

    FRPoint_ToFPoint(lhs, NULL, &vl);
    FRPoint_ToFPoint(rhs, NULL, &vr);

    vs.x = vl.x + vr.x;
    vs.y = vl.y + vr.y;

    FPoint_ToFRPoint(&vs, NULL, result);
}

static inline void FRPoint_WAvg(const FRPoint *lhs, float lw,
                                const FRPoint *rhs, float rw,
                                FRPoint *result)
{
    FRPoint lhsW, rhsW;
    FPoint vl, vr;
    FPoint vs;

    lhsW = *lhs;
    lhsW.radius *= lw;

    rhsW = *rhs;
    rhsW.radius *= rw;

    FRPoint_ToFPoint(&lhsW, NULL, &vl);
    FRPoint_ToFPoint(&rhsW, NULL, &vr);

    vs.x = vl.x + vr.x;
    vs.y = vl.y + vr.y;

    FPoint_ToFRPoint(&vs, NULL, result);
    result->radius /= (lw + rw);
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

static inline void Geometry_UnitTest()
{
    bool print = FALSE;
    FPoint p, p2, p3, c;
    FRPoint r, r2, r3;

    p.x = 1.0f;
    p.y = 1.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = -1.0f;
    p.y = 1.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = -1.0f;
    p.y = -1.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = 1.0f;
    p.y = -1.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = 0.0f;
    p.y = -1.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = 0.0f;
    p.y = 1.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = 1.0f;
    p.y = 0.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = -1.0f;
    p.y = 0.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FRPoint_ToFPoint(&r, NULL, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("%s:%d p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    p.x = 1.0f;
    p.y = 1.0f;
    p2.x = -1.0f;
    p2.y = -1.0f;
    FPoint_ToFRPoint(&p, NULL, &r);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    FRPoint_Add(&r, &r2, &r);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), p2.xy(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, p2.x, p2.y);
        Warning("%s:%d p + p2=rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, r.radius, r.theta);
        Warning("\n");
    }

    p.x = 0.0f;
    p.y = -95.4f;
    FPoint_ToFRPoint(&p, NULL, &r);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, r.radius, r.theta);
        Warning("\n");
    }

    c.x = 1092.5;
    c.y = 95.4;
    p.x = c.x;
    p.y = 0.0;

    FPoint_Subtract(&p, &c, &p2);
    FPoint_ToFRPoint(&p2, NULL, &r2);
    FPoint_ToFRPoint(&p, &c, &r);
    if (print) {
        Warning("%s:%d  p.xy(%0.1f, %0.1f), c.xy(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p.x, p.y, c.x, c.y);

        Warning("%s:%d  p - c = xy(%0.1f, %0.1f) rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r.radius, r.theta);
        Warning("%s:%d                      r2.rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, r2.radius, r2.theta);
    }

    r2 = r;
    r2.radius *= -1.0f;
    FRPoint_ToFPoint(&r2, NULL, &p2);
    if (print) {
        Warning("%s:%d  p2.xy(%0.1f, %0.1f), rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p2.x, p2.y, r2.radius, r2.theta);
    }

    FPoint_ToFRPoint(&p2, NULL, &r3);
    if (print) {
        Warning("%s:%d  r3 rt(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, r3.radius, r3.theta);
    }

    FRPoint_ToFPoint(&r3, NULL, &p3);
    if (print) {
        Warning("%s:%d  p3.xy(%0.1f, %0.1f)\n",
                __FUNCTION__, __LINE__, p3.x, p3.y);
    }

    if (print) {
        NOT_IMPLEMENTED();
    }
}

#endif // _GEOMETRY_H_202005310649
