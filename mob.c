/*
 * mob.c --
 */

#include "mob.h"

void Mob_GetCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;

    switch (mob->type) {
        case MOB_TYPE_BASE:
            c->radius = 50.0f;
            break;
        case MOB_TYPE_FIGHTER:
            c->radius = 10.0f;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", mob->type);
            break;
    }
}

float Mob_GetSpeed(const Mob *mob)
{
    float scale = 2.0f;
    switch (mob->type) {
        case MOB_TYPE_BASE:
            return 1.0f / scale;
            break;
        case MOB_TYPE_FIGHTER:
            return 5.0f / scale;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", mob->type);
            break;
    }

    NOT_REACHED();
}
