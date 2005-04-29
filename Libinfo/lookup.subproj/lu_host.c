/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <netinet/if_ether.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_host.h"
#include "lu_utils.h"

#define HOST_CACHE_SIZE 10
#define DEFAULT_HOST_CACHE_TTL 10

#define CACHE_BYNAME 0
#define CACHE_BYADDR 1

static pthread_mutex_t _host_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int _host_cache_ttl = DEFAULT_HOST_CACHE_TTL;

static void *_host_byname_cache[HOST_CACHE_SIZE] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int _host_byname_cache_flavor[HOST_CACHE_SIZE] = { WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING };
static unsigned int _host_byname_cache_best_before[HOST_CACHE_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned int _host_byname_cache_index = 0;

static void *_host_byaddr_cache[HOST_CACHE_SIZE] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static int _host_byaddr_cache_flavor[HOST_CACHE_SIZE] = { WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING, WANT_NOTHING };
static unsigned int _host_byaddr_cache_best_before[HOST_CACHE_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned int _host_byaddr_cache_index = 0;

static pthread_mutex_t _host_lock = PTHREAD_MUTEX_INITIALIZER;

extern struct hostent *_old_gethostbyaddr();
extern struct hostent *_old_gethostbyname();
extern struct hostent *_old_gethostent();
extern void _old_sethostent();
extern void _old_endhostent();
extern void _old_sethostfile();

extern int _old_ether_hostton(const char *, struct ether_addr *);
extern int _old_ether_ntohost(char *, const struct ether_addr *);

extern mach_port_t _lu_port;
extern int _lu_running(void);

extern int h_errno;

#define IPV6_ADDR_LEN 16
#define IPV4_ADDR_LEN 4

__private_extern__ void
free_host_data(struct hostent *h)
{
	char **aliases;
	int i;

	if (h == NULL) return;

	if (h->h_name != NULL) free(h->h_name);

	aliases = h->h_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(h->h_aliases);
	}

	if (h->h_addr_list != NULL)
	{
		for (i = 0; h->h_addr_list[i] != NULL; i++) free(h->h_addr_list[i]);
		free(h->h_addr_list);
	}
}

void
freehostent(struct hostent *h)
{
	if (h == NULL) return;
	free_host_data(h);
	free(h);
}

static void
free_lu_thread_info_host(void *x)
{
	struct lu_thread_info *tdata;

	if (x == NULL) return;

	tdata = (struct lu_thread_info *)x;
	
	if (tdata->lu_entry != NULL)
	{
		freehostent((struct hostent *)tdata->lu_entry);
		tdata->lu_entry = NULL;
	}

	_lu_data_free_vm_xdr(tdata);

	free(tdata);
}

__private_extern__ struct hostent *
extract_host(XDR *xdr, int want, int *err)
{
	struct hostent *h;
	int i, j, nvals, nkeys, status, addr_len;
	int family, addr_count, map_count;
	struct in_addr addr;
	struct in6_addr addr6;
	char *key, **vals, **mapvals;

	mapvals = NULL;
	map_count = 0;
	addr_count = 0;
	addr_len = sizeof(u_long *);

	if (xdr == NULL)
	{
		*err = NO_RECOVERY;
		return NULL;
	}

	if (!xdr_int(xdr, &nkeys))
	{
		*err = NO_RECOVERY;
		return NULL;
	}

	h = (struct hostent *)calloc(1, sizeof(struct hostent));

	family = AF_INET;
	h->h_length = IPV4_ADDR_LEN;

	if (want > WANT_A4_ONLY)
	{
		family = AF_INET6;
		h->h_length = IPV6_ADDR_LEN;
	}

	h->h_addrtype = family;	

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		vals = NULL;
		nvals = 0;

		status = _lu_xdr_attribute(xdr, &key, &vals, &nvals);
		if (status < 0)
		{
			freehostent(h);
			*err = NO_RECOVERY;
			return NULL;
		}

		if (nvals == 0)
		{
			free(key);
			continue;
		}

		j = 0;

		if ((h->h_name == NULL) && (!strcmp("name", key)))
		{
			h->h_name = vals[0];
			if (nvals > 1)
			{
				h->h_aliases = (char **)calloc(nvals, sizeof(char *));
				for (j = 1; j < nvals; j++) h->h_aliases[j-1] = vals[j];
			}
			j = nvals;
		}
		else if ((family == AF_INET) && (h->h_addr_list == NULL) && (!strcmp("ip_address", key)))
		{
			addr_count = nvals;
			h->h_addr_list = (char **)calloc(nvals + 1, addr_len);

			for (j = 0; j < nvals; j++)
			{
				addr.s_addr = 0;
				inet_aton(vals[j], &addr);
				h->h_addr_list[j] = (char *)calloc(1, IPV4_ADDR_LEN);
				memmove(h->h_addr_list[j], &(addr.s_addr), IPV4_ADDR_LEN);
			}

			h->h_addr_list[nvals] = NULL;
			j = 0;
		}
		else if ((family == AF_INET6) && (h->h_addr_list == NULL) && (!strcmp("ipv6_address", key)))
		{
			addr_count = nvals;
			h->h_addr_list = (char **)calloc(nvals + 1, addr_len);

			for (j = 0; j < nvals; j++)
			{
				memset(&addr6, 0, sizeof(struct in6_addr));
				inet_pton(family, vals[j], &addr6);
				h->h_addr_list[j] = (char *)calloc(1, IPV6_ADDR_LEN);
				memmove(h->h_addr_list[j], &(addr6.__u6_addr.__u6_addr32[0]), IPV6_ADDR_LEN);
			}

			h->h_addr_list[nvals] = NULL;
			j = 0;
		}
		else if ((family == AF_INET6) && (mapvals == NULL) && (!strcmp("ip_address", key)))
		{
			map_count = nvals;
			mapvals = vals;
			vals = NULL;
		}

		free(key);
		if (vals != NULL)
		{
			for (; j < nvals; j++) free(vals[j]);
			free(vals);
		}
	}

	if ((mapvals != NULL) && (want > WANT_A6_ONLY))
	{
		addr6.__u6_addr.__u6_addr32[0] = 0x00000000;
		addr6.__u6_addr.__u6_addr32[1] = 0x00000000;
		addr6.__u6_addr.__u6_addr32[2] = htonl(0x0000ffff);

		if (addr_count == 0)
		{
			h->h_addr_list = (char **)calloc(map_count + 1, addr_len);
		}
		else
		{
			h->h_addr_list = (char **)realloc(h->h_addr_list, (addr_count + map_count + 1) * addr_len);
		}

		for (i = 0; i < map_count; i++)
		{
			addr.s_addr = 0;
			inet_aton(mapvals[i], &addr);
			h->h_addr_list[addr_count] = (char *)calloc(1, IPV6_ADDR_LEN);
			memmove(&(addr6.__u6_addr.__u6_addr32[3]), &(addr.s_addr), IPV4_ADDR_LEN);
			memcpy(h->h_addr_list[addr_count++], &(addr6.__u6_addr.__u6_addr32[0]), IPV6_ADDR_LEN);
		}

		h->h_addr_list[addr_count] = NULL;
	}

	if (mapvals != NULL)
	{
		for (i = 0; i < map_count; i++) free(mapvals[i]);
		free(mapvals);
	}

	if (h->h_addr_list == NULL) 
	{
		freehostent(h);
		*err = NO_DATA;
		return NULL;
	}

	if (h->h_name == NULL) h->h_name = strdup("");
	if (h->h_aliases == NULL) h->h_aliases = (char **)calloc(1, sizeof(char *));

	return h;
}

