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

#ifndef _WEBDAV_CACHE_H_INCLUDE
#define _WEBDAV_CACHE_H_INCLUDE

#include <sys/types.h>
#include <sys/queue.h>

/*****************************************************************************/

/* define node_head structure */
LIST_HEAD(node_head, node_entry);

struct node_entry
{
	LIST_ENTRY(node_entry)  entries;				/* the other nodes on the parent's children list */
	struct node_entry		*parent;				/* the parent node_entry, or NULL if this is the root node */
	LIST_HEAD(, node_entry) children;				/* this node's children (if any) */
	
	/*
	 * Node identification fields
	 */
	size_t					name_length;			/* length of name */
	char					*name;					/* the utf8 name */
	CFStringRef				name_ref;				/* the name as a CFString */
	webdav_ino_t			fileid;					/* file ID number */
	webdav_filetype_t		node_type;				/* (int) either WEBDAV_FILE_TYPE or WEBDAV_DIR_TYPE */
	u_int32_t				flags;
	opaque_id				nodeid;					/* opaque_id assigned to this node */
	time_t					node_time;				/* local time - when node was validated on server */
	
	/*
	 * Attribute fields
	 *
	 * Set attr_time to 0 and free the memory used by attr_ref (if any) to invalidate attributes.
	 */
	uid_t					attr_uid;				/* user authorized to use attribute data */
	time_t					attr_time;				/* local time - when attribute data was received from server */
	struct stat				attr_stat;				/* stat attributes */
	/* file system specific attribute data */
	time_t					attr_appledoubleheader_time; /* local time - when attr_appledoubleheader was received from server */
	char					*attr_appledoubleheader; /* NULL if no appledoubleheader data */
	
	/*
	 * File cache fields
	 *
	 * Clear the nodeInFileList flag bit and insert into the from the file_list make it a valid cache file.
	 *
	 * Clear the nodeInFileList flag bit and remove from the file_list to invalidate a cache file. Don't forget to free the 
	 * file_ref before you free the node.
	 *
	 * A valid cache file can be either active (the file or directory it is a
	 * cache for is open) or inactive (the file or directory it is a cache for
	 * is closed). Active cache files have a file_inactive_time of zero;
	 * inactive cache files have the time the cache file was made inactive so
	 * that they can be aged out of the cache.
	 */
	LIST_ENTRY(node_entry)  file_list;				/* the g_file_list */
	int						file_fd;				/* the cache file's file descriptor or -1 if none */
	u_int32_t				file_status;			/* the status of the cache file download:
													 *		WEBDAV_DOWNLOAD_NEVER (never downloaded)
													 *		WEBDAV_DOWNLOAD_IN_PROGRESS (download still in progress)
													 *		WEBDAV_DOWNLOAD_FINISHED (download complete)
													 *		WEBDAV_DOWNLOAD_ABORTED (download was stopped before complete because of close or error)
													 * If WEBDAV_DOWNLOAD_TERMINATED is set, an in-progress download should be stopped.
													 * Note: the download_status field should be word aligned.
													 */
	time_t					file_validated_time;	/* local time - when cache file was last validated by server */
	time_t					file_inactive_time;		/* local time - when cache file was made inactive (the file this cache is for was closed) - 0 if active */
	/* file system specific file cache data */
	time_t					file_last_modified;		/* the HTTP-date converted to time_t from the Last-Modified entity-header or from the getlastmodified property, or -1 if no valid Last-Modified date */
	char					*file_entity_tag;		/* The entity-tag from the ETag response-header or from the getetag property */
	uid_t					file_locktoken_uid;		/* the uid associated with the locktoken (filesystem_close and filesystem_lock need it to renew locks and to unlock). */
	char					*file_locktoken;		/* the lock token, or NULL */

	/* Context for sequential writes */
	struct stream_put_ctx* put_ctx;
};

#define WEBDAV_DOWNLOAD_NEVER		0
#define WEBDAV_DOWNLOAD_IN_PROGRESS	1
#define WEBDAV_DOWNLOAD_FINISHED	2
#define WEBDAV_DOWNLOAD_ABORTED		3
#define WEBDAV_DOWNLOAD_STATUS_MASK	0x7fffffff
#define WEBDAV_DOWNLOAD_TERMINATED	0x80000000

/* node_entry flags */
enum
{
	nodeDeletedBit			= 0,			/* the node is deleted and is on the deleted list */ 
	nodeDeletedMask			= 0x00000001,
	nodeInFileListBit		= 1,			/* the node is cached and is on the file list */
	nodeInFileListMask		= 0x00000002,
	nodeRecentBit			= 2,			/* the file node was recently created by this client or the directory was recently read */
	nodeRecentMask			= 0x00000004
};

