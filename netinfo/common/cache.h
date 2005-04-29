#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdio.h>
#include <stdint.h>

#define CACHE_FLAG_REPLACE 0x00000001

typedef struct __cache_private cache_t;
typedef void (*cache_datum_callback)(void *datum);

cache_t *cache_new(uint32_t size, uint32_t flags);

void cache_set_retain_callback(cache_t *c, cache_datum_callback cb);
void cache_set_release_callback(cache_t *c, cache_datum_callback cb);

void *cache_find(cache_t *c, const char *key);
void *cache_find_reset(cache_t *c, const char *key);

void cache_insert(cache_t *c, const char *key, void *datum);
void cache_insert_ttl(cache_t *c, const char *key, void *datum, uint32_t ttl);
void cache_insert_ttl_time(cache_t *c, const char *key, void *datum, uint32_t ttl, uint32_t time);

void cache_delete(cache_t *c, const char *key);
void cache_delete_datum(cache_t *cin, void *d);

int cache_contains_datum(cache_t *cin, void *d);

void cache_sweep(cache_t *c);

void cache_free(cache_t *c);
void cache_print(cache_t *c, FILE *f);

#endif _CACHE_H_
