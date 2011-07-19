#!/bin/sh
set -e
set -x

for X in ex rview rvim vi view vimdiff ; do
	ln -s vim "$DSTROOT"/usr/bin/"$X"
done
