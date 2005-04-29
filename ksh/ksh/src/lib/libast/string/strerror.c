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
 * return error message string given errno
 */

#include <ast.h>
#include <error.h>

#include "FEATURE/errno"

#undef	strerror

#if !_dat_sys_errlist
#undef		_dat_sys_nerr
#undef		sys_errlist
char*		sys_errlist[] = { 0 };
#else
#if !_def_errno_sys_errlist
extern char*	sys_errlist[];
#endif
#endif

#if !_dat_sys_nerr
#undef		sys_nerr
int		sys_nerr = 0;
#else
#if !_def_errno_sys_nerr
extern int	sys_nerr;
#endif
#endif

#if _lib_strerror
extern char*	strerror(int);
#endif

char*
_ast_strerror(int err)
{
	char*		msg;
	int		z;

	static int	sys;

#if _lib_strerror
	z = errno;
	msg = strerror(err);
	errno = z;
#else
	if (err > 0 && err <= sys_nerr)
		msg = (char*)sys_errlist[err];
	else
		msg = 0;
#endif
	if (msg)
	{
		if (ERROR_translating())
		{
#if _lib_strerror
			if (!sys)
			{
				char*	s;
				char*	t;
				char*	p;

#if _lib_strerror
				/*
				 * stash the pending strerror() msg
				 */

				msg = strcpy(fmtbuf(strlen(msg) + 1), msg);
#endif

				/*
				 * make sure that strerror() translates
				 */

				if (!(s = strerror(1)))
					sys = -1;
				else
				{
					t = fmtbuf(z = strlen(s) + 1);
					strcpy(t, s);
					p = setlocale(LC_MESSAGES, NiL);
					setlocale(LC_MESSAGES, "C");
					sys = (s = strerror(1)) && strcmp(s, t) ? 1 : -1;
					setlocale(LC_MESSAGES, p);
				}
			}
			if (sys > 0)
				return msg;
#endif
			return ERROR_translate(NiL, NiL, "errlist", msg);
		}
		return msg;
	}
	msg = fmtbuf(z = 32);
	sfsprintf(msg, z, ERROR_translate(NiL, NiL, "errlist", "Error %d"), err);
	return msg;
}

#if !_lib_strerror

#if defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern char*
strerror(int err)
{
	return _ast_strerror(err);
}

#endif
