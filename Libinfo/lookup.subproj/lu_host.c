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
#include "lu_host.h"

#define ENTRY_SIZE sizeof(struct hostent)
#define ENTRY_KEY _li_data_key_host
#define HOST_CACHE_SIZE 20

#define CACHE_BYNAME 0
#define CACHE_BYADDR 1

static pthread_mutex_t _host_cache_lock = PTHREAD_MUTEX_INITIALIZER;

static void *_host_byname_cache[HOST_CACHE_SIZE] = { NULL };
static int _host_byname_cache_flavor[HOST_CACHE_SIZE] = { WANT_NOTHING };
static unsigned int _host_byname_cache_index = 0;

static void *_host_byaddr_cache[HOST_CACHE_SIZE] = { NULL };
static int _host_byaddr_cache_flavor[HOST_CACHE_SIZE] = { WANT_NOTHING };
static unsigned int _host_byaddr_cache_index = 0;

static unsigned int _host_cache_init = 0;

static pthread_mutex_t _host_lock = PTHREAD_MUTEX_INITIALIZER;

__private_extern__ struct hostent *LI_files_gethostbyname(const char *name);
__private_extern__ struct hostent *LI_files_gethostbyname2(const char *name, int af);
__private_extern__ struct hostent *LI_files_gethostbyaddr(const void *addr, socklen_t len, int type);
__private_extern__ struct hostent *LI_files_gethostent();
__private_extern__ void LI_files_sethostent(int stayopen);
__private_extern__ void LI_files_endhostent();

extern int _old_ether_hostton(const char *, struct ether_addr *);
extern int _old_ether_ntohost(char *, const struct ether_addr *);

extern int h_errno;

#define IPV6_ADDR_LEN 16
#define IPV4_ADDR_LEN 4

void
freehostent(struct hostent *h)
{
	char **aliases;
	int i;

	if (LI_ils_free(h, ENTRY_SIZE) == 0) return;

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
	free(h);
}

static struct hostent *
copy_host(struct hostent *in)
{
	if (in == NULL) return NULL;

	if (in->h_addrtype == AF_INET)
		return (struct hostent *)LI_ils_create("s*44a", in->h_name, in->h_aliases, in->h_addrtype, in->h_length, in->h_addr_list);

	if (in->h_addrtype == AF_INET6)
		return (struct hostent *)LI_ils_create("s*44c", in->h_name, in->h_aliases, in->h_addrtype, in->h_length, in->h_addr_list);

	return NULL;
}

static void
_free_addr_list(char **l)
{
	int i;

	if (l == NULL) return;
	for (i = 0; l[i] != NULL; i++) free(l[i]);
	free(l);
}

/* map ipv4 addresses and append to v6 list */
static int 
_map_v4(char ***v6, uint32_t n6, char **v4, uint32_t n4)
{
	struct in6_addr a6;
	uint32_t i;

	a6.__u6_addr.__u6_addr32[0] = 0x00000000;
	a6.__u6_addr.__u6_addr32[1] = 0x00000000;
	a6.__u6_addr.__u6_addr32[2] = htonl(0x0000ffff);

	if (*v6 == NULL)
	{
		*v6 = (char **)calloc(n4 + 1, sizeof(char *));
	}
	else
	{
		*v6 = (char **)reallocf(*v6, (n6 + n4 + 1) * sizeof(char *));
	}

	if (*v6 == NULL) return -1;

	for (i = 0; i < n4; i++)
	{
		(*v6)[n6] = (char *)calloc(1, IPV6_ADDR_LEN);
		if ((*v6)[n6] == NULL) return -1;

		memcpy(&(a6.__u6_addr.__u6_addr32[3]), v4[i], IPV4_ADDR_LEN);
		memcpy((*v6)[n6], &(a6.__u6_addr.__u6_addr32[0]), IPV6_ADDR_LEN);

		n6++;
	}

	return 0;
}

