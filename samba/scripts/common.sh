# Common utility functions
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

ASROOT=${ASROOT:-sudo}

# Run a command with optional echoing. We echo to stderr so that we don't
# interfere with pipelines.
vrun()
{
    [ "$QUIET" = "y" ] || echo "$@" 1>&2
    [ "$DRYRUN" = "y" ] || "$@"
}

indent()
{
    awk '{ print "\t"$0 }'
}

submsg()
{
    echo "$@" | indent
}

files_are_the_same()
{
    cmp "$1" "$2"
}

# Just line wc -l, except that we only report the total count
count_lines()
{
    wc -l "$@" | tail -1 | awk '{print $1}'
}

save_config_file()
{
    local path="$1"
    local base="$2"
    local newpath=$(dirname path)/$base-$(basename $path)

    if [ -e "$path" ]; then
	$ASROOT cp "$path" "$newpath"
    else
	# Create empty backup if the file is not there
	touch "$path"
	touch "$newpath"
    fi

    $ASROOT chmod 666 "$path"
}

restore_config_file()
{
    local path="$1"
    local base="$2"
    local newpath=$(dirname path)/$base-$(basename $path)

    if [ -s "$newpath" ]; then
	$ASROOT cp "$newpath" "$path"
	$ASROOT chmod 644 "$path"
    else
	# If we backed up an empty file, assume there was no original
	$ASROOT rm -f "$path"
    fi

    rm -f "$newpath"
}

create_smb_share()
{
    local name="$1"
    local path="$2"


    (
	set -e

	# FYI this can fail if there is already a record with a case variation
	# of the same name we are trying to create.
	local dscl=${DSCL:-"/usr/bin/dscl ."}
	$ASROOT $dscl -create "/SharePoints/$name" \
	    smb_shared 1

	$ASROOT $dscl -create "/SharePoints/$name" \
	    smb_name "$name"

	$ASROOT $dscl -create "/SharePoints/$name" \
	    directory_path "$path"
    )

    if [ "$?" = "0" ]; then
	$ASROOT /usr/libexec/samba/synchronize-shares --enable-guest
    else
	# Make sure we don't leave partial junk in the directory.
	local dscl=${DSCL:-"/usr/bin/dscl ."}
	$ASROOT $dscl -delete "/SharePoints/$name"
	false
    fi

}

remove_smb_share()
{
    local name="$1"

    local dscl=${DSCL:-"/usr/bin/dscl ."}

    $ASROOT $dscl -delete "/SharePoints/$name"  && \
    $ASROOT /usr/libexec/samba/synchronize-shares --enable-guest
}

# create_temp_file(testname)
create_temp_file()
{
    base=$(basename "$1")
    tag="$2"

    if [ "$tag" != "" ]; then
        base="$base.$tag"
    fi

    tmpfile=$(mktemp -t ${base})
    if [ $? -ne 0 ]; then
        false
    else
        echo $tmpfile
    fi

}

# create_temp_dir(testname)
create_temp_dir()
{
    base=$(basename "$1")
    tag="$2"

    if [ "$tag" != "" ]; then
        base="$base.$tag"
    fi

    tmpfile=$(mktemp -d -t ${base})
    if [ $? -ne 0 ]; then
        false
    else
        echo $tmpfile
    fi

}

# testerr(testname, message) - exit with error message
testerr()
{
    testname=$(basename "$1")
    message="$2"

    echo $testname FAILED "($message)"
    exit 2
}

# testok(testname, status) - print test result
testok()
{
    testname=$(basename "$1")
    result="$2"

    case "$result" in
	0) echo $testname PASSED ;;
	*) echo $testname FAILED "(exit code $result)" ;;
    esac

    exit $result
}

register_cleanup_handler()
{
    trap "$@" 0 1 2 3 15
}

