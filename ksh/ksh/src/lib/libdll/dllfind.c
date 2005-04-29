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
#include <error.h>

/*
 * find and load library name with optional version ver and dlopen() flags
 * at least one dlopen() is called to initialize dlerror()
 * if path!=0 then library path up to size chars copied to path with trailing 0
 */

extern void*
dllfind(const char* lib, const char* ver, int flags, char* path, size_t size)
{
	char*	id;
	void*	dll;

	if (id = error_info.id)
	{
		/*
		 * workaround until ksh calls dllplug() directly
		 */

		if (streq(id, "builtin"))
			id = "ksh";
		if (dll = dllplug(id, lib, ver, flags, path, size))
			return dll;
	}
	return dllplug(NiL, lib, ver, flags, path, size);
}
