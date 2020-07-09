#!/bin/bash

OPTS="-H"
OPTS="${OPTS} -t 14"

if [ "$1" != "" ]; then
    LOOP="$1";
else
    LOOP=100
fi;

./compile.sh release && build/sr2 $OPTS -l $LOOP
