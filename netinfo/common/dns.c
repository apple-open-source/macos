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

/*
 * Thread-safe DNS client library
 *
 * Copyright (c) 1998 Apple Computer Inc.  All Rights Reserved.
 * Written by Marc Majka
 */

#include <netinfo/ni.h>
#include <stdio.h>
#include <string.h>
#include <libc.h>
#include <unistd.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/syslog.h>
#include <errno.h>
#include <sys/types.h>

#include <NetInfo/dns.h>
#include <NetInfo/system.h>
#include <NetInfo/syslock.h>

#define REPLY_BUF_SIZE 1024

#define NI_LOCATION_RESOLVER_OLD "/locations/resolver"
#define NI_LOCATION_RESOLVER "/domains"
#define FF_LOCATION_RESOLVER "/etc/resolv.conf"
#define NI_TIMEOUT 10

#define DNS_SERVER_TIMEOUT 2
#define DNS_SERVER_RETRIES 3

#define PLX_DOMAIN 0
#define PLX_NAMESERVER 1
#define PLX_SEARCH 2
#define PLX_DEBUG 3
#define PLX_NDOTS 4
#define PLX_PROTOCOL 5
#define PLX_SORTLIST 6
#define PLX_LENGTH 7

#define DNS_SERVICE_PORT 53

dns_handle_t *dns_open_lock(char *, u_int32_t);

static syslock *_dnsLock = NULL;
static syslock *_logLock = NULL;

#ifdef _UNIX_BSD_43_
extern char *strdup(char *s);
#endif

