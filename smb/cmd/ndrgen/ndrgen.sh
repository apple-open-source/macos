#!/bin/ksh -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Portions Copyright 2008 Apple, Inc.  All rights reserved.
# Use is subject to license terms.
#
#

# This is a wrapper script around the ndrgen compiler (ndrgen1).
# CC must be defined in the environment or on the command line.

NDRPROG="${NDRPROG:-${ROOT}/usr/libexec/smbsrv/ndrgen1}"
INCDIR="${INCDIR:-${ROOT}/usr/include/smbsrv}"
CC="${CC:-/usr/bin/cc}"

PROGNAME=`basename $0`

ndrgen_usage()
{
	if [[ $1 != "" ]] ; then
		print "$PROGNAME: ERROR: $1"
	fi

	echo "usage: $PROGNAME [-Y cpp-path] [-o output] file"
	exit 1
}

ndrgen_cleanup()
{
    rm -f "$TMP_NAME"
    rm -f "$BASENAME"
}

ndrgen_error()
{
    rm -f "$OUTFILE"
}

if [[ $# -lt 1 ]] ; then
	ndrgen_usage
fi

while getopts "Yo:" FLAG $*; do
	case $FLAG in
	Y) shift; CC="$1";;
	o) shift; OUTFILE="$1";;
	*) ndrgen_usage ;;
	esac
	shift
done

INFILE="$1"

if [[ $CC = "" ]] ; then
	ndrgen_usage "C pre-processor is not defined"
fi

if [ ! -f $CC ] || [ ! -x $CC ] ; then
	ndrgen_usage "cannot run $CC"
fi

# Alwys remove temp files. Only remove output on signal.
trap "ndrgen_cleanup" 0
trap "ndrgen_cleanup ; ndrgen_error" 1 2 3 15

if [[ ! -r ${INFILE} ]] ; then
	print "$PROGNAME: ERROR: cannot read $INFILE"
	exit 1
fi

BASENAME=`basename "$INFILE" .ndl`

if [[ $OUTFILE = "" ]] ; then
	OUTFILE="${BASENAME}_ndr.c"
fi

TMP_NAME=$(dirname "$OUTFILE")"/.$BASENAME.$$.ndl.c"
RAWFILE=$(dirname "$OUTFILE")"/.${BASENAME}.$$.raw"

cp "$INFILE" "$TMP_NAME"

if $CC $CPPFLAGS -E -D__a64 -D__EXTENSIONS__ -D_FILE_OFFSET_BITS=64 \
	-I. "-I${INCDIR}" "-I${INCDIR}/ndl" -DNDRGEN "$TMP_NAME" | \
	$NDRPROG > "$RAWFILE"
then
	cat - << EOF > "$OUTFILE"
/*
* Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
* Use is subject to license terms.
*/

/*
* THIS FILE IS GENERATED. DO NOT EDIT IT
*/
#include <strings.h>
#include <libmlrpc/ndr.h>
#include <ndl/$BASENAME.ndl>
EOF

	cat "$RAWFILE" >> "$OUTFILE"
	ndrgen_cleanup
else
	ndrgen_cleanup
	exit 1
fi

