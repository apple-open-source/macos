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
 * mime/mailcap to magic support
 */

#include "mimelib.h"

/*
 * close magic handle
 * done this way so that magic is only pulled in
 * if mimetype() is used
 */

static void
drop(Mime_t* mp)
{
	if (mp->magic)
	{
		magicclose(mp->magic);
		mp->magic = 0;
	}
}

/*
 * return mime type for file
 */

char*
mimetype(Mime_t* mp, Sfio_t* fp, const char* file, struct stat* st)
{
	if (mp->disc->flags & MIME_NOMAGIC)
		return 0;
	if (!mp->magic)
	{
		mp->magicd.version = MAGIC_VERSION;
		mp->magicd.flags = MAGIC_MIME;
		mp->magicd.errorf = mp->disc->errorf;
		if (!(mp->magic = magicopen(&mp->magicd)))
		{
			mp->disc->flags |= MIME_NOMAGIC;
			return 0;
		}
		mp->freef = drop;
		magicload(mp->magic, NiL, 0);
	}
	return magictype(mp->magic, fp, file, st);
}
