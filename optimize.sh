#!/bin/bash

OPTS="-H"
OPTS="${OPTS} -t 14"
OPTS="${OPTS} -l 10"

./compile.sh develperf && build/sr2 optimize $OPTS "$@"
