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
#include <stdint.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <ifaddrs.h>
#include "lu_utils.h"
#include "netdb_async.h"

#define SOCK_UNSPEC 0
#define IPPROTO_UNSPEC 0

#define MAX_LOOKUP_ATTEMPTS 10

#define INET_NTOP_AF_INET_OFFSET 4
#define INET_NTOP_AF_INET6_OFFSET 8

static int gai_proc = -1;
static int gni_proc = -1;

static const int32_t supported_family[] =
{
	PF_UNSPEC,
	PF_INET,
	PF_INET6
};
static const int32_t supported_family_count = (sizeof(supported_family) / sizeof(supported_family[0]));

static const int32_t supported_socket[] =
{
	SOCK_RAW,
	SOCK_UNSPEC,
	SOCK_DGRAM,
	SOCK_STREAM
};
static const int32_t supported_socket_count = (sizeof(supported_socket) / sizeof(supported_socket[0]));

static const int32_t supported_protocol[] =
{
	IPPROTO_UNSPEC,
	IPPROTO_ICMP,
	IPPROTO_ICMPV6,
	IPPROTO_UDP,
	IPPROTO_TCP
};
static const int32_t supported_protocol_count = (sizeof(supported_protocol) / sizeof(supported_protocol[0]));

static const int32_t supported_socket_protocol_pair[] =
{
	SOCK_RAW,    IPPROTO_UNSPEC,
	SOCK_RAW,    IPPROTO_UDP,
	SOCK_RAW,    IPPROTO_TCP,
	SOCK_RAW,    IPPROTO_ICMP,
	SOCK_RAW,    IPPROTO_ICMPV6,
	SOCK_UNSPEC, IPPROTO_UNSPEC,
	SOCK_UNSPEC, IPPROTO_UDP,
	SOCK_UNSPEC, IPPROTO_TCP,
	SOCK_UNSPEC, IPPROTO_ICMP,
	SOCK_UNSPEC, IPPROTO_ICMPV6,
	SOCK_DGRAM,  IPPROTO_UNSPEC,
	SOCK_DGRAM,  IPPROTO_UDP,
	SOCK_STREAM, IPPROTO_UNSPEC,
	SOCK_STREAM, IPPROTO_TCP
};
static const int32_t supported_socket_protocol_pair_count = (sizeof(supported_socket_protocol_pair) / (sizeof(supported_socket_protocol_pair[0]) * 2));

static int
gai_family_type_check(int32_t f)
{
	int32_t i;

	for (i = 0; i < supported_family_count; i++)
	{
		if (f == supported_family[i]) return 0;
	}

	return 1;
}

static int
gai_socket_type_check(int32_t s)
{
	int32_t i;

	for (i = 0; i < supported_socket_count; i++)
	{
		if (s == supported_socket[i]) return 0;
	}

	return 1;
}

static int
gai_protocol_type_check(int32_t p)
{
	int32_t i;

	for (i = 0; i < supported_protocol_count; i++)
	{
		if (p == supported_protocol[i]) return 0;
	}

	return 1;
}

static int
gai_socket_protocol_type_check(int32_t s, int32_t p)
{
	int32_t i, j, ss, sp;

	for (i = 0, j = 0; i < supported_socket_protocol_pair_count; i++, j+=2)
	{
		ss = supported_socket_protocol_pair[j];
		sp = supported_socket_protocol_pair[j+1];
		if ((s == ss) && (p == sp)) return 0;
	}

	return 1;
}

