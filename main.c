/*
 * main.c -- part of SpaceRobots2
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

#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <SDL2/SDL.h>

#include "SR2Config.h"

#include "MBConfig.h"
#include "MBTypes.h"
#include "MBUtil.h"
#include "MBAssert.h"
#include "Random.h"
#include "display.h"
#include "geometry.h"
#include "battle.h"
#include "fleet.h"
#include "MBOpt.h"
#include "workQueue.h"
#include "MBString.h"
#include "mutate.h"
#include "MBStrTable.h"
#include "MBUnitTest.h"

// From ml.hpp
extern void ML_UnitTest();

typedef enum MainBattleType {
    /*
     * Single battle-royale with all players.
     */
    MAIN_BT_SINGLE,

    /*
     * Run each control fleet 1x1 against each target fleet.
     */
    MAIN_BT_OPTIMIZE,

    /*
     * Run every fleet 1x1 against each other fleet.
     */
    MAIN_BT_TOURNAMENT,
} MainBattleType;

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
    uint targetFPS;
    uint displayFrames;
    int loop;
    uint tickLimit;
    bool printWinnerBreakdown;
    bool startPaused;
    const char *scenario;

    bool reuseSeed;
    uint64 seed;
    RandomState rs;

    uint numBSCs;
    BattleScenario *bscs;
    uint maxBscs;

    uint totalBattles;
    bool doneQueueing;

    uint numPlayers;
    BattlePlayer players[MAX_PLAYERS];
    MainWinnerData winners[MAX_PLAYERS];
    MainWinnerData winnerBreakdown[MAX_PLAYERS][MAX_PLAYERS];

    bool threadsInitialized;
    bool threadsRequestExit;
    uint numThreads;
    MainEngineThreadData *tData;
    WorkQueue workQ;
    WorkQueue resultQ;

    volatile bool asyncExit;
} mainData;

static void MainRunBattle(MainEngineThreadData *tData,
                          MainEngineWorkUnit *wu);
static void MainLoadScenario(MBRegistry *mreg, const char *scenario);

static void MainAddTargetPlayersForOptimize(void);
static void MainUsePopulation(const char *file,
                              bool incrementAge);
static uint32 MainFindRandomFleet(BattlePlayer *mainPlayers, uint32 mpSize,
                                  uint32 startingMPIndex, uint32 numFleets,
                                  bool useWinRatio, float *weightOut);
static uint32 MainFleetCompetition(BattlePlayer *mainPlayers, uint32 mpSize,
                                   uint32 startingMPIndex, uint32 numFleets,
                                   bool useWinRatio);
static void MainKillFleet(BattlePlayer *mainPlayers,
                          uint32 mpSize, uint32 *mpIndex,
                          uint32 startingMPIndex, uint32 *numFleets,
                          uint32 *numTargetFleets, uint32 fi);
static bool MainIsFleetDefective(BattlePlayer *player, float defectiveLevel);
static void MainResetFleetStats(void);
static void MainCleanupPlayers(void);
static void MainMutateFleet(BattlePlayer *mainPlayers, uint32 mpSize,
                            BattlePlayer *dest,
                            uint32 mi, uint32 bi);

static void MainThreadsInit(void);
static void MainThreadsRequestExit(void);
static void MainThreadsExit(void);

static void MainProcessSingleResult(MainEngineResultUnit *ru);
static void MainPrintWinners(void);

static void MainAddNeutralPlayer(void)
{
    if (mainData.numPlayers > 0) {
        ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    } else {
        mainData.players[0].aiType = FLEET_AI_NEUTRAL;
        mainData.players[0].playerType = PLAYER_TYPE_NEUTRAL;
        mainData.numPlayers++;
    }
}

static void MainLoadAllAsControlPlayers(void)
{
    MainAddNeutralPlayer();

    /*
     * Add everybody in order of rankings.
     */
    for (int i = 0; i < FLEET_AI_MAX; i++) {
        FleetAIType aiType = Fleet_GetTypeFromRanking(i);
        if (aiType != FLEET_AI_INVALID) {
            ASSERT(Fleet_GetRanking(aiType) == i);
            ASSERT(mainData.numPlayers < ARRAYSIZE(mainData.players));
            mainData.players[mainData.numPlayers].aiType = aiType;
            mainData.players[mainData.numPlayers].playerType = PLAYER_TYPE_CONTROL;
            mainData.numPlayers++;
        }
    }
}

static void MainLoadDefaultPlayers(void)
{
    /*
     * The NEUTRAL fleet always needs to be there.
     */
    MainAddNeutralPlayer();

    if (MBOpt_IsPresent("usePopulation")) {
        MainUsePopulation(MBOpt_GetCStr("usePopulation"), TRUE);
    } else {
        uint p = mainData.numPlayers;
        /*
         * See fleet.c::gRankings for a rough order of fleet strength.
         */

        mainData.players[p].aiType = FLEET_AI_HOLD;
        p++;

        mainData.players[p].aiType = FLEET_AI_FLOCK9;
        p++;

        mainData.players[p].aiType = FLEET_AI_BUNDLE15;
        p++;

        mainData.players[p].aiType = FLEET_AI_NEURAL12;
        p++;

        mainData.players[p].aiType = FLEET_AI_BINEURAL1;
        p++;

        //mainData.players[p].playerName = "HoldMod";
        //mainData.players[p].aiType = FLEET_AI_HOLD;
        //mainData.players[p].mreg = MBRegistry_Alloc();
        //MBRegistry_PutConst(mainData.players[p].mreg, "holdCount", "1");
        //p++;

        ASSERT(p <= ARRAYSIZE(mainData.players));
        mainData.numPlayers = p;
    }
}

