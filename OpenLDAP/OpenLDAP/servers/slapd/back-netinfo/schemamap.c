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
#include <ac/ctype.h>

#include "slap.h"
#include "back-netinfo.h"
#include "avl.h"

struct schemamapping {
	Avlnode *x500_to_ni;
	Avlnode *ni_to_x500;
};

struct schemamapinfo {
	struct schemamapping ocs;
	struct schemamapping global;
	struct schemamapping contextual;
};

struct ocmap {
	/* dsid of super record */
	u_int32_t super;

	/* objectClass chain to merge */
	ObjectClass **chain;

	/* retain count */
	u_int32_t retain;
};

static int ocmap_cmp LDAP_P((
	struct ocmap *om1,
	struct ocmap *om2));

static int atmap_x500_global_cmp LDAP_P((
	struct atmap *am1,
	struct atmap *am2));

static int atmap_x500_contextual_cmp LDAP_P((
	struct atmap *am1,
	struct atmap *am2));

static int atmap_ni_global_cmp LDAP_P((
	struct atmap *am1,
	struct atmap *am2));

static int atmap_ni_contextual_cmp LDAP_P((
	struct atmap *am1,
	struct atmap *am2));

static void atmap_release LDAP_P((
	struct atmap *am));

static void ocmap_release LDAP_P((
	struct ocmap *om));

static struct atmap *schemamap_find_ni_at LDAP_P((
	Backend *be,
	u_int32_t where,
	dsdata *name,
	u_int32_t sel));

static struct atmap *schemamap_find_x500_at LDAP_P((
	Backend *be,
	u_int32_t dsid,
	AttributeDescription *desc));

static struct ocmap *schemamap_find_oc LDAP_P((
	Backend *be,
	u_int32_t where));


static struct ni_to_x500_transformtab {
	const char *name;
	ni_to_x500_transform_t transform;
} ni_to_x500_transformtab[] = {
	{ "posixNameToDistinguishedNameTransform", posixNameToDistinguishedNameTransform },
	{ "appendPrefixTransform", appendPrefixTransform },
	{ "appendCaseIgnorePrefixTransform", appendCaseIgnorePrefixTransform },
	{ "appendCaseExactPrefixTransform", appendCaseExactPrefixTransform },
	{ "distinguishedNameRetrieveTransform", distinguishedNameRetrieveTransform },
	{ "dsdataToBerval", dsdataToBerval },
	{ NULL, NULL }
};

static struct x500_to_ni_transformtab {
	const char *name;
	x500_to_ni_transform_t transform;
} x500_to_ni_transformtab[] = {
	{ "distinguishedNameToPosixNameTransform", distinguishedNameToPosixNameTransform },
	{ "removePrefixTransform", removePrefixTransform },
	{ "removeCaseIgnorePrefixTransform", removeCaseIgnorePrefixTransform },
	{ "removeCaseExactPrefixTransform", removeCaseExactPrefixTransform },
	{ "distinguishedNameStoreTransform", distinguishedNameStoreTransform },
	{ "bervalToDsdata", bervalToDsdata },
	{ NULL, NULL }
};

static int ocmap_cmp(struct ocmap *om1, struct ocmap *om2)
{
	return (om1->super) - (om2->super);
}

static int atmap_x500_global_cmp(struct atmap *am1, struct atmap *am2)
{
	return ad_cmp(am1->x500, am2->x500);
}

static int atmap_x500_contextual_cmp(struct atmap *am1, struct atmap *am2)
{
	int s;

	s = (am1->super - am2->super);
	if (s == 0)
	{
		s = ad_cmp(am1->x500, am2->x500);
	}

	return s;
}

static int atmap_ni_global_cmp(struct atmap *am1, struct atmap *am2)
{
	int s;

	s = am1->selector - am2->selector;
	if (s == 0)
	{
		s = dsdata_compare(am1->ni_key, am2->ni_key);
	}

	return s;
}

static int atmap_ni_contextual_cmp(struct atmap *am1, struct atmap *am2)
{
	int s;

	s = am1->super - am2->super;
	if (s == 0)
	{
		s = am1->selector - am2->selector;
		if (s == 0)
		{
			s = dsdata_compare(am1->ni_key, am2->ni_key);
		}
	}

	return s;
}

/*
 * to be called from outside this module; only releases
 * NetInfo attribute name, nothing owned by the AVL tree.
 */
