/*
 * fleet.h --
 */

#ifndef _FLEET_H_202005311442
#define _FLEET_H_202005311442

#include "mob.h"
#include "battleTypes.h"

void Fleet_Init();
void Fleet_Exit();
void Fleet_RunTick(const BattleStatus *bs, Mob *mobs, uint32 numMobs);

#endif // _FLEET_H_202005311442
