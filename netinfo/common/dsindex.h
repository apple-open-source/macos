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

#ifndef __DSINDEX_H__
#define __DSINDEX_H__

#include <NetInfo/dsrecord.h>
#include <NetInfo/dsattribute.h>
#include <NetInfo/dsdata.h>
#include <stdio.h>

typedef struct {
	dsdata *val;
	u_int32_t dsid_count;
	u_int32_t *dsid;
} dsindex_val_t;

typedef struct {
	dsdata *key;
	u_int32_t val_count;
	dsindex_val_t **vindex;
} dsindex_key_t;

typedef struct {
	u_int32_t key_count;
	dsindex_key_t **kindex;
} dsindex;

dsindex *dsindex_new(void);
void dsindex_free(dsindex *x);

void dsindex_insert_key(dsindex *x, dsdata *key);
void dsindex_insert_attribute(dsindex *x, dsattribute *a, u_int32_t dsid);
void dsindex_insert_record(dsindex *x, dsrecord *r);
dsindex_val_t *dsindex_lookup(dsindex *x, dsdata *key, dsdata *val);
dsindex_key_t *dsindex_lookup_key(dsindex *x, dsdata *key);
dsindex_val_t *dsindex_lookup_val(dsindex_key_t *kx, dsdata *val);

void dsindex_delete_dsid(dsindex *x, u_int32_t dsid);

void dsindex_print(dsindex *, FILE *);

#endif __DSINDEX_H__
