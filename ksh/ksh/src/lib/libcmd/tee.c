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
 * tee
 */

static const char usage[] =
"[-?\n@(#)$Id: tee (AT&T Labs Research) 1999-04-28 $\n]"
USAGE_LICENSE
"[+NAME?tee - duplicate standard input]"
"[+DESCRIPTION?\btee\b copies standard input to standard output "
	"and to zero or more files.  The options determine whether "
	"the specified files are overwritten or appended to.  The "
	"\btee\b utility does not buffer output.  If writes to any "
	"\afile\a fail, writes to other files continue although \btee\b "
	"will exit with a non-zero exit status.]"
"[+?The number of \afile\a operands that can be specified is limited "
	"by the underlying operating system.]"
"[a:append?Append the standard input to the given files rather "
	"than overwriting them.]"
"[i:ignore-interrupts?Ignore SIGINT signal.]"
"\n"
"\n[file ...]\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?All files copies successfully.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bcat\b(1), \bsignal\b(3)]"
;


#include <cmdlib.h>

#include <ls.h>
#include <sig.h>

struct tee
{
	Sfdisc_t	disc;
	int		fd[1];
};

/*
 * This discipline writes to each file in the list given in handle
 */

static ssize_t tee_write(Sfio_t* fp, const void* buf, size_t n, Sfdisc_t* handle)
{
	register const char*	bp;
	register const char*	ep;
	register int*		hp = ((struct tee*)handle)->fd;
	register int		fd = sffileno(fp);
	register ssize_t	r;

	do
	{
		bp = (const char*)buf;
		ep = bp + n;
		while (bp < ep)
		{
			if ((r = write(fd, bp, ep - bp)) <= 0)
				return(-1);
			bp += r;
		}
	} while ((fd = *hp++) >= 0);
	return(n);
}

static Sfdisc_t tee_disc = { 0, tee_write, 0, 0, 0 };

int
b_tee(int argc, register char** argv, void* context)
{
	register struct tee*	tp = 0;
	register int		oflag = O_WRONLY|O_TRUNC|O_CREAT|O_BINARY;
	register int		n;
	register int*		hp;
	register char*		cp;

	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
	case 'a':
		oflag &= ~O_TRUNC;
		oflag |= O_APPEND;
		break;
	case 'i':
		signal(SIGINT, SIG_IGN);
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
	argv += opt_info.index;
	argc -= opt_info.index;

	/*
	 * for backward compatibility
	 */

	if (*argv && streq(*argv, "-"))
	{
		signal(SIGINT, SIG_IGN);
		argv++;
		argc--;
	}
	if (argc > 0)
	{
		if (!(tp = (struct tee*)stakalloc(sizeof(struct tee) + argc * sizeof(int))))
			error(ERROR_exit(1), "no space");
		tp->disc = tee_disc;
		hp = tp->fd;
		while (cp = *argv++)
		{
			if ((*hp = open(cp, oflag, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0)
				error(ERROR_system(0), "%s: cannot create", cp);
			else hp++;
		}
		if (hp == tp->fd) tp = 0;
		else
		{
			*hp = -1;
			sfdisc(sfstdout, &tp->disc);
		}
	}
	if (sfmove(sfstdin, sfstdout, SF_UNBOUND, -1) < 0 || !sfeof(sfstdin) || sfsync(sfstdout))
		error(ERROR_system(1), "cannot copy");

	/*
	 * close files and free resources
	 */

	if (tp)
	{
		sfdisc(sfstdout, NiL);
		for(hp = tp->fd; (n = *hp) >= 0; hp++)
			close(n);
	}
	return(error_info.errors);
}
