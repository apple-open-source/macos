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

/*	Construct a string with the given format and data.
**	This function allocates space as necessary to store the string.
**	This avoids overflow problems typical with sprintf() in stdio.
**
**	Written by Kiem-Phong Vo.
*/

#if __STD_C
char* sfprints(const char* form, ...)
#else
char* sfprints(va_alist)
va_dcl
#endif
{
	va_list		args;
	reg int		rv;
	static Sfio_t*	f;

#if __STD_C
	va_start(args,form);
#else
	reg char	*form;
	va_start(args);
	form = va_arg(args,char*);
#endif

	/* make a fake stream */
	if(!f &&
	   !(f = sfnew(NIL(Sfio_t*),NIL(char*),(size_t)SF_UNBOUND,
			-1,SF_WRITE|SF_STRING)) )
		return NIL(char*);

	sfseek(f,(Sfoff_t)0,0);
	rv = sfvprintf(f,form,args);
	va_end(args);

	if(rv < 0 || sfputc(f,'\0') < 0)
		return NIL(char*);

	_Sfi = (f->next - f->data) - 1;
	return (char*)f->data;
}
