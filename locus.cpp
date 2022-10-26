/*
 * locus.cpp -- part of SpaceRobots2
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

#include "locus.hpp"
#include "textDump.hpp"


static TextMapEntry tmLTypes[] = {
    { TMENTRY(LOCUS_TYPE_INVALID),                    },
    { TMENTRY(LOCUS_TYPE_ORBIT),                      },
};

static TextMapEntry tmLPoints[] = {
    { TMENTRY(LOCUS_POINT_INVALID),                   },
    { TMENTRY(LOCUS_POINT_BASE),                      },
    { TMENTRY(LOCUS_POINT_CENTER),                    },
};

const char *LocusType_ToString(LocusType type)
{
    return TextMap_ToString(type, tmLTypes, ARRAYSIZE(tmLTypes));
}

const char *LocusPoint_ToString(LocusPoint pType)
{
    return TextMap_ToString(pType, tmLPoints, ARRAYSIZE(tmLPoints));
}

LocusType LocusType_FromString(const char *str)
{
    return (LocusType) TextMap_FromString(str, tmLTypes, ARRAYSIZE(tmLTypes));
}

LocusPoint LocusPoint_FromString(const char *str)
{
    return (LocusPoint) TextMap_FromString(str, tmLPoints, ARRAYSIZE(tmLPoints));
}

LocusType LocusType_Random()
{
    uint i = Random_Int(1, ARRAYSIZE(tmLTypes) - 1);
    ASSERT(ARRAYSIZE(tmLTypes) == LOCUS_TYPE_MAX);
    ASSERT(tmLTypes[0].value == LOCUS_TYPE_INVALID);
    return (LocusType) tmLTypes[i].value;
}

LocusPoint LocusPoint_Random()
{
    uint i = Random_Int(1, ARRAYSIZE(tmLPoints) - 1);
    ASSERT(ARRAYSIZE(tmLPoints) == LOCUS_POINT_MAX);
    ASSERT(tmLPoints[0].value == LOCUS_POINT_INVALID);
    return (LocusPoint) tmLPoints[i].value;
}

void Locus_Load(MBRegistry *mreg, LocusDesc *desc, const char *prefix)
{
    NOT_IMPLEMENTED();
}

void Locus_Init(AIContext *nc, LocusState *locus, LocusDesc *desc)
{
    ASSERT(locus != NULL);

    MBUtil_Zero(locus, sizeof(*locus));
    locus->desc = *desc;
    locus->active = FALSE;
}


bool LocusGetPoint(AIContext *nc, LocusPoint pType,
                   FPoint *focus)
{
    if (pType == LOCUS_POINT_BASE) {
        FPoint *pos = nc->sg->friendBasePos();
        if (pos != NULL) {
            *focus = *pos;
            return TRUE;
        }
        return FALSE;
    } else if (pType == LOCUS_POINT_CENTER) {
        focus->x = nc->ai->bp.width / 2;
        focus->y = nc->ai->bp.height / 2;
        return TRUE;
    } else {
        NOT_IMPLEMENTED();
    }
}

void Locus_RunTick(AIContext *nc, LocusState *locus)
{
    FPoint focusPoint;
    FPoint newPoint;
    bool hasFocus;
    FRPoint rp;

    ASSERT(locus != NULL);
    ASSERT(locus->desc.type == LOCUS_TYPE_ORBIT);

    hasFocus = LocusGetPoint(nc, locus->desc.orbitDesc.focus, &focusPoint);

    if (!hasFocus) {
        locus->active = FALSE;
        return;
    }

    if (!locus->active) {
        rp.theta = RandomState_Float(nc->rs, 0, M_PI * 2.0f);
    } else {
        FPoint_ToFRPoint(&locus->pos, &focusPoint, &rp);
    }
    rp.radius = locus->desc.orbitDesc.radius;

    rp.theta += M_PI * 2.0f / locus->desc.orbitDesc.period;
    rp.theta = fmodf(rp.theta, M_PI * 2.0f);

    FRPoint_ToFPoint(&rp, &focusPoint, &newPoint);

    if (!locus->active) {
        locus->active = TRUE;
        locus->pos = newPoint;
    } else if (!locus->desc.speedLimited ||
               FPoint_Distance(&locus->pos, &newPoint) <= locus->desc.speed) {
        locus->pos = newPoint;
    } else {
        FPoint_MoveToPointAtSpeed(&locus->pos, &newPoint, locus->desc.speed);
    }
}