void schemamap_atmap_release(struct atmap *am)
{
	assert(am != NULL);
	am->retain--;

	if (am->retain > 0)
		return;

	assert(am->ni_key != NULL);
	dsdata_release(am->ni_key);
}

/*
 * Internal AVL attribute map tree release function.
 */
static void atmap_release(struct atmap *am)
{
	assert(am != NULL);

	schemamap_atmap_release(am);

	if (am->retain > 0)
		return;

	if (am->niToX500Arg != NULL)
		ber_bvfree((struct berval *)am->niToX500Arg);

	if (am->x500ToNiArg != NULL)
		ber_bvfree((struct berval *)am->x500ToNiArg);

	ch_free(am);
}

static void ocmap_release(struct ocmap *om)
{
	assert(om != NULL);
	om->retain--;

	if (om->retain > 0)
		return;

	ch_free(om->chain);

	ch_free(om);
}

void schemamap_destroy(BackendDB *be)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);

	avl_free(di->map->global.x500_to_ni, (AVL_FREE)atmap_release);
	avl_free(di->map->global.ni_to_x500, (AVL_FREE)atmap_release);
	avl_free(di->map->contextual.x500_to_ni, (AVL_FREE)atmap_release);
	avl_free(di->map->contextual.ni_to_x500, (AVL_FREE)atmap_release);
	avl_free(di->map->ocs.x500_to_ni, (AVL_FREE)ocmap_release);
	avl_free(di->map->ocs.ni_to_x500, (AVL_FREE)ocmap_release);
}

static struct atmap *schemamap_find_ni_at(
	Backend *be,
	u_int32_t where,
	dsdata *name,
	u_int32_t sel)
{
	struct schemamapinfo *m = (struct schemamapinfo *)((struct dsinfo *)be->be_private)->map;
	struct atmap *am = NULL;
	struct atmap key;

	assert(m != NULL);

	key.super = where;
	key.ni_key = name;
	key.selector = sel;

	/* look for a context-specific mapping */
	if (where != (u_int32_t)-1 && m->contextual.ni_to_x500 != NULL)
	{
		am = (struct atmap *)avl_find(m->contextual.ni_to_x500, &key, (AVL_CMP)atmap_ni_contextual_cmp);
	}

	if (am == NULL && m->global.ni_to_x500 != NULL)
	{
		am = (struct atmap *)avl_find(m->global.ni_to_x500, &key, (AVL_CMP)atmap_ni_global_cmp);
	}

	if (am != NULL)
	{
		assert(am->ni_key != NULL);
		assert(am->x500 != NULL);
	}

	return am;
}

static struct atmap *schemamap_find_x500_at(
	Backend *be,
	u_int32_t where,
	AttributeDescription *desc)
{
	struct schemamapinfo *m = (struct schemamapinfo *)((struct dsinfo *)be->be_private)->map;
	struct atmap *am = NULL;
	struct atmap key;

	assert(m != NULL);

	key.super = where;
	key.x500 = desc;

	/* look for a context-specific mapping */
	if (where != (u_int32_t)-1 && m->contextual.x500_to_ni != NULL)
	{
		am = (struct atmap *)avl_find(m->contextual.x500_to_ni, &key, (AVL_CMP)atmap_x500_contextual_cmp);
	}

	if (am == NULL && m->global.x500_to_ni != NULL)
	{
		am = (struct atmap *)avl_find(m->global.x500_to_ni, &key, (AVL_CMP)atmap_x500_global_cmp);
	}

	if (am != NULL)
	{
		assert(am->ni_key != NULL);
		assert(am->x500 != NULL);
	}

	return am;
}

static struct ocmap *schemamap_find_oc(
	Backend *be,
	u_int32_t where)
{
	struct schemamapinfo *m = (struct schemamapinfo *)((struct dsinfo *)be->be_private)->map;
	struct ocmap *om;
	struct ocmap key;

	assert(m != NULL);

	if (m->ocs.ni_to_x500 == NULL)
	{
		return NULL;
	}

	key.super = where;
	om = (struct ocmap *)avl_find(m->ocs.ni_to_x500, &key, (AVL_CMP)ocmap_cmp);
	if (om != NULL)
	{
		assert(om->chain != NULL);
	}

	return om;
}

