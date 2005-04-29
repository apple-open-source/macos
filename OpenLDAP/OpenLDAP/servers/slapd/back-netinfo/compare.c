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
	struct slap_op *op, 
	struct slap_rep *rs)
/*	BackendDB	*be,
	Connection	*conn,
	Operation	*op,
	struct berval *dn,
	struct berval *ndn,
	AttributeAssertion *ava*/
{
	struct dsinfo *di = (struct dsinfo *)op->o_bd->be_private;
	dsassertion *assertion;
	dsrecord *record;
	dsstatus status;
	u_int32_t dsid;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_compare: DN %s\n", op->o_req_dn.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_compare dn=%s\n", op->o_req_dn.bv_val, 0, 0);
#endif

	if (netinfo_back_send_referrals(op, rs, &op->o_req_ndn) == DSStatusOK)
	{
		return 1;
	}

	ENGINE_LOCK(di);

	/* get the base dsid */
	status = netinfo_back_dn_pathmatch(op->o_bd, &op->o_req_ndn, &dsid);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(op, rs, status);
	}

	/* do ACL authorization */
	status = netinfo_back_access_allowed(op, dsid, op->orc_ava->aa_desc, &op->orc_ava->aa_value, ACL_COMPARE);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(op, rs, status);
	}

	assertion = attribute_assertion_to_dsassertion(op->o_bd, op->orc_ava, LDAP_FILTER_EQUALITY);
	if (assertion == NULL)
	{
		send_ldap_error(op, rs, LDAP_OPERATIONS_ERROR,
			"Could not translate attribute value assertion");
		ENGINE_UNLOCK(di);
		return -1;
	}

	status = dsengine_fetch(di->engine, dsid, &record);
	if (status != DSStatusOK)
	{
		dsassertion_release(assertion);
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(op, rs, status);
	}

	switch (wrapped_assertion_test(assertion, record, (void *)op->o_bd))
	{
		case L3False:
			rs->sr_err = LDAP_COMPARE_FALSE;
			break;
		case L3True:
			rs->sr_err = LDAP_COMPARE_TRUE;
			break;
		default:
		case L3Undefined:
			rs->sr_err = SLAPD_COMPARE_UNDEFINED;
			break;
	}

	dsassertion_release(assertion);
	dsrecord_release(record);

	ENGINE_UNLOCK(di);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "netinfo_back_compare: %d\n", rs->sr_err));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_compare rc=%d\n", rs->sr_err, 0, 0);
#endif

	send_ldap_result(op, rs);

	return 0;
}
