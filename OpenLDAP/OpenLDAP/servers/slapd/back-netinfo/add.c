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

int netinfo_back_add(
	struct slap_op *op, 
	struct slap_rep *rs)
{
	struct dsinfo *di = (struct dsinfo *)op->o_bd->be_private;
	dsrecord *record;
	dsstatus status;
	u_int32_t dsid;
	const char *text;
	int rc;
	char textbuf[SLAP_TEXT_BUFLEN];
	struct berval parentNDN;

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "netinfo_back_add: %s\n", 
		op->ora_e->e_name.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_add\n", 0, 0, 0);
#endif

	dnParent(&op->ora_e->e_nname, &parentNDN);

	/* don't allow add if parent is a referral */
	if (netinfo_back_send_referrals(op, rs, &parentNDN) == DSStatusOK)
	{
		return -1;
	}

	rc = entry_schema_check(op->o_bd, op->ora_e, NULL, &text, textbuf, sizeof(textbuf));
	if (rc != LDAP_SUCCESS)
	{
		send_ldap_error(op, rs, rc, text);
		return -1;
	}

	ENGINE_LOCK(di);

	/* Get the DSID of the parent entry */
	status = netinfo_back_dn_pathmatch(op->o_bd, &parentNDN, &dsid);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(op, rs, status);
	}

	/* Check NetInfo/slapd ACLs on children */
	status = netinfo_back_access_allowed(op, dsid, slap_schema.si_ad_children, NULL, ACL_WRITE);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(op, rs, status);
	}

	/* Check whether it satisfies structure rules enforced by schema map. */
	status = schemamap_validate_objectclasses(op->o_bd, dsid, op->ora_e);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(op, rs, status);
	}

	status = entry_to_dsrecord(op->o_bd, dsid, op->ora_e, &record);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
		return netinfo_back_op_result(op, rs, status);
	}

	status = dsengine_create(di->engine, record, dsid);

	dsrecord_release(record);

	ENGINE_UNLOCK(di);

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ENTRY, "netinfo_back_add: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_add\n", 0, 0, 0);
#endif

	return netinfo_back_op_result(op, rs, status);
}