const char *
gai_strerror(int32_t err)
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
new_addrinfo_v4(int32_t flags, int32_t sock, int32_t proto, uint16_t port, struct in_addr addr, uint32_t iface, const char *cname)
{
	struct addrinfo *a;
	struct sockaddr_in *sa;
	int32_t len;

	a = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
	if (a == NULL) return NULL;

	a->ai_next = NULL;

	a->ai_flags = flags;
	a->ai_family = PF_INET;
	a->ai_socktype = sock;
	a->ai_protocol = proto;

	a->ai_addrlen = sizeof(struct sockaddr_in);

	sa = (struct sockaddr_in *)calloc(1, a->ai_addrlen);
	if (sa == NULL)
	{
		free(a);
		return NULL;
	}

	sa->sin_len = a->ai_addrlen;
	sa->sin_family = PF_INET;
	sa->sin_port = htons(port);
	sa->sin_addr = addr;

	/* Kludge: Jam the interface number into sin_zero. */
	memmove(sa->sin_zero, &iface, sizeof(uint32_t));

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
new_addrinfo_v6(int32_t flags, int32_t sock, int32_t proto, uint16_t port, struct in6_addr addr, uint16_t scopeid, const char *cname)
{
	struct addrinfo *a;
	struct sockaddr_in6 *sa;
	int32_t len;
	uint16_t esid;

	a = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
	if (a == NULL) return NULL;

	a->ai_next = NULL;

	a->ai_flags = flags;
	a->ai_family = PF_INET6;
	a->ai_socktype = sock;
	a->ai_protocol = proto;

	a->ai_addrlen = sizeof(struct sockaddr_in6);

	sa = (struct sockaddr_in6 *)calloc(1, a->ai_addrlen);
	if (sa == NULL)
	{
		free(a);
		return NULL;
	}

	sa->sin6_len = a->ai_addrlen;
	sa->sin6_family = PF_INET6;
	sa->sin6_port = htons(port);
	sa->sin6_addr = addr;

	/* sin6_scope_id is in host byte order */
	sa->sin6_scope_id = scopeid;

	a->ai_addr = (struct sockaddr *)sa;

	if (IN6_IS_ADDR_LINKLOCAL(&sa->sin6_addr))
	{
		/* check for embedded scopeid */
		esid = ntohs(sa->sin6_addr.__u6_addr.__u6_addr16[1]);
		if (esid != 0)
		{
			sa->sin6_addr.__u6_addr.__u6_addr16[1] = 0;
			if (scopeid == 0) sa->sin6_scope_id = esid;
		}
	}

	if (cname != NULL)
	{
		len = strlen(cname) + 1;
		a->ai_canonname = malloc(len);
		memmove(a->ai_canonname, cname, len);
	}

	return a;
}

/*
 * getaddrinfo
 *
 * Input dict may contain the following
 *
 * name: nodename
 * service: servname
 * protocol: [IPPROTO_UNSPEC] | IPPROTO_UDP | IPPROTO_TCP
 * socktype: [SOCK_UNSPEC] | SOCK_DGRAM | SOCK_STREAM
 * family: [PF_UNSPEC] | PF_INET | PF_INET6
 * canonname: [0] | 1
 * passive: [0] | 1
 * numerichost: [0] | 1
 *
 * Output dictionary may contain the following
 * All values are encoded as strings.
 *
 * flags: unsigned long
 * family: unsigned long
 * socktype: unsigned long
 * protocol: unsigned long
 * port: unsigned long
 * address: char *
 * scopeid: unsigned long
 * canonname: char *
 *
 */

static struct addrinfo *
gai_extract(kvarray_t *in)
{
	uint32_t d, k, kcount;
	uint32_t flags, family, socktype, protocol, port32;
	uint16_t port, scopeid;
	const char *addr, *canonname;
	struct addrinfo *a;
	struct in_addr a4;
	struct in6_addr a6;

	flags = 0;
	family = PF_UNSPEC;
	socktype = SOCK_UNSPEC;
	protocol = IPPROTO_UNSPEC;
	port = 0;
	scopeid = 0;
	addr = NULL;
	canonname = NULL;

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "gai_flags"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			flags = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "gai_family")) 
		{
			if (in->dict[d].vcount[k] == 0) continue;
			family = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "gai_socktype"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			socktype = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "gai_protocol"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			protocol = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "gai_port"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			port32 = atoi(in->dict[d].val[k][0]);
			port = port32;
		}
		else if (!strcmp(in->dict[d].key[k], "gai_scopeid"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			scopeid = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "gai_address"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			addr = in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "gai_canonname"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			canonname = in->dict[d].val[k][0];
		}
	}

	if (family == PF_UNSPEC) return NULL;

	a = NULL;
	if (family == PF_INET)
	{
		inet_aton(addr, &a4);
		a = new_addrinfo_v4(flags, socktype, protocol, port, a4, scopeid, canonname);
	}
	else if (family == PF_INET6)
	{
		inet_pton(AF_INET6, addr, &a6);
		a = new_addrinfo_v6(flags, socktype, protocol, port, a6, scopeid, canonname);
	}

	return a;
}

