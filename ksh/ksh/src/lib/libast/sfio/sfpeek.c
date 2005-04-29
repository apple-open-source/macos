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

/*	Safe access to the internal stream buffer.
**	This function is obsolete. sfreserve() should be used.
**
**	Written by Kiem-Phong Vo (06/27/90).
*/

#if _BLD_sfio && defined(__EXPORT__)
#define extern	__EXPORT__
#endif

#if __STD_C
extern ssize_t sfpeek(reg Sfio_t* f, Void_t** bp, reg size_t size)
#else
extern ssize_t sfpeek(f,bp,size)
reg Sfio_t*	f;	/* file to peek */
Void_t**	bp;	/* start of data area */
reg size_t	size;	/* size of peek */
#endif
{	reg ssize_t	n, sz;
	reg int		mode;

	/* query for the extent of the remainder of the buffer */
	if((sz = size) == 0 || !bp)
	{	if(f->mode&SF_INIT)
			(void)_sfmode(f,0,0);

		if((f->flags&SF_RDWRSTR) == SF_RDWRSTR)
		{	SFSTRSIZE(f);
			n = (f->data+f->here) - f->next;
		}
		else	n = f->endb - f->next;

		if(!bp)
			return n;
		else if(n > 0)	/* size == 0 */
		{	*bp = (Void_t*)f->next;
			return 0;
		}
		/* else fall down and fill buffer */
	}

	if(!(mode = f->flags&SF_READ) )
		mode = SF_WRITE;
	if((int)f->mode != mode && _sfmode(f,mode,0) < 0)
		return -1;

	*bp = sfreserve(f, sz <= 0 ? 0 : sz > f->size ? f->size : sz, 0);

	if(*bp && sz >= 0)
		return sz;

	if((n = sfvalue(f)) > 0)
	{	*bp = (Void_t*)f->next;
		if(sz < 0)
		{	f->mode |= SF_PEEK;
			f->endr = f->endw = f->data;
		}
		else
		{	if(sz > n)
				sz = n;
			f->next += sz;
		}
	}

	return (sz >= 0 && n >= sz) ? sz : n;
}
