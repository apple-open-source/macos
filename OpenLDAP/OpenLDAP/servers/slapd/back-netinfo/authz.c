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
#include <ac/ctype.h>

#include "slap.h"
#include "back-netinfo.h"

/* define for _readers auth */
#undef AUTHZ_READERS

/* trusted networks authorization policies, derived from netinfod */
static int network_match LDAP_P((struct in_addr n, struct in_addr h));
static dsstatus getnetwork LDAP_P((dsengine *s, u_int32_t *networks, dsdata *name, struct in_addr *addr));
static dsstatus is_admin LDAP_P((BackendDB *be, dsdata *user));

/*
 * Apply NetInfo authorization (note: _readers authz optional)
 *
 * 1. Reads are by default allowed.
 * 2. Writes are by default denied.
 * 3. Reads can be selectively authorized with the  _readers
 *    property; values are POSIX user names that are authorized to read.
 * 4. Writes can be selectively authorized with the _writers
 *    property; values are POSIX user names that are authorized to read.
 * 5. As a special case of 3 and 4 the properties
 *    _writers_<attribute> and _readers_<attribute>
 *    are also supported.
 */
dsstatus netinfo_back_authorize(
	BackendDB *be,
	Connection *conn,
	Operation *op,
	dsrecord *r,
	AttributeDescription *desc,
	slap_access_t access)
{
	dsattribute *a;
	dsdata *k, *posix, wr;
	dsstatus status;
	int i;

#ifdef NEW_LOGGING
	LDAP_LOG((BACK_NETINFO, ARGS, "netinfo_back_authorize: "
		"dSID %u attribute %s\n", r->dsid, desc->ad_cname.bv_val));
#else
	Debug(LDAP_DEBUG_TRACE, "==> netinfo_back_authorize dsid=%u desc=%s\n", r->dsid, desc->ad_cname.bv_val, 0);
#endif

	/* apply NetInfo authorization model */ 

	status = (access >= ACL_WRITE) ? DSStatusWriteRestricted : DSStatusReadRestricted;

	switch (access)
	{
		case ACL_INVALID_ACCESS:
		case ACL_NONE:
		case ACL_AUTH:
#ifdef NEW_LOGGING
			LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_authorize: "
				"access granted\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_authorize\n", 0, 0, 0);
#endif
			return DSStatusOK;
			break;
		case ACL_COMPARE:
		case ACL_SEARCH:
		case ACL_READ:
#ifdef AUTHZ_READERS
			wr = netinfo_back_name_readers; /* struct copy */
#else
# ifdef NEW_LOGGING
			LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_authorize: "
				"read access granted\n"));
# else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_authorize\n", 0, 0, 0);
# endif
			return DSStatusOK;
#endif /* AUTHZ_READERS */
			break;
		case ACL_WRITE:
			wr = netinfo_back_name_writers; /* struct copy */
			break;
	}

	if ((op->o_dn.bv_len > 0) &&
	    (distinguishedNameToPosixNameTransform(be, &posix,
	     &op->o_dn, DataTypeCaseUTF8Str, NULL) == DSStatusOK))
	{
		/* Always let the NetInfo root user, and optionally admin users, in. */
		if (dsdata_equal(posix, (dsdata *)&netinfo_back_access_user_super) ||
		    (is_admin(be, posix) == DSStatusOK))
		{
#ifdef NEW_LOGGING
			LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_authorize: "
				"NetInfo super-user authorized\n"));
#else
			Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_authorize (NetInfo super-user)\n", 0, 0, 0);
#endif
			dsdata_release(posix);

			return DSStatusOK;
		}
	}
	else
	{
		posix = NULL;
	}

	if (ad_cmp(desc, slap_schema.si_ad_children) == 0 ||
	    ad_cmp(desc, slap_schema.si_ad_entry) == 0)
	{
		k = dsdata_retain(&wr);
	}
	else
	{
		struct atmap map;
		size_t len;

		(void) schemamap_x500_to_ni_at(be, SUPER(r), desc, &map);

		len = wr.length /* includes \0 */ + map.ni_key->length; /* includes \0 */
		if (map.selector == SELECT_META_ATTRIBUTE)
			++len;

		k = dsdata_alloc(len);
		assert(k != NULL);

		k->type = DataTypeCStr;
		k->retain = 1;

		assert(k->data != NULL);
		snprintf(k->data, len, "%s_%s%s", wr.data,
			map.selector == SELECT_META_ATTRIBUTE ? "_" : "",
			dsdata_to_cstring(map.ni_key));

		schemamap_atmap_release(&map);
	}

	assert(k != NULL);

	a = dsrecord_attribute(r, k, SELECT_META_ATTRIBUTE);
	if (a == NULL)
	{
		/*
		 * default NetInfo behaviour is world-read, nobody-write.
		 * if we have a _readers ACL, though, it is exclusive
		 */
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, INFO, "netinfo_back_authorize: "
			"no NetInfo authorization attribute\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_authorize (no auth attr)\n", 0, 0, 0);
