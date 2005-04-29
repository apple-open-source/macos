/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1992-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * David Korn
 * AT&T Bell Laboratories
 *
 * mkfifo
 */


static const char usage[] =
"[-?\n@(#)$Id: mkfifo (AT&T Labs Research) 1999-04-20 $\n]"
USAGE_LICENSE
"[+NAME?mkfifo - make FIFOs (named pipes)]"
"[+DESCRIPTION?\bmkfifo\b creates one or more FIFO's.  By "
	"default, the mode of created FIFO is \ba=rw\b minus the "
	"bits set in the \bumask\b(1).]"
"[m:mode]:[mode?Set the mode of created FIFO to \amode\a.  "
	"\amode\a is symbolic or octal mode as in \bchmod\b(1).  Relative "
	"modes assume an initial mode of \ba=rw\b.]"
"\n"
"\nfile ...\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?All FIFO's created successfully.]"
        "[+>0?One or more FIFO's could not be created.]"
"}"
"[+SEE ALSO?\bchmod\b(1), \bumask\b(1)]"
;

#include <cmdlib.h>
#include <ls.h>

#define RWALL	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

int
b_mkfifo(int argc, char *argv[], void* context)
{
	register char *arg;
	register mode_t mode=RWALL, mask=0;
	register int n;

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
	  case 'm':
		mode = strperm(arg=opt_info.arg,&opt_info.arg,mode);
		if(*opt_info.arg)
			error(ERROR_exit(0),"%s: invalid mode",arg);
		break;
	  case ':':
		error(2, "%s",opt_info.arg);
		break;
	  case '?':
		error(ERROR_usage(2), "%s",opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(error_info.errors || !*argv)
		error(ERROR_usage(2),"%s",optusage(NiL));
	while(arg = *argv++)
	{
		if(mkfifo(arg,mode) < 0)
			error(ERROR_system(0),"%s:",arg);
	}
	if(mask)
		umask(mask);
	return(error_info.errors!=0);
}

