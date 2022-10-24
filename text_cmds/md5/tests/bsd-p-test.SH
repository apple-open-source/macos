#!/bin/sh

#ifdef __APPLE__
:> /AppleInternal/Tests/text_cmds/md5/1.inp
#endif
exitcode=0

testloop () {
	opt=$1
	sum=$2

	for algorithm in sha512; do
#ifdef __APPLE__
		for f in /AppleInternal/Tests/text_cmds/md5/*.inp; do
#else
#		for f in %%TESTSBASE%%/sbin/md5/*.inp; do
#endif
			outbase=$(basename $f .inp)
			$algorithm$sum $opt -p < $f > $outbase.out
#ifdef __APPLE__
			diff /AppleInternal/Tests/text_cmds/md5/$outbase.$algorithm$sum-p.chk $outbase.out || exitcode=1
#else
#			diff %%TESTSBASE%%/sbin/md5/$outbase.$algorithm$sum-p.chk $outbase.out || exitcode=1
#endif
		done
	done
}

testloop "" ""
testloop -q ""
testloop -qr ""
testloop "" sum
testloop -q sum

exit $exitcode
