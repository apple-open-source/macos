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
#include "ldap_schema.h"
#include "back-netinfo.h"

#include <ldap_pvt.h>
#include <ldap_utf8.h>

#include <NetInfo/utf-8.h>

static ldap_pvt_thread_mutex_t rpc_lock;

static int32_t utf8_compare_cb LDAP_P((dsdata *, dsdata *, u_int32_t));
static dsdata *utf8_normalize_cb LDAP_P((dsdata *, u_int32_t));

static int get_boolForKey LDAP_P((dsengine *, const dsdata *, int));

const dsdata netinfo_back_name_name = { DataTypeCStr, sizeof("name"), "name", 1 };
const dsdata netinfo_back_name_passwd = { DataTypeCStr, sizeof("passwd"), "passwd", 1 };
const dsdata netinfo_back_name_address = { DataTypeCStr, sizeof("address"), "address", 1 };
const dsdata netinfo_back_name_trusted_networks = { DataTypeCStr, sizeof("trusted_networks"), "trusted_networks", 1 };
const dsdata netinfo_back_name_readers = { DataTypeCStr, sizeof("readers"), "readers", 1 };
const dsdata netinfo_back_name_writers = { DataTypeCStr, sizeof("writers"), "writers", 1 };
const dsdata netinfo_back_name_rdn = { DataTypeCStr, sizeof("rdn"), "rdn", 1 };
const dsdata netinfo_back_name_networks = { DataTypeCStr, sizeof("networks"), "networks", 1 };
const dsdata netinfo_back_name_groups = { DataTypeCStr, sizeof("groups"), "groups", 1 };
const dsdata netinfo_back_name_admin = { DataTypeCStr, sizeof("admin"), "admin", 1 };
const dsdata netinfo_back_name_users = { DataTypeCStr, sizeof("users"), "users", 1 };
const dsdata netinfo_back_name_promote_admins = { DataTypeCStr, sizeof("promote_admins"), "promote_admins", 1 };

const dsdata netinfo_back_access_user_anybody = { DataTypeCStr, sizeof("*"), "*", 1 };
const dsdata netinfo_back_access_user_super = { DataTypeCStr, sizeof("root"), "root", 1 };

AttributeDescription *netinfo_back_ad_dSID = NULL;
AttributeDescription *netinfo_back_ad_nIVersionNumber = NULL;
AttributeDescription *netinfo_back_ad_nISerialNumber = NULL;

#ifdef SLAPD_NETINFO_DYNAMIC
int back_netinfo_LTX_init_module(int arc, char *arv[])
{
	BackendInfo bi;

	memset(&bi, '\0', sizeof(bi));
	bi.bi_type = "netinfo";
	bi.bi_init = netinfo_back_initialize;
	backend_add(&bi);

	return 0;
}
#endif /* SLAPD_NETINFO_DYNAMIC */

static int32_t utf8_compare_cb(dsdata *a, dsdata *b, u_int32_t casefold)
{
	struct berval bva, bvb;

	/* We assume these are strings. */
	bva.bv_val = a->data;
	bva.bv_len = a->length - 1;

	bvb.bv_val = b->data;
	bvb.bv_len = b->length - 1;

	return UTF8bvnormcmp(&bva, &bvb, casefold ? LDAP_UTF8_CASEFOLD : LDAP_UTF8_NOCASEFOLD);
}

static dsdata *utf8_normalize_cb(dsdata *d, u_int32_t casefold)
{
	struct berval bv, newbv, *bvp;
	dsdata *x;

	bv.bv_val = d->data;
	bv.bv_len = d->length - 1;

	bvp = UTF8bvnormalize(&bv, &newbv, casefold ? LDAP_UTF8_CASEFOLD : LDAP_UTF8_NOCASEFOLD);
	if (bvp == NULL)
		return NULL;

	x = berval_to_dsdata(bvp, d->type);

	ldap_memfree(bvp->bv_val);

	return x;
}

