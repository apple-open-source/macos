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
netinfo_back_referrals(
	struct slap_op *op, 
	struct slap_rep *rs )
/*	Backend	*be,
	Connection *conn,
	Operation *op,
	struct berval *dn,
	struct berval *ndn,
	const char **text*/
{
	struct dsinfo *di = (struct dsinfo *)op->o_bd->be_private;
	struct berval localDN;

#ifdef NO_NETINFO_REFERRALS
	return LDAP_SUCCESS;
#endif

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_ARGS, "netinfo_back_referrals DN %s\n", op->o_req_dn.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_referrals dn=%s ndn=%s\n", op->o_req_dn.bv_val, op->o_req_ndn.bv_val, 0);
#endif

	if (op->o_tag == LDAP_REQ_SEARCH)
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_referrals: is search tag\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_referrals\n", 0, 0, 0);
#endif
		return LDAP_SUCCESS;
	}

	if (get_manageDSAit(op))
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_referrals: manageDSAit control enabled\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_referrals\n", 0, 0, 0);
#endif
		return LDAP_SUCCESS;
	}

	if (netinfo_back_send_referrals(op, rs, &op->o_req_ndn) == DSStatusOK)
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_referrals: referred to children\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_referrals (ref children)\n", 0, 0, 0);
#endif
		return LDAP_SUCCESS;
	}

	if (dnMakeLocal(op->o_bd, &localDN, &op->o_req_ndn) == DSStatusPathNotLocal)
	{
#ifdef NEW_LOGGING
		LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_referrals: referred to parent\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_referrals (ref parent)\n", 0, 0, 0);
#endif

		/* Send parent referral */
		if (di->parent)
			rs->sr_ref = di->parent->refs;
		rs->sr_err = LDAP_REFERRAL;
		send_ldap_result(op, rs);
	}
	else
	{
		if (localDN.bv_val != NULL)
			ch_free(localDN.bv_val);
	}

#ifdef NEW_LOGGING
	LDAP_LOG(("backend", LDAP_LEVEL_INFO, "netinfo_back_referrals: done\n"));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_referrals (ref local)\n", 0, 0, 0);
#endif

	return LDAP_SUCCESS;
}
