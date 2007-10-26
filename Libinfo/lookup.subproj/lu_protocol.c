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
/*
 * Protocol lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include "lu_utils.h"

extern struct protoent *_old_getprotobynumber();
extern struct protoent *_old_getprotobyname();
extern struct protoent *_old_getprotoent();
extern void _old_setprotoent();
extern void _old_endprotoent();

static pthread_mutex_t _protocol_lock = PTHREAD_MUTEX_INITIALIZER;

#define PROTO_GET_NAME 1
#define PROTO_GET_NUM 2
#define PROTO_GET_ENT 3

#define ENTRY_SIZE sizeof(struct protoent)
#define ENTRY_KEY _li_data_key_protocol

static struct protoent *
copy_protocol(struct protoent *in)
{
	if (in == NULL) return NULL;

	return (struct protoent *)LI_ils_create("s*4", in->p_name, in->p_aliases, in->p_proto);
}

/*
 * Extract the next protocol entry from a kvarray.
 */
static void *
extract_protocol(kvarray_t *in)
{
	struct protoent tmp;
	uint32_t d, k, kcount;
	char *empty[1];

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, ENTRY_SIZE);

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "p_name"))
		{
			if (tmp.p_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.p_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "p_proto"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.p_proto = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "p_aliases"))
		{
			if (tmp.p_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.p_aliases = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.p_name == NULL) tmp.p_name = "";
	if (tmp.p_aliases == NULL) tmp.p_aliases = empty;

	return copy_protocol(&tmp);
}

/*
 * Send a query to the system information daemon.
 */
static struct protoent *
ds_getprotobynumber(uint32_t number)
{
	static int proc = -1;
	char val[16];

	snprintf(val, sizeof(val), "%u", number);
	return (struct protoent *)LI_getone("getprotobynumber", &proc, extract_protocol, "number", val);
}

/*
 * Send a query to the system information daemon.
 */
static struct protoent *
ds_getprotobyname(const char *name)
{
	static int proc = -1;

	return (struct protoent *)LI_getone("getprotobyname", &proc, extract_protocol, "name", name);
}

/*
 * Clean up / initialize / reinitialize the kvarray used to hold a list of all protocol entries.
 */
static void
ds_endprotoent()
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static void
ds_setprotoent()
{
	ds_endprotoent();
}

static struct protoent *
ds_getprotoent()
{
	static int proc = -1;

	return (struct protoent *)LI_getent("getprotoent", &proc, extract_protocol, ENTRY_KEY, ENTRY_SIZE);
}

static struct protoent *
getproto(const char *name, int number, int source)
{
	struct protoent *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	if (_ds_running())
	{
		switch (source)
		{
			case PROTO_GET_NAME:
				res = ds_getprotobyname(name);
				break;
			case PROTO_GET_NUM:
				res = ds_getprotobynumber(number);
				break;
			case PROTO_GET_ENT:
				res = ds_getprotoent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_protocol_lock);

		switch (source)
		{
			case PROTO_GET_NAME:
				res = copy_protocol(_old_getprotobyname(name));
				break;
			case PROTO_GET_NUM:
				res = copy_protocol(_old_getprotobynumber(number));
				break;
			case PROTO_GET_ENT:
				res = copy_protocol(_old_getprotoent());
				break;
			default: res = NULL;
		}

		pthread_mutex_unlock(&_protocol_lock);
	}

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct protoent *)tdata->li_entry;
}

struct protoent *
getprotobyname(const char *name)
{
	return getproto(name, -2, PROTO_GET_NAME);
}

struct protoent *
getprotobynumber(int number)
{
	return getproto(NULL, number, PROTO_GET_NUM);
}

struct protoent *
getprotoent(void)
{
	return getproto(NULL, -2, PROTO_GET_ENT);
}

void
setprotoent(int stayopen)
{
	if (_ds_running()) ds_setprotoent();
	else _old_setprotoent(stayopen);
}

void
endprotoent(void)
{
	if (_ds_running()) ds_endprotoent();
	else _old_endprotoent();
}
