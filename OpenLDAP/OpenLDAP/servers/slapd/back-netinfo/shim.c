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

static Attribute *dsattribute_to_attribute LDAP_P ((BackendDB *be, dsrecord *r, dsattribute *a, u_int32_t sel));
static dsattribute *attribute_to_dsattribute LDAP_P ((BackendDB *be, u_int32_t dsid, Attribute *attr, u_int32_t *sel));

/*
 * Return the dsdata type for a particular attribute type,
 * based on syntax or matching rule.
 */
u_int32_t ad_to_dsdata_type(AttributeDescription *ad)
{
	u_int32_t type;
	AttributeType *at = ad->ad_type;
	char *printableType;

	/* Binary attributes are stored as blobs. */
	if (is_at_syntax(at, SLAPD_OCTETSTRING_SYNTAX) ||
	    slap_ad_is_binary(ad))
	{
		type = DataTypeBlob;
	}
	/* Distinguished names _may_ be stored as references. */
	else if (is_at_syntax(at, SLAPD_DN_SYNTAX))
	{
		type = DataTypeDirectoryID;
	}
	/* DirectoryStrings are stored as UTF-8 */
	else if (is_at_syntax(at, "1.3.6.1.4.1.1466.115.121.1.15"))
	{
		if (at->sat_equality != NULL &&
		    strcmp(at->sat_equality->smr_oid, "2.5.13.5") == 0)
			type = DataTypeUTF8Str; /* caseExactMatch */
		else
			type = DataTypeCaseUTF8Str; /* caseIgnoreMatch */
	}
	/* Everything else is presently stored as ASCII (IA5String). */
	else
	{
		if (at->sat_equality != NULL &&
	            strcmp(at->sat_equality->smr_oid, "1.3.6.1.4.1.1466.109.114.1") == 0)
			type = DataTypeCStr; /* caseExactIA5Match */
		else
			type = DataTypeCaseCStr; /* caseIgnoreIA5Match, others */
	}

	switch (type)
	{
		case DataTypeBlob: printableType = "DataTypeBlob"; break;
		case DataTypeDirectoryID: printableType = "DataTypeDirectoryID"; break;
		case DataTypeUTF8Str: printableType = "DataTypeUTF8Str"; break;
		case DataTypeCaseUTF8Str: printableType = "DataTypeCaseUTF8Str"; break;
		case DataTypeCStr: printableType = "DataTypeCStr"; break;
		case DataTypeCaseCStr: printableType = "DataTypeCaseCStr"; break;
		default: printableType = "<unknown>"; break;
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "ad_to_dsdata_type %s %s\n", ad->ad_cname.bv_val, printableType));
#else
	Debug(LDAP_DEBUG_TRACE, "<=> ad_to_dsdata_type attribute=%s type=%s\n", ad->ad_cname.bv_val, printableType, 0);
#endif

	return type;
}

/*
 * Create a berval from a dsdata instance.
 */
struct berval *dsdata_to_berval(struct berval *bv, dsdata *data)
{
	bv->bv_len = data->length;
	if (IsStringDataType(data->type))
	{
		/* Seems that bv_len doesn't include the terminating NUL. */
		bv->bv_len--;
	}
	/* However, be sure to allocate enough room for terminating NUL. */
	bv->bv_val = ch_malloc(data->length);
	memmove(bv->bv_val, data->data, data->length);

	return bv;
}

/*
 * As above, but in place (must not use berval after freeing dsdata)
 */
struct berval *dsdata_to_berval_no_copy(struct berval *bv, dsdata *data)
{
	bv->bv_len = data->length;
	if (IsStringDataType(data->type))
	{
		bv->bv_len--;
	}
	bv->bv_val = data->data;

	return bv;
}

/*
 * Create a dsdata instance from a berval.
 */
dsdata *berval_to_dsdata(struct berval *bv, u_int32_t type)
{
	dsdata *d;

	if (IsStringDataType(type))
	{
		/* dsengine strings are NUL terminated, slapd not. */
		d = dsdata_alloc(bv->bv_len + 1);
		assert(d != NULL);

		d->type = type;
		d->retain = 1;

		assert(d->data != NULL);
		memmove(d->data, bv->bv_val, bv->bv_len);
		d->data[bv->bv_len] = '\0';
	}
	else
	{
		d = dsdata_new(type, bv->bv_len, bv->bv_val);
	}

	assert(d != NULL);

	return d;
}

/*
 * Create a dsdata instance from a berval, in-place. Be careful,
 * because the string is ASSUMED to be NUL terminated.
 */
dsdata *berval_to_dsdata_no_copy(dsdata *dst, struct berval *bv, u_int32_t type)
{
	if (IsStringDataType(type))
	{
		dst->length = bv->bv_len + 1;
	}
	else
	{
		dst->length = bv->bv_len;
	}
	dst->retain = 1;
	dst->type = type;
	dst->data = bv->bv_val;

	return dst;
}