#endif
		status = (access >= ACL_WRITE) ? DSStatusWriteRestricted : DSStatusOK;
	}
	else
	{
		for (i = 0; i < a->count; i++)
		{
			dsdata *v = a->value[i];
	
			if (dsdata_equal(v, (dsdata *)&netinfo_back_access_user_anybody) ||
			    dsdata_equal(v, posix))
			{
				status = DSStatusOK;
				break;
			}
		}
#ifdef NEW_LOGGING
		LDAP_LOG((BACK_NETINFO, ENTRY, "netinfo_back_authorize: done\n"));
#else
		Debug(LDAP_DEBUG_TRACE, "<== netinfo_back_authorize\n", 0, 0, 0);
#endif
	}

	dsdata_release(k);
	if (a != NULL)
		dsattribute_release(a);
	if (posix != NULL)
		dsdata_release(posix);

	return status;
}

/*
 * Check ACLs. If the engine was initialized with
 * DSENGINE_FLAGS_NATIVE_AUTHORIZATION, then we
 * apply NetInfo's authorization model:
 *
 * IMPORTANT NOTE: Caller acquires engine lock.
 */
dsstatus netinfo_back_access_allowed(BackendDB *be,
	Connection *conn,
	Operation *op,
	u_int32_t dsid,
	AttributeDescription *desc,
	struct berval *val,
	slap_access_t access)
{
	Entry *e;
	dsstatus status;
	dsrecord *r;
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	/* short circuit if we are root */
	if (be_isroot(be, &op->o_ndn))
	{
		return DSStatusOK;
	}

	assert(di != NULL);

	if (di->flags & DSENGINE_FLAGS_NATIVE_AUTHORIZATION)
	{
		/* First, check whether we are coming in on a trusted network. */
		status = is_trusted_network(be, conn);
		if (status != DSStatusOK)
		{
			return status;
		}
	}

	status = dsengine_fetch(di->engine, dsid, &r);
	if (status != DSStatusOK)
	{
		return status;
	}

	status = (access >= ACL_WRITE) ? DSStatusWriteRestricted : DSStatusReadRestricted;

	if (di->flags & DSENGINE_FLAGS_NATIVE_AUTHORIZATION)
	{
		status = netinfo_back_authorize(be, conn, op, r, desc, access);
		if (status != DSStatusOK)
		{
			dsrecord_release(r);
			return status;
		}
	}

	/* Always AND NetInfo authorization with slapd authorization. */
	/* Consistent with search behaviour (which we can't help) */

	if (dsrecord_to_entry(be, r, &e) == DSStatusOK)
	{
		if (is_entry_alias(e))
			status = DSStatusInvalidUpdate;
		else if (access_allowed(be, conn, op, e, desc, val, access, NULL))
			status = DSStatusOK;
		netinfo_back_entry_free(e);
	}

	dsrecord_release(r);

	return status;
}

static int network_match(struct in_addr n, struct in_addr h)
{
	/* network_match() from Services/netinfo/servers/netinfod/getstuff.c */
	union
	{
		char s_byte[4];
		u_long s_address;
	} net, host;

	net.s_address = n.s_addr;
	host.s_address = h.s_addr;
	
	if (n.s_addr == 0) return (0);

	if (net.s_byte[0] != host.s_byte[0]) return (0);
	if (net.s_byte[1] == 0) return (1);

	if (net.s_byte[1] != host.s_byte[1]) return (0);
	if (net.s_byte[2] == 0) return (1);

	if (net.s_byte[2] != host.s_byte[2]) return (0);
	return (1);
}

