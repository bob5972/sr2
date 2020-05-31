/*
 * geometry.h --
 */

#ifndef _GEOMETRY_H_202005310649
#define _GEOMETRY_H_202005310649

#include <math.h>
#include "mbassert.h"

typedef struct FPoint {
    float x;
    float y;
} FPoint;

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

#endif // _GEOMETRY_H_202005310649