/*
 * Translate a dsrecord into a slapd entry. The DN of
 * the entry is formed from the store suffix and the
 * path traced to the record. Each attribute is then
 * appended to the entry as well as the requisite
 * object classes. The record ID is stored in the
 * e_id field in case we later want to cache at
 * the slapd level.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus dsrecord_to_entry(BackendDB *be, dsrecord *rec, Entry **pEntry)
{
	Entry *ent;
	Attribute *attr, **attrp;
	int i;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsstatus status;

	assert(di != NULL);

	ent = (Entry *)ch_calloc(1, sizeof(Entry));
	ent->e_id = (ID)rec->dsid;
	ent->e_bv.bv_val = NULL;
	ent->e_ocflags = 0;

	/*
	 * Get the canonical, prettied DN for the record.
	 */
	status = netinfo_back_global_dn(be, rec->dsid, &ent->e_name);
	if (status != DSStatusOK)
	{
		ch_free(ent);
		return status;
	}

	if (dnNormalize2(NULL, &ent->e_name, &ent->e_nname) != LDAP_SUCCESS)
	{
		ch_free(ent->e_name.bv_val);
		return DSStatusInvalidPath;
	}

	dsrecord_retain(rec);
	ent->e_private = (void *)rec;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "dsrecord_to_entry: dSID %u DN %s\n",
		rec->dsid, ent->e_name.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> dsrecord_to_entry dsid=%u dn=%s\n", rec->dsid, ent->e_name.bv_val, 0);
#endif

	ent->e_attrs = NULL;
	attrp = &ent->e_attrs;

	/* Add attributes. */
	for (i = 0; i < rec->count; i++)
	{
		attr = dsattribute_to_attribute(be, rec, rec->attribute[i], SELECT_ATTRIBUTE);
		if (attr == NULL) 
			continue;

		*attrp = attr;
		attrp = &attr->a_next;
	}

	/* Add meta attributes. */	
	for (i = 0; i < rec->meta_count; i++)
	{
		attr = dsattribute_to_attribute(be, rec, rec->meta_attribute[i], SELECT_META_ATTRIBUTE);
		if (attr == NULL)
			continue;

		*attrp = attr;
		attrp = &attr->a_next;
	}

	/* Add mapped object classes. */
	schemamap_add_objectclasses(be, SUPER(rec), ent);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "dsrecord_to_entry: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== dsrecord_to_entry\n", 0, 0, 0);
#endif

	*pEntry = ent;

	return DSStatusOK;
}

