/*
 * display.h --
 */

#ifndef _DISPLAY_H_202005252017
#define _DISPLAY_H_202005252017

#include "battle.h"

void Display_Init();
void Display_Exit();
BattleMob *Display_AcquireMobs(uint32 numMobs);
void Display_ReleaseMobs();
void Display_Main();

#endif // _DISPLAY_H_202005252017