void
dns_log_msg(dns_handle_t *dns, int priority, char *message, ...)
{
	va_list ap;
	char *p, buf[2048];

	if (dns == NULL) return;

	if (_logLock == NULL)
	{
		_logLock = syslock_new(FALSE);
	}

	syslock_lock(_logLock);

	
	if (dns->log_dest & DNS_LOG_SYSLOG)
	{
		va_start(ap, message);
		vsyslog(priority, message, ap);
		va_end(ap);
	}

	if (dns->log_dest & DNS_LOG_FILE)
	{
		if (dns->log_title == NULL)
			fprintf(dns->log_file, "DNS Client: ");
		else
			fprintf(dns->log_file, "%s: ", dns->log_title);
	
		va_start(ap, message);
		vfprintf(dns->log_file, message, ap);
		fprintf(dns->log_file, "\n");
		fflush(dns->log_file);
		va_end(ap);
	}

	if (dns->log_dest & DNS_LOG_STDERR)
	{
		if (dns->log_title == NULL)
			fprintf(stderr, "DNS Client: ");
		else
			fprintf(stderr, "%s: ", dns->log_title);
	
		va_start(ap, message);
		vfprintf(stderr, message, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}

	if (dns->log_dest & DNS_LOG_CALLBACK)
	{
		if (dns->log_title == NULL)
		{
			sprintf(buf, "DNS Client: ");
			p = buf + 12;
		}
		else
		{
			sprintf(buf, "%s: ", dns->log_title);
			p = buf + strlen(dns->log_title) + 2;
		}
	
		va_start(ap, message);
		vsprintf(p, message, ap);
		va_end(ap);
		(*dns->log_callback)(priority, buf);
	}

	syslock_unlock(_logLock);
}

static void
_dns_lock(void)
{
	if (_dnsLock == NULL)
	{
		_dnsLock = syslock_new(FALSE);
	}

	syslock_lock(_dnsLock);
}

static void
_dns_unlock(void)
{
	syslock_unlock(_dnsLock);
}

void
dns_log_open_syslog(dns_handle_t *dns, char *title, int flags, int facility)
{
	if (dns == NULL) return;

	if (title == NULL) openlog("DNS Client", flags, facility);
	else openlog(title, flags, facility);
	dns->log_dest |= DNS_LOG_SYSLOG;
}

void
dns_log_close_syslog(dns_handle_t *dns)
{
	if (dns == NULL) return;
	dns->log_dest &= ~DNS_LOG_SYSLOG;
}

void
dns_log_close_file(dns_handle_t *dns)
{
	if (dns == NULL) return;
	if (dns->log_file != NULL) fclose(dns->log_file);
	dns->log_file = NULL;
	dns->log_dest &= ~DNS_LOG_FILE;
}

void
dns_log_open_file(dns_handle_t *dns, char *title, char *name, char *mode)
{
	if (dns == NULL) return;

	if (dns->log_file != NULL) dns_log_close_file(dns);

	dns->log_file = fopen(name, mode);
	if (title != NULL)
	{
		if (dns->log_title != NULL) free(dns->log_title);
		dns->log_title = strdup(title);
	}
}

void
dns_open_log(dns_handle_t *dns, char *title, int dest, FILE *file, int flags, int facility, int (*callback)(int, char *))
{
	if (dns == NULL) return;

	if (title != NULL)
	{
		if (dns->log_title != NULL) free(dns->log_title);
		dns->log_title = strdup(title);
	}

	dns->log_dest = dest;

	if (dest & DNS_LOG_FILE)
	{
		if (file == NULL) dns->log_dest &= ~DNS_LOG_FILE;
		else
		{
			if (dns->log_file != NULL) fclose(dns->log_file);
			dns->log_file = file;
		}
	}

	if (dest & DNS_LOG_SYSLOG)
	{
		dns_log_open_syslog(dns, title, flags, facility);
	}

	if (dest & DNS_LOG_CALLBACK)
	{
		if (callback == NULL) dns->log_dest &= ~DNS_LOG_CALLBACK;
		else dns->log_callback = callback;
	}
}

/*
 * Find a directory in the lookup chain and return the domain and
 * directory ID.
 */
static ni_status
_dns_ni_find(char *dirname, void **dom, ni_id *nid, u_int32_t timeout)
{
	void *d, *p;
	ni_id n;
	ni_status status;

	*dom = NULL;
	nid->nii_object = NI_INDEX_NULL;
	nid->nii_instance = NI_INDEX_NULL;
	
	status = ni_open(NULL, ".", &d);
	if (status != NI_OK) return status;

	if (timeout > 0)
	{
		ni_setreadtimeout(d, timeout);
		ni_setabort(d, 1);
	}

	while (d != NULL)
	{
		status = ni_pathsearch(d, &n, dirname);
		if (status == NI_OK)
		{
			*dom = d;
			*nid = n;
			return NI_OK;
		}
	
		status = ni_open(d, "..", &p);
		ni_free(d);
		d = NULL;
		if (status == NI_OK) d = p;
	}
	
	return NI_NODIR;
}

static u_int16_t
_dns_port(u_int32_t proto)
{
	/* Can't do getservbyname() since that would call lookupd! */
	return htons(DNS_SERVICE_PORT);
}

/*
 * Use default domain to lookup nameserver records for another domain
 */
static ni_proplist *
_dns_dns_init(char *dom, u_int32_t timeout)
{
	ni_proplist *p;
	ni_property *dp;
	ni_namelist *nl;
	char s[32];
	int i, len;
	dns_reply_t *r;
	dns_question_t dnsq;
	dns_handle_t *t;

	if (dom == NULL) return NULL;

	t = dns_open_lock(NULL, 0);
	if (t == NULL) return NULL;

	len = strlen(dom);
	dnsq.name = malloc(len + 2);
	memmove(dnsq.name, dom, len);
	dnsq.name[len] = '.';
	dnsq.name[len + 1] = '\0';
	dnsq.class = DNS_CLASS_IN;
	dnsq.type = DNS_TYPE_NS;

	r = dns_query(t, &dnsq);
	free(dnsq.name);
	dns_free(t);

	if (r == NULL) return NULL;

	if ((r->header->ancount == 0) || (r->header->arcount == 0))
	{
		dns_free_reply(r);
		return NULL;
	}

	p = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(p);

	p->ni_proplist_len = 2;

	dp = (ni_property *)malloc(p->ni_proplist_len * sizeof(ni_property));
	NI_INIT(dp);
	
	dp[PLX_DOMAIN].nip_name = strdup("domain");
	dp[PLX_DOMAIN].nip_val.ni_namelist_len = 0;
	dp[PLX_DOMAIN].nip_val.ni_namelist_val = NULL;
	
	dp[PLX_NAMESERVER].nip_name = strdup("nameserver");
	dp[PLX_NAMESERVER].nip_val.ni_namelist_len = 0;
	dp[PLX_NAMESERVER].nip_val.ni_namelist_val = NULL;

	p->ni_proplist_val = dp;

	nl = &(dp[PLX_DOMAIN].nip_val);
	nl->ni_namelist_len = 1;
	nl->ni_namelist_val = malloc(sizeof(char *));
	nl->ni_namelist_val[0] = strdup(dom);

	nl = &(dp[PLX_NAMESERVER].nip_val);
	nl->ni_namelist_val = malloc(r->header->arcount * sizeof(char *));
	nl->ni_namelist_len = r->header->arcount;

	for (i = 0; i < r->header->arcount; i++)
	{
		sprintf(s, "%s", inet_ntoa(r->additional[i]->data.A->addr));
		nl->ni_namelist_val[i] = strdup(s);
	}

	dns_free_reply(r);
	return p;
}

static ni_proplist *
_dns_ni_init(char *dom, u_int32_t timeout)
{
	void *d;
	ni_id nid;
	ni_status status;
	ni_index where;
	ni_proplist *p;
	ni_namelist *nl;
	char *s = NULL;
	char *domainname, *hname, *dot;
	int using_domains_dir;

	using_domains_dir = 0;

	p = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(p);

	status = NI_NODIR;

	if (dom != NULL)
	{
		/* dom is given, use /domains/<dom> */
		using_domains_dir = 1;
		s = malloc(strlen(NI_LOCATION_RESOLVER) + strlen(dom) + 2);
		sprintf(s, "%s/%s", NI_LOCATION_RESOLVER, dom);
		status = _dns_ni_find(s, &d, &nid, timeout);
		free(s);
		if (status != NI_OK)
		{
			ni_proplist_free(p);
			free(p);
			return NULL;
		}
	}
	else
	{
		/* Find /domains and use the value of the "domain" property */
		status = _dns_ni_find(NI_LOCATION_RESOLVER, &d, &nid, timeout);
		if (status == NI_OK)
		{
			status = ni_read(d, &nid, p);
			if (status == NI_OK)
			{
				where = ni_proplist_match(*p, "domain", NULL);
				if (where == NI_INDEX_NULL) status = NI_NOPROP;
				else
				{
					nl = &(p->ni_proplist_val[where].nip_val);
					if (nl->ni_namelist_len == 0)
					{
						status = NI_NONAME;
					}
					else
					{
						using_domains_dir = 1;
						status = ni_pathsearch(d, &nid, nl->ni_namelist_val[0]);
					}
				}
			}
			ni_proplist_free(p);
		}
	}
	
	if (status != NI_OK)
	{
		/*
		 * try /locations/resolver
		 */
		status = _dns_ni_find(NI_LOCATION_RESOLVER_OLD, &d, &nid, timeout);
		if (status != NI_OK)
		{
			ni_proplist_free(p);
			return NULL;
		}
	}

	status = ni_read(d, &nid, p);
	if (status != NI_OK)
	{
		ni_free(d);
		ni_proplist_free(p);
		free(p);
		return NULL;
	}

	/*
	 * Check for "domain" property
	 */
	where = ni_proplist_match(*p, "domain", NULL);
	if (where == NI_INDEX_NULL) 
	{
		/*
		 * No "domain" property.
		 * If this came from /domains/<dom> use the value of the "name" property.
		 * Else if there is a "search" property, use the first value.
		 * Else if hostname has a ".", use trailing part as a domain.
		 * Else use "local".
		 */

		domainname = NULL;
	
		if (using_domains_dir == 1)
		{
			where = ni_proplist_match(*p, "name", NULL);
			if ((where != NI_INDEX_NULL) && (p->ni_proplist_val[where].nip_val.ni_namelist_len > 0))
				domainname = p->ni_proplist_val[where].nip_val.ni_namelist_val[0];
		}

		if (domainname == NULL)
		{
			where = ni_proplist_match(*p, "search", NULL);
			if ((where != NI_INDEX_NULL) && (p->ni_proplist_val[where].nip_val.ni_namelist_len > 0))
				domainname = p->ni_proplist_val[where].nip_val.ni_namelist_val[0];
		}

		if (domainname == NULL)
		{
			hname = sys_hostname();
			if (hname != NULL)
			{
				dot = strchr(hname, '.');
				if (dot != NULL) domainname = dot + 1;
			}
		}

		if (domainname == NULL) domainname = "local";
		
		/*
		 * Add a "domain" property to the proplist.
		 */
		p->ni_proplist_val = (ni_property *)realloc(p->ni_proplist_val, (p->ni_proplist_len + 1) * sizeof(ni_property));
		p->ni_proplist_val[p->ni_proplist_len].nip_name = strdup("domain");
		p->ni_proplist_val[p->ni_proplist_len].nip_val.ni_namelist_len = 1;
		p->ni_proplist_val[p->ni_proplist_len].nip_val.ni_namelist_val = malloc(sizeof(char *));
		p->ni_proplist_val[p->ni_proplist_len].nip_val.ni_namelist_val[0] = strdup(domainname);
		p->ni_proplist_len++;
	}

	ni_free(d);
	return p;
}

/*
 * Get resolver configuration from /etc/resolv.conf
 */
static ni_proplist *
_dns_file_init(char *dom)
{
	ni_proplist *p;
	ni_property *dp;
	ni_namelist *nl;
	char line[1024], *s, t[1024], *dot, *hname;
	FILE *fp;
	int i;
	
	fp = fopen(FF_LOCATION_RESOLVER, "r");
	if (fp == NULL) return NULL;

	p = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(p);

	p->ni_proplist_len = PLX_LENGTH;

	dp = (ni_property *)malloc(p->ni_proplist_len * sizeof(ni_property));
	NI_INIT(dp);
	
	dp[PLX_DOMAIN].nip_name = strdup("domain");
	dp[PLX_DOMAIN].nip_val.ni_namelist_len = 0;
	dp[PLX_DOMAIN].nip_val.ni_namelist_val = NULL;
	
	dp[PLX_NAMESERVER].nip_name = strdup("nameserver");
	dp[PLX_NAMESERVER].nip_val.ni_namelist_len = 0;
	dp[PLX_NAMESERVER].nip_val.ni_namelist_val = NULL;
	
	dp[PLX_SEARCH].nip_name = strdup("search");
	dp[PLX_SEARCH].nip_val.ni_namelist_len = 0;
	dp[PLX_SEARCH].nip_val.ni_namelist_val = NULL;

	dp[PLX_DEBUG].nip_name = strdup("debug");
	dp[PLX_DEBUG].nip_val.ni_namelist_len = 1;
	dp[PLX_DEBUG].nip_val.ni_namelist_val =  malloc(sizeof(char *));
	dp[PLX_DEBUG].nip_val.ni_namelist_val[0] = strdup("NO");

	dp[PLX_NDOTS].nip_name = strdup("ndots");
	dp[PLX_NDOTS].nip_val.ni_namelist_len = 0;
	dp[PLX_NDOTS].nip_val.ni_namelist_val = NULL;

	dp[PLX_PROTOCOL].nip_name = strdup("protocol");
	dp[PLX_PROTOCOL].nip_val.ni_namelist_len = 1;
	dp[PLX_PROTOCOL].nip_val.ni_namelist_val = malloc(sizeof(char *));
	dp[PLX_PROTOCOL].nip_val.ni_namelist_val[0] = strdup("udp");

	dp[PLX_SORTLIST].nip_name = strdup("sortlist");
	dp[PLX_SORTLIST].nip_val.ni_namelist_len = 0;
	dp[PLX_SORTLIST].nip_val.ni_namelist_val = NULL;

	p->ni_proplist_val = dp;

	fgets(line, 1024, fp);
	while (!feof(fp))
	{
		if (!strncmp(line, "domain", 6) &&
			((line[6] == ' ') || (line[6] == '\t')))
		{
			nl = &(dp[PLX_DOMAIN].nip_val);
			
			if (nl->ni_namelist_len != 0) continue;

			for (i = 7; (line[i] == ' ') || (line[i] == '\t'); i++);
			if (line[i] == '\0') continue;
			s = line + i;
			for (;
				(line[i] != ' ') && (line[i] != '\t') &&
				(line[i] != '\0') && (line[i] != '\n');
				i++);
			line[i] = '\0';
			nl->ni_namelist_len = 1;
			nl->ni_namelist_val = malloc(sizeof(char *));
			nl->ni_namelist_val[0] = strdup(s);
			
			if (dom != NULL)
			{
				if (strcmp(dom, s))
				{
					ni_proplist_free(p);
					free(p);
					fclose(fp);
					return NULL;
				}
			}
		}
		
		else if (!strncmp(line, "nameserver", 10) &&
			((line[10] == ' ') || (line[10] == '\t')))
		{
			nl = &(dp[PLX_NAMESERVER].nip_val);
			
			for (i = 11; (line[i] == ' ') || (line[i] == '\t'); i++);
			if (line[i] == '\0') continue;
			s = line + i;
			for (;
				(line[i] != ' ') && (line[i] != '\t') &&
				(line[i] != '\0') && (line[i] != '\n');
				i++);
			line[i] = '\0';
			if (nl->ni_namelist_len == 0)
			{
				nl->ni_namelist_val = malloc(sizeof(char *));
			}
			else
			{
				nl->ni_namelist_val =
					realloc(nl->ni_namelist_val,
						(nl->ni_namelist_len + 1) * sizeof(char *));
			}
			
			nl->ni_namelist_val[nl->ni_namelist_len] = strdup(s);
			nl->ni_namelist_len++;
		}

		else if (!strncmp(line, "search", 6) &&
			((line[6] == ' ') || (line[6] == '\t')))
		{
			nl = &(dp[PLX_SEARCH].nip_val);
			
			for (i = 7; (line[i] == ' ') || (line[i] == '\t'); i++);
			while (sscanf(line+i, "%s", t) == 1)
			{
				if (nl->ni_namelist_len == 0)
				{
					nl->ni_namelist_val = malloc(sizeof(char *));
				}
				else
				{
					nl->ni_namelist_val =
						realloc(nl->ni_namelist_val,
							(nl->ni_namelist_len + 1) * sizeof(char *));
				}
			
				nl->ni_namelist_val[nl->ni_namelist_len] = strdup(t);
				nl->ni_namelist_len++;

				for (i += strlen(t); (line[i] == ' ') || (line[i] == '\t'); i++);
			}
		}

		else if (!strncmp(line, "sortlist", 8) &&
			((line[8] == ' ') || (line[8] == '\t')))
		{
			nl = &(dp[PLX_SORTLIST].nip_val);
			
			for (i = 9; (line[i] == ' ') || (line[i] == '\t'); i++);
			while (sscanf(line+i, "%s", t) == 1)
			{
				if (nl->ni_namelist_len == 0)
				{
					nl->ni_namelist_val = malloc(sizeof(char *));
				}
				else
				{
					nl->ni_namelist_val =
						realloc(nl->ni_namelist_val,
							(nl->ni_namelist_len + 1) * sizeof(char *));
				}
			
				nl->ni_namelist_val[nl->ni_namelist_len] = strdup(t);
				nl->ni_namelist_len++;

				for (i += strlen(t); (line[i] == ' ') || (line[i] == '\t'); i++);
			}
		}

		else if (!strncmp(line, "options", 7) &&
			((line[7] == ' ') || (line[7] == '\t')))
		{
			for (i = 8; (line[i] == ' ') || (line[i] == '\t'); i++);
			while (sscanf(line+i, "%s", t) == 1)
			{
				if (!strcmp(t, "debug"))
				{
					nl = &(dp[PLX_DEBUG].nip_val);
					if (nl->ni_namelist_len != 0)
					{
						free(nl->ni_namelist_val);
					}
					nl->ni_namelist_val = malloc(sizeof(char *));
					nl->ni_namelist_val[0] = strdup("YES");
				}
				else if (!strncmp(t, "ndots:", 6))
				{
					nl = &(dp[PLX_DEBUG].nip_val);
					if (nl->ni_namelist_len != 0)
					{
						free(nl->ni_namelist_val);
					}
					nl->ni_namelist_val = malloc(sizeof(char *));
					nl->ni_namelist_val[0] = strdup(t + 6);
				}
				else if (!strcmp(t, "tcp"))
				{
					nl = &(dp[PLX_PROTOCOL].nip_val);
					if (nl->ni_namelist_len != 0)
					{
						free(nl->ni_namelist_val);
					}
					nl->ni_namelist_val = malloc(sizeof(char *));
					nl->ni_namelist_val[0] = strdup(t);
				}
				
				i += strlen(t);
			}
		}

		fgets(line, 1024, fp);
	}

	fclose(fp);

	/*
	 * If no nameserver addresses are set, return NULL.
	 */
	if (dp[PLX_NAMESERVER].nip_val.ni_namelist_len == 0)
	{
		ni_proplist_free(p);
		free(p);
		return NULL;
	}

	if (dp[PLX_DOMAIN].nip_val.ni_namelist_len == 0)
	{
		/*
		 * domain is unset
		 * If search was specified, use first value.
		 * Else if hostname has a ".", use trailing part as a domain.
		 * Else use "local".
		 */
		if (dp[PLX_SEARCH].nip_val.ni_namelist_len != 0)
		{
			nl = &(dp[PLX_DOMAIN].nip_val);
			nl->ni_namelist_len = 1;
			nl->ni_namelist_val = malloc(sizeof(char *));
			nl->ni_namelist_val[0] = strdup(dp[PLX_SEARCH].nip_val.ni_namelist_val[0]);
		}
		else
		{
			s = "local";
			hname = sys_hostname();
			if (hname != NULL)
			{
				dot = strchr(hname, '.');
				if (dot != NULL) s = dot + 1;
			}

		    nl = &(dp[PLX_DOMAIN].nip_val);
			
	  	  nl->ni_namelist_len = 1;
	  	  nl->ni_namelist_val = malloc(sizeof(char *));
	  	  nl->ni_namelist_val[0] = strdup(s);
	  }
	}

	if (dom != NULL)
	{
		if (strcmp(dom, dp[PLX_DOMAIN].nip_val. ni_namelist_val[0]))
		{
			ni_proplist_free(p);
			free(p);
			return NULL;
		}
	}

	return p;
}

/*
 * Utility to break up sortlist entries into addresses and netmasks
 */
static int
_dns_parse_network(char *s, u_int32_t *addr, u_int32_t *mask)
{
	char *p, *q;
	u_int32_t v, i, m, bits;

	if (s == NULL) return 1;

	p = strchr(s, '/');
	if (p != NULL) *p++ = '\0';
	
	*addr = inet_addr(s);
	if (*addr == (u_int32_t)-1) return 1;
	if (p == NULL)
	{
		if (IN_CLASSA(*addr)) *mask = IN_CLASSA_NET;
		else if (IN_CLASSB(*addr)) *mask = IN_CLASSB_NET;
		else if (IN_CLASSC(*addr)) *mask = IN_CLASSC_NET;
		else return 1;
		return 0;
	}

	*(p - 1) = '/';

	q = strchr(p, '.');
	if (q == NULL)
	{
		bits = atoi(p);
		if (bits == 0) return 1;
		if (bits > 32) bits = 32;

		bits = 33 - bits;
		m = 0;
		for (i = 1, v = 1; i < bits; i++, v *= 2) m |= v;
		*mask = ~m;
		return 0;
	}
	
	*mask = inet_addr(p);
	if (*mask == (u_int32_t)-1) return 1;
	return 0;
}

/*
 * Create a DNS client handle
 */
dns_handle_t *
dns_open_lock(char *dom, u_int32_t lockit)
{
	dns_handle_t *dns;
	ni_proplist *p;
	ni_index dx, nx, where;
	int i, len, s, proto, stype;
	unsigned long addr;
	u_int16_t port;
	u_int32_t sa, sm;
	char *str;

	if (lockit != 0) _dns_lock();

	p = _dns_file_init(dom);
	if (p == NULL) p = _dns_ni_init(dom, NI_TIMEOUT);
	if (p == NULL) p = _dns_dns_init(dom, NI_TIMEOUT);
	if (p == NULL)
	{
		if (lockit != 0) _dns_unlock();
		return NULL;
	}

	dx = ni_proplist_match(*p, "domain", NULL);
	if (dx == NI_INDEX_NULL)
	{
		ni_proplist_free(p);
		free(p);
		if (lockit != 0) _dns_unlock();
		return NULL;
	}
	if (p->ni_proplist_val[dx].nip_val.ni_namelist_len == 0)
	{
		ni_proplist_free(p);
		free(p);
		if (lockit != 0) _dns_unlock();
		return NULL;
	}
	if ((dom != NULL) && strcmp(p->ni_proplist_val[dx].nip_val.ni_namelist_val[0], dom))
	{
		ni_proplist_free(p);
		free(p);
		if (lockit != 0) _dns_unlock();
		return NULL;
	}
		
	nx = ni_proplist_match(*p, "nameserver", NULL);
	if (nx == NI_INDEX_NULL)
	{
		ni_proplist_free(p);
		free(p);
		if (lockit != 0) _dns_unlock();
		return NULL;
	}

	len = p->ni_proplist_val[nx].nip_val.ni_namelist_len;
	if (len == 0)
	{
		ni_proplist_free(p);
		free(p);
		if (lockit != 0) _dns_unlock();
		return NULL;
	}

	proto = IPPROTO_UDP;
	stype = SOCK_DGRAM;
	where = ni_proplist_match(*p, "protocol", NULL);
	if (where != NI_INDEX_NULL)
	{
		if (p->ni_proplist_val[where].nip_val.ni_namelist_len != 0)
		{
			str = p->ni_proplist_val[where].nip_val.ni_namelist_val[0];
			if (!strcmp(str, "tcp")) proto = IPPROTO_TCP;
			else if (!strcmp(str, "TCP")) proto = IPPROTO_TCP;
		}
	}

	if (proto == IPPROTO_TCP) stype = SOCK_STREAM;
	
	s = socket(AF_INET, stype, proto);	
	if (s < 0)
	{
		ni_proplist_free(p);
		free(p);
		if (lockit != 0) _dns_unlock();
		return NULL;
	}

	dns = (dns_handle_t *)malloc(sizeof(dns_handle_t));
	memset(dns, 0, sizeof(dns_handle_t));

	dns->sock = s;
	dns->protocol = proto;
	dns->sockstate = DNS_SOCK_UDP;
	if (proto == IPPROTO_TCP) dns->sockstate = DNS_SOCK_TCP_UNCONNECTED;

	dns->xid = random() % 0x10000;

	dns->ias_dots = 1;
	where = ni_proplist_match(*p, "ndots", NULL);
	if ((where != NI_INDEX_NULL) &&
		(p->ni_proplist_val[where].nip_val.ni_namelist_len > 0))
	dns->ias_dots = atoi(p->ni_proplist_val[where].nip_val.ni_namelist_val[0]);

	port = _dns_port(dns->protocol);
	
	dns->selected_server = 0;
	dns->server_count = len;
	dns->server = (struct sockaddr_in *)malloc(len * sizeof(struct sockaddr_in));
	for (i = 0; i < len; i++)
	{
		addr = inet_addr(p->ni_proplist_val[nx].nip_val.ni_namelist_val[i]);
		dns->server[i].sin_addr.s_addr = addr;
		dns->server[i].sin_family = AF_INET;
		dns->server[i].sin_port = port;
#ifdef _UNIX_BSD_44_
		dns->server[i].sin_len = sizeof(struct sockaddr_in);
#endif
	}

	dns->server_timeout.tv_sec = DNS_SERVER_TIMEOUT;
	dns->server_timeout.tv_usec = 0;
	dns->server_retries = DNS_SERVER_RETRIES;

	dns->timeout.tv_sec = DNS_SERVER_TIMEOUT * DNS_SERVER_RETRIES * dns->server_count;
	dns->timeout.tv_usec = 0;

	dns->domain = strdup(p->ni_proplist_val[dx].nip_val.ni_namelist_val[0]);

	dns->search_count = 0;
	dns->search = NULL;
	where = ni_proplist_match(*p, "search", NULL);
	if (where != NI_INDEX_NULL)
	{
		dns->search_count = p->ni_proplist_val[where].nip_val.ni_namelist_len;
		if (dns->search_count > 0) dns->search = (char **)malloc(dns->search_count * sizeof(char *));
		for (i = 0; i < dns->search_count; i++)
		{
			dns->search[i] = strdup(p->ni_proplist_val[where].nip_val.ni_namelist_val[i]);
		}
	}

	dns->sort_count = 0;
	where = ni_proplist_match(*p, "sortlist", NULL);
	if (where != NI_INDEX_NULL)
	{
		len = p->ni_proplist_val[where].nip_val.ni_namelist_len;
		for (i = 0; i < len; i++)
		{
			str = p->ni_proplist_val[where].nip_val.ni_namelist_val[i];
			if (_dns_parse_network(str, &sa, &sm) == 0)
			{
				if (dns->sort_count == 0)
				{
					dns->sort_addr = (u_int32_t *)malloc(sizeof(u_int32_t));
					dns->sort_mask = (u_int32_t *)malloc(sizeof(u_int32_t));
				}
				else
				{
					dns->sort_addr = (u_int32_t *)realloc((char *)dns->sort_addr,
						(dns->sort_count + 1) * sizeof(u_int32_t));
					dns->sort_mask = (u_int32_t *)realloc((char *)dns->sort_mask,
						(dns->sort_count + 1) * sizeof(u_int32_t));
				}
				dns->sort_addr[dns->sort_count] = sa & sm;
				dns->sort_mask[dns->sort_count] = sm;
				dns->sort_count++;
			}
		}
	}

	ni_proplist_free(p);
	free(p);

	dns->log_dest = DNS_LOG_NONE;
	dns->log_file = NULL;
	dns->log_title = NULL;

	if (lockit != 0) _dns_unlock();
	return dns;
}

dns_handle_t *
dns_open(char *dom)
{
	return dns_open_lock(dom, 1);
}

dns_handle_t *
dns_connect(char *name, struct sockaddr_in *addr)
{
	dns_handle_t *dns;
	int s;

	_dns_lock();

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
	{
		_dns_unlock();
		return NULL;
	}

	dns = (dns_handle_t *)malloc(sizeof(dns_handle_t));
	memset(dns, 0, sizeof(dns_handle_t));

	dns->sock = s;
	dns->protocol = IPPROTO_UDP;
	dns->sockstate = DNS_SOCK_UDP;
	
	dns->xid = random() % 0x10000;

	dns->ias_dots = 1;
	dns->selected_server = 0;
	dns->server_count = 1;
	dns->server = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	dns->server[0].sin_addr.s_addr = addr->sin_addr.s_addr;
	dns->server[0].sin_family = AF_INET;
	dns->server[0].sin_port = addr->sin_port;
#ifndef _NO_SOCKADDR_LENGTH_
	dns->server[0].sin_len = sizeof(struct sockaddr_in);
#endif
	dns->server_timeout.tv_sec = DNS_SERVER_TIMEOUT;
	dns->server_timeout.tv_usec = 0;
	dns->server_retries = DNS_SERVER_RETRIES;
	dns->timeout.tv_sec = DNS_SERVER_TIMEOUT * DNS_SERVER_RETRIES;
	dns->timeout.tv_usec = 0;
	dns->domain = strdup(name);
	dns->search_count = 0;

	dns->log_dest = 0;
	dns->log_file = NULL;
	dns->log_title = NULL;

	_dns_unlock();
	return dns;
}

void
dns_shutdown(void)
{
	if (_dnsLock != NULL) syslock_free(_dnsLock);
	_dnsLock = NULL;
	if (_logLock != NULL) syslock_free(_logLock);
	_logLock = NULL;
}

void
dns_add_server(dns_handle_t *dns, struct sockaddr_in *s)
{
	u_int32_t i;

	if (dns == NULL) return;
	if (s == NULL) return;
	
	if (dns->server_count == 0)
		dns->server = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	else
		dns->server = (struct sockaddr_in *)realloc((char *)dns->server,
			(dns->server_count + 1) * sizeof(struct sockaddr_in));

	i = dns->server_count;
	dns->server[i].sin_addr.s_addr = s->sin_addr.s_addr;
	dns->server[i].sin_port = s->sin_port;
	dns->server[i].sin_family = AF_INET;
#ifndef _NO_SOCKADDR_LENGTH_
	dns->server[i].sin_len = sizeof(struct sockaddr_in);
#endif
	
	dns->server_count++;
}

void
dns_remove_server(dns_handle_t *dns, u_int32_t x)
{
	u_int32_t i;

	if (dns == NULL) return;
	if (x >= dns->server_count) return;

	if (dns->server_count == 1)
	{
		free(dns->server);
		dns->server = NULL;
		dns->server_count = 0;
		dns->selected_server = 0;
		return;
	}

	for (i = x + 1; i < dns->server_count; i++)
	{
		dns->server[i-1] = dns->server[i];
		if (dns->selected_server == i) dns->selected_server--;
	}

	dns->server_count--;
	dns->server = (struct sockaddr_in *)realloc((char *)dns->server,
		dns->server_count * sizeof(struct sockaddr_in));
}

/*
 * Release a DNS client handle
 */
void
dns_free(dns_handle_t *dns)
{
	u_int32_t i;

	if (dns == NULL) return;

	shutdown(dns->sock, 2);
	close(dns->sock);

	free(dns->server);
	free(dns->domain);
	for (i = 0; i < dns->search_count; i++) free(dns->search[i]);
	if (dns->search_count > 0) free(dns->search);
	if (dns->sort_count > 0)
	{
		free(dns->sort_addr);
		free(dns->sort_mask);
	}
	if (dns->log_title != NULL) free(dns->log_title);
	if (dns->log_file != NULL) fclose(dns->log_file);

	free(dns);
}

void
dns_set_xid(dns_handle_t *dns, u_int32_t x)
{
	if (dns == NULL) return;
	dns->xid = x;
}

void
dns_set_server_timeout(dns_handle_t *dns, struct timeval *tv)
{
	u_int32_t us;

	if (dns == NULL) return;

	us = (tv->tv_sec * 1000) + tv->tv_usec;

	dns->server_timeout.tv_sec = us / 1000;
	dns->server_timeout.tv_usec = us % 1000;
	
	us = us * (dns->server_retries + 1) * dns->server_count;

	dns->timeout.tv_sec = us / 1000;
	dns->timeout.tv_usec = us % 1000;
}

void
dns_set_timeout(dns_handle_t *dns, struct timeval *tv)
{
	u_int32_t us;
	struct timeval m;

	if (dns == NULL) return;

	us = (tv->tv_sec * 1000) + tv->tv_usec;

	dns->timeout.tv_sec = us / 1000;
	dns->timeout.tv_usec = us % 1000;

	us = us / ((dns->server_retries + 1) * dns->server_count);
	if (us == 0)
	{
		m.tv_sec = 0;
		m.tv_usec = 1;
		dns_set_server_timeout(dns, &m);
		return;
	}

	dns->server_timeout.tv_sec = us / 1000;
	dns->server_timeout.tv_usec = us % 1000;
}

void
dns_set_server_retries(dns_handle_t *dns, u_int32_t n)
{
	u_int32_t us;

	if (dns == NULL) return;
	dns->server_retries = n;

	us = (dns->server_timeout.tv_sec * 1000) + dns->server_timeout.tv_usec;
	
	us = us * (dns->server_retries + 1) * dns->server_count;

	dns->timeout.tv_sec = us / 1000;
	dns->timeout.tv_usec = us % 1000;
}

void
dns_set_protocol(dns_handle_t *dns, u_int32_t protocol)
{
	u_int32_t i, stype;
	u_int16_t port;

	if (dns == NULL) return;
	if (protocol == dns->protocol) return;

	if ((protocol != IPPROTO_UDP) && (protocol != IPPROTO_TCP))
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_set_protocol - unknown protocol %u", protocol);
		return;
	}

	_dns_lock();

	dns->protocol = protocol;
	shutdown(dns->sock, 2);
	close(dns->sock);

	if (dns->protocol == IPPROTO_UDP)
	{
		stype = SOCK_DGRAM;
		dns->sockstate = DNS_SOCK_UDP;
	}
	else
	{
		stype = SOCK_STREAM;
		dns->sockstate = DNS_SOCK_TCP_UNCONNECTED;
	}

	dns->sock = socket(AF_INET, stype, dns->protocol);
	port = _dns_port(dns->protocol);
	for (i = 0; i < dns->server_count; i++) dns->server[i].sin_port = port;
	
	_dns_unlock();
}

