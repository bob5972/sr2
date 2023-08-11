#!/bin/bash

OPTS="-H"
OPTS="${OPTS} -t 14"
OPTS="${OPTS} -l 1"

POPFILE="build/tmp/popTournament.txt";
OPTS="${OPTS} --dumpPopulation $POPFILE"

./compile.sh develperf && build/sr2 tournament $OPTS "$@"
