/*
 * Copyright (c) 2008 Apple, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <dns.h>
#include <dns_util.h>
#include <resolv.h>
#include <nameser.h>
#include <pthread.h>
#include <si_module.h>
#include <ils.h>
#include <dns_private.h>

/* from dns_util.c */
#define DNS_MAX_RECEIVE_SIZE 65536

#define MDNS_HANDLE_NAME "*MDNS*"

#define DNS_HANDLE_BUSY  0x00000001
#define MDNS_HANDLE_BUSY 0x00000002

#define MODULE_DNS 0
#define MODULE_MDNS 1

static pthread_mutex_t dns_plugin_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
	struct hostent host;
	int alias_count;
	int addr_count;
	uint64_t ttl;
} dns_build_hostent_t;

typedef struct
{
	dns_handle_t dns;
	dns_handle_t mdns;
	uint32_t flags;
} dns_plugin_private_t;

#define DNS_FLAGS_RCODE_MASK 0x000f
static const char hexchar[] = "0123456789abcdef";

static dns_handle_t
dns_checkout_handle(si_mod_t *si)
{
	dns_plugin_private_t *pp;
	dns_handle_t dns;

	if (si == NULL) return NULL;

	pthread_mutex_lock(&dns_plugin_lock);
	if (si->private == NULL)
	{
		pp = (dns_plugin_private_t *)calloc(1, sizeof(dns_plugin_private_t));
		si->private = pp;
	}

	dns = NULL;
	pp = (dns_plugin_private_t *)si->private;

	if (pp == NULL)
	{
		/* shouldn't happen */
		dns = dns_open(NULL);
		pthread_mutex_unlock(&dns_plugin_lock);
		return dns;
	}

	if ((pp->flags & DNS_HANDLE_BUSY) == 0)
	{
		if (pp->dns == NULL) pp->dns = dns_open(NULL);
		pp->flags |= DNS_HANDLE_BUSY;
		pthread_mutex_unlock(&dns_plugin_lock);
		return pp->dns;
	}

	/* main dns handle is busy - create a temporary one */
	dns = dns_open(NULL);
	pthread_mutex_unlock(&dns_plugin_lock);
	return dns;
}

static dns_handle_t
mdns_checkout_handle(si_mod_t *si)
{
	dns_plugin_private_t *pp;
	dns_handle_t dns;

	if (si == NULL) return NULL;

	pthread_mutex_lock(&dns_plugin_lock);
	if (si->private == NULL)
	{
		pp = (dns_plugin_private_t *)calloc(1, sizeof(dns_plugin_private_t));
		si->private = pp;
	}

	dns = NULL;
	pp = (dns_plugin_private_t *)si->private;

	if (pp == NULL)
	{
		/* shouldn't happen */
		dns = dns_open(MDNS_HANDLE_NAME);
		pthread_mutex_unlock(&dns_plugin_lock);
		return dns;
	}

	if ((pp->flags & MDNS_HANDLE_BUSY) == 0)
	{
		if (pp->mdns == NULL) pp->mdns = dns_open(MDNS_HANDLE_NAME);
		pp->flags |= MDNS_HANDLE_BUSY;
		pthread_mutex_unlock(&dns_plugin_lock);
		return pp->mdns;
	}

	/* main mdns handle is busy - create a temporary one */
	dns = dns_open(MDNS_HANDLE_NAME);
	pthread_mutex_unlock(&dns_plugin_lock);
	return dns;
}

static void
dns_checkin_handle(si_mod_t *si, dns_handle_t dns)
{
	dns_plugin_private_t *pp;

	if (si == NULL) return;
	if (dns == NULL) return;

	pthread_mutex_lock(&dns_plugin_lock);
	if (si->private == NULL)
	{
		/* shouldn't happen */
		pp = (dns_plugin_private_t *)calloc(1, sizeof(dns_plugin_private_t));
		si->private = pp;
	}

	pp = (dns_plugin_private_t *)si->private;

	if (pp == NULL)
	{
		/* shouldn't happen */
		dns_free(dns);
		pthread_mutex_unlock(&dns_plugin_lock);
		return;
	}

	if (pp->dns == dns)
	{
		pp->flags &= ~DNS_HANDLE_BUSY;
		pthread_mutex_unlock(&dns_plugin_lock);
		return;
	}
	else if (pp->mdns == dns)
	{
		pp->flags &= ~MDNS_HANDLE_BUSY;
		pthread_mutex_unlock(&dns_plugin_lock);
		return;
	}

	dns_free(dns);
	pthread_mutex_unlock(&dns_plugin_lock);
}

