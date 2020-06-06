/*
 * display.c --
 */

#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "SDL.h"
#include "mbbasic.h"
#include "mbtypes.h"
#include "mbassert.h"
#include "random.h"
#include "display.h"
#include "MBVector.h"

#define SHIP_ALPHA 0x88

typedef struct DisplayShip {
    bool initialized;
    uint32 color;
    SDL_Rect rect;
    SDL_Surface *sprite;
} DisplayShip;

DECLARE_MBVECTOR_TYPE(DisplayShip, DisplayShipVector);

typedef struct DisplayGlobalData {
    bool initialized;
    uint32 width;
    uint32 height;

    SDL_Window *sdlWindow;
    bool paused;
    bool inMain;
    uint64 mobGenerationDrawn;

    SDL_mutex *mobMutex;
    bool mainWaiting;
    SDL_sem *mainSignal;
    uint64 mobGeneration;
    bool mobsAcquired;
    Mob *mobs;
    uint32 numMobs;

    DisplayShipVector ships;
} DisplayGlobalData;

static DisplayGlobalData display;

void Display_Init()
{
    SDL_Surface *sdlSurface = NULL;
    const BattleParams *bp = Battle_GetParams();

    ASSERT(Util_IsZero(&display, sizeof(display)));
    display.width = bp->width;
    display.height = bp->height;

    DisplayShipVector_CreateEmpty(&display.ships);
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
        PANIC("Failed to create window\n");
    }

    sdlSurface = SDL_GetWindowSurface(display.sdlWindow);
    SDL_FillRect(sdlSurface, NULL,
                 SDL_MapRGB(sdlSurface->format, 0x00, 0x00, 0x00));
    SDL_UpdateWindowSurface(display.sdlWindow);

    display.initialized = TRUE;
}

void Display_Exit()
{
    ASSERT(display.initialized);

    free(display.mobs);
    display.mobs = NULL;
    DisplayShipVector_Destroy(&display.ships);

    SDL_DestroyWindow(display.sdlWindow);
    display.sdlWindow = NULL;

    SDL_DestroyMutex(display.mobMutex);
    display.mobMutex = NULL;
    SDL_DestroySemaphore(display.mainSignal);
    display.mainSignal = NULL;

    display.initialized = FALSE;
}


Mob *Display_AcquireMobs(uint32 numMobs)
{
    SDL_LockMutex(display.mobMutex);
    ASSERT(!display.mobsAcquired);

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

    //XXX: We're assuming the caller doesn't re-arrange mobs...
    ASSERT(DisplayShipVector_Size(&display.ships) == display.numMobs);
    if (numMobs > display.numMobs) {
        DisplayShipVector_Resize(&display.ships, numMobs);
        for (uint32 i = display.numMobs; i < numMobs; i++) {
            DisplayShip *ship = DisplayShipVector_GetPtr(&display.ships, i);
            ship->initialized = FALSE;
        }

        //XXX: realloc ?
        display.numMobs = numMobs;
        free(display.mobs);
        display.mobs = malloc(numMobs * sizeof(display.mobs[0]));
    } else {
        ASSERT(display.numMobs == numMobs);
    }

    display.mobsAcquired = TRUE;

    /*
     * We don't unlock the mutex until Display_ReleaseMobs.
     */
    return display.mobs;
}

void Display_ReleaseMobs()
{
    /*
     * We acquired the lock in Display_AcquireMobs.
     */
    ASSERT(display.mobsAcquired);
    display.mobsAcquired = FALSE;
    display.mobGeneration++;
    SDL_UnlockMutex(display.mobMutex);
}

static uint32 DisplayGetColor(uint32 index)
{
    uint32 colors[] = {
        0xFF0000,
        0x00FF00,
        0x0000FF,
        0x808000,
        0x800080,
        0x008080,
        0xFFFFFF,
    };

    index %= ARRAYSIZE(colors);
    return colors[index] | ((SHIP_ALPHA & 0xFF) << 24);
}

