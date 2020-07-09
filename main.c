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
#include "workQueue.h"

typedef enum MainEngineWorkType {
    MAIN_WORK_INVALID = 0,
    MAIN_WORK_EXIT    = 1,
    MAIN_WORK_BATTLE  = 2,
} MainEngineWorkType;

typedef struct MainEngineWorkUnit {
    MainEngineWorkType type;
    uint battleId;
    uint64 seed;
    BattleParams bp;
} MainEngineWorkUnit;

typedef struct MainEngineResultUnit {
    BattleStatus bs;
} MainEngineResultUnit;

typedef struct MainEngineThreadData {
    uint threadId;
    SDL_Thread *sdlThread;
    char threadName[64];
    uint32 startTimeMS;
    uint battleId;
    uint64 seed;
    BattleParams bp;
    Battle *battle;
} MainEngineThreadData;

struct MainData {
    bool headless;
    bool frameSkip;
    uint displayFrames;
    int loop;
    uint timeLimit;

    bool reuseSeed;
    uint64 seed;
    RandomState rs;

    BattleParams bp;
    int winners[MAX_PLAYERS];

    uint numThreads;
    MainEngineThreadData *tData;
    WorkQueue workQ;
    WorkQueue resultQ;

    volatile bool asyncExit;
} mainData;

static void MainRunBattle(MainEngineThreadData *tData,
                          MainEngineWorkUnit *wu);

