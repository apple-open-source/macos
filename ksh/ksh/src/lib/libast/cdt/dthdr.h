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
#ifndef _DTHDR_H
#define _DTHDR_H	1
#ifndef _BLD_cdt
#define _BLD_cdt	1
#endif

/*	Internal definitions for libcdt.
**	Written by Kiem-Phong Vo (5/25/96)
*/

#if _PACKAGE_ast
#include	<ast.h>
#endif

#include	<cdt.h>

/* short-hand notations */
#define NIL(t)	((t)0)
#define reg	register
#define uint	unsigned int
#define left	hl._left
#define hash	hl._hash
#define htab	hh._htab
#define head	hh._head

/* this must be disjoint from DT_METHODS */
#define DT_FLATTEN	010000	/* dictionary already flattened	*/
#define DT_WALK		020000	/* hash table being walked	*/

/* how the Dt_t handle was allocated */
#define DT_MALLOC	0
#define DT_MEMORYF	1

/* max search length before splaying */
#define DT_MINP		(sizeof(size_t)*8 - 2)

/* hash start size and load factor */
#define HSLOT		(32)
#define HRESIZE(n)	((n) << 1)
#define HLOAD(s)	((s) << 1)
#define HINDEX(n,h)	((h)&((n)-1))

#define UNFLATTEN(dt) \
		((dt->data->type&DT_FLATTEN) ? dtrestore(dt,NIL(Dtlink_t*)) : 0)

/* tree rotation/linking functions */
#define rrotate(x,y)	((x)->left  = (y)->right, (y)->right = (x))
#define lrotate(x,y)	((x)->right = (y)->left,  (y)->left  = (x))
#define rlink(r,x)	((r) = (r)->left   = (x) )
#define llink(l,x)	((l) = (l)->right  = (x) )

#define RROTATE(x,y)	(rrotate(x,y), (x) = (y))
#define LROTATE(x,y)	(lrotate(x,y), (x) = (y))

#if !_PACKAGE_ast
_BEGIN_EXTERNS_
extern Void_t*	malloc _ARG_((size_t));
extern Void_t*	realloc _ARG_((Void_t*, size_t));
extern void	free _ARG_((Void_t*));
_END_EXTERNS_
#endif

#endif /* _DTHDR_H */
