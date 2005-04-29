#! /bin/sh

## Temporarily stop running this script until we have a chance to
## get cached symfiles working correctly again.
## jmolenda/2004-05-13
## Actually, run it up to the point where it deletes the old
## cache, since then gdb won't trip over it every time it
## launches.
## jingham/2004-06-08

if [ `id -u` != "0" ]; then
    echo "This program must be run as root."
    exit 1
fi

dir=/usr/libexec/gdb/symfiles
gdb=/usr/bin/gdb

echo -n "Removing current cache ... "
rm -rf "$dir"
mkdir -p "$dir"
echo "done"

exit 0

#echo -n "Finding libraries ... "

#libs="/usr/lib/dyld"

#for i in \
#    /System/Library/Frameworks/*.framework \
#    /System/Library/PrivateFrameworks/*.framework \
#    /System/Library/Frameworks/*.framework/Frameworks/*.framework \
#    /System/Library/PrivateFrameworks/*.framework/Frameworks/*.framework \
#    ; do
#    name=`basename $i .framework`
#    # FIXME: the first run of nm is to check that the file is a valid Mach-O file.  That's okay.
#    # The second is because gdb crashes when there are types in the cached symfile (Radar 3418798).
#    # So for now we just leave out all libraries that have any stabs.
#    if [ -f $i/$name ]; then
#        if nm "$i/$name" >/dev/null 2>&1 && ! nm -ap "$i/$name" | grep 'SO ' >/dev/null 2>&1
#        then
#	  libs="$libs $i/$name"
#        fi
#    fi
#done 

#for i in \
#    `find /usr/lib -name lib\*.dylib -type f` \
#    /System/Library/Frameworks/*.framework/Libraries/*.dylib \
#    ; do
#    # FIXME: see fixme above.
#    if nm "$i" >/dev/null 2>&1  && ! nm -ap "$i" | grep 'SO ' >/dev/null 2>&1
#    then
#      libs="$libs $i"
#    fi
#done 

#echo "done"

#for i in $libs; do
#    if [ `basename $i` = "dyld" ]; then
#       echo "sharedlibrary cache-symfile $i $dir __dyld_" >> /tmp/syms_$$.gdb
#    else
#       echo "sharedlibrary cache-symfile $i $dir" >> /tmp/syms_$$.gdb
#    fi
#done

#echo -n "Processing libraries ... "
#$gdb -nx --batch --command=/tmp/syms_$$.gdb
#echo "done"

#rm "/tmp/syms_$$.gdb"
