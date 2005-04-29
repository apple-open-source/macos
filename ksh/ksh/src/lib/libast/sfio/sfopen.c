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

/*
 * _sfopen() wrapper to allow user sfopen() intercept
 */

extern Sfio_t*		_sfopen _ARG_((Sfio_t*, const char*, const char*));

#if __STD_C
Sfio_t* sfopen(Sfio_t* f, const char* file, const char* mode)
#else
Sfio_t* sfopen(f,file,mode)
Sfio_t*		f;		/* old stream structure */
char*		file;		/* file/string to be opened */
reg char*	mode;		/* mode of the stream */
#endif
{
	return _sfopen(f, file, mode);
}
