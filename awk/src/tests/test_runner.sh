#!/bin/sh

# This script will execute tests in BATS.
# It expects the python test files following by the DISABLED tests
# e.g: test_runner.sh REGRESS-t.py disabled_test1 disabled_test2

cd /tmp/awk/src/tests
pwd
python3 "$@"