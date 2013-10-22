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

#include "webdavd.h"

#include <sys/syslog.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "webdav_cache.h"
#include "webdav_parse.h"
#include "OpaqueIDs.h"
#include "LogMessage.h"

/*****************************************************************************/

static int open_cache_files = 0;

pthread_mutex_t g_node_cache_lock;

/*****************************************************************************/

/*
 * The root node_entry.
 * The rest of the node_entry entries will be in the tree below the root node_entry.
 */
struct node_entry *g_root_node;

/*
 * The deleted root node_entry.
 * When an open file is deleted (including when something is renamed over an
 * existing node), it will be removed from the main tree and inserted as a child
 * of this "fake" node_entry. URLs cannot be created for children of this node
 * (but they should never need to talk to the server anyway -- they are deleted)
 * Nodes are freed (using nodecache_free_nodes()) only when they are children of this node. 
 */
struct node_entry *g_deleted_root_node;

/*
 * The next fileid number to be assigned.
*/
u_int32_t g_next_fileid;

/*
 * The file_cache_head list.
 * Entries are stored in the order inserted into the list.
 */
struct node_head g_file_list;

/* static prototypes */

static int internal_add_attributes(
	struct node_entry *node,
	uid_t uid,
	struct webdav_stat_attr *statp,
	char *appledoubleheader);
static int internal_remove_attributes(
	struct node_entry *node,
	int remove_appledoubleheader);
static int internal_node_appledoubleheader_valid(
	struct node_entry *node,
	uid_t uid);
static void invalidate_level(
	struct node_entry *dir_node);
static void internal_remove_file_cache(
	struct node_entry *node);
static int internal_add_file_cache(
	struct node_entry *node,
	int fd);
static int internal_get_node(
	struct node_entry *parent,
	size_t name_length,
	const char *name,
	int make_entry,
	int client_created,
	webdav_filetype_t node_type,
	struct node_entry **node);
static void internal_free_nodes(void);
static int internal_move_node(
	struct node_entry *node,
	struct node_entry *new_parent,
	size_t new_name_length,
	char *new_name);
static int delete_node_tree(
	struct node_entry *node,
	int recursive);
static int internal_invalidate_directory_node_time(
	struct node_entry *dir_node);
static int internal_delete_invalid_directory_nodes(
	struct node_entry *dir_node);
static int internal_get_path_from_node(
	struct node_entry *target_node,
	bool *pathHasRedirection,
	char **path);
static CFArrayRef internal_get_locktokens(
	struct node_entry *a_node);

static int init_node_cache_lock(void);

/*****************************************************************************/

/* this adds or replaces attribute information to node */
static int internal_add_attributes(
	struct node_entry *node,		/* the node_entry to update or add attributes_entry to */
	uid_t uid,						/* the uid these attributes are valid for */
	struct webdav_stat_attr *statp,	/* the stat buffer */
	char *appledoubleheader)	/* pointer appledoubleheader or NULL */
{
	int error;
	time_t current_time;
	
	error = 0;
	
	/* first, clear out any old attributes */
	(void)internal_remove_attributes(node, (appledoubleheader != NULL));
	
	current_time = time(NULL);
	require_action(current_time != -1, time, error = errno);
	/* add appledoubleheader first since it could fail */
	if ( appledoubleheader != NULL )
	{
		node->attr_appledoubleheader = malloc(APPLEDOUBLEHEADER_LENGTH);
		require_action(node->attr_appledoubleheader != NULL, malloc_attr_appledoubleheader, error = ENOMEM);
		
		memcpy(node->attr_appledoubleheader, appledoubleheader, APPLEDOUBLEHEADER_LENGTH);
		node->attr_appledoubleheader_time = current_time;
	}
	/* fill in the rest of the fields */
	node->attr_uid = uid;
	node->attr_time = current_time;
	memcpy(&(node->attr_stat_info), statp, sizeof(struct webdav_stat_attr));
	
malloc_attr_appledoubleheader:
time:
	
	return ( error );
}

/*****************************************************************************/

int nodecache_add_attributes(
	struct node_entry *node,		/* the node_entry to update or add attributes_entry to */
	uid_t uid,						/* the uid these attributes are valid for */
	struct webdav_stat_attr *statp,	/* the stat buffer */
	char *appledoubleheader)	/* pointer appledoubleheader or NULL */
{
	int error;

	lock_node_cache();

	error = internal_add_attributes(node, uid, statp, appledoubleheader);

	unlock_node_cache();
	
	return ( error );
}

/*****************************************************************************/

/* clear out the attribute fields and release any memory */
static int internal_remove_attributes(
	struct node_entry *node,
	int remove_appledoubleheader)
{
	node->attr_uid = 0;
	node->attr_time = 0;
	node->attr_stat_info.attr_create_time.tv_sec = 0;
	memset(&node->attr_stat_info, 0, sizeof(struct webdav_stat_attr));
	if ( remove_appledoubleheader && (node->attr_appledoubleheader != NULL) )
	{
		free(node->attr_appledoubleheader);
		node->attr_appledoubleheader = NULL;
		node->attr_appledoubleheader_time = 0;
	}
	return ( 0 );
}

/*****************************************************************************/

int nodecache_remove_attributes(
	struct node_entry *node)	/* the node to remove attributes from */
{
	int error;

	lock_node_cache();

