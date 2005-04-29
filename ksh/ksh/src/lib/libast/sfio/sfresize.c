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

/*	Resize a stream.
	Written by Kiem-Phong Vo.
*/

#if __STD_C
int sfresize(Sfio_t* f, Sfoff_t size)
#else
int sfresize(f, size)
Sfio_t*		f;
Sfoff_t		size;
#endif
{
	SFMTXSTART(f, -1);

	if(size < 0 || f->extent < 0 ||
	   (f->mode != SF_WRITE && _sfmode(f,SF_WRITE,0) < 0) )
		SFMTXRETURN(f, -1);

	SFLOCK(f,0);

	if(f->flags&SF_STRING)
	{	SFSTRSIZE(f);

		if(f->extent >= size)
		{	if((f->flags&SF_MALLOC) && (f->next - f->data) <= size)
			{	size_t	s = (((size_t)size + 1023)/1024)*1024;
				Void_t*	d;
				if(s < f->size && (d = realloc(f->data, s)) )
				{	f->data = d;
					f->size = s;
					f->extent = s;
				}
			}
			memclear((char*)(f->data+size), (int)(f->extent-size));
		}
		else
		{	if(SFSK(f, size, SEEK_SET, f->disc) != size)
				SFMTXRETURN(f, -1);
			memclear((char*)(f->data+f->extent), (int)(size-f->extent));
		}
	}
	else
	{	if(f->next > f->data)
			SFSYNC(f);
#if _lib_ftruncate
		if(ftruncate(f->file, (sfoff_t)size) < 0)
			SFMTXRETURN(f, -1);
#else
		SFMTXRETURN(f, -1);
#endif
	}

	f->extent = size;

	SFOPEN(f, 0);

	SFMTXRETURN(f, 0);
}
