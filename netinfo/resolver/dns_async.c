/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_headER_START@
 * 
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All Rights
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
 * @APPLE_LICENSE_headER_END@
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <mach/mach.h>
#include <pthread.h>
#include <netinfo/_lu_types.h>
#include <netinfo/lookup.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb_async.h>
#include <netinfo/ni.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dns.h>
#include <dns_util.h>

#define LU_QBUF_SIZE 8192
extern mach_port_t _lookupd_port();
static int dns_proc = -1;

extern mach_port_t _lu_port;
kern_return_t _lookup_link(mach_port_t server, lookup_name name, int *procno);
extern int _lu_running(void);
extern ni_proplist *_lookupd_xdr_dictionary(XDR *inxdr);

static int
encode_kv(XDR *x, const char *k, const char *v)
{
	int32_t n = 1;

	if (!xdr_string(x, (char **)&k, _LU_MAXLUSTRLEN)) return 1;
	if (!xdr_int(x, &n)) return 1;
	if (!xdr_string(x, (char **)&v, _LU_MAXLUSTRLEN)) return 1;

	return 0;
}

static int32_t
dns_make_query(const char *name, uint16_t dnsclass, uint16_t dnstype, uint32_t do_search, char *buf, uint32_t *len)
{
	XDR outxdr;
	uint32_t n;
	char str[128];

	if (name == NULL) return -1;
	if (buf == NULL) return -1;
	if (len == 0) return -1;

	xdrmem_create(&outxdr, buf, *len, XDR_ENCODE);

	/* Encode attribute count */
	n = 4;
	if (!xdr_int(&outxdr, &n))
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	/* Encode name */
	if (encode_kv(&outxdr, "name", (char *)name) != 0)
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	/* Encode class */
	snprintf(str, 128, "%hu", dnsclass);
	if (encode_kv(&outxdr, "class", str) != 0)
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	/* Encode type */
	snprintf(str, 128, "%hu", dnstype);
	if (encode_kv(&outxdr, "type", str) != 0)
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	/* Encode do_search */
	snprintf(str, 128, "%hu", do_search);
	if (encode_kv(&outxdr, "do_search", str) != 0)
	{
		xdr_destroy(&outxdr);
		return -1;
	}

	*len = xdr_getpos(&outxdr);

	xdr_destroy(&outxdr);

	return 0;
}

int32_t
dns_async_start(mach_port_t *p, const char *name, uint16_t dnsclass, uint16_t dnstype, uint32_t do_search, dns_async_callback callback, void *context)
{
	int32_t status;
	kern_return_t ks;
	uint32_t qlen;
	char qbuf[LU_QBUF_SIZE];
	mach_port_t server_port;

	*p = MACH_PORT_NULL;

	if (name == NULL) return NO_RECOVERY;

	server_port = MACH_PORT_NULL;
	if (_lu_running()) server_port = _lookupd_port(0);
	if (server_port == MACH_PORT_NULL) return NO_RECOVERY;

	if (dns_proc < 0)
	{
		status = _lookup_link(server_port, "dns_proxy", &dns_proc);
		if (status != KERN_SUCCESS) return NO_RECOVERY;
	}

	qlen = LU_QBUF_SIZE;
	status = dns_make_query(name, dnsclass, dnstype, do_search, qbuf, &qlen);
	if (status != 0) return NO_RECOVERY;

	qlen /= BYTES_PER_XDR_UNIT;

	ks = lu_async_start(p, dns_proc, qbuf, qlen, (void *)callback, context);
	if (ks != KERN_SUCCESS) return NO_RECOVERY;

	return 0;
}

int32_t
dns_async_send(mach_port_t *p, const char *name, uint16_t dnsclass, uint16_t dnstype, uint32_t do_search)
{
	return dns_async_start(p, name, dnsclass, dnstype, do_search, NULL, NULL);
}

