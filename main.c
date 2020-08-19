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
#include <unistd.h>
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
#include "MBString.h"

typedef enum MainEngineWorkType {
    MAIN_WORK_INVALID = 0,
    MAIN_WORK_EXIT    = 1,
    MAIN_WORK_BATTLE  = 2,
} MainEngineWorkType;

typedef struct MainEngineWorkUnit {
    MainEngineWorkType type;
    uint battleId;
    uint64 seed;
    BattleScenario bsc;
} MainEngineWorkUnit;

typedef struct MainEngineResultUnit {
    BattleStatus bs;
} MainEngineResultUnit;

typedef struct MainWinnerData {
    uint battles;
    uint wins;
    uint losses;
    uint draws;
} MainWinnerData;

typedef struct MainEngineThreadData {
    uint threadId;
    SDL_Thread *sdlThread;
    char threadName[64];
    uint32 startTimeMS;
    uint battleId;
    uint64 seed;
    BattleScenario bsc;
    Battle *battle;
} MainEngineThreadData;

struct MainData {
    bool headless;
    bool frameSkip;
    uint displayFrames;
    int loop;
    uint tickLimit;
    bool tournament;
    const char *scenario;

    bool reuseSeed;
    uint64 seed;
    RandomState rs;

    uint numBSCs;
    BattleScenario *bscs;

    int numPlayers;
    BattlePlayer players[MAX_PLAYERS];
    MainWinnerData winners[MAX_PLAYERS];
    MainWinnerData winnerBreakdown[MAX_PLAYERS][MAX_PLAYERS];

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

    for (uint i = 0; i < ARRAYSIZE(bStatus->players); i++) {
        if (bStatus->players[i].playerUID != PLAYER_ID_INVALID) {
            Warning("\tplayer[%d] numMobs = %d\n", i,
                    bStatus->players[i].numMobs);
        }
    }

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
        BattlePlayer *bpp = &mainData.players[bStatus->winnerUID];
        ASSERT(bStatus->winnerUID < ARRAYSIZE(mainData.players));
        Warning("Winner: %s\n", bpp->playerName);
    }
}

static void MainRecordWinner(MainWinnerData *wd, PlayerUID puid,
                             BattleStatus *bs)
{
    BattlePlayer *bpp = &mainData.players[puid];
    ASSERT(puid < ARRAYSIZE(mainData.players));
    ASSERT(puid == bpp->playerUID);
    if (puid == bs->winnerUID) {
        wd->wins++;
    } else if (bs->winnerUID == PLAYER_ID_NEUTRAL) {
        wd->draws++;
    } else {
        wd->losses++;
    }
    wd->battles++;
}

static void MainPrintWinnerData(MainWinnerData *wd)
{
    int wins = wd->wins;
    int losses = wd->losses;
    int battles = wd->battles;
    int draws = wd->draws;
    float percent = 100.0f * wins / (float)battles;

    Warning("\t%3d wins, %3d losses, %3d draws => %4.1f\% wins\n",
            wins, losses, draws, percent);
}