static struct hostent *
copy_host(struct hostent *in)
{
	int i, len, addr_len;
	struct hostent *h;

	if (in == NULL) return NULL;

	h = (struct hostent *)calloc(1, sizeof(struct hostent));

	h->h_name = LU_COPY_STRING(in->h_name);

	len = 0;
	if (in->h_aliases != NULL)
	{
		for (len = 0; in->h_aliases[len] != NULL; len++);
	}

	h->h_aliases = (char **)calloc(len + 1, sizeof(char *));
	for (i = 0; i < len; i++)
	{
		h->h_aliases[i] = strdup(in->h_aliases[i]);
	}

	h->h_addrtype = in->h_addrtype;
	h->h_length = in->h_length;

	len = 0;
	if (in->h_addr_list != NULL)
	{
		for (len = 0; in->h_addr_list[len] != NULL; len++);
	}

	addr_len = sizeof(u_long *);
	h->h_addr_list = (char **)calloc(len + 1, addr_len);
	for (i = 0; i < len; i++)
	{
		h->h_addr_list[i] = (char *)malloc(h->h_length);
		memmove(h->h_addr_list[i], in->h_addr_list[i], h->h_length);
	}

	return h;
}

static void
recycle_host(struct lu_thread_info *tdata, struct hostent *in)
{
	struct hostent *h;

	if (tdata == NULL) return;
	h = (struct hostent *)tdata->lu_entry;

	if (in == NULL)
	{
		freehostent(h);
		tdata->lu_entry = NULL;
	}

	if (tdata->lu_entry == NULL)
	{
		tdata->lu_entry = in;
		return;
	}

	free_host_data(h);

	h->h_name = in->h_name;
	h->h_aliases = in->h_aliases;
	h->h_addrtype = in->h_addrtype;
	h->h_length = in->h_length;
	h->h_addr_list = in->h_addr_list;

	free(in);
}

