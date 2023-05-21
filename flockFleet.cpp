/*
 * flockFleet.cpp -- part of SpaceRobots2
 * Copyright (C) 2020-2021 Michael Banack <github@banack.net>
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

extern "C" {
#include "fleet.h"
#include "Random.h"
#include "battle.h"
}

#include "MBVarMap.h"

#include "mutate.h"
#include "sensorGrid.hpp"
#include "basicShipAI.hpp"
#include "MBMap.hpp"

#include "flockFleet.hpp"

typedef struct FlockConfigValue {
    const char *key;
    const char *value;
} FlockConfigValue;

typedef enum FlockPullType {
    PULL_ALWAYS,
    PULL_RANGE,
} FlockPullType;


static void FlockFleetAlign(const FlockFleetConfig *ffc,
                            const FPoint *avgVel, FRPoint *rPos);
static void FlockFleetCohere(AIContext *aic, const FlockFleetConfig *ffc,
                             Mob *mob, const FPoint *avgPos, FRPoint *rPos);
static void FlockFleetBrokenCoherePos(AIContext *aic,
                                      const FlockFleetConfig *ffc,
                                      FPoint *avgPos, const FPoint *center);
static void FlockFleetSeparate(AIContext *aic,
                               Mob *mob, FRPoint *rPos,
                               float radius, float weight);

static void FlockFleetRepulseVector(AIContext *aic,
                                    FRPoint *repulseVec,
                                    FPoint *pos, FPoint *c,
                                    float repulseRadius);

static void FlockFleetAvoidEdges(AIContext *aic,
                                 Mob *mob, FRPoint *rPos,
                                 float repulseRadius, float weight);

static float FlockFleetEdgeDistance(AIContext *aic, FPoint *pos);

static void FlockFleetFindCenter(AIContext *aic,
                                 Mob *mob, FRPoint *rPos,
                                 float radius, float weight);

static void
FlockFleetPullVector(FRPoint *curForce,
                     const FPoint *cPos, const FPoint *tPos,
                     float radius, float weight, FlockPullType pType);

static void
FlockFleetFindBase(AIContext *aic,
                   Mob *mob, FRPoint *rPos,
                   float radius, float weight);


static void
FlockFleetFindEnemyBase(AIContext *aic,
                        Mob *mob, FRPoint *rPos,
                        float radius, float weight);

static void
FlockFleetFindEnemies(AIContext *aic,
                      const FlockFleetConfig *ffc,
                      Mob *mob, FRPoint *rPos,
                      float radius, float weight);

static void
FlockFleetFindCores(AIContext *aic,
                    const FlockFleetConfig *ffc,
                    Mob *mob, FRPoint *rPos, float radius, float weight);

static void FlockFleetFindLocus(AIContext *aic,
                                const FlockFleetConfig *ffc,
                                FlockFleetLiveState *ffls,
                                Mob *mob, FRPoint *rPos);

class FlockAIGovernor : public BasicAIGovernor
{
public:
    FlockAIGovernor(FleetAI *ai, SensorGrid *sg)
    :BasicAIGovernor(ai, sg) { }

    virtual ~FlockAIGovernor() { }

    virtual void putDefaults(MBRegistry *mreg, FleetAIType flockType) {
        FlockConfigValue defaults[] = {
            { "randomIdle",           "TRUE",       },
            { "alwaysFlock",          "FALSE",      },
            { "baseSpawnJitter",        "1",        },

            { "flockRadius",          "166.7",      },
            { "flockCrowding",        "2.0",        },
            { "alignWeight",          "0.2",        },
            { "cohereWeight",         "-0.1",       },
            { "brokenCohere",         "FALSE",      },

            { "separateRadius",       "50.0",       },
            { "separatePeriod",       "0.0",        },
            { "separateScale",        "50.0",       },
            { "separateWeight",       "0.2",        },

            { "edgeRadius",           "100.0",      },
            { "edgesWeight",          "0.9",        },
            { "centerRadius",         "0.0",        },
            { "centerWeight",         "0.0",        },

            { "coresRadius",          "166.7",      },
            { "coresWeight",          "0.1",        },
            { "coresCrowdRadius",     "166.7",      },
            { "coresCrowding",        "5",          },

            { "baseRadius",           "100",        },
            { "baseWeight",           "0.0",        },
            { "nearBaseRadius",       "250.0",      },
            { "baseDefenseRadius",    "250.0",      },

            { "enemyRadius",          "166.7",      },
            { "enemyWeight",          "0.3",        },
            { "enemyCrowdRadius",     "166.7",      },
            { "enemyCrowding",        "5",          },

            { "enemyBaseRadius",      "100",        },
            { "enemyBaseWeight",      "0.0",        },

            { "curHeadingWeight",     "0.5",        },

            { "attackSeparateRadius", "166.7",      },
            { "attackSeparateWeight", "0.5",        },

            { "locusRadius",          "10000.0",    },
            { "locusWeight",          "0.0",        },
            { "locusCircularPeriod",  "1000.0",     },
            { "locusCircularWeight",  "0.0",        },
            { "locusLinearXPeriod",   "1000.0",     },
            { "locusLinearYPeriod",   "1000.0",     },
            { "locusLinearWeight",    "0.0",        },
            { "locusRandomWeight",    "0.0",        },
            { "locusRandomPeriod",    "1000.0",     },
            { "useScaledLocus",       "TRUE",       },
        };

        FlockConfigValue configs1[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE", },
            { "gatherRange",          "100",  },
            { "attackRange",          "250",  },

            // FlockFleet specific options
            { "flockRadius",          "166.7",      }, // baseSensorRadius / 1.5
            { "alignWeight",          "0.2",        },
            { "cohereWeight",         "-0.1",       },
            { "brokenCohere",         "TRUE",       },

            { "separateRadius",       "50.0",       }, // 2 * fighterSensorRadius
            { "separateWeight",       "0.2",        },

            { "edgeRadius",           "100.0",      }, // fighterSensorRadius
            { "edgesWeight",          "0.9",        },

            { "coresRadius",          "166.7",      },
            { "coresWeight",          "0.1",        },
            { "coresCrowdRadius",     "166.7",      },
            { "coresCrowding",        "5",          },

            { "enemyRadius",          "166.7",      },
            { "enemyWeight",          "0.3",        },
            { "enemyCrowdRadius",     "166.7",      },
            { "enemyCrowding",        "5",          },

            { "curHeadingWeight",     "0.5",        },

            { "attackSeparateRadius", "166.7",      },
            { "attackSeparateWeight", "0.5",        },
        };

        FlockConfigValue configs2[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "68.465767",  },
            { "attackRange",          "32.886688",  },

            // FlockFleet specific options
            { "flockRadius",          "398.545197", },
            { "alignWeight",          "0.239648",   },
            { "cohereWeight",         "-0.006502",  },
            { "brokenCohere",         "TRUE",       },

            { "separateRadius",       "121.312904", },
            { "separateWeight",       "0.781240",   },

            { "edgeRadius",           "161.593430", },
            { "edgesWeight",          "0.704170",   },
            { "centerRadius",         "0.0",        },
            { "centerWeight",         "0.0",        },

            { "coresRadius",          "398.545197", },
            { "coresWeight",          "0.122679",   },
            { "coresCrowdRadius",     "398.545197", },
            { "coresCrowding",        "5.0",        },

            { "enemyRadius",          "398.545197", },
            { "enemyWeight",          "0.556688",   },
            { "enemyCrowdRadius",     "398.545197", },
            { "enemyCrowding",        "5",          },

            { "curHeadingWeight",     "0.838760",   },

            { "attackSeparateRadius", "398.545197", },
            { "attackSeparateWeight", "0.188134",   },
        };

        FlockConfigValue configs3[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "61",         },
            { "attackRange",          "13.183991",  },
            { "guardRange",           "82.598732",  },
            { "evadeStrictDistance",  "25",         },
            { "attackExtendedRange",  "TRUE",       },
            { "evadeRange",           "485",        },
            { "evadeUseStrictDistance", "TRUE",     },
            { "rotateStartingAngle", "TRUE",        },

            // FlockFleet specific options
            { "flockRadius",          "338",        },
            { "alignWeight",          "0.000000",   },
            { "cohereWeight",         "-0.233058",  },
            { "brokenCohere",         "TRUE",       },

            { "separateRadius",       "121.312904", },
            { "separateWeight",       "0.781240",   },

            { "edgeRadius",           "10.0",       },
            { "edgesWeight",          "0.10",       },

            { "coresRadius",          "1.000000",   },
            { "coresWeight",          "0.0",        },
            { "coresCrowdRadius",     "1.000000",   },
            { "coresCrowding",        "2.0",        },

            { "baseRadius",           "54.0",       },
            { "baseWeight",           "-0.589485",  },
            { "nearBaseRadius",       "8.000000",   },
            { "baseDefenseRadius",    "64.0",       },

            { "enemyRadius",          "398.545197", },
            { "enemyWeight",          "0.931404",   },
            { "enemyCrowdRadius",     "398.545197", },
            { "enemyCrowding",        "5",          },

            { "enemyBaseRadius",      "103",        },
            { "enemyBaseWeight",      "0.000000",   },

            { "curHeadingWeight",     "0.838760",   },

            { "attackSeparateRadius", "8.000000",   },
            { "attackSeparateWeight", "0.0",        },
        };

        FlockConfigValue configs4[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "61",         },
            { "attackRange",          "50.625603",  },
            { "guardRange",           "2.148767",   },
            { "evadeStrictDistance",  "20.359625",  },
            { "attackExtendedRange",  "TRUE",       },
            { "evadeRange",           "25.040209",  },
            { "evadeUseStrictDistance", "TRUE",     },
            { "rotateStartingAngle", "TRUE",        },

            // FlockFleet specific options
            { "flockRadius",          "129.883743", },
            { "alignWeight",          "0.295573",   },
            { "cohereWeight",         "-0.097492",  },
            { "brokenCohere",         "TRUE",       },

            { "separateRadius",       "121.312904", },
            { "separateWeight",       "0.781240",   },

            { "edgeRadius",           "23.606379",  },
            { "edgesWeight",          "0.958569",   },

            { "coresRadius",          "93.769035",  },
            { "coresWeight",          "0.210546",   },
            { "coresCrowdRadius",     "93.769035",  },
            { "coresCrowding",        "7.429844",   },

            { "baseRadius",           "38.207771",  },
            { "baseWeight",           "0.181976",   },
            { "nearBaseRadius",       "53.931396",  },
            { "baseDefenseRadius",    "49.061054",  },

            { "enemyRadius",          "398.545197", },
            { "enemyWeight",          "0.931404",   },
            { "enemyCrowdRadius",     "398.545197", },
            { "enemyCrowding",        "5",          },

            { "enemyBaseRadius",      "10.000000",  },
            { "enemyBaseWeight",      "-0.950000",  },

            { "curHeadingWeight",     "0.215320",   },

            { "attackSeparateRadius", "26.184313",  },
            { "attackSeparateWeight", "-0.942996",  },
        };

        FlockConfigValue configs5[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "FALSE",      },
            { "gatherRange",          "61",         },
            { "attackRange",          "50.903362",  },
            { "guardRange",           "-0.528344",  },
            { "evadeStrictDistance",  "3.119897",   },
            { "attackExtendedRange",  "TRUE",       },
            { "evadeRange",           "72.195099",  },
            { "evadeUseStrictDistance", "TRUE",     },
            { "rotateStartingAngle", "TRUE",        },

            // FlockFleet specific options
            { "flockRadius",          "136.132584", },
            { "alignWeight",          "0.193725",   },
            { "cohereWeight",         "-0.365141",  },
            { "brokenCohere",         "TRUE",       },

            { "separateRadius",       "121.312904", },
            { "separateWeight",       "0.781240",   },

            { "edgeRadius",           "117.935951", },
            { "edgesWeight",          "0.008065",   },
            { "centerRadius",         "45.782734",  },
            { "centerWeight",         "0.613753",   },

            { "coresRadius",          "134.762024", },
            { "coresWeight",          "0.239872",   },
            { "coresCrowdRadius",     "0.000000",   },
            { "coresCrowding",        "18.770977",  },

            { "baseRadius",           "391.563629", },
            { "baseWeight",           "-0.319866",  },
            { "nearBaseRadius",       "1.102500",   },
            { "baseDefenseRadius",    "66.977211",  },

            { "enemyRadius",          "0.000000",   },
            { "enemyWeight",          "0.936234",   },
            { "enemyCrowdRadius",     "0.000000",   },
            { "enemyCrowding",        "-0.041383",  },

            { "enemyBaseRadius",      "43.751724",  },
            { "enemyBaseWeight",      "0.096284",   },

            { "curHeadingWeight",     "0.987313",   },

            { "attackSeparateRadius", "451.420227", },
            { "attackSeparateWeight", "-1.000000",  },
        };

        FlockConfigValue configs6[] = {
            // Override BasicFleet defaults
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "63.598724",  },
            { "attackRange",          "468.731812", },
            { "guardRange",           "-1.000000",  },
            { "evadeStrictDistance",  "4.275044",   },
            { "attackExtendedRange",  "FALSE",      },
            { "evadeRange",           "181.451782", },
            { "evadeUseStrictDistance", "TRUE",     },
            { "rotateStartingAngle",  "TRUE",       },
            { "sensorGrid.staleFighterTime", "2.904688"  },
            { "sensorGrid.staleCoreTime",    "71.208900" },

            // FlockFleet specific options
            { "randomIdle",           "TRUE",       },
            { "alwaysFlock",          "TRUE",       },
            { "flockRadius",          "97.054489",  },
            { "flockCrowding",        "2.269907",   },
            { "alignWeight",          "-0.355190",  },
            { "cohereWeight",         "-0.356305",  },
            { "brokenCohere",         "TRUE",       },

            { "separateRadius",       "129.375519", },
            { "separatePeriod",       "104.161858", },
            { "separateScale",        "57.952076",  },
            { "separateWeight",       "0.782420",   },

            { "edgeRadius",           "27.186251",  },
            { "edgesWeight",          "0.742008",   },
            { "centerRadius",         "341.787628", },
            { "centerWeight",         "0.094766",   },

            { "coresRadius",          "579.377625", },
            { "coresWeight",          "0.012672",   },
            { "coresCrowdRadius",     "822.282104", },
            { "coresCrowding",        "7.761457",   },

            { "baseRadius",           "364.446167", },
            { "baseWeight",           "-0.578069",  },
            { "nearBaseRadius",       "31.823872",  },
            { "baseDefenseRadius",    "64.155891",  },

            { "enemyRadius",          "335.253326", },
            { "enemyWeight",          "0.893276",   },
            { "enemyCrowdRadius",     "178.703293", },
            { "enemyCrowding",        "2.050628",   },

            { "enemyBaseRadius",      "46.037949",  },
            { "enemyBaseWeight",      "-0.692255",  },

            { "curHeadingWeight",     "1.000000",   },

            { "attackSeparateRadius", "3.158908",   },
            { "attackSeparateWeight", "-0.846666",  },

            { "locusRadius",          "1.050000",   },
            { "locusWeight",          "0.796089",   },
            { "locusCircularPeriod",  "1986.383179",},
            { "locusCircularWeight",  "-0.623963",  },
            { "locusLinearXPeriod",   "4605.293945",},
            { "locusLinearYPeriod",   "9429.933594",},
            { "locusLinearWeight",    "-0.002683",  },
            { "useScaledLocus",       "FALSE",      },
        };

        FlockConfigValue configs7[] = {
            { "alignWeight",          "-0.070892",  },
            { "alwaysFlock",          "TRUE",       },
            { "attackExtendedRange",  "FALSE",      },
            { "attackRange",          "578.199402", },
            { "attackSeparateRadius", "1.102500",   },
            { "attackSeparateWeight", "1.000000",   },
            { "baseDefenseRadius",    "67.363686",  },
            { "baseRadius",           "339.388031", },
            { "baseSpawnJitter",      "1.000000",   },
            { "baseWeight",           "-0.585778",  },
            { "brokenCohere",         "TRUE",       },
            { "centerRadius",         "432.775909", },
            { "centerWeight",         "0.090749",   },
            { "cohereWeight",         "-0.063437",  },
            { "coresCrowding",        "11.318711",  },
            { "coresCrowdRadius",     "809.355225", },
            { "coresRadius",          "579.820801", },
            { "coresWeight",          "0.113382",   },
            { "creditReserve",        "104.999992", },
            { "curHeadingWeight",     "0.857375",   },
            { "edgeRadius",           "25.161718",  },
            { "edgesWeight",          "0.296447",   },
            { "enemyBaseRadius",      "85.485863",  },
            { "enemyBaseWeight",      "-0.619157",  },
            { "enemyCrowding",        "2.432107",   },
            { "enemyCrowdRadius",     "143.273010", },
            { "enemyRadius",          "278.176453", },
            { "enemyWeight",          "0.998551",   },
            { "evadeFighters",        "FALSE",      },
            { "evadeRange",           "95.060516",  },
            { "evadeStrictDistance",  "3.848581",   },
            { "evadeUseStrictDistance", "FALSE",    },
            { "flockCrowding",        "2.960368",   },
            { "flockRadius",          "106.468208", },
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "53.329815",  },
            { "guardRange",           "22.143234",  },
            { "locusCircularPeriod",  "6650.758301",},
            { "locusCircularWeight",  "0.581691",   },
            { "locusLinearWeight",    "0.624552",   },
            { "locusLinearXPeriod",   "5635.052734",},
            { "locusLinearYPeriod",   "2768.862061",},
            { "locusRadius",          "70.071892",  },
            { "locusWeight",          "0.026167",   },
            { "nearBaseRadius",       "36.932438",  },
            { "randomIdle",           "TRUE",       },
            { "rotateStartingAngle",  "TRUE",       },
            { "sensorGrid.staleCoreTime",    "78.830215" },
            { "sensorGrid.staleFighterTime", "9.505237"  },
            { "separatePeriod",       "0.000000",   },
            { "separateRadius",       "119.961555", },
            { "separateScale",        "0.000000",   },
            { "separateWeight",       "0.950000",   },
            { "useScaledLocus",       "FALSE",      },
        };

        FlockConfigValue configs8[] = {
            { "alignWeight",          "0.941104",   },
            { "alwaysFlock",          "TRUE",       },
            { "attackExtendedRange",  "FALSE",      },
            { "attackRange",          "-0.950000",  },
            { "attackSeparateRadius", "5.244739",   },
            { "attackSeparateWeight", "1.000000",   },
            { "baseDefenseRadius",    "7.765651",   },
            { "baseRadius",           "251.561218", },
            { "baseSpawnJitter",      "1.050000",   },
            { "baseWeight",           "-0.594068",  },
            { "brokenCohere",         "TRUE",       },
            { "centerRadius",         "0.000000",   },
            { "centerWeight",         "-0.049964",  },
            { "cohereWeight",         "0.111733",   },
            { "coresCrowding",        "10.063643",  },
            { "coresCrowdRadius",     "446.784180", },
            { "coresRadius",          "561.107605", },
            { "coresWeight",          "0.270990",   },
            { "creditReserve",        "149.861420", },
            { "curHeadingWeight",     "0.608142",   },
            { "edgeRadius",           "24.759119",  },
            { "edgesWeight",          "0.753383",   },
            { "enemyBaseRadius",      "190.747162", },
            { "enemyBaseWeight",      "-0.268014",  },
            { "enemyCrowding",        "8.292590",   },
            { "enemyCrowdRadius",     "737.966675", },
            { "enemyRadius",          "469.026489", },
            { "enemyWeight",          "0.827751",   },
            { "evadeFighters",        "FALSE",      },
            { "evadeRange",           "384.699890", },
            { "evadeStrictDistance",  "268.416046", },
            { "evadeUseStrictDistance", "FALSE",    },
            { "flockCrowding",        "2.360445",   },
            { "flockRadius",          "110.022324", },
            { "gatherAbandonStale",   "FALSE",      },
            { "gatherRange",          "11.025000",  },
            { "guardRange",           "-0.950000",  },
            { "locusCircularPeriod",  "9389.412109",},
            { "locusCircularWeight",  "-0.191549",  },
            { "locusLinearWeight",    "0.024249",   },
            { "locusLinearXPeriod",   "4819.627441",},
            { "locusLinearYPeriod",   "4481.782227",},
            { "locusRadius",          "1.000000",   },
            { "locusWeight",          "-0.181500",  },
            { "nearBaseRadius",       "58.276283",  },
            { "randomIdle",           "TRUE",       },
            { "rotateStartingAngle",  "TRUE",       },
            { "sensorGrid.staleCoreTime",    "53.971874" },
            { "sensorGrid.staleFighterTime", "5.159447"  },
            { "separatePeriod",       "198.535645", },
            { "separateRadius",       "117.649010", },
            { "separateScale",        "0.000000",   },
            { "separateWeight",       "0.902500",   },
            { "useScaledLocus",       "FALSE",      },
        };

        FlockConfigValue configs9[] = {
            { "alignWeight",          "1.000000",   },
            { "alwaysFlock",          "TRUE",       },
            { "attackExtendedRange",  "FALSE",      },
            { "attackRange",          "36.357330",  },
            { "attackSeparateRadius", "116.610649", },
            { "attackSeparateWeight", "-0.846049",  },
            { "baseDefenseRadius",    "1.102500",   },
            { "baseRadius",           "292.362305", },
            { "baseSpawnJitter",      "1.000000",   },
            { "baseWeight",           "-0.328720",  },
            { "brokenCohere",         "TRUE",       },
            { "centerRadius",         "761.465576", },
            { "centerWeight",         "-0.048965",  },
            { "cohereWeight",         "0.048618",   },
            { "coresCrowding",        "4.913648",   },
            { "coresCrowdRadius",     "135.280548", },
            { "coresRadius",          "776.426697", },
            { "coresWeight",          "0.197949",   },
            { "creditReserve",        "120.438179", },
            { "curHeadingWeight",     "0.499466",   },
            { "edgeRadius",           "26.930847",  },
            { "edgesWeight",          "0.482821",   },
            { "enemyBaseRadius",      "224.461044", },
            { "enemyBaseWeight",      "0.633770",   },
            { "enemyCrowding",        "9.255432",   },
            { "enemyCrowdRadius",     "728.962708", },
            { "enemyRadius",          "261.936279", },
            { "enemyWeight",          "0.518455",   },
            { "evadeFighters",        "FALSE",      },
            { "evadeRange",           "246.765274", },
            { "evadeStrictDistance",  "2.582255",   },
            { "evadeUseStrictDistance", "TRUE",     },
            { "flockCrowding",        "2.705287",   },
            { "flockRadius",          "105.816391", },
            { "gatherAbandonStale",   "TRUE",       },
            { "gatherRange",          "25.859146",  },
            { "guardRange",           "23.338100",  },
            { "locusCircularPeriod",  "9653.471680",},
            { "locusCircularWeight",  "-0.779813",  },
            { "locusLinearWeight",    "-0.803491",  },
            { "locusLinearXPeriod",   "7472.032227",},
            { "locusLinearYPeriod",   "8851.404297",},
            { "locusRadius",          "104.198990", },
            { "locusWeight",          "-0.655256",  },
            { "nearBaseRadius",       "10.077254",  },
            { "randomIdle",           "TRUE",       },
            { "rotateStartingAngle",  "FALSE",      },
            { "sensorGrid.staleCoreTime",    "28.385160" },
            { "sensorGrid.staleFighterTime", "16.703636" },
            { "separatePeriod",       "1543.553345",},
            { "separateRadius",       "105.912781", },
            { "separateScale",        "0.000000",   },
            { "separateWeight",       "0.839316",   },
            { "useScaledLocus",       "FALSE",      },
        };

        FlockConfigValue *configDefaults;
        uint configDefaultsSize;

        if (flockType == FLEET_AI_FLOCK1) {
            configDefaults = configs1;
            configDefaultsSize = ARRAYSIZE(configs1);
        } else if (flockType == FLEET_AI_FLOCK2) {
            configDefaults = configs2;
            configDefaultsSize = ARRAYSIZE(configs2);
        } else if (flockType == FLEET_AI_FLOCK3) {
            configDefaults = configs3;
            configDefaultsSize = ARRAYSIZE(configs3);
        } else if (flockType == FLEET_AI_FLOCK4) {
            configDefaults = configs4;
            configDefaultsSize = ARRAYSIZE(configs4);
        } else if (flockType == FLEET_AI_FLOCK5) {
            configDefaults = configs5;
            configDefaultsSize = ARRAYSIZE(configs5);
        } else if (flockType == FLEET_AI_FLOCK6) {
            configDefaults = configs6;
            configDefaultsSize = ARRAYSIZE(configs6);
        } else if (flockType == FLEET_AI_FLOCK7) {
            configDefaults = configs7;
            configDefaultsSize = ARRAYSIZE(configs7);
        } else if (flockType == FLEET_AI_FLOCK8) {
            configDefaults = configs8;
            configDefaultsSize = ARRAYSIZE(configs8);
        } else if (flockType == FLEET_AI_FLOCK9) {
            configDefaults = configs9;
            configDefaultsSize = ARRAYSIZE(configs9);
        } else {
            PANIC("Unknown aiType: %d\n", flockType);
        }

        for (uint i = 0; i < configDefaultsSize; i++) {
            if (configDefaults[i].value != NULL &&
                !MBRegistry_ContainsKey(mreg, configDefaults[i].key)) {
                MBRegistry_PutConst(mreg, configDefaults[i].key, configDefaults[i].value);
            }
        }

        for (uint i = 0; i < ARRAYSIZE(defaults); i++) {
            if (defaults[i].value != NULL &&
                !MBRegistry_ContainsKey(mreg, defaults[i].key)) {
                MBRegistry_PutConst(mreg, defaults[i].key, defaults[i].value);
            }
        }
    }


    virtual void loadRegistry(MBRegistry *mreg) {
        this->myConfig.randomIdle = MBRegistry_GetBool(mreg, "randomIdle");
        this->myConfig.alwaysFlock = MBRegistry_GetBool(mreg, "alwaysFlock");

        this->myConfig.flockRadius = MBRegistry_GetFloat(mreg, "flockRadius");
        this->myConfig.flockCrowding = (uint)MBRegistry_GetFloat(mreg, "flockCrowding");
        this->myConfig.alignWeight = MBRegistry_GetFloat(mreg, "alignWeight");
        this->myConfig.cohereWeight = MBRegistry_GetFloat(mreg, "cohereWeight");
        this->myConfig.brokenCohere = MBRegistry_GetBool(mreg, "brokenCohere");

        this->myConfig.separateRadius = MBRegistry_GetFloat(mreg, "separateRadius");
        this->myConfig.separatePeriod = MBRegistry_GetFloat(mreg, "separatePeriod");
        this->myConfig.separateScale = MBRegistry_GetFloat(mreg, "separateScale");
        this->myConfig.separateWeight = MBRegistry_GetFloat(mreg, "separateWeight");

        this->myConfig.edgeRadius = MBRegistry_GetFloat(mreg, "edgeRadius");
        this->myConfig.edgesWeight = MBRegistry_GetFloat(mreg, "edgesWeight");
        this->myConfig.centerRadius = MBRegistry_GetFloat(mreg, "centerRadius");
        this->myConfig.centerWeight = MBRegistry_GetFloat(mreg, "centerWeight");

        this->myConfig.coresRadius = MBRegistry_GetFloat(mreg, "coresRadius");
        this->myConfig.coresWeight = MBRegistry_GetFloat(mreg, "coresWeight");
        this->myConfig.coresCrowdRadius = MBRegistry_GetFloat(mreg, "coresCrowdRadius");
        this->myConfig.coresCrowding = (uint)MBRegistry_GetFloat(mreg, "coresCrowding");

        this->myConfig.baseRadius = MBRegistry_GetFloat(mreg, "baseRadius");
        this->myConfig.baseWeight = MBRegistry_GetFloat(mreg, "baseWeight");
        this->myConfig.nearBaseRadius = MBRegistry_GetFloat(mreg, "nearBaseRadius");
        this->myConfig.baseDefenseRadius = MBRegistry_GetFloat(mreg, "baseDefenseRadius");

        this->myConfig.enemyRadius = MBRegistry_GetFloat(mreg, "enemyRadius");
        this->myConfig.enemyWeight = MBRegistry_GetFloat(mreg, "enemyWeight");
        this->myConfig.enemyCrowdRadius = MBRegistry_GetFloat(mreg, "enemyCrowdRadius");
        this->myConfig.enemyCrowding = (uint)MBRegistry_GetFloat(mreg, "enemyCrowding");

        this->myConfig.enemyBaseRadius = MBRegistry_GetFloat(mreg, "enemyBaseRadius");
        this->myConfig.enemyBaseWeight = MBRegistry_GetFloat(mreg, "enemyBaseWeight");

        this->myConfig.curHeadingWeight =
            MBRegistry_GetFloat(mreg, "curHeadingWeight");

        this->myConfig.attackSeparateRadius =
            MBRegistry_GetFloat(mreg, "attackSeparateRadius");
        this->myConfig.attackSeparateWeight =
            MBRegistry_GetFloat(mreg, "attackSeparateWeight");

        this->myConfig.locusRadius =
            MBRegistry_GetFloat(mreg, "locusRadius");
        this->myConfig.locusWeight =
            MBRegistry_GetFloat(mreg, "locusWeight");
        this->myConfig.locusCircularPeriod =
            MBRegistry_GetFloat(mreg, "locusCircularPeriod");
        this->myConfig.locusCircularWeight =
            MBRegistry_GetFloat(mreg, "locusCircularWeight");
        this->myConfig.locusLinearXPeriod =
            MBRegistry_GetFloat(mreg, "locusLinearXPeriod");
        this->myConfig.locusLinearYPeriod =
            MBRegistry_GetFloat(mreg, "locusLinearYPeriod");
        this->myConfig.locusLinearWeight =
            MBRegistry_GetFloat(mreg, "locusLinearWeight");
        this->myConfig.useScaledLocus =
            MBRegistry_GetFloat(mreg, "useScaledLocus");

        this->myConfig.locusRandomWeight =
            MBRegistry_GetFloat(mreg, "locusRandomWeight");
        this->myConfig.locusRandomPeriod =
            (uint)MBRegistry_GetFloat(mreg, "locusRandomPeriod");

        this->BasicAIGovernor::loadRegistry(mreg);
    }

    virtual void doAttack(Mob *mob, Mob *enemyTarget) {
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        BasicAIGovernor::doAttack(mob, enemyTarget);
        FRPoint rPos;
        AIContext aic;

        aic.rs = &myRandomState;
        aic.sg = (MappingSensorGrid *)mySensorGrid;
        aic.ai = myFleetAI;

        FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

        FlockFleetSeparate(&aic, mob, &rPos, myConfig.attackSeparateRadius,
                           myConfig.attackSeparateWeight);

        rPos.radius = speed;
        FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
    }

    virtual void doIdle(Mob *mob, bool newlyIdle) {
        BasicShipAI *ship;
        AIContext aic;

        aic.rs = &myRandomState;
        aic.sg = (MappingSensorGrid *)mySensorGrid;
        aic.ai = myFleetAI;

        ship = (BasicShipAI *)getShip(mob->mobid);
        ASSERT(ship != NULL);
        ship->state = BSAI_STATE_IDLE;

        if (mob->type != MOB_TYPE_FIGHTER) {
            BasicAIGovernor::doIdle(mob, newlyIdle);
            return;
        }

        FlockFleet_DoIdle(&aic, &myConfig, &myLive, mob, newlyIdle);
    }

    virtual void runTick() {
        SensorGrid *sg = mySensorGrid;

        if (myConfig.separatePeriod > 0.0f &&
            myConfig.separateScale > 0.0f) {
            float p = myConfig.separatePeriod;
            float s = myConfig.separateScale;
            myLive.separateRadius = myConfig.separateRadius +
                                   s * fabs(sinf(myFleetAI->tick / p));
        } else {
            myLive.separateRadius = myConfig.separateRadius;
        }

        BasicAIGovernor::runTick();

        Mob *base = sg->friendBase();

        if (base != NULL) {
            MBVector<Mob *> fv;
            MBVector<Mob *> tv;
            int f = 0;
            int t = 0;
            int fNum = 0;
            int fMin;
            Mob *fighter = NULL;

            CMBComparator comp;
            MobP_InitDistanceComparator(&comp, &base->pos);

            sg->pushClosestTargetsInRange(tv, MOB_FLAG_SHIP, &base->pos,
                                          myConfig.baseDefenseRadius);

            if (tv.size() > 0) {
                sg->pushFriends(fv, MOB_FLAG_FIGHTER);

                fNum = fv.size();
                fMin = fv.findMin(MBComparator<Mob *>(&comp), f, fNum);
                fighter = fMin >= 0 ? fv[fMin] : NULL;
                if (fighter != NULL) {
                    fv[fMin] = fv[0];
                    f++;
                    fNum--;
                }
            }

            Mob *target = (t < tv.size()) ? tv[t++] : NULL;

            while (target != NULL && fighter != NULL) {
                BasicShipAI *ship = (BasicShipAI *)getShip(fighter->mobid);

                ship->attack(target);

                fMin = fv.findMin(MBComparator<Mob *>(&comp), f, fNum);
                fighter = fMin >= 0 ? fv[fMin] : NULL;
                if (fighter != NULL) {
                    fv[fMin] = fv[0];
                    f++;
                    fNum--;
                    ASSERT(fNum >= 0);
                }

                target = (t < tv.size()) ? tv[t++] : NULL;
            }
        }
    }

    virtual void runMob(Mob *mob) {
        BasicAIGovernor::runMob(mob);
    }

    FlockFleetConfig myConfig;
    FlockFleetLiveState myLive;
};

class FlockFleet {
public:
    FlockFleet(FleetAI *ai)
    :sg(), gov(ai, &sg)
    {
        this->ai = ai;
        RandomState_CreateWithSeed(&this->rs, ai->seed);
        gov.setSeed(RandomState_Uint64(&this->rs));

        mreg = MBRegistry_AllocCopy(ai->player.mreg);

        this->gov.putDefaults(mreg, ai->player.aiType);
        this->gov.loadRegistry(mreg);
    }

    ~FlockFleet() {
        RandomState_Destroy(&this->rs);
        MBRegistry_Free(mreg);
    }

    FleetAI *ai;
    RandomState rs;
    SensorGrid sg;
    FlockAIGovernor gov;
    MBRegistry *mreg;
};

static void *FlockFleetCreate(FleetAI *ai);
static void FlockFleetDestroy(void *aiHandle);
static void FlockFleetRunAITick(void *aiHandle);
static void *FlockFleetMobSpawned(void *aiHandle, Mob *m);
static void FlockFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle);
static void FlockFleetMutate(FleetAIType aiType, MBRegistry *mreg);

void FlockFleet_GetOps(FleetAIType aiType, FleetAIOps *ops)
{
    ASSERT(ops != NULL);
    MBUtil_Zero(ops, sizeof(*ops));

    if (aiType == FLEET_AI_FLOCK1) {
        ops->aiName = "FlockFleet1";
    } else if (aiType == FLEET_AI_FLOCK2) {
        ops->aiName = "FlockFleet2";
    } else if (aiType == FLEET_AI_FLOCK3) {
        ops->aiName = "FlockFleet3";
    } else if (aiType == FLEET_AI_FLOCK4) {
        ops->aiName = "FlockFleet4";
    } else if (aiType == FLEET_AI_FLOCK5) {
        ops->aiName = "FlockFleet5";
    } else if (aiType == FLEET_AI_FLOCK6) {
        ops->aiName = "FlockFleet6";
    } else if (aiType == FLEET_AI_FLOCK7) {
        ops->aiName = "FlockFleet7";
    } else if (aiType == FLEET_AI_FLOCK8) {
        ops->aiName = "FlockFleet8";
    } else if (aiType == FLEET_AI_FLOCK9) {
        ops->aiName = "FlockFleet9";
    } else {
        NOT_IMPLEMENTED();
    }

    ops->aiAuthor = "Michael Banack";

    ops->createFleet = &FlockFleetCreate;
    ops->destroyFleet = &FlockFleetDestroy;
    ops->runAITick = &FlockFleetRunAITick;
    ops->mobSpawned = &FlockFleetMobSpawned;
    ops->mobDestroyed = &FlockFleetMobDestroyed;
    ops->mutateParams = &FlockFleetMutate;
}

static void FlockFleetMutate(FleetAIType aiType, MBRegistry *mreg)
{
    MutationFloatParams vf[] = {
        // key                     min    max      mag   jump   mutation
        { "gatherRange",          10.0f, 500.0f,  0.05f, 0.15f, 0.02f},
        { "evadeStrictDistance",  -1.0f, 500.0f,  0.05f, 0.15f, 0.02f},
        { "evadeRange",           -1.0f, 500.0f,  0.05f, 0.15f, 0.02f},
        { "attackRange",          -1.0f, 700.0f,  0.05f, 0.15f, 0.02f},
        { "guardRange",           -1.0f, 500.0f,  0.05f, 0.15f, 0.02f},

        { "creditReserve",       100.0f,1000.0f,  0.05f, 0.15f, 0.005f},
        { "baseSpawnJitter",       1.0f, 100.0f,  0.05f, 0.15f, 0.005f},
        { "fighterFireJitter",     1.0f,  10.0f,  0.05f, 0.15f, 0.005f},

        { "sensorGrid.staleCoreTime",
                                    0.0f, 100.0f,  0.05f, 0.15f, 0.02f},
        { "sensorGrid.staleFighterTime",
                                    0.0f, 100.0f,  0.05f, 0.15f, 0.02f},

        { "flockRadius",          10.0f, 500.0f, 0.05f, 0.15f, 0.02f},
        { "flockCrowding",         0.0f,  20.0f, 0.05f, 0.15f, 0.02f},
        { "alignWeight",          -1.0f,   1.0f, 0.05f, 0.15f, 0.02f},
        { "cohereWeight",         -1.0f,   1.0f, 0.05f, 0.15f, 0.02f},

        { "separateRadius",        5.0f, 500.0f, 0.05f, 0.15f, 0.02f},
        { "separatePeriod",        0.0f,2000.0f, 0.05f, 0.15f, 0.02f},
        { "separateScale",         0.0f, 500.0f, 0.05f, 0.15f, 0.02f},
        { "separateWeight",       -1.0f,   1.0f, 0.05f, 0.15f, 0.02f},

        { "edgeRadius",            1.0f, 500.0f, 0.05f, 0.15f, 0.02f},
        { "edgesWeight",          -0.2f,   1.0f, 0.05f, 0.15f, 0.02f},
        { "centerRadius",          0.0f, 900.0f, 0.05f, 0.15f, 0.02f},
        { "centerWeight",         -0.1f,   0.1f, 0.05f, 0.15f, 0.01f},

        { "coresRadius",           0.0f, 900.0f, 0.05f, 0.15f, 0.02f},
        { "coresWeight",          -1.0f,   1.0f, 0.05f, 0.15f, 0.02f},
        { "coresCrowdRadius",      0.0f, 900.0f, 0.05f, 0.15f, 0.005f},
        { "coresCrowding",        -1.0f,  20.0f, 0.05f, 0.15f, 0.005f},

        { "baseRadius",           10.0f, 500.0f, 0.05f, 0.15f, 0.01f},
        { "baseWeight",           -1.0f,   0.3f, 0.05f, 0.15f, 0.01f},
        { "nearBaseRadius",        1.0f, 500.0f, 0.05f, 0.15f, 0.01f},
        { "baseDefenseRadius",     1.0f, 500.0f, 0.05f, 0.15f, 0.01f},

        { "enemyRadius",           0.0f, 900.0f, 0.05f, 0.15f, 0.02f},
        { "enemyWeight",          -1.0f,   1.0f, 0.05f, 0.15f, 0.02f},
        { "enemyCrowdRadius",      0.0f, 900.0f, 0.05f, 0.15f, 0.01f},
        { "enemyCrowding",        -1.0f,  20.0f, 0.05f, 0.15f, 0.01f},

        { "enemyBaseRadius",       0.0f, 900.0f, 0.05f, 0.15f, 0.01f},
        { "enemyBaseWeight",      -1.0f,   1.0f, 0.05f, 0.15f, 0.01f},

        { "curHeadingWeight",     -1.0f,   2.0f, 0.05f, 0.15f, 0.02f},

        { "attackSeparateRadius",  1.0f, 500.0f, 0.05f, 0.15f, 0.02f},
        { "attackSeparateWeight", -1.0f,   1.0f, 0.05f, 0.15f, 0.02f},

        { "locusRadius",          1.0f, 12345.0f, 0.05f, 0.15f, 0.02f},
        { "locusWeight",         -1.0f,    1.0f,  0.05f, 0.15f, 0.02f},
        { "locusCircularPeriod", -1.0f, 12345.0f, 0.05f, 0.15f, 0.02f},
        { "locusCircularWeight",  0.0f,    2.0f,  0.05f, 0.15f, 0.02f},
        { "locusLinearXPeriod",  -1.0f, 12345.0f, 0.05f, 0.15f, 0.02f},
        { "locusLinearYPeriod",  -1.0f, 12345.0f, 0.05f, 0.15f, 0.02f},
        { "locusLinearWeight",    0.0f,    2.0f,  0.05f, 0.15f, 0.02f},
        { "locusRandomWeight",    0.0f,    2.0f,  0.05f, 0.15f, 0.02f},
        { "locusRandomPeriod",   -1.0f, 12345.0f, 0.05f, 0.15f, 0.02f},
    };

    MutationBoolParams vb[] = {
        // key                       mutation
        { "evadeFighters",           0.01f},
        { "evadeUseStrictDistance",  0.01f},
        { "attackExtendedRange",     0.01f},
        { "rotateStartingAngle",     0.01f},
        { "gatherAbandonStale",      0.01f},
        { "alwaysFlock",             0.01f},
        { "randomIdle",              0.01f},
        { "brokenCohere",            0.01f},
        { "useScaledLocus",          0.01f},
    };

    Mutate_Float(mreg, vf, ARRAYSIZE(vf));
    Mutate_Bool(mreg, vb, ARRAYSIZE(vb));
}

static void *FlockFleetCreate(FleetAI *ai)
{
    ASSERT(ai != NULL);
    return new FlockFleet(ai);
}

static void FlockFleetDestroy(void *handle)
{
    FlockFleet *sf = (FlockFleet *)handle;
    ASSERT(sf != NULL);
    delete(sf);
}

static void *FlockFleetMobSpawned(void *aiHandle, Mob *m)
{
    FlockFleet *sf = (FlockFleet *)aiHandle;

    ASSERT(sf != NULL);
    ASSERT(m != NULL);

    sf->gov.addMobid(m->mobid);
    return NULL;
}

/*
 * Potentially invalidates any outstanding ship references.
 */
