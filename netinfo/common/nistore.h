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

#ifndef __NISTORE_H__
#define __NISTORE_H__

/*
 * NetInfo shim
 */

#include <NetInfo/dsstore.h>

dsstatus nistore_open(dsstore **s, char *dname, u_int32_t flags);
dsstatus nistore_close(dsstore *s);
dsstatus nistore_authenticate(dsstore *s, dsdata *user, dsdata *password);

dsrecord *nistore_fetch(dsstore *s, u_int32_t dsid);

dsstatus nistore_save(dsstore *s, dsrecord *r);
dsstatus nistore_save_attribute(dsstore *s, dsrecord *r, dsattribute *a, u_int32_t asel);

dsstatus nistore_remove(dsstore *s, u_int32_t dsid);
/*
	NOT IMPLEMENTED
	dsstatus nistore_remove_attribute(dsstore *s, dsrecord *r, dsattribute *a, u_int32_t asel);
*/

dsstatus nistore_list(dsstore *s, u_int32_t dsid, dsdata *key, u_int32_t asel, dsrecord **list);
dsstatus nistore_match(dsstore *s, u_int32_t dsid, dsdata *key, dsdata *val, u_int32_t asel, u_int32_t *match);

u_int32_t nistore_record_version(dsstore *s, u_int32_t dsid);
u_int32_t nistore_version_record(dsstore *s, u_int32_t vers);
u_int32_t nistore_record_serial(dsstore *s, u_int32_t dsid);
u_int32_t nistore_record_super(dsstore *s, u_int32_t dsid);
u_int32_t nistore_version(dsstore *s);

dsstatus nistore_vital_statistics(dsstore *s, u_int32_t dsid, u_int32_t *vers, u_int32_t *serial, u_int32_t *super);

dsrecord *nistore_statistics(dsstore *s);

#endif __NISTORE_H__
