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

// Poor mans command-line options...
#define DRAW_SENSORS TRUE

typedef struct FleetSprites {
    uint32 color;
    SDL_Surface *mobSprites[MOB_TYPE_MAX];
    SDL_Surface *scanSprites[MOB_TYPE_MAX];
} FleetSprites;

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
    MobVector mobs;

    FleetSprites fleets[8];
} DisplayGlobalData;

static DisplayGlobalData display;

static uint32 DisplayGetColor(uint32 index);
static void DisplayDrawCircle(SDL_Surface *sdlSurface, uint32 color,
                              const SDL_Point *center, int radius);
static SDL_Surface *DisplayCreateCircleSprite(uint32 radius, uint32 color);

void Display_Init()
{
    SDL_Surface *sdlSurface = NULL;
    const BattleParams *bp = Battle_GetParams();

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
        PANIC("Failed to create window\n");
    }

    sdlSurface = SDL_GetWindowSurface(display.sdlWindow);
    SDL_FillRect(sdlSurface, NULL,
                 SDL_MapRGB(sdlSurface->format, 0x00, 0x00, 0x00));
    SDL_UpdateWindowSurface(display.sdlWindow);

    ASSERT(bp->numPlayers <= ARRAYSIZE(display.fleets));
    for (uint x = 0; x < bp->numPlayers; x++) {
        uint32 color = DisplayGetColor(x);
        display.fleets[x].color = color;

        for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
            uint32 radius = (uint32)MobType_GetRadius(t);
            display.fleets[x].mobSprites[t] =
                DisplayCreateCircleSprite(radius, color);

            radius = (uint32)MobType_GetSensorRadius(t);
            display.fleets[x].scanSprites[t] =
                DisplayCreateCircleSprite(radius, color / 2);
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
            if (display.fleets[x].mobSprites[i] != NULL) {
                SDL_FreeSurface(display.fleets[x].mobSprites[i]);
            }
        }
        for (uint i = 0; i < ARRAYSIZE(display.fleets[x].scanSprites); i++) {
            if (display.fleets[x].scanSprites[i] != NULL) {
                SDL_FreeSurface(display.fleets[x].scanSprites[i]);
            }
        }
    }

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

    MobVector_Resize(&display.mobs, numMobs);
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
    display.mobsAcquired = FALSE;
    display.mobGeneration++;
    SDL_UnlockMutex(display.mobMutex);
}

static uint32 DisplayGetColor(uint32 index)
{
    uint32 colors[] = {
        0x888888, // 0 is NEUTRAL
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

static SDL_Surface *DisplayCreateCircleSprite(uint32 radius, uint32 color)
{
    uint32 d = 2 * radius;
    SDL_Point cPoint;
    SDL_Surface *sprite;

    sprite = SDL_CreateRGBSurfaceWithFormat(0, d, d, 32,
                                            SDL_PIXELFORMAT_BGRA32);
    cPoint.x = d / 2;
    cPoint.y = d / 2;
    DisplayDrawCircle(sprite, color, &cPoint, radius);

    return sprite;
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

    for (i = 0; i < MobVector_Size(&display.mobs); i++) {
        Mob *mob = MobVector_GetPtr(&display.mobs, i);

        if (mob->alive) {
            FCircle circle;
            SDL_Surface *sprite;
            SDL_Rect rect;

            Mob_GetCircle(mob, &circle);
            rect.x = (uint32)(circle.center.x - circle.radius);
            rect.y = (uint32)(circle.center.y - circle.radius);
            rect.w = (uint32)(2 * circle.radius);
            rect.h = rect.w;

            ASSERT(mob->playerID == PLAYER_ID_NEUTRAL ||
                   mob->playerID < ARRAYSIZE(display.fleets));
            ASSERT(mob->type < ARRAYSIZE(display.fleets[0].mobSprites));
            sprite = display.fleets[mob->playerID].mobSprites[mob->type];
            ASSERT(sprite != NULL);
            SDL_BlitSurface(sprite, NULL, sdlSurface, &rect);

            if (DRAW_SENSORS) {
                Mob_GetSensorCircle(mob, &circle);
                rect.x = (uint32)(circle.center.x - circle.radius);
                rect.y = (uint32)(circle.center.y - circle.radius);
                rect.w = (uint32)(2 * circle.radius);
                rect.h = rect.w;
                sprite = display.fleets[mob->playerID].scanSprites[mob->type];
                SDL_BlitSurface(sprite, NULL, sdlSurface, &rect);
            }
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
