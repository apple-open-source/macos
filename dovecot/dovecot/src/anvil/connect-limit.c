/* Copyright (c) 2009-2011 Dovecot authors, see the included COPYING file */

#include "common.h"
#include "hash.h"
#include "array.h"						/* APPLE */
#include "str.h"
#include "strescape.h"
#include "ostream.h"
#include "connect-limit.h"

struct ident_pid {
	/* ident string points to ident_hash keys */
	const char *ident;
	pid_t pid;

	/* APPLE replaced refcount with array of connection times for more
	   informative reporting by "doveadm who" */
	ARRAY_DEFINE(times, time_t);
};

struct connect_limit {
	/* ident => refcount */
	struct hash_table *ident_hash;
	/* struct ident_pid => struct ident_pid */
	struct hash_table *ident_pid_hash;
};

static unsigned int ident_pid_hash(const void *p)
{
	const struct ident_pid *i = p;

	return str_hash(i->ident) ^ i->pid;
}

static int ident_pid_cmp(const void *p1, const void *p2)
{
	const struct ident_pid *i1 = p1, *i2 = p2;

	if (i1->pid < i2->pid)
		return -1;
	else if (i1->pid > i2->pid)
		return 1;
	else
		return strcmp(i1->ident, i2->ident);
}

struct connect_limit *connect_limit_init(void)
{
	struct connect_limit *limit;

	limit = i_new(struct connect_limit, 1);
	limit->ident_hash =
		hash_table_create(default_pool, default_pool, 0,
				  str_hash, (hash_cmp_callback_t *)strcmp);
	limit->ident_pid_hash =
		hash_table_create(default_pool, default_pool, 0,
				  ident_pid_hash, ident_pid_cmp);
	return limit;
}

void connect_limit_deinit(struct connect_limit **_limit)
{
	struct connect_limit *limit = *_limit;

	*_limit = NULL;
	hash_table_destroy(&limit->ident_hash);
	hash_table_destroy(&limit->ident_pid_hash);
	i_free(limit);

	/* APPLE this leaks all the ident strings.  if that's ever fixed
	   upstream, then make it also free the "times" array we added */
}

unsigned int connect_limit_lookup(struct connect_limit *limit,
				  const char *ident)
{
	void *value;

	value = hash_table_lookup(limit->ident_hash, ident);
	if (value == NULL)
		return 0;

	return POINTER_CAST_TO(value, unsigned int);
}

void connect_limit_connect(struct connect_limit *limit, pid_t pid,
			   const char *ident)
{
	struct ident_pid *i, lookup_i;
	void *key, *value;

	if (!hash_table_lookup_full(limit->ident_hash, ident, &key, &value)) {
		key = i_strdup(ident);
		value = POINTER_CAST(1);
		hash_table_insert(limit->ident_hash, key, value);
	} else {
		value = POINTER_CAST(POINTER_CAST_TO(value, unsigned int) + 1);
		hash_table_update(limit->ident_hash, key, value);
	}

	lookup_i.ident = ident;
	lookup_i.pid = pid;
	i = hash_table_lookup(limit->ident_pid_hash, &lookup_i);
	if (i == NULL) {
		i = i_new(struct ident_pid, 1);
		i->ident = key;
		i->pid = pid;
		i_array_init(&i->times, 5);			/* APPLE */
		array_append(&i->times, &ioloop_time, 1);	/* APPLE */
		hash_table_insert(limit->ident_pid_hash, i, i);
	} else {
		array_append(&i->times, &ioloop_time, 1);	/* APPLE */
	}
}

static void
connect_limit_ident_hash_unref(struct connect_limit *limit, const char *ident)
{
	void *key, *value;
	unsigned int new_refcount;

	if (!hash_table_lookup_full(limit->ident_hash, ident, &key, &value))
		i_panic("connect limit hash tables are inconsistent");

	new_refcount = POINTER_CAST_TO(value, unsigned int) - 1;
	if (new_refcount > 0) {
		value = POINTER_CAST(new_refcount);
		hash_table_update(limit->ident_hash, key, value);
	} else {
		hash_table_remove(limit->ident_hash, key);
		i_free(key);
	}
}

void connect_limit_disconnect(struct connect_limit *limit, pid_t pid,
			      const char *ident)
{
	struct ident_pid *i, lookup_i;

	lookup_i.ident = ident;
	lookup_i.pid = pid;

	i = hash_table_lookup(limit->ident_pid_hash, &lookup_i);
	if (i == NULL) {
		i_error("connect limit: disconnection for unknown "
			"pid %s + ident %s", dec2str(pid), ident);
		return;
	}

	array_delete(&i->times, 0, 1);				/* APPLE */
	if (array_count(&i->times) == 0) {			/* APPLE */
		hash_table_remove(limit->ident_pid_hash, i);
		array_free(&i->times);				/* APPLE */
		i_free(i);
	}

	connect_limit_ident_hash_unref(limit, ident);
}

void connect_limit_disconnect_pid(struct connect_limit *limit, pid_t pid)
{
	struct hash_iterate_context *iter;
	struct ident_pid *i;
	void *key, *value;
	const time_t *tp;					/* APPLE */

	/* this should happen rarely (or never), so this slow implementation
	   should be fine. */
	iter = hash_table_iterate_init(limit->ident_pid_hash);
	while (hash_table_iterate(iter, &key, &value)) {
		i = key;
		if (i->pid == pid) {
			hash_table_remove(limit->ident_pid_hash, i);
			array_foreach(&i->times, tp) {		/* APPLE */
				connect_limit_ident_hash_unref(limit, i->ident);
			}
			array_free(&i->times);			/* APPLE */
			i_free(i);
		}
	}
	hash_table_iterate_deinit(&iter);
}

void connect_limit_dump(struct connect_limit *limit, struct ostream *output,
			bool dump_elapsed)			/* APPLE */
{
	struct hash_iterate_context *iter;
	void *key, *value;
	string_t *str = t_str_new(256);

	iter = hash_table_iterate_init(limit->ident_pid_hash);
	while (hash_table_iterate(iter, &key, &value)) {
		struct ident_pid *i = key;

		str_truncate(str, 0);
		str_tabescape_write(str, i->ident);
		str_printfa(str, "\t%ld\t%u", (long)i->pid,
			    array_count(&i->times));		/* APPLE */

		/* APPLE */
		if (dump_elapsed) {
			time_t earliest = ioloop_time;
			const time_t *tp;

			array_foreach(&i->times, tp) {
				if (earliest > *tp)
					earliest = *tp;
			}

			str_printfa(str, "\t%lu",
				    (unsigned long) (ioloop_time - earliest));
		}

		str_append_c(str, '\n');
		if (o_stream_send(output, str_data(str), str_len(str)) < 0)
			break;
	}
	hash_table_iterate_deinit(&iter);
	(void)o_stream_send(output, "\n", 1);
}
