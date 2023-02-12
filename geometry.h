/*
 * geometry.h -- part of SpaceRobots2
 * Copyright (C) 2020-2022 Michael Banack <github@banack.net>
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

#ifdef __cplusplus
    extern "C" {
#endif

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

bool FPoint_Clamp(FPoint *p, float xMin, float xMax, float yMin, float yMax);

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

static inline float FPoint_ToRadius(const FPoint *p)
{
    float radius;
    ASSERT(p != NULL);
    radius = sqrtf((p->x * p->x) + (p->y * p->y));
    ASSERT(radius >= 0.0f);
    return radius;
}

static inline float FPoint_ToFRPointRadius(const FPoint *p, const FPoint *c)
{
    ASSERT(p != NULL);
    ASSERT(c != NULL);

    FPoint temp = *p;
    temp.x -= c->x;
    temp.y -= c->y;

    return FPoint_ToRadius(&temp);
}

static inline float FPoint_ToTheta(const FPoint *p)
{
    ASSERT(p != NULL);

    /*
     * We could use atan2f here to have it deal with the signs,
     * but then it gives negative angles.
     */
    float theta = atanf(p->y / p->x);

    if (isnanf(theta)) {
        theta = 0.0f;
    } else if (p->x < 0.0f) {
        theta += M_PI;
    } else if (theta < 0.0f) {
        theta += 2.0f * M_PI;
    }

    ASSERT(theta <= 2.0f * (float)M_PI);
    ASSERT(theta >= -2.0f * (float)M_PI);
    ASSERT(theta >= 0.0f);
    return theta;
}

static inline float FPoint_ToFRPointTheta(const FPoint *p, const FPoint *c)
{
    ASSERT(p != NULL);
    ASSERT(c != NULL);

    FPoint temp = *p;
    temp.x -= c->x;
    temp.y -= c->y;

    return FPoint_ToTheta(&temp);
}

static inline void FPoint_ToFRPoint(const FPoint *p, const FPoint *c,
                                    FRPoint *rp)
{
    ASSERT(p != NULL);
    ASSERT(rp != NULL);

    FPoint temp = *p;

    if (c != NULL) {
        temp.x -= c->x;
        temp.y -= c->y;
    }

    rp->radius = FPoint_ToRadius(&temp);
    rp->theta = FPoint_ToTheta(&temp);
}

static inline void FPoint_ToFRPointWithRadius(const FPoint *p, const FPoint *c,
                                              float radius, FRPoint *rp)
{
    ASSERT(p != NULL);
    ASSERT(rp != NULL);

    FPoint temp = *p;

    if (c != NULL) {
        temp.x -= c->x;
        temp.y -= c->y;
    }

    rp->radius = radius;
    rp->theta = FPoint_ToTheta(&temp);
}

void FRPoint_ToFPoint(const FRPoint *rp, const FPoint *c, FPoint *p);

bool FPoint_IsFacing(const FPoint *p, const FPoint *c, const FRPoint *dir,
                     bool forward);

static inline bool
FPoint_IsFacingFPoint(const FPoint *pp, const FPoint *pc,
                      const FPoint *dp, const FPoint *dc,
                      bool forward)
{
    FPoint pv;
    FPoint dv;

    FPoint_Subtract(pp, pc, &pv);
    FPoint_Subtract(dp, dc, &dv);

    float dot = pv.x * dv.x + pv.y * dv.y;
    return forward ? dot >= 0 : dot < 0;
}

static inline bool
FPoint_IsFacingFPointVec(const FPoint *pp, const FPoint *pc,
                         const FPoint *dv, bool forward)
{
    FPoint pv;

    FPoint_Subtract(pp, pc, &pv);

    float dot = pv.x * dv->x + pv.y * dv->y;
    return forward ? dot >= 0 : dot < 0;
}

static inline void FPoint_Add(const FPoint *lhs, const FPoint *rhs,
                              FPoint *result)
{
    result->x = lhs->x + rhs->x;
    result->y = lhs->y + rhs->y;
}

static inline void FRPoint_Add(const FRPoint *lhs, const FRPoint *rhs,
                               FRPoint *result)
{
    FPoint vl, vr;
    FPoint vs;

    FRPoint_ToFPoint(lhs, NULL, &vl);
    FRPoint_ToFPoint(rhs, NULL, &vr);

    FPoint_Add(&vl, &vr, &vs);

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

void Geometry_UnitTest();

#ifdef __cplusplus
    }
#endif

#endif // _GEOMETRY_H_202005310649
