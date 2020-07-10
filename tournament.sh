#!/bin/bash

OPTS="-H -T"
OPTS="${OPTS} -t 14"

if [ "$1" != "" ]; then
    LOOP="$1";
else
    LOOP=4
fi;

./compile.sh release && build/sr2 $OPTS -l $LOOP
