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
 * ftw,nftw over ftwalk
 */

#ifndef _FTW_H
#define _FTW_H

#define FTW		FTWALK
#include <ftwalk.h>
#undef			FTW

#define FTW_SLN		(FTW_SL|FTW_NR)

#define FTW_PHYS	(FTW_PHYSICAL)
#define FTW_CHDIR	(FTW_DOT)
#define FTW_DEPTH	(FTW_POST)
#define FTW_OPEN	(0)

struct FTW
{
	int		quit;
	int		base;
	int		level;
};

#define FTW_SKD		FTW_SKIP
#define FTW_PRUNE	FTW_SKIP

#if _BLD_ast && defined(__EXPORT__)
#define extern		__EXPORT__
#endif

extern int	ftw(const char*, int(*)(const char*, const struct stat*, int), int);
extern int	nftw(const char*, int(*)(const char*, const struct stat*, int, struct FTW*), int, int);

#undef	extern

#endif
