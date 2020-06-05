/*
 * fleet.h --
 */

#ifndef _FLEET_H_202005311442
#define _FLEET_H_202005311442

#include "battle.h"
#include "mob.h"

void Fleet_Init();
void Fleet_Exit();
void Fleet_RunTick(Mob *mobs, uint32 numMobs);

#endif // _FLEET_H_202005311442
