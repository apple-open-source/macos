/*
 * String hash table.
 * Copyright (c) 1995, 1996, 1997 Markku Rossi.
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

#ifndef STRHASH_H
#define STRHASH_H

#ifndef ___P
#if PROTOTYPES
#define ___P(protos) protos
#else /* no PROTOTYPES */
#define ___P(protos) ()
#endif /* no PROTOTYPES */
#endif

typedef struct stringhash_st *StringHashPtr;

/*
 * Init a hash and return a hash handle or NULL if there were errors.
 */
StringHashPtr strhash_init ___P ((void));

/*
 * Free hash <hash>. Frees all resources that hash has allocated. <hash>
 * shouldn't be used after this function is called.
 */
void strhash_free ___P ((StringHashPtr hash));

/*
 * Put key <key> to hash <hash>. <data> will be bind to <key>. Returns
 * true (1) if operation was successful or false (0) otherwise. If <key>
 * is already bind to another data, then <old_data> will be set to old
 * data. Otherwise it will be set to NULL.
 */
int strhash_put ___P ((StringHashPtr hash, char *key, int keylen, void *data,
		       void **old_data_return));

/*
 * Get data associated to key <key>. Data is returned in <*data>.
 * Returns true (1) is key was found or false (0) otherwise.
 */
int strhash_get ___P ((StringHashPtr hash, const char *key, int keylen,
		       void **data_return));

/*
 * Deletes key <key> form <hash>. Data is returned in <*data>. Returns
 * true (1) if <key> was found or false (0) if <key> was not found or
 * errors were encountered.
 */
int strhash_delete ___P ((StringHashPtr hash, const char *key, int keylen,
			  void **data_return));

/*
 * Get first item from hash <hash>.  Returns 1 if there were items
 * or 0 otherwise.
 */
int strhash_get_first ___P ((StringHashPtr hash, char **key_return,
			     int *keylen_return, void **data_return));

/*
 * Get next item from hash <hash>.  Returns 1 if there were items
 * or 0 otherwise.
 */
int strhash_get_next ___P ((StringHashPtr hash, char **key_return,
			    int *keylen_return, void **data_return));

#endif /* not STRHASH_H */
