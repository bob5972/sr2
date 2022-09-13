#!/bin/bash

STABLE_POP=50
NOOB_POP=50

TICK_LIMIT=40000
SCENARIO=fast
THREADS=14
BUILDTYPE="release";

STABLE_FILE="build/tmp/stable.zoo";
NOOB_FILE="build/tmp/noob.zoo";

SCREEN1_FILE="build/tmp/screen1.zoo";
SCREEN1_ITERATIONS=3
SCREEN1_DEFECTIVE=0.1

SCREEN2_FILE="build/tmp/screen2.zoo";
SCREEN2_ITERATIONS=5
SCREEN2_DEFECTIVE=0.1

SCREEN3_FILE="build/tmp/screen3.zoo";
SCREEN3_ITERATIONS=10
SCREEN3_DEFECTIVE=0.1

SCREENS_FILE="build/tmp/screenS.zoo";
SCREENS_DEFECTIVE=0.1
SCREENS_NEW_ITERATIONS=20
SCREENS_STALE_ITERATIONS=1

if [ -f evolve.local ]; then
    source evolve.local ;
fi;

OPTS="-H"
OPTS="${OPTS} --tickLimit $TICK_LIMIT";
OPTS="${OPTS} -S $SCENARIO"
OPTS="${OPTS} -t $THREADS"

./compile.sh $BUILDTYPE
if [ $? != 0 ]; then exit $? ; fi;

echo 'sr2: mutate'
build/sr2 mutate $OPTS --usePopulation $STABLE_FILE \
                       --outputFile $NOOB_FILE \
                       --mutationCount $NOOB_POP
if [ $? != 0 ]; then exit $? ; fi;

if [ -f $SCREEN1_FILE ]; then
    echo 'sr2: measure screen1'
    build/sr2 measure $OPTS --usePopulation $NOOB_FILE \
                            --controlPopulation $SCREEN1_FILE \
                            --loop $SCREEN1_ITERATIONS
    if [ $? != 0 ]; then exit $? ; fi;

    echo 'sr2: kill screen1'
    build/sr2 kill $OPTS --usePopulation $NOOB_FILE \
                        --defectiveLevel $SCREEN1_DEFECTIVE \
                        --resetAfter
    if [ $? != 0 ]; then exit $? ; fi;
fi;

if [ -f $SCREEN2_FILE ]; then
    echo 'sr2: measure screen2'
    build/sr2 measure $OPTS --usePopulation $NOOB_FILE \
                            --controlPopulation $SCREEN2_FILE \
                            --loop $SCREEN2_ITERATIONS
    if [ $? != 0 ]; then exit $? ; fi;

    echo 'sr2: kill screen2'
    build/sr2 kill $OPTS --usePopulation $NOOB_FILE \
                        --defectiveLevel $SCREEN2_DEFECTIVE \
                        --resetAfter
    if [ $? != 0 ]; then exit $? ; fi;
fi;

if [ -f $SCREEN3_FILE ]; then
    echo 'sr2: measure screen3'
    build/sr2 measure $OPTS --usePopulation $NOOB_FILE \
                            --controlPopulation $SCREEN3_FILE \
                            --loop $SCREEN3_ITERATIONS
    if [ $? != 0 ]; then exit $? ; fi;

    echo 'sr2: kill screen3'
    build/sr2 kill $OPTS --usePopulation $NOOB_FILE \
                        --defectiveLevel $SCREEN3_DEFECTIVE \
                        --resetAfter
    if [ $? != 0 ]; then exit $? ; fi;
fi;

# Final screen
echo 'sr2: measure screenS (noob)'
build/sr2 measure $OPTS --usePopulation $NOOB_FILE \
                        --controlPopulation $SCREENS_FILE \
                        --loop $SCREENS_NEW_ITERATIONS
if [ $? != 0 ]; then exit $? ; fi;

echo 'sr2: kill screenS (noob)'
build/sr2 kill $OPTS --usePopulation $NOOB_FILE \
                     --defectiveLevel $SCREENS_DEFECTIVE
if [ $? != 0 ]; then exit $? ; fi;

echo 'sr2: merge'
build/sr2 merge $OPTS --usePopulation $STABLE_FILE \
                      --inputPopulation $NOOB_FILE
if [ $? != 0 ]; then exit $? ; fi;

echo 'sr2: measure screenS (stable)'
build/sr2 measure $OPTS --usePopulation $STABLE_FILE \
                        --controlPopulation $SCREENS_FILE \
                        --loop $SCREENS_STALE_ITERATIONS
if [ $? != 0 ]; then exit $? ; fi;

echo 'sr2: kill screenS (stable)'
build/sr2 kill $OPTS --usePopulation $STABLE_FILE \
                     --defectiveLevel $SCREENS_DEFECTIVE \
                     --minPop $STABLE_POP \
                     --maxPop $STABLE_POP
if [ $? != 0 ]; then exit $? ; fi;
