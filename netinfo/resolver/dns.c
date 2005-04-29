/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "dns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <fcntl.h>
#include <notify.h>
#include <dnsinfo.h>
#include "dns_private.h"
#include "res_private.h"

#define INET_NTOP_AF_INET_OFFSET 4
#define INET_NTOP_AF_INET6_OFFSET 8

#define DNS_RESOLVER_DIR "/etc/resolver"

#define NOTIFY_DIR_NAME "com.apple.system.dns.resolver.dir"
#define DNS_DELAY_NAME "com.apple.system.dns.delay"

extern uint32_t notify_monitor_file(int token, const char *name, int flags);

/* notification for sys config changes via dns_configuration_notify_key() */
static uint32_t notify_resolver_sys_config_token = -1;

/* notify_resolver_dir_token monitors /etc/resolvers */
static uint32_t notify_resolver_dir_token = -1;

/* notify_resolver_dir_token monitors DNS delay for pppd */
static uint32_t notify_resolver_delay_token = -1;
static time_t dns_delay = 0;
#define DNS_DELAY_INTERVAL 4

#define DNS_PRIVATE_HANDLE_TYPE_SUPER 0
#define DNS_PRIVATE_HANDLE_TYPE_PLAIN 1

extern void res_client_close(res_state res);
extern res_state res_state_new();
extern int res_nquery_2(res_state statp, const char *name, int class, int type, u_char *answer, int anslen, struct sockaddr *from, int *fromlen);
extern int res_nsearch_2(res_state statp, const char *name, int class, int type, u_char *answer, int anslen, struct sockaddr *from, int *fromlen);
extern int __res_nsearch_list_2(res_state statp, const char *name,	int class, int type,  u_char *answer, int anslen, struct sockaddr *from, int *fromlen, int nsearch, char **search);

extern char *res_next_word(char **p);
extern res_state res_build_start(res_state res);
extern int res_build(res_state res, uint16_t port, uint32_t *nsrch, char *key, char *val);
extern int res_build_sortlist(res_state res, struct in_addr addr, struct in_addr mask);

static pdns_handle_t *
_pdns_build_start(char *name)
{
	pdns_handle_t *pdns;

	pdns = (pdns_handle_t *)calloc(1, sizeof(pdns_handle_t));
	if (pdns == NULL) return NULL;

	pdns->res = res_build_start(NULL);
	if (pdns->res == NULL)
	{
		free(pdns);
		return NULL;
	}

	if (name != NULL) pdns->name = strdup(name);
	pdns->port = NS_DEFAULTPORT;

	return pdns;
}

static int
_pdns_build_finish(pdns_handle_t *pdns)
{
	uint32_t n;

	if (pdns == NULL) return -1;

	n = pdns->res->nscount;
	if (n == 0) n = 1;

	if (pdns->total_timeout == 0)
	{
		if (pdns->send_timeout == 0) pdns->total_timeout = RES_MAXRETRANS;
		else pdns->total_timeout = pdns->send_timeout * pdns->res->retry * n;
	}

	if (pdns->total_timeout == 0) pdns->res->retrans = RES_MAXRETRANS;
	else pdns->res->retrans = pdns->total_timeout;
	
	pdns->res->options |= RES_INIT;

	return 0;
}

static int
_pdns_build_sortlist(pdns_handle_t *pdns, struct in_addr addr, struct in_addr mask)
{
	if (pdns == NULL) return -1;
	return res_build_sortlist(pdns->res, addr, mask);
}

static int
_pdns_build(pdns_handle_t *pdns, char *key, char *val)
{				
	struct in6_addr addr6;
	int32_t status;

	if (pdns == NULL) return -1;	

	if (((pdns->flags &= DNS_FLAG_HAVE_IPV6_SERVER) == 0) && (!strcmp(key, "nameserver")))
	{
		memset(&addr6, 0, sizeof(struct in6_addr));
		status = inet_pton(AF_INET6, val, &addr6);
		if (status == 1) pdns->flags |= DNS_FLAG_HAVE_IPV6_SERVER;
	}

	if (!strcmp(key, "port"))
	{
		pdns->port = atoi(val);
		return 0;
	}

	else if (!strcmp(key, "search"))
	{
		if (pdns->search_count == 0)
		{
			pdns->search_list = (char **)calloc(1, sizeof(char *));
		}
		else
		{
			pdns->search_list = (char **)realloc(pdns->search_list, (pdns->search_count + 1) * sizeof(char *));
		}
	
		pdns->search_list[pdns->search_count] = strdup(val);
		pdns->search_count++;
		return 0;
	}
	
	else if (!strcmp(key, "total_timeout"))
	{
		pdns->total_timeout = atoi(val);
		return 0;
	}
	
	else if (!strcmp(key, "timeout"))
	{
		pdns->send_timeout = atoi(val);
		return 0;
	}
	
	else if (!strcmp(key, "search_order"))
	{
		pdns->search_order = atoi(val);
		return 0;
	}

	return res_build(pdns->res, pdns->port, &(pdns->search_count), key, val);
}

