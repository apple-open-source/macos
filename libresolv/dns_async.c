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
#include <netdb.h>
#include <netdb_async.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dns.h>
#include <dns_util.h>

typedef struct
{
	uint32_t datalen;
	char *databuf;
	uint32_t _size;
	uint32_t _dict;
	uint32_t _key;
	uint32_t _vlist;
	uint32_t _val;
} kvbuf_t;

typedef struct
{
	uint32_t kcount;
	const char **key;
	uint32_t *vcount;
	const char ***val;
} kvdict_t;

typedef struct
{
	uint32_t count;
	uint32_t curr;
	kvdict_t *dict;
	kvbuf_t *kv;
} kvarray_t;

extern kvbuf_t *kvbuf_new(void);
extern void kvbuf_add_dict(kvbuf_t *kv);
extern void kvbuf_add_key(kvbuf_t *kv, const char *key);
extern void kvbuf_add_val(kvbuf_t *kv, const char *val);
extern void kvbuf_free(kvbuf_t *kv);
extern void kvarray_free(kvarray_t *kva);
extern uint32_t kvbuf_get_val_len(const char *val);
extern kern_return_t LI_DSLookupGetProcedureNumber(const char *name, int *procno);
extern kern_return_t LI_async_start(mach_port_t *p, uint32_t proc, kvbuf_t *query, void *callback, void *context);
extern kern_return_t LI_async_handle_reply(void *msg, kvarray_t **reply, void **callback, void **context);
extern kern_return_t LI_async_receive(mach_port_t p, kvarray_t **reply);
extern void LI_async_call_cancel(mach_port_t p, void **context);
extern uint32_t kvbuf_get_len(const char *p);

static kvbuf_t *
dns_make_query(const char *name, uint16_t dnsclass, uint16_t dnstype, uint32_t do_search)
{
	kvbuf_t *request;
	char str[128];

	if (name == NULL) return NULL;

	request = kvbuf_new();
	if (request == NULL) return NULL;
	
	kvbuf_add_dict(request);
	
	/* Encode name */
	kvbuf_add_key(request, "domain");
	kvbuf_add_val(request, name);

	/* Encode class */
	snprintf(str, 128, "%hu", dnsclass);
	kvbuf_add_key(request, "class");
	kvbuf_add_val(request, str);

	/* Encode type */
	snprintf(str, 128, "%hu", dnstype);
	kvbuf_add_key(request, "type");
	kvbuf_add_val(request, str);

	/* Encode do_search */
	snprintf(str, 128, "%hu", do_search);
	kvbuf_add_key(request, "search");
	kvbuf_add_val(request, str);

	return request;
}

int32_t
dns_async_start(mach_port_t *p, const char *name, uint16_t dnsclass, uint16_t dnstype, uint32_t do_search, dns_async_callback callback, void *context)
{
	int32_t status;
	kvbuf_t *request;
	static int proc = -1;

	*p = MACH_PORT_NULL;

	if (name == NULL) return NO_RECOVERY;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("dns_proxy", &proc);
		if (status != KERN_SUCCESS) return NO_RECOVERY;
	}

	request = dns_make_query(name, dnsclass, dnstype, do_search);
	if (request == NULL) return NO_RECOVERY;

	status = LI_async_start(p, proc, request, (void *)callback, context);
	
	kvbuf_free(request);
	if (status != 0) return NO_RECOVERY;
	return 0;
}

void
dns_async_cancel(mach_port_t p)
{
	LI_async_call_cancel(p, NULL);
}

int32_t
dns_async_send(mach_port_t *p, const char *name, uint16_t dnsclass, uint16_t dnstype, uint32_t do_search)
{
	return dns_async_start(p, name, dnsclass, dnstype, do_search, NULL, NULL);
}

