#!/bin/sh

#ifdef __APPLE__
:> /AppleInternal/Tests/text_cmds/md5/1.inp
#endif
exitcode=0

testloop () {
	opt=$1

	while read algorithm; do
		n=0
#ifdef __APPLE__
		for f in /AppleInternal/Tests/text_cmds/md5/*.inp; do
#else
#		for f in %%TESTSBASE%%/sbin/md5/*.inp; do
#endif
			n=$((n + 1))
#ifdef __APPLE__
			expected=$(head -$n /AppleInternal/Tests/text_cmds/md5/$algorithm.digest | tail -1 | cut -w -f4)
#else
#			expected=$(head -$n %%TESTSBASE%%/sbin/md5/$algorithm.digest | tail -1 | cut -w -f4)
#endif
			hash=$($algorithm $opt -c "$expected" $f) || exitcode=1
		done
#ifdef __APPLE__
	done < /AppleInternal/Tests/text_cmds/md5/algorithms.txt
#else
#	done < %%TESTSBASE%%/sbin/md5/algorithms.txt
#endif
}

testloop ""
testloop -q
testloop -r
testloop -qr

exit $exitcode
