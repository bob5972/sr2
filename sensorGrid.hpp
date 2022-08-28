/*
 * sensorGrid.hpp -- part of SpaceRobots2
 * Copyright (C) 2020-2021 Michael Banack <github@banack.net>
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

#ifndef _SENSORGRID_H_202008021543
#define _SENSORGRID_H_202008021543

extern "C" {
#include "battleTypes.h"
#include "mob.h"
#include "MBRegistry.h"
#include "Random.h"
}

#include "mobSet.hpp"
#include "IntMap.hpp"
#include "BitVector.hpp"

#define SG_STALE_CORE_DEFAULT   40
#define SG_STALE_FIGHTER_DEFAULT 2

class SensorGrid
{
public:
    /**
     * Construct a new SensorGrid.
     */
    SensorGrid() {
        myTargetLastSeenMap.setEmptyValue(0);
        myLastTick = 0;

        myEnemyBaseDestroyedCount = 0;

        myFriendBasePos.x = 0.0f;
        myFriendBasePos.y = 0.0f;

        myStaleCoreTime = SG_STALE_CORE_DEFAULT;
        myStaleFighterTime = SG_STALE_FIGHTER_DEFAULT;
    }

    /**
     * Destroy this SensorGrid.
     */
    ~SensorGrid() { }

    /**
     * Load settings from MBRegistry.
     */
    void loadRegistry(MBRegistry *mreg) {
        if (mreg == NULL) {
            return;
        }

        myStaleCoreTime =
            (uint)MBRegistry_GetFloatD(mreg, "sensorGrid.staleCoreTime",
                                       SG_STALE_CORE_DEFAULT);
        myStaleFighterTime =
            (uint)MBRegistry_GetFloatD(mreg, "sensorGrid.staleFighterTime",
                                       SG_STALE_FIGHTER_DEFAULT);
    }

    /**
     * Update this SensorGrid with the new sensor information in the tick.
     *
     * This invalidates any Mob pointers previously obtained from this
     * SensorGrid.
     */
    virtual void updateTick(FleetAI *ai);

    /**
     * How many friends do we have?
     */
    int numFriends() {
        return myFriends.size();
    }

    int numFriends(MobTypeFlags filter) {
        return myFriends.numMobs(filter);
    }


    int numTargets() {
        return myTargets.size();
    }

    int numTargets(MobTypeFlags filter) {
        return myTargets.numMobs(filter);
    }

    int numFriendsInRange(MobTypeFlags filter, const FPoint *pos, float range) {
        return myFriends.numMobsInRange(filter, pos, range);

    }

    int numTargetsInRange(MobTypeFlags filter, const FPoint *pos, float range) {
        return myTargets.numMobsInRange(filter, pos, range);
    }

    /**
     * Find the friendly mob closest to the specified point.
     */
    Mob *findClosestFriend(const FPoint *pos, MobTypeFlags filter) {
        return findNthClosestFriend(pos, filter, 0);
    }

    /**
     * Find the friendly mob closest to the specified point, that's
     * not the provided mob.
     */
    Mob *findClosestFriend(const Mob *self, MobTypeFlags filter) {
        Mob *m = findNthClosestFriend(&self->pos, filter, 0);
        if (m != NULL && m->mobid == self->mobid) {
            m = findNthClosestFriend(&m->pos, filter, 1);
        }
        return m;
    }

    /**
     * Find the Nth closest friendly mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestFriend(const FPoint *pos, MobTypeFlags filter, int n) {
        return myFriends.findNthClosestMob(pos, filter, n);
    }

    /**
     * Find the target mob closest to the specified point.
     */
    Mob *findClosestTarget(const FPoint *pos, MobTypeFlags filter) {
        return findNthClosestTarget(pos, filter, 0);
    }

    /**
     * Find the Nth closest friendly mob to the specified point.
     * This is 0-based, so the closest mob is found when n=0.
     */
    Mob *findNthClosestTarget(const FPoint *pos, MobTypeFlags filter, int n) {
        return myTargets.findNthClosestMob(pos, filter, n);
    }

    /**
     * Find the closest mob to the specified point, if it's within
     * the specified range.
     */
    Mob *findClosestTargetInRange(const FPoint *pos, MobTypeFlags filter,
                                  float radius) {
        Mob *m = findClosestTarget(pos, filter);

        if (m != NULL) {
            if (FPoint_Distance(pos, &m->pos) <= radius) {
                return m;
            }
        }

        return NULL;
    }

    /**
     * Look-up a friendly Mob from this SensorGrid.
     */
    Mob *getFriend(MobID mobid) {
        return myFriends.get(mobid);
    }

    /**
     * Look-up an enemy Mob from this SensorGrid.
     */
    Mob *getEnemy(MobID mobid) {
        return myTargets.get(mobid);
    }

    /**
     * Look-up a Mob from this SensorGrid.
     */
    Mob *get(MobID mobid) {
        Mob *m = getFriend(mobid);
        if (m != NULL) {
            return m;
        }

        m = getEnemy(mobid);
        if (m != NULL) {
            return m;
        }

        return NULL;
    }

    /**
     * Find an enemy base.
     */
    Mob *enemyBase() {
        return myTargets.getBase();
    }

    bool hasEnemyBase() {
        return enemyBase() != NULL;
    }

    /**
     * How many enemyBases can we confirm were destroyed?
     */
    int enemyBasesDestroyed() {
        return myEnemyBaseDestroyedCount;
    }

    /**
     * Find a friendly base.
     */
    Mob *friendBase() {
        return myFriends.getBase();
    }

    FPoint *friendBasePos() {
        Mob *fbase = friendBase();

        if (fbase != NULL) {
            myFriendBasePos = fbase->pos;
        }

        return &myFriendBasePos;
    }

    MobSet::MobIt friendsIterator(MobTypeFlags filter) {
        return myFriends.iterator(filter);
    }

    void pushFriends(MBVector<Mob *> &v, MobTypeFlags filter) {
        myFriends.pushMobs(v, filter);
    }

    void pushClosestFriendsInRange(MBVector<Mob *> &v, MobTypeFlags filter,
                                   const FPoint *pos, float range) {
        myFriends.pushClosestMobsInRange(v, filter, pos, range);
    }

    void pushTargets(MBVector<Mob *> &v, MobTypeFlags filter) {
        myTargets.pushMobs(v, filter);
    }

    void pushClosestTargetsInRange(MBVector<Mob *> &v, MobTypeFlags filter,
                                   const FPoint *pos, float range) {
        myTargets.pushClosestMobsInRange(v, filter, pos, range);
    }

    /**
     * Return the tick we last scanned the specified mob at, if it's
     * still tracked on the SensorGrid.
     */
    uint getLastSeenTick(MobID mobid) {
        if (getFriend(mobid) != NULL) {
            return myLastTick;
        }

        return myTargetLastSeenMap.get(mobid);
    }

    void avgFlock(FPoint *avgVel, FPoint *avgPos,
                  const FPoint *p, float radius,
                  MobTypeFlags filter,
                  bool useFriends) {
        uint n = 0;
        MobSet::MobIt mit;
        FPoint lAvgVel;
        FPoint lAvgPos;

        if (useFriends) {
            mit = myFriends.iterator(filter);
        } else {
            mit = myTargets.iterator(filter);
        }

        lAvgVel.x = 0.0f;
        lAvgVel.y = 0.0f;

        lAvgPos.x = 0.0f;
        lAvgPos.y = 0.0f;

        while (mit.hasNext()) {
            Mob *f = mit.next();
            ASSERT(f != NULL);

            if (FPoint_Distance(&f->pos, p) <= radius) {
                n++;
                lAvgVel.x += (f->pos.x - f->lastPos.x);
                lAvgVel.y += (f->pos.y - f->lastPos.y);

                lAvgPos.x += f->pos.x;
                lAvgPos.y += f->pos.y;
            }
        }

        if (n != 0) {
            lAvgVel.x /= n;
            lAvgVel.y /= n;

            lAvgPos.x /= n;
            lAvgPos.y /= n;
        }

        if (avgVel != NULL) {
            *avgVel = lAvgVel;
        }
        if (avgPos != NULL) {
            *avgPos = lAvgPos;
        }
    }

    void friendAvgVelocity(FPoint *avgVel, const FPoint *p, float radius,
                           MobTypeFlags filter) {
        avgFlock(avgVel, NULL, p, radius, filter, TRUE);
    }

    void friendAvgPos(FPoint *avgPos, const FPoint *p, float radius,
                      MobTypeFlags filter) {
        avgFlock(NULL, avgPos, p, radius, filter, TRUE);
    }

    void targetAvgVelocity(FPoint *avgVel, const FPoint *p, float radius,
                           MobTypeFlags filter) {
        avgFlock(avgVel, NULL, p, radius, filter, FALSE);
    }

    void targetAvgPos(FPoint *avgPos, const FPoint *p, float radius,
                      MobTypeFlags filter) {
        avgFlock(NULL, avgPos, p, radius, filter, FALSE);
    }

