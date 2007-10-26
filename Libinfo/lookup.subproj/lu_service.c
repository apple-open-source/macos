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
 * Services file lookup
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

#define SERVICE_CACHE_SIZE 10

static pthread_mutex_t _service_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static void *_service_cache[SERVICE_CACHE_SIZE] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static unsigned int _service_cache_index = 0;
static unsigned int _service_cache_init = 0;

static pthread_mutex_t _service_lock = PTHREAD_MUTEX_INITIALIZER;

#define S_GET_NAME 1
#define S_GET_PORT 2
#define S_GET_ENT 3

extern struct servent *_old_getservbyport();
extern struct servent *_old_getservbyname();
extern struct servent *_old_getservent();
extern void _old_setservent();
extern void _old_endservent();
extern void _old_setservfile();

#define ENTRY_SIZE sizeof(struct servent)
#define ENTRY_KEY _li_data_key_service

static struct servent *
copy_service(struct servent *in)
{
	if (in == NULL) return NULL;

	return LI_ils_create("s*4s", in->s_name, in->s_aliases, in->s_port, in->s_proto);
}

/*
 * Extract the next service entry from a kvarray.
 */
static void *
extract_service(kvarray_t *in)
{
	struct servent tmp;
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
		if (!strcmp(in->dict[d].key[k], "s_name"))
		{
			if (tmp.s_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.s_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "s_aliases"))
		{
			if (tmp.s_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.s_aliases = (char **)in->dict[d].val[k];
		}
		else if (!strcmp(in->dict[d].key[k], "s_port"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.s_port = htons(atoi(in->dict[d].val[k][0]));
		}
		else if (!strcmp(in->dict[d].key[k], "s_proto"))
		{
			if (tmp.s_proto != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.s_proto = (char *)in->dict[d].val[k][0];
		}
	}

	if (tmp.s_name == NULL) tmp.s_name = "";
	if (tmp.s_proto == NULL) tmp.s_proto = "";
	if (tmp.s_aliases == NULL) tmp.s_aliases = empty;

	return copy_service(&tmp);
}

static void
cache_service(struct servent *s)
{
	struct servent *scache;

	if (s == NULL) return;

	pthread_mutex_lock(&_service_cache_lock);

	scache = copy_service(s);
        
	if (_service_cache[_service_cache_index] != NULL) LI_ils_free(_service_cache[_service_cache_index], ENTRY_SIZE);
	_service_cache[_service_cache_index] = scache;
	_service_cache_index = (_service_cache_index + 1) % SERVICE_CACHE_SIZE;

	_service_cache_init = 1;

	pthread_mutex_unlock(&_service_cache_lock);
}

static int
service_cache_check()
{
	uint32_t i, status;

	/* don't consult cache if it has not been initialized */
	if (_service_cache_init == 0) return 1;

	status = LI_L1_cache_check(ENTRY_KEY);

	/* don't consult cache if it is disabled or if we can't validate */
	if ((status == LI_L1_CACHE_DISABLED) || (status == LI_L1_CACHE_FAILED)) return 1;

	/* return 0 if cache is OK */
	if (status == LI_L1_CACHE_OK) return 0;

	/* flush cache */
	pthread_mutex_lock(&_service_cache_lock);

	for (i = 0; i < SERVICE_CACHE_SIZE; i++)
	{
		LI_ils_free(_service_cache[i], ENTRY_SIZE);
		_service_cache[i] = NULL;
	}

	_service_cache_index = 0;

	pthread_mutex_unlock(&_service_cache_lock);

	/* don't consult cache - it's now empty */
	return 1;
}


static struct servent *
cache_getservbyname(const char *name, const char *proto)
{
	int i;
	struct servent *s, *res;
	char **aliases;

	if (name == NULL) return NULL;
	if (service_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_service_cache_lock);

	for (i = 0; i < SERVICE_CACHE_SIZE; i++)
	{
		s = (struct servent *)_service_cache[i];
		if (s == NULL) continue;

		if (s->s_name != NULL) 
		{
			if (!strcmp(name, s->s_name))
			{
				if ((proto == NULL) || ((s->s_proto != NULL) && (!strcmp(proto, s->s_proto))))
				{
					res = copy_service(s);
					pthread_mutex_unlock(&_service_cache_lock);
					return res;
				}
			}
		}

		aliases = s->s_aliases;
		if (aliases == NULL)
		{
			pthread_mutex_unlock(&_service_cache_lock);
			return NULL;
		}

		for (; *aliases != NULL; *aliases++)
		{
			if (!strcmp(name, *aliases))
			{
				if ((proto == NULL) || ((s->s_proto != NULL) && (!strcmp(proto, s->s_proto))))
				{
					res = copy_service(s);
					pthread_mutex_unlock(&_service_cache_lock);
					return res;
				}
			}
		}
	}

	pthread_mutex_unlock(&_service_cache_lock);
	return NULL;
}

static struct servent *
cache_getservbyport(int port, const char *proto)
{
	int i;
	struct servent *s, *res;

	if (service_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_service_cache_lock);

	for (i = 0; i < SERVICE_CACHE_SIZE; i++)
	{
		s = (struct servent *)_service_cache[i];
		if (s == NULL) continue;

		if (port == s->s_port)
		{
			if ((proto == NULL) || ((s->s_proto != NULL) && (!strcmp(proto, s->s_proto))))
			{
				res = copy_service(s);
				pthread_mutex_unlock(&_service_cache_lock);
				return res;
			}
		}
	}

	pthread_mutex_unlock(&_service_cache_lock);
	return NULL;
}

static struct servent *
ds_getservbyport(int port, const char *proto)
{
	struct servent *entry;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;
	static int proc = -1;
	uint16_t sport;
	char val[16];

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getservbyport", &proc);
		if (status != KERN_SUCCESS) return NULL;
	}

	/* Encode NULL */
	if (proto == NULL) proto = "";

	sport = port;
	snprintf(val, sizeof(val), "%d", ntohs(sport));

	request = kvbuf_query("ksks", "port", val, "proto", proto);
	if (request == NULL) return NULL;

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS) return NULL;

	entry = extract_service(reply);
	kvarray_free(reply);

	return entry;
}