static dsstatus getnetwork(dsengine *s, u_int32_t *networks, dsdata *name, struct in_addr *addr)
{
	dsstatus status;
	dsrecord *r;
	dsattribute *a;
	char *p;
	u_int32_t dsid;

	addr->s_addr = 0;

	if (*networks == (u_int32_t)-1)
	{
		status = dsengine_match(s, 0, (dsdata *)&netinfo_back_name_name,
			(dsdata *)&netinfo_back_name_networks, networks);
		if (status != DSStatusOK)
		{
			return status;
		}
	}

	status = dsengine_match(s, *networks, (dsdata *)&netinfo_back_name_name, name, &dsid);
	if (status != DSStatusOK)
	{
		return status;
	}

	status = dsengine_fetch(s, dsid, &r);
	if (status != DSStatusOK)
	{
		return status;
	}

	a = dsrecord_attribute(r, (dsdata *)&netinfo_back_name_address, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		dsrecord_release(r);
		return DSStatusInvalidKey;
	}

	dsrecord_release(r);

	p = dsdata_to_cstring(dsattribute_value(a, 0));
	if (p == NULL)
	{
		dsattribute_release(a);
		return DSStatusNoData;
	}

	*addr = inet_makeaddr(inet_network(p), 0);

	dsattribute_release(a);

	return DSStatusOK;
}

dsstatus is_trusted_network(BackendDB *be, Connection *conn)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsstatus status;
	dsrecord *r;
	dsattribute *a;
	int i;
	struct in_addr network, hostaddr;
	u_int32_t networks = (u_int32_t)-1;
	char *p;

	assert(conn->c_peer_name.bv_val != NULL);
	if (strncmp(conn->c_peer_name.bv_val, "IP=", 3) != 0)
	{
		/* Not an IP client. Allow. */
		return DSStatusOK;
	}

	p = strchr(conn->c_peer_name.bv_val, ':');
	if (p != NULL)
	{
		*p = '\0';
	}

	if (inet_aton(conn->c_peer_name.bv_val + 3, &hostaddr) == 0)
	{
		/* Couldn't parse. */
		if (p != NULL)
			*p = ':';
		return DSStatusAccessRestricted;
	}

	if (p != NULL)
		*p = ':';

	status = dsengine_fetch(di->engine, 0, &r);
	if (status != DSStatusOK)
	{
		return status;
	}

	a = dsrecord_attribute(r, (dsdata *)&netinfo_back_name_trusted_networks, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		dsrecord_release(r);
		return DSStatusOK;
	}

	status = DSStatusAccessRestricted;
	for (i = 0; i < a->count; i++)
	{
		char *val = dsdata_to_cstring(a->value[i]);

		if (val != NULL && isdigit(*val))
		{
			if (NULL == strchr(val, '.'))
			{
				char *temp = ch_malloc(strlen(val) + 2);
				strcpy(temp, val);
				strcat(temp, ".");
				network = inet_makeaddr(inet_network(temp), 0);
				ch_free(temp);
			}
			else
			{
				network = inet_makeaddr(inet_network(val), 0);
			}
		}
		else
		{
			/* Network specified by name */
			if (getnetwork(di->engine, &networks, a->value[i], &network) != DSStatusOK)
			{
				/* Can't find network. Bzzt. */
				continue;
			}
		}

		if (network_match(network, hostaddr))
		{
			status = DSStatusOK;
			break;
		}
	}

	dsattribute_release(a);
	dsrecord_release(r);

	if (status == DSStatusAccessRestricted)
	{
		/* Always trust local connections */
		if (sys_is_my_address(&hostaddr))
		{
			status = DSStatusOK;
		}	
	}

	return status;
}

static dsstatus
is_admin(BackendDB *be, dsdata *user)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private; 
	u_int32_t dsid;
	dsrecord *group;
	dsattribute *a;
	dsstatus status;

	if (di->promote_admins == FALSE)
		return DSStatusAccessRestricted;

	if (user == NULL)
		return DSStatusAccessRestricted;

	status = dsengine_match(di->engine, 0, (dsdata *)&netinfo_back_name_name,
		(dsdata *)&netinfo_back_name_groups, &dsid);
	if (status != DSStatusOK)
		return status;

	status = dsengine_match(di->engine, dsid, (dsdata *)&netinfo_back_name_name,
		(dsdata *)&netinfo_back_name_admin, &dsid);
	if (status != DSStatusOK)
		return status;

	status = dsengine_fetch(di->engine, dsid, &group);
	if (status != DSStatusOK)
		return status;

	a = dsrecord_attribute(group, (dsdata *)&netinfo_back_name_users, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		dsrecord_release(group);
		return DSStatusAccessRestricted;
	}

	status = (dsattribute_index(a, user) == IndexNull) ?
		DSStatusAccessRestricted : DSStatusOK;

	dsattribute_release(a);
	dsrecord_release(group);

	return status;
}
