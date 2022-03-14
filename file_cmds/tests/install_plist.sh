#!/bin/bash -e

inpath="$SRCROOT/$1"
outdir="$DSTROOT/$2"
outfile="$3"
outpath="$outdir/$outfile"

mkdir -p "$outdir"

xcrun clang -x c -P -E                  \
        -imacros TargetConditionals.h   \
        "$inpath"                       \
        -o "$outpath"

if test $PLATFORM_NAME != macosx; then
    plutil -convert binary1 "$outpath"
else
    plutil -convert xml1    "$outpath"
fi
