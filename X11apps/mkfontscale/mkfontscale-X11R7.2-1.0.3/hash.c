/*
  Copyright (c) 2003 by Juliusz Chroboczek

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
/* $XFree86$ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hash.h"
#include "list.h"

#define LOG2_NUMBUCKETS 10
#define NUMBUCKETS (1 << LOG2_NUMBUCKETS)

static unsigned
hash(char *string)
{
    int i;
    unsigned u = 0;
    for(i = 0; string[i] != '\0'; i++)
        u = (u<<5) + (u >> (LOG2_NUMBUCKETS - 5)) + (unsigned char)string[i];
    return (u & (NUMBUCKETS - 1));
}

static char
lwr(char a)
{
    if(a >= 'A' && a <= 'Z')
        return a | 0x20;
    else
        return a;
}

static void
strcpy_lwr(char *dst, char *src)
{
    while(1) {
        *dst = lwr(*src);
        if(*src == '\0')
            break;
        src++;
        dst++;
    }
}

static int
strcmp_lwr(char *a, char *b)
{
    while(*a != '\0' && *b != '\0') {
        if(lwr(*a) != lwr(*b)) {
            if(lwr(*a) < lwr(*b))
                return -1;
            if(lwr(*a) > lwr(*b))
                return 1;
        }
        a++;
        b++;
    }
    if (*a != '\0')
        return -1;
    else if(*b == '\0')
        return 1;
    else
        return 0;
}

HashTablePtr
makeHashTable()
{
    return calloc(NUMBUCKETS, sizeof(HashBucketPtr));
}

void
destroyHashTable(HashTablePtr table)
{
    int i;
    HashBucketPtr bp;

    for(i = 0; i < NUMBUCKETS; i++) {
        while(table[i]) {
            bp = table[i];
            table[i] = table[i]->next;
            free(bp->key);
            free(bp->value);
            free(bp);
        }
    }
    free(table);
}

char *
getHash(HashTablePtr table, char *key)
{
    int i = hash(key);
    HashBucketPtr bp;
    for(bp = table[i]; bp; bp = bp->next) {
        if(strcmp_lwr(bp->key, key) == 0)
            return bp->value;
    }
    return NULL;
}

int
putHash(HashTablePtr table, char *key, char *value, int prio)
{
    int i = hash(key);
    char *keycopy = NULL, *valuecopy = NULL;
    HashBucketPtr bp;
    for(bp = table[i]; bp; bp = bp->next) {
        if(strcmp_lwr(bp->key, key) == 0) {
            if(prio > bp->prio) {
                keycopy = malloc(strlen(key) + 1);
                if(keycopy == NULL) goto fail;
                strcpy_lwr(keycopy, key);
                valuecopy = malloc(strlen(value) + 1);
                if(valuecopy == NULL) goto fail;
                strcpy(valuecopy, value);
                free(bp->key);
                free(bp->value);
                bp->key = keycopy;
                bp->value = valuecopy;
            }
            return 1;
        }
    }
    keycopy = malloc(strlen(key) + 1);
    if(keycopy == NULL)
        goto fail;
    strcpy_lwr(keycopy, key);
    valuecopy = malloc(strlen(value) + 1);
    if(valuecopy == NULL)
        goto fail;
    strcpy(valuecopy, value);
    bp = malloc(sizeof(HashBucketRec));
    if(bp == NULL)
        goto fail;
    bp->key = keycopy;
    bp->value = valuecopy;
    bp->prio = prio;
    bp->next = table[i];
    table[i] = bp;
    return 1;

 fail:
    if(keycopy) free(keycopy);
    if(valuecopy) free(valuecopy);
    return -1;
}

int
hashElements(HashTablePtr table)
{
    int i, n;
    HashBucketPtr bp;

    n = 0;
    for(i = 0; i < NUMBUCKETS; i++) {
        for(bp = table[i]; bp; bp = bp->next) {
            n++;
        }
    }
    return n;
}

static int
key_first_cmp(const void *v1, const void *v2)
{
    const HashBucketPtr *b1 = v1, *b2 = v2;
    int c1 = strcmp_lwr((*b1)->key, (*b2)->key);
    if(c1 != 0) return c1;
    return strcmp((*b1)->value, (*b2)->value);
}

static int
value_first_cmp(const void *v1, const void *v2)
{
    const HashBucketPtr *b1 = v1, *b2 = v2;
    int c1 = strcmp((*b1)->value, (*b2)->value);
    if(c1 != 0) return c1;
    return strcmp_lwr((*b1)->key, (*b2)->key);
}

HashBucketPtr *
hashArray(HashTablePtr table, int value_first)
{
    int i, j, n;
    HashBucketPtr *dst;
    
    n = hashElements(table);
    dst = malloc((n + 1) * sizeof(HashBucketPtr));
    if(dst == NULL)
        return NULL;

    j = 0;
    for(i = 0; i < NUMBUCKETS; i++) {
        while(table[i]) {
            dst[j++] = table[i];
            table[i] = table[i]->next;
        }
    }
    qsort(dst, j, sizeof(HashBucketPtr),
          value_first ? value_first_cmp : key_first_cmp);
    dst[j++] = NULL;
    free(table);

    return dst;
}

void
destroyHashArray(HashBucketPtr *array)
{
    int i = 0;
    while(array[i]) {
        free(array[i]->key);
        free(array[i]->value);
        free(array[i]);
        i++;
    }
    free(array);
}
