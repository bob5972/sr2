/*
 * battle.h --
 */

#ifndef _BATTLE_H_202005310644
#define _BATTLE_H_202005310644

#include "geometry.h"
#include "mob.h"
#include "fleet.h"
#include "battleTypes.h"

void Battle_Init(const BattleParams *bp);
void Battle_Exit();
void Battle_RunTick();
const BattleParams *Battle_GetParams();
Mob *Battle_AcquireMobs(uint32 *numMobs);
void Battle_ReleaseMobs();
const BattleStatus *Battle_AcquireStatus();
void Battle_ReleaseStatus();


#endif // _BATTLE_H_202005310644
