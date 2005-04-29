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
 * OBSOLETE 19970101 -- use ccmapcpy(op,ip,n,CC_EBCDIC2,CC_ASCII)
 */

#include <ast.h>
#include <ccode.h>

/*
 * convert n bytes of CC_EBCDIC2 ip to CC_ASCII in op
 * ip==op is ok
 */

void*
memetoa(void* op, const void* ip, size_t n)
{
	return op == ip ? ccmaps(op, n, CC_EBCDIC2, CC_ASCII) : ccmapcpy(op, ip, n, CC_EBCDIC2, CC_ASCII);
}
