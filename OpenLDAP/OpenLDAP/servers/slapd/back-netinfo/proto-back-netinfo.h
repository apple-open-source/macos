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

#ifndef _NETINFO_PROTO_BACK_NETINFO_H
#define _NETINFO_PROTO_BACK_NETINFO_H

LDAP_BEGIN_DECL

/*
 * Compensate for the root entry's super being
 * 0, so that we don't apply schema mapping 
 * for directories immediately under the root
 * to the root directory itself.
 */
#define SUPER(r) (((r)->dsid == 0) ? -1 : (r)->super)

/* in shim.c */

/* ENGINE MUST ALWAYS BE LOCKED */

/*
 * Return the appropriate dsdata type for an attribute
 * type.
 */
extern u_int32_t ad_to_dsdata_type LDAP_P((
	AttributeDescription *ad));

/*
 * Convert a dsdata into a berval.
 */
extern struct berval *dsdata_to_berval LDAP_P((
	struct berval *dst,
	dsdata *data));

/*
 * Convert a dsdata into a berval in-place.
 */
extern struct berval *dsdata_to_berval_no_copy LDAP_P((
	struct berval *dst,
	dsdata *data));

/*
 * Convert a berval into a dsdata of specified type.
 * If type is DataTypeCStr, then treats bv->bv_val
 * as a nul terminated C string.
 */
extern dsdata *berval_to_dsdata LDAP_P((
	struct berval *bv,
	u_int32_t type));

/*
 * Convert a berval to a dsdata in-place
 */
extern dsdata *berval_to_dsdata_no_copy LDAP_P((
	dsdata *dst,
	struct berval *bv,
	u_int32_t type));

/*
 * Translate a dsrecord into a slapd Entry.
 */
extern dsstatus dsrecord_to_entry LDAP_P((
	BackendDB *db,
	dsrecord *rec,
	Entry **entry));

/*
 * Translate a slapd entry into a dsrecord
 */
extern dsstatus entry_to_dsrecord LDAP_P((
	BackendDB *db,
	u_int32_t parent,
	Entry *e,
	dsrecord **rec));

/*
 * Get all values of an attribute.
 */
extern dsstatus dsattribute_to_bervals LDAP_P((
	BackendDB *be,
	BerVarray *vals,
	dsattribute *a,
	struct atmap *map));

extern void netinfo_back_entry_free LDAP_P((Entry *e));

/*
 * Map errors between dsstore and LDAP.
 */
extern int dsstatus_to_ldap_err LDAP_P((
	dsstatus status));

/*
 * Send back a result to the client based on the
 * dsstatus code.
 */
extern int netinfo_back_op_result LDAP_P((
	struct slap_op *op, 
	struct slap_rep *rs, 
	dsstatus status));

/*
 * Parse a local DN into a hierarchical path,
 * after schema mapping.
 */
extern dsstatus netinfo_back_parse_dn LDAP_P((
	BackendDB *be,
	struct berval *path,
	dsrecord **pr));

/*
 * A replacement for dsengine_x500_string_path() that
 * is schema-mapping aware (returns global DN).
 */
extern dsstatus netinfo_back_global_dn LDAP_P((BackendDB *be,
	u_int32_t dsid,
	struct berval *path));

/*
 * A replacement for dsengine_x500_string_path() that
 * is schema-mapping aware (returns local DN).
 */
extern dsstatus netinfo_back_local_dn LDAP_P((BackendDB *be,
	u_int32_t dsid,
	struct berval *path));

/*
 * Cover for dsengine_x500_string_pathmatch() that makes
 * the DN store-relative first.
 */
extern dsstatus netinfo_back_dn_pathmatch LDAP_P((
	BackendDB *be,
	struct berval *ndn,
	u_int32_t *match));

/*
 * Cover for dsengine_x500_string_pathcreate() that makes
 * the DN store-relative first.
 */
extern dsstatus netinfo_back_dn_pathcreate LDAP_P((
	BackendDB *be,
	struct berval *ndn,
	u_int32_t *match));

/*
 * Send child referrals if necessary. Returns DSStatusInvalidPath
 * if we should look in the local store instead.
 */
extern dsstatus netinfo_back_send_referrals LDAP_P((
	struct slap_op *op, 
	struct slap_rep *rs, 
	struct berval *nbase));

/*
 * Send search continuation results.
 */
extern dsstatus netinfo_back_send_references LDAP_P((
	struct slap_op *op, 
	struct slap_rep *rs, 
	struct berval *relativeBase));

/*
 * Make a distinguished name store-relative (ie. strip off
 * any known suffixes).
 */
extern dsstatus dnMakeLocal LDAP_P((
	BackendDB *be,
	struct berval *localDN,
	struct berval *ndn));

/*
 * Make a distinguished name absolute (add the canonical
 * backend suffix).
 */
extern dsstatus dnMakeGlobal LDAP_P((BackendDB *be,
	struct berval *globalDN,
	struct berval *dn));

/* in init.c */

/*
 * Get DIT info.
 */
extern dsstatus netinfo_back_get_ditinfo LDAP_P((
	dsengine *s,
	struct berval *psuffix,
	struct berval *pnsuffix,
	struct netinfo_referral **prefs,
	struct netinfo_referral ***children));

/* in schemamap.c */

/*
 * Return the NetInfo attribute for an LDAP
 * attribute type.
 */
