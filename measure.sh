#!/bin/bash

STABLE_FILE="zoo/stable.zoo";

SCREENS_FILE="zoo/screenS.zoo";

if [ -f evolve.local ]; then
    source evolve.local ;
fi;

OPTS="-H"
OPTS="${OPTS} --tickLimit $TICK_LIMIT";
OPTS="${OPTS} -S $SCENARIO"
OPTS="${OPTS} -t $THREADS"

./compile.sh $BUILDTYPE
if [ $? != 0 ]; then exit $? ; fi;

echo 'sr2: measure screenS (stable)'
build/sr2 measure $OPTS --usePopulation $STABLE_FILE \
                        --controlPopulation $SCREENS_FILE \
                        $@




