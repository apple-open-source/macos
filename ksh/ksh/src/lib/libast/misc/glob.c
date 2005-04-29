/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1985-2004 AT&T Corp.                *
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
*                 Phong Vo <kpv@research.att.com>                  *
*                                                                  *
*******************************************************************/
#pragma prototyped

/*
 * file name expansion - posix.2 glob with gnu and ast extensions
 *
 *	David Korn
 *	Glenn Fowler
 *	AT&T Research
 */

#include <ast.h>
#include <ls.h>
#include <stak.h>
#include <ast_dir.h>
#include <error.h>
#include <ctype.h>
#include <regex.h>

#define GLOB_MAGIC	0xaaaa0000

#define MATCH_RAW	1
#define MATCH_MAKE	2

#define MATCHPATH	offsetof(globlist_t, gl_path)

typedef int (*GL_error_f)(const char*, int);
typedef void* (*GL_opendir_f)(const char*);
typedef struct dirent* (*GL_readdir_f)(void*);
typedef void (*GL_closedir_f)(void*);
typedef int (*GL_stat_f)(const char*, struct stat*);

#define _GLOB_PRIVATE_ \
	GL_error_f	gl_errfn; \
	int		gl_error; \
	char*		gl_nextpath; \
	globlist_t*	gl_rescan; \
	globlist_t*	gl_match; \
	Stak_t*		gl_stak; \
	int		re_flags; \
	regex_t*	gl_ignore; \
	regex_t*	gl_ignorei; \
	regex_t		re_ignore; \
	regex_t		re_ignorei; \
	unsigned long	gl_starstar; \
	char*		gl_pad[6];

#include <glob.h>

/*
 * default gl_diropen
 */

static void*
gl_diropen(glob_t* gp, const char* path)
{
	return (*gp->gl_opendir)(path);
}

/*
 * default gl_dirnext
 */

static char*
gl_dirnext(glob_t* gp, void* handle)
{
	struct dirent*	dp;

	while (dp = (struct dirent*)(*gp->gl_readdir)(handle))
#ifdef D_FILENO
		if (D_FILENO(dp))
#endif
		{
#ifdef D_TYPE
			if (D_TYPE(dp) != DT_UNKNOWN && D_TYPE(dp) != DT_DIR && D_TYPE(dp) != DT_LNK)
				gp->gl_status |= GLOB_NOTDIR;
#endif
			return dp->d_name;
		}
	return 0;
}

/*
 * default gl_dirclose
 */

static void
gl_dirclose(glob_t* gp, void* handle)
{
	(gp->gl_closedir)(handle);
}

/*
 * default gl_type
 */

static int
gl_type(glob_t* gp, const char* path)
{
	register int	type;
	struct stat	st;

	if ((*gp->gl_stat)(path, &st))
		type = 0;
	else if (S_ISDIR(st.st_mode))
		type = GLOB_DIR;
	else if (!S_ISREG(st.st_mode))
		type = GLOB_DEV;
	else if (st.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))
		type = GLOB_EXE;
	else
		type = GLOB_REG;
	return type;
}

/*
 * default gl_attr
 */

static int
gl_attr(glob_t* gp, const char* path)
{
	return strchr(astconf("PATH_ATTRIBUTES", path, NiL), 'c') ? GLOB_ICASE : 0;
}

/*
 * default gl_nextdir
 */

static char*
gl_nextdir(glob_t* gp, char* dir)
{
	if (!(dir = gp->gl_nextpath))
		dir = gp->gl_nextpath = stakcopy(pathbin());
	switch (*gp->gl_nextpath)
	{
	case 0:
		dir = 0;
		break;
	case ':':
		while (*gp->gl_nextpath == ':')
			gp->gl_nextpath++;
		dir = ".";
		break;
	default:
		while (*gp->gl_nextpath)
			if (*gp->gl_nextpath++ == ':')
			{
				*(gp->gl_nextpath - 1) = 0;
				break;
			}
		break;
	}
	return dir;
}

/*
 * error intercept
 */