__private_extern__ struct hostent *
extract_host(kvarray_t *in, int want)
{
	struct hostent tmp, *out;
	uint32_t i, d, k, kcount, vcount, v4count, v6count;
	int status, addr_len;
	int family, addr_count;
	struct in_addr a4;
	struct in6_addr a6;
	char **v4addrs, **v6addrs;
	char *empty[1];

	v4addrs = NULL;
	v6addrs = NULL;
	v4count = 0;
	v6count = 0;
	addr_count = 0;
	addr_len = sizeof(void *);

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, ENTRY_SIZE);

	family = AF_INET;
	tmp.h_length = IPV4_ADDR_LEN;

	if (want != WANT_A4_ONLY)
	{
		family = AF_INET6;
		tmp.h_length = IPV6_ADDR_LEN;
	}

	tmp.h_addrtype = family;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "h_name"))
		{
			if (tmp.h_name != NULL) continue;

			vcount = in->dict[d].vcount[k];
			if (vcount == 0) continue;

			tmp.h_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "h_aliases"))
		{
			if (tmp.h_aliases != NULL) continue;

			vcount = in->dict[d].vcount[k];
			if (vcount == 0) continue;

			tmp.h_aliases = (char **)in->dict[d].val[k];
		}
		else if (!strcmp(in->dict[d].key[k], "h_ipv4_addr_list"))
		{
			if (v4addrs != NULL) continue;

			v4count = in->dict[d].vcount[k];
			if (v4count == 0) continue;

			v4addrs = (char **)calloc(v4count + 1, sizeof(char *));
			if (v4addrs == NULL)
			{
				_free_addr_list(v6addrs);
				return NULL;
			}

			for (i = 0; i < v4count; i++)
			{
				v4addrs[i] = calloc(1, IPV4_ADDR_LEN);
				if (v4addrs[i] == NULL)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memset(&a4, 0, sizeof(struct in_addr));
				status = inet_pton(AF_INET, in->dict[d].val[k][i], &a4);
				if (status != 1)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memcpy(v4addrs[i], &a4, IPV4_ADDR_LEN);
			}
		}
		else if (!strcmp(in->dict[d].key[k], "h_ipv6_addr_list"))
		{
			if (v6addrs != NULL) continue;

			v6count = in->dict[d].vcount[k];
			if (v6count == 0) continue;

			v6addrs = (char **)calloc(v6count + 1, sizeof(char *));
			if (v6addrs == NULL)
			{
				_free_addr_list(v4addrs);
				return NULL;
			}

			for (i = 0; i < v6count; i++)
			{
				v6addrs[i] = calloc(1, IPV6_ADDR_LEN);
				if (v6addrs[i] == NULL)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memset(&a6, 0, sizeof(struct in6_addr));
				status = inet_pton(AF_INET6, in->dict[d].val[k][i], &a6);
				if (status != 1)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memcpy(v6addrs[i], &(a6.__u6_addr.__u6_addr32[0]), IPV6_ADDR_LEN);
			}
		}
	}

	if (tmp.h_name == NULL) tmp.h_name = "";
	if (tmp.h_aliases == NULL) tmp.h_aliases = empty;

	if (want == WANT_A4_ONLY)
	{
		_free_addr_list(v6addrs);
		if (v4addrs == NULL) return NULL;

		tmp.h_addr_list = v4addrs;
		out = copy_host(&tmp);
		_free_addr_list(v4addrs);

		return out;
	}
	else if ((want == WANT_A6_ONLY) || ((want == WANT_A6_OR_MAPPED_A4_IF_NO_A6) && (v6count > 0)))
	{
		_free_addr_list(v4addrs);
		if (v6addrs == NULL) return NULL;

		tmp.h_addr_list = v6addrs;
		out = copy_host(&tmp);
		_free_addr_list(v6addrs);

		return out;
	}

	/*
	 * At this point, want is WANT_A6_PLUS_MAPPED_A4, WANT_MAPPED_A4_ONLY,
	 * or WANT_A6_OR_MAPPED_A4_IF_NO_A6.  In the last case, there are no ipv6
	 * addresses, so that case degenerates into WANT_MAPPED_A4_ONLY.
	 */
	if (want == WANT_A6_OR_MAPPED_A4_IF_NO_A6) want = WANT_MAPPED_A4_ONLY;

	if (want == WANT_MAPPED_A4_ONLY)
	{
		_free_addr_list(v6addrs);
		v6addrs = NULL;
		v6count = 0;
	}

	status = _map_v4(&v6addrs, v6count, v4addrs, v4count);
	_free_addr_list(v4addrs);
	if (status != 0)
	{
		_free_addr_list(v6addrs);
		return NULL;
	}

	if (v6addrs == NULL) return NULL;

	tmp.h_addr_list = v6addrs;
	out = copy_host(&tmp);
	_free_addr_list(v6addrs);

	return out;
}