/*
 * Map an entry into a dsrecord. The dsrecord does not have a 
 * distinguished name, so naming the record must be dealt with
 * separately. However, the "rdn" meta-attribute is set to
 * the RDN type if it is not "name".
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus entry_to_dsrecord(BackendDB *be, u_int32_t super, Entry *e, dsrecord **pRecord)
{
	dsrecord *rec;
	Attribute *attr;
	LDAPRDN *rdn;
	u_int32_t tmp, sel;
	struct atmap map;
	dsattribute *a;
	struct berval _rdn;
	char *p;
	dsdata *rdnValue;
	AttributeDescription *ad = NULL;
	const char *text;
	dsstatus status;
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	/* If the RDN type is not "name", set the _rdn meta-attribute. */
	if (dnExtractRdn(&e->e_name, &_rdn) != LDAP_SUCCESS)
	{
		return DSStatusInvalidPath;
	}

	if (ldap_str2rdn(_rdn.bv_val, &rdn, &p, LDAP_DN_FORMAT_LDAP) != LDAP_SUCCESS)
	{
		ch_free(_rdn.bv_val);
		return DSStatusFailed;
	}

	ch_free(_rdn.bv_val);

	if (slap_bv2ad(&rdn[0][0]->la_attr, &ad, &text) != LDAP_SUCCESS)
	{
		ldap_rdnfree(rdn);
		return DSStatusFailed;
	}

	/*
	 * Get the mapped naming attribute.
	 */
	status = schemamap_x500_to_ni_at(be, super, ad, &map);
	if (status != DSStatusOK)
	{
		ldap_rdnfree(rdn);
		return status;
	}

	status = (map.x500ToNiTransform)(be, &rdnValue, &rdn[0][0]->la_value, map.type, map.x500ToNiArg);
	if (status != DSStatusOK)
	{
		schemamap_atmap_release(&map);
		ldap_rdnfree(rdn);
		return status;
	}

	ldap_rdnfree(rdn);

	/*
	 * Meta-attributes cannot be used to name entries.
	 */
	if (map.selector == SELECT_META_ATTRIBUTE)
	{
		schemamap_atmap_release(&map);
		dsdata_release(rdnValue);
		return DSStatusFailed;
	}

	if ((slapMode & SLAP_TOOL_MODE) == 0)
	{
		/*
		 * Check whether the record already exists.
		 */
		dsrecord *query;

		query = dsrecord_new();
		if (query == NULL)
		{
			schemamap_atmap_release(&map);
			dsdata_release(rdnValue);
			return DSStatusFailed;
		}
	
		a = dsattribute_new(map.ni_key);
		if (a == NULL)
		{
			schemamap_atmap_release(&map);
			dsdata_release(rdnValue);
			dsrecord_release(query);
			return DSStatusFailed;
		}
	
		dsattribute_append(a, rdnValue);
		dsrecord_append_attribute(query, a, SELECT_ATTRIBUTE);
		dsattribute_release(a);
	
		status = dsengine_pathmatch(di->engine, super, query, &tmp);
		if (status != DSStatusInvalidPath)
		{
			if (status == DSStatusOK)
				status = DSStatusDuplicateRecord;
			schemamap_atmap_release(&map);
			dsdata_release(rdnValue);
			dsrecord_release(query);
			return status;
		}
	
		dsrecord_release(query);
	}

	rec = dsrecord_new();
	if (rec == NULL)
	{
		schemamap_atmap_release(&map);
		dsdata_release(rdnValue);
		return DSStatusFailed;
	}

	/*
	 * Check whether mapped RDN is "name". 
	 */
	if (dsdata_equal(map.ni_key, (dsdata *)&netinfo_back_name_name) == 0)
	{
		/* Need to add _rdn meta-attribute. */
		dsdata *rdnKey;

		rdnKey = dsdata_copy((dsdata *)&netinfo_back_name_rdn);
		a = dsattribute_new(rdnKey);
		if (a == NULL)
		{
			schemamap_atmap_release(&map);
			dsdata_release(rdnValue);
			dsdata_release(rdnKey);
			return DSStatusFailed;
		}

		dsdata_release(rdnKey);
		dsattribute_append(a, map.ni_key);
		dsrecord_append_attribute(rec, a, SELECT_META_ATTRIBUTE);
		dsattribute_release(a);
	}

	for (attr = e->e_attrs; attr != NULL; attr = attr->a_next)
	{
		a = attribute_to_dsattribute(be, super, attr, &sel);
		if (a == NULL)
			continue; /* maybe it was an automagic attribute */

		dsrecord_append_attribute(rec, a, sel);
		dsattribute_release(a);
	}

	/*
	 * Add the RDN as an attribute of the entry, or move it
	 * to value 0. Schema should enforce it being existant.
	 */
	a = dsrecord_attribute(rec, map.ni_key, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		a = dsattribute_new(map.ni_key);
		if (a == NULL)
		{
			schemamap_atmap_release(&map);
			dsdata_release(rdnValue);
			return DSStatusFailed;
		}
		/* add to the record */
		dsrecord_append_attribute(rec, a, SELECT_ATTRIBUTE);
	}
	else
	{
		u_int32_t index;

		/* remove the old value ... */
		index = dsattribute_index(a, rdnValue);
		if (index != IndexNull)
		{
			dsattribute_remove(a, index);
		}
	}

	/* Insert the RDN value at index 0, this attr is retained by the rec */
	dsattribute_insert(a, rdnValue, 0);

	schemamap_atmap_release(&map);
	dsdata_release(rdnValue);
	dsattribute_release(a);

	*pRecord = rec;

	return DSStatusOK;
}

/*
 * Just get the values of an attribute. This is used
 * by netinfo_back_attribute() as well as 
 * dsattribute_to_attribute().
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus dsattribute_to_bervals(BackendDB *be, BerVarray *pvals, dsattribute *a, struct atmap *map)
{
	int i, j;
	BerVarray vals;

	/* Empty values not allowed by LDAP. */
	if (a->count == 0)
		return DSStatusInvalidKey;

	vals = (BerVarray)ch_calloc(a->count + 1, sizeof(struct berval));
	for (i = 0, j = 0; i < a->count; i++)
	{
		if ((map->niToX500Transform)(be, &vals[j], a->value[i], map->niToX500Arg) != DSStatusOK)
			continue;

		j++;
	}

	if (j == 0)
	{
		/* Empty values not allowed by LDAP. */
		ch_free(vals);
		return DSStatusInvalidKey;
	}

	vals[j].bv_val = NULL;
	vals[j].bv_len = 0;

	*pvals = vals;

	return DSStatusOK;
}

/*
 * Translate a dsattribute to a slapd attribute. An attempt is
 * made to reconstitute directory ID values by using the
 * dsengine_convert_name() API.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
static Attribute *dsattribute_to_attribute(BackendDB *be, dsrecord *rec, dsattribute *a, u_int32_t sel)
{
	Attribute *attr;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsstatus status;
	struct atmap map;

	assert(di != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "dsattribute_to_attribute: dSID %d NetInfo attribute %s\n", rec->dsid, dsdata_to_cstring(a->key)));
#else
	Debug(LDAP_DEBUG_TRACE, "==> dsattribute_to_attribute rec=%d key=%s\n", rec->dsid, dsdata_to_cstring(a->key), 0);
#endif

	status = schemamap_ni_to_x500_at(be, SUPER(rec), a->key, sel, &map);
	if (status != DSStatusOK) 
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "dsattribute_to_attribute: could not map attribute\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== dsattribute_to_attribute: could not map attribute\n", 0, 0, 0);
#endif
		return NULL;
	}

	attr = (Attribute *)ch_malloc(sizeof(Attribute));
	attr->a_next = NULL;
	attr->a_desc = map.x500;

	if (dsattribute_to_bervals(be, &attr->a_vals, a, &map) != DSStatusOK)
	{
		/* empty valued attributes disallowed */
		ch_free(attr);
		schemamap_atmap_release(&map);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "dsattribute_to_attribute: no values\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== dsattribute_to_attribute no values\n", 0, 0, 0);
