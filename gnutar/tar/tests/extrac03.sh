#! /bin/sh
# Paths going up and down were inducing extraction loops.

. ./preset
. $srcdir/before

set -e
mkdir directory
tar cfv archive directory/../directory
echo -----
tar xfv archive

out="\
directory/../directory/
-----
directory/../directory/
"

. $srcdir/after
