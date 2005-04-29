####################################################################
#                                                                  #
#             This software is part of the ast package             #
#                Copyright (c) 1985-2004 AT&T Corp.                #
#        and it may only be used by you under license from         #
#                       AT&T Corp. ("AT&T")                        #
#         A copy of the Source Code Agreement is available         #
#                at the AT&T Internet web site URL                 #
#                                                                  #
#       http://www.research.att.com/sw/license/ast-open.html       #
#                                                                  #
#    If you have copied or used this software without agreeing     #
#        to the terms of the license you are infringing on         #
#           the license and copyright and are violating            #
#               AT&T's intellectual property rights.               #
#                                                                  #
#            Information and Software Systems Research             #
#                        AT&T Labs Research                        #
#                         Florham Park NJ                          #
#                                                                  #
#               Glenn Fowler <gsf@research.att.com>                #
#                David Korn <dgk@research.att.com>                 #
#                 Phong Vo <kpv@research.att.com>                  #
#                                                                  #
####################################################################
: generate "<sys/param.h> + <sys/types.h> + <sys/stat.h>" include sequence
case $# in
0)	;;
*)	eval $1
	shift
	;;
esac
for i in "#include <sys/param.h>" "#include <sys/param.h>
#ifndef S_IFDIR
#include <sys/stat.h>
#endif" "#include <sys/param.h>
#ifndef S_IFDIR
#include <sys/types.h>
#include <sys/stat.h>
#endif" "#ifndef S_IFDIR
#include <sys/types.h>
#include <sys/stat.h>
#endif"
do	echo "$i
struct stat V_stat_V;
F_stat_F() { V_stat_V.st_mode = 0; }" > $tmp.c
	if	$cc -c $tmp.c >/dev/null
	then	echo "$i"
		break
	fi
done