#endif
		return NULL;
	}

	schemamap_atmap_release(&map);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "dsattribute_to_attribute: X.500 attribute %s flags %d\n",
		attr->a_desc->ad_cname.bv_val, attr->a_desc->ad_flags, 0));
#else
	Debug(LDAP_DEBUG_TRACE, "<== dsattribute_to_attribute cname=%s flags=%d\n",
		attr->a_desc->ad_cname.bv_val, attr->a_desc->ad_flags, 0);
#endif

	return attr;
}

/*
 * Translate a slapd attribute to a dsattribute. If the attribute
 * syntax is distinguishedName, we might attempt to store the
 * directory ID if the distinguishedName refers to a store-local
 * record.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsattribute *attribute_to_dsattribute(BackendDB *be, u_int32_t dsid, Attribute *attr, u_int32_t *sel)
{
	dsstatus status;
	dsattribute *a;
	struct berval *bvp;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct atmap map;

	assert(di != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "attribute_to_dsattribute: X.500 attribute %s dSID %u\n", attr->a_desc->ad_cname.bv_val, dsid));
#else
	Debug(LDAP_DEBUG_TRACE, "==> attribute_to_dsattribute %s dsid=%u\n", attr->a_desc->ad_cname.bv_val, dsid, 0);
#endif

	status = schemamap_x500_to_ni_at(be, dsid, attr->a_desc, &map);
	if (status != DSStatusOK)
		return NULL;

	*sel = map.selector;

	a = dsattribute_new(map.ni_key);
	if (a == NULL)
	{
		schemamap_atmap_release(&map);
		return NULL;
	}

	for (bvp = attr->a_vals; bvp->bv_val != NULL; bvp++)
	{
		dsdata *d;

		/* Skip non-transformable values. */
		status = (map.x500ToNiTransform)(be, &d, bvp, map.type, map.x500ToNiArg);
		if (status != DSStatusOK)
			continue;

		dsattribute_append(a, d);
		dsdata_release(d);
	}

	schemamap_atmap_release(&map);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "attribute_to_dsattribute: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== attribute_to_dsattribute\n", 0, 0, 0);
#endif

	return a;
}

/*
 * Simple mapping between dsstatus and LDAP C API error
 * enumerations.
 */
int dsstatus_to_ldap_err(dsstatus status)
{
	switch (status)
	{
		case DSStatusOK:
			return LDAP_SUCCESS;
		case DSStatusInvalidStore:
		case DSStatusNoFile:
			return LDAP_UNAVAILABLE;
		case DSStatusDuplicateRecord:
			return LDAP_ALREADY_EXISTS;
		case DSStatusNoRootRecord:
		case DSStatusInvalidRecordID:
		case DSStatusInvalidPath:
			return LDAP_NO_SUCH_OBJECT;
		case DSStatusPathNotLocal:
			return LDAP_REFERRAL;
		case DSStatusConstraintViolation:
			return LDAP_CONSTRAINT_VIOLATION;
		case DSStatusNamingViolation:
			return LDAP_NAMING_VIOLATION;
		case DSStatusObjectClassViolation:
			return LDAP_OBJECT_CLASS_VIOLATION;
		case DSStatusInvalidKey:
			return LDAP_NO_SUCH_ATTRIBUTE;
		case DSStatusAccessRestricted:
			return LDAP_INVALID_CREDENTIALS;
		case DSStatusReadRestricted:
		case DSStatusWriteRestricted:
			return LDAP_INSUFFICIENT_ACCESS;
		case DSStatusReadFailed:
		case DSStatusWriteFailed:
		case DSStatusInvalidUpdate:
			return LDAP_OPERATIONS_ERROR;
		case DSStatusInvalidRecord:
		case DSStatusNoData:
		case DSStatusStaleRecord:
		case DSStatusInvalidSessionMode:
		case DSStatusInvalidSession:
			return LDAP_UNWILLING_TO_PERFORM;
		case DSStatusLocked:
			return LDAP_BUSY;
		case DSStatusFailed:
			return LDAP_OTHER;
	}
	/*NOTREACHED*/
	return LDAP_OTHER;
}

/*
 * Calls send_ldap_result() with a mapped dsstatus and 
 * message.
 */
int netinfo_back_op_result(BackendDB *be, Connection *conn, Operation *op, dsstatus status)
{
	int rc;
	char *message, *statusMessage;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	BerVarray refs;

	if (status == DSStatusPathNotLocal)
		refs = (di->parent != NULL) ? di->parent->refs : NULL;
	else
		refs = NULL;
	rc = dsstatus_to_ldap_err(status);

	statusMessage = dsstatus_message(status);
	message = ch_malloc(strlen(statusMessage) + sizeof("DSAXXXX: "));
	sprintf(message, "DSA%04u: %s", status, statusMessage);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_op_result: status %d\n", status));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_op_result dsstatus=%d rc=%d msg=%s\n",
		status, rc, message);
