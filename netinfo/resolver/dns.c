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
#include <notify.h>
#include "dns_private.h"
#include "res_private.h"

#define DNS_RESOLVER_DIR "/etc/resolver"

#define RESOLV1_NOTIFY_NAME "com.apple.system.dns.resolv.conf"
#define RESOLV2_NOTIFY_NAME "com.apple.system.dns.resolver"

extern uint32_t notify_monitor_file(int token, const char *name, int flags);

static uint32_t resolv1_token = -1;
static uint32_t resolv2_token = -1;

#define DNS_PRIVATE_HANDLE_TYPE_SUPER 0
#define DNS_PRIVATE_HANDLE_TYPE_PLAIN 1
#define DNS_DEFAULT_RECEIVE_SIZE 1024

#define SDNS_DEFAULT_STAT_LATENCY 10

extern void res_client_close(res_state res);
extern res_state res_client_open(char *path);
extern int res_nquery_2(res_state statp, const char *name, int class, int type, u_char *answer, int anslen, struct sockaddr *from, int *fromlen);
extern int res_nsearch_2(res_state statp, const char *name, int class, int type, u_char *answer, int anslen, struct sockaddr *from, int *fromlen);

/*
 * Open a named resolver client.
 */
static pdns_handle_t *
_pdns_open(const char *name)
{
	pdns_handle_t *pdns;
	char *path;
	int status;
	struct stat sb;
	struct timeval now;

	memset(&sb, 0, sizeof(struct stat));

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

	status = stat(path, &sb);
	if (status < 0)
	{
		free(path);
		return NULL;
	}

	pdns = (pdns_handle_t *)calloc(1, sizeof(pdns_handle_t));
	if (pdns == NULL)
	{
		free(path);
		return NULL;
	}

	pdns->res = res_client_open(path);
	
	if (pdns->res == NULL)
	{
		free(path);
		free(pdns);
		return NULL;
	}

	pdns->source = path;

	pdns->name = NULL;
	if (pdns->res->defdname[0] != '\0') pdns->name = strdup(pdns->res->defdname);
	else if (name != NULL) pdns->name = strdup(name);

	gettimeofday(&now, NULL);
	pdns->modtime = sb.st_mtimespec.tv_sec;
	pdns->stattime = now.tv_sec;

	pdns->search_count = -1;

	return pdns;
}

static pdns_handle_t *
_pdns_copy(pdns_handle_t *in)
{
	pdns_handle_t *pdns;
	char *path;
	int status;
	struct stat sb;
	struct timeval now;

	if (in == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));

	path = NULL;
	if (in->source != NULL) path = strdup(in->source);

	if (path == NULL)
	{
		if (in->name == NULL)
		{
			asprintf(&path, "%s", _PATH_RESCONF);
		}
		else if ((in->name[0] == '.') || (in->name[0] == '/'))
		{
			asprintf(&path, "%s", in->name);
		}
		else
		{
			asprintf(&path, "%s/%s", DNS_RESOLVER_DIR, in->name);
		}
	}

	if (path == NULL) return NULL;

	status = stat(path, &sb);
	if (status < 0)
	{
		free(path);
		return NULL;
	}

	pdns = (pdns_handle_t *)calloc(1, sizeof(pdns_handle_t));
	if (pdns == NULL) return NULL;

	pdns->res = res_client_open(path);
	free(path);
	if (pdns->res == NULL)
	{
		free(pdns);
		return NULL;
	}

	pdns->name = NULL;
	if (in->name != NULL) pdns->name = strdup(in->name);
	else if (pdns->res->defdname[0] != '\0') pdns->name = strdup(pdns->res->defdname);

	gettimeofday(&now, NULL);
	pdns->modtime = sb.st_mtimespec.tv_sec;
	pdns->stattime = now.tv_sec;

	pdns->search_count = -1;

	return pdns;
}

