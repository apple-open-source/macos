#!/bin/sh
# Usage: xsmallpot.sh hello-foo [hello-foobar.pot]

set -e

test $# = 1 || test $# = 2 || { echo "Usage: xsmallpot.sh hello-foo [hello-foobar.pot]" 1>&2; exit 1; }
directory=$1
potfile=${2-$directory.pot}

cd ..
rm -rf tmp-$directory
cp -p -r $directory tmp-$directory
cd tmp-$directory
case $directory in
  hello-c++-kde)
    ./autogen.sh
    sed -e 's,tmp-,,' < configure.in > configure.ac
    grep '^\(AC_INIT\|AC_CONFIG\|AM_INIT\|AM_CONDITIONAL\|AM_GNU_GETTEXT\|AM_PO_SUBDIRS\|AC_OUTPUT\)' configure.ac > configure.in
    rm -f configure.ac 
    autoconf
    ./configure
    ;;
  hello-objc-gnustep)
    ./autogen.sh
    ;;
  *)
    grep '^\(AC_INIT\|AC_CONFIG\|AM_INIT\|AM_CONDITIONAL\|AM_GNU_GETTEXT\|AM_PO_SUBDIRS\|AC_OUTPUT\)' configure.ac > configure.in
    rm -f configure.ac 
    ./autogen.sh
    ./configure
    ;;
esac
cd po
make $potfile
sed -e "/^#:/ {
s, \\([^ ]\\), $directory/\\1,g
}" < $potfile > ../../po/$potfile
cd ..
cd ..
rm -rf tmp-$directory
