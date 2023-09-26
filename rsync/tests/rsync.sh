#!/bin/sh

# Really basic smoke test, to make sure that the wrapper isn't interfering with
# normal rsync operations.

fails=0
basedir=$(pwd)

rsync --help > ${basedir}/rsync.help
if [ $? -ne 0 ]; then
	1>&2 echo "rsync --help failed"
	fails=$((fails + 1))
elif ! grep -q -- -archive ${basedir}/rsync.help; then
	# Choice of -archive is arbitrary, we just need a definitively rsync
	# specific option.
	1>&2 echo "rsync --help output incorrect"
	fails=$((fails + 1))
fi

mkdir foo
cd foo

mkdir a
touch a/{x,y,z}

ls -lR > ${basedir}/hier

rsync -avz a b > ${basedir}/rsync.out
if [ $? -ne 0 ]; then
	1>&2 echo "rsync -avz failed"
	fails=$((fails + 1))
fi

(cd b && ls -lR) > ${basedir}/hier.new

if ! cmp -s ${basedir}/hier ${basedir}/hier.new; then
	1>&2 echo "rsync -avz resulted in a different structure"
	fails=$((fails + 1))
fi

if [ "${fails}" -eq 0 ]; then
	echo "All tests passed"
else
	echo "${fails} tests failed"
fi

exit ${fails}
