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

#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <nameser.h>
#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#define SOCK_UNSPEC 0
#define IPPROTO_UNSPEC 0

#define WANT_A4_ONLY 1
#define WANT_A6_ONLY 2
#define WANT_A6_PLUS_MAPPED_A4 3
#define WANT_A6_OR_MAPPED4_IF_NO_A6 4

#define LONG_STRING_LENGTH 8192
#define _LU_MAXLUSTRLEN 256

extern int _lu_running(void);
extern mach_port_t _lookupd_port();
extern int _lookup_link();
extern int _lookup_one();
extern int _lookup_all();

struct lu_dict
{
	int count;
	char *type;
	char *name;
	char *cname;
	char *mx;
	char *ipv4;
	char *ipv6;
	char *service;
	char *port;
	char *protocol;
	char *target;
	char *priority;
	char *weight;
	struct lu_dict *lu_next;
};

#define LU_NAME     1
#define LU_CNAME    2
#define LU_MX       3
#define LU_IPV4     4
#define LU_IPV6     5
#define LU_PORT     6
#define LU_TARGET   7
#define LU_PRIORITY 8
#define LU_WEIGHT   9

static int supported_family[] =
{
	PF_UNSPEC,
	PF_INET,
	PF_INET6
};
static int supported_family_count = 3;

static int supported_socket[] =
{
	SOCK_RAW,
	SOCK_UNSPEC,
	SOCK_DGRAM,
	SOCK_STREAM
};
static int supported_socket_count = 4;

static int supported_protocol[] =
{
	IPPROTO_UNSPEC,
	IPPROTO_ICMPV6,
	IPPROTO_UDP,
	IPPROTO_TCP
};
static int supported_protocol_count = 4;

static int supported_socket_protocol_pair[] =
{
	SOCK_RAW,    IPPROTO_UNSPEC,
	SOCK_RAW,    IPPROTO_UDP,
	SOCK_RAW,    IPPROTO_TCP,
	SOCK_RAW,    IPPROTO_ICMPV6,
	SOCK_UNSPEC, IPPROTO_UNSPEC,
	SOCK_UNSPEC, IPPROTO_UDP,
	SOCK_UNSPEC, IPPROTO_TCP,
	SOCK_UNSPEC, IPPROTO_ICMPV6,
	SOCK_DGRAM,  IPPROTO_UNSPEC,
	SOCK_DGRAM,  IPPROTO_UDP,
	SOCK_STREAM, IPPROTO_UNSPEC,
	SOCK_STREAM, IPPROTO_TCP
};
static int supported_socket_protocol_pair_count = 12;

static int
gai_family_type_check(int f)
{
	int i;
	
	for (i = 0; i < supported_family_count; i++)
	{
		if (f == supported_family[i]) return 0;
	}

	return 1;
}

static int
gai_socket_type_check(int s)
{
	int i;
	
	for (i = 0; i < supported_socket_count; i++)
	{
		if (s == supported_socket[i]) return 0;
	}

	return 1;
}

static int
gai_protocol_type_check(int p)
{
	int i;
	
	for (i = 0; i < supported_protocol_count; i++)
	{
		if (p == supported_protocol[i]) return 0;
	}

	return 1;
}

static int
gai_socket_protocol_type_check(int s, int p)
{
	int i, j, ss, sp;
	
	for (i = 0, j = 0; i < supported_socket_protocol_pair_count; i++, j+=2)
	{
		ss = supported_socket_protocol_pair[j];
		sp = supported_socket_protocol_pair[j+1];
		if ((s == ss) && (p == sp)) return 0;
	}

	return 1;
}

static int
gai_inet_pton(const char *s, struct in6_addr *a6)
{
	if (s == NULL) return 0;
	if (a6 == NULL) return 0;
	return inet_pton(AF_INET6, s, (void *)&a6->__u6_addr.__u6_addr32[0]);
}

char *
gai_strerror(int err)
{
	switch (err)
	{
		case EAI_ADDRFAMILY: return "Address family for nodename not supported";
		case EAI_AGAIN: return "Temporary failure in name resolution";
		case EAI_BADFLAGS:	return "Invalid value for ai_flags";
		case EAI_FAIL: return "Non-recoverable failure in name resolution";
		case EAI_FAMILY: return "ai_family not supported";
		case EAI_MEMORY: return "Memory allocation failure";
		case EAI_NODATA: return "No address associated with nodename";
		case EAI_NONAME: return "nodename nor servname provided, or not known";
		case EAI_SERVICE: return "servname not supported for ai_socktype";
		case EAI_SOCKTYPE: return "ai_socktype not supported";
		case EAI_SYSTEM: return "System error";
		case EAI_BADHINTS: return "Bad hints";
		case EAI_PROTOCOL: return "ai_protocol not supported";
	}

	return "Unknown error";
}

static int
is_ipv4_address(const char *s)
{
	struct in_addr a;

	if (s == NULL) return 0;
	return inet_aton(s, &a);
}

static int
is_ipv6_address(const char *s)
{
	struct in6_addr a;

	if (s == NULL) return 0;
	return gai_inet_pton(s, &a);
}

static void
append_addrinfo(struct addrinfo **l, struct addrinfo *a)
{
	struct addrinfo *x;

	if (l == NULL) return;
	if (a == NULL) return;

	if (*l == NULL)
	{
		*l = a;
		return;
	}

	x = *l;

	if (a->ai_family == PF_INET6)
	{
		if (x->ai_family == PF_INET)
		{
			*l = a;
			a->ai_next = x;
			return;
		}

		while ((x->ai_next != NULL) && (x->ai_next->ai_family != PF_INET)) x = x->ai_next;
		a->ai_next = x->ai_next;
		x->ai_next = a;
	}
	else
	{
		while (x->ai_next != NULL) x = x->ai_next;
		a->ai_next = NULL;
		x->ai_next = a;
	}
}

