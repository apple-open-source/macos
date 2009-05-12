/*
 * String hash table.
 * Copyright (c) 1995-1999 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <afmint.h>
#include <strhash.h>

/*
 * Types and definitions.
 */

#define STRHASH_DEBUG 0

#define HASH_SIZE 8192

struct hash_list_st
{
  struct hash_list_st *next;
  char *key;			/* malloc()ated copy of key. */
  int keylen;
  void *data;
};

typedef struct hash_list_st HashList;

typedef HashList *HashTable;

typedef struct stringhash_st
{
  HashTable *hash_table;

  /* Scan support. */
  unsigned int next_idx;
  HashList *next_item;

#if STRHASH_DEBUG
  int items_in_hash;
#endif /* STRHASH_DEBUG */
} *hash_t;


/*
 * Prototypes for static functions.
 */

static int count_hash ___P ((const char *key, int keylen));


/*
 * Global functions.
 */

StringHashPtr
strhash_init ()
{
  StringHashPtr tmp;

  tmp = (StringHashPtr) calloc (1, sizeof (*tmp));
  if (!tmp)
    return NULL;

  tmp->hash_table = (HashTable *) calloc (HASH_SIZE, sizeof (HashTable));
  if (!tmp->hash_table)
    {
      free (tmp);
      return NULL;
    }

#if STRHASH_DEBUG
  tmp->items_in_hash = 0;
#endif /* STRHASH_DEBUG */
  return tmp;
}


void
strhash_free (StringHashPtr hash)
{
  HashList *list, *list_next;
  int i;

  if (!hash)
    return;

  /* Free chains. */
  for (i = 0; i < HASH_SIZE; i++)
    for (list = hash->hash_table[i]; list; list = list_next)
      {
	list_next = list->next;
	free (list->key);
	free (list);
      }

  /* Free hash. */
  free (hash->hash_table);
  free (hash);
}


int
strhash_put (StringHashPtr hash, char *key, int keylen, void *data,
	     void **old_data)
{
  HashList *list, *prev = NULL;
  int pos, cmp_val;

  if (!hash || !key || keylen <= 0)
    return 0;

  if (old_data)
    *old_data = NULL;
  pos = count_hash (key, keylen);

  /* Is it already here? */
  for (list = hash->hash_table[pos]; list; prev = list, list = list->next)
    if (list->keylen == keylen)
      {
	cmp_val = memcmp (key, list->key, keylen);
	if (cmp_val == 0)
	  {
	    /* We had an old occurence. */
	    if (old_data)
	      *old_data = list->data;
	    list->data = data;
	    return 1;
	  }
	else if (cmp_val < 0)
	  {
	    /* Run over. Correct position is prev->next. */
	    break;
	  }
      }
    else if (list->keylen > keylen)
      /* Lists are kept sorted so that smallest keys are at the head and
	 keys with equal length are in normal sorted order. */
      break;

  /* No old data. */
  list = (HashList *) calloc (1, sizeof (HashList));
  if (!list)
    return 0;
  list->key = (char *) malloc (keylen);
  if (!list->key)
    {
      free (list);
      return 0;
    }

  memcpy (list->key, key, keylen);
  list->keylen = keylen;
  list->data = data;

  /* Insert list to the correct position. */
  if (!prev)
    {
      list->next = hash->hash_table[pos];
      hash->hash_table[pos] = list;
    }
  else
    {
      list->next = prev->next;
      prev->next = list;
    }
#if STRHASH_DEBUG
  hash->items_in_hash++;
#endif /* STRHASH_DEBUG */
  return 1;
}


int
strhash_get (StringHashPtr hash, const char *key, int keylen, void **data)
{
  HashList *list;
  int pos, cmp_val;

  if (!hash || !key || keylen <= 0 || !data)
    return 0;

  *data = NULL;
  pos = count_hash (key, keylen);
  for (list = hash->hash_table[pos]; list; list = list->next)
    if (list->keylen == keylen)
      {
	cmp_val = memcmp (key, list->key, keylen);
	if (cmp_val == 0)
	  {
	    *data = list->data;
	    return 1;
	  }
	else if (cmp_val < 0)
	  /* Run over. */
	  break;
      }
    else if (list->keylen > keylen)
      /* Run over. */
      break;

  return 0;
}


