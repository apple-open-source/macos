#!/bin/sh
# Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
# Wrapper around ar which behaves more like ar.
# Problem is Rhapsody's ar doesn't work on a file that's been ranlib'ed
# and some makefiles want to edit ranlib'ed archives.
#
# The interesting and functional routine here in unranlib().
# The "main" code, which wraps ar, is a hack and may not parse the
# arguments correctly, but seems to work for most uses of ar, where
# the library is argv[2].
##
# Wilfredo Sanchez Jr. | wsanchez@apple.com
# Copyright 1998 Apple Computer, Inc.
##

##
# Set up PATH
##

MyPath=/usr/bin:/bin;

if [ -z "${PATH}" ]; then
    export PATH=${MyPath};
else
    export PATH=${PATH}:${MyPath};
fi

##
# Functions
##

unranlib ()
{
    local archive;

    for archive in $*; do

	local   name="$(basename ${archive})";
	local    dir="/tmp/unranlib.$$/${name}";
	local ofiles="";
	local  archs="$(file ${archive}			| \
			grep '(for architecture'	| \
			awk '{print $4}'		| \
			sed 's/)://')";

	for arch in ${archs}; do
	    local archdir="${dir}/${arch}";
	    mkdir -p "${archdir}";

	    lipo -thin "${arch}" "${archive}" -o "${archdir}/${name}";

	    ( cd "${archdir}" && ar -xo "./${name}"; );

	    local ofile;
	    for ofile in `find "${archdir}" -name \*.o`; do
		ofiles="${ofiles} $(basename ${ofile})";
	    done

	done

	ofiles=$(echo ${ofiles} | tr ' ' '\012' | sort | uniq);

	local ofile;
	for ofile in ${ofiles}; do
	    lipo -create $(find "${dir}" -name "${ofile}" -print) -o "${dir}/${ofile}";
	done

	( cd "${dir}" && ar -cr "${name}" ${ofiles}; );

	mv "${dir}/${name}" "${archive}";

	rm -rf "${dir}";

    done

    rm -rf "/tmp/unranlib.$$";
}

##
# Handle command line
##

# This is totally bogus, but enough for now.
archive=$2;

if [ -f "${archive}" ] &&
   file "${archive}" | grep -E 'Mach-O (fat file|universal binary)' > /dev/null; then

    # File is fat. Undo ranlib.
    unranlib "${archive}";
fi

ar $*;
