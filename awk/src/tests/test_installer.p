#!/bin/sh

# This script is used to generate and install expected results for all p.* tests in testdir
# Run this script when new p.* tests comming

source ./test_installer.sh

tests=$(cd ../testdir;ls p.? p.??*)
expected_mode=0
mode=${1:-$expected_mode}
test_data=test.countries

install_tests "${tests[@]}" $test_data $mode
