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
#include "fleet.h"

struct MainData {
    SDL_Thread *engineThread;
} mainData;

int Main_EngineThreadMain(void *data)
{
    bool finished = FALSE;
    const BattleStatus *bStatus;

    ASSERT(data == NULL);

    while (!finished) {
        BattleMob *bMobs;
        uint32 numMobs;
        BattleMob *dMobs;

        // Run the AI
        bMobs = Battle_AcquireMobs(&numMobs);
        Fleet_RunTick(bMobs, numMobs);
        Battle_ReleaseMobs();

        // Run the Physics
        Battle_RunTick();

        // Draw
        bMobs = Battle_AcquireMobs(&numMobs);
        dMobs = Display_AcquireMobs(numMobs);
        memcpy(dMobs, bMobs, numMobs * sizeof(bMobs[0]));
        Display_ReleaseMobs();
        Battle_ReleaseMobs();

        bStatus = Battle_AcquireStatus();
        if (bStatus->tick % 1000 == 0) {
            Warning("Finished tick %d\n", bStatus->tick);
            Warning("\tcollisions = %d\n", bStatus->collisions);
        }
        if (bStatus->finished) {
            finished = TRUE;
        }
        Battle_ReleaseStatus();
    }

    bStatus = Battle_AcquireStatus();
    Warning("Finished tick %d\n", bStatus->tick);
    Warning("\tcollisions = %d\n", bStatus->collisions);
    Battle_ReleaseStatus();
    Warning("\n");
    Warning("Battle Finished!\n");

    return 0;
}

int main(void)
{
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
    Fleet_Init();
    Display_Init();

    // Launch Engine Thread
    mainData.engineThread = SDL_CreateThread(Main_EngineThreadMain, "battle",
                                             NULL);
    ASSERT(mainData.engineThread != NULL);

    Display_Main();
    SDL_WaitThread(mainData.engineThread, NULL);

    //Cleanup
    Display_Exit();
    Fleet_Exit();
    Battle_Exit();
    Random_Exit();
    SDL_Quit();
    Warning("Done!\n");
    return 0;
}
