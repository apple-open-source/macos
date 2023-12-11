/*
 * Copyright (c) 2000-2023 Apple Inc.  All Rights Reserved.
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
 * ifutil.c
 * - pulled out of ip_plugin.c
 * - handles various interface specific utility functions including:
 *   + interface <-> name cache
 *   + effective index cache
 *   + shared configuration sockets
 *   + various ioctls
 */

#define __SC_CFRELEASE_NEEDED	1
#include "ip_plugin.h"
#include "symbol_scope.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <CoreFoundation/CFDictionary.h>

#pragma mark -
#pragma mark Effective Index

#define IFIndex_NONE	((unsigned int)-1)

static CFMutableDictionaryRef	S_effective_ifindex_cache;

static Boolean
IFIndexEqual(const void *ptr1, const void *ptr2)
{
    return (IFIndex)ptr1 == (IFIndex)ptr2;
}

static CFHashCode
IFIndexHash(const void *ptr)
{
    return (CFHashCode)((IFIndex)ptr);
}

static CFStringRef
IFIndexCopyDescription(const void *ptr)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), (IFIndex)ptr);
}

static CFDictionaryKeyCallBacks
IFIndexKeyCallBacks = {
	0, NULL, NULL, IFIndexCopyDescription, IFIndexEqual, IFIndexHash
};

static CFDictionaryValueCallBacks
IFIndexValueCallBacks = {
	0, NULL, NULL, IFIndexCopyDescription, IFIndexEqual
};

static void
effective_ifindex_add(IFIndex ifindex, IFIndex effective_ifindex)
{
	if (S_effective_ifindex_cache == NULL) {
		S_effective_ifindex_cache
			= CFDictionaryCreateMutable(NULL, 0,
						    &IFIndexKeyCallBacks,
						    &IFIndexValueCallBacks);
	}
	CFDictionarySetValue(S_effective_ifindex_cache,
			     (const void *)(uintptr_t)ifindex,
			     (const void *)(uintptr_t)effective_ifindex);
	return;
}

static IFIndex
effective_ifindex_lookup(IFIndex ifindex)
{
	IFIndex	effective_ifindex = 0;

	if (S_effective_ifindex_cache != NULL) {
		const void *	val;

		val = CFDictionaryGetValue(S_effective_ifindex_cache,
					   (const void *)(uintptr_t)ifindex);
		effective_ifindex = (IFIndex)(uintptr_t)val;
	}
	return (effective_ifindex);
}

PRIVATE_EXTERN void
effective_ifindex_free(void)
{
	if (S_effective_ifindex_cache != NULL) {
		my_log(LOG_DEBUG, "%s: count %lu",
		       __func__,
		       CFDictionaryGetCount(S_effective_ifindex_cache));
		__SC_CFRELEASE(S_effective_ifindex_cache);
	}
}

PRIVATE_EXTERN IFIndex
effective_ifindex_get(const char * ifname, IFIndex ifindex)
{
	IFIndex		effective_ifindex = 0;
	struct ifreq	ifr;
	int		s;

	if (ifindex != 0) {
		effective_ifindex = effective_ifindex_lookup(ifindex);
		if (effective_ifindex != 0) {
			if (effective_ifindex == IFIndex_NONE) {
				/* cached negative entry */
				effective_ifindex = 0;
			}
			goto done;
		}
	}
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	s = open_inet_dgram_socket();
	if (s != -1 && ioctl(s, SIOCGIFDELEGATE, &ifr) != -1) {
		char	effective_ifname[IFNAMSIZ];

		effective_ifindex = ifr.ifr_delegated;
		if (effective_ifindex != 0
		    && my_if_indextoname(effective_ifindex,
					 effective_ifname) == NULL) {
			effective_ifindex = 0;
		}
		if (effective_ifindex == 0) {
			effective_ifindex_add(ifindex, IFIndex_NONE);
		}
		else {
			my_log(LOG_NOTICE,
			       "%s: %s (%d): effective %s (%d)",
			       __func__, ifname, ifindex,
			       effective_ifname, effective_ifindex);
			effective_ifindex_add(ifindex, effective_ifindex);
		}
	}

 done:
	return (effective_ifindex);
}

#pragma mark -
#pragma mark if_nameindex Cache

#ifndef TEST_ROUTELIST

static struct if_nameindex *	S_if_nameindex_cache;

static dispatch_queue_t
__my_if_nametoindex_queue(void)
{
	static dispatch_once_t	once;
	dispatch_block_t	once_block;
	static dispatch_queue_t	q;

	once_block = ^{
		q = dispatch_queue_create("my_if_nametoindex queue", NULL);
	};
	dispatch_once(&once, once_block);
	return q;
}

