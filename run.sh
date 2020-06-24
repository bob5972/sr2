#!/bin/bash

if [ "$1" != "" ]; then
    LOOP="$1";
else
    LOOP=10
fi;

if [ ! -f config.mk ]; then
    CLEAN=1;
else
    cat config.mk | grep DEBUG=0 > /dev/null
    if [ "$?" == "1" ]; then
        CLEAN=1;
    fi;
fi;

if [ "$CLEAN" != "" ]; then
    DEBUG=0 ./configure && make clean;
fi;

make -j 20 && build/sr2 -H -l $LOOP
