/*
 * mob.c --
 */

#include "mob.h"

void Mob_GetQuad(const Mob *mob, FQuad *q)
{
    q->x = mob->pos.x;
    q->y = mob->pos.y;

    switch (mob->type) {
        case MOB_TYPE_BASE:
            q->w = 50.0f;
            q->h = 50.0f;
            break;
        case MOB_TYPE_FIGHTER:
            q->w = 10.0f;
            q->h = 10.0f;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", mob->type);
            break;
    }
}

float Mob_GetSpeed(const Mob *mob)
{
    switch (mob->type) {
        case MOB_TYPE_BASE:
            return 1.0f;
            break;
        case MOB_TYPE_FIGHTER:
            return 5.0f;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", mob->type);
            break;
    }

    NOT_REACHED();
}