	error = internal_remove_attributes(node, TRUE);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

static int internal_node_appledoubleheader_valid(
	struct node_entry *node,
	uid_t uid)
{
	int result;

	result = ( (node->attr_appledoubleheader != NULL) && /* is there attr_appledoubleheader? */
			 (node->attr_appledoubleheader_time != 0) && /* 0 attr_appledoubleheader_time is invalid */
			 ((uid == node->attr_uid) || (0 == node->attr_uid)) && /* does this user or root have access to the cached attributes? */
			 (time(NULL) < (node->attr_appledoubleheader_time + FILE_VALIDATION_TIMEOUT)) ); /* don't cache them too long */

	return ( result );
}

/*****************************************************************************/

int node_appledoubleheader_valid(
	struct node_entry *node,
	uid_t uid)
{
	int result;

	lock_node_cache();

	result = internal_node_appledoubleheader_valid(node, uid);

	unlock_node_cache();

	return ( result );
}

/*****************************************************************************/

int node_attributes_valid(
	struct node_entry *node,
	uid_t uid)
{
	int result;

	lock_node_cache();

	result = ( (node->attr_time != 0) && /* 0 attr_time is invalid */
			 ((uid == node->attr_uid) || (0 == node->attr_uid)) && /* does this user or root have access to the cached attributes */
			 (time(NULL) < (node->attr_time + ATTRIBUTES_TIMEOUT_MAX)) ); /* don't cache them too long */

	unlock_node_cache();

	return ( result );

#if 0 /* FUTURE */	
	int result;
	
	/* are the cached attributes possibly valid and does this user or root have access to them? */
	if ( (node->attr_time != 0) && ((uid == node->attr_uid) || (0 == node->attr_uid)) )
	{
		/*
		 * Determine attribute_time_out. It will be something between
		 * ATTRIBUTES_TIMEOUT_MIN and ATTRIBUTES_TIMEOUT_MAX where recently
		 * modified files have a short timeout and files that haven't been
		 * modified in a long time have a long timeout. This is the same
		 * algorithm used by NFS (with different min and max values).
		 */
		time_t current_time;
		time_t attribute_time_out;
		
		current_time = time(NULL);
		attribute_time_out = (current_time - node->attr_stat.st_mtimespec.tv_sec) / 10;
		if (attribute_time_out < ATTRIBUTES_TIMEOUT_MIN)
		{
			attribute_time_out = ATTRIBUTES_TIMEOUT_MIN;
		}
		else if (attribute_time_out > ATTRIBUTES_TIMEOUT_MAX)
		{
			attribute_time_out = ATTRIBUTES_TIMEOUT_MAX;
		}
		/* has too much time passed? */
		result = ((current_time - node->attr_time) <= attribute_time_out);
	}
	else
	{
		result = FALSE;
	}
	return ( result );
#endif /* FUTURE */
}

/*****************************************************************************/

/* invalidate the node attribute and file cache caches for dir_node's children */
static void invalidate_level(struct node_entry *dir_node)
{
	struct node_entry *node;
	
	/* invalidate each child node */
	LIST_FOREACH(node, &(dir_node->children), entries)
	{
		node->attr_time = 0;
		node->attr_stat_info.attr_create_time.tv_sec = -1;
		node->file_validated_time = 0;
		/* invalidate this node's children (if any) */
		invalidate_level(node);
	}
}

/* filesystem_invalidate_caches will call this to do all the work */
void nodecache_invalidate_caches(void)
{
	struct node_entry *node;
	
	lock_node_cache();
	
	node = g_root_node;
	node->attr_time = 0;
	node->attr_stat_info.attr_create_time.tv_sec = -1;
	node->file_validated_time = 0;
	/* invalidate this node's children (if any) */
	invalidate_level(node);
	
	unlock_node_cache();
}

/*****************************************************************************/

#if 0

static int gtabs;

static void display_node_tree_level(struct node_entry *dir_node)
{
	struct node_entry *node;
	
	LIST_FOREACH(node, &(dir_node->children), entries)
	{
		++gtabs;
		syslog(LOG_ERR, "%*s%s: %ld %s, node ino %ld, attr ino %u", gtabs*3, "", node->name, (unsigned long)node, node->node_type == WEBDAV_DIR_TYPE ? "d" : "f", node->fileid, node->attr_stat.st_ino);
		display_node_tree_level(node);
		--gtabs;
	}
}

void nodecache_display_node_tree(void)
{
	struct node_entry *node;
	
	lock_node_cache();
	
	node = g_root_node;
	gtabs = 0;
	syslog(LOG_ERR, "%*s%s: %ld %s, node ino %ld, attr ino %u", gtabs*3, "", node->name, (unsigned long)node, node->node_type == WEBDAV_DIR_TYPE ? "d" : "f", node->fileid, node->attr_stat.st_ino);
	display_node_tree_level(node);
	syslog(LOG_ERR, "-----");
	
	unlock_node_cache();
}

void nodecache_display_file_cache(void)
{
	struct node_entry *node;
	unsigned long count;
	
	count = 0;
	
	lock_node_cache();

	LIST_FOREACH(node, &g_file_list, file_list)
	{
		++count;
		
		if ( !NODE_IS_DELETED(node) )
		{
			syslog(LOG_ERR, "fd: %d, ino %ld %s%s %s",
				node->file_fd,
				node->fileid,
				node->node_type == WEBDAV_DIR_TYPE ? "d" : "f",
				NODE_FILE_IS_OPEN(node) ? "+ " : "- ",
				node->name);
		}
		else
		{
			syslog(LOG_ERR, "fd: %d, ino %ld %s%s %s",
				node->file_fd,
				node->fileid,
				node->node_type == WEBDAV_DIR_TYPE ? "d" : "f",
				NODE_FILE_IS_OPEN(node) ? "+X" : "-X",
				node->name);
		}
	}
	syslog(LOG_ERR, "----- %ld -----", count);

	unlock_node_cache();
}

#endif

/*****************************************************************************/

static struct node_entry *g_next_file_cache_node = NULL;

struct node_entry *nodecache_get_next_file_cache_node(
	int get_first)					/* if true, return first file cache node; otherwise, the next one */
{
	struct node_entry *node;
	
	lock_node_cache();
	
	if ( get_first )
	{
		node = g_file_list.lh_first;
	}
	else
	{
		node = g_next_file_cache_node;
	}
	
	if ( node != NULL )
	{
		g_next_file_cache_node = node->file_list.le_next;
	}
	else
	{
		g_next_file_cache_node = NULL;
	}
	
	unlock_node_cache();
	
	return ( node );
}

/*****************************************************************************/

static int internal_add_file_cache(
	struct node_entry *node,		/* the node_entry to add a file_cache_entry to */
	int fd)							/* the file descriptor of the cache file */
{
	int error;
	
	error = 0;
	
	/* only add it if it's not already cached */
	require_quiet(!NODE_FILE_IS_CACHED(node), already_cached);
	
	while ( open_cache_files >= WEBDAV_MAX_OPEN_FILES )
	{
		struct node_entry *file_node;
		struct node_entry *victim_node;
		
		/* find oldest victim node */
		victim_node = NULL;
		LIST_FOREACH(file_node, &g_file_list, file_list)
		{
			if ( !NODE_FILE_IS_OPEN(file_node) )
			{
				victim_node = file_node;
			}
		}
		
		require_action(victim_node != NULL, too_many_files_open, error = ENFILE);

		internal_remove_file_cache(victim_node);
	}
	
	++open_cache_files;
	node->flags |= nodeInFileListMask;
	node->file_fd = fd;
	node->file_status = WEBDAV_DOWNLOAD_NEVER;
	node->file_validated_time = 0;
	node->file_inactive_time = 0;
	node->file_last_modified = -1;
	if ( node->file_entity_tag != NULL )
	{
		free(node->file_entity_tag);
		node->file_entity_tag = NULL;
	}
	node->file_locktoken_uid = 0;
	if ( node->file_locktoken != NULL )
	{
		free(node->file_locktoken);
		node->file_locktoken = NULL;
	}
	
	/* add it to the head of the g_file_active_list */
	LIST_INSERT_HEAD(&g_file_list, node, file_list);
	
too_many_files_open:
already_cached:
	
	return ( error );
}

/*****************************************************************************/

int nodecache_add_file_cache(
	struct node_entry *node,		/* the node_entry to add a file_cache_entry to */
	int fd)							/* the file descriptor of the cache file */
{
	int error;

	lock_node_cache();

	error = internal_add_file_cache(node, fd);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

static void internal_remove_file_cache(struct node_entry *node)
{
	if ( NODE_FILE_IS_CACHED(node) )
	{
		if (open_cache_files > 0)
		{
			--open_cache_files;
		}
		else
		{
			debug_string("internal_remove_file_cache: open_cache_files was zero");
		}
		node->flags &= ~nodeInFileListMask;
		close(node->file_fd);
		node->file_fd = -1;
		if ( node == g_next_file_cache_node )
		{
			g_next_file_cache_node = node->file_list.le_next;
		}
		/* remove from g_file_list */
		LIST_REMOVE(node, file_list);
		node->file_list.le_next = NULL;
		node->file_list.le_prev = NULL;
		node->file_status = WEBDAV_DOWNLOAD_NEVER;
		node->file_validated_time = 0;
		node->file_inactive_time = 0;
		node->file_last_modified = -1;
		if ( node->file_entity_tag != NULL )
		{
			free(node->file_entity_tag);
			node->file_entity_tag = NULL;
		}
		node->file_locktoken_uid = 0;
		if ( node->file_locktoken != NULL )
		{
			free(node->file_locktoken);
			node->file_locktoken = NULL;
		}
	}
}

/*****************************************************************************/

void nodecache_remove_file_cache(struct node_entry *node)
{
	lock_node_cache();
	
	internal_remove_file_cache(node);
	
	unlock_node_cache();
}

/*****************************************************************************/

/* called at mount time to initialize */
int nodecache_init(
	size_t name_length,				/* length of root node name */
	char *name,						/* the utf8 name of root node */
	struct node_entry **root_node)	/* the root node */
{
	int error;

	error = 0;
	
	g_next_fileid = WEBDAV_ROOTFILEID;
		
	/* allocate space for g_root_node and its name */
	g_root_node = calloc(1, sizeof(struct node_entry));
	require_action(g_root_node != NULL, calloc_g_root_node, error = ENOMEM);
	
	*root_node = g_root_node;
	g_root_node->name = malloc(name_length + 1);
	require_action(g_root_node->name != NULL, malloc_name, error = ENOMEM);
	
	/* initialize the node_entry */
	g_root_node->parent = NULL;
	LIST_INIT(&g_root_node->children);
	g_root_node->name_length = name_length;
	memcpy(g_root_node->name, name, name_length);
	g_root_node->name[name_length] = '\0';
	g_root_node->name_ref = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, g_root_node->name, kCFStringEncodingUTF8, kCFAllocatorNull);
	g_root_node->fileid = g_next_fileid++;
	g_root_node->node_type = WEBDAV_DIR_TYPE;
	g_root_node->node_time = time(NULL);
	error = AssignOpaqueID(g_root_node, &g_root_node->nodeid);
	require_noerr(error, AssignOpaqueID);

	/* attribute fields are already zeroed */
	/* file cache fields are already zeroed - set file_fd to indicate there is no cache file */
	g_root_node->file_fd = -1;
	
	/* allocate space for g_deleted_root_node and its empty name */
	g_deleted_root_node = calloc(1, sizeof(struct node_entry));
	require_action(g_root_node != NULL, calloc_g_deleted_root_node, error = ENOMEM);
	
	g_deleted_root_node->name = malloc(1);
	require_action(g_root_node->name != NULL, malloc_g_deleted_root_node_name, error = ENOMEM);
	
	/* initialize the node_entry */
	g_deleted_root_node->parent = NULL;
	LIST_INIT(&g_deleted_root_node->children);
	g_deleted_root_node->name_length = 0;
	g_deleted_root_node->name[0] = '\0';
	g_deleted_root_node->name_ref = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, g_deleted_root_node->name, kCFStringEncodingUTF8, kCFAllocatorNull);
	/* attribute fields are already zeroed */
	/* file cache fields are already zeroed - set file_fd to indicate there is no cache file */
	g_deleted_root_node->file_fd = -1;
	