__private_extern__ struct hostent *
fake_hostent(const char *name, struct in_addr addr)
{
	struct hostent tmp;
	char *addrs[2];
	char *aliases[1];

	if (name == NULL) return NULL;

	memset(&tmp, 0, ENTRY_SIZE);

	tmp.h_name = (char *)name;
	tmp.h_addrtype = AF_INET;
	tmp.h_length = IPV4_ADDR_LEN;
	tmp.h_addr_list = addrs;
	addrs[0] = (char *)&(addr.s_addr);
	addrs[1] = NULL;
	tmp.h_aliases = aliases;
	aliases[0] = NULL;

	return copy_host(&tmp);
}

__private_extern__ struct hostent *
fake_hostent6(const char *name, struct in6_addr addr)
{
	struct hostent tmp;
	char *addrs[2];
	char *aliases[1];

	if (name == NULL) return NULL;

	memset(&tmp, 0, ENTRY_SIZE);

	tmp.h_name = (char *)name;
	tmp.h_addrtype = AF_INET6;
	tmp.h_length = IPV6_ADDR_LEN;
	tmp.h_addr_list = addrs;
	addrs[0] = (char *)&(addr.__u6_addr.__u6_addr32[0]);
	addrs[1] = NULL;
	tmp.h_aliases = aliases;
	aliases[0] = NULL;

	return copy_host(&tmp);
}

static void
cache_host(struct hostent *h, int want, int how)
{
	struct hostent *hcache;

	if (h == NULL) return;

	pthread_mutex_lock(&_host_cache_lock);

	hcache = copy_host(h);

	if (how == CACHE_BYNAME)
	{
		if (_host_byname_cache[_host_byname_cache_index] != NULL) LI_ils_free(_host_byname_cache[_host_byname_cache_index], ENTRY_SIZE);

		_host_byname_cache[_host_byname_cache_index] = hcache;
		_host_byname_cache_flavor[_host_byname_cache_index] = want;
		_host_byname_cache_index = (_host_byname_cache_index + 1) % HOST_CACHE_SIZE;
	}
	else
	{
		if (_host_byaddr_cache[_host_byaddr_cache_index] != NULL) LI_ils_free(_host_byaddr_cache[_host_byaddr_cache_index], ENTRY_SIZE);

		_host_byaddr_cache[_host_byaddr_cache_index] = hcache;
		_host_byaddr_cache_flavor[_host_byaddr_cache_index] = want;
		_host_byaddr_cache_index = (_host_byaddr_cache_index + 1) % HOST_CACHE_SIZE;
	}

	_host_cache_init = 1;

	pthread_mutex_unlock(&_host_cache_lock);
}

static int
host_cache_check()
{
	uint32_t i, status;

	/* don't consult cache if it has not been initialized */
	if (_host_cache_init == 0) return 1;

	status = LI_L1_cache_check(ENTRY_KEY);

	/* don't consult cache if it is disabled or if we can't validate */
	if ((status == LI_L1_CACHE_DISABLED) || (status == LI_L1_CACHE_FAILED)) return 1;

	/* return 0 if cache is OK */
	if (status == LI_L1_CACHE_OK) return 0;

	/* flush cache */
	pthread_mutex_lock(&_host_cache_lock);

	for (i = 0; i < HOST_CACHE_SIZE; i++)
	{
		LI_ils_free(_host_byname_cache[i], ENTRY_SIZE);
		_host_byname_cache[i] = NULL;
		_host_byname_cache_flavor[i] = WANT_NOTHING;

		LI_ils_free(_host_byaddr_cache[i], ENTRY_SIZE);
		_host_byaddr_cache[i] = NULL;
		_host_byaddr_cache_flavor[i] = WANT_NOTHING;
	}

	_host_byname_cache_index = 0;
	_host_byaddr_cache_index = 0;

	pthread_mutex_unlock(&_host_cache_lock);

	/* don't consult cache - it's now empty */
	return 1;
}


