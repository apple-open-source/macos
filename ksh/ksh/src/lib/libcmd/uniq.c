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
 * uniq
 *
 * Written by David Korn
 */

static const char usage[] =
"[-?\n@(#)$Id: uniq (AT&T Labs Research) 1999-04-28 $\n]"
USAGE_LICENSE
"[+NAME?uniq - Report or filter out repeated lines in a file]"
"[+DESCRIPTION?\buniq\b reads an input, comparing adjacent lines, and "
	"writing one copy of each input line on the output.  The second "
	"and succeeding copies of the repeated adjacent lines are not "
	"written.]"
"[+?If the output file, \aoutfile\a, is not specified, \buniq\b writes "
	"to standard output.  If no \ainfile\a is given, or if the \ainfile\a "
	"is \b-\b, \buniq\b reads from standard input with  the start of "
	"the file is defined as the current offset.]"
"[c:count?Output the number of times each line occurred  along with "
	"the line.]"
"[d:repeated|duplicates?Only output duplicate lines.]"
"[f:skip-fields]#[fields?\afields\a is the number of fields to skip over "
	"before checking for uniqueness.  A field is the minimal string "
	"matching the BRE \b[[:blank:]]]]*[^[:blank:]]]]*\b.]"
"[s:skip-chars]#[chars?\achars\a is the number of characters to skip over "
	"before checking for uniqueness.  If specified along with \b-f\b, "
	"the first \achars\a after the first \afields\a are ignored.  If "
	"the \achars\a specifies more characters than are on the line, "
	"an empty string will be used for comparison.]"
"[u:unique?Output unique lines.]"
"[w:check-chars]#[chars?\achars\a is the number of characters to compare " 
	"after skipping any specified fields and characters.]"
"\n"
"\n[infile [outfile]]\n"
"\n"
"[+EXIT STATUS?]{"
	"[+0?The input file was successfully processed.]"
	"[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bsort\b(1), \bgrep\b(1)]"
;


#include <cmdlib.h>

#define C_FLAG	1
#define D_FLAG	2
#define U_FLAG	4
#define CWIDTH	4
#define MAXCNT	9999

/*
 * return a pointer to a side buffer
 */
static char *sidebuff(int size)
{
	static int maxbuff;
	static char *buff;
	if(size)
	{
		if(size <= maxbuff)
			return(buff);
		if (!(buff = newof(buff, char, size, 0)))
			error(ERROR_exit(1), "out of space [side buffer]");
	}
	else
	{
		free(buff);
		buff = 0;
	}
	maxbuff = size;
	return(buff);
}

static int uniq(Sfio_t *fdin, Sfio_t *fdout, int fields, int chars, int width,int mode)
{
	register int n, outsize=0;
	register char *cp, *bufp, *outp;
	char *orecp, *sbufp=0, *outbuff;
	int reclen,oreclen= -1,count=0, cwidth=0;
	if(mode&C_FLAG)
		cwidth = CWIDTH+1;
	while(1)
	{
		if(cp = bufp = sfgetr(fdin,'\n',0))
		{
			if(n=fields)
			{
				while(*cp!='\n') /* skip over fields */
				{
					while(*cp==' ' || *cp=='\t')
						cp++;
					if(n-- <=0)
						break;
					while(*cp!=' ' && *cp!='\t' && *cp!='\n')
						cp++;
				}
			}
			if(chars)
				cp += chars;
			n = sfvalue(fdin);
			if((reclen = n - (cp-bufp)) <=0)
			{
				reclen = 1;
				cp = bufp + sfvalue(fdin)-1;
			}
			else if(width>0 && width < reclen)
				reclen = width;
		}
		else
			reclen=0;
		if(reclen==oreclen && memcmp(cp,orecp,reclen)==0)
		{
			count++;
			continue;
		}
		/* no match */
		if(outsize>0)
		{
			if(((mode&D_FLAG)&&count==0) || ((mode&U_FLAG)&&count))
			{
				if(outp!=sbufp)
					sfwrite(fdout,outp,0);
			}
			else
			{
				if(cwidth)
				{
					outp[CWIDTH] = ' ';
					if(count<MAXCNT)
					{
						sfsprintf(outp,cwidth,"%*d",CWIDTH,count+1);
						outp[CWIDTH] = ' ';
					}
					else
					{
						outsize -= (CWIDTH+1);
						if(outp!=sbufp)
						{
							if(!(sbufp=sidebuff(outsize)))
								return(1);
							memcpy(sbufp,outp+CWIDTH+1,outsize);
							sfwrite(fdout,outp,0);
							outp = sbufp;
						}
						else
							outp += CWIDTH+1;
						sfprintf(fdout,"%4d ",count+1);
					}
				}
				if(sfwrite(fdout,outp,outsize) < 0)
					return(1);
			}
		}
		if(reclen==0)
			break;
		count = 0;
		/* save current record */
		if (!(outbuff = sfreserve(fdout, 0, 0)) || (outsize = sfvalue(fdout)) < 0)
			return(1);
		outp = outbuff;
		if(outsize < n+cwidth)
		{
			/* no room in outp, clear lock and use side buffer */
			sfwrite(fdout,outp,0);
			if(!(sbufp = outp=sidebuff(outsize=n+cwidth)))
				return(1);
		}
		else
			outsize = n+cwidth;
		memcpy(outp+cwidth,bufp,n);
		oreclen = reclen;
		orecp = outp+cwidth + (cp-bufp);
	}
	sidebuff(0);
	return(0);
}

int
b_uniq(int argc, char** argv, void* context)
{
	register int n, mode=0;
	register char *cp;
	int fields=0, chars=0, width=0;
	Sfio_t *fpin, *fpout;

	NoP(argc);
	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
	    case 'c':
		mode |= C_FLAG;
		break;
	    case 'd':
		mode |= D_FLAG;
		break;
	    case 'u':
		mode |= U_FLAG;
		break;
	    case 'f':
		if(*opt_info.option=='-')
			fields = opt_info.num;
		else
			chars = opt_info.num;
		break;
	    case 's':
		chars = opt_info.num;
		break;
	    case 'w':
		width = opt_info.num;
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
	if((cp = *argv) && (argv++,!streq(cp,"-")))
	{
		if(!(fpin = sfopen(NiL,cp,"r")))
			error(ERROR_system(1),"%s: cannot open",cp);
	}
	else
		fpin = sfstdin;
	if(cp = *argv)
	{
		argv++;
		if(!(fpout = sfopen(NiL,cp,"w")))
			error(ERROR_system(1),"%s: cannot create",cp);
	}
	else
		fpout = sfstdout;
	if(*argv)
	{
		error(2, "too many arguments");
		error(ERROR_usage(2), "%s", optusage(NiL));
	}
	error_info.errors = uniq(fpin,fpout,fields,chars,width,mode);
	if(fpin!=sfstdin)
		sfclose(fpin);
	if(fpout!=sfstdout)
		sfclose(fpout);
	return(error_info.errors);
}