	/* initialize g_file_list header */
	LIST_INIT(&g_file_list);

	error = init_node_cache_lock();

malloc_g_deleted_root_node_name:
calloc_g_deleted_root_node:
malloc_name:
AssignOpaqueID:
calloc_g_root_node:
	
	return ( error );
}

/*****************************************************************************/

/*
 * Finds a node by name in the parent node's children. If the node is
 * not found and make_entry is TRUE, then internal_get_node creates a new node.
 */
static int internal_get_node(
	struct node_entry *parent,		/* the parent node_entry */
	size_t name_length,				/* length of name */
	const char *name,				/* the utf8 name of the node */
	int make_entry,					/* TRUE if a new node_entry should be created if needed */
	int client_created,				/* TRUE if this client created the node (used in conjunction with make_entry */
	webdav_filetype_t node_type,	/* if make_entry is TRUE, the type of node to create */
	struct node_entry **node)		/* the found (or new) node_entry */
{
	struct node_entry *node_ptr;
	int error;
	
	node_ptr = NULL;
	error = 0;
	
	if ( name_length != 0 && name != NULL )
	{
		CFStringRef name_string;
		
		name_string = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)name, name_length, kCFStringEncodingUTF8, false);
		require_action(name_string != NULL, out, error = EINVAL);

		/* search for an existing node_entry */
		LIST_FOREACH(node_ptr, &(parent->children), entries)
		{
			if ( CFStringCompare(name_string, node_ptr->name_ref, kCFCompareNonliteral) == kCFCompareEqualTo )
			{
				break;
			}
		}
		
		CFRelease(name_string);
	}
	else
	{
		node_ptr = parent;
	}
	
	/* if we didn't find it and we're supposed to create it... */
	if ( node_ptr == NULL )
	{
		if ( make_entry )
		{
			/* allocate new node_entry */
			node_ptr = calloc(1, sizeof(struct node_entry));
			require_action(node_ptr != NULL, calloc_node_ptr, error = ENOMEM; webdav_kill(-1));
			
			/* allocate space for its name */
			node_ptr->name = malloc(name_length + 1);
			require_action(node_ptr->name != NULL, malloc_name, node_ptr = NULL; error = ENOMEM; webdav_kill(-1));
			
			/* initialize the node_entry */
			node_ptr->parent = parent;
			LIST_INIT(&node_ptr->children);
			node_ptr->name_length = name_length;
			memcpy(node_ptr->name, name, name_length);
			node_ptr->name[name_length] = '\0';
			node_ptr->name_ref = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, node_ptr->name, kCFStringEncodingUTF8, kCFAllocatorNull);
			node_ptr->fileid = g_next_fileid++;
			node_ptr->node_type = node_type;
			node_ptr->node_time = time(NULL);
			node_ptr->attr_stat_info.attr_create_time.tv_sec = 0;
			node_ptr->isRedirected = false;
			if ( client_created )
			{
				node_ptr->flags |= nodeRecentMask;
			}
			error = AssignOpaqueID(node_ptr, &node_ptr->nodeid);
			require_noerr_action(error, AssignOpaqueID, node_ptr = NULL; webdav_kill(-1));
			
			/* attribute fields are already zeroed */
			/* file cache fields are already zeroed - set file_fd to indicate there is no cache file */
			node_ptr->file_fd = -1;
			
			/* insert the node_entry into the parent's children list */
			LIST_INSERT_HEAD(&parent->children, node_ptr, entries);
		}
		else
		{
			error = ENOENT;
		}
	}
	else
	{
		/* update node_time of existing node */
		if ( make_entry )
		{
			node_ptr->node_time = time(NULL);
		}
		/* set or clear the recent flag */
		if ( client_created )
		{
			node_ptr->flags |= nodeRecentMask;
		}
		else
		{
			node_ptr->flags &= ~nodeRecentMask;
		}
	}
	
