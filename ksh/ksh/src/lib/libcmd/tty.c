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
 * tty
 */

static const char usage[] =
"[-?\n@(#)$Id: tty (AT&T Labs Research) 1999-04-10 $\n]"
USAGE_LICENSE
"[+NAME?tty - write the name of the terminal to standard output]"
"[+DESCRIPTION?\btty\b writes the name of the terminal that is connected "
	"to standard input onto standard output.  If standard input is not "
	"a terminal, \"\bnot a tty\b\" will be written to standard output.]"
"[s:silent|quiet?Don't write anything, just return exit status.  This option "
	"is obsolete.]"
"[+EXIT STATUS?]{"
        "[+0?Standard input is a tty.]"
        "[+1?Standard input is not a tty.]"
        "[+2?Invalid arguments.]"
        "[+3?A an error occurred.]"
"}"
;


#include <cmdlib.h>

int
b_tty(int argc, char *argv[], void* context)
{
	register int n,sflag=0;
	register char *tty;

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
	case 's':
		sflag++;
		break;
	case ':':
		error(2, "%s", opt_info.arg);
		break;
	case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	if(error_info.errors)
		error(ERROR_usage(2), "%s", optusage(NiL));
	if(!(tty=ttyname(0)))
	{
		tty = "not a tty";
		error_info.errors++;
	}
	if(!sflag)
		sfputr(sfstdout,tty,'\n');
	return(error_info.errors);
}