static struct hostent *
cache_gethostbyname(const char *name, int want)
{
	int i;
	struct hostent *h, *res;
	char **aliases;

	if (name == NULL) return NULL;
	if (host_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_host_cache_lock);

	for (i = 0; i < HOST_CACHE_SIZE; i++)
	{
		if (_host_byname_cache_flavor[i] != want) continue;

		h = (struct hostent *)_host_byname_cache[i];
		if (h == NULL) continue;

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

	if (addr == NULL) return NULL;
	if (host_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_host_cache_lock);

	len = IPV4_ADDR_LEN;
	if (want > WANT_A4_ONLY) len = IPV6_ADDR_LEN;

	for (i = 0; i < HOST_CACHE_SIZE; i++)
	{
		if (_host_byaddr_cache_flavor[i] != want) continue;

		h = (struct hostent *)_host_byaddr_cache[i];
		if (h == NULL) continue;

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
ds_gethostbyaddr(const char *paddr, uint32_t family, int *err)
{
	struct hostent *entry;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;
	static int proc = -1;
	struct in_addr addr4;
	struct in6_addr addr6;
	char tmp[64];
	int want;

	if (paddr == NULL)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbyaddr", &proc);
		if (status != KERN_SUCCESS)
		{
			if (err != NULL) *err = NO_RECOVERY;
			return NULL;
		}
	}

	memset(&addr4, 0, sizeof(struct in_addr));
	memset(&addr6, 0, sizeof(struct in6_addr));
	memset(tmp, 0, sizeof(tmp));
	want = WANT_A4_ONLY;

	if (family == AF_INET)
	{
		want = WANT_A4_ONLY;
		memcpy(&(addr4.s_addr), paddr, IPV4_ADDR_LEN);
		if (inet_ntop(family, &addr4, tmp, sizeof(tmp)) == NULL)
		{
			if (err != NULL) *err = NO_RECOVERY;
			return NULL;
		}
	}
	else if (family == AF_INET6)
	{
		want = WANT_A6_ONLY;
		memcpy(addr6.s6_addr, paddr, IPV6_ADDR_LEN);
		if (inet_ntop(family, &addr6, tmp, sizeof(tmp)) == NULL)
		{
			if (err != NULL) *err = NO_RECOVERY;
			return NULL;
		}
	}
	else
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	request = kvbuf_query("ksku", "address", tmp, "family", family);
	if (request == NULL)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	if (err != NULL) *err = 0;
	entry = extract_host(reply, want);

	if ((entry == NULL) && (err != NULL)) *err = HOST_NOT_FOUND;
	kvarray_free(reply);

	return entry;
}

static struct hostent *
ds_gethostbyname(const char *name, uint32_t want, int *err)
{
	struct hostent *entry;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;
	static int proc = -1;
	uint32_t want4, want6;

	want4 = 1;
	want6 = 1;

	if (want == WANT_A4_ONLY) want6 = 0;
	else if (want == WANT_A6_ONLY) want4 = 0;
	else if (WANT_MAPPED_A4_ONLY) want6 = 0;

	if (name == NULL)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbyname", &proc);
		if (status != KERN_SUCCESS)
		{
			if (err != NULL) *err = NO_RECOVERY;
			return NULL;
		}
	}

	request = kvbuf_query("kskuku", "name", name, "ipv4", want4, "ipv6", want6);
	if (request == NULL)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	if (err != NULL) *err = 0;
	entry = extract_host(reply, want);
	if ((entry == NULL) && (err != NULL)) *err = HOST_NOT_FOUND;
	kvarray_free(reply);

	return entry;
}

static void
ds_endhostent()
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static void
ds_sethostent()
{
	ds_endhostent();
}

static struct hostent *
ds_gethostent(int *err)
{
	struct hostent *entry;
	struct li_thread_info *tdata;
	kvarray_t *reply, *vma;
	kern_return_t status;
	static int proc = -1;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	if (tdata->li_vm == NULL)
	{
		if (proc < 0)
		{
			status = LI_DSLookupGetProcedureNumber("gethostent", &proc);
			if (status != KERN_SUCCESS)
			{
				if (err != NULL) *err = NO_RECOVERY;
				LI_data_free_kvarray(tdata);
				tdata->li_vm = NULL;
				return NULL;
			}
		}

		reply = NULL;
		status = LI_DSLookupQuery(proc, NULL, &reply);

		if (status != KERN_SUCCESS)
		{
			if (err != NULL) *err = NO_RECOVERY;
			LI_data_free_kvarray(tdata);
			tdata->li_vm = NULL;
			return NULL;
		}

		tdata->li_vm = (char *)reply;
	}

	if (err != NULL) *err = 0;

	vma = (kvarray_t *)(tdata->li_vm);
	if (vma == NULL)
	{
		if (err != NULL) *err = HOST_NOT_FOUND;
		return NULL;
	}

	/*
	 * gethostent only returns IPv4 addresses, but the reply
	 * from Directory Service may contain a mix of IPv4 and Ipv6
	 * entries.  extract_host will return NULL if the current
	 * dictionary is not the family we want, so we loop until
	 * we get the next IPv4 entry or we run out of entries.
	 */
	entry = NULL;
	while ((vma->curr < vma->count) && (entry == NULL))
	{
		entry = extract_host(vma, WANT_A4_ONLY);
	}

	if (entry == NULL)
	{
		if (err != NULL) *err = HOST_NOT_FOUND;
		LI_data_free_kvarray(tdata);
		tdata->li_vm = NULL;
		return NULL;
	}

	return entry;
}

static struct hostent *
gethostbyaddrerrno(const char *addr, int len, uint32_t family, int *err)
{
	struct hostent *res = NULL;
	int want, add_to_cache;

	if (err != NULL) *err = 0;

	want = WANT_A4_ONLY;
	if (family == AF_INET6) want = WANT_A6_ONLY;

	if ((family == AF_INET6) && (len == IPV6_ADDR_LEN) && (is_a4_mapped((const char *)addr) || is_a4_compat((const char *)addr)))
	{
		addr += 12;
		len = 4;
		family = AF_INET;
		want = WANT_MAPPED_A4_ONLY;
	}

	add_to_cache = 0;
	res = cache_gethostbyaddr(addr, want);

	if (res != NULL)
	{
	}
	else if (_ds_running())
	{
		res = ds_gethostbyaddr(addr, family, err);
		if (res != NULL) add_to_cache = 1;
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(LI_files_gethostbyaddr(addr, len, family));
		if (err != NULL) *err = h_errno;
		pthread_mutex_unlock(&_host_lock);
	}

	if (add_to_cache == 1) cache_host(res, want, CACHE_BYADDR);

	return res;
}

struct hostent *
gethostbyaddr(const void *addr, socklen_t len, int type)
{
	struct hostent *res;
	struct li_thread_info *tdata;

	res = gethostbyaddrerrno(addr, len, type, &h_errno);
	if (res == NULL) return NULL;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct hostent *)tdata->li_entry;
}

