#!/bin/sh

# This script extracts an openssl tarball and applies all Apple-local changes.

OUTDIR="$1"

if [ ! -d "$OUTDIR" ]; then
   echo "Output directory '$OUTDIR' missing"
   exit 1
fi

if [ -d "$OUTDIR/src" ]; then
   echo "'$OUTDIR/src' already exists"
   exit 1
fi

pushd "$OUTDIR" > /dev/null
OUTDIR=$( pwd -P )
popd > /dev/null

SCRIPTDIR=$( dirname "$0" )
pushd "$SCRIPTDIR" > /dev/null
SCRIPTDIR=$( pwd -P )
popd > /dev/null

cd "$SCRIPTDIR/../patches/" || exit 1
PATCHES=$(echo *.patch)

# copy source

VER=$(ls "$SCRIPTDIR"/../openssl-*.tar.gz | sort | tail -1 | sed 's|^.*/openssl-\(.*\).tar.gz|\1|')

cd "$OUTDIR" || exit 1
tar xzf "$SCRIPTDIR/../openssl-$VER.tar.gz" || exit 1
mv "openssl-$VER" src
cd src
find . -type l -exec rm -f {} \; # remove symlinks; they'll be recreated as necessary by Configure

echo Remove IDEA src

# <rdar://problem/9396666> Code related to the IDEA encryption algorithm must be removed from OpenSSL before we ship
find . -type f | xargs grep -l OPENSSL_NO_IDEA | egrep -v 'mk1mf.pl|Makefile' | awk '{print "unifdef -DOPENSSL_NO_IDEA -o "$0" "$0}' | sh -

echo Applying patches...

for patch in $PATCHES; do
   echo "   $patch"
   patch -s -p0 < "$SCRIPTDIR/../patches/$patch" || exit 1
done

export PERL=/usr/bin/perl   # make sure we use the perl in /usr/bin

echo Configuring tools

./Configure --openssldir="/System/Library/OpenSSL" threads shared zlib no-hw no-idea enable-rc5 enable-mdc2 enable-seed darwin64-x86_64-cc > /tmp/config.$$.out
if [ "$?" != 0 ]; then
   cat /tmp/config.$$.out
   exit 1
fi
mv tools/c_rehash c_rehash

echo Configuring i386

./Configure --prefix=/usr --openssldir="/System/Library/OpenSSL" threads shared zlib no-hw no-idea enable-rc5 enable-mdc2 enable-seed darwin-i386-cc > /tmp/config.$$.out
if [ "$?" != 0 ]; then
   cat /tmp/config.$$.out
   exit 1
fi
mv crypto/opensslconf.h i386.h

echo Configuring x86_64

./Configure --prefix=/usr --openssldir="/System/Library/OpenSSL" threads shared zlib no-hw no-idea enable-rc5 enable-mdc2 enable-seed darwin64-x86_64-cc > /tmp/config.$$.out
if [ "$?" != 0 ]; then
   cat /tmp/config.$$.out
   exit 1
fi
mv crypto/opensslconf.h x86_64.h

echo Merging configs

diff --old-group-format='#ifdef __LP64__
%<#endif
' \
--new-group-format='#ifndef __LP64__
%>#endif
' \
--unchanged-group-format='%=' \
--changed-group-format='#ifdef __LP64__
%<#else
%>#endif
' x86_64.h i386.h > crypto/opensslconf.h
rm -f x86_64.h i386.h

# move c_rehash back

mv c_rehash tools/c_rehash

# we don't use these files and they just cause extraneous diffs, so remove them
rm -f Makefile
find . -name '*.orig' -or -name '*.bak' | xargs rm -f

rm -f /tmp/config.$$.out
echo $VER > .version
