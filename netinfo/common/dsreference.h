/*
 * Copyright (c) 2002 PADL Software Pty Ltd. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2002 PADL Software Pty Ltd.  All Rights
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


/*
 * A dsreference is a attribute value type which represents a 
 * pointer to another record. It is designed to help directory
 * servers provide cross-domain referential integrity.
 *
 * When a user adds a value with distinguishedName syntax,
 * for example a member of an X.500 group, the directory
 * server should create a new dsreference with the DN,
 * serialize it as a dsdata, and use that for the attribute
 * value.
 *
 * If the reference is local to the store, then it may also
 * fill out the the other structure fields and touch the
 * timestamp.
 *
 * Otherwise, it should lodge a notification with the
 * referential integrity service, which will read the entry
 * being referred to (via LDAP) and rewrite the dsreference
 * with the entry's UUID, DSID, canonical DN, etc.
 *
 * The referential integrity service is also responsible for
 * periodically skulking the directory for stale references,
 * and either deleting or updating them. If an entry has
 * the entryUUID operational attribute, then this should be
 * used as the canonical identifier for an entry; otherwise,
 * its datastore ID (DSID) should be used.
 */

#ifndef __DSREFERENCE_H__
#define __DSREFERENCE_H__

#include <NetInfo/dsstatus.h>
#include <NetInfo/dsdata.h>
#include <NetInfo/dsrecord.h>
#include <stdio.h>

typedef struct {
	u_int32_t time_low;
	u_int16_t time_mid;
	u_int16_t time_hi_and_version;
	u_int8_t clock_seq_hi_and_reserved;
	u_int8_t clock_seq_low;
	char node[6];
} dsuuid_t;

#define IsNullUuid(u) (((u).time_low == 0) && \
	((u).time_mid == 0) && \
	((u).time_hi_and_version == 0) && \
	((u).clock_seq_hi_and_reserved == 0) && \
	((u).clock_seq_low == 0) && \
	((u).node[0] == 0) && \
	((u).node[1] == 0) && \
	((u).node[2] == 0) && \
	((u).node[3] == 0) && \
	((u).node[4] == 0) && \
	((u).node[5] == 0))
	
typedef struct {
	u_int32_t dsid;    /* dSID */
	u_int32_t serial;  /* nISerialNumber */
	u_int32_t vers;    /* nIVersionNumber */
	u_int32_t timestamp;

	dsuuid_t uuid;     /* entryUUID */

	dsdata *dn;        /* distinguishedName */
	dsdata *name;      /* name */

	u_int32_t retain;
} dsreference;

/*
 * Create an empty, initialized dsreference.
 */
dsreference *dsreference_new(void);

dsreference *dsreference_copy(dsreference *);

dsreference *dsreference_retain(dsreference *);
void dsreference_release(dsreference *);

/*
 * Unserialize a dsreference from an attribute value.
 */
dsreference *dsdata_to_dsreference(dsdata *);

/*
 * Serialize a dsreference for attribute storage.
 */
dsdata *dsreference_to_dsdata(dsreference *);

/*
 * Compare two dsreferences.
 */
int dsreference_equal(dsreference *, dsreference *);

/*
 * Read attributes from a dsrecord to construct a 
 * reference.
 */
dsreference *dsrecord_to_dsreference(dsrecord *);

#endif __DSREFERENCE_H__
