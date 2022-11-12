#!/bin/bash

OPTS="-H -T"
OPTS="${OPTS} -t 14"
OPTS="${OPTS} -l 4"

POPFILE="build/tmp/popTournament.txt";
OPTS="${OPTS} --dumpPopulation $POPFILE"

./compile.sh develperf && build/sr2 $OPTS "$@"
