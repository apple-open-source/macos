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
netinfo_back_compare(
	BackendDB	*be,
	Connection	*conn,
	Operation	*op,
	struct berval *dn,
	struct berval *ndn,
	AttributeAssertion *ava)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsassertion *assertion;
	dsrecord *record;
	int rc;
	dsstatus status;
	u_int32_t dsid;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_compare: DN %s\n", dn->bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_compare dn=%s\n", dn->bv_val, 0, 0);
#endif

	if (netinfo_back_send_referrals(be, conn, op, ndn) == DSStatusOK)
	{
		return 1;
	}

	ENGINE_LOCK(di);

	/* get the base dsid */
	status = netinfo_back_dn_pathmatch(be, ndn, &dsid);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	/* do ACL authorization */
	status = netinfo_back_access_allowed(be, conn, op, dsid, ava->aa_desc, &ava->aa_value, ACL_COMPARE);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	assertion = attribute_assertion_to_dsassertion(be, ava, LDAP_FILTER_EQUALITY);
	if (assertion == NULL)
	{
		send_ldap_result(conn, op, LDAP_OPERATIONS_ERROR, NULL,
			"Could not translate attribute value assertion", NULL, NULL);
		ENGINE_UNLOCK(di);
		return -1;
	}

	status = dsengine_fetch(di->engine, dsid, &record);
	if (status != DSStatusOK)
	{
		dsassertion_release(assertion);
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(be, conn, op, status);
	}

	switch (wrapped_assertion_test(assertion, record, (void *)be))
	{
		case L3False:
			rc = LDAP_COMPARE_FALSE;
			break;
		case L3True:
			rc = LDAP_COMPARE_TRUE;
			break;
		default:
		case L3Undefined:
			rc = SLAPD_COMPARE_UNDEFINED;
			break;
	}

	dsassertion_release(assertion);
	dsrecord_release(record);

	ENGINE_UNLOCK(di);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "netinfo_back_compare: %d\n", rc));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_compare rc=%d\n", rc, 0, 0);
#endif

	send_ldap_result(conn, op, rc, NULL, NULL, NULL, NULL);

	return 0;
}
