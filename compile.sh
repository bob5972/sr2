#!/bin/bash
## compile -- part of SpaceRobots2

if [ "$1" == "debug" ]; then
    WANTDEBUG=1
    WANTDEVEL=1
elif [ "$1" == "develperf" ]; then
    WANTDEBUG=0
    WANTDEVEL=1
elif [ "$1" == "release" ]; then
    WANTDEBUG=0
    WANTDEVEL=0
else
    WANTDEBUG=""
    WANTDEVEL=""
fi;

if [ ! -f config.mk ]; then
    CLEAN=1;
else
    cat config.mk | grep MB_DEBUG=1 > /dev/null
    if [ "$?" == "0" ]; then
        export MB_DEBUG=1;
    else
        export MB_DEBUG=0;
    fi;

    cat config.mk | grep MB_DEVEL=1 > /dev/null
    if [ "$?" == "0" ]; then
        export MB_DEVEL=1;
    else
        export MB_DEVEL=0;
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

if [ "$WANTDEVEL" != "" ] &&
   [ "$WANTDEVEL" != "$MB_DEVEL" ]; then
    CLEAN=1;
    export MB_DEVEL="$WANTDEVEL"
fi;

if [ "$CLEAN" != "" ]; then
    ./configure && make clean;
fi;

make -j 32
