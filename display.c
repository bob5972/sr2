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

#define SHIP_ALPHA 0x88
#define MAP_WIDTH 800
#define MAP_HEIGHT 600

typedef struct DisplayShip {
    SDL_Rect rect;
    uint32 color;
} DisplayShip;

typedef struct DisplayGlobalData {
    uint32 frameNum;
} DisplayGlobalData;

static DisplayGlobalData displayData;

static SDL_Window *sdlWindow;
static DisplayShip ships[2];

static void DisplayDrawFrame()
{
    SDL_Surface *sdlSurface = NULL;
    uint32 i;

    sdlSurface = SDL_GetWindowSurface(sdlWindow);

    if (displayData.frameNum == 0) {
        for (i = 0; i < ARRAYSIZE(ships); i++) {
            ships[i].rect.x = i * 10;
            ships[i].rect.y = i * 20;
            ships[i].rect.w = 20 * (i + 1);
            ships[i].rect.h = 20 * (i + 1);
        }

        ships[0].color = SDL_MapRGBA(sdlSurface->format,
                                    SHIP_ALPHA, 0xFF, 0x00, 0x00);
        ships[1].color = SDL_MapRGBA(sdlSurface->format,
                                    SHIP_ALPHA, 0x00, 0xFF, 0x00);
    }

    displayData.frameNum++;

    SDL_FillRect(sdlSurface, NULL,
                 SDL_MapRGB(sdlSurface->format, 0x00, 0x00, 0x00));

    for (i = 0; i < ARRAYSIZE(ships); i++) {
        ships[i].rect.x += i + 1;
        ships[i].rect.y += i + 1;
        ships[i].rect.x %= MAP_WIDTH;
        ships[i].rect.y %= MAP_HEIGHT;

        SDL_FillRect(sdlSurface, &ships[i].rect, ships[i].color);
    }

    SDL_UpdateWindowSurface(sdlWindow);
}

static void DisplayLoop()
{
    SDL_Event event;
    int done = 0;

    while (!done) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    done = 1;
                    break;
                default:
                    break;
            }
        }

        DisplayDrawFrame();
        usleep(1000);
    }
}

static void DisplayCreateWindow(void)
{
    SDL_Surface *sdlSurface = NULL;

    sdlWindow = SDL_CreateWindow("SpaceRobots2", 0, 0, MAP_WIDTH, MAP_HEIGHT,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (sdlWindow == NULL) {
        PANIC("Failed to create window\n");
    }

    sdlSurface = SDL_GetWindowSurface(sdlWindow);
    SDL_FillRect(sdlSurface, NULL,
                 SDL_MapRGB(sdlSurface->format, 0x00, 0x00, 0x00));
    SDL_UpdateWindowSurface(sdlWindow);
}

int Display_Main(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    DisplayCreateWindow();
    DisplayLoop();

    SDL_DestroyWindow(sdlWindow);
    sdlWindow = NULL;

    SDL_Quit();

    return 0;
}
