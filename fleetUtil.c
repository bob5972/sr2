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

int FleetUtil_FindClosestSensor(FleetAI *ai, const FPoint *pos, uint scanFilter)
{
    return FleetUtil_FindClosestMob(&ai->sensors, pos, scanFilter);
}

int FleetUtil_FindClosestMob(MobVector *mobs, const FPoint *pos, uint scanFilter)
{
    float distance;
    int index = -1;

    for (uint i = 0; i < MobVector_Size(mobs); i++) {
        Mob *m = MobVector_GetPtr(mobs, i);
        if (!m->alive) {
            continue;
        }
        if (((1 << m->type) & scanFilter) != 0) {
            float curDistance = FPoint_Distance(pos, &m->pos);
            if (index == -1 || curDistance < distance) {
                distance = curDistance;
                index = i;
            }
        }
    }

    return index;
}

MobPVec *FleetUtil_AllocMobPVec(MobVector *mobs)
{
    MobPVec *mobps = malloc(sizeof(*mobps));
    MobPVec_CreateWithSize(mobps, MobVector_Size(mobs));

    for (uint i = 0; i < MobVector_Size(mobs); i++) {
        Mob *mp = MobVector_GetPtr(mobs, i);
        MobPVec_PutValue(mobps, i, mp);
    }

    return mobps;
}
void FleetUtil_FreeMobPVec(MobPVec *mobps)
{
    MobPVec_Destroy(mobps);
    free(mobps);
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

void FleetUtil_SortMobsByDistance(MobVector *mobs, const FPoint *pos)
{
    if (MobVector_Size(mobs) <= 1) {
        return;
    }

    for (int i = 1; i < MobVector_Size(mobs); i++) {
        Mob *iMob = MobVector_GetPtr(mobs, i);
        float iDistance = FPoint_Distance(&iMob->pos, pos);

        for (int n = i - 1; n >= 0; n--) {
            Mob *nMob = MobVector_GetPtr(mobs, n);
            float nDistance = FPoint_Distance(&nMob->pos, pos);

            if (nDistance > iDistance) {
                Mob tMob = *iMob;
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

void FleetUtil_RandomPointInRange(FPoint *p, const FPoint *center, float radius)
{
    ASSERT(p != NULL);
    ASSERT(center != NULL);

    p->x = Random_Float(MAX(0, center->x - radius), center->x + radius);
    p->y = Random_Float(MAX(0, center->y - radius), center->y + radius);
}

Mob *FleetUtil_GetMob(FleetAI *ai, MobID mobid)
{
    Mob *m;
    ASSERT(ai != NULL);

    int i = IntMap_Get(&ai->mobMap, mobid);
    if (i == -1) {
        return NULL;
    }

    m = MobVector_GetPtr(&ai->mobs, i);
    if (m->mobid == mobid) {
        return m;
    }

    if (DEBUG) {
        /*
        * XXX: If the fleet sorted their mob vector, then the mobMap is
        * all mis-aligned... There isn't an obviously better way to handle
        * this than using pointers, or some other abstraction?
        */
        for (i = 0; i < MobVector_Size(&ai->mobs); i++) {
            m = MobVector_GetPtr(&ai->mobs, i);
            if (m->mobid == mobid) {
                PANIC("Fleet mobMap invalidated without call to "
                      "FleetUtil_UpdateMobMap\n");
            }
        }
    }

    return NULL;
}

void FleetUtil_UpdateMobMap(FleetAI *ai)
{
    IntMap_MakeEmpty(&ai->mobMap);

    for (uint32 i = 0; i < MobVector_Size(&ai->mobs); i++) {
        Mob *m = MobVector_GetPtr(&ai->mobs, i);
        IntMap_Put(&ai->mobMap, m->mobid, i);
        ASSERT(IntMap_ContainsKey(&ai->mobMap, m->mobid));
    }
}
