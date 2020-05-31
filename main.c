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
#include "geometry.h"
#include "battle.h"

struct MainData {
    SDL_Thread *engineThread;
} mainData;

int Main_EngineThreadMain(void *data)
{
    uint32 tick;
    const BattleStatus *bStatus;

    ASSERT(data == NULL);

    for (tick = 0; tick < 10000; tick++) {
        const BattleMob *bMobs;
        uint32 numMobs;
        DisplayMob *dMobs;

        // Run the AI and Physics
        Battle_RunTick();

        // Draw
        bMobs = Battle_AcquireMobs(&numMobs);
        dMobs = Display_AcquireMobs(numMobs);
        for (uint32 i = 0; i < numMobs; i++) {
            dMobs[i].rect.x = (uint32)bMobs[i].pos.x;
            dMobs[i].rect.y = (uint32)bMobs[i].pos.y;
            dMobs[i].rect.w = (uint32)bMobs[i].pos.w;
            dMobs[i].rect.h = (uint32)bMobs[i].pos.h;
        }
        Display_ReleaseMobs();
        Battle_ReleaseMobs();

        if (tick % 1000 == 0) {
            Warning("Finished tick %d\n", tick);
        }
    }

    bStatus = Battle_AcquireStatus();
    Warning("targetsReached = %d\n", bStatus->targetsReached);
    Battle_ReleaseStatus();

    return 0;
}

int main(void)
{
    DisplayMapParams dmp;
    BattleParams bp;

    ASSERT(Util_IsZero(&mainData, sizeof(mainData)));

    // Setup
    Warning("Starting SpaceRobots2 ...\n");
    SDL_Init(SDL_INIT_VIDEO);
    Random_Init();

    Util_Zero(&bp, sizeof(bp));
    bp.width = 1600;
    bp.height = 1200;
    Battle_Init(&bp);

    Util_Zero(&dmp, sizeof(dmp));
    dmp.width = bp.width;
    dmp.height = bp.height;
    Display_Init(&dmp);

    // Launch Engine Thread
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