#endif

	send_ldap_result(conn, op, rc, NULL, message, refs, NULL);

	ch_free(message);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_op_result: result code %d message %s\n", rc, message));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_op_result\n", 0, 0, 0);
#endif

	return (status == DSStatusOK) ? 0 : -1;
}

/*
 * Create the store-local version of a distinguished name. 
 * This function will attempt to strip off the store suffix
 * (presently specified in the "suffix" property of the master
 * machine) and all the suffixes specified for this backend in
 * slapd.conf. If the supplied DN _is_ the suffix, or is the
 * NULL DN, then the DN "" will be returned.
 */
dsstatus dnMakeLocal(BackendDB *be, struct berval *localDN, struct berval *ndn)
{
	int i, where;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct berval *nsuffix;

	assert(di != NULL);
	assert(di->nsuffix.bv_val != NULL);

	nsuffix = NULL;

	if (ndn == NULL || ndn->bv_len == 0)
	{
		localDN->bv_val = NULL;
		localDN->bv_len = 0;

		return DSStatusOK;
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "dnMakeLocal DN %s\n", ndn->bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> dnMakeLocal dn=%s\n", ndn->bv_val, 0, 0);
#endif

	if (dnIsSuffix(ndn, &di->nsuffix))
	{
		nsuffix = &di->nsuffix;
	}
	else if (be->be_nsuffix != NULL)
	{
		for (i = 0; be->be_nsuffix[i] != NULL; i++)
		{
			if (be->be_nsuffix[i]->bv_len > 0 && dnIsSuffix(ndn, be->be_nsuffix[i]))
			{
				nsuffix = be->be_nsuffix[i];
				break;
			}
		}
	}

	if (nsuffix == NULL)
	{
		/* Not mastered by this store. Sorry! */
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "dnMakeLocal: not local to store\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== dnMakeLocal (not local to store)\n", 0, 0, 0);
#endif

		return DSStatusPathNotLocal;
	}

	if (nsuffix->bv_len == ndn->bv_len)
	{
		/* NDN _is_ the suffix; return NULL DN. */
		where = 0; 
	}
	else if (nsuffix->bv_len > 0)
	{
		/* Copy NDN until the comma before the suffix. */
		where = ndn->bv_len - nsuffix->bv_len - 1; 
	}
	else
	{
		/* Copy the NDN as the suffix is empty. */
		where = ndn->bv_len; 
	}

	assert(where >= 0);
	if (where == 0)
	{
		/* Don't make a copy for the root entry. */
		localDN->bv_val = NULL;
		localDN->bv_len = 0;
	}
	else
	{
		localDN->bv_val = ch_malloc(where + 1);
		AC_MEMCPY(localDN->bv_val, ndn->bv_val, where);
		localDN->bv_val[where] = '\0';
		localDN->bv_len = where;
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "dnMakeLocal: DN %s normalized suffix %s\n", (localDN->bv_val ? localDN->bv_val : ""), nsuffix->bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "<== dnMakeLocal dn=%s nsuffix=%s\n", (localDN->bv_val ? localDN->bv_val : ""), nsuffix->bv_val, 0);
#endif

	return DSStatusOK;
}

/*
 * Add the canonical backend suffix to a DN, making it
 * absolute, or "global".
 */
dsstatus dnMakeGlobal(BackendDB *be, struct berval *globalDN, struct berval *localDN)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);
	assert(di->suffix.bv_val != NULL);

	if (localDN->bv_len > 0)
	{
		if (di->suffix.bv_len > 0)
		{
			/*
			 * Concatenate local record DN with backend
			 * suffix.
			 */
			build_new_dn(globalDN, &di->suffix, localDN);
		}
		else
		{
			ber_dupbv(globalDN, localDN);
		}
	}
	else
	{
		/*
		 * Root directory, copy backend suffix.
		 */
		ber_dupbv(globalDN, &di->suffix);
	}

	return DSStatusOK;
}

/*
 * Return the global DN for a directory ID.
 */
dsstatus netinfo_back_global_dn(BackendDB *be, u_int32_t dsid, struct berval *globalDN)
{
	dsstatus status;
	struct berval localDN;

	status = netinfo_back_local_dn(be, dsid, &localDN);
	if (status != DSStatusOK)
		return status;

	status = dnMakeGlobal(be, globalDN, &localDN);
	ch_free(localDN.bv_val);

	return status;
}

/*
 * A replacement for dsengine_x500_string_path() that
 * is schema-mapping aware.
 */
