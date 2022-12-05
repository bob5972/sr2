#!/bin/bash

./compile.sh debug && \
    echo && echo build/sr2 display $OPTS "$@" && \
    build/sr2 display $OPTS "$@"
