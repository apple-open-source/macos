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

/*
 * Algorithm for modifying a DN.
 *
 * 1. Get the dsid for the old DN's record
 * 2. Fetch record
 * 3. Set the rdn meta-attribute to the new RDN attribute type,
 *    iff the type is not "name".
 * 4. Set the RDN attribute value at index 0 for the RDN attribute type
 * 5. If we are not moving to a new superior DN, go to step 8.
 * 6. Get the new superior DN's dsid.
 * 7. Set the record's parent to the new parent ID.
 * 8. Write the record to disk.
 * 9. If we are moving to a new parent, remove the reference to the
 *    child from the old parent.
 * 10. If we are moving to a new parent, add a reference to the
 *     child to the new parent.
 */
int
netinfo_back_modrdn(
	BackendDB *be,
	Connection *conn,
	Operation *op,
	struct berval *dn,
	struct berval *ndn,
	struct berval *newrdn,
	struct berval *nnewrdn,
	int deleteoldrdn,
	struct berval *newSuperior,
	struct berval *nnewSuperior
)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsstatus status;
	u_int32_t dsid, newParentId, oldParentId, index;
	dsrecord *child, *parent;
	dsdata *rdnValue, *rdnKey;
	LDAPRDN *new_rdn;
	dsattribute *a;
	char *p;
	AttributeDescription *ad = NULL;
	const char *text;
	int rc;
	struct atmap map;

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, ARGS, "netinfo_back_modrdn DN %s "
		"new RDN %s new superior DN %s\n", dn->bv_val, newrdn->bv_val,
		(newSuperior != NULL) ? newSuperior->bv_val : "(null)"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_modrdn dn=%s newrdn=%s newsuperior=%s\n",
		dn->bv_val, newrdn->bv_val, newSuperior ? newSuperior->bv_val : "(null)");