__private_extern__ struct hostent *
fake_hostent(const char *name, struct in_addr addr)
{
	int addr_len;
	struct hostent *h;

	if (name == NULL) return NULL;

	h = (struct hostent *)calloc(1, sizeof(struct hostent));

	h->h_name = strdup(name);

	h->h_aliases = (char **)calloc(1, sizeof(char *));

	h->h_addrtype = AF_INET;
	h->h_length = sizeof(long);

	addr_len = sizeof(u_long *);
	h->h_addr_list = (char **)calloc(2, addr_len);

	h->h_addr_list[0] = (char *)malloc(h->h_length);
	memmove(h->h_addr_list[0], &(addr.s_addr), h->h_length);

	return h;
}

__private_extern__ struct hostent *
fake_hostent6(const char *name, struct in6_addr addr)
{
	int addr_len;
	struct hostent *h;

	if (name == NULL) return NULL;

	h = (struct hostent *)calloc(1, sizeof(struct hostent));

	h->h_name = strdup(name);

	h->h_aliases = (char **)calloc(1, sizeof(char *));

	h->h_addrtype = AF_INET6;
	h->h_length = 16;

	addr_len = sizeof(u_long *);
	h->h_addr_list = (char **)calloc(2, addr_len);

	h->h_addr_list[0] = (char *)malloc(h->h_length);
	memmove(h->h_addr_list[0], &(addr.__u6_addr.__u6_addr32[0]), h->h_length);

	return h;
}

__private_extern__ unsigned int
get_host_cache_ttl()
{
	return _host_cache_ttl;
}

__private_extern__ void
set_host_cache_ttl(unsigned int ttl)
{
	int i;

	pthread_mutex_lock(&_host_cache_lock);

	_host_cache_ttl = ttl;

	if (ttl == 0)
	{
		for (i = 0; i < HOST_CACHE_SIZE; i++)
		{
			if (_host_byname_cache[i] == NULL) continue;

			freehostent((struct hostent *)_host_byname_cache[i]);
			_host_byname_cache[i] = NULL;
			_host_byname_cache_flavor[i] = WANT_NOTHING;
			_host_byname_cache_best_before[i] = 0;
		}

		for (i = 0; i < HOST_CACHE_SIZE; i++)
		{
			if (_host_byaddr_cache[i] == NULL) continue;

			freehostent((struct hostent *)_host_byaddr_cache[i]);
			_host_byaddr_cache[i] = NULL;
			_host_byaddr_cache_flavor[i] = WANT_NOTHING;
			_host_byaddr_cache_best_before[i] = 0;
		}
	}

	pthread_mutex_unlock(&_host_cache_lock);
}

static void
cache_host(struct hostent *h, int want, int how)
{
	struct timeval now;
	struct hostent *hcache;

	if (_host_cache_ttl == 0) return;
	if (h == NULL) return;

	pthread_mutex_lock(&_host_cache_lock);

	hcache = copy_host(h);

	gettimeofday(&now, NULL);

	if (how == CACHE_BYNAME)
	{
		if (_host_byname_cache[_host_byname_cache_index] != NULL)
			freehostent((struct hostent *)_host_byname_cache[_host_byname_cache_index]);

		_host_byname_cache[_host_byname_cache_index] = hcache;
		_host_byname_cache_flavor[_host_byname_cache_index] = want;
		_host_byname_cache_best_before[_host_byname_cache_index] = now.tv_sec + _host_cache_ttl;
		_host_byname_cache_index = (_host_byname_cache_index + 1) % HOST_CACHE_SIZE;
	}
	else
	{
		if (_host_byaddr_cache[_host_byaddr_cache_index] != NULL)
			freehostent((struct hostent *)_host_byaddr_cache[_host_byaddr_cache_index]);

		_host_byaddr_cache[_host_byaddr_cache_index] = hcache;
		_host_byaddr_cache_flavor[_host_byaddr_cache_index] = want;
		_host_byaddr_cache_best_before[_host_byaddr_cache_index] = now.tv_sec + _host_cache_ttl;
		_host_byaddr_cache_index = (_host_byaddr_cache_index + 1) % HOST_CACHE_SIZE;
	}

	pthread_mutex_unlock(&_host_cache_lock);
}

