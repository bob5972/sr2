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
SCREENS_ITERATIONS=10

OPTS="-H"
OPTS="${OPTS} --tickLimit $TICK_LIMIT";
OPTS="${OPTS} -S $SCENARIO"
OPTS="${OPTS} -t $THREADS"

cp -f $POPFILE $POPFILE.old

./compile.sh $BUILDTYPE
if [ $? != 0 ]; then
    exit $?
fi;

echo

build/sr2 mutate $OPTS --usePopulation $STABLE_FILE \
                       --outputFile $NOOB_FILE \
                       --mutationCount $NOOB_POP
if [ $? != 0 ]; then
    exit $?
fi;

build/sr2 measure $OPTS --usePopulation $NOOB_FILE \
                        --controlPopulation $SCREEN1_FILE \
                        --loop $SCREEN1_ITERATIONS
if [ $? != 0 ]; then
    exit $?
fi;

build/sr2 kill $OPTS --usePopulation $NOOB_FILE \
                     --defectiveLevel $SCREEN1_DEFECTIVE \
                     --resetAfter
if [ $? != 0 ]; then
    exit $?
fi;

build/sr2 merge $OPTS --usePopulation $STABLE_FILE \
                      --inputPopulation $NOOB_FILE
if [ $? != 0 ]; then
    exit $?
fi;

build/sr2 measure $OPTS --usePopulation $STABLE_FILE \
                        --controlPopulation $SCREENS_FILE \
                        --loop $SCREENS_ITERATIONS
if [ $? != 0 ]; then
    exit $?
fi;

build/sr2 kill $OPTS --usePopulation $STABLE_FILE \
                     --maxPop $STABLE_POP
if [ $? != 0 ]; then
    exit $?
fi;