static struct servent *
ds_getservbyname(const char *name, const char *proto)
{
	struct servent *entry;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;
	static int proc = -1;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getservbyname", &proc);
		if (status != KERN_SUCCESS) return NULL;
	}

	/* Encode NULL */
	if (name == NULL) name = "";
	if (proto == NULL) proto = "";

	request = kvbuf_query("ksks", "name", name, "proto", proto);
	if (request == NULL) return NULL;

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS) return NULL;

	entry = extract_service(reply);
	kvarray_free(reply);

	return entry;
}

static void
ds_endservent()
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static void
ds_setservent()
{
	ds_endservent();
}

static struct servent *
ds_getservent()
{
	static int proc = -1;

	return (struct servent *)LI_getent("getservent", &proc, extract_service, ENTRY_KEY, ENTRY_SIZE);
}

static struct servent *
getserv(const char *name, const char *proto, int port, int source)
{
	struct servent *res = NULL;
	struct li_thread_info *tdata;
	int add_to_cache;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	add_to_cache = 0;
	res = NULL;

	switch (source)
	{
		case S_GET_NAME:
			res = cache_getservbyname(name, proto);
			break;
		case S_GET_PORT:
			res = cache_getservbyport(port, proto);
			break;
		default: res = NULL;
	}

	if (res != NULL)
	{
	}
	else if (_ds_running())
	{
		switch (source)
		{
			case S_GET_NAME:
				res = ds_getservbyname(name, proto);
				break;
			case S_GET_PORT:
				res = ds_getservbyport(port, proto);
				break;
			case S_GET_ENT:
				res = ds_getservent();
				break;
			default: res = NULL;
		}

		if (res != NULL) add_to_cache = 1;
	}
	else
	{
		pthread_mutex_lock(&_service_lock);
		switch (source)
		{
			case S_GET_NAME:
				res = copy_service(_old_getservbyname(name, proto));
				break;
			case S_GET_PORT:
				res = copy_service(_old_getservbyport(port, proto));
				break;
			case S_GET_ENT:
				res = copy_service(_old_getservent());
				break;
			default: res = NULL;
		}
		pthread_mutex_unlock(&_service_lock);
	}

	if (add_to_cache == 1) cache_service(res);

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct servent *)tdata->li_entry;
}

struct servent *
getservbyport(int port, const char *proto)
{
	return getserv(NULL, proto, port, S_GET_PORT);
}

struct servent *
getservbyname(const char *name, const char *proto)
{
	return getserv(name, proto, 0, S_GET_NAME);
}

struct servent *
getservent(void)
{
	return getserv(NULL, NULL, 0, S_GET_ENT);
}

void
setservent(int stayopen)
{
	if (_ds_running()) ds_setservent();
	else _old_setservent();
}

void
endservent(void)
{
	if (_ds_running()) ds_endservent();
	else _old_endservent();
}
