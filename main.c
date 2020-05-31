/*
 * main.c --
 */

#include <stdio.h>
#include <math.h>

#include <SDL.h>

#include "mbtypes.h"
#include "mbutil.h"
#include "mbassert.h"
#include "random.h"
#include "display.h"

#define INVALID_ID ((uint32)-1)
#define MOB_SPEED (1.0f)
#define MICRON (0.1f)
#define MOB_DIM 10

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
    uint32 targetsReached;
    SDL_Thread *engineThread;
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

//         Warning("tPos(%f, %f) pos(%f, %f) newPos(%f, %f)\n",
//                 mob->target.x, mob->target.y,
//                 mob->pos.x, mob->pos.y,
//                 newPos.x, newPos.y);
//         Warning("diffPos(%f, %f)\n",
//                 (newPos.x - mob->pos.x),
//                 (newPos.y - mob->pos.y));
//         Warning("distance=%f, speed+micron=%f, error=%f\n",
//                 (float)FPoint_Distance(&newPos, &mob->pos),
//                 (float)(MOB_SPEED + MICRON),
//                 (float)(FPoint_Distance(&newPos, &mob->pos) - (MOB_SPEED + MICRON)));

        //XXX: This ASSERT is hitting for resonable-seeming micron values...?
        ASSERT(FPoint_Distance(&newPos, &mob->pos) <= MOB_SPEED + MICRON);
        mob->pos = newPos;
    }
    ASSERT(Mob_CheckInvariants(mob));
}

int Main_EngineThreadMain(void *data)
{
    uint32 tick;

    ASSERT(data == NULL);

    for (tick = 0; tick < 10000; tick++) {
        // Run AI
        for (uint32 i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
            Mob *mob = &mainData.mobs[i];
            ASSERT(Mob_CheckInvariants(mob));
            if (mob->pos.x == mob->target.x &&
                    mob->pos.y == mob->target.y) {
                mainData.targetsReached++;
                //Warning("Mob %d has reached target (%f, %f)\n",
                //        i, mob->target.x, mob->target.y);
                mob->target.x = Random_Float(0.0f, mainData.world.width);
                mob->target.y = Random_Float(0.0f, mainData.world.height);
            }
            ASSERT(Mob_CheckInvariants(mob));
        }

        // Run Physics
        for (uint32 i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
            Mob *mob = &mainData.mobs[i];
            ASSERT(Mob_CheckInvariants(mob));
            Mob_MoveToTarget(mob);
            ASSERT(Mob_CheckInvariants(mob));
        }

        // Draw
        DisplayMob *displayMobs;
        displayMobs = Display_AcquireMobs(ARRAYSIZE(mainData.mobs));
        for (uint32 i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
            displayMobs[i].rect.x = (uint32)mainData.mobs[i].pos.x;
            displayMobs[i].rect.y = (uint32)mainData.mobs[i].pos.y;
            displayMobs[i].rect.w = MOB_DIM;
            displayMobs[i].rect.h = MOB_DIM;
        }
        Display_ReleaseMobs();

        if (tick % 1000 == 0) {
            Warning("Finished tick %d\n", tick);
        }
    }

    Warning("targetsReached = %d\n", mainData.targetsReached);
    return 0;
}

int main(void)
{
    uint32 i;
    DisplayMapParams dmp;

    //Setup
    Warning("Starting SpaceRobots2 ...\n");
    SDL_Init(SDL_INIT_VIDEO);
    Random_Init();

    Util_Zero(&dmp, sizeof(dmp));
    dmp.width = 800;
    dmp.height = 600;
    Display_Init(&dmp);

    Util_Zero(&mainData, sizeof(mainData));
    for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
        mainData.mobs[i].id = INVALID_ID;
    }

    mainData.world.width = dmp.width;
    mainData.world.height = dmp.height;

    for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
        Mob *mob = &mainData.mobs[i];
        mob->id = i;
        mob->pos.x = Random_Float(0.0f, mainData.world.width);
        mob->pos.y = Random_Float(0.0f, mainData.world.height);
        mob->target.x = Random_Float(0.0f, mainData.world.width);
        mob->target.y = Random_Float(0.0f, mainData.world.height);
    }

    mainData.engineThread = SDL_CreateThread(Main_EngineThreadMain, "engine",
                                             NULL);
    ASSERT(mainData.engineThread != NULL);

    Display_Main();
    SDL_WaitThread(mainData.engineThread, NULL);

    //Cleanup
    Display_Exit();
    Random_Exit();
    SDL_Quit();
    Warning("Done!\n");
    return 0;
}
