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
 * hash table library
 */

#include "hashlib.h"

/*
 * apply walker to each active bucket in the table
 */

int
hashwalk(Hash_table_t* tab, int flags, register int (*walker)(const char*, char*, void*), void* handle)
{
	register Hash_bucket_t*	b;
	register int		v;
	Hash_position_t*	pos;

	if (!(pos = hashscan(tab, flags)))
		return(-1);
	v = 0;
	while (b = hashnext(pos))
		if ((v = (*walker)(hashname(b), (tab->flags & HASH_VALUE) ? b->value : (char*)b, handle)) < 0)
			break;
	hashdone(pos);
	return(v);
}
