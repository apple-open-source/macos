#!/bin/sh

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
   file "${archive}" | grep 'Mach-O fat file' > /dev/null; then

    # File is fat. Undo ranlib.
    unranlib "${archive}";
fi

ar $*;
ranlib $2;