static int get_boolForKey(dsengine *e, const dsdata *name, int def)
{
	int ret;
	dsrecord *r;
	dsattribute *a;

	ret = def;

	if (dsengine_fetch(e, 0, &r) != DSStatusOK)
	{
		return ret;
	}

	a = dsrecord_attribute(r, (dsdata *)name, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		dsrecord_release(r);
		return ret;
	}

	dsrecord_release(r);

	if (a->count == 0 || (IsStringDataType(a->value[0]->type) == 0))
	{
		dsattribute_release(a);
		return ret;
	}

	if (!strcmp(a->value[0]->data, "YES")) ret = TRUE;
	else if (!strcmp(a->value[0]->data, "yes")) ret = TRUE;
	else if (!strcmp(a->value[0]->data, "Yes")) ret = TRUE;
	else if (!strcmp(a->value[0]->data, "1")) ret = TRUE;
	else if (!strcmp(a->value[0]->data, "Y")) ret = TRUE;
	else if (!strcmp(a->value[0]->data, "y")) ret = TRUE;
	else if (!strcmp(a->value[0]->data, "NO")) ret = FALSE;
	else if (!strcmp(a->value[0]->data, "no")) ret = FALSE;
	else if (!strcmp(a->value[0]->data, "No")) ret = FALSE;
	else if (!strcmp(a->value[0]->data, "0")) ret = FALSE;
	else if (!strcmp(a->value[0]->data, "N")) ret = FALSE;
	else if (!strcmp(a->value[0]->data, "n")) ret = FALSE;

	dsattribute_release(a);

	return ret; 
}

int netinfo_back_open(BackendInfo *bi)
{
	LDAPAttributeType *at;
	const char *err;
	int code;

	at = ldap_str2attributetype(
		"( 1.3.6.1.4.1.5322.14.1.1 "
		"NAME 'dSID' "
		"DESC 'NetInfo Directory Identifier' "
		"EQUALITY integerMatch "
		"SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 "
		"SINGLE-VALUE NO-USER-MODIFICATION USAGE directoryOperation )",
		&code, &err, LDAP_SCHEMA_ALLOW_ALL);
	if (at == NULL)
	{
		fprintf(stderr, "netinfo_back_schema_init: dSID: %s before %s\n",
			ldap_scherr2str(code), err);
		return code;
	}

	code = at_add(at, &err);
	if (code)
	{
		fprintf(stderr, "netinfo_back_schema_init: dSID: %s: \"%s\"\n",
			scherr2str(code), err);
		ldap_memfree(at);
		return code;
	}
	ldap_memfree(at);

	code = slap_str2ad("dSID", &netinfo_back_ad_dSID, &err);
	if (code != LDAP_SUCCESS)
		return code;	

	at = ldap_str2attributetype(
		"( 1.3.6.1.4.1.5322.14.1.2 "
		"NAME 'nIVersionNumber' "
		"DESC 'NetInfo Directory Version Number' "
		"EQUALITY integerMatch "
		"SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 "
		"SINGLE-VALUE NO-USER-MODIFICATION USAGE directoryOperation )",
		&code, &err, LDAP_SCHEMA_ALLOW_ALL);
	if (at == NULL)
	{
		fprintf(stderr, "netinfo_back_schema_init: nIVersionNumber: %s before %s\n",
			ldap_scherr2str(code), err);
		return code;
	}

	code = at_add(at, &err);
	if (code)
	{
		fprintf(stderr, "netinfo_back_schema_init: nIVersionNumber: %s: \"%s\"\n",
			scherr2str(code), err);
		ldap_memfree(at);
		return code;
	}
	ldap_memfree(at);

	code = slap_str2ad("nIVersionNumber", &netinfo_back_ad_nIVersionNumber, &err);
	if (code != LDAP_SUCCESS)
		return code;	

	at = ldap_str2attributetype(
		"( 1.3.6.1.4.1.5322.14.1.3 "
		"NAME 'nISerialNumber' "
		"DESC 'NetInfo Directory Serial Number' "
		"EQUALITY integerMatch "
		"SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 "
		"SINGLE-VALUE NO-USER-MODIFICATION USAGE directoryOperation )",
		&code, &err, LDAP_SCHEMA_ALLOW_ALL);
	if (at == NULL)
	{
		fprintf(stderr, "netinfo_back_schema_init: nISerialNumber: %s before %s\n",
			ldap_scherr2str(code), err);
		return code;
	}

	code = at_add(at, &err);
	if (code)
	{
		fprintf(stderr, "netinfo_back_schema_init: nISerialNumber: %s: \"%s\"\n",
			scherr2str(code), err);
		ldap_memfree(at);
		return code;
	}
	ldap_memfree(at);

	code = slap_str2ad("nISerialNumber", &netinfo_back_ad_nISerialNumber, &err);
	if (code != LDAP_SUCCESS)
		return code;	

	return LDAP_SUCCESS;
}

