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
 * change table size and rehash
 * size must be a power of 2
 */

void
hashsize(register Hash_table_t* tab, int size)
{
	register Hash_bucket_t**	old_s;
	register Hash_bucket_t**	new_s;
	register Hash_bucket_t*		old_b;
	register Hash_bucket_t*		new_b;
	Hash_bucket_t**			old_sx;
	unsigned int			index;
	Hash_region_f			region;
	void*				handle;

	if (size > 0 && size != tab->size && !(size & (size - 1)))
	{
		if (region = tab->root->local->region)
		{
			handle = tab->root->local->handle;
			new_s = (Hash_bucket_t**)(*region)(handle, NiL, sizeof(Hash_bucket_t*) * size, 0);
		}
		else new_s = newof(0, Hash_bucket_t*, size, 0);
		if (!new_s) tab->flags |= HASH_FIXED;
		else
		{
			old_sx = (old_s = tab->table) + tab->size;
			tab->size = size;
			while (old_s < old_sx)
			{
				old_b = *old_s++;
				while (old_b)
				{
					new_b = old_b;
					old_b = old_b->next;
					index = new_b->hash;
					HASHMOD(tab, index);
					new_b->next = new_s[index];
					new_s[index] = new_b;
				}
			}
			if ((tab->flags & (HASH_RESIZE|HASH_STATIC)) != HASH_STATIC)
			{
				if (region) (*region)(handle, tab->table, 0, 0);
				else free(tab->table);
			}
			tab->table = new_s;
			tab->flags |= HASH_RESIZE;
		}
	}
}
