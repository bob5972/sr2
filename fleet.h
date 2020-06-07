/*
 * fleet.h --
 */

#ifndef _FLEET_H_202005311442
#define _FLEET_H_202005311442

#include "mob.h"

typedef enum FleetAIType {
    FLEET_AI_INVALID = 0,
    FLEET_AI_DUMMY   = 1,
    FLEET_AI_SIMPLE  = 2,
    FLEET_AI_MAX,
} FleetAIType;

void Fleet_Init();
void Fleet_Exit();
void Fleet_RunTick(Mob *mobs, uint32 numMobs);

#endif // _FLEET_H_202005311442
