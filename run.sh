#!/bin/bash

if [ "$1" != "" ]; then
    LOOP="$1";
else
    LOOP=10
fi;

./compile.sh release && build/sr2 -H -l $LOOP
