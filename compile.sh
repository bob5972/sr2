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
    if [ "$?" == "0" ]; then
        export MB_DEBUG=1;
    else
        export MB_DEBUG=0;
    fi;

    cat config.mk | grep SR2_GUI=1 > /dev/null
    if [ "$?" == "0" ]; then
        export SR2_GUI=1;
    else
        export SR2_GUI=0;
    fi;
fi;

if [ "$WANTDEBUG" != "" ] &&
   [ "$WANTDEBUG" != "$MB_DEBUG" ]; then
    CLEAN=1;
    export MB_DEBUG="$WANTDEBUG"
fi;

if [ "$CLEAN" != "" ]; then
    ./configure && make clean;
fi;

make -j 32
