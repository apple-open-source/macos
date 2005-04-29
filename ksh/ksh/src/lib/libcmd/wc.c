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
 * wc [-c] [-m] [-w] [-l] [file] ...
 *
 * count the number of bytes, words, and lines in a file
 */

static const char usage[] =
"[-?\n@(#)$Id: wc (AT&T Labs Research) 2003-05-04 $\n]"
USAGE_LICENSE
"[+NAME?wc - print the number of bytes, words, and lines in files]"
"[+DESCRIPTION?\bwc\b reads one or more input files and, by default, "
	"for each file writes a line containing the number of newlines, "
	"\aword\as, and bytes contained in each file followed by the "
	"file name to standard output in that order.  A \aword\a is "
	"defined to be a non-zero length string delimited by \bisspace\b(3) "
	"characters.]"
"[+?If more than one file is specified, \bwc\b writes a total count "
	"for all of the named files with \btotal\b written instead "
	"of the file name.]"
"[+?By default, \bwc\b writes all three counts.  Options can specified "
	"so that only certain counts are written.  The options \b-c\b "
	"and \b-m\b are mutually exclusive.]"
"[+?If no \afile\a is given, or if the \afile\a is \b-\b, \bwc\b "
        "reads from standard input and no filename is written to standard "
	"output.  The start of the file is defined as the current offset.]"
"[l:lines?Writes the line counts.]"
"[w:words?Writes the word counts.]"
"[ [c:bytes|chars:chars?Writes the byte counts.]"
"[m:multibyte-chars?Writes the character counts.] ]"
"\n"
"\n[file ...]\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?All files processed successfully.]"
        "[+>0?One or more files failed to open or could not be read.]"
"}"
"[+SEE ALSO?\bcat\b(1), \bisspace\b(3)]"
;


#include <cmdlib.h>
#include <wc.h>
#include <ls.h>

#define ERRORMAX	125

static void printout(register Wc_t *wp, register char *name,register int mode)
{
	if(mode&WC_LINES)
		sfprintf(sfstdout," %7I*d",sizeof(wp->lines),wp->lines);
	if(mode&WC_WORDS)
		sfprintf(sfstdout," %7I*d",sizeof(wp->words),wp->words);
	if(mode&WC_CHARS)
		sfprintf(sfstdout," %7I*d",sizeof(wp->chars),wp->chars);
	if(name)
		sfprintf(sfstdout," %s",name);
	sfputc(sfstdout,'\n');
}

int
b_wc(int argc,register char **argv, void* context)
{
	register char	*cp;
	register int	mode=0, n;
	register Wc_t	*wp;
	Sfio_t		*fp;
	Sfoff_t		tlines=0, twords=0, tchars=0;
	struct stat	statb;

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv,usage)) switch (n)
	{
	case 'w':
		mode |= WC_WORDS;
		break;
	case 'c':
	case 'm':
		if(mode&WC_CHARS)
		{
			if((n=='m') ^ ((mode&WC_MBYTE)!=0))
				error(2, "c and m are mutually exclusive");
		}
		mode |= WC_CHARS;
		if(n=='m')
			mode |= WC_MBYTE;
		break;
	case 'l':
		mode |= WC_LINES;
		break;
	case ':':
		error(2, "%s", opt_info.arg);
		break;
	case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if (error_info.errors)
		error(ERROR_usage(2), "%s", optusage(NiL));
	if(!(mode&(WC_WORDS|WC_CHARS|WC_LINES)))
		mode |= (WC_WORDS|WC_CHARS|WC_LINES);
	if(!(wp = wc_init(NiL)))
		error(3,"internal error");
	if(!(mode&WC_WORDS))
	{
		memzero(wp->space, (1<<CHAR_BIT));
		wp->space['\n'] = -1;
	}
	if(cp = *argv)
		argv++;
	do
	{
		if(!cp || streq(cp,"-"))
			fp = sfstdin;
		else if(!(fp = sfopen(NiL,cp,"r")))
		{
			error(ERROR_system(0),"%s: cannot open",cp);
			continue;
		}
		if(cp)
			n++;
		if(!(mode&(WC_WORDS|WC_LINES)) && fstat(sffileno(fp),&statb)>=0
			 && S_ISREG(statb.st_mode))
		{
			wp->chars = statb.st_size - lseek(sffileno(fp),0L,1);
			lseek(sffileno(fp),0L,2);
		}
		else
			wc_count(wp, fp);
		if(fp!=sfstdin)
			sfclose(fp);
		tchars += wp->chars;
		twords += wp->words;
		tlines += wp->lines;
		printout(wp,cp,mode);
	}
	while(cp= *argv++);
	if(n>1)
	{
		wp->lines = tlines;
		wp->chars = tchars;
		wp->words = twords;
		printout(wp,"total",mode);
	}
	return(error_info.errors<ERRORMAX?error_info.errors:ERRORMAX);
}

