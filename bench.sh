#!/bin/bash

./compile.sh release && time build/sr2 -H -l 4 -s 1