static void
free_lu_dict(struct lu_dict *d)
{
	struct lu_dict *next;

	while (d != NULL)
	{
		next = d->lu_next;
		if (d->type != NULL) free(d->type);
		if (d->name != NULL) free(d->name);
		if (d->cname != NULL) free(d->cname);
		if (d->mx != NULL) free(d->mx);
		if (d->ipv4 != NULL) free(d->ipv4);
		if (d->ipv6 != NULL) free(d->ipv6);
		if (d->service != NULL) free(d->service);
		if (d->port != NULL) free(d->port);
		if (d->protocol != NULL) free(d->protocol);
		if (d->target != NULL) free(d->target);
		if (d->priority != NULL) free(d->priority);
		if (d->weight != NULL) free(d->weight);
		free(d);
		d = next;
	}
}

static int
_lu_str_equal(char *a, char *b)
{
	if (a == NULL)
	{
		if (b == NULL) return 1;
		return 0;
	}

	if (b == NULL) return 0;

	if (!strcmp(a, b)) return 1;
	return 0;
}

static int
lu_dict_equal(struct lu_dict *a, struct lu_dict *b)
{
	if (a == NULL) return 0;
	if (b == NULL) return 0;

	if (_lu_str_equal(a->type, b->type) == 0) return 0;
	if (_lu_str_equal(a->name, b->name) == 0) return 0;
	if (_lu_str_equal(a->cname, b->cname) == 0) return 0;
	if (_lu_str_equal(a->mx, b->mx) == 0) return 0;
	if (_lu_str_equal(a->ipv4, b->ipv4) == 0) return 0;
	if (_lu_str_equal(a->ipv6, b->ipv6) == 0) return 0;
	if (_lu_str_equal(a->service, b->service) == 0) return 0;
	if (_lu_str_equal(a->port, b->port) == 0) return 0;
	if (_lu_str_equal(a->protocol, b->protocol) == 0) return 0;
	if (_lu_str_equal(a->target, b->target) == 0) return 0;
	if (_lu_str_equal(a->priority, b->priority) == 0) return 0;
	if (_lu_str_equal(a->weight, b->weight) == 0) return 0;
	return 1;
}

/*
 * Append a single dictionary to a list if it is unique.
 * Free it if it is not appended.
 */
static void
merge_lu_dict(struct lu_dict **l, struct lu_dict *d)
{
	struct lu_dict *x, *e;

	if (l == NULL) return;
	if (d == NULL) return;

	if (*l == NULL)
	{
		*l = d;
		return;
	}

	e = *l;
	for (x = *l; x != NULL; x = x->lu_next)
	{
		e = x;
		if (lu_dict_equal(x, d))
		{
			free_lu_dict(d);
			return;
		}
	}

	e->lu_next = d;
}

static void
append_lu_dict(struct lu_dict **l, struct lu_dict *d)
{
	struct lu_dict *x, *next;

	if (l == NULL) return;
	if (d == NULL) return;

	if (*l == NULL)
	{
		*l = d;
		return;
	}

	x = d;

	while (x != NULL)
	{
		next = x->lu_next;
		x->lu_next = NULL;
		merge_lu_dict(l, x);
		x = next;
	}
}

/*
 * We collect values for the following keys:
 *
 *    name
 *    cname
 *    port
 *    ip_address
 *    ipv6_address
 *    target
 *    mail_exchanger
 *    priority
 *    preference
 *    weight
 */
static void
lookupd_process_dictionary(XDR *inxdr, struct lu_dict **l)
{
	int i, nkeys, j, nvals;
	int addme;
	char *key, *val;
	struct lu_dict *d;

	if (!xdr_int(inxdr, &nkeys)) return;

	d = (struct lu_dict *)malloc(sizeof(struct lu_dict));
	memset(d, 0, sizeof(struct lu_dict));

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;

		if (!xdr_string(inxdr, &key, LONG_STRING_LENGTH))
		{
			free_lu_dict(d);
			return;
		}

		addme = 0;
		if (!strcmp(key, "name")) addme = LU_NAME;
		else if (!strcmp(key, "cname")) addme = LU_CNAME;
		else if (!strcmp(key, "mail_exchanger")) addme = LU_MX;
		else if (!strcmp(key, "ip_address")) addme = LU_IPV4;
		else if (!strcmp(key, "ipv6_address")) addme = LU_IPV6;
		else if (!strcmp(key, "port")) addme = LU_PORT;
		else if (!strcmp(key, "target")) addme = LU_TARGET;
		else if (!strcmp(key, "priority")) addme = LU_PRIORITY;
		else if (!strcmp(key, "preference")) addme = LU_PRIORITY;
		else if (!strcmp(key, "weight")) addme = LU_WEIGHT;
		free(key);

		if (!xdr_int(inxdr, &nvals))
		{
			free_lu_dict(d);
			return;
		}
	
		for (j = 0; j < nvals; j++)
		{
			val = NULL;
			if (!xdr_string(inxdr, &val, LONG_STRING_LENGTH))
			{
				free_lu_dict(d);
				return;
			}

			if (addme == 0) free(val);
			else if (addme == LU_NAME) d->name = val;
			else if (addme == LU_CNAME) d->cname = val;
			else if (addme == LU_MX) d->mx = val;
			else if (addme == LU_IPV4) d->ipv4 = val;
			else if (addme == LU_IPV6) d->ipv6 = val;
			else if (addme == LU_PORT) d->port = val;
			else if (addme == LU_TARGET) d->target = val;
			else if (addme == LU_PRIORITY) d->priority = val;
			else if (addme == LU_WEIGHT) d->weight = val;

			if (addme != 0) d->count++;
			addme = 0;
		}
	}

	merge_lu_dict(l, d);
}

