#!/bin/bash

if [ "$1" == "" ] ||
   [ "$1" == "capture" ]; then
    ACTION="capture"
elif [ "$1" == "top" ]; then
    ACTION="top";
else
    echo "Unknown cmd: $1"
    exit 1
fi;

if [ "$ACTION" == "capture" ]; then
    perf record -g build/sr2 -H -l 1 -s 1
else
    PID=`ps aux |grep sr2 | grep -v grep | awk '{print $2}'`
    perf top --pid $PID
fi;