static kvbuf_t *
gai_make_query(const char *nodename, const char *servname, const struct addrinfo *hints)
{
	int32_t flags, family, proto, socktype;
	kvbuf_t *request;
	char str[64], *cname;

	/* new default for SULeoDeo */
	flags = AI_PARALLEL;
	family = PF_UNSPEC;
	proto = IPPROTO_UNSPEC;
	socktype = SOCK_UNSPEC;
	cname = NULL;

	if (hints != NULL)
	{
		family = hints->ai_family;
		if (hints->ai_flags & AI_NUMERICHOST) flags |= AI_NUMERICHOST;
		if (hints->ai_flags & AI_CANONNAME) flags |= AI_CANONNAME;
		if (hints->ai_flags & AI_PASSIVE) flags |= AI_PASSIVE;

		proto = hints->ai_protocol;

		if (hints->ai_socktype == SOCK_DGRAM)
		{
			socktype = SOCK_DGRAM;
			proto = IPPROTO_UDP;
		}

		if (hints->ai_socktype == SOCK_STREAM)
		{
			socktype = SOCK_STREAM;
			proto = IPPROTO_TCP;
		}
	}

	request = kvbuf_new();
	if (request == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	kvbuf_add_dict(request);

	if (nodename != NULL)
	{
		kvbuf_add_key(request, "name");
		kvbuf_add_val(request, nodename);
	}

	if (servname != NULL)
	{
		kvbuf_add_key(request, "service");
		kvbuf_add_val(request, servname);
	}

	if (proto != IPPROTO_UNSPEC)
	{
		snprintf(str, sizeof(str), "%u", proto);
		kvbuf_add_key(request, "protocol");
		kvbuf_add_val(request, str);
	}

	if (socktype != SOCK_UNSPEC)
	{
		snprintf(str, sizeof(str), "%u", socktype);
		kvbuf_add_key(request, "socktype");
		kvbuf_add_val(request, str);
	}

	if (family != PF_UNSPEC)
	{
		snprintf(str, sizeof(str), "%u", family);
		kvbuf_add_key(request, "family");
		kvbuf_add_val(request, str);
	}

	snprintf(str, sizeof(str), "%u", flags);
	kvbuf_add_key(request, "ai_flags");
	kvbuf_add_val(request, str);

	return request;
}

static int32_t
is_a_number(const char *s)
{
	int32_t i, len;

	if (s == NULL) return 0;

	len = strlen(s);
	for (i = 0; i < len; i++)
	{
		if (isdigit(s[i]) == 0) return 0;
	}

	return 1;
}

static int
gai_trivial(struct in_addr *in4, struct in6_addr *in6, int16_t port, const struct addrinfo *hints, struct addrinfo **res)
{
	int32_t family, wantv4, wantv6, proto;
	char *loopv4, *loopv6;
	struct in_addr a4;
	struct in6_addr a6;
	struct addrinfo *a;

	family = PF_UNSPEC;
	if (hints != NULL) family = hints->ai_family;

	wantv4 = 1;
	wantv6 = 1;

	if (family == PF_INET6) wantv4 = 0;
	if (family == PF_INET) wantv6 = 0;

	memset(&a4, 0, sizeof(struct in_addr));
	memset(&a6, 0, sizeof(struct in6_addr));

	if ((in4 == NULL) && (in6 == NULL))
	{
		loopv4 = "127.0.0.1";
		loopv6 = "0:0:0:0:0:0:0:1";

		if ((hints != NULL) && ((hints->ai_flags & AI_PASSIVE) == 1))
		{
			loopv4 = "0.0.0.0";
			loopv6 = "0:0:0:0:0:0:0:0";
		}

		if ((family == PF_UNSPEC) || (family == PF_INET))
		{
			inet_pton(AF_INET, loopv4, &a4);
		}

		if ((family == PF_UNSPEC) || (family == PF_INET6))
		{
			inet_pton(AF_INET6, loopv6, &a6);
		}
	}
	else if (in4 == NULL)
	{
		if (family == PF_INET) return EAI_BADHINTS;

		wantv4 = 0;
		memcpy(&a6, in6, sizeof(struct in6_addr));
	}
	else if (in6 == NULL)
	{
		if (family == PF_INET6) return EAI_BADHINTS;

		wantv6 = 0;
		memcpy(&a4, in4, sizeof(struct in_addr));
	}
	else
	{
		return EAI_NONAME;
	}

	proto = IPPROTO_UNSPEC;

	if (hints != NULL)
	{
		proto = hints->ai_protocol;
		if (proto == IPPROTO_UNSPEC)
		{
			if (hints->ai_socktype == SOCK_DGRAM) proto = IPPROTO_UDP;
			else if (hints->ai_socktype == SOCK_STREAM) proto = IPPROTO_TCP;
		}
	}

	if (wantv4 == 1)
	{
		if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
		{
			a = new_addrinfo_v4(0, SOCK_DGRAM, IPPROTO_UDP, port, a4, 0, NULL);
			append_addrinfo(res, a);
		}

		if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
		{
			a = new_addrinfo_v4(0, SOCK_STREAM, IPPROTO_TCP, port, a4, 0, NULL);
			append_addrinfo(res, a);
		}

		if (proto == IPPROTO_ICMP)
		{
			a = new_addrinfo_v4(0, SOCK_RAW, IPPROTO_ICMP, port, a4, 0, NULL);
			append_addrinfo(res, a);
		}
	}

	if (wantv6 == 1)
	{
		if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
		{
			a = new_addrinfo_v6(0, SOCK_DGRAM, IPPROTO_UDP, port, a6, 0, NULL);
			append_addrinfo(res, a);
		}

		if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
		{
			a = new_addrinfo_v6(0, SOCK_STREAM, IPPROTO_TCP, port, a6, 0, NULL);
			append_addrinfo(res, a);
		}

		if (proto == IPPROTO_ICMPV6)
		{
			a = new_addrinfo_v6(0, SOCK_RAW, IPPROTO_ICMPV6, port, a6, 0, NULL);
			append_addrinfo(res, a);
		}
	}

	return 0;
}

static int
gai_files(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	int32_t i, status, numericserv, numerichost, family, proto, wantv4, wantv6, count;
	uint16_t scopeid;
	int16_t port;
	struct servent *s;
	struct hostent *h;
	char *protoname, *loopv4, *loopv6;
	struct in_addr a4;
	struct in6_addr a6;
	struct addrinfo *a;

	count = 0;
	scopeid = 0;

	numericserv = 0;
	if (servname != NULL) numericserv = is_a_number(servname);

	family = PF_UNSPEC;
	if (hints != NULL) family = hints->ai_family;

	numerichost = 0;

	if (nodename == NULL)
	{
		numerichost = 1;

		loopv4 = "127.0.0.1";
		loopv6 = "0:0:0:0:0:0:0:1";

		if ((hints != NULL) && ((hints->ai_flags & AI_PASSIVE) == 1))
		{
			loopv4 = "0.0.0.0";
			loopv6 = "0:0:0:0:0:0:0:0";
		}

		if ((family == PF_UNSPEC) || (family == PF_INET))
		{
			inet_pton(AF_INET, loopv4, &a4);
		}

		if ((family == PF_UNSPEC) || (family == PF_INET6))
		{
			inet_pton(AF_INET6, loopv6, &a6);
		}
	}
	else
	{
		if ((family == PF_UNSPEC) || (family == PF_INET))
		{
			status = inet_pton(AF_INET, nodename, &a4);
			if (status == 1)
			{
				numerichost = 1;
				if (family == PF_UNSPEC) family = PF_INET;
			}
		}

		if ((family == PF_UNSPEC) || (family == PF_INET6))
		{
			status = inet_pton(AF_INET6, nodename, &a6);
			if (status == 1)
			{
				numerichost = 1;
				if (family == PF_UNSPEC) family = PF_INET6;
				if ((IN6_IS_ADDR_LINKLOCAL(&a6)) && (a6.__u6_addr.__u6_addr16[1] != 0))
				{
					scopeid = ntohs(a6.__u6_addr.__u6_addr16[1]);
					a6.__u6_addr.__u6_addr16[1] = 0;
				}
			}
		}
	}

	wantv4 = 1;
	wantv6 = 1;
	if (family == PF_INET6) wantv4 = 0;
	if (family == PF_INET) wantv6 = 0;

	proto = IPPROTO_UNSPEC;
	protoname = NULL;

	if (hints != NULL)
	{
		proto = hints->ai_protocol;
		if (proto == IPPROTO_UNSPEC)
		{
			if (hints->ai_socktype == SOCK_DGRAM) proto = IPPROTO_UDP;
			else if (hints->ai_socktype == SOCK_STREAM) proto = IPPROTO_TCP;
		}
	}

	if (proto == IPPROTO_UDP) protoname = "udp";
	else if (proto == IPPROTO_TCP) protoname = "tcp";

	s = NULL;
	port = 0;

	if (numericserv != 0)
	{
		port = htons(atoi(servname));
	}
	else if (servname != NULL)
	{
		s = getservbyname(servname, protoname);
		if (s != NULL) port = s->s_port;
	}

	/* new_addrinfo_v4 and new_addrinfo_v6 expect port in host byte order */
	port = ntohs(port);

	if (numerichost != 0)
	{
		if (wantv4 == 1)
		{
			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
			{
				a = new_addrinfo_v4(0, SOCK_DGRAM, IPPROTO_UDP, port, a4, 0, NULL);
				append_addrinfo(res, a);
				count++;
			}

			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
			{
				a = new_addrinfo_v4(0, SOCK_STREAM, IPPROTO_TCP, port, a4, 0, NULL);
				append_addrinfo(res, a);
				count++;
			}

			if (proto == IPPROTO_ICMP)
			{
				a = new_addrinfo_v4(0, SOCK_RAW, IPPROTO_ICMP, port, a4, 0, NULL);
				append_addrinfo(res, a);
				count++;
			}
		}

		if (wantv6 == 1)
		{
			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
			{
				a = new_addrinfo_v6(0, SOCK_DGRAM, IPPROTO_UDP, port, a6, scopeid, NULL);
				append_addrinfo(res, a);
				count++;
			}

			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
			{
				a = new_addrinfo_v6(0, SOCK_STREAM, IPPROTO_TCP, port, a6, scopeid, NULL);
				append_addrinfo(res, a);
				count++;
			}

			if (proto == IPPROTO_ICMPV6)
			{
				a = new_addrinfo_v6(0, SOCK_RAW, IPPROTO_ICMPV6, port, a6, scopeid, NULL);
				append_addrinfo(res, a);
				count++;
			}
		}

		if (count == 0) return EAI_AGAIN;
		return 0;
	}

	if (wantv4 == 1)
	{
		h = gethostbyname(nodename);
		if (h == NULL) return EAI_AGAIN;

		for (i = 0; h->h_addr_list[i] != 0; i++)
		{
			memmove((void *)&a4.s_addr, h->h_addr_list[i], h->h_length);

			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
			{
				a = new_addrinfo_v4(0, SOCK_DGRAM, IPPROTO_UDP, port, a4, 0, NULL);
				append_addrinfo(res, a);
				count++;
			}

			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
			{
				a = new_addrinfo_v4(0, SOCK_STREAM, IPPROTO_TCP, port, a4, 0, NULL);
				append_addrinfo(res, a);
				count++;
			}
		}
	}

	if (wantv6 == 1)
	{
		h = gethostbyname2(nodename, AF_INET6);
		if (h == NULL) return EAI_AGAIN;

		for (i = 0; h->h_addr_list[i] != 0; i++)
		{
			memmove(&(a6.__u6_addr.__u6_addr32[0]), h->h_addr_list[i], h->h_length);

			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_UDP))
			{
				a = new_addrinfo_v6(0, SOCK_DGRAM, IPPROTO_UDP, port, a6, 0, NULL);
				append_addrinfo(res, a);
				count++;
			}

			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
			{
				a = new_addrinfo_v6(0, SOCK_STREAM, IPPROTO_TCP, port, a6, 0, NULL);
				append_addrinfo(res, a);
				count++;
			}
		}
	}

	if (count == 0) return EAI_AGAIN;
	return 0;
}

