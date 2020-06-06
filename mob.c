/*
 * mob.c --
 */

#include "mob.h"

#define SPEED_SCALE (2.0f)
#define SIZE_SCALE (1.0f)

float MobGetRadius(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return 50.0f / SIZE_SCALE;
            break;
        case MOB_TYPE_FIGHTER:
            return 10.0f / SIZE_SCALE;
            break;
        case MOB_TYPE_ROCKET:
            return 3.0f / SIZE_SCALE;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}

void Mob_GetCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;
    c->radius = MobGetRadius(mob->type);
}

float Mob_GetSpeed(const Mob *mob)
{
    float speed;

    switch (mob->type) {
        case MOB_TYPE_BASE:
            speed = 1.0f / SPEED_SCALE;
            break;
        case MOB_TYPE_FIGHTER:
            speed = 5.0f / SPEED_SCALE;
            break;
        case MOB_TYPE_ROCKET:
            speed = 10.0f / SPEED_SCALE;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", mob->type);
            break;
    }

    ASSERT(MobGetRadius(mob->type) >= SPEED_SCALE);
    return speed;
}

uint Mob_GetMaxFuel(const Mob *mob)
{
    return Mob_GetMaxFuelForType(mob->type);
}


uint Mob_GetMaxFuelForType(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return -1;
        case MOB_TYPE_FIGHTER:
            return -1;
        case MOB_TYPE_ROCKET:
            return 100;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}

