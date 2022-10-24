#!/bin/sh -
#
# Copyright (c) 2022 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
#
# @APPLE_LICENSE_HEADER_END@

infile=/dev/stdin
outfile=/dev/stdout
bpos=0
dodecode="0"
decodeflag=""

usage() {
	echo 'Usage:	base64 [-hDd] [-b num] [-i in_file] [-o out_file]
  -h, --help     display this message
  -Dd, --decode   decodes input
  -b, --break    break encoded string into num character lines
  -i, --input    input file (default: "-" for stdin)
  -o, --output   output file (default: "-" for stdout)' >&2

	if [ ! -z "$1" ]; then
		exit "$1"
	fi
}

while :; do
	if [ "$#" -eq 0 ]; then
		break
	fi

	case "$1" in
	-b)
		bpos="$2"
		shift;shift;;

	--break=*|--breaks=*)
		bpos="${1#--break*=}"
		shift;;

	-d|-D|--decode)
		dodecode="1"
		shift;;

	-h|--help)
		usage 0 2>&1
		;;

	-i)
		infile="$2"
		shift;shift;;

	--input=*)
		infile="${1#--input=}"
		shift;;

	-o)
		outfile="$2"
		shift;shift;;

	--output=*)
		outfile="${1#--output=}"
		shift;;

	*)
		echo "base64: invalid argument $1" >&2
		usage 64
		break;;
	esac
done

if [ "$infile" = "-" ]; then
	infile="/dev/stdin"
fi

if [ "$outfile" = "-" ]; then
	outfile="/dev/stdout"
fi

ioerr() {
	ppid=$1

	1>&2 echo "$0: I/O error on input"

	kill -PIPE $ppid
}

if [ "$dodecode" = 1 ]; then
	# if decoding, never break the output
	bpos=0
	decodeflag="-d"

	# When we're decoding, we break the input up into smaller blocks
	# to keep OpenSSL happy -- based on experimentation, it wants to see
	# a newline within the first 80 bytes.  dd conv=unblock works for both
	# short and long inputs, as it'll always output a trailing newline, even
	# for short inputs.
	#
	# We avoid the -A flag to openssl because that breaks decoding
	# multi-line base64 text.
	inpipe="(dd if=\"$infile\" conv=unblock cbs=79 2>/dev/null || ioerr $$)"

	# openssl(1) doesn't support base64url, so we'll do the necessary
	# replacements here to shim it out.
	inpipe="$inpipe | sed -e 's,-,+,g' -e 's,_,/,g'"
else
	inpipe="(cat \"$infile\" || ioerr $$)"
fi

cmd="$inpipe | openssl enc -base64 $decodeflag"
if [ "$dodecode" -eq 0 ]; then
	# Decode should never be processed through paste(1), lest we decode
	# incorrectly if the original file had newline bytes.
	cmd="$cmd | paste -s -d '\0' -"
fi
if [ "$bpos" -ne 0 ]; then
	cmd="$cmd | dd conv=unblock cbs="$bpos" 2>/dev/null | sed -e '$d'"
fi
eval $cmd > "$outfile"