static void FlockFleetMobDestroyed(void *aiHandle, Mob *m, void *aiMobHandle)
{
    FlockFleet *sf = (FlockFleet *)aiHandle;

    sf->gov.removeMobid(m->mobid);
}

static void FlockFleetRunAITick(void *aiHandle)
{
    FlockFleet *sf = (FlockFleet *)aiHandle;
    sf->gov.runTick();
}


void FlockFleet_DoIdle(AIContext *aic,
                       const FlockFleetConfig *ffc,
                       FlockFleetLiveState *ffls,
                       Mob *mob, bool newlyIdle)
{
        FleetAI *ai = aic->ai;
        RandomState *rs = aic->rs;
        SensorGrid *sg = aic->sg;

        Mob *base = sg->friendBase();
        float speed = MobType_GetSpeed(MOB_TYPE_FIGHTER);
        bool nearBase;
        bool doFlock;

        ASSERT(mob->type == MOB_TYPE_FIGHTER);

        nearBase = FALSE;
        if (base != NULL &&
            ffc->nearBaseRadius > 0.0f &&
            FPoint_Distance(&base->pos, &mob->pos) < ffc->nearBaseRadius) {
            nearBase = TRUE;
        }

        doFlock = FALSE;
        if (ffc->flockCrowding <= 1 ||
            sg->numFriendsInRange(MOB_FLAG_FIGHTER, &mob->pos,
                                  ffc->flockRadius) >= ffc->flockCrowding) {
            doFlock = TRUE;
        }

        if (!nearBase && (ffc->alwaysFlock || doFlock)) {
            FRPoint rForce, rPos;

            FRPoint_Zero(&rForce);
            FPoint_ToFRPoint(&mob->pos, &mob->lastPos, &rPos);

            if (doFlock) {
                FPoint avgVel;
                FPoint avgPos;
                sg->friendAvgVel(&avgVel, &mob->pos,
                                ffc->flockRadius, MOB_FLAG_FIGHTER);
                sg->friendAvgPos(&avgPos, &mob->pos,
                                 ffc->flockRadius, MOB_FLAG_FIGHTER);

                FlockFleetAlign(ffc, &avgVel, &rForce);
                FlockFleetCohere(aic, ffc, mob, &avgPos, &rForce);

                FlockFleetSeparate(aic, mob, &rForce, ffls->separateRadius,
                                   ffc->separateWeight);
            }

            FlockFleetAvoidEdges(aic, mob, &rForce, ffc->edgeRadius,
                                 ffc->edgesWeight);
            FlockFleetFindCenter(aic, mob, &rForce, ffc->centerRadius,
                                 ffc->centerWeight);
            FlockFleetFindBase(aic, mob, &rForce, ffc->baseRadius,
                               ffc->baseWeight);
            FlockFleetFindEnemies(aic, ffc, mob, &rForce, ffc->enemyRadius,
                                  ffc->enemyWeight);
            FlockFleetFindEnemyBase(aic, mob, &rForce, ffc->enemyBaseRadius,
                                    ffc->enemyBaseWeight);
            FlockFleetFindCores(aic, ffc, mob, &rForce, ffc->coresRadius,
                                ffc->coresWeight);
            FlockFleetFindLocus(aic, ffc, ffls, mob, &rForce);

            rPos.radius = ffc->curHeadingWeight;
            FRPoint_Add(&rPos, &rForce, &rPos);
            rPos.radius = speed;

            FRPoint_ToFPoint(&rPos, &mob->pos, &mob->cmd.target);
            ASSERT(!isnanf(mob->cmd.target.x));
            ASSERT(!isnanf(mob->cmd.target.y));
        } else if (newlyIdle) {
            if (ffc->randomIdle) {
                mob->cmd.target.x = RandomState_Float(rs, 0.0f, ai->bp.width);
                mob->cmd.target.y = RandomState_Float(rs, 0.0f, ai->bp.height);
            }
        }

        ASSERT(!isnanf(mob->cmd.target.x));
        ASSERT(!isnanf(mob->cmd.target.y));
    }


    static void FlockFleetAlign(const FlockFleetConfig *ffc,
                                const FPoint *avgVel, FRPoint *rPos)
    {
        float weight = ffc->alignWeight;
        FRPoint ravgVel;

        FPoint_ToFRPoint(avgVel, NULL, &ravgVel);
        ravgVel.radius = weight;

        FRPoint_Add(rPos, &ravgVel, rPos);
    }

    static void FlockFleetCohere(AIContext *aic,
                                 const FlockFleetConfig *ffc,
                                 Mob *mob,
                                 const FPoint *avgPos,
                                 FRPoint *rPos) {
        FPoint lAvgPos;
        float weight = ffc->cohereWeight;

        if (ffc->brokenCohere) {
            FlockFleetBrokenCoherePos(aic, ffc, &lAvgPos, &mob->pos);
        } else {
            lAvgPos = *avgPos;
        }

        FRPoint ravgPos;
        FPoint_ToFRPoint(&lAvgPos, NULL, &ravgPos);
        ravgPos.radius = weight;
        FRPoint_Add(rPos, &ravgPos, rPos);
    }


