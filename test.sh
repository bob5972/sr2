#!/bin/bash

POP="build/tmp/popDump.txt";
OPTS="-L 1000 --dumpPopulation $POP"
OPTS="${OPTS} -H"

./compile.sh debug &&
    build/sr2 unitTests &&
    echo &&
    echo build/sr2 $OPTS "$@" &&
    build/sr2 $OPTS "$@" &&
    build/sr2 mutate -c 10 --usePopulation $POP --outputFile $POP