private:
    int myEnemyBaseDestroyedCount;
    FPoint myFriendBasePos;

    uint myLastTick;
    IntMap myTargetLastSeenMap;

    MobSet myFriends;
    MobSet myTargets;

    uint myStaleFighterTime;
    uint myStaleCoreTime;
};

class MappingSensorGrid : public SensorGrid
{
public:
    MappingSensorGrid(uint width, uint height, uint64 seed) {
        myData.bvWidth = (width / TILE_SIZE) + 1;
        myData.bvHeight = (height / TILE_SIZE) + 1;
        myData.scannedBV.resize(myData.bvWidth * myData.bvHeight);
        ASSERT(myData.scannedBV.getFillValue() == FALSE);

        myData.enemyBaseGuessIndex = -1;
        myData.enemyBaseGuessPos.x = 0.0f;
        myData.enemyBaseGuessPos.y = 0.0f;
        myData.hasEnemyBaseGuess = FALSE;
        myData.noMoreEnemyBaseGuess = FALSE;

        RandomState_CreateWithSeed(&myData.rs, seed);
    }

    virtual void updateTick(FleetAI *ai);

    bool hasBeenScanned(const FPoint *pos) {
        int i = GetTileIndex(pos);
        return myData.scannedBV.get(i);
    }

    bool hasEnemyBaseGuess() {
        return myData.hasEnemyBaseGuess;
    }