static void FlockFleetBrokenCoherePos(AIContext *aic,
                                      const FlockFleetConfig *ffc,
                                      FPoint *avgPos, const FPoint *center)
{
    SensorGrid *sg = aic->sg;
    MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);
    FPoint lAvgPos;
    float flockRadius = ffc->flockRadius;

    lAvgPos.x = 0.0f;
    lAvgPos.y = 0.0f;

    while (mit.hasNext()) {
        Mob *f = mit.next();
        ASSERT(f != NULL);

        if (FPoint_Distance(&f->pos, center) <= flockRadius) {
            /*
                * The broken version just sums the positions and doesn't
                * properly average them.
                */
            lAvgPos.x += f->pos.x;
            lAvgPos.y += f->pos.y;
        }
    }

    ASSERT(avgPos != NULL);
    *avgPos = lAvgPos;
}


static void
FlockFleetSeparate(AIContext *aic,
                   Mob *mob, FRPoint *rPos,
                   float radius, float weight) {
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    SensorGrid *sg = aic->sg;

    MobSet::MobIt mit = sg->friendsIterator(MOB_FLAG_FIGHTER);
    FRPoint repulseVec;

    repulseVec.radius = 0.0f;
    repulseVec.theta = 0.0f;

    while (mit.hasNext()) {
        Mob *f = mit.next();
        ASSERT(f != NULL);


        if (f->mobid != mob->mobid &&
            FPoint_Distance(&f->pos, &mob->pos) <= radius) {
            FlockFleetRepulseVector(aic, &repulseVec, &f->pos,
                                    &mob->pos, radius);
        }
    }

    repulseVec.radius = weight;
    FRPoint_Add(rPos, &repulseVec, rPos);
}

