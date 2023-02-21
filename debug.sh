#!/bin/bash

#echo 0 > /proc/sys/kernel/yama/ptrace_scope
PID=`ps aux |grep build/sr2 | grep -v grep | awk '{print $2}'`
gdb "$@" --pid $PID
