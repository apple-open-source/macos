/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1990-2011 AT&T Intellectual Property          *
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
 *
 * single quote s into sp
 * if type!=0 then /<getenv(<CO_ENV_TYPE>)/ translated to /$<CO_ENV_TYPE>/
 */

#include "colib.h"

void
coquote(register Sfio_t* sp, register const char* s, int type)
{
	register int	c;

	if (type && (!state.type || !*state.type))
		type = 0;
	while (c = *s++)
	{
		sfputc(sp, c);
		if (c == '\'')
		{
			sfputc(sp, '\\');
			sfputc(sp, '\'');
			sfputc(sp, '\'');
		}
		else if (type && c == '/' && *s == *state.type)
		{
			register const char*	x = s;
			register char*		t = state.type;

			while (*t && *t++ == *x) x++;
			if (!*t && *x == '/')
			{
				s = x;
				sfprintf(sp, "'$%s'", CO_ENV_TYPE);
			}
		}
	}
}
