/* Shared NetInfo handle */

#ifndef _NI_SHARED_H_
#define _NI_SHARED_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct
{
	unsigned int flags;
	unsigned int isroot_time;
	void *ni;
	void *parent;
} ni_shared_handle_t;

/*
 * These calls are not thread-safe.
 */
 
typedef int ni_status;

ni_shared_handle_t *ni_shared_connection(struct in_addr *addr, char *tag);
ni_shared_handle_t *ni_shared_local(void);
ni_shared_handle_t *ni_shared_parent(ni_shared_handle_t *h);
ni_shared_handle_t *ni_shared_open(void *x, char *rel);
void ni_shared_clear(int keep_local);

unsigned int get_ni_connect_timeout(void);
void set_ni_connect_timeout(unsigned int t);

unsigned int get_ni_connect_abort(void);
void set_ni_connect_abort(unsigned int a);

void ni_shared_set_flags(unsigned int mask);
void ni_shared_clear_flags(unsigned int mask);

ni_status sa_addrtag(ni_shared_handle_t *d, struct sockaddr_in *addr, char *tag);

void sa_setabort(ni_shared_handle_t *d, unsigned int a);
void sa_setreadtimeout(ni_shared_handle_t *d, unsigned int t);
void sa_setpassword(ni_shared_handle_t *d, char *pw);

ni_status sa_list(ni_shared_handle_t *d, void *n, char *pname, void *entries);
ni_status sa_children(ni_shared_handle_t *d, void *n, void *children);
ni_status sa_statistics(ni_shared_handle_t *d, void *pl);
ni_status sa_read(ni_shared_handle_t *d, void *n, void *pl);
ni_status sa_pathsearch(ni_shared_handle_t *d, void *n, char *p);
ni_status sa_lookup(ni_shared_handle_t *d, void *n, char *pname, char *pval, void *hits);
ni_status sa_self(ni_shared_handle_t *d, void *n);

#endif /* _NI_SHARED_H_ */
