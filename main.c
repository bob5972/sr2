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
    uint32 startTimeMS;
} mainData;

void MainPrintBattleStatus(const BattleStatus *bStatus)
{
    uint32 stopTimeMS = SDL_GetTicks();
    uint32 elapsedMS = stopTimeMS - mainData.startTimeMS;

    Warning("Finished tick %d\n", bStatus->tick);
    Warning("\tcollisions = %d\n", bStatus->collisions);
    Warning("\t%d ticks in %d ms\n", bStatus->tick, elapsedMS);
    Warning("\t%.1f ticks/second\n", ((float)bStatus->tick)/elapsedMS * 1000.0f);
    Warning("\n");
}

int Main_EngineThreadMain(void *data)
{
    bool finished = FALSE;
    const BattleStatus *bStatus;


    mainData.startTimeMS = SDL_GetTicks();

    ASSERT(data == NULL);

    while (!finished) {
        Mob *bMobs;
        uint32 numMobs;
        Mob *dMobs;

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
        if (bStatus->tick % 200 == 0) {
            MainPrintBattleStatus(bStatus);
        }
        if (bStatus->finished) {
            finished = TRUE;
        }
        Battle_ReleaseStatus();
    }

    bStatus = Battle_AcquireStatus();
    MainPrintBattleStatus(bStatus);
    Battle_ReleaseStatus();

    Warning("Battle Finished!\n");

    return 0;
}

int main(void)
{
    BattleParams bp;

    ASSERT(Util_IsZero(&mainData, sizeof(mainData)));

    // Setup
    Warning("Starting SpaceRobots2 ...\n");
    Warning("\n");
    SDL_Init(SDL_INIT_VIDEO);
    Random_Init();

    Util_Zero(&bp, sizeof(bp));
    bp.numPlayers = 8;
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
