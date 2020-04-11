#!/usr/bin/env bash

for testfile in tests/*.lisp; do
    echo "Testing $testfile"
    poetry run lang $testfile || echo "COMPILE FAILED"
    ./out || echo "TEST FAILED"
    echo
done