static void MainConstructScenarios(bool loadPlayers, MainBattleType bt)
{
    MBRegistry *mreg = MBRegistry_Alloc();
    BattleScenario bsc;

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
    bsc.bp.powerCoreDropRate = MBRegistry_GetFloat(mreg, "powerCoreDropRate");
    bsc.bp.powerCoreSpawnRate = MBRegistry_GetFloat(mreg, "powerCoreSpawnRate");
    bsc.bp.minPowerCoreSpawn = MBRegistry_GetUint(mreg, "minPowerCoreSpawn");
    bsc.bp.maxPowerCoreSpawn = MBRegistry_GetUint(mreg, "maxPowerCoreSpawn");
    bsc.bp.restrictedStart = MBRegistry_GetBool(mreg, "restrictedStart");
    bsc.bp.startingBases = MBRegistry_GetUint(mreg, "startingBases");
    bsc.bp.startingFighters = MBRegistry_GetUint(mreg, "startingFighters");
    bsc.bp.baseVictory = MBRegistry_GetBool(mreg, "baseVictory");

    MBRegistry_Free(mreg);
    mreg = NULL;

    if (mainData.tickLimit != 0) {
        bsc.bp.tickLimit = mainData.tickLimit;
    }

    if (loadPlayers) {
        MainLoadDefaultPlayers();
    }
    ASSERT(mainData.numPlayers > 1);
    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    uint p = mainData.numPlayers;

    for (uint i = 0; i < p; i++) {
        mainData.players[i].playerUID = i;
        if (mainData.players[i].playerName == NULL) {
            FleetAIType aiType = mainData.players[i].aiType;
            mainData.players[i].playerName = Fleet_GetName(aiType);
        }

        ASSERT(mainData.players[i].playerType < PLAYER_TYPE_MAX);
        if (mainData.players[i].playerType == PLAYER_TYPE_INVALID) {
            mainData.players[i].playerType = PLAYER_TYPE_TARGET;
        }
    }

    if (bt == MAIN_BT_OPTIMIZE) {
        mainData.maxBscs = p * p + 1;
        ASSERT(mainData.maxBscs > p);
        ASSERT(mainData.maxBscs > p * p);
        ASSERT(mainData.maxBscs * sizeof(mainData.bscs[0]) > mainData.maxBscs);
        mainData.bscs = malloc(sizeof(mainData.bscs[0]) * mainData.maxBscs);

        mainData.numBSCs = 0;

        for (uint ti = 0; ti < p; ti++) {
            if (mainData.players[ti].playerType != PLAYER_TYPE_TARGET) {
                continue;
            }

            for (uint ci = 0; ci < p; ci++) {
                if (mainData.players[ci].playerType != PLAYER_TYPE_CONTROL) {
                    continue;
                }

                uint b = mainData.numBSCs++;
                ASSERT(b < mainData.maxBscs);
                mainData.bscs[b].bp = bsc.bp;
                mainData.bscs[b].bp.numPlayers = 3;
                ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
                mainData.bscs[b].players[0] = mainData.players[0];
                mainData.bscs[b].players[1] = mainData.players[ti];
                ASSERT(mainData.players[ti].playerType == PLAYER_TYPE_TARGET);
                mainData.bscs[b].players[2] = mainData.players[ci];
                ASSERT(mainData.players[ci].playerType == PLAYER_TYPE_CONTROL);
            }
        }

        ASSERT(mainData.numBSCs <= mainData.maxBscs);
    } else if (bt == MAIN_BT_TOURNAMENT) {
        // This is too big, but it works.
        mainData.maxBscs = mainData.numPlayers * mainData.numPlayers;
        mainData.bscs = malloc(sizeof(mainData.bscs[0]) * mainData.maxBscs);

        mainData.numBSCs = 0;
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

        ASSERT(mainData.numBSCs <= mainData.maxBscs);
    } else {
        ASSERT(bt == MAIN_BT_SINGLE);

        mainData.numBSCs = 1;
        mainData.bscs = malloc(sizeof(mainData.bscs[0]));
        mainData.bscs[0] = bsc;
        ASSERT(sizeof(mainData.players) ==
               sizeof(mainData.bscs[0].players));
        mainData.bscs[0].bp.numPlayers = mainData.numPlayers;
        memcpy(&mainData.bscs[0].players, &mainData.players,
               sizeof(mainData.players));
    }
}

static void MainRunScenarios(void)
{
    if (!mainData.headless) {
        if (mainData.numThreads != 1) {
            PANIC("Multiple threads requires --headless\n");
        }
        if (mainData.numBSCs != 1) {
            PANIC("Multiple scenarios requries --headless\n");
        }
        if (mainData.loop != 1) {
            PANIC("Multiple battles requires --headless\n");
        }
        Display_Init(&mainData.bscs[0]);
        Display_SetFPS(mainData.targetFPS);

    }

    ASSERT(mainData.numBSCs > 0);
    ASSERT(mainData.loop > 0);
    ASSERT(mainData.numThreads > 0);

    mainData.totalBattles = mainData.loop * mainData.numBSCs;
    mainData.numThreads = MAX(1, mainData.numThreads);
    mainData.numThreads = MIN(mainData.totalBattles, mainData.numThreads);
    MainThreadsInit();

    uint battleId = 0;

    WorkQueue_Lock(&mainData.workQ);
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

            Warning("Queueing Battle %d of %d...\n", wu.battleId,
                    mainData.totalBattles);
            WorkQueue_QueueItemLocked(&mainData.workQ, &wu, sizeof(wu));

            if ((battleId + 1) % mainData.numThreads == 0) {
                WorkQueue_Unlock(&mainData.workQ);
                uint workTarget = MAX(10, mainData.numThreads * 4);

                /*
                 * Try to process the results as they come in, to reduce
                 * memory usage.
                 */
                while (!WorkQueue_IsEmpty(&mainData.resultQ) &&
                       !WorkQueue_IsCountBelow(&mainData.workQ, workTarget)) {
                    MainEngineResultUnit ru;
                    WorkQueue_WaitForItem(&mainData.resultQ, &ru, sizeof(ru));
                    MainProcessSingleResult(&ru);
                }

                WorkQueue_WaitForCountBelow(&mainData.workQ, workTarget);
                WorkQueue_Lock(&mainData.workQ);
            }
        }
    }
    WorkQueue_Unlock(&mainData.workQ);
    Warning("Done Queueing\n");
    mainData.doneQueueing = TRUE;

    if (!mainData.headless) {
        Display_Main(mainData.startPaused);
        mainData.asyncExit = TRUE;
        Display_Exit();
    }

    WorkQueue_WaitForAllFinished(&mainData.workQ);
    ASSERT(WorkQueue_IsIdle(&mainData.workQ));
    MainThreadsRequestExit();

    WorkQueue_Lock(&mainData.resultQ);
    uint qSize = WorkQueue_QueueSize(&mainData.resultQ);
    for (uint i = 0; i < qSize; i++) {
        MainEngineResultUnit ru;
        WorkQueue_GetItemLocked(&mainData.resultQ, &ru, sizeof(ru));
        MainProcessSingleResult(&ru);
    }
    WorkQueue_Unlock(&mainData.resultQ);
    ASSERT(WorkQueue_IsEmpty(&mainData.resultQ));

    MainPrintWinners();

    free(mainData.bscs);
    mainData.bscs = NULL;

    MainThreadsExit();
}

