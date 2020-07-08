/*
 * main.c -- part of SpaceRobots2
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
#include "MBOpt.h"

struct MainData {
    bool headless;
    bool frameSkip;
    uint displayFrames;
    int loop;
    SDL_Thread *engineThread;
    uint32 startTimeMS;
    Battle *battle;
    BattleParams bp;
    int winners[MAX_PLAYERS];
    uint64 seed;
    uint timeLimit;
    volatile bool asyncExit;
} mainData;

void MainPrintBattleStatus(const BattleStatus *bStatus)
{
    uint32 stopTimeMS = SDL_GetTicks();
    uint32 elapsedMS = stopTimeMS - mainData.startTimeMS;

    Warning("Finished tick %d\n", bStatus->tick);
    Warning("\tcollisions = %d\n", bStatus->collisions);
    Warning("\tsensor contacts = %d\n", bStatus->sensorContacts);
    Warning("\tspawns = %d\n", bStatus->spawns);
    Warning("\tship spawns = %d\n", bStatus->shipSpawns);
    Warning("\t%d ticks in %d ms\n", bStatus->tick, elapsedMS);
    Warning("\tavg %.1f ticks/second\n", ((float)bStatus->tick)/elapsedMS * 1000.0f);

    if (mainData.displayFrames > 0) {
        float fps = ((float)mainData.displayFrames)/(elapsedMS / 1000.0f);
        Warning("\t%d frames in %d ms\n", mainData.displayFrames, elapsedMS);
        Warning("\tavg %.1f frames/second\n", fps);
    }

    Warning("\n");

    if (bStatus->finished) {
        ASSERT(bStatus->winner < ARRAYSIZE(bStatus->players));
        Warning("Winner: %s\n", bStatus->players[bStatus->winner].playerName);
    }
}

void MainPrintWinners(void)
{
    uint32 totalBattles = 0;

    Warning("\n");
    Warning("Summary:\n");

    for (uint32 i = 0; i < mainData.bp.numPlayers; i++) {
        totalBattles += mainData.winners[i];
    }

    for (uint32 i = 0; i < mainData.bp.numPlayers; i++) {
        int wins = mainData.winners[i];
        int losses = totalBattles - wins;
        float percent = 100.0f * wins / (float)totalBattles;

        Warning("Fleet: %s\n", mainData.bp.players[i].playerName);
        Warning("\t%d wins, %d losses: %0.1f\%\n", wins, losses, percent);
    }

    Warning("Total Battles: %d\n", totalBattles);
}

int Main_EngineThreadMain(void *data)
{
    bool finished = FALSE;
    const BattleStatus *bStatus;

    Warning("Starting Battle ...\n");

    mainData.startTimeMS = SDL_GetTicks();

    ASSERT(data == NULL);

    while (!finished && !mainData.asyncExit) {
        Mob *bMobs;
        uint32 numMobs;

        // Run the AI
        bMobs = Battle_AcquireMobs(mainData.battle, &numMobs);
        bStatus = Battle_AcquireStatus(mainData.battle);
        Fleet_RunTick(bStatus, bMobs, numMobs);
        Battle_ReleaseMobs(mainData.battle);
        Battle_ReleaseStatus(mainData.battle);

        // Run the Physics
        Battle_RunTick(mainData.battle);

        // Draw
        if (!mainData.headless) {
            bMobs = Battle_AcquireMobs(mainData.battle, &numMobs);
            Mob *dMobs = Display_AcquireMobs(numMobs, mainData.frameSkip);
            if (dMobs != NULL) {
                mainData.displayFrames++;
                memcpy(dMobs, bMobs, numMobs * sizeof(bMobs[0]));
                Display_ReleaseMobs();
            }
            Battle_ReleaseMobs(mainData.battle);
        }

        bStatus = Battle_AcquireStatus(mainData.battle);
        if (bStatus->tick % 1000 == 0) {
            MainPrintBattleStatus(bStatus);
        }
        if (bStatus->finished) {
            finished = TRUE;
        }
        Battle_ReleaseStatus(mainData.battle);
    }

    bStatus = Battle_AcquireStatus(mainData.battle);
    MainPrintBattleStatus(bStatus);
    ASSERT(bStatus->winner < ARRAYSIZE(mainData.winners));
    mainData.winners[bStatus->winner]++;
    Battle_ReleaseStatus(mainData.battle);

    Warning("Battle %s!\n", finished ? "Finished" : "Aborted");
    Warning("\n");

    return 0;
}

void MainParseCmdLine(int argc, char **argv)
{
    MBOption opts[] = {
        { "-h", "--help",      FALSE, "Print help text"       },
        { "-H", "--headless",  FALSE, "Run headless",         },
        { "-F", "--frameSkip", FALSE, "Allow frame skipping", },
        { "-l", "--loop",      TRUE,  "Loop <arg> times"      },
        { "-s", "--seed",      TRUE,  "Set random seed"       },
        { "-L", "--timeLimit", TRUE,  "Time limit in ticks"   },
    };

    MBOpt_Init(opts, ARRAYSIZE(opts), argc, argv);

    if (MBOpt_IsPresent("help")) {
        MBOpt_PrintHelpText();
        exit(1);
    }

    mainData.headless = MBOpt_GetBool("headless");
    mainData.frameSkip = MBOpt_GetBool("frameSkip");

    if (MBOpt_IsPresent("loop")) {
        mainData.loop = MBOpt_GetInt("loop");
    } else {
        mainData.loop = 1;
    }

    mainData.seed = MBOpt_GetInt("seed");
    mainData.timeLimit = MBOpt_GetInt("timeLimit");

}

int main(int argc, char **argv)
{
    uint32 p;

    ASSERT(MBUtil_IsZero(&mainData, sizeof(mainData)));
    MainParseCmdLine(argc, argv);

    // Setup
    Warning("Starting SpaceRobots2 %s...\n", DEBUG ? "(debug enabled)" : "");
    Warning("\n");
    SDL_Init(mainData.headless ? 0 : SDL_INIT_VIDEO);

    if (mainData.seed != 0) {
        Random_SetSeed(mainData.seed);
    }
    Random_Init();
    DebugPrint("Random seed: 0x%llX\n", Random_GetSeed());

    /*
     * Battle Scenario
     */
    mainData.bp.width = 1600;
    mainData.bp.height = 1200;
    mainData.bp.startingCredits = 1000;
    mainData.bp.creditsPerTick = 1;
    if (mainData.timeLimit != 0) {
        mainData.bp.timeLimit = mainData.timeLimit;
    } else{
        mainData.bp.timeLimit = 100 * 1000;
    }
    mainData.bp.lootDropRate = 0.25f;
    mainData.bp.lootSpawnRate = 2.0f;
    mainData.bp.minLootSpawn = 10;
    mainData.bp.maxLootSpawn = 20;
    mainData.bp.restrictedStart = TRUE;

    /*
     * The NEUTRAL fleet needs to be there.
     *
     * Otherwise these are in rough order of difficulty.
     */
    p = 0;
    mainData.bp.players[p].playerName = "Neutral";
    mainData.bp.players[p].aiType = FLEET_AI_NEUTRAL;
    p++;

