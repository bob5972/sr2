#!/bin/bash

#if [ "$DISPLAY" == "" ]; then
#    OPTS="${OPTS} -H"
#fi;

./compile.sh debug && \
    echo && echo build/sr2 $OPTS "$@" && \
    build/sr2 $OPTS "$@"
