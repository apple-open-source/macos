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

static dsstatus enqueue_modification LDAP_P((BackendDB *be, dsrecord *rec, Modification *mod));

/*
 * Queue a modification onto a record. Again, an attempt is made
 * to turn distinguishedName syntax values into store-local DSIDs.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
static dsstatus enqueue_modification(
	BackendDB *be,
	dsrecord *rec,
	Modification *mod)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct atmap map;
	dsattribute *a, *existing;
	dsstatus status;
	BerVarray bvp;
	int i;
	u_int32_t super;

	assert(di != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "enqueue_modification: attribute %s\n", mod->sm_desc->ad_cname.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> enqueue_modification %s\n", mod->sm_desc->ad_cname.bv_val, 0, 0);
#endif

	super = SUPER(rec);

	/*
	 * Check we're not removing any object classes that
	 * violate would structure rules.
	 */
	if (ad_cmp(mod->sm_desc, slap_schema.si_ad_objectClass) == 0)
	{
		dsstatus status;

		status = schemamap_validate_objectclass_mods(be, super, mod);
		if (status != DSStatusOK)
		{
			return status;
		}
	}

	status = schemamap_x500_to_ni_at(be, super, mod->sm_desc, &map);
	if (status != DSStatusOK)
	{
		return status;
	}

	a = dsattribute_new(map.ni_key);
	if (a == NULL)
	{
		schemamap_atmap_release(&map);
		return DSStatusFailed;
	}

	for (bvp = mod->sm_bvalues; bvp != NULL && bvp->bv_val != NULL; bvp++)
	{
		dsdata *d;

		status = (map.x500ToNiTransform)(be, &d, bvp, map.type, map.x500ToNiArg);

		if (status != DSStatusOK)
		{
			schemamap_atmap_release(&map);
			dsattribute_release(a);
			return status;
		}

		dsattribute_append(a, d);
		dsdata_release(d);
	}

	/* Now, we've got to do something with the attribute. */
	switch (mod->sm_op & ~LDAP_MOD_BVALUES)
	{
		case LDAP_MOD_ADD:
			/*
			 * add: add values listed to the given attribute, creating
			 * the attribute if necessary;
			 */
			dsrecord_merge_attribute(rec, a, map.selector);
			break;
		case LDAP_MOD_DELETE:
			/*
			 * delete: delete values listed from the given attribute,
			 * removing the entire attribute if no values are listed, or
			 * if all current values of the attribute are listed for
			 * deletion;
			 */
			if (a->count == 0)
			{
				dsrecord_remove_key(rec, a->key, map.selector);
			}
			else
			{
				/* remove values from existing attribute. */
				existing = dsrecord_attribute(rec, a->key, map.selector);
				if (existing == NULL)
				{
					dsattribute_release(a);
					schemamap_atmap_release(&map);
					return DSStatusOK;
				}

				/* loop through the values we've got */
				for (i = 0; i < a->count; i++)
				{
					u_int32_t index;

					index = dsattribute_index(existing, a->value[i]);
					if (index != IndexNull)
					{
						/* found this value in the existing attribute */
						dsattribute_remove(existing, index);
					}
				}
				dsattribute_release(existing);
			}
			break;
		case LDAP_MOD_REPLACE:
		default:
			/*
			 * replace: replace all existing values of the given attribute
			 * with the new values listed, creating the attribute if it
			 * did not already exist.  A replace with no value will delete
			 * the entire attribute if it exists, and is ignored if the
			 * attribute does not exist.
			 */
			dsrecord_remove_key(rec, a->key, map.selector);
			if (a->count)
			{
				dsrecord_append_attribute(rec, a, map.selector);
			}
			break;
	}

	dsattribute_release(a);
	schemamap_atmap_release(&map);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "enqueue_modification: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== enqueue_modification\n", 0, 0, 0);
#endif

	return DSStatusOK;
}

int netinfo_back_modify(
    BackendDB	*be,
    Connection	*conn,
    Operation	*op,
    struct berval *dn,
    struct berval *ndn,
    Modifications *modlist)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsstatus status;
	u_int32_t dsid;
	dsrecord *rec;
	Modifications *p;
	Entry *ent;
	int rc;
	const char *text;
	char textbuf[SLAP_TEXT_BUFLEN];

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_modify: DN %s\n", dn->bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_modify dn=%s\n", dn->bv_val, 0, 0);
#endif

	if (netinfo_back_send_referrals(be, conn, op, ndn) == DSStatusOK)
	{
		return -1;
	}

	ENGINE_LOCK(di);

	status = netinfo_back_dn_pathmatch(be, ndn, &dsid);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
 	}

	status = dsengine_fetch(di->engine, dsid, &rec);
	dsengine_flush_cache(di->engine);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	for (p = modlist; p != NULL; p = p->sml_next)
	{
		/* do some ACL checking; should check per value XXX */
		/* slightly inefficient as this calls dsengine_fetch() */
		/* but that should be cached by the dsengine layer */
		status = netinfo_back_access_allowed(be, conn, op, dsid, p->sml_desc, NULL, ACL_WRITE);
		if (status != DSStatusOK)
		{
			dsrecord_release(rec);
			ENGINE_UNLOCK(di);
			return netinfo_back_op_result(be, conn, op, status);
		}

		status = enqueue_modification(be, rec, &p->sml_mod);
		if (status != DSStatusOK)
		{
			dsrecord_release(rec);
			ENGINE_UNLOCK(di);
			return netinfo_back_op_result(be, conn, op, status);
		}
	}

	status = dsrecord_to_entry(be, rec, &ent);
	if (status != DSStatusOK)
	{
		dsrecord_release(rec);
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	rc = entry_schema_check(be, ent, NULL, &text, textbuf, sizeof(textbuf));
	if (rc != LDAP_SUCCESS)
	{
		/* make up for not passing in oldattrs */
		if (rc == LDAP_NO_OBJECT_CLASS_MODS && rec->count > 0)
			rc = LDAP_OBJECT_CLASS_VIOLATION;

		dsrecord_release(rec);
		netinfo_back_entry_free(ent);
		ENGINE_UNLOCK(di);
		send_ldap_result(conn, op, rc, NULL, text, NULL, NULL);
		return rc;
	}

	netinfo_back_entry_free(ent);

	/* check for abandon */
	ldap_pvt_thread_mutex_lock(&op->o_abandonmutex);
	if (op->o_abandon)
	{
		dsrecord_release(rec);
		ENGINE_UNLOCK(di);
		ldap_pvt_thread_mutex_unlock(&op->o_abandonmutex);
		return SLAPD_ABANDON;
	}
	ldap_pvt_thread_mutex_unlock(&op->o_abandonmutex);

	status = dsengine_save(di->engine, rec);

	dsrecord_release(rec);

	ENGINE_UNLOCK(di);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_modify: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_modify\n", 0, 0, 0);
#endif

	return netinfo_back_op_result(be, conn, op, status);
}

