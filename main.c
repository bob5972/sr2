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
    SDL_Thread *engineThread;
    uint32 startTimeMS;
} mainData;

void MainPrintBattleStatus(const BattleStatus *bStatus)
{
    uint32 stopTimeMS = SDL_GetTicks();
    uint32 elapsedMS = stopTimeMS - mainData.startTimeMS;

    Warning("Finished tick %d\n", bStatus->tick);
    Warning("\tcollisions = %d\n", bStatus->collisions);
    Warning("\tsensor contacts = %d\n", bStatus->sensorContacts);
    Warning("\tspawns = %d\n", bStatus->spawns);
    Warning("\t%d ticks in %d ms\n", bStatus->tick, elapsedMS);
    Warning("\t%.1f ticks/second\n", ((float)bStatus->tick)/elapsedMS * 1000.0f);
    Warning("\n");

    if (bStatus->finished) {
        ASSERT(bStatus->winner < ARRAYSIZE(bStatus->players));
        Warning("Winner: %s\n", bStatus->players[bStatus->winner].playerName);
    }
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
        bStatus = Battle_AcquireStatus();
        Fleet_RunTick(bStatus, bMobs, numMobs);
        Battle_ReleaseMobs();
        Battle_ReleaseStatus();

        // Run the Physics
        Battle_RunTick();

        // Draw
        if (!MBOpt_IsPresent("headless")) {
            bMobs = Battle_AcquireMobs(&numMobs);
            dMobs = Display_AcquireMobs(numMobs);
            memcpy(dMobs, bMobs, numMobs * sizeof(bMobs[0]));
            Display_ReleaseMobs();
            Battle_ReleaseMobs();
        }

        bStatus = Battle_AcquireStatus();
        if (!MBOpt_IsPresent("headless") &&
            bStatus->tick % 1000 == 0) {
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

void MainParseCmdLine(int argc, char **argv)
{
    MBOption opts[] = {
        { "-h", "--help",     "Print help text" },
        { "-H", "--headless", "Run headless",   },
    };

    MBOpt_Init(opts, ARRAYSIZE(opts), argc, argv);

    if (MBOpt_IsPresent("help")) {
        MBOpt_PrintHelpText();
        exit(1);
    }
}

int main(int argc, char **argv)
{
    uint32 p;
    BattleParams bp;

    ASSERT(MBUtil_IsZero(&mainData, sizeof(mainData)));
    MainParseCmdLine(argc, argv);

    // Setup
    Warning("Starting SpaceRobots2 ...\n");
    Warning("\n");
    SDL_Init(MBOpt_IsPresent("headless") ? 0 : SDL_INIT_VIDEO);
    Random_Init();

    MBUtil_Zero(&bp, sizeof(bp));
    bp.width = 1600;
    bp.height = 1200;
    bp.startingCredits = 1000;
    bp.creditsPerTick = 1;
    bp.timeLimit = 1000 * 1000;
    bp.lootDropRate = 0.5f;
    bp.lootSpawnRate = 0.5f;
    bp.minLootSpawn = 5;
    bp.maxLootSpawn = 10;

    p = 0;
    bp.players[p].playerName = "Neutral";
    bp.players[p].aiType = FLEET_AI_NEUTRAL;
    p++;
    bp.players[p].playerName = "SimpleFleet";
    bp.players[p].aiType = FLEET_AI_SIMPLE;
    p++;
    bp.players[p].playerName = "BobFleet";
    bp.players[p].aiType = FLEET_AI_BOB;
    p++;
    //bp.players[p].playerName = "Player 3";
    //bp.players[p].aiType = FLEET_AI_DUMMY;
    //p++;
    bp.numPlayers = p;

    Battle_Init(&bp);
    Fleet_Init();

    if (!MBOpt_IsPresent("headless")) {
        Display_Init();
    }

    // Launch Engine Thread
    mainData.engineThread = SDL_CreateThread(Main_EngineThreadMain, "battle",
                                             NULL);
    ASSERT(mainData.engineThread != NULL);

    if (!MBOpt_IsPresent("headless")) {
        Display_Main();
    }

    SDL_WaitThread(mainData.engineThread, NULL);

    //Cleanup
    if (!MBOpt_IsPresent("headless")) {
        Display_Exit();
    }

    Fleet_Exit();
    Battle_Exit();
    Random_Exit();
    SDL_Quit();
    MBOpt_Exit();

    Warning("Done!\n");
    return 0;
}
