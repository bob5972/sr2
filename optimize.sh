#!/bin/bash

OPTS="-H -O"
OPTS="${OPTS} -t 14"
OPTS="${OPTS} -l 4"

./compile.sh release && build/sr2 $OPTS "$@"