static void
_pdns_free(pdns_handle_t *pdns)
{
	if (pdns == NULL) return;

	if (pdns->name != NULL) free(pdns->name);
	if (pdns->source != NULL) free(pdns->source);
	if (pdns->res != NULL) res_client_close(pdns->res);

	free(pdns);
}

static void
_check_cache(sdns_handle_t *sdns)
{
	struct stat sb;
	int i, n, status;
	DIR *dp;
	struct direct *d;
	pdns_handle_t *c;
	struct timeval now;

	if (sdns == NULL) return;

	if ((sdns->stattime != 0) && (resolv1_token != -1) && (resolv2_token != -1))
	{
		n = 1;
		status = notify_check(resolv1_token, &n);
		if ((status == NOTIFY_STATUS_OK) && (n == 0))
		{
			n = 1;
			status = notify_check(resolv2_token, &n);
			if ((status == NOTIFY_STATUS_OK) && (n == 0)) return;
		}
	}

	gettimeofday(&now, NULL);

	if (sdns->dns_default != NULL)
	{
		/* Check default (/etc/resolv.conf) client */
		memset(&sb, 0, sizeof(struct stat));
		status = stat(_PATH_RESCONF, &sb);
		if ((status != 0) || (sb.st_mtimespec.tv_sec > sdns->dns_default->modtime))
		{
			_pdns_free(sdns->dns_default);
			sdns->dns_default = NULL;
		}
		else if (status == 0)
		{
			sdns->dns_default->stattime = now.tv_sec;
		}
	}
	
	if (sdns->dns_default == NULL)
	{
		sdns->dns_default = _pdns_open(NULL);
		if ((sdns->dns_default != NULL) && (sdns->flags & DNS_FLAG_DEBUG)) sdns->dns_default->res->options |= RES_DEBUG;
	}
	
	/* Check /etc/resolver clients */
	memset(&sb, 0, sizeof(struct stat));
	status = stat(DNS_RESOLVER_DIR, &sb);
	if ((status != 0) || (sb.st_mtimespec.tv_sec > sdns->modtime))
	{
		for (i = 0; i < sdns->client_count; i++)
		{
			_pdns_free(sdns->client[i]);
		}

		sdns->client_count = 0;
		if (sdns->client != NULL) free(sdns->client);
		sdns->client = NULL;
	}

	if ((status == 0) && (sb.st_mtimespec.tv_sec > sdns->modtime))
	{
		dp = opendir(DNS_RESOLVER_DIR);
		if (dp != NULL)
		{
			while (NULL != (d = readdir(dp)))
			{
				if (d->d_name[0] == '.') continue;

				c = _pdns_open(d->d_name);
				if (c == NULL) continue;
				if (sdns->flags & DNS_FLAG_DEBUG) c->res->options |= RES_DEBUG;

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
			
		sdns->modtime = sb.st_mtimespec.tv_sec;
		sdns->stattime = now.tv_sec;
	}
	else if (status == 0)
	{
		sdns->stattime = now.tv_sec;
	}
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
					if (sdns->client[i]->res->_u._ext.ext->search_order < (*pdns)[j]->res->_u._ext.ext->search_order) break;
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

	if (sdns->dns_default == NULL)
	{
		sdns->dns_default = _pdns_open(NULL);
		if (sdns->dns_default == NULL) return 0;

		if (sdns->flags & DNS_FLAG_DEBUG) sdns->dns_default->res->options |= RES_DEBUG;
	}

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

static uint32_t
_pdns_search_list_domain_count(pdns_handle_t *pdns)
{
	if (pdns == NULL) return 0;
	
	_pdns_process_res_search_list(pdns);
	return (pdns->search_count);
}

static char *
_pdns_search_list_domain(pdns_handle_t *pdns, uint32_t i)
{
	char *s;

	if (pdns == NULL) return NULL;
	
	_pdns_process_res_search_list(pdns);
	if (i >= pdns->search_count) return NULL;

	s = pdns->res->dnsrch[i];
	if (s == NULL) return NULL;
	return strdup(s);
}

void
__dns_open_notify()
{
	uint32_t status;

	if (resolv1_token == -1)
	{
		status = notify_register_check(RESOLV1_NOTIFY_NAME, &resolv1_token);
		if (status == NOTIFY_STATUS_OK)
		{
			status = notify_monitor_file(resolv1_token, "/var/run/resolv.conf", 0);
			if (status != NOTIFY_STATUS_OK)
			{
				notify_cancel(resolv1_token);
				resolv1_token = -1;
			}
		}
		else 
		{
			resolv1_token = -1;
		}
	}

	if ((resolv1_token != -1 ) && (resolv2_token == -1))
	{
		status = notify_register_check(RESOLV2_NOTIFY_NAME, &resolv2_token);
		if (status == NOTIFY_STATUS_OK)
		{
			status = notify_monitor_file(resolv2_token, "/private/etc/resolver", 0);
			if (status != NOTIFY_STATUS_OK)
			{
				notify_cancel(resolv1_token);
				notify_cancel(resolv2_token);
				resolv1_token = -1;
				resolv2_token = -1;
			}
		}
		else 
		{
			notify_cancel(resolv1_token);
			resolv1_token = -1;
			resolv2_token = -1;
		}
	}
}

void
__dns_close_notify()
{
	if (resolv1_token != -1) notify_cancel(resolv1_token);
	resolv1_token = -1;

	if (resolv2_token != -1) notify_cancel(resolv2_token);
	resolv2_token = -1;
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

		dns->sdns->stat_latency = SDNS_DEFAULT_STAT_LATENCY;
		return (dns_handle_t)dns;
	}

	dns->handle_type = DNS_PRIVATE_HANDLE_TYPE_PLAIN;
	dns->pdns = _pdns_open(name);
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

	return _pdns_search_list_domain_count(pdns);
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
_pdns_query(pdns_handle_t *pdns, const char *name, uint32_t class, uint32_t type, char *buf, uint32_t len, struct sockaddr *from, int *fromlen)
{
	if (pdns == NULL) return -1;
	if (pdns->res == NULL) return -1;

	return res_nquery_2(pdns->res, name, class, type, buf, len, from, fromlen);
}

static int
_pdns_search(pdns_handle_t *pdns, const char *name, uint32_t class, uint32_t type, char *buf, uint32_t len, struct sockaddr *from, int *fromlen)
{
	char *dot, *qname;
	uint32_t append, status;

	if (pdns->res == NULL) return -1;
	if (name == NULL) return -1;

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
		status = res_nsearch_2(pdns->res, name, class, type, buf, len, from, fromlen);
	}
	else
	{
		asprintf(&qname, "%s.%s.", name, pdns->name);
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
	int i, n, status;
	char *dot, *s, *qname;

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

	if (sdns == NULL) return NULL;
	if (name == NULL) return NULL;

	dot = strrchr(name, '.');

	if ((fqdn == 0 ) && (dot != NULL) && (*(dot + 1) == '\0')) fqdn = 1;

	/* If name is qualified: */
	if (dot != NULL)
	{
		status = _sdns_send(sdns, name, class, type, fqdn, buf, len, from, fromlen);
		if (status > 0) return status;
		if (fqdn == 1) return -1;
	}

	if (recurse == 0) return -1;

	pdns = _pdns_default_handle(sdns);
	if (pdns == NULL) return -1;

	n = _pdns_search_list_domain_count(pdns);
	if (n > 0)
	{
		for (i = 0; i < n ; i++)
		{
			s = _pdns_search_list_domain(pdns, i);
			if (s == NULL) continue;

			asprintf(&qname, "%s.%s", name, s);
			free(s);
			status = _sdns_search(sdns, qname, class, type, fqdn, 0, buf, len, from, fromlen);
			free(qname);
			if (status > 0) return status;
		}

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