extern dsstatus schemamap_x500_to_ni_at LDAP_P((
	BackendDB *be,
	u_int32_t dsid,
	AttributeDescription *desc,
	struct atmap *map));

/*
 * Return the LDAP attribute type for a NetInfo
 * attribute.
 */
extern dsstatus schemamap_ni_to_x500_at LDAP_P((
	BackendDB *be,
	u_int32_t dsid,
	dsdata *name,
	u_int32_t sel,
	struct atmap *map));

/*
 * Enforce DIT structure rules on add.
 */
extern dsstatus schemamap_validate_objectclasses LDAP_P((
	BackendDB *be,
	u_int32_t dsid,
	Entry *e));

/*
 * Enforce DIT structure rules on modify.
 */
extern dsstatus schemamap_validate_objectclass_mods LDAP_P((
	BackendDB *be,
	u_int32_t dsid,
	Modification *mod));

/*
 * Add an objectclass chain to an entry based on its
 * location in the DIT.
 */
extern void schemamap_add_objectclasses LDAP_P((
	BackendDB *be,
	u_int32_t dsid,
	Entry *e));

/*
 * Add a mapping between NetInfo directory and
 * implied objectclass chain.
 */
extern int schemamap_add_oc LDAP_P((
	BackendDB *be,
	const char *where,
	int argc,
	const char **argv));

/*
 * Add a mapping between (bidirectional) a NetInfo
 * and LDAP attribute, optionally for a specific
 * part of the DIT.
 */
extern int schemamap_add_at LDAP_P((
	BackendDB *be,
	const char *where,
	const char *netinfo,
	const char *x500,
	const char *ni_to_x500_sym,
	const char *ni_to_x500_arg,
	const char *x500_to_ni_sym,
	const char *x500_to_ni_arg));

/*
 * Determine whether an objectclass should be
 * implied for an entry (doesn't check 
 * real values of objectClass).
 */
extern int schemamap_check_oc LDAP_P((
	BackendDB *be,
	u_int32_t dsid,
	struct berval *ava));

extern int schemamap_check_structural_oc LDAP_P((
	BackendDB *be,
	u_int32_t dsid,
	struct berval *ava));

extern void schemamap_destroy LDAP_P((BackendDB *be));

extern void schemamap_init LDAP_P((BackendDB *be));

extern void schemamap_atmap_release LDAP_P((struct atmap *atmap));

/* in authz.c */

extern dsstatus netinfo_back_authorize LDAP_P((
	Operation *op,
	dsrecord *r,
	AttributeDescription *desc,
	slap_access_t access));

extern dsstatus netinfo_back_access_allowed LDAP_P((
	Operation *op,
	u_int32_t dsid,
	AttributeDescription *desc,
	struct berval *val,
	slap_access_t access));

extern dsstatus is_trusted_network LDAP_P((
	BackendDB *be,
	Connection *conn));

/* in filter.c */


/*
 * Translate a slapd Filter into a dsfilter.
 */
extern dsfilter *filter_to_dsfilter LDAP_P((
	BackendDB *db,
	Filter *filter));

/*
 * Translate an attribute assertion to a dsassertion.
 */
extern dsassertion *attribute_assertion_to_dsassertion LDAP_P((
	BackendDB *db,
	AttributeAssertion *ava,
	ber_tag_t choice));

/*
 * Filter test delegate for schema mapping.
 */
extern Logic3 wrapped_filter_test LDAP_P((
	dsfilter *filter,
	dsrecord *record,
	void *db));

/*
 * Assertion test delegate for schema mapping.
 */
extern Logic3 wrapped_assertion_test LDAP_P((dsassertion *t,
	dsrecord *r,
	void *private));

/* in transforms.c */

dsstatus distinguishedNameToPosixNameTransform LDAP_P((BackendDB *be,
	dsdata **dst,
	struct berval *src,
	u_int32_t type,
	void *private));

dsstatus posixNameToDistinguishedNameTransform LDAP_P((BackendDB *be,
	struct berval *dst,
	dsdata *src,
	void *private));

dsstatus appendPrefixTransform LDAP_P((BackendDB *be,
	struct berval *dst,
	dsdata *src,
	void *private));

#define appendCaseIgnorePrefixTransform appendPrefixTransform
#define appendCaseExactPrefixTransform appendPrefixTransform

dsstatus removeCaseIgnorePrefixTransform LDAP_P((BackendDB *be,
	dsdata **dst,
	struct berval *src,
	u_int32_t type,
	void *private));

dsstatus removeCaseExactPrefixTransform LDAP_P((BackendDB *be,
	dsdata **dst,
	struct berval *src,
	u_int32_t type,
	void *private));

#define removePrefixTransform removeCaseIgnorePrefixTransform

dsstatus distinguishedNameRetrieveTransform LDAP_P((BackendDB *be,
	struct berval *dst,
	dsdata *src,
	void *private));

dsstatus distinguishedNameStoreTransform LDAP_P((BackendDB *be,
	dsdata **dst,
	struct berval *bv,
	u_int32_t type,
	void *private));

dsstatus bervalToDsdata LDAP_P((BackendDB *be,
	dsdata **dst,
	struct berval *bv,
	u_int32_t type,
	void *private));

dsstatus dsdataToBerval LDAP_P((BackendDB *be,
	struct berval *dst,
	dsdata *src,
	void *private));

LDAP_END_DECL

#endif /* _NETINFO_PROTO_BACK_NETINFO_H */