static void FlockFleetRepulseVector(AIContext *aic,
                                    FRPoint *repulseVec,
                                    FPoint *pos, FPoint *c,
                                    float repulseRadius) {
    RandomState *rs = aic->rs;

    FRPoint drp;

    FPoint_ToFRPoint(pos, c, &drp);

    ASSERT(drp.radius >= 0.0f);
    ASSERT(repulseRadius >= 0.0f);

    if (drp.radius <= MICRON) {
        drp.theta = RandomState_Float(rs, 0, M_PI * 2.0f);
        drp.radius = 1.0f;
    } else {
        float repulsion;
        float k = (drp.radius / repulseRadius) + 1.0f;
        repulsion = 1.0f / (k * k);
        drp.radius = -1.0f * repulsion;
    }

    FRPoint_Add(&drp, repulseVec, repulseVec);
}


static void FlockFleetAvoidEdges(AIContext *aic,
                                 Mob *mob, FRPoint *rPos,
                                 float repulseRadius, float weight)
{
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    FleetAI *ai = aic->ai;

    if (FlockFleetEdgeDistance(aic, &mob->pos) >= repulseRadius) {
        return;
    }

    FRPoint repulseVec;

    repulseVec.radius = 0.0f;
    repulseVec.theta = 0.0f;

    FPoint edgePoint;

    /*
        * Left Edge
        */
    edgePoint = mob->pos;
    edgePoint.x = 0.0f;
    if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
        FlockFleetRepulseVector(aic, &repulseVec, &edgePoint, &mob->pos,
                                repulseRadius);
    }

    /*
        * Right Edge
        */
    edgePoint = mob->pos;
    edgePoint.x = ai->bp.width;
    if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
        FlockFleetRepulseVector(aic, &repulseVec, &edgePoint, &mob->pos,
                                repulseRadius);
    }

    /*
        * Top Edge
        */
    edgePoint = mob->pos;
    edgePoint.y = 0.0f;
    if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
        FlockFleetRepulseVector(aic, &repulseVec, &edgePoint, &mob->pos,
                                repulseRadius);
    }

    /*
        * Bottom edge
        */
    edgePoint = mob->pos;
    edgePoint.y = ai->bp.height;
    if (FPoint_Distance(&edgePoint, &mob->pos) <= repulseRadius) {
        FlockFleetRepulseVector(aic, &repulseVec, &edgePoint, &mob->pos,
                                repulseRadius);
    }

    repulseVec.radius = weight;
    FRPoint_Add(rPos, &repulseVec, rPos);
}

