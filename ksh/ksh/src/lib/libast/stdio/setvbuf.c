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

#include "stdhdr.h"

int
setvbuf(Sfio_t* f, char* buf, int type, size_t size)
{
	STDIO_INT(f, "setvbuf", int, (Sfio_t*, char*, int, size_t), (f, buf, type, size))

	if (type == _IOLBF)
		sfset(f, SF_LINE, 1);
	else if (f->flags & SF_STRING)
		return -1;
	else if (type == _IONBF)
	{	
		sfsync(f);
		sfsetbuf(f, NiL, 0);
	}
	else if (type == _IOFBF)
	{	
		if (size == 0)
			size = SF_BUFSIZE;
		sfsync(f);
		sfsetbuf(f, (Void_t*)buf, size);
	}
	return 0;
}
