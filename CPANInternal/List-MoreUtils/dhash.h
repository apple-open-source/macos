#ifndef __DHASH_H__
#define __DHASH_H__

/* A special hash-type for use in part(). It is a store-only
 * hash in that all key/value pairs are put into the hash. Then it is sorted by
 * keys ascending with dhash_sort_final where the empty elements come at the end
 * of the internal array. This need for sorting is actually what prevents us from
 * using a dhash-based implementaion right now as it is the bottleneck for cases
 * with many very small partitions.
 *
 * It doesn't use a linked list for collision recovery. Instead, on collision it will
 * walk right in the array to find the first free spot. This search should never take
 * too long as it uses a fairly good integer-hash function.
 *
 * The 'step' parameter isn't currently used. 
 */

#include <stdlib.h>	/* for qsort() */

#define INITIAL_SIZE 4

typedef unsigned int hash_t;

typedef struct {
    int key;
    AV *val;
} dhash_val_t;

typedef struct {
    int max;
    int size;
    int count;
    int step;
    dhash_val_t *ary;
} dhash_t;

void dhash_dump(dhash_t *h);

int cmp (dhash_val_t *a, dhash_val_t *b) {
    /* all empty buckets should be at the end of the array */
    if (!a->val)
	return 1;
    if (!b->val)
	return -1;
    return a->key - b->key;
}

dhash_t * dhash_init() {
    dhash_t *h;
    New(0, h, 1, dhash_t);
    Newz(0, h->ary, INITIAL_SIZE, dhash_val_t);
    h->max = 0;
    h->size = INITIAL_SIZE;
    h->count = 0;
    return h;
}

void dhash_destroy(dhash_t *h) {
    Safefree(h->ary);
    Safefree(h);
}

inline hash_t HASH(register hash_t k) {
    k += (k << 12);
    k ^= (k >> 22);
    k += (k << 4);
    k ^= (k >> 9);
    k += (k << 10);
    k ^= (k >> 2);
    k += (k << 7);
    k ^= (k >> 12);
    return k;
}

void dhash_insert(dhash_t *h, int key, SV *sv, register hash_t hash) {

    while (h->ary[hash].val && h->ary[hash].key != key)
	hash = (hash + 1) % h->size;
    
    if (!h->ary[hash].val) {
	h->ary[hash].val = newAV();
	h->ary[hash].key = key;
	h->count++;
    }
    
    av_push(h->ary[hash].val, sv);
    SvREFCNT_inc(sv);
}

void dhash_resize(dhash_t *h) {
    
    register int i;
    register hash_t hash;
    dhash_val_t *old = h->ary;
    
    h->size <<= 1;
    h->count = 0;
    Newz(0, h->ary, h->size, dhash_val_t);
    
    for (i = 0; i < h->size>>1; ++i) {
	if (!old[i].val)
	    continue;
	hash = HASH(old[i].key) % h->size;
	while (h->ary[hash].val)
	    hash = (hash + 1) % h->size;
	h->ary[hash] = old[i];
	++h->count;
    }
    Safefree(old);
}

void dhash_store(dhash_t *h, int key, SV *val) {
    hash_t hash;
    if ((double)h->count / (double)h->size > 0.75)
	dhash_resize(h);
    hash = HASH(key) % h->size;
    dhash_insert(h, key, val, hash);
    if (key > h->max)
	h->max = key;
}

/* Once this is called, the hash is no longer useable. The only thing
 * that may be done with it is iterate over h->ary to get the values
 * sorted by keys */
void dhash_sort_final(dhash_t *h) {
    qsort(h->ary, h->size, sizeof(dhash_val_t), (int(*)(const void*,const void*))cmp);
}

void dhash_dump(dhash_t *h) {
    int i;
    fprintf(stderr, "max=%i, size=%i, count=%i, ary=%p\n", h->max, h->size, h->count, h->ary);
    for (i = 0; i < h->size; i++) {
	fprintf(stderr, "%2i:  key=%-5i => val=(AV*)%p\n", i, h->ary[i].key, h->ary[i].val);
    }
}

#endif
