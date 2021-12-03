#!/bin/bash

OPTS="-H"
OPTS="${OPTS} -t 14"

POPLIMIT=50
KILLRATIO=0.30
DEFECTIVERATIO=0.0
STALE_IT=1
NEW_IT=5
TICK_LIMIT=40000
SCENARIO=fast

if [ -f evolve.local ]; then
    source evolve.local ;
fi;

POPFILE="build/tmp/popMutate.txt";
OPTS="${OPTS} --tickLimit $TICK_LIMIT";
OPTS="${OPTS} --dumpPopulation $POPFILE --usePopulation $POPFILE"
OPTS="${OPTS} --usePopulation $POPFILE"
OPTS="${OPTS} --mutatePopulation"
OPTS="${OPTS} --populationKillRatio $KILLRATIO"
OPTS="${OPTS} --populationLimit $POPLIMIT"
OPTS="${OPTS} --mutationNewIterations $NEW_IT"
OPTS="${OPTS} --mutationStaleIterations $STALE_IT"
OPTS="${OPTS} --populationDefectiveRatio $DEFECTIVERATIO"
OPTS="${OPTS} -S fast"

cp -f $POPFILE $POPFILE.old

BUILDTYPE="release";
./compile.sh $BUILDTYPE && echo && \
    echo build/sr2 $OPTS "$@" && \
    build/sr2 $OPTS "$@"

#if [ $? == 0 ]; then
#    echo
#    echo "$POPFILE: "
#    cat $POPFILE
#fi;
