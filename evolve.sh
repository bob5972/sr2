#!/bin/bash

POPLIMIT=50
KILLRATIO=0.30
DEFECTIVERATIO=0.0
STALE_IT=1
NEW_IT=5
TICK_LIMIT=40000
SCENARIO=fast
THREADS=14
BUILDTYPE="release";

if [ -f evolve.local ]; then
    source evolve.local ;
fi;

POPFILE="build/tmp/popMutate.txt";

OPTS="-H"
OPTS="${OPTS} --tickLimit $TICK_LIMIT";
OPTS="${OPTS} --dumpPopulation $POPFILE"
OPTS="${OPTS} --usePopulation $POPFILE"
OPTS="${OPTS} --mutatePopulation"
OPTS="${OPTS} --populationKillRatio $KILLRATIO"
OPTS="${OPTS} --populationLimit $POPLIMIT"
OPTS="${OPTS} --mutationNewIterations $NEW_IT"
OPTS="${OPTS} --mutationStaleIterations $STALE_IT"
OPTS="${OPTS} --populationDefectiveRatio $DEFECTIVERATIO"

if [ "$MINAGE" != "" ]; then
    OPTS="${OPTS} --mutationMinAge $MINAGE";
fi;

OPTS="${OPTS} -S fast"
OPTS="${OPTS} -t $THREADS"

cp -f $POPFILE $POPFILE.old

./compile.sh $BUILDTYPE && echo && \
    echo build/sr2 $OPTS "$@" && \
    build/sr2 $OPTS "$@"