int
strhash_delete (StringHashPtr hash, const char *key, int keylen, void **data)
{
  HashList *list, *prev = NULL;
  int pos, cmp_val;

  if (!hash || !key || keylen <= 0 || !data)
    return 0;

  *data = NULL;
  pos = count_hash (key, keylen);
  for (list = hash->hash_table[pos]; list; prev = list, list = list->next)
    if (list->keylen == keylen)
      {
	cmp_val = memcmp (key, list->key, keylen);
	if (cmp_val == 0)
	  {
	    /* Value found. */
	    if (prev == NULL)
	      hash->hash_table[pos] = list->next;
	    else
	      prev->next = list->next;

	    *data = list->data;
	    free (list->key);
	    free (list);

	    /* Init scan. */
	    hash->next_idx = 0;
	    hash->next_item = NULL;

#if STRHASH_DEBUG
	    hash->items_in_hash--;
#endif /* STRHASH_DEBUG */
	    return 1;
	  }
	else if (cmp_val < 0)
	  /* Not found. */
	  break;
      }
    else if (list->keylen > keylen)
      /* Run over. */
      break;

  return 0;
}


int
strhash_get_first (StringHashPtr hash, char **key_return,
		   int *keylen_return, void **data_return)
{
  if (!hash || !key_return || !keylen_return || !data_return)
    return 0;

  for (hash->next_idx = 0; hash->next_idx < HASH_SIZE; hash->next_idx++)
    {
      hash->next_item = hash->hash_table[hash->next_idx];
      if (hash->next_item)
	{
	  *key_return = hash->next_item->key;
	  *keylen_return = hash->next_item->keylen;
	  *data_return = hash->next_item->data;
	  return 1;
	}
    }
  return 0;
}


int
strhash_get_next (StringHashPtr hash, char **key_return,
		  int *keylen_return, void **data_return)
{
  if (!hash || !key_return || !keylen_return || !data_return)
    return 0;

  for (; hash->next_idx < HASH_SIZE; hash->next_idx++)
    {
      if (hash->next_item == NULL)
	hash->next_item = hash->hash_table[hash->next_idx];
      else
	hash->next_item = hash->next_item->next;

      if (hash->next_item)
	{
	  *key_return = hash->next_item->key;
	  *keylen_return = hash->next_item->keylen;
	  *data_return = hash->next_item->data;
	  return 1;
	}
    }
  return 0;
}


#if STRHASH_DEBUG
void
strhash_debug (StringHashPtr hash)
{
  int i, count = 0, max = 0;
  HashList *tmp;

  if (!hash)
    {
      fprintf (stderr, "Invalid hash handle!\n");
      return;
    }
  fprintf (stderr, "hash_size\t%d\n", HASH_SIZE);
  fprintf (stderr, "items_in_hash\t%d\n", hash->items_in_hash);

  for (i = 0; i < HASH_SIZE; i++)
    if (hash->hash_table[i] == NULL)
      count++;
  fprintf (stderr, "empty entries\t%d\n", count);

  count = 0;
  for (i = 0; i < HASH_SIZE; i++)
    {
      for (tmp = hash->hash_table[i]; tmp; tmp = tmp->next)
	count++;
      max = count > max ? count : max;
      count = 0;
    }
  fprintf (stderr, "longest list\t%d\n", max);

  if (max > 0)
    {
      /* Print the first longest list. */
      for (i = 0; i < HASH_SIZE; i++)
	{
	  count = 0;
	  for (tmp = hash->hash_table[i]; tmp; tmp = tmp->next)
	    count++;
	  if (count == max)
	    {
	      for (count = 0, tmp = hash->hash_table[i]; tmp;
		   tmp = tmp->next, count++)
		{
		  fprintf (stderr, "%d\t", count);
		  for (i = 0; i < tmp->keylen; i++)
		    fprintf (stderr, "%c", tmp->key[i]);
		}
	      break;
	    }
	}
    }
}
#endif /* STRHASH_DEBUG */


/*
 * Static functions.
 */

static int
count_hash (const char *key, int keylen)
{
  unsigned int val = 0;
  int i;

  for (i = 0; i < keylen; i++)
    val = (val << 5) ^ (unsigned char) key[i]
      ^ (val >> 16) ^ (val >> 7);
  return val % HASH_SIZE;
}
