/*
 * geometry.h --
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

static inline float FPoint_Distance(const FPoint *a, const FPoint *b)
{
    float dx = (b->x - a->x);
    float dy = (b->y - a->y);
    float d = dx * dx + dy * dy;

    ASSERT(d >= 0);
    return sqrt(d);
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
//    FQuad aBox, bBox;
//    aBox.x = a->center.x - a->radius;
//    aBox.y = a->center.y - a->radius;
//    aBox.w = 2 * a->radius;
//    aBox.h = 2 * a->radius;
//
//    bBox.x = b->center.x - b->radius;
//    bBox.y = b->center.y - b->radius;
//    bBox.w = 2 * b->radius;
//    bBox.h = 2 * b->radius;
//
//    if (!FQuad_Intersect(&aBox, &bBox)) {
//        return FALSE;
//    }

    float dx = a->center.x - b->center.x;
    float dy = a->center.y - b->center.y;
    float dr = a->radius + b->radius;

    if (dx * dx + dy * dy <= dr * dr) {
        return TRUE;
    }
    return FALSE;
}

#endif // _GEOMETRY_H_202005310649
