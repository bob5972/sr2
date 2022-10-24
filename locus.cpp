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

void Locus_Load(MBRegistry *mreg, LocusDesc *desc, const char *prefix);

void Locus_Init(AIContext *nc, LocusState *locus, LocusDesc *desc)
{
    ASSERT(locus != NULL);

    MBUtil_Zero(locus, sizeof(*locus));
    locus->desc = *desc;
    locus->active = FALSE;
}


void Locus_RunTick(AIContext *nc, LocusState *locus)
{
    ASSERT(locus->desc.type == LOCUS_TYPE_ORBIT);

    NOT_IMPLEMENTED();
}
