#!/bin/bash

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

make -j 8 && time build/sr2 -H -l 4 -s 1
