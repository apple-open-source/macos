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
 * Low-level NetInfo file handling definitions
 * Copyright (C) 1989 by NeXT, Inc.
 */
ni_status file_init(char *, void **);
void file_renamedir(void *, char *);
char *file_dirname(void *);
void file_free(void *);
void file_shutdown(void *, unsigned);
ni_status file_rootid(void *, ni_id *);
ni_status file_idalloc(void *, ni_id *);
ni_status file_idunalloc(void *, ni_id);
ni_status file_read(void *, ni_id *, ni_object **);
ni_status file_write(void *, ni_object *);
ni_status file_writecopy(void *, ni_object *);

unsigned file_getchecksum(void *);

ni_index file_highestid(void *);
void file_sync(void *);

void file_forget(void *);

ni_index file_store_version(void *);
ni_index file_version(void *, ni_id);

void file_notify(void *hdl);