#endif

	if (netinfo_back_send_referrals(be, conn, op, ndn) == DSStatusOK)
	{
		return 1;
	}

	ENGINE_LOCK(di);

	status = netinfo_back_dn_pathmatch(be, ndn, &dsid);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
 	}

	/* check the entry ACL to see whether we can change it */
	/* really we should check the attribute type for newrdn/oldrdn */
	status = netinfo_back_access_allowed(be, conn, op, dsid, slap_schema.si_ad_entry, NULL, ACL_WRITE);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	status = dsengine_fetch(di->engine, dsid, &child);
	dsengine_flush_cache(di->engine);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	if (ldap_str2rdn(newrdn->bv_val, &new_rdn, &p, LDAP_DN_FORMAT_LDAP) != LDAP_SUCCESS)
	{
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		send_ldap_result(conn, op, LDAP_OPERATIONS_ERROR, NULL,
			"Could not parse new RDN", NULL, NULL);
		return -1;
	}

	rc = slap_bv2ad(&new_rdn[0][0]->la_attr, &ad, &text);
	if (rc != LDAP_SUCCESS)
	{
		ldap_rdnfree(new_rdn);
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		send_ldap_result(conn, op, rc, NULL,
			text, NULL, NULL);
		return -1;
	}

	/*
	 * Get the mapped naming attribute.
	 */
	status = schemamap_x500_to_ni_at(be, SUPER(child), ad, &map);
	if (status != DSStatusOK)
	{
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		ldap_rdnfree(new_rdn);
		send_ldap_result(conn, op, LDAP_OPERATIONS_ERROR, NULL,
			"Could not parse new RDN type", NULL, NULL);
		return -1;
	}

	if (map.selector == SELECT_META_ATTRIBUTE)
	{
		schemamap_atmap_release(&map);
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		ldap_rdnfree(new_rdn);
		send_ldap_result(conn, op, LDAP_NAMING_VIOLATION, NULL,
			"Meta-attributes cannot name entries", NULL, NULL);
		return -1;
	}

	status = (map.x500ToNiTransform)(be, &rdnValue, &new_rdn[0][0]->la_value,
		map.type, map.x500ToNiArg);
	if (status != DSStatusOK)
	{
		schemamap_atmap_release(&map);
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		ldap_rdnfree(new_rdn);
		send_ldap_result(conn, op, LDAP_OPERATIONS_ERROR, NULL,
			"Could not transform RDN value", NULL, NULL);
		return -1;
	}

	ldap_rdnfree(new_rdn);

	rdnKey = dsdata_copy((dsdata *)&netinfo_back_name_rdn); 

	/* Make sure there is no "rdn" meta-attribute. */
	dsrecord_remove_key(child, rdnKey, SELECT_META_ATTRIBUTE);

	if (dsdata_equal(map.ni_key, (dsdata *)&netinfo_back_name_name) == 0)
	{
		a = dsattribute_new(rdnKey);
		assert(a != NULL);
		dsrecord_append_attribute(child, a, SELECT_META_ATTRIBUTE);
		/* set the RDN type as the first value of "rdn" meta attribute */
		dsattribute_insert(a, map.ni_key, 0);
		dsattribute_release(a);
	}

	dsdata_release(rdnKey);

	a = dsrecord_attribute(child, map.ni_key, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		a = dsattribute_new(map.ni_key);
		assert(a != NULL);
		/* add to the record */
		dsrecord_append_attribute(child, a, SELECT_ATTRIBUTE);
	}
	else
	{
		if (deleteoldrdn)
		{
			/* RDN is by definition first value; trash it. */
			dsattribute_remove(a, 0);
		}

		/* always remove the old RDN value */
		index = dsattribute_index(a, rdnValue);
		if (index != IndexNull)
			dsattribute_remove(a, index);
	}

	/* Insert the RDN value at index 0 */
	dsattribute_insert(a, rdnValue, 0);

	/* don't need these anymore */
	schemamap_atmap_release(&map);
	dsdata_release(rdnValue);
	dsattribute_release(a);

	/* check for abandon */
	if (op->o_abandon)
	{
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		return SLAPD_ABANDON;
	}

	/*
	 * If newSuperior != NULL, move to a new parent. 
	 */
	if (newSuperior != NULL)
	{
		/* get the parent dir ID */
		status = netinfo_back_dn_pathmatch(be, nnewSuperior, &newParentId);
		if (status != DSStatusOK)
		{
			dsrecord_release(child);
			ENGINE_UNLOCK(di);
			return netinfo_back_op_result(be, conn, op, status);
 		}

		/* check ACLs, fail if we can't modify new parent record */
		status = netinfo_back_access_allowed(be, conn, op, newParentId,
			slap_schema.si_ad_children, NULL, ACL_WRITE);
		if (status != DSStatusOK)
		{
			dsrecord_release(child);
			ENGINE_UNLOCK(di);
			return netinfo_back_op_result(be, conn, op, status);
 		}

		oldParentId = child->super;
		if (oldParentId != newParentId)
		{
			Entry *ent;

			/* check ACLs, fail if we can't modify old parent record */
			status = netinfo_back_access_allowed(be, conn, op, oldParentId,
				slap_schema.si_ad_children, NULL, ACL_WRITE);
			if (status != DSStatusOK)
			{
				dsrecord_release(child);
				ENGINE_UNLOCK(di);
				return netinfo_back_op_result(be, conn, op, status);
 			}

			/* temporarily convert to an entry */
			status = dsrecord_to_entry(be, child, &ent);
			if (status != DSStatusOK)
			{
				dsrecord_release(child);
				ENGINE_UNLOCK(di);
				return netinfo_back_op_result(be, conn, op, status);
			}

			/* check that structure rules allow move */
			status = schemamap_validate_objectclasses(be, newParentId, ent);
			if (status != DSStatusOK)
			{
				dsrecord_release(child);
				netinfo_back_entry_free(ent);
				ENGINE_UNLOCK(di);
				return netinfo_back_op_result(be, conn, op, status);
			}

			netinfo_back_entry_free(ent);

			/* only now move to a new parent */
			child->super = newParentId;
		}
	}
	else
	{
		/* keep gcc happy */
		oldParentId = newParentId;
	}

	/* check for abandon */
	if (op->o_abandon)
	{
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		return SLAPD_ABANDON;
	}

	/* there is no going back now */
	/* should we call dsengine_save() or dsstore_save()? */
	status = dsstore_save(di->engine->store, child);
	if (status != DSStatusOK)
	{
		/* XXX can't save changes to record; DB may be inconsistent */
		dsrecord_release(child);
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	/* now fix up parent, if it wasn't the existing parent */
	if (newSuperior != NULL && oldParentId != newParentId)
	{
		parent = dsstore_fetch(di->engine->store, oldParentId);
		if (parent == NULL)
		{
			dsrecord_release(child);
			ENGINE_UNLOCK(di);
			return netinfo_back_op_result(be, conn, op, DSStatusInvalidPath);
		}

		dsrecord_remove_sub(parent, dsid);

		/* Remove child from original parent's index */
		dsindex_delete_dsid(parent->index, dsid);

		status = dsstore_save(di->engine->store, parent);
		dsrecord_release(parent);
		if (status != DSStatusOK)
		{
			/* atomicity violation */
			/* XXX can't save changes to original parent!! */
			dsrecord_release(child);
			ENGINE_UNLOCK(di);
			return netinfo_back_op_result(be, conn, op, status);
		}

		parent = dsstore_fetch(di->engine->store, newParentId);
		if (parent == NULL)
		{
			dsrecord_release(child);
			ENGINE_UNLOCK(di);
			return netinfo_back_op_result(be, conn, op, DSStatusInvalidPath);
		}

		dsrecord_append_sub(parent, dsid);

		/* Add child to new parent's index */
		dsindex_insert_record(parent->index, child);

		status = dsstore_save(di->engine->store, parent);
		dsrecord_release(parent);
	}

	dsrecord_release(child);

	ENGINE_UNLOCK(di);

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_modrdn: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_modrdn \n", 0, 0, 0);
#endif

	return netinfo_back_op_result(be, conn, op, status);
}