int schemamap_add_oc(
	BackendDB *be,
	const char *where,
	int argc,
	const char **argv
)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct ocmap *om;
	dsstatus status;
	int i;

	assert(di != NULL);
	assert(di->engine != NULL);
	assert(where != NULL);
	assert(argc > 0);

	om = (struct ocmap *)ch_calloc(1, sizeof(struct ocmap));
	om->retain = 1;
	status = dsengine_netinfo_string_pathmatch(di->engine, 0, (char *)where, &om->super);
	if (status != DSStatusOK)
	{
		ch_free(om);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_oc: "
			"Could not add objectClass mapping for directory %s: %s\n",
			where, dsstatus_message(status)));
#else
		Debug(LDAP_DEBUG_ANY, "schemamap_add_oc: "
			"Could not add objectClass mapping for directory %s: %s\n",
			where, dsstatus_message(status), 0);
#endif
		return -1;
	}

	/* NB: first object class is assumed to be structural object class. */
	om->chain = (ObjectClass **)ch_calloc(argc + 1, sizeof(ObjectClass *));
	for (i = 0; i < argc; i++)
	{
		om->chain[i] = oc_find(argv[i]);
		if (om->chain[i] == NULL)
		{
			ocmap_release(om);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_oc: "
				"Could not find objectClass %s\n", argv[i]));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_oc: "
				"Could not find objectClass %s\n", argv[i], 0, 0);
#endif
			return -1;
		}
	}
	om->chain[argc] = NULL;

	if (avl_insert(&di->map->ocs.ni_to_x500, (caddr_t)om,
		(AVL_CMP)ocmap_cmp, (AVL_DUP)avl_dup_error))
	{
		ocmap_release(om);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_oc: "
			"Could not add objectClass mapping for directory %s\n", where));
#else
		Debug(LDAP_DEBUG_ANY, "schemamap_add_oc: "
			"Could not add objectClass mapping for directory %s\n", where, 0, 0);
#endif
		return -1;
	}

	om->retain++;
	if (avl_insert(&di->map->ocs.x500_to_ni, (caddr_t)om,
		(AVL_CMP)ocmap_cmp, (AVL_DUP)avl_dup_ok))
	{
		ocmap_release(om);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_oc: "
			"Could not add objectClass mapping for directory %s\n", where));
#else
		Debug(LDAP_DEBUG_ANY, "schemamap_add_oc: "
			"Could not add objectClass mapping for directory %s\n", where, 0, 0);
#endif
		return -1;
	}

	return 0;
}

/* Automatic object class insertion info */
int schemamap_add_at(
	BackendDB *be,
	const char *where,
	const char *netinfo,
	const char *x500,
	const char *ni_to_x500_sym,
	const char *niToX500Arg,
	const char *x500_to_ni_sym,
	const char *x500ToNiArg)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct atmap *am;
	const char *text;
	dsstatus status;
	int i;

	assert(di != NULL);
	assert(di->engine != NULL);
	assert(netinfo != NULL);
	assert(x500 != NULL);

	am = (struct atmap *)ch_calloc(1, sizeof(struct atmap));
	am->retain = 1;
	am->x500ToNiTransform = NULL;
	am->x500ToNiArg = NULL;
	am->niToX500Transform = NULL;
	am->niToX500Arg = NULL;

	if (where == NULL)
	{
		am->super = (u_int32_t)-1;
	}
	else
	{
		status = dsengine_netinfo_string_pathmatch(di->engine, 0, (char *)where, &am->super);
		if (status != DSStatusOK)
		{
			ch_free(am);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s: %s\n"
				netinfo, dsstatus_message(status)));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s: %s\n",
				netinfo, dsstatus_message(status), 0);
#endif
			return -1;
		}
	}

	if (netinfo[0] == '_')
	{
		am->ni_key = cstring_to_dsdata((char *)netinfo + 1);
		am->selector = SELECT_META_ATTRIBUTE;
	}
	else
	{
		am->ni_key = cstring_to_dsdata((char *)netinfo);
		am->selector = SELECT_ATTRIBUTE;
	}

	assert(am->ni_key != NULL);

	am->x500 = NULL;
	if (slap_str2ad(x500, &am->x500, &text) != LDAP_SUCCESS)
	{
		atmap_release(am);
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
			"Could not add attribute mapping for NetInfo attribute %s: %s\n",
			netinfo, text));
