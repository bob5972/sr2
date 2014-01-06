/*
 * main.c --
 */

#include <stdio.h>
#include <math.h>

#include "mbtypes.h"
#include "mbutil.h"
#include "mbassert.h"
#include "random.h"

#define INVALID_ID ((uint32)-1)
#define MOB_SPEED (1.0f)
#define MICRON (0.00001f)

typedef struct FPoint {
    float x;
    float y;
} FPoint;

float FPoint_Distance(const FPoint *a, const FPoint *b)
{
    float dx = (b->x - a->x);
    float dy = (b->y - a->y);
    float d = dx * dx + dy * dy;

    ASSERT(d >= 0);
    return sqrt(d);
}

typedef struct Mob {
    uint32 id;
    FPoint pos;
    FPoint target;
} Mob;

typedef struct World {
    float width;
    float height;
} World;

struct MainData {
    Mob mobs[100];
    World world;
} mainData;

bool Mob_CheckInvariants(const Mob *mob)
{
    ASSERT(mob->pos.x >= 0.0f);
    ASSERT(mob->pos.y >= 0.0f);
    ASSERT(mob->pos.x <= mainData.world.width);
    ASSERT(mob->pos.y <= mainData.world.height);

    ASSERT(mob->target.x >= 0.0f);
    ASSERT(mob->target.y >= 0.0f);
    ASSERT(mob->target.x <= mainData.world.width);
    ASSERT(mob->target.y <= mainData.world.height);

    return TRUE;
}

void Mob_MoveToTarget(Mob *mob)
{
    float distance = FPoint_Distance(&mob->pos, &mob->target);
    ASSERT(Mob_CheckInvariants(mob));
    if (distance <= MOB_SPEED) {
        mob->pos = mob->target;
    } else {
        float dx = mob->target.x - mob->pos.x;
        float dy = mob->target.y - mob->pos.y;
        float factor = MOB_SPEED / distance;
        FPoint newPos;

        newPos.x = mob->pos.x + dx * factor;
        newPos.y = mob->pos.y + dy * factor;
        ASSERT(FPoint_Distance(&newPos, &mob->pos) <= MOB_SPEED + MICRON);
        mob->pos = newPos;
    }
    ASSERT(Mob_CheckInvariants(mob));
}

int main(void)
{
    uint32 i;
    uint32 tick;
    uint32 targetsReached = 0;

    //Setup
    Warning("Starting ants...\n");
    Random_Init();

    Util_Zero(&mainData, sizeof(mainData));
    for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
        mainData.mobs[i].id = INVALID_ID;
    }

    mainData.world.width = 100;
    mainData.world.height = 100;

    for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
        Mob *mob = &mainData.mobs[i];
        mob->id = i;
        mob->pos.x = Random_Float(0.0f, mainData.world.width);
        mob->pos.y = Random_Float(0.0f, mainData.world.height);
        mob->target.x = Random_Float(0.0f, mainData.world.width);
        mob->target.y = Random_Float(0.0f, mainData.world.height);
    }

    for (tick = 0; tick < 10000; tick++) {
        // Run AI
        for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
            Mob *mob = &mainData.mobs[i];
            ASSERT(Mob_CheckInvariants(mob));
            if (mob->pos.x == mob->target.x &&
                    mob->pos.y == mob->target.y) {
                targetsReached++;
                //Warning("Mob %d has reached target (%f, %f)\n",
                //        i, mob->target.x, mob->target.y);
                mob->target.x = Random_Float(0.0f, mainData.world.width);
                mob->target.y = Random_Float(0.0f, mainData.world.height);
            }
            ASSERT(Mob_CheckInvariants(mob));
        }

        // Run Physics
        for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
            Mob *mob = &mainData.mobs[i];
            ASSERT(Mob_CheckInvariants(mob));
            Mob_MoveToTarget(mob);
            ASSERT(Mob_CheckInvariants(mob));
        }

        // Draw

        if (tick % 1000 == 0) {
            Warning("Finished tick %d\n", tick);
        }
    }

    //Cleanup
    Warning("targetsReached = %d\n", targetsReached);
    Random_Exit();
    Warning("Done!\n");
    return 0;
}

