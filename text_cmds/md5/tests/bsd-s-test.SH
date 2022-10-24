#!/bin/sh

#ifdef __APPLE__
:> /AppleInternal/Tests/text_cmds/md5/1.inp
#endif
exitcode=0

testloop () {
	opt=$1
	sum=$2
	field=$3

	while read algorithm; do
		n=0
#ifdef __APPLE__
		for f in /AppleInternal/Tests/text_cmds/md5/*.inp; do
#else
#		for f in %%TESTSBASE%%/sbin/md5/*.inp; do
#endif
			n=$((n + 1))
			read text < $f
			hash=$($algorithm$sum $opt -s "$text" | cut -d= -f$field)
			hash=$(echo $hash | cut -w -f1)
#ifdef __APPLE__
			expected=$(head -$n /AppleInternal/Tests/text_cmds/md5/$algorithm.digest | tail -1 | cut -w -f4)
#else
#			expected=$(head -$n %%TESTSBASE%%/sbin/md5/$algorithm.digest | tail -1 | cut -w -f4)
#endif
			[ "$hash" = "$expected" ] || exitcode=1
		done
#ifdef __APPLE__
	done < /AppleInternal/Tests/text_cmds/md5/algorithms.txt
#else
#	done < %%TESTSBASE%%/sbin/md5/algorithms.txt
#endif
}

testloop "" "" 2
testloop -q "" 1
testloop -r "" 1
testloop -qr "" 1
testloop "" sum 1
testloop -q sum 1

exit $exitcode
