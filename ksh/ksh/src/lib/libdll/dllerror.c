/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1997-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                                                                      *
***********************************************************************/
#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 */

#include "dlllib.h"

Dllstate_t	state;

/*
 * return error message from last failed dl*() call
 * retain==0 resets the last dl*() error
 */

extern char*
dllerror(int retain)
{
	char*	s;

	if (state.error)
	{
		state.error = retain;
		return state.errorbuf;
	}
	s = dlerror();
	if (retain)
	{
		state.error = retain;
		sfsprintf(state.errorbuf, sizeof(state.errorbuf), "%s", s);
	}
	return s;
}
