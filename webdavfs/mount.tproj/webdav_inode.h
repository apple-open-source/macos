#ifndef _WEBDAV_INODE_H_INCLUDE
#define _WEBDAV_INODE_H_INCLUDE


/* webdav_inode.h created by warner_c on Wed 03-Jan-2001 */

/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').	You may not use this file
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
/*		@(#)webdav_inode.h		*
 *		(c) 2001   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_inode.h -- Headers for WebDAV inode cacheing
 *
 *		MODIFICATION HISTORY:
 *				3-Jan-01	 Clark Warner	   File Creation
 */


#include <sys/types.h>
#include "../webdav_fs.kextproj/webdav_fs.kmodproj/vnops.h"

/*definitions */

#define WEBDAV_FILE_RECORD_HASH_BUCKETS	 8192
#define WEBDAV_UNIQUE_HASH_CHARS	 16

/* Type definitions */

typedef struct webdav_file_record_tag
{
	char *uri;									/* The file record has utf8 uri's only */
	int uri_length;
	int inode;
	webdav_filehandle_t file_handle;
	struct webdav_file_record_tag *next;
} webdav_file_record_t;


/*Externals */
extern webdav_file_record_t * ginode_hashtbl[WEBDAV_FILE_RECORD_HASH_BUCKETS];
extern u_int32_t ginode_cntr;
extern pthread_mutex_t ginode_lock;

/* Functions */
extern int webdav_inode_init(char * uri, int urilen);
extern int webdav_get_inode (const char * uri, int length,int make_entry, int * inode);
extern int webdav_set_inode (const char * uri, int length, int inode);
extern int webdav_remove_inode (const char * uri, int length);
extern int webdav_get_file_handle (const char * uri, int length, webdav_filehandle_t * a_file_handle);
extern int webdav_set_file_handle (const char * uri, int length, webdav_filehandle_t file_handle);

#endif