static int
dns_extract_data(char *reply, uint32_t rlen, char **buf, uint32_t *len, struct sockaddr **from, uint32_t *fromlen)
{
	XDR xdr;
	uint32_t n;
	int32_t test, b64len, status;
	ni_index where;
	ni_proplist *pl;
	char *b64;
	struct in_addr addr4;
	struct sockaddr_in sin4;
	struct in6_addr addr6;
	struct sockaddr_in6 sin6;

	if (reply == NULL) return -1;
	if (rlen == 0) return -1;

	xdrmem_create(&xdr, reply, rlen, XDR_DECODE);

	if (!xdr_int(&xdr, &n))
	{
		xdr_destroy(&xdr);
		return -1;
	}


	if (n != 1)
	{
		xdr_destroy(&xdr);
		return -1;
	}

	pl = _lookupd_xdr_dictionary(&xdr);
	xdr_destroy(&xdr);

	if (pl == NULL) return -1;

	where = ni_proplist_match(*pl, "buffer", NULL);
	if (where == NI_INDEX_NULL)
	{
		ni_proplist_free(pl);
		return -1;
	}

 	if (pl->ni_proplist_val[where].nip_val.ni_namelist_len == 0) 
	{
		ni_proplist_free(pl);
		return -1;
	}

	b64 = pl->ni_proplist_val[where].nip_val.ni_namelist_val[0];
	b64len = strlen(b64);
	*buf = calloc(1, b64len);

	test = b64_pton(b64, *buf, b64len);
	if (test < 0)
	{
		free(*buf);
		ni_proplist_free(pl);
		return -1;
	}
	*len = test;

	*from = NULL;
	where = ni_proplist_match(*pl, "server", NULL);

	if (where == NI_INDEX_NULL)
	{
		ni_proplist_free(pl);
		return 0;
	}

 	if (pl->ni_proplist_val[where].nip_val.ni_namelist_len == 0) 
	{
		ni_proplist_free(pl);
		return 0;
	}

	memset(&addr4, 0, sizeof(struct in_addr));
	memset(&sin4, 0, sizeof(struct sockaddr_in));

	memset(&addr6, 0, sizeof(struct in6_addr));
	memset(&sin6, 0, sizeof(struct sockaddr_in6));

	status = inet_pton(AF_INET6, pl->ni_proplist_val[where].nip_val.ni_namelist_val[0], &addr6);
	if (status == 1)
	{
		sin6.sin6_addr = addr6;
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		*from = (struct sockaddr *)calloc(1, sin6.sin6_len);
		memcpy(*from, &sin6, sin6.sin6_len);
		*fromlen = sin6.sin6_len;
		return 0;
	}

	status = inet_pton(AF_INET, pl->ni_proplist_val[where].nip_val.ni_namelist_val[0], &addr4);
	if (status == 1)
	{
		sin4.sin_addr = addr4;
		sin4.sin_family = AF_INET;
		sin4.sin_len = sizeof(struct sockaddr_in);
		*from = (struct sockaddr *)calloc(1, sin4.sin_len);
		memcpy(*from, &sin4, sin4.sin_len);
		*fromlen = sin4.sin_len;
		return 0;
	}

	return -1;
}

int32_t
dns_async_receive(mach_port_t p, char **buf, uint32_t *len, struct sockaddr **from, uint32_t *fromlen)
{
	kern_return_t status;
	char *reply;
	uint32_t rlen;

	reply = NULL;
	rlen = 0;

	status = lu_async_receive(p, &reply, &rlen);
	if (status != KERN_SUCCESS) return NO_RECOVERY;

	status = dns_extract_data(reply, rlen, buf, len, from, fromlen);
	vm_deallocate(mach_task_self(), (vm_address_t)reply, rlen);
	if (status != KERN_SUCCESS) return NO_RECOVERY;

	if (*buf == NULL) return NO_DATA;

	return 0;
}

int32_t
dns_async_handle_reply(void *msg)
{
	dns_async_callback callback;
	void *context;
	char *reply, *buf;
	uint32_t rlen;
	kern_return_t status;
	struct sockaddr *from;
	uint32_t len, fromlen;

	callback = (dns_async_callback)NULL;
	context = NULL;
	reply = NULL;
	rlen = 0;
	buf = NULL;
	len = 0;
	from = NULL;
	fromlen = 0;

	status = lu_async_handle_reply(msg, &reply, &rlen, (void **)&callback, &context);
	if (status != KERN_SUCCESS)
	{
		if (status == MIG_REPLY_MISMATCH) return 0;
		callback(NO_RECOVERY, NULL, 0, NULL, 0, context);
		return NO_RECOVERY;
	}

	status = dns_extract_data(reply, rlen, &buf, &len, &from, &fromlen);
	vm_deallocate(mach_task_self(), (vm_address_t)reply, rlen);
	if (status != KERN_SUCCESS)
	{
		callback(NO_RECOVERY, NULL, 0, NULL, 0, context);
		return NULL;
	}

	if (buf == NULL)
	{
		callback(NO_DATA, NULL, 0, NULL, 0, context);
		return NO_DATA;
	}

	callback(0, buf, len, from, fromlen, context);

	return 0;
}

