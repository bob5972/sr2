#!/bin/bash
# compile.sh -- part of SpaceRobots2

if [ -f compile.local ]; then
    source compile.local
fi;

MOPTS="";
COPTS="";

if [ "$THREADS" != "" ]; then
    OPTS="${OPTS} -j ${THREADS}"
else
    OPTS="${OPTS} -j 8"
fi;

if [ "$BUILDTYPE" != "" ]; then
    COPTS="${COPTS} --buildType $BUILDTYPE";
fi;

./configure $@ ${COPTS} && make $OPTS