void
dns_select_server(dns_handle_t *dns, u_int32_t which)
{
	u_int32_t stype;
	if (dns == NULL) return;

	if (which >= dns->server_count)
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_select_server - only %u server%s, can't select server %u",
			dns->server_count, (dns->server_count == 1) ? "" : "s", which);
		return;
	}

	if (dns->selected_server == which) return;

	if (dns->sockstate == DNS_SOCK_TCP_CONNECTED)
	{
		_dns_lock();
		
		shutdown(dns->sock, 2);
		close(dns->sock);
		dns->sockstate = DNS_SOCK_TCP_UNCONNECTED;

		if (dns->protocol == IPPROTO_UDP) stype = SOCK_DGRAM;
		else stype = SOCK_STREAM;

		dns->sock = socket(AF_INET, stype, dns->protocol);

		_dns_unlock();
	}
	
	dns->selected_server = which;
}

static u_int8_t
_dns_parse_uint8(char **p)
{
	u_int8_t v;

	v = (u_int8_t)**p;
	*p += 1;
	return v;
}
	
static u_int16_t
_dns_parse_uint16(char **p)
{
	u_int16_t *x, v;

	x = (u_int16_t *)*p;
	v = ntohs(*x);
	*p += 2;
	return v;
}
	
static u_int32_t
_dns_parse_uint32(char **p)
{
	u_int32_t *x, v;

	x = (u_int32_t *)*p;
	v = ntohl(*x);
	*p += 4;
	return v;
}
	
