
/* webdav_parse.h created by warner_c on Mon 19-Jul-1999 */

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
/*		@(#)webdav_parse.h		*
 *		(c) 1999   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_parse.h -- Headers for WebDAV XML parsing
 *
 *		MODIFICATION HISTORY:
 *				19-JUL-99	  Clark Warner		File Creation
 */
#include <stdio.h>
#include <sys/dirent.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>
#include "webdav_memcache.h"
#include "../webdav_fs.kextproj/webdav_fs.kmodproj/vnops.h"

/* Types */

typedef struct webdav_parse_lookup_element_tag
{
	webdav_filetype_t file_type;
	struct webdav_parse_lookup_element_tag *next;
} webdav_parse_lookup_element_t;

typedef struct
{
	webdav_parse_lookup_element_t *head;
	webdav_parse_lookup_element_t *tail;
} webdav_parse_lookup_struct_t;

/* This needs to be big enough for a pathname where every character comes to us
 * in URI Escaped Encoding. That means that each character could expand to
 * 3 characters (i.e., ' ' = '%20').
 */
#define WEBDAV_MAX_URI_LEN (MAXPATHLEN * 3)

/* This structure must be exactly like "struct dirent", defined in sys/dirent.h,
   except that d_name is long enough to hold the entire URI and the URI length
   is stored in d_name_URI_length. */
struct large_dirent
{
	u_int32_t d_fileno;							/* file number of entry */
	u_int16_t d_reclen;							/* sizeof(struct dirent) */
	u_int8_t d_type;							/* file type, see below */
	u_int8_t d_namlen;							/* length of string in d_name */
	char d_name[WEBDAV_MAX_URI_LEN];			/* name must be no longer than this */
	u_int32_t d_name_URI_length;				/* the length of the URI stored in
												 * d_name until it is shortened
												 * to a file name */
};

typedef struct webdav_parse_opendir_element_tag
{
	struct large_dirent dir_data;
	struct timespec stattime;
	u_quad_t statsize;
	int appledoubleheadervalid;	/* TRUE if appledoubleheader field is valid */
	char appledoubleheader[APPLEDOUBLEHEADER_LENGTH];
	struct webdav_parse_opendir_element_tag *next;
} webdav_parse_opendir_element_t;

typedef struct
{
	int error;
	webdav_parse_opendir_element_t *head;
	webdav_parse_opendir_element_t *tail;
} webdav_parse_opendir_struct_t;

typedef struct
{
	size_t size;
	char name[WEBDAV_MAX_URI_LEN];
} webdav_parse_opendir_text_t;

typedef struct
{
	int id;
	void *data_ptr;
} webdav_parse_opendir_return_t;

/* Functions */
extern int parse_lookup(char *xmlp, int xmlp_len, webdav_filetype_t *a_file_type);
extern int parse_stat(char *xmlp, int xmlp_len, const char *orig_uri, struct vattr *statbuf, uid_t uid);
extern int parse_statfs(char *xmlp, int xmlp_len, struct statfs *statfsbuf);
extern int parse_lock(char *xmlp, int xmlp_len, char **locktoken);
extern int parse_opendir(char *xmlp, int xmlp_len, int fd, char *dir_name, char *hostname, uid_t uid);
extern int parse_file_count(char *xmlp, int xmlp_len, int *file_count);
extern int parse_getlastmodified(char *xmlp, int xmlp_len, time_t *last_modified);

/* Definitions */

#define WEBDAV_OPENDIR_ELEMENT 1	/* Make it not 0 (for null) but small enough to not be a ptr */
#define WEBDAV_OPENDIR_ELEMENT_LENGTH 2
#define WEBDAV_OPENDIR_ELEMENT_MODDATE 3
#define WEBDAV_OPENDIR_TEXT 4
#define WEBDAV_OPENDIR_APPLEDOUBLEHEADER 5
#define WEBDAV_OPENDIR_IGNORE 6		/* Same Rules Apply */

#define WEBDAV_STAT_IGNORE 1
#define WEBDAV_STAT_LENGTH 2
#define WEBDAV_STAT_MODDATE 3

#define WEBDAV_STATFS_IGNORE 1
#define WEBDAV_STATFS_QUOTA 2
#define WEBDAV_STATFS_QUOTAUSED 3

#define WEBDAV_LOCK_CONTINUE 1
#define WEBDAV_LOCK_TOKEN 1
#define WEBDAV_LOCK_HREF 2

#define WEBDAV_GETLASTMODIFIED_IGNORE 1
#define WEBDAV_GETLASTMODIFIED_MODDATE 2

#define WEBDAV_MAX_STATFS_SIZE 256
#define WEBDAV_MAX_STAT_SIZE 256	/* needs to be large enough to hold maximum date and maximum
										char length of size */
 