PRIVATE_EXTERN IFIndex
my_if_nametoindex(const char * ifname)
{
	__block IFIndex	idx = 0;

	dispatch_sync(__my_if_nametoindex_queue(), ^{
			struct if_nameindex *	scan;

			if (S_if_nameindex_cache == NULL) {
				idx = if_nametoindex(ifname);
				return;
			}
			for (scan = S_if_nameindex_cache;
			     scan->if_index != 0 && scan->if_name != NULL;
			     scan++) {
				if (strcmp(scan->if_name, ifname) == 0) {
					idx = scan->if_index;
					break;
				}
			}
		});

	return (idx);
}

PRIVATE_EXTERN const char *
my_if_indextoname(IFIndex idx, char if_name[IFNAMSIZ])
{
	dispatch_block_t	b;
	__block const char *	name = NULL;

	b = ^{
		struct if_nameindex *	scan;

		if (S_if_nameindex_cache == NULL) {
			name = if_indextoname(idx, if_name);
			return;
		}
		for (scan = S_if_nameindex_cache;
		     scan->if_index != 0 && scan->if_name != NULL;
		     scan++) {
			if (scan->if_index == idx) {
				name = if_name;
				strlcpy(if_name, scan->if_name, IFNAMSIZ);
				break;
			}
		}
	};
	dispatch_sync(__my_if_nametoindex_queue(), b);
	return (name);
}

static void
_my_if_freenameindex(void)
{
	if (S_if_nameindex_cache != NULL) {
		if_freenameindex(S_if_nameindex_cache);
		S_if_nameindex_cache = NULL;
	}
}

PRIVATE_EXTERN void
my_if_freenameindex(void)
{
	dispatch_block_t	b;

	b = ^{
		_my_if_freenameindex();
	};
	dispatch_sync(__my_if_nametoindex_queue(), b);
	return;
}

PRIVATE_EXTERN void
my_if_nameindex(void)
{
	dispatch_block_t	b;

	b = ^{
		_my_if_freenameindex();
		S_if_nameindex_cache = if_nameindex();
	};		
	dispatch_sync(__my_if_nametoindex_queue(), b);
	return;
}

#else /* TEST_ROUTELIST */

static const char * * 	list;
static int		list_count;
static int		list_size;

PRIVATE_EXTERN IFIndex
my_if_nametoindex(const char * ifname)
{
	IFIndex		ret;

	if (list == NULL) {
		list_size = 4;
		list_count = 2;
		list = (const char * *)malloc(sizeof(*list) * list_size);
		list[0] = strdup("");
		list[1] = strdup(kLoopbackInterface);
	}
	else {
		int	i;

		for (i = 1; i < list_count; i++) {
			if (strcmp(list[i], ifname) == 0) {
				ret = i;
				goto done;
			}
		}
	}
	if (list_count == list_size) {
		list_size += 2;
		list = (const char * *)realloc(list, sizeof(*list) * list_size);
	}
	list[list_count] = strdup(ifname);
	ret = list_count;
	list_count++;
 done:
	return (ret);
}

PRIVATE_EXTERN const char *
my_if_indextoname(IFIndex idx, char if_name[IFNAMSIZ])
{
	const char *	name = NULL;

	if (idx < list_count) {
		name = if_name;
		strlcpy(if_name, list[idx], IFNAMSIZ);
	}
	return (name);
}

PRIVATE_EXTERN void
my_if_nameindex(void)
{
}

PRIVATE_EXTERN void
my_if_freenameindex(void)
{
}

#endif /* TEST_ROUTELIST */

PRIVATE_EXTERN const char *
my_if_indextoname2(IFIndex ifindex, char ifname[IFNAMSIZ])
{
	if (ifindex == 0) {
		return (NULL);
	}
	if (my_if_indextoname(ifindex, ifname) == NULL) {
		snprintf(ifname, IFNAMSIZ, "[%d]", ifindex);
	}
	return (ifname);
}


PRIVATE_EXTERN IFIndex
lo0_ifindex(void)
{
	static IFIndex		idx;

	if (idx == 0) {
		idx = my_if_nametoindex(kLoopbackInterface);
	}
	return (idx);
}


#pragma mark -
#pragma mark configuration sockets

static int inet_dgram_socket = -1;

PRIVATE_EXTERN int
open_inet_dgram_socket(void)
{
	if (inet_dgram_socket != -1) {
		goto done;
	}
	inet_dgram_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (inet_dgram_socket == -1) {
		my_log(LOG_ERR, "socket() failed: %s", strerror(errno));
	}
 done:
	return inet_dgram_socket;
}

