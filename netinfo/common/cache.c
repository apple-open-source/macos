#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include "cache.h"

#define mix(a, b, c) \
{ \
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<< 8); \
	c -= a; c -= b; c ^= (b>>13); \
	a -= b; a -= c; a ^= (c>>12); \
	b -= c; b -= a; b ^= (a<<16); \
	c -= a; c -= b; c ^= (b>> 5); \
	a -= b; a -= c; a ^= (c>> 3); \
	b -= c; b -= a; b ^= (a<<10); \
	c -= a; c -= b; c ^= (b>>15); \
}

typedef struct cache_node_s
{
	char *key;
	void *datum;
	time_t best_before;
	uint32_t ttl;
	struct cache_node_s *next;
} cache_node_t;

typedef struct __cache_private
{
	uint32_t bucket_count;
	cache_node_t **bucket;
	uint32_t hash_mask;
	uint32_t flags;
	cache_datum_callback release_callback;
	cache_datum_callback retain_callback;
} cache_private_t;

void 
cache_set_retain_callback(cache_t *c, cache_datum_callback cb)
{
	if (c == NULL) return;
	c->retain_callback = cb;
}

void
cache_set_release_callback(cache_t *c, cache_datum_callback cb)
{
	if (c == NULL) return;
	c->release_callback = cb;
}

cache_t *
cache_new(uint32_t size, uint32_t flags)
{
	cache_private_t *c;

	if (size == 0) return NULL;

	c = (cache_t *)calloc(1, sizeof(cache_t));
	if (c == NULL) return NULL;

	c->bucket_count = size;
	c->bucket = (cache_node_t **)calloc(c->bucket_count, sizeof(sizeof(cache_node_t *)));
	if (c->bucket == NULL)
	{
		free(c);
		return NULL;
	}

	c->hash_mask = size - 1;
	c->flags = flags;
	
	return (cache_t *)c;
}

static uint32_t
cache_hash(const char *k, uint32_t mask)
{
	uint32_t a, b, c, l, len;

	l = strlen(k);

	len = l;
	a = b = 0x9e3779b9; 
	c = 0;
	
	while (len >= 12)
	{
		a += (k[0] + ((uint32_t)k[1]<<8) + ((uint32_t)k[ 2]<<16) + ((uint32_t)k[ 3]<<24));
		b += (k[4] + ((uint32_t)k[5]<<8) + ((uint32_t)k[ 6]<<16) + ((uint32_t)k[ 7]<<24));
		c += (k[8] + ((uint32_t)k[9]<<8) + ((uint32_t)k[10]<<16) + ((uint32_t)k[11]<<24));
		
		mix(a, b, c);
		
		k += 12;
		len -= 12;
	}
	
	c += l;
	switch(len)
	{
		case 11: c += ((uint32_t)k[10]<<24);
		case 10: c += ((uint32_t)k[9]<<16);
		case 9 : c += ((uint32_t)k[8]<<8);
			
		case 8 : b += ((uint32_t)k[7]<<24);
		case 7 : b += ((uint32_t)k[6]<<16);
		case 6 : b += ((uint32_t)k[5]<<8);
		case 5 : b += k[4];
			
		case 4 : a += ((uint32_t)k[3]<<24);
		case 3 : a += ((uint32_t)k[2]<<16);
		case 2 : a += ((uint32_t)k[1]<<8);
		case 1 : a += k[0];
	}
	
	mix(a, b, c);
	
	return c & mask;
}

static void
cache_remove_node(cache_private_t *c, uint32_t b, cache_node_t *p, cache_node_t *n)
{
	if (p == NULL) c->bucket[b] = n->next;
	else p->next = n->next;
	if (n->key != NULL) free(n->key);
	if (c->release_callback != NULL) c->release_callback(n->datum);
	free(n);
}

static void *
cache_find_internal(cache_t *cin, const char *key, uint32_t reset)
{
	cache_private_t *c;
	cache_node_t *p, *n;
	uint32_t b;
	time_t now;

	if (cin == NULL) return NULL;
	if (key == NULL) return NULL;

	c = (cache_private_t *)cin;
	b = cache_hash(key, c->hash_mask);

	p = NULL;
	for (n = c->bucket[b]; n != NULL; n = n->next)
	{
		if ((n->key != NULL) && (!strcmp(key, n->key)))
		{
			if (n->ttl != 0)
			{
				now = time(NULL);
				if (now >= n->best_before)
				{
					cache_remove_node(c, b, p, n);
					return NULL;
				}
				
				if (reset != 0) n->best_before = now + n->ttl;
			}

			return n->datum;
		}
		p = n;
	}

	return NULL;
}

void *
cache_find_reset(cache_t *cin, const char *key)
{
	return cache_find_internal(cin, key, 1);
}

void *
cache_find(cache_t *cin, const char *key)
{
	return cache_find_internal(cin, key, 0);
}

