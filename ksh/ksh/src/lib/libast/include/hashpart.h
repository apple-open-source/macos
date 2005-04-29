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
 * K. P. Vo
 * G. S. Fowler
 * AT&T Research
 *
 * ``the best'' combined linear congruent checksum/hash/PRNG
 */

#ifndef _HASHPART_H
#define _HASHPART_H

#define HASH_ADD(h)	(0x9c39c33dL)

#if __sparc__ || __sparc || sparc

#define HASH_A(h,n)	((((h) << 2) - (h)) << (n))
#define HASH_B(h,n)	((((h) << 4) - (h)) << (n))
#define HASH_C(h,n)	((HASH_A(h,7) + HASH_B(h,0)) << (n))
#define HASH_MPY(h)	(HASH_C(h,22)+HASH_C(h,10)+HASH_A(h,6)+HASH_A(h,3)+(h))

#else

#define HASH_MPY(h)	((h)*0x63c63cd9L)

#endif

#define HASHPART(h,c)	(h = HASH_MPY(h) + HASH_ADD(h) + (c))

#endif
