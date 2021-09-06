#!/bin/bash

OPTS="-H"
OPTS="${OPTS} -t 14"
OPTS="${OPTS} -l 1"

POPLIMIT=10
KILLRATIO=0.1

POPFILE="build/tmp/popMutate.txt";
OPTS="${OPTS} --dumpPopulation $POPFILE --usePopulation $POPFILE"
OPTS="${OPTS} --usePopulation $POPFILE"
OPTS="${OPTS} --mutatePopulation"
OPTS="${OPTS} --populationKillRatio $KILLRATIO"
OPTS="${OPTS} --populationLimit $POPLIMIT"

cp -f $POPFILE $POPFILE.old

#BUILDTYPE="release";
BUILDTYPE="debug";
./compile.sh $BUILDTYPE && echo && \
    echo build/sr2 $OPTS "$@" && \
    build/sr2 $OPTS "$@"

if [ $? == 0 ]; then
    echo
    echo "$POPFILE: "
    cat $POPFILE
fi;
