#!/bin/sh

pwd=`pwd`
name=`basename $(pwd)`
echo "name=$name"

make distclean
cd ..
tar zcvf ./$name.tar.gz \
    `find ./$name -type f -o -type l| fgrep -v .git| fgrep -v CVS | fgrep -v SCCS | \
	fgrep -v Attic | fgrep -v "autom4te"| fgrep -v "~"|fgrep -v .db | \
	fgrep -v .junk | fgrep -v .orig | fgrep -v "#" |fgrep -v .DS_Store| fgrep -v config. | \
        fgrep -v .gdb`

