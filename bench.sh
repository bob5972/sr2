#!/bin/bash

./compile.sh develperf && time build/sr2 -H -l 64 -s 1 -R -t 4 "$@"
