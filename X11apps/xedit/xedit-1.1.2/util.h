/*
 * Copyright (c) 2007,2008 Paulo Cesar Pereira de Andrade
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Paulo Cesar Pereira de Andrade
 */

/* Generic hash table */

#ifndef _util_h
#define _util_h

/*
 * Types
 */
typedef struct _hash_key	hash_key;
typedef struct _hash_entry	hash_entry;
typedef struct _hash_table	hash_table;
typedef int (*hash_compare)(hash_key *left, hash_key *right);

struct _hash_key {
    char		*value;
    unsigned int	length;
};

struct _hash_entry {
    hash_key		*key;
    hash_entry		*next;
};

struct _hash_table {
    hash_entry		**entries;
    unsigned int	count;		/* length of entries */
    unsigned int	length;		/* sum of entries */
    hash_compare	compare;

    struct {
	int		offset;
	hash_entry	*entry;
    } iter;
};

/*
 * Prototypes
 */
hash_table *hash_new(unsigned int length, hash_compare compare);
hash_entry *hash_put(hash_table *hash, hash_entry *entry);
hash_entry *hash_get(hash_table *hash, hash_key *name);
hash_entry * hash_check(hash_table *hash, char *name, unsigned int length);
void hash_rem(hash_table *hash, hash_entry *entry);
/* Removes from hash table but doesn't release any memory */
hash_entry *hash_rem_no_free(hash_table *hash, hash_entry *entry);
void hash_rehash(hash_table *hash, unsigned int length);
hash_entry *hash_iter_first(hash_table *hash);
hash_entry *hash_iter_next(hash_table *hash);

/* Frees all data. When casting to another type, use the
 * iterator to free extra data */
void hash_clr(hash_table *hash);
void hash_del(hash_table *hash);

#endif /* _util_h */
