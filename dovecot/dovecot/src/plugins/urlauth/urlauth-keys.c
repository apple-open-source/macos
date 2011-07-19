/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF 
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

#include "lib.h"
#include "istream.h"
#include "ostream.h"
#include "hash.h"
#include "mail-storage-private.h"
#include "hex-binary.h"
#include "randgen.h"
#include "urlauth-plugin.h"

#include <ctype.h>

#define URLAUTH_KEYS_FILENAME	"urlauthkeys"

enum urlauth_keys_action {
	URLAUTH_KEYS_GET,	// get the key for a mailbox
	URLAUTH_KEYS_SET,	// set a new random key for a mailbox
	URLAUTH_KEYS_DELETE	// delete the key for a mailbox
};

// get the path to the access keys table
static const char *urlauth_keys_path(struct mailbox_list *list)
{
	const char *control_dir =
		mailbox_list_get_path(list, NULL,
				      MAILBOX_LIST_PATH_TYPE_CONTROL);
	return t_strconcat(control_dir, "/"URLAUTH_KEYS_FILENAME, NULL);
	
}

// read the access keys into the hash table
static struct hash_table *urlauth_keys_read(pool_t pool, int fd)
{
	struct hash_table *keys;
	struct istream *is;
	const char *line;

	is = i_stream_create_fd(fd, 4096, FALSE);
	if (is == NULL)
		return NULL;

	keys = hash_table_create(pool, pool, 16, strcase_hash,
				 (hash_cmp_callback_t *) strcasecmp);
	if (keys == NULL) {
		i_stream_destroy(&is);
		return NULL;
	}

	i_stream_set_return_partial_line(is, TRUE);
	while ((line = i_stream_read_next_line(is)) != NULL) {
		const char *sep;

		if (*line == '\0' || *line == '#')
			continue;

		/* future-proofing */
		if (strcmp(line, URLAUTH_KEYS_FILENAME" version 1") == 0)
			continue;

		/* key:mailbox, both nonempty */
		sep = strchr(line, ':');
		if (sep == NULL || sep == line || sep[1] == '\0')
			continue;

		/* keys[mailbox] = key */
		hash_table_insert(keys,
				  p_strdup(pool, sep + 1),
				  p_strdup_until(pool, line, sep));
	}
	i_stream_destroy(&is);

	return keys;
}

// write the access keys to the file
static bool urlauth_keys_write(int fd, struct hash_table *keys)
{
	struct ostream *os;
	struct hash_iterate_context *iter;
	void *key, *value;

	os = o_stream_create_fd_file(fd, 0, FALSE);
	if (os == NULL)
		return NULL;

	o_stream_send_str(os, URLAUTH_KEYS_FILENAME" version 1\n");
	iter = hash_table_iterate_init(keys);
	while (hash_table_iterate(iter, &key, &value)) {
		/* key is the mailbox, value is the access key */
		o_stream_send_str(os, (const char *) value);
		o_stream_send(os, ":", 1);
		o_stream_send_str(os, (const char *) key);
		o_stream_send(os, "\n", 1);
	}
	hash_table_iterate_deinit(&iter);
	o_stream_destroy(&os);

	return TRUE;
}

// read/modify/write the access keys
static bool urlauth_keys_update(struct mailbox *box,
				enum urlauth_keys_action action,
				buffer_t *out_key)
{
	const char *path;
	int flags, fd;
	pool_t pool;
	struct hash_table *keys;
	unsigned char new_key[URLAUTH_KEY_BYTES];
	const char *hex = NULL;
	bool modified = FALSE;

	/* open and lock the keys file */
	path = urlauth_keys_path(box->list);
	flags = O_RDWR | O_EXLOCK;
	if (action == URLAUTH_KEYS_GET || action == URLAUTH_KEYS_SET)
		flags |= O_CREAT;
	fd = open(path, flags, 0644);
	if (fd < 0) {
		if ((flags & O_CREAT) || errno != ENOENT)
			i_error("open(%s) failed: %m", path);
		return FALSE;
	}

	/* read the keys */
	pool = pool_alloconly_create("urlauth_keys", 4096);
	keys = urlauth_keys_read(pool, fd);
	if (keys == NULL) {
		i_error("error reading %s: %m", path);
		pool_unref(&pool);
		close(fd);
		return FALSE;
	}

	/* modify the keys */
	switch (action) {
	case URLAUTH_KEYS_GET:
		hex = hash_table_lookup(keys, (void *) box->name);

		/* if hex key is bogus, set a new one */
		if (hex) {
			const char *cp;
			for (cp = hex; *cp; cp++)
				if (!i_isxdigit(*cp))
					break;
			if (*cp == '\0')
				break;	/* good hex */
		}

		/* if not present, set one -- fall through */
	case URLAUTH_KEYS_SET:
		random_fill(new_key, sizeof new_key);
		hex = binary_to_hex(new_key, sizeof new_key);
		if (*box->name) {
			hash_table_insert(keys, (void *) box->name,
					  (void *) hex);
			modified = TRUE;
		} /* else box->name="" for foiling timing attacks */
		break;
	case URLAUTH_KEYS_DELETE:
		hex = hash_table_lookup(keys, (void *) box->name);
		if (hex) {
			hash_table_remove(keys, (void *) box->name);
			modified = TRUE;
		}
		break;
	}

	if (out_key)
		hex_to_binary(hex, out_key);

	/* write the keys */
	if (modified) {
		lseek(fd, 0, SEEK_SET);
		ftruncate(fd, 0);
		urlauth_keys_write(fd, keys);
	}

	/* clean up */
	close(fd);
	hash_table_destroy(&keys);
	pool_unref(&pool);

	return TRUE;
}

void urlauth_keys_init(void)
{
	random_init();
}

void urlauth_keys_deinit(void)
{
	random_deinit();
}

// get the access key for a mailbox
bool urlauth_keys_get(struct mailbox *box, buffer_t *key)
{
	return urlauth_keys_update(box, URLAUTH_KEYS_GET, key);
}

// set a new random access key for a mailbox
bool urlauth_keys_set(struct mailbox *box)
{
	return urlauth_keys_update(box, URLAUTH_KEYS_SET, NULL);
}

// delete the access key for a mailbox
bool urlauth_keys_delete(struct mailbox *box)
{
	return urlauth_keys_update(box, URLAUTH_KEYS_DELETE, NULL);
}

// delete all access keys
bool urlauth_keys_reset(struct mailbox_list *list)
{
	const char *path;

	path = urlauth_keys_path(list);
	if (unlink(path) < 0 && errno != ENOENT && errno != ESTALE) {
		i_error("unlink(%s) failed: %m", path);
		return FALSE;
	}
	return TRUE;
}