static char *
dns_reverse_ipv4(const char *addr)
{
	union
	{
		uint32_t a;
		unsigned char b[4];
	} ab;
	char *p;

	if (addr == NULL) return NULL;

	memcpy(&(ab.a), addr, 4);

	asprintf(&p, "%u.%u.%u.%u.in-addr.arpa.", ab.b[3], ab.b[2], ab.b[1], ab.b[0]);
	return p;
}

static char *
dns_reverse_ipv6(const char *addr)
{
	char x[65], *p;
	int i, j;
	u_int8_t d, hi, lo;

	if (addr == NULL) return NULL;

	x[64] = '\0';
	j = 63;
	for (i = 0; i < 16; i++)
	{
		d = addr[i];
		lo = d & 0x0f;
		hi = d >> 4;
		x[j--] = '.';
		x[j--] = hexchar[hi];
		x[j--] = '.';
		x[j--] = hexchar[lo];
	}

	p = calloc(1, 75);
	if (p == NULL) return NULL;

	memmove(p, x, 64);
	strcat(p, "ip6.arpa.");

	return p;
}

static char *
dns_lower_case(const char *s)
{
	int i;
	char *t;

	if (s == NULL) return NULL;
	t = malloc(strlen(s) + 1);

	for (i = 0; s[i] != '\0'; i++) 
	{
		if ((s[i] >= 'A') && (s[i] <= 'Z')) t[i] = s[i] + 32;
		else t[i] = s[i];
	}
	t[i] = '\0';
	return t;
}

static int
dns_host_merge_alias(const char *name, dns_build_hostent_t *h)
{
	int i;

	if (name == NULL) return 0;
	if (h == NULL) return 0;

	if ((h->host.h_name != NULL) && (!strcmp(name, h->host.h_name))) return 0;
	for (i = 0; i < h->alias_count; i++) if (!strcmp(name, h->host.h_aliases[i])) return 0;

	h->host.h_aliases = (char **)reallocf(h->host.h_aliases, (h->alias_count + 2) * sizeof(char *));
	if (h->host.h_aliases == NULL)
	{
		h->alias_count = 0;
		return -1;
	}

	h->host.h_aliases[h->alias_count] = dns_lower_case(name);
	h->alias_count++;
	h->host.h_aliases[h->alias_count] = NULL;

	return 0;
}

static int
dns_host_append_addr(const char *addr, uint32_t len, dns_build_hostent_t *h)
{
	if (addr == NULL) return 0;
	if (h == NULL) return 0;

	if (h->addr_count == 0) h->host.h_addr_list = (char **)calloc(2, sizeof(char *));
	else h->host.h_addr_list = (char **)reallocf(h->host.h_addr_list, (h->addr_count + 2) * sizeof(char *));

	if (h->host.h_addr_list == NULL)
	{
		h->addr_count = 0;
		return -1;
	}

	h->host.h_addr_list[h->addr_count] = malloc(len);
	if (h->host.h_addr_list[h->addr_count] == NULL) return -1;

	memcpy(h->host.h_addr_list[h->addr_count], addr, len);
	h->addr_count++;
	h->host.h_addr_list[h->addr_count] = NULL;

	return 0;
}

static void
dns_plugin_clear_host(dns_build_hostent_t *h)
{
	uint32_t i;
	char **aliases;

	if (h == NULL) return;

	if (h->host.h_name != NULL) free(h->host.h_name);
	h->host.h_name = NULL;

	aliases = h->host.h_aliases;
	if (aliases != NULL)
	{
		while (*aliases != NULL) free(*aliases++);
		free(h->host.h_aliases);
	}

	h->host.h_aliases = NULL;

	if (h->host.h_addr_list != NULL)
	{
		for (i = 0; h->host.h_addr_list[i] != NULL; i++) free(h->host.h_addr_list[i]);
		free(h->host.h_addr_list);
	}

	h->host.h_addr_list = NULL;
}