static u_int8_t
_dns_cname_length(char *s)
{
	u_int8_t l;

	if (s == NULL) return 1;
	l = strlen(s);
	while ((s[l - 1] == '.') && (l > 1)) l--;
	return l;
}

static void
_dns_insert_cname(char *s, char *p)
{
	int i;
	u_int8_t len, dlen;

	if (s == NULL)
	{
		*p = 0;
		return;
	}

	if (!strcmp(s, "."))
	{
		p[0] = 1;
		p[1] = '.';
		p[2] = 0;
		return;
	}

	len = _dns_cname_length(s);
	
	p[0] = '.';
	memmove(p + 1, s, len);
	p[len + 1] = '.';

	dlen = 0;

	for (i = len + 1; i >= 0; i--)
	{
		if (p[i] == '.')
		{
			p[i] = dlen;
			dlen = 0;
		}
		else dlen++;
	}
}

static char *
_dns_parse_string(char *p, char **x)
{
	char *str;
	u_int8_t len;

	len = (u_int8_t)**x;
	*x += 1;
	str = malloc(len + 1);
	memmove(str, *x, len);
	str[len] = '\0';
	*x += len;
	return str;
}

static char *
_dns_parse_domain_name(char *p, char **x)
{
	u_int8_t *v8;
	u_int16_t *v16, skip;
	u_int16_t i, j, dlen, len;
	int more, compressed;
	char *name, *start;

	start = *x;
	compressed = 0;
	more = 1;
	name = malloc(1);
	name[0] = '\0';
	len = 1;
	j = 0;
	skip = 0;

	while (more == 1)
	{
		v8 = (u_int8_t *)*x;
		dlen = *v8;

		if ((dlen & 0xc0) == 0xc0)
		{
			v16 = (u_int16_t *)*x;
			*x = p + (ntohs(*v16) & 0x3fff);
			if (compressed == 0) skip += 2;
			compressed = 1;
			continue;
		}

		*x += 1;
		if (dlen > 0)
		{
			len += dlen;
			name = realloc(name, len);
		}
	
		for (i = 0; i < dlen; i++)
		{
			name[j++] = **x;
			*x += 1;
		}
		name[j] = '\0';
		if (compressed == 0) skip += (dlen + 1);
		
		if (dlen == 0) more = 0;
		else
		{
			v8 = (u_int8_t *)*x;
			if (*v8 != 0)
			{
				len += 1;
				name = realloc(name, len);
				name[j++] = '.';
				name[j] = '\0';
			}
		}
	}
	
	*x = start + skip;

	return name;
}