static void
MainPrintBattleStatus(MainEngineThreadData *tData,
                      const BattleStatus *bStatus)
{
    uint32 stopTimeMS = SDL_GetTicks();
    uint32 elapsedMS = stopTimeMS - tData->startTimeMS;

    Warning("Finished tick %d\n", bStatus->tick);
    Warning("\tbattleId = %d\n", tData->battleId);
    Warning("\tseed = 0x%llX\n", tData->seed);
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

static void MainPrintWinners(void)
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

static int MainEngineThreadMain(void *data)
{
    MainEngineThreadData *tData = data;
    MainEngineWorkUnit wu;

    while (TRUE) {
        WorkQueue_WaitForItem(&mainData.workQ, &wu, sizeof(wu));

        if (wu.type == MAIN_WORK_BATTLE) {
            MainRunBattle(tData, &wu);
        } else if (wu.type == MAIN_WORK_EXIT) {
            return 0;
        } else {
            NOT_IMPLEMENTED();
        }

        WorkQueue_FinishItem(&mainData.workQ);
    }
}

static void MainRunBattle(MainEngineThreadData *tData,
                          MainEngineWorkUnit *wu)
{
    bool finished = FALSE;
    const BattleStatus *bStatus;

    tData->battleId = wu->battleId;
    tData->seed = wu->seed;
    tData->bp = wu->bp;
    tData->battle = Battle_Create(&tData->bp, wu->seed);

    Warning("Starting Battle %d ...\n", tData->battleId);
    Warning("\n");

    tData->startTimeMS = SDL_GetTicks();

    while (!finished && !mainData.asyncExit) {
        Mob *bMobs;
        uint32 numMobs;

        // Run the simulation
        Battle_RunTick(tData->battle);

        // Draw
        if (!mainData.headless) {
            bMobs = Battle_AcquireMobs(tData->battle, &numMobs);
            Mob *dMobs = Display_AcquireMobs(numMobs, mainData.frameSkip);
            if (dMobs != NULL) {
                mainData.displayFrames++;
                memcpy(dMobs, bMobs, numMobs * sizeof(bMobs[0]));
                Display_ReleaseMobs();
            }
            Battle_ReleaseMobs(tData->battle);
        }

        bStatus = Battle_AcquireStatus(tData->battle);
        if (mainData.numThreads == 1) {
            if (bStatus->tick % 1000 == 0) {
                MainPrintBattleStatus(tData, bStatus);
            }
        }
        if (bStatus->finished) {
            finished = TRUE;
        }
        Battle_ReleaseStatus(tData->battle);
    }

    {
        MainEngineResultUnit ru;

        bStatus = Battle_AcquireStatus(tData->battle);
        MainPrintBattleStatus(tData, bStatus);

        MBUtil_Zero(&ru, sizeof(ru));
        ru.bs = *bStatus;
        WorkQueue_QueueItem(&mainData.resultQ, &ru, sizeof(ru));
        Battle_ReleaseStatus(tData->battle);
    }

    Warning("Battle %d %s!\n", tData->battleId,
            finished ? "Finished" : "Aborted");
    Warning("\n");

    Battle_Destroy(tData->battle);
    tData->battle = NULL;
}

void MainConstructScenario(void)
{
    uint p;

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
}

void MainParseCmdLine(int argc, char **argv)
{
    MBOption opts[] = {
        { "-h", "--help",       FALSE, "Print help text"               },
        { "-H", "--headless",   FALSE, "Run headless",                 },
        { "-F", "--frameSkip",  FALSE, "Allow frame skipping",         },
        { "-l", "--loop",       TRUE,  "Loop <arg> times"              },
        { "-s", "--seed",       TRUE,  "Set random seed"               },
        { "-L", "--timeLimit",  TRUE,  "Time limit in ticks"           },
        { "-t", "--numThreads", TRUE,  "Number of engine threads"      },
        { "-R", "--reuseSeed",  FALSE, "Reuse the seed across battles" },
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

    mainData.seed = MBOpt_GetUint64("seed");
    mainData.reuseSeed = MBOpt_GetBool("reuseSeed");

    mainData.timeLimit = MBOpt_GetInt("timeLimit");

    if (MBOpt_IsPresent("numThreads")) {
        mainData.numThreads = MBOpt_GetInt("numThreads");
    } else {
        mainData.numThreads = 1;
    }
    ASSERT(mainData.numThreads >= 1);
}

int main(int argc, char **argv)
{
    ASSERT(MBUtil_IsZero(&mainData, sizeof(mainData)));
    MainParseCmdLine(argc, argv);

    // Setup
    Warning("Starting SpaceRobots2 %s...\n", DEBUG ? "(debug enabled)" : "");
    Warning("\n");

    RandomState_Create(&mainData.rs);
    if (mainData.seed != 0) {
        RandomState_SetSeed(&mainData.rs, mainData.seed);
    }
    DebugPrint("Random seed: 0x%llX\n",
               RandomState_GetSeed(&mainData.rs));

    SDL_Init(mainData.headless ? 0 : SDL_INIT_VIDEO);

    WorkQueue_Create(&mainData.workQ, sizeof(MainEngineWorkUnit));
    WorkQueue_Create(&mainData.resultQ, sizeof(MainEngineResultUnit));

    uint tDataSize = mainData.numThreads * sizeof(mainData.tData[0]);
    mainData.tData = malloc(tDataSize);
    for (uint i = 0; i < mainData.numThreads; i++) {
        MainEngineThreadData *tData = &mainData.tData[i];

        MBUtil_Zero(tData, sizeof(*tData));

        tData->threadId = i;
        uint threadNameLen = sizeof(tData->threadName);
        snprintf(&tData->threadName[0], threadNameLen, "battle%d", i);
        tData->threadName[threadNameLen - 1] = '\0';

        tData->sdlThread =
            SDL_CreateThread(MainEngineThreadMain, &tData->threadName[0], tData);
        ASSERT(tData->sdlThread != NULL);
    }

    MainConstructScenario();

    if (!mainData.headless) {
        if (mainData.numThreads != 1) {
            PANIC("Multiple threads requires --headless\n");
        }
        Display_Init(&mainData.bp);
    }

    for (uint32 i = 0; i < mainData.loop; i++) {
        MainEngineWorkUnit wu;

        MBUtil_Zero(&wu, sizeof(wu));
        wu.type = MAIN_WORK_BATTLE;
        wu.battleId = i;

        if (i == 0 || mainData.reuseSeed) {
            /*
             * Use the actual seed for the first battle, so that it's
             * easy to re-create a single battle from the battle seed
             * without specifying --reuseSeed.
             */
            wu.seed = RandomState_GetSeed(&mainData.rs);
        } else {
            wu.seed = RandomState_Uint64(&mainData.rs);
        }
        wu.bp = mainData.bp;

        WorkQueue_QueueItem(&mainData.workQ, &wu, sizeof(wu));
    }

    if (!mainData.headless) {
        Display_Main();
        mainData.asyncExit = TRUE;
        Display_Exit();
    }

    WorkQueue_WaitForAllFinished(&mainData.workQ);

    for (uint i = 0; i < mainData.numThreads; i++) {
        MainEngineWorkUnit wu;
        MBUtil_Zero(&wu, sizeof(wu));
        wu.type = MAIN_WORK_EXIT;
        WorkQueue_QueueItem(&mainData.workQ, &wu, sizeof(wu));
    }
    for (uint i = 0; i < mainData.numThreads; i++) {
        SDL_WaitThread(mainData.tData[i].sdlThread, NULL);
    }

    WorkQueue_Lock(&mainData.resultQ);
    uint qSize = WorkQueue_QueueSizeLocked(&mainData.resultQ);
    ASSERT(qSize == mainData.loop);
    for (uint i = 0; i < qSize; i++) {
        MainEngineResultUnit ru;
        WorkQueue_GetItemLocked(&mainData.resultQ, &ru, sizeof(ru));
        ASSERT(ru.bs.winner < ARRAYSIZE(mainData.winners));
        mainData.winners[ru.bs.winner]++;
    }
    WorkQueue_Unlock(&mainData.resultQ);

    MainPrintWinners();

    for (uint i = 0; i < mainData.bp.numPlayers; i++) {
        if (mainData.bp.players[i].mreg != NULL) {
            MBRegistry_Free(mainData.bp.players[i].mreg);
        }
    }

    RandomState_Destroy(&mainData.rs);

    WorkQueue_Destroy(&mainData.workQ);
    WorkQueue_Destroy(&mainData.resultQ);
    free(mainData.tData);
    mainData.tData = NULL;

    SDL_Quit();
    MBOpt_Exit();

    Warning("Done!\n");
    return 0;
}