static int
dns_reply_to_hostent(dns_reply_t *r, int af, const char *addr, dns_build_hostent_t *h)
{
	int i, got_data, got_addr;
	int ptrx, cnamex;

	if (r == NULL) return -1;
	if (r->status != DNS_STATUS_OK) return -1;
	if ((r->header->flags & DNS_FLAGS_RCODE_MASK) != ns_r_noerror) return -1;

	if (r == NULL) return -1;
	if (r->header == NULL) return -1;
	if (r->header->ancount == 0) return -1;

	got_data = 0;
	got_addr = 0;
	ptrx = -1;
	cnamex = -1;

	for (i = 0; i < r->header->ancount; i++)
	{
		if ((af == AF_INET) && (r->answer[i]->dnstype == ns_t_a))
		{
			got_data++;
			got_addr++;
			dns_host_append_addr((const char *)&(r->answer[i]->data.A->addr), 4, h);
			if (h->ttl == 0) h->ttl = r->answer[i]->ttl;
			else if (r->answer[i]->ttl < h->ttl) h->ttl = r->answer[i]->ttl;
		}

		else if ((af == AF_INET6) && (r->answer[i]->dnstype == ns_t_aaaa))
		{
			got_data++;
			got_addr++;
			dns_host_append_addr((const char *)&(r->answer[i]->data.AAAA->addr), 16, h);
			if (h->ttl == 0) h->ttl = r->answer[i]->ttl;
			else if (r->answer[i]->ttl < h->ttl) h->ttl = r->answer[i]->ttl;
		}

		else if (r->answer[i]->dnstype == ns_t_cname)
		{
			got_data++;
			if (cnamex == -1) cnamex = i;
			if (h->ttl == 0) h->ttl = r->answer[i]->ttl;
			else if (r->answer[i]->ttl < h->ttl) h->ttl = r->answer[i]->ttl;
		}

		else if (r->answer[i]->dnstype == ns_t_ptr)
		{
			got_data++;
			if (ptrx == -1) ptrx = i;
			if (h->ttl == 0) h->ttl = r->answer[i]->ttl;
			else if (r->answer[i]->ttl < h->ttl) h->ttl = r->answer[i]->ttl;
		}
	}

	if (addr != NULL)
	{
		if (af == AF_INET)
		{
			got_addr++;
			dns_host_append_addr(addr, 4, h);
		}
		else if (af == AF_INET6)
		{
			got_addr++;
			dns_host_append_addr(addr, 16, h);
		}
	}

	if (got_data == 0) return -1;
	if (got_addr == 0) return -1;

	h->host.h_addrtype = af;
	if (af == AF_INET) h->host.h_length = 4;
	else h->host.h_length = 16;

	if (ptrx != -1)
	{
		/* use name from PTR record */
		h->host.h_name = dns_lower_case(r->answer[ptrx]->data.PTR->name);
	}
	else if (cnamex != -1)
	{
		/* use name from CNAME record */
		h->host.h_name = dns_lower_case(r->answer[cnamex]->data.CNAME->name);
	}
	else
	{
		/* use name in first answer */
		h->host.h_name = dns_lower_case(r->answer[0]->name);
	}

	for (i = 0; i < r->header->ancount; i++)
	{
		if (r->answer[i]->dnstype == ns_t_cname)
		{
			dns_host_merge_alias(r->answer[cnamex]->data.CNAME->name, h);
			dns_host_merge_alias(r->answer[cnamex]->name, h);
		}
	}

	if (h->alias_count == 0) h->host.h_aliases = (char **)calloc(1, sizeof(char *));

	return 0;
}

