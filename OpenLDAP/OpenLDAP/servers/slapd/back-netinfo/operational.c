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
#include "back-netinfo.h"

static Attribute *make_dsrecord_operational_attr LDAP_P((AttributeDescription *desc, u_int32_t value));

int
netinfo_back_operational(
	BackendDB *be,
	Connection *conn,
	Operation *op,
	Entry *e,
	AttributeName *attrs,
	int opattrs,
	Attribute **a)
{
	dsrecord *r;
	Attribute **aa = a;
	struct dsinfo *di = (struct dsinfo *)be->be_private;

	assert(di != NULL);

	/* NB: engine already locked. */
	r = (dsrecord *)e->e_private;
	assert(r != NULL);

	if (opattrs || ad_inlist(slap_schema.si_ad_hasSubordinates, attrs))
	{
		int hasChildren;

		/*
		 * Check whether we have any sub-ordinate entries. We
		 * can tell by counting the number of child records or,
		 * failing that, child referrals.
		 */
		hasChildren = (r->sub_count > 0);
		if (!hasChildren && r->dsid == 0)
			hasChildren = (di->children != NULL) && (*(di->children) != NULL);

		*aa = slap_operational_hasSubordinate(hasChildren);
		if (*aa != NULL)
			aa = &(*aa)->a_next;
	}

	/*
	 * The following operational attributes return information
	 * about the dsrecord generally used for replication purposes.
	 */
	if (opattrs || ad_inlist(netinfo_back_ad_dSID, attrs))
	{
		*aa = make_dsrecord_operational_attr(netinfo_back_ad_dSID, r->dsid);
		if (*aa != NULL)
			aa = &(*aa)->a_next;
	}

	if (opattrs || ad_inlist(netinfo_back_ad_nIVersionNumber, attrs))
	{
		*aa = make_dsrecord_operational_attr(netinfo_back_ad_nIVersionNumber, r->vers);
		if (*aa != NULL)
			aa = &(*aa)->a_next;
	}

	if (opattrs || ad_inlist(netinfo_back_ad_nISerialNumber, attrs))
	{
		*aa = make_dsrecord_operational_attr(netinfo_back_ad_nISerialNumber, r->serial);
		if (*aa != NULL)
			aa = &(*aa)->a_next;
	}

	return 0;
}

static Attribute *make_dsrecord_operational_attr(AttributeDescription *desc, u_int32_t value)
{
	Attribute *a;
	char buf[32];

	snprintf(buf, sizeof(buf), "%u", value);

	a = ch_malloc(sizeof(Attribute));
	a->a_desc = desc;
	a->a_vals = ch_malloc(2 * sizeof(struct berval));

	a->a_vals[0].bv_val = ch_strdup(buf);
	a->a_vals[0].bv_len = strlen(a->a_vals[0].bv_val);

	a->a_vals[1].bv_val = NULL;

	a->a_next = NULL;
	a->a_flags = 0;

	return a;
}