struct hostent *
gethostbynameerrno(const char *name, int *err)
{
	struct hostent *res = NULL;
	struct in_addr addr;
	int i, is_addr, add_to_cache;

	if (err != NULL) *err = 0;

	/* 
	 * If name is all dots and digits without a trailing dot, 
	 * call inet_aton.  If it's OK, return a fake entry.
	 * Otherwise, return an error.
	 *
	 * If name has alpha or ends with a dot, proceed as usual...
	 */
	if (name == NULL)
	{
		if (err != NULL) *err = HOST_NOT_FOUND;
		return NULL;
	}

	if (name[0] == '\0')
	{
		if (err != NULL) *err = HOST_NOT_FOUND;
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
	add_to_cache = 0;

	if (is_addr == 1)
	{
		if (inet_aton(name, &addr) == 0)
		{
			if (err != NULL) *err = HOST_NOT_FOUND;
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
	}
	else if (_ds_running())
	{
		res = ds_gethostbyname(name, WANT_A4_ONLY, err);
		if (res != NULL) add_to_cache = 1;
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(LI_files_gethostbyname(name));
		if (err != NULL) *err = h_errno;
		pthread_mutex_unlock(&_host_lock);
	}

	if (res == NULL)
	{
		if (inet_aton(name, &addr) == 0)
		{
			if (err != NULL) *err = HOST_NOT_FOUND;
			return NULL;
		}

		res = gethostbyaddrerrno((char *)&addr, sizeof(addr), AF_INET, err);
		if (res == NULL)
		{
			res = fake_hostent(name, addr);
		}
	}

	if (add_to_cache == 1) cache_host(res, WANT_A4_ONLY, CACHE_BYNAME);

	return res;
}

struct hostent *
gethostbyname(const char *name)
{
	struct hostent *res;
	struct li_thread_info *tdata;

	res = gethostbynameerrno(name, &h_errno);
	if (res == NULL) return NULL;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct hostent *)tdata->li_entry;
}

struct hostent *
gethostbyname2(const char *name, int family)
{
	struct hostent *res;
	struct li_thread_info *tdata;

	res = getipnodebyname(name, family, 0, &h_errno);
	if (res == NULL)
	{
		errno = EAFNOSUPPORT;
		return NULL;
	}

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct hostent *)tdata->li_entry;
}

