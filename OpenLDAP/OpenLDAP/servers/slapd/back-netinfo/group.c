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
netinfo_back_group(Backend *be,
	Connection *conn,
	Operation *op,
	Entry *target,
	struct berval *gr_ndn,
	struct berval *op_ndn,
	ObjectClass *group_oc,
	AttributeDescription *group_at)
{
	int rc;
	BerVarray vals;
	AttributeDescription *oc_at = slap_schema.si_ad_objectClass;
	ObjectClass *oc;
	BerVarray p;
	int found;

	rc = netinfo_back_attribute(be, conn, op, target, gr_ndn, oc_at, &vals);
	if (rc)
		return rc;

	found = 0;

	for (p = vals; p->bv_val != NULL; p++)
	{
		oc = oc_bvfind(p);
		if (oc == NULL)
			continue;
		if (is_object_subclass(oc, group_oc))
		{
			found = 1;
			break;
		}
	}

	ber_bvarray_free(vals);

	if (found == 0)
		return LDAP_OBJECT_CLASS_VIOLATION;

	rc = netinfo_back_attribute(be, conn, op, target, gr_ndn, group_at, &vals);
	if (rc)
		return rc;

	rc = value_find(group_at, vals, op_ndn);
	ber_bvarray_free(vals);

	return rc;
}

