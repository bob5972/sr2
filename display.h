/*
 * display.h --
 */

#ifndef _DISPLAY_H_202005252017
#define _DISPLAY_H_202005252017

#include "battle.h"
#include "mob.h"

void Display_Init();
void Display_Exit();
Mob *Display_AcquireMobs(uint32 numMobs);
void Display_ReleaseMobs();
void Display_Main();

#endif // _DISPLAY_H_202005252017