struct hostent *
gethostent(void)
{
	struct hostent *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	if (_ds_running()) 
	{
		res = ds_gethostent(&h_errno);
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(LI_files_gethostent());
		pthread_mutex_unlock(&_host_lock);
	}

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct hostent *)tdata->li_entry;
}

void
sethostent(int stayopen)
{
	if (_ds_running()) ds_sethostent();
	else LI_files_sethostent(stayopen);
}

void
endhostent(void)
{
	if (_ds_running()) ds_endhostent();
	else LI_files_endhostent();
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
getipnodebyaddr(const void *src, size_t len, int family, int *err)
{
	struct hostent *res;

	if (err != NULL) *err = 0;

	res = gethostbyaddrerrno((const char *)src, len, family, err);
	if (res == NULL) return NULL;

	if (res->h_name == NULL)
	{
		freehostent(res);
		return NULL;
	}

	return res;
}

struct hostent *
getipnodebyname(const char *name, int family, int flags, int *err)
{
	int status, want, if4, if6, add_to_cache;
	struct hostent *res;
	struct ifaddrs *ifa, *ifap;
	struct in_addr addr4;
	struct in6_addr addr6;

	memset(&addr4, 0, sizeof(struct in_addr));
	memset(&addr6, 0, sizeof(struct in6_addr));

	if (err != NULL) *err = 0;

	if (family == AF_INET)
	{
		status = inet_aton(name, &addr4);
		if (status == 1)
		{
			/* return a fake hostent */
			res = fake_hostent(name, addr4);
			return res;
		}
	}
	else if (family == AF_INET6)
	{
		status = inet_pton(family, name, &addr6);
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
				if (err != NULL) *err = HOST_NOT_FOUND;
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
		if (err != NULL) *err = NO_RECOVERY;
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
			if (err != NULL) *err = NO_RECOVERY;
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
			if (err != NULL) *err = NO_ADDRESS;
			return NULL;
		}
	}

	/*
	 * Figure out what we want.
	 * If user asked for AF_INET, we only want V4 addresses.
	 */
	want = WANT_A4_ONLY;

	if (family == AF_INET)
	{
		if ((flags & AI_ADDRCONFIG) && (if4 == 0))
		{
			if (err != NULL) *err = NO_ADDRESS;
			return NULL;
		}
	}
	else
	{
		/* family == AF_INET6 */
		want = WANT_A6_ONLY;

		if (flags & (AI_V4MAPPED | AI_V4MAPPED_CFG))
		{
			if (flags & AI_ALL)
			{
				want = WANT_A6_PLUS_MAPPED_A4;
			}
			else
			{
				want = WANT_A6_OR_MAPPED_A4_IF_NO_A6;
			}
		}
		else
		{
			if ((flags & AI_ADDRCONFIG) && (if6 == 0)) 
			{
				if (err != NULL) *err = NO_ADDRESS;
				return NULL;
			}
		}
	}

	add_to_cache = 0;
	res = cache_gethostbyname(name, want);

	if (res != NULL)
	{
	}
	else if (_ds_running())
	{
		res = ds_gethostbyname(name, want, err);
		if (res != NULL) add_to_cache = 1;
	}
	else
	{
		pthread_mutex_lock(&_host_lock);
		res = copy_host(LI_files_gethostbyname2(name, family));
		if (err != NULL) *err = h_errno;
		pthread_mutex_unlock(&_host_lock);
	}

	if (res == NULL)
	{
		if (err != NULL) *err = HOST_NOT_FOUND;
		return NULL;
	}

	if (add_to_cache == 1) cache_host(res, want, CACHE_BYNAME);

	return res;
}

