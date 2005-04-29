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
 * output the beginning portion of one or more files
 */

static const char usage[] =
"[-?\n@(#)$Id: head (AT&T Labs Research) 2004-01-05 $\n]"
USAGE_LICENSE
"[+NAME?head - output beginning portion of one or more files ]"
"[+DESCRIPTION?\bhead\b copies one or more input files to standard output "
	"stopping at a designated point for each file or to the end of the "
	"file whichever comes first.  Copying ends "
	"at the point indicated by the options.  By default a header "
	"of the form \b==> \b\afilename\a\b <==\b "
	"is output before all but the first file but this can be changed "
	"with the \b-q\b and \b-v\b options.]"
"[+?If no \afile\a is given, or if the \afile\a is \b-\b, \bhead\b "
	"copies from standard input starting at the current location.]"
"[+?The option argument for \b-c\b, and \b-s\b can optionally be "
	"followed by one of the following characters to specify a different "
	"unit other than a single byte:]{"
		"[+b?512 bytes.]"
		"[+k?1-killobyte.]"
		"[+m?1-megabyte.]"
	"}"
"[+?For backwards compatibility, \b-\b\anumber\a  is equivalent to "
	" \b-n\b \anumber\a.]"

"[n:lines]#[lines:=10?Copy \alines\a lines from each file.]"
"[c:bytes]#[chars?Copy \achars\a bytes from each file.]"
"[q:quiet|silent?Never ouput filename headers.]"
"[s:skip]#[skip?Skip \askip\a characters or lines from each file before "
	"copying.]"
"[v:verbose?Always ouput filename headers.]"
"\n"
"\n[file ...]\n"
"\n"
"[+EXIT STATUS?]{"
	"[+0?All files copied successfully.]"
	"[+>0?One or more files did not copy.]"
"}"
"[+SEE ALSO?\bcat\b(1), \btail\b(1)]"
;

#include <cmdlib.h>

int
b_head(int argc, register  char *argv[], void* context)
{
	static char header_fmt[] = "\n==> %s <==\n";
	register Sfio_t	*fp;
	register char		*cp;
	register long		number = 10;
	register off_t		skip = 0;
	register int		n;
	register int		delim = '\n';
	int			header = 1;
	char			*format = header_fmt+1;

	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
	case 'c':
		delim = -1;
		/* FALL THRU */
	case 'n':
		if((number = opt_info.num) <=0)
			error(2, "%c: %d: option requires positive number", n, number);
		break;
	case 'q':
		header = argc;
		break;
	case 'v':
		header = 0;
		break;
	case 's':
		skip = opt_info.number;
		break;
	case ':':
		error(2, "%s", opt_info.arg);
		break;
	case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	argc -= opt_info.index;
	if(error_info.errors)
		error(ERROR_usage(2), "%s", optusage(NiL));
	if(cp = *argv)
		argv++;
	do
	{
		if(!cp || streq(cp,"-"))
		{
			fp = sfstdin;
			sfset(fp, SF_SHARE, 1);
		}
		else if(!(fp = sfopen(NiL,cp,"r")))
		{
			error(ERROR_system(0),"%s: cannot open",cp);
			error_info.errors = 1;
			continue;
		}
		if(argc>header)
			sfprintf(sfstdout,format,cp);
		format = header_fmt;
		if(skip>0)
			sfmove(fp,NiL,skip,delim);
		sfmove(fp, sfstdout,number,delim);
		if(fp!=sfstdin)
			sfclose(fp);
	}
	while(cp= *argv++);
	return(error_info.errors);
}
