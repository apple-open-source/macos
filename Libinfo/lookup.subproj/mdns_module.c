/*
 * Copyright (c) 2008-2009 Apple Inc. All rights reserved.
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
/*
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
/*
 * Copyright (c) 1988, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "ils.h"
#include "netdb.h"
#include "si_module.h"

#include <assert.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include <libkern/OSAtomic.h>
#include <netinet/in.h>
#include <ctype.h>
#include <dns_sd.h>
#include <dnsinfo.h>
#include <errno.h>
#include <nameser.h>
#include <notify.h>
#include <pthread.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <time.h>
#include <unistd.h>
#include <asl.h>
#include <dns.h>
#include <dns_util.h>
#include <TargetConditionals.h>

/* from dns_util.c */
#define DNS_MAX_RECEIVE_SIZE 65536

#define INET_NTOP_AF_INET_OFFSET 4
#define INET_NTOP_AF_INET6_OFFSET 8

#define IPPROTO_UNSPEC 0

static int _mdns_debug = 0;

// mutex protects DNSServiceProcessResult and DNSServiceRefDeallocate
static pthread_mutex_t _mdns_mutex = PTHREAD_MUTEX_INITIALIZER;

// XXX
// Options: timeout:n total_timeout attempts

/* _dns_config_token: notify token indicating dns config needs refresh
 */
static int _dns_config_token = -1;

typedef struct {
	int32_t rc;
	dns_config_t *dns;
	dns_resolver_t *primary;
	uint32_t n_defaults;
	dns_resolver_t **defaults;
	char **search_list;
	int ndots;
} mdns_config_t;

typedef struct {
	uint16_t priority;
	uint16_t weight;
	uint16_t port;
	uint8_t target[0];
} mdns_rr_srv_t;

typedef struct mdns_srv_t mdns_srv_t;
struct mdns_srv_t {
	si_srv_t srv;
	mdns_srv_t *next;
};

typedef struct {
	struct hostent host;
	int alias_count;
	int addr_count;
} mdns_hostent_t;

typedef struct {
	mdns_hostent_t *h4;
	mdns_hostent_t *h6;
	mdns_srv_t *srv;
	uint64_t ttl;
	uint32_t ifnum;
} mdns_reply_t;

static uint32_t _mdns_generation = 0;
DNSServiceRef _mdns_sdref;
DNSServiceRef _mdns_old_sdref;

static int _mdns_query_mDNSResponder(const char *name, int class, int type,
	const char *interface, DNSServiceFlags flags,
	uint8_t *answer, uint32_t *anslen,
	mdns_reply_t *reply, uint32_t timeout);

static int _mdns_resolver_get_option(dns_resolver_t *resolver, const char* option);
static void _mdns_hostent_clear(mdns_hostent_t *h);
static void _mdns_reply_clear(mdns_reply_t *r);

static const char hexchar[] = "0123456789abcdef";

#define BILLION 1000000000

/* length of a reverse DNS IPv6 address query name, e.g. "9.4.a.f.c.e.e.f.e.e.1.5.4.1.4.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.e.f.ip6.arpa" */
#define IPv6_REVERSE_LEN 72

/* index of the trailing char that must be "8", "9", "A", "a", "b", or "B" */
#define IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR 58

/* index of low-order nibble of embedded scope id */
#define IPv6_REVERSE_LINK_LOCAL_SCOPE_ID_LOW 48

const static uint8_t hexval[128] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/*  0 - 15 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 16 - 31 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 32 - 47 */
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,		/* 48 - 63 */
	0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 64 - 79 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 80 - 95 */
	0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,		/* 96 - 111 */
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0		/* 112 - 127 */
};

/*
 * _mdns_create_search_list
 * Creates a NULL terminated array of strings from the specied resolver's
 * search list, or from the components of the specified resolver's domain
 * if search list is empty.
 * Free the list and elements with free(3) when done.
 */
static char **
_mdns_create_search_list(dns_resolver_t *resolver)
{
	int n, m;
	char *p, *domain;
	char **list;

	if (resolver == NULL) return NULL;

	// return the search list if present
	if (resolver->n_search > 0) {
		list = (char **)calloc(resolver->n_search+1, sizeof(char *));
		if (list == NULL) return NULL;
		for (n = 0; n < resolver->n_search; ++n) {
			list[n] = strdup(resolver->search[n]);
		}
		return list;
	}

	if (resolver->domain == NULL) return NULL;
	domain = strdup(resolver->domain);
	if (domain == NULL) return NULL;

	// count dots
	n = 0;
	for (p = domain; *p != '\0'; p++) {
		if (*p == '.') n++;
	}

	// trim trailing dots
	for (p--; (p >= domain) && (*p == '.'); p--) {
		*p = '\0';
		n--;
	}

	// make sure the resulting string is not empty
	if (p < domain) {
		free(domain);
		return NULL;
	}

	// dots are separators, so number of components is one larger
	n++;

	m = 0;
	list = (char **)calloc(n+1, sizeof(char *));
	if (list == NULL) return NULL;
	// first item in list is domain itself
	list[m++] = domain;

	// include parent domains with at least LOCALDOMAINPARTS components
	p = domain;
	while (n > LOCALDOMAINPARTS) {
		// find next component
		while ((*p != '.') && (*p != '\0')) p++;
		if (*p == '\0') break;
		p++;
		// add to the list
		n--;
		list[m++] = strdup(p);
	}
	return list;
}

/* _mdns_resolver_get_option
 * Determines whether the specified option is present in the resolver.
 */
static int
_mdns_resolver_get_option(dns_resolver_t *resolver, const char* option)
{
	if (resolver == NULL) return 0;
	int len = strlen(option);
	char *options = resolver->options;
	if (options == NULL) return 0;
	// look for "(^| )option( |:|$)"
	char *ptr = strstr(options, option);
	while (ptr) {
		if (ptr == options || ptr[-1] == ' ') {
			if (ptr[len] == ' ' || ptr[len] == 0) {
				return 1;
			} else if (ptr[len] == ':') {
				return strtol(&ptr[len+1], NULL, 10);
			}
		}
		ptr = strstr(ptr, option);
	}
	return 0;
}

/* _mdns_compare_resolvers
 * Compares two dns_resolver_t pointers by search order ascending.
 */
static int
_mdns_compare_resolvers(const void *a, const void *b) 
{
	dns_resolver_t **x = (dns_resolver_t **)a, **y = (dns_resolver_t **)b;
	return ((*x)->search_order - (*y)->search_order);
}

/* _mdns_create_default_resolvers_list
 * Returns an array of dns_resolver_t containing only default resolvers.
 * A resolver is a default resolver if it is the primary resolver or if it
 * contains the "default" configuration option.
 */
static void
_mdns_config_init_default_resolvers(mdns_config_t *config)
{
	uint32_t count = config->dns->n_resolver;
	if (count == 0) return;
	config->defaults = calloc(count, sizeof(dns_resolver_t *));
	if (config->defaults == NULL) return;
	int m = 0, i, j;
	if (config->primary) config->defaults[m++] = config->primary;
	// iterate the resolvers, add any default resolvers that are not
	// already in the list.
	for (i = 0; i < count; ++i) {
		dns_resolver_t *resolver = config->dns->resolver[i];
		if (_mdns_resolver_get_option(resolver, "default")) {
			int exists = 0;
			for (j = 0; j < m; ++j) {
				if (config->defaults[j] == resolver) {
					exists = 1;
					break;
				}
			}
			if (!exists) {
				config->defaults[m++] = resolver;
			}
		}
	}
	config->n_defaults = m;
	// sort list by search order ascending
	qsort(config->defaults, config->n_defaults, sizeof(dns_resolver_t *), _mdns_compare_resolvers);
}

