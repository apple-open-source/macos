#!/bin/sh
########################################################################
#                                                                      #
#               This software is part of the ast package               #
#                     Copyright (c) 1994-2011 AT&T                     #
#                      and is licensed under the                       #
#                  Common Public License, Version 1.0                  #
#                               by AT&T                                #
#                                                                      #
#                A copy of the License is available at                 #
#            http://www.opensource.org/licenses/cpl1.0.txt             #
#         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         #
#                                                                      #
#              Information and Software Systems Research               #
#                            AT&T Research                             #
#                           Florham Park NJ                            #
#                                                                      #
#                 Glenn Fowler <gsf@research.att.com>                  #
#                                                                      #
########################################################################
: mkdir for systems that do not support -p : 2002-09-01 :
MKDIR=/bin/mkdir
CHMOD=chmod
mode=
parents=
while	:
do	case $1 in
	-m)	case $# in
		1)	echo "mkdir: -m: mode argument expected" >&2
			exit 1
			;;
		esac
		shift
		mode=$1
		;;
	-m*)	mode=`echo X$1 | sed 's/X-m//'`
		;;
	-p)	parents=1
		;;
	*)	break
		;;
	esac
	shift
done
if	test "" != "$parents"
then	for d
	do	if	test ! -d $d
		then	ifs=${IFS-'
	 '}
			IFS=/
			set '' $d
			IFS=$ifs
			shift
			dir=$1
			shift
			if	test -n "$dir" -a ! -d "$dir"
			then	$MKDIR "$dir" || exit 1
				if	test "" != "$mode"
				then	$CHMOD "$mode" "$dir" || exit 1
				fi
			fi
			for d
			do	dir=$dir/$d
				if	test ! -d "$dir"
				then	$MKDIR "$dir" || exit 1
					if	test "" != "$mode"
					then	$CHMOD "$mode" "$dir" || exit 1
					fi
				fi
			done
		fi
	done
else	$MKDIR "$@" || exit 1
	if	test "" != "$mode"
	then	for d
		do	$CHMOD "$mode" "$d" || exit 1
		done
	fi
fi
exit 0
