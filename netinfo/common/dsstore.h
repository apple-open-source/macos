/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

#ifndef __DSSTORE_H__
#define __DSSTORE_H__

/*
 * Data Store 
 */

#include <NetInfo/dsstatus.h>
#include <NetInfo/dsrecord.h>
#include <NetInfo/dscache.h>
#include <stdio.h>

#define DSSTORE_VERSION 1

#define DSSTORE_FLAGS_ACCESS_MASK      0x0001
#define DSSTORE_FLAGS_ACCESS_READONLY  0x0000
#define DSSTORE_FLAGS_ACCESS_READWRITE 0x0001

#define DSSTORE_FLAGS_SERVER_MASK      0x0002
#define DSSTORE_FLAGS_SERVER_CLONE     0x0000
#define DSSTORE_FLAGS_SERVER_MASTER    0x0002

#define DSSTORE_FLAGS_CACHE_MASK       0x0004
#define DSSTORE_FLAGS_CACHE_ENABLED    0x0000
#define DSSTORE_FLAGS_CACHE_DISABLED   0x0004

#define DSSTORE_FLAGS_REMOTE_NETINFO   0x8000
#define DSSTORE_FLAGS_OPEN_BY_TAG      0x4000
#define DSSTORE_FLAGS_NOTIFY_CHANGES   0x0010

typedef struct
{
	char *dsname;
	int store_lock;
	u_int32_t last_sec;
	u_int32_t last_nsec;
	u_int32_t flags;
	u_int32_t max_vers;
	u_int32_t nichecksum;
	dscache *cache;
	int cache_enabled;
	u_int32_t fetch_count;
	u_int32_t save_count;
	u_int32_t remove_count;
	u_int32_t index_count;
	u_int32_t file_info_count;
	u_int32_t unused; /* removed "dirty" flag for 3423618 */
	void **index;
	void **file_info;
	int notify_token;
	char *notification_name;
	void (*sync_delegate)(void *);
	void *sync_private;
} dsstore;

void dsstore_print_index(dsstore *s, FILE *);

dsstatus dsstore_new(dsstore **s, char *, u_int32_t);
dsstatus dsstore_open(dsstore **s, char *, u_int32_t);
dsstatus dsstore_close(dsstore *s);

dsstatus dsstore_authenticate(dsstore *s, dsdata *user, dsdata *password);

dsrecord *dsstore_fetch(dsstore *s, u_int32_t);

dsstatus dsstore_save(dsstore *s, dsrecord *r);
dsstatus dsstore_save_copy(dsstore *s, dsrecord *r);
dsstatus dsstore_save_fast(dsstore *s, dsrecord *r, u_int32_t lock);
dsstatus dsstore_save_attribute(dsstore *s, dsrecord *r, dsattribute *a, u_int32_t asel);

dsstatus dsstore_remove(dsstore *s, u_int32_t);
/*
	NOT IMPLEMENTED
	dsstatus dsstore_remove_attribute(dsstore *s, dsrecord *r, dsattribute *a, u_int32_t asel);
*/

dsstatus dsstore_list(dsstore *s, u_int32_t dsid, dsdata *key, u_int32_t asel, dsrecord **list);
dsstatus dsstore_match(dsstore *s, u_int32_t, dsdata *, dsdata *, u_int32_t, u_int32_t *);

u_int32_t dsstore_max_id(dsstore *s);

u_int32_t dsstore_record_version(dsstore *s, u_int32_t dsid);
u_int32_t dsstore_version_record(dsstore *s, u_int32_t vers);
u_int32_t dsstore_record_serial(dsstore *s, u_int32_t dsid);
u_int32_t dsstore_record_super(dsstore *s, u_int32_t dsid);
u_int32_t dsstore_version(dsstore *s);
dsstatus dsstore_vital_statistics(dsstore *s, u_int32_t dsid, u_int32_t *vers, u_int32_t *serial, u_int32_t *super);

u_int32_t dsstore_nichecksum(dsstore *s);

dsrecord *dsstore_statistics(dsstore *s);

void dsstore_flush_cache(dsstore *s);
void dsstore_reset(dsstore *s);

void dsstore_set_sync_delegate(dsstore *, void (*)(void *), void *);
void dsstore_set_notification_name(dsstore *, const char *);
void dsstore_notify(dsstore *);

#endif __DSSTORE_H__