static void DisplayDrawCircle(SDL_Surface *sdlSurface, uint32 color,
                              const SDL_Point *center, int radius)
{
    uint8 *pixels;
    int minX = MAX(0, center->x - radius);
    int maxX = MIN(sdlSurface->w, center->x + radius + 1);
    int minY = MAX(0, center->y - radius);
    int maxY = MIN(sdlSurface->h, center->y + radius + 1);

    SDL_LockSurface(sdlSurface);
    pixels = (uint8 *)sdlSurface->pixels;
    pixels += sdlSurface->pitch * minY;

    for (int y = minY; y < maxY; y++) {
        uint32 *row = (uint32 *)pixels;
        int dy = abs(y - center->y);
        for (int x = minX; x < maxX; x++) {
            int dx = abs(x - center->x);
            if (dx * dx + dy * dy <= radius * radius) {
                row[x] = color;
            }
        }
        pixels += sdlSurface->pitch;
    }

    SDL_UnlockSurface(sdlSurface);
}


static void DisplayDrawFrame()
{
    SDL_Surface *sdlSurface = NULL;
    uint32 i;

    ASSERT(display.initialized);

    if (display.paused) {
        return;
    }

    SDL_LockMutex(display.mobMutex);
    if (display.mobGenerationDrawn == display.mobGeneration) {
        SDL_UnlockMutex(display.mobMutex);
        return;
    }
    display.mobGenerationDrawn = display.mobGeneration;

    sdlSurface = SDL_GetWindowSurface(display.sdlWindow);
    if (sdlSurface == NULL) {
        PANIC("SDL_GetWindowSurface failed: %s\n", SDL_GetError());
    }

    SDL_FillRect(sdlSurface, NULL,
                 SDL_MapRGB(sdlSurface->format, 0x00, 0x00, 0x00));

    ASSERT(DisplayShipVector_Size(&display.ships) == display.numMobs);
    for (i = 0; i < display.numMobs; i++) {
        FCircle circle;
        DisplayShip *ship = DisplayShipVector_GetPtr(&display.ships, i);
        Mob *mob = &display.mobs[i];

        Mob_GetCircle(mob, &circle);
        ship->rect.x = (uint32)(circle.center.x - circle.radius);
        ship->rect.y = (uint32)(circle.center.y - circle.radius);
        ship->rect.w = (uint32)(2 * circle.radius);
        ship->rect.h = ship->rect.w;

        if (!ship->initialized) {
            SDL_Point cPoint;
            ship->color = DisplayGetColor(mob->playerID);
            ship->sprite = SDL_CreateRGBSurfaceWithFormat(0, ship->rect.w, ship->rect.h,
                                                          32, SDL_PIXELFORMAT_BGRA32);
            cPoint.x = ship->rect.w / 2;
            cPoint.y = ship->rect.h / 2;
            DisplayDrawCircle(ship->sprite, ship->color, &cPoint, circle.radius);
            ship->initialized = TRUE;

            //XXX: This is never cleaned up.
        }

        if (mob->alive) {
            SDL_BlitSurface(ship->sprite, NULL, sdlSurface, &ship->rect);
        }
    }

    SDL_UpdateWindowSurface(display.sdlWindow);

    if (display.mainWaiting) {
        /*
        * Signal that we're ready for a new frame.
        */
        SDL_SemPost(display.mainSignal);
    }

    SDL_UnlockMutex(display.mobMutex);
}

void Display_Main(void)
{
    SDL_Event event;
    int done = 0;

    ASSERT(display.initialized);
    display.inMain = TRUE;

    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    done = 1;
                    break;
                case SDL_MOUSEBUTTONUP:
                    display.paused = !display.paused;
                    break;
                default:
                    break;
            }
        }

        DisplayDrawFrame();
        usleep(1000);
    }

    /*
     * Ensure the main thread wakes up if we exited early.
     */
    SDL_SemPost(display.mainSignal);
    display.inMain = FALSE;
}
