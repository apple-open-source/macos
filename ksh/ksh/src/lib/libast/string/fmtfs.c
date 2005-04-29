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
 * AT&T Bell Laboratories
 *
 * return fs type string given stat
 */

#include <ast.h>
#include <ls.h>
#include <mnt.h>

#include "FEATURE/fs"

#if _str_st_fstype

char*
fmtfs(struct stat* st)
{
	return st->st_fstype;
}

#else

#include <cdt.h>

typedef struct Id_s
{
	Dtlink_t	link;
	dev_t		id;
	char		name[1];
} Id_t;

char*
fmtfs(struct stat* st)
{
	register Id_t*		ip;
	register void*		mp;
	register Mnt_t*		mnt;
	register char*		s;
	struct stat		rt;
	char*			buf;

	static Dt_t*		dict;
	static Dtdisc_t		disc;

	if (!dict)
	{
		disc.key = offsetof(Id_t, id);
		disc.size = sizeof(dev_t);
		dict = dtopen(&disc, Dthash);
	}
	else if (ip = (Id_t*)dtmatch(dict, &st->st_dev))
		return ip->name;
	s = FS_default;
	if (mp = mntopen(NiL, "r"))
	{
		while ((mnt = mntread(mp)) && (stat(mnt->dir, &rt) || rt.st_dev != st->st_dev));
		if (mnt && mnt->type)
			s = mnt->type;
	}
	if (!dict || !(ip = newof(0, Id_t, 1, strlen(s))))
	{
		if (!mp)
			return s;
		buf = fmtbuf(strlen(s) + 1);
		strcpy(buf, s);
		mntclose(mp);
		return buf;
	}
	strcpy(ip->name, s);
	if (mp)
		mntclose(mp);
	dtinsert(dict, ip);
	return ip->name;
}

#endif
