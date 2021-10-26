#!/bin/bash

POP="build/tmp/popDump.txt";
OPTS="-L 1000 --dumpPopulation $POP"
OPTS="${OPTS} -H"

#if [ "$DISPLAY" == "" ]; then
#    OPTS="${OPTS} -H"
#fi;

./compile.sh debug && build/MBLib/test.bin && build/sr2 -u && \
    echo && echo build/sr2 $OPTS "$@" && build/sr2 $OPTS "$@"

if [ $? == 0 ]; then
    echo
    echo "$POP: "
    cat $POP
fi;
