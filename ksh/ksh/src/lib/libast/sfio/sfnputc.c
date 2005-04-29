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

/*	Write out a character n times
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
ssize_t sfnputc(reg Sfio_t* f, reg int c, reg size_t n)
#else
ssize_t sfnputc(f,c,n)
reg Sfio_t*	f;	/* file to write */
reg int		c;	/* char to be written */
reg size_t	n;	/* number of time to repeat */
#endif
{
	reg uchar*	ps;
	reg ssize_t	p, w;
	uchar		buf[128];
	reg int		local;

	SFMTXSTART(f,-1);

	GETLOCAL(f,local);
	if(SFMODE(f,local) != SF_WRITE && _sfmode(f,SF_WRITE,local) < 0)
		SFMTXRETURN(f, -1);

	SFLOCK(f,local);

	/* write into a suitable buffer */
	if((size_t)(p = (f->endb-(ps = f->next))) < n)
		{ ps = buf; p = sizeof(buf); }
	if((size_t)p > n)
		p = n;
	MEMSET(ps,c,p);
	ps -= p;

	w = n;
	if(ps == f->next)
	{	/* simple sfwrite */
		f->next += p;
		if(c == '\n')
			(void)SFFLSBUF(f,-1);
		goto done;
	}

	for(;;)
	{	/* hard write of data */
		if((p = SFWRITE(f,(Void_t*)ps,p)) <= 0 || (n -= p) <= 0)
		{	w -= n;
			goto done;
		}
		if((size_t)p > n)
			p = n;
	}
done :
	SFOPEN(f,local);
	SFMTXRETURN(f, w);
}
