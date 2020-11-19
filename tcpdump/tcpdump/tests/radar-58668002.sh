#!/bin/sh -v

name=radar-58668002

passed=`cat .passed`
failed=`cat .failed`

if [ ${EUID} -ne 0 ]; then
	printf '    %-35s: TEST SKIPPED (no running as root)\n' $name
	exit 0
fi

echo "$ tcpdump -n -d tcp" >>verbose-outputs.txt
tcpdump -n -d tcp 1>>verbose-outputs.txt 2>&1
r=$?

if [ $r -eq 1 ]; then
	printf "    %-35s: passed\n" $name
	passed=`expr $passed + 1`
	echo $passed >.passed
else
	printf "    %-35s: TEST FAILED" $name
	printf "Failed test: $name\n\n" >> failure-outputs.txt
	failed=`expr $failed + 1`
	echo $failed >.failed
fi

printf "\n"
exit $(( $r >> 8 ))