dsstatus netinfo_back_local_dn(BackendDB *be, u_int32_t dsid, struct berval *localDN)
{
	dsrecord *r = NULL;
	dsstatus status;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	LDAPDN *dn = NULL;
	u_int32_t depth;

	/*
	 * Short circuit; create a DN with no RDNs.
	 */
	if (dsid == 0)
	{
		dn = NULL;
		status = DSStatusOK;
		goto out;
	}

	/*
	 * Fetch the record with the supplied dsid.
	 */
	status = dsengine_fetch(di->engine, dsid, &r);
	if (status != DSStatusOK)
	{
		goto out;
	}

	depth = 1;

	/*
	 * Loop until we hit the root directory.
	 */
	while (r->dsid != 0)
	{
		dsrecord *parent;
		dsdata *rdnKey, *rdnValue;
		dsattribute *rdnSelector, *rdnAttribute;
		LDAPRDN *rdn;
		LDAPAVA *ava;
		struct atmap map;
		char tmp[32];
		struct berval *attrname = NULL;

		/*
		 * Find out the (unmapped) attribute that is to be
	 	 * used as the relative distinguished name.
		 */
		rdnSelector = dsrecord_attribute(r, (dsdata *)&netinfo_back_name_rdn,
			SELECT_META_ATTRIBUTE);
		if (rdnSelector == NULL || rdnSelector->count == 0)
			rdnKey = dsdata_retain((dsdata *)&netinfo_back_name_name);
		else
			rdnKey = dsdata_retain(rdnSelector->value[0]);

		dsattribute_release(rdnSelector);

		/*
		 * Fetch the attribute that is to be the RDN from the
		 * record.
		 */
		rdnAttribute = dsrecord_attribute(r, rdnKey, SELECT_ATTRIBUTE);
		if (rdnAttribute == NULL)
		{
			/* If it is not there, use DSID=xxx */
			rdnValue = NULL;
			attrname = &netinfo_back_ad_dSID->ad_cname;
		}
		else
		{
			rdnValue = dsdata_retain(rdnAttribute->value[0]);
			dsattribute_release(rdnAttribute);

			/*
			 * Find the X.500 attribute description that is to
			 * be used for the RDN.
			 */
			status = schemamap_ni_to_x500_at(be, SUPER(r), rdnKey, SELECT_ATTRIBUTE, &map);
			if (status != DSStatusOK)
			{
				dsdata_release(rdnKey);
				dsdata_release(rdnValue);
				goto out;
			}
			attrname = &map.x500->ad_cname;
		}

		dsdata_release(rdnKey);

		assert(attrname != NULL);

		/*
		 * Allocate RDN. Careful, because the attribute type is 
		 * allocated contiguously with the AVA itself, and should
		 * just be copied from the schema-mapped description.
		 */
		ava = (LDAPAVA *)ch_malloc(sizeof(LDAPAVA) + attrname->bv_len + 1);
		ava->la_private = NULL;
		ava->la_flags = 0;
		ava->la_attr.bv_len = attrname->bv_len;
		ava->la_attr.bv_val = (char *)(ava + 1);
		AC_MEMCPY(ava->la_attr.bv_val, attrname->bv_val, attrname->bv_len);
		ava->la_attr.bv_val[attrname->bv_len] = '\0';

		/*
		 * Transform the RDN value.
		 */
		if (rdnValue == NULL)
		{
			snprintf(tmp, sizeof(tmp), "%u", r->dsid);
			ava->la_value.bv_val = ch_strdup(tmp);
			ava->la_value.bv_len = strlen(ava->la_value.bv_val);
			ava->la_flags = LDAP_AVA_STRING;
		}
		else
		{
			status = (map.niToX500Transform)(be, &ava->la_value, rdnValue, map.niToX500Arg);
			if (status != DSStatusOK)
			{
				dsdata_release(rdnValue);
				ldap_avafree(ava);
				schemamap_atmap_release(&map);
				goto out;
			}
			ava->la_flags = IsStringDataType(rdnValue->type) ? LDAP_AVA_STRING : LDAP_AVA_BINARY;
			dsdata_release(rdnValue);
		}

		schemamap_atmap_release(&map);

		/*
		 * Get the parent record.
		 */
		status = dsengine_fetch(di->engine, r->super, &parent);
		if (status != DSStatusOK)
		{
			ldap_avafree(ava);
			goto out;
		}
		dsrecord_release(r);
		r = parent;

		/*
		 * Commit the AVA to the DN. Note that we presently
		 * do not support multi-valued RDNs. 
		 */
		rdn = (LDAPRDN *)ch_malloc(sizeof(LDAPRDN) + 2 * sizeof(LDAPAVA *));
		rdn[0] = (LDAPAVA **)(rdn + 1);
		rdn[0][0] = ava;
		rdn[0][1] = NULL;

		dn = (LDAPDN *)ch_realloc(dn, sizeof(LDAPDN) +
			(depth + 1) * sizeof(LDAPRDN *));
		dn[0] = (LDAPRDN **)(dn + 1);
		dn[0][depth - 1] = rdn;
		dn[0][depth] = NULL;

		depth++;
	}

out:

	if (status == DSStatusOK)
	{
		int rc;

		/* Stringify the DN to return to the caller. */
		rc = ldap_dn2bv(dn, localDN, LDAP_DN_FORMAT_LDAPV3 | LDAP_DN_PRETTY);
		if (rc != LDAP_SUCCESS)
			status = DSStatusInvalidPath;
	}

	if (dn != NULL)
		ldap_dnfree(dn);

	if (r != NULL)
		dsrecord_release(r);

	return status;
}

