#!/bin/sh

# Really basic smoke test.

fails=0
basedir=$(pwd)

echo "Machine: $(machine)"
echo "Arch: $(arch)"

rsync --help > ${basedir}/rsync.help
if [ $? -ne 0 ]; then
	1>&2 echo "rsync --help failed"
	fails=$((fails + 1))
elif ! grep -q -- --rsync-path ${basedir}/rsync.help; then
	# Choice of --rsync-path is arbitrary, we just need a definitively rsync
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

# Try making a big file and rsync'ing it (32-bit clean)
fullsz=$((1 + (4 * 1024 * 1024 * 1024)))
truncate -s "$fullsz" bigfile
bigsz=$(stat -f '%z' bigfile)
if [ "$bigsz" -ne "$fullsz" ]; then
	1>&2 echo "truncate(1) failed to create the big file"
	fails=$((fails + 1))
else
	rsync -Savz bigfile bigfile.out
	outsz=$(stat -f '%z' bigfile.out)
	if [ "$outsz" -ne "$bigsz" ]; then
		1>&2 echo "rsync should have produced $bigsz, but produced $outsz"
		fails=$((fails + 1))
	fi
fi

# Try an underflow in blk_match()
truncate -s 1 small
truncate -s 16 medium
if ! rsync -av --no-W small medium; then
	1>&2 echo "underflow triggered?  small -> medium failed"
	fails=$((fails + 1))
fi

if [ "${fails}" -eq 0 ]; then
	echo "All tests passed"
else
	echo "${fails} tests failed"
fi

exit ${fails}