int netinfo_back_destroy(BackendInfo *bi)
{
	ldap_pvt_thread_mutex_destroy(&rpc_lock);
}

int netinfo_back_initialize(BackendInfo *bi)
{
	static char *controls[] = { LDAP_CONTROL_MANAGEDSAIT, NULL };
	dsutil_utf8_callbacks callbacks;

	/*
	 * Set these callbacks so the dsstore can do meaningful
	 * Unicode string comparisons.
	 */
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.version = DSUTIL_UTF8_CALLBACKS_VERSION;
	callbacks.normalize = utf8_normalize_cb;
	callbacks.compare = utf8_compare_cb;
	dsutil_utf8_set_callbacks(&callbacks);

	ldap_pvt_thread_mutex_init(&rpc_lock);

	bi->bi_controls = controls;

	bi->bi_open = netinfo_back_open;
	bi->bi_config = 0;
	bi->bi_close = 0;
	bi->bi_destroy = netinfo_back_destroy;

	bi->bi_db_init = netinfo_back_db_init;
	bi->bi_db_config = netinfo_back_db_config;
	bi->bi_db_open = netinfo_back_db_open;
	bi->bi_db_close = netinfo_back_db_close;
	bi->bi_db_destroy = netinfo_back_db_destroy;

	bi->bi_op_bind = netinfo_back_bind;
	bi->bi_op_unbind = 0; /* use conn_destroy instead? */
	bi->bi_op_search = netinfo_back_search;
	bi->bi_op_compare = netinfo_back_compare;
	bi->bi_op_modify = netinfo_back_modify;
	bi->bi_op_modrdn = netinfo_back_modrdn;
	bi->bi_op_add = netinfo_back_add;
	bi->bi_op_delete = netinfo_back_delete;
	bi->bi_op_abandon = 0;

	bi->bi_extended = netinfo_back_extended;

	bi->bi_entry_release_rw = netinfo_back_entry_release;
	bi->bi_chk_referrals = netinfo_back_referrals;

	bi->bi_operational = netinfo_back_operational;

	bi->bi_acl_group = netinfo_back_group;
	bi->bi_acl_attribute = netinfo_back_attribute;

	bi->bi_connection_init = 0;
	bi->bi_connection_destroy = 0;

	/* tools are single threaded, doesn't acquire lock. */
	bi->bi_tool_entry_open = netinfo_tool_entry_open;
	bi->bi_tool_entry_close = netinfo_tool_entry_close;
	bi->bi_tool_entry_first = netinfo_tool_entry_next;
	bi->bi_tool_entry_next = netinfo_tool_entry_next;
	bi->bi_tool_entry_get = netinfo_tool_entry_get;
	bi->bi_tool_entry_put = netinfo_tool_entry_put;
	bi->bi_tool_entry_reindex = netinfo_tool_entry_reindex;
	bi->bi_tool_sync = netinfo_tool_sync;

	return 0;
}

int netinfo_back_db_init(BackendDB *be)
{
	struct dsinfo *di;

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, ENTRY, "netinfo_back_db_init: enter\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_db_init\n", 0, 0, 0);
#endif

	di = (struct dsinfo *)ch_calloc(1, sizeof(struct dsinfo));

	di->suffix.bv_val = NULL;
	di->nsuffix.bv_val = NULL;
	di->parent = NULL;
	di->children = NULL;
	di->datasource = NULL;
	di->auth_user = NULL;
	di->auth_password = NULL;
	di->flags = 0;
	di->engine = NULL;
	di->lock = NULL;
	di->promote_admins = TRUE;

	be->be_private = di;

	schemamap_init(be);
	assert(di->map != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_db_init: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_db_init\n", 0, 0, 0);
#endif

	return 0;
}

