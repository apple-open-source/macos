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

/*	Print data with a given format
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
int sfprintf(Sfio_t* f, const char* form, ...)
#else
int sfprintf(va_alist)
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
	rv = sfvprintf(f,form,args);

	va_end(args);
	return rv;
}

#if __STD_C
ssize_t sfvsprintf(char* s, size_t n, const char* form, va_list args)
#else
ssize_t sfvsprintf(s, n, form, args)
char*	s;
size_t	n;
char*	form;
va_list	args;
#endif
{
	Sfio_t	*f;
	ssize_t	rv;

	/* make a temp stream */
	if(!(f = sfnew(NIL(Sfio_t*),NIL(char*),(size_t)SF_UNBOUND,
                        -1,SF_WRITE|SF_STRING)) )
		return -1;

	if((rv = sfvprintf(f,form,args)) < 0 )
		return -1;
	if(s)
	{	if(sfputc(f, 0) < 0)
			return -1;
		if(n > 0)
			memcpy(s, f->data, rv < n ? rv+1 : n);
	}

	sfclose(f);

	_Sfi = rv;

	return rv;
}

#if __STD_C
ssize_t sfsprintf(char* s, size_t n, const char* form, ...)
#else
ssize_t sfsprintf(va_alist)
va_dcl
#endif
{
	va_list	args;
	ssize_t	rv;

#if __STD_C
	va_start(args,form);
#else
	reg char*	s;
	reg size_t	n;
	reg char*	form;
	va_start(args);
	s = va_arg(args,char*);
	n = va_arg(args,size_t);
	form = va_arg(args,char*);
#endif

	rv = sfvsprintf(s,n,form,args);
	va_end(args);

	return rv;
}
