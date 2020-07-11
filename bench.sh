#!/bin/bash

./compile.sh release && time build/sr2 -H -l 16 -s 1 -R -t 4