dns_resource_record_t *
dns_parse_resource_record(char *p, char **x)
{
	u_int32_t size, bx, mi;
	u_int16_t rdlen;
	u_int8_t byte, i;
	dns_resource_record_t *r;
	char *eor;

	r = (dns_resource_record_t *)malloc(sizeof(dns_resource_record_t));
	memset(r, 0, sizeof(dns_resource_record_t));

	r->name = _dns_parse_domain_name(p, x);
	r->type = _dns_parse_uint16(x);
	r->class = _dns_parse_uint16(x);
	r->ttl = _dns_parse_uint32(x);
	rdlen = _dns_parse_uint16(x);

	eor = *x;
	r->data.A = NULL;

	switch (r->type)
	{
		case DNS_TYPE_A:
			size = sizeof(dns_address_record_t);
			r->data.A = (dns_address_record_t *)malloc(size);
			r->data.A->addr.s_addr = htonl(_dns_parse_uint32(x));
			break;

		case DNS_TYPE_AAAA:
			size = sizeof(dns_in6_address_record_t);
			r->data.AAAA = (dns_in6_address_record_t *)malloc(size);
			r->data.AAAA->addr.__u6_addr.__u6_addr32[0] = htonl(_dns_parse_uint32(x));
			r->data.AAAA->addr.__u6_addr.__u6_addr32[1] = htonl(_dns_parse_uint32(x));
			r->data.AAAA->addr.__u6_addr.__u6_addr32[2] = htonl(_dns_parse_uint32(x));
			r->data.AAAA->addr.__u6_addr.__u6_addr32[3] = htonl(_dns_parse_uint32(x));
			break;

		case DNS_TYPE_NS:
		case DNS_TYPE_CNAME:
		case DNS_TYPE_MB:
		case DNS_TYPE_MG:
		case DNS_TYPE_MR:
		case DNS_TYPE_PTR:
			size = sizeof(dns_domain_name_record_t);
			r->data.CNAME = (dns_domain_name_record_t *)malloc(size);

			r->data.CNAME->name = _dns_parse_domain_name(p, x);
			break;
	
		case DNS_TYPE_SOA:
			size = sizeof(dns_SOA_record_t);
			r->data.SOA = (dns_SOA_record_t *)malloc(size);

			r->data.SOA->mname = _dns_parse_domain_name(p, x);
			r->data.SOA->rname = _dns_parse_domain_name(p, x);
			r->data.SOA->serial = _dns_parse_uint32(x);
			r->data.SOA->refresh = _dns_parse_uint32(x);
			r->data.SOA->retry = _dns_parse_uint32(x);
			r->data.SOA->expire = _dns_parse_uint32(x);
			r->data.SOA->minimum = _dns_parse_uint32(x);	
			break;

		case DNS_TYPE_NULL:
			size = sizeof(dns_raw_resource_record_t);
			r->data.DNSNULL = (dns_raw_resource_record_t *)malloc(size);

			r->data.DNSNULL->length = rdlen;
			r->data.DNSNULL->data = malloc(rdlen);
			memmove(r->data.DNSNULL->data, *x, rdlen);
			x += rdlen;
			break;

		case DNS_TYPE_WKS:
			size = sizeof(dns_WKS_record_t);
			r->data.WKS = (dns_WKS_record_t *)malloc(size);

			r->data.WKS->addr.s_addr = htonl(_dns_parse_uint32(x));
			r->data.WKS->protocol = _dns_parse_uint8(x);
			size = rdlen - 5;
			r->data.WKS->maplength = size * 8;
			r->data.WKS->map = (u_int8_t *)malloc(r->data.WKS->maplength);
			mi = 0;
			for (bx = 0; bx < size; bx++)
			{
				byte = _dns_parse_uint8(x);
				for (i = 128; i >= 1; i = i/2)
				{
					if (byte & i) r->data.WKS->map[mi] = 0xff;
					else r->data.WKS->map[mi] = 0;
					mi++;
				}
			}
			break;

		case DNS_TYPE_HINFO:
			size = sizeof(dns_HINFO_record_t);
			r->data.HINFO = (dns_HINFO_record_t *)malloc(size);

			r->data.HINFO->cpu = _dns_parse_string(p, x);
			r->data.HINFO->os = _dns_parse_string(p, x);
			break;

		case DNS_TYPE_MINFO:
			size = sizeof(dns_MINFO_record_t);
			r->data.MINFO = (dns_MINFO_record_t *)malloc(size);

			r->data.MINFO->rmailbx = _dns_parse_domain_name(p, x);
			r->data.MINFO->emailbx = _dns_parse_domain_name(p, x);
			break;

		case DNS_TYPE_MX:
			size = sizeof(dns_MX_record_t);
			r->data.MX = (dns_MX_record_t *)malloc(size);

			r->data.MX->preference = _dns_parse_uint16(x);
			r->data.MX->name = _dns_parse_domain_name(p, x);
			break;

		case DNS_TYPE_TXT:
			size = sizeof(dns_TXT_record_t);
			r->data.TXT = (dns_TXT_record_t *)malloc(size);

			r->data.TXT->string = _dns_parse_string(p, x);
			break;

		case DNS_TYPE_RP:
			size = sizeof(dns_RP_record_t);
			r->data.RP = (dns_RP_record_t *)malloc(size);

			r->data.RP->mailbox = _dns_parse_domain_name(p, x);
			r->data.RP->txtdname = _dns_parse_domain_name(p, x);
			break;

		case DNS_TYPE_AFSDB:
			size = sizeof(dns_AFSDB_record_t);
			r->data.AFSDB = (dns_AFSDB_record_t *)malloc(size);

			r->data.AFSDB->subtype = _dns_parse_uint32(x);
			r->data.AFSDB->hostname = _dns_parse_domain_name(p, x);
			break;

		case DNS_TYPE_X25:
			size = sizeof(dns_X25_record_t);
			r->data.X25 = (dns_X25_record_t *)malloc(size);

			r->data.X25->psdn_address = _dns_parse_string(p, x);
			break;

		case DNS_TYPE_ISDN:
			size = sizeof(dns_ISDN_record_t);
			r->data.ISDN = (dns_ISDN_record_t *)malloc(size);

			r->data.ISDN->isdn_address = _dns_parse_string(p, x);
			if (*x < (eor + rdlen))
				r->data.ISDN->subaddress = _dns_parse_string(p, x);
			else
				r->data.ISDN->subaddress = NULL;
			break;

		case DNS_TYPE_RT:
			size = sizeof(dns_RT_record_t);
			r->data.RT = (dns_RT_record_t *)malloc(size);

			r->data.RT->preference = _dns_parse_uint16(x);
			r->data.RT->intermediate = _dns_parse_domain_name(p, x);
			break;

		case DNS_TYPE_LOC:
			size = sizeof(dns_LOC_record_t);
			r->data.LOC = (dns_LOC_record_t *)malloc(size);

			r->data.LOC->version = _dns_parse_uint8(x);
			r->data.LOC->size = _dns_parse_uint8(x);
			r->data.LOC->horizontal_precision = _dns_parse_uint8(x);
			r->data.LOC->vertical_precision = _dns_parse_uint8(x);
			r->data.LOC->latitude = _dns_parse_uint32(x);
			r->data.LOC->longitude = _dns_parse_uint32(x);
			r->data.LOC->altitude = _dns_parse_uint32(x);
			break;

		case DNS_TYPE_SRV:
			size = sizeof(dns_SRV_record_t);
			r->data.SRV = (dns_SRV_record_t *)malloc(size);

			r->data.SRV->priority = _dns_parse_uint16(x);
			r->data.SRV->weight = _dns_parse_uint16(x);
			r->data.SRV->port = _dns_parse_uint16(x);
			r->data.SRV->target = _dns_parse_domain_name(p, x);
			break;
	}

	*x = eor + rdlen;	
	return r;
}

