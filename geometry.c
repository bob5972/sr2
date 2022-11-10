/*
 * geometry.c -- part of SpaceRobots2
 * Copyright (C) 2022 Michael Banack <github@banack.net>
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

#include "geometry.h"

bool FPoint_Clamp(FPoint *p, float xMin, float xMax,
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

void FPoint_ToFRPoint(const FPoint *p, const FPoint *c, FRPoint *rp)
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

void Geometry_UnitTest()
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