static int
ether_extract_mac(kvarray_t *in, struct ether_addr *e)
{
	uint32_t d, k, kcount, t[6];
	int i;

	if (in == NULL) return -1;
	if (e == NULL) return -1;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return -1;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "mac"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			i = sscanf(in->dict[d].val[k][0], " %x:%x:%x:%x:%x:%x", &t[0], &t[1], &t[2], &t[3], &t[4], &t[5]);
			if (i != 6) return -1;
			for (i = 0; i < 6; i++) e->ether_addr_octet[i] = t[i];
			return 0;
		}
	}

	return -1;
}

/*
 * Given a host's name, this routine returns its 48 bit ethernet address.
 * Returns zero if successful, non-zero otherwise.
 */
static int
ds_ether_hostton(const char *host, struct ether_addr *e)
{
	static int proc = -1;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;

	if (host == NULL) return -1;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getmacbyname", &proc);
		if (status != KERN_SUCCESS) return -1;
	}

	request = kvbuf_query_key_val("name", host);
	if (request == NULL) return -1;

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS) return -1;

	status = ether_extract_mac(reply, e);
	kvarray_free(reply);

	return status;
}

static int
ether_extract_name(kvarray_t *in, char *name)
{
	uint32_t d, k, kcount;

	if (in == NULL) return -1;
	if (name == NULL) return -1;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return -1;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "name"))
		{
			memcpy(name, in->dict[d].val[k][0], strlen(in->dict[d].val[k][0]) + 1);
			return 0;
		}
	}

	return -1;
}

/*
 * Given a 48 bit ethernet address, this routine return its host name.
 * Returns zero if successful, non-zero otherwise.
 */
static int
ds_ether_ntohost(char *host, const struct ether_addr *e)
{
	uint32_t i, x[6];
	char str[256];
	static int proc = -1;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;

	if (host == NULL) return -1;
	if (e == NULL) return -1;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbymac", &proc);
		if (status != KERN_SUCCESS) return -1;
	}

	for (i = 0; i < 6; i++) x[i] = e->ether_addr_octet[i];
	sprintf(str, "%x:%x:%x:%x:%x:%x", x[0], x[1], x[2], x[3], x[4], x[5]);

	request = kvbuf_query_key_val("mac", str);
	if (request == NULL) return -1;

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS) return -1;

	status = ether_extract_name(reply, host);
	kvarray_free(reply);

	return status;
}

int
ether_hostton(const char *host, struct ether_addr *e)
{
	if (_ds_running()) return ds_ether_hostton(host, e);
	return _old_ether_hostton(host, e);
}

int
ether_ntohost(char *host, const struct ether_addr *e)
{
	if (_ds_running()) return ds_ether_ntohost(host, e);
	return _old_ether_ntohost(host, e);
}