static struct hostent *
cache_gethostbyname(const char *name, int want)
{
	int i;
	struct hostent *h, *res;
	char **aliases;
	struct timeval now;

	if (_host_cache_ttl == 0) return NULL;
	if (name == NULL) return NULL;

	pthread_mutex_lock(&_host_cache_lock);

	gettimeofday(&now, NULL);

	for (i = 0; i < HOST_CACHE_SIZE; i++)
	{
		if (_host_byname_cache_best_before[i] == 0) continue;
		if ((unsigned int)now.tv_sec > _host_byname_cache_best_before[i]) continue;

		if (_host_byname_cache_flavor[i] != want) continue;

		h = (struct hostent *)_host_byname_cache[i];

		if (h->h_name != NULL) 
		{
			if (!strcmp(name, h->h_name))
			{
				res = copy_host(h);
				pthread_mutex_unlock(&_host_cache_lock);
				return res;
			}
		}

		aliases = h->h_aliases;
		if (aliases == NULL)
		{
			pthread_mutex_unlock(&_host_cache_lock);
			return NULL;
		}

		for (; *aliases != NULL; *aliases++)
		{
			if (!strcmp(name, *aliases))
			{
				res = copy_host(h);
				pthread_mutex_unlock(&_host_cache_lock);
				return res;
			}
		}
	}

	pthread_mutex_unlock(&_host_cache_lock);
	return NULL;
}

static struct hostent *
cache_gethostbyaddr(const char *addr, int want)
{
	int i, j, len;
	struct hostent *h, *res;
	struct timeval now;

	if (addr == NULL) return NULL;
	if (_host_cache_ttl == 0) return NULL;

	pthread_mutex_lock(&_host_cache_lock);

	gettimeofday(&now, NULL);

	len = IPV4_ADDR_LEN;
	if (want > WANT_A4_ONLY) len = IPV6_ADDR_LEN;

	for (i = 0; i < HOST_CACHE_SIZE; i++)
	{
		if (_host_byaddr_cache_best_before[i] == 0) continue;
		if ((unsigned int)now.tv_sec > _host_byaddr_cache_best_before[i]) continue;

		if (_host_byaddr_cache_flavor[i] != want) continue;

		h = (struct hostent *)_host_byaddr_cache[i];

		if (h->h_addr_list == NULL) continue;

		for (j = 0; h->h_addr_list[j] != NULL; j++)
		{
			if (memcmp(addr, h->h_addr_list[j], len) == 0)
			{
				res = copy_host(h);
				pthread_mutex_unlock(&_host_cache_lock);
				return res;
			}
		}
	}

	pthread_mutex_unlock(&_host_cache_lock);
	return NULL;
}

static struct hostent *
lu_gethostbyaddr(const char *addr, int want, int *err)
{
	struct hostent *h;
	unsigned int datalen;
	XDR inxdr;
	static int proc4 = -1;
	static int proc6 = -1;
	char *lookup_buf, *address;
	int proc, count, len, family;
	struct in_addr addr4;
	struct in6_addr addr6;

	family = AF_INET;
	len = IPV4_ADDR_LEN;
	if ((want == WANT_A6_ONLY) || (want == WANT_A6_PLUS_MAPPED_A4))
	{
		family = AF_INET6;
		len = IPV6_ADDR_LEN;
	}

	if ((family == AF_INET) && (proc4 < 0))
	{
		if (_lookup_link(_lu_port, "gethostbyaddr", &proc4) != KERN_SUCCESS)
		{
			*err = NO_RECOVERY;
			return NULL;
		}
	}
	else if ((family == AF_INET6) && (proc6 < 0))
	{
		if (_lookup_link(_lu_port, "getipv6nodebyaddr", &proc6) != KERN_SUCCESS)
		{
			*err = NO_RECOVERY;
			return NULL;
		}
	}

	address = NULL;

	if (family == AF_INET)
	{
		memmove(&(addr4.s_addr), addr, IPV4_ADDR_LEN);
		addr4.s_addr = htonl(addr4.s_addr);
		address = (char *)&(addr4.s_addr);
		proc = proc4;
	}
	else
	{
		memmove(&(addr6.__u6_addr.__u6_addr32[0]), addr, IPV6_ADDR_LEN);
		addr6.__u6_addr.__u6_addr32[0] = htonl(addr6.__u6_addr.__u6_addr32[0]);
		addr6.__u6_addr.__u6_addr32[1] = htonl(addr6.__u6_addr.__u6_addr32[1]);
		addr6.__u6_addr.__u6_addr32[2] = htonl(addr6.__u6_addr.__u6_addr32[2]);
		addr6.__u6_addr.__u6_addr32[3] = htonl(addr6.__u6_addr.__u6_addr32[3]);
		address = (char *)&(addr6.__u6_addr.__u6_addr32[0]);
		proc = proc6;
	}
		
	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)address, len / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen) != KERN_SUCCESS)
	{
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		*err = NO_RECOVERY;
		return NULL;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	*err = 0;

	h = extract_host(&inxdr, want, err);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return h;
}