static int
encode_kv(XDR *x, char *k, char *v)
{
	int n = 1;

	if (!xdr_string(x, &k, _LU_MAXLUSTRLEN)) return 1;
	if (!xdr_int(x, &n)) return 1;
	if (!xdr_string(x, &v, _LU_MAXLUSTRLEN)) return 1;

	return 0;
}

static int
gai_files(struct lu_dict *q, struct lu_dict **list)
{
	int port, i;
	struct servent *s;
	struct hostent *h;
	struct lu_dict *d;
	char str[64], portstr[64];
	struct in_addr a4;

	if (!strcmp(q->type, "service"))
	{
		s = NULL;
		if (q->name != NULL)
		{
			s = getservbyname(q->name, q->protocol);
		}
		else if (q->port != NULL)
		{
			port = atoi(q->port);
			s = getservbyport(port, q->protocol);
		}
		if (s == NULL) return 0;
	
		d = (struct lu_dict *)malloc(sizeof(struct lu_dict));
		memset(d, 0, sizeof(struct lu_dict));

		if (s->s_name != NULL) d->name = strdup(s->s_name);
		sprintf(str, "%u", ntohl(s->s_port));
		d->port = strdup(str);
		if (s->s_proto != NULL) d->protocol = strdup(s->s_proto);

		merge_lu_dict(list, d);
		return 1;
	}

	if (!strcmp(q->type, "host"))
	{
		s = NULL;
		if (q->service != NULL)
		{
			s = getservbyname(q->service, q->protocol);
		}
		else if (q->port != NULL)
		{
			port = atoi(q->port);
			s = getservbyport(port, q->protocol);
		}

		sprintf(portstr, "0");
		if (s != NULL) sprintf(portstr, "%u", ntohl(s->s_port));

		h = NULL;
		if (q->name != NULL)
		{
			h = gethostbyname(q->name);
		}
		else if (q->ipv4 != NULL)
		{
			if (inet_aton(q->ipv4, &a4) == 0) return -1;
			h = gethostbyaddr((char *)&a4, sizeof(struct in_addr), PF_INET);
		}
		else 
		{
			/* gethostbyaddr for IPV6? */
			return 0;
		}
		
		if (h == NULL) return 0;

		for (i = 0; h->h_addr_list[i] != 0; i++)
		{
			d = (struct lu_dict *)malloc(sizeof(struct lu_dict));
			memset(d, 0, sizeof(struct lu_dict));

			if (h->h_name != NULL)
			{
				d->name = strdup(h->h_name);
				d->target = strdup(h->h_name);
			}
			memmove((void *)&a4.s_addr, h->h_addr_list[i], h->h_length);

			sprintf(str, "%s", inet_ntoa(a4));
			d->ipv4 = strdup(str);

			if (s != NULL)
			{
				if (s->s_name != NULL) d->service = strdup(s->s_name);
				d->port = strdup(portstr);
			}

			merge_lu_dict(list, d);
		}
		return i;
	}

	return 0;
}

