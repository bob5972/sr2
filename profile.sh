#!/bin/bash

if [ "$1" == "capture" ]; then
    perf record -g build/sr2 -H -l 1
else
    PID=`ps aux |grep sr2 | grep -v grep | awk '{print $2}'`
    perf top --pid $PID
fi;
