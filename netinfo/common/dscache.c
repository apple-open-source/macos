/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <NetInfo/dscache.h>
#include <NetInfo/dsrecord.h>
#include <NetInfo/dsindex.h>
#include <stdlib.h>
#include <stdio.h>

#define CACHE_DEFAULT_SIZE 1000

/* remove n items from the cache */
static u_int32_t
dscache_prune(dscache *cache, u_int32_t n)
{
	u_int32_t i, j, x, m;

	if (cache == NULL) return (u_int32_t)-1;

	if (cache->cache_count == 0) return 0;
	if (n == 0) return (u_int32_t)-1;

	if (n >= cache->cache_count)
	{
		cache->prune_count += cache->cache_count;
	
		for (i = 0; i < cache->cache_size; i++)
		{
			if (cache->cache[i].record == NULL) continue;

			cache->cache[i].merit = 0;
			dsrecord_release(cache->cache[i].record);
			cache->cache[i].record = NULL;
		}
		cache->cache_count = 0;
		return 0;
	}

	cache->prune_count += n;
	x = 0;

	for (j = 0; j < n; j++)
	{
		x = (u_int32_t)-1;
		m = (u_int32_t)-1;

		for (i = 0; i < cache->cache_size; i++)
		{
			if (cache->cache[i].record == NULL) continue;

			if (cache->cache[i].merit < m)
			{
				x = i;
				m = cache->cache[i].merit;
			}
		}

		cache->cache[x].merit = 0;
		dsrecord_release(cache->cache[x].record);
		cache->cache[x].record = NULL;
		cache->cache_count--;
	}

	for (i = 0; i < cache->cache_size; i++) 
		if (cache->cache[i].merit > 1) cache->cache[i].merit--;

	return x;
}

static u_int32_t
index_merit(dsrecord *r)
{
	dsindex *x;
	u_int32_t i, m;

	if (r == NULL) return 0;
	if (r->index == NULL) return 0;

	x = r->index;
	m = 0;
	for (i = 0; i < x->key_count; i++) m += x->kindex[i]->val_count;
	return m;
}

dscache *
dscache_new(u_int32_t m)
{
	u_int32_t i;
	dscache *cache;

	cache = (dscache *)malloc(sizeof(dscache));

	cache->cache_count = 0;
	cache->prune_count = 0;
	cache->save_count = 0;
	cache->remove_count = 0;
	cache->fetch_count = 0;

	cache->cache_size = m;
	if (m == 0) cache->cache_size = CACHE_DEFAULT_SIZE;

	cache->cache = (dscache_record *)malloc(cache->cache_size * sizeof(dscache_record));
	for (i = 0; i < cache->cache_size; i++)
	{
		cache->cache[i].merit = 0;
		cache->cache[i].record = NULL;
	}
	return cache;
}

void
dscache_free(dscache *cache)
{
	if (cache == NULL) return;
	dscache_flush(cache);
	free(cache->cache);
	free(cache);
}

void
dscache_flush(dscache *cache)
{
	if (cache == NULL) return;
	dscache_prune(cache, cache->cache_size);
}

void
dscache_save(dscache *cache, dsrecord *r)
{
	u_int32_t i, where;

	if (r == NULL) return;

	where = dscache_index(cache, r->dsid);
	if (where != (u_int32_t)-1)
	{
		/* Cache already contains this record id - update */
		dsrecord_retain(r);
		dsrecord_release(cache->cache[where].record);
		cache->cache[where].record = r;
		if (r->dsid == 0) cache->cache[where].merit = (u_int32_t)-1;
		else cache->cache[where].merit = cache->cache_size + index_merit(r);
		return;
	}

	cache->save_count++;

	if (cache->cache_count == cache->cache_size)
	{
		where = dscache_prune(cache, 1);
	}
	else
	{
		for (i = 0; i < cache->cache_size; i++)
		{
			if (cache->cache[i].record == NULL)
			{
				where = i;
				break;
			}
		}
	}

	if (where >= cache->cache_size) where = 0;
	if (cache->cache[where].record != NULL)
		dsrecord_release(cache->cache[where].record);

	dsrecord_retain(r);
	cache->cache[where].record = r;
	if (r->dsid == 0) cache->cache[where].merit = (u_int32_t)-1;
	else cache->cache[where].merit = cache->cache_size + index_merit(r);
	cache->cache_count++;
}

void 
dscache_remove(dscache *cache, u_int32_t dsid)
{
	u_int32_t where;

	if (cache == NULL) return;

	where = dscache_index(cache, dsid);
	if (where == IndexNull) return;

	cache->remove_count++;

	dsrecord_release(cache->cache[where].record);
	cache->cache[where].record = NULL;
	cache->cache[where].merit = 0;
	cache->cache_count--;
}

u_int32_t
dscache_index(dscache *cache, u_int32_t dsid)
{
	u_int32_t i;

	if (cache == NULL) return IndexNull;

	if (cache->cache_count == 0) return IndexNull;

	for (i = 0; i < cache->cache_size; i++)
	{
		if (cache->cache[i].record == NULL) continue;
		if (cache->cache[i].record->dsid == dsid) return i;
	}

	return IndexNull;
}

dsrecord *
dscache_fetch(dscache *cache, u_int32_t dsid)
{
	u_int32_t where;

	if (cache == NULL) return NULL;

	where = dscache_index(cache, dsid);
	if (where == IndexNull) return NULL;

	cache->fetch_count++;

	if (dsid == 0) cache->cache[where].merit = (u_int32_t)-1;
	else cache->cache[where].merit = cache->cache_size + index_merit(cache->cache[where].record);
	dsrecord_retain(cache->cache[where].record);
	return(cache->cache[where].record);
}

void
dscache_print_statistics(dscache *cache, FILE *f)
{
	u_int32_t i;

	if (f == NULL) return;
	if (cache == NULL) fprintf(f, "-nil-\n");

	fprintf(f, "cache_count = %u\n", cache->cache_count);

	for (i = 0; i < cache->cache_size; i++)
	{
		if (cache->cache[i].record == NULL) continue;
		fprintf(f, "%u: %u %u\n", i, cache->cache[i].record->dsid,
			cache->cache[i].merit);
	}
}