/*
 * A replacement for dsutil_parse_x500_string_path()
 * which is schema-mapping aware.
 */
dsstatus 
netinfo_back_parse_dn(BackendDB *be, struct berval *path, dsrecord **pr)
{
	dsrecord *r;
	LDAPDN *dn;
	int i, max;
	dsattribute *a;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	u_int32_t dsid;
	dsstatus status;

	if (ldap_bv2dn(path, &dn, LDAP_DN_FORMAT_LDAP) != LDAP_SUCCESS)
		return DSStatusInvalidPath;

	/* Count the number of RDNs. */
	for (i = 0; dn[0][i] != NULL; i++)
		;

	max = i;

	r = dsrecord_new();
	if (r == NULL)
	{
		status = DSStatusFailed;
		goto out;
	}

	if (max == 0)
	{
		status = DSStatusOK;
		goto out;
	}

	max--;
	dsid = 0;

	for (i = max; i >= 0; i--)
	{
		dsdata *value;
		dsrecord *query;
		struct atmap map;
		AttributeDescription *ad = NULL;
		const char *text = NULL;

		/*
		 * Check that the RDN is not multi-valued; we don't
		 * permit those.
		 */
		if (dn[0][i][0][1] != NULL)
		{
			status = DSStatusInvalidPath;
			goto out;
		}

		/*
		 * Find the attribute description.
		 */
		if (slap_bv2ad(&dn[0][i][0][0]->la_attr, &ad, &text) != LDAP_SUCCESS)
		{
			status = DSStatusInvalidKey;
			goto out;
		}

		/*
		 * Un-map the relative distinguished name to a NetInfo
		 * key and value.
		 */
		status = schemamap_x500_to_ni_at(be, dsid, ad, &map);
		if (status != DSStatusOK)
		{
			goto out;
		}

		status = (map.x500ToNiTransform)(be, &value, &dn[0][i][0][0]->la_value, DataTypeCaseUTF8Str, map.x500ToNiArg);
		if (status != DSStatusOK)
		{
			schemamap_atmap_release(&map);
			goto out;
		}

		a = dsattribute_new(map.ni_key);
		if (a == NULL)
		{
			schemamap_atmap_release(&map);
			status = DSStatusFailed;
			goto out;
		}

		dsattribute_append(a, value);
		dsdata_release(value);

		dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);

		schemamap_atmap_release(&map);

		/*
		 * Get the DSID of the currently-traversed path for future
		 * schema mapping.
		 */
		if (i != 0)
		{
			query = dsrecord_new();
			if (query == NULL)
			{
				status = DSStatusFailed;
				dsattribute_release(a);
				goto out;
			}

			dsrecord_append_attribute(query, a, SELECT_ATTRIBUTE);
			status = dsengine_pathmatch(di->engine, dsid, query, &dsid);
			dsrecord_release(query);
		}
		dsattribute_release(a);
		if (status != DSStatusOK)
			goto out;
	}

out:

	ldap_dnfree(dn);

	if (status != DSStatusOK)
		dsrecord_release(r);
	else
		*pr = r;

	return status;
}

/*
 * A cover for dsengine_x500_string_pathmatch() that makes sure the
 * DN is store-relative first. As an optimization, if the DN
 * corresponding to the root directory is supplied, we set
 * *match ourselves.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus netinfo_back_dn_pathmatch(BackendDB *be, struct berval *ndn, u_int32_t *match)
{
	dsstatus status;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct berval localDN;
	dsrecord *path;

	assert(di != NULL);

	status = dnMakeLocal(be, &localDN, ndn);
	if (status != DSStatusOK)
	{
		return status;
	}

	if (localDN.bv_len == 0)
	{
		/* null DN */
		*match = 0;
		return DSStatusOK;
	}

	/* Check for shortcut */
	if (strncasecmp(localDN.bv_val, "DSID=", 5) == 0)
	{
		char *p = NULL;

		*match = strtoul(localDN.bv_val + 5, &p, 10);
		if (p == NULL || (*p != ',' && *p != '\0'))
			status = DSStatusInvalidPath;
		ch_free(localDN.bv_val);
		return status;
	}

	status = netinfo_back_parse_dn(be, &localDN, &path);
	if (status == DSStatusOK)
	{
		status = dsengine_pathmatch(di->engine, 0, path, match);
		dsrecord_release(path);
	}

	ch_free(localDN.bv_val);

	return status;
}

