/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

/*
 * System Info Agent
 * Written by Marc Majka
 */

#include <stdio.h>
#include <string.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/dsengine.h>
#include <NetInfo/syslock.h>
#include <NetInfo/system_log.h>
#include <NetInfo/DynaAPI.h>

#define DEFAULT_STORE_DIR "/var/db/sysinfo"

typedef struct
{
	dsengine *engine;
	syslock *threadLock;
	dynainfo *dyna;
} agent_private;

static char *pathForCategory[] =
{
	"/users",
	"/groups",
	"/machines",
	"/networks",
	"/services",
	"/protocols",
	"/rpcs",
	"/mounts",
	"/printers",
	"/machines",
	"/machines",
	"/aliases",
	"/netdomains",
	NULL,
	NULL,
	NULL,
	NULL
};

static dsengine *
SI_open(char *store)
{
	dsstatus status;
	char *path;
	int flags;
	dsengine *engine;

	path = NULL;
	if (store[0] == '/')
	{
		path = copyString(store);
	}
	else
	{
		path = malloc(strlen(DEFAULT_STORE_DIR) + strlen(store) + 2);
		sprintf(path, "%s/%s", DEFAULT_STORE_DIR, store);
	}

	flags = 0;

	flags |= DSSTORE_FLAGS_ACCESS_READONLY;
	flags |= DSENGINE_FLAGS_NETINFO_NAMING;

	status = dsengine_open(&engine, path, flags);
	if (status != DSStatusOK)
	{
		system_log(LOG_ERR, "SysInfo open datastore %s failed: %s", path, dsstatus_message(status));
		free(path);
		return NULL;
	}

	free(path);
	return engine;
}

u_int32_t
SI_new(void **c, char *args, dynainfo *d)
{
	agent_private *ap;

	if (c == NULL) return 1;
	ap = (agent_private *)malloc(sizeof(agent_private));
	*c = ap;

	if (args == NULL) ap->engine = SI_open("local");
	else ap->engine = SI_open(args);

	if (ap->engine == NULL)
	{
		free(ap);
		*c = NULL;
		return 1;
	}

	ap->threadLock = syslock_new(0);
	ap->dyna = d;

	system_log(LOG_DEBUG, "Allocated SI 0x%08x\n", (int)ap);

	return 0;
}

u_int32_t
SI_free(void *c)
{
	agent_private *ap;

	if (c == NULL) return 0;

	ap = (agent_private *)c;

	if (ap->engine != NULL) dsengine_close(ap->engine);
	ap->engine = NULL;

	syslock_free(ap->threadLock);
	ap->threadLock = NULL;

	system_log(LOG_DEBUG, "Deallocated SI 0x%08x\n", (int)ap);

	free(ap);
	c = NULL;

	return 0;
}

static void
add_validation(dsrecord *r)
{
	char str[64];
	dsdata *d;
	dsattribute *a;

	if (r == NULL) return;

	d = cstring_to_dsdata("lookup_validation");
	dsrecord_remove_key(r, d, SELECT_META_ATTRIBUTE);

	a = dsattribute_new(d);
	dsrecord_append_attribute(r, a, SELECT_META_ATTRIBUTE);

	dsdata_release(d);

	sprintf(str, "%lu %lu %lu", (unsigned long)r->dsid, (unsigned long)r->serial, (unsigned long)r->vers);

	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);

	dsattribute_release(a);
}

u_int32_t
SI_query(void *c, dsrecord *pattern, dsrecord **list)
{
	dsstatus status;
	agent_private *ap;
	char *path, *str, *catname;
	u_int32_t pdsid;
	u_int32_t *match, count, i, cat;
	dsattribute *a;
	dsdata *k;
	dsrecord **lp;
	int single_item, stamp;

	if (c == NULL) return 1;
	if (pattern == NULL) return 1;
	if (list == NULL) return 1;

	*list = NULL;
	single_item = 0;
	stamp = 0;

	ap = (agent_private *)c;

	k = cstring_to_dsdata(CATEGORY_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);

	if (a == NULL) return 1;
	if (a->count == 0) return 1;
	dsrecord_remove_attribute(pattern, a, SELECT_META_ATTRIBUTE);

	catname = dsdata_to_cstring(a->value[0]);
	if (catname == NULL) return 1;

	str = NULL;
	if (catname[0] == '/')
	{
		cat = -1;
		str = catname;
	}
	else
	{
		cat = atoi(catname);

		str = pathForCategory[cat];
		if (str == NULL)
		{
			dsattribute_release(a);
			return 1;
		}
	}

	path = strdup(str);
	dsattribute_release(a);

	k = cstring_to_dsdata(STAMP_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);
	if (a != NULL)
	{
		dsrecord_remove_attribute(pattern, a, SELECT_META_ATTRIBUTE);
		stamp = 1;
	}
	dsattribute_release(a);

	if (stamp == 1)
	{
		*list = dsrecord_new();
		add_validation(*list);
		free(path);
		return 0;
	}

	k = cstring_to_dsdata(SINGLE_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);
	if (a != NULL)
	{
		dsrecord_remove_attribute(pattern, a, SELECT_META_ATTRIBUTE);
		dsattribute_release(a);
		single_item = 1;
	}

	status = dsengine_netinfo_string_pathmatch(ap->engine, 0, path, &pdsid);
	free(path);
	if (status != DSStatusOK) return 1;

	match = NULL;
	count = 0;
	status = dsengine_search_pattern(ap->engine, pdsid, pattern, 1, 1, &match, &count);
	if (status != DSStatusOK) return 1;
	if (match == NULL) return 0;

	if (single_item == 1) count = 1;
		
	status = dsengine_fetch_list(ap->engine, count, match, &lp);
	free(match);

	if (status != DSStatusOK) return 1;
	if (lp == NULL) return 0;

	*list = lp[0];
	if (lp != NULL) add_validation(lp[0]);

	for (i = 1; i < count; i++)
	{
		add_validation(lp[i]);
		lp[i-1]->next = lp[i];
	}

	return 0;
}

u_int32_t
SI_validate(void *c, char *v)
{
	agent_private *ap;
	int n;
	u_int32_t dsid, vers, serial;
	u_int32_t evers, eserial, esuper;
	dsstatus status;

	if (c == NULL) return 0;
	if (v == NULL) return 0;

	ap = (agent_private *)c;

	n = sscanf(v, "%u %u %u", &dsid, &serial, &vers);
	if (n != 3) return 0;

	status = dsengine_vital_statistics(ap->engine, dsid, &evers, &eserial, &esuper);
	if (status != DSStatusOK) return 0;
	if (evers != vers) return 0;
	
	return 1;
}
