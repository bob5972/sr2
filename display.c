/*
 * display.c -- part of SpaceRobots2
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
#include <time.h>
#include <png.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "MBConfig.h"
#include "MBBasic.h"
#include "MBTypes.h"
#include "MBAssert.h"
#include "Random.h"
#include "display.h"
#include "MBVector.h"
#include "sprite.h"

// Poor man's command-line options...
#define DRAW_SENSORS TRUE

typedef struct FleetSprites {
    uint32 color;
    Sprite *mobSprites[MOB_TYPE_MAX];
    Sprite *scanSprites[MOB_TYPE_MAX];
} FleetSprites;

typedef struct DisplayGlobalData {
    bool initialized;
    uint32 width;
    uint32 height;

    uint targetFPS;

    SDL_Window *sdlWindow;
    SDL_Renderer *sdlRenderer;
    bool paused;
    bool oneTick;
    bool inMain;
    uint64 mobGenerationDrawn;

    TTF_Font *font;
    SDL_Surface *textSurface;
    SDL_Texture *textTexture;

    SDL_mutex *mobMutex;
    bool mainWaiting;
    SDL_sem *mainSignal;
    uint64 mobGeneration;
    bool mobsAcquired;
    MobVector mobs;

    FleetSprites fleets[32];
} DisplayGlobalData;

static DisplayGlobalData display;

void DisplayInitText(const BattleScenario *bsc);
void DisplayExitText();

void Display_Init(const BattleScenario *bsc)
{
    const BattleParams *bp = &bsc->bp;
    ASSERT(MBUtil_IsZero(&display, sizeof(display)));

    display.targetFPS = 101;

    display.width = bp->width;
    display.height = bp->height;

    display.mobGenerationDrawn = 0;
    display.mobGeneration = 1;

    display.mobMutex = SDL_CreateMutex();
    ASSERT(display.mobMutex != NULL);

    display.mainSignal = SDL_CreateSemaphore(0);
    ASSERT(display.mainSignal != NULL);

    display.sdlWindow =
        SDL_CreateWindow("SpaceRobots2",
                         SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         display.width, display.height,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (display.sdlWindow == NULL) {
        PANIC("Failed to create SDL window\n");
    }

    display.sdlRenderer = SDL_CreateRenderer(display.sdlWindow, -1, 0);
    if (display.sdlRenderer == NULL) {
        PANIC("Failed to create SDL renderer\n");
    }

    Sprite_Init();
    DisplayInitText(bsc);

    SDL_SetRenderDrawColor(display.sdlRenderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(display.sdlRenderer);
    SDL_RenderPresent(display.sdlRenderer);

    ASSERT(bp->numPlayers <= ARRAYSIZE(display.fleets));
    uint repeatCount[FLEET_AI_MAX];
    MBUtil_Zero(&repeatCount[0], sizeof(repeatCount));

    for (uint x = 0; x < bp->numPlayers; x++) {
        FleetAIType aiType = bsc->players[x].aiType;
        uint32 color = Sprite_GetColor(aiType, ++repeatCount[aiType]);
        display.fleets[x].color = color;

        for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
            display.fleets[x].mobSprites[t] =
                Sprite_CreateMob(t, aiType, repeatCount[aiType]);
            Sprite_PrepareTexture(display.fleets[x].mobSprites[t],
                                    display.sdlRenderer);

            uint32 radius = (uint32)MobType_GetSensorRadius(t);
            display.fleets[x].scanSprites[t] =
                Sprite_CreateCircle(radius, color / 2);
            Sprite_PrepareTexture(display.fleets[x].scanSprites[t],
                                  display.sdlRenderer);
        }
    }

    MobVector_CreateEmpty(&display.mobs);

    display.initialized = TRUE;
}

void Display_Exit()
{
    ASSERT(display.initialized);

    MobVector_Destroy(&display.mobs);

    for (uint x = 0; x < ARRAYSIZE(display.fleets); x++) {
        for (uint i = 0; i < ARRAYSIZE(display.fleets[x].mobSprites); i++) {
            Sprite_Free(display.fleets[x].mobSprites[i]);
            display.fleets[x].mobSprites[i] = NULL;
        }
        for (uint i = 0; i < ARRAYSIZE(display.fleets[x].scanSprites); i++) {
            Sprite_Free(display.fleets[x].scanSprites[i]);
            display.fleets[x].scanSprites[i] = NULL;
        }
    }

    SDL_DestroyWindow(display.sdlWindow);
    display.sdlWindow = NULL;

    DisplayExitText();
    Sprite_Exit();

    SDL_DestroyMutex(display.mobMutex);
    display.mobMutex = NULL;
    SDL_DestroySemaphore(display.mainSignal);
    display.mainSignal = NULL;

    display.initialized = FALSE;
}

void Display_SetFPS(uint fps)
{
    display.targetFPS = fps;
}


void DisplayInitText(const BattleScenario *bsc)
{
    char *displayText = NULL;
    SDL_Color textColor = { 0xFF, 0xFF, 0xFF, 0xFF };

    TTF_Init();

    display.font = TTF_OpenFont("/usr/share/fonts/corefonts/arial.ttf", 20);
    if (display.font == NULL) {
        display.font = TTF_OpenFont("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf", 20);
    }
    VERIFY(display.font != NULL);

    if (bsc->bp.numPlayers == 3) {
        ASSERT(bsc->players[0].aiType == FLEET_AI_NEUTRAL);

        asprintf(&displayText, "%s vs %s",
                 bsc->players[1].playerName,
                 bsc->players[2].playerName);
    } else {
        displayText = strdup("Battle Royale");
    }

    display.textSurface = TTF_RenderText_Solid(display.font, displayText, textColor);
    display.textTexture = SDL_CreateTextureFromSurface(display.sdlRenderer,
                                                       display.textSurface);

    free(displayText);
}

void DisplayExitText()
{
    SDL_DestroyTexture(display.textTexture);
    SDL_FreeSurface(display.textSurface);
    display.textSurface = NULL;

    TTF_CloseFont(display.font);
    display.font = NULL;

    TTF_Quit();
}

void Display_DumpPNG(const char *fileName)
{
    uint32 color = 0xFFFF0000; // ARGB
    SDL_Surface *sdlSurface;

    sdlSurface = Sprite_CreateMobSheet(color);
    Sprite_SavePNG(fileName, sdlSurface);

    SDL_FreeSurface(sdlSurface);
}

Mob *Display_AcquireMobs(uint32 numMobs, bool frameSkip)
{
    SDL_LockMutex(display.mobMutex);
    ASSERT(!display.mobsAcquired);

    ASSERT(!display.mainWaiting);

    if (frameSkip) {
        if (display.mobGenerationDrawn != display.mobGeneration) {
            SDL_UnlockMutex(display.mobMutex);
            return NULL;
        }
    } else {
        while (display.mobGenerationDrawn != display.mobGeneration) {
            // We haven't drawn the last frame yet.
            display.mainWaiting = TRUE;
            SDL_UnlockMutex(display.mobMutex);
            SDL_SemWait(display.mainSignal);
            SDL_LockMutex(display.mobMutex);

            if (!display.inMain) {
                PANIC("display thread quit\n");
            }
        }
        display.mainWaiting = FALSE;
    }

    MobVector_Resize(&display.mobs, numMobs);
    MobVector_Pin(&display.mobs);
    display.mobsAcquired = TRUE;

    /*
     * We don't unlock the mutex until Display_ReleaseMobs.
     */
    return MobVector_GetCArray(&display.mobs);
}

