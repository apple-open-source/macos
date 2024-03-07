#!/bin/sh
#
# Copyright (c) April 1996 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# makewhatis.local - start makewhatis(1) only for file systems 
#		     physically mounted on the system
#
# Running makewhatis from /etc/periodic/weekly/320.whatis for rw nfs-mounted
# /usr may kill your NFS server -- all clients start makewhatis at the same
# time! So use this wrapper instead calling makewhatis directly.
#
# PS: this wrapper works also for catman(1)
#
# $FreeBSD: src/usr.bin/makewhatis/makewhatis.local.sh,v 1.7 1999/08/27 23:36:10 peter Exp $
PATH=/bin:/usr/bin:/usr/libexec:$PATH; export PATH
opt= dirs= localdirs= stdout=

for arg
do
	case "$arg" in
        "-o /dev/fd/1")
			opt="$opt $arg"
            stdout=1;;
		-*) opt="$opt $arg";;
		*)	dirs="$dirs:$arg";;
	esac
done

if [ -z "$stdout" ]; then
    dirs=`echo $dirs | awk -F: '{ for (i = 1; i <= NF; i++) { if ($i !~ "\\.app/" && $i !~ "/CommandLineTools/") { printf $i ":" } } printf "\n" }'`
fi
case X"$dirs" in X) echo "usage: $0 [options] directories ..."; exit 1;; esac

IFS=:
set -- $dirs
for dir in $@; do
	if [ -z "$dir" ]; then
		continue
	fi

	# $dirs is colon-separated to avoid issues with spaces... rather than
	# trying to do gymnastics to just make one call to find(1), we'll exec
	# once per dir to keep it relatively simple.
	unset IFS
	localdir=`find -H "$dir" -fstype local -type d -prune -print`
	if [ -n "$localdir" ]; then
		localdirs="$localdirs:$localdir"
	fi
	IFS=:
done
unset IFS

case X"$localdirs" in
	X) 	echo "$0: no local-mounted manual directories found: $dirs"
		exit 1;;
	*)
		localdirs=${localdirs##:}
		mopt=
		if [ -n "$opt" ]; then
			# Colon separate the arguments we were provided, just to
			# simplify life below since we continue using colons to
			# separate directory names.  -i / -n / -o arguments to
			# makewhatis using colons is somewhat unlikely.
			set -- $opt
			mopt="$1"
			shift
			for arg in $@; do
				mopt="$mopt:$arg"
			done

			# We set $mopt up to just be glued to the beginning of
			# $localdirs to simplify the logic below.  It's either
			# an empty string, or a series of :-separated args that
			# also ends in an : to denote where paths begin.
			mopt="$mopt:"
		fi

		IFS=:
		exec `basename $0 .local` $mopt$localdirs;;
esac
