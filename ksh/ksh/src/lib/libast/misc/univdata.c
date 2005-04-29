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
 * AT&T Bell Laboratories
 *
 * universe common data
 */

#include "univlib.h"

#ifndef UNIV_MAX

char		univ_env[] = "__UNIVERSE__";

#else

#ifndef NUMUNIV

#if !_lib_universe
#undef	U_GET
#endif

#ifdef	U_GET
char*		univ_name[] = { "ucb", "att" };
#else
char*		univ_name[] = { "att", "ucb" };
#endif

int		univ_max = sizeof(univ_name) / sizeof(univ_name[0]);

#endif

char		univ_cond[] = "$(UNIVERSE)";

int		univ_size = sizeof(univ_cond) - 1;

#endif
