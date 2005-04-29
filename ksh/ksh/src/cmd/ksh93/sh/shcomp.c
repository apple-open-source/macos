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
#pragma prototyped
/*
 * David Korn
 * AT&T Labs
 *
 * shell script to shell binary converter
 *
 */

static const char usage[] =
"[-?\n@(#)$Id: shcomp (AT&T Labs Research) 2003-03-02 $\n]"
USAGE_LICENSE
"[+NAME?shcomp - compile a shell script]"
"[+DESCRIPTION?Unless \b-D\b is specified, \bshcomp\b takes a shell script, "
	"\ainfile\a, and creates a binary format file, \aoutfile\a, that "
	"\bksh\b can read and execute with the same effect as the original "
	"script.]"
"[+?If \b-D\b is specifed, all double quoted strings that are preceded by "
	"\b$\b are output.  These are the messages that need to be "
	"translated to locale specific versions for internationalization.]"
"[+?If \aoutfile\a is omitted, then the results will be written to "
	"standard output.  If \ainfile\a is also omitted, the shell script "
	"will be read from standard input.]"
"[D:dictionary?Generate a list of strings that need to be placed in a message "
	"catalog for internationalization.]"
"[n:noexec?Displays warning messages for obsolete or non-conforming "
	"constructs.] "
"[v:verbose?Displays input from \ainfile\a onto standard error as it "
	"reads it.]"
"\n"
"\n[infile [outfile]]\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?Successful completion.]"
        "[+>0?An error occurred.]"
"}"   
"[+SEE ALSO?\bksh\b(1)]"
;

#include	<shell.h>
#include	"shnodes.h"

#define CNTL(x)	((x)&037)
#define VERSION	3
static const char header[6] = { CNTL('k'),CNTL('s'),CNTL('h'),0,VERSION,0 };

main(int argc, char *argv[])
{
	Sfio_t *in, *out;
	Shell_t	*shp;
	Namval_t *np;
	union anynode *t;
	char *cp;
	int n, nflag=0, vflag=0, dflag=0;
	error_info.id = argv[0];
	while(n = optget(argv, usage )) switch(n)
	{
	    case 'D':
		dflag=1;
		break;
	    case 'v':
		vflag=1;
		break;
	    case 'n':
		nflag=1;
		break;
	    case ':':
		errormsg(SH_DICT,2,"%s",opt_info.arg);
		break;
	    case '?':
		errormsg(SH_DICT,ERROR_usage(2),"%s",opt_info.arg);
		break;
	}
	shp = sh_init(argc,argv,(Shinit_f)0);
	argv += opt_info.index;
	argc -= opt_info.index;
	if(error_info.errors || argc>2)
		errormsg(SH_DICT,ERROR_usage(2),"%s",optusage((char*)0));
	if(cp= *argv)
	{
		argv++;
		in = sh_pathopen(cp);
	}
	else
		in = sfstdin;
	if(cp= *argv)
	{
		struct stat statb;
		if(!(out = sfopen((Sfio_t*)0,cp,"w")))
			errormsg(SH_DICT,ERROR_system(1),"%s: cannot create",cp);
		if(fstat(sffileno(out),&statb) >=0)
			chmod(cp,(statb.st_mode&~S_IFMT)|S_IXUSR|S_IXGRP|S_IXOTH);
	}
	else
		out = sfstdout;
	if(dflag)
	{
		sh_onoption(SH_DICTIONARY);
		sh_onoption(SH_NOEXEC);
	}
	if(nflag)
		sh_onoption(SH_NOEXEC);
	if(vflag)
		sh_onoption(SH_VERBOSE);
	if(!dflag)
		sfwrite(out,header,sizeof(header));
	shp->inlineno = 1;
	while(1)
	{
		stakset((char*)0,0);
		if(t = (union anynode*)sh_parse(shp,in,0))
		{
			if(!dflag && sh_tdump(out,t) < 0)
				errormsg(SH_DICT,ERROR_exit(1),"dump failed");
		}
		else if(sfeof(in))
			break;
		if(sferror(in))
			errormsg(SH_DICT,ERROR_system(1),"I/O error");
		if(t && ((t->tre.tretyp&COMMSK)==TCOM) && (np=t->com.comnamp) && (cp=nv_name(np)))
		{
			if(strcmp(cp,"exit")==0)
				break;
			/* check for exec of a command */
			if(strcmp(cp,"exec")==0)
			{
				if(t->com.comtyp&COMSCAN)
				{
					if(t->com.comarg->argnxt.ap)
						break;
				}
				else
				{
					struct dolnod *ap = (struct dolnod*)t->com.comarg;
					if(ap->dolnum>1)
						break;
				}
			}
		}
	}
	/* copy any remaining input */
	sfmove(in,out,SF_UNBOUND,-1);
	if(in!=sfstdin)
		sfclose(in);
	if(out!=sfstdout)
		sfclose(out);
	return(0);
}