/*
 * A cover for dsengine_x500_string_pathcreate() that makes sure the
 * DN is store-relative first. As an optimization, if the DN
 * corresponding to the root directory is supplied, we set
 * *match ourselves.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus netinfo_back_dn_pathcreate(BackendDB *be, struct berval *ndn, u_int32_t *match)
{
	dsstatus status;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct berval localDN;
	dsrecord *path;

	assert(di != NULL);

	status = dnMakeLocal(be, &localDN, ndn);
	if (status != DSStatusOK)
	{
		return status;
	}

	if (localDN.bv_len == 0)
	{
		/* null DN */
		*match = 0;
		return DSStatusOK;
	}

	status = netinfo_back_parse_dn(be, &localDN, &path);
	if (status == DSStatusOK)
	{
		status = dsengine_pathcreate(di->engine, 0, path, match);
		dsrecord_release(path);
	}

	ch_free(localDN.bv_val);

	return status;
}

/*
 * Check for child referrals. Should coalesce bases.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus netinfo_back_send_referrals(BackendDB *be, Connection *conn, Operation *op, struct berval *nbase)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct netinfo_referral **p;

#ifdef NO_NETINFO_REFERRALS
	return DSStatusInvalidPath;
#endif

	assert(di != NULL);

	if (di->children == NULL)
		return DSStatusInvalidPath;

	if (get_manageDSAit(op))
		return DSStatusInvalidPath;

	for (p = di->children; *p != NULL; p++)
	{
		if (dnIsSuffix(nbase, &(*p)->nnc))
		{
			/* Yes! */
			send_ldap_result(conn, op, LDAP_REFERRAL, (*p)->nc.bv_val, NULL, (*p)->refs, NULL);
			return DSStatusOK;
		}
	}

	return DSStatusInvalidPath;
}

/*
 * Send parent search continuation results.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus netinfo_back_send_references(BackendDB *be, Connection *conn, Operation *op, struct berval *relativeBase, int scope)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct netinfo_referral *parent;
	Entry *e;
	int i;
	BerVarray refs;
	BerVarray v2refs;

	assert(di != NULL);

	if (scope == LDAP_SCOPE_BASE)
		return DSStatusOK;

	if (di->parent == NULL)
		return DSStatusOK;

	if (get_manageDSAit(op))
		return DSStatusOK;

	e = (Entry *)ch_calloc(1, sizeof(Entry));
	e->e_attrs = NULL;
	e->e_private = NULL;
	e->e_id = NOID;
	e->e_bv.bv_val = NULL;
	e->e_ocflags = 0;

	parent = di->parent;
	if (relativeBase->bv_len == 0)
	{
		ber_dupbv(&e->e_name, &parent->nc);
	}
	else
	{
		if (parent->nc.bv_len > 0)
			build_new_dn(&e->e_name, &parent->nc, relativeBase);
		else
			ber_dupbv(&e->e_name, relativeBase);
	}

	if (dnNormalize2(NULL, &e->e_name, &e->e_nname) != LDAP_SUCCESS)
	{
		ch_free(e->e_name.bv_val);
		return DSStatusInvalidPath;
	}

	refs = (BerVarray)ch_malloc((parent->count + 1) * sizeof(struct berval));
	for (i = 0; i < parent->count; i++)
	{
		char *p;

		/* assert dsengine provides us with referrals ending in a / */
		assert(parent->refs[i].bv_val[parent->refs[i].bv_len - 1] == '/');
		refs[i].bv_len = parent->refs[i].bv_len + e->e_name.bv_len +
			sizeof("??XXX") - 1;
		p = refs[i].bv_val = (char *)ch_malloc(refs[i].bv_len + 1);

		AC_MEMCPY(p, parent->refs[i].bv_val, parent->refs[i].bv_len);
		p += parent->refs[i].bv_len;

		AC_MEMCPY(p, e->e_name.bv_val, e->e_name.bv_len);
		p += e->e_name.bv_len;

		/*
		 * Be sure to explicitly specify the scope, RFC 2251
		 * is ambiguous on which scope should be used when
	 	 * chasing referrals, and OpenLDAP will use a base
		 * scope when chasing if the original scope was one-
		 * level.
		 */
		if (scope == LDAP_SCOPE_SUBTREE)
			strcpy(p, "??sub");
		else
			strcpy(p, "??one");
	}

	refs[parent->count].bv_val = NULL;
	refs[parent->count].bv_len = 0;
	v2refs = NULL;

	send_search_reference(be, conn, op, e, refs, NULL, &v2refs);
    
	entry_free(e);
	ber_bvarray_free(refs);
	ber_bvarray_free(v2refs);

	return DSStatusOK;
}

void netinfo_back_entry_free(Entry *ent)
{
	assert(ent != NULL);

	if (ent->e_private != NULL)
	{
		dsrecord_release((dsrecord *)ent->e_private);
		ent->e_private = NULL;
	}
	entry_free(ent);
}

int netinfo_back_entry_release(
	BackendDB *be,
	Connection *c,
	Operation *o,
	Entry *e,
	int rw)
{
	
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);

	/* lock engine in case cache is being modified */

	ENGINE_LOCK(di);
	netinfo_back_entry_free(e);
	ENGINE_UNLOCK(di);

	return 0;
}
