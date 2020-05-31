/*
 * display.h --
 */

#ifndef _DISPLAY_H_202005252017
#define _DISPLAY_H_202005252017

#include "battle.h"

typedef struct DisplayMapParams {
    uint32 width;
    uint32 height;
} DisplayMapParams;

void Display_Init(const DisplayMapParams *dmp);
void Display_Exit();
BattleMob *Display_AcquireMobs(uint32 numMobs);
void Display_ReleaseMobs();
void Display_Main();

#endif // _DISPLAY_H_202005252017
