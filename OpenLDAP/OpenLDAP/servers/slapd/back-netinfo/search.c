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
netinfo_back_search(
	BackendDB *be,
	Connection *conn,
	Operation *op,
	struct berval *base,
	struct berval *nbase,
	int scope,
	int deref,
	int slimit,
	int tlimit,
	Filter *filter,
	struct berval *filterstr,
	AttributeName *attrs,
	int attrsonly
)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsfilter *dsf;
	dsstatus status;
	u_int32_t dsid, *match, count, scopemin, scopemax;
	int i, rc;
	time_t stoptime;
	struct slap_limits_set *limit = NULL;
	int isroot = 0;

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, ARGS, "netinfo_back_search base %s scope %d\n", base->bv_val, scope));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_search base=%s nbase=%s scope=%d\n", base->bv_val, nbase->bv_val, scope);
#endif

	/* check if we should send referral first */
	if (netinfo_back_send_referrals(be, conn, op, nbase) == DSStatusOK)
	{
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: sent referrals\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search\n", 0, 0, 0);
#endif

		return 1;
	}

	ENGINE_LOCK(di);

	/*
	 * Check for trusted_networks here. Previously, trusted_networks
	 * was tested for every search results, which was inefficient
	 * and could potentially result in a DoS attack.
	 */
	if (di->flags & DSENGINE_FLAGS_NATIVE_AUTHORIZATION)
	{
		status = is_trusted_network(be, conn);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
			LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: untrusted network\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search\n", 0, 0, 0);
#endif
			return netinfo_back_op_result(be, conn, op, status);
		}
	}

	status = netinfo_back_dn_pathmatch(be, nbase, &dsid);
	if (status != DSStatusOK)
	{
		ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: pathmatch failed\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search\n", 0, 0, 0);
#endif

		return netinfo_back_op_result(be, conn, op, status);
	}

	switch (scope)
	{
		case LDAP_SCOPE_BASE:
			scopemin = scopemax = 0;
			break;
		case LDAP_SCOPE_SUBTREE:
			scopemin = 0;
			scopemax = (u_int32_t)-1;
			break;
		case LDAP_SCOPE_ONELEVEL:
		default:
			scopemin = scopemax = 1;
			break;
	}

	dsf = filter_to_dsfilter(be, filter);
	if (dsf == NULL)
	{
		ENGINE_UNLOCK(di);
		send_ldap_result(conn, op, LDAP_OPERATIONS_ERROR, NULL,
			"Could not translate filter", NULL, NULL);
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: could not translate filter\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search\n", 0, 0, 0);
#endif
		return -1;
	}

	if (be_isroot(be, &op->o_ndn))
	{
		isroot = 1;
	}
	else
	{
		get_limits(be, &op->o_ndn, &limit);
	}

	/* If root and no specific limit is required, allow untimed search. */
	if (isroot)
	{
		if (tlimit == 0)
			tlimit = -1;
		if (slimit == 0)
			slimit = -1;
	}
	else
	{
		/* If no limit is required, use soft limit. */
		if (tlimit <= 0)
		{
			tlimit = limit->lms_t_soft;
		}
		/* If requested limit is higher than hard limit, abort. */
		else if (tlimit > limit->lms_t_hard)
		{
			/* No hard limit means use soft limit instead. */
			if (limit->lms_t_hard == 0)
				tlimit = limit->lms_t_soft;
			/* Positive hard limit means abort */
			else if (limit->lms_t_hard > 0)
			{
				send_search_result(conn, op, LDAP_UNWILLING_TO_PERFORM,
					NULL, NULL, NULL, NULL, 0);
#ifdef NEW_LOGGING
				LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: reached hard time limit\n"));
#else
				Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search\n", 0, 0, 0);
#endif
				return 0;
			}
			/* Negative hard limit means no limit. */
		}

		/* If no limit is required, use soft limit. */
		if (slimit <= 0)
		{
			slimit = limit->lms_s_soft;
		}
		/* If requested limit is higher than hard limit, abort. */
		else if (slimit > limit->lms_s_hard)
		{
			/* No hard limit means use soft limit instead. */
			if (limit->lms_s_hard == 0)
				slimit = limit->lms_s_soft;
			/* Positive hard limit means abort */
			else if (limit->lms_s_hard > 0)
			{
				send_search_result(conn, op, LDAP_UNWILLING_TO_PERFORM,
					NULL, NULL, NULL, NULL, 0);
#ifdef NEW_LOGGING
				LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: reached hard size limit\n"));
#else
				Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search\n", 0, 0, 0);
#endif
				return 0;
			}
			/* Negative hard limit means no limit. */
		}
	}

	/* Computer stoptime although root odes not use it. */
	stoptime = op->o_time + tlimit;

	status = dsengine_search_filter(di->engine, dsid, dsf, scopemin, scopemax, &match, &count);
	if (status != DSStatusOK)
	{
		dsfilter_release(dsf);
		ENGINE_UNLOCK(di);
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: search failed\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search\n", 0, 0, 0);
#endif
		return netinfo_back_op_result(be, conn, op, status);
	}

	dsfilter_release(dsf);

	if (slimit != LDAP_NO_LIMIT && count > slimit)
	{
		count = slimit;
		rc = LDAP_SIZELIMIT_EXCEEDED;
	}
	else
	{
		rc = LDAP_SUCCESS;
	}

	for (i = 0; i < count; i++)
	{
		Entry *ent;
		dsrecord *rec;

		/* check for abandon */
		if (op->o_abandon)
		{
			ENGINE_UNLOCK(di);
			free(match);
			return 0; /* XXX SLAPD_ABANDON */
		}

		/* check timelimit */
		if (tlimit != -1 && slap_get_time() > stoptime)
		{
			rc = LDAP_TIMELIMIT_EXCEEDED;
			break;
		}

#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: "
			"Fetching record dSID %d\n", match[i]));
#else
		Debug(LDAP_DEBUG_TRACE, "Fetching DS record ID=%d\n", match[i], 0, 0);
#endif

		status = dsengine_fetch(di->engine, match[i], &rec);
		if (status != DSStatusOK)
		{
			ENGINE_UNLOCK(di);
			free(match);
			return netinfo_back_op_result(be, conn, op, status);
		}

		if (di->flags & DSENGINE_FLAGS_NATIVE_AUTHORIZATION)
		{
			status = netinfo_back_authorize(be, conn, op, rec, slap_schema.si_ad_entry, ACL_READ);
			if (status != DSStatusOK)
			{
				dsrecord_release(rec);
				continue;
			}
		}

		status = dsrecord_to_entry(be, rec, &ent);
		if (status != DSStatusOK)
		{
			dsrecord_release(rec);
			continue;
		}

		dsrecord_release(rec);

		send_search_entry(be, conn, op, ent, attrs, attrsonly, NULL);

		netinfo_back_entry_free(ent);
	}

	if (rc == LDAP_SUCCESS)
	{
		struct berval canonicalRelativeDN;

		/* Don't use the normalized relative DN for search references. */
		status = netinfo_back_local_dn(be, dsid, &canonicalRelativeDN);
		if (status == DSStatusOK)
		{
			status = netinfo_back_send_references(be, conn, op, &canonicalRelativeDN, scope);
			ch_free(canonicalRelativeDN.bv_val);
		}
		rc = dsstatus_to_ldap_err(status);
	}

	ENGINE_UNLOCK(di);

	if (count > 0)
		free(match);	

	send_search_result(conn, op, rc, NULL, NULL, NULL, NULL, count);

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_search: %d entries\n", count));
#else
	Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_search count=%d\n", count, 0, 0);
#endif

	return 0;
}
