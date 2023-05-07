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
    static const FleetConfigTable neural1 = {
        FC_neural1, ARRAYSIZE(FC_neural1),
    };
    static const FleetConfigTable neural2 = {
        FC_neural2, ARRAYSIZE(FC_neural2),
    };
    static const FleetConfigTable neural3 = {
        FC_neural3, ARRAYSIZE(FC_neural3),
    };
    static const FleetConfigTable neural4 = {
        FC_neural4, ARRAYSIZE(FC_neural4),
    };
    static const FleetConfigTable neural5 = {
        FC_neural5, ARRAYSIZE(FC_neural5),
    };
    static const FleetConfigTable neural6 = {
        FC_neural6, ARRAYSIZE(FC_neural6),
    };
    static const FleetConfigTable neural7 = {
        FC_neural7, ARRAYSIZE(FC_neural7),
    };
    static const FleetConfigTable neural8 = {
        FC_neural8, ARRAYSIZE(FC_neural8),
    };
    static const FleetConfigTable neural9 = {
        FC_neural9, ARRAYSIZE(FC_neural9),
    };
    static const FleetConfigTable neural10 = {
        FC_neural10, ARRAYSIZE(FC_neural10),
    };
    static const FleetConfigTable neural11 = {
        FC_neural11, ARRAYSIZE(FC_neural11),
    };
    static const FleetConfigTable neural12 = {
        FC_neural12, ARRAYSIZE(FC_neural12),
    };
    static const FleetConfigTable neural13 = {
        FC_neural13, ARRAYSIZE(FC_neural13),
    };

    static const FleetConfigTable bineuralDefaults = {
        FC_bineuralDefaults, ARRAYSIZE(FC_bineuralDefaults),
    };
    static const FleetConfigTable bineural1 = {
        FC_bineural1, ARRAYSIZE(FC_bineural1),
    };
    static const FleetConfigTable bineural2 = {
        FC_bineural2, ARRAYSIZE(FC_bineural2),
    };
    static const FleetConfigTable bineural3 = {
        FC_bineural3, ARRAYSIZE(FC_bineural3),
    };
    static const FleetConfigTable bineural4 = {
        FC_bineural4, ARRAYSIZE(FC_bineural4),
    };
    static const FleetConfigTable bineural5 = {
        FC_bineural5, ARRAYSIZE(FC_bineural5),
    };

    if (Fleet_IsBineuralFleet(aiType)) {
        if (aiType == FLEET_AI_BINEURAL1) {
            FleetConfigPush(mreg, &bineuralDefaults, &bineural1);
        } else if (aiType == FLEET_AI_BINEURAL2) {
            FleetConfigPush(mreg, &bineuralDefaults, &bineural2);
        } else if (aiType == FLEET_AI_BINEURAL3) {
            FleetConfigPush(mreg, &bineuralDefaults, &bineural3);
        } else if (aiType == FLEET_AI_BINEURAL4) {
            FleetConfigPush(mreg, &bineuralDefaults, &bineural4);
        } else if (aiType == FLEET_AI_BINEURAL5) {
            FleetConfigPush(mreg, &bineuralDefaults, &bineural5);
        } else {
            NOT_IMPLEMENTED();
        }
    } else if (Fleet_IsNeuralFleet(aiType)) {
        if (aiType == FLEET_AI_NEURAL1) {
            FleetConfigPush(mreg, &neuralDefaults, &neural1);
        } else if (aiType == FLEET_AI_NEURAL2) {
            FleetConfigPush(mreg, &neuralDefaults, &neural2);
        } else if (aiType == FLEET_AI_NEURAL3) {
            FleetConfigPush(mreg, &neuralDefaults, &neural3);
        } else if (aiType == FLEET_AI_NEURAL4) {
            FleetConfigPush(mreg, &neuralDefaults, &neural4);
        } else if (aiType == FLEET_AI_NEURAL5) {
            FleetConfigPush(mreg, &neuralDefaults, &neural5);
        } else if (aiType == FLEET_AI_NEURAL6) {
            FleetConfigPush(mreg, &neuralDefaults, &neural6);
        } else if (aiType == FLEET_AI_NEURAL7) {
            FleetConfigPush(mreg, &neuralDefaults, &neural7);
        } else if (aiType == FLEET_AI_NEURAL8) {
            FleetConfigPush(mreg, &neuralDefaults, &neural8);
        } else if (aiType == FLEET_AI_NEURAL9) {
            FleetConfigPush(mreg, &neuralDefaults, &neural9);
        } else if (aiType == FLEET_AI_NEURAL10) {
            FleetConfigPush(mreg, &neuralDefaults, &neural10);
        } else if (aiType == FLEET_AI_NEURAL11) {
            FleetConfigPush(mreg, &neuralDefaults, &neural11);
        } else if (aiType == FLEET_AI_NEURAL12) {
            FleetConfigPush(mreg, &neuralDefaults, &neural12);
        } else if (aiType == FLEET_AI_NEURAL13) {
            FleetConfigPush(mreg, &neuralDefaults, &neural13);
        } else {
            NOT_IMPLEMENTED();
        }
    } else {
        NOT_IMPLEMENTED();
    }
}