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

typedef struct im_handle {
	void *private;
} im_handle;

im_handle im_alloc(void);
void im_free(im_handle *);
void im_forget(im_handle *);

void im_newnode(im_handle *, ni_object *, ni_index);
void im_remove(im_handle *, ni_object *);

void im_create_all(im_handle *, ni_object *, ni_proplist);
void im_destroy_all(im_handle *, ni_object *, ni_proplist);
int im_has_indexed_dir(im_handle *, ni_index, ni_name_const, ni_name_const,
		       ni_index **, ni_index *);
void im_store_index(im_handle *, index_handle, ni_index, ni_name_const);
void im_destroy_list(im_handle *, ni_object *, ni_name_const, ni_namelist);
void im_create_list(im_handle *, ni_object *, ni_name_const, ni_namelist);
void im_destroy(im_handle *, ni_object *, ni_name_const, ni_name_const, ni_index);
void im_create(im_handle *, ni_object *, ni_name_const, ni_name_const, ni_index);

int im_has_saved_list(im_handle *, ni_index, ni_name_const, ni_entrylist *);
void im_store_list(im_handle *, ni_index, ni_name_const, ni_entrylist);
