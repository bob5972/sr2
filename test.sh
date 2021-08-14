#!/bin/bash

POP="build/tmp/popDump.txt";
OPTS="--dumpPopulation $POP"

if [ "$DISPLAY" == "" ]; then
    OPTS="${OPTS} -H"
fi;

./compile.sh debug && build/sr2 -u && build/sr2 $OPTS "$@"

if [ $? == 0 ]; then
    echo
    echo "$POP: "
    cat $POP
fi;