static float FlockFleetEdgeDistance(AIContext *aic, FPoint *pos)
{
    FleetAI *ai = aic->ai;
    float edgeDistance;
    FPoint edgePoint;

    edgePoint = *pos;
    edgePoint.x = 0.0f;
    edgeDistance = FPoint_Distance(pos, &edgePoint);

    edgePoint = *pos;
    edgePoint.x = ai->bp.width;
    edgeDistance = MIN(edgeDistance, FPoint_Distance(pos, &edgePoint));

    edgePoint = *pos;
    edgePoint.y = 0.0f;
    edgeDistance = MIN(edgeDistance, FPoint_Distance(pos, &edgePoint));

    edgePoint = *pos;
    edgePoint.y = ai->bp.height;
    edgeDistance = MIN(edgeDistance, FPoint_Distance(pos, &edgePoint));

    return edgeDistance;
}


static void FlockFleetFindCenter(AIContext *aic,
                                 Mob *mob, FRPoint *rPos,
                                 float radius, float weight)
{
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    FPoint center;
    center.x = aic->ai->bp.width / 2;
    center.y = aic->ai->bp.height / 2;
    FlockFleetPullVector(rPos, &mob->pos, &center, radius, weight, PULL_RANGE);
}


static void
FlockFleetPullVector(FRPoint *curForce,
                     const FPoint *cPos, const FPoint *tPos,
                     float radius, float weight, FlockPullType pType)
{
    ASSERT(pType == PULL_ALWAYS ||
            pType == PULL_RANGE);

    if (pType == PULL_RANGE &&
        FPoint_Distance(cPos, tPos) > radius) {
        return;
    } else if (weight == 0.0f) {
        return;
    }

    FPoint eVec;
    FRPoint reVec;
    FPoint_Subtract(tPos, cPos, &eVec);
    FPoint_ToFRPoint(&eVec, NULL, &reVec);
    reVec.radius = weight;
    FRPoint_Add(curForce, &reVec, curForce);
}


