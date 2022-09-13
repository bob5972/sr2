#!/bin/bash

STABLE_POP=50
NOOB_POP=50

TICK_LIMIT=40000
SCENARIO=fast
THREADS=14
BUILDTYPE="release";

if [ -f evolve.local ]; then
    source evolve.local ;
fi;

STABLE_FILE="build/tmp/stable.zoo";
NOOB_FILE="build/tmp/noob.zoo";

SCREEN1_FILE="build/tmp/screen1.zoo";
SCREEN1_ITERATIONS=3
SCREEN1_DEFECTIVE=0.1

SCREENS_FILE="build/tmp/screenS.zoo";
SCREENS_NEW_ITERATIONS=10
SCREENS_STALE_ITERATIONS=1

OPTS="-H"
OPTS="${OPTS} --tickLimit $TICK_LIMIT";
OPTS="${OPTS} -S $SCENARIO"
OPTS="${OPTS} -t $THREADS"

./compile.sh $BUILDTYPE
if [ $? != 0 ]; then
    exit $?
fi;

echo

echo sr2: mutate
build/sr2 mutate $OPTS --usePopulation $STABLE_FILE \
                       --outputFile $NOOB_FILE \
                       --mutationCount $NOOB_POP
if [ $? != 0 ]; then
    exit $?
fi;

echo sr2: measure screen1
build/sr2 measure $OPTS --usePopulation $NOOB_FILE \
                        --controlPopulation $SCREEN1_FILE \
                        --loop $SCREEN1_ITERATIONS
if [ $? != 0 ]; then
    exit $?
fi;

echo sr2: kill screen1
build/sr2 kill $OPTS --usePopulation $NOOB_FILE \
                     --defectiveLevel $SCREEN1_DEFECTIVE \
                     --resetAfter
if [ $? != 0 ]; then
    exit $?
fi;

echo sr2: measure screenS
build/sr2 measure $OPTS --usePopulation $NOOB_FILE \
                        --controlPopulation $SCREENS_FILE \
                        --loop $SCREENS_NEW_ITERATIONS
if [ $? != 0 ]; then
    exit $?
fi;

echo sr2: merge
build/sr2 merge $OPTS --usePopulation $STABLE_FILE \
                      --inputPopulation $NOOB_FILE
if [ $? != 0 ]; then
    exit $?
fi;

echo sr2: measure screenS
build/sr2 measure $OPTS --usePopulation $STABLE_FILE \
                        --controlPopulation $SCREENS_FILE \
                        --loop $SCREENS_STALE_ITERATIONS
if [ $? != 0 ]; then
    exit $?
fi;

echo sr2: kill screenS
build/sr2 kill $OPTS --usePopulation $STABLE_FILE \
                     --maxPop $STABLE_POP
if [ $? != 0 ]; then
    exit $?
fi;


