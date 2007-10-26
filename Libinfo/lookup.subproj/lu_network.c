/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
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
 * network lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "lu_utils.h"

static pthread_mutex_t _network_lock = PTHREAD_MUTEX_INITIALIZER;

#define N_GET_NAME 1
#define N_GET_ADDR 2
#define N_GET_ENT 3

extern struct netent *_res_getnetbyaddr();
extern struct netent *_res_getnetbyname();
extern struct netent *_old_getnetbyaddr();
extern struct netent *_old_getnetbyname();
extern struct netent *_old_getnetent();
extern void _old_setnetent();
extern void _old_endnetent();

#define ENTRY_SIZE sizeof(struct netent)
#define ENTRY_KEY _li_data_key_network

static struct netent *
copy_network(struct netent *in)
{
	if (in == NULL) return NULL;

	return (struct netent *)LI_ils_create("s*44", in->n_name, in->n_aliases, in->n_addrtype, in->n_net);
}

/*
 * Extract the next network entry from a kvarray.
 */
static void *
extract_network(kvarray_t *in)
{
	struct netent tmp;
	uint32_t d, k, kcount;
	char *empty[1];

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, ENTRY_SIZE);

	tmp.n_addrtype = AF_INET;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "n_name"))
		{
			if (tmp.n_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.n_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "n_net"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.n_net = inet_network(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "n_addrtype"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.n_addrtype = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "n_aliases"))
		{
			if (tmp.n_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.n_aliases = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.n_name == NULL) tmp.n_name = "";
	if (tmp.n_aliases == NULL) tmp.n_aliases = empty;

	return copy_network(&tmp);
}

static struct netent *
ds_getnetbyaddr(uint32_t addr, int type)
{
	static int proc = -1;
	unsigned char f1, f2, f3;
	char val[64];

	if (type != AF_INET) return NULL;

	f1 = addr & 0xff;
	addr >>= 8;
	f2 = addr & 0xff;
	addr >>= 8;
	f3 = addr & 0xff;

	if (f3 != 0) snprintf(val, sizeof(val), "%u.%u.%u", f3, f2, f1);
	else if (f2 != 0) snprintf(val, sizeof(val), "%u.%u", f2, f1);
	else snprintf(val, sizeof(val), "%u", f1);

	return (struct netent *)LI_getone("getnetbyaddr", &proc, extract_network, "net", val);
}

static struct netent *
ds_getnetbyname(const char *name)
{
	static int proc = -1;

	return (struct netent *)LI_getone("getnetbyname", &proc, extract_network, "name", name);
}

static void
ds_endnetent()
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static void
ds_setnetent()
{
	ds_endnetent();
}

static struct netent *
ds_getnetent()
{
	static int proc = -1;

	return (struct netent *)LI_getent("getnetent", &proc, extract_network, ENTRY_KEY, ENTRY_SIZE);
}

static struct netent *
getnet(const char *name, uint32_t addr, int type, int source)
{
	struct netent *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	if (_ds_running())
	{
		switch (source)
		{
			case N_GET_NAME:
				res = ds_getnetbyname(name);
				break;
			case N_GET_ADDR:
				res = ds_getnetbyaddr(addr, type);
				break;
			case N_GET_ENT:
				res = ds_getnetent();
				break;
			default: res = NULL;
		}
	}
	else
	{
		pthread_mutex_lock(&_network_lock);

		switch (source)
		{
			case N_GET_NAME:
				res = copy_network(_old_getnetbyname(name));
				break;
			case N_GET_ADDR:
				res = copy_network(_old_getnetbyaddr(addr, type));
				break;
			case N_GET_ENT:
				res = copy_network(_old_getnetent());
				break;
			default: res = NULL;
		}

		pthread_mutex_unlock(&_network_lock);
	}

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct netent *)tdata->li_entry;
}

struct netent *
getnetbyaddr(uint32_t addr, int type)
{
	return getnet(NULL, addr, type, N_GET_ADDR);
}

struct netent *
getnetbyname(const char *name)
{
	return getnet(name, 0, 0, N_GET_NAME);
}

struct netent *
getnetent(void)
{
	return getnet(NULL, 0, 0, N_GET_ENT);
}

void
setnetent(int stayopen)
{
	if (_ds_running()) ds_setnetent();
	else _old_setnetent(stayopen);
}

void
endnetent(void)
{
	if (_ds_running()) ds_endnetent();
	else _old_endnetent();
}
