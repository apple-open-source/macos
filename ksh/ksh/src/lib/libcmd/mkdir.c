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
 * mkdir
 */

static const char usage[] =
"[-?\n@(#)$Id: mkdir (AT&T Labs Research) 2001-10-31 $\n]"
USAGE_LICENSE
"[+NAME?mkdir - make directories]"
"[+DESCRIPTION?\bmkdir\b creates one or more directories.  By "
	"default, the mode of created directories is \ba=rwx\b minus the "
	"bits set in the \bumask\b(1).]"
"[m:mode]:[mode?Set the mode of created directories to \amode\a.  "
	"\amode\a is symbolic or octal mode as in \bchmod\b(1).  Relative "
	"modes assume an initial mode of \ba=rwx\b.]"
"[p:parents?Ensure  that  each  given directory exists.  Create "
	"any missing parent directories for  each  argument.  "
	"Parent directories default to the umask modified by "
	"\bu+wx\b.  Do not consider an argument directory that "
	"already exists to be an error.]"
"\n"
"\ndirectory ...\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?All directories created successfully, or the \b-p\b option "
	"was specified and all the specified directories now exist.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bchmod\b(1), \brmdir\b(1), \bumask\b(1)]"
;



#include <cmdlib.h>
#include <ls.h>

#define DIRMODE	(S_IRWXU|S_IRWXG|S_IRWXO)

int
b_mkdir(int argc, char** argv, void* context)
{
	register char*	arg;
	register int	n;
	register mode_t	mode = DIRMODE;
	register mode_t	mask = 0;
	register int	mflag = 0;
	register int	pflag = 0;
	char*		name;
	mode_t		dmode;

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
	case 'p':
		pflag = 1;
		break;
	case 'm':
		mflag = 1;
		mode = strperm(arg = opt_info.arg, &opt_info.arg, mode);
		if (*opt_info.arg)
			error(ERROR_exit(0), "%s: invalid mode", arg);
		break;
	case ':':
		error(2, "%s", opt_info.arg);
		break;
	case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if (error_info.errors || !*argv)
		error(ERROR_usage(2), "%s", optusage(NiL));
	mask = umask(0);
	if (mflag || pflag)
	{
		dmode = (DIRMODE & ~mask) | S_IWUSR | S_IXUSR;
		if (!mflag)
			mode = dmode;
	}
	else
	{
		mode &= ~mask;
		umask(mask);
		mask = 0;
	}
	while (arg = *argv++)
	{
		if (mkdir(arg, mode) < 0)
		{
			if (!pflag || !(errno == ENOENT || errno == EEXIST || errno == ENOTDIR))
			{
				error(ERROR_system(0), "%s:", arg);
				continue;
			}
			if (errno == EEXIST)
				continue;

			/*
			 * -p option, preserve intermediates
			 * first eliminate trailing /'s
			 */

			n = strlen(arg);
			while (n > 0 && arg[--n] == '/');
			arg[n + 1] = 0;
			for (name = arg, n = *arg; n;)
			{
				/* skip over slashes */
				while (*arg == '/')
					arg++;
				/* skip to next component */
				while ((n = *arg) && n != '/')
					arg++;
				*arg = 0;
				if (mkdir(name, n ? dmode : mode) < 0 && errno != EEXIST && access(name, F_OK) < 0)
				{
					*arg = n;
					error(ERROR_system(0), "%s:", name);
					break;
				}
				*arg = n;
			}
		}
	}
	if (mask)
		umask(mask);
	return error_info.errors != 0;
}
