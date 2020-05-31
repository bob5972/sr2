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
    SDL_Surface *sprite;
} DisplayShip;

typedef struct DisplayGlobalData {
    SDL_Window *sdlWindow;
    uint32 frameNum;
    bool paused;
    DisplayShip ships[10];
} DisplayGlobalData;

static DisplayGlobalData displayData;

static void DisplayDrawFrame()
{
    SDL_Surface *sdlSurface = NULL;
    uint32 i;

    if (displayData.paused) {
        return;
    }

    sdlSurface = SDL_GetWindowSurface(displayData.sdlWindow);

    if (displayData.frameNum == 0) {
        for (i = 0; i < ARRAYSIZE(displayData.ships); i++) {
            DisplayShip *ship = &displayData.ships[i];
            SDL_Rect spriteRect;
            ship->rect.x = i * 10;
            ship->rect.y = i * 20;
            ship->rect.w = Random_Int(10, 100);
            ship->rect.h = Random_Int(10, 100);

            ship->color = SDL_MapRGBA(sdlSurface->format, SHIP_ALPHA,
                                      Random_Uint32(), Random_Uint32(),
                                      Random_Uint32());

            ship->sprite = SDL_CreateRGBSurface(0, ship->rect.w, ship->rect.h,
                                                32, 0xFF << 24, 0xFF << 16,
                                                0xFF << 8, 0xFF << 0);

            spriteRect = ship->rect;
            spriteRect.x = 0;
            spriteRect.y = 0;
            SDL_FillRect(ship->sprite, &spriteRect, ship->color);
        }

        //XXX: Clean up?
    }

    displayData.frameNum++;

    SDL_FillRect(sdlSurface, NULL,
                 SDL_MapRGB(sdlSurface->format, 0x00, 0x00, 0x00));

    for (i = 0; i < ARRAYSIZE(displayData.ships); i++) {
        DisplayShip *ship = &displayData.ships[i];
        ship->rect.x += 1 + (i / 4);
        ship->rect.y += 1 + (i / 4);
        ship->rect.x %= MAP_WIDTH;
        ship->rect.y %= MAP_HEIGHT;

        SDL_BlitSurface(ship->sprite, NULL, sdlSurface, &ship->rect);
    }

    SDL_UpdateWindowSurface(displayData.sdlWindow);
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
                case SDL_MOUSEBUTTONUP:
                    displayData.paused = !displayData.paused;
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

    displayData.sdlWindow =
        SDL_CreateWindow("SpaceRobots2", 0, 0, MAP_WIDTH, MAP_HEIGHT,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (displayData.sdlWindow == NULL) {
        PANIC("Failed to create window\n");
    }

    sdlSurface = SDL_GetWindowSurface(displayData.sdlWindow);
    SDL_FillRect(sdlSurface, NULL,
                 SDL_MapRGB(sdlSurface->format, 0x00, 0x00, 0x00));
    SDL_UpdateWindowSurface(displayData.sdlWindow);
}

int Display_Main(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    DisplayCreateWindow();
    DisplayLoop();

    SDL_DestroyWindow(displayData.sdlWindow);
    displayData.sdlWindow = NULL;

    SDL_Quit();

    return 0;
}