static int
gai_lookupd(struct lu_dict *q, struct lu_dict **list)
{
	unsigned datalen;
	XDR outxdr;
	XDR inxdr;
	int proc;
	char *listbuf;
	char databuf[_LU_MAXLUSTRLEN * BYTES_PER_XDR_UNIT];
	int n, i, na;
	kern_return_t status;
	mach_port_t server_port;

	if (q == NULL) return 0;
	if (q->count == 0) return 0;
	if (q->type == NULL) return 0;
	
	if (list == NULL) return -1;

	server_port = MACH_PORT_NULL;
	if (_lu_running()) server_port = _lookupd_port(0);

	if (server_port == MACH_PORT_NULL) return gai_files(q, list);

	status = _lookup_link(server_port, "query", &proc);
	if (status != KERN_SUCCESS) return gai_files(q, list);

	xdrmem_create(&outxdr, databuf, sizeof(databuf), XDR_ENCODE);

	/* Encode attribute count */
	na = q->count;
	if (!xdr_int(&outxdr, &na))
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	if (encode_kv(&outxdr, "_lookup_category", q->type) != 0)
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	if (q->name != NULL)
	{
		if (encode_kv(&outxdr, "name", q->name) != 0)
		{
			xdr_destroy(&outxdr);
			return -1;
		}
	}

	if (q->ipv4 != NULL)
	{
		if (encode_kv(&outxdr, "ip_address", q->ipv4) != 0)
		{
			xdr_destroy(&outxdr);
			return -1;
		}
	}

	if (q->ipv6 != NULL)
	{
		if (encode_kv(&outxdr, "ipv6_address", q->ipv6) != 0)
		{
			xdr_destroy(&outxdr);
			return -1;
		}
	}

	if (q->service != NULL)
	{
		if (encode_kv(&outxdr, "service", q->service) != 0)
		{
			xdr_destroy(&outxdr);
			return -1;
		}
	}

	if (q->port != NULL)
	{
		if (encode_kv(&outxdr, "port", q->port) != 0)
		{
			xdr_destroy(&outxdr);
			return -1;
		}
	}

	if (q->protocol != NULL)
	{
		if (encode_kv(&outxdr, "protocol", q->protocol) != 0)
		{
			xdr_destroy(&outxdr);
			return -1;
		}
	}

	listbuf = NULL;
	datalen = 0;

	n = xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT;
	status = _lookup_all(server_port, proc, databuf, n, &listbuf, &datalen);
	if (status != KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;

	xdrmem_create(&inxdr, listbuf, datalen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		return -1;
	}
	
	for (i = 0; i < n; i++)
	{
		lookupd_process_dictionary(&inxdr, list);
	}

	xdr_destroy(&inxdr);

	vm_deallocate(mach_task_self(), (vm_address_t)listbuf, datalen);
	
	return n;
}

void
freeaddrinfo(struct addrinfo *a)
{
	struct addrinfo *next;

	while (a != NULL)
	{
		next = a->ai_next;
		if (a->ai_addr != NULL) free(a->ai_addr);
		if (a->ai_canonname != NULL) free(a->ai_canonname);
		free(a);
		a = next;
	}
}

static struct addrinfo *
new_addrinfo_v4(int flags, int sock, int proto, unsigned short port, struct in_addr addr, char *cname)
{
	struct addrinfo *a;
	struct sockaddr_in *sa;
	int len;

	a = (struct addrinfo *)malloc(sizeof(struct addrinfo));
	memset(a, 0, sizeof(struct addrinfo));
	a->ai_next = NULL;

	a->ai_flags = flags;
	a->ai_family = PF_INET;
	a->ai_socktype = sock;
	a->ai_protocol = proto;

	a->ai_addrlen = sizeof(struct sockaddr_in);
	sa = (struct sockaddr_in *)malloc(a->ai_addrlen);
	memset(sa, 0, a->ai_addrlen);
	sa->sin_len = a->ai_addrlen;
	sa->sin_family = PF_INET;
	sa->sin_port = port;
	sa->sin_addr = addr;
	a->ai_addr = (struct sockaddr *)sa;

	if (cname != NULL)
	{
		len = strlen(cname) + 1;
		a->ai_canonname = malloc(len);
		memmove(a->ai_canonname, cname, len);
	}

	return a;
}

static struct addrinfo *
new_addrinfo_v6(int flags, int sock, int proto, unsigned short port, struct in6_addr addr, char *cname)
{
	struct addrinfo *a;
	struct sockaddr_in6 *sa;
	int len;

	a = (struct addrinfo *)malloc(sizeof(struct addrinfo));
	memset(a, 0, sizeof(struct addrinfo));
	a->ai_next = NULL;

	a->ai_flags = flags;
	a->ai_family = PF_INET6;
	a->ai_socktype = sock;
	a->ai_protocol = proto;

	a->ai_addrlen = sizeof(struct sockaddr_in6);
	sa = (struct sockaddr_in6 *)malloc(a->ai_addrlen);
	memset(sa, 0, a->ai_addrlen);
	sa->sin6_len = a->ai_addrlen;
	sa->sin6_family = PF_INET6;
	sa->sin6_port = port;
	sa->sin6_addr = addr;
	a->ai_addr = (struct sockaddr *)sa;

	if (cname != NULL)
	{
		len = strlen(cname) + 1;
		a->ai_canonname = malloc(len);
		memmove(a->ai_canonname, cname, len);
	}

	return a;
}

static void
grok_nodename(const char *nodename, int family, struct lu_dict *q)
{
	if (nodename == NULL) return;
	if (q == NULL) return;

	if (((family == PF_UNSPEC) || (family == PF_INET)) && is_ipv4_address(nodename))
	{
		q->ipv4 = (char *)nodename;
	}
	else if (((family == PF_UNSPEC) || (family == PF_INET6)) && is_ipv6_address(nodename))
	{
		q->ipv6 = (char *)nodename;
	}
	else
	{
		q->name = (char *)nodename;
	}
}
	
static void
grok_service(const char *servname, struct lu_dict *q)
{
	int port;
	char *p;

	if (servname == NULL) return;
	if (q == NULL) return;

	port = 0;
	for (p = (char *)servname; (port == 0) && (*p != '\0'); p++)
	{
		if (!isdigit(*p)) port = -1;
	}

	if (port == 0) q->port = (char *)servname;
	else q->service = (char *)servname;
}

static int
gai_numerichost(struct lu_dict *h, struct lu_dict **list)
{
	struct lu_dict *a;
	int n;

	if (h == NULL) return 0;
	if (list == NULL) return -1;

	n = 0;

	if (h->ipv4 != NULL)
	{
		a = (struct lu_dict *)malloc(sizeof(struct lu_dict));
		memset(a, 0, sizeof(struct lu_dict));
		a->ipv4 = strdup(h->ipv4);
		merge_lu_dict(list, a);
		n++;
	}

	if (h->ipv6 != NULL)
	{
		a = (struct lu_dict *)malloc(sizeof(struct lu_dict));
		memset(a, 0, sizeof(struct lu_dict));
		a->ipv6 = strdup(h->ipv6);
		merge_lu_dict(list, a);
		n++;
	}

	return n;	
}

static int
gai_numericserv(struct lu_dict *s, struct lu_dict **list)
{
	struct lu_dict *a;

	if (s == NULL) return 0;
	if (s->port == NULL) return 0;
	if (list == NULL) return -1;

	a = (struct lu_dict *)malloc(sizeof(struct lu_dict));
	memset(a, 0, sizeof(struct lu_dict));
	a->port = strdup(s->port);
	merge_lu_dict(list, a);
	return 1;	
}

static void
merge_addr4(struct in_addr a, struct sockaddr_in ***l, int *n)
{
	int i;
	struct sockaddr_in *sa4;

	for (i = 0; i < *n; i++)
	{
		if (((*l)[i]->sin_family == PF_INET) && (a.s_addr == (*l)[i]->sin_addr.s_addr)) return;
	}
	
	sa4 = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	memset(sa4, 0, sizeof(struct sockaddr_in));

	sa4->sin_family = PF_INET;
	sa4->sin_addr = a;

	if (*n == 0) *l = (struct sockaddr_in **)malloc(sizeof(struct sockaddr_in *));
	else *l = (struct sockaddr_in **)realloc(*l, (*n + 1) * sizeof(struct sockaddr_in *));

	(*l)[*n] = sa4;
	(*n)++;
}

static void
merge_addr6(struct in6_addr a, struct sockaddr_in6 ***l, int *n)
{
	int i;
	struct sockaddr_in6 *sa6;

	for (i = 0; i < *n; i++)
	{
		if (((*l)[i]->sin6_family == PF_INET6)
		&& (a.__u6_addr.__u6_addr32[0] == (*l)[i]->sin6_addr.__u6_addr.__u6_addr32[0])) return;
	}
	
	sa6 = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6));
	memset(sa6, 0, sizeof(struct sockaddr_in6));

	sa6->sin6_family = PF_INET6;
	sa6->sin6_addr = a;

	if (*n == 0) *l = (struct sockaddr_in6 **)malloc(sizeof(struct sockaddr_in6 *));
	else *l = (struct sockaddr_in6 **)realloc(*l, (*n + 1) * sizeof(struct sockaddr_in6 *));

	(*l)[*n] = sa6;
	(*n)++;
}

