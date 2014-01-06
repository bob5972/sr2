/*
 * main.c --
 */

#include <stdio.h>
#include "mbtypes.h"
#include "random.h"

#define INVALID ((uint32)-1)

typedef struct Mob {
    uint32 id;
    float x;
    float y;
} Mob;

typedef struct World {
    float width;
    float height;
} World;

struct MainData {
    Mob mobs[100];
    World world;
} mainData;

int main(void)
{
    uint32 i;
    bool done;
    uint32 iterations;

    //Setup
    printf("Starting ants...\n");
    Util_Zero(&mainData, sizeof(mainData));
    for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
        mainData.mobs[i].id = INVALID;
    }

    mainData.world.width = 100;
    mainData.world.height = 100;

    for (i = 0; i < ARRAYSIZE(mainData.mobs); i++) {
        mainData.mobs[i].id = i;
        mainData.mobs[i].x = Random_Float(0.0f, mainData.world.width);
        mainData.mobs[i].y = Random_Float(0.0f, mainData.world.height);
    }

    for (iterations = 0; iterations < 100; iterations++) {
        // Run AI
        // Run Physics
        // Draw
    }

    //Cleanup
    printf("Done!\n");
    return 0;
}

