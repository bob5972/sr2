/*
 * fleet.h --
 */

#ifndef _FLEET_H_202005311442
#define _FLEET_H_202005311442

#include "battle.h"

void Fleet_Init();
void Fleet_Exit();
void Fleet_RunTick(BattleMob *mobs, uint32 numMobs);

#endif // _FLEET_H_202005311442
