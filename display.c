/*
 * display.c --
 */

#include <stdio.h>

#include "config.h"
#include "SDL.h"
#include "mbtypes.h"
#include "mbassert.h"
#include "random.h"

static SDL_Window *sdlWindow;

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

        //SDL_GL_SwapBuffers();
    }
}

static void DisplayCreateWindow(void)
{
    SDL_Surface *sdlSurface = NULL;

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    sdlWindow = SDL_CreateWindow("SpaceRobots2", 0, 0, 800, 600,
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
