#!/bin/sh

#
# This script processes the directory hierarchy
# passed to it and eliminates all source code,
# makefile fragments, and documentation that is
# not suitable for open source posting.
#

OPENSOURCE=1

DST=/tmp/hfs-open-source

rm -rf $DST
mkdir $DST
xcodebuild installsrc SRCROOT=$DST

SRCROOT="$DST"

if [ ! -d "${SRCROOT}" ]; then
    echo "Could not access ${SRCROOT}" 1>&2
    exit 1
fi


UNIFDEF_FLAGS=""
if [ "$OPENSOURCE" -eq 1 ]; then
    UNIFDEF_FLAGS="$UNIFDEF_FLAGS -D_OPEN_SOURCE_ -D__OPEN_SOURCE__ -U__arm__ -Uarm -UARM -U__ARM__ -U__arm64__ -Uarm64 -UARM64 -U__ARM64__ -UTARGET_OS_EMBEDDED -UHFS_CONFIG_KEY_ROLL"
fi

# From this point forward, all paths are ./-relative
cd "${SRCROOT}"

find -d . -name .open_source_exclude | while read f; do
    dir=`dirname $f`
    if [ -s $f ]; then
	cat $f | while read g; do
	    if [ -n "$g" ]; then
		echo "Removing $dir/$g (Listed in $f)"
		rm -f "$dir/$g" || exit 1
	    else
		echo "Bad entry '$g' in $f"
		exit 1
	    fi
	done
	if [ $? -ne 0 ]; then
	    exit 1
	fi
    else
	echo "Removing $dir (Contains empty $f)"
	rm -rf "$dir"
    fi
    rm -f "$f"
done

if [ $? -ne 0 ]; then
    # Propagate error from sub-shell invocation above
    exit 1
fi

function stripfile() {
    local extraflags="$1"
    local path="$2"

    unifdef $extraflags $UNIFDEF_FLAGS $path > $path.new
    if [ $? -eq 0  ]; then
	    # no change
	rm $path.new
    else
	if [ $? -eq 2 ]; then
	    echo "Problems parsing $path, removing..."
	    rm $path.new $path
	else
	    if [ -s $path.new ]; then
		echo "Modified $path"
		mv -f $path.new $path
	    else
		echo "Removing empty $path"
		rm -f $path.new $path
	    fi
	fi
    fi
}

# C/C++ Source files
find . \( -type f -o -type l \) -a \( -name "*.[chy]" -o -name "*.cpp" \) | while read f; do
	    stripfile "" "$f"
done

# Free-form plain text files
find . \( -type f -o -type l \) -a \( -name "*.[sS]" -o -name "*.sh" -o -name "README" -o -name "*.py" \) | while read f; do
	    stripfile "-t" "$f"
	    case "$f" in
		*.sh)
		    chmod +x "$f"
		    ;;
	    esac
done

# Remove project references
grep -i -v -E '(hfs_key_roll)' ./hfs.xcodeproj/project.pbxproj > ./hfs.xcodeproj/project.pbxproj.new
mv -f ./hfs.xcodeproj/project.pbxproj.new ./hfs.xcodeproj/project.pbxproj

# Check for remaining bad file names
BADFILES=`find . \( -name "*.arm*" -o -name "arm*" \) | xargs echo`;
if [ -n "$BADFILES" ]; then
    echo "Bad file names $BADFILES"
    exit 1
fi

# Check for remaining bad file contents
if grep -iEr '([^UD_]_?_OPEN_SOURCE_|XNU_HIDE_SEED|XNU_HIDE_HARDWARE|CONFIG_EMBEDDED)' .; then
    echo "cleanup FAILURE"
    exit 1
else
    echo "done"
    exit 0
fi