static int
ds_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	uint32_t i;
	kvbuf_t *request;
	kvarray_t *reply;
	char *cname;
	kern_return_t status;
	struct addrinfo *a;

	if (_ds_running() == 0)
	{
		return gai_files(nodename, servname, hints, res);
	}

	if (gai_proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getaddrinfo", &gai_proc);
		if (status != KERN_SUCCESS)
		{
			errno = ECONNREFUSED;
			return EAI_SYSTEM;
		}
	}

	request = gai_make_query(nodename, servname, hints);
	if (request == NULL) return EAI_SYSTEM;

	reply = NULL;
	status = LI_DSLookupQuery(gai_proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS)
	{
		errno = ECONNREFUSED;
		return EAI_SYSTEM;
	}

	if (reply == NULL) return EAI_NONAME;

	cname = NULL;
	for (i = 0; i < reply->count; i++)
	{
		a = gai_extract(reply);
		if (a == NULL) continue;
		if ((cname == NULL) && (a->ai_canonname != NULL)) cname = a->ai_canonname;
		append_addrinfo(res, a);
	}

	kvarray_free(reply);

	if ((cname != NULL) && (res != NULL) && (res[0] != NULL) && (res[0]->ai_canonname == NULL))
	{
		res[0]->ai_canonname = strdup(cname);
	}

	return 0;
}

