/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
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

#ifndef __DSX500DIT_H__
#define __DSX500DIT_H__

/*
 * Traverse the NetInfo hierarchy, looking for the "suffix"
 * property and/or the domain name (which we map to an
 * organizationalUnit).
 *
 * The "suffix" property behaves like the "serves" property,
 * ie. it is stored a subdirectory of /machines, and
 * refers to the child entry. Indexes must match up with
 * the serves property, for example:
 *
 *	/machines/trane
 *		serves "./network" "trane/local"
 *		suffix "" "ou=John Coltrane"
 *
 * Note the blank suffix property value so that the
 * suffix matches up with the child domain serves
 * value. A special case of the suffix property is
 * in the root domain, where (rather than determining
 * the suffix for a child domain) the value of the
 * property determine the suffix for the current domain.
 *
 * If a dsstore handle is passed in that refers to
 * a NetInfo server, then the working domain is
 * determined from that. If a dsstore is passed in,
 * then the working domain is determined from the
 * "master" property in the root directory. If there is
 * no "master" property, then the suffix is "" and
 * no parent/child referrals are returned.
 *
 * We also return referrals to parent and child domains.
 */
#include <NetInfo/dsdata.h>
#include <NetInfo/dsengine.h>

typedef struct
{
	dsdata *local_suffix;         /* local DN suffix */
	dsattribute *parent_referrals; /* parent context/referrals */
	u_int32_t child_count;        /* number of child referrals */
	dsattribute **child_referrals;/* subcontext/referral chain */
	u_int32_t retain;             /* reference count */
} dsx500dit;

dsx500dit *dsx500dit_new(dsengine *s);
dsx500dit *dsx500dit_retain(dsx500dit *i);
void dsx500dit_release(dsx500dit *i);

#endif  __DSX500DIT_H__
