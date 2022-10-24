#!/bin/sh

#ifdef __APPLE__
:> 1.inp
/bin/cp /AppleInternal/Tests/text_cmds/md5/*.inp . || exit 127
#else
#/bin/cp %%TESTSBASE%%/sbin/md5/*.inp . || exit 127
#endif

exitcode=0

testloop () {
	opt=$1

	while read algorithm; do
#ifdef __APPLE__
		${algorithm}sum -c /AppleInternal/Tests/text_cmds/md5/${algorithm}.digest || exitcode=1
		${algorithm}sum -c /AppleInternal/Tests/text_cmds/md5/${algorithm}sum.digest || exitcode=1
	done < /AppleInternal/Tests/text_cmds/md5/algorithms.txt
#else
#		${algorithm}sum -c %%TESTSBASE%%/sbin/md5/${algorithm}.digest || exitcode=1
#		${algorithm}sum -c %%TESTSBASE%%/sbin/md5/${algorithm}sum.digest || exitcode=1
#	done < %%TESTSBASE%%/sbin/md5/algorithms.txt
#endif
}

testloop ""
testloop -q
testloop -r
testloop -qr

exit $exitcode