#else
		Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
			"Could not add attribute mapping for NetInfo attribute %s: %s\n",
			netinfo, text, 0);
#endif
		return -1;
	}

	assert(am->x500 != NULL);

	am->type = ad_to_dsdata_type(am->x500);

	if (ni_to_x500_sym != NULL)
	{
		for (i = 0; ni_to_x500_transformtab[i].name != NULL; i++)
		{
			if (strcmp(ni_to_x500_transformtab[i].name, ni_to_x500_sym) == 0)
			{
				assert(ni_to_x500_transformtab[i].transform != NULL);
				am->niToX500Transform = ni_to_x500_transformtab[i].transform;
				break;
			}
		}
		if (am->niToX500Transform == NULL)
		{
			atmap_release(am);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
				"Could not find NetInfo to X.500 transform %s\n", ni_to_x500_sym));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
				"Could not find NetInfo to X.500 transform %s\n", ni_to_x500_sym, 0, 0);
#endif
			return -1;
		}
	}
	else
	{
		am->niToX500Transform = (am->type == DataTypeDirectoryID) ?
			distinguishedNameRetrieveTransform : dsdataToBerval;
	}

	assert(am->niToX500Transform != NULL);

	if (niToX500Arg != NULL)
	{
		struct berval *bv = (struct berval *)ch_malloc(sizeof(struct berval));

		bv->bv_val = ch_strdup(niToX500Arg);
		bv->bv_len = strlen(bv->bv_val);

		am->niToX500Arg = bv;
	}

	if (x500_to_ni_sym != NULL)
	{
		for (i = 0; x500_to_ni_transformtab[i].name != NULL; i++)
		{
			if (strcmp(x500_to_ni_transformtab[i].name, x500_to_ni_sym) == 0)
			{
				assert(x500_to_ni_transformtab[i].transform != NULL);
				am->x500ToNiTransform = x500_to_ni_transformtab[i].transform;
				break;
			}
		}
		if (am->x500ToNiTransform == NULL)
		{
			atmap_release(am);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
				"Could not find X.500 to NetInfo transform %s\n", x500_to_ni_sym));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
				"Could not find X.500 to NetInfo transform %s\n", x500_to_ni_sym, 0, 0);
#endif
			return -1;
		}
	}
	else
	{
		am->x500ToNiTransform = (am->type == DataTypeDirectoryID) ?
			distinguishedNameStoreTransform : bervalToDsdata;
	}

	assert(am->x500ToNiTransform != NULL);

	if (x500ToNiArg != NULL)
	{
		struct berval *bv = (struct berval *)ch_malloc(sizeof(struct berval));

		bv->bv_val = ch_strdup(x500ToNiArg);
		bv->bv_len = strlen(bv->bv_val);

		am->x500ToNiArg = bv;
	}

	if (am->super == (u_int32_t)-1)
	{
		if (avl_insert(&di->map->global.ni_to_x500, (caddr_t)am,
			(AVL_CMP)atmap_ni_global_cmp, (AVL_DUP)avl_dup_ok))
		{
			atmap_release(am);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo, 0, 0);
#endif
			return -1;
		}

		am->retain++;
		if (avl_insert(&di->map->global.x500_to_ni, (caddr_t)am,
			(AVL_CMP)atmap_x500_global_cmp, (AVL_DUP)avl_dup_ok))
		{
			atmap_release(am);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo, 0, 0);
#endif
			return -1;
		}
	}
	else
	{
		if (avl_insert(&di->map->contextual.ni_to_x500, (caddr_t)am,
			(AVL_CMP)atmap_ni_contextual_cmp, (AVL_DUP)avl_dup_ok))
		{
			atmap_release(am);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo, 0, 0);
#endif
			return -1;
		}
		
		am->retain++;
		if (avl_insert(&di->map->contextual.x500_to_ni, (caddr_t)am,
			(AVL_CMP)atmap_x500_contextual_cmp, (AVL_DUP)avl_dup_ok))
		{
			atmap_release(am);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo));
#else
			Debug(LDAP_DEBUG_ANY, "schemamap_add_at: "
				"Could not add attribute mapping for NetInfo attribute %s\n", netinfo, 0, 0);
#endif
			return -1;
		}
	}

	return 0;
}

