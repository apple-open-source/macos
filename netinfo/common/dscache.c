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
#define CACHE_MIN_MERIT 1

/* remove n items from the cache */
static void
dscache_prune(dscache *cache, uint32_t n)
{
	dscache_node *x;
	uint32_t i;

	if (cache == NULL) return;

	if (cache->cache_head == NULL) return;

	for (i = 0; (i < n) && (cache->cache_head != NULL); i++)
	{
		cache->prune_count++;

		x = cache->cache_head->next;
		dsrecord_release(cache->cache_head->record);
		free(cache->cache_head);
		cache->cache_head = x;
	}
}

static uint32_t
record_merit(dsrecord *r)
{
	if (r->dsid == 0) return (uint32_t)-1;
	return r->sub_count;
}

dscache *
dscache_new(uint32_t m)
{
	dscache *cache;

	cache = (dscache *)calloc(1, sizeof(dscache));

	cache->cache_max = m;
	if (m == 0) cache->cache_max = CACHE_DEFAULT_SIZE;

	return cache;
}

void
dscache_free(dscache *cache)
{
	if (cache == NULL) return;

	dscache_flush(cache);
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
	uint32_t merit;
	dscache_node *p, *n, *new;

	if (cache == NULL) return;
	if (r == NULL) return;

	merit = record_merit(r);
	if (merit < CACHE_MIN_MERIT) return;

	if (cache->cache_size == cache->cache_max)
	{
		if ((cache->cache_head != NULL) && (merit < cache->cache_head->merit)) return;
		dscache_prune(cache, 1);
	}

	cache->cache_size++;
	cache->save_count++;
	
	new = (dscache_node *)calloc(1, sizeof(dscache_node));
	new->merit = merit;
	new->record = dsrecord_retain(r);

	if (cache->cache_head == NULL)
	{
		cache->cache_head = new;
		return;
	}

	if (merit < cache->cache_head->merit)
	{
		new->next = cache->cache_head;
		cache->cache_head = new;
		return;
	}

	p = cache->cache_head;
	for (n = p->next; n != NULL; n = n->next)
	{
		if (merit < n->merit)
		{
			new->next = n;
			p->next = new;
			return;
		}
		p = n;
	}

	p->next = new;
}

void 
dscache_remove(dscache *cache, uint32_t dsid)
{
	dscache_node *p, *n;
	
	if (cache == NULL) return;
	if (cache->cache_head == NULL) return;
	if (cache->cache_head->record->dsid == dsid)
	{
		dsrecord_release(cache->cache_head->record);
		n = cache->cache_head->next;
		free(cache->cache_head);
		cache->cache_head = n;
		return;
	}

	p = cache->cache_head;
	for (n = p->next; n != NULL; n = n->next)
	{
		if (n->record == NULL) continue;
		if (n->record->dsid == dsid) 
		{
			cache->remove_count++;

			p->next = n->next;
			dsrecord_release(n->record);
			free(n);
			return;
		}
		p = n;
	}
}

dsrecord *
dscache_fetch(dscache *cache, uint32_t dsid)
{
	dscache_node *n;
	
	if (cache == NULL) return NULL;

	for (n = cache->cache_head; n != NULL; n = n->next)
	{
		if (n->record == NULL) continue;
		if (n->record->dsid == dsid)
		{
			cache->fetch_count++;
			return dsrecord_retain(n->record);
		}
	}

	return NULL;
}

void
dscache_print_statistics(dscache *cache, FILE *f)
{
	dscache_node *n;
	uint32_t i;
	
	if (f == NULL) return;
	if (cache == NULL) fprintf(f, "-nil-\n");

	fprintf(f, "cache_size = %u\n", cache->cache_size);

	for (n = cache->cache_head; n != NULL; n = n->next)
	{
		if (n->record == NULL) continue;
		fprintf(f, "%u: %u %u\n", i, n->record->dsid, n->merit);
	}
}
