/*
 * display.c -- part of SpaceRobots2
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
#include <time.h>
#include <png.h>

#include "config.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include "mbbasic.h"
#include "mbtypes.h"
#include "mbassert.h"
#include "random.h"
#include "display.h"
#include "MBVector.h"
#include "sprite.h"

#define SHIP_ALPHA 0x88

// Poor man's command-line options...
#define DRAW_SENSORS TRUE

typedef struct FleetSprites {
    uint32 color;
    SDL_Surface *mobSpriteSheet;
    Sprite *mobSprites[MOB_TYPE_MAX];
    Sprite *scanSprites[MOB_TYPE_MAX];
} FleetSprites;

typedef struct DisplayGlobalData {
    bool initialized;
    uint32 width;
    uint32 height;

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

    FleetSprites fleets[8];
} DisplayGlobalData;

static DisplayGlobalData display;

void DisplayInitText(const BattleScenario *bsc);
void DisplayExitText();
static uint32 DisplayGetColor(FleetAIType aiType, uint repeatCount);


void Display_Init(const BattleScenario *bsc)
{
    const BattleParams *bp = &bsc->bp;
    ASSERT(MBUtil_IsZero(&display, sizeof(display)));

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

    DisplayInitText(bsc);

    SDL_SetRenderDrawColor(display.sdlRenderer, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(display.sdlRenderer);
    SDL_RenderPresent(display.sdlRenderer);

    ASSERT(bp->numPlayers <= ARRAYSIZE(display.fleets));
    uint repeatCount[FLEET_AI_MAX];
    MBUtil_Zero(&repeatCount[0], sizeof(repeatCount));

    for (uint x = 0; x < bp->numPlayers; x++) {
        FleetAIType aiType = bsc->players[x].aiType;
        uint32 color = DisplayGetColor(aiType, repeatCount[aiType]++);
        display.fleets[x].color = color;

        display.fleets[x].mobSpriteSheet = Sprite_CreateMobSheet(color);

        for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
            display.fleets[x].mobSprites[t] =
                Sprite_CreateFromMobSheet(t, display.fleets[x].mobSpriteSheet);
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
        SDL_FreeSurface(display.fleets[x].mobSpriteSheet);

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

    SDL_DestroyMutex(display.mobMutex);
    display.mobMutex = NULL;
    SDL_DestroySemaphore(display.mainSignal);
    display.mainSignal = NULL;

    display.initialized = FALSE;
}


void DisplayInitText(const BattleScenario *bsc)
{
    char *displayText = NULL;
    SDL_Color textColor = { 0xFF, 0xFF, 0xFF, 0xFF };

    TTF_Init();

    display.font = TTF_OpenFont("/usr/share/fonts/corefonts/arial.ttf", 20);
    VERIFY(display.font != NULL);

    if (bsc->bp.numPlayers == 3) {
        ASSERT(bsc->players[0].aiType == FLEET_AI_NEUTRAL);

        asprintf(&displayText, "%s vs %s",
                 bsc->players[1].playerName,
                 bsc->players[2].playerName);
    } else {
        displayText = "Battle Royale";
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

static uint32 DisplayGetColor(FleetAIType aiType, uint repeatCount)
{
    struct {
        FleetAIType aiType;
        uint32 color;
    } colors[] = {
        { FLEET_AI_INVALID, 0x000000, }, // 0x(AA)RRGGBB
        { FLEET_AI_NEUTRAL, 0x888888, },
        { FLEET_AI_DUMMY,   0xFFFFFF, },
        { FLEET_AI_SIMPLE,  0xFF0000, },
        { FLEET_AI_GATHER,  0x00FF00, },
        { FLEET_AI_CLOUD,   0x0000FF, },
        { FLEET_AI_MAPPER,  0x808000, },
        { FLEET_AI_RUNAWAY, 0x800080, },
        { FLEET_AI_COWARD,  0x008080, },
        { FLEET_AI_BASIC,   0x808080, },
        { FLEET_AI_HOLD,    0xF00080, },
        { FLEET_AI_BOB,     0x80F080, },
    };
    uint32 color;

    ASSERT(ARRAYSIZE(colors) == FLEET_AI_MAX);
    ASSERT(aiType < ARRAYSIZE(colors));
    ASSERT(colors[aiType].aiType == aiType);


    uint i  = aiType % ARRAYSIZE(colors);
    color = colors[i].color;
    color /= (1 + repeatCount);
    return color | ((SHIP_ALPHA & 0xFF) << 24);
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

    for (i = 0; i < MobVector_Size(&display.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&display.mobs, i);

        if (mob->alive) {
            FCircle circle;
            FleetSprites *fs;
            Sprite *sprite;
            SDL_Point p;

            fs = &display.fleets[mob->playerID];
            sprite = fs->mobSprites[mob->type];

            Mob_GetCircle(mob, &circle);
            p.x = (uint32)(circle.center.x - circle.radius);
            p.y = (uint32)(circle.center.y - circle.radius);

            ASSERT(mob->playerID == PLAYER_ID_NEUTRAL ||
                   mob->playerID < ARRAYSIZE(display.fleets));
            ASSERT(mob->type < ARRAYSIZE(display.fleets[0].mobSprites));

            ASSERT(sprite != NULL);
            Sprite_Blit(sprite, display.sdlRenderer, p.x, p.y);

            if (DRAW_SENSORS) {
                sprite = fs->scanSprites[mob->type];

                Mob_GetSensorCircle(mob, &circle);
                p.x = (int32)(circle.center.x - circle.radius);
                p.y = (int32)(circle.center.y - circle.radius);

                Sprite_Blit(sprite, display.sdlRenderer, p.x, p.y);
            }
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

void Display_Main(void)
{
    SDL_Event event;
    int done = 0;

    uint32 targetFPS = 101;
    uint64 targetUSPerFrame = (1000 * 1000) / targetFPS;
    uint64 startTimeUS, endTimeUS;

    ASSERT(display.initialized);
    display.inMain = TRUE;

    // Start paused
    //display.paused = TRUE;

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
