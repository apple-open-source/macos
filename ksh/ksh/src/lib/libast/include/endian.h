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
/*
 * linux/gnu compatibility
 */

#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <bytesex.h>

#define	__LITTLE_ENDIAN	1234
#define	__BIG_ENDIAN	4321
#define	__PDP_ENDIAN	3412

#if defined (__USE_BSD) && !defined(__STRICT_ANSI__)

#ifndef LITTLE_ENDIAN
#define	LITTLE_ENDIAN	__LITTLE_ENDIAN
#endif

#ifndef BIG_ENDIAN
#define	BIG_ENDIAN	__BIG_ENDIAN
#endif

#ifndef PDP_ENDIAN
#define	PDP_ENDIAN	__PDP_ENDIAN
#endif

#undef	BYTE_ORDER
#define	BYTE_ORDER	__BYTE_ORDER

#endif

#endif
