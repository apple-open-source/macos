#!/bin/sh
# 
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
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
# 
# Mach Operating System
# Copyright (c) 1991,1990 Carnegie Mellon University
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
# 
# Carnegie Mellon requests users of this software to return to
# 
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
# 
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
#

C=${MIGCC-cc}
M=${MIGCOM-${NEXT_ROOT}/usr/libexec/migcom}

cppflags="-traditional-cpp -D__MACH30__"
migflags=
files=
arch=`/usr/bin/arch`

# If an argument to this shell script contains whitespace,
# then we will screw up.  migcom will see it as multiple arguments.
#
# As a special hack, if -i is specified first we don't pass -user to migcom.
# We do use the -user argument for the dependencies.
# In this case, the -user argument can have whitespace.

until [ $# -eq 0 ]
do
    case "$1" in
	-[dtqkKQvVtTrRsSlLxX] ) migflags="$migflags $1"; shift;;
	-i	) sawI=1; migflags="$migflags $1 $2"; shift; shift;;
	-user   ) user="$2"; if [ ! "${sawI-}" ]; then migflags="$migflags $1 $2"; fi; shift; shift;;
	-server ) server="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-header ) header="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-sheader ) sheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-iheader ) iheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-dheader ) dheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-arch ) arch="$2"; shift ; shift;;
	-maxonstack ) migflags="$migflags $1 $2"; shift; shift;;
	-split ) migflags="$migflags $1"; shift;;
	-MD ) sawMD=1; cppflags="$cppflags $1"; shift;;
	-cpp) shift; shift;;
	-cc) C=$2; shift; shift;;
	-migcom) M=$2; shift; shift;;
	-* ) cppflags="$cppflags $1"; shift;;
	* ) files="$files $1"; shift;;
    esac
done

for file in $files
do
    base="`basename "$file" .defs`"
    temp="$base".$$
    rm -f "$temp".c "$temp".d
    (echo '#line 1 '\""$file"\"; cat "$file") > "$temp".c
    $C -E -arch $arch $cppflags "$temp".c | $M  $migflags || exit
    if [ "$sawMD" -a -f "$temp".d ]
    then
	deps=
	s=
	rheader="${header-${base}.h}"
	if [ "$rheader" != /dev/null ]; then
		deps="${deps}${s}${rheader}"; s=" "
	fi
	ruser="${user-${base}User.c}"
	if [ "$ruser" != /dev/null ]; then
		deps="${deps}${s}${ruser}"; s=" "
	fi
	rserver="${server-${base}Server.c}"
	if [ "$rserver" != /dev/null ]; then
		deps="${deps}${s}${rserver}"; s=" "
	fi
	rsheader="${sheader-/dev/null}"
	if [ "$rsheader" != /dev/null ]; then
		deps="${deps}${s}${rsheader}"; s=" "
	fi
	riheader="${iheader-/dev/null}"
	if [ "$riheader" != /dev/null ]; then
		deps="${deps}${s}${riheader}"; s=" "
	fi
	rdheader="${dheader-/dev/null}"
	if [ "$rdheader" != /dev/null ]; then
		deps="${deps}${s}${rdheader}"; s=" "
	fi
	for target in ${deps}
	do
		sed -e 's;^'"${temp}"'.o[ 	]*:;'"${target}"':;' \
		    -e 's;: '"${temp}"'.c;: '"$file"';' \
		< "${temp}".d > "${target}".d
	done
	rm -f "$temp".d
    fi
    rm -f "$temp".c
done

exit 0

