#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Script to apply a quilt patch series without requiring quilt to be
# installed. Pays attention to the patch level in the series file.

PROGNAME=$(basename $0)
SCRIPTBASE=$(cd $( dirname $0) && pwd)

. $SCRIPTBASE/common.sh

SERIES=$(cd $(dirname $0) && pwd)/../patches/series
SRCROOT=${SRCROOT:-"$SCRIPTBASE/.."}

function error {
	echo $PROGNAME "$@" 2>&-
	exit 2
}

function strip_comments {
	sed '-es/#.*//'
}

function strip_blank_lines {
	grep -v '^$'
}

function apply_patch {
	local pname="$1"
	local plevel="$2"
	
	rm -f .rejects
	echo applying patch $pname with $plevel
	patch --reject-file=.rejects \
		$plevel \
		-i "patches/$pname" | indent
		
	if [ "$?" != 0 -o -r .rejects ]; then
		rm -f .rejects
		error patch $pname FAILED
	fi
}


[[ -r $SERIES ]] || error "can't find series file"

cd $SRCROOT || error "can't chdir to $SRCROOT"
cat $SERIES | \
	strip_comments | \
	strip_blank_lines | \
	while read patchname patchlevel ; do
		patchlevel=${patchlevel:-"-p1"}
		apply_patch $patchname $patchlevel
	done
	

