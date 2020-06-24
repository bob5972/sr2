#!/bin/bash

PID=`ps aux |grep sr2 | grep -v grep | awk '{print $2}'`
perf top --pid $PID
