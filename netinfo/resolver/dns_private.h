#ifndef __DNS_PRIVATE_H__
#define __DNS_PRIVATE_H__

#include <sys/cdefs.h>

#define DNS_FLAG_DEBUG 0x00000001

typedef struct
{
	res_state res;
	char *source;
	char *name;
	uint32_t search_count;
	uint32_t modtime;
	uint32_t stattime;
	uint32_t reserved1;
	void *reserved_pointer1;
} pdns_handle_t;

typedef struct 
{
	pdns_handle_t *dns_default;
	uint32_t client_count;
	pdns_handle_t **client;
	uint32_t modtime;
	uint32_t stattime;
	uint32_t stat_latency;
	uint32_t flags;
	uint32_t reserved1;
	void *reserved_pointer1;
} sdns_handle_t;

typedef struct __dns_handle_private_struct
{
	uint32_t handle_type;
	sdns_handle_t *sdns;
	pdns_handle_t *pdns;
	char *recvbuf;
	uint32_t recvsize;
	uint32_t reserved1;
	uint32_t reserved2;
	void *reserved_pointer1;
	void *reserved_pointer2;
} dns_private_handle_t;


__BEGIN_DECLS

/*
 * Get a list of DNS client handles which may be used to query for the input
 * name.  The returned array is terminated by a NULL.  The input dns_handle_t
 * must be a "Super" DNS handle.  Caller should free the returned handles in
 * the array using dns_free(), and free the returned array. 
 */
extern dns_handle_t* dns_clients_for_name(dns_handle_t d, const char *name);

/*
 * Returns the number of nameserver addresses available to the input
 * DNS client.  Returns zero if the input handle is a "Super" DNS handle.
 */
extern uint32_t dns_server_list_count(dns_handle_t d);

/*
 * Returns the nameserver address at the given index.  Returns NULL
 * if the index is out of range.  Caller should free the returned sockaddr.
 */
extern struct sockaddr *dns_server_list_address(dns_handle_t d, uint32_t i);

/*
 * Returns a list of all server addresses for all clients.
 * Caller must free each list entry, and the returned list.
 */
extern void dns_all_server_addrs(dns_handle_t d, struct sockaddr ***addrs, uint32_t *count);

/*
 * Returns the number of names in the search list.
 */
uint32_t dns_search_list_count(dns_handle_t d);

__END_DECLS

#endif __DNS_PRIVATE_H__
