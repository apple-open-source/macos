#!/bin/sh

mode=$1
shift

f="$1"
src="$2"
dst="$3"

if [ $mode = build -o $mode = install ] ; then
    if ! cmp "$src/$f" "$dst/$f" 2>/dev/null ; then
        cp "$src/$f" "$dst/$f" || exit 1
    fi
elif [ $mode = clean ] ; then
    rm -f "$dst/$f"
fi

exit 0
