#!/bin/sh

while read algorithm; do
	$algorithm -x > self-test.$algorithm.out || exitcode=$?
#ifdef __APPLE__
	diff /AppleInternal/Tests/text_cmds/md5/self-test.$algorithm.chk self-test.$algorithm.out
done < /AppleInternal/Tests/text_cmds/md5/algorithms.txt
#else
#	diff %%TESTSBASE%%/sbin/md5/self-test.$algorithm.chk self-test.$algorithm.out
#done < %%TESTSBASE%%/sbin/md5/algorithms.txt
#endif

exit $exitcode
