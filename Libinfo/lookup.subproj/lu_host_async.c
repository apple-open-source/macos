/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2002 Apple Computer, Inc. All Rights
 * Reserved. This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License"). You may not use this file
 * except in compliance with the License. Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT. Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <netdb.h>
#include <netdb_async.h>
#include <pthread.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>

#include "lu_host.h"
#include "lu_utils.h"

#define IPV6_ADDR_LEN 16
#define IPV4_ADDR_LEN 4

typedef struct
{
	void *user_context;
	int want;
} my_context_t;

mach_port_t
gethostbyaddr_async_start(const char *addr, int len, int family, gethostbyaddr_async_callback callback, void *context)
{
	static int proc = 1;
	int32_t want, status;
	kvbuf_t *request;
	mach_port_t mp;
	my_context_t *my_context;

	mp = MACH_PORT_NULL;

	if (addr == NULL) return MACH_PORT_NULL;
	if (len == 0) return MACH_PORT_NULL;
	if ((family != AF_INET) && (family != AF_INET6)) return MACH_PORT_NULL;

	want = WANT_A4_ONLY;
	if (family == AF_INET6) want = WANT_A6_ONLY;

	if ((family == AF_INET6) && (len == IPV6_ADDR_LEN) && (is_a4_mapped((const char *)addr) || is_a4_compat((const char *)addr)))
	{
		addr += 12;
		len = 4;
		family = AF_INET;
		want = WANT_MAPPED_A4_ONLY;
	}

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbyaddr", &proc);
		if (status != KERN_SUCCESS) return MACH_PORT_NULL;
	}

	request = kvbuf_query("ksku", "address", addr, "family", want);
	if (request == NULL) return MACH_PORT_NULL;

	my_context = (my_context_t *)calloc(1, sizeof(my_context_t));
	if (my_context == NULL) return MACH_PORT_NULL;

	my_context->user_context = context;
	my_context->want = want;

	status = LI_async_start(&mp, proc, request, (void *)callback, my_context);

	kvbuf_free(request);
	return mp;
}

void
gethostbyaddr_async_cancel(mach_port_t port)
{
	my_context_t *my_context;

	my_context = NULL;

	LI_async_call_cancel(port, (void **)&my_context);

	if (my_context != NULL) free(my_context);
}

void
gethostbyaddr_async_handleReply(void *msg)
{
	gethostbyaddr_async_callback callback;
	struct hostent *out;
	uint32_t len, want;
	int status;
	kvarray_t *reply;
	my_context_t *my_context;
	void *context;

	callback = (gethostbyaddr_async_callback)NULL;
	my_context = NULL;
	context = NULL;
	len = 0;
	reply = NULL;

	status = LI_async_handle_reply(msg, &reply, (void **)&callback, (void **)&my_context);
	if ((status != KERN_SUCCESS) || (reply == NULL))
	{
		if (status == MIG_REPLY_MISMATCH) return;
		if (callback != NULL)
		{
			if (my_context != NULL) context = my_context->user_context;
			callback(NULL, context);
			free(my_context);
			return;
		}
	}

	want = WANT_A4_ONLY;
	if (my_context != NULL)
	{
		context = my_context->user_context;
		want = my_context->want;
		free(my_context);
	}

	out = extract_host(reply, want);
	kvarray_free(reply);

	callback(out, context);
}

mach_port_t
getipnodebyaddr_async_start(const void *addr, size_t len, int family, int *error, getipnodebyaddr_async_callback callback, void *context)
{
	static int proc = 1;
	int32_t want, status;
	kvbuf_t *request;
	mach_port_t mp;
	my_context_t *my_context;

	mp = MACH_PORT_NULL;

	if (addr == NULL) return MACH_PORT_NULL;
	if (len == 0) return MACH_PORT_NULL;
	if ((family != AF_INET) && (family != AF_INET6)) return MACH_PORT_NULL;

	want = WANT_A4_ONLY;
	if (family == AF_INET6) want = WANT_A6_ONLY;

	if ((family == AF_INET6) && (len == IPV6_ADDR_LEN) && (is_a4_mapped((const char *)addr) || is_a4_compat((const char *)addr)))
	{
		addr += 12;
		len = 4;
		family = AF_INET;
		want = WANT_MAPPED_A4_ONLY;
	}

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbyaddr", &proc);
		if (status != KERN_SUCCESS) return MACH_PORT_NULL;
	}

	request = kvbuf_query("ksku", "address", addr, "family", want);
	if (request == NULL) return MACH_PORT_NULL;

	my_context = (my_context_t *)calloc(1, sizeof(my_context_t));
	if (my_context == NULL) return MACH_PORT_NULL;

	my_context->user_context = context;
	my_context->want = want;

	status = LI_async_start(&mp, proc, request, (void *)callback, my_context);

	kvbuf_free(request);
	return mp;
}

void
getipnodebyaddr_async_cancel(mach_port_t port)
{
	my_context_t *my_context;

	my_context = NULL;

	LI_async_call_cancel(port, (void **)&my_context);

	if (my_context != NULL) free(my_context);
}

void
getipnodebyaddr_async_handleReply(void *msg)
{
	getipnodebyaddr_async_callback callback;
	struct hostent *out;
	uint32_t len, want;
	int status;
	kvarray_t *reply;
	my_context_t *my_context;
	void *context;

	callback = (getipnodebyaddr_async_callback)NULL;
	my_context = NULL;
	context = NULL;
	len = 0;
	reply = NULL;

	status = LI_async_handle_reply(msg, &reply, (void **)&callback, (void **)&my_context);
	if ((status != KERN_SUCCESS) || (reply == NULL))
	{
		if (status == MIG_REPLY_MISMATCH) return;
		if (callback != NULL)
		{
			if (my_context != NULL) context = my_context->user_context;
			callback(NULL, NO_RECOVERY, context);
			free(my_context);
			return;
		}
	}

	want = WANT_A4_ONLY;
	if (my_context != NULL)
	{
		context = my_context->user_context;
		want = my_context->want;
		free(my_context);
	}

	out = extract_host(reply, want);
	kvarray_free(reply);

	if (out == NULL)
	{
		callback(NULL, HOST_NOT_FOUND, context);
		return;
	}

	callback(out, 0, context);
}

