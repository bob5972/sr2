/*
 * fleetUtil.c -- part of SpaceRobots2
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

#include "fleet.h"
#include "random.h"
#include "IntMap.h"
#include "battle.h"

Mob *FleetUtil_FindClosestSensor(FleetAI *ai, const FPoint *pos, uint filter)
{
    return FleetUtil_FindClosestMob(&ai->sensors, pos, filter);
}

Mob *FleetUtil_FindClosestMob(MobPSet *ms, const FPoint *pos, uint filter)
{
    MobIt mit;
    float distance;
    Mob *best = NULL;

    MobIt_Start(ms, &mit);
    while (MobIt_HasNext(&mit)) {
        Mob *m = MobIt_Next(&mit);

        if (!m->alive) {
            continue;
        }
        if (((1 << m->type) & filter) != 0) {
            float curDistance = FPoint_Distance(pos, &m->pos);
            if (best == NULL || curDistance < distance) {
                distance = curDistance;
                best = m;
            }
        }
    }

    return best;
}

Mob *FleetUtil_FindClosestMobInRange(MobPSet *ms, const FPoint *pos, uint filter,
                                     float radius)
{
    Mob *mob = FleetUtil_FindClosestMob(ms, pos, filter);

    if (mob != NULL) {
        if (FPoint_Distance(pos, &mob->pos) <= radius) {
            return mob;
        }
    }

    return NULL;
}

int FleetUtil_FindNthClosestMobP(MobPVec *mobps, const FPoint *pos, int n)
{
    ASSERT(mobps != NULL);
    ASSERT(pos != NULL);

    // This is 0-based.  (ie n=0 means closest thing)
    ASSERT(n >= 0);

    uint size = MobPVec_Size(mobps);

    if (size == 0 || n >= size) {
        return -1;
    } else if (size == 1) {
        return 0;
    }

    for (int sortedCount = 0; sortedCount < n + 1; sortedCount++) {
        int i = size - 1;
        Mob **iMob = MobPVec_GetPtr(mobps, i);
        float iDistance = FPoint_Distance(&(*iMob)->pos, pos);

        for (int k = i - 1; k >= sortedCount; k--) {
            Mob **kMob = MobPVec_GetPtr(mobps, k);
            float kDistance = FPoint_Distance(&(*kMob)->pos, pos);

            if (kDistance > iDistance) {
                Mob *tMob = *iMob;
                *iMob = *kMob;
                *kMob = tMob;
            }

            iMob = kMob;
            iDistance = kDistance;
        }
    }

    /*
     * We partially sorted the MobPVec, so the Nth closest
     * is now in slot N, and then adjust for 1-based vs 0-based.
     */
    return n;
}

void FleetUtil_SortMobPByDistance(MobPVec *mobps, const FPoint *pos)
{
    if (MobPVec_Size(mobps) <= 1) {
        return;
    }

    for (int i = 1; i < MobPVec_Size(mobps); i++) {
        Mob **iMob = MobPVec_GetPtr(mobps, i);
        float iDistance = FPoint_Distance(&(*iMob)->pos, pos);

        for (int n = i - 1; n >= 0; n--) {
            Mob **nMob = MobPVec_GetPtr(mobps, n);
            float nDistance = FPoint_Distance(&(*nMob)->pos, pos);

            if (nDistance > iDistance) {
                Mob *tMob = *iMob;
                *iMob = *nMob;
                *nMob = tMob;

                iMob = nMob;
                iDistance = nDistance;
            } else {
                break;
            }
        }
    }
}

void FleetUtil_RandomPointInRange(RandomState *rs, FPoint *p,
                                  const FPoint *center, float radius)
{
    ASSERT(rs != NULL);
    ASSERT(p != NULL);
    ASSERT(center != NULL);

    // This technically generates a point within the square...
    p->x = RandomState_Float(rs, MAX(0, center->x - radius),
                             center->x + radius);
    p->y = RandomState_Float(rs, MAX(0, center->y - radius),
                             center->y + radius);
}
