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
 * Netgroup lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <netdb.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lu_utils.h"
#include "lu_overrides.h"

struct li_netgrent
{
	char *ng_host;
	char *ng_user;
	char *ng_domain;
};

#define ENTRY_SIZE sizeof(struct li_netgrent)
#define ENTRY_KEY _li_data_key_netgroup

static struct li_netgrent *
copy_netgroup(struct li_netgrent *in)
{
	if (in == NULL) return NULL;

	return (struct li_netgrent *)LI_ils_create("sss", in->ng_host, in->ng_user, in->ng_domain);
}

/*
 * Extract the next netgroup entry from a kvarray.
 */
static struct li_netgrent *
extract_netgroup(kvarray_t *in)
{
	struct li_netgrent tmp;
	uint32_t d, k, kcount;

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	memset(&tmp, 0, ENTRY_SIZE);

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "user"))
		{
			if (tmp.ng_user != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.ng_user = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "host"))
		{
			if (tmp.ng_host != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.ng_host = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "domain"))
		{
			if (tmp.ng_domain != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.ng_domain = (char *)in->dict[d].val[k][0];
		}
	}

	if (tmp.ng_user == NULL) tmp.ng_user = "";
	if (tmp.ng_host == NULL) tmp.ng_host = "";
	if (tmp.ng_domain == NULL) tmp.ng_domain = "";

	return copy_netgroup(&tmp);
}

static int
check_innetgr(kvarray_t *in)
{
	uint32_t d, k, kcount;

	if (in == NULL) return 0;

	d = in->curr;
	if (d >= in->count) return 0;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "result"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			return atoi(in->dict[d].val[k][0]);
		}
	}

	return 0;
}

static int
ds_innetgr(const char *group, const char *host, const char *user, const char *domain)
{
	int is_innetgr;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;
	static int proc = -1;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("innetgr", &proc);
		if (status != KERN_SUCCESS) return 0;
	}

	/* Encode NULL */
	if (group == NULL) group = "";
	if (host == NULL) host = "";
	if (user == NULL) user = "";
	if (domain == NULL) domain = "";

	request = kvbuf_query("ksksksks", "netgroup", group, "host", host, "user", user, "domain", domain);
	if (request == NULL) return 0;

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS) return 0;

	is_innetgr = check_innetgr(reply);
	kvarray_free(reply);

	return is_innetgr;
}

static void
ds_endnetgrent(void)
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

/* 
 * This is different than the other setXXXent routines
 * since this is really more like getnetgrbyname() than
 * getnetgrent().
 */ 
static void
ds_setnetgrent(const char *name)
{
	struct li_thread_info *tdata;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;
	static int proc = -1;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return;

	if (tdata->li_vm != NULL) return;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("getnetgrent", &proc);
		if (status != KERN_SUCCESS)
		{
			LI_data_free_kvarray(tdata);
			return;
		}
	}

	request = kvbuf_query_key_val("netgroup", name);
	if (request == NULL) return;

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if (status != KERN_SUCCESS)
	{
		LI_data_free_kvarray(tdata);
		return;
	}

	tdata->li_vm = (char *)reply;
}


static struct li_netgrent *
ds_getnetgrent(void)
{
	struct li_netgrent *entry;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	entry = extract_netgroup((kvarray_t *)(tdata->li_vm));
	if (entry == NULL)
	{
		ds_endnetgrent();
		return NULL;
	}

	return entry;
}

int 
innetgr(const char *group, const char *host, const char *user,
	const char *domain)
{
	if (_ds_running()) return (ds_innetgr(group, host, user, domain));
	return 0;
}

int
getnetgrent(char **host, char **user, char **domain)
{
	struct li_netgrent *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return 0;

	res = NULL;
	if (_ds_running()) res = ds_getnetgrent();

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	if (res == NULL) return 0;

	if (host != NULL) *host = res->ng_host;
	if (user != NULL) *user = res->ng_user;
	if (domain != NULL) *domain = res->ng_domain;

	return 1;
}

void
setnetgrent(const char *name)
{
	if (_ds_running()) ds_setnetgrent(name);
}

void
endnetgrent(void)
{
	if (_ds_running()) ds_endnetgrent();
}