static struct hostent *
lu_gethostbyname(const char *name, int want, int *err)
{
	struct hostent *h;
	unsigned int datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	static int proc4 = -1;
	static int proc6 = -1;
	char *lookup_buf;
	int proc, count, family;

	family = AF_INET;
	if (want > WANT_A4_ONLY) family = AF_INET6;

	if (((want == WANT_MAPPED_A4_ONLY) || (family == AF_INET)) && (proc4 < 0))
	{
		if (_lookup_link(_lu_port, "gethostbyname", &proc4) != KERN_SUCCESS)
		{
			*err = NO_RECOVERY;
			return NULL;
		}
	}
	else if ((family == AF_INET6) && (proc6 < 0))
	{
		if (_lookup_link(_lu_port, "getipv6nodebyname", &proc6) != KERN_SUCCESS)
		{
			*err = NO_RECOVERY;
			return NULL;
		}
	}

	proc = proc4;
	if ((family == AF_INET6) && (want != WANT_MAPPED_A4_ONLY)) proc = proc6;

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, (_lu_string *)&name))
	{
		xdr_destroy(&outxdr);
		*err = NO_RECOVERY;
		return NULL;
	}

	datalen = 0;
	lookup_buf = NULL;

	if (_lookup_all(_lu_port, proc, (unit *)namebuf, xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, &lookup_buf, &datalen) != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	if ((lookup_buf == NULL) || (datalen == 0)) return NULL;

	xdrmem_create(&inxdr, lookup_buf, datalen, XDR_DECODE);

	count = 0;
	if (!xdr_int(&inxdr, &count))
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		*err = NO_RECOVERY;
		return NULL;
	}

	if (count == 0)
	{
		xdr_destroy(&inxdr);
		vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	*err = 0;

	h = extract_host(&inxdr, want, err);
	xdr_destroy(&inxdr);
	vm_deallocate(mach_task_self(), (vm_address_t)lookup_buf, datalen);

	return h;
}

static void
lu_endhostent()
{
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_host, free_lu_thread_info_host);
	_lu_data_free_vm_xdr(tdata);
}

static void
lu_sethostent()
{
	lu_endhostent();
}

static struct hostent *
lu_gethostent(int want, int *err)
{
	static int proc = -1;
	struct lu_thread_info *tdata;
	struct hostent *h;

	tdata = _lu_data_create_key(_lu_data_key_host, free_lu_thread_info_host);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_host, tdata);
	}

	if (tdata->lu_vm == NULL)
	{
		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "gethostent", &proc) != KERN_SUCCESS)
			{
				lu_endhostent();
				*err = NO_RECOVERY;
				return NULL;
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &(tdata->lu_vm), &(tdata->lu_vm_length)) != KERN_SUCCESS)
		{
			lu_endhostent();
			*err = HOST_NOT_FOUND;
			return NULL;
		}

		/* mig stubs measure size in words (4 bytes) */
		tdata->lu_vm_length *= 4;

		if (tdata->lu_xdr != NULL)
		{
			xdr_destroy(tdata->lu_xdr);
			free(tdata->lu_xdr);
		}
		tdata->lu_xdr = (XDR *)calloc(1, sizeof(XDR));

		xdrmem_create(tdata->lu_xdr, tdata->lu_vm, tdata->lu_vm_length, XDR_DECODE);
		if (!xdr_int(tdata->lu_xdr, &tdata->lu_vm_cursor))
		{
			lu_endhostent();
			*err = NO_RECOVERY;
			return NULL;
		}
	}

	if (tdata->lu_vm_cursor == 0)
	{
		lu_endhostent();
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	h = extract_host(tdata->lu_xdr, want, err);
	if (h == NULL)
	{
		lu_endhostent();
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	*err = 0;
	tdata->lu_vm_cursor--;
	
	return h;
}

static struct hostent *
gethostbyaddrerrno(const char *addr, int len, int type, int *err)
{
	struct hostent *res = NULL;
	int want, from_cache;

	*err = 0;

	want = WANT_A4_ONLY;
	if (type == AF_INET6) want = WANT_A6_ONLY;

	if ((type == AF_INET6) && (len == 16) && (is_a4_mapped((const char *)addr) || is_a4_compat((const char *)addr)))
	{
		addr += 12;
		len = 4;
		type = AF_INET;
		want = WANT_MAPPED_A4_ONLY;
	}

	from_cache = 0;
	res = cache_gethostbyaddr(addr, want);

	if (res != NULL)
	{
		from_cache = 1;
	}
	else if (_lu_running())
	{
		res = lu_gethostbyaddr(addr, want, err);
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(_old_gethostbyaddr(addr, len, type));
		*err = h_errno;
		pthread_mutex_unlock(&_host_lock);
	}

	if (from_cache == 0) cache_host(res, want, CACHE_BYADDR);

	return res;
}

struct hostent *
gethostbyaddr(const void *addr, socklen_t len, int type)
{
	struct hostent *res;
	struct lu_thread_info *tdata;

	res = gethostbyaddrerrno(addr, len, type, &h_errno);
	if (res == NULL)
	{
		return NULL;
	}

	tdata = _lu_data_create_key(_lu_data_key_host, free_lu_thread_info_host);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_host, tdata);
	}

	recycle_host(tdata, res);
	return (struct hostent *)tdata->lu_entry;
}
	
