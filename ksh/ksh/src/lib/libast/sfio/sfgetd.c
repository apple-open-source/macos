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
#include	"sfhdr.h"

/*	Read a portably coded double value
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
Sfdouble_t sfgetd(Sfio_t* f)
#else
Sfdouble_t sfgetd(f)
Sfio_t*	f;
#endif
{
	reg uchar	*s, *ends, c;
	reg int		p, sign, exp;
	Sfdouble_t	v;

	SFMTXSTART(f,-1.);

	if((sign = sfgetc(f)) < 0 || (exp = (int)sfgetu(f)) < 0)
		SFMTXRETURN(f, -1.);

	if(f->mode != SF_READ && _sfmode(f,SF_READ,0) < 0)
		SFMTXRETURN(f, -1.);

	SFLOCK(f,0);

	v = 0.;
	for(;;)
	{	/* fast read for data */
		if(SFRPEEK(f,s,p) <= 0)
		{	f->flags |= SF_ERROR;
			v = -1.;
			goto done;
		}

		for(ends = s+p; s < ends; )
		{	c = *s++;
			v += SFUVALUE(c);
			v = ldexpl(v,-SF_PRECIS);
			if(!(c&SF_MORE))
			{	f->next = s;
				goto done;
			}
		}
		f->next = s;
	}

done:
	v = ldexpl(v,(sign&02) ? -exp : exp);
	if(sign&01)
		v = -v;

	SFOPEN(f,0);
	SFMTXRETURN(f, v);
}