AssignOpaqueID:
malloc_name:
calloc_node_ptr:
out:

	/* return node_entry */
	if ( error == 0 )
	{
		*node = node_ptr;
	}
	else
	{
		*node = NULL;
	}
	
	return ( error );
}

/*****************************************************************************/

int nodecache_get_node(
	struct node_entry *parent,		/* the parent node_entry */
	size_t name_length,				/* length of name */
	const char *name,				/* the utf8 name of the node */
	int make_entry,					/* TRUE if a new node_entry should be created if needed*/
	int client_created,				/* TRUE if this client created the node (used in conjunction with make_entry */
	webdav_filetype_t node_type,	/* if make_entry is TRUE, the type of node to create */
	struct node_entry **node)		/* the found (or new) node */
{
	int error;

	lock_node_cache();
	
	error = internal_get_node(parent, name_length, name, make_entry, client_created, node_type, node);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

/*
 * internal_free_nodes
 * This function frees uncached nodes on the deleted list.
 */
static void internal_free_nodes(void)
{
	struct node_entry *node;

	node = g_deleted_root_node->children.lh_first;
	while ( node != NULL )
	{
		struct node_entry *next_node;

		next_node = node->entries.le_next;
		/* free this node ONLY if it's  NOT on the file list */
		if ( !NODE_FILE_IS_CACHED(node) )
		{
			/* remove the node_entry from the list it is in */
			LIST_REMOVE(node, entries);
			
			/* invalidate the nodeid */
			(void) DeleteOpaqueID(node->nodeid);
			node->nodeid = kInvalidOpaqueID;

			if ( node->file_entity_tag != NULL )
			{
				free(node->file_entity_tag);
				node->file_entity_tag = NULL;
			}
			node->file_locktoken_uid = 0;
			if ( node->file_locktoken != NULL )
			{
				free(node->file_locktoken);
				node->file_locktoken = NULL;
			}

			/* free memory used by the node */
			free(node->name);
			CFRelease(node->name_ref);
			if (node->redir_name != NULL) 
				free (node->redir_name);

			(void) internal_remove_attributes(node, TRUE);

			free(node);
		}
		node = next_node;
	}
}

/*****************************************************************************/

void nodecache_free_nodes(void)
{
	lock_node_cache();

	internal_free_nodes();

	unlock_node_cache();
}

/*****************************************************************************/

static int internal_move_node(
	struct node_entry *node,		/* the node_entry to move */
	struct node_entry *new_parent,  /* the new parent node_entry */
	size_t new_name_length,			/* length of new_name or 0 if not renaming */
	char *new_name)					/* the utf8 new name of the node or NULL */
{
	int error;
	
	error = 0;
	
	/* new name? */
	if ( (new_name_length != 0) && (new_name != NULL) )
	{
		CFStringRef name_string;
		CFComparisonResult compare_result;
			
		name_string = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)new_name, new_name_length, kCFStringEncodingUTF8, false);
		compare_result = CFStringCompare(name_string, node->name_ref, kCFCompareNonliteral);
		CFRelease(name_string);
		
