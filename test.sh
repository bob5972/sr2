#!/bin/bash

POP="build/tmp/popDump.txt";
OPTS="-L 1000 --dumpPopulation $POP"
OPTS="${OPTS} -H"

./compile.sh debug && build/sr2 -u && \
    echo && echo build/sr2 $OPTS "$@" && build/sr2 $OPTS "$@"