struct hostent *
gethostbynameerrno(const char *name, int *err)
{
	struct hostent *res = NULL;
	struct in_addr addr;
	int i, is_addr, from_cache;

	*err = 0;

	/* 
	 * If name is all dots and digits without a trailing dot, 
	 * call inet_aton.  If it's OK, return a fake entry.
	 * Otherwise, return an error.
	 *
	 * If name has alpha or ends with a dot, proceed as usual...
	 */
	if (name == NULL)
	{
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	if (name[0] == '\0')
	{
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	is_addr = 1;
	for (i = 0; name[i] != '\0'; i++)
	{
		if (name[i] == '.') continue;
		if ((name[i] >= '0') && (name[i] <= '9')) continue;
		is_addr = 0;
		break;
	}

	if ((is_addr == 1) && (name[i-1] == '.')) is_addr = 0;

	res = NULL;
	from_cache = 0;

	if (is_addr == 1)
	{
		if (inet_aton(name, &addr) == 0)
		{
			*err = HOST_NOT_FOUND;
			return NULL;
		}
		res = fake_hostent(name, addr);
	}

	if (res == NULL) 
	{
		res = cache_gethostbyname(name, WANT_A4_ONLY);
	}

	if (res != NULL)
	{
		from_cache = 1;
	}
	else if (_lu_running())
	{
		res = lu_gethostbyname(name, WANT_A4_ONLY, err);
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(_old_gethostbyname(name));
		*err = h_errno;
		pthread_mutex_unlock(&_host_lock);
	}

	if (res == NULL)
	{
		if (inet_aton(name, &addr) == 0)
		{
			*err = HOST_NOT_FOUND;
			return NULL;
		}

		res = gethostbyaddrerrno((char *)&addr, sizeof(addr), AF_INET, err);
		if (res == NULL)
		{
			res = fake_hostent(name, addr);
		}
	}

	if (from_cache == 0) cache_host(res, WANT_A4_ONLY, CACHE_BYNAME);

	return res;
}

struct hostent *
gethostbyname(const char *name)
{
	struct hostent *res;
	struct lu_thread_info *tdata;

	res = gethostbynameerrno(name, &h_errno);
	if (res == NULL)
	{
		return NULL;
	}

	tdata = _lu_data_create_key(_lu_data_key_host, free_lu_thread_info_host);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_host, tdata);
	}

	recycle_host(tdata, res);
	return (struct hostent *)tdata->lu_entry;
}

struct hostent *
gethostbyname2(const char *name, int af)
{
	struct hostent *res;
	struct lu_thread_info *tdata;

	res = getipnodebyname(name, af, 0, &h_errno);
	if (res == NULL)
	{
		errno = EAFNOSUPPORT;
		return NULL;
	}

	tdata = _lu_data_create_key(_lu_data_key_host, free_lu_thread_info_host);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_host, tdata);
	}

	recycle_host(tdata, res);
	return (struct hostent *)tdata->lu_entry;
}

struct hostent *
gethostent(void)
{
	struct hostent *res = NULL;
	struct lu_thread_info *tdata;

	tdata = _lu_data_create_key(_lu_data_key_host, free_lu_thread_info_host);
	if (tdata == NULL)
	{
		tdata = (struct lu_thread_info *)calloc(1, sizeof(struct lu_thread_info));
		_lu_data_set_key(_lu_data_key_host, tdata);
	}

	if (_lu_running()) 
	{
		res = lu_gethostent(WANT_A4_ONLY, &h_errno);
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(_old_gethostent());
		pthread_mutex_unlock(&_host_lock);
	}

	recycle_host(tdata, res);
	return (struct hostent *)tdata->lu_entry;
}

void
sethostent(int stayopen)
{
	if (_lu_running()) lu_sethostent();
	else _old_sethostent(stayopen);
}

void
endhostent(void)
{
	if (_lu_running()) lu_endhostent();
	else _old_endhostent();
}

