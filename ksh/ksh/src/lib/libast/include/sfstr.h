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
#pragma prototyped
/*
 * macro interface for sfio write strings
 *
 * NOTE: see <stak.h> for an alternative interface
 *	 read operations require sfseek()
 */

#ifndef _SFSTR_H
#define _SFSTR_H

#include <sfio.h>

#define sfstropen()	sfnew((Sfio_t*)0,(char*)0,-1,-1,SF_WRITE|SF_STRING)
#define sfstrnew(m)	sfnew((Sfio_t*)0,(char*)0,-1,-1,(m)|SF_STRING)
#define sfstrclose(f)	sfclose(f)

#define sfstrtell(f)	((f)->_next - (f)->_data)
#define sfstrpend(f)	((f)->_endb - (f)->_next)
#define sfstrrel(f,p)	((p) == (0) ? (char*)(f)->_next : \
			 ((f)->_next += (p), \
			  ((f)->_next >= (f)->_data && (f)->_next  <= (f)->_endb) ? \
				(char*)(f)->_next : ((f)->_next -= (p), (char*)0) ) )

#define sfstrset(f,p)	(((p) >= 0 && (p) <= (f)->_size) ? \
				(char*)((f)->_next = (f)->_data+(p)) : (char*)0 )

#define sfstrbase(f)	((char*)(f)->_data)
#define sfstrsize(f)	((f)->_size)

#define sfstrrsrv(f,n)	(sfreserve(f,(long)(n),1)?(sfwrite(f,(char*)(f)->_next,0),(char*)(f)->_next):(char*)0)

#define sfstruse(f)	(sfputc(f,0), (char*)((f)->_next = (f)->_data) )

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern int		sfstrtmp(Sfio_t*, int, void*, size_t);

#undef	extern

#endif
