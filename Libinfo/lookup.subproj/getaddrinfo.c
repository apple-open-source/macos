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

#define LONG_STRING_LENGTH 8192
#define _LU_MAXLUSTRLEN 256
#define LU_QBUF_SIZE 8192

#define MAX_LOOKUP_ATTEMPTS 10

#define INET_NTOP_AF_INET_OFFSET 4
#define INET_NTOP_AF_INET6_OFFSET 8

extern mach_port_t _lookupd_port();

static int gai_proc = -1;
static int gni_proc = -1;

static int32_t supported_family[] =
{
	PF_UNSPEC,
	PF_INET,
	PF_INET6
};
static int32_t supported_family_count = 3;

static int32_t supported_socket[] =
{
	SOCK_RAW,
	SOCK_UNSPEC,
	SOCK_DGRAM,
	SOCK_STREAM
};
static int32_t supported_socket_count = 4;

static int32_t supported_protocol[] =
{
	IPPROTO_UNSPEC,
	IPPROTO_ICMPV6,
	IPPROTO_UDP,
	IPPROTO_TCP
};
static int32_t supported_protocol_count = 4;

static int32_t supported_socket_protocol_pair[] =
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
static int32_t supported_socket_protocol_pair_count = 12;

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

static int
encode_kv(XDR *x, const char *k, const char *v)
{
	int32_t n = 1;

	if (!xdr_string(x, (char **)&k, _LU_MAXLUSTRLEN)) return 1;
	if (!xdr_int(x, &n)) return 1;
	if (!xdr_string(x, (char **)&v, _LU_MAXLUSTRLEN)) return 1;

	return 0;
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
new_addrinfo_v4(int32_t flags, int32_t sock, int32_t proto, uint16_t port, struct in_addr addr, uint32_t iface, char *cname)
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
new_addrinfo_v6(int32_t flags, int32_t sock, int32_t proto, uint16_t port, struct in6_addr addr, uint32_t scopeid, char *cname)
{
	struct addrinfo *a;
	struct sockaddr_in6 *sa;
	int32_t len;

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
	sa->sin6_scope_id = scopeid;
	a->ai_addr = (struct sockaddr *)sa;

	if (cname != NULL)
	{
		len = strlen(cname) + 1;
		a->ai_canonname = malloc(len);
		memmove(a->ai_canonname, cname, len);
	}

	return a;
}

/*
 * getaddrinfo support in lookupd
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
gai_lookupd_process_dictionary(XDR *inxdr)
{
	int32_t i, nkeys, nvals;
	char *key, *val;
	uint32_t flags, family, socktype, protocol, longport, scopeid;
	uint16_t port;
	char *addr, *canonname;
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

	if (!xdr_int(inxdr, &nkeys)) return NULL;

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		val = NULL;

		if (!xdr_string(inxdr, &key, LONG_STRING_LENGTH)) return NULL;
		if (!xdr_int(inxdr, &nvals)) 
		{
			free(key);
			return NULL;
		}

		if (nvals != 1)
		{
			free(key);
			return NULL;
		}

		if (!xdr_string(inxdr, &val, LONG_STRING_LENGTH))
		{
			free(key);
			return NULL;
		}

		if (!strcmp(key, "flags"))
		{
			flags = atoi(val);
		}
		else if (!strcmp(key, "family")) 
		{
			family = atoi(val);
		}
		else if (!strcmp(key, "socktype"))
		{
			socktype = atoi(val);
		}
		else if (!strcmp(key, "protocol"))
		{
			protocol = atoi(val);
		}
		else if (!strcmp(key, "port"))
		{
			longport = atoi(val);
			port = longport;
		}
		else if (!strcmp(key, "scopeid"))
		{
			scopeid = atoi(val);
		}
		else if (!strcmp(key, "address")) addr = strdup(val);
		else if (!strcmp(key, "canonname")) canonname = strdup(val);
		free(key);
		free(val);
	}

	if (family == PF_UNSPEC)
	{
		if (addr != NULL) free(addr);
		if (canonname != NULL) free(canonname);
		return NULL;
	}

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

	if (addr != NULL) free(addr);
	if (canonname != NULL) free(canonname);

	return a;
}

static int
gai_make_query(const char *nodename, const char *servname, const struct addrinfo *hints, char *buf, uint32_t *len)
{
	int32_t numerichost, family, proto, socktype, canonname, passive;
	uint32_t na;
	XDR outxdr;
	char str[64], *cname;

	numerichost = 0;
	family = PF_UNSPEC;
	proto = IPPROTO_UNSPEC;
	socktype = SOCK_UNSPEC;
	canonname = 0;
	passive = 0;
	cname = NULL;

	if (hints != NULL)
	{
		family = hints->ai_family;
		if (hints->ai_flags & AI_NUMERICHOST) numerichost = 1;
		if (hints->ai_flags & AI_CANONNAME) canonname = 1;
		if ((hints->ai_flags & AI_PASSIVE) == 1) passive = 1;

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

	xdrmem_create(&outxdr, buf, *len, XDR_ENCODE);

	/* Attribute count */
	na = 0;
	if (nodename != NULL) na++;
	if (servname != NULL) na++;
	if (proto != IPPROTO_UNSPEC) na++;
	if (socktype != SOCK_UNSPEC) na++;
	if (family != PF_UNSPEC) na++;
	if (canonname != 0) na++;
	if (passive != 0) na++;
	if (numerichost != 0) na++;

	if (!xdr_int(&outxdr, &na))
	{
		xdr_destroy(&outxdr);
		return EAI_SYSTEM;
	}

	if (nodename != NULL)
	{
		if (encode_kv(&outxdr, "name", nodename) != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (servname != NULL)
	{
		if (encode_kv(&outxdr, "service", servname) != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (proto != IPPROTO_UNSPEC)
	{
		snprintf(str, 64, "%u", proto);
		if (encode_kv(&outxdr, "protocol", str) != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}
	
	if (socktype != SOCK_UNSPEC)
	{
		snprintf(str, 64, "%u", socktype);
		if (encode_kv(&outxdr, "socktype", str) != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}
	
	if (family != PF_UNSPEC)
	{
		snprintf(str, 64, "%u", family);
		if (encode_kv(&outxdr, "family", str) != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}
	
	if (canonname != 0)
	{
		if (encode_kv(&outxdr, "canonname", "1") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}
	
	if (passive != 0)
	{
		if (encode_kv(&outxdr, "passive", "1") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}
	
	if (numerichost != 0)
	{
		if (encode_kv(&outxdr, "numerichost", "1") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	*len = xdr_getpos(&outxdr);

	xdr_destroy(&outxdr);
	
	return 0;
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

int
gai_files(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	int32_t i, numericserv, numerichost, family, proto, wantv4, wantv6;
	int16_t port;
	struct servent *s;
	struct hostent *h;
	char *protoname, *loopv4, *loopv6;
	struct in_addr a4;
	struct in6_addr a6;
	struct addrinfo *a;

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
			numerichost = inet_pton(AF_INET, nodename, &a4);
			if ((numerichost == 1) && (family == PF_UNSPEC)) family = PF_INET;
		}

		if ((family == PF_UNSPEC) || (family == PF_INET6))
		{
			numerichost = inet_pton(AF_INET6, nodename, &a6);
			if ((numerichost == 1) && (family == PF_UNSPEC)) family = PF_INET6;
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
			}

			if ((proto == IPPROTO_UNSPEC) || (proto == IPPROTO_TCP))
			{
				a = new_addrinfo_v4(0, SOCK_STREAM, IPPROTO_TCP, port, a4, 0, NULL);
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
		}

		return 0;
	}

	if (wantv4 == 1)
	{
		h = gethostbyname(nodename);
		if (h == NULL) return 0;

		for (i = 0; h->h_addr_list[i] != 0; i++)
		{
			memmove((void *)&a4.s_addr, h->h_addr_list[i], h->h_length);

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
		}
	}

	return 0;
}

static int
gai_lookupd(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	uint32_t n, i, qlen, rlen;
	XDR inxdr;
	char qbuf[LU_QBUF_SIZE];
	char *rbuf;
	char *cname;
	mach_port_t server_port;
	kern_return_t status;
	struct addrinfo *a;

	server_port = MACH_PORT_NULL;
	if (_lu_running()) server_port = _lookupd_port(0);
	if (server_port == MACH_PORT_NULL)
	{
		/* lookupd isn't available - fall back to the flat files */
		return gai_files(nodename, servname, hints, res);
	}

	if (gai_proc < 0)
	{
		status = _lookup_link(server_port, "getaddrinfo", &gai_proc);
		if (status != KERN_SUCCESS) return EAI_SYSTEM;
	}

	qlen = LU_QBUF_SIZE;
	i = gai_make_query(nodename, servname, hints, qbuf, &qlen);
	if (i != 0) return EAI_SYSTEM;

	qlen /= BYTES_PER_XDR_UNIT;

	rbuf = NULL;

	status = _lookup_all(server_port, gai_proc, (unit *)qbuf, qlen, &rbuf, &rlen);
	if (status != KERN_SUCCESS) return EAI_NODATA;

	rlen *= BYTES_PER_XDR_UNIT;

	xdrmem_create(&inxdr, rbuf, rlen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		return EAI_SYSTEM;
	}

	cname = NULL;
	for (i = 0; i < n; i++)
	{
		a = gai_lookupd_process_dictionary(&inxdr);
		if ((cname == NULL) && (a->ai_canonname != NULL)) cname = a->ai_canonname;
		append_addrinfo(res, a);
	}

	xdr_destroy(&inxdr);
	if (rbuf != NULL) vm_deallocate(mach_task_self(), (vm_address_t)rbuf, rlen);

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
getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	int32_t status, nodenull, servnull;

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

	if (nodenull == 1) status = gai_lookupd(NULL, servname, hints, res);
	else if (servnull == 1) status = gai_lookupd(nodename, NULL, hints, res);
	else status = gai_lookupd(nodename, servname, hints, res);

	if ((status == 0) && (*res == NULL)) status = EAI_NODATA;

	return status;
}

int32_t
getaddrinfo_async_start(mach_port_t *p, const char *nodename, const char *servname, const struct addrinfo *hints, getaddrinfo_async_callback callback, void *context)
{
	int32_t status;
	uint32_t i, qlen;
	char qbuf[LU_QBUF_SIZE];
	mach_port_t server_port;

	*p = MACH_PORT_NULL;

	if ((nodename == NULL) && (servname == NULL)) return EAI_NONAME;

	status = gai_checkhints(hints);
	if (status != 0) return EAI_BADHINTS;

	server_port = MACH_PORT_NULL;
	if (_lu_running()) server_port = _lookupd_port(0);
	if (server_port == MACH_PORT_NULL) return EAI_SYSTEM;

	if (gai_proc < 0)
	{
		status = _lookup_link(server_port, "getaddrinfo", &gai_proc);
		if (status != KERN_SUCCESS) return EAI_SYSTEM;
	}

	qlen = LU_QBUF_SIZE;
	i = gai_make_query(nodename, servname, hints, qbuf, &qlen);
	if (i != 0) return EAI_SYSTEM;

	qlen /= BYTES_PER_XDR_UNIT;

	return lu_async_start(p, gai_proc, qbuf, qlen, (void *)callback, context);
}

int32_t
getaddrinfo_async_send(mach_port_t *p, const char *nodename, const char *servname, const struct addrinfo *hints)
{
	return getaddrinfo_async_start(p, nodename, servname, hints, NULL, NULL);
}

static int
gai_extract_data(char *buf, uint32_t len, struct addrinfo **res)
{
	XDR xdr;
	uint32_t i, n;
	char *cname;
	struct addrinfo *a;

	*res = NULL;

	if (buf == NULL) return EAI_NODATA;
	if (len == 0) return EAI_NODATA;

	xdrmem_create(&xdr, buf, len, XDR_DECODE);

	if (!xdr_int(&xdr, &n))
	{
		xdr_destroy(&xdr);
		return EAI_SYSTEM;
	}

	cname = NULL;
	for (i = 0; i < n; i++)
	{
		a = gai_lookupd_process_dictionary(&xdr);
		if (a == NULL) break;

		if ((cname == NULL) && (a->ai_canonname != NULL)) cname = a->ai_canonname;
		append_addrinfo(res, a);
	}

	xdr_destroy(&xdr);

	if ((cname != NULL) && (res != NULL) && (res[0] != NULL) && (res[0]->ai_canonname == NULL))
	{
		res[0]->ai_canonname = strdup(cname);
	}

	if (*res == NULL) return EAI_NODATA;
	return 0;
}

int32_t
getaddrinfo_async_receive(mach_port_t p, struct addrinfo **res)
{
	kern_return_t status;
	char *buf;
	uint32_t len;

	if (res == NULL) return 0;
	*res = NULL;

	buf = NULL;
	len = 0;

	status = lu_async_receive(p, &buf, &len);
	if (status < 0) return EAI_FAIL;

	status = gai_extract_data(buf, len, res);
	if (buf != NULL) vm_deallocate(mach_task_self(), (vm_address_t)buf, len);
	if (status != 0) return status;

	if (*res == NULL) return EAI_NODATA;

	return 0;
}

int32_t
getaddrinfo_async_handle_reply(void *msg)
{
	getaddrinfo_async_callback callback;
	void *context;
	char *buf;
	uint32_t len;
	int status;
	struct addrinfo *res;

	callback = (getaddrinfo_async_callback)NULL;
	context = NULL;
	buf = NULL;
	len = 0;
	res = NULL;

	status = lu_async_handle_reply(msg, &buf, &len, (void **)&callback, &context);
	if (status != KERN_SUCCESS)
	{
		if (status == MIG_REPLY_MISMATCH) return 0;
		if (callback != NULL) callback(EAI_SYSTEM, NULL, context);
		return EAI_SYSTEM;
	}

	status = gai_extract_data(buf, len, &res);
	if (buf != NULL) vm_deallocate(mach_task_self(), (vm_address_t)buf, len);
	if (status != 0)
	{
		if (callback != NULL) callback(status, NULL, context);
		return status;
	}

	if (res == NULL)
	{
		callback(EAI_NODATA, NULL, context);
		return EAI_NODATA;
	}

	callback(0, res, context);
	return 0;
}

/*
 * getnameinfo
 */
 
/*
 * getnameinfo support in lookupd
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
gni_lookupd_process_dictionary(XDR *inxdr, char **host, char **serv)
{
	int32_t i, nkeys, nvals;
	char *key, *val;

	if ((host == NULL) || (serv == NULL)) return EAI_SYSTEM;
	if (!xdr_int(inxdr, &nkeys)) return EAI_SYSTEM;

	for (i = 0; i < nkeys; i++)
	{
		key = NULL;
		val = NULL;

		if (!xdr_string(inxdr, &key, LONG_STRING_LENGTH)) return EAI_SYSTEM;
		if (!xdr_int(inxdr, &nvals)) 
		{
			free(key);
			return EAI_SYSTEM;
		}

		if (nvals != 1)
		{
			free(key);
			return EAI_SYSTEM;
		}

		if (!xdr_string(inxdr, &val, LONG_STRING_LENGTH))
		{
			free(key);
			return EAI_SYSTEM;
		}

		if (!strcmp(key, "name"))
		{
			*host = val;
			val = NULL;
		}

		else if (!strcmp(key, "service"))
		{
			*serv = val;
			val = NULL;
		}

		if (key != NULL) free(key);
		if (val != NULL) free(val);
	}

	return 0;
}

static int
gni_make_query(const struct sockaddr *sa, size_t salen, int wanthost, int wantserv, int flags, char *buf, uint32_t *len)
{
	XDR outxdr;
	uint16_t port;
	char str[_LU_MAXLUSTRLEN], *key, ifname[IF_NAMESIZE];
	uint32_t a4, ifnum, offset, na, proto, fqdn, numerichost, numericserv, name_req;
	struct sockaddr_in6 *s6;

	if (sa == NULL) return EAI_FAIL;
	if (sa->sa_len != salen) return EAI_FAMILY;

	proto = IPPROTO_TCP;
	fqdn = 1;
	numerichost = 0;
	numericserv = 0;
	name_req = 0;

	offset = INET_NTOP_AF_INET_OFFSET;
	key = "ip_address";
	port = 0;

	if (sa->sa_family == PF_INET)
	{
		a4 = ntohl(((const struct sockaddr_in *)sa)->sin_addr.s_addr);
		if (IN_MULTICAST(a4) || IN_EXPERIMENTAL(a4)) flags |= NI_NUMERICHOST;
		a4 >>= IN_CLASSA_NSHIFT;
		if (a4 == 0) flags |= NI_NUMERICHOST;

		port = ((struct sockaddr_in *)sa)->sin_port;
	}
	else if (sa->sa_family == PF_INET6)
	{
		s6 = (struct sockaddr_in6 *)sa;
		switch (s6->sin6_addr.s6_addr[0])
		{
			case 0x00:
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
			default:
				if (IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr))
				{
					flags |= NI_NUMERICHOST;
				}
				else if (IN6_IS_ADDR_MULTICAST(&s6->sin6_addr))
				{
					flags |= NI_NUMERICHOST;
				}
				break;
		}

		offset = INET_NTOP_AF_INET6_OFFSET;
		key = "ipv6_address";
		port = s6->sin6_port;
	}
	else
	{
		return EAI_FAMILY;
	}

	na = 0;

	if (wanthost != 0) na++;
	if (wantserv != 0) na++;

	if (flags & NI_NOFQDN)
	{
		fqdn = 0;
		na++;
	}

	if (flags & NI_NUMERICHOST)
	{
		numerichost = 1;
		na++;
	}

	if (flags & NI_NUMERICSERV)
	{
		numericserv = 1;
		na++;
	}

	if (flags & NI_NAMEREQD)
	{
		name_req = 1;
		na++;
	}

	if (flags & NI_DGRAM)
	{
		proto = IPPROTO_UDP;
		na++;
	}

	xdrmem_create(&outxdr, buf, *len, XDR_ENCODE);

	if (!xdr_int(&outxdr, &na))
	{
		xdr_destroy(&outxdr);
		return EAI_SYSTEM;
	}

	if (wanthost != 0)
	{
		inet_ntop(sa->sa_family, (char *)(sa) + offset, str, _LU_MAXLUSTRLEN);

		if ((flags & NI_WITHSCOPEID) && (sa->sa_family == AF_INET6))
		{
			ifnum = ((struct sockaddr_in6 *)sa)->sin6_scope_id;
			if (if_indextoname(ifnum, ifname) != NULL)
			{
				strcat(str, "%");
				strcat(str, ifname);
			}
		}

		if (encode_kv(&outxdr, key, str) != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (wantserv != 0)
	{
		snprintf(str, _LU_MAXLUSTRLEN, "%hu", port);
		if (encode_kv(&outxdr, "port", str) != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (proto == IPPROTO_UDP)
	{
		if (encode_kv(&outxdr, "protocol", "udp") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (fqdn == 0)
	{
		if (encode_kv(&outxdr, "fqdn", "0") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (numerichost == 1)
	{
		if (encode_kv(&outxdr, "numerichost", "1") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (numericserv == 1)
	{
		if (encode_kv(&outxdr, "numericserv", "1") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	if (name_req == 1)
	{
		if (encode_kv(&outxdr, "name_required", "1") != 0)
		{
			xdr_destroy(&outxdr);
			return EAI_SYSTEM;
		}
	}

	*len = xdr_getpos(&outxdr);

	xdr_destroy(&outxdr);
	
	return 0;
}

int
getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	uint32_t n, i, qlen, rlen;
	int wanth, wants;
	XDR inxdr;
	char qbuf[LU_QBUF_SIZE];
	char *rbuf, *hval, *sval;
	mach_port_t server_port;
	kern_return_t status;
	struct sockaddr_in *s4;
	struct sockaddr_in6 *s6;

	/* Check input */
	if (sa == NULL) return EAI_FAIL;

	/* V4 mapped and compat addresses are converted to plain V4 */
	if (sa->sa_family == AF_INET6)
	{
		s6 = (struct sockaddr_in6 *)sa;
		if ((IN6_IS_ADDR_V4MAPPED(&s6->sin6_addr)) || (IN6_IS_ADDR_V4COMPAT(&s6->sin6_addr)))
		{
			s4 = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
			s4->sin_len = sizeof(struct sockaddr_in);
			s4->sin_family = AF_INET;
			s4->sin_port = s6->sin6_port;
			memcpy(&(s4->sin_addr.s_addr), &(s6->sin6_addr.s6_addr[12]), 4);

			i = getnameinfo((const struct sockaddr *)s4, s4->sin_len, host, hostlen, serv, servlen, flags);
			free(s4);
			return i;
		}
	}

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
		i = INET_NTOP_AF_INET_OFFSET;
		if (sa->sa_family == PF_INET6) i = INET_NTOP_AF_INET6_OFFSET;
		if (inet_ntop(sa->sa_family, (char *)(sa) + i, host, hostlen) == NULL) return EAI_FAIL;

		if (wants == 0) return 0;
	}

	if ((wants == 1) && (flags & NI_NUMERICSERV))
	{
		if (sa->sa_family == PF_INET)
		{
			s4 = (struct sockaddr_in *)sa;
			n = snprintf(serv, servlen, "%hu", ntohs(s4->sin_port));
			if (n >= servlen) return EAI_FAIL;
		}
		else if (sa->sa_family == PF_INET6)
		{
			s6 = (struct sockaddr_in6 *)sa;
			n = snprintf(serv, servlen, "%hu", ntohs(s6->sin6_port));
			if (n >= servlen) return EAI_FAIL;
		}
		else return EAI_FAMILY;

		if (wanth == 0) return 0;
	}

	if ((wanth == 1) && (flags & NI_NUMERICHOST) && (wants == 1) && (flags & NI_NUMERICSERV)) return 0;

	/*
	 * Ask lookupd
	 */
	server_port = MACH_PORT_NULL;
	if (_lu_running()) server_port = _lookupd_port(0);
	if (server_port == MACH_PORT_NULL) return EAI_SYSTEM;

	if (gni_proc < 0)
	{
		status = _lookup_link(server_port, "getnameinfo", &gni_proc);
		if (status != KERN_SUCCESS) return EAI_SYSTEM;
	}

	qlen = LU_QBUF_SIZE;
	i = gni_make_query(sa, salen, wanth, wants, flags, qbuf, &qlen);
	if (i != 0) return EAI_SYSTEM;

	qlen /= BYTES_PER_XDR_UNIT;

	rbuf = NULL;

	status = _lookup_all(server_port, gni_proc, (unit *)qbuf, qlen, &rbuf, &rlen);
	if (status != KERN_SUCCESS) return EAI_NONAME;

	rlen *= BYTES_PER_XDR_UNIT;

	xdrmem_create(&inxdr, rbuf, rlen, XDR_DECODE);

	if (!xdr_int(&inxdr, &n))
	{
		xdr_destroy(&inxdr);
		return EAI_SYSTEM;
	}

	if (n != 1)
	{
		xdr_destroy(&inxdr);
		return EAI_NONAME;
	}

	hval = NULL;
	sval = NULL;

	i = gni_lookupd_process_dictionary(&inxdr, &hval, &sval);

	xdr_destroy(&inxdr);
	if (rbuf != NULL) vm_deallocate(mach_task_self(), (vm_address_t)rbuf, rlen);

	if (i != 0) return i;

	i = 0;
	if (hval != NULL) i = strlen(hval) + 1;
	if ((host != NULL) && (hostlen != 0) && (i != 0))
	{
		if (i > hostlen) return EAI_FAIL;
		memcpy(host, hval, i);
		free(hval);
	}

	i = 0;
	if (sval != NULL) i = strlen(sval) + 1;
	if ((serv != NULL) && (servlen != 0) && (i != 0))
	{
		if (i > servlen) return EAI_FAIL;
		memcpy(serv, sval, i);
		free(sval);
	}

	return 0;
}

int32_t
getnameinfo_async_start(mach_port_t *p, const struct sockaddr *sa, size_t salen, int flags, getnameinfo_async_callback callback, void *context)
{
	uint32_t i, qlen;
	char qbuf[LU_QBUF_SIZE];
	mach_port_t server_port;
	kern_return_t status;

	/* Check input */
	if (sa == NULL) return EAI_FAIL;

	server_port = MACH_PORT_NULL;
	if (_lu_running()) server_port = _lookupd_port(0);
	if (server_port == MACH_PORT_NULL) return EAI_SYSTEM;

	if (gni_proc < 0)
	{
		status = _lookup_link(server_port, "getnameinfo", &gni_proc);
		if (status != KERN_SUCCESS) return EAI_SYSTEM;
	}

	qlen = LU_QBUF_SIZE;
	i = gni_make_query(sa, salen, 1, 1, flags, qbuf, &qlen);
	if (i != 0) return EAI_SYSTEM;

	qlen /= BYTES_PER_XDR_UNIT;

	return lu_async_start(p, gni_proc, qbuf, qlen, (void *)callback, context);
}

int32_t
getnameinfo_async_send(mach_port_t *p, const struct sockaddr *sa, size_t salen, int flags)
{
	return getnameinfo_async_start(p, sa, salen, flags, NULL, NULL);
}

static int
gni_extract_data(char *buf, uint32_t len, char **host, char **serv)
{
	XDR xdr;
	uint32_t n;

	*host = NULL;
	*serv = NULL;

	if (buf == NULL) return EAI_NODATA;
	if (len == 0) return EAI_NODATA;

	xdrmem_create(&xdr, buf, len, XDR_DECODE);

	if (!xdr_int(&xdr, &n))
	{
		xdr_destroy(&xdr);
		return EAI_SYSTEM;
	}

	if (n != 1)
	{
		xdr_destroy(&xdr);
		return EAI_NONAME;
	}

	return gni_lookupd_process_dictionary(&xdr, host, serv);
}

int32_t
getnameinfo_async_receive(mach_port_t p, char **host, char **serv)
{
	kern_return_t status;
	char *buf;
	uint32_t len;

	buf = NULL;
	len = 0;

	status = lu_async_receive(p, &buf, &len);
	if (status < 0) return EAI_FAIL;

	status = gni_extract_data(buf, len, host, serv);
	if (buf != NULL) vm_deallocate(mach_task_self(), (vm_address_t)buf, len);
	if (status != 0) return status;

	return 0;
}

int32_t
getnameinfo_async_handle_reply(void *msg)
{
	getnameinfo_async_callback callback;
	void *context;
	char *buf, *hval, *sval;
	uint32_t len;
	int status;

	callback = (getnameinfo_async_callback)NULL;
	context = NULL;
	buf = NULL;
	len = 0;

	status = lu_async_handle_reply(msg, &buf, &len, (void **)&callback, &context);
	if (status != KERN_SUCCESS)
	{
		if (status == MIG_REPLY_MISMATCH) return 0;
		if (callback != NULL) callback(EAI_SYSTEM, NULL, NULL, context);
		return EAI_SYSTEM;
	}

	hval = NULL;
	sval = NULL;

	status = gni_extract_data(buf, len, &hval, &sval);
	if (buf != NULL) vm_deallocate(mach_task_self(), (vm_address_t)buf, len);
	if (status != 0)
	{
		if (callback != NULL) callback(status, NULL, NULL, context);
		return status;
	}

	callback(0, hval, sval, context);
	return 0;
}