static si_item_t *
_internal_host_byname(si_mod_t *si, const char *name, int af, const char *ignored, uint32_t *err, int which)
{
	uint32_t type;
	dns_reply_t *r;
	dns_build_hostent_t h;
	si_item_t *out;
	dns_handle_t dns;
	uint64_t bb;
	int status;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if (name == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	if (af == AF_INET) type = ns_t_a;
	else if (af == AF_INET6) type = ns_t_aaaa;
	else
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	r = NULL;
	dns = NULL;

	if (which == MODULE_DNS) dns = dns_checkout_handle(si);
	else if (which == MODULE_MDNS) dns = mdns_checkout_handle(si);

	if (dns == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	r = dns_lookup(dns, name, ns_c_in, type);
	dns_checkin_handle(si, dns);

	if (r == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
		return NULL;
	}

	memset(&h, 0, sizeof(dns_build_hostent_t));

	status = dns_reply_to_hostent(r, af, NULL, &h);
	dns_free_reply(r);

	if (status < 0)
	{
		dns_plugin_clear_host(&h);
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	bb = h.ttl + time(NULL);

	if (af == AF_INET)
	{
		out = (si_item_t *)LI_ils_create("L4488s*44a", (unsigned long)si, CATEGORY_HOST_IPV4, 1, bb, 0LL, h.host.h_name, h.host.h_aliases, af, h.host.h_length, h.host.h_addr_list);
	}
	else
	{
		out = (si_item_t *)LI_ils_create("L4488s*44c", (unsigned long)si, CATEGORY_HOST_IPV6, 1, bb, 0LL, h.host.h_name, h.host.h_aliases, af, h.host.h_length, h.host.h_addr_list);
	}

	dns_plugin_clear_host(&h);

	if ((out == NULL) && (err != NULL)) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;

	return out;
}

si_item_t *
dns_host_byname(si_mod_t *si, const char *name, int af, const char *ignored, uint32_t *err)
{
	return _internal_host_byname(si, name, af, NULL, err, MODULE_DNS);
}

si_item_t *
mdns_host_byname(si_mod_t *si, const char *name, int af, const char *ignored, uint32_t *err)
{
	return _internal_host_byname(si, name, af, NULL, err, MODULE_MDNS);
}

static si_item_t *
_internal_host_byaddr(si_mod_t *si, const void *addr, int af, const char *ignored, uint32_t *err, int which)
{
	uint32_t type;
	dns_reply_t *r;
	dns_build_hostent_t h;
	char *name;
	si_item_t *out;
	dns_handle_t dns;
	socklen_t len;
	uint64_t bb;
	int cat;
	struct sockaddr_storage from;
	uint32_t fromlen;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if (addr == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	if (si == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	fromlen = sizeof(struct sockaddr_storage);
	memset(&from, 0, fromlen);

	name = NULL;
	type = ns_t_ptr;

	len = 0;
	cat = -1;

	if (af == AF_INET)
	{
		len = 4;
		name = dns_reverse_ipv4(addr);
		cat = CATEGORY_HOST_IPV4;
	}
	else if (af == AF_INET6)
	{
		len = 16;
		name = dns_reverse_ipv6(addr);
		cat = CATEGORY_HOST_IPV6;
	}
	else 
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	r = NULL;
	dns = NULL;

	if (which == MODULE_DNS) dns = dns_checkout_handle(si);
	else if (which == MODULE_MDNS) dns = mdns_checkout_handle(si);

	if (dns == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	r = dns_lookup(dns, name, ns_c_in, type);
	dns_checkin_handle(si, dns);

	free(name);
	if (r == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
		return NULL;
	}

	memset(&h, 0, sizeof(dns_build_hostent_t));

	if (dns_reply_to_hostent(r, af, addr, &h) < 0)
	{
		dns_plugin_clear_host(&h);
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	bb = h.ttl + time(NULL);
	out = (si_item_t *)LI_ils_create("L4488s*44a", (unsigned long)si, cat, 1, bb, 0LL, h.host.h_name, h.host.h_aliases, af, h.host.h_length, h.host.h_addr_list);

	dns_plugin_clear_host(&h);

	if ((out == NULL) && (err != NULL)) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;

	return out;
}

si_item_t *
dns_host_byaddr(si_mod_t *si, const void *addr, int af, const char *ignored, uint32_t *err)
{
	return _internal_host_byaddr(si, addr, af, NULL, err, MODULE_DNS);
}

si_item_t *
mdns_host_byaddr(si_mod_t *si, const void *addr, int af, const char *ignored, uint32_t *err)
{
	return _internal_host_byaddr(si, addr, af, NULL, err, MODULE_MDNS);
}

/*
 * We support dns_async_start / cancel / handle_reply using dns_item_call
 */
static si_item_t *
_internal_item_call(si_mod_t *si, int call, const char *name, const char *ignored1, const char *ignored2, uint32_t class, uint32_t type, uint32_t *err, int which)
{
	dns_handle_t dns;
	char buf[DNS_MAX_RECEIVE_SIZE];
	int len;
	struct sockaddr_storage from;
	uint32_t fromlen;
	si_item_t *out;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;
	if ((call != SI_CALL_DNS_QUERY) && (call != SI_CALL_DNS_SEARCH))
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	if (name == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	dns = NULL;

	if (which == MODULE_DNS) dns = dns_checkout_handle(si);
	else if (which == MODULE_MDNS) dns = mdns_checkout_handle(si);

	if (dns == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	fromlen = sizeof(struct sockaddr_storage);
	memset(&from, 0, fromlen);

	len = 0;
	if (call == SI_CALL_DNS_QUERY) len = dns_query(dns, name, class, type, buf, sizeof(buf), (struct sockaddr *)&from, &fromlen);
	else if (call == SI_CALL_DNS_SEARCH) len = dns_search(dns, name, class, type, buf, sizeof(buf), (struct sockaddr *)&from, &fromlen);

	dns_checkin_handle(si, dns);

	if ((len <= 0) || (len > DNS_MAX_RECEIVE_SIZE))
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
		return NULL;
	}

	out = (si_item_t *)LI_ils_create("L4488@@", (unsigned long)si, CATEGORY_DNSPACKET, 1, 0LL, 0LL, len, buf, fromlen, &from);
	if ((out == NULL) && (err != NULL)) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;

	return out;
}

si_item_t *
dns_item_call(si_mod_t *si, int call, const char *name, const char *ignored1, const char *ignored2, uint32_t class, uint32_t type, uint32_t *err)
{
	return _internal_item_call(si, call, name, ignored1, ignored2, class, type, err, MODULE_DNS);
}

si_item_t *
mdns_item_call(si_mod_t *si, int call, const char *name, const char *ignored1, const char *ignored2, uint32_t class, uint32_t type, uint32_t *err)
{
	return _internal_item_call(si, call, name, ignored1, ignored2, class, type, err, MODULE_MDNS);
}

int
dns_is_valid(si_mod_t *si, si_item_t *item)
{
	uint64_t now;
	si_mod_t *src;

	if (si == NULL) return 0;
	if (item == NULL) return 0;
	if (si->name == NULL) return 0;
	if (item->src == NULL) return 0;

	src = (si_mod_t *)item->src;

	if (src->name == NULL) return 0;
	if (string_not_equal(si->name, src->name)) return 0;

	now = time(NULL);
	if (item->validation_a < now) return 0;
	return 1;
}

int
mdns_is_valid(si_mod_t *si, si_item_t *item)
{
	return 0;
}

void
dns_close(si_mod_t *si)
{
	dns_plugin_private_t *pp;

	if (si == NULL) return;

	pp = (dns_plugin_private_t *)si->private;
	if (pp == NULL) return;

	if (pp->dns != NULL) dns_free(pp->dns);
	free(si->private);
}

int
dns_init(si_mod_t *si)
{
	if (si == NULL) return 1;

	si->vers = 1;

	si->sim_close = dns_close;
	si->sim_is_valid = dns_is_valid;
	si->sim_host_byname = dns_host_byname;
	si->sim_host_byaddr = dns_host_byaddr;
	si->sim_item_call = dns_item_call;

	return 0;
}

void
mdns_close(si_mod_t *si)
{
	dns_close(si);
}

int
mdns_init(si_mod_t *si)
{
	if (si == NULL) return 1;

	si->vers = 1;

	si->sim_close = dns_close;
	si->sim_is_valid = mdns_is_valid;
	si->sim_host_byname = mdns_host_byname;
	si->sim_host_byaddr = mdns_host_byaddr;
	si->sim_item_call = mdns_item_call;

	return 0;
}
