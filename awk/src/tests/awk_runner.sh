#!/bin/sh
# This script takes 3 arguments test file, test data and test output
# to generate output of the test to the output file.

cd ../testdir

test_file=$1
test_data=$2
test_out_put=$3

t_data=test.data
p_data=test.countries

oldawk=${oldawk-awk}
empty_content=EMPTY
if [ $test_data ==  $t_data ]
then
$oldawk -f $test_file $test_data >$test_out_put
elif [ $test_data == $p_data ]
then
$oldawk -f $test_file $test_data $test_data >$test_out_put
else
>&2 echo "test data not supported"
fi
if ! [ -s $test_out_put ]
then
echo $empty_content >$test_out_put
fi
echo $test_out_put