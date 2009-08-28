#!/bin/bash
v="$(/usr/bin/autoconf --version | \
    awk '/^autoconf / {v=$NF; split(v,a,/\./); if (a[1]==2 && a[2]>=61) print v; nextfile}')"
if [ -z "$v" ]; then
    echo "Invalid autoconf, version 2.61 or later required" >&2 && exit 1; fi
for f in "$@"; do
    if [ "$(basename $f)" != "configure" ]; then
	echo "Don't know how to regenerate $f" >&2 && continue; fi
    d=$(dirname $f) && cd $d && rm -rf autom4te.cache &&
    echo "Running autoconf-$v in $d" && /usr/bin/autoconf &&
    if grep -q 'ac_config_headers=' configure; then
	echo "Running autoheader-$v in $d" && /usr/bin/autoheader; fi &&
    rm -rf autom4te.cache && cd $OLDPWD
done