//     mainData.bp.players[p].playerName = "DummyFleet";
//     mainData.bp.players[p].aiType = FLEET_AI_DUMMY;
//     p++;

//     mainData.bp.players[p].playerName = "SimpleFleet";
//     mainData.bp.players[p].aiType = FLEET_AI_SIMPLE;
//     p++;

//     mainData.bp.players[p].playerName = "BobFleet";
//     mainData.bp.players[p].aiType = FLEET_AI_BOB;
//     p++;

//     mainData.bp.players[p].playerName = "GatherFleet";
//     mainData.bp.players[p].aiType = FLEET_AI_GATHER;
//     p++;

    mainData.bp.players[p].playerName = "CloudFleet";
    mainData.bp.players[p].aiType = FLEET_AI_CLOUD;
    mainData.bp.players[p].mreg = MBRegistry_Alloc();
    MBRegistry_Put(mainData.bp.players[p].mreg, "CrazyMissiles", "TRUE");
    p++;

    mainData.bp.players[p].playerName = "MapperFleet";
    mainData.bp.players[p].mreg = MBRegistry_Alloc();
    MBRegistry_Put(mainData.bp.players[p].mreg, "StartingWaveSize", "5");
    MBRegistry_Put(mainData.bp.players[p].mreg, "WaveSizeIncrement", "0");
    MBRegistry_Put(mainData.bp.players[p].mreg, "RandomWaves", "FALSE");
    mainData.bp.players[p].aiType = FLEET_AI_MAPPER;
    p++;

    ASSERT(p <= ARRAYSIZE(mainData.bp.players));
    mainData.bp.numPlayers = p;

    for (uint32 i = 0; i < mainData.loop; i++) {
        mainData.battle = Battle_Create(&mainData.bp);
        Fleet_Init();

        if (!mainData.headless) {
            Display_Init(&mainData.bp);
        }

        // Launch Engine Thread
        mainData.engineThread = SDL_CreateThread(Main_EngineThreadMain,
                                                 "battle", NULL);
        ASSERT(mainData.engineThread != NULL);

        if (!mainData.headless) {
            Display_Main();
            mainData.asyncExit = TRUE;
        }

        SDL_WaitThread(mainData.engineThread, NULL);

        //Cleanup
        if (!mainData.headless) {
            Display_Exit();
        }

        Fleet_Exit();
        Battle_Destroy(mainData.battle);
        mainData.battle = NULL;
    }

    MainPrintWinners();

    for (uint32 i = 0; i < mainData.bp.numPlayers; i++) {
        if (mainData.bp.players[i].mreg != NULL) {
            MBRegistry_Free(mainData.bp.players[i].mreg);
        }
    }

    Random_Exit();
    SDL_Quit();
    MBOpt_Exit();

    Warning("Done!\n");
    return 0;
}