static pdns_handle_t *
_pdns_convert_sc(dns_resolver_t *r)
{
	pdns_handle_t *pdns;
	char *val, *p, *x;
	int i;

	pdns = _pdns_build_start(r->domain);

	p = getenv("RES_RETRY_TIMEOUT");
	if (p != NULL) pdns->send_timeout = atoi(p);

	p = getenv("RES_RETRY");
	if (p != NULL) pdns->res->retry= atoi(p);

	if (r->port != 0)
	{
		asprintf(&val, "%hu", r->port);
		_pdns_build(pdns, "port", val);
		free(val);
	}

	if (r->n_nameserver > MAXNS) r->n_nameserver = MAXNS;
	for (i = 0; i < r->n_nameserver; i++)
	{
		if (r->nameserver[i]->sa_family == AF_INET)
		{
			val = calloc(1, 256);
			inet_ntop(AF_INET, (char *)(r->nameserver[i]) + INET_NTOP_AF_INET_OFFSET, val, 256);
			_pdns_build(pdns, "nameserver", val);
			free(val);
		}
		else if (r->nameserver[i]->sa_family == AF_INET6)
		{
			pdns->flags |= DNS_FLAG_HAVE_IPV6_SERVER;
			val = calloc(1, 256);
			inet_ntop(AF_INET6, (char *)(r->nameserver[i]) + INET_NTOP_AF_INET6_OFFSET, val, 256);
			_pdns_build(pdns, "nameserver", val);
			free(val);
		}
	}

	if (r->n_search > MAXDNSRCH) r->n_search = MAXDNSRCH;
	for (i = 0; i < r->n_search; i++)
	{
		asprintf(&val, "%s", r->search[i]);
		_pdns_build(pdns, "search", val);
		free(val);
	}

	if (r->timeout > 0)
	{
		asprintf(&val, "%d", r->timeout);
		_pdns_build(pdns, "total_timeout", val);
		free(val);
	}

	asprintf(&val, "%d", r->search_order);
	_pdns_build(pdns, "search_order", val);
	free(val);

	if (r->n_sortaddr > MAXRESOLVSORT) r->n_sortaddr = MAXRESOLVSORT;
	for (i = 0; i < r->n_sortaddr; i++)
	{
		_pdns_build_sortlist(pdns, r->sortaddr[i]->address, r->sortaddr[i]->mask);
	}

	p = r->options;
	while (NULL != (x = res_next_word(&p)))
	{
		/* search for and process individual options */
		if (!strncmp(x, "ndots:", 6))
		{
			_pdns_build(pdns, "ndots", x+6);
		}

		else if (!strncmp(x, "nibble:", 7))
		{
			_pdns_build(pdns, "nibble", x+7);
		}
		
		else if (!strncmp(x, "nibble2:", 8))
		{
			_pdns_build(pdns, "nibble2", x+8);
		}
		
		else if (!strncmp(x, "timeout:", 8))
		{
			_pdns_build(pdns, "timeout", x+8);
		}
		
		else if (!strncmp(x, "attempts:", 9))
		{
			_pdns_build(pdns, "attempts", x+9);
		}
		
		else if (!strncmp(x, "bitstring:", 10))
		{
			_pdns_build(pdns, "bitstring", x+10);
		}
		
		else if (!strncmp(x, "v6revmode:", 10))
		{
			_pdns_build(pdns, "v6revmode", x+10);
		}
		
		else if (!strcmp(x, "debug"))
		{
			_pdns_build(pdns, "debug", NULL);
		}
		
		else if (!strcmp(x, "no_tld_query"))
		{
			_pdns_build(pdns, "no_tld_query", NULL);
		}

		else if (!strcmp(x, "inet6"))
		{
			_pdns_build(pdns, "inet6", NULL);
		}
		
		else if (!strcmp(x, "rotate"))
		{
			_pdns_build(pdns, "rotate", NULL);
		}

		else if (!strcmp(x, "no-check-names"))
		{
			_pdns_build(pdns, "no-check-names", NULL);
		}
		
#ifdef RES_USE_EDNS0
		else if (!strcmp(x, "edns0"))
		{
			_pdns_build(pdns, "edns0", NULL);
		}		
#endif
		else if (!strcmp(x, "a6"))
		{
			_pdns_build(pdns, "a6", NULL);
		}
		
		else if (!strcmp(x, "dname"))
		{
			_pdns_build(pdns, "dname", NULL);
		}
	}

	_pdns_build_finish(pdns);
	return pdns;
}

/*
 * Open a named resolver client from the system config data.
 */