static void
MainAddTargetPlayersForOptimize(void)
{
    const int doSimple = 0;
    const int doTable = 1;
    const int doRandom = 2;
    int method = doSimple;
    BattlePlayer targetPlayers[MAX_PLAYERS];
    uint32 tpIndex = 0;
    BattlePlayer *mainPlayers = &mainData.players[0];
    uint32 mpSize = ARRAYSIZE(mainData.players);
    uint32 *mpIndex = &mainData.numPlayers;

    ASSERT(Fleet_GetTypeFromRanking(-1) == FLEET_AI_INVALID);
    ASSERT(Fleet_GetTypeFromRanking(FLEET_AI_MAX) == FLEET_AI_INVALID);

    /*
     * Target fleets to optimize.
     * Customize as needed.
     */
    if (method == doSimple) {
        targetPlayers[tpIndex].aiType = FLEET_AI_FLOCK9;
        targetPlayers[tpIndex].playerName = "FlockFleet9.Test";
        tpIndex++;

        // targetPlayers[tpIndex].mreg = MBRegistry_Alloc();
        // MBRegistry_PutConst(targetPlayers[tpIndex].mreg, "gatherRange", "200");
        // MBRegistry_PutConst(targetPlayers[tpIndex].mreg, "attackRange", "100");
    } else if (method == doTable) {
        struct {
            float attackRange;
            bool attackExtendedRange;
            float holdCount;
        } v[] = {
            { 100, TRUE,  100, },
            { 100, TRUE,   50, },
            { 100, FALSE,  50, },
        };

        for (uint i = 0; i < ARRAYSIZE(v); i++) {
            char *vstr[3];

            MBUtil_Zero(&vstr, sizeof(vstr));

            targetPlayers[tpIndex].mreg = MBRegistry_Alloc();

            asprintf(&vstr[0], "%1.0f", v[i].attackRange);
            MBRegistry_PutConst(targetPlayers[tpIndex].mreg, "attackRange", vstr[0]);

            asprintf(&vstr[1], "%d", v[i].attackExtendedRange);
            MBRegistry_PutConst(targetPlayers[tpIndex].mreg, "attackExtendedRange", vstr[1]);

            asprintf(&vstr[2], "%1.0f", v[i].holdCount);
            MBRegistry_PutConst(targetPlayers[tpIndex].mreg, "holdCount", vstr[2]);

            targetPlayers[tpIndex].aiType = FLEET_AI_HOLD;

            char *name = NULL;
            asprintf(&name, "%s %s:%s:%s",
                    Fleet_GetName(targetPlayers[tpIndex].aiType),
                    vstr[0], vstr[1], vstr[2]);
            targetPlayers[tpIndex].playerName = name;

            tpIndex++;

            // XXX: Leak strings!
        }
    } else {
        ASSERT(method == doRandom);

        struct {
            const char *param;
            float minValue;
            float maxValue;
        } v[] = {
            // { "baseRadius",        50.0f, 500.0f,  },
            // { "baseWeight",        -1.0f, 1.0f,    },
            // { "nearBaseRadius",    50.0f, 500.0f,  },
            // { "baseDefenseRadius", 50.0f, 500.0f,  },

            // { "enemyBaseRadius",    50.0f, 500.0f, },
            // { "enemyBaseWeight",    -1.0f, 1.0f,   },

            { "baseRadius",        50.0f, 500.0f,  },
            { "baseWeight",        -0.01f, 0.01f,  },
            { "nearBaseRadius",    50.0f, 200.0f,  },
            { "baseDefenseRadius", 200.0f, 500.0f, },

            { "enemyBaseRadius",  300.0f, 700.0f,  },
            { "enemyBaseWeight",    0.1f, 0.3f,   },
        };

        for (uint f = 0; f < 5; f++) {
            char *vstr[6];
            MBUtil_Zero(vstr, sizeof(vstr));
            ASSERT(ARRAYSIZE(vstr) >= ARRAYSIZE(v));
            MBUtil_Zero(&vstr, sizeof(vstr));

            targetPlayers[tpIndex].mreg = MBRegistry_Alloc();

            for (uint i = 0; i < ARRAYSIZE(v); i++) {
                float value = Random_Float(v[i].minValue, v[i].maxValue);
                asprintf(&vstr[i], "%1.2f", value);
                MBRegistry_PutConst(targetPlayers[tpIndex].mreg, v[i].param, vstr[i]);
            }

            targetPlayers[tpIndex].aiType = FLEET_AI_FLOCK4;
            char *name = NULL;
            asprintf(&name, "%s %s:%s %s:%s %s:%s",
                     Fleet_GetName(targetPlayers[tpIndex].aiType),
                     vstr[0], vstr[1], vstr[2], vstr[3], vstr[4],
                     vstr[5]);
            // asprintf(&name, "%s %s",
            //              Fleet_GetName(targetPlayers[tpIndex].aiType),
            //              vstr[0]);
            targetPlayers[tpIndex].playerName = name;

            tpIndex++;

            // XXX: Leak strings!
        }
    }

    /*
     * Copy over target players.
     */
    for (uint i = 0; i < tpIndex; i++) {
        ASSERT(*mpIndex < mpSize);
        targetPlayers[i].playerType = PLAYER_TYPE_TARGET;
        mainPlayers[*mpIndex] = targetPlayers[i];
        (*mpIndex)++;
    }
}

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

    ASSERT(wd->wins >= 0);
    ASSERT(wd->losses >= 0);
    ASSERT(wd->draws >= 0);
    ASSERT(wd->battles >= 0);
    ASSERT(wd->wins + wd->losses + wd->draws == wd->battles);
}

static void MainPrintWinnerData(MainWinnerData *wd)
{
    int wins = wd->wins;
    int losses = wd->losses;
    int battles = wd->battles;
    int draws = wd->draws;
    float percent = 100.0f * wins / (float)battles;

    ASSERT(wd->wins >= 0);
    ASSERT(wd->losses >= 0);
    ASSERT(wd->draws >= 0);
    ASSERT(wd->battles >= 0);
    ASSERT(wd->wins + wd->losses + wd->draws == wd->battles);

    Warning("\t%3d wins, %3d losses, %3d draws => %4.1f%% wins\n",
            wins, losses, draws, percent);
}