static int
errorcheck(register glob_t* gp, const char* path)
{
	int	r = 1;

	if (gp->gl_errfn)
		r = (*gp->gl_errfn)(path, errno);
	if (gp->gl_flags & GLOB_ERR)
		r = 0;
	if (!r)
		gp->gl_error = GLOB_ABORTED;
	return r;
}

/*
 * remove backslashes
 */

static void
trim(register char* sp, register char* p1, int* n1, register char* p2, int* n2)
{
	register char*	dp = sp;
	register int	c;
	register int	n;

	if (p1)
		*n1 = 0;
	if (p2)
		*n2 = 0;
	do
	{
		if ((c = *sp++) == '\\' && (c = *sp++))
			n++;
		if (sp == p1)
		{
			p1 = 0;
			*n1 = sp - dp - 1;
		}
		if (sp == p2)
		{
			p2 = 0;
			*n2 = sp - dp - 1;
		}
	} while (*dp++ = c);
}

static void
addmatch(register glob_t* gp, const char* dir, const char* pat, register const char* rescan, char* endslash)
{
	register globlist_t*	ap;
	int			offset;
	int			type;

	stakseek(MATCHPATH);
	if (dir)
	{
		stakputs(dir);
		stakputc(gp->gl_delim);
	}
	if (endslash)
		*endslash = 0;
	stakputs(pat);
	if (rescan)
	{
		if ((*gp->gl_type)(gp, stakptr(MATCHPATH)) != GLOB_DIR)
			return;
		stakputc(gp->gl_delim);
		offset = staktell();
		/* if null, reserve room for . */
		if (*rescan)
			stakputs(rescan);
		else
			stakputc(0);
		stakputc(0);
		rescan = stakptr(offset);
		ap = (globlist_t*)stakfreeze(0);
		ap->gl_begin = (char*)rescan;
		ap->gl_next = gp->gl_rescan;
		gp->gl_rescan = ap;
	}
	else
	{
		if (!endslash && (gp->gl_flags & GLOB_MARK) && (type = (*gp->gl_type)(gp, stakptr(MATCHPATH))))
		{
			if ((gp->gl_flags & GLOB_COMPLETE) && type != GLOB_EXE)
			{
				stakseek(0);
				return;
			}
			else if (type == GLOB_DIR && (gp->gl_flags & GLOB_MARK))
				stakputc(gp->gl_delim);
		}
		ap = (globlist_t*)stakfreeze(1);
		ap->gl_next = gp->gl_match;
		gp->gl_match = ap;
		gp->gl_pathc++;
	}
	ap->gl_flags = MATCH_RAW;
	if (gp->gl_flags & GLOB_COMPLETE)
		ap->gl_flags |= MATCH_MAKE;
}

/*
 * this routine builds a list of files that match a given pathname
 * uses REG_SHELL of <regex> to match each component
 * a leading . must match explicitly
 */

