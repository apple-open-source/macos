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

int
netinfo_back_delete(
	BackendDB *be,
	Connection *conn,
	Operation *op,
	struct berval *dn,
	struct berval *ndn
)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsstatus status;
	u_int32_t dsid, parent;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_delete: DN %s\n", dn->bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_delete dn=%s\n", dn->bv_val, 0, 0);
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
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_delete: patchmatch failed\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_delete\n", 0, 0, 0);
#endif
		return netinfo_back_op_result(be, conn, op, status);
	}

	/* get the parent as we do ACL checking based on it */
	status = dsengine_record_super(di->engine, dsid, &parent);
	if (status == DSStatusOK)
	{
		/*
		 * Check NetInfo/slapd ACLs on removing children
		 * passing dn/ndn is for NetInfo attribute authorization only
		 * which is not relevant for deleting children; however we
		 * pass it in for good measure.
		 */
		status = netinfo_back_access_allowed(be, conn, op, parent,
			slap_schema.si_ad_children, NULL, ACL_WRITE);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_delete: insufficient access\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_delete\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}
	}
	else
	{
		/* no parent, must be root to delete */
		if (!be_isroot(be, &op->o_ndn))
		{
			ENGINE_UNLOCK(di);
			send_ldap_result(conn, op, LDAP_INSUFFICIENT_ACCESS, NULL, NULL, NULL, NULL);
#ifdef NEW_LOGGING
			LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_delete: insufficient access\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_delete\n", 0, 0, 0);
#endif
			return -1;
		}
	}

	status = dsengine_remove(di->engine, dsid);
	ENGINE_UNLOCK(di);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_delete: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_delete\n", 0, 0, 0);
#endif
	return netinfo_back_op_result(be, conn, op, status);
}