/*
 * N.B. We use sim_family to store protocol in the sockaddr.
 * sockaddr is just used here as a data structure to keep
 * (port, protocol) pairs.
 */
static void
merge_plist(unsigned short port, unsigned short proto, struct sockaddr_in ***l, int *n)
{
	int i;
	struct sockaddr_in *sa4;

	for (i = 0; i < *n; i++)
	{
		if ((port == (*l)[i]->sin_port) && (proto == (*l)[i]->sin_family)) return;
	}
	
	sa4 = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	memset(sa4, 0, sizeof(struct sockaddr_in));

	sa4->sin_port = port;
	sa4->sin_family = proto;

	if (*n == 0) *l = (struct sockaddr_in **)malloc(sizeof(struct sockaddr_in *));
	else *l = (struct sockaddr_in **)realloc(*l, (*n + 1) * sizeof(struct sockaddr_in *));

	(*l)[*n] = (struct sockaddr_in *)sa4;
	(*n)++;
}

/*
 * Compute x[n + 1] = (7^5 * x[n]) mod (2^31 - 1).
 * From "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 */
static int
gai_random()
{
	static int did_init = 0;
	static unsigned int randseed = 1;
	int x, hi, lo, t; 
	struct timeval tv;

	if (did_init++ == 0)
	{
		gettimeofday(&tv, NULL);
		randseed = tv.tv_usec;
		if(randseed == 0) randseed = 1;
	}

	x = randseed;
	hi = x / 127773;
	lo = x % 127773;
	t = 16807 * lo - 2836 * hi;
	if (t <= 0) t += 0x7fffffff;
	randseed = t;
	return t;
}

/*
 * Sort by priority and weight.
 */
void
lu_prioity_sort(struct lu_dict **l)
{
	struct lu_dict *d, **nodes;
	unsigned int x, count, i, j, bottom, *pri, *wgt, swap, t;

	if (*l == NULL) return;

	count = 0;
	for (d = *l; d != NULL; d = d->lu_next) count++;
	nodes = (struct lu_dict **)malloc(count * sizeof(struct lu_dict *));
	pri = (unsigned int *)malloc(count * sizeof(unsigned int));
	wgt = (unsigned int *)malloc(count * sizeof(unsigned int));

	for (i = 0, d = *l; d != NULL; d = d->lu_next, i++)
	{
		nodes[i] = d;

		x = (unsigned int)-1;
		if (d->priority != NULL) x = atoi(d->priority);
		pri[i] = x;

		x = 0;
		if (d->weight != NULL) x = atoi(d->weight);
		wgt[i] = (gai_random() % 10000) * x;
	}

	/* bubble sort by priority */
	swap = 1;
	bottom = count - 1;

	while (swap == 1)
	{
		swap = 0;
		for (i = 0, j = 1; i < bottom; i++, j++)
		{
			if (pri[i] < pri[j]) continue;
			if ((pri[i] == pri[j]) && (wgt[i] < wgt[j])) continue;
			swap = 1;

			t = pri[i];
			pri[i] = pri[j];
			pri[j] = t;

			t = wgt[i];
			wgt[i] = wgt[j];
			wgt[j] = t;

			d = nodes[i];
			nodes[i] = nodes[j];
			nodes[j] = d;
		}

		bottom--;
	}
	
	*l = nodes[0];
	bottom = count - 1;
	for (i = 0, j = 1; i < bottom; i++, j++) nodes[i]->lu_next = nodes[j];
	nodes[bottom]->lu_next = NULL;

	free(pri);
	free(wgt);
	free(nodes);
}

/*
 * Get service records from lookupd
 */
static void
gai_serv_lookupd(const char *servname, int proto, int numericserv, struct lu_dict **list)
{
	struct lu_dict q, *sub;

	memset(&q, 0, sizeof(struct lu_dict));
	q.count = 3;
	q.type = "service";

	grok_service(servname, &q);
	if (q.service != NULL)
	{
		q.name = q.service;
		q.service = NULL;
	}

	if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
	{
		sub = NULL;
		q.protocol = "udp";
		if (numericserv == 1) gai_numericserv(&q, &sub);
		else gai_lookupd(&q, &sub);
		append_lu_dict(list, sub);
		for (; sub != NULL; sub = sub->lu_next) sub->protocol = strdup("udp");
	}

	if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
	{
		sub = NULL;
		q.protocol = "tcp";
		if (numericserv == 1) gai_numericserv(&q, &sub);
		else gai_lookupd(&q, &sub);
		append_lu_dict(list, sub);
		for (; sub != NULL; sub = sub->lu_next) sub->protocol = strdup("tcp");
	}
}

