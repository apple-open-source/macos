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
 * Xopen 4.2 compatibility
 */

#include <ast_lib.h>

#if _lib_getsubopt

#include <ast.h>

NoN(getsubopt)

#else

#define getsubopt	______getsubopt

#include <ast.h>

#undef	getsubopt

#include <error.h>

#if defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern int
getsubopt(register char** op, char* const* tp, char** vp)
{
	register char*	b;
	register char*	s;
	register char*	v;

	if (*(b = *op))
	{
		v = 0;
		s = b;
		for (;;)
		{
			switch (*s++)
			{
			case 0:
				s--;
				break;
			case ',':
				*(s - 1) = 0;
				break;
			case '=':
				if (!v)
				{
					*(s - 1) = 0;
					v = s;
				}
				continue;
			default:
				continue;
			}
			break;
		}
		*op = s;
		*vp = v;
		for (op = (char**)tp; *op; op++)
			if (streq(b, *op))
				return op - (char**)tp;
	}
	*vp = b;
	return -1;
}

#endif