static pdns_handle_t *
_pdns_sc_open(const char *name)
{
	pdns_handle_t *pdns;
	int i;
	dns_config_t *sc_dns;
	dns_resolver_t *sc_res;

	sc_dns = dns_configuration_copy();
	if (sc_dns == NULL) return NULL;

	sc_res = NULL;

	if (name == NULL)
	{
		if (sc_dns->n_resolver != 0) sc_res = sc_dns->resolver[0];
	}
	else
	{
		for (i = 0; (sc_res == NULL) && (i < sc_dns->n_resolver); i++)
		{
			if (sc_dns->resolver[i] == NULL) continue;
			if (sc_dns->resolver[i]->domain == NULL) continue;
			if (!strcasecmp(name, sc_dns->resolver[i]->domain)) sc_res = sc_dns->resolver[i];
		}
	}

	if (sc_res == NULL)
	{
		dns_configuration_free(sc_dns);
		return NULL;
	}

	pdns = (pdns_handle_t *)calloc(1, sizeof(pdns_handle_t));
	if (pdns == NULL)
	{
		dns_configuration_free(sc_dns);
		return NULL;
	}

	pdns = _pdns_convert_sc(sc_res);
	
	dns_configuration_free(sc_dns);

	if (pdns == NULL) return NULL;

	if (pdns->res == NULL)
	{
		free(pdns);
		return NULL;
	}

	pdns->name = NULL;
	if (pdns->res->defdname[0] != '\0') pdns->name = strdup(pdns->res->defdname);
	else if (name != NULL) pdns->name = strdup(name);
		
	if (name != NULL) pdns->search_count = -1;

	return pdns;
}

/*
 * Open a named resolver client from file.
 */
static pdns_handle_t *
_pdns_file_open(const char *name)
{
	pdns_handle_t *pdns;
	char *path, buf[1024];
	char *p, *x, *y;
	FILE *fp;

	path = NULL;
	if (name == NULL)
	{
		asprintf(&path, "%s", _PATH_RESCONF);
	}
	else if ((name[0] == '.') || (name[0] == '/'))
	{
		asprintf(&path, "%s", name);
	}
	else
	{
		asprintf(&path, "%s/%s", DNS_RESOLVER_DIR, name);
	}

	fp = fopen(path, "r");
	free(path);
	if (fp == NULL) return NULL;
	
	pdns = _pdns_build_start(NULL);
	if (pdns == NULL) 
	{
		fclose(fp);
		return NULL;
	}

	p = getenv("RES_RETRY_TIMEOUT");
	if (p != NULL) pdns->send_timeout = atoi(p);

	p = getenv("RES_RETRY");
	if (p != NULL) pdns->res->retry= atoi(p);

	while (fgets(buf, sizeof(buf), fp) != NULL)
	{
		/* skip comments */
		if ((buf[0] == ';') || (buf[0] == '#')) continue;
		p = buf;
		x = res_next_word(&p);
		if (x == NULL) continue;
		if (!strcmp(x, "sortlist"))
		{
			while (NULL != (x = res_next_word(&p)))
			{
				_pdns_build(pdns, "sortlist", x);
			}
		}
		else if (!strcmp(x, "timeout"))
		{
			x = res_next_word(&p);
			if (x != NULL) _pdns_build(pdns, "total_timeout", x);
		}
		else if (!strcmp(x, "options"))
		{
			while (NULL != (x = res_next_word(&p)))
			{
				y = strchr(x, ':');
				if (y != NULL)
				{
					*y = '\0';
					y++;
				}
				_pdns_build(pdns, x, y);
			}
		}
		else 
		{
			y = res_next_word(&p);
			_pdns_build(pdns, x, y);

			if ((!strcmp(x, "domain")) && (pdns->name == NULL)) pdns->name = strdup(y);
		}
	}

	fclose(fp);

	if (pdns->name == NULL)
	{
		if (name == NULL) pdns->name = strdup("nil");
		else pdns->name = strdup(name);
	}

	_pdns_build_finish(pdns);

	return pdns;
}

static pdns_handle_t *
_pdns_copy(pdns_handle_t *in)
{
	pdns_handle_t *pdns;

	if (in == NULL) return NULL;

	pdns = (pdns_handle_t *)calloc(1, sizeof(pdns_handle_t));
	if (pdns == NULL) return NULL;

	memcpy(pdns, in, sizeof(pdns_handle_t));

	pdns->res = res_state_new();
	if (pdns->res == NULL)
	{
		free(pdns);
		return NULL;
	}

	memcpy(pdns->res, in->res, sizeof(struct __res_state));
	/* NB res_state_new() allocates a struct __res_state_ext for _u._ext.ext */
	if (in->res->_u._ext.ext != NULL) 
	{
		memcpy(pdns->res->_u._ext.ext, in->res->_u._ext.ext, sizeof(struct __res_state_ext));
	}

	return pdns;
}

static void
_pdns_free(pdns_handle_t *pdns)
{
	int i;

	if (pdns == NULL) return;

	if (pdns->search_count > 0)
	{
		for (i = 0; i < pdns->search_count; i++) free(pdns->search_list[i]);
		free(pdns->search_list);
	}

	if (pdns->name != NULL) free(pdns->name);
	if (pdns->res != NULL) res_client_close(pdns->res);

	free(pdns);
}

/*
 * If there was no search list, use domain name and parent domain components.
 */
