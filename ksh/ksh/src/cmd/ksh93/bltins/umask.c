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
 * umask [-S] [mask]
 *
 *   David Korn
 *   AT&T Labs
 *   research!dgk
 *
 */

#include	<ast.h>	
#include	<sfio.h>	
#include	<error.h>	
#include	<ctype.h>	
#include	<ls.h>	
#include	<shell.h>	
#include	"builtins.h"
#ifndef SH_DICT
#   define SH_DICT	"libshell"
#endif

int	b_umask(int argc,char *argv[],void *extra)
{
	register char *mask;
	register int flag = 0, sflag = 0;
	NOT_USED(extra);
	while((argc = optget(argv,sh_optumask))) switch(argc)
	{
		case 'S':
			sflag++;
			break;
		case ':':
			errormsg(SH_DICT,2, "%s", opt_info.arg);
			break;
		case '?':
			errormsg(SH_DICT,ERROR_usage(2), "%s",opt_info.arg);
			break;
	}
	if(error_info.errors)
		errormsg(SH_DICT,ERROR_usage(2),"%s",optusage((char*)0));
	argv += opt_info.index;
	if(mask = *argv)
	{
		register int c;	
		if(isdigit(*mask))
		{
			while(c = *mask++)
			{
				if (c>='0' && c<='7')	
					flag = (flag<<3) + (c-'0');	
				else
					errormsg(SH_DICT,ERROR_exit(1),e_number,*argv);
			}
		}
		else
		{
			char *cp = mask;
			flag = umask(0);
			c = strperm(cp,&cp,~flag);
			if(*cp)
			{
				umask(flag);
				errormsg(SH_DICT,ERROR_exit(1),e_format,mask);
			}
			flag = (~c&0777);
		}
		umask(flag);	
	}	
	else
	{
		umask(flag=umask(0));
		if(sflag)
			sfprintf(sfstdout,"%s\n",fmtperm(~flag&0777));
		else
			sfprintf(sfstdout,"%0#4o\n",flag);
	}
	return(0);
}