int netinfo_back_db_open(BackendDB *be)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsstatus status;
	int i;
	struct netinfo_referral **q;

	assert(di != NULL);

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, ENTRY, "netinfo_back_db_open: enter\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_db_open\n", 0, 0, 0);
#endif

	if (di->engine == NULL)
	{
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_db_open: "
			"could not open engine\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_db_open\n", 0, 0, 0);
#endif
		return 1;
	}

	/* Set filter delegate for schema mapping. */
	dsengine_set_filter_test_delegate(di->engine, wrapped_filter_test, (void *)be);

	if (di->auth_user != NULL)
	{
		status = dsengine_authenticate(di->engine, di->auth_user, di->auth_password);
		if (status != DSStatusOK)
		{
			dsengine_close(di->engine);
#ifdef NEW_LOGGING
			LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_db_open: "
				"authentication failed\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_db_open\n", 0, 0, 0);
#endif
			return 1;
		}
	}

	status = netinfo_back_get_ditinfo(di->engine, &di->suffix, &di->nsuffix, &di->parent, &di->children);
	if (status != DSStatusOK)
	{
		di->suffix.bv_val = ch_strdup("");
		di->suffix.bv_len = 0;
		di->nsuffix.bv_val = ch_strdup("");
		di->nsuffix.bv_len = 0;
		di->children = NULL;
		di->parent = NULL;
	}

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, INFO, "(Canonical suffix %s)\n", di->suffix.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "(Canonical suffix %s)\n", di->suffix.bv_val, 0, 0);
#endif

	if (di->parent != NULL)
	{
		for (i = 0; i < di->parent->count; i++)
		{
#ifdef NEW_LOGGING
			LDAP_LOG((BACK_NETINFO, INFO, "(Parent naming context %s "
				"referred to %s)\n",
				di->parent->nc.bv_val, di->parent->refs[i].bv_val, 0));
#else
			Debug(LDAP_DEBUG_TRACE, "(Parent naming context %s referred to %s)\n",
				di->parent->nc.bv_val, di->parent->refs[i].bv_val, 0);
#endif
		}
	}

	if (di->children != NULL)
	{
		for (q = di->children; *q != NULL; q++)
		{
			for (i = 0; i < (*q)->count; i++)
			{
#ifdef NEW_LOGGING
				LDAP_LOG((BACK_NETINFO, INFO, "(Child naming context %s "
					"referred to %s)\n",
					(*q)->nc.bv_val, (*q)->refs[i].bv_val, 0));
#else
		 		Debug(LDAP_DEBUG_TRACE, "(Child naming context %s referred to %s)\n",
					(*q)->nc.bv_val, (*q)->refs[i].bv_val, 0);
#endif
			}
		}
	}

	if (di->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
	{
		/* Global RPC runtime lock */
		di->lock = &rpc_lock;
	}
	else
	{
		di->lock = (ldap_pvt_thread_mutex_t *)ch_malloc(sizeof(ldap_pvt_thread_mutex_t));
		ldap_pvt_thread_mutex_init(di->lock);
	}

	/* check whether members of the admin group should be promoted */
	di->promote_admins = get_boolForKey(di->engine, &netinfo_back_name_promote_admins, TRUE);

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_db_open: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_db_open\n", 0, 0, 0);
#endif

	return 0;
}

int netinfo_back_db_close(BackendDB *be)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);

	if (di->engine != NULL)
	{
		dsengine_close(di->engine);
		di->engine = NULL;
	}

	if (di->suffix.bv_val != NULL)
	{
		ch_free(di->suffix.bv_val);
		di->suffix.bv_val = NULL;
	}

	if (di->nsuffix.bv_val != NULL)
	{
		ch_free(di->nsuffix.bv_val);
		di->nsuffix.bv_val = NULL;
	}

	if (di->parent != NULL)
	{
		ch_free(di->parent->nc.bv_val);
		ch_free(di->parent->nnc.bv_val);
		ber_bvarray_free(di->parent->refs);
		ch_free(di->parent);
		di->parent = NULL;
	}

	if (di->children != NULL)
	{
		struct netinfo_referral **p;

		for (p = di->children; *p; p++)
		{
			ch_free((*p)->nc.bv_val);
			ch_free((*p)->nnc.bv_val);
			ber_bvarray_free((*p)->refs);
			ch_free(*p);
		}
		ch_free(di->children);
		di->children = NULL;
	}

	return 0;
}

