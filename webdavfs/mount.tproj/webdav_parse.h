/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef _WEBDAV_PARSE_H_INCLUDE
#define _WEBDAV_PARSE_H_INCLUDE

#include <sys/types.h>
#include <sys/dirent.h> // for __DARWIN_MAXNAMELEN

/* Types */

typedef struct
{
	int context;
	char *locktoken;
} webdav_parse_lock_struct_t;

struct webdav_parse_cachevalidators_struct
{
	time_t last_modified;
	char *entity_tag;
};

struct webdav_quotas
{
	int			use_bytes_values;		/* if TRUE, use quota_available_bytes and quota_used_bytes */
	uint64_t	quota_available_bytes;	/* DAV:quota-available-bytes property value */
	uint64_t	quota_used_bytes;		/* DAV:quota-used-bytes property value */
	uint64_t	quota;					/* deprecated DAV:quota property value */
	uint64_t	quotaused;				/* deprecated DAV:quotaused property value */
};

/* This needs to be big enough for a pathname where every character comes to us
 * in URI Escaped Encoding. That means that each character could expand to
 * 3 characters (i.e., ' ' = '%20').
 */
#define WEBDAV_MAX_URI_LEN ((MAXPATHLEN * 3) + 1)

/* This structure must be exactly like "struct dirent", defined in sys/dirent.h,
   except that d_name is long enough to hold the entire URI and the URI length
   is stored in d_name_URI_length. */
struct large_dirent
{
	webdav_ino_t d_ino;							/* file number of entry */
	u_int16_t d_reclen;							/* sizeof(struct dirent) */
	u_int8_t d_type;							/* file type, see below */
	u_int8_t d_namlen;							/* length of string in d_name */
	char d_name[WEBDAV_MAX_URI_LEN];			/* name must be no longer than this */
	u_int32_t d_name_URI_length;				/* the length of the URI stored in
												 * d_name until it is shortened
												 * to a file name */
};

// XXX Dependency on __DARWIN_64_BIT_INO_T
// struct dirent is in flux right now because __DARWIN_64_BIT_INO_T is set to 1 for user space,
// but set to zero for kernel space.
// So user space sees dirent as:
//
// struct dirent {
// __uint64_t  d_ino;      /* file number of entry */
// __uint64_t  d_seekoff;  /* seek offset (optional, used by servers) */
// __uint16_t  d_reclen;   /* length of this record */
// __uint16_t  d_namlen;   /* length of string in d_name */
// __uint8_t   d_type;     /* file type, see below */
// char      d_name[__DARWIN_MAXPATHLEN]; /* entry name (up to MAXPATHLEN bytes) */
// };
// 
// But kernel space sees dirent like this:
//
// struct dirent {
//	ino_t d_ino;			/* file number of entry */
//	__uint16_t d_reclen;		/* length of this record */
//	__uint8_t  d_type; 		/* file type, see below */
//	__uint8_t  d_namlen;		/* length of string in d_name */
//	char d_name[__DARWIN_MAXNAMLEN + 1];	/* name must be no longer than this */
// };
//
// So until kernel and user space sees the same dirent, we need to use our
// own type since we pass a struct dirent from user to kernel space when we
// process readdir vnop.
//
// Once __DARWIN_64_BIT_INO_T is set to 1 for both user and kernel space, we
// can get rid of webdav_dirent and just use dirent exclusively.
//
struct webdav_dirent {
		webdav_ino_t d_ino;			/* file number of entry */
		__uint16_t d_reclen;		/* length of this record */
		__uint8_t  d_type; 		/* file type, see below */
		__uint8_t  d_namlen;		/* length of string in d_name */
		char d_name[__DARWIN_MAXNAMLEN + 1];	/* name must be no longer than this */
};

typedef struct webdav_parse_opendir_element_tag
{
	struct large_dirent dir_data;
	struct timespec stattime;
	struct timespec createtime;
	u_quad_t statsize;
	int appledoubleheadervalid;	/* TRUE if appledoubleheader field is valid */
	int seen_href;	/* TRUE if we've seen the <D:href> entity for this element (otherwise this is a place holder) */
	int seen_response_end; /* TRUE if we've seen <d:/response> for this element */
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
	CFIndex size;
	UInt8 name[WEBDAV_MAX_URI_LEN];
} webdav_parse_opendir_text_t;

typedef struct
{
	int id;
	void *data_ptr;
} webdav_parse_opendir_return_t;

/* Parsing Multistatus replies for LOCK & DELETE */

