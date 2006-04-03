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

/*
 * dsrecord
 *
 * A dsrecord is the basic data type in the directory service.
 * Records are hierarchical.  Each record is identified by a unique 
 * number (dsid) and contains the ID number of its super-record and
 * a list of its sub-records.  A record has any number of attributes
 * and meta-attributes.  All attribute keys must be distinct from
 * one-another, as must meta-attribute keys.
 *
 * Each record has two version numbers which are changed each time the
 * record is modified.  The serial number of a record is simply incremented
 * whenever the record's data changes.  This is the same as the "instance"
 * number used in NetInfo.  The "vers" number is a version number for
 * the entire data store, which is incremented whenever any record's data
 * is modified or its list of child records is changed.  This forms the
 * basis for a simple update strategy for replicated databases.  The 
 * highest version number found in a database is the database version.
 * If a replica has a lower version number than the master version (or
 * another replica's version) it may be updated by fetching all records
 * with a higher version number.
 *
 * We maintain both versioning systems to provide compatibility on
 * a network with both old and new NetInfo servers.
 *
 */
 
#ifndef __DSRECORD_H__
#define __DSRECORD_H__

#include <NetInfo/dsstatus.h>
#include <NetInfo/dsdata.h>
#include <NetInfo/dsattribute.h>
#include <stdio.h>

#define SELECT_ATTRIBUTE 0x0
#define SELECT_META_ATTRIBUTE 0x1

typedef struct
{
	u_int32_t dsid;
	u_int32_t serial;
	u_int32_t vers;

	u_int32_t super;

	u_int32_t sub_count;
	u_int32_t *sub;

	u_int32_t count;
	dsattribute **attribute;

	u_int32_t meta_count;
	dsattribute **meta_attribute;

	u_int32_t retain;

	void *index;
	void *next;
} dsrecord;

dsrecord *dsrecord_new(void);
dsrecord *dsrecord_copy(dsrecord *);

dsrecord *dsrecord_retain(dsrecord *);
void dsrecord_release(dsrecord *);

dsrecord *dsrecord_read(char *);
dsrecord *dsrecord_fread(FILE *);

dsstatus dsrecord_write(dsrecord *, char *);
dsstatus dsrecord_fwrite(dsrecord *, FILE *);

dsrecord *dsdata_to_dsrecord(dsdata *);
dsdata *dsrecord_to_dsdata(dsrecord *);

u_int32_t dsrecord_has_sub(dsrecord *, u_int32_t);
void dsrecord_append_sub(dsrecord *, u_int32_t);
void dsrecord_remove_sub(dsrecord *, u_int32_t);

int dsrecord_match(dsrecord *, dsrecord *);
int dsrecord_match_select(dsrecord *, dsrecord *, u_int32_t);
int dsrecord_equal(dsrecord *, dsrecord *);

u_int32_t dsrecord_attribute_index(dsrecord *, dsdata *, u_int32_t);
void dsrecord_merge_attribute(dsrecord *, dsattribute *, u_int32_t);
void dsrecord_append_attribute(dsrecord *, dsattribute *, u_int32_t);
void dsrecord_insert_attribute(dsrecord *r, dsattribute *a, u_int32_t where, u_int32_t asel);
void dsrecord_remove_attribute(dsrecord *, dsattribute *, u_int32_t);
dsattribute *dsrecord_attribute(dsrecord *, dsdata *, u_int32_t);
void dsrecord_remove_key(dsrecord *, dsdata *, u_int32_t);
int dsrecord_match_key_val(dsrecord *, dsdata *, dsdata *, u_int32_t);

dsstatus dsrecord_fstats(FILE *f, u_int32_t *dsid, u_int32_t *vers, u_int32_t *serial, u_int32_t *super);

#endif __DSRECORD_H__