static int
dns_extract_data(kvarray_t *in, char **buf, uint32_t *len, struct sockaddr **from, uint32_t *fromlen)
{
	int32_t status;
	struct in_addr addr4;
	struct sockaddr_in sin4;
	struct in6_addr addr6;
	struct sockaddr_in6 sin6;
	uint32_t d, k, kcount;
	
	if (in == NULL) return -1;
	if (buf == NULL) return -1;
	if (len == NULL) return -1;
	if (from == NULL) return -1;
	if (fromlen == NULL) return -1;
	
	*buf = NULL;
	*len = 0;
	*from = NULL;
	*fromlen = 0;
	
	d = in->curr;
	in->curr++;
	
	if (d >= in->count) return -1;
	
	kcount = in->dict[d].kcount;
	
	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "data"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			if (*buf != NULL) continue;

			/*
			 * dns_proxy contains binary data, possibly with embedded nuls,
			 * so we extract the string length from the kvbuf_t reply that
			 * Libinfo got from directory services, rather than calling strlen().
			 */
			*len = kvbuf_get_len(in->dict[d].val[k][0]);
			if (*len == 0) continue;

			*buf = malloc(*len);
			if (*buf == NULL) return -1;

			memcpy(*buf, in->dict[d].val[k][0], *len);
		}
		else if (!strcmp(in->dict[d].key[k], "server"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			if (*from != NULL) continue;

			memset(&addr4, 0, sizeof(struct in_addr));
			memset(&sin4, 0, sizeof(struct sockaddr_in));
			
			memset(&addr6, 0, sizeof(struct in6_addr));
			memset(&sin6, 0, sizeof(struct sockaddr_in6));
			
			status = inet_pton(AF_INET6, in->dict[d].val[k][0], &addr6);
			if (status == 1)
			{
				sin6.sin6_addr = addr6;
				sin6.sin6_family = AF_INET6;
				sin6.sin6_len = sizeof(struct sockaddr_in6);
				*from = (struct sockaddr *)calloc(1, sin6.sin6_len);
				memcpy(*from, &sin6, sin6.sin6_len);
				*fromlen = sin6.sin6_len;
			}

			status = inet_pton(AF_INET, in->dict[d].val[k][0], &addr4);
			if (status == 1)
			{
				sin4.sin_addr = addr4;
				sin4.sin_family = AF_INET;
				sin4.sin_len = sizeof(struct sockaddr_in);
				*from = (struct sockaddr *)calloc(1, sin4.sin_len);
				memcpy(*from, &sin4, sin4.sin_len);
				*fromlen = sin4.sin_len;
			}
		}
	}
			
	return 0;
}

int32_t
dns_async_receive(mach_port_t p, char **buf, uint32_t *len, struct sockaddr **from, uint32_t *fromlen)
{
	kern_return_t status;
	kvarray_t *reply;

	reply = NULL;
	
	status = LI_async_receive(p, &reply);
	if (status != 0) return NO_RECOVERY;
	if (reply == NULL) return HOST_NOT_FOUND;

	status = dns_extract_data(reply, buf, len, from, fromlen);
	kvarray_free(reply);
	if (status != 0) return NO_RECOVERY;

	if (*buf == NULL) return NO_DATA;

	return 0;
}

int32_t
dns_async_handle_reply(void *msg)
{
	dns_async_callback callback;
	void *context;
	char *buf;
	kvarray_t *reply;
	kern_return_t status;
	struct sockaddr *from;
	uint32_t len, fromlen;

	callback = (dns_async_callback)NULL;
	context = NULL;
	reply = NULL;
	buf = NULL;
	len = 0;
	from = NULL;
	fromlen = 0;

	status = LI_async_handle_reply(msg, &reply, (void **)&callback, &context);
	if (status != KERN_SUCCESS)
	{
		if (status == MIG_REPLY_MISMATCH) return 0;
		callback(NO_RECOVERY, NULL, 0, NULL, 0, context);
		return NO_RECOVERY;
	}

	status = dns_extract_data(reply, &buf, &len, &from, &fromlen);
	kvarray_free(reply);
	if (status != 0)
	{
		callback(NO_RECOVERY, NULL, 0, NULL, 0, context);
		return 0;
	}

	if (buf == NULL)
	{
		callback(NO_DATA, NULL, 0, NULL, 0, context);
		return NO_DATA;
	}

	callback(0, buf, len, from, fromlen, context);

	return 0;
}

