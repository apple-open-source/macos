#!/bin/sh

#set -e

if [ -z $1 ]
then
	echo "must pass in tests binary to run"
	exit
fi

if [ -z $2 ]
then
	echo "must pass in the library binary to run"
	exit
fi

testprog=$1
dyld_path=$2

export DYLD_LIBRARY_PATH=$dyld_path
export DYLD_PRINT_LIBRARIES=YES


passed=""
failed=""
tests=""

if [ -z $3 ]
then
	tests=$($testprog -l | grep "ident:" | awk '{print $2}')
else
	shift
	shift
	tests=$@
fi

echo "running the the following tests:\n$tests"

coveragefiles=""
for test in $tests
do
	printf "running test $test\n"
	#$testprog $test 2>/dev/null > /dev/null
	$testprog $test

	if [ $? -eq 0 ]
	then
		passed="$passed $test"
	else
		failed="$failed $test"
	fi

done

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo "\n\n###############################\n\n"
echo "Testing complete"
echo "tested with tests binary\t$testprog"
echo "tested with tests library\t${dyld_path}\n\n"

for x in $passed
do
	printf "%-40s${GREEN}passed${NC}\n" $x
done

for x in $failed
do
	printf "%-40s${RED}failed${NC}\n" $x
done