#if 0
static void
_mdns_print_dns_resolver(dns_resolver_t *resolver)
{
	printf("resolver = {\n");
	printf("\tdomain = %s\n", resolver->domain);
	int j;
	for (j = 0; j < resolver->n_nameserver; ++j) {
		int res;
		char host[255], serv[255];
		res = getnameinfo(resolver->nameserver[j], resolver->nameserver[j]->sa_len, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);
		if (res == 0) {
			printf("\tnameserver[%d] = %s:%s\n", j, host, serv);
		} else {
			printf("\tnameserver[%d] = %s\n", j, gai_strerror(res));
		}
	}
	printf("\tport = %d\n", resolver->port);
	for (j = 0; j < resolver->n_search; ++j) {
		printf("\tsearch[%d] = %s\n", j, resolver->search[j]);
	}
	// sortaddr
	printf("\tn_sortaddr = %d\n", resolver->n_sortaddr);
	// options
	printf("\toptions = %s\n", resolver->options);
	printf("\ttimeout = %d\n", resolver->timeout);
	printf("\tsearch_order = %d\n", resolver->search_order);
	printf("}\n");
}

static void
_mdns_print_dns_config(dns_config_t *config)
{
	int i;
	dns_resolver_t **list = _mdns_create_sorted_resolver_list(config);
	dns_resolver_t **ptr = list;
	while (*ptr) {
		_mdns_print_dns_resolver(*ptr);
		ptr++;
	}
	free(list);
}

static void
_mdns_print_hostent(mdns_hostent_t* h)
{
	if (h == NULL) return;
	printf("hostent[%p] = {\n", h);
	printf("\thost = {\n");
	printf("\t\th_name = %s\n", h->host.h_name);
	printf("\t\th_length = %d\n", h->host.h_length);
	printf("\t\th_addrtype = %d\n", h->host.h_addrtype);
	char **alias = h->host.h_aliases;
	while (alias && *alias) {
		printf("\t\th_aliases = %s\n", *alias++);
	}
	char **addr = h->host.h_addr_list;
	while (addr && *addr) {
		printf("\t\th_addr_list = %x\n", ntohl(*(uint32_t*)*addr++));
	}
	printf("\t}\n");
	printf("\talias_count = %d\n", h->alias_count);
	printf("\taddr_count = %d\n", h->addr_count);
	printf("}\n");
}

#endif

/* _mdns_config_retain
 * Retain the mdns configuration.
 */
static mdns_config_t *
_mdns_config_retain(mdns_config_t *config)
{
	int32_t rc;

	if (config == NULL) return NULL;
	rc = OSAtomicIncrement32Barrier(&config->rc);
	assert(rc > 1);
	return config;
}

/* _mdns_config_release
 * Releases the mdns configuration structure and
 * frees the data if no references remain.
 */
static void
_mdns_config_release(mdns_config_t *config)
{
	int32_t rc;

	if (config == NULL) return;
	rc = OSAtomicDecrement32Barrier(&config->rc);
	assert(rc >= 0);
	if (rc == 0) {
		if (config->dns) dns_configuration_free(config->dns);
		free(config->defaults);
		char **p = config->search_list;
		while (p && *p) { free(*p++); }
		free(config->search_list);
		free(config);
	}
}

/* _mdns_copy_system_config
 * Retrieves DNS configuration from SystemConfiguration.framework.
 * Checks notify notification to determine whether configuration is in need
 * of a refresh.
 */
static mdns_config_t *
_mdns_copy_system_config(void)
{
	// first call needs refresh
	static mdns_config_t *current_config;
	mdns_config_t *config = NULL;
	int refresh = 1;
	int res;

	pthread_mutex_lock(&_mdns_mutex);

	// check whether the global configuration has changed
	if (_dns_config_token == -1) {
		res = notify_register_check(dns_configuration_notify_key(), &_dns_config_token);
		if (res != NOTIFY_STATUS_OK) _dns_config_token = -1;
	}

	if (_dns_config_token != -1) {
		res = notify_check(_dns_config_token, &refresh);
		if (res != NOTIFY_STATUS_OK) refresh = 1;
	}

	// return the current configuration if still valid
	if (refresh == 0) {
		mdns_config_t *config = _mdns_config_retain(current_config);
		pthread_mutex_unlock(&_mdns_mutex);
		return config;
	}

	// need to allocate a new configuration

	config = calloc(1, sizeof(mdns_config_t));
	if (config != NULL) config->dns = dns_configuration_copy();

	// failed to get new config, return previous config
	if (config == NULL || config->dns == NULL) {
		free(config);
		config = _mdns_config_retain(current_config);
		pthread_mutex_unlock(&_mdns_mutex);
		return config;
	}

	config->rc = 1;
	if (config->dns->n_resolver > 0) {
		// primary resolver is always index 0 and contains the
		// search domains
		config->primary = config->dns->resolver[0];
		config->search_list = _mdns_create_search_list(config->primary);
		_mdns_config_init_default_resolvers(config);
	}
	config->ndots = _mdns_resolver_get_option(config->primary, "ndots");

	// promote the new configuration to current
	_mdns_config_release(current_config);
	current_config = config;

	// return the new configuration
	config = _mdns_config_retain(config);
	pthread_mutex_unlock(&_mdns_mutex);
	return config;
}

/* _mdns_timeout_for_name
 * Returns the appropriate timeout for the specified name based on the
 * sum of the timeouts of all resolvers that match the name.
 */
static uint32_t
_mdns_timeout_for_name(mdns_config_t *config, const char *name)
{
	int i;
	uint32_t timeout = 0;

	if (name == NULL) return 0;

	// use strncasecmp to ignore a trailing '.' in name
	int len = strlen(name);
	if ((len - 1) >= 0 && name[len-1] == '.') --len;

	const char *p = name;
	while (len > 0) {
		uint32_t count = config->dns->n_resolver;
		for (i = 0; i < count; ++i) {
			dns_resolver_t *resolver = config->dns->resolver[i];
			if (resolver->domain == NULL) continue;
			if (strncasecmp(resolver->domain, p, len) == 0) {
				timeout += resolver->timeout;
			}
		}
		// discard the current label
		while (len > 0) {
			++p;
			--len;
			if (p[-1] == '.') break;
		}
	}
	return timeout;
}

/* _mdns_query_unqualified
 * Performs a query for the name as an unqualified name (appends each
 * of the default resolver's domains).
 */
static int
_mdns_query_unqualified(mdns_config_t *config, const char *name, uint32_t class, uint32_t type, const char *interface, DNSServiceFlags flags, uint8_t *buf, uint32_t *len, mdns_reply_t *reply)
{
	int i, res = -1;

	for (i = 0; i < config->n_defaults; ++i) {
		dns_resolver_t *resolver = config->defaults[i];
		char *qname;

		asprintf(&qname, "%s.%s", name, resolver->domain ? resolver->domain : "");
		res = _mdns_query_mDNSResponder(qname, class, type, interface, flags, buf, len, reply, resolver->timeout);
		free(qname);

		if (res == 0) break;
		else _mdns_reply_clear(reply);
	}
	return res;
}

