/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*                     Copyright (c) 1994-2011 AT&T                     *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                               by AT&T                                *
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
#include <wchar.h>

int
main()
{
	wchar_t	w = ' ';
	return iswspace(w) == 0;
}