static int
gai_checkhints(const struct addrinfo *hints)
{
	if (hints == NULL) return 0;
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

	return 0;
}

int
getaddrinfo(const char * __restrict nodename, const char * __restrict servname, const struct addrinfo * __restrict hints, struct addrinfo ** __restrict res)
{
	int32_t status, nodenull, servnull;
	int32_t numericserv, numerichost, family;
	int16_t port;
	struct in_addr a4, *p4;
	struct in6_addr a6, *p6;

	if (res == NULL) return 0;
	*res = NULL;

	/* Check input */
	nodenull = 0;
	if ((nodename == NULL) || (nodename[0] == '\0')) nodenull = 1;

	servnull = 0;
	if ((servname == NULL) || (servname[0] == '\0')) servnull = 1;

	if ((nodenull == 1) && (servnull == 1)) return EAI_NONAME;

	status = gai_checkhints(hints);
	if (status != 0) return status;

	/*
	 * Trap the "trivial" cases that can be answered without a query.
	 * (nodename == numeric) && (servname == NULL)
	 * (nodename == numeric) && (servname == numeric)
	 * (nodename == NULL) && (servname == numeric)
	 */
	p4 = NULL;
	p6 = NULL;

	memset(&a4, 0, sizeof(struct in_addr));
	memset(&a6, 0, sizeof(struct in6_addr));

	numericserv = 0;
	port = 0;
	if (servnull == 0) numericserv = is_a_number(servname);
	if (numericserv == 1) port = atoi(servname);

	family = PF_UNSPEC;
	if (hints != NULL) family = hints->ai_family;

	numerichost = 0;
	if (nodenull == 0)
	{
		if ((family == PF_UNSPEC) || (family == PF_INET))
		{
			status = inet_pton(AF_INET, nodename, &a4);
			if (status == 1)
			{
				p4 = &a4;
				numerichost = 1;
			}
		}

		if ((family == PF_UNSPEC) || (family == PF_INET6))
		{
			status = inet_pton(AF_INET6, nodename, &a6);
			if (status == 1)
			{
				p6 = &a6;
				numerichost = 1;
			}
		}
	}

	if ((nodenull == 1) && (numericserv == 1)) return gai_trivial(NULL, NULL, port, hints, res);
	if ((numerichost == 1) && (numericserv == 1)) return gai_trivial(p4, p6, port, hints, res);
	if ((numerichost == 1) && (servnull == 1)) return gai_trivial(p4, p6, 0, hints, res);

	if (nodenull == 1) status = ds_getaddrinfo(NULL, servname, hints, res);
	else if (servnull == 1) status = ds_getaddrinfo(nodename, NULL, hints, res);
	else status = ds_getaddrinfo(nodename, servname, hints, res);

	if ((status == 0) && (*res == NULL)) status = EAI_NONAME;

	return status;
}

int32_t
getaddrinfo_async_start(mach_port_t *p, const char *nodename, const char *servname, const struct addrinfo *hints, getaddrinfo_async_callback callback, void *context)
{
	int32_t status;
	kvbuf_t *request;

	*p = MACH_PORT_NULL;

	if ((nodename == NULL) && (servname == NULL)) return EAI_NONAME;

	status = gai_checkhints(hints);
	if (status != 0) return EAI_BADHINTS;

	if (gai_proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getaddrinfo", &gai_proc);
		if (status != KERN_SUCCESS)
		{
			errno = ECONNREFUSED;
			return EAI_SYSTEM;
		}
	}

	request = gai_make_query(nodename, servname, hints);
	if (request == NULL) return EAI_SYSTEM;

	status = LI_async_start(p, gai_proc, request, (void *)callback, context);

	kvbuf_free(request);

	if (status != KERN_SUCCESS)
	{
		errno = ECONNREFUSED;
		return EAI_SYSTEM;
	}

	return 0;
}

void
getaddrinfo_async_cancel(mach_port_t p)
{
	LI_async_call_cancel(p, NULL);
}

int32_t
getaddrinfo_async_send(mach_port_t *p, const char *nodename, const char *servname, const struct addrinfo *hints)
{
	return getaddrinfo_async_start(p, nodename, servname, hints, NULL, NULL);
}