/* _mdns_query_absolute
 * Performs a query for the name as an absolute name (does not qualify with any
 * additional domains).
 */
static int
_mdns_query_absolute(mdns_config_t *config, const char *name, uint32_t class, uint32_t type, const char *interface, DNSServiceFlags flags, uint32_t fqdn, uint8_t *buf, uint32_t *len, mdns_reply_t *reply)
{
	int res = -1;
	char *qname = (char *)name;

	uint32_t timeout = _mdns_timeout_for_name(config, name);

	if (fqdn == 0) asprintf(&qname, "%s.", name);
	res = _mdns_query_mDNSResponder(qname, class, type, interface, flags, buf, len, reply, timeout);
	if (fqdn == 0) free(qname);
	if (res != 0) _mdns_reply_clear(reply);
	return res;
}

static int
_mdns_search(const char *name, uint32_t class, uint32_t type, const char *interface, DNSServiceFlags flags, uint32_t fqdn, uint32_t recurse, uint8_t *buf, uint32_t *len, mdns_reply_t *reply)
{
	int res = -1;
	int i, n, ndots;
	char *dot;

	if (name == NULL) return -1;

	mdns_config_t *config = _mdns_copy_system_config();
	if (config == NULL) return -1;

	// NDOTS is the threshold for trying a qualified name "as is"
	ndots = config->ndots;
	if (ndots == 0) ndots = 1;

	// count the dots, and remember position of the last one
	n = 0;
	dot = NULL;
	for (i = 0; name[i] != '\0'; i++) {
		if (name[i] == '.') {
			n++;
			dot = (char *)(name + i);
		}
	}
	// FQDN has dot for last character
	if (fqdn == 0 && dot != NULL && dot[1] == '\0') fqdn = 1;

	// if the name has at least ndots, try first as an absolute query.
	// FQDN and PTR queries are always absolute.
	if (n >= ndots || fqdn == 1 || type == ns_t_ptr) {
		res = _mdns_query_absolute(config, name, class, type, interface, flags, fqdn, buf, len, reply);
		if (res == 0) {
			_mdns_config_release(config);
			return res;
		}
	}

	// stop if FQDN, PTR, or no recursion requested
	if (fqdn == 1 || type == ns_t_ptr || recurse == 0) {
		_mdns_config_release(config);
		return -1;
	}

	// Qualify the name with each of the search domains looking for a match.
	char **search = config->search_list;
	if (search != NULL) {
		res = -1;
		for (i = 0; i < MAXDNSRCH && search[i] != NULL; ++i) {
			char *qname;
			asprintf(&qname, "%s.%s", name, search[i]);
			res = _mdns_search(qname, class, type, interface, flags, 0, 0, buf, len, reply);
			free(qname);
			if (res == 0) break;
		}
	} else {
		// The name is not fully qualified and there is no search list.
		// Try each default resolver, qualifying the name with that
		// resolver's domain.
		res = _mdns_query_unqualified(config, name, class, type, interface, flags, buf, len, reply);
	}
	_mdns_config_release(config);
	return res;
}

static char *
_mdns_reverse_ipv4(const char *addr)
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
_mdns_reverse_ipv6(const char *addr)
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

	asprintf(&p, "%sip6.arpa.", x);

	return p;
}

/* _mdns_canonicalize
 * Canonicalize the domain name by converting to lower case and removing the
 * trailing '.' if present.
 */
static char *
_mdns_canonicalize(const char *s)
{
	int i;
	char *t;
	if (s == NULL) return NULL;
	t = strdup(s);
	if (t == NULL) return NULL;
	if (t[0] == '\0') return t;
	for (i = 0; t[i] != '\0'; i++) {
		if (t[i] >= 'A' && t[i] <= 'Z') t[i] += 32;
	}
	if (t[i-1] == '.') t[i-1] = '\0';
	return t;
}

/* _mdns_hostent_append_alias
 * Appends an alias to the mdns_hostent_t structure.
 */
static int
_mdns_hostent_append_alias(mdns_hostent_t *h, const char *alias)
{
	int i;
	char *name;
	if (h == NULL || alias == NULL) return 0;
	name = _mdns_canonicalize(alias);
	if (name == NULL) return -1;

	// don't add the name if it matches an existing name
	if (h->host.h_name && string_equal(h->host.h_name, name)) {
		free(name);
		return 0;
	}
	for (i = 0; i < h->alias_count; ++i) {
		if (string_equal(h->host.h_aliases[i], name)) {
			free(name);
			return 0;
		}
	}

	// add the alias and NULL terminate the list
	h->host.h_aliases = (char **)reallocf(h->host.h_aliases, (h->alias_count+2) * sizeof(char *));
	if (h->host.h_aliases == NULL) {
		h->alias_count = 0;
		free(name);
		return -1;
	}
	h->host.h_aliases[h->alias_count] = name;
	++h->alias_count;
	h->host.h_aliases[h->alias_count] = NULL;
	return 0;
}

/* _mdns_hostent_append_addr
 * Appends an alias to the mdns_hostent_t structure.
 */
static int
_mdns_hostent_append_addr(mdns_hostent_t *h, const uint8_t *addr, uint32_t len)
{
	if (h == NULL || addr == NULL || len == 0) return 0;

	// copy the address buffer
	uint8_t *buf = malloc(len);
	if (buf == NULL) return -1;
	memcpy(buf, addr, len);

	// add the address and NULL terminate the list
	h->host.h_addr_list = (char **)reallocf(h->host.h_addr_list, (h->addr_count+2) * sizeof(char *));
	if (h->host.h_addr_list == NULL) {
		h->addr_count = 0;
		return -1;
	}
	h->host.h_addr_list[h->addr_count] = (char*)buf;
	h->addr_count++;
	h->host.h_addr_list[h->addr_count] = NULL;
	return 0;
}

static void
_mdns_hostent_clear(mdns_hostent_t *h)
{
	if (h == NULL) return;
	free(h->host.h_name);
	h->host.h_name = NULL;

	char **aliases = h->host.h_aliases;
	while (aliases && *aliases) {
		free(*aliases++);
	}
	free(h->host.h_aliases);
	h->host.h_aliases = NULL;
	h->alias_count = 0;

	char **addrs = h->host.h_addr_list;
	while (addrs && *addrs) {
		free(*addrs++);
	}
	free(h->host.h_addr_list);
	h->host.h_addr_list = NULL;
	h->addr_count = 0;

}

static void
_mdns_reply_clear(mdns_reply_t *r)
{
	if (r == NULL) return;
	r->ifnum = 0;
	_mdns_hostent_clear(r->h4);
	_mdns_hostent_clear(r->h6);
	mdns_srv_t *srv = r->srv;
	r->srv = NULL;
	while (srv) {
		mdns_srv_t *next = srv->next;
		free(srv->srv.target);
		free(srv);
		srv = next;
	}
}

