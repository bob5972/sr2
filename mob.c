/*
 * mob.c --
 */

#include "mob.h"

#define SPEED_SCALE (2.0f)
#define SIZE_SCALE (1.0f)

float MobType_GetRadius(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return 50.0f / SIZE_SCALE;
            break;
        case MOB_TYPE_FIGHTER:
            return 5.0f / SIZE_SCALE;
            break;
        case MOB_TYPE_MISSILE:
            return 3.0f / SIZE_SCALE;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}

float MobType_GetSensorRadius(MobType type)
{
    float sensorScale;

    if (type == MOB_TYPE_BASE) {
        sensorScale = 5.0f;
    } else {
        sensorScale = 10.0f;
    }

    return sensorScale * MobType_GetRadius(type);
}

void Mob_GetCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;
    c->radius = MobType_GetRadius(mob->type);
}


void Mob_GetSensorCircle(const Mob *mob, FCircle *c)
{
    c->center.x = mob->pos.x;
    c->center.y = mob->pos.y;
    c->radius = MobType_GetSensorRadius(mob->type);
}

float MobType_GetSpeed(MobType type)
{
    float speed;

    switch (type) {
        case MOB_TYPE_BASE:
            speed = 0.5f / SPEED_SCALE;
            break;
        case MOB_TYPE_FIGHTER:
            speed = 5.0f / SPEED_SCALE;
            break;
        case MOB_TYPE_MISSILE:
            speed = 10.0f / SPEED_SCALE;
            break;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    ASSERT(MobType_GetRadius(type) >= SPEED_SCALE);
    return speed;
}

int MobType_GetCost(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return -1;
        case MOB_TYPE_FIGHTER:
            return 100;
        case MOB_TYPE_MISSILE:
            return 1;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}

int MobType_GetMaxFuel(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return -1;
        case MOB_TYPE_FIGHTER:
            return -1;
        case MOB_TYPE_MISSILE:
            return 50;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}


int MobType_GetMaxHealth(MobType type)
{
    switch (type) {
        case MOB_TYPE_BASE:
            return 50;
        case MOB_TYPE_FIGHTER:
            return 1;
        case MOB_TYPE_MISSILE:
            return 1;
        default:
            PANIC("Unhandled mob type: %d\n", type);
            break;
    }

    NOT_REACHED();
}

void Mob_Init(Mob *mob, MobType t)
{
    MBUtil_Zero(mob, sizeof(*mob));
    mob->alive = TRUE;
    mob->playerID = PLAYER_ID_INVALID;
    mob->id = MOB_ID_INVALID;
    mob->type = t;
    mob->fuel = MobType_GetMaxFuel(t);
    mob->health = MobType_GetMaxHealth(t);
    mob->cmd.spawn = MOB_TYPE_INVALID;
}

bool Mob_CheckInvariants(const Mob *m)
{
    ASSERT(m != NULL);
    ASSERT(m->id != MOB_ID_INVALID);
    ASSERT(m->type != MOB_TYPE_INVALID);
    ASSERT(m->type >= MOB_TYPE_MIN);
    ASSERT(m->type < MOB_TYPE_MAX);
    ASSERT(m->playerID != PLAYER_ID_INVALID);
    ASSERT(!(m->removeMob && m->alive));
    ASSERT(m->fuel <= MobType_GetMaxFuel(m->type));
    ASSERT(m->health <= MobType_GetMaxHealth(m->type));
    return TRUE;
}

void SensorMob_InitFromMob(SensorMob *sm, const Mob *mob)
{
    ASSERT(sm != NULL);
    ASSERT(mob != NULL);

    MBUtil_Zero(sm, sizeof(*sm));
    sm->mobid = mob->id;
    sm->type = mob->type;
    sm->playerID = mob->playerID;
    sm->alive = mob->alive;
    sm->pos = mob->pos;
}
