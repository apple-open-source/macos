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
 * regular expression library definitions
 */

#ifndef _RE_H
#define _RE_H

#include <sfio.h>

#define RE_ALL		(1<<0)	/* substitute all occurrences		*/
#define RE_EDSTYLE	(1<<1)	/* ed(1) style meta characters		*/
#define RE_LOWER	(1<<2)	/* substitute to lower case		*/
#define RE_MATCH	(1<<3)	/* record matches in Re_program_t.match	*/
#define RE_UPPER	(1<<4)	/* substitute to upper case		*/
#define RE_LEFTANCHOR	(1<<5)	/* match anchored on left		*/
#define RE_RIGHTANCHOR	(1<<6)	/* match anchored on right		*/
#define RE_EXTERNAL	8	/* first external flag bit		*/

typedef struct			/* sub-expression match			*/
{
	char*	sp;		/* start in source string		*/
	char*	ep;		/* end in source string			*/
} Re_match_t;

typedef struct			/* compiled regular expression program	*/
{
	Re_match_t	match['9'-'0'+1];/* sub-expression match table*/
#ifdef _RE_PROGRAM_PRIVATE_
	_RE_PROGRAM_PRIVATE_
#endif
} Re_program_t, reprogram;

/*
 * interface routines
 */

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern Re_program_t*	recomp(const char*, int);
extern int		reexec(Re_program_t*, const char*);
extern void		refree(Re_program_t*);
extern void		reerror(const char*);
extern char*		resub(Re_program_t*, const char*, const char*, char*, int);
extern void		ressub(Re_program_t*, Sfio_t*, const char*, const char*, int);

#undef	extern

#endif
