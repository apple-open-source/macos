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
: generate preroot features
case $# in
0)	;;
*)	eval $1
	shift
	;;
esac
if	/etc/preroot / /bin/echo >/dev/null
then	cat <<!
#pragma prototyped

#define FS_PREROOT	1			/* preroot enabled	*/
#define PR_BASE		"CCS"			/* preroot base env var	*/
#define PR_COMMAND	"/etc/preroot"		/* the preroot command	*/
#define PR_REAL		"/dev/.."		/* real root pathname	*/
#define PR_SILENT	"CCSQUIET"		/* no command trace	*/

extern char*		getpreroot(char*, const char*);
extern int		ispreroot(const char*);
extern int		realopen(const char*, int, int);
extern void		setpreroot(char**, const char*);

!
else	echo "/* preroot not enabled */"
fi