dns_question_t *
dns_parse_question(char *p, char **x)
{
	dns_question_t *q;
	
	if (x == NULL) return NULL;
	if (*x == NULL) return NULL;

	q = (dns_question_t *)malloc(sizeof(dns_question_t));

	q->name = _dns_parse_domain_name(p, x);
	q->type = _dns_parse_uint16(x);
	q->class = _dns_parse_uint16(x);

	return q;
}

u_int32_t
dns_dname_cmp(char *a, char *b)
{
	return strcmp(a, b);
}

dns_reply_t *
dns_parse_packet(char *p)
{
	dns_reply_t *r;
	dns_header_t *h;
	char *x;
	u_int32_t i, size;
	
	if (p == NULL) return NULL;
	x = p;

	r = (dns_reply_t *)malloc(sizeof(dns_reply_t));
	memset(r, 0, sizeof(dns_reply_t));

	r->header = (dns_header_t *)malloc(sizeof(dns_header_t));	
	h = r->header;
	memset(h, 0, sizeof(dns_header_t));

	h->xid = _dns_parse_uint16(&x);
	h->flags = _dns_parse_uint16(&x);
	h->qdcount = _dns_parse_uint16(&x);
	h->ancount = _dns_parse_uint16(&x);
	h->nscount = _dns_parse_uint16(&x);
	h->arcount = _dns_parse_uint16(&x);
		
	size = sizeof(dns_question_t *);
	r->question = (dns_question_t **)malloc(h->qdcount * size);
	for (i = 0; i < h->qdcount; i++)
		r->question[i] = dns_parse_question(p, &x);
	
	size = sizeof(dns_resource_record_t *);
	
	r->answer = (dns_resource_record_t **)malloc(h->ancount * size);
	for (i = 0; i < h->ancount; i++)
		r->answer[i] = dns_parse_resource_record(p, &x);
		
	r->authority = (dns_resource_record_t **)malloc(h->nscount * size);
	for (i = 0; i < h->nscount; i++)
		r->authority[i] = dns_parse_resource_record(p, &x);
	
	r->additional = (dns_resource_record_t **)malloc(h->arcount * size);
	for (i = 0; i < h->arcount; i++)
		r->additional[i] = dns_parse_resource_record(p, &x);
		
	return r;
}

void
dns_apply_sortlist(dns_handle_t *dns, dns_reply_t *r)
{
	u_int32_t i, j, len, swap;
	u_int32_t *o, *x, a, m, n, t;
	dns_resource_record_t *tr;

	if (dns == NULL) return;
	if (r == NULL) return;
	if (dns->sort_count == 0) return;
	if (r->header == NULL) return;
	len = r->header->ancount;
	if (len == 0) return;

	/* Find each DNS_TYPE_A record */
	/* Keep its index (variable x) */
	/* Assign it an "order" (variable o) based on which sortlist entry it matches */

	o = NULL;
	x = NULL;

	n = 0;
	for (i = 0; i < len; i++)
	{
		if (r->answer[i]->type == DNS_TYPE_A)
		{
			a = r->answer[i]->data.A->addr.s_addr;
			
			if (n == 0)
			{
				o = (u_int32_t *)malloc(sizeof(u_int32_t));
				x = (u_int32_t *)malloc(sizeof(u_int32_t));
			}
			else
			{
				o = (u_int32_t *)realloc((char *)o, (n + 1) * sizeof(u_int32_t));
				x = (u_int32_t *)realloc((char *)x, (n + 1) * sizeof(u_int32_t));
			}

			o[n] = dns->sort_count;
			x[n] = i;

			for (j = 0; j < dns->sort_count; j++)
			{
				m = dns->sort_mask[j];
				if ((a & m) == (dns->sort_addr[j] & m))
				{
					o[n] = j;
					break;
				}
			}

			n++;
		}
	}

	if (n == 0) return;

	/* Bubble sort the DNS_TYPE_A records by their "order" */
	swap = 1;
	len = n - 1;

	while (swap == 1)
	{
		swap = 0;
		for (i = 0; i < len; i++)
		{
			j = i + 1;
			if (o[i] > o[j])
			{
				swap = 1;
				t = o[i];
				o[i] = o[j];
				o[j] = t;

				tr = r->answer[x[i]];
				r->answer[x[i]] = r->answer[x[j]];
				r->answer[x[j]] = tr;

				t = x[i];
				x[i] = x[j];
				x[j] = t;
			}
		}
		len--;
	}

	free(o);
	free(x);
}

void
dns_free_resource_record(dns_resource_record_t *r)
{
	free(r->name);

	switch (r->type)
	{
		case DNS_TYPE_A:
			free(r->data.A);
			break;

		case DNS_TYPE_AAAA:
			free(r->data.AAAA);
			break;

		case DNS_TYPE_NS:
		case DNS_TYPE_CNAME:
		case DNS_TYPE_MB:
		case DNS_TYPE_MG:
		case DNS_TYPE_MR:
		case DNS_TYPE_PTR:
			free(r->data.CNAME->name);
			free(r->data.CNAME);
			break;
	
		case DNS_TYPE_SOA:
			free(r->data.SOA->mname);
			free(r->data.SOA->rname);
			free(r->data.SOA);	
			break;

		case DNS_TYPE_NULL:
			free(r->data.DNSNULL->data);
			free(r->data.DNSNULL);
			break;

		case DNS_TYPE_WKS:
			free(r->data.WKS->map);
			free(r->data.WKS);
			break;

		case DNS_TYPE_HINFO:
			free(r->data.HINFO->cpu);
			free(r->data.HINFO->os);
			free(r->data.HINFO);	
			break;

		case DNS_TYPE_MINFO:
			free(r->data.MINFO->rmailbx);
			free(r->data.MINFO->emailbx);
			free(r->data.MINFO);	
			break;

		case DNS_TYPE_MX:
			free(r->data.MX->name);
			free(r->data.MX);
			break;

		case DNS_TYPE_TXT:
			free(r->data.TXT->string);
			free(r->data.TXT);
			break;

		case DNS_TYPE_RP:
			free(r->data.RP->mailbox);
			free(r->data.RP->txtdname);
			free(r->data.RP);
			break;

		case DNS_TYPE_AFSDB:
			free(r->data.AFSDB->hostname);
			free(r->data.AFSDB);
			break;

		case DNS_TYPE_X25:
			free(r->data.X25->psdn_address);
			free(r->data.X25);
			break;

		case DNS_TYPE_ISDN:
			free(r->data.ISDN->isdn_address);
			if (r->data.ISDN->subaddress != NULL)
				free(r->data.ISDN->subaddress);
			free(r->data.ISDN);
			break;

		case DNS_TYPE_RT:
			free(r->data.RT->intermediate);
			free(r->data.RT);
			break;

		case DNS_TYPE_LOC:
			free(r->data.LOC);
			break;

		case DNS_TYPE_SRV:
			free(r->data.SRV->target);
			free(r->data.SRV);
			break;
	}

	free(r);
}

void
dns_free_reply(dns_reply_t *r)
{
	u_int32_t i;

	if (r == NULL) return;
	if (r->header == NULL) return;

	for (i = 0; i < r->header->qdcount; i++)
	{
		free(r->question[i]->name);
		free(r->question[i]);
	}
	free(r->question);

	for (i = 0; i < r->header->ancount; i++)
		dns_free_resource_record(r->answer[i]);
	free(r->answer);
	
	for (i = 0; i < r->header->nscount; i++)
		dns_free_resource_record(r->authority[i]);
	free(r->authority);
	
	for (i = 0; i < r->header->arcount; i++)
		dns_free_resource_record(r->additional[i]);
	free(r->additional);
	
	free(r->header);

	free(r);
}