static si_item_t *
_mdns_hostbyname(si_mod_t *si, const char *name, int af, const char *interface, uint32_t *err)
{
	uint32_t type;
	mdns_hostent_t h;
	mdns_reply_t reply;
	si_item_t *out = NULL;
	uint64_t bb;
	int status;
	DNSServiceFlags flags = 0;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if (name == NULL) {
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	memset(&h, 0, sizeof(h));
	memset(&reply, 0, sizeof(reply));

	switch (af) {
		case AF_INET:
			type = ns_t_a;
			h.host.h_length = 4;
			reply.h4 = &h;
			break;
		case AF_INET6:
			type = ns_t_aaaa;
			h.host.h_length = 16;
			reply.h6 = &h;
			break;
		default:
			if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
			return NULL;
	}
	h.host.h_addrtype = af;

	status = _mdns_search(name, ns_c_in, type, interface, flags, 0, 1, NULL, NULL, &reply);
	if (status != 0 || h.addr_count == 0) {
		_mdns_reply_clear(&reply);
		if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
		return NULL;
	}

	bb = reply.ttl + time(NULL);

	switch (af) {
		case AF_INET:
			out = (si_item_t *)LI_ils_create("L4488s*44a", (unsigned long)si, CATEGORY_HOST_IPV4, 1, bb, 0LL, h.host.h_name, h.host.h_aliases, h.host.h_addrtype, h.host.h_length, h.host.h_addr_list);
			break;
		case AF_INET6:
			out = (si_item_t *)LI_ils_create("L4488s*44c", (unsigned long)si, CATEGORY_HOST_IPV6, 1, bb, 0LL, h.host.h_name, h.host.h_aliases, h.host.h_addrtype, h.host.h_length, h.host.h_addr_list);
			break;
	}

	_mdns_reply_clear(&reply);

	if (out == NULL && err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;

	return out;
}

static si_item_t *
_mdns_hostbyaddr(si_mod_t *si, const void *addr, int af, const char *interface, uint32_t *err)
{
	mdns_hostent_t h;
	mdns_reply_t reply;
	char *name;
	si_item_t *out;
	uint64_t bb;
	int cat;
	int status;
	DNSServiceFlags flags = 0;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if (addr == NULL || si == NULL) {
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	memset(&h, 0, sizeof(h));
	memset(&reply, 0, sizeof(reply));

	switch (af) {
		case AF_INET:
			h.host.h_length = 4;
			reply.h4 = &h;
			name = _mdns_reverse_ipv4(addr);
			cat = CATEGORY_HOST_IPV4;
			break;
		case AF_INET6:
			h.host.h_length = 16;
			reply.h6 = &h;
			name = _mdns_reverse_ipv6(addr);
			cat = CATEGORY_HOST_IPV6;
			break;
		default:
			if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
			return NULL;
	}
	h.host.h_addrtype = af;

	status = _mdns_search(name, ns_c_in, ns_t_ptr, interface, flags, 0, 1, NULL, NULL, &reply);
	free(name);
	if (status != 0) {
		_mdns_reply_clear(&reply);
		if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
		return NULL;
	}

	status = _mdns_hostent_append_addr(&h, addr, h.host.h_length);
	if (status != 0) {
		_mdns_hostent_clear(&h);
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	bb = reply.ttl + time(NULL);
	out = (si_item_t *)LI_ils_create("L4488s*44a", (unsigned long)si, cat, 1, bb, 0LL, h.host.h_name, h.host.h_aliases, h.host.h_addrtype, h.host.h_length, h.host.h_addr_list);

	_mdns_hostent_clear(&h);

	if (out == NULL && err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
	return out;
}

static si_list_t *
_mdns_addrinfo(si_mod_t *si, const void *node, const void *serv, uint32_t family, uint32_t socktype, uint32_t proto, uint32_t flags, const char *interface, uint32_t *err)
{
	int wantv4 = 1;
	int wantv6 = 1;
	if (family == AF_INET6) wantv4 = 0;
	else if (family == AF_INET) wantv6 = 0;
	else if (family != AF_UNSPEC) return NULL;

	struct in_addr a4;
	struct in6_addr a6;
	mdns_hostent_t h4;
	mdns_hostent_t h6;
	mdns_reply_t reply;
	uint32_t type;
	uint16_t port;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	si_list_t *out = NULL;

	memset(&h4, 0, sizeof(h4));
	memset(&h6, 0, sizeof(h6));
	memset(&reply, 0, sizeof(reply));

	h4.host.h_addrtype = AF_INET;
	h4.host.h_length = 4;
	h6.host.h_addrtype = AF_INET6;
	h6.host.h_length = 16;

	if (wantv4 && wantv6) {
		type = 0;
		reply.h4 = &h4;
		reply.h6 = &h6;
	} else if (wantv4) {
		reply.h4 = &h4;
		type = ns_t_a;
	} else if (wantv6) {
		type = ns_t_aaaa;
		reply.h6 = &h6;
	} else {
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	// service lookup
	if ((flags & AI_NUMERICSERV) != 0) {
		port = *(uint16_t *)serv;
	} else {
		if (_gai_serv_to_port(serv, proto, &port) != 0) {
			if (err) *err = SI_STATUS_EAI_NONAME;
			return NULL;
		}
	}

	// host lookup
	if ((flags & AI_NUMERICHOST) != 0) {
		char *cname = NULL;
		struct in_addr *p4 = NULL;
		struct in6_addr *p6 = NULL;
		if (family == AF_INET) {
			p4 = &a4;
			memcpy(p4, node, sizeof(a4));
		} else if (family == AF_INET6) {
			p6 = &a6;
			memcpy(p6, node, sizeof(a6));
		}
		out = si_addrinfo_list(si, socktype, proto, p4, p6, port, 0, cname, cname);
	} else {
		DNSServiceFlags dns_flags = 0;
		if (flags & AI_ADDRCONFIG) {
			dns_flags |= kDNSServiceFlagsSuppressUnusable;
		}
		int res;
		res = _mdns_search(node, ns_c_in, type, interface, dns_flags, 0, 1, NULL, NULL, &reply);
		if (res == 0 && (h4.addr_count > 0 || h6.addr_count > 0)) {
			out = si_addrinfo_list_from_hostent(si, socktype, proto,
												port, 0,
												(wantv4 ? &h4.host : NULL),
												(wantv6 ? &h6.host : NULL));
		} else if (err != NULL) {
			*err = SI_STATUS_EAI_NONAME;
		}
		_mdns_reply_clear(&reply);
	}
	return out;
}

static si_list_t *
_mdns_srv_byname(si_mod_t* si, const char *qname, const char *interface, uint32_t *err)
{
	si_list_t *out = NULL;
	mdns_reply_t reply;
	mdns_srv_t *srv;
	int res;
	const uint64_t unused = 0;
	DNSServiceFlags flags = 0;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	memset(&reply, 0, sizeof(reply));
	res = _mdns_search(qname, ns_c_in, ns_t_srv, interface, flags, 0, 1, NULL, NULL, &reply);
	if (res == 0) {
		srv = reply.srv;
		while (srv) {
			si_item_t *item;
			item = (si_item_t *)LI_ils_create("L4488222s", (unsigned long)si, CATEGORY_SRV, 1, unused, unused, srv->srv.priority, srv->srv.weight, srv->srv.port, srv->srv.target);
			out = si_list_add(out, item);
			si_item_release(item);
			srv = srv->next;
		}
	}
	_mdns_reply_clear(&reply);
	return out;
}

/*
 * We support dns_async_start / cancel / handle_reply using dns_item_call
 */
static si_item_t *
_mdns_item_call(si_mod_t *si, int call, const char *name, const char *ignored, const char *interface, uint32_t class, uint32_t type, uint32_t *err)
{
	int res;
	uint8_t buf[DNS_MAX_RECEIVE_SIZE];
	uint32_t len = sizeof(buf);
	mdns_reply_t reply;
	mdns_hostent_t h4;
	mdns_hostent_t h6;
	si_item_t *out;
	int norecurse = 0;
	DNSServiceFlags flags = 0;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	switch (call) {
		case SI_CALL_DNS_QUERY:
			norecurse = 1;
			break;
		case SI_CALL_DNS_SEARCH:
			break;
		default:
			if (err) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
			return NULL;
			break;
	}

	if (name == NULL) {
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	memset(&h4, 0, sizeof(h4));
	memset(&h6, 0, sizeof(h6));
	memset(&reply, 0, sizeof(reply));

	h4.host.h_addrtype = AF_INET;
	h4.host.h_length = 4;
	h6.host.h_addrtype = AF_INET6;
	h6.host.h_length = 16;
	reply.h4 = &h4;
	reply.h6 = &h6;

	res = _mdns_search(name, class, type, interface, flags, norecurse, 1, buf, &len, &reply);
	if (res != 0 || len <= 0 || len > DNS_MAX_RECEIVE_SIZE) {
		_mdns_reply_clear(&reply);
		if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
		return NULL;
	}

	struct sockaddr_in6 from;
	uint32_t fromlen = sizeof(from);
	memset(&from, 0, fromlen);
	from.sin6_len = fromlen;
	from.sin6_family = AF_INET6;
	from.sin6_addr.__u6_addr.__u6_addr8[15] = 1;
	if (reply.ifnum != 0) {
		from.sin6_addr.__u6_addr.__u6_addr16[0] = htons(0xfe80);
		from.sin6_scope_id = reply.ifnum;
	}

	out = (si_item_t *)LI_ils_create("L4488@@", (unsigned long)si, CATEGORY_DNSPACKET, 1, 0LL, 0LL, len, buf, fromlen, &from);
	if (out == NULL && err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;

	_mdns_reply_clear(&reply);

	return out;
}

static int
_mdns_is_valid(si_mod_t *si, si_item_t *item)
{
	return 0;
}

static void
_mdns_close(si_mod_t *si)
{
	if (_dns_config_token != -1) notify_cancel(_dns_config_token);
	//_mdns_dir_token;
}

static void
_mdns_atfork_prepare(void)
{
	// acquire our lock so that we know all other threads have "drained"
	pthread_mutex_lock(&_mdns_mutex);
}

static void
_mdns_atfork_parent(void)
{
	// parent can simply resume
	pthread_mutex_unlock(&_mdns_mutex);
}

static void
_mdns_atfork_child(void)
{
	// child needs to force re-initialization
	_mdns_old_sdref = _mdns_sdref; // for later deallocation
	_mdns_sdref = NULL;
	_dns_config_token = -1;
	pthread_mutex_unlock(&_mdns_mutex);
}

__private_extern__ si_mod_t *
si_module_static_mdns(void)
{
	si_mod_t *out = (si_mod_t *)calloc(1, sizeof(si_mod_t));
	char *outname = strdup("mdns");

	if ((out == NULL) || (outname == NULL))
	{
		free(out);
		free(outname);
		return NULL;
	}

	out->name = outname;
	out->vers = 1;
	out->refcount = 1;
	out->private = NULL;

	out->sim_close = _mdns_close;
	out->sim_is_valid = _mdns_is_valid;
	out->sim_host_byname = _mdns_hostbyname;
	out->sim_host_byaddr = _mdns_hostbyaddr;
	out->sim_item_call = _mdns_item_call;
	out->sim_addrinfo = _mdns_addrinfo;
	out->sim_srv_byname = _mdns_srv_byname;

	int res;

	res = notify_register_check(dns_configuration_notify_key(), &_dns_config_token);
	if (res != NOTIFY_STATUS_OK) _dns_config_token = -1;

	pthread_atfork(_mdns_atfork_prepare, _mdns_atfork_parent, _mdns_atfork_child);

	_mdns_debug = getenv("RES_DEBUG") != NULL;

	return out;
}

/*
 * _mdns_parse_domain_name
 * Combine DNS labels to form a string.
 * DNSService API does not return compressed names.
 */
static char *
_mdns_parse_domain_name(const uint8_t *data, uint32_t datalen)
{
	int i = 0, j = 0;
	uint32_t len;
	uint32_t domainlen = 0;
	char *domain = NULL;

	if ((data == NULL) || (datalen == 0)) return NULL;

	// i: index into input data
	// j: index into output string
	while (datalen-- > 0) {
		len = data[i++];
		domainlen += (len + 1);
		domain = reallocf(domain, domainlen);
		if (domain == NULL) return NULL;
		if (len == 0) break;	// DNS root (NUL)
		if (j > 0) {
			domain[j++] = datalen ? '.' : '\0'; 
		}

		while ((len-- > 0) && (datalen--)) {
			if (data[i] == '.') {
				// special case: escape the '.' with a '\'
				domain = reallocf(domain, ++domainlen);
				if (domain == NULL) return NULL;
				domain[j++] = '\\';
			}
			domain[j++] = data[i++];
		}
	}
	domain[j] = '\0';

	return domain;
}

/*
 * _mdns_pack_domain_name
 * Format the string as packed DNS labels.
 * Only used for one string at a time, therefore no need for compression.
 */
static int
_mdns_pack_domain_name(const char* str, uint8_t *buf, size_t buflen) {
	int i = 0;
	while (i < buflen) {
		uintptr_t len;
		// calculate length to next '.' or '\0'
		char *dot = strchr(str, '.');
		if (dot == NULL) dot = strchr(str, '\0');
		len = (dot - str);
		if (len > NS_MAXLABEL) return -1;
		// copy data for label
		buf[i++] = len;
		while (str < dot) {
			buf[i++] = *str++;
		}
		// skip past '.', break if '\0'
		if (*str++ == '\0') break;
	}
	if (i >= buflen) return -1;
	buf[i] = '\0';
	return i;
}

static int
_is_rev_link_local(const char *name)
{
	int len, i;

	if (name == NULL) return 0;

	len = strlen(name);
	if (len == 0) return 0;

	/* check for trailing '.' */
	if (name[len - 1] == '.') len--;

	if (len != IPv6_REVERSE_LEN) return 0;

	i = IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR;
	if ((name[i] != '8') && (name[i] != '9') && (name[i] != 'A') && (name[i] != 'a') && (name[i] != 'B') && (name[i] != 'b')) return 0;

	i = IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR + 1;
	if (strncasecmp(name + i, ".e.f.ip6.arpa", 13)) return 0;

	for (i = 0; i < IPv6_REVERSE_LINK_LOCAL_TRAILING_CHAR; i += 2)
	{
		if (name[i] < '0') return 0;
		if ((name[i] > '9') && (name[i] < 'A')) return 0;
		if ((name[i] > 'F') && (name[i] < 'a')) return 0;
		if (name[i] > 'f') return 0;
		if (name[i + 1] != '.') return 0;
	}

	return 1;
}

/* _mdns_ipv6_extract_scope_id
 * If the input string is a link local IPv6 address with an encoded scope id,
 * the scope id is extracted and a new string is constructed with the scope id removed.
 */
static char *
_mdns_ipv6_extract_scope_id(const char *name, uint32_t *out_ifnum)
{
	char *qname = NULL;
	uint16_t nibble;
	uint32_t iface;
	int i;

	if (out_ifnum != NULL) *out_ifnum = 0;

	/* examine the address, extract the scope id if present */
	if ((name != NULL) && (_is_rev_link_local(name)))
	{
		/* _is_rev_link_local rejects chars > 127 so it's safe to index into hexval */
		i = IPv6_REVERSE_LINK_LOCAL_SCOPE_ID_LOW;
		nibble = hexval[(uint32_t)name[i]];
		iface = nibble;

		i += 2;
		nibble = hexval[(uint32_t)name[i]];
		iface += (nibble << 4);

		i += 2;
		nibble = hexval[(uint32_t)name[i]];
		iface += (nibble << 8);

		i += 2;
		nibble = hexval[(uint32_t)name[i]];
		iface += (nibble << 12);

		if (iface != 0)
		{
			qname = strdup(name);
			if (qname == NULL) return NULL;

			i = IPv6_REVERSE_LINK_LOCAL_SCOPE_ID_LOW;
			qname[i] = '0';
			qname[i + 2] = '0';
			qname[i + 4] = '0';
			qname[i + 6] = '0';

			if (out_ifnum) *out_ifnum = iface;
		}
	}

	return qname;
}

static int
_mdns_make_query(const char* name, int class, int type, uint8_t *buf, uint32_t buflen)
{
	uint32_t len = 0;

	if (buf == NULL || buflen < (NS_HFIXEDSZ + NS_QFIXEDSZ)) return -1;
	memset(buf, 0, NS_HFIXEDSZ);
	HEADER *hp = (HEADER *)buf;

	len += NS_HFIXEDSZ;
	hp->id = arc4random();
	hp->qr = 1;
	hp->opcode = ns_o_query;
	hp->rd = 1;
	hp->rcode = ns_r_noerror;
	hp->qdcount = htons(1);

	int n = _mdns_pack_domain_name(name, &buf[len], buflen - len);
	if (n < 0) return -1;

	len += n;
	uint16_t word;
	word = htons(type);
	memcpy(&buf[len], &word, sizeof(word));
	len += sizeof(word);
	word = htons(class);
	memcpy(&buf[len], &word, sizeof(word));
	len += sizeof(word);
	return len;
}

typedef struct {
	mdns_reply_t *reply;
	mdns_hostent_t *host;
	uint8_t *answer; // DNS packet buffer
	size_t anslen; // DNS packet buffer current length
	size_t ansmaxlen; // DNS packet buffer maximum length
	int type; // type of query: A, AAAA, PTR, SRV...
	uint16_t last_type; // last type received
	uint32_t sd_gen;
	DNSServiceRef sd;
	DNSServiceFlags flags;
	DNSServiceErrorType error;
	int kq; // kqueue to notify when callback received
} mdns_query_context_t;

static void
_mdns_query_callback(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType, const char *, uint16_t, uint16_t, uint16_t, const void *, uint32_t, void *);

/* _mdns_query_start
 * initializes the context and starts a DNS-SD query.
 */
static DNSServiceErrorType
_mdns_query_start(mdns_query_context_t *ctx, mdns_reply_t *reply, uint8_t *answer, uint32_t *anslen, const char* name, int class, int type, const char *interface, DNSServiceFlags flags, int kq)
{
	DNSServiceErrorType status;

	flags |= kDNSServiceFlagsShareConnection;
	flags |= kDNSServiceFlagsReturnIntermediates;

	memset(ctx, 0, sizeof(mdns_query_context_t));

	if (answer && anslen) {
		// build a dummy DNS header to return to the caller
		ctx->answer = answer;
		ctx->ansmaxlen = *anslen;
		ctx->anslen = _mdns_make_query(name, class, type, answer, ctx->ansmaxlen);
		if (ctx->anslen <= 0) return -1;
	}

	ctx->type = type;
	ctx->sd = _mdns_sdref;
	ctx->sd_gen = _mdns_generation;
	ctx->kq = kq;
	if (reply) {
		ctx->reply = reply;
		if (type == ns_t_a) ctx->host = reply->h4;
		else if (type == ns_t_aaaa) ctx->host = reply->h6;
		else if (type == ns_t_ptr && reply->h4) ctx->host = reply->h4;
		else if (type == ns_t_ptr && reply->h6) ctx->host = reply->h6;
		else if (type != ns_t_srv && type != ns_t_cname) abort();
	}

	uint32_t iface = 0;
	char *qname = _mdns_ipv6_extract_scope_id(name, &iface);
	if (qname == NULL) qname = (char *)name;

	if (interface != NULL)
	{
		/* get interface number from name */
		int iface2 = if_nametoindex(interface);

		/* balk if interface name lookup failed */
		if (iface2 == 0) return -1;

		/* balk if scope id is set AND interface is given AND they don't match */
		if ((iface != 0) && (iface2 != 0) && (iface != iface2)) return -1;
		if (iface2 != 0) iface = iface2;
	}

	if (_mdns_debug) printf(";; mdns query %s %d %d\n", qname, type, class);
	status = DNSServiceQueryRecord(&ctx->sd, flags, iface, qname, type, class, _mdns_query_callback, ctx);
	if (qname != name) free(qname);
	return status;
}

/* _mdns_query_is_complete
 * Determines whether the specified query has sufficient information to be
 * considered complete.
 */
static int
_mdns_query_is_complete(mdns_query_context_t *ctx)
{
	if (ctx == NULL) return 1;
	//if (ctx->flags & kDNSServiceFlagsMoreComing) return 0;
	if (ctx->last_type != ctx->type) return 0;
	switch (ctx->type) {
		case ns_t_a:
		case ns_t_aaaa:
			if (ctx->host != NULL && ctx->host->addr_count > 0) {
				return 1;
			}
			break;
		case ns_t_ptr:
			if (ctx->host != NULL && ctx->host->host.h_name != NULL) {
				return 1;
			}
			break;
		case ns_t_srv:
			if (ctx->reply != NULL && ctx->reply->srv != NULL) {
				return 1;
			}
			break;
		default:
			abort();
	}
	return 0;
}

/* _mdns_query_clear
 * Clear out the temporary fields of the context, and clear any result
 * structures that are incomplete.  Retrns 1 if the query was complete.
 */
static int
_mdns_query_clear(mdns_query_context_t *ctx)
{
	int complete = _mdns_query_is_complete(ctx);
	if (ctx == NULL) return complete;

	/* only dealloc this DNSServiceRef if the "main" _mdns_sdref has not been deallocated */
	if (ctx->sd != NULL && ctx->sd_gen == _mdns_generation) {
		DNSServiceRefDeallocate(ctx->sd);
	}

	ctx->sd = NULL;
	ctx->sd_gen = 0;
	ctx->flags = 0;
	ctx->kq = -1;

	if (!complete) {
		_mdns_hostent_clear(ctx->host);
		ctx->anslen = -1;
	}
	return complete;
}

static void
_mdns_query_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t ifIndex, DNSServiceErrorType errorCode, const char *fullname, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl, void *ctx)
{
	mdns_query_context_t *context;
	struct in6_addr a6;

	context = (mdns_query_context_t *)ctx;

	context->flags = flags;
	context->error = errorCode;
	context->last_type = rrtype;

	if (errorCode != kDNSServiceErr_NoError) {
		if (_mdns_debug) printf(";; [%s %hu %hu]: error %d\n", fullname, rrtype, rrclass, errorCode);
		goto wakeup_kevent;
	}

	// embed the scope ID into link-local IPv6 addresses
	if (rrtype == ns_t_aaaa && rdlen == sizeof(struct in6_addr) &&
	    IN6_IS_ADDR_LINKLOCAL((struct in6_addr *)rdata)) {
		memcpy(&a6, rdata, rdlen);
		a6.__u6_addr.__u6_addr16[1] = htons(ifIndex);
		rdata = &a6;
	}

	if (context->reply) {
		char *name;
		int malformed = 0;
		mdns_reply_t *reply = context->reply;

		if (reply->ifnum == 0) {
			reply->ifnum = ifIndex;
		}

		_mdns_hostent_append_alias(context->host, fullname);
		if (reply->ttl == 0 || ttl < reply->ttl) reply->ttl = ttl;

		switch (rrtype) {
			case ns_t_a:
			case ns_t_aaaa:
				if (((rrtype == ns_t_a && context->host->host.h_addrtype == AF_INET) ||
					 (rrtype == ns_t_aaaa && context->host->host.h_addrtype == AF_INET6)) &&
					rdlen >= context->host->host.h_length) {
					if (context->host->host.h_name == NULL) {
						int i;
						mdns_hostent_t *h = context->host;
						char *h_name = _mdns_canonicalize(fullname);
						context->host->host.h_name = h_name;

						// 6863416 remove h_name from h_aliases
						for (i = 0; i < h->alias_count; ++i) {
							if (h_name == NULL) break;
							if (string_equal(h->host.h_aliases[i], h_name)) {
								// includes trailing NULL pointer
								int sz = sizeof(char *) * (h->alias_count - i);
								free(h->host.h_aliases[i]);
								memmove(&h->host.h_aliases[i], &h->host.h_aliases[i+1], sz);
								h->alias_count -= 1;
								break;
							}
						}
					}
					_mdns_hostent_append_addr(context->host, rdata, context->host->host.h_length);
				} else {
					malformed = 1;
				}
				break;
			case ns_t_cname:
				name = _mdns_parse_domain_name(rdata, rdlen);
				if (!name) malformed = 1;
				_mdns_hostent_append_alias(context->host, name);
				free(name);
				break;
			case ns_t_ptr:
				name = _mdns_parse_domain_name(rdata, rdlen);
				if (!name) malformed = 1;
				if (context->host && context->host->host.h_name == NULL) {
					context->host->host.h_name = _mdns_canonicalize(name);
				}
				_mdns_hostent_append_alias(context->host, name);
				free(name);
				break;
			case ns_t_srv: {
				mdns_rr_srv_t *p = (mdns_rr_srv_t*)rdata;
				mdns_srv_t *srv = calloc(1, sizeof(mdns_srv_t));
				if (srv == NULL) break;
				if (rdlen < sizeof(mdns_rr_srv_t)) {
					malformed = 1;
					break;
				}
				srv->srv.priority = ntohs(p->priority);
				srv->srv.weight = ntohs(p->weight);
				srv->srv.port = ntohs(p->port);
				srv->srv.target = _mdns_parse_domain_name(&p->target[0], rdlen - 3*sizeof(uint16_t));
				if (srv->srv.target == NULL) {
					malformed = 1;
					break;
				}
				// append to the end of the list
				if (reply->srv == NULL) {
					reply->srv = srv;
				} else {
					mdns_srv_t *iter = reply->srv;
					while (iter->next) iter = iter->next;
					iter->next = srv;
				}
				break;
			}
			default:
				malformed = _mdns_debug;
				break;
		}
		if (malformed && _mdns_debug) {
			printf(";; [%s %hu %hu]: malformed reply\n", fullname, rrtype, rrclass);
			goto wakeup_kevent;
		}
	}

	if (context->answer) {
		int n;
		uint8_t *cp;
		HEADER *ans;
		size_t buflen = context->ansmaxlen - context->anslen;
		if (buflen < NS_HFIXEDSZ)
		{
			if (_mdns_debug) printf(";; [%s %hu %hu]: malformed reply\n", fullname, rrtype, rrclass);
			goto wakeup_kevent;
		}

		cp = context->answer + context->anslen;

		n = _mdns_pack_domain_name(fullname, cp, buflen);
		if (n < 0) {
			if (_mdns_debug) printf(";; [%s %hu %hu]: name mismatch\n", fullname, rrtype, rrclass);
			goto wakeup_kevent;
		}

		// check that there is enough space in the buffer for the
		// resource name (n), the resource record data (rdlen) and
		// the resource record header (10).
		if (buflen < n + rdlen + 10) {
			if (_mdns_debug) printf(";; [%s %hu %hu]: insufficient buffer space for reply\n", fullname, rrtype, rrclass);
			goto wakeup_kevent;
		}

		cp += n;
		buflen -= n;

		uint16_t word;
		uint32_t longword;

		word = htons(rrtype);
		memcpy(cp, &word, sizeof(word));
		cp += sizeof(word);

		word = htons(rrclass);
		memcpy(cp, &word, sizeof(word));
		cp += sizeof(word);

		longword = htonl(ttl);
		memcpy(cp, &longword, sizeof(longword));
		cp += sizeof(longword);

		word = htons(rdlen);
		memcpy(cp, &word, sizeof(word));
		cp += sizeof(word);

		memcpy(cp, rdata, rdlen);
		cp += rdlen;

		ans = (HEADER *)context->answer;
		ans->ancount = htons(ntohs(ans->ancount) + 1);

		context->anslen = (size_t)(cp - context->answer);
	}

	if (_mdns_debug) printf(";; [%s %hu %hu]\n", fullname, rrtype, rrclass);

wakeup_kevent:
	// Ping the waiting thread in case this callback was invoked on another
	if (context->kq != -1) {
		struct kevent ev;
		EV_SET(&ev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, 0);
		int res = kevent(context->kq, &ev, 1, NULL, 0, NULL);
		if (res && _mdns_debug) printf(";; kevent EV_TRIGGER: %s\n", strerror(errno));
	}
}

static void
_mdns_now(struct timespec *now) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	now->tv_sec = tv.tv_sec;
	now->tv_nsec = tv.tv_usec * 1000;
}

static void
_mdns_add_time(struct timespec *sum, const struct timespec *a, const struct timespec *b)
{
	sum->tv_sec = a->tv_sec + b->tv_sec;
	sum->tv_nsec = a->tv_nsec + b->tv_nsec;
	if (sum->tv_nsec > 1000000000) {
		sum->tv_sec += (sum->tv_nsec / 1000000000);
		sum->tv_nsec %= 1000000000;
	}
}

// calculate a deadline from the current time based on the desired timeout
static void
_mdns_deadline(struct timespec *deadline, const struct timespec *delta)
{
	struct timespec now;
	_mdns_now(&now);
	_mdns_add_time(deadline, &now, delta);
}

static void
_mdns_sub_time(struct timespec *delta, const struct timespec *a, const struct timespec *b)
{
	delta->tv_sec = a->tv_sec - b->tv_sec;
	delta->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (delta->tv_nsec < 0) {
		delta->tv_nsec += 1000000000;
		delta->tv_sec -= 1;
	}
}

// calculate a timeout remaining before the given deadline
static void
_mdns_timeout(struct timespec *timeout, const struct timespec *deadline)
{
	struct timespec now;
	_mdns_now(&now);
	_mdns_sub_time(timeout, deadline, &now);
}

int
_mdns_query_mDNSResponder(const char *name, int class, int type, const char *interface, DNSServiceFlags flags, uint8_t *answer, uint32_t *anslen, mdns_reply_t *reply, uint32_t timeout_sec)
{
	DNSServiceErrorType err = 0;
	int kq, n, wait = 1;
	struct kevent ev;
	struct timespec start, finish, delta, timeout;
	int res = 0;
	int i, complete, got_response = 0;
	int initialize = 1;

	// 2 for A and AAAA parallel queries
	int n_ctx = 0;
	mdns_query_context_t ctx[2];

	if (name == NULL) return -1;

#if TARGET_OS_EMBEDDED
	// log a warning for queries from the main thread 
	if (pthread_main_np()) asl_log(NULL, NULL, ASL_LEVEL_WARNING, "Warning: Libinfo call to mDNSResponder on main thread");
#endif // TARGET_OS_EMBEDDED

	// Timeout Logic
	// The kevent(2) API timeout parameter is used to enforce the total
	// timeout of the DNS query.  Each iteraion recalculates the relative
	// timeout based on the desired end time (total timeout from origin).
	//
	// In order to workaround some DNS configurations that do not return
	// responses for AAAA queries, parallel queries modify the total
	// timeout upon receipt of the first response.  The new total timeout is
	// set to an effective value of 2N where N is the time taken to receive
	// the A response (the original total timeout is preserved if 2N would
	// have exceeded it).  However, since mDNSResponder caches values, a
	// minimum value of 50ms for N is enforced in order to give some time
	// for the receipt of a AAAA response.

	// determine the maximum time to wait for a result
	if (timeout_sec == 0) timeout_sec = RES_MAXRETRANS;
	delta.tv_sec = timeout_sec;
	delta.tv_nsec = 0;
	_mdns_deadline(&finish, &delta);
	timeout = delta;
	_mdns_now(&start);

	for (i = 0; i < 2; ++i) {
		memset(&ctx[i], 0 , sizeof(mdns_query_context_t));
	}

	// set up the kqueue
	kq = kqueue();
	EV_SET(&ev, 1, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, 0);
	n = kevent(kq, &ev, 1, NULL, 0, NULL);
	if (n != 0) wait = 0;

	while (wait == 1) {
		if (initialize) {
			initialize = 0;
			pthread_mutex_lock(&_mdns_mutex);
			// clear any stale contexts
			for (i = 0; i < n_ctx; ++i) {
				_mdns_query_clear(&ctx[i]);
			}
			n_ctx = 0;

			if (_mdns_sdref == NULL) {
				if (_mdns_old_sdref != NULL) {
					_mdns_generation++;
					DNSServiceRefDeallocate(_mdns_old_sdref);
					_mdns_old_sdref = NULL;
				}
				// (re)initialize the shared connection
				err = DNSServiceCreateConnection(&_mdns_sdref);
				if (err != 0) {
					wait = 0;
					pthread_mutex_unlock(&_mdns_mutex);
					break;
				}
			}

			// issue (or reissue) the queries
			// unspecified type: do parallel A and AAAA
			if (err == 0) {
				err = _mdns_query_start(&ctx[n_ctx++], reply,
										answer, anslen,
										name, class,
										(type == 0) ? ns_t_a : type, interface, flags, kq);
			}
			if (err == 0 && type == 0) {
				err = _mdns_query_start(&ctx[n_ctx++], reply,
										answer, anslen,
										name, class, ns_t_aaaa, interface, flags, kq);
			}
			if (err && _mdns_debug) printf(";; initialization error %d\n", err);
			// try to reinitialize
			if (err == kDNSServiceErr_Unknown ||
				err == kDNSServiceErr_ServiceNotRunning ||
				err == kDNSServiceErr_BadReference) {
				if (_mdns_sdref) {
					_mdns_generation++;
					DNSServiceRefDeallocate(_mdns_sdref);
					_mdns_sdref = NULL;
				}
				err = 0;
				initialize = 1;
				pthread_mutex_unlock(&_mdns_mutex);
				continue;
			} else if (err != 0) {
				pthread_mutex_unlock(&_mdns_mutex);
				break;
			}

			// (re)register the fd with kqueue
			int fd = DNSServiceRefSockFD(_mdns_sdref);
			EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, 0);
			n = kevent(kq, &ev, 1, NULL, 0, NULL);
			pthread_mutex_unlock(&_mdns_mutex);
			if (err != 0 || n != 0) break;
		}

		if (_mdns_debug) printf(";; kevent timeout %ld.%ld\n", timeout.tv_sec, timeout.tv_nsec);
		n = kevent(kq, NULL, 0, &ev, 1, &timeout);
		if (n < 0 && errno != EINTR) {
			res = -1;
			break;
		}

		pthread_mutex_lock(&_mdns_mutex);
		// DNSServiceProcessResult() is a blocking API
		// confirm that there is still data on the socket
		const struct timespec notimeout = { 0, 0 };
		int m = kevent(kq, NULL, 0, &ev, 1, &notimeout);
		if (_mdns_sdref == NULL) {
			initialize = 1;
		} else if (m > 0 && ev.filter == EVFILT_READ) {
			err = DNSServiceProcessResult(_mdns_sdref);
			if (err == kDNSServiceErr_ServiceNotRunning ||
			    err == kDNSServiceErr_BadReference) {
				if (_mdns_debug) printf(";; DNSServiceProcessResult status %d\n", err);
				err = 0;
				// re-initialize the shared connection
				_mdns_generation++;
				DNSServiceRefDeallocate(_mdns_sdref);
				_mdns_sdref = NULL;
				initialize = 1;
			}
		}

		// Check if all queries are complete (including errors)
		complete = 1;
		for (i = 0; i < n_ctx; ++i) {
			if (_mdns_query_is_complete(&ctx[i]) || ctx[i].error != 0) {
				if (ctx[i].type == ns_t_a) {
					got_response = 1;
				}
			} else {
				complete = 0;
			}
		}
		pthread_mutex_unlock(&_mdns_mutex);

		if (err != 0) {
			if (_mdns_debug) printf(";; DNSServiceProcessResult status %d\n", err);
			break;
		} else if (complete == 1) {
			if (_mdns_debug) printf(";; done\n");
			break;
		} else if (got_response == 1) {
			// got A, adjust deadline for AAAA
			struct timespec now, tn, ms100;

			// delta = now - start
			_mdns_now(&now);
			_mdns_sub_time(&delta, &now, &start);

			// tn = 2 * delta
			_mdns_add_time(&tn, &delta, &delta);

			// delta = tn + 100ms
			ms100.tv_sec = 0;
			ms100.tv_nsec = 100000000;
			_mdns_add_time(&delta, &tn, &ms100);

			// check that delta doesn't exceed our total timeout
			_mdns_sub_time(&tn, &timeout, &delta);
			if (tn.tv_sec >= 0) {
				if (_mdns_debug) printf(";; new timeout (waiting for AAAA) %ld.%ld\n", delta.tv_sec, delta.tv_nsec);
				_mdns_deadline(&finish, &delta);
			}
		}

		// calculate remaining timeout
		_mdns_timeout(&timeout, &finish);

		// check for time remaining
		if (timeout.tv_sec < 0) {
			if (_mdns_debug) printf(";; timeout\n");
			break;
		}
	}

	complete = 0;
	pthread_mutex_lock(&_mdns_mutex);
	for (i = 0; i < n_ctx; ++i) {
		if (err == 0) err = ctx[i].error;
		// Only clears hostents if result is incomplete.
		complete = _mdns_query_clear(&ctx[i]) || complete;
	}
	pthread_mutex_unlock(&_mdns_mutex);
	// Everything should be done with the kq by now.
	close(kq);

	// Return error if everything is incomplete
	if (complete == 0) {
		res = -1;
	}

	if (anslen) *anslen = ctx[0].anslen;
	return res;
}