dsstatus schemamap_x500_to_ni_at(
	BackendDB *be,
	u_int32_t dsid,
	AttributeDescription *desc,
	struct atmap *map)
{
	struct atmap *am;
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);
	assert(desc != NULL);
	assert(map != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "schemamap_x500_to_ni_at: "
		"dSID %u X.500 attribute %s\n", dsid, desc->ad_cname.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> schemamap_x500_to_ni_at: dsid=%u desc=%s\n",
		dsid, desc->ad_cname.bv_val, 0);
#endif

	am = schemamap_find_x500_at(be, dsid, desc);
	if (am == NULL)
	{
		map->super = dsid;
		map->ni_key = berval_to_dsdata(&desc->ad_cname, DataTypeCStr);
		map->selector = SELECT_ATTRIBUTE;
		map->x500 = desc;
		map->type = ad_to_dsdata_type(desc);
		map->retain = 1;
		if (map->type == DataTypeDirectoryID)
		{
			map->niToX500Transform = distinguishedNameRetrieveTransform;
			map->x500ToNiTransform = distinguishedNameStoreTransform;
		}
		else
		{
			map->niToX500Transform = dsdataToBerval;
			map->x500ToNiTransform = bervalToDsdata;
		}
		map->niToX500Arg = NULL;
		map->x500ToNiArg = NULL;
	}
	else
	{
		AC_MEMCPY(map, am, sizeof(struct atmap));
		dsdata_retain(map->ni_key);
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_x500_to_ni_at: "
		"NetInfo %sattribute %s\n", map->selector ? "meta-" : "",
		dsdata_to_cstring(map->ni_key)));
#else
	Debug(LDAP_DEBUG_TRACE, "<== schemamap_x500_to_ni_at: attribute=%s meta=%d\n",
		dsdata_to_cstring(map->ni_key), map->selector, 0);
#endif

	return DSStatusOK;
}

dsstatus schemamap_ni_to_x500_at(
	BackendDB *be,
	u_int32_t dsid,
	dsdata *name,
	u_int32_t sel,
	struct atmap *map)
{
	struct atmap *am;
	const char *text = "Success";
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);
	assert(name != NULL);
	assert(map != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "schemamap_ni_to_x500_at: "
		"dSID %u NetInfo %sattribute %s\n", dsid, sel ? "meta-" : "",
		dsdata_to_cstring(name)));
#else
	Debug(LDAP_DEBUG_TRACE, "==> schemamap_ni_to_x500_at: dsid=%u attribute=%s meta=%d\n",
		dsid, dsdata_to_cstring(name), sel);
#endif

	am = schemamap_find_ni_at(be, dsid, name, sel);
	if (am == NULL)
	{
		char *type;

		/* meta attributes require explicit mapping  */
		if (sel == SELECT_META_ATTRIBUTE)
			return DSStatusInvalidKey;

		type = dsdata_to_cstring(name);
		if (type == NULL)
			return DSStatusInvalidKey;

		map->x500 = NULL;
	
		if (slap_str2ad(type, &map->x500, &text) != LDAP_SUCCESS)
			return DSStatusInvalidKey;

		map->super = dsid;
		map->ni_key = name;
		dsdata_retain(map->ni_key);
		map->selector = sel;
		map->type = ad_to_dsdata_type(map->x500);
		map->retain = 1;
		if (map->type == DataTypeDirectoryID)
		{
			map->niToX500Transform = distinguishedNameRetrieveTransform;
			map->x500ToNiTransform = distinguishedNameStoreTransform;
		}
		else
		{
			map->niToX500Transform = dsdataToBerval;
			map->x500ToNiTransform = bervalToDsdata;
		}
		map->niToX500Arg = NULL;
		map->x500ToNiArg = NULL;
	}
	else
	{
		AC_MEMCPY(map, am, sizeof(struct atmap));
		dsdata_retain(map->ni_key);
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_ni_to_x500_at: "
		"X.500 attribute %s [%s]\n", map->x500 ? map->x500->ad_cname.bv_val : "(null)", text));
#else
	Debug(LDAP_DEBUG_TRACE, "<== schemamap_ni_to_x500_at: desc=%s, text=%s\n",
		map->x500 ? map->x500->ad_cname.bv_val : "(null)", text, 0);
#endif

	return DSStatusOK;
}