static void MainPrintWinners(void)
{
    uint32 totalBattles = 0;

    if (mainData.tournament) {
        Warning("\n");
        Warning("Winner Breakdown:\n");
        for (uint p1 = 0; p1 < mainData.numPlayers; p1++) {
            Warning("Fleet %s:\n", mainData.players[p1].playerName);
        for (uint p2 = 0; p2 < mainData.numPlayers; p2++) {
                    if (mainData.winnerBreakdown[p1][p2].battles > 0) {
                    Warning("\tvs %s:\n", mainData.players[p2].playerName,
                            mainData.players[p2].playerName);
                    MainPrintWinnerData(&mainData.winnerBreakdown[p1][p2]);
                }
            }
        }
    }

    Warning("\n");
    Warning("Summary:\n");

    for (uint i = 0; i < mainData.numPlayers; i++) {
        totalBattles += mainData.winners[i].wins;
    }

    for (uint i = 0; i < mainData.numPlayers; i++) {
        Warning("Fleet: %s\n", mainData.players[i].playerName);
        MainPrintWinnerData(&mainData.winners[i]);
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
    tData->bsc = wu->bsc;
    tData->battle = Battle_Create(&tData->bsc, wu->seed);

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
            uint s = mainData.headless ? 5000 : 500;
            if (bStatus->tick % s == 0) {
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

    for (uint i = 0; i < tData->bsc.bp.numPlayers; i++) {
        if (tData->bsc.players[i].mreg != NULL) {
            MBRegistry_Free(tData->bsc.players[i].mreg);
            tData->bsc.players[i].mreg = NULL;
        }
    }
}

void MainLoadScenario(MBRegistry *mreg, const char *scenario)
{
    MBString filename;
    bool useDefault = FALSE;

    struct {
        const char *key;
        const char *value;
    } defaultValues[] = {
        { "width", "1600",           },
        { "height", "1200",          },
        { "startingCredits", "1000", },
        { "creditsPerTick", "1",     },
        { "tickLimit", "100000",     },
        { "lootDropRate", "0.25",    },
        { "lootSpawnRate", "2.0",    },
        { "minLootSpawn", "10",      },
        { "maxLootSpawn", "20",      },
        { "restrictedStart", "TRUE", },
        { "startingBases", "1",      },
        { "startingFighters", "0",   },
    };

    if (scenario == NULL) {
        scenario = "default";
        useDefault = TRUE;
    }

    MBString_Create(&filename);
    MBString_AppendCStr(&filename, "scenarios/");
    MBString_AppendCStr(&filename, scenario);
    MBString_AppendCStr(&filename, ".sc");

    if (access(MBString_GetCStr(&filename), F_OK) == -1) {
        PANIC("Cannot access: %s\n", MBString_GetCStr(&filename));
    }

    if (useDefault) {
        for (uint i = 0; i < ARRAYSIZE(defaultValues); i++) {
            MBRegistry_Put(mreg, defaultValues[i].key,
                           defaultValues[i].value);
        }
    }

    MBRegistry_LoadSubset(mreg, MBString_GetCStr(&filename));
    MBString_Destroy(&filename);

    if (useDefault) {
        for (uint i = 0; i < ARRAYSIZE(defaultValues); i++) {
            ASSERT(strcmp(MBRegistry_GetCStr(mreg, defaultValues[i].key),
                          defaultValues[i].value) == 0);
        }
    }
}

void MainConstructScenario(void)
{
    uint p;
    MBRegistry *mreg;
    BattleScenario bsc;

    mreg = MBRegistry_Alloc();
    MainLoadScenario(mreg, NULL);
    if (mainData.scenario != NULL) {
        MainLoadScenario(mreg, mainData.scenario);
    }

    MBUtil_Zero(&bsc, sizeof(bsc));

    bsc.bp.width = MBRegistry_GetUint(mreg, "width");
    bsc.bp.height = MBRegistry_GetUint(mreg, "height");
    bsc.bp.startingCredits = MBRegistry_GetUint(mreg, "startingCredits");
    bsc.bp.creditsPerTick = MBRegistry_GetUint(mreg, "creditsPerTick");
    bsc.bp.tickLimit = MBRegistry_GetUint(mreg, "tickLimit");
    bsc.bp.lootDropRate = MBRegistry_GetFloat(mreg, "lootDropRate");
    bsc.bp.lootSpawnRate = MBRegistry_GetFloat(mreg, "lootSpawnRate");
    bsc.bp.minLootSpawn = MBRegistry_GetUint(mreg, "minLootSpawn");
    bsc.bp.maxLootSpawn = MBRegistry_GetUint(mreg, "maxLootSpawn");
    bsc.bp.restrictedStart = MBRegistry_GetBool(mreg, "restrictedStart");
    bsc.bp.startingBases = MBRegistry_GetUint(mreg, "startingBases");
    bsc.bp.startingFighters = MBRegistry_GetUint(mreg, "startingFighters");

    MBRegistry_Free(mreg);
    mreg = NULL;

    if (mainData.tickLimit != 0) {
        bsc.bp.tickLimit = mainData.tickLimit;
    }

    /*
     * The NEUTRAL fleet always needs to be there.
     */
    p = 0;
    mainData.players[p].aiType = FLEET_AI_NEUTRAL;
    p++;

    if (mainData.tournament) {
        uint i = FLEET_AI_NEUTRAL + 1;
        ASSERT(p == FLEET_AI_NEUTRAL);

        while (i < FLEET_AI_MAX) {
            mainData.players[p].aiType = i;
            i++;
            p++;
        }
    } else {
        /*
         * Rough order of difficulty:
         *    DummyFleet
         *    SimpleFleet
         *    GatherFleet
         *    CloudFleet
         *    MapperFleet
         *    BobFleet
         *    CowardFleet
         */

//         mainData.players[p].mreg = MBRegistry_Alloc();
//         MBRegistry_Put(mainData.players[p].mreg, "StartingWaveSize", "5");
//         MBRegistry_Put(mainData.players[p].mreg, "WaveSizeIncrement", "0");
//         MBRegistry_Put(mainData.players[p].mreg, "RandomWaves", "FALSE");
//         mainData.players[p].aiType = FLEET_AI_MAPPER;
//         p++;

//         mainData.players[p].aiType = FLEET_AI_FF;
//         p++;

        mainData.players[p].aiType = FLEET_AI_COWARD;
        p++;

        mainData.players[p].aiType = FLEET_AI_BOB;
        p++;
    }

    ASSERT(p <= ARRAYSIZE(mainData.players));
    mainData.numPlayers = p;

    for (uint i = 0; i < p; i++) {
        mainData.players[i].playerUID = i;
        if (mainData.players[i].playerName == NULL) {
            FleetAIType aiType = mainData.players[i].aiType;
            mainData.players[i].playerName = Fleet_GetName(aiType);
        }
    }

    if (!mainData.tournament) {
        mainData.numBSCs = 1;
        mainData.bscs = malloc(sizeof(mainData.bscs[0]));
        mainData.bscs[0] = bsc;
        ASSERT(sizeof(mainData.players) ==
               sizeof(mainData.bscs[0].players));
        mainData.bscs[0].bp.numPlayers = mainData.numPlayers;
        memcpy(&mainData.bscs[0].players, &mainData.players,
               sizeof(mainData.players));
    } else {
        // This is too big, but it works.
        uint maxBscs = mainData.numPlayers * mainData.numPlayers;
        mainData.bscs = malloc(sizeof(mainData.bscs[0]) * maxBscs);

        mainData.numBSCs = 0;
        ASSERT(mainData.numPlayers > 0);
        ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
        for (uint x = 1; x < mainData.numPlayers; x++) {
            for (uint y = 1; y < mainData.numPlayers; y++) {
                if (x == y) {
                    continue;
                }

                uint b = mainData.numBSCs++;
                mainData.bscs[b].bp = bsc.bp;
                mainData.bscs[b].bp.numPlayers = 3;
                ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
                mainData.bscs[b].players[0] = mainData.players[0];
                mainData.bscs[b].players[1] = mainData.players[x];
                mainData.bscs[b].players[2] = mainData.players[y];
            }
        }

//         // Battle royale
//         uint b = mainData.numBSCs++;
//         mainData.bscs[b].bp = bsc.bp;
//         mainData.bscs[b].bp.numPlayers = mainData.numPlayers;
//         memcpy(&mainData.bscs[b].players, &mainData.players,
//                sizeof(mainData.players));

        ASSERT(mainData.numBSCs <= maxBscs);
    }
}

void MainUnitTests()
{
    Warning("Starting Unit Tests ...\n");
    MobPSet_UnitTest();
    Warning("Done!\n");
}

void MainParseCmdLine(int argc, char **argv)
{
    MBOption opts[] = {
        { "-h", "--help",       FALSE, "Print help text"               },
        { "-H", "--headless",   FALSE, "Run headless"                  },
        { "-F", "--frameSkip",  FALSE, "Allow frame skipping"          },
        { "-l", "--loop",       TRUE,  "Loop <arg> times"              },
        { "-S", "--scenario",   TRUE,  "Scenario type"                 },
        { "-T", "--tournament", FALSE, "Tournament mode"               },
        { "-s", "--seed",       TRUE,  "Set random seed"               },
        { "-L", "--tickLimit",  TRUE,  "Time limit in ticks"           },
        { "-t", "--numThreads", TRUE,  "Number of engine threads"      },
        { "-R", "--reuseSeed",  FALSE, "Reuse the seed across battles" },
        { "-u", "--unitTests",  FALSE, "Run unit tests"                },
    };

    MBOpt_Init(opts, ARRAYSIZE(opts), argc, argv);

    if (MBOpt_IsPresent("help")) {
        MBOpt_PrintHelpText();
        exit(1);
    }

    if (MBOpt_IsPresent("unitTests")) {
        MainUnitTests();
        exit(0);
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

    mainData.scenario = NULL;
    if (MBOpt_IsPresent("scenario")) {
        mainData.scenario = MBOpt_GetCStr("scenario");
    }
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
        if (mainData.numBSCs != 1) {
            PANIC("Multiple scenarios requries --headless\n");
        }
        Display_Init(&mainData.bscs[0].bp);
    }

    uint battleId = 0;
    for (uint i = 0; i < mainData.loop; i++) {
        for (uint b = 0; b < mainData.numBSCs; b++) {
            MainEngineWorkUnit wu;
            MBUtil_Zero(&wu, sizeof(wu));
            wu.bsc = mainData.bscs[b];

            for (uint p = 0; p < wu.bsc.bp.numPlayers; p++) {
                if (wu.bsc.players[p].mreg != NULL) {
                    wu.bsc.players[p].mreg =
                        MBRegistry_AllocCopy(wu.bsc.players[p].mreg);
                }
            }

            wu.type = MAIN_WORK_BATTLE;
            wu.battleId = battleId++;

            if ((i == 0 && b == 0) || mainData.reuseSeed) {
                /*
                 * Use the actual seed for the first battle, so that it's
                 * easy to re-create a single battle from the battle seed
                 * without specifying --reuseSeed.
                 */
                wu.seed = RandomState_GetSeed(&mainData.rs);
            } else {
                wu.seed = RandomState_Uint64(&mainData.rs);
            }

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
            ASSERT(puid < ARRAYSIZE(mainData.winners));
            MainRecordWinner(&mainData.winners[puid], puid, &ru.bs);
        }
        if (ru.bs.numPlayers == 3) {
            PlayerUID puid1 = ru.bs.players[1].playerUID;
            PlayerUID puid2 = ru.bs.players[2].playerUID;
            ASSERT(ru.bs.players[0].playerUID == PLAYER_ID_NEUTRAL);
            MainRecordWinner(&mainData.winnerBreakdown[puid1][puid2], puid1, &ru.bs);
            MainRecordWinner(&mainData.winnerBreakdown[puid2][puid1], puid2, &ru.bs);
        }
    }
    WorkQueue_Unlock(&mainData.resultQ);

    MainPrintWinners();

    for (uint p = 0; p < mainData.numPlayers; p++) {
        if (mainData.players[p].mreg != NULL) {
            MBRegistry_Free(mainData.players[p].mreg);
            mainData.players[p].mreg = NULL;
        }
    }

    free(mainData.bscs);
    mainData.bscs = NULL;

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