		/* did the name change? */
		if ( compare_result != kCFCompareEqualTo )
		{
			char *name;
			
			/* allocate space for new name */
			name = malloc(new_name_length + 1);
			require_action(name != NULL, malloc_name, error = errno; webdav_kill(-1));
			
			free(node->name);
			CFRelease(node->name_ref);
			node->name = name;
			memcpy(node->name, new_name, new_name_length);
			node->name[new_name_length] = '\0';
			node->name_ref = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, node->name, kCFStringEncodingUTF8, kCFAllocatorNull);
			node->name_length = new_name_length;
		}
	}

	/* change parents? */
	if ( node->parent != new_parent )
	{
		/* move the node_entry to the new parent */
		LIST_REMOVE(node, entries);
		LIST_INSERT_HEAD(&new_parent->children, node, entries);
		node->parent = new_parent;
	}

malloc_name:

	return ( error );
}

/*****************************************************************************/

int nodecache_move_node(
	struct node_entry *node,		/* the node_entry to move */
	struct node_entry *new_parent,  /* the new parent node_entry */
	size_t new_name_length,			/* length of new_name or 0 if not renaming */
	char *new_name)					/* the utf8 new name of the node or NULL */
{
	int error;

	lock_node_cache();

	error = internal_move_node(node, new_parent, new_name_length, new_name);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

/* delete dir_node's descendents and then delete dir_node */
static int delete_node_tree(
	struct node_entry *node,		/* the node_entry to delete */
	int recursive)					/* delete recursively if node is a directory */
{
	int error;
	
	error = 0;
		
	if ( recursive )
	{
		struct node_entry *child_node;
		
		child_node = (node->children).lh_first;
		while ( child_node != NULL )
		{
			struct node_entry *next_node;
			
			next_node = child_node->entries.le_next;
			error = delete_node_tree(child_node, TRUE);
			require_noerr(error, delete_child_node);
			
			child_node = next_node;
		}
	}
	else
	{
		require_action((node->children).lh_first == NULL, has_children, error = EBUSY);
	}
	
	/* move node over to the deleted list and mark it as deleted  */
	error = internal_move_node(node, g_deleted_root_node, 0, NULL);
	require_noerr(error, internal_move_node);
	
	node->flags |= nodeDeletedMask;
	node->attr_time = 0;
	node->attr_stat_info.attr_create_time.tv_sec = 0;
	
internal_move_node:
has_children:
delete_child_node:

	return ( error );
}

/*****************************************************************************/

int nodecache_delete_node(
	struct node_entry *node,		/* the node_entry to delete */
	int recursive)					/* delete recursively if node is a directory */
{
	int error;

	lock_node_cache();

	error = delete_node_tree(node, recursive);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

/* invalidate dir_node's children's node_times */
static int internal_invalidate_directory_node_time(struct node_entry *dir_node)
{
	int error;
	struct node_entry *node;

	error = 0;

	require_action(dir_node->node_type == WEBDAV_DIR_TYPE, not_directory, error = ENOTDIR);

	LIST_FOREACH(node, &(dir_node->children), entries)
	{
		node->node_time = 0;
	}

not_directory:

	return ( error );
}

/*****************************************************************************/

int nodecache_invalidate_directory_node_time(
	struct node_entry *dir_node)		/* parent directory node */
{
	int error;

	lock_node_cache();

	error = internal_invalidate_directory_node_time(dir_node);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

/* invalidate dir_node's children's node_times */
static int internal_delete_invalid_directory_nodes(struct node_entry *dir_node)
{
	int error;
	struct node_entry *node;
	
	error = 0;
	
	require_action(dir_node->node_type == WEBDAV_DIR_TYPE, not_directory, error = ENOTDIR);

	node = (&(dir_node->children))->lh_first;
	while ( node != NULL )
	{
		struct node_entry *next_node;
		
		next_node = node->entries.le_next;
		if ( node->node_time == 0 )
		{
			//syslog(LOG_ERR,"NODE NAME IS %s",node->name);
			error = delete_node_tree(node, TRUE);
			require_noerr(error, delete_node_tree);
		}
		node = next_node;
	}

delete_node_tree:
not_directory:

	return ( error );
}

/*****************************************************************************/

int nodecache_delete_invalid_directory_nodes(
	struct node_entry *dir_node)		/* parent directory node */
{
	int error;

	lock_node_cache();

	error = internal_delete_invalid_directory_nodes(dir_node);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

/*
 * nodecache_get_path_from_node
 *
 * Builds a UTF-8 slash '/' delimited NULL terminated path from
 * the root node (but not including the root node) to the target_node.
 *
 * Note: If a node in the path has been redirected by an HTTP 3xx redirect, then
 * the returned path will be from the redirected node 
 * to the target_node, and the 'pathHasRedirection' output arg is set to true.
 *
 * If the node is directory, the path MUST end with a slash.
 * The path MUST NOT start with a slash since this is always a relative path.
 * If result is 0 (no error), then a newly allocated buffer containing the
 * path is returned and the caller is responsible for freeing it.
 */
static int internal_get_path_from_node(
	struct node_entry *target_node,
	bool *pathHasRedirection,		/* true if path contains a URL from a redirected node (http 3xx redirect) */
	char **path)
{
	int error;
	struct node_entry *cur_node;
	char *cur_ptr;
	char *pathbuf;
	size_t path_len;
	
	error = 0;
	pathbuf = NULL;
	path_len = 0;
	*pathHasRedirection = false;
	
	require_action(!NODE_IS_DELETED(target_node), node_deleted, error = ENOENT);
	
	pathbuf = malloc(PATH_MAX);
	require_action(pathbuf != NULL, malloc_pathbuf, error = errno);	
		
	if ( target_node == g_root_node )
	{
		/* nothing to do */
		*pathbuf = '\0';
	}
	else
	{
		/* point at last character and put in the string terminator */
		cur_ptr = pathbuf + PATH_MAX - 1;
		*cur_ptr = '\0';
		
		cur_node = target_node;
		while ( cur_node != g_root_node )
		{
			require_action(cur_node != NULL, name_too_long, error = ENAMETOOLONG);
			
			/* was this node redirected? */
			if (cur_node->isRedirected == TRUE) {
				/* make sure there's enough room for the node redir_name and a slash */
				require_action((cur_ptr - cur_node->redir_name_length + 1) >= pathbuf,
							   name_too_long, error = ENAMETOOLONG);
				
				/* add a slash */
				--cur_ptr;
				*cur_ptr = '/';
				
				/* copy in the redir_name */
				cur_ptr -= cur_node->redir_name_length;
				memcpy(cur_ptr, cur_node->redir_name, cur_node->redir_name_length);
				
				*pathHasRedirection = true;
				break;
			}
			
			/* make sure there's enough room for the next node name and a slash */
			require_action((cur_ptr - cur_node->name_length + 1) >= pathbuf,
				name_too_long, error = ENAMETOOLONG);
			
			/* add a slash */
			--cur_ptr;
			*cur_ptr = '/';
			
			/* copy in the name */
			cur_ptr -= cur_node->name_length;
			memcpy(cur_ptr, cur_node->name, cur_node->name_length);
			cur_node = cur_node->parent;
		}
		
		/* move the path to the front of the buffer */
		path_len = strlen(cur_ptr);
		memmove(pathbuf, cur_ptr, path_len + 1);
		
		/* remove trailing slash if not directory */
		if ( target_node->node_type != WEBDAV_DIR_TYPE )
		{
			--(path_len);
			pathbuf[path_len] = '\0';
		}
	}

name_too_long:
malloc_pathbuf:
node_deleted:

	if ( error )
	{
		if ( pathbuf != NULL )
		{
			free(pathbuf);
			pathbuf = NULL;
		}
	}

	*path = pathbuf;

	return ( error );
}

/*****************************************************************************/

int nodecache_get_path_from_node(
	struct node_entry *node,		/* -> node */
	bool *pathHasRedirection,		/* true if path contains a URL from a redirected node (http 3xx redirect) */
	char **path)					/* <- relative path to root node */
{
	int error;

	lock_node_cache();

	error = internal_get_path_from_node(node, pathHasRedirection, path);

	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

int nodecache_redirect_node(
	CFURLRef url,						/* original url that caused the redirect */
	struct node_entry *redirected_node,	/* the node_entry that was redirected */
	CFHTTPMessageRef responseRef,		/* the 3xx redirect response message */
	CFIndex statusCode)					/* 3xx status code from response message */
{
	CFMutableStringRef newLocationRef;
	CFStringRef locationRef, mmeHostRef, tmpStringRef;
	CFURLRef newBaseURL;
	boolean_t result, isRootNode = false;
	size_t name_ptr_len;
	char *name_ptr;	
	int error = 0;
	
	newLocationRef = NULL;
	locationRef = NULL;
	mmeHostRef = NULL;
	
	if (responseRef == NULL) {
		syslog(LOG_ERR, "%s: NULL responseRef\n", __FUNCTION__);
		error = EIO;
		goto out;
	}
	
	newLocationRef = CFStringCreateMutable(kCFAllocatorDefault,0);
	if (newLocationRef == NULL){
		syslog(LOG_ERR, "%s: no mem for newLocationRef\n", __FUNCTION__);
		error = ENOMEM;
		goto out;
	}
	
	// First need to build a Location string
	if (statusCode == 330) {
		// This is a MobileMe Realm type of redirection.
		// Need to pull out X-Apple-MMe-Host
		mmeHostRef = CFHTTPMessageCopyHeaderFieldValue(responseRef, CFSTR("X-Apple-MMe-Host"));
		
		if (mmeHostRef == NULL) {
			syslog(LOG_ERR, "%s: 330 Redirection missing MMe-Host header\n", __FUNCTION__);
			error = EIO;
			goto out;
		}
		
		if (gSecureConnection == TRUE)
			CFStringAppend(newLocationRef, CFSTR("https://"));
		else 
			CFStringAppend(newLocationRef, CFSTR("http://"));
		
		// add host from X-Apple-MMe-Host header
		CFStringAppend(newLocationRef, mmeHostRef);
		
		// fetch the path from the original URL
		tmpStringRef = CFURLCopyPath(url);
		
		if (tmpStringRef != NULL) {
			CFStringAppend(newLocationRef, tmpStringRef);
			CFRelease(tmpStringRef);
		}
	}
	else {
		// Not an MMe 330, just grab Location field
		locationRef = CFHTTPMessageCopyHeaderFieldValue(responseRef, CFSTR("Location"));
		
		if (locationRef == NULL) {
			syslog(LOG_ERR, "%s: 3xx response missing Location field\n", __FUNCTION__);
			error = EIO;
			return (error);
		}

		CFStringAppend(newLocationRef, locationRef);
	}	
	
	if (redirected_node == NULL || redirected_node == g_root_node)
		isRootNode = true;
	else
		isRootNode = false;
	
	// Add or remove trailing slash as needed
	if (isRootNode == true) {
		// we want a trailing slash
		if (CFStringHasSuffix(newLocationRef, CFSTR("/")) == false)
			CFStringAppend(newLocationRef, CFSTR("/"));
	}
	else {
		// Not the root node, don't want a trailing slash
		if (CFStringHasSuffix(newLocationRef, CFSTR("/")) == true)
			CFStringDelete(newLocationRef, CFRangeMake(CFStringGetLength(newLocationRef) - 1, 1));
	}
			
	// Setup C string for new redir_name
	name_ptr = malloc(WEBDAV_MAX_URI_LEN + 1);
	if (name_ptr == NULL) {
		syslog(LOG_ERR, "%s: no mem for name_ptr\n", __FUNCTION__);
		error = ENOMEM;
		goto out;
	}
	
	result = CFStringGetCString(newLocationRef, name_ptr, WEBDAV_MAX_URI_LEN, kCFStringEncodingUTF8);
	if (result == false) {
		syslog(LOG_ERR, "%s: CFStringGetCString returned false for newLocationRef\n", __FUNCTION__);
		error = EIO;
		free(name_ptr);
		goto out;
	}
			
	name_ptr_len = strlen(name_ptr);
	if (name_ptr_len <= 0) {
		error = EIO;
		free(name_ptr);
		goto out;		
	}
				
	// Block any attempt to redirect from a secure host to a non-secure host
	if (gSecureConnection == TRUE) {
		if (CFStringHasPrefix(newLocationRef, CFSTR("https://")) == false) {
			syslog(LOG_ERR, "%s: redirect to non-secure host denied: %s\n", __FUNCTION__, name_ptr);
			error = EIO;
			free(name_ptr);
			goto out;
		}
	}
			
	lock_node_cache();
			
	if (isRootNode == false) {
		if (redirected_node->isRedirected == true) {
			// cleanup old redirection
			if (redirected_node->redir_name != NULL)
				free(redirected_node->redir_name);
		}
				
		// now change node state
		redirected_node->isRedirected = true;
		redirected_node->redir_name = name_ptr;
		redirected_node->redir_name_length = name_ptr_len;
		
		syslog(LOG_DEBUG, "Node %s redirected to: %s", redirected_node->name, name_ptr);
	}
	else {
		// The root node, g_root_node is being redirected
		// Create the new gBaseURL
		newBaseURL = CFURLCreateAbsoluteURLWithBytes(kCFAllocatorDefault, (UInt8 *)name_ptr, name_ptr_len, kCFStringEncodingUTF8, NULL, FALSE);
		if (newBaseURL == NULL) {
			syslog(LOG_ERR, "%s: Location field was not legal UTF8: %s\n", __FUNCTION__, name_ptr);
			free(name_ptr);
			error = ENOMEM;
			goto create_baseurl;
		}
				
		if (g_root_node->isRedirected == true) {
			// cleanup old redirection
			if (g_root_node->redir_name != NULL)
				free(g_root_node->redir_name);
		}		
				
		// Now change node state
		CFRelease(gBaseURL);
		gBaseURL = newBaseURL;
		
		if (gBasePath != NULL) {
			CFRelease(gBasePath);
			gBasePath = NULL;
		}
		
		gBasePath = CFURLCopyPath(gBaseURL);
		if (gBasePath != NULL) {
			CFStringGetCString(gBasePath, gBasePathStr, MAXPATHLEN, kCFStringEncodingUTF8);
		}
		
		g_root_node->isRedirected = true;
		g_root_node->redir_name = name_ptr;
		g_root_node->redir_name_length = name_ptr_len;
		
		syslog(LOG_DEBUG, "Base node redirected to: %s", name_ptr);
	}

create_baseurl:
	unlock_node_cache();
out:
	if (newLocationRef != NULL)
		CFRelease(newLocationRef);
	if (locationRef != NULL)
		CFRelease(locationRef);
	if (mmeHostRef != NULL)
		CFRelease(mmeHostRef);
			
	return (error);
}
			

/* Returns a CFArray containing (1) the a_node's file_locktoken (if exists) */
/* AND (2) all the a_node's children's file_locktokens (if any exists) */
static CFArrayRef internal_get_locktokens(struct node_entry *a_node)
{
	int error;
	CFMutableArrayRef arr;
	CFStringRef lockTokenStr;
	struct node_entry *node;
	
	arr = NULL;
	error = 0;
	arr = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	require(arr != NULL, create_array);
	
	// First do parent
	if (a_node->file_locktoken != NULL) {
		lockTokenStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("(<%s>)"), a_node->file_locktoken);
		if (lockTokenStr != NULL) {
			CFArrayAppendValue(arr, lockTokenStr);
			CFRelease(lockTokenStr);	// we don't need to hold a reference
		}
		else {
			// apparently no mem to do our job, so punt.
			goto done;
		}
	}
	
	// We're done if no child nodes
	if (a_node->node_type != WEBDAV_DIR_TYPE)
		goto done;
	
	// Now get child locktokens
	LIST_FOREACH(node, &(a_node->children), entries)
	{
		if (node->file_locktoken != NULL) {
			lockTokenStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("(<%s>)"), node->file_locktoken);
			if (lockTokenStr != NULL) {
				CFArrayAppendValue(arr, lockTokenStr);
				CFRelease(lockTokenStr);	// we don't need to hold a reference
			}
		}
	}
	
done:
	if (!CFArrayGetCount(arr)) {
		// no file_locktokens found, return NULL
		CFRelease(arr);
		arr = NULL;
	}
	
create_array:
	
	return ( arr );
}

/*****************************************************************************/

CFArrayRef nodecache_get_locktokens(struct node_entry *a_node)
{
	CFArrayRef arr;
	
	lock_node_cache();
	
	arr = internal_get_locktokens(a_node);
	
	unlock_node_cache();
	
	return ( arr );
}

/*****************************************************************************/
static int init_node_cache_lock(void)
{
	int error;
	pthread_mutexattr_t mutexattr;
	
	error = pthread_mutexattr_init(&mutexattr);
	require_noerr(error, pthread_mutexattr_init);
	
	error = pthread_mutex_init(&g_node_cache_lock, &mutexattr);
	require_noerr(error, pthread_mutex_init);

pthread_mutex_init:
pthread_mutexattr_init:

	return ( error );
}
			
/*****************************************************************************/
			
/*
 * CFURLRef nodecache_get_baseURL(void)
 *
 * Returns a retained reference to the current gBaseURL. Caller must CFRelease
 * the returned reference when done with it.
 *
 */
CFURLRef nodecache_get_baseURL(void)
{
	CFURLRef baseURL;
			
	lock_node_cache();
	CFRetain(gBaseURL);
	baseURL = gBaseURL;
	unlock_node_cache();
	return baseURL;
}

/*****************************************************************************/
 void lock_node_cache(void)
{
	int error;
	
	error = pthread_mutex_lock(&g_node_cache_lock);
	require_noerr_action(error, pthread_mutex_lock, webdav_kill(-1));

pthread_mutex_lock:
	
	return;
}

/*****************************************************************************/

void unlock_node_cache(void)
{
	int error;
	
	error = pthread_mutex_unlock(&g_node_cache_lock);
	require_noerr_action(error, pthread_mutex_unlock, webdav_kill(-1));

pthread_mutex_unlock:
	
	return;
}

/*****************************************************************************/
