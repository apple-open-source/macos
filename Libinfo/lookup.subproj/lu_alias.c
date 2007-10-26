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
 * Alias lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <aliasdb.h>
#include <pthread.h>
#include "lu_utils.h"
#include "lu_overrides.h"

#define ENTRY_SIZE sizeof(struct aliasent)
#define ENTRY_KEY _li_data_key_alias

static pthread_mutex_t _alias_lock = PTHREAD_MUTEX_INITIALIZER;

static struct aliasent *
copy_alias(struct aliasent *in)
{
	if (in == NULL) return NULL;

	return (struct aliasent *)LI_ils_create("s4*4", in->alias_name, in->alias_members_len, in->alias_members, in->alias_local);
}

/*
 * Extract the next alias entry from a kvarray.
 */
static void *
extract_alias(kvarray_t *in)
{
	struct aliasent tmp;
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
		if (!strcmp(in->dict[d].key[k], "alias_name"))
		{
			if (tmp.alias_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.alias_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "alias_members"))
		{
			if (tmp.alias_members != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.alias_members_len = in->dict[d].vcount[k];
			tmp.alias_members = (char **)in->dict[d].val[k];
		}
		else if (!strcmp(in->dict[d].key[k], "alias_local"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.alias_local = atoi(in->dict[d].val[k][0]);
		}
	}

	if (tmp.alias_name == NULL) tmp.alias_name = "";
	if (tmp.alias_members == NULL) tmp.alias_members = empty;

	return copy_alias(&tmp);
}

/*
 * Send a query to the system information daemon.
 */
static struct aliasent *
ds_alias_getbyname(const char *name)
{
	static int proc = -1;

	return (struct aliasent *)LI_getone("alias_getbyname", &proc, extract_alias, "name", name);
}

/*
 * Clean up / initialize / reinitialize the kvarray used to hold a list of all rpc entries.
 */
static void
ds_alias_endent(void)
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static void
ds_alias_setent(void)
{
	ds_alias_endent();
}

/*
 * Get an entry from the getrpcent kvarray.
 * Calls the system information daemon if the list doesn't exist (first call),
 * or extracts the next entry if the list has been fetched.
 */
static struct aliasent *
ds_alias_getent(void)
{
	static int proc = -1;

	return (struct aliasent *)LI_getent("alias_getent", &proc, extract_alias, ENTRY_KEY, ENTRY_SIZE);
}

struct aliasent *
alias_getbyname(const char *name)
{
	struct aliasent *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	if (_ds_running())
	{
		res = ds_alias_getbyname(name);
	}
	else
	{
		pthread_mutex_lock(&_alias_lock);
		res = copy_alias(_old_alias_getbyname(name));
		pthread_mutex_unlock(&_alias_lock);
	}

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct aliasent *)tdata->li_entry;

}

struct aliasent *
alias_getent(void)
{
	struct aliasent *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	if (_ds_running())
	{
		res = ds_alias_getent();
	}
	else
	{
		pthread_mutex_lock(&_alias_lock);
		res = copy_alias(_old_alias_getent());
		pthread_mutex_unlock(&_alias_lock);
	}

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct aliasent *)tdata->li_entry;

}

void
alias_setent(void)
{
	if (_ds_running()) ds_alias_setent();
	else _old_alias_setent();
}

void
alias_endent(void)
{
	if (_ds_running()) ds_alias_endent();
	else _old_alias_endent();
}