static void
_pdns_check_search_list(pdns_handle_t *pdns)
{
	int n;
	char *p;
	
	if (pdns == NULL) return;
	if (pdns->name == NULL) return;
	if (pdns->search_count > 0) return;
	
	n = 1;
	for (p = pdns->name; *p != '\0'; p++) 
	{
		if (*p == '.') n++;
	}
	
	_pdns_build(pdns, "search", pdns->name);
	
	/* Include parent domains with at least LOCALDOMAINPARTS components */
	p = pdns->name;
	while (n > LOCALDOMAINPARTS)
	{
		/* Find next component */
		while ((*p != '.') && (*p != '\0')) p++;
		if (*p == '\0') break;
		p++;
		
		n--;
		_pdns_build(pdns, "search", p);
	}
}

static void
_check_cache(sdns_handle_t *sdns)
{
	int i, n, status, refresh, sc_dns_count;
	DIR *dp;
	struct direct *d;
	pdns_handle_t *c;
	dns_config_t *sc_dns;

	if (sdns == NULL) return;

	refresh = 0;

	if (sdns->stattime == 0) refresh = 1;
	
	if (refresh == 0)
	{
		if (notify_resolver_sys_config_token == -1) refresh = 1;
		else
		{
			n = 1;
			status = notify_check(notify_resolver_sys_config_token, &n);
			if ((status != NOTIFY_STATUS_OK) || (n == 1)) refresh = 1;
		}
	}

	if (refresh == 0)
	{
		if (notify_resolver_dir_token == -1) refresh = 1;
		else
		{
			n = 1;
			status = notify_check(notify_resolver_dir_token, &n);
			if ((status != NOTIFY_STATUS_OK) || (n == 1)) refresh = 1;
		}
	}
	
	if (refresh == 0) return;

	/* Free old clients */
	_pdns_free(sdns->dns_default);
	sdns->dns_default = NULL;

	for (i = 0; i < sdns->client_count; i++)
	{
		_pdns_free(sdns->client[i]);
	}
	
	sdns->client_count = 0;
	if (sdns->client != NULL) free(sdns->client);
	sdns->client = NULL;

	/* Fetch clients from System Configuration */
	sc_dns = dns_configuration_copy();

	sc_dns_count = 0;
	if ((sc_dns != NULL) && (sc_dns->n_resolver > 0))
	{
		sc_dns_count = sc_dns->n_resolver;
		sdns->dns_default = _pdns_convert_sc(sc_dns->resolver[0]);
		_pdns_check_search_list(sdns->dns_default);
	}
	else
	{
		sdns->dns_default = _pdns_file_open(_PATH_RESCONF);
	}

	if (sdns->dns_default != NULL)
	{
		if (sdns->flags & DNS_FLAG_DEBUG) sdns->dns_default->res->options |= RES_DEBUG;
		if (sdns->flags & DNS_FLAG_OK_TO_SKIP_AAAA) sdns->dns_default->flags |= DNS_FLAG_OK_TO_SKIP_AAAA;
	}

	/* Convert System Configuration resolvers */
	for (i = 1; i < sc_dns_count; i++)
	{
		c = _pdns_convert_sc(sc_dns->resolver[i]);
		if (c == NULL) continue;

		if (sdns->flags & DNS_FLAG_DEBUG) c->res->options |= RES_DEBUG;
		if (sdns->flags & DNS_FLAG_OK_TO_SKIP_AAAA) c->flags |= DNS_FLAG_OK_TO_SKIP_AAAA;

		if (sdns->client_count == 0)
		{
			sdns->client = (pdns_handle_t **)malloc(sizeof(pdns_handle_t *));
		}
		else
		{
			sdns->client = (pdns_handle_t **)realloc(sdns->client, (sdns->client_count + 1) * sizeof(pdns_handle_t *));
		}
	
		sdns->client[sdns->client_count] = c;
		sdns->client_count++;
	}

	if (sc_dns != NULL) dns_configuration_free(sc_dns);

	if (sdns->flags & DNS_FLAG_CHECK_RESOLVER_DIR)
	{
		/* Read /etc/resolvers clients */
		dp = opendir(DNS_RESOLVER_DIR);
		if (dp == NULL)
		{
			sdns->flags &= ~DNS_FLAG_CHECK_RESOLVER_DIR;
		}
		else
		{
			while (NULL != (d = readdir(dp)))
			{
				if (d->d_name[0] == '.') continue;
				
				c = _pdns_file_open(d->d_name);
				if (c == NULL) continue;
				if (sdns->flags & DNS_FLAG_DEBUG) c->res->options |= RES_DEBUG;
				if (sdns->flags & DNS_FLAG_OK_TO_SKIP_AAAA) c->flags |= DNS_FLAG_OK_TO_SKIP_AAAA;
				
				if (sdns->client_count == 0)
				{
					sdns->client = (pdns_handle_t **)malloc(sizeof(pdns_handle_t *));
				}
				else
				{
					sdns->client = (pdns_handle_t **)realloc(sdns->client, (sdns->client_count + 1) * sizeof(pdns_handle_t *));
				}
				
				sdns->client[sdns->client_count] = c;
				sdns->client_count++;
			}
			closedir(dp);
		}
	}
	
	sdns->stattime = 1;
}

