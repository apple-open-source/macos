#!/bin/sh

# This script is used to generate and install expected results for all t.* tests in testdir
# Run this script when new t.* tests comming

source ./test_installer.sh

tests=$(cd ../testdir;ls t.*)
test_data=test.data
expected_mode=0
mode=${1:-$expected_mode}


install_tests "${tests[@]}" $test_data $mode