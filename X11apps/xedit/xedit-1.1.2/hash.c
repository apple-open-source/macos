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

#include "util.h"
#include <stdlib.h>
#include <string.h>

/*
 *   This is a very simplified and adapted version of the hash tables I am
 * using in a personal project. It was added to try to have a single hash
 * table implementation in xedit. The lisp (for user code) version was not
 * converted, but the following hastables have been converted:
 *  ispell.c - list of replace and ignore words
 *  hook.c   - list of auto replace words
 *  internal lisp data structures:
 *	atoms
 *	strings
 *	packages
 *	opaque types
 *   also, all code traversing hash tables is now using
 *	hash_iter_first() and hash_iter_next()
 *  conversion isn't as good as I originally wanted, code is using hash_check
 *  instead of hash_get, but this is due to the code not having a basic
 *  { void *data; int length; } object to store string like objects
 *
 *   Also, this hash table implementation was added mainly for the tags
 * support.
 */

/*
 * Prototypes
 */
static int hash_equal(hash_table *hash, hash_key *left, hash_key *right);
static unsigned int hash_data(char *value, unsigned int length);
static unsigned int hash_value(hash_key *key);


/*
 * Implementation
 */
static int
hash_equal(hash_table *hash, hash_key *left, hash_key *right)
{
    if (left->length == right->length) {
	if (left == right)
	    return (1);
	if (hash->compare)
	    return (hash->compare(left, right));
	return (memcmp(left->value, right->value, left->length) == 0);
    }

    return (0);
}

static unsigned int
hash_data(char *value, unsigned int length)
{
    char		*ptr;
    unsigned int	i, key;

    for (i = key = 0, ptr = value; i < length; i++)
	key = (key << (key & 1)) ^ ptr[i];

    return (key);
}

static unsigned int
hash_value(hash_key *key)
{
    return (hash_data(key->value, key->length));
}

hash_table *
hash_new(unsigned int length, hash_compare compare)
{
    hash_table	*hash;

    hash = calloc(1, sizeof(hash_table));
    if (hash) {
	hash->entries = calloc(length, sizeof(hash_entry *));
	if (hash->entries) {
	    hash->length = length;
	    hash->compare = compare;
	    hash->iter.offset = -1;
	}
	else {
	    free(hash);
	    hash = (hash_table *)0;
	}
    }

    return (hash);
}

hash_entry *
hash_put(hash_table *hash, hash_entry *entry)
{
    unsigned int	key;
    hash_entry		*prev, *ptr;

    /* Offset in hash table vector for this entry */
    key = hash_value(entry->key) % hash->length;

    /* We hope this is nil for most calls */
    ptr = hash->entries[key];

    /* Check if clashed with another entry */
    for (prev = ptr; ptr; prev = ptr, ptr = ptr->next) {
	/* Replace current entry */
	if (hash_equal(hash, entry->key, ptr->key)) {
	    /* If not trying to readd same value */
	    if (entry != ptr) {
		if (ptr == prev)
		    hash->entries[key] = entry;
		else
		    prev->next = entry;
		entry->next = ptr->next;
		/* Finished */
	    }
	    else
		ptr = (hash_entry *)0;
	    goto hash_put_done;
	}
    }

    /* Add new entry */
    if (prev == (hash_entry *)0)
	/* If no entry in offset */
	hash->entries[key] = entry;
    else
	/* Add to end of clashing list */
	prev->next = entry;
    entry->next = (hash_entry *)0;

    /* Increase sum of entries counter*/
    ++hash->count;

hash_put_done:
    /* ptr will be nil if no entry was replaced, of tried to add
     * again an entry already in the hash table */
    return (ptr);
}

hash_entry *
hash_get(hash_table *hash, hash_key *name)
{
    unsigned int	key;
    hash_entry		*entry;

    key = hash_value(name) % hash->length;
    for (entry = hash->entries[key]; entry; entry = entry->next) {
	if (hash_equal(hash, name, entry->key)) {

	    return (entry);
	}
    }

    return ((hash_entry *)0);
}

hash_entry *
hash_check(hash_table *hash, char *name, unsigned int length)
{
    unsigned int	key;
    hash_entry		*entry;

    key = hash_data(name, length) % hash->length;
    for (entry = hash->entries[key]; entry; entry = entry->next) {
	if (length == entry->key->length &&
	    memcmp(name, entry->key->value, length) == 0) {

	    return (entry);
	}
    }

    return ((hash_entry *)0);
}

hash_entry *
hash_rem_no_free(hash_table *hash, hash_entry *entry)
{
    unsigned int	key;
    hash_entry		*ptr, *prev;

    key = hash_value(entry->key) % hash->length;
    for (ptr = prev = hash->entries[key]; ptr; prev = ptr, ptr = ptr->next) {
	if (ptr == entry) {
	    --hash->count;
	    if (ptr == prev)
		hash->entries[key] = ptr->next;
	    else
		prev->next = ptr->next;
	    break;
	}
    }

    if (ptr && ptr == hash->iter.entry)
	hash->iter.entry = ptr->next;

    /* If entry wasn't in hash table ptr will be nil */
    return (ptr);
}

void
hash_rem(hash_table *hash, hash_entry *entry)
{
    entry = hash_rem_no_free(hash, entry);
    if (entry) {
	free(entry->key->value);
	free(entry->key);
	free(entry);
    }
}

void
hash_rehash(hash_table *hash, unsigned int length)
{
    unsigned int	i, key;
    hash_entry		*entry, *next, **entries;

    entries = (hash_entry **)calloc(length, sizeof(hash_entry *));
    if (entries) {
	/* Populate the new table, note that clashes are now in reverse order */
	for (i = 0; i < hash->length; i++) {
	    for (entry = hash->entries[i]; entry; entry = next) {
		next = entry->next;
		key = hash_value(entry->key) % length;
		entry->next = entries[key];
		entries[key] = entry;
	    }
	}

	/* Finish updating hash table */
	free(hash->entries);
	hash->entries = entries;
	hash->length = length;
    }
    hash->iter.offset = -1;
}

hash_entry *
hash_iter_first(hash_table *hash)
{
    hash->iter.offset = 0;
    hash->iter.entry = (hash_entry *)0;

    return (hash_iter_next(hash));
}

hash_entry *
hash_iter_next(hash_table *hash)
{
    if (hash->iter.offset >= 0) {
	if (hash->iter.entry) {
	    if ((hash->iter.entry = hash->iter.entry->next))
		return (hash->iter.entry);
	    ++hash->iter.offset;
	}
	for (; hash->iter.offset < hash->length; hash->iter.offset++) {
	    if ((hash->iter.entry = hash->entries[hash->iter.offset]))
		return (hash->iter.entry);
	}
	hash->iter.entry = (hash_entry *)0;
	hash->iter.offset = -1;
    }

    return ((hash_entry *)0);
}

void
hash_clr(hash_table *hash)
{
    unsigned int	i;
    hash_entry		*entry, *next;

    /* Extra data should be free'd with the iterator */
    for (i = 0; i < hash->length; i++) {
	entry = hash->entries[i];
	if (entry) {
	    for (next = entry; entry; entry = next) {
		next = entry->next;
		free(entry->key->value);
		free(entry->key);
		free(entry);
	    }
	    hash->entries[i] = (hash_entry *)0;
	}
    }

    hash->count = 0;
    hash->iter.offset = -1;
}

void
hash_del(hash_table *hash)
{
    hash_clr(hash);
    free(hash->entries);
    free(hash);
}
