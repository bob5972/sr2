#!/bin/bash

if [ "$1" != "" ]; then
    LOOP="$1";
else
    LOOP=1
fi;

if [ ! -f config.mk ]; then
    CLEAN=1;
else
    cat config.mk | grep DEBUG=1 > /dev/null
    if [ "$?" == "1" ]; then
        CLEAN=1;
    fi;
fi;

if [ "$CLEAN" != "" ]; then
    DEBUG=1 ./configure && make clean;
fi;

make -j 8 && build/sr2
