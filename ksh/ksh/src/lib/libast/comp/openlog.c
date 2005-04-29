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
 * openlog implementation
 */

#include <ast.h>

#if _lib_syslog

NoN(openlog)

#else

#include "sysloglib.h"

void
openlog(const char* ident, int flags, int facility)
{
	int		n;

	if (ident)
	{
		n = strlen(ident);
		if (n >= sizeof(log.ident))
			n = sizeof(log.ident) - 1;
		memcpy(log.ident, ident, n);
		log.ident[n] = 0;
	}
	else
		log.ident[0] = 0;
	log.facility = facility;
	log.flags = flags;
	if (!(log.flags & LOG_ODELAY))
		sendlog(NiL);
}

#endif