/*
 * Find a service.
 */
static int
gai_serv(const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	struct lu_dict *list, *d;
	int proto, family, socktype, setcname, wantv4, wantv6;
	unsigned short port;
	char *loopv4, *loopv6;
	struct addrinfo *a;
	struct in_addr a4;
	struct in6_addr a6;

	loopv4 = "127.0.0.1";
	loopv6 = "0:0:0:0:0:0:0:1";

	family = PF_UNSPEC;
	proto = IPPROTO_UNSPEC;
	setcname = 0;

	if (hints != NULL)
	{
		proto = hints->ai_protocol;
		if (hints->ai_socktype == SOCK_DGRAM) proto = IPPROTO_UDP;
		if (hints->ai_socktype == SOCK_STREAM) proto = IPPROTO_TCP;

		if (hints->ai_flags & AI_CANONNAME) setcname = 1;

		if ((hints->ai_flags & AI_PASSIVE) == 1)
		{
			loopv4 = "0.0.0.0";
			loopv6 = "0:0:0:0:0:0:0:0";
		}

		family = hints->ai_family;
	}

	wantv4 = 1;
	wantv6 = 1;
	if (family == PF_INET6) wantv4 = 0;
	if (family == PF_INET) wantv6 = 0;

	list = NULL;
	gai_serv_lookupd(servname, proto, 0, &list);
	if (list == NULL) gai_serv_lookupd(servname, proto, 1, &list);
		
	for (d = list; d != NULL; d = d->lu_next)
	{
		/* We only want records with port and protocol specified */
		if ((d->port == NULL) || (d->protocol == NULL)) continue;

		port = htons(atoi(d->port));
		proto = IPPROTO_UDP;
		socktype = SOCK_DGRAM;
		if (!strcasecmp(d->protocol, "tcp"))
		{
			proto = IPPROTO_TCP;
			socktype = SOCK_STREAM;
		}

		if (wantv4 == 1)
		{
			inet_aton(loopv4, &a4);
			a = new_addrinfo_v4(0, socktype, proto, port, a4, NULL);
			append_addrinfo(res, a);
		}

		if (wantv6 == 1)
		{
			gai_inet_pton(loopv6, &a6);
			a = new_addrinfo_v6(0, socktype, proto, port, a6, NULL);
			append_addrinfo(res, a);
		}
	}

	free_lu_dict(list);

	/* Set cname in first result */
	if ((setcname == 1) && (*res != NULL))
	{
		if (res[0]->ai_canonname == NULL) res[0]->ai_canonname = strdup("localhost");
	}

	return 0;
}

/*
 * Find a node.
 */
static void
gai_node_lookupd(const char *nodename, int family, int numerichost, struct lu_dict **list)
{
	struct lu_dict q;

	memset(&q, 0, sizeof(struct lu_dict));
	q.count = 2;
	q.type = "host";

	grok_nodename(nodename, family, &q);
	if (numerichost) gai_numerichost(&q, list);
	else gai_lookupd(&q, list);
}

/*
 * Find a node.
 */
static int
gai_node(const char *nodename, const struct addrinfo *hints, struct addrinfo **res)
{
	int i, family, numerichost, setcname, a_list_count, wantv4, wantv6;
	struct lu_dict *list, *d, *t;
	char *cname;
	struct in_addr a4;
	struct in6_addr a6;
	struct addrinfo *a;
	struct sockaddr **a_list;
	struct sockaddr_in *sa4;
	struct sockaddr_in6 *sa6;

	a_list_count = 0;
	a_list = NULL;

	numerichost = 0;
	family = PF_UNSPEC;
	setcname = 0;
	cname = NULL;

	if (hints != NULL)
	{
		family = hints->ai_family;
		if (hints->ai_flags & AI_NUMERICHOST) numerichost = 1;
		if (hints->ai_flags & AI_CANONNAME) setcname = 1;
	}

	wantv4 = 1;
	wantv6 = 1;
	if (family == PF_INET6) wantv4 = 0;
	if (family == PF_INET) wantv6 = 0;

	t = NULL;
	gai_node_lookupd(nodename, family, numerichost, &t);
	if ((t == NULL) && (numerichost == 0))
	{
		gai_node_lookupd(nodename, family, 1, &t);
	}

	/* If the nodename is an alias, look up the real name */
	list = NULL;
	for (d = t; d != NULL; d = d->lu_next)
	{
		if (d->cname != NULL)
		{
			if (cname == NULL) cname = strdup(d->cname);
			gai_node_lookupd(d->cname, family, 0, &t);
		}
	}

	append_lu_dict(&list, t);
	
	for (d = list; d != NULL; d = d->lu_next)
	{
		/* Check for cname */
		if ((cname == NULL) && (d->name != NULL) && (strchr(d->name, '.') != NULL))
		{
			cname = strdup(d->name);
		}

		/* Check for ipv4 address */
		if ((d->ipv4 != NULL) && (wantv4 == 1))
		{
			inet_aton(d->ipv4, &a4);
			merge_addr4(a4, (struct sockaddr_in ***)&a_list, &a_list_count);
		}

		/* Check for ipv6 address */
		if ((d->ipv6 != NULL) && (wantv6 == 1))
		{
			gai_inet_pton(d->ipv6, &a6);
			merge_addr6(a6, (struct sockaddr_in6 ***)&a_list, &a_list_count);
		}
	}

	/* Last chance for a name */
	for (d = list; (cname == NULL) && (d != NULL); d = d->lu_next)
	{
		if (d->name != NULL) cname = strdup(d->name);
	}

	free_lu_dict(list);

	for (i = 0; i < a_list_count; i++)
	{
		if (a_list[i]->sa_family == PF_INET)
		{
			sa4 = (struct sockaddr_in *)a_list[i];
			a = new_addrinfo_v4(0, 0, 0, 0, sa4->sin_addr, NULL);
			append_addrinfo(res, a);
		}
		else if (a_list[i]->sa_family == PF_INET6)
		{
			sa6 = (struct sockaddr_in6 *)a_list[i];
			a = new_addrinfo_v6(0, 0, 0, 0, sa6->sin6_addr, NULL);
			append_addrinfo(res, a);
		}

		free(a_list[i]);
	}

	if (a_list_count > 0) free(a_list);

	/* Set cname in first result */
	if ((setcname == 1) && (*res != NULL))
	{
		if (res[0]->ai_canonname == NULL)
		{
			res[0]->ai_canonname = cname;
			cname = NULL;
		}
	}
	
	if (cname != NULL) free(cname);

	return 0;
}

