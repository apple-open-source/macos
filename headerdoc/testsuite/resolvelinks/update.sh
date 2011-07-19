#!/bin/sh

OLDDIR="$PWD"

if [ "$(echo "$1" | grep '^\/')" != "" ] ; then
	TESTPATH="$1"
else
	TESTPATH="$PWD/tests/$1"
fi

if [ "$UPDATE_SCRIPT" != "" ] ; then
	cd "$(dirname "$UPDATE_SCRIPT")"
fi

UPDATE=1 ./runtests.sh "$TESTPATH".rltest

find /tmp/headerdoc_temptestdir -name CVS -exec rm -rf {} \; > /dev/null 2>&1

if [ -d "$TESTPATH".expected/ ] ; then
	cd "$TESTPATH".expected/
	find . -name CVS -exec mv {} /tmp/headerdoc_temptestdir/{} \; > /dev/null 2>&1
	cd ../..
fi

rm -rf "$TESTPATH".expected
mv /tmp/headerdoc_temptestdir "$TESTPATH".expected

cd "$OLDDIR"