int32_t
getaddrinfo_async_receive(mach_port_t p, struct addrinfo **res)
{
	kern_return_t status;
	char *cname;
	kvarray_t *reply;
	uint32_t i;
	struct addrinfo *a;

	if (res == NULL) return 0;
	*res = NULL;

	reply = NULL;

	status = LI_async_receive(p, &reply);
	if (status < 0) return EAI_FAIL;
	if (reply == NULL) return EAI_NONAME;

	cname = NULL;
	for (i = 0; i < reply->count; i++)
	{
		a = gai_extract(reply);
		if (a == NULL) continue;
		if ((cname == NULL) && (a->ai_canonname != NULL)) cname = a->ai_canonname;
		append_addrinfo(res, a);
	}

	kvarray_free(reply);

	if ((cname != NULL) && (res != NULL) && (res[0] != NULL) && (res[0]->ai_canonname == NULL))
	{
		res[0]->ai_canonname = strdup(cname);
	}


	if (*res == NULL) return EAI_NONAME;

	return 0;
}

int32_t
getaddrinfo_async_handle_reply(void *msg)
{
	getaddrinfo_async_callback callback;
	void *context;
	char *buf, *cname;
	uint32_t i, len;
	int status;
	kvarray_t *reply;
	struct addrinfo *l, *a, **res;

	callback = (getaddrinfo_async_callback)NULL;
	context = NULL;
	buf = NULL;
	len = 0;
	reply = NULL;
	l = NULL;
	res = &l;

	status = LI_async_handle_reply(msg, &reply, (void **)&callback, &context);
	if (status != KERN_SUCCESS)
	{
		if (status == MIG_REPLY_MISMATCH) return 0;
		if (callback != NULL) callback(EAI_FAIL, NULL, context);
		return EAI_FAIL;
	}

	if (reply == NULL)
	{
		if (callback != NULL) callback(EAI_NONAME, NULL, context);
		return EAI_NONAME;
	}

	cname = NULL;
	for (i = 0; i < reply->count; i++)
	{
		a = gai_extract(reply);
		if (a == NULL) continue;
		if ((cname == NULL) && (a->ai_canonname != NULL)) cname = a->ai_canonname;
		append_addrinfo(res, a);
	}

	kvarray_free(reply);

	if ((cname != NULL) && (res[0] != NULL) && (res[0]->ai_canonname == NULL))
	{
		res[0]->ai_canonname = strdup(cname);
	}

	if (*res == NULL)
	{
		callback(EAI_NONAME, NULL, context);
		return EAI_NONAME;
	}

	callback(0, *res, context);
	return 0;
}

/*
 * getnameinfo
 */
 
/*
 * getnameinfo support in Directory Service
 * Input dict may contain the following
 *
 * ip_address: node address
 * ipv6_address: node address
 * port: service number
 * protocol: [tcp] | udp
 * fqdn: [1] | 0
 * numerichost: [0] | 1
 * name_required: [0] | 1
 * numericserv: [0] | 1
 *
 * Output dictionary may contain the following
 * All values are encoded as strings.
 *
 * name: char *
 * service: char *
 */

static int
gni_extract(kvarray_t *in, const char **host, const char **serv)
{
	uint32_t d, k, kcount;

	if (in == NULL) return -1;

	if ((host == NULL) || (serv == NULL))
	{
		errno = EINVAL;
		return EAI_SYSTEM;
	}

	*host = NULL;
	*serv = NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return EAI_NONAME;

	kcount = in->dict[d].kcount;
	if (kcount == 0) return EAI_NONAME;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "gni_name"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			if (*host != NULL) continue;

			*host = in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "gni_service")) 
		{
			if (in->dict[d].vcount[k] == 0) continue;
			if (*serv != NULL) continue;

			*serv = in->dict[d].val[k][0];
		}
	}

	if ((*host == NULL) && (*serv == NULL)) return EAI_NONAME;
	return 0;
}

static kvbuf_t *
gni_make_query(const struct sockaddr *sa, size_t salen, int wanthost, int wantserv, int flags)
{
	kvbuf_t *request;
	uint16_t port, ifnum;
	char str[NI_MAXHOST], ifname[IF_NAMESIZE], tmp[64];
	uint32_t a4, offset, isll;
	struct sockaddr_in6 *s6;

	if (sa == NULL)
	{
		errno = EINVAL;
		return NULL;
	}

	if (sa->sa_len != salen)
	{
		errno = EINVAL;
		return NULL;
	}

	isll = 0;

	offset = INET_NTOP_AF_INET_OFFSET;
	port = 0;

	if (sa->sa_family == PF_INET)
	{
		a4 = ntohl(((const struct sockaddr_in *)sa)->sin_addr.s_addr);
		if (IN_MULTICAST(a4) || IN_EXPERIMENTAL(a4)) flags |= NI_NUMERICHOST;
		a4 >>= IN_CLASSA_NSHIFT;
		if (a4 == 0) flags |= NI_NUMERICHOST;

		port = ntohs(((struct sockaddr_in *)sa)->sin_port);
	}
	else if (sa->sa_family == PF_INET6)
	{
		s6 = (struct sockaddr_in6 *)sa;
		switch (s6->sin6_addr.s6_addr[0])
		{
			case 0x00:
			{
				if (IN6_IS_ADDR_V4MAPPED(&s6->sin6_addr))
				{
				}
				else if (IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr))
				{
				}
				else
				{
					flags |= NI_NUMERICHOST;
				}
				break;
			}
			default:
			{
				if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr))
				{
					isll = 1;
				}
				else if (IN6_IS_ADDR_MULTICAST(&s6->sin6_addr))
				{
					flags |= NI_NUMERICHOST;
				}
				break;
			}
		}

		if (isll != 0)
		{
			ifnum = ntohs(s6->sin6_addr.__u6_addr.__u6_addr16[1]);
			if (ifnum == 0) ifnum = s6->sin6_scope_id;
			else if ((s6->sin6_scope_id != 0) && (ifnum != s6->sin6_scope_id))
			{
				errno = EINVAL;
				return NULL;
			}

			s6->sin6_addr.__u6_addr.__u6_addr16[1] = 0;
			s6->sin6_scope_id = ifnum;
			if ((ifnum != 0) && (flags & NI_NUMERICHOST)) flags |= NI_WITHSCOPEID;
		}

		offset = INET_NTOP_AF_INET6_OFFSET;
		port = ntohs(s6->sin6_port);
	}
	else
	{
		errno = EPFNOSUPPORT;
		return NULL;
	}

	request = kvbuf_new();
	if (request == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	kvbuf_add_dict(request);

	if (wanthost != 0)
	{
		inet_ntop(sa->sa_family, (char *)(sa) + offset, str, NI_MAXHOST);

		if (isll != 0)
		{
			ifnum = ((struct sockaddr_in6 *)sa)->sin6_scope_id;
			if ((ifnum != 0) && (if_indextoname(ifnum, ifname) != NULL))
			{
				strcat(str, "%");
				strcat(str, ifname);
			}
		}

		kvbuf_add_key(request, "address");
		kvbuf_add_val(request, str);

		kvbuf_add_key(request, "family");
		snprintf(tmp, sizeof(tmp), "%u", sa->sa_family);
		kvbuf_add_val(request, tmp);
	}

	if (wantserv != 0)
	{
		snprintf(tmp, sizeof(tmp), "%hu", port);
		kvbuf_add_key(request, "port");
		kvbuf_add_val(request, tmp);
	}

	snprintf(tmp, sizeof(tmp), "%u", flags);
	kvbuf_add_key(request, "flags");
	kvbuf_add_val(request, tmp);

	return request;
}