/*****************************************************************************/

#define ATTRIBUTES_TIMEOUT_MIN		2		/* Minimum number of seconds attributes are valid */
#define ATTRIBUTES_TIMEOUT_MAX		60		/* Maximum number of seconds attributes are valid */
#define FILE_VALIDATION_TIMEOUT		60		/* Number of seconds file is valid from file_validated_time */
#define FILE_CACHE_TIMEOUT			3600	/* 1 hour */
#define FILE_RECENTLY_CREATED_TIMEOUT	1	/* Maximum number of seconds to skip GETs on opens after a create */

#define NODE_IS_DELETED(node)		(((node)->flags & nodeDeletedMask) != 0)

int node_appledoubleheader_valid(
	struct node_entry *node,
	uid_t uid);
int node_attributes_valid(
	struct node_entry *node,
	uid_t uid);
										  
#define NODE_FILE_IS_CACHED(node)	( ((node)->flags & nodeInFileListMask) != 0 )
#define NODE_FILE_IS_OPEN(node)		( (node)->file_inactive_time == 0 )
#define NODE_FILE_CACHE_INVALID(node) ( ((node)->file_inactive_time != 0) && \
									  (time(NULL) >= ((node)->file_inactive_time + FILE_CACHE_TIMEOUT)) )
#define NODE_FILE_INVALID(node)		( ((node)->file_validated_time == 0) || \
									  (time(NULL) >= ((node)->file_validated_time + FILE_VALIDATION_TIMEOUT)) )
#define NODE_FILE_RECENTLY_CREATED(node) ( ((node)->node_time != 0) && \
									  (((node)->flags & nodeRecentMask) != 0) && \
									  (time(NULL) <= ((node)->node_time + FILE_RECENTLY_CREATED_TIMEOUT)) )

/*****************************************************************************/

int nodecache_init(
	size_t name_length,				/* length of root node name */
	char *name,						/* the utf8 name of root node */
	struct node_entry **root_node);	/* the root node */

int nodecache_get_node(
	struct node_entry *parent,		/* the parent node_entry */
	size_t name_length,				/* length of name */
	const char *name,				/* the utf8 name of the node */
	int make_entry,					/* TRUE if a new node_entry should be created if needed*/
	int client_created,				/* TRUE if this client created the node (used in conjunction with make_entry */
	webdav_filetype_t node_type,	/* if make_entry is TRUE, the type of node to create */
	struct node_entry **node);		/* the found (or new) node_entry */

void nodecache_free_nodes(void);

int nodecache_move_node(
	struct node_entry *node,		/* the node_entry to move */
	struct node_entry *new_parent,  /* the new parent node_entry */
	size_t new_name_length,			/* length of new_name or 0 if not renaming */
	char *new_name);				/* the utf8 new name of the node or NULL */

int nodecache_delete_node(
	struct node_entry *node,		/* the node_entry to delete */
	int recursive);					/* delete recursively if node is a directory */

int nodecache_add_attributes(
	struct node_entry *node,		/* the node_entry to update or add attributes_entry to */
	uid_t uid,						/* the uid these attributes are valid for */
	struct stat *statp,				/* the stat buffer */
	char *appledoubleheader);		/* pointer appledoubleheader or NULL */
	
int nodecache_remove_attributes(
	struct node_entry *node);		/* the node_entry to remove attributes from */

void nodecache_invalidate_caches(void);

int nodecache_add_file_cache(
	struct node_entry *node,		/* the node_entry to add a file_cache_entry to */
	int fd);						/* the file descriptor of the cache file */
	
void nodecache_remove_file_cache(
	struct node_entry *node);		/* the node_entry to remove file_cache_entry from */

struct node_entry *nodecache_get_next_file_cache_node(
	int get_first);					/* if true, return first file cache node; otherwise, the next one */

int nodecache_get_path_from_node(
	struct node_entry *node,		/* -> node */
	char **path);					/* <- relative path to root node */

int nodecache_invalidate_directory_node_time(
	struct node_entry *dir_node);	/* parent directory node */

int nodecache_delete_invalid_directory_nodes(
	struct node_entry *dir_node);	/* parent directory node */

/*****************************************************************************/

#if 0
void nodecache_display_node_tree(void);
void nodecache_display_file_cache(void);
#endif

/*****************************************************************************/

#endif