void
dns_free_reply_list(dns_reply_list_t *l)
{
	u_int32_t i;

	if (l == NULL) return;

	for (i = 0; i < l->count; i++) dns_free_reply(l->reply[i]);
	free(l);
}

char *
dns_build_query_packet(dns_handle_t *dns, dns_question_t *dnsq,
	u_int16_t *ql, u_int16_t *xid)
{
	u_int16_t *p, len, x, flags;
	char *q, *s;
	dns_header_t *h;

	if (dnsq == NULL)
	{
		dns_log_msg(dns, LOG_ERR, "dns_build_query_packet - NULL query");
		return NULL;
	}

	if (dnsq->name == NULL)
	{
		dns_log_msg(dns, LOG_ERR, "dns_build_query_packet - NULL name in query");
		return NULL;
	}

	len = DNS_HEADER_SIZE + _dns_cname_length(dnsq->name) + 6;
	if (dns->protocol == IPPROTO_TCP) len += 2;
	*ql = len;

	s = malloc(len);
	memset(s, 0, len);

	q = s;
	if (dns->protocol == IPPROTO_TCP) 
	{
		x = htons(len - 2);
		memmove(s, &x, sizeof(u_int16_t));
		q = s + 2;
	}

	h = (dns_header_t *)q;

	x = random() % 0x10000;
	if (x == 0) x++;
	if (x == dns->xid) x++;
	*xid = x;
	h->xid = htons(*xid);
	flags = DNS_FLAGS_RD;
	h->flags = htons(flags);
	h->qdcount = htons(1);

	_dns_insert_cname(dnsq->name, (char *)h + DNS_HEADER_SIZE);
	p = (u_int16_t *)(s + (len - 4));
	*p = htons(dnsq->type);
	p = (u_int16_t *)(s + (len - 2));
	*p = htons(dnsq->class);

	return s;
}

int32_t
dns_read_reply(dns_handle_t *dns, u_int32_t which,
	u_int16_t qxid, char *qname,
	char **r, u_int16_t *rlen)
{
	ssize_t rsize;
	u_int16_t len;
	u_int32_t flen;
	u_int16_t *prxid, rxid;
	struct sockaddr_in from;
	int status;

	if (dns->protocol == IPPROTO_UDP)
	{
		*rlen = REPLY_BUF_SIZE;
	}
	else
	{
		flen = sizeof(struct sockaddr_in);
		rsize = recvfrom(dns->sock, &len, 2, 0, (struct sockaddr *)&from, &flen);
		if (rsize <= 0)
		{
			dns_log_msg(dns, LOG_ERR, "dns_read_reply - size receive failed");
			return DNS_STATUS_RECEIVE_FAILED;
		}
		*rlen = ntohs(len);
		status = setsockopt(dns->sock, SOL_SOCKET, SO_RCVLOWAT, rlen, 4);
		if (status < 0) dns_log_msg(dns, LOG_ERR, "dns_read_reply - setsockopt status %d errno=%d", status, errno);
	}

	*r = malloc(*rlen);
	memset(*r, 0, *rlen);

	flen = sizeof(struct sockaddr_in);

	rsize = recvfrom(dns->sock, *r, *rlen, 0, (struct sockaddr *)&from, &flen);
	if (rsize <= 0)
	{
		free(*r);
		dns_log_msg(dns, LOG_ERR, "dns_read_reply - receive failed");
		return DNS_STATUS_RECEIVE_FAILED;
	}

	if ((dns->protocol == IPPROTO_TCP) && (*rlen != rsize))
	{
		free(*r);
		dns_log_msg(dns, LOG_ERR,
			"dns_read_reply - short reply %d %d\n", *rlen, rsize);
		return DNS_STATUS_RECEIVE_FAILED;
	}

#ifdef CHECK_REPLY_SERVER
	if (dns->protocol == IPPROTO_UDP)
	{
		if ((from.sin_family != dns->server[which].sin_family) ||
		(from.sin_port != dns->server[which].sin_port) ||
		(from.sin_addr.s_addr != dns->server[which].sin_addr.s_addr))
		{
			free(*r);
			dns_log_msg(dns, LOG_ERR,
				"dns_read_reply - reply from wrong server (%s)",
				inet_ntoa(from.sin_addr));
			return DNS_STATUS_WRONG_SERVER;
		}
	}
#endif

	/* Check for wrong xid in reply */
	prxid = (u_int16_t *)*r;
	rxid = ntohs(*prxid);

	if ((qxid != 0) && (rxid != qxid))
	{
		free(*r);
		dns_log_msg(dns, LOG_NOTICE,
			"dns_read_reply - wrong XID in reply (expected %hu got %hu)",
			qxid, rxid);
		return DNS_STATUS_WRONG_XID;
	}

	/* Check for wrong question in reply */
	if ((qname != NULL) && (dns_dname_cmp((*r) + DNS_HEADER_SIZE, qname)))
	{
		free(*r);
		dns_log_msg(dns, LOG_ERR, "dns_read_reply - bad query");
		return DNS_STATUS_WRONG_QUESTION;
	}

	return DNS_STATUS_OK;
}

int32_t
dns_send_query(dns_handle_t *dns, u_int32_t which, char *qp, u_int16_t qplen)
{
	ssize_t i;
	int32_t status;
	u_int32_t slen;
	struct sockaddr *dst;

	if (dns == NULL) return DNS_STATUS_BAD_HANDLE;
	if (which >= dns->server_count)
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_send_query - invalid server selection");
		return DNS_STATUS_BAD_HANDLE;
	}

	slen = sizeof(struct sockaddr_in);
	dst = (struct sockaddr *)&(dns->server[which]);

	dns_select_server(dns, which);

	if (dns->sockstate == DNS_SOCK_TCP_UNCONNECTED)
	{
		/* connect to server */
		status = connect(dns->sock, dst, sizeof(struct sockaddr_in));
		if (status < 0)
		{
			dns_log_msg(dns, LOG_ERR, "dns_send_query - TCP connect failed");
			return DNS_STATUS_CONNECTION_FAILED;
		}
		dns->sockstate = DNS_SOCK_TCP_CONNECTED;
	}

	i = sendto(dns->sock, qp, qplen, 0, dst, slen);
	if (i < 0)
	{
		dns_log_msg(dns, LOG_ERR, "dns_send_query - send failed");
		return DNS_STATUS_SEND_FAILED;
	}

	return DNS_STATUS_OK;
}

int32_t
dns_zone_transfer_query(dns_handle_t *dns, u_int16_t class, u_int16_t *xid, u_int32_t *which)
{
	dns_question_t ztq;
	int32_t i, j, status, *si, silen;
	char *qp;
	u_int16_t qlen;

	if (dns == NULL) return DNS_STATUS_BAD_HANDLE;

	ztq.type = DNS_TYPE_AXFR;
	ztq.class = class;
	ztq.name = dns->domain;
	
	qp = dns_build_query_packet(dns, &ztq, &qlen, xid);
	if (qp == NULL)
	{
		dns_log_msg(dns, LOG_ERR, "dns_zone_transfer_query - malformed query");
		return DNS_STATUS_MALFORMED_QUERY;
	}

	silen = dns->server_count;
	si = (u_int32_t *)malloc(silen * sizeof(u_int32_t));
	for (j = 0, i = dns->selected_server; i < dns->server_count; i++, j++)
		si[j] = i;
	for (i = 0; i < dns->selected_server; i++, j++)
		si[j] = i;

	for (i = 0; i < silen; i++)
	{
		*which = si[i];
		dns_select_server(dns, si[i]);
	
		status = dns_send_query(dns, si[i], qp, qlen);
		if (status == DNS_STATUS_OK)
		{
			free(qp);
			free(si);
			return status;
		}
	}

	dns_select_server(dns, si[0]);

	free(qp);
	free(si);
	dns_log_msg(dns, LOG_ERR, "dns_zone_transfer_query - send failed");
	return DNS_STATUS_SEND_FAILED;
}

dns_reply_list_t *
dns_zone_transfer(dns_handle_t *dns, u_int16_t class)
{
	char *rp;
	u_int32_t status, which;
	u_int16_t xid, rplen;
	int proto;
	dns_reply_t *r;
	dns_reply_list_t *rlist;

	if (dns == NULL) return NULL;

	proto = dns->protocol;
	dns_set_protocol(dns, IPPROTO_TCP);
	status = dns_zone_transfer_query(dns, class, &xid, &which);
	if (status != DNS_STATUS_OK)
	{
		dns_log_msg(dns, LOG_ERR, "dns_zone_transfer - query failed");
		dns_set_protocol(dns, proto);
		return NULL;
	}

	rlist = (dns_reply_list_t *)malloc(sizeof(dns_reply_list_t));
	rlist->count = 0;
	rlist->reply = NULL;

	status = dns_read_reply(dns, which, 0, NULL, &rp, &rplen);
	while (status == DNS_STATUS_OK)
	{
		r = dns_parse_packet(rp);
		free(rp);

		r->status = status;
		r->server.s_addr = dns->server[which].sin_addr.s_addr;

		if (rlist->count == 0)
		{
			rlist->reply = (dns_reply_t **)malloc(sizeof(dns_reply_t *));
		}
		else
		{
			rlist->reply = (dns_reply_t **)realloc(rlist->reply, (rlist->count + 1) * sizeof(dns_reply_t *));
		}

		rlist->reply[rlist->count] = r;
		rlist->count++;

		if ((rlist->count > 1) &&
			(r->header->ancount > 0) &&
			(r->answer[0]->type == DNS_TYPE_SOA))
		{
			break;
		}

		status = dns_read_reply(dns, which, 0, NULL, &rp, &rplen);
	}

	dns_set_protocol(dns, proto);
	return rlist;
}

