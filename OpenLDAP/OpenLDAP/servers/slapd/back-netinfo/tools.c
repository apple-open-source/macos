/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "portable.h"

#include <stdio.h>

#include <ac/unistd.h>
#include <ac/socket.h>
#include <ac/string.h>

#include <lutil.h>

#include "slap.h"
#include "back-netinfo.h"

static u_int32_t cursor = NOID;

int netinfo_tool_entry_open(BackendDB *be, int mode)
{
	struct dsinfo *di = ((struct dsinfo *)be->be_private);

	assert(slapMode & SLAP_TOOL_MODE);

	assert(di != NULL);

	if (di->engine == NULL)
		return 1;

	return 0;
}

int netinfo_tool_entry_close(BackendDB *be)
{
	cursor = NOID;

	return 0;
}

ID netinfo_tool_entry_next(BackendDB *be)
{
	struct dsinfo *di = ((struct dsinfo *)be->be_private);
	dsstore *store;
	u_int32_t max;

	assert(slapMode & SLAP_TOOL_MODE);

	assert(di != NULL);
	assert(di->engine != NULL);

	store = di->engine->store;

	max = dsstore_max_id(store);

	do
	{
		cursor++;

		if (dsstore_record_version(store, cursor) != IndexNull)
			return (ID)cursor;
	}
	while (cursor < max);

	return NOID;
}

Entry *netinfo_tool_entry_get(BackendDB *be, ID id)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsrecord *rec;
	Entry *ent = NULL;

	assert(slapMode & SLAP_TOOL_MODE);

	assert(di != NULL);
	assert(di->engine != NULL);

	if (dsengine_fetch(di->engine, (u_int32_t)id, &rec) != DSStatusOK)
		return NULL;

	if (dsrecord_to_entry(be, rec, &ent) != DSStatusOK)
		ent = NULL;

	/*
	 * Entries retain a reference to the dsrecord,
	 * however slapcat uses the entry release callback
	 * which will call dsrecord_release().
	 */
	dsrecord_release(rec);

	return ent;
}

ID netinfo_tool_entry_put(BackendDB *be, Entry *e, struct berval *text)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsrecord *rec;
	dsstatus status;
	u_int32_t dsid;
	struct berval parentNDN;

	assert(slapMode & SLAP_TOOL_MODE);

	assert(di != NULL);
	assert(di->engine != NULL);

	assert(text);
	assert(text->bv_val);
	assert(text->bv_val[0] == '\0');

	dnParent(&e->e_nname, &parentNDN);

	status = netinfo_back_dn_pathcreate(be, &parentNDN, &dsid);
	if (status != DSStatusOK)
	{
		snprintf(text->bv_val, text->bv_len, "unable to get parent: %s",
			dsstatus_message(status));
		return NOID;
	}

	status = entry_to_dsrecord(be, dsid, e, &rec);
	if (status != DSStatusOK)
	{
		snprintf(text->bv_val, text->bv_len, "unable to parse entry: %s",
			dsstatus_message(status));
		return NOID;
	}

	status = dsengine_create(di->engine, rec, dsid);
	if (status != DSStatusOK)
	{
		snprintf(text->bv_val, text->bv_len, "unable to create entry: %s",
			dsstatus_message(status));
		dsrecord_release(rec);
		return NOID;
	}

	dsid = rec->dsid;

	dsrecord_release(rec);

	return (ID)dsid;
}

int netinfo_tool_entry_reindex(BackendDB *be, ID id)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(slapMode & SLAP_TOOL_MODE);

	assert(di != NULL);
	assert(di->engine != NULL);

	return 0;
}

int netinfo_tool_sync(BackendDB *be)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(slapMode & SLAP_TOOL_MODE);

	assert(di != NULL);
	assert(di->engine != NULL);

	dsstore_flush_cache(di->engine->store);

	return 0;
}

