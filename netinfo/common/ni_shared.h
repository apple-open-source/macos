/* Shared NetInfo handle */

#ifndef _NI_SHARED_H_
#define _NI_SHARED_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NI_SHARED_ISROOT 0x00000001
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

#endif _NI_SHARED_H_