void Display_ReleaseMobs()
{
    /*
     * We acquired the lock in Display_AcquireMobs.
     */
    ASSERT(display.mobsAcquired);
    MobVector_Unpin(&display.mobs);
    display.mobsAcquired = FALSE;
    display.mobGeneration++;
    SDL_UnlockMutex(display.mobMutex);
}


static void DisplayDrawFrame()
{
    uint32 i;
    SDL_Rect rect;

    ASSERT(display.initialized);

    if (display.oneTick) {
        display.paused = TRUE;
        display.oneTick = FALSE;
    } else if (display.paused) {
        return;
    }

    SDL_LockMutex(display.mobMutex);
    if (display.mobGenerationDrawn == display.mobGeneration) {
        SDL_UnlockMutex(display.mobMutex);
        return;
    }
    display.mobGenerationDrawn = display.mobGeneration;

    SDL_SetRenderDrawColor(display.sdlRenderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(display.sdlRenderer);

    if (DRAW_SENSORS) {
        for (i = 0; i < MobVector_Size(&display.mobs); i++) {
            Mob *mob = MobVector_GetPtr(&display.mobs, i);

            if (!mob->alive) {
                continue;
            }

            FCircle circle;
            FleetSprites *fs;
            Sprite *sprite;
            IPoint p;

            fs = &display.fleets[mob->playerID];
            sprite = fs->scanSprites[mob->type];

            Mob_GetSensorCircle(mob, &circle);
            FCircle_CenterToIPoint(&circle, &p);

            Sprite_BlitCentered(sprite, display.sdlRenderer, p.x, p.y);
        }
    }

    /*
     * Paint mobs back to front.
     */
    for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
        for (i = 0; i < MobVector_Size(&display.mobs); i++) {
            Mob *mob = MobVector_GetPtr(&display.mobs, i);

            if (mob->type != t || !mob->alive) {
                continue;
            }

            FCircle circle;
            FleetSprites *fs;
            Sprite *sprite;
            IPoint p;

            fs = &display.fleets[mob->playerID];
            sprite = fs->mobSprites[mob->type];

            Mob_GetCircle(mob, &circle);
            FCircle_CenterToIPoint(&circle, &p);

            ASSERT(mob->playerID == PLAYER_ID_NEUTRAL ||
                mob->playerID < ARRAYSIZE(display.fleets));
            ASSERT(mob->type < ARRAYSIZE(display.fleets[0].mobSprites));

            ASSERT(sprite != NULL);
            Sprite_BlitCentered(sprite, display.sdlRenderer, p.x, p.y);
        }
    }


    rect.x = 5;
    rect.y = 5;
    rect.w = display.textSurface->w;
    rect.h = display.textSurface->h;
    SDL_RenderCopy(display.sdlRenderer, display.textTexture, NULL, &rect);

    SDL_RenderPresent(display.sdlRenderer);

    if (display.mainWaiting) {
        /*
        * Signal that we're ready for a new frame.
        */
        SDL_SemPost(display.mainSignal);
    }

    SDL_UnlockMutex(display.mobMutex);
}