static void
glob_dir(glob_t* gp, globlist_t* ap)
{
	register char*		rescan;
	register char*		prefix;
	register char*		pat;
	register char*		name;
	register int		c;
	char*			dirname;
	void*			dirf;
	char			first;
	regex_t*		ire;
	regex_t*		pre;
	regex_t			rec;
	regex_t			rei;
	int			notdir;
	int			t1;
	int			t2;

	int			bracket = 0;
	int			complete = 0;
	int			err = 0;
	int			meta = 0;
	int			quote = 0;
	int			savequote = 0;
	char*			restore1 = 0;
	char*			restore2 = 0;
	regex_t*		prec = 0;
	regex_t*		prei = 0;
	char*			matchdir = 0;
	int			starstar = 0;

	if (*gp->gl_intr)
	{
		gp->gl_error = GLOB_INTR;
		return;
	}
	pat = rescan = ap->gl_begin;
	prefix = dirname = ap->gl_path;
	first = (rescan == prefix);
again:
	for (;;)
	{
		switch (c = *rescan++)
		{
		case 0:
			if (meta)
			{
				rescan = 0;
				break;
			}
			if (quote)
			{
				trim(ap->gl_begin, rescan, &t1, NiL, NiL);
				rescan -= t1;
			}
			if (!first && !*rescan && *(rescan - 2) == gp->gl_delim)
			{
				*(rescan - 2) = 0;
				c = (*gp->gl_type)(gp, prefix);
				*(rescan - 2) = gp->gl_delim;
				if (c == GLOB_DIR)
					addmatch(gp, NiL, prefix, NiL, rescan - 1);
			}
			else if ((*gp->gl_type)(gp, prefix))
				addmatch(gp, NiL, prefix, NiL, NiL);
			return;
		case '[':
			bracket = 1;
			continue;
		case ']':
			meta |= bracket;
			continue;
		case '(':
			if (!(gp->gl_flags & GLOB_AUGMENTED))
				continue;
		case '*':
		case '?':
			meta = 1;
			continue;
		case '\\':
			if (!(gp->gl_flags & GLOB_NOESCAPE))
			{
				quote = 1;
				if (*rescan)
					rescan++;
			}
			continue;
		default:
			if (c == gp->gl_delim)
			{
				if (meta)
					break;
				pat = rescan;
				bracket = 0;
				savequote = quote;
			}
			continue;
		}
		break;
	}
	if(matchdir)
		goto skip;
	if (pat == prefix)
	{
		prefix = 0;
		if (!rescan && (gp->gl_flags & GLOB_COMPLETE))
		{
			complete = 1;
			dirname = 0;
		}
		else
			dirname = ".";
	}
	else
	{
		if (pat == prefix + 1)
			dirname = "/";
		if (savequote)
		{
			quote = 0;
			trim(ap->gl_begin, pat, &t1, rescan, &t2);
			pat -= t1;
			if (rescan)
				rescan -= t2;
		}
		*(restore1 = pat - 1) = 0;
	}
	if (!complete && (gp->gl_flags & GLOB_STARSTAR))
		while ( pat[0] == '*' && pat[1] == '*' && (pat[2] == '/'  || pat[2]==0))
		{
			matchdir = pat;
			if(pat[2])
			{
				pat += 3;
				while(*pat=='/') pat++;
				if(*pat)
					continue;
			}
			rescan = *pat?0:pat;
			pat = "*";
			goto skip;
		}
	if(matchdir)
	{
		rescan = pat;
		goto again;
	}
skip:
	if (rescan)
		*(restore2 = rescan - 1) = 0;
	if (rescan && !complete && (gp->gl_flags & GLOB_STARSTAR))
	{
		register char *p = rescan;
		while ( p[0] == '*' && p[1] == '*' && (p[2] == '/'  || p[2]==0))
		{
			rescan = p;
			if(starstar = (p[2]==0))
				break;
			p += 3;
			while(*p=='/') p++;
			if(*p==0)
			{
				starstar = 2;
				break;
			}
		}
	}
	if (matchdir)
		gp->gl_starstar++;
	for (;;)
	{
		if (complete)
		{
			if (!(dirname = (*gp->gl_nextdir)(gp, dirname)))
				break;
			prefix = streq(dirname, ".") ? (char*)0 : dirname;
		}
		if (dirf = (*gp->gl_diropen)(gp, dirname))
		{
			if (!(gp->re_flags & REG_ICASE) && ((*gp->gl_attr)(gp, dirname) & GLOB_ICASE))
			{
				if (!prei)
				{
					if (err = regcomp(&rei, pat, gp->re_flags|REG_ICASE))
						break;
					prei = &rei;
				}
				pre = prei;
				if (gp->gl_ignore)
				{
					if (!gp->gl_ignorei)
					{
						if (regcomp(&gp->re_ignorei, gp->gl_fignore, gp->re_flags|REG_ICASE))
						{
							gp->gl_error = GLOB_APPERR;
							break;
						}
						gp->gl_ignorei = &gp->re_ignorei;
					}
					ire = gp->gl_ignorei;
				}
				else
					ire = 0;
			}
			else
			{
				if (!prec)
				{
					if (err = regcomp(&rec, pat, gp->re_flags))
						break;
					prec = &rec;
				}
				pre = prec;
				ire = gp->gl_ignore;
			}
			if (restore2)
				*restore2 = gp->gl_delim;
			while ((name = (*gp->gl_dirnext)(gp, dirf)) && !*gp->gl_intr)
			{
				if (notdir = (gp->gl_status & GLOB_NOTDIR))
					gp->gl_status &= ~GLOB_NOTDIR;
				if (ire && !regexec(ire, name, 0, NiL, 0))
					continue;
				if (matchdir && (name[0] != '.' || name[1] && (name[1] != '.' || name[2])) && !notdir)
					addmatch(gp,prefix,name,matchdir,NiL);
				if (!regexec(pre, name, 0, NiL, 0))
				{
					if(!rescan || !notdir)
						addmatch(gp, prefix, name, rescan, NiL);
					if(starstar==1 || (starstar==2 && !notdir))
						addmatch(gp, prefix, name, starstar==2?"":NiL, NiL);
				}
				errno = 0;
			}
			(*gp->gl_dirclose)(gp, dirf);
			if (err || errno && !errorcheck(gp, dirname))
				break;
		}
		else if (!complete && !errorcheck(gp, dirname))
			break;
		if (!complete)
			break;
		if (*gp->gl_intr)
		{
			gp->gl_error = GLOB_INTR;
			break;
		}
	}
	if (restore1)
		*restore1 = gp->gl_delim;
	if (restore2)
		*restore2 = gp->gl_delim;
	if (prec)
		regfree(prec);
	if (prei)
		regfree(prei);
	if (err == REG_ESPACE)
		gp->gl_error = GLOB_NOSPACE;
}