static uint32_t
_pdns_get_handles_for_name(sdns_handle_t *sdns, const char *name, pdns_handle_t ***pdns)
{
	char *p, *vname;
	int i, j, k, use_default, count;

	if (sdns == NULL) return 0;
	if (pdns == NULL) return 0;
	
	_check_cache(sdns);

	use_default = 0;
	if (name == NULL) use_default = 1;
	else if (name[0] == '\0') use_default = 1;

	if (use_default == 1)
	{
		if (sdns->dns_default == NULL) return 0;

		*pdns = (pdns_handle_t **)calloc(1, sizeof(pdns_handle_t *));
		(*pdns)[0] = sdns->dns_default;
		return 1;
	}

	vname = strdup(name);
	i = strlen(vname) - 1;
	if ((i >= 0) && (vname[i] == '.'))
	{
		vname[i] = '\0';
	}

	count = 0;
	p = vname;
	while (p != NULL)
	{
		for (i = 0; i < sdns->client_count; i++)
		{
			if (!strcasecmp(sdns->client[i]->name, p))
			{
				if (count == 0)
				{
					*pdns = (pdns_handle_t **)malloc(sizeof(pdns_handle_t *));
				}
				else
				{
					*pdns = (pdns_handle_t **)realloc((*pdns), (count + 1) * sizeof(pdns_handle_t *));
				}

				/* Insert sorted by search_order */
				for (j = 0; j < count; j++)
				{
					if (sdns->client[i]->search_order < (*pdns)[j]->search_order) break;
				}

				for (k = count; k > j; k--) (*pdns)[k] = (*pdns)[k-1];
				(*pdns)[j] = sdns->client[i];
				count++;
			}
		}

		p = strchr(p, '.');
		if (p != NULL) p++;
	}

	free(vname);

	if (count != 0)  return count;

	if (sdns->dns_default == NULL) return 0;

	*pdns = (pdns_handle_t **)calloc(1, sizeof(pdns_handle_t *));
	(*pdns)[0] = sdns->dns_default;
	return 1;
}
	
static pdns_handle_t *
_pdns_default_handle(sdns_handle_t *sdns)
{
	if (sdns == NULL) return NULL;

	_check_cache(sdns);

	return sdns->dns_default;
}

static void
_pdns_process_res_search_list(pdns_handle_t *pdns)
{
	if (pdns->search_count != -1) return;
	for (pdns->search_count = 0; (pdns->res->dnsrch[pdns->search_count] != NULL) && (pdns->res->dnsrch[pdns->search_count][0] != '\0'); pdns->search_count++);
}

static char *
_pdns_search_list_domain(pdns_handle_t *pdns, uint32_t i)
{
	char *s;

	if (pdns == NULL) return NULL;
	
	if (i >= pdns->search_count) return NULL;

	s = pdns->search_list[i];
	if (s == NULL) return NULL;
	return strdup(s);
}

void
__dns_open_notify()
{
	uint32_t status, n;

	if (notify_resolver_delay_token == -1)
	{
		status = notify_register_check(DNS_DELAY_NAME, &notify_resolver_delay_token);
		if (status != NOTIFY_STATUS_OK) notify_resolver_delay_token = -1;
		else status = notify_check(notify_resolver_delay_token, &n);
	}

	if (notify_resolver_sys_config_token == -1)
	{
		status = notify_register_check(dns_configuration_notify_key(), &notify_resolver_sys_config_token);
		if (status != NOTIFY_STATUS_OK) notify_resolver_sys_config_token = -1;
	}

	if (notify_resolver_dir_token == -1)
	{
		status = notify_register_check(NOTIFY_DIR_NAME, &notify_resolver_dir_token);
		if (status == NOTIFY_STATUS_OK)
		{
			status = notify_monitor_file(notify_resolver_dir_token, "/private/etc/resolver", 0);
			if (status != NOTIFY_STATUS_OK)
			{
				notify_cancel(notify_resolver_dir_token);
				notify_resolver_dir_token = -1;
			}
		}
		else 
		{
			notify_resolver_dir_token = -1;
		}
	}
}

void
__dns_close_notify()
{
	if (notify_resolver_sys_config_token != -1) notify_cancel(notify_resolver_sys_config_token);
	notify_resolver_sys_config_token = -1;

	if (notify_resolver_dir_token != -1) notify_cancel(notify_resolver_dir_token);
	notify_resolver_dir_token = -1;
}

dns_handle_t
dns_open(const char *name)
{
	dns_private_handle_t *dns;

	dns = (dns_private_handle_t *)calloc(1, sizeof(dns_private_handle_t));
	if (dns == NULL) return NULL;

	if (name == NULL)
	{
		dns->handle_type = DNS_PRIVATE_HANDLE_TYPE_SUPER;
		dns->sdns = (sdns_handle_t *)calloc(1, sizeof(sdns_handle_t));
		if (dns->sdns == NULL)
		{
			free(dns);
			return NULL;
		}

		dns->sdns->flags |= DNS_FLAG_CHECK_RESOLVER_DIR;

		return (dns_handle_t)dns;
	}

	dns->handle_type = DNS_PRIVATE_HANDLE_TYPE_PLAIN;

	/*  Look for name in System Configuration first */
	dns->pdns = _pdns_sc_open(name);
	if (dns->pdns == NULL) dns->pdns = _pdns_file_open(name);

	if (dns->pdns == NULL)
	{
		free(dns);
		return NULL;
	}

	return (dns_handle_t)dns;
}

