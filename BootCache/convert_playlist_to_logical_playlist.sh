#!/bin/bash

if [ $# -eq 0 ] || [ ! -f $1 ]; then
	echo "Usage: $0 <playlist> [partition]"
	exit 1
fi

if [ "x$2" = "x" ]; then
	dev=$(mount | grep "on / " | cut -d ' ' -f 1)
else
	dev=$2
fi

in=$1
out=${in%.playlist}.logical_playlist

echo "Using $dev as the root device"
echo "Converting $in to $out"

blocks=$(mktemp /tmp/blocks.XXXXXX)
files=$(mktemp /tmp/files.XXXXXX)
sorted=$(mktemp /tmp/sorted.XXXXXX)

echo -n "Getting blocks from $file..."
sudo BootCacheControl -f $in print | egrep '^([0-9A-F]){8}-' |\
       	awk '{for (i = 0;i < $3; i += 4096) {print ($2+i)/512}}' > $blocks
echo " done."

echo -n "Finding files on disk..."
sudo fsck_hfs -B $blocks -f $dev > $files
echo " done."

echo -n "Sorting..."
grep ROOT_OF_VOLUME $files | cut -d '"' -f 2 | sed 's/ROOT_OF_VOLUME//' | sort -u > $sorted
echo " done."

echo -n "Filtering out non files..."
(
	while read file; do
		if [ -f "$file" ]; then echo $file; fi
	done
) <$sorted >$out
echo " done."

rm $blocks
rm $files
rm $sorted