/*
 * Find a service+node.
 */
static void
gai_nodeserv_lookupd(const char *nodename, const char *servname, const struct addrinfo *hints, struct lu_dict **list)
{
	struct lu_dict q, *sub;
	int proto, family;

	family = PF_UNSPEC;
	proto = IPPROTO_UNSPEC;

	if (hints != NULL)
	{
		if (hints->ai_flags & AI_NUMERICHOST) return;
		family = hints->ai_family;

		proto = hints->ai_protocol;
		if (hints->ai_socktype == SOCK_DGRAM) proto = IPPROTO_UDP;
		if (hints->ai_socktype == SOCK_STREAM) proto = IPPROTO_TCP;
	}

	memset(&q, 0, sizeof(struct lu_dict));
	q.count = 4;
	q.type = "host";

	grok_nodename(nodename, family, &q);
	grok_service(servname, &q);

	if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
	{
		sub = NULL;
		q.protocol = "udp";
		gai_lookupd(&q, &sub);
		append_lu_dict(list, sub);
		for (; sub != NULL; sub = sub->lu_next) sub->protocol = strdup("udp");
	}

	if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
	{
		sub = NULL;
		q.protocol = "tcp";
		gai_lookupd(&q, &sub);
		append_lu_dict(list, sub);
		for (; sub != NULL; sub = sub->lu_next) sub->protocol = strdup("tcp");
	}
}

static void
gai_node_pp(const char *nodename, unsigned short port, int proto, int family, int setcname, struct addrinfo **res)
{
	struct lu_dict *list, *d;
	int i, wantv4, wantv6, a_list_count, socktype;
	char *cname;
	struct sockaddr **a_list;
	struct in_addr a4;
	struct in6_addr a6;
	struct sockaddr_in *sa4;
	struct sockaddr_in6 *sa6;
	struct addrinfo *a;

	socktype = SOCK_UNSPEC;
	if (proto == IPPROTO_UDP) socktype = SOCK_DGRAM;
	if (proto == IPPROTO_TCP) socktype = SOCK_STREAM;

	cname = NULL;

	wantv4 = 1;
	wantv6 = 1;
	if (family == PF_INET6) wantv4 = 0;
	if (family == PF_INET) wantv6 = 0;

	/* Resolve node name */
	list = NULL;
	gai_node_lookupd(nodename, family, 0, &list);
	if (list == NULL)
	{
		gai_node_lookupd(nodename, family, 1, &list);
	}

	/* Resolve aliases */
	for (d = list; d != NULL; d = d->lu_next)
	{
		if (d->cname != NULL)
		{
			if (cname == NULL) cname = strdup(d->cname);
			gai_node_lookupd(d->cname, family, 0, &list);
		}
	}

	a_list_count = 0;

	for (d = list; d != NULL; d = d->lu_next)
	{
		/* Check for cname */
		if ((cname == NULL) && (d->name != NULL) && (strchr(d->name, '.') != NULL))
		{
			cname = strdup(d->name);
		}

		/* Check for ipv4 address */
		if ((d->ipv4 != NULL) && (wantv4 == 1))
		{
			inet_aton(d->ipv4, &a4);
			merge_addr4(a4, (struct sockaddr_in ***)&a_list, &a_list_count);
		}

		/* Check for ipv6 address */
		if ((d->ipv6 != NULL) && (wantv6 == 1))
		{
			gai_inet_pton(d->ipv6, &a6);
			merge_addr6(a6, (struct sockaddr_in6 ***)&a_list, &a_list_count);
		}
	}

	free_lu_dict(list);
	
	for (i = 0; i < a_list_count; i++)
	{
		if (a_list[i]->sa_family == PF_INET)
		{
			sa4 = (struct sockaddr_in *)a_list[i];
			a = new_addrinfo_v4(0, socktype, proto, port, sa4->sin_addr, NULL);
			append_addrinfo(res, a);
		}
		else if (a_list[i]->sa_family == PF_INET6)
		{
			sa6 = (struct sockaddr_in6 *)a_list[i];
			a = new_addrinfo_v6(0, socktype, proto, port, sa6->sin6_addr, NULL);
			append_addrinfo(res, a);
		}

		free(a_list[i]);
	}

	if (a_list_count > 0) free(a_list);

	/* Set cname in first result */
	if ((setcname == 1) && (*res != NULL))
	{
		if (res[0]->ai_canonname == NULL)
		{
			res[0]->ai_canonname = cname;
			cname = NULL;
		}
	}
	
	if (cname != NULL) free(cname);
}

