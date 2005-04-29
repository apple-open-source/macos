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
 * Glenn Fowler
 * AT&T Research
 *
 * return in path the full path name of the probe(1)
 * information for lang and tool using proc
 * if attr != 0 then path attribute assignments placed here
 *
 * if path==0 then the space is malloc'd
 *
 * op:
 *
 *	-3	return non-writable path name with no generation
 *	-2	return path name with no generation
 *	-1	return no $HOME path name with no generation
 *	0	verbose probe
 *	1	silent probe
 *
 * 0 returned if the info does not exist and cannot be generated
 */

#include <ast.h>
#include <error.h>
#include <ls.h>
#include <proc.h>

#ifndef PROBE
#define PROBE		"probe"
#endif

#if defined(ST_RDONLY) || defined(ST_NOSUID)

/*
 * return non-0 if path is in a readonly or non-setuid fs
 */

static int
rofs(const char* path)
{
	struct statvfs	vfs;

	if (!statvfs(path, &vfs))
	{
#if defined(ST_RDONLY)
		if (vfs.f_flag & ST_RDONLY)
			return 1;
#endif
#if defined(ST_NOSUID)
		if (vfs.f_flag & ST_NOSUID)
			return 1;
#endif
	}
	return 0;
}

#else

#define rofs(p)		0

#endif

char*
pathprobe(char* path, char* attr, const char* lang, const char* tool, const char* aproc, int op)
{
	char*		proc = (char*)aproc;
	register char*	p;
	register char*	k;
	register char*	x;
	register char**	ap;
	int		n;
	char*		e;
	char*		np;
	char*		nx;
	char*		probe;
	const char*	dirs;
	const char*	dir;
	char		buf[PATH_MAX];
	char		cmd[PATH_MAX];
	char		exe[PATH_MAX];
	char		lib[PATH_MAX];
	char		key[16];
	char*		arg[6];
	unsigned long	ptime;
	struct stat	st;
	struct stat	ps;

	if (*proc != '/')
	{
		if (p = strchr(proc, ' '))
		{
			strncopy(buf, proc, p - proc + 1);
			proc = buf;
		}
		if (!(proc = pathpath(cmd, proc, NiL, PATH_ABSOLUTE|PATH_REGULAR|PATH_EXECUTE)))
			proc = (char*)aproc;
		else if (p)
		{
			n = strlen(proc);
			strncopy(proc + n, p, PATH_MAX - n - 1);
		}
	}
	if (!path)
		path = buf;
	probe = PROBE;
	x = lib + sizeof(lib) - 1;
	k = lib + sfsprintf(lib, x - lib, "lib/%s/", probe);
	p = k + sfsprintf(k, x - k, "%s/%s/", lang, tool);
	pathkey(key, attr, lang, tool, proc);
	if (op >= -2)
	{
		strncopy(p, key, x - p);
		if (pathpath(path, lib, "", PATH_ABSOLUTE) && !stat(path, &st) && (st.st_mode & S_IWUSR))
			return path == buf ? strdup(path) : path;
	}
	e = strncopy(p, probe, x - p);
	if (!pathpath(path, lib, "", PATH_ABSOLUTE|PATH_EXECUTE) || stat(path, &ps))
		return 0;
	for (;;)
	{
		ptime = ps.st_mtime;
		n = strlen(path);
		if (n < (PATH_MAX - 5))
		{
			strcpy(path + n, ".ini");
			if (!stat(path, &st) && st.st_size && ptime < (unsigned long)st.st_mtime)
				ptime = st.st_mtime;
			path[n] = 0;
		}
		np = path + n - (e - k);
		nx = path + PATH_MAX - 1;
		strncopy(np, probe, nx - np);
		if (!stat(path, &st))
			break;

		/*
		 * yes lib/probe/<lang>/<proc>/probe
		 *  no lib/probe/probe
		 *
		 * do a manual pathaccess() to find a dir with both
		 */

		sfsprintf(exe, sizeof(exe), "lib/%s/%s", probe, probe);
		dirs = pathbin();
		for (;;)
		{
			if (!(dir = dirs))
				return 0;
			dirs = pathcat(path, dir, ':', "..", exe);
			pathcanon(path, 0);
			if (*path == '/' && pathexists(path, PATH_REGULAR|PATH_EXECUTE))
			{
				pathcat(path, dir, ':', "..", lib);
				pathcanon(path, 0);
				if (*path == '/' && pathexists(path, PATH_REGULAR|PATH_EXECUTE) && !stat(path, &ps))
					break;
			}
		}
	}
	strncopy(p, key, x - p);
	p = np;
	x = nx;
	strcpy(exe, path);
	if (op >= -1 && (!(st.st_mode & S_ISUID) && ps.st_uid != geteuid() || rofs(path)))
	{
		if (!(p = getenv("HOME")))
			return 0;
		p = path + sfsprintf(path, PATH_MAX - 1, "%s/.%s/", p, probe);
	}
	strncopy(p, k, x - p);
	if (op >= 0 && !stat(path, &st))
	{
		if (ptime <= (unsigned long)st.st_mtime || ptime <= (unsigned long)st.st_ctime)
			op = -1;
		else if (st.st_mode & S_IWUSR)
		{
			if (op == 0)
				error(0, "%s probe information for %s language processor %s must be manually regenerated", tool, lang, proc);
			op = -1;
		}
	}
	if (op >= 0)
	{
		ap = arg;
		*ap++ = exe;
		if (op > 0)
			*ap++ = "-s";
		*ap++ = (char*)lang;
		*ap++ = (char*)tool;
		*ap++ = proc;
		*ap = 0;
		if (procrun(exe, arg))
			return 0;
		if (access(path, R_OK))
			return 0;
	}
	return path == buf ? strdup(path) : path;
}
