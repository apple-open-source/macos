#!/bin/sh
# This guider will guide the test to the right output
# based on the demand of test runner

cd ../testdir

oldawk=${oldawk-awk}

if [ -z "$1" ]
then
    >&2 echo "No test name supplied!!!"
    exit 1
elif [ ! -e $1 ]
then
    >&2 echo "The test does not exist!!!"
    exit 1
fi

test_file=$1
expected_mode=0
actual_mode=1
mode=$actual_mode
default_dest=/tmp

mode=${2:-$actual_mode}
dest=${3:-$default_dest}
file_ext=""

if [ $mode == $expected_mode ]
then
  file_ext=expected
elif [ $mode == $actual_mode ]
then
  file_ext=actual
else
  >&2 echo "Wrong mode suppiled: 0 or 1 only"
  exit 1
fi

test_out_put=$dest/$file_ext.$test_file

echo $test_out_put