static uint64 DisplayGetTimerUS(void)
{
    uint32 retVal;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    retVal = ts.tv_sec * 1000 * 1000;
    retVal += ts.tv_nsec / 1000;

    return retVal;
}

void Display_Main(bool startPaused)
{
    SDL_Event event;
    int done = 0;

    uint64 targetUSPerFrame = (1000 * 1000) / display.targetFPS;
    uint64 startTimeUS, endTimeUS;

    ASSERT(display.initialized);
    display.inMain = TRUE;

    // Start paused
    if (startPaused) {
        display.paused = TRUE;
    }

    while (!done) {
        startTimeUS = DisplayGetTimerUS();

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    done = 1;
                    break;
                case SDL_MOUSEBUTTONUP:
                    display.paused = !display.paused;
                    break;
                case SDL_KEYUP:
                    if (event.key.keysym.sym == SDLK_PERIOD) {
                        display.oneTick = TRUE;
                    } else if (event.key.keysym.sym == SDLK_ESCAPE ||
                               event.key.keysym.sym == SDLK_q) {
                        done = 1;
                    } else if (event.key.keysym.sym == SDLK_SPACE) {
                        display.paused = !display.paused;
                    }
                    break;
                default:
                    break;
            }
        }

        DisplayDrawFrame();
        endTimeUS = DisplayGetTimerUS();
        if (endTimeUS - startTimeUS < targetUSPerFrame) {
            usleep(targetUSPerFrame - (endTimeUS - startTimeUS));
        }
    }

    /*
     * Ensure the main thread wakes up if we exited early.
     */
    SDL_SemPost(display.mainSignal);
    display.inMain = FALSE;
}
