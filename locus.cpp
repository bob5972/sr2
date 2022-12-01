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

const char *LocusType_ToString(LocusType type)
{
    return TextMap_ToString(type, tmLTypes, ARRAYSIZE(tmLTypes));
}

LocusType LocusType_FromString(const char *str)
{
    return (LocusType) TextMap_FromString(str, tmLTypes, ARRAYSIZE(tmLTypes));
}

LocusType LocusType_Random()
{
    uint i = Random_Int(1, ARRAYSIZE(tmLTypes) - 1);
    ASSERT(ARRAYSIZE(tmLTypes) == LOCUS_TYPE_MAX);
    ASSERT(tmLTypes[0].value == LOCUS_TYPE_INVALID);
    return (LocusType) tmLTypes[i].value;
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


void Locus_RunTick(AIContext *nc, LocusState *locus)
{
    FPoint newPoint;
    FRPoint rp;

    ASSERT(locus != NULL);
    ASSERT(locus->desc.type == LOCUS_TYPE_ORBIT);

    if (!locus->active) {
        rp.theta = RandomState_Float(nc->rs, 0, M_PI * 2.0f);
    } else {
        FPoint_ToFRPoint(&locus->pos, &locus->desc.orbitDesc.focus, &rp);
    }
    rp.radius = locus->desc.orbitDesc.radius;

    rp.theta += M_PI * 2.0f / locus->desc.orbitDesc.period;
    rp.theta = fmodf(rp.theta, M_PI * 2.0f);

    FRPoint_ToFPoint(&rp, &locus->desc.orbitDesc.focus, &newPoint);

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
