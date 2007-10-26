#! /bin/bash

# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# disk-images.sh - Create and destroy disk images

ASROOT=${ASROOT:-sudo}

# Usage: make_disk_image 5g image.dmg
make_disk_image()
{
    local size="$1"
    local filename="$2"

    # Make sure that IMAGE is foo.dmg
    case $filename in
	*.dmg) ;;
	*) filename="$filename.dmg" ;;
    esac

    local volname=$(echo $filename | sed '-es/.dmg//')

    # Blow away an old image file so that hdiutil doesn't spuriously fail.
    rm -f "$filename"

    hdiutil create -fs HFSJ -type SPARSE \
	-size "$size" -volname "$volname" "$filename"
}

# Usage: path=$(mount_disk_image image.dmg) || error
mount_disk_image()
{
    local filename="$1"

    # Make sure that IMAGE is foo.dmg
    case $filename in
	*.dmg) ;;
	*) filename="$filename.dmg" ;;
    esac

    local volname=$(echo $filename | sed '-es/.dmg//')

    # hdiutil always appends .sparseimage if we used -type SPARSE
    [ -r "$filename" ] || filename="$filename.sparseimage"

    if hdiutil attach "$filename" \
	-readwrite -mount required -owners on >/dev/null 2>&1 ; then

	# tell spotlight not not index this
	$ASROOT mdutil -v -i off /Volumes/$volname >/dev/null 2>&1

	# Tell callers where this was mounted
	echo /Volumes/$volname
    else
	false
    fi
}

# Usage: unmount_disk_image image.dmg
unmount_disk_image()
{
    local filename="$1"

    # Make sure that IMAGE is foo.dmg
    case $filename in
	*.dmg) ;;
	*) filename="$filename.dmg" ;;
    esac

    local volname=$(echo $filename | sed '-es/.dmg//')

    # hdiutil always appends .sparseimage if we used -type SPARSE
    [ -r "$filename" ] || filename="$filename.sparseimage"

    $ASROOT quotaoff -ugv /Volumes/$volname

    # This does whatever magic is needed to stop mds and fseventsd.
    hdiutil detach /Volumes/$volname
    rm -f $filename

    true
}