int
getnameinfo(const struct sockaddr * __restrict sa, socklen_t salen, char * __restrict host, socklen_t hostlen, char * __restrict serv, socklen_t servlen, int flags)
{
	uint32_t n, i, ifnum;
	int wanth, wants, isll;
	kvbuf_t *request;
	kvarray_t *reply;
	char ifname[IF_NAMESIZE];
	const char *hval, *sval;
	kern_return_t status;
	struct sockaddr_in *s4, s4buf;
	struct sockaddr_in6 *s6, s6buf;
	struct in_addr *a4;
	struct in6_addr *a6;

	/* Check input */
	if (sa == NULL) return EAI_FAIL;

	isll = 0;
	ifnum = 0;

	s4 = &s4buf;
	a4 = NULL;

	s6 = &s6buf;
	a6 = NULL;

	if (sa->sa_family == AF_INET)
	{
		memcpy(s4, sa, sizeof(struct sockaddr_in));
		a4 = &(s4->sin_addr);
	}
	else if (sa->sa_family == AF_INET6)
	{
		memcpy(s6, sa, sizeof(struct sockaddr_in6));
		a6 = &(s6->sin6_addr);

		if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr)) isll = 1;

		/*
		 * Link-local IPv6 addresses may have a scope id 
		 * in s6->sin6_addr.__u6_addr.__u6_addr16[1] as well as in s6->sin6_scope_id.
		 * If they are both non-zero, they must be equal.
		 * We zero s6->sin6_addr.__u6_addr.__u6_addr16[1] and set s6->sin6_scope_id.
		 */
		if (isll != 0)
		{
			ifnum = ntohs(s6->sin6_addr.__u6_addr.__u6_addr16[1]);
			if (ifnum == 0) ifnum = s6->sin6_scope_id;
			else if ((s6->sin6_scope_id != 0) && (ifnum != s6->sin6_scope_id)) return EAI_FAIL;

			s6->sin6_addr.__u6_addr.__u6_addr16[1] = 0;
			s6->sin6_scope_id = ifnum;
		}

		/* V4 mapped and compat addresses are converted to plain V4 */
		if ((IN6_IS_ADDR_V4MAPPED(&s6->sin6_addr)) || (IN6_IS_ADDR_V4COMPAT(&s6->sin6_addr)))
		{
			memset(s4, 0, sizeof(struct sockaddr_in));

			s4->sin_len = sizeof(struct sockaddr_in);
			s4->sin_family = AF_INET;
			s4->sin_port = s6->sin6_port;
			memcpy(&(s4->sin_addr.s_addr), &(s6->sin6_addr.s6_addr[12]), 4);

			return getnameinfo((const struct sockaddr *)s4, s4->sin_len, host, hostlen, serv, servlen, flags);
		}
	}
	else return EAI_FAMILY;

	wanth = 0;
	if ((host != NULL) && (hostlen != 0)) wanth = 1;

	wants = 0;
	if ((serv != NULL) && (servlen != 0)) wants = 1;

	if ((wanth == 0) && (wants == 0)) return 0;

	/*
	 * Special cases handled by the library
	 */
	if ((wanth == 1) && (flags & NI_NUMERICHOST))
	{
		if (sa->sa_family == AF_INET)
		{
			if (inet_ntop(AF_INET, a4, host, hostlen) == NULL) return EAI_FAIL;
		}
		else
		{
			if (inet_ntop(AF_INET6, a6, host, hostlen) == NULL) return EAI_FAIL;
		}

		if ((isll != 0) && (ifnum != 0))
		{
			/* append interface name */
			if (if_indextoname(ifnum, ifname) != NULL)
			{
				strcat(host, "%");
				strcat(host, ifname);
			}
		}

		if (wants == 0) return 0;
	}

	if ((wants == 1) && (flags & NI_NUMERICSERV))
	{
		if (sa->sa_family == PF_INET)
		{
			n = snprintf(serv, servlen, "%hu", ntohs(s4->sin_port));
			if (n >= servlen) return EAI_FAIL;
		}
		else
		{
			n = snprintf(serv, servlen, "%hu", ntohs(s6->sin6_port));
			if (n >= servlen) return EAI_FAIL;
		}

		if (wanth == 0) return 0;
	}

	if ((wanth == 1) && (flags & NI_NUMERICHOST) && (wants == 1) && (flags & NI_NUMERICSERV)) return 0;

	if (_ds_running() == 0)
	{
		errno = ECONNREFUSED;
		return EAI_SYSTEM;
	}

	if (gni_proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getnameinfo", &gni_proc);
		if (status != KERN_SUCCESS)
		{
			errno = ECONNREFUSED;
			return EAI_SYSTEM;
		}
	}

	request = gni_make_query(sa, salen, wanth, wants, flags);
	if (request == NULL) return EAI_SYSTEM;

	reply = NULL;
	status = LI_DSLookupQuery(gni_proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS)
	{
		errno = ECONNREFUSED;
		return EAI_SYSTEM;
	}

	if (reply == NULL) return EAI_NONAME;

	hval = NULL;
	sval = NULL;

	status = gni_extract(reply, &hval, &sval);

	if (status != 0)
	{
		kvarray_free(reply);
		return status;
	}

	i = 0;
	if (hval != NULL) i = strlen(hval) + 1;
	if ((host != NULL) && (hostlen != 0) && (i != 0))
	{
		if (i > hostlen)
		{
			kvarray_free(reply);
			return EAI_FAIL;
		}

		memcpy(host, hval, i);
	}

	i = 0;
	if (sval != NULL) i = strlen(sval) + 1;
	if ((serv != NULL) && (servlen != 0) && (i != 0))
	{
		if (i > servlen)
		{
			kvarray_free(reply);
			return EAI_FAIL;
		}

		memcpy(serv, sval, i);
	}

	kvarray_free(reply);
	return 0;
}