static void
FlockFleetFindBase(AIContext *aic,
                   Mob *mob, FRPoint *rPos,
                   float radius, float weight)
{
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    SensorGrid *sg = aic->sg;
    Mob *base = sg->friendBase();

    if (base != NULL) {
        FlockFleetPullVector(rPos, &mob->pos, &base->pos, radius, weight,
                             PULL_RANGE);
    }
}

static void
FlockFleetFindEnemyBase(AIContext *aic,
                        Mob *mob, FRPoint *rPos,
                        float radius, float weight)
{
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    SensorGrid *sg = aic->sg;
    Mob *base = sg->enemyBase();

    if (base != NULL) {
        FlockFleetPullVector(rPos, &mob->pos, &base->pos, radius, weight,
                             PULL_RANGE);
    }
}


static void
FlockFleetFindEnemies(AIContext *aic,
                      const FlockFleetConfig *ffc,
                      Mob *mob, FRPoint *rPos,
                      float radius, float weight)
{
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    SensorGrid *sg = aic->sg;
    Mob *enemy = sg->findClosestTarget(&mob->pos, MOB_FLAG_SHIP);

    if (enemy != NULL) {
        int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                &mob->pos, ffc->enemyCrowdRadius);
        FlockPullType pType = numFriends >= ffc->enemyCrowding ?
                                PULL_ALWAYS : PULL_RANGE;
        FlockFleetPullVector(rPos, &mob->pos, &enemy->pos, radius, weight,
                             pType);
    }
}

