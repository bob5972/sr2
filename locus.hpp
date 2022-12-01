/*
 * locus.hpp -- part of SpaceRobots2
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

#ifndef _LOCUS_HPP_202210231659
#define _LOCUS_HPP_202210231659

#include "sensorGrid.hpp"
#include "aiTypes.hpp"

typedef enum LocusType {
    LOCUS_TYPE_INVALID,
    LOCUS_TYPE_ORBIT,
    // LOCUS_TYPE_PATROL_MAP,
    // LOCUS_TYPE_PATROL_POINTS,

    LOCUS_TYPE_MAX,
} LocusType;

typedef struct LocusOrbitDesc {
    FPoint focus;
    float radius;
    float period;
} LocusOrbitDesc;

typedef struct LocusDesc {
    LocusType type;

    union {
        LocusOrbitDesc orbitDesc;
    };

    bool speedLimited;
    float speed;
} LocusDesc;

typedef struct LocusState {
    LocusDesc desc;
    bool active;
    FPoint pos;
} LocusState;

const char *LocusType_ToString(LocusType type);

LocusType LocusType_FromString(const char *str);

LocusType LocusType_Random();

void Locus_Load(MBRegistry *mreg, LocusDesc *desc, const char *prefix);

void Locus_Init(AIContext *nc, LocusState *locus, LocusDesc *desc);
void Locus_RunTick(AIContext *nc, LocusState *locus);


static inline FPoint *
Locus_GetPos(LocusState *locus)
{
    ASSERT(locus != NULL);
    if (locus->active) {
        return &locus->pos;
    }
    return NULL;
}

#endif // _LOCUS_HPP_202210231659