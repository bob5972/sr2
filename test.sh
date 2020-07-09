#!/bin/bash

OPTS=""

if [ "$DISPLAY" == "" ]; then
    OPTS="${OPTS} -H"
fi;

./compile.sh debug && build/sr2 $OPTS "$@"
