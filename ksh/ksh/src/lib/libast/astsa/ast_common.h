/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#include "ast_sa.h"
#include <sys/types.h>

#define Void_t	void
#define _ARG_(x)	x
#define _BEGIN_EXTERNS_
#define _END_EXTERNS_
#define __STD_C		1

#if _hdr_stdint
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#define _typ_int32_t	1
#ifdef _ast_int8_t
#define _typ_int64_t	1
#endif
