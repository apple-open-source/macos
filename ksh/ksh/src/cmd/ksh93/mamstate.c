/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1982-2004 AT&T Corp.                *
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
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
/*
 * AT&T Bell Laboratories
 * make abstract machine file state support
 *
 * mamstate reference [ file ... | <files ]
 *
 * stdout is list of <file,delta> pairs where delta
 * is diff between reference and file times
 * non-existent files are not listed
 */

#if !lint
static char id[] = "\n@(#)$Id: mamstate (AT&T Bell Laboratories) 1989-06-26 $\0\n";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

main(argc, argv)
int		argc;
register char**	argv;
{
	register char*	s;
	register int	c;
	long		ref;
	struct stat	st;
	char		buf[1024];

	if (!(s = *++argv) || stat(s, &st))
	{
		fprintf(stderr, "Usage: mamstate reference [ file ... | <files ]\n");
		exit(1);
	}
	ref = (long)st.st_mtime;
	if (s = *++argv) do
	{
		if (!stat(s, &st))
			printf("%s %ld\n", s, (long)st.st_mtime - ref);
	} while (s = *++argv);
	else do
	{
		s = buf;
		while ((c = getchar()) != EOF && c != ' ' && c != '\n')
			if (s < buf + sizeof(buf) - 1) *s++ = c;
		if (s > buf)
		{
			*s = 0;
			if (!stat(buf, &st))
				printf("%s %ld\n", buf, (long)st.st_mtime - ref);
		}
	} while (c != EOF);
	exit(0);
}
