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
 * fmt.c
 * Written by David Korn
 * Tue Oct  6 23:57:44 EDT 1998
 */

static const char usage[] =
"[-?\n@(#)$Id: fmt (AT&T Labs Research) 2003-07-15 $\n]"
USAGE_LICENSE
"[+NAME?fmt - simple text formatter]"
"[+DESCRIPTION?\bfmt\b reads the input files and left justifies space separated"
"	words into lines \awidth\a characters or less in length and writes"
"	the lines to the standard output. The standard input is read if \b-\b"
"	or no files are specified. Blank lines and interword spacing are"
"	preserved in the output. Indentation is preserved, and lines with"
"	identical indentation are joined and justified.]"
"[+?\bfmt\b is meant to format mail messages prior to sending, but may also be"
"	useful for other simple tasks. For example, in \bvi\b(1) the command"
"	\b:!}fmt\b will justify the lines in the current paragraph.]"

"[c:crown-margin?Preserve the indentation of the first two lines within a"
"	paragraph, and align the left margin of each subsequent line with"
"	that of the second line.]"
"[s:split-only?Split lines only; do not join short lines to form longer ones.]"
"[u:uniform-spacing?One space between words, two after sentences.]"
"[w:width?Set the output line width to \acolumns\a.]#[columns:=72]"

"\n"
"\n[ file ... ]\n"
"\n"

"[+SEE ALSO?\bmailx\b(1), \bnroff\b(1), \btroff\b(1), \bvi\b(1)]"
;

#include	<cmdlib.h>
#include	<ctype.h>

typedef struct _fmt_
{
	long	flags;
	char	*outp;
	char	*outbuff;
	char	*endbuff;
	Sfio_t	*in;
	Sfio_t	*out;
	int	nwords;
	int	prefix;
} Fmt_t;

#define TABSZ	8
#define isoption(fp,c)	((fp)->flags&(1L<<((c)-'a')))

static int outline(Fmt_t *fp)
{
	register char *cp = fp->outbuff;
	int n=0;
	if(!fp->outp)
		return(0);
	while(fp->outp[-1]==' ')
		fp->outp--;
	*fp->outp = 0;
	while(*cp++==' ')
		n++;
	if(n>=TABSZ)
	{
		n  /= TABSZ;
		cp = &fp->outbuff[TABSZ*n];
		while(n-->0)
			*--cp = '\t';
	}
	else
		cp = fp->outbuff;
	fp->nwords = 0;
	fp->outp = 0;
	return(sfputr(fp->out,cp,'\n'));

}

static void split(Fmt_t *fp, char *buff)
{
	register char *cp,*ep;
	register int c=1,n;
	int prefix;
	for(ep=buff; *ep==' ';ep++);
	prefix = (ep-buff);
	/* preserve blank lines */
	if(*ep==0 || *buff=='.')
	{
		if(*ep)
			prefix = strlen(buff);
		outline(fp);
		strcpy(fp->outbuff,buff);
		fp->outp = fp->outbuff+prefix;
		outline(fp);
		return;
	}
	if(fp->prefix<prefix && !isoption(fp,'c'))
		outline(fp);
	if(!fp->outp || prefix<fp->prefix)
		fp->prefix = prefix;
	while(c)
	{
		cp = ep;
		while(*ep==' ')
			ep++;
		if(cp!=ep && isoption(fp,'u'))
			cp = ep-1;
		while(c = *ep)
		{
			if(c==' ')
				break;
			ep++;
			/* skip over \space */
			if(c=='\\' && *ep)
				ep++;
		}
		n = (ep-cp);
		if(fp->nwords>0 && &fp->outp[n] >= fp->endbuff)
			outline(fp);
		if(fp->nwords==0)
		{
			if(fp->prefix)
				memset(fp->outbuff,' ',fp->prefix);
			fp->outp =  &fp->outbuff[fp->prefix];
			while(*cp==' ')
				cp++;
			n = (ep-cp);
		}
		memcpy(fp->outp,cp,n);
		fp->outp += n;
		fp->nwords++;
	}
	if(isoption(fp,'s') || *buff==0)
		outline(fp);
	else if(fp->outp)
	{
		/* two spaces at ends of sentences */
		if(strchr(".:!?",fp->outp[-1]))
			*fp->outp++ = ' ';
		*fp->outp++ = ' ';
	}
}

static int dofmt(Fmt_t *fp)
{
	char buff[8192];
	char *cp, *dp, *ep;
	register int c;
	while((cp=sfgetr(fp->in,'\n',0)))
	{
		ep = 0;
		dp = buff;
		while(c=*cp++)
		{
			if(c=='\b')
			{
				if(dp>buff)
				{
					dp--;
					if(ep)
						ep--;
						
				}
				continue;
			}
			else if(c=='\t')
			{
				/* expand tabs */
				if(!ep)
					ep = dp;
				c = TABSZ - (dp-buff)%TABSZ;
				while(c-->0)
					*dp++ = ' ';
			}
			else if(c=='\n')
				break;
			else if(!isprint(c))
				continue;
			else
			{
				if(c!=' ')
					ep = 0;
				else if(!ep)
					ep = dp;
				*dp++ = c;
			}
		}
		if(ep)
			*ep = 0;
		else
			*dp = 0;
		split(fp,buff);
	}
	return(0);
}

int
b_fmt(int argc, char** argv, void *context)
{
	register int n;
	Fmt_t fmt;
	char outbuff[8 * 1024];
	char *cp;
	fmt.flags = 0;
	fmt.out = sfstdout;
	fmt.outbuff = outbuff;
	fmt.endbuff = &outbuff[72];
	fmt.outp = 0;
	fmt.nwords = 0;
	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
	    case 'c':
	    case 's':
	    case 'u':
		fmt.flags |= 1L<< (n-'a');
		break;
	    case 'w':
		if(opt_info.num>0 && opt_info.num<sizeof(outbuff))
			fmt.endbuff = &outbuff[opt_info.num];
		else
			error(2, "width out of range");
		break;
	    case ':':
		error(2, "%s", opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(error_info.errors)
		error(ERROR_usage(2), "%s", optusage(NiL));
	if(isoption(&fmt,'s'))
		fmt.flags &=  ~isoption(&fmt,'u');
	if(cp = *argv)
		argv++;
	do
	{
		if(!cp || streq(cp,"-"))
			fmt.in = sfstdin;
		else if(!(fmt.in = sfopen(NiL,cp,"r")))
		{
			error(ERROR_system(0),"%s: cannot open",cp);
			error_info.errors = 1;
			continue;
		}
		dofmt(&fmt);
		if(fmt.in!=sfstdin)
			sfclose(fmt.in);
        }
	while(cp= *argv++);
	outline(&fmt);
	return(error_info.errors?1:0);
}