__private_extern__ int
is_a4_mapped(const char *s)
{
	int i;
	u_int8_t c;

	if (s == NULL) return 0;

	for (i = 0; i < 10; i++)
	{
		c = s[i];
		if (c != 0x0) return 0;
	}

	for (i = 10; i < 12; i++)
	{
		c = s[i];
		if (c != 0xff) return 0;
	}

	return 1;
}
	
__private_extern__ int
is_a4_compat(const char *s)
{
	int i;
	u_int8_t c;

	if (s == NULL) return 0;

	for (i = 0; i < 12; i++)
	{
		c = s[i];
		if (c != 0x0) return 0;
	}

	/* Check for :: and ::1 */
	for (i = 13; i < 15; i++)
	{
		/* anything non-zero in these 3 bytes means it's a V4 address */
		c = s[i];
		if (c != 0x0) return 1;
	}

	/* Leading 15 bytes are all zero */
	c = s[15];
	if (c == 0x0) return 0;
	if (c == 0x1) return 0;

	return 1;
}

struct hostent *
getipnodebyaddr(const void *src, size_t len, int af, int *err)
{
	struct hostent *res;

	*err = 0;

	res = gethostbyaddrerrno((const char *)src, len, af, err);
	if (res == NULL)
	{
		return NULL;
	}

	if (res->h_name == NULL)
	{
		freehostent(res);
		return NULL;
	}

	return res;
}

struct hostent *
getipnodebyname(const char *name, int af, int flags, int *err)
{
	int status, want, really_want, if4, if6, from_cache;
	struct hostent *res;
	struct ifaddrs *ifa, *ifap;
	struct in_addr addr4;
	struct in6_addr addr6;

	memset(&addr4, 0, sizeof(struct in_addr));
	memset(&addr6, 0, sizeof(struct in6_addr));

	*err = 0;

	if (af == AF_INET)
	{
		status = inet_aton(name, &addr4);
		if (status == 1)
		{
			/* return a fake hostent */
			res = fake_hostent(name, addr4);
			return res;
		}
	}
	else if (af == AF_INET6)
	{
		status = inet_pton(af, name, &addr6);
		if (status == 1)
		{
			/* return a fake hostent */
			res = fake_hostent6(name, addr6);
			return res;
		}
		status = inet_aton(name, &addr4);
		if (status == 1)
		{
			if (!(flags & (AI_V4MAPPED|AI_V4MAPPED_CFG)))
			{
				*err = HOST_NOT_FOUND;
				return NULL;
			}

			addr6.__u6_addr.__u6_addr32[0] = 0x00000000;
			addr6.__u6_addr.__u6_addr32[1] = 0x00000000;
			addr6.__u6_addr.__u6_addr32[2] = htonl(0x0000ffff);
			memmove(&(addr6.__u6_addr.__u6_addr32[3]), &(addr4.s_addr), IPV4_ADDR_LEN);

			/* return a fake hostent */
			res = fake_hostent6(name, addr6);
			return res;
		}
	}
	else
	{
		*err = NO_RECOVERY;
		return NULL;
	}

	/*
	 * IF AI_ADDRCONFIG is set, we need to know what interface flavors we really have.
	 */

	if4 = 0;
	if6 = 0;

	if (flags & AI_ADDRCONFIG)
	{
		if (getifaddrs(&ifa) < 0)
		{
			*err = NO_RECOVERY;
			return NULL;
		}

		for (ifap = ifa; ifap != NULL; ifap = ifap->ifa_next)
		{
			if (ifap->ifa_addr == NULL) continue;
			if ((ifap->ifa_flags & IFF_UP) == 0) continue;
			if (ifap->ifa_addr->sa_family == AF_INET) if4++;
			else if (ifap->ifa_addr->sa_family == AF_INET6) if6++;
		}

		freeifaddrs(ifa);

		/* Bail out if there are no interfaces */
		if ((if4 == 0) && (if6 == 0))
		{
			*err = NO_ADDRESS;
			return NULL;
		}
	}

	/*
	 * Figure out what we want.
	 * If user asked for AF_INET, we only want V4 addresses.
	 */
	want = WANT_A4_ONLY;
	really_want = want;

	if (af == AF_INET)
	{
		want = WANT_A4_ONLY;
		if ((flags & AI_ADDRCONFIG) && (if4 == 0))
		{
			*err = NO_ADDRESS;
			return NULL;
		}
	}
	else
	{
		/* af == AF_INET6 */
		want = WANT_A6_ONLY;
		really_want = want;
		if (flags & (AI_V4MAPPED|AI_V4MAPPED_CFG))
		{
			if (flags & AI_ALL)
			{
				want = WANT_A6_PLUS_MAPPED_A4;
				really_want = want;
			}
			else
			{
				want = WANT_A6_ONLY;
				really_want = WANT_A6_OR_MAPPED_A4_IF_NO_A6;
			}
		}
		else
		{
			if ((flags & AI_ADDRCONFIG) && (if6 == 0)) 
			{
				*err = NO_ADDRESS;
				return NULL;
			}
		}
	}

	from_cache = 0;
	res = cache_gethostbyname(name, want);

	if (res != NULL)
	{
		from_cache = 1;
	}
	else if (_lu_running())
	{
		res = lu_gethostbyname(name, want, err);
		if ((res == NULL) && 
		    ((really_want == WANT_A6_OR_MAPPED_A4_IF_NO_A6) ||
		     (really_want == WANT_A6_PLUS_MAPPED_A4       )))
		{
			res = lu_gethostbyname(name, WANT_MAPPED_A4_ONLY, err);
		}
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(_old_gethostbyname(name));
		*err = h_errno;
		pthread_mutex_unlock(&_host_lock);
	}

	if (res == NULL)
	{
		*err = HOST_NOT_FOUND;
		return NULL;
	}

	if (from_cache == 0) cache_host(res, want, CACHE_BYNAME);

	return res;
}