dsstatus schemamap_validate_objectclass_mods(
	BackendDB *be,
	u_int32_t dsid,
	Modification *mod)
{
	struct ocmap *om;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	ObjectClass **p;
	int op = mod->sm_op & ~LDAP_MOD_BVALUES;

	assert(di != NULL);
	assert(mod != NULL);

	if (op == LDAP_MOD_ADD)
		return DSStatusOK;

	/* No OC mapping, no structure rules. */
	om = schemamap_find_oc(be, dsid);
	if (om == NULL)
		return DSStatusOK;

	/*
	 * Check each mapped object class to ensure that one is
	 * not being deleted, or that replaced object classes
	 * include all mapped object classes.
	 */
	for (p = om->chain; *p != NULL; p++)
	{
		BerVarray q;
		dsstatus status = DSStatusConstraintViolation;
		ObjectClass *mapped = *p, *real;

		/*
		 * If mapped object class does not mandate any
		 * attributes other than objectClass, then don't require
		 * that it be preserved.
		 */
		if (mapped->soc_required == NULL)
			continue;

		if (strncasecmp("objectClass",
			mapped->soc_required[0]->sat_cname.bv_val,
			mapped->soc_required[0]->sat_cname.bv_len) == 0 &&
			mapped->soc_required[1] == NULL)
			continue;

		for (q = mod->sm_values; q->bv_val != NULL; q++)
		{
			real = oc_bvfind(q);

			if (op == LDAP_MOD_DELETE)
			{
				/* Cannot delete superclass of mapped object class. */
				if (is_object_subclass(real, mapped))
				{
					/* can't delete mapped object class */
					return DSStatusConstraintViolation;
				}
			}
			else
			{
				if (is_object_subclass(mapped, real))
				{
					/* Can add subclass of mapped object class. */
					status = DSStatusOK;
					break;
				}
			}
		}

		if (status != DSStatusOK)
			return status;
	}

	return DSStatusOK;
}

dsstatus schemamap_validate_objectclasses(
	BackendDB *be,
	u_int32_t dsid,
	Entry *ent)
{
	Attribute *at;
	struct ocmap *om;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	ObjectClass *sup;
	ObjectClass *sub;
	ObjectClass **p;

	assert(di != NULL);
	assert(ent != NULL);

	om = schemamap_find_oc(be, dsid);
	if (om == NULL)
	{
		/* No objectclass map, anything permitted. */
		return DSStatusOK;
	}

	/* Check structural object class first. */
	at = attr_find(ent->e_attrs, slap_schema.si_ad_structuralObjectClass);
	if (at == NULL)
	{
		/* No structural object class. */
		return DSStatusOK;
	}

	sup = om->chain[0];
	sub = oc_bvfind(&at->a_vals[0]);

	/*
	 * Check if actual structural object class is a subclass of
	 * or equal to mapped object class. 
	 */
	if (is_object_subclass(sup, sub) == 0)
	{
		/* Incorrect structural object class. */
		return DSStatusConstraintViolation;
	}

	/* Check all object classes. */
	at = attr_find(ent->e_attrs, slap_schema.si_ad_objectClass);
	if (at == NULL)
	{
		/* No object class. */
		return DSStatusOK;
	}

	/*
	 * Check each mapped object class to see it or a subclass
	 * is included in the entry.
	 */
	for (p = om->chain; *p != NULL; p++)
	{
		BerVarray q;
		dsstatus status = DSStatusConstraintViolation;
	
		sup = *p;

		/*
		 * If mapped object class does not mandate any
		 * attributes other than objectClass, then don't require
		 * that it be added.
		 */
		if (sup->soc_required == NULL)
			continue;

		if (strncasecmp("objectClass",
			sup->soc_required[0]->sat_cname.bv_val,
			sup->soc_required[0]->sat_cname.bv_len) == 0 &&
			sup->soc_required[1] == NULL)
			continue;
	
		for (q = at->a_vals; q->bv_val != NULL; q++)
		{
			sub = oc_bvfind(q);
			if (is_object_subclass(sup, sub))
			{
				status = DSStatusOK;
				break;
			}
		}

		/* Mapped object class or subclass not present. */
		if (status != DSStatusOK)
			return status;
	}

	return DSStatusOK;
}

