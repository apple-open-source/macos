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

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "back-netinfo.h"

static int fetch_attribute LDAP_P((Backend *be, Connection *conn, Operation *op, Entry *target, AttributeDescription *entry_at, BerVarray *vals));

/*
 * Fetch an attribute from an existing entry. 
 */
static int fetch_attribute(
	Backend *be,
	Connection *conn,
	Operation *op,
	Entry *target,
	AttributeDescription *entry_at,
	BerVarray *vals)
{
	Attribute *attr;
	struct berval *iv, *jv;
	BerVarray v;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "fetch_attribute: enter\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> fetch_attribute\n", 0, 0, 0);
#endif

	if (is_entry_alias(target))
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "fetch_attribute: is alias\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== fetch_attribute\n", 0, 0, 0);
#endif
		return LDAP_ALIAS_PROBLEM;
	}

	if (conn != NULL && op != NULL &&
		access_allowed(be, conn, op, target,
			slap_schema.si_ad_entry, NULL, ACL_READ, NULL) == 0)
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "fetch_attribute: insufficient access\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== fetch_attribute\n", 0, 0, 0);
#endif
		return LDAP_INSUFFICIENT_ACCESS;
	}

	attr = attr_find(target->e_attrs, entry_at);
	if (attr == NULL)
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "fetch_attribute: no such attribute\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== fetch_attribute\n", 0, 0, 0);
#endif
		return LDAP_NO_SUCH_ATTRIBUTE;
	}

	if (conn != NULL && op != NULL &&
		access_allowed(be, conn, op, target,
			entry_at, NULL, ACL_READ, NULL) == 0)
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "fetch_attribute: insufficient access\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== fetch_attribute\n", 0, 0, 0);
#endif
		return LDAP_INSUFFICIENT_ACCESS;
	}

	for (iv = attr->a_vals; iv->bv_val != NULL; iv++)
	{
		/* count them */
	}

	v = (BerVarray)ch_malloc(sizeof(struct berval) * ((iv - attr->a_vals)+1));
	for (iv = attr->a_vals, jv = v; iv->bv_val != NULL; iv++)
	{
		if (conn != NULL && op != NULL &&
			access_allowed(be, conn, op, target,
				entry_at, iv, ACL_READ, NULL) == 0) {
			continue;
		}
		ber_dupbv(jv, iv);
		if (jv->bv_val != NULL)
			jv++;
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "fetch_attribute: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== fetch_attribute\n", 0, 0, 0);
#endif

	if (jv == v)
	{
		ch_free(v);
		*vals = NULL;
		return LDAP_INSUFFICIENT_ACCESS;
	}

	jv->bv_val = NULL;
	*vals = v;
	
	return LDAP_SUCCESS;
}

int
netinfo_back_attribute(
	Backend	*be,
	Connection *conn,
	Operation *op,
	Entry	*target,
	struct berval *endn,
	AttributeDescription *entry_at,
	BerVarray *vals
)
{
	u_int32_t dsid;
	struct atmap map;
	dsstatus status;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsrecord *r;
	dsattribute *a;
	int match = 0;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_attribute: "
		"ID %ld NDN %s\n",
		target ? target->e_id : -1,
		endn ? endn : "(null)"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_attribute target->e_id=%ld endn=%s\n",
		target ? target->e_id : -1, endn ? endn->bv_val : "(null)", 0);
#endif

	if (target != NULL &&
	    dnMatch(&match, 0, NULL, NULL, &target->e_nname, (void *)endn) == LDAP_SUCCESS &&
	    match != 0)
	{
		/* We have the entry. Don't poke around in NetInfo. */
		return fetch_attribute(be, conn, op, target, entry_at, vals);
	}

	ENGINE_LOCK(di);

	status = netinfo_back_dn_pathmatch(be, endn, &dsid);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_attribute: patchmatch failed"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_attribute\n", 0, 0, 0);
#endif
		return dsstatus_to_ldap_err(status);
	}

	status = netinfo_back_access_allowed(be, conn, op, dsid, entry_at, NULL, ACL_READ);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_attribute: authorization failed\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_attribute\n", 0, 0, 0);
#endif
		return dsstatus_to_ldap_err(status);
	}

	status = dsengine_fetch(di->engine, dsid, &r);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_attribute: fetch failed\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_attribute\n", 0, 0, 0);
#endif
		return dsstatus_to_ldap_err(status);
	}

	status = schemamap_x500_to_ni_at(be, SUPER(r), entry_at, &map);
	if (status != DSStatusOK)
	{
		dsrecord_release(r);
		ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_attribute: schema mapping failed\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_attribute\n", 0, 0, 0);
#endif
		return LDAP_NO_SUCH_ATTRIBUTE;
	}

	a = dsrecord_attribute(r, map.ni_key, map.selector);
	if (a == NULL)
	{
		schemamap_atmap_release(&map);
		dsrecord_release(r);
		ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_attribute: no such attribute\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_attribute\n", 0, 0, 0);
#endif
		return LDAP_NO_SUCH_ATTRIBUTE;
	}

	status = dsattribute_to_bervals(be, vals, a, &map);

	/* XXX should do access checking on each value */

	dsattribute_release(a);
	schemamap_atmap_release(&map);
	dsrecord_release(r);

	ENGINE_UNLOCK(di);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_attribute: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_attribute\n", 0, 0, 0);
#endif

	return dsstatus_to_ldap_err(status);
}
