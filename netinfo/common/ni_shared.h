/* Shared NetInfo handle */

#ifndef _NI_SHARED_H_
#define _NI_SHARED_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <netinfo/ni.h>
#include <arpa/inet.h>

#define NI_SHARED_ISROOT   0x00000001
#define NI_SHARED_LOCALRAW 0x00000002
#define NI_SHARED_ISROOT_TIMEOUT 300

typedef struct
{
	unsigned long refcount;
	unsigned long flags;
	unsigned long isroot_time;
	void *ni;
	void *parent;
} ni_shared_handle_t;

/*
 * These calls are not thread-safe.
 */
 
ni_shared_handle_t *ni_shared_connection(struct in_addr *addr, char *tag);
ni_shared_handle_t *ni_shared_local(void);
ni_shared_handle_t *ni_shared_retain(ni_shared_handle_t *h);
ni_shared_handle_t *ni_shared_parent(ni_shared_handle_t *h);
ni_shared_handle_t *ni_shared_open(void *x, char *rel);
void ni_shared_release(ni_shared_handle_t *h);

unsigned long get_ni_connect_timeout(void);
void set_ni_connect_timeout(unsigned long t);

unsigned long get_ni_connect_abort(void);
void set_ni_connect_abort(unsigned long a);

void ni_shared_set_flags(unsigned long mask);
void ni_shared_clear_flags(unsigned long mask);

ni_status sa_find(ni_shared_handle_t **dom, ni_id *nid, ni_name dirname, unsigned int timeout);

ni_status sa_addrtag(ni_shared_handle_t *d, struct sockaddr_in *addr, ni_name *tag);

void sa_setabort(ni_shared_handle_t *d, unsigned int a);
void sa_setreadtimeout(ni_shared_handle_t *d, unsigned int t);
void sa_setpassword(ni_shared_handle_t *d, char *pw);

ni_status sa_list(ni_shared_handle_t *d, ni_id *n, ni_name_const pname, ni_entrylist *entries);
ni_status sa_children(ni_shared_handle_t *d, ni_id *n, ni_idlist *children);
ni_status sa_statistics(ni_shared_handle_t *d, ni_proplist *pl);
ni_status sa_read(ni_shared_handle_t *d, ni_id *n, ni_proplist *pl);
ni_status sa_pathsearch(ni_shared_handle_t *d, ni_id *n, char *p);
ni_status sa_lookup(ni_shared_handle_t *d, ni_id *n, ni_name_const pname, ni_name_const pval, ni_idlist *hits);
ni_status sa_self(ni_shared_handle_t *d, ni_id *n);
ni_status sa_root(ni_shared_handle_t *d, ni_id *n);

#endif _NI_SHARED_H_
