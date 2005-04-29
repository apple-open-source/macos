/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1997-2004 AT&T Corp.                *
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
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Labs Research
 */

#include <ast.h>
#include <dlldefs.h>

/*
 * dlsym() with `_' weak fallback
 */

void*
dlllook(void* dll, const char* name)
{
	register void*	addr;
	char		buf[256];

	if (!(addr = dlsym(dll, name)) && strlen(name) < (sizeof(buf) - 2))
	{
		buf[0] = '_';
		strcpy(buf + 1, name);
		addr = dlsym(dll, buf);
	}
	return addr;
}
