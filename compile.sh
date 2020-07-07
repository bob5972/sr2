#!/bin/bash

if [ "$1" == "debug" ]; then
    WANTDEBUG=1
elif [ "$1" == "release" ]; then
    WANTDEBUG=0
else
    WANTDEBUG=""
fi;

if [ ! -f config.mk ]; then
    CLEAN=1;
else
    cat config.mk | grep DEBUG=1 > /dev/null
    if [ "$?" == "1" ]; then
        export DEBUG=1;
    else
        export DEBUG=0;
    fi;
fi;

if [ "$WANTDEBUG" != "" ] &&
   [ "$WANTDEBUG" != "$DEBUG" ]; then
    CLEAN=1;
    export DEBUG="$WANTDEBUG"
fi;

if [ "$CLEAN" != "" ]; then
    ./configure && make clean;
fi;

make -j 32
