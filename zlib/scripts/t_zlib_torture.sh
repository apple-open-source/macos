#!/bin/zsh

binary=$1/usr/local/bin/t_zlib_verify
test_dir=$2
script=$(pwd)/scripts/t_zlib_all.pl

function run
{
	cmd=$1

	echo "RUN $cmd"
	eval $cmd
	rc=$?
	if [[ $rc != 0 ]] then;
		echo "FAIL $cmd"
		exit 1
	fi;
}

run "cd $test_dir"
for opts in '-t' '-t -f' '-t -f -r' '-t -r';
do
	run "$script -d . -b \"$binary -e 1 -d 1 $opts\""
done
