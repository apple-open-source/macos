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
	struct slap_op *op, 
	struct slap_rep *rs
)
{
	struct dsinfo *di = (struct dsinfo *)op->o_bd->be_private;
	int rc;
	u_int32_t dsid;
	struct atmap map;
	dsstatus status;
	dsrecord *user;
	dsattribute *passwd;
	BerVarray vals, bvp;
	dsattribute *authAuthorityAttr;
	char *authAuthority;

	op->orb_edn.bv_val = NULL;
	op->orb_edn.bv_len = 0;

	rc = LDAP_INVALID_CREDENTIALS;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_bind: DN %s\n",
		op->o_req_dn != NULL ? op->o_req_dn->bv_val : "(null)"));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_bind dn=%s\n", op->o_req_dn.bv_val ? op->o_req_dn.bv_val : "(null)", 0, 0);
#endif

	if (op->orb_method != LDAP_AUTH_SIMPLE)
	{
		send_ldap_error( op, rs, LDAP_AUTH_METHOD_NOT_SUPPORTED,
				"Only simple authentication supported by NetInfo" );
		return LDAP_AUTH_METHOD_NOT_SUPPORTED;
	}

	if (be_isroot_pw(op))
	{
		ber_dupbv(&op->orb_edn, be_root_dn(op->o_bd));
		return LDAP_SUCCESS; /* FE sends result */
	}

	if (op->orb_cred.bv_val != NULL)
	{
		ENGINE_LOCK(di);

		status = netinfo_back_dn_pathmatch(op->o_bd, &op->o_req_ndn, &dsid);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: pathmatch failed\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: pathmatch failed\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(op, rs, status);
		}

		status = netinfo_back_access_allowed(op, dsid, slap_schema.si_ad_authAuthority, NULL, ACL_AUTH);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: access to authAuthority denied\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: access to authAuthority denied\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(op, rs, status);
		}

		status = netinfo_back_access_allowed(op, dsid, slap_schema.si_ad_userPassword, NULL, ACL_AUTH);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: access to userPassword denied\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: access to userPassword denied\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(op, rs, status);
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
			return netinfo_back_op_result(op, rs, status);
		}

		status = schemamap_x500_to_ni_at(op->o_bd, SUPER(user), slap_schema.si_ad_authAuthority, &map);
		if (status != DSStatusOK)
		{
			dsrecord_release(user);
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: could not map authAuthority\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: could not map authAuthority\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(op, rs, status);
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

				status = (DoPSAuth(name, op->orb_cred.bv_val, authAuthority) == kAuthNoError) ? DSStatusOK : DSStatusAccessRestricted;
				if (status == DSStatusOK)
				{
					/* Set the DN. */
					status = netinfo_back_global_dn(op->o_bd, user->dsid, &op->orb_edn);
				}

				dsrecord_release(user);
				schemamap_atmap_release(&map);
				dsattribute_release(nameAttr);
				dsattribute_release(authAuthorityAttr);
				ENGINE_UNLOCK(di);
				return netinfo_back_op_result(op, rs, status);
			}
		}

		dsattribute_release(authAuthorityAttr);
		schemamap_atmap_release(&map);

		status = schemamap_x500_to_ni_at(op->o_bd, SUPER(user), slap_schema.si_ad_userPassword, &map);
		if (status != DSStatusOK)
		{
			dsrecord_release(user);
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: could not map userPassword\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind: could not map userPassword\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(op, rs, status);
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
			return netinfo_back_op_result(op, rs, DSStatusInvalidKey);
		}

		status = dsattribute_to_bervals(op->o_bd, &vals, passwd, &map);
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
			return netinfo_back_op_result(op, rs, status);
		}

		for (bvp = vals; bvp->bv_val != NULL; bvp++)
		{
			if (lutil_passwd(bvp, &op->orb_cred, NULL, NULL) == 0)
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
			status = netinfo_back_global_dn(op->o_bd, user->dsid, &op->orb_edn);
			rc = dsstatus_to_ldap_err(status);
		}

		dsrecord_release(user);
		schemamap_atmap_release(&map);
		dsattribute_release(passwd);

		ENGINE_UNLOCK(di);

		ber_bvarray_free(vals);
	}

	rs->sr_err = rc;
	if (rc != LDAP_SUCCESS)
	{
		send_ldap_result(op, rs);
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_bind: done rc=%d\n", rc));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_bind done rc=%d\n", rc, 0, 0);
#endif

	/* front end will send result on success (rs->sr_err==0) */
	return rs->sr_err;
}

