/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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
 * RPC lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <rpc/rpc.h>
#include <netdb.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <pthread.h>
#include "lu_utils.h"
#include "lu_overrides.h"

static pthread_mutex_t _rpc_lock = PTHREAD_MUTEX_INITIALIZER;

#define RPC_GET_NAME 1
#define RPC_GET_NUM 2
#define RPC_GET_ENT 3

#define ENTRY_SIZE sizeof(struct rpcent)
#define ENTRY_KEY _li_data_key_rpc

static struct rpcent *
copy_rpc(struct rpcent *in)
{
	if (in == NULL) return NULL;

	return (struct rpcent *)LI_ils_create("s*4", in->r_name, in->r_aliases, in->r_number);
}

/*
 * Extract the next rpc entry from a kvarray.
 */
static void *
extract_rpc(kvarray_t *in)
{
	struct rpcent tmp;
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
		if (!strcmp(in->dict[d].key[k], "r_name"))
		{
			if (tmp.r_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.r_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "r_number"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.r_number = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "r_aliases"))
		{
			if (tmp.r_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.r_aliases = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.r_name == NULL) tmp.r_name = "";
	if (tmp.r_aliases == NULL) tmp.r_aliases = empty;

	return copy_rpc(&tmp);
}

/*
 * Send a query to the system information daemon.
 */
static struct rpcent *
ds_getrpcbynumber(uint32_t number)
{
	static int proc = -1;
	char val[16];

	snprintf(val, sizeof(val), "%u", number);
	return (struct rpcent *)LI_getone("getrpcbynumber", &proc, extract_rpc, "number", val);
}

/*
 * Send a query to the system information daemon.
 */
static struct rpcent *
ds_getrpcbyname(const char *name)
{
	static int proc = -1;

	return (struct rpcent *)LI_getone("getrpcbyname", &proc, extract_rpc, "name", name);
}

/*
 * Clean up / initialize / reinitialize the kvarray used to hold a list of all rpc entries.
 */
static void
ds_endrpcent(void)
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static void
ds_setrpcent(void)
{
	ds_endrpcent();
}

/*
 * Get an entry from the getrpcent kvarray.
 * Calls the system information daemon if the list doesn't exist (first call),
 * or extracts the next entry if the list has been fetched.
 */
static struct rpcent *
ds_getrpcent(void)
{
	static int proc = -1;

	return (struct rpcent *)LI_getent("getrpcent", &proc, extract_rpc, ENTRY_KEY, ENTRY_SIZE);
}

/*
 * Checks if the system information daemon is running.
 * If so, calls the appropriate fetch routine.
 * If not, calls the appropriate "_old" routine.
 * Places the result in thread-specific memory.
 */
static struct rpcent *
getrpc(const char *name, uint32_t number, int source)
{
	struct rpcent *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	if (_ds_running())
	{
		switch (source)
		{
			case RPC_GET_NAME:
				res = ds_getrpcbyname(name);
				break;
			case RPC_GET_NUM:
				res = ds_getrpcbynumber(number);
				break;
			case RPC_GET_ENT:
				res = ds_getrpcent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_rpc_lock);

		switch (source)
		{
			case RPC_GET_NAME:
				res = copy_rpc(_old_getrpcbyname(name));
				break;
			case RPC_GET_NUM:
				res = copy_rpc(_old_getrpcbynumber(number));
				break;
			case RPC_GET_ENT:
				res = copy_rpc(_old_getrpcent());
				break;
			default: res = NULL;
		}

		pthread_mutex_unlock(&_rpc_lock);
	}

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct rpcent *)tdata->li_entry;
}

struct rpcent *
getrpcbyname(const char *name)
{
	return getrpc(name, -2, RPC_GET_NAME);
}

struct rpcent *
#ifdef __LP64__
getrpcbynumber(int number)
#else
getrpcbynumber(long number)
#endif
{
	uint32_t n;

	n = number;
	return getrpc(NULL, n, RPC_GET_NUM);
}

struct rpcent *
getrpcent(void)
{
	return getrpc(NULL, -2, RPC_GET_ENT);
}

void
setrpcent(int stayopen)
{
	if (_ds_running()) ds_setrpcent();
	else _old_setrpcent(stayopen);
}

void
endrpcent(void)
{
	if (_ds_running()) ds_endrpcent();
	else _old_endrpcent();
}