    FPoint getEnemyBaseGuess() {
        return myData.enemyBaseGuessPos;
    }

    void setSeed(uint64 seed) {
        RandomState_SetSeed(&myData.rs, seed);
    }

private:
    /*
     * Inscribe a tile-square inside the fighter sensor radius,
     * then quarter it to ensure that the full tile is always inside the sensor
     * circle even if the circle is centered on a corner.
     * Then, quarter the square again, to ensure that we can always safely take
     * 4 tiles centered on the mob and avoid having a fighter bouncing around
     * a target and just missing scanning that particular tile.
     */
    const float TILE_SIZE = MOB_FIGHTER_SENSOR_RADIUS * sqrtf(0.5f) * 0.5f;

    struct {
        uint bvWidth, bvHeight;
        CPBitVector scannedBV;
        FPoint enemyBaseGuessPos;
        int enemyBaseGuessIndex;
        bool hasEnemyBaseGuess;
        bool noMoreEnemyBaseGuess;
        RandomState rs;
    } myData;

    void generateGuess();

    inline void GetTileCoord(const FPoint *pos,
                             uint32 *x,
                             uint32 *y)
    {
        ASSERT(pos != NULL);
        ASSERT(x != NULL);
        ASSERT(y != NULL);

        if (pos->x <= 0.0f) {
            *x = 0;
        } else {
            *x = pos->x / TILE_SIZE;
        }

        if (pos->y <= 0.0f) {
            *y = 0;
        } else {
            *y = pos->y / TILE_SIZE;
        }

        if (*x >= myData.bvWidth) {
            *x = myData.bvWidth - 1;
        }
        if (*y >= myData.bvHeight) {
            *y = myData.bvHeight - 1;
        }
    }

    inline int GetTileIndex(const FPoint *pos)
    {
        uint32 x, y;
        ASSERT(pos != NULL);
        GetTileCoord(pos, &x, &y);
        int i = x + y * myData.bvWidth;
        return i;
    }
};


#endif // _SENSORGRID_H_202008021543
