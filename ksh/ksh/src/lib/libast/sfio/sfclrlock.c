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

/*	Function to clear a locked stream.
**	This is useful for programs that longjmp from the mid of an sfio function.
**	There is no guarantee on data integrity in such a case.
**
**	Written by Kiem-Phong Vo
*/
#if __STD_C
int sfclrlock(reg Sfio_t* f)
#else
int sfclrlock(f)
reg Sfio_t	*f;
#endif
{
	int	rv;

	/* already closed */
	if(f && (f->mode&SF_AVAIL))
		return 0;

	SFMTXSTART(f,0);

	/* clear error bits */
	f->flags &= ~(SF_ERROR|SF_EOF);

	/* clear peek locks */
	if(f->mode&SF_PKRD)
	{	f->here -= f->endb-f->next;
		f->endb = f->next;
	}

	SFCLRBITS(f);

	/* throw away all lock bits except for stacking state SF_PUSH */
	f->mode &= (SF_RDWR|SF_INIT|SF_POOL|SF_PUSH|SF_SYNCED|SF_STDIO);

	rv = (f->mode&SF_PUSH) ? 0 : (f->flags&SF_FLAGS);

	SFMTXRETURN(f, rv);
}
