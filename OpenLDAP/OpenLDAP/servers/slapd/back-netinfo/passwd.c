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

#include <lutil.h>

#include "slap.h"
#include "back-netinfo.h"

int netinfo_back_exop_passwd(
	BackendDB *be,
	Connection *conn,
	Operation *op,
	const char *reqoid,
	struct berval *reqdata,
	char **rspoid,
	struct berval **rspdata,
	LDAPControl ***rspctrls,
	const char **text,
	BerVarray *refs)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	char *hashMechanism;
	struct berval *ndn, *hash = NULL;
	struct berval new = { 0, NULL };
	struct berval ndn_buf = { 0, NULL };
	struct berval id = { 0, NULL };
	dsdata *value = NULL;
	dsrecord *user = NULL;
	dsattribute *passwd = NULL;
	dsstatus status;
	u_int32_t dsid;
	struct atmap map;
	int rc;

	assert(reqoid != NULL);
	assert(strcmp(LDAP_EXOP_MODIFY_PASSWD, reqoid) == 0);

	/* Parse the password. */
	rc = slap_passwd_parse(reqdata, &id, NULL, &new, text);
	if (rc != LDAP_SUCCESS)
		return rc;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_exop_passwd: \"%s\"\n",
		(id.bv_val != NULL) ? id.bv_val : "(null)"));
#else
	Debug(LDAP_DEBUG_ARGS, "==> netinfo_back_exop_passwd: \"%s\"\n",
		id.bv_val ? id.bv_val : "(null)", 0, 0);
#endif

	*text = NULL;

	/* If no password is specified, generate one. */
	if (new.bv_len == 0)
	{
		slap_passwd_generate(&new);
		if (new.bv_val == NULL || new.bv_len == 0)
		{
			*text = "Password generation failed";
			return LDAP_OTHER;
		}
		*rspdata = slap_passwd_return(&new);
	}

	if (id.bv_val != NULL)
	{
		ndn = &ndn_buf;
		dnNormalize2(NULL, &id, &ndn_buf);
	}
	else
	{
		ndn = &op->o_ndn;
	}

	if (ndn->bv_len == 0)
	{
		if (ndn_buf.bv_val != NULL)
			ch_free(ndn_buf.bv_val);
		*text = "No password is associated with the Root DSE";
		return LDAP_OPERATIONS_ERROR;
	}

	ENGINE_LOCK(di);

	status = netinfo_back_dn_pathmatch(be, ndn, &dsid);
	if (status != DSStatusOK)
	{
		goto out;
	}

	status = dsengine_fetch(di->engine, dsid, &user);
	dsengine_flush_cache(di->engine);
	if (status != DSStatusOK)
	{
		goto out;
	}

	/* Retrieve the mapping for the userPassword attribute. */
	status = schemamap_x500_to_ni_at(be, SUPER(user),
		slap_schema.si_ad_userPassword, &map);
	if (status != DSStatusOK)
	{
		goto out;
	}

	/* Hash mechanism is implicitly indicated in attribute mapping. */
	if (map.x500ToNiTransform == removeCaseIgnorePrefixTransform &&
	    map.x500ToNiArg != NULL)
	{
		hashMechanism = ((struct berval *)map.x500ToNiArg)->bv_val;
	}
	else
	{
		/* use slapd password hash mechanism default */
#ifdef LUTIL_SHA1_BYTES
		hashMechanism = (default_passwd_hash != NULL) ? default_passwd_hash : "{SSHA}";
#else
		hashMechanism = (default_passwd_hash != NULL) ? default_passwd_hash : "{SMD5}";
#endif
	}

#if defined( SLAPD_CRYPT ) || defined( SLAPD_SPASSWD )
	ldap_pvt_thread_mutex_lock(&passwd_mutex);
#endif
	hash = lutil_passwd_hash(&new, hashMechanism);
#if defined( SLAPD_CRYPT ) || defined( SLAPD_SPASSWD )
	ldap_pvt_thread_mutex_unlock(&passwd_mutex);
#endif

	/* Check we got back something sensible. */
	if (hash->bv_len == 0)
	{
		status = DSStatusFailed;
		goto out;
	}
	
	/* Check mapped userPassword can be written to. */
	status = netinfo_back_access_allowed(be, conn, op, dsid,
		slap_schema.si_ad_userPassword, NULL, ACL_WRITE);
	if (status != DSStatusOK)
	{
		goto out;
	}

	/* Remove the old userPassword attribute. */
	dsrecord_remove_key(user, map.ni_key, map.selector);

	/* Create a new userPassword attribute. */
	passwd = dsattribute_new(map.ni_key);
	if (passwd == NULL)
	{
		status = DSStatusFailed;
		goto out;
	}

	/* Map userPassword value, e.g. strip {CRYPT} */
	status = (map.x500ToNiTransform)(be, &value, hash, map.type, map.x500ToNiArg);
	if (status != DSStatusOK)
	{
		goto out;
	}
	
	dsattribute_insert(passwd, value, 0);
	dsrecord_append_attribute(user, passwd, map.selector);
	
	status = dsengine_save_attribute(di->engine, user, passwd, map.selector);
	/* fallthough */

out:

	ENGINE_UNLOCK(di);

	if (ndn_buf.bv_val != NULL)
		ch_free(ndn_buf.bv_val);

	if (hash != NULL)
		ber_bvfree(hash);

	if (user != NULL)
		dsrecord_release(user);

	if (passwd != NULL)
		dsattribute_release(passwd);

	schemamap_atmap_release(&map);

	if (value != NULL)
		dsdata_release(value);

	*text = dsstatus_message(status);

	return dsstatus_to_ldap_err(status);
}