int
glob(const char* pattern, int flags, int (*errfn)(const char*, int), register glob_t* gp)
{
	register globlist_t*	ap;
	register char*		pat;
	globlist_t*		top;
	Stak_t*			oldstak;
	char**			argv;
	char**			av;
	size_t			skip;

	int			suflen = 0;
	int			extra = 1;
	unsigned char		intr = 0;

	gp->gl_rescan = 0;
	gp->gl_error = 0;
	gp->gl_errfn = errfn;
	if (flags & GLOB_APPEND)
	{
		if ((gp->gl_flags |= GLOB_APPEND) ^ (flags|GLOB_MAGIC))
			return GLOB_APPERR;
		if (((gp->gl_flags & GLOB_STACK) == 0) == (gp->gl_stak == 0))
			return GLOB_APPERR;
		if (gp->gl_starstar > 1)
			gp->gl_flags |= GLOB_STARSTAR;
		else
			gp->gl_starstar = 0;
	}
	else
	{
		gp->gl_flags = (flags&0xffff)|GLOB_MAGIC;
		gp->re_flags = REG_SHELL|REG_NOSUB|REG_LEFT|REG_RIGHT|((flags&GLOB_AUGMENTED)?REG_AUGMENTED:0);
		gp->gl_pathc = 0;
		gp->gl_ignore = 0;
		gp->gl_ignorei = 0;
		gp->gl_starstar = 0;
		if (!(flags & GLOB_DISC))
		{
			gp->gl_fignore = 0;
			gp->gl_suffix = 0;
			gp->gl_intr = 0;
			gp->gl_delim = 0;
			gp->gl_handle = 0;
			gp->gl_diropen = 0;
			gp->gl_dirnext = 0;
			gp->gl_dirclose = 0;
			gp->gl_type = 0;
			gp->gl_attr = 0;
			gp->gl_nextdir = 0;
		}
		if (!(flags & GLOB_ALTDIRFUNC))
		{
			gp->gl_opendir = (GL_opendir_f)opendir;
			gp->gl_readdir = (GL_readdir_f)readdir;
			gp->gl_closedir = (GL_closedir_f)closedir;
			gp->gl_stat = (flags & GLOB_STARSTAR) ? (GL_stat_f)lstat : (GL_stat_f)pathstat;
		}
		if (!gp->gl_intr)
			gp->gl_intr = &intr;
		if (!gp->gl_delim)
			gp->gl_delim = '/';
		if (!gp->gl_diropen)
			gp->gl_diropen = gl_diropen;
		if (!gp->gl_dirnext)
			gp->gl_dirnext = gl_dirnext;
		if (!gp->gl_dirclose)
			gp->gl_dirclose = gl_dirclose;
		if (!gp->gl_type)
			gp->gl_type = gl_type;
		if (!gp->gl_attr)
			gp->gl_attr = gl_attr;
		if (flags & GLOB_ICASE)
			gp->re_flags |= REG_ICASE;
		if (!gp->gl_fignore)
			gp->re_flags |= REG_SHELL_DOT;
		else if (*gp->gl_fignore)
		{
			if (regcomp(&gp->re_ignore, gp->gl_fignore, gp->re_flags))
				return GLOB_APPERR;
			gp->gl_ignore = &gp->re_ignore;
		}
		if (gp->gl_flags & GLOB_STACK)
			gp->gl_stak = 0;
		else if (!(gp->gl_stak = stakcreate(0)))
			return GLOB_NOSPACE;
		if ((gp->gl_flags & GLOB_COMPLETE) && !gp->gl_nextdir)
			gp->gl_nextdir = gl_nextdir;
	}
	skip = gp->gl_pathc;
	if (gp->gl_stak)
		oldstak = stakinstall(gp->gl_stak, 0);
	if (flags & GLOB_DOOFFS)
		extra += gp->gl_offs;
	if (gp->gl_suffix)
		suflen =  strlen(gp->gl_suffix);
	top = ap = (globlist_t*)stakalloc(strlen(pattern) + sizeof(globlist_t) + suflen);
	ap->gl_begin = ap->gl_path;
	ap->gl_next = 0;
	ap->gl_flags = 0;
	pat = strcopy(ap->gl_path, pattern);
	if (suflen)
		strcpy(pat, gp->gl_suffix);
	suflen = 0;
	if (!(flags & GLOB_LIST))
		gp->gl_match = 0;
	do
	{
		gp->gl_rescan = ap->gl_next;
		glob_dir(gp, ap);
	} while (!gp->gl_error && (ap = gp->gl_rescan));
	if (gp->gl_pathc == skip)
	{
		if (flags & GLOB_NOCHECK)
		{
			gp->gl_pathc++;
			top->gl_next = gp->gl_match;
			gp->gl_match = top;
			strcopy(top->gl_path, pattern);
		}
		else
			gp->gl_error = GLOB_NOMATCH;
	}
	if (flags & GLOB_LIST)
		gp->gl_list = gp->gl_match;
	else
	{
		argv = (char**)stakalloc((gp->gl_pathc + extra) * sizeof(char*));
		if (gp->gl_flags & GLOB_APPEND)
		{
			skip += --extra;
			memcpy(argv, gp->gl_pathv, skip * sizeof(char*));
			av = argv + skip;
		}
		else
		{
			av = argv;
			while (--extra > 0)
				*av++ = 0;
		}
		gp->gl_pathv = argv;
		argv = av;
		ap = gp->gl_match;
		while (ap)
		{
			*argv++ = ap->gl_path;
			ap = ap->gl_next;
		}
		*argv = 0;
		if (!(flags & GLOB_NOSORT) && (argv - av) > 1)
		{
			strsort(av, argv - av, strcoll);
			if (gp->gl_starstar > 1)
				av[gp->gl_pathc = struniq(av, argv - av)] = 0;
			gp->gl_starstar = 0;
		}
	}
	if (gp->gl_starstar > 1)
		gp->gl_flags &= ~GLOB_STARSTAR;
	if (gp->gl_stak)
		stakinstall(oldstak, 0);
	return gp->gl_error;
}

void
globfree(glob_t* gp)
{
	if ((gp->gl_flags & GLOB_MAGIC) == GLOB_MAGIC)
	{
		gp->gl_flags &= ~GLOB_MAGIC;
		if (gp->gl_stak)
			stkclose(gp->gl_stak);
		if (gp->gl_ignore)
			regfree(gp->gl_ignore);
		if (gp->gl_ignorei)
			regfree(gp->gl_ignorei);
	}
}
