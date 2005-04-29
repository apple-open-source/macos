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

/*	Read formated data from a stream
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int sfscanf(Sfio_t* f, const char* form, ...)
#else
int sfscanf(va_alist)
va_dcl
#endif
{
	va_list	args;
	reg int	rv;

#if __STD_C
	va_start(args,form);
#else
	reg Sfio_t*	f;
	reg char*	form;
	va_start(args);
	f = va_arg(args,Sfio_t*);
	form = va_arg(args,char*);
#endif

	rv = (f && form) ? sfvscanf(f,form,args) : -1;
	va_end(args);
	return rv;
}

#if __STD_C
int sfvsscanf(const char* s, const char* form, va_list args)
#else
int sfvsscanf(s, form, args)
char*	s;
char*	form;
va_list	args;
#endif
{
	Sfio_t	f;

	if(!s || !form)
		return -1;

	/* make a fake stream */
	SFCLEAR(&f,NIL(Vtmutex_t*));
	f.flags = SF_STRING|SF_READ;
	f.bits = SF_PRIVATE;
	f.mode = SF_READ;
	f.size = strlen((char*)s);
	f.data = f.next = f.endw = (uchar*)s;
	f.endb = f.endr = f.data+f.size;

	return sfvscanf(&f,form,args);
}

#if __STD_C
int sfsscanf(const char* s, const char* form,...)
#else
int sfsscanf(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;
#if __STD_C
	va_start(args,form);
#else
	reg char*	s;
	reg char*	form;
	va_start(args);
	s = va_arg(args,char*);
	form = va_arg(args,char*);
#endif

	rv = (s && form) ? sfvsscanf(s,form,args) : -1;
	va_end(args);
	return rv;
}
