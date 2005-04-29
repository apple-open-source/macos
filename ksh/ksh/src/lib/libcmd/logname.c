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
 * AT&T Research
 *
 * logname
 */

static const char usage[] =
"[-?\n@(#)$Id: logname (AT&T Labs Research) 1999-04-30 $\n]"
USAGE_LICENSE
"[+NAME?logname - return the user's login name]"
"[+DESCRIPTION?\blogname\b writes the users's login name to standard "
	"output.  The login name is the string that is returned by the "
	"\bgetlogin\b(2) function.  If \bgetlogin\b(2) does not return "
	"successfully, the corresponding to the real user id of the calling "
	"process is used instead.]"

"\n"
"\n\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Successful Completion.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bgetlogin\b(2)]"
;


#include <cmdlib.h>

int
b_logname(int argc, char** argv, void* context)
{
	register char*	logname;

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case ':':
			error(2, "%s", opt_info.arg);
			continue;
		case '?':
			error(ERROR_usage(2), "%s", opt_info.arg);
			continue;
		}
		break;
	}
	if (error_info.errors)
		error(ERROR_usage(2), "%s", optusage(NiL));
	if (!(logname = getlogin()))
		logname = fmtuid(getuid());
	sfputr(sfstdout, logname, '\n');
	return 0;
}