static void
FlockFleetFindCores(AIContext *aic,
                    const FlockFleetConfig *ffc,
                    Mob *mob, FRPoint *rPos, float radius, float weight)
{
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    SensorGrid *sg = aic->sg;
    Mob *core = sg->findClosestTarget(&mob->pos, MOB_FLAG_POWER_CORE);

    if (core != NULL) {
        int numFriends = sg->numFriendsInRange(MOB_FLAG_FIGHTER,
                                                &mob->pos,
                                                ffc->coresCrowdRadius);
        FlockPullType pType = numFriends >= ffc->coresCrowding ?
                                PULL_ALWAYS : PULL_RANGE;
        FlockFleetPullVector(rPos, &mob->pos, &core->pos, radius, weight,
                             pType);
    }
}


static void FlockFleetFindLocus(AIContext *aic,
                                const FlockFleetConfig *ffc,
                                FlockFleetLiveState *ffls,
                                Mob *mob, FRPoint *rPos)
{
    ASSERT(mob->type == MOB_TYPE_FIGHTER);
    FPoint circular;
    FPoint linear;
    FPoint locus;
    bool haveCircular = FALSE;
    bool haveLinear = FALSE;
    bool haveRandom = FALSE;
    float width = aic->ai->bp.width;
    float height = aic->ai->bp.height;
    float temp;

    if (ffc->locusCircularPeriod > 0.0f &&
        ffc->locusCircularWeight != 0.0f) {
        float cwidth = width / 2;
        float cheight = height / 2;
        float ct = aic->ai->tick / ffc->locusCircularPeriod;

        /*
            * This isn't actually the circumference of an ellipse,
            * but it's a good approximation.
            */
        ct /= M_PI * (cwidth + cheight);

        circular.x = cwidth + cwidth * cosf(ct);
        circular.y = cheight + cheight * sinf(ct);
        haveCircular = TRUE;
    }

    if (ffc->locusRandomPeriod > 0.0f &&
        ffc->locusRandomWeight != 0.0f) {
        /*
            * XXX: Each ship will get a different random locus on the first
            * tick.
            */
        if (ffls->randomLocusTick == 0 ||
            aic->ai->tick - ffls->randomLocusTick >
            ffc->locusRandomPeriod) {
            RandomState *rs = aic->rs;
            ffls->randomLocus.x = RandomState_Float(rs, 0.0f, width);
            ffls->randomLocus.y = RandomState_Float(rs, 0.0f, height);
            ffls->randomLocusTick = aic->ai->tick;
        }
        haveRandom = TRUE;
    }

    if (ffc->locusLinearXPeriod > 0.0f &&
        ffc->locusLinearWeight != 0.0f) {
        float ltx = aic->ai->tick / ffc->locusLinearXPeriod;
        ltx /= 2 * width;
        linear.x = width * modff(ltx / width, &temp);
        if (((uint)temp) % 2 == 1) {
            /*
                * Go backwards for the return trip.
                */
            linear.x = width - linear.x;
        }
        haveLinear = TRUE;
    } else {
        linear.x = mob->pos.x;
    }

    if (ffc->locusLinearYPeriod > 0.0f &&
        ffc->locusLinearWeight != 0.0f) {
        float lty = aic->ai->tick / ffc->locusLinearYPeriod;
        lty /= 2 * height;
        linear.y = height * modff(lty / height, &temp);
        if (((uint)temp) % 2 == 1) {
            /*
                * Go backwards for the return trip.
                */
            linear.y = height - linear.y;
        }
        haveLinear = TRUE;
    } else {
        linear.y = mob->pos.y;
    }

    if (haveLinear || haveCircular || haveRandom) {
        float scale = 0.0f;
        locus.x = 0.0f;
        locus.y = 0.0f;
        if (haveLinear) {
            locus.x += ffc->locusLinearWeight * linear.x;
            locus.y += ffc->locusLinearWeight * linear.y;
            scale += ffc->locusLinearWeight;
        }
        if (haveCircular) {
            locus.x += ffc->locusCircularWeight * circular.x;
            locus.y += ffc->locusCircularWeight * circular.y;
            scale += ffc->locusCircularWeight;
        }
        if (haveRandom) {
            locus.x += ffc->locusRandomWeight * ffls->randomLocus.x;
            locus.y += ffc->locusRandomWeight *  ffls->randomLocus.y;
            scale += ffc->locusRandomWeight;
        }

        if (ffc->useScaledLocus) {
            if (scale != 0.0f) {
                locus.x /= scale;
                locus.y /= scale;
            }
        }

        FlockFleetPullVector(rPos, &mob->pos, &locus,
                             ffc->locusRadius,
                             ffc->locusWeight, PULL_RANGE);
    }
}