void
cache_insert_ttl_time(cache_t *cin, const char *key, void *datum, uint32_t ttl, uint32_t time)
{
	cache_private_t *c;
	cache_node_t *p, *n;
	uint32_t b;
	
	if (cin == NULL) return;
	if (key == NULL) return;
	if (datum == NULL) return;
	
	c = (cache_private_t *)cin;
	b = cache_hash(key, c->hash_mask);

	p = NULL;
	for (n = c->bucket[b]; n != NULL; n = n->next)
	{
		if ((n->key != NULL) && (!strcmp(key, n->key)))
		{
			if (c->flags & CACHE_FLAG_REPLACE)
			{
				cache_remove_node(c, b, p, n);
				break;
			}
			else
			{
				n->best_before = time + ttl;
				return;
			}
		}
		p = n;
	}

	n = (cache_node_t *)malloc(sizeof(cache_node_t));
	n->key = strdup(key);

	if (c->retain_callback != NULL) c->retain_callback(datum);

	n->datum = datum;
	n->best_before = time + ttl; 
	n->ttl = ttl; 
	n->next = c->bucket[b];
	c->bucket[b] = n;
}


void
cache_insert_ttl(cache_t *cin, const char *key, void *datum, uint32_t ttl)
{
	return cache_insert_ttl_time(cin, key, datum, ttl, time(NULL));
}

void
cache_insert(cache_t *cin, const char *key, void *datum)
{
	return cache_insert_ttl_time(cin, key, datum, 0, 0);
}

void
cache_delete(cache_t *cin, const char *key)
{
	cache_private_t *c;
	cache_node_t *n, *p;
	uint32_t b;
	
	if (cin == NULL) return;
	if (key == NULL) return;
	
	c = (cache_private_t *)cin;
	b = cache_hash(key, c->hash_mask);
	
	p = NULL;
	for (n = c->bucket[b]; n != NULL; n = n->next)
	{
		if ((n->key != NULL) && (!strcmp(key, n->key)))
		{
			cache_remove_node(c, b, p, n);
			return;
		}
		p = n;
	}
}

void
cache_delete_datum(cache_t *cin, void *d)
{
	cache_private_t *c;
	cache_node_t *n, *p, *x;
	uint32_t b;
	
	if (cin == NULL) return;
	if (d == NULL) return;
	
	c = (cache_private_t *)cin;

	for (b = 0; b < c->bucket_count; b++)
	{
		p = NULL;
		x = NULL;
		for (n = c->bucket[b]; n != NULL; n = x)
		{
			x = n->next;
			if (n->datum == d) cache_remove_node(c, b, p, n);
			p = n;
		}
	}
}

int
cache_contains_datum(cache_t *cin, void *d)
{
	cache_private_t *c;
	cache_node_t *n;
	uint32_t b;
	
	if (cin == NULL) return 0;
	if (d == NULL) return 0;
	
	c = (cache_private_t *)cin;
	
	for (b = 0; b < c->bucket_count; b++)
	{
		for (n = c->bucket[b]; n != NULL; n = n->next)
		{
			if (n->datum == d) return 1;
		}
	}

	return 0;
}

void
cache_sweep(cache_t *cin)
{
	cache_private_t *c;
	cache_node_t *p, *n, *x;
	uint32_t b;
	time_t now;
	
	if (cin == NULL) return;

	now = time(NULL);

	c = (cache_private_t *)cin;
	
	for (b = 0; b < c->bucket_count; b++)
	{
		x = NULL;
		p = NULL;
		for (n = c->bucket[b]; n != NULL; n = x)
		{
			x = n->next;
			if ((n->ttl != 0) && (now >= n->best_before)) cache_remove_node(c, b, p, n);
			else p = n;
		}
	}
}

void
cache_free(cache_t *cin)
{
	cache_private_t *c;
	cache_node_t *n, *x;
	uint32_t b;
	
	if (cin == NULL) return;
	
	c = (cache_private_t *)cin;
	
	for (b = 0; b < c->bucket_count; b++)
	{
		x = NULL;
		for (n = c->bucket[b]; n != NULL; n = x)
		{
			x = n->next;
			free(n->key);
			if (c->release_callback != NULL) c->release_callback(n->datum);
			free(n);
		}
	}
	
	free(c->bucket);
	free(c);
}

void
cache_print(cache_t *cin, FILE *f)
{
	cache_private_t *c;
	cache_node_t *n;
	uint32_t b;
	
	if (cin == NULL) return;
	
	c = (cache_private_t *)cin;
	
	for (b = 0; b < c->bucket_count; b++)
	{
		n = c->bucket[b];
		if (n != NULL) fprintf(f, "Bucket %d\n", b);
		for (; n != NULL; n = n->next)
		{
			fprintf(f, "\t%s 0x%08x %u %s", n->key, (unsigned int)n->datum,  (unsigned int)n->ttl, ctime((time_t *)&(n->best_before)));
		}
	}
}

