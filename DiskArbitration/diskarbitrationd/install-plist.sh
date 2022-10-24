#!/bin/bash -e
 
inpath="$1"
outfile="$2"
 
mkdir -p $(dirname "$outfile")
 
xcrun clang -x c -P -E                          \
        -imacros TargetConditionals.h           \
        "$inpath"                               \
        -o "$outfile"
 
cat "$outfile"
 
if test $PLATFORM_NAME != macosx; then
    plutil -convert binary1 "$outfile"
else
    plutil -convert xml1    "$outfile"
fi
