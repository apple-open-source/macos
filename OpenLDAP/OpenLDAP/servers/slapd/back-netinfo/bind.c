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
#include "psauth.h"
#include "back-netinfo.h"

int
netinfo_back_bind(
	BackendDB *be,
	Connection *conn,
	Operation *op,
	struct berval *dn,
	struct berval *ndn,
	int method,
	struct berval *cred,
	struct berval *edn
)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	int rc;
	u_int32_t dsid;
	struct atmap map;
	dsstatus status;
	dsrecord *user;
	dsattribute *passwd;
	BerVarray vals, bvp;
	dsattribute *authAuthorityAttr;
	char *authAuthority;

	edn->bv_val = NULL;
	edn->bv_len = 0;

	rc = LDAP_INVALID_CREDENTIALS;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_bind: DN %s\n",
		dn != NULL ? dn->bv_val : "(null)"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_bind dn=%s\n", dn ? dn->bv_val : "(null)", 0, 0);
#endif

	if (method != LDAP_AUTH_SIMPLE)
	{
		send_ldap_result(conn, op, LDAP_AUTH_METHOD_NOT_SUPPORTED, NULL,
			"Only simple authentication supported by NetInfo", NULL, NULL);
		return LDAP_AUTH_METHOD_NOT_SUPPORTED;
	}

	if (be_isroot_pw(be, conn, ndn, cred))
	{
		ber_dupbv(edn, be_root_dn(be));
		return LDAP_SUCCESS; /* FE sends result */
	}

	if (cred != NULL && cred->bv_val != NULL)
	{
		ENGINE_LOCK(di);

		status = netinfo_back_dn_pathmatch(be, ndn, &dsid);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: pathmatch failed\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: pathmatch failed\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}

		status = netinfo_back_access_allowed(be, conn, op, dsid, slap_schema.si_ad_authAuthority, NULL, ACL_AUTH);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: access to authAuthority denied\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: access to authAuthority denied\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}

		status = netinfo_back_access_allowed(be, conn, op, dsid, slap_schema.si_ad_userPassword, NULL, ACL_AUTH);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: access to userPassword denied\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: access to userPassword denied\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}

		status = dsengine_fetch(di->engine, dsid, &user);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: fetch user record failed\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: fetch user record failed\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}

		status = schemamap_x500_to_ni_at(be, SUPER(user), slap_schema.si_ad_authAuthority, &map);
		if (status != DSStatusOK)
		{
			dsrecord_release(user);
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: could not map authAuthority\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: could not map authAuthority\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}

		authAuthorityAttr = dsrecord_attribute(user, map.ni_key, map.selector);
		if (authAuthorityAttr != NULL && authAuthorityAttr->count > 0 &&
			((authAuthority = dsdata_to_cstring(authAuthorityAttr->value[0])) != NULL))
		{
			if (CheckAuthType(authAuthority, BASIC_AUTH_TYPE) == 0)
			{
				/*
				 * Must use authentication authority.
				 */
				dsattribute *nameAttr = dsrecord_attribute(user, (dsdata *)&netinfo_back_name_name, SELECT_ATTRIBUTE);
				char *name = NULL;

				if (nameAttr != NULL && nameAttr->count > 0)
					name = dsdata_to_cstring(nameAttr->value[0]);

				status = (DoPSAuth(name, cred->bv_val, authAuthority) == kAuthNoError) ? DSStatusOK : DSStatusAccessRestricted;
				if (status == DSStatusOK)
				{
					/* Set the DN. */
					status = netinfo_back_global_dn(be, user->dsid, edn);
				}

				dsrecord_release(user);
				schemamap_atmap_release(&map);
				dsattribute_release(nameAttr);
				dsattribute_release(authAuthorityAttr);
				ENGINE_UNLOCK(di);
				return netinfo_back_op_result(be, conn, op, status);
			}
		}

		dsattribute_release(authAuthorityAttr);
		schemamap_atmap_release(&map);

		status = schemamap_x500_to_ni_at(be, SUPER(user), slap_schema.si_ad_userPassword, &map);
		if (status != DSStatusOK)
		{
			dsrecord_release(user);
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: could not map userPassword\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: could not map userPassword\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}

		passwd = dsrecord_attribute(user, map.ni_key, map.selector);
		if (passwd == NULL)
		{
			dsrecord_release(user);
			schemamap_atmap_release(&map);
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: user has no userPassword attribute\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: user has no userPassword attribute\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, DSStatusInvalidKey);
		}

		status = dsattribute_to_bervals(be, &vals, passwd, &map);
		if (status != DSStatusOK)
		{
			dsrecord_release(user);
			schemamap_atmap_release(&map);
			dsattribute_release(passwd);
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: user has no userPassword attribute\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: user has no userPassword attribute\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}

		for (bvp = vals; bvp->bv_val != NULL; bvp++)
		{
			if (lutil_passwd(bvp, cred, NULL) == 0)
			{
				rc = LDAP_SUCCESS;
				break;
			}
		}

		/*
		 * Set the bind DN to the canonical, global DN for the
		 * bound user.
		 */
		if (rc == LDAP_SUCCESS)
		{
			status = netinfo_back_global_dn(be, user->dsid, edn);
			rc = dsstatus_to_ldap_err(status);
		}

		dsrecord_release(user);
		schemamap_atmap_release(&map);
		dsattribute_release(passwd);

		ENGINE_UNLOCK(di);

		ber_bvarray_free(vals);
	}

	send_ldap_result(conn, op, rc, NULL, NULL, NULL, NULL);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: done rc=%d\n", rc));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind done rc=%d\n", rc, 0, 0);
#endif

	return rc;
}