int32_t
getnameinfo_async_start(mach_port_t *p, const struct sockaddr *sa, size_t salen, int flags, getnameinfo_async_callback callback, void *context)
{
	int32_t status;
	kvbuf_t *request;

	*p = MACH_PORT_NULL;

	/* Check input */
	if (sa == NULL) return EAI_FAIL;

	if (gni_proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getnameinfo", &gni_proc);
		if (status != KERN_SUCCESS)
		{
			errno = ECONNREFUSED;
			return EAI_SYSTEM;
		}
	}

	request = gni_make_query(sa, salen, 1, 1, flags);
	if (request == NULL) return EAI_SYSTEM;

	status = LI_async_start(p, gni_proc, request, (void *)callback, context);

	kvbuf_free(request);

	if (status != KERN_SUCCESS)
	{
		errno = ECONNREFUSED;
		return EAI_SYSTEM;
	}

	return 0;
}

void
getnameinfo_async_cancel(mach_port_t p)
{
	LI_async_call_cancel(p, NULL);
}

int32_t
getnameinfo_async_send(mach_port_t *p, const struct sockaddr *sa, size_t salen, int flags)
{
	return getnameinfo_async_start(p, sa, salen, flags, NULL, NULL);
}

int32_t
getnameinfo_async_receive(mach_port_t p, char **host, char **serv)
{
	kern_return_t status;
	const char *hval, *sval;
	kvarray_t *reply;

	reply = NULL;

	status = LI_async_receive(p, &reply);
	if (status < 0) return EAI_FAIL;
	if (reply == NULL) return EAI_NONAME;

	hval = NULL;
	sval = NULL;

	status = gni_extract(reply, &hval, &sval);
	if (status != 0)
	{
		kvarray_free(reply);
		return status;
	}

	if (hval != NULL) *host = strdup(hval);
	if (sval != NULL) *serv = strdup(sval);

	kvarray_free(reply);
	return 0;
}

int32_t
getnameinfo_async_handle_reply(void *msg)
{
	getnameinfo_async_callback callback;
	void *context;
	const char *hval, *sval;
	char *host, *serv;
	uint32_t len;
	int status;
	kvarray_t *reply;

	callback = (getnameinfo_async_callback)NULL;
	context = NULL;
	reply = NULL;
	len = 0;

	status = LI_async_handle_reply(msg, &reply, (void **)&callback, &context);
	if ((status != KERN_SUCCESS) || (reply == NULL))
	{
		if (status == MIG_REPLY_MISMATCH) return 0;
		if (callback != NULL) callback(EAI_NONAME, NULL, NULL, context);
		return EAI_NONAME;
	}

	hval = NULL;
	sval = NULL;

	status = gni_extract(reply, &hval, &sval);
	if (status != 0)
	{
		if (callback != NULL) callback(status, NULL, NULL, context);
		kvarray_free(reply);
		return status;
	}

	host = NULL;
	serv = NULL;

	if (hval != NULL) host = strdup(hval);
	if (sval != NULL) serv = strdup(sval);
	kvarray_free(reply);

	callback(0, host, serv, context);
	return 0;
}
