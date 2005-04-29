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
 * David Korn
 * AT&T Research
 *
 * Interface definitions for a stack-like storage library
 *
 */

#ifndef _STK_H
#define _STK_H

#include <sfio.h>

#define _Stk_data	_Stak_data

#define stkstd		(&_Stk_data)

#define	Stk_t		Sfio_t

#define STK_SMALL	1		/* small stkopen stack		*/
#define STK_NULL	2		/* return NULL on overflow	*/

#define	stkptr(sp,n)	((char*)((sp)->_data)+(n))
#define stktop(sp)	((char*)(sp)->_next)
#define	stktell(sp)	((sp)->_next-(sp)->_data)
#define stkseek(sp,n)	((n)==0?(char*)((sp)->_next=(sp)->_data):_stkseek(sp,n))

#if _BLD_ast && defined(__EXPORT__)
#define extern		extern __EXPORT__
#endif
#if !_BLD_ast && defined(__IMPORT__)
#define extern		extern __IMPORT__
#endif

extern Sfio_t		_Stk_data;

#undef	extern

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern Stk_t*		stkopen(int);
extern Stk_t*		stkinstall(Stk_t*, char*(*)(int));
extern int		stkclose(Stk_t*);
extern int		stklink(Stk_t*);
extern char*		stkalloc(Stk_t*, unsigned);
extern char*		stkcopy(Stk_t*,const char*);
extern char*		stkset(Stk_t*, char*, unsigned);
extern char*		_stkseek(Stk_t*, unsigned);
extern char*		stkfreeze(Stk_t*, unsigned);

#undef	extern

#endif