PRIVATE_EXTERN void
close_inet_dgram_socket(void)
{
	if (inet_dgram_socket != -1) {
		close(inet_dgram_socket);
		inet_dgram_socket = -1;
	}
}

static int inet6_dgram_socket = -1;

PRIVATE_EXTERN int
open_inet6_dgram_socket(void)
{
	if (inet6_dgram_socket != -1) {
		goto done;
	}
	inet6_dgram_socket = socket(AF_INET6, SOCK_DGRAM, 0);
	if (inet6_dgram_socket == -1) {
		my_log(LOG_ERR, "socket() failed: %s", strerror(errno));
	}
 done:
	return inet6_dgram_socket;
}

PRIVATE_EXTERN void
close_inet6_dgram_socket(void)
{
	if (inet6_dgram_socket != -1) {
		close(inet6_dgram_socket);
		inet6_dgram_socket = -1;
	}
}

#pragma mark -
#pragma mark ioctls

PRIVATE_EXTERN int
siocdradd_in6(int s, int if_index, const struct in6_addr * addr, u_char flags)
{
	struct in6_defrouter	dr;
	struct sockaddr_in6 *	sin6;

	memset(&dr, 0, sizeof(dr));
	sin6 = &dr.rtaddr;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = *addr;
	dr.flags = flags;
	dr.if_index = if_index;
	return (ioctl(s, SIOCDRADD_IN6, &dr));
}

PRIVATE_EXTERN int
siocdrdel_in6(int s, int if_index, const struct in6_addr * addr)
{
	struct in6_defrouter	dr;
	struct sockaddr_in6 *	sin6;

	memset(&dr, 0, sizeof(dr));
	sin6 = &dr.rtaddr;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_addr = *addr;
	dr.if_index = if_index;
	return (ioctl(s, SIOCDRDEL_IN6, &dr));
}

PRIVATE_EXTERN int
siocsifnetsignature(int s, const char * ifname, int af,
		    const uint8_t * signature, size_t signature_length)
{
	struct if_nsreq	nsreq;

	memset(&nsreq, 0, sizeof(nsreq));
	strlcpy(nsreq.ifnsr_name, ifname, sizeof(nsreq.ifnsr_name));
	nsreq.ifnsr_family = af;
	if (signature_length > 0) {
		if (signature_length > sizeof(nsreq.ifnsr_data)) {
			signature_length = sizeof(nsreq.ifnsr_data);
		}
		nsreq.ifnsr_len = signature_length;
		memcpy(nsreq.ifnsr_data, signature, signature_length);
	}
	return (ioctl(s, SIOCSIFNETSIGNATURE, &nsreq));
}

PRIVATE_EXTERN boolean_t
set_ipv6_default_interface(IFIndex ifindex)
{
	struct in6_ndifreq	ndifreq;
	int			sock;
	boolean_t		success = FALSE;

	memset((char *)&ndifreq, 0, sizeof(ndifreq));
	strlcpy(ndifreq.ifname, kLoopbackInterface, sizeof(ndifreq.ifname));
	if (ifindex != 0) {
		ndifreq.ifindex = ifindex;
	}
	else {
		ndifreq.ifindex = lo0_ifindex();
	}
	sock = open_inet6_dgram_socket();
	if (sock < 0) {
		goto done;
	}
	if (ioctl(sock, SIOCSDEFIFACE_IN6, (caddr_t)&ndifreq) == -1) {
		my_log(LOG_ERR,
		       "ioctl(SIOCSDEFIFACE_IN6) failed: %s",
		       strerror(errno));
	}
	else {
		success = TRUE;
	}
 done:
	return (success);
}

#ifdef TEST_IF_NAMEINDEX
#include <ifaddrs.h>

int
main(int argc, char * argv[])
{
	struct ifaddrs *	list;

	my_if_nameindex();
	if (getifaddrs(&list) != 0) {
		perror("getifaddrs");
		exit(2);
	}
	for (int i = 0; i < 4; i++) {
		printf("Round %d\n", i);
		for (struct ifaddrs * scan = list;
		     scan != NULL;
		     scan = scan->ifa_next) {
			IFIndex		ifindex;
			char		name[IFNAMSIZ];
			const char *	name_p;
			
			ifindex = my_if_nametoindex(scan->ifa_name);		
			name_p = my_if_indextoname(ifindex, name);
			printf("%s %d %s effective %d\n",
			       scan->ifa_name, ifindex, name_p,
			       effective_ifindex_get(scan->ifa_name, ifindex));
		}
		if (i > 1) {
			effective_ifindex_free();
			my_if_freenameindex();
		}
	}
	exit(0);
}

#endif /* TEST_IF_NAMEINDEX */
