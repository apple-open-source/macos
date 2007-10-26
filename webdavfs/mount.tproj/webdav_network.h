/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _WEBDAV_NETWORK_H_INCLUDE
#define _WEBDAV_NETWORK_H_INCLUDE

#include "webdavd.h"

#include <CoreServices/CoreServices.h>

enum
{
	kHttpDefaultPort = 80,	/* default port for HTTP */
	kHttpsDefaultPort = 443	/* default port for HTTPS */
};

struct ReadStreamRec
{
	int inUse;						/* non-zero if this ReadStreamRec is in use */
	CFReadStreamRef readStreamRef;	/* the read stream, or NULL */
	CFStringRef uniqueValue;		/* CFString used to make stream unique */
	int connectionClose;			/* if TRUE, readStreamRef should be closed when transaction is complete */
};

int network_init(
	const UInt8 *uri,			/* -> bytes containing base URI to server */
	CFIndex uriLength,			/* -> length of uri string */
	int *store_notify_fd,		/* <-> pointer to int where the dynamic store notification fd is returned */
	int add_mirror_comment);	/* -> if true, add the mirror comment to the WebDAVFS product in the User-Agent header */

int network_get_proxy_settings(
	int *httpProxyEnabled,		/* <- true if HTTP proxy is enabled */
	char **httpProxyServer,		/* <- HTTP proxy server name */
	int *httpProxyPort,			/* <- HTTP proxy port number */
	int *httpsProxyEnabled,		/* <- true if HTTPS proxy is enabled */
	char **httpsProxyServer,	/* <- HTTPS proxy server name */
	int* httpsProxyPort);		/* <- HTTPS proxy port number */

int network_update_proxy(void *arg);

int network_lookup(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> parent node */
	char *name,					/* -> filename to find */
	size_t name_length,			/* -> length of name */
	struct stat *statbuf);		/* <- stat information is returned in this buffer except for st_ino */

int network_getattr(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> parent node */
	struct stat *statbuf);		/* <- stat information is returned in this buffer */

int network_mount(
	uid_t uid,					/* -> uid of the user making the request */
	int *server_mount_flags);	/* <- flags to OR in with mount flags (i.e., MNT_RDONLY) */
								/* NOTE: if webdavfs is changed to support advlocks, then 
								 * server_mount_flags parameter is not needed.
								 */

int network_finish_download(
	struct node_entry *node,	/* -> node to download to */
	struct ReadStreamRec *readStreamRecPtr); /* -> the ReadStreamRec */

/*
 * Sends an "OPTIONS" request to the server after 'delay' seconds
 * Returns 0 on success. 
 */
int network_server_ping(u_int32_t delay);

int network_open(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> node to open */
	int write_access);			/* -> open requires write access */

int network_statfs(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> root node */
	struct statfs *fs_attr);	/* <- file system information */

int network_create(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> parent node */
	char *name,					/* -> file name to create */
	size_t name_length,			/* -> length of name */
	time_t *creation_date);		/* <- date of the creation */
	
int network_fsync(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> node to sync with server */
	off_t *file_length,			/* <- length of file */
	time_t *file_last_modified); /* <- date of last modification */

int network_remove(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> file node to remove on the server */
	time_t *remove_date);		/* <- date of the removal */

int network_rmdir(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> directory node to remove on the server */
	time_t *remove_date);		/* <- date of the removal */

int network_rename(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *from_node, /* node to move */
	struct node_entry *to_node,	/* node to move over (ignored if NULL) */
	struct node_entry *to_dir_node, /* directory node move into (ignored if to_node != NULL) */
	char *to_name,				/* new name for the object (ignored if to_node != NULL) */
	size_t to_name_length,		/* length of to_name (ignored if to_node != NULL) */
	time_t *rename_date);		/* <- date of the rename */
								/* NOTE: the destination either is to_node, or is the combination
								 * of to_dir_node and to_name.
								 */

int network_lock(
	uid_t uid,					/* -> uid of the user making the request (ignored if refreshing) */
	int refresh,				/* -> if FALSE, we're getting the lock (for uid); if TRUE, we're refreshing the lock */
	struct node_entry *node);	/* -> node to get/renew server lock on */

int network_unlock(
	struct node_entry *node);	/* -> node to unlock on server */
								/* NOTE: uid comes from node */

int network_readdir(
	uid_t uid,					/* -> uid of the user making the request */
	int cache,					/* -> if TRUE, perform additional caching */
	struct node_entry *node);	/* -> directory node to read */

int network_mkdir(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> parent node */
	char *name,					/* -> directory name to create */
	size_t name_length,			/* -> length of name */
	time_t *creation_date);		/* <- date of the creation */

int network_read(
	uid_t uid,					/* -> uid of the user making the request */
	struct node_entry *node,	/* -> node to read */
	off_t offset,				/* -> position within the file at which the read is to begin */
	size_t count,				/* -> number of bytes of data to be read */
	char **buffer,				/* <- buffer data was read into (allocated by network_read) */
	size_t *actual_count);		/* <- number of bytes actually read */

time_t DateBytesToTime(			/* <- time_t value; -1 if error */
	const UInt8 *bytes,			/* -> pointer to bytes to parse */
	CFIndex length);			/* -> number of bytes to parse */

#endif