/*
 * Release a DNS client handle
 */
void
dns_free(dns_handle_t d)
{
	dns_private_handle_t *dns;
	int i;

	if (d == NULL) return;

	dns = (dns_private_handle_t *)d;

	if (dns->recvbuf != NULL) free(dns->recvbuf);

	if (dns->handle_type == DNS_PRIVATE_HANDLE_TYPE_SUPER)
	{
		if (dns->sdns == NULL) return;

		_pdns_free(dns->sdns->dns_default);

		for (i = 0; i < dns->sdns->client_count; i++)
		{
			_pdns_free(dns->sdns->client[i]);
		}

		dns->sdns->client_count = 0;
		if (dns->sdns->client != NULL) free(dns->sdns->client);

		free(dns->sdns);
	}
	else
	{
		_pdns_free(dns->pdns);
	}

	free(dns);
}

static void
_pdns_debug(pdns_handle_t *pdns, uint32_t flag)
{
	if (pdns == NULL) return;

	if (flag == 0)
	{
		pdns->res->options &= ~RES_DEBUG;
	}
	else
	{
		pdns->res->options |= RES_DEBUG;
	}
}

static void
_sdns_debug(sdns_handle_t *sdns, uint32_t flag)
{
	int i;

	if (sdns == NULL) return;

	if (flag == 0)
	{
		sdns->flags &= ~ DNS_FLAG_DEBUG;

		if (sdns->dns_default != NULL)
		{
			sdns->dns_default->res->options &= ~RES_DEBUG;
		}

		for (i = 0; i < sdns->client_count; i++)
		{
			sdns->client[i]->res->options &= ~RES_DEBUG;
		}
	}
	else
	{
		sdns->flags |= DNS_FLAG_DEBUG;

		if (sdns->dns_default != NULL)
		{
			sdns->dns_default->res->options |= RES_DEBUG;
		}

		for (i = 0; i < sdns->client_count; i++)
		{
			sdns->client[i]->res->options |= RES_DEBUG;
		}
	}
}

/*
 * Enable / Disable debugging
 */
void
dns_set_debug(dns_handle_t d, uint32_t flag)
{
	dns_private_handle_t *dns;

	if (d == NULL) return;

	dns = (dns_private_handle_t *)d;

	if (dns->handle_type == DNS_PRIVATE_HANDLE_TYPE_SUPER)
	{
		_sdns_debug(dns->sdns, flag);
	}
	else
	{
		_pdns_debug(dns->pdns, flag);
	}
}

/*
 * Returns the number of names in the search list
 */
uint32_t
dns_search_list_count(dns_handle_t d)
{
	dns_private_handle_t *dns;
	pdns_handle_t *pdns;

	if (d == NULL) return 0;

	dns = (dns_private_handle_t *)d;

	if (dns->handle_type == DNS_PRIVATE_HANDLE_TYPE_SUPER)
	{
		pdns = _pdns_default_handle(dns->sdns);
	}
	else
	{
		pdns = dns->pdns;
	}

	return pdns->search_count;
}

/* 
 * Returns the domain name at index i in the search list.
 * Returns NULL if there are no names in the search list.
 * Caller must free the returned name.
 */
char *
dns_search_list_domain(dns_handle_t d, uint32_t i)
{
	dns_private_handle_t *dns;
	pdns_handle_t *pdns;

	if (d == NULL) return NULL;

	dns = (dns_private_handle_t *)d;

	if (dns->handle_type == DNS_PRIVATE_HANDLE_TYPE_SUPER)
	{
		pdns = _pdns_default_handle(dns->sdns);
	}
	else
	{
		pdns = dns->pdns;
	}

	return _pdns_search_list_domain(pdns, i);
}

static int
_pdns_delay()
{
	int status, n, snooze;
	time_t tick;

	snooze = 0;
	n = 0;

	/* No delay if we are not receiving notifications */
	if (notify_resolver_delay_token == -1) return 0;

	if (dns_delay == 0)
	{
		status = notify_check(notify_resolver_delay_token, &n);
		if ((status == NOTIFY_STATUS_OK) && (n == 1))
		{
			/*
			 * First thread to hit this condition sleeps for DNS_DELAY_INTERVAL seconds
			 */
			dns_delay = time(NULL) + DNS_DELAY_INTERVAL;
			snooze = DNS_DELAY_INTERVAL;
		}
	}
	else
	{
		tick = time(NULL);
		/*
		 * Subsequent threads sleep for the remaining duration.
		 * We add one to round up the interval since our garnularity is coarse.
		 */
		snooze = 1 + (dns_delay - tick);
		if (snooze < 0) snooze = 0;
	}

	if (snooze == 0) return 0;

	sleep(snooze);

	/* When exiting, first thread in resets the delay condition */
	if (n == 1) dns_delay = 0;

	return 0;
}

