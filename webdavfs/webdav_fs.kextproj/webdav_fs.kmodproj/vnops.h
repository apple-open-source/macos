
#ifndef _VNOPS_H_INCLUDE
#define _VNOPS_H_INCLUDE


/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*		@(#)vnops.h
*
*		(c) 1999 Apple Computer, Inc.  All Rights Reserved
*
*		HISTORY
*		01-June-1999	CHW Created this file
*/

/* Webdav file operation constants */
#define WEBDAV_FILE_OPEN		1
#define WEBDAV_DIR_OPEN			2
#define WEBDAV_CLOSE			3
#define WEBDAV_FILE_FSYNC		4
#define WEBDAV_LOOKUP			5
#define WEBDAV_STAT				6
#define WEBDAV_FILE_CREATE		7
#define WEBDAV_DIR_CREATE		8
#define WEBDAV_FILE_DELETE		9
#define WEBDAV_DIR_DELETE		10
#define WEBDAV_RENAME			11
#define WEBDAV_DIR_REFRESH		12
#define WEBDAV_STATFS			13
#define WEBDAV_BYTE_READ		14
#define WEBDAV_DIR_REFRESH_CACHE 15
#define WEBDAV_INVALIDATE_CACHES 16

/* Constants for sending mesages */

#define WEBDAV_USE_URL		   1
#define WEBDAV_USE_HANDLE	   2
#define WEBDAV_USE_INPUT	   3

/* Webdav file type constants */
#define WEBDAV_FILE_TYPE		1
#define WEBDAV_DIR_TYPE			2

/* Other constants */
#define WEBDAV_DIR_SIZE			2048

/* Shared (kernel & processs) WebDAV structures */

typedef int webdav_filehandle_t;
typedef int webdav_filetype_t;

/* Shared (kernel & process) WebDAV defninitions */

#define WEBDAV_ROOTFILEID		3

/* Macros */

#define WEBDAV_CHECK_VNODE(vp) ((vp)->v_type == VBAD ? -1:0)

#endif /*ifndef _VNOPS_H_INCLUDE */
