#!/bin/bash

OPTS="-H"
OPTS="${OPTS} -t 14"
OPTS="${OPTS} -l 4"

POPLIMIT=50
KILLRATIO=0.5

POPFILE="build/tmp/popMutate.txt";
OPTS="${OPTS} --dumpPopulation $POPFILE --usePopulation $POPFILE"
OPTS="${OPTS} --usePopulation $POPFILE"
OPTS="${OPTS} --mutatePopulation"
OPTS="${OPTS} --populationKillRatio $KILLRATIO"
OPTS="${OPTS} --populationLimit $POPLIMIT"

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