static int
_pdns_query(pdns_handle_t *pdns, const char *name, uint32_t class, uint32_t type, char *buf, uint32_t len, struct sockaddr *from, int *fromlen)
{
	if (name == NULL) return -1;
	if (pdns == NULL) return -1;
	if (pdns->res == NULL) return -1;
	if (pdns->res->nscount == 0) return -1;

	if ((type == ns_t_aaaa) && ((pdns->flags & DNS_FLAG_HAVE_IPV6_SERVER) == 0) && (pdns->flags & DNS_FLAG_OK_TO_SKIP_AAAA)) return -1;

	_pdns_delay();

	/* BIND_9 API */
	return res_nquery_2(pdns->res, name, class, type, buf, len, from, fromlen);
}

static int
_pdns_search(pdns_handle_t *pdns, const char *name, uint32_t class, uint32_t type, char *buf, uint32_t len, struct sockaddr *from, int *fromlen)
{
	char *dot, *qname;
	int append, status;

	if (name == NULL) return -1;
	if (pdns == NULL) return -1;
	if (pdns->res == NULL) return -1;
	if (pdns->res->nscount == 0) return -1;

	if ((type == ns_t_aaaa) && ((pdns->flags & DNS_FLAG_HAVE_IPV6_SERVER) == 0) && (pdns->flags & DNS_FLAG_OK_TO_SKIP_AAAA)) return -1;

	qname = NULL;
	append = 1;

	/*
	 * don't append my name if:
	 * - my name is NULL
	 * - input name is qualified (i.e. not single component)
	 * - there is a search list
	 * - there is a domain name
	 */

	if (pdns->name == NULL) append = 0;

	if (append == 1)
	{
		dot = strrchr(name, '.');
		if (dot != NULL) append = 0;
	}

	if (append == 1)
	{
		_pdns_process_res_search_list(pdns);
		if (pdns->search_count > 0) append = 0;
	}

	if ((append == 1) && (pdns->res->defdname != NULL) && (pdns->res->defdname[0] != '\0')) append = 0;

	status = -1;
	if (append == 0)
	{
		/* BIND_9 API */
		_pdns_delay();

		status = __res_nsearch_list_2(pdns->res, name, class, type, buf, len, from, fromlen, pdns->search_count, pdns->search_list);
	}
	else
	{
		_pdns_delay();

		asprintf(&qname, "%s.%s.", name, pdns->name);
		/* BIND_9 API */
		status = res_nsearch_2(pdns->res, qname, class, type, buf, len, from, fromlen);
		free(qname);
	}

	return status;
}

static int
_sdns_send(sdns_handle_t *sdns, const char *name, uint32_t class, uint32_t type, uint32_t fqdn, char *buf, uint32_t len, struct sockaddr *from, uint32_t *fromlen)
{
	char *qname;
	pdns_handle_t **pdns;
	uint32_t pdns_count;
	int i, n;

	pdns = NULL;
	pdns_count = 0;
	n = -1;

	pdns_count = _pdns_get_handles_for_name(sdns, name, &pdns);

	if (pdns_count == 0) return -1;

	qname = NULL;
	asprintf(&qname, "%s%s", name, (fqdn == 0) ? "." : "");

	for (i = 0; i < pdns_count; i++)
	{
		n = _pdns_query(pdns[i], qname, class, type, buf, len, from, fromlen);
		if (n > 0) break;
	}

	free(pdns);
	free(qname);
	return n;
}

static int
_sdns_search(sdns_handle_t *sdns, const char *name, uint32_t class, uint32_t type, uint32_t fqdn, uint32_t recurse, char *buf, uint32_t len, struct sockaddr *from, uint32_t *fromlen)
{
	pdns_handle_t *pdns;
	int i, j, n, ndots, status;
	char *dot, *qname, **search_domain;

	/*
	 * If name is qualified:
	 *    If we have a client for the specified domain, use it.
	 *    Else use default client.
	 * If name is unqualified:
	 *    If there is a search list:
	 *        for each domain in default search list,
	 *            call sdns_query with the name qualified with that domain.
	 *    Else
	 *         call sdns_query with the name qualified with default domain name.
	 */

	if (sdns == NULL) return -1;
	if (name == NULL) return -1;

	ndots = 1;
	pdns = _pdns_default_handle(sdns);
	if ((pdns != NULL) && (pdns->res != NULL)) ndots = pdns->res->ndots;

	n = 0;
	dot = NULL;

	for (i = 0; name[i] != '\0'; i++)
	{
		if (name[i] == '.')
		{
			n++;
			dot = (char *)(name + i);
		}
	}

	if ((fqdn == 0 ) && (dot != NULL) && (*(dot + 1) == '\0')) fqdn = 1;

	/* If name is partly qualified: */
	if (n >= ndots)
	{
		status = _sdns_send(sdns, name, class, type, fqdn, buf, len, from, fromlen);
		if (status > 0) return status;
		if (fqdn == 1) return -1;
	}

	if (recurse == 0) return -1;

	pdns = _pdns_default_handle(sdns);
	if (pdns == NULL) return -1;

	n = pdns->search_count;
	if (n > 0)
	{
		/* We need to copy the search list since _sdns_search() can change the cache */
		search_domain = calloc(n, sizeof(char *));
		if (search_domain == NULL) return -1;

		for (i = 0; i < n ; i++) search_domain[i] = strdup(pdns->search_list[i]);

		for (i = 0; i < n ; i++)
		{
			asprintf(&qname, "%s.%s", name, search_domain[i]);
			status = _sdns_search(sdns, qname, class, type, fqdn, 0, buf, len, from, fromlen);
			free(qname);
			if (status > 0)
			{
				for (j = 0; j < n ; j++) free(search_domain[j]);
				free(search_domain);
				return status;
			}
		}

		for (i = 0; i < n ; i++) free(search_domain[i]);
		free(search_domain);

		return -1;
	}
	
	if (pdns->name == NULL) asprintf(&qname, "%s", name);
	else asprintf(&qname, "%s.%s", name, pdns->name);
	status = _sdns_search(sdns, qname, class, type, fqdn, 0, buf, len, from, fromlen);
	free(qname);
	return status;
}

