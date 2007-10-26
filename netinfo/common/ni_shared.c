#include "ni_shared.h"
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <notify.h>
#include <unistd.h>
#include <sys/socket.h>
#include <NetInfo/dsstore.h>

#define NI_FAILED 9999

unsigned int
get_ni_connect_timeout(void)
{
	return 0;
}

void
set_ni_connect_timeout(unsigned int t)
{
}

ni_shared_handle_t *
ni_raw_local(void)
{
	return NULL;
}

void
ni_release_raw_local(void)
{
}
	
ni_shared_handle_t *
ni_shared_local(void)
{
	return NULL;
}

ni_shared_handle_t *
ni_shared_connection(struct in_addr *addr, char *tag)
{
	return NULL;
}

void
ni_shared_clear(int keep_local)
{
}

ni_shared_handle_t *
ni_shared_parent(ni_shared_handle_t *h)
{
	return NULL;
}

ni_shared_handle_t *
ni_shared_open(void *x, char *rel)
{
	return NULL;
}

ni_status
sa_self(ni_shared_handle_t *d, void *n)
{
	return NI_FAILED;
}

ni_status
sa_lookup(ni_shared_handle_t *d, void *n, char *pname, char *pval, void *hits)
{
	return NI_FAILED;
}

ni_status
sa_root(ni_shared_handle_t *d, void *n)
{
	return NI_FAILED;
}

ni_status 
sa_pathsearch(ni_shared_handle_t *d, void *n, char *p)
{
	return NI_FAILED;
}

ni_status
sa_read(ni_shared_handle_t *d, void *n, void *pl)
{
	return NI_FAILED;
}

void
sa_setpassword(ni_shared_handle_t *d, char *pw)
{
}

ni_status
sa_statistics(ni_shared_handle_t *d, void *pl)
{
	return NI_FAILED;
}

ni_status
sa_children(ni_shared_handle_t *d, void *n, void *children)
{
	return NI_FAILED;
}

ni_status
sa_list(ni_shared_handle_t *d, void *n, char *pname, void *entries)
{
	return NI_FAILED;
}

ni_status
sa_addrtag(ni_shared_handle_t *d, struct sockaddr_in *addr, char *tag)
{
	return NI_FAILED;
}

void
sa_setabort(ni_shared_handle_t *d, unsigned int a)
{
}

void
sa_setreadtimeout(ni_shared_handle_t *d, unsigned int t)
{
}
