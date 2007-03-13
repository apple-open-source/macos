#!/bin/sh

# usage: merge-libs.sh SRCDIR DSTDIR

merge_one () {
  srcdir="$1"
  destdir="$2"

  for f in `ls $srcdir`; do
    srcfile="$srcdir/$f"
    destfile="$destdir/$f"

    if [ -d "$srcfile" ]; then
      if [ -d "$destfile" ]; then
	merge_one "$srcfile" "$destfile"
      fi
    else
      if [ -f "$destfile" ]; then
	# ignore the symlinks
	if [ ! -L "$destfile" ]; then
	  # lipo the two files together
	  lipo -create -output /tmp/foo.$$ "$srcfile" "$destfile" \
	      && rm -f "$destfile" && mv /tmp/foo.$$ "$destfile" \
	      && echo "$destfile"
	fi
      fi
    fi
  done
}

merge_one "$1" "$2"