int netinfo_back_db_destroy(BackendDB *be)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);

	if (di->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
	{
		di->lock = NULL;
	}
	else
	{
		if (di->lock != NULL)
		{
			ldap_pvt_thread_mutex_destroy(di->lock);
			ch_free(di->lock);
		}
	}

	if (di->datasource)
		free(di->datasource);

	if (di->auth_user)
		dsdata_release(di->auth_user);

	if (di->auth_password)
		dsdata_release(di->auth_password);

	schemamap_destroy(be);

	free(be->be_private);
	be->be_private = NULL;

	return 0;
}

/*
 * Get DIT structural info.
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus netinfo_back_get_ditinfo(
	dsengine *s,
	struct berval *psuffix,
	struct berval *pnsuffix,
	struct netinfo_referral **prefs,
	struct netinfo_referral ***crefs)
{
	int i, j;
	dsx500dit *info;
	dsattribute *a;
	dsdata *k, *v;
	struct berval tmp;


#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, ENTRY, "netinfo_back_get_ditinfo: enter\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_get_ditinfo\n", 0, 0, 0);
#endif

	info = dsx500dit_new(s);
	if (info == NULL)
	{
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_get_ditinfo: could not retrieve DIT info\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_get_ditinfo\n", 0, 0, 0);
#endif
		return DSStatusFailed;
	}

	/* Could be UTF8 */
	if (info->local_suffix != NULL)
	{
		dsdata_to_berval_no_copy(&tmp, info->local_suffix);
		dnPrettyNormal(NULL, &tmp, psuffix, pnsuffix);
	}
	else
	{
		psuffix->bv_val = ch_strdup("");
		psuffix->bv_len = 0;
		pnsuffix->bv_val = ch_strdup("");
		pnsuffix->bv_len = 0;
	}

	*prefs = NULL;
	*crefs = NULL;

	if (info->parent_referrals != NULL)
	{
		a = info->parent_referrals;

		k = a->key;
		assert(k != NULL);

		*prefs = (struct netinfo_referral *)ch_malloc(sizeof(struct netinfo_referral));
		dsdata_to_berval_no_copy(&tmp, k);
		dnPrettyNormal(NULL, &tmp, &(*prefs)->nc, &(*prefs)->nnc);
		(*prefs)->refs = (BerVarray)ch_malloc((a->count + 1) * sizeof(struct berval));

		for (j = 0; j < a->count; j++)
		{
			v = dsattribute_value(a, j);
			assert(v != NULL);
			dsdata_to_berval(&(*prefs)->refs[j], v);
			dsdata_release(v);
		}	
		(*prefs)->refs[a->count].bv_val = NULL;
		(*prefs)->count = a->count;
	}

	if (info->child_referrals != NULL)
	{
		int count = info->child_count;

		*crefs = (struct netinfo_referral **)ch_malloc((count + 1) * sizeof(struct netinfo_referral *));
		for (i = 0; i < count; i++)
		{
			a = info->child_referrals[i];

			k = a->key;
			assert(k != NULL);

			(*crefs)[i] = (struct netinfo_referral *)ch_malloc(sizeof(struct netinfo_referral));
			dsdata_to_berval_no_copy(&tmp, k);
			dnPrettyNormal(NULL, &tmp, &(*crefs)[i]->nc, &(*crefs)[i]->nnc);
			(*crefs)[i]->refs = (BerVarray)ch_malloc((a->count + 1) * sizeof(struct berval));

			for (j = 0; j < a->count; j++)
			{
				v = dsattribute_value(a, j);
				assert(v != NULL);
				dsdata_to_berval(&(*crefs)[i]->refs[j], v);
				dsdata_release(v);
			}
			(*crefs)[i]->refs[a->count].bv_val = NULL;
			(*crefs)[i]->count = a->count;
		}
		(*crefs)[count] = NULL;
	}

	dsx500dit_release(info);

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_get_ditinfo: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_get_ditinfo\n", 0, 0, 0);
#endif

	return DSStatusOK;
}

