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

typedef struct MainWinnerData {
    uint battles;
    uint wins;
    uint losses;
} MainWinnerData;

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
    uint tickLimit;
    bool tournament;

    bool reuseSeed;
    uint64 seed;
    RandomState rs;

    uint numBPs;
    BattleParams *bps;

    int numPlayers;
    BattlePlayerParams players[MAX_PLAYERS];
    MainWinnerData winners[MAX_PLAYERS];

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
        BattlePlayerParams *bpp = &mainData.players[bStatus->winnerUID];
        ASSERT(bStatus->winnerUID < ARRAYSIZE(mainData.players));
        Warning("Winner: %s\n", bpp->playerName);
    }
}

static void MainPrintWinners(void)
{
    uint32 totalBattles = 0;

    Warning("\n");
    Warning("Summary:\n");

    for (uint32 i = 0; i < mainData.numPlayers; i++) {
        totalBattles += mainData.winners[i].wins;
    }

    for (uint32 i = 0; i < mainData.numPlayers; i++) {
        int wins = mainData.winners[i].wins;
        int losses = mainData.winners[i].losses;
        int battles = mainData.winners[i].battles;
        float percent = 100.0f * wins / (float)battles;

        Warning("Fleet: %s\n", mainData.players[i].playerName);
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
    BattleParams bp;

    MBUtil_Zero(&bp, sizeof(bp));
    bp.width = 1600;
    bp.height = 1200;
    bp.startingCredits = 1000;
    bp.creditsPerTick = 1;

    if (mainData.tickLimit != 0) {
        bp.tickLimit = mainData.tickLimit;
    } else {
        bp.tickLimit = 100 * 1000;
    }

    bp.lootDropRate = 0.25f;
    bp.lootSpawnRate = 2.0f;
    bp.minLootSpawn = 10;
    bp.maxLootSpawn = 20;
    bp.restrictedStart = TRUE;

    /*
     * The NEUTRAL fleet needs to be there.
     *
     * Otherwise these are in rough order of difficulty.
     */
    p = 0;
    mainData.players[p].playerName = "Neutral";
    mainData.players[p].aiType = FLEET_AI_NEUTRAL;
    p++;

    if (mainData.tournament) {
        mainData.players[p].playerName = "DummyFleet";
        mainData.players[p].aiType = FLEET_AI_DUMMY;
        p++;

        mainData.players[p].playerName = "SimpleFleet";
        mainData.players[p].aiType = FLEET_AI_SIMPLE;
        p++;

        mainData.players[p].playerName = "BobFleet";
        mainData.players[p].aiType = FLEET_AI_BOB;
        p++;

        mainData.players[p].playerName = "GatherFleet";
        mainData.players[p].aiType = FLEET_AI_GATHER;
        p++;
    }

    mainData.players[p].playerName = "CloudFleet";
    mainData.players[p].aiType = FLEET_AI_CLOUD;
    mainData.players[p].mreg = MBRegistry_Alloc();
    MBRegistry_Put(mainData.players[p].mreg, "CrazyMissiles", "TRUE");
    p++;

    mainData.players[p].playerName = "MapperFleet";
    mainData.players[p].mreg = MBRegistry_Alloc();
    MBRegistry_Put(mainData.players[p].mreg, "StartingWaveSize", "5");
    MBRegistry_Put(mainData.players[p].mreg, "WaveSizeIncrement", "0");
    MBRegistry_Put(mainData.players[p].mreg, "RandomWaves", "FALSE");
    mainData.players[p].aiType = FLEET_AI_MAPPER;
    p++;

    ASSERT(p <= ARRAYSIZE(mainData.players));
    mainData.numPlayers = p;

    for (uint i = 0; i < p; i++) {
        mainData.players[i].playerUID = i;
    }

    if (!mainData.tournament) {
        mainData.numBPs = 1;
        mainData.bps = malloc(sizeof(mainData.bps[0]));
        mainData.bps[0] = bp;
        ASSERT(sizeof(mainData.players) == sizeof(mainData.bps[0].players));
        mainData.bps[0].numPlayers = mainData.numPlayers;
        memcpy(&mainData.bps[0].players, &mainData.players,
               sizeof(mainData.players));
    } else {
        // This is too big, but it works.
        uint maxBPs = mainData.numPlayers * mainData.numPlayers;
        mainData.bps = malloc(sizeof(mainData.bps[0]) * maxBPs);

        mainData.numBPs = 0;
        ASSERT(mainData.numPlayers > 0);
        ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
        for (uint x = 1; x < mainData.numPlayers; x++) {
            for (uint y = 1; y < mainData.numPlayers; y++) {
                if (x == y) {
                    continue;
                }

                uint b = mainData.numBPs++;
                mainData.bps[b] = bp;
                mainData.bps[b].numPlayers = 3;
                ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
                mainData.bps[b].players[0] = mainData.players[0];
                mainData.bps[b].players[1] = mainData.players[x];
                mainData.bps[b].players[2] = mainData.players[y];
            }
        }

        uint b = mainData.numBPs++;
        mainData.bps[b] = bp;
        mainData.bps[b].numPlayers = mainData.numPlayers;
        memcpy(&mainData.bps[b].players, &mainData.players,
               sizeof(mainData.players));

        ASSERT(mainData.numBPs <= maxBPs);

        for (uint b = 0; b < mainData.numBPs; b++) {
            BattleParams *bp = &mainData.bps[b];
            for (uint p = 0; p < bp->numPlayers; p++) {
                if (bp->players[p].mreg != NULL) {
                    bp->players[p].mreg =
                        MBRegistry_AllocCopy(bp->players[p].mreg);
                }
            }
        }
    }

    for (uint p = 0; p < bp.numPlayers; p++) {
        if (bp.players[p].mreg != NULL) {
            MBRegistry_Free(bp.players[p].mreg);
        }
    }
}

void MainParseCmdLine(int argc, char **argv)
{
    MBOption opts[] = {
        { "-h", "--help",       FALSE, "Print help text"               },
        { "-H", "--headless",   FALSE, "Run headless"                  },
        { "-F", "--frameSkip",  FALSE, "Allow frame skipping"          },
        { "-l", "--loop",       TRUE,  "Loop <arg> times"              },
        { "-T", "--tournament", FALSE, "Tournament mode"               },
        { "-s", "--seed",       TRUE,  "Set random seed"               },
        { "-L", "--tickLimit",  TRUE,  "Time limit in ticks"           },
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

    if (MBOpt_IsPresent("tournament")) {
        mainData.tournament = TRUE;
    } else {
        mainData.tournament = FALSE;
    }

    mainData.seed = MBOpt_GetUint64("seed");
    mainData.reuseSeed = MBOpt_GetBool("reuseSeed");

    mainData.tickLimit = MBOpt_GetInt("tickLimit");

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
        if (mainData.numBPs != 1) {
            PANIC("Multiple scenarios requries --headless\n");
        }
        Display_Init(&mainData.bps[0]);
    }

    uint battleId = 0;
    for (uint i = 0; i < mainData.loop; i++) {
        for (uint b = 0; b < mainData.numBPs; b++) {
            MainEngineWorkUnit wu;

            MBUtil_Zero(&wu, sizeof(wu));
            wu.type = MAIN_WORK_BATTLE;
            wu.battleId = battleId++;

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
            wu.bp = mainData.bps[b];

            WorkQueue_QueueItem(&mainData.workQ, &wu, sizeof(wu));
        }
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

        for (uint p = 0; p < ru.bs.numPlayers; p++) {
            PlayerUID puid = ru.bs.players[p].playerUID;
            BattlePlayerParams *bpp = &mainData.players[puid];
            ASSERT(puid < ARRAYSIZE(mainData.players));
            ASSERT(puid < ARRAYSIZE(mainData.winners));
            ASSERT(puid == bpp->playerUID);
            if (puid == ru.bs.winnerUID) {
                mainData.winners[puid].wins++;
            } else {
                mainData.winners[puid].losses++;
            }
            mainData.winners[puid].battles++;
        }
    }
    WorkQueue_Unlock(&mainData.resultQ);

    MainPrintWinners();

    for (uint b = 0; b < mainData.numBPs; b++) {
        BattleParams *bp = &mainData.bps[b];
        for (uint i = 0; i < bp->numPlayers; i++) {
            if (bp->players[i].mreg != NULL) {
                MBRegistry_Free(bp->players[i].mreg);
            }
        }
    }

    free(mainData.bps);
    mainData.bps = NULL;

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
