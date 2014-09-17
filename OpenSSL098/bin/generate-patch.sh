#!/bin/sh

# This script creates a patch based on the current changes in the "src" directory that aren't already accounted for.

SCRIPTDIR=$( dirname "$0" )
pushd "$SCRIPTDIR" > /dev/null
cd ..

EXTDIR=/tmp/out.$$
mkdir -p $EXTDIR

VERSION=$( cat src/.version )

rm -rf ${EXTDIR}/src
bin/extract-source.sh $EXTDIR/

# remove symlinks since we can't diff them

ditto src /tmp/oldsrc.$$ || exit 1
cd /tmp/oldsrc.$$ || exit 1
find . ${EXTDIR}/src -type l -exec rm -f {} \;

# generate the patch

diff -Nur $EXTDIR/src/ . | sed "s|$EXTDIR|openssl-$VERSION|" > /tmp/$$.patch
rm -rf /tmp/oldsrc.$$ $EXTDIR

LINES=$( wc -l < /tmp/$$.patch )

if [ $LINES == 0 ]; then
   rm -f /tmp/$$.patch
   echo "No local diffs"
else
   PATCHFILE="$1"
   if [ "$PATCHFILE" != "" ]; then
      popd > /dev/null
      mv /tmp/$$.patch "$PATCHFILE"
   else
      PATCHFILE=/tmp/$$.patch
   fi

   echo "Patch in $PATCHFILE"
fi
