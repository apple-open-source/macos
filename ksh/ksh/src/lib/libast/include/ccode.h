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
 * Glenn Fowler
 * AT&T Research
 *
 * character code map interface
 *
 * NOTE: used for mapping between 8-bit character encodings
 *	 ISO character sets are handled by sfio
 */

#ifndef _CHARCODE_H
#define _CHARCODE_H	1

#include <ast.h>
#include <ast_ccode.h>

/* _cc_map[] for backwards compatibility -- drop 20050101 */

#if _BLD_ast && defined(__EXPORT__)
#define extern		extern __EXPORT__
#endif
#if !_BLD_ast && defined(__IMPORT__)
#define extern		extern __IMPORT__
#endif

extern const unsigned char*	_cc_map[];

#undef	extern

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern unsigned char*	_ccmap(int, int);
extern void*		_ccmapcpy(unsigned char*, void*, const void*, size_t);
extern void*		_ccmapstr(unsigned char*, void*, size_t);

extern int		ccmapid(const char*);
extern char*		ccmapname(int);
extern void*		ccnative(void*, const void*, size_t);

#undef	extern

#define CCOP(i,o)		((i)==(o)?0:(((o)<<8)|(i)))
#define CCIN(x)			((x)&0xFF)
#define CCOUT(x)		(((x)>>8)&0xFF)
#define CCCONVERT(x)		((x)&0xFF00)

#define CCCVT(x)		CCMAP(x,0)
#define CCMAP(i,o)		((i)==(o)?(unsigned char*)0:_ccmap(i,o))
#define CCMAPCHR(m,c)		((m)?m[c]:(c))
#define CCMAPCPY(m,t,f,n)	((m)?_ccmapcpy(m,t,f,n):memcpy(t,f,n))
#define CCMAPSTR(m,s,n)		((m)?_ccmapstr(m,s,n):(void*)(s))

#define ccmap(i,o)		CCMAP(i,o)
#define ccmapchr(m,c)		CCMAPCHR(m,c)
#define ccmapcpy(m,t,f,n)	CCMAPCPY(m,t,f,n)
#define ccmapstr(m,s,n)		CCMAPSTR(m,s,n)

#define CCMAPC(c,i,o)		((i)==(o)?(c):CCMAP(i,o)[c])
#define CCMAPM(t,f,n,i,o)	((i)==(o)?memcpy(t,f,n):_ccmapcpy(CCMAP(i,o),t,f,n))
#define CCMAPS(s,n,i,o)		((i)==(o)?(void*)(s):_ccmapstr(CCMAP(i,o),s,n))

#define ccmapc(c,i,o)		CCMAPC(c,i,o)
#define ccmapm(t,f,n,i,o)	CCMAPM(t,f,n,i,o)
#define ccmaps(s,n,i,o)		CCMAPS(s,n,i,o)

#endif
