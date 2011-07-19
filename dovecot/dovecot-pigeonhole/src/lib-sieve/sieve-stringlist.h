/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_STRINGLIST_H
#define __SIEVE_STRINGLIST_H

/*
 * Stringlist API
 */

struct sieve_stringlist {
	int (*next_item)
		(struct sieve_stringlist *strlist, string_t **str_r);
	void (*reset)
		(struct sieve_stringlist *strlist);
	int (*get_length)
		(struct sieve_stringlist *strlist);

	int (*read_all)
		(struct sieve_stringlist *strlist, pool_t pool,
			const char * const **list_r);

	void (*set_trace)
		(struct sieve_stringlist *strlist, bool trace);

	const struct sieve_runtime_env *runenv;
	int exec_status;

	unsigned int trace:1;
};

static inline void sieve_stringlist_set_trace
(struct sieve_stringlist *strlist, bool trace)
{
	strlist->trace = trace;

	if ( strlist->set_trace != NULL )
		strlist->set_trace(strlist, trace);
}

static inline int sieve_stringlist_next_item
(struct sieve_stringlist *strlist, string_t **str_r) 
{
	return strlist->next_item(strlist, str_r);
}

static inline void sieve_stringlist_reset
(struct sieve_stringlist *strlist) 
{
	strlist->reset(strlist);
}

int sieve_stringlist_get_length
	(struct sieve_stringlist *strlist);

int sieve_stringlist_read_all
	(struct sieve_stringlist *strlist, pool_t pool,
		const char * const **list_r);

/*
 * Single Stringlist
 */

struct sieve_stringlist *sieve_single_stringlist_create
	(const struct sieve_runtime_env *renv, string_t *str, bool count_empty);
struct sieve_stringlist *sieve_single_stringlist_create_cstr
(const struct sieve_runtime_env *renv, const char *cstr, bool count_empty);

#endif /* __SIEVE_STRINGLIST_H */