static int
gai_nodeserv(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	struct lu_dict *srv_list, *node_list, *s, *n;
	int numerichost, family, proto, setcname;
	int wantv4, wantv6, i, j, gotmx, p_list_count;
	unsigned short port;
	char *cname;
	struct sockaddr **p_list;
	struct sockaddr_in *sa4;

	numerichost = 0;
	family = PF_UNSPEC;
	proto = IPPROTO_UNSPEC;
	setcname = 0;
	cname = NULL;
	wantv4 = 1;
	wantv6 = 1;

	if (hints != NULL)
	{
		family = hints->ai_family;
		if (hints->ai_flags & AI_NUMERICHOST) numerichost = 1;
		if (hints->ai_flags & AI_CANONNAME) setcname = 1;

		proto = hints->ai_protocol;
		if (hints->ai_socktype == SOCK_DGRAM) proto = IPPROTO_UDP;
		if (hints->ai_socktype == SOCK_STREAM) proto = IPPROTO_TCP;
	}

	if (family == PF_INET6) wantv4 = 0;
	if (family == PF_INET) wantv6 = 0;

	/* First check for this particular host / service (e.g. DNS_SRV) */

	srv_list = NULL;
	gai_nodeserv_lookupd(nodename, servname, hints, &srv_list);
	lu_prioity_sort(&srv_list);

	if (srv_list != NULL)
	{
		for (s = srv_list; s != NULL; s = s->lu_next)
		{
			if (s->port == NULL) continue;
			if (s->protocol == NULL) continue;

			i = htons(atoi(s->port));
			j = IPPROTO_UDP;
			if (!strcmp(s->protocol, "tcp")) j = IPPROTO_TCP;
			gai_node_pp(s->target, i, j, family, setcname, res);
		}

		free_lu_dict(srv_list);
		return 0;
	}

	/*
	 * Special case for smtp: collect mail_exchangers.
	 */
	gotmx = 0;
	node_list = NULL;

	if (!strcmp(servname, "smtp"))
	{
		gai_node_lookupd(nodename, family, numerichost, &node_list);
		if ((node_list == NULL) && (numerichost == 0))
		{
			gai_node_lookupd(nodename, family, 1, &node_list);
		}

		lu_prioity_sort(&node_list);

		for (n = node_list; (n != NULL) && (gotmx == 0); n = n->lu_next)
		{
			if (n->mx != NULL) gotmx = 1;
		}

		if ((gotmx == 0) && (node_list != NULL))
		{
			free_lu_dict(node_list);
			node_list = NULL;
		}
	}

	/*
	 * Look up service, look up node, and combine port/proto with node addresses.
	 */
	srv_list = NULL;
	gai_serv_lookupd(servname, proto, 0, &srv_list);
	if (srv_list == NULL) gai_serv_lookupd(servname, proto, 1, &srv_list);
	if (srv_list == NULL) return 0;

	p_list_count = 0;
	for (s = srv_list; s != NULL; s = s->lu_next)
	{
		if (s->port == NULL) continue;
		if (s->protocol == NULL) continue;

		i = htons(atoi(s->port));
		j = IPPROTO_UDP;
		if (!strcmp(s->protocol, "tcp")) j = IPPROTO_TCP;

		merge_plist(i, j, (struct sockaddr_in ***)&p_list, &p_list_count);
	}

	free_lu_dict(srv_list);
	
	for (i = 0; i < p_list_count; i++)
	{
		sa4 = (struct sockaddr_in *)p_list[i];
		port = sa4->sin_port;
		/* N.B. sin_family is overloaded */
		proto = sa4->sin_family;

		if (gotmx == 1)
		{
			for (n = node_list; n != NULL; n = n->lu_next)
			{
				if (n->mx != NULL) gai_node_pp(n->mx, port, proto, family, setcname, res);
			}
		}
		else
		{
			gai_node_pp(nodename, port, proto, family, setcname, res);
		}

		free(p_list[i]);
	}

	if (node_list != NULL) free_lu_dict(node_list);

	if (p_list_count > 0) free(p_list);
	return 0;
}

int
getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	int status;
	struct lu_dict *list;

	if (res == NULL) return 0;
	*res = NULL;
	list = NULL;

	/* Check input */
	if ((nodename == NULL) && (servname == NULL)) return EAI_NONAME;

	/* Check hints */
	if (hints != NULL)
	{
		if (hints->ai_addrlen != 0) return EAI_BADHINTS;
		if (hints->ai_canonname != NULL) return EAI_BADHINTS;
		if (hints->ai_addr != NULL) return EAI_BADHINTS;
		if (hints->ai_next != NULL) return EAI_BADHINTS;

		/* Check for supported protocol family */
		if (gai_family_type_check(hints->ai_family) != 0) return EAI_FAMILY;

		/* Check for supported socket */
		if (gai_socket_type_check(hints->ai_socktype) != 0) return EAI_BADHINTS;

		/* Check for supported protocol */
		if (gai_protocol_type_check(hints->ai_protocol) != 0) return EAI_BADHINTS;

		/* Check that socket type is compatible with protocol */
		if (gai_socket_protocol_type_check(hints->ai_socktype, hints->ai_protocol) != 0) return EAI_BADHINTS;
	}

	status = 0;

	if (nodename == NULL)
	{
		/* If node is NULL, find service */
		status = gai_serv(servname, hints, res);
		if ((status == 0) && (*res == NULL)) status = EAI_NODATA;
		return status;
	}

	if (servname == NULL)
	{
		/* If service is NULL, find node */
		status = gai_node(nodename, hints, res);
		if ((status == 0) && (*res == NULL)) status = EAI_NODATA;
		return status;
	}

	/* Find node + service */
	status = gai_nodeserv(nodename, servname, hints, res);
	if ((status == 0) && (*res == NULL)) status = EAI_NODATA;
	return status;
}