/*
 * Given a host's name, this routine returns its 48 bit ethernet address.
 * Returns zero if successful, non-zero otherwise.
 */
int
lu_ether_hostton(const char *host, struct ether_addr *e)
{
	unsigned int i, n, j, x[6];
	ni_proplist *q, **r;
	char *s;
	
	if (host == NULL) return -1;
	if (e == NULL) return -1;
	
	q = lookupd_make_query("2", "kvk", "name", host, "en_address");
	if (q == NULL) return -1;
	
	n = lookupd_query(q, &r);
	ni_proplist_free(q);
	free(q);
	
	if (n == 0) return -1;
	if (r[0] == NULL) return -1;
	
	i = ni_proplist_match(*r[0], "en_address", NULL);
	if (i == (unsigned int)NI_INDEX_NULL) return -1;
	
	if (r[0]->ni_proplist_val[i].nip_val.ni_namelist_len == 0) return -1;
	
	s = r[0]->ni_proplist_val[i].nip_val.ni_namelist_val[0];
	j = sscanf(s, " %x:%x:%x:%x:%x:%x", &x[0], &x[1], &x[2], &x[3], &x[4], &x[5]);
	if (j != 6)
	{
		for (i = 0; i < n; i++)
		{
			ni_proplist_free(r[i]);
			free(r[i]);
		}
		free(r);
		return -1;
	}
	
	for (i = 0; i < 6; i++) e->ether_addr_octet[i] = x[i];
	
	for (i = 0; i < n; i++)
	{
		ni_proplist_free(r[i]);
		free(r[i]);
	}
	
	free(r);
	return 0;
}

/*
 * Given a 48 bit ethernet address, this routine return its host name.
 * Returns zero if successful, non-zero otherwise.
 */
int
lu_ether_ntohost(char *host, const struct ether_addr *e)
{
	unsigned int i, n, len, x[6];
	ni_proplist *q, **r;
	char str[256];
	
	if (host == NULL) return -1;
	if (e == NULL) return -1;
	
	for (i = 0; i < 6; i++) x[i] = e->ether_addr_octet[i];
	sprintf(str, "%x:%x:%x:%x:%x:%x", x[0], x[1], x[2], x[3], x[4], x[5]);
	
	q = lookupd_make_query("2", "kv", "en_address", str);
	if (q == NULL) return -1;
	
	n = lookupd_query(q, &r);
	ni_proplist_free(q);
	free(q);
	if (n == 0) return -1;
	if (r[0] == NULL) return -1;
	
	i = ni_proplist_match(*r[0], "name", NULL);
	if (i == (unsigned int)NI_INDEX_NULL) return -1;
	
	if (r[0]->ni_proplist_val[i].nip_val.ni_namelist_len == 0) return -1;
	
	len = strlen(r[0]->ni_proplist_val[i].nip_val.ni_namelist_val[0]) + 1;
	memcpy(host, r[0]->ni_proplist_val[i].nip_val.ni_namelist_val[0], len);
	
	for (i = 0; i < n; i++) ni_proplist_free(r[i]);
	free(r);
	return 0;
}

int
ether_hostton(const char *host, struct ether_addr *e)
{
	if (_lu_running()) return lu_ether_hostton(host, e);
	return _old_ether_hostton(host, e);
}

int
ether_ntohost(char *host, const struct ether_addr *e)
{
	if (_lu_running()) return lu_ether_ntohost(host, e);
	return _old_ether_ntohost(host, e);
}
