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
 * Server-side definitions
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * Rename these routines so we can link with the 
 * shared C library
 */
#define ni_setuser	_ni_setuser
#define ni_destroyname	_ni_destroyname
#define ni_writeprop	_ni_writeprop
#define ni_createname	_ni_createname
#define ni_writename	_ni_writename
#define ni_write	_ni_write
#define ni_createprop	_ni_createprop
#define ni_listprops	_ni_listprops
#define ni_readname	_ni_readname
#define ni_setpassword	_ni_setpassword
#define ni_readprop	_ni_readprop
#define ni_parent	_ni_parent
#define ni_create	_ni_create
#define ni_root 	_ni_root
#define ni_self 	_ni_self
#define ni_lookup 	_ni_lookup
#define ni_lookupread 	_ni_lookupread
#define ni_lookupprop 	_ni_lookupprop
#define ni_read 	_ni_read
#define ni_listall 	_ni_listall
#define ni_free 	_ni_free
#define ni_renameprop 	_ni_renameprop
#define ni_destroy 	_ni_destroy
#define ni_destroyprop	_ni_destroyprop
#define ni_children 	_ni_children

#ifdef notdef
/*
 * ni_list is superceded on server-side by ni_list_const (an efficiency hack)
 */
#define ni_list 	_ni_list
#endif

/*
 * Import the standard declarations so they get renamed
 */
#include <netinfo/ni.h>

/*
 * Server-side useful macro
 */
#define ni_name_match_seg(a, b, len) (strncmp(a, b, len) == 0 && (a)[len] == 0)

/*
 * These ni_ routines are visible only on the server side
 */
ni_status ni_list_const(void *, ni_id *, ni_name_const, ni_entrylist *);
void ni_list_const_free(void *);
ni_index ni_highestid(void *);
char *ni_tagname(void *);
ni_status ni_init(char *, void **);
void ni_renamedir(void *, char *);
void ni_forget(void *);
unsigned ni_getchecksum(void *);
void ni_shutdown(void *, unsigned);