static void MainPrintWinners(void)
{
    uint32 totalBattles = 0;

    if (mainData.printWinnerBreakdown) {
        Warning("\n");
        Warning("Winner Breakdown:\n");
        for (uint p1 = 0; p1 < mainData.numPlayers; p1++) {
            Warning("Fleet %s:\n", mainData.players[p1].playerName);
            for (uint p2 = 0; p2 < mainData.numPlayers; p2++) {
                if (mainData.winnerBreakdown[p1][p2].battles > 0) {
                    Warning("\tvs %s:\n", mainData.players[p2].playerName);
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

static void MainDumpAddToKey(MBRegistry *source, MBRegistry *dest,
                             const MBString *destPrefix,
                             const char *key,
                             uint value)
{
    uint x;
    MBString destKey;


    MBString_Create(&destKey);


    MBString_MakeEmpty(&destKey);
    if (destPrefix != NULL) {
        MBString_Copy(&destKey, destPrefix);
    }

    MBString_AppendCStr(&destKey, key);
    if (source != NULL) {
        x = MBRegistry_GetUint(source, key);
    } else {
        x = 0;
    }

    if (value + x == 0) {
        MBRegistry_Remove(dest, MBString_GetCStr(&destKey));
    } else {
        MBString tmp;
        MBString_Create(&tmp);
        MBString_IntToString(&tmp, value + x);
        MBRegistry_PutCopy(dest, MBString_GetCStr(&destKey),
                           MBString_GetCStr(&tmp));
        MBString_Destroy(&tmp);
    }

    MBString_Destroy(&destKey);
}

static void MainDumpPopulation(const char *outputFile, bool targetOnly)
{
    uint32 i;
    MBRegistry *popReg;
    MBString prefix;
    MBString key;
    MBString tmp;
    uint32 numFleets = 0;
    uint32 fleetI = 1;

    ASSERT(outputFile != NULL);

    MBString_Create(&prefix);
    MBString_Create(&key);
    MBString_Create(&tmp);

    popReg = MBRegistry_Alloc();
    VERIFY(popReg != NULL);

    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    for (i = 1; i < mainData.numPlayers; i++) {
        if (targetOnly &&
            mainData.players[i].playerType != PLAYER_TYPE_TARGET) {
            continue;
        }

        MainWinnerData *wd = &mainData.winners[i];
        const char *fleetName = Fleet_GetName(mainData.players[i].aiType);

        numFleets++;

        MBString_CopyCStr(&prefix, "fleet");
        MBString_IntToString(&tmp, fleetI);
        fleetI++;
        MBString_AppendStr(&prefix, &tmp);
        MBString_AppendCStr(&prefix, ".");

        /*
         * Copy them over first, so we can override some of them.
         * Because we're using unique prefixes, we can assume that the
         * key isn't already in the registry.
         */
        if (mainData.players[i].mreg != NULL) {
            MBRegistry_PutAllUnique(popReg, mainData.players[i].mreg,
                                    MBString_GetCStr(&prefix));
        }

        MBString_Copy(&key, &prefix);
        MBString_AppendCStr(&key, "abattle.fleetName");
        MBRegistry_PutCopy(popReg, MBString_GetCStr(&key), fleetName);

        MBString_Copy(&key, &prefix);
        MBString_AppendCStr(&key, "abattle.playerType");
        MBRegistry_PutCopy(popReg, MBString_GetCStr(&key),
                           PlayerType_ToString(mainData.players[i].playerType));

        MainDumpAddToKey(mainData.players[i].mreg, popReg,
                         &prefix, "abattle.numBattles", wd->battles);
        MainDumpAddToKey(mainData.players[i].mreg, popReg,
                         &prefix, "abattle.numWins", wd->wins);
        MainDumpAddToKey(mainData.players[i].mreg, popReg,
                         &prefix, "abattle.numLosses", wd->losses);
        MainDumpAddToKey(mainData.players[i].mreg, popReg,
                         &prefix, "abattle.numDraws", wd->draws);
    }

    MBString_IntToString(&tmp, numFleets);
    MBRegistry_PutCopy(popReg, "numFleets", MBString_GetCStr(&tmp));

    MBRegistry_Save(popReg, outputFile);

    MBString_Destroy(&prefix);
    MBString_Destroy(&key);
    MBString_Destroy(&tmp);
    MBRegistry_Free(popReg);
}
static void MainUsePopulation(const char *file,
                              bool incrementAge)
{
    MBRegistry *popReg;
    MBRegistry *fleetReg;
    uint32 numFleets;
    uint32 numTargetFleets = 0;
    MBString tmp;
    BattlePlayer *mainPlayers = &mainData.players[0];
    uint32 mpSize = ARRAYSIZE(mainData.players);
    uint32 *mpIndex = &mainData.numPlayers;

    MainAddNeutralPlayer();

    MBString_Create(&tmp);

    popReg = MBRegistry_Alloc();
    VERIFY(popReg != NULL);

    fleetReg = MBRegistry_Alloc();
    VERIFY(fleetReg != NULL);

    ASSERT(file != NULL);
    MBRegistry_Load(popReg, file);

    if (!MBRegistry_ContainsKey(popReg, "numFleets")) {
        PANIC("Missing key: numFleets (file=%s)\n", file);
    }
    numFleets = MBRegistry_GetUint(popReg, "numFleets");
    if (numFleets == 0) {
        PANIC("Bad value for numFleets=%d (file=%s)\n", numFleets, file);
    }

    /*
     * Load fleets.
     */
    for (uint32 i = 1; i <= numFleets; i++) {
        MBRegistry_MakeEmpty(fleetReg);

        MBString_IntToString(&tmp, i);
        MBString_PrependCStr(&tmp, "fleet");
        MBString_AppendCStr(&tmp, ".");

        MBRegistry_SplitOnPrefix(fleetReg, popReg, MBString_GetCStr(&tmp),
                                 FALSE);

        ASSERT(*mpIndex < mpSize);

        if (MBRegistry_GetCStr(fleetReg, "abattle.fleetName") == NULL) {
            MBRegistry_DebugDump(fleetReg);
            PANIC("Missing key: fleetName, i=%d\n", i);
        }
        if (MBRegistry_ContainsKey(fleetReg, "abattle.playerName")) {
            mainPlayers[*mpIndex].playerName =
                strdup(MBRegistry_GetCStr(fleetReg, "abattle.playerName"));
        } else {
            mainPlayers[*mpIndex].playerName =
                strdup(MBRegistry_GetCStr(fleetReg, "abattle.fleetName"));
        }

        if (MBRegistry_ContainsKey(fleetReg, "abattle.age")) {
            if (incrementAge) {
                uint age = MBRegistry_GetUint(fleetReg, "abattle.age");
                MBString_IntToString(&tmp, age + 1);
                MBRegistry_PutCopy(fleetReg, "abattle.age",
                                   MBString_GetCStr(&tmp));
            }
        } else {
            MBRegistry_PutCopy(fleetReg, "abattle.age", "0");
        }

        mainPlayers[*mpIndex].mreg = MBRegistry_AllocCopy(fleetReg);
        mainPlayers[*mpIndex].playerType =
            PlayerType_FromString(MBRegistry_GetCStr(fleetReg, "abattle.playerType"));
        VERIFY(mainPlayers[*mpIndex].playerType != PLAYER_TYPE_INVALID);
        VERIFY(mainPlayers[*mpIndex].playerType < PLAYER_TYPE_MAX);

        if (mainPlayers[*mpIndex].playerType == PLAYER_TYPE_TARGET) {
            numTargetFleets++;
        }

        const char *fleetName = MBRegistry_GetCStr(fleetReg, "abattle.fleetName");
        mainPlayers[*mpIndex].aiType = Fleet_GetTypeFromName(fleetName);
        if (mainPlayers[*mpIndex].aiType == FLEET_AI_INVALID) {
            PANIC("Unknown Fleet Name: %s\n", fleetName);
        }
        (*mpIndex)++;
    }

    MBRegistry_Free(popReg);
    MBRegistry_Free(fleetReg);
    MBString_Destroy(&tmp);
}

static uint32 MainFleetCompetition(BattlePlayer *mainPlayers, uint32 mpSize,
                                   uint32 startingMPIndex, uint32 numFleets,
                                   bool useWinRatio)
{
    float w1, w2;
    uint f1 = MainFindRandomFleet(mainPlayers, mpSize,
                                  startingMPIndex, numFleets,
                                  useWinRatio, &w1);

    uint f2 = MainFindRandomFleet(mainPlayers, mpSize,
                                  startingMPIndex, numFleets,
                                  useWinRatio, &w2);

    if (Random_Bit() || w1 >= w2) {
        return f1;
    } else {
        return f2;
    }
}

static uint32 MainFindRandomFleet(BattlePlayer *mainPlayers, uint32 mpSize,
                                  uint32 startingMPIndex, uint32 numFleets,
                                  bool useWinRatio, float *weightOut)
{
    uint32 iterations = 0;
    uint32 i;

    i = Random_Int(0, numFleets - 1);
    while (TRUE) {
        uint32 fi = i + startingMPIndex;
        MBRegistry *fleetReg = mainPlayers[fi].mreg;
        float sProb;
        uint32 numBattles = MBRegistry_GetUint(fleetReg, "abattle.numBattles");
        uint32 weight;

        if (useWinRatio) {
            weight = MBRegistry_GetUint(fleetReg, "abattle.numWins");
        } else {
            weight = MBRegistry_GetUint(fleetReg, "abattle.numLosses") +
                     MBRegistry_GetUint(fleetReg, "abattle.numDraws");
        }

        sProb = numBattles > 0 ? weight / (float)numBattles : 0.0f;

        sProb += (iterations / numFleets) + 0.01;
        sProb = MIN(1.0f, sProb);
        sProb = MAX(0.0f, sProb);
        if (mainPlayers[fi].playerType == PLAYER_TYPE_TARGET) {
            if (Random_Flip(sProb)) {
                ASSERT(fi < mpSize);

                if (weightOut != NULL) {
                    *weightOut = sProb;
                }
                return fi;
            }
        }

        //i = (i + 1) % numFleets;
        i = Random_Int(0, numFleets - 1);
        iterations++;

        if (iterations > numFleets * 101) {
            PANIC("Unable to select enough fleets\n");
        }
    }

    NOT_REACHED();
}

static bool MainIsFleetDefective(BattlePlayer *player,
                                 float defectiveLevel)
{
    MBRegistry *fleetReg = player->mreg;
    uint numBattles;

    if (defectiveLevel == 0.0f) {
        return FALSE;
    }

    if (player->playerType != PLAYER_TYPE_TARGET) {
        /*
         * Keep anything that's not targeted.
         */
        return FALSE;
    }

    numBattles = MBRegistry_GetUint(fleetReg, "abattle.numBattles");
    if (numBattles == 0) {
        /*
         * Keep brand-new fleets.
         */
        return FALSE;
    }

    uint numWins = MBRegistry_GetUint(fleetReg, "abattle.numWins");
    float winRatio = (float)numWins / (float)numBattles;

    if (winRatio < 0.0f) {
        /*
         * Something funny happened, probably someone manually editing
         * the population file wrong.
         */
        return FALSE;
    }

    if (winRatio < defectiveLevel) {
        return TRUE;
    }

    return FALSE;
}

static void MainKillFleet(BattlePlayer *mainPlayers,
                          uint32 mpSize, uint32 *mpIndex,
                          uint32 startingMPIndex, uint32 *numFleets,
                          uint32 *numTargetFleets,
                          uint32 fi)
{
    uint lastI = startingMPIndex + *numFleets - 1;
    VERIFY(mainPlayers[fi].playerType == PLAYER_TYPE_TARGET);
    mainPlayers[fi] = mainPlayers[lastI];
    MBUtil_Zero(&mainPlayers[lastI], sizeof(mainPlayers[lastI]));
    mainPlayers[lastI].playerType = PLAYER_TYPE_INVALID;

    ASSERT(*numFleets > 0);
    (*numFleets)--;

    if (numTargetFleets != NULL) {
        ASSERT(*numTargetFleets > 0);
        (*numTargetFleets)--;
    }

    if (mpIndex != NULL) {
        ASSERT(*mpIndex > 0);
        (*mpIndex)--;
    }
}


static void MainMutateFleet(BattlePlayer *mainPlayers, uint32 mpSize,
                            BattlePlayer *dest,
                            uint32 mi, uint32 bi)
{
    BattlePlayer *src = &mainPlayers[mi];
    BattlePlayer *breeder = &mainPlayers[bi];

    ASSERT(mi < mpSize);

    ASSERT(dest->playerType == PLAYER_TYPE_INVALID);
    ASSERT(src->playerType == PLAYER_TYPE_TARGET);

    MBRegistry_Free(dest->mreg);
    *dest =*src;
    dest->mreg = MBRegistry_AllocCopy(src->mreg);

    dest->playerType = PLAYER_TYPE_TARGET;

    /*
     * Occaisonally, randomly mix traits with another fleet.
     * This increases the odds that two beneficial traits will end up together
     * without having to independently evolve twice.
     */
    if (Random_Flip(0.1f) &&
        src != breeder &&
        breeder->aiType == src->aiType &&
        breeder->mreg != NULL) {
        uint size = MBRegistry_NumEntries(breeder->mreg);
        for (uint i = 0; i < size; i++) {
            if (Random_Bit()) {
                const char *key = MBRegistry_GetKeyAt(breeder->mreg, i);
                const char *value = MBRegistry_GetValueAt(breeder->mreg, i);
                MBRegistry_PutCopy(dest->mreg, key, value);
            }
        }

        MainDumpAddToKey(breeder->mreg, breeder->mreg, NULL, "abattle.numSpawn", 1);
    }

    MainDumpAddToKey(src->mreg, src->mreg, NULL, "abattle.numSpawn", 1);

    Fleet_Mutate(src->aiType, dest->mreg);

    MBRegistry_Remove(dest->mreg, "abattle.numBattles");
    MBRegistry_Remove(dest->mreg, "abattle.numWins");
    MBRegistry_Remove(dest->mreg, "abattle.numLosses");
    MBRegistry_Remove(dest->mreg, "abattle.numDraws");
    MBRegistry_Remove(dest->mreg, "abattle.numSpawn");
    MBRegistry_PutConst(dest->mreg, "abattle.age", "0");
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

    Warning("Starting Battle %d of %d...\n", tData->battleId,
            mainData.totalBattles);
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

    Warning("Battle %d of %d %s!\n", tData->battleId, mainData.totalBattles,
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

static void MainProcessSingleResult(MainEngineResultUnit *ru)
{
    for (uint p = 0; p < ru->bs.numPlayers; p++) {
        PlayerUID puid = ru->bs.players[p].playerUID;
        ASSERT(puid < ARRAYSIZE(mainData.winners));
        MainRecordWinner(&mainData.winners[puid], puid, &ru->bs);
    }
    if (ru->bs.numPlayers == 3) {
        PlayerUID puid1 = ru->bs.players[1].playerUID;
        PlayerUID puid2 = ru->bs.players[2].playerUID;
        ASSERT(ru->bs.players[0].playerUID == PLAYER_ID_NEUTRAL);
        MainRecordWinner(&mainData.winnerBreakdown[puid1][puid2],
                         puid1, &ru->bs);
        MainRecordWinner(&mainData.winnerBreakdown[puid2][puid1],
                         puid2, &ru->bs);
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
        { "width",  "1600",            },
        { "height", "1200",            },
        { "startingCredits", "400",    },
        { "creditsPerTick", "1",       },
        { "tickLimit", "50000",        },
        { "powerCoreDropRate", "0.25", },
        { "powerCoreSpawnRate", "2.0", },
        { "minPowerCoreSpawn", "10",   },
        { "maxPowerCoreSpawn", "20",   },
        { "restrictedStart", "TRUE",   },
        { "startingBases",    "1",     },
        { "startingFighters", "0",     },
        { "baseVictory", "FALSE",      },
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
            MBRegistry_PutConst(mreg, defaultValues[i].key,
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

void MainSanitizeFleetCmd()
{
    /*
     * This is a hack-job, but it works well enough for now.
     */
    uint fleetNum = MBOpt_GetUint("dumpFleet");
    FleetAI ai;
    MBRegistry *popReg = MBRegistry_Alloc();
    MBRegistry *fleetReg = MBRegistry_Alloc();
    MBRegistry *cleanReg = MBRegistry_Alloc();
    MBString tmp;

    VERIFY(popReg != NULL);
    VERIFY(fleetReg != NULL);

    MBRegistry_Load(popReg, MBOpt_GetCStr("usePopulation"));

    MBString_Create(&tmp);

    MBString_IntToString(&tmp, fleetNum);
    MBString_PrependCStr(&tmp, "fleet");
    MBString_AppendCStr(&tmp, ".");

    MBRegistry_SplitOnPrefix(fleetReg, popReg, MBString_GetCStr(&tmp),
                             FALSE);

    const char *fleetStr = MBRegistry_GetCStr(fleetReg, "abattle.fleetName");
    FleetAIType aiType = Fleet_GetTypeFromName(fleetStr);

    BattleParams bp;
    MBUtil_Zero(&bp, sizeof(bp));

    BattlePlayer player;
    MBUtil_Zero(&player, sizeof(player));
    player.mreg = fleetReg;
    Fleet_CreateAI(&ai, aiType, 0, &bp, &player, 0x0);

    if (ai.ops.dumpSanitizedParams == NULL) {
        PANIC("Unsupported fleet: %s\n", fleetStr);
    }

    ai.ops.dumpSanitizedParams(ai.aiHandle, cleanReg);

    MBRegistry_SaveToConsole(cleanReg);

    MBRegistry_Free(popReg);
    MBRegistry_Free(fleetReg);
    MBRegistry_Free(cleanReg);
    MBString_Destroy(&tmp);
}

static void MainThreadsInit(void)
{
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

    mainData.threadsInitialized = TRUE;
}

static void MainThreadsRequestExit(void)
{
    ASSERT(mainData.threadsInitialized);
    for (uint i = 0; i < mainData.numThreads; i++) {
        MainEngineWorkUnit wu;
        MBUtil_Zero(&wu, sizeof(wu));
        wu.type = MAIN_WORK_EXIT;
        WorkQueue_QueueItem(&mainData.workQ, &wu, sizeof(wu));
    }

    mainData.threadsRequestExit = TRUE;
}

static void MainThreadsExit(void)
{
    ASSERT(mainData.threadsInitialized);
    ASSERT(mainData.threadsRequestExit);

    for (uint i = 0; i < mainData.numThreads; i++) {
        SDL_WaitThread(mainData.tData[i].sdlThread, NULL);
    }
    ASSERT(WorkQueue_IsEmpty(&mainData.workQ));

    WorkQueue_Destroy(&mainData.workQ);
    WorkQueue_Destroy(&mainData.resultQ);
    free(mainData.tData);
    mainData.tData = NULL;

    mainData.threadsInitialized = FALSE;
}

static void MainCleanupSinglePlayer(uint p)
{
    ASSERT(p < mainData.numPlayers);
    ASSERT(mainData.players[p].playerType != PLAYER_TYPE_INVALID);

    if (mainData.players[p].mreg != NULL) {
        MBRegistry_Free(mainData.players[p].mreg);
        mainData.players[p].mreg = NULL;
    }
    mainData.players[p].playerType = PLAYER_TYPE_INVALID;
}

static void MainCleanupPlayers(void)
{
    for (uint p = 0; p < mainData.numPlayers; p++) {
        MainCleanupSinglePlayer(p);
    }
}

static void MainMutateCmd(void)
{
    BattlePlayer mutants[MAX_PLAYERS];
    const char *outputFile = MBOpt_GetCStr("outputFile");
    const char *inputFile = MBOpt_GetCStr("usePopulation");

    if (outputFile == NULL) {
        PANIC("--outputFile required for mutate\n");
    }
    if (inputFile == NULL) {
        PANIC("--usePopulation required for mutate\n");
    }

    ASSERT(mainData.numPlayers == 0);
    MainUsePopulation(MBOpt_GetCStr("usePopulation"), FALSE);

    VERIFY(mainData.numPlayers > 0);

    uint targetMutateCount = MBOpt_GetUint("mutationCount");
    uint actualMutateCount = 0;

    VERIFY(targetMutateCount <= ARRAYSIZE(mutants));
    MBUtil_Zero(mutants, sizeof(mutants));

    while (targetMutateCount > 0) {
        uint32 mi = MainFleetCompetition(&mainData.players[0],
                                         mainData.numPlayers,
                                         1, mainData.numPlayers - 1, TRUE);
        uint32 bi = MainFleetCompetition(&mainData.players[0],
                                         mainData.numPlayers,
                                         1, mainData.numPlayers - 1, TRUE);
        MainMutateFleet(&mainData.players[0], mainData.numPlayers,
                        &mutants[actualMutateCount], mi, bi);
        targetMutateCount--;
        actualMutateCount++;
    }

    // Dump the original population (with updated numSpawns)
    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    MainDumpPopulation(inputFile, FALSE);
    MainCleanupPlayers();

    // Dump the new mutants to the outputFile
    mainData.numPlayers = 0;
    mainData.players[0].aiType = FLEET_AI_NEUTRAL;
    mainData.players[0].playerType = PLAYER_TYPE_NEUTRAL;
    mainData.numPlayers++;

    for (uint i = 0; i < actualMutateCount; i++) {
        mainData.players[mainData.numPlayers] = mutants[i];
        mainData.numPlayers++;
    }

    MainDumpPopulation(outputFile, FALSE);
    MainCleanupPlayers();
}

void MainDefaultCmd(void)
{
    MainBattleType bt = MAIN_BT_SINGLE;

    MainConstructScenarios(TRUE, bt);
    MainRunScenarios();

    if (MBOpt_IsPresent("dumpPopulation")) {
        MainDumpPopulation(MBOpt_GetCStr("dumpPopulation"), FALSE);
    }

    MainCleanupPlayers();
}

static void MainKillCmd(void)
{
    const char *file = MBOpt_GetCStr("usePopulation");

    if (file == NULL) {
        PANIC("--usePopulation required for kill\n");
    }

    ASSERT(mainData.numPlayers == 0);
    MainUsePopulation(MBOpt_GetCStr("usePopulation"), FALSE);
    VERIFY(mainData.numPlayers > 0);

    // Account for FLEET_AI_NEUTRAL
    uint numFleets = mainData.numPlayers - 1;

    uint actualKillCount = 0;
    uint minPop = 0;
    uint maxPop = numFleets;
    if (MBOpt_IsPresent("minPop")) {
        minPop = MBOpt_GetUint("minPop");
    }
    if (MBOpt_IsPresent("maxPop")) {
        maxPop = MBOpt_GetUint("maxPop");
    }
    if (numFleets <= minPop) {
        Warning("Population is already too low pop=%d, minPop=%d\n",
                numFleets, minPop);
        Warning("Not killing anything.\n");
        MainCleanupPlayers();
        return;
    }

    /*
     * Kill defective fleets.
     */
    float defectiveLevel = MBOpt_GetFloat("defectiveLevel");
    uint i = 1;
    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    while (numFleets > minPop && i < mainData.numPlayers) {
        ASSERT(numFleets > 0);
        ASSERT(numFleets < mainData.numPlayers);
        ASSERT(mainData.numPlayers > 1);
        ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);

        if (MainIsFleetDefective(&mainData.players[i], defectiveLevel)) {
            MainKillFleet(&mainData.players[0], mainData.numPlayers,
                          NULL, 1, &numFleets, NULL, i);
            /*
             * Re-use the same i index because we swapped the fleet
             * out.
             */
            ASSERT(i > 0);
            i--;

            ASSERT(numFleets >= 0);
            ASSERT(mainData.numPlayers > 0);
            mainData.numPlayers--;
            ASSERT(mainData.numPlayers > 0);
            ASSERT(numFleets < mainData.numPlayers);

            actualKillCount++;
        }

        i++;
    }

    if (actualKillCount > 0) {
        Warning("Killed %d defective fleets.\n", actualKillCount);
    }

    uint targetKillCount = 0;

    if (MBOpt_IsPresent("killRatio")) {
        targetKillCount = numFleets * MBOpt_GetFloat("killRatio");
    }

    if (minPop > 0) {
        targetKillCount = MIN(numFleets - minPop, targetKillCount);
    }
    if (numFleets > maxPop) {
        targetKillCount = MAX(numFleets - maxPop, targetKillCount);
    }

    ASSERT(targetKillCount <= numFleets);

    while (targetKillCount > 0) {
        ASSERT(numFleets > 0);
        ASSERT(numFleets < mainData.numPlayers);
        ASSERT(mainData.numPlayers > 1);
        ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);

        uint32 ki = MainFleetCompetition(&mainData.players[0],
                                         mainData.numPlayers,
                                         1, numFleets, FALSE);
        MainKillFleet(&mainData.players[0], mainData.numPlayers,
                      NULL, 1, &numFleets, NULL, ki);
        targetKillCount--;
        actualKillCount++;
        ASSERT(numFleets >= 0);
        ASSERT(mainData.numPlayers > 0);
        mainData.numPlayers--;
        ASSERT(mainData.numPlayers > 0);
        ASSERT(numFleets < mainData.numPlayers);
    }

    ASSERT(numFleets >= minPop);
    ASSERT(numFleets <= maxPop);

    Warning("Killed %d total fleets.\n", actualKillCount);
    Warning("%d fleets remaining.\n", numFleets);

    // Dump the original population (with updated numSpawns)
    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    if (actualKillCount > 0 || MBOpt_IsPresent("resetAfter")) {
        if (MBOpt_IsPresent("resetAfter")) {
            MainResetFleetStats();
        }
        MainDumpPopulation(file, FALSE);
    }

    MainCleanupPlayers();
}

static void MainMeasureCmd(void)
{
    const char *file = MBOpt_GetCStr("usePopulation");
    if (file == NULL) {
        PANIC("--usePopulation required for measure\n");
    }

    const char *controlFile = MBOpt_GetCStr("controlPopulation");
    if (controlFile == NULL) {
        PANIC("--controlPopulation required for measure\n");
    }

    ASSERT(mainData.numPlayers == 0);
    MainUsePopulation(controlFile, FALSE);
    VERIFY(mainData.numPlayers > 0);

    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    for (uint i = 1; i < mainData.numPlayers; i++) {
        VERIFY(mainData.players[i].playerType == PLAYER_TYPE_CONTROL);
    }
    uint lastControl = mainData.numPlayers - 1;

    MainUsePopulation(file, TRUE);
    VERIFY(mainData.numPlayers > 0);
    for (uint i = lastControl + 1; i < mainData.numPlayers; i++) {
        VERIFY(mainData.players[i].playerType == PLAYER_TYPE_TARGET);
    }

    MainConstructScenarios(FALSE, MAIN_BT_OPTIMIZE);
    MainRunScenarios();

    MainDumpPopulation(file, TRUE);

    MainCleanupPlayers();
}

static void MainOptimizeCmd(void)
{
    const char *file = MBOpt_GetCStr("usePopulation");
    const char *controlFile = MBOpt_GetCStr("controlPopulation");

    ASSERT(mainData.numPlayers == 0);
    if (controlFile == NULL) {
        MainLoadAllAsControlPlayers();
    } else {
        MainUsePopulation(controlFile, FALSE);
    }

    VERIFY(mainData.numPlayers > 0);
    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    for (uint i = 1; i < mainData.numPlayers; i++) {
        VERIFY(mainData.players[i].playerType == PLAYER_TYPE_CONTROL);
    }
    uint lastControl = mainData.numPlayers - 1;

    if (file == NULL) {
        MainAddTargetPlayersForOptimize();
    } else {
        MainUsePopulation(file, TRUE);
    }

    VERIFY(mainData.numPlayers > 1);
    for (uint i = lastControl + 1; i < mainData.numPlayers; i++) {
        VERIFY(mainData.players[i].playerType == PLAYER_TYPE_TARGET);
    }

    MainConstructScenarios(FALSE, MAIN_BT_OPTIMIZE);
    MainRunScenarios();
    MainCleanupPlayers();
}

static void MainTournamentCmd(void)
{
    const char *file = MBOpt_GetCStr("usePopulation");

    mainData.printWinnerBreakdown = TRUE;

    ASSERT(mainData.numPlayers == 0);
    MainAddNeutralPlayer();

    if (file == NULL) {
        MainLoadAllAsControlPlayers();
    } else {
        MainUsePopulation(file, TRUE);
    }

    MainConstructScenarios(FALSE, MAIN_BT_TOURNAMENT);
    MainRunScenarios();
    MainCleanupPlayers();
}

static void MainMergeCmd(void)
{
    const char *file = MBOpt_GetCStr("usePopulation");
    if (file == NULL) {
        PANIC("--usePopulation required for merge\n");
    }

    const char *inputFile = MBOpt_GetCStr("inputPopulation");
    if (inputFile == NULL) {
        PANIC("--inputPopulation required for merge\n");
    }

    ASSERT(mainData.numPlayers == 0);
    mainData.players[0].aiType = FLEET_AI_NEUTRAL;
    mainData.players[0].playerType = PLAYER_TYPE_NEUTRAL;
    mainData.numPlayers++;

    MainUsePopulation(file, FALSE);
    VERIFY(mainData.numPlayers > 0);

    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);

    MainUsePopulation(inputFile, FALSE);
    VERIFY(mainData.numPlayers > 0);

    MainDumpPopulation(file, FALSE);
    MainCleanupPlayers();
}

static void MainResetFleetStats(void)
{
    ASSERT(mainData.players[0].aiType == FLEET_AI_NEUTRAL);
    for (uint i = 1; i < mainData.numPlayers; i++) {
        MBRegistry *mreg = mainData.players[i].mreg;
        if (mreg != NULL) {
            MBRegistry_Remove(mreg, "abattle.numBattles");
            MBRegistry_Remove(mreg, "abattle.numWins");
            MBRegistry_Remove(mreg, "abattle.numLosses");
            MBRegistry_Remove(mreg, "abattle.numDraws");
        }
    }
}

static void MainResetCmd(void)
{
    const char *file = MBOpt_GetCStr("usePopulation");
    if (file == NULL) {
        PANIC("--usePopulation required for reset\n");
    }

    ASSERT(mainData.numPlayers == 0);
    mainData.players[0].aiType = FLEET_AI_NEUTRAL;
    mainData.players[0].playerType = PLAYER_TYPE_NEUTRAL;
    mainData.numPlayers++;

    MainUsePopulation(file, FALSE);
    VERIFY(mainData.numPlayers > 0);

    MainResetFleetStats();

    MainDumpPopulation(file, FALSE);
    MainCleanupPlayers();
}

void MainUnitTests()
{
    if (mb_devel) {
        MBUnitTest_RunTests();
        Warning("Starting sr2 Unit Tests ...\n");
        MobPSet_UnitTest();
        Geometry_UnitTest();
        ML_UnitTest();
    } else {
        Warning("Unit tests disabled on non-devel build.\n");
    }
}

void MainParseCmdLine(int argc, char **argv)
{
    MBOption opts[] = {
        //{ "-h", "--help",              FALSE, "Print the help text"           },
        //{ "-v", "--version",           FALSE, "Print the version information" },
        { "-H", "--headless",          FALSE, "Run headless"                  },
        { "-l", "--loop",              TRUE,  "Loop <arg> times"              },
        { "-S", "--scenario",          TRUE,  "Scenario type"                 },
        { "-D", "--dumpPopulation",    TRUE,  "Dump Population to file"       },
        { "-U", "--usePopulation",     TRUE,  "Use Population from file"      },
        { "-s", "--seed",              TRUE,  "Set random seed"               },
        { "-L", "--tickLimit",         TRUE,  "Time limit in ticks"           },
        { "-t", "--numThreads",        TRUE,  "Number of engine threads"      },
        { "-R", "--reuseSeed",         FALSE, "Reuse the seed across battles" },
    };

    MBOption display_opts[] = {
        { NULL, "--frameSkip",         FALSE, "Allow frame skipping"          },
        { "-F", "--targetFPS",         TRUE,  "Target FPS for window"         },
        { "-P", "--startPaused",       FALSE, "Start paused"                  },
    };
    MBOption dumpPNG_opts[] = {
        { "-o", "--outputFile",        TRUE, "Output file for PNG"            },
    };
    MBOption sanitizeFleet_opts[] = {
        { "-f", "--dumpFleet",         TRUE,  "Fleet number to dump"          },
    };
    MBOption mutate_opts[] = {
        { "-o", "--outputFile",        TRUE,  "Output population file"        },
        { "-c", "--mutationCount",     TRUE,  "Number of mutations"           },
    };
    MBOption kill_opts[] = {
        { NULL, "--killRatio",         TRUE,  "Kill ratio for population"     },
        { NULL, "--minPop",            TRUE,  "Minimum population"            },
        { NULL, "--maxPop",            TRUE,  "Maximum population"            },
        { NULL, "--defectiveLevel",    TRUE,  "Defective win ratio"           },
        { NULL, "--resetAfter",        FALSE, "Reset after killing"           },
    };
    MBOption measure_opts[] = {
        { "-C", "--controlPopulation", TRUE,  "Population file for control fleets" },
    };
    MBOption optimize_opts[] = {
        { "-C", "--controlPopulation", TRUE,  "Population file for control fleets" },
    };
    MBOption merge_opts[] = {
        { "-i", "--inputPopulation",   TRUE,  "Input file for extra population" },
    };

    MBOpt_SetProgram("sr2", NULL);
    MBOpt_LoadOptions(NULL, opts, ARRAYSIZE(opts));
    MBOpt_LoadOptions("unitTests", NULL, 0);
    MBOpt_LoadOptions("display", display_opts, ARRAYSIZE(display_opts));
    MBOpt_LoadOptions("dumpPNG", dumpPNG_opts, ARRAYSIZE(dumpPNG_opts));
    MBOpt_LoadOptions("sanitizeFleet",
                      sanitizeFleet_opts, ARRAYSIZE(sanitizeFleet_opts));
    MBOpt_LoadOptions("mutate", mutate_opts, ARRAYSIZE(mutate_opts));
    MBOpt_LoadOptions("kill", kill_opts, ARRAYSIZE(kill_opts));
    MBOpt_LoadOptions("measure", measure_opts, ARRAYSIZE(measure_opts));
    MBOpt_LoadOptions("optimize", optimize_opts, ARRAYSIZE(optimize_opts));
    MBOpt_LoadOptions("reset", NULL, 0);
    MBOpt_LoadOptions("merge", merge_opts, ARRAYSIZE(merge_opts));
    MBOpt_LoadOptions("tournament", NULL, 0);
    MBOpt_LoadOptions("run", NULL, 0);
    MBOpt_Init(argc, argv);

    const char *cmd = MBOpt_GetCmd();
    mainData.headless = TRUE;
    mainData.frameSkip = FALSE;
    mainData.startPaused = FALSE;
    mainData.targetFPS = 101;
    if (!sr2_gui) {
        // Use default headless configuration.
    } else if (cmd == NULL) {
        mainData.headless = MBOpt_GetBool("headless");
    } else if (strcmp(cmd, "display") == 0) {
        mainData.headless = FALSE;
        mainData.frameSkip = MBOpt_GetBool("frameSkip");
        if (MBOpt_IsPresent("targetFPS")) {
            mainData.targetFPS = MBOpt_GetUint("targetFPS");
        }
        mainData.startPaused = MBOpt_IsPresent("startPaused");
    }

    if (MBOpt_IsPresent("loop")) {
        mainData.loop = MBOpt_GetInt("loop");
    } else {
        mainData.loop = 1;
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
    const char *cmd;
    ASSERT(MBUtil_IsZero(&mainData, sizeof(mainData)));

    MBStrTable_Init();
    MainParseCmdLine(argc, argv);

    // Setup
    Random_Init();
    RandomState_Create(&mainData.rs);
    if (mainData.seed != 0) {
        RandomState_SetSeed(&mainData.rs, mainData.seed);
    }

    SDL_Init(mainData.headless ? 0 : SDL_INIT_VIDEO);

    Warning("Starting SpaceRobots2 %s...\n", mb_debug ? "(debug enabled)" : "");
    Warning("\n");

    DebugPrint("Random seed: 0x%llX\n",
               RandomState_GetSeed(&mainData.rs));

    cmd = MBOpt_GetCmd();
    if (cmd == NULL) {
        cmd = "default";
    }

    if (strcmp(cmd, "unitTests") == 0) {
        MainUnitTests();
    } else if (strcmp(cmd, "dumpPNG") == 0) {
        Display_DumpPNG(MBOpt_GetCStr("outputFile"));
    } else if (strcmp(cmd, "sanitizeFleet") == 0) {
        MainSanitizeFleetCmd();
    } else if (strcmp(cmd, "mutate") == 0) {
        MainMutateCmd();
    } else if (strcmp(cmd, "kill") == 0) {
        MainKillCmd();
    } else if (strcmp(cmd, "measure") == 0) {
        MainMeasureCmd();
    } else if (strcmp(cmd, "reset") == 0) {
        MainResetCmd();
    } else if (strcmp(cmd, "merge") == 0) {
        MainMergeCmd();
    } else if (strcmp(cmd, "optimize") == 0) {
        MainOptimizeCmd();
    } else if (strcmp(cmd, "tournament") == 0) {
        MainTournamentCmd();
    } else if (strcmp(cmd, "display") == 0 ||
               strcmp(cmd, "run") == 0 ||
               strcmp(cmd, "default") == 0) {
        MainDefaultCmd();
    } else {
        PANIC("Unknown command: %s\n", cmd);
    }

    RandomState_Destroy(&mainData.rs);

    SDL_Quit();
    MBOpt_Exit();

    Warning("Done!\n");
    return 0;
}