int
dns_query(dns_handle_t d, const char *name, uint32_t class, uint32_t type, char *buf, uint32_t len, struct sockaddr *from, uint32_t *fromlen)
{
	dns_private_handle_t *dns;
	int status;

	if (d == NULL) return -1;
	if (name == NULL) return -1;
	dns = (dns_private_handle_t *)d;

	status = -1;
	if (dns->handle_type == DNS_PRIVATE_HANDLE_TYPE_SUPER)
	{
		status = _sdns_search(dns->sdns, name, class, type, 1, 1, buf, len, from, fromlen);
	}
	else
	{
		status = _pdns_query(dns->pdns, name, class, type, buf, len, from, fromlen);
	}

	return status;
}


int
dns_search(dns_handle_t d, const char *name, uint32_t class, uint32_t type, char *buf, uint32_t len, struct sockaddr *from, uint32_t *fromlen)
{
	dns_private_handle_t *dns;
	int status;

	if (d == NULL) return -1;
	if (name == NULL) return -1;
	dns = (dns_private_handle_t *)d;

	status = -1;
	if (dns->handle_type == DNS_PRIVATE_HANDLE_TYPE_SUPER)
	{
		status = _sdns_search(dns->sdns, name, class, type, 0, 1, buf, len, from, fromlen);
	}
	else
	{
		status = _pdns_search(dns->pdns, name, class, type, buf, len, from, fromlen);
	}

	return status;
}

/*
 * PRIVATE
 */

dns_handle_t *
dns_clients_for_name(dns_handle_t d, const char *name)
{
	dns_private_handle_t *dns, **c;
	pdns_handle_t **x;
	uint32_t i, count;

	if (d == NULL) return NULL;
	dns = (dns_private_handle_t *)d;

	if (dns->handle_type != DNS_PRIVATE_HANDLE_TYPE_SUPER) return NULL;

	/* Get handles for name */
	count = _pdns_get_handles_for_name(dns->sdns, name, &x);
	if (count == 0) return NULL;

	c = (dns_private_handle_t **)calloc(count + 1, sizeof(dns_private_handle_t *));
	if (c == NULL) return NULL;

	for (i = 0; i < count; i++)
	{
		c[i] = (dns_private_handle_t *)calloc(1, sizeof(dns_private_handle_t));
		c[i]->handle_type = DNS_PRIVATE_HANDLE_TYPE_PLAIN;
		c[i]->pdns = _pdns_copy(x[i]);
	}

	return (dns_handle_t *)c;
}

uint32_t
dns_server_list_count(dns_handle_t d)
{
	dns_private_handle_t *dns;
	res_state r;

	if (d == NULL) return 0;
	dns = (dns_private_handle_t *)d;

	if (dns->handle_type != DNS_PRIVATE_HANDLE_TYPE_PLAIN) return 0;

	if (dns->pdns == NULL) return 0;

	r = dns->pdns->res;
	if (r == NULL) return 0;

	return r->nscount;
}

struct sockaddr *
dns_server_list_address(dns_handle_t d, uint32_t i)
{
	dns_private_handle_t *dns;
	res_state r;
	struct sockaddr_storage *s;
	struct sockaddr *sa;

	if (d == NULL) return NULL;
	dns = (dns_private_handle_t *)d;

	if (dns->handle_type != DNS_PRIVATE_HANDLE_TYPE_PLAIN) return NULL;

	if (dns->pdns == NULL) return NULL;

	r = dns->pdns->res;
	if (r == NULL) return NULL;

	if (i >= r->nscount) return NULL;
	sa = get_nsaddr(r, i);
	if (sa == NULL) return NULL;

	s = (struct sockaddr_storage *)calloc(1, sizeof(struct sockaddr_storage));
	memcpy(s, sa, sizeof(struct sockaddr_storage));
	return (struct sockaddr *)s;
}
