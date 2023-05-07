/*
 * fleetConfig.c -- part of SpaceRobots2
 * Copyright (C) 2023 Michael Banack <github@banack.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "fleetConfig.h"
#include "fleet.h"

FleetConfigValue FC_neuralDefaults[] = {
    { "attackExtendedRange",         "TRUE"      },
    { "attackRange",                 "117.644791"},
    { "baseDefenseRadius",           "143.515045"},
    { "baseSpawnJitter",             "1"         },
    { "creditReserve",               "200"       },

    { "evadeFighters",               "FALSE"     },
    { "evadeRange",                  "289.852631"},
    { "evadeStrictDistance",         "105.764320"},
    { "evadeUseStrictDistance",      "TRUE"      },

    { "gatherAbandonStale",          "FALSE"     },
    { "gatherRange",                 "50"        },
    { "guardRange",                  "0"         },

    { "nearBaseRandomIdle.forceOn",  "TRUE"      },
    { "randomIdle.forceOn",          "TRUE"      },
    { "randomizeStoppedVelocity.forceOn", "TRUE" },
    { "rotateStartingAngle",         "TRUE",     },
    { "simpleAttack.forceOn",        "TRUE"      },

    { "nearBaseRadius",              "100.0"     },
    { "baseDefenseRadius",           "250.0"     },

    { "sensorGrid.staleCoreTime",    "28.385160" },
    { "sensorGrid.staleFighterTime", "16.703636" },

    { "startingMaxRadius",           "300"       },
    { "startingMinRadius",           "250"       },
};

FleetConfigValue FC_neural1[] = {
#include "fleetData/neural1.config"
};

FleetConfigValue FC_neural2[] = {
#include "fleetData/neural2.config"
};
FleetConfigValue FC_neural3[] = {
#include "fleetData/neural3.config"
};
FleetConfigValue FC_neural4[] = {
#include "fleetData/neural4.config"
};
FleetConfigValue FC_neural5[] = {
#include "fleetData/neural5.config"
};
FleetConfigValue FC_neural6[] = {
#include "fleetData/neural6.config"
};
FleetConfigValue FC_neural7[] = {
#include "fleetData/neural7.config"
};
FleetConfigValue FC_neural8[] = {
#include "fleetData/neural8.config"
};
FleetConfigValue FC_neural9[] = {
#include "fleetData/neural9.config"
};
FleetConfigValue FC_neural10[] = {
#include "fleetData/neural10.config"
};
FleetConfigValue FC_neural11[] = {
#include "fleetData/neural11.config"
};
FleetConfigValue FC_neural12[] = {
#include "fleetData/neural12.config"
};
FleetConfigValue FC_neural13[] = {
#include "fleetData/neural13.config"
};

static const FleetConfigValue FC_bineuralDefaults[] = {
    { "attackExtendedRange",         "TRUE"      },
    { "attackRange",                 "117.644791"},
    { "baseDefenseRadius",           "143.515045"},
    { "baseSpawnJitter",             "1"         },
    { "creditReserve",               "200"       },

    { "evadeFighters",               "FALSE"     },
    { "evadeRange",                  "289.852631"},
    { "evadeStrictDistance",         "105.764320"},
    { "evadeUseStrictDistance",      "TRUE"      },

    { "gatherAbandonStale",          "FALSE"     },
    { "gatherRange",                 "50"        },
    { "guardRange",                  "0"         },

    { "nearBaseRandomIdle.forceOn",  "TRUE"      },
    { "randomIdle.forceOn",          "TRUE"      },
    { "randomizeStoppedVelocity.forceOn", "TRUE" },
    { "rotateStartingAngle",         "TRUE",     },
    { "simpleAttack.forceOn",        "TRUE"      },

    { "nearBaseRadius",              "100.0"     },
    { "baseDefenseRadius",           "250.0"     },

    { "sensorGrid.staleCoreTime",    "28.385160" },
    { "sensorGrid.staleFighterTime", "16.703636" },

    { "startingMaxRadius",           "300"       },
    { "startingMinRadius",           "250"       },

    { "useAttackForces",             "FALSE"     },
};

static const FleetConfigValue FC_bineural1[] = {
#include "fleetData/bineural1.config"
};
static const FleetConfigValue FC_bineural2[] = {
#include "fleetData/bineural2.config"
};
static const FleetConfigValue FC_bineural3[] = {
#include "fleetData/bineural3.config"
};
static const FleetConfigValue FC_bineural4[] = {
#include "fleetData/bineural4.config"
};

static const FleetConfigValue FC_bineural5[] = {
#include "fleetData/bineural5.config"
};

static const FleetConfigValue FC_matrix1[] = {
};

static const FleetConfigValue FC_matrixDefaults[] = {
    { "attackExtendedRange", "TRUE" },
    { "attackRange", "119.589478" },
    { "creditReserve", "0.000000" },
    { "evadeFighters", "FALSE" },
    { "evadeRange", "-0.997500" },
    { "evadeStrictDistance", "130.109604" },
    { "evadeUseStrictDistance", "FALSE" },
    { "gatherAbandonStale", "FALSE" },
    { "gatherRange", "51.572159" },
    { "guardRange", "113.814850" },
    { "rotateStartingAngle", "TRUE" },
    { "sensorGrid.mapping.recentlyScannedMoveFocusTicks", "0.000000" },
    { "sensorGrid.mapping.recentlyScannedResetTicks", "976.480957" },
    { "sensorGrid.staleCoreTime", "0.000000" },
    { "sensorGrid.staleFighterTime", "0.000000" },
    { "startingMaxRadius", "1362.524536" },
    { "startingMinRadius", "774.700012" },
};

static void FleetConfigPush(MBRegistry *mreg, const FleetConfigTable *defaults,
                            const FleetConfigTable *values)
{
    const FleetConfigValue *config;
    uint size;

    config = values->values;
    size = values->numValues;
    for (uint k = 0; k < size; k++) {
        if (config[k].value != NULL &&
            !MBRegistry_ContainsKey(mreg, config[k].key)) {
            MBRegistry_PutConst(mreg, config[k].key, config[k].value);
        }
    }

    config = defaults->values;
    size = defaults->numValues;
    for (uint k = 0; k < size; k++) {
        if (config[k].value != NULL &&
            !MBRegistry_ContainsKey(mreg, config[k].key)) {
            MBRegistry_PutConst(mreg, config[k].key,
                                config[k].value);
        }
    }
}

void FleetConfig_PushDefaults(MBRegistry *mreg, FleetAIType aiType)
{
    static const FleetConfigTable neuralDefaults = {
        FC_neuralDefaults, ARRAYSIZE(FC_neuralDefaults),
    };
    static const FleetConfigTable bineuralDefaults = {
        FC_bineuralDefaults, ARRAYSIZE(FC_bineuralDefaults),
    };
    static const FleetConfigTable matrixDefaults = {
        FC_matrixDefaults, ARRAYSIZE(FC_matrixDefaults),
    };

#define F(_fleetAI, _defaults, _lcName) \
    { FLEET_AI_ ## _fleetAI, _defaults, { FC_ ## _lcName, ARRAYSIZE(FC_ ## _lcName), }, }

    static const struct {
        FleetAIType aiType;
        const FleetConfigTable *defaults;
        FleetConfigTable config;
    } fleets[] = {
        F(NEURAL1,  &neuralDefaults, neural1),
        F(NEURAL2,  &neuralDefaults, neural2),
        F(NEURAL3,  &neuralDefaults, neural3),
        F(NEURAL4,  &neuralDefaults, neural4),
        F(NEURAL5,  &neuralDefaults, neural5),
        F(NEURAL6,  &neuralDefaults, neural6),
        F(NEURAL7,  &neuralDefaults, neural7),
        F(NEURAL8,  &neuralDefaults, neural8),
        F(NEURAL9,  &neuralDefaults, neural9),
        F(NEURAL10, &neuralDefaults, neural10),
        F(NEURAL11, &neuralDefaults, neural11),
        F(NEURAL12, &neuralDefaults, neural12),
        F(NEURAL13, &neuralDefaults, neural13),

        F(BINEURAL1, &bineuralDefaults, bineural1),
        F(BINEURAL2, &bineuralDefaults, bineural2),
        F(BINEURAL3, &bineuralDefaults, bineural3),
        F(BINEURAL4, &bineuralDefaults, bineural4),
        F(BINEURAL5, &bineuralDefaults, bineural5),

        F(MATRIX1, &matrixDefaults, matrix1),
    };

#undef F

    uint i;
    for (i = 0; i < ARRAYSIZE(fleets); i++) {
        if (fleets[i].aiType == aiType) {
            FleetConfigPush(mreg, fleets[i].defaults, &fleets[i].config);
            return;
        }
    }

    NOT_IMPLEMENTED();
}