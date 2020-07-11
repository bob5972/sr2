#!/bin/bash

OPTS="-H"
OPTS="${OPTS} -t 14"
OPTS="${OPTS} -l 100"

./compile.sh release && build/sr2 $OPTS "$@"
