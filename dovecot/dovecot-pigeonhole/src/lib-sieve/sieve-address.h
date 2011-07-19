/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
#ifndef __SIEVE_ADDRESS_H
#define __SIEVE_ADDRESS_H
 
#include "lib.h"
#include "strfuncs.h"

#include "sieve-common.h"
#include "sieve-stringlist.h"

/*
 * Generic address representation
 */ 
 
struct sieve_address {
	const char *local_part;
	const char *domain;
};

static inline const char *sieve_address_to_string
(const struct sieve_address *address) 
{
	if ( address == NULL || address->local_part == NULL )
		return NULL;

	if ( address->domain == NULL )
		return address->local_part;

	return t_strconcat(address->local_part, "@", address->domain, NULL);
}

/*
 * Address list API
 */

struct sieve_address_list {
	struct sieve_stringlist strlist;

	int (*next_item)
		(struct sieve_address_list *_addrlist, struct sieve_address *addr_r, 
			string_t **unparsed_r);
};

static inline int sieve_address_list_next_item
(struct sieve_address_list *addrlist, struct sieve_address *addr_r, 
	string_t **unparsed_r)
{
	return addrlist->next_item(addrlist, addr_r, unparsed_r);
}

static inline void sieve_address_list_reset
(struct sieve_address_list *addrlist) 
{
	sieve_stringlist_reset(&addrlist->strlist);
}

static inline int sieve_address_list_get_length
(struct sieve_address_list *addrlist)
{
	return sieve_stringlist_get_length(&addrlist->strlist);
}

static inline void sieve_address_list_set_trace
(struct sieve_address_list *addrlist, bool trace)
{
	sieve_stringlist_set_trace(&addrlist->strlist, trace);
}

/*
 * Header address list
 */

struct sieve_address_list *sieve_header_address_list_create
	(const struct sieve_runtime_env *renv, struct sieve_stringlist *field_values);

/* 
 * RFC 2822 addresses
 */ 

bool sieve_rfc2822_mailbox_validate
	(const char *address, const char **error_r);
const char *sieve_rfc2822_mailbox_normalize
	(const char *address, const char **error_r);


const char *sieve_address_normalize
	(string_t *address, const char **error_r);
bool sieve_address_validate
	(string_t *address, const char **error_r);
	
int sieve_address_compare
	(const char *address1, const char *address2, bool normalized);

/*
 * RFC 2821 addresses (paths)
 */

const struct sieve_address *sieve_address_parse_envelope_path
	(pool_t pool, const char *field_value);

#endif