void schemamap_add_objectclasses(
	BackendDB *be,
	u_int32_t dsid,
	Entry *ent)
{
	struct ocmap *om;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	AttributeDescription *ad = slap_schema.si_ad_objectClass;
	ObjectClass **p;
	struct berval bv[2];
	ObjectClass *oc;

	assert(di != NULL);
	assert(ent != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "schemamap_add_objectclasses: enter\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> schemamap_add_objectclasses\n", 0, 0, 0);
#endif

	/*
	 * Only add mapped object classes if the entry does not already contain
	 * an objectClass or structuralObjectClass attribute.
	 */
	om = schemamap_find_oc(be, dsid);
	if (om != NULL)
	{
		assert(om->chain != NULL);

		if (attr_find(ent->e_attrs, slap_schema.si_ad_objectClass) == NULL)
		{
			ad = slap_schema.si_ad_objectClass;

			/* Don't merge superclasses for now. */
			for (p = om->chain; *p != NULL; p++)
			{
				oc = *p;

				bv[0].bv_val = (oc->soc_names != NULL) ? oc->soc_names[0] : oc->soc_oid;
				assert(bv[0].bv_val != NULL);
				bv[0].bv_len = strlen(bv[0].bv_val);
				bv[1].bv_val = NULL;

				attr_merge(ent, ad, bv, NULL);
			}
		}

		/* XXX should we move this check into the above block? */
		if (attr_find(ent->e_attrs, slap_schema.si_ad_structuralObjectClass) == NULL)
		{
			ad = slap_schema.si_ad_structuralObjectClass;
			oc = om->chain[0];

			bv[0].bv_val = (oc->soc_names != NULL) ? oc->soc_names[0] : oc->soc_oid;
			assert(bv[0].bv_val != NULL);
			bv[0].bv_len = strlen(bv[0].bv_val);
			bv[1].bv_val = NULL;

			attr_merge(ent, ad, bv, NULL);
		}
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_add_objectclasses: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== schemamap_add_objectclasses\n", 0, 0, 0);
#endif
}

void schemamap_init(BackendDB *be)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);

	di->map = (struct schemamapinfo *)ch_calloc(1, sizeof(struct schemamapinfo));
}

int schemamap_check_oc(
	BackendDB *be,
	u_int32_t dsid,
	struct berval *va)
{
	struct ocmap *om;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	ObjectClass **p, *assertedOC;

	assert(di != NULL);
	assert(va != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "schemamap_check_oc: "
		"dSID %u value %s\n",
		dsid, va->bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> schemamap_check_oc: dsid=%u val=%s\n",
		dsid, va->bv_val, 0);
#endif

	assertedOC = oc_bvfind(va);
	if (assertedOC == NULL)
		return 0;

	om = schemamap_find_oc(be, dsid);
	if (om != NULL)
	{
		assert(om->chain != NULL);

		for (p = om->chain; *p != NULL; p++)
		{
			if (is_object_subclass(assertedOC, *p))
			{
#ifdef NEW_LOGGING
				LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_check_oc: found mapped objectclass"));
#else
				Debug(LDAP_DEBUG_TRACE, "<== schemamap_check_oc (found mapped)\n", 0, 0, 0);
#endif
				return 1;
			}
		}
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_check_oc: did not find objectclass"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== schemamap_check_oc (not found)\n", 0, 0, 0);
#endif

	return 0;
}

int schemamap_check_structural_oc(
	BackendDB *be,
	u_int32_t dsid,
	struct berval *va)
{
	struct ocmap *om;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	ObjectClass *assertedOC;

	assert(di != NULL);
	assert(va != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "schemamap_check_structural_oc: "
		"dSID %u value %s\n",
		dsid, va->bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> schemamap_check_structural_oc: dsid=%u val=%s\n",
		dsid, va->bv_val, 0);
#endif

	assertedOC = oc_bvfind(va);
	if (assertedOC == NULL)
		return 0;

	om = schemamap_find_oc(be, dsid);
	if (om != NULL)
	{
		assert(om->chain != NULL);

		if (is_object_subclass(assertedOC, om->chain[0]))
		{
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_check_structural_oc: found mapped objectclass"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== schemamap_check_structural_oc (found mapped)\n", 0, 0, 0);
#endif
			return 1;
		}
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "schemamap_check_structural_oc: did not find objectclass"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== schemamap_check_structural_oc (not found)\n", 0, 0, 0);
#endif

	return 0;
}