static int32_t
dns_send_query_server(dns_handle_t *dns, u_int32_t which,
	u_int16_t qxid,
	char *q, u_int16_t qlen,
	char **r, u_int16_t *rlen)
{
	ssize_t i;
	int32_t status, ss, dosend;
	u_int32_t slen;
	struct sockaddr *dst;
	fd_set readfds;
	int try, maxfd;
	char *qname;

	if (dns == NULL) return DNS_STATUS_BAD_HANDLE;
	if (which >= dns->server_count)
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_send_query_server - invalid server selection");
		return DNS_STATUS_BAD_HANDLE;
	}
	
	if (q == NULL)
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_send_query_server - malformed query");
		return DNS_STATUS_MALFORMED_QUERY;
	}

	if (qlen == 0)
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_send_query_server - malformed query");
		return DNS_STATUS_MALFORMED_QUERY;
	}

	slen = sizeof(struct sockaddr_in);
	maxfd = dns->sock + 1;
	dst = (struct sockaddr *)&(dns->server[which]);

	dns_select_server(dns, which);
	qname = q + DNS_HEADER_SIZE;
	if (dns->protocol == IPPROTO_TCP) qname += 2;

	if (dns->sockstate == DNS_SOCK_TCP_UNCONNECTED)
	{
		/* connect to server */
		status = connect(dns->sock, dst, sizeof(struct sockaddr_in));
		if (status < 0)
		{
			dns_log_msg(dns, LOG_ERR,
				"dns_send_query_server - TCP connect failed");
			return DNS_STATUS_CONNECTION_FAILED;
		}

		dns->sockstate = DNS_SOCK_TCP_CONNECTED;
	}

	status = DNS_STATUS_TIMEOUT;
	dosend = 1;
	try = 0;
	
	while (try <= dns->server_retries)
	{
		if (dosend == 1)
		{
			try++;
			i = -1;
#ifdef DEBUG_QUERY
			dns_log_msg(dns, LOG_DEBUG,
				"dns_send_query_server - XID %hu server %s",
				qxid, inet_ntoa(dns->server[which].sin_addr));
#endif
			i = sendto(dns->sock, q, qlen, 0, dst, slen);
			if (i < 0)
			{
				dns_log_msg(dns, LOG_ERR,
					"dns_send_query_server - send failed for %s",
					inet_ntoa(dns->server[which].sin_addr));
				return DNS_STATUS_SEND_FAILED;
			}
		}
		else dosend = 1;

		FD_ZERO(&readfds);
		FD_SET(dns->sock, &readfds);

		ss = select(maxfd, &readfds, NULL, NULL, &(dns->server_timeout));
		if (ss < 0)
		{
			dns_log_msg(dns, LOG_ERR,
				"dns_send_query_server - select failed");
			continue;
		}

		if (ss == 0)
		{
			dns_log_msg(dns, LOG_ERR,
				"dns_send_query_server - timeout for %s",
				inet_ntoa(dns->server[which].sin_addr));
			continue;
		}
	
		if (! FD_ISSET(dns->sock, &readfds))
		{
			dns_log_msg(dns, LOG_ERR,
				"dns_send_query_server - bad reply for %s",
				inet_ntoa(dns->server[which].sin_addr));
			continue;
		}
	
		status = dns_read_reply(dns, which, qxid, qname, r, rlen);

		if ((status == DNS_STATUS_WRONG_SERVER) ||
			(status == DNS_STATUS_WRONG_XID))
		{
			dosend = 0;
			continue;
		}
		
		return status;
	}

	return status;
}

dns_reply_t *
dns_fqdn_query_server(dns_handle_t *dns, u_int32_t which, dns_question_t *dnsq)
{
	dns_reply_t *r;
	char *qp, *rp;
	int32_t i, j, status, *si, silen;
	u_int16_t rplen, qplen, xid;

	if (dns == NULL) return NULL;
	if (dnsq == NULL) return NULL;

	if ((which != (u_int32_t)-1) && (which >= dns->server_count))
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_fqdn_query_server - invalid server selection");
		return NULL;
	}

	status = DNS_STATUS_SEND_FAILED;

	qp = dns_build_query_packet(dns, dnsq, &qplen, &xid);
	if (qp == NULL)
	{
		dns_log_msg(dns, LOG_ERR,
			"dns_fqdn_query_server - malformed query");
		return NULL;
	}

	silen = 0;
	
	if (which == (u_int32_t)-1)
	{
		silen = dns->server_count;
		si = (u_int32_t *)malloc(silen * sizeof(u_int32_t));
		for (j = 0, i = dns->selected_server; i < dns->server_count; i++, j++)
			si[j] = i;
		for (i = 0; i < dns->selected_server; i++, j++)
			si[j] = i;
	}
	else
	{
		silen = 1;
		si = (u_int32_t *)malloc(sizeof(u_int32_t));
		si[0] = which;
	}

	for (i = 0; i < silen; i++)
	{
		dns_select_server(dns, si[i]);
	
		rplen = REPLY_BUF_SIZE;
		status = dns_send_query_server(dns, si[i], xid, qp, qplen, &rp, &rplen);

		if (status == DNS_STATUS_OK)
		{
			r = dns_parse_packet(rp);
			if (r->header->flags & DNS_FLAGS_TC)
			{
				/* Switch to TCP and try again */
				free(rp);
				free(qp);
				dns_free_reply(r);

				dns_set_protocol(dns, IPPROTO_TCP);
				qp = dns_build_query_packet(dns, dnsq, &qplen, &xid);
				if (qp == NULL) return NULL;
				rplen = REPLY_BUF_SIZE;
				status = dns_send_query_server(dns, si[i], xid, qp, qplen, &rp, &rplen);

				/* Switch back to UDP */
				dns_set_protocol(dns, IPPROTO_UDP);

				/* If the query failed, return to the loop to try again. */
				if (status != DNS_STATUS_OK) continue;
				r = dns_parse_packet(rp);
			}

			dns_apply_sortlist(dns, r);
			free(qp);
			free(rp);
			r->status = status;
			r->server.s_addr = dns->server[si[i]].sin_addr.s_addr;
			free(si);
			return r;
		}
		else if (status == DNS_STATUS_WRONG_QUESTION)
		{
			break;
		}
		else
		{
			dns_log_msg(dns, LOG_ERR,
				"dns_fqdn_query_server - query failed for %s",
				inet_ntoa(dns->server[si[i]].sin_addr));
		}
	}

	dns_select_server(dns, si[0]);

	free(qp);
	free(si);
	r = (dns_reply_t *)malloc(sizeof(dns_reply_t));
	memset(r, 0, sizeof(dns_reply_t));
	r->status = status;
	return r;
}

dns_reply_t *
dns_fqdn_query(dns_handle_t *dns, dns_question_t *dnsq)
{
	return dns_fqdn_query_server(dns, (u_int32_t)-1, dnsq);
}

dns_reply_t *
dns_query_server(dns_handle_t *dns, u_int32_t which, dns_question_t *dnsq)
{
	dns_reply_t *r;
	dns_question_t fqdnq;
	u_int32_t i, ndots, len;
	char *p, *name;

	len = strlen(dnsq->name);
	if (dnsq->name[len - 1] == '.')
		return dns_fqdn_query_server(dns, which, dnsq);

	ndots = 0;
	for (p = dnsq->name; *p != '\0'; p++) if (*p == '.') ndots++;

	if (ndots >= dns->ias_dots) 
	{
		r = dns_fqdn_query_server(dns, which, dnsq);
		if (r != NULL)
		{
			if ((r->status == DNS_STATUS_OK) && (r->header->ancount != 0)) return r;
			dns_free_reply(r);
		}
	}

	fqdnq.type = dnsq->type;
	fqdnq.class = dnsq->class;
	name = strdup(dnsq->name);
	while (name[strlen(name) - 1] == '.')
		name[strlen(name) - 1] = '\0';
		
	if (dns->search_count == 0)
	{
		fqdnq.name = malloc(strlen(name) + strlen(dns->domain) + 2);
		sprintf(fqdnq.name, "%s.%s", name, dns->domain);
	
		r = dns_fqdn_query_server(dns, which, &fqdnq);
		free(fqdnq.name);
		free(name);
		return r;
	}

	for (i = 0; i < dns->search_count; i++)
	{
		fqdnq.name = malloc(strlen(name) + strlen(dns->search[i]) + 2);
		sprintf(fqdnq.name, "%s.%s", name, dns->search[i]);
	
		r = dns_fqdn_query_server(dns, which, &fqdnq);
		free(fqdnq.name);
		if (r != NULL)
		{
			if ((r->status == DNS_STATUS_OK) && (r->header->ancount != 0))
			{
				free(name);
				return r;
			}
			dns_free_reply(r);
		}
	}

	free(name);
	return NULL;
}

dns_reply_t *
dns_query(dns_handle_t *dns, dns_question_t *dnsq)
{
	return dns_query_server(dns, (u_int32_t)-1, dnsq);
}
