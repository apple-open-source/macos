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
 * pathchk
 *
 * Written by David Korn
 */

static const char usage[] =
"[-?\n@(#)$Id: pathchk (AT&T Labs Research) 1999-04-29 $\n]"
USAGE_LICENSE
"[+NAME?pathchk - check pathnames for portability]"
"[+DESCRIPTION?\bpathchk\b checks each \apathname\a to see if it "
	"is valid and/or portable.  A \apathname\a is valid if it "
	"can be used to access or create a file without causing syntax "
	"errors.  A file is portable, if no truncation will result on "
	"any conforming POSIX.1 implementation.]"
"[+?By default \bpathchk\b checks each component of each \apathname\a "
	"based on the underlying file system.  A diagnostic is written "
	"to standard error for each pathname that:]{"
	"[+-?Is longer than \bPATH_MAX\b bytes as returned by \bgetconf\b(1).]"
	"[+-?Contains any component longer that \bNAME_MAX\b bytes.]"
	"[+-?Contains any directory component in a directory that is "
		"not searchable.]"
	"[+-?Contains any character in any component that is not valid in "
		"its containing directory.]"
	"}"
"[p:portability?Instead of performing length checks on the underlying "
	"file system, the length of each \apathname\a and its components is "
	"tested against the POSIX.1 minimum limits for portability.]"
"\n"
"\npathname ...\n"
"\n"
"[+EXIT STATUS?]{"
        "[+0?All \apathname\a operands passed all of the checks.]"
        "[+>0?An error occurred.]"
"}"
"[+SEE ALSO?\bgetconf\b(1), \bcreat\b(2)]"
;


#include	<cmdlib.h>
#include	<ls.h>

#define isport(c)	(((c)>='a' && (c)<='z') || ((c)>='A' && (c)<='Z') || ((c)>='0' && (c)<='9') || (strchr("._-",(c))!=0) )

/*
 * call pathconf and handle unlimited sizes
 */ 
static long mypathconf(const char *path, int op)
{
	register long r;
	errno=0;
	if((r=pathconf(path, op))<0 && errno==0)
		return(LONG_MAX);
	return(r);
}

/*
 * returns 1 if <path> passes test
 */
static int pathchk(char* path, int mode)
{
	register char *cp=path, *cpold;
	register int c;
	register long r,name_max,path_max;
	if(mode)
	{
		name_max = _POSIX_NAME_MAX;
		path_max = _POSIX_PATH_MAX;
	}
	else
	{
		static char buff[2];
		name_max = path_max = 0;
		buff[0] = (*cp=='/'? '/': '.');
		if((r=mypathconf(buff, _PC_NAME_MAX)) > _POSIX_NAME_MAX)
			name_max = r;
		if((r=mypathconf(buff, _PC_PATH_MAX)) > _POSIX_PATH_MAX)
			path_max = r;
		if(*cp!='/')
		{
			if(name_max==0||path_max==0)
			{
				if(!(cpold = getcwd((char*)0, 0)) && errno == EINVAL && (cpold = newof(0, char, PATH_MAX, 0)) && !getcwd(cpold, PATH_MAX))
				{
					free(cpold);
					cpold = 0;
				}
				if(cpold)
				{
					cp = cpold + strlen(cpold);
					while(name_max==0 || path_max==0)
					{
						if(cp>cpold)
							while(--cp>cpold && *cp=='/');
						*++cp = 0;
						if(name_max==0 && (r=mypathconf(cpold, _PC_NAME_MAX)) > _POSIX_NAME_MAX)
							name_max = r;
						if(path_max==0 && (r=mypathconf(cpold, _PC_PATH_MAX)) > _POSIX_PATH_MAX)
							path_max=r;
						if(--cp==cpold)
						{
							free(cpold);
							break;
						}
						while(*cp!='/')
							cp--;
					}
					cp=path;
				}
			}
			while(*cp=='/')
				cp++;
		}
		if(name_max==0)
			name_max=_POSIX_NAME_MAX;
		if(path_max==0)
			path_max=_POSIX_PATH_MAX;
		while(*(cpold=cp))
		{
			while((c= *cp++) && c!='/');
			if((cp-cpold) > name_max)
				goto err;
			errno=0;
			cp[-1] = 0;
			r = mypathconf(path, _PC_NAME_MAX);
			if((cp[-1]=c)==0)
				cp--;
			else while(*cp=='/')
				cp++;
			if(r>=0)
				name_max=(r<_POSIX_NAME_MAX?_POSIX_NAME_MAX:r);
			else if(errno==EINVAL)
				continue;
#ifdef ENAMETOOLONG
			else if(errno==ENAMETOOLONG)
			{
				error(2,"%s: pathname too long",path);
				return(0);
			}
#endif /*ENAMETOOLONG*/
			else
				break;
		}
	}
	while(*(cpold=cp))
	{
		while((c= *cp++) && c!='/')
		{
			if(mode && !isport(c))
			{
				error(2,"%s: %c not in portable character set",path,c);
				return(0);
			}
		}
		if((cp-cpold) > name_max)
			goto err;
		if(c==0)
			break;
		while(*cp=='/')
			cp++;
	}
	if((cp-path) >= path_max)
	{
		error(2,"%s: pathname too long",path);
		return(0);
	}
	return(1);
err:
	error(2,"%s: component name %.*s too long",path,cp-cpold-1,cpold);
	return(0);
}

int
b_pathchk(int argc, char** argv, void* context)
{
	register int n, mode=0;
	register char *cp;

	cmdinit(argv, context, ERROR_CATALOG, 0);
	while (n = optget(argv, usage)) switch (n)
	{
  	    case 'p':
		mode = 1;
		break;
	    case ':':
		error(2, "%s", opt_info.arg);
		break;
	    case '?':
		error(ERROR_usage(2), "%s", opt_info.arg);
		break;
	}
	argv += opt_info.index;
	if(*argv==0 || error_info.errors)
		error(ERROR_usage(2),"%s", optusage((char*)0));
	while(cp = *argv++)
	{
		if(!pathchk(cp,mode))
			error_info.errors=1;
	}
	return(error_info.errors);
}