#define WEBDAV_MULTISTATUS_INVALID_STATUS 999
typedef struct
{
	CFIndex size;
	UInt8 name[WEBDAV_MAX_URI_LEN];
} webdav_parse_multistatus_text_t;

typedef struct
{
	int id;
	void *data_ptr;
} webdav_parse_multistatus_return_t;

typedef struct webdav_parse_multistatus_element_tag
{
	UInt32 statusCode;
	UInt8  name_len;		/* length of string in name */
	UInt8 name[WEBDAV_MAX_URI_LEN];
	
	/* some bookkeeping fields, only used during parsing */
	int seen_href;	/* TRUE if we've seen the <D:href> entity for this element (otherwise this is a place holder) */
	int seen_response_end; /* TRUE if we've seen <d:/response> for this element */
	
	struct webdav_parse_multistatus_element_tag *next;
} webdav_parse_multistatus_element_t;

typedef struct
{
	int error;
	webdav_parse_multistatus_element_t *head;
	webdav_parse_multistatus_element_t *tail;
} webdav_parse_multistatus_list_t;

/* Functions */
extern int parse_stat(const UInt8 *xmlp, CFIndex xmlp_len, struct webdav_stat_attr *statbuf);
extern int parse_statfs(const UInt8 *xmlp, CFIndex xmlp_len, struct statfs *statfsbuf);
extern int parse_lock(const UInt8 *xmlp, CFIndex xmlp_len, char **locktoken);
extern int parse_opendir(
	UInt8 *xmlp,					/* -> xml data returned by PROPFIND with depth of 1 */
	CFIndex xmlp_len,				/* -> length of xml data */
	CFURLRef urlRef,				/* -> the CFURL to the parent directory (may be a relative CFURL) */
	uid_t uid,						/* -> uid of the user making the request */ 
	struct node_entry *parent_node);/* -> pointer to the parent directory's node_entry */
extern int parse_file_count(const UInt8 *xmlp, CFIndex xmlp_len, int *file_count);
extern int parse_cachevalidators(const UInt8 *xmlp, CFIndex xmlp_len, time_t *last_modified, char **entity_tag);
extern webdav_parse_multistatus_list_t *parse_multi_status(	UInt8 *xmlp, CFIndex xmlp_len);
/* Definitions */

#define WEBDAV_OPENDIR_ELEMENT 1	/* Make it not 0 (for null) but small enough to not be a ptr */
#define WEBDAV_OPENDIR_ELEMENT_LENGTH 2
#define WEBDAV_OPENDIR_ELEMENT_MODDATE 3
#define WEBDAV_OPENDIR_ELEMENT_CREATEDATE 4
#define WEBDAV_OPENDIR_TEXT 5
#define WEBDAV_OPENDIR_APPLEDOUBLEHEADER 6
#define WEBDAV_OPENDIR_ELEMENT_RESPONSE 7
#define WEBDAV_OPENDIR_IGNORE 8		/* Same Rules Apply */

#define WEBDAV_MULTISTATUS_ELEMENT 1
#define WEBDAV_MULTISTATUS_STATUS 2
#define WEBDAV_MULTISTATUS_TEXT 3
#define WEBDAV_MULTISTATUS_RESPONSE 4
#define WEBDAV_MULTISTATUS_IGNORE 8	

#define WEBDAV_STAT_IGNORE 1
#define WEBDAV_STAT_LENGTH 2
#define WEBDAV_STAT_MODDATE 3
#define WEBDAV_STAT_CREATEDATE 4

#define WEBDAV_STATFS_IGNORE 1
#define WEBDAV_STATFS_QUOTA_AVAILABLE_BYTES 2
#define WEBDAV_STATFS_QUOTA_USED_BYTES 3
/*
 * WEBDAV_STATFS_QUOTA and WEBDAV_STATFS_QUOTAUSED correspond to
 * the deprecated "quota" and "quotaused" properties in the "DAV:" namespace.
 */
#define WEBDAV_STATFS_QUOTA 4
#define WEBDAV_STATFS_QUOTAUSED 5

#define WEBDAV_LOCK_CONTINUE 1
#define WEBDAV_LOCK_TOKEN 1
#define WEBDAV_LOCK_HREF 2

#define WEBDAV_CACHEVALIDATORS_IGNORE 1
#define WEBDAV_CACHEVALIDATORS_MODDATE 2
#define WEBDAV_CACHEVALIDATORS_ETAG 3

#endif