mach_port_t
gethostbyname_async_start(const char *name, gethostbyname_async_callback callback, void *context)
{
	static int proc = 1;
	int32_t status;
	kvbuf_t *request;
	mach_port_t mp;

	mp = MACH_PORT_NULL;

	if (name == NULL) return MACH_PORT_NULL;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbyname", &proc);
		if (status != KERN_SUCCESS) return MACH_PORT_NULL;
	}

	request = kvbuf_query("ksksks", "name", name, "ipv4", "1", "ipv6", "0");
	if (request == NULL) return MACH_PORT_NULL;

	status = LI_async_start(&mp, proc, request, (void *)callback, context);

	kvbuf_free(request);
	return mp;
}

void
gethostbyname_async_cancel(mach_port_t port)
{
	LI_async_call_cancel(port, NULL);
}

void
gethostbyname_async_handleReply(void *msg)
{
	gethostbyname_async_callback callback;
	struct hostent *out;
	uint32_t len;
	int status;
	kvarray_t *reply;
	void *context;

	callback = (gethostbyname_async_callback)NULL;
	context = NULL;
	len = 0;
	reply = NULL;

	status = LI_async_handle_reply(msg, &reply, (void **)&callback, (void **)&context);
	if ((status != KERN_SUCCESS) || (reply == NULL))
	{
		if (status == MIG_REPLY_MISMATCH) return;
		if (callback != NULL)
		{
			callback(NULL, context);
			return;
		}
	}

	out = extract_host(reply, AF_INET);
	kvarray_free(reply);

	callback(out, context);
}

mach_port_t
getipnodebyname_async_start(const char *name, int family, int flags, int *err, getipnodebyname_async_callback callback, void *context)
{
	static int proc = 1;
	int32_t status, want, want4, want6, if4, if6;
	kvbuf_t *request;
	mach_port_t mp;
	struct ifaddrs *ifa, *ifap;
	struct in_addr addr4;
	struct in6_addr addr6;
	my_context_t *my_context;

	if (name == NULL) return MACH_PORT_NULL;

	if (err != NULL) *err = 0;

	if4 = 0;
	if6 = 0;
	mp = MACH_PORT_NULL;
	memset(&addr4, 0, sizeof(struct in_addr));
	memset(&addr6, 0, sizeof(struct in6_addr));

	if (flags & AI_ADDRCONFIG)
	{
		if (getifaddrs(&ifa) < 0)
		{
			if (err != NULL) *err = NO_RECOVERY;
			return MACH_PORT_NULL;
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
			if (err != NULL) *err = NO_RECOVERY;
			return MACH_PORT_NULL;
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
			if (err != NULL) *err = NO_RECOVERY;
			return MACH_PORT_NULL;
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
				if (err != NULL) *err = NO_RECOVERY;
				return MACH_PORT_NULL;
			}
		}
	}

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbyname", &proc);
		if (status != KERN_SUCCESS) return MACH_PORT_NULL;
	}

	my_context = (my_context_t *)calloc(1, sizeof(my_context_t));
	if (my_context == NULL)
	{
		*err = NO_RECOVERY;
		return MACH_PORT_NULL;
	}

	my_context->user_context = context;
	my_context->want = want;

	want4 = 1;
	want6 = 1;

	if (want == WANT_A4_ONLY) want6 = 0;
	else if (want == WANT_A6_ONLY) want4 = 0;
	else if (WANT_MAPPED_A4_ONLY) want6 = 0;

	request = kvbuf_query("kskuku", "name", name, "ipv4", want4, "ipv6", want6);
	if (request == NULL) return MACH_PORT_NULL;

	status = LI_async_start(&mp, proc, request, (void *)callback, my_context);

	kvbuf_free(request);
	return mp;
}

void
getipnodebyname_async_cancel(mach_port_t port)
{
	my_context_t *my_context;

	my_context = NULL;

	LI_async_call_cancel(port, (void **)&my_context);

	if (my_context != NULL) free(my_context);
}

void
getipnodebyname_async_handleReply(void *msg)
{
	getipnodebyname_async_callback callback;
	struct hostent *out;
	uint32_t len, want;
	int status, err;
	kvarray_t *reply;
	my_context_t *my_context;
	void *context;

	callback = (getipnodebyname_async_callback)NULL;
	my_context = NULL;
	context = NULL;
	len = 0;
	reply = NULL;

	status = LI_async_handle_reply(msg, &reply, (void **)&callback, (void **)&my_context);
	if ((status != KERN_SUCCESS) || (reply == NULL))
	{
		if (status == MIG_REPLY_MISMATCH) return;
		if (callback != NULL)
		{
			if (my_context != NULL) context = my_context->user_context;
			callback(NULL, NO_RECOVERY, context);
			free(my_context);
			return;
		}
	}

	want = WANT_A4_ONLY;
	if (my_context != NULL)
	{
		context = my_context->user_context;
		want = my_context->want;
		free(my_context);
	}

	out = extract_host(reply, want);
	kvarray_free(reply);

	if (out == NULL)
	{
		err = HOST_NOT_FOUND;
		callback(NULL, err, context);
		return;
	}

	callback(out, 0, context);
}
