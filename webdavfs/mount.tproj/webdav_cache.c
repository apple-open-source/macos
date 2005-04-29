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

static int init_node_cache_lock(void);
static void lock_node_cache(void);
static void unlock_node_cache(void);

/*****************************************************************************/

/* this adds or replaces attribute information to node */
int nodecache_add_attributes(
	struct node_entry *node,		/* the node_entry to update or add attributes_entry to */
	uid_t uid,						/* the uid these attributes are valid for */
	struct stat *statp,				/* the stat buffer */
	char *appledoubleheader)	/* pointer appledoubleheader or NULL */
{
	int error;
	time_t current_time;
	
	error = 0;
	
	/* first, clear out any old attributes */
	(void)nodecache_remove_attributes(node);
	
	current_time = time(NULL);
	require_action(current_time != -1, time, error = errno);
	
	/* add appledoubleheader first since it could fail */
	if ( appledoubleheader != NULL )
	{
		node->attr_appledoubleheader = malloc(APPLEDOUBLEHEADER_LENGTH);
		require_action(node->attr_appledoubleheader != NULL, malloc_attr_appledoubleheader, error = ENOMEM);
		
		memcpy(node->attr_appledoubleheader, appledoubleheader, APPLEDOUBLEHEADER_LENGTH);
	}
	else
	{
		node->attr_appledoubleheader = NULL;
	}
	/* fill in the rest of the fields */
	node->attr_uid = uid;
	node->attr_time = current_time;
	memcpy(&(node->attr_stat), statp, sizeof(struct stat));
	
malloc_attr_appledoubleheader:
time:
	
	return ( error );
}

/*****************************************************************************/

/* clear out the attribute fields and release any memory */
int nodecache_remove_attributes(struct node_entry *node)
{
	node->attr_uid = 0;
	node->attr_time = 0;
	memset(&node->attr_stat, 0, sizeof(struct stat));
	if ( node->attr_appledoubleheader != NULL )
	{
		free(node->attr_appledoubleheader);
		node->attr_appledoubleheader = NULL;
	}
	return ( 0 );
}

/*****************************************************************************/

int node_attributes_valid(struct node_entry *node, uid_t uid)
{
	return ( (node->attr_time != 0) && /* 0 attr_time is invalid */
			 ((uid == node->attr_uid) || (0 == node->attr_uid)) && /* does this user or root have access to the cached attributes */
			 (time(NULL) < (node->attr_time + ATTRIBUTES_TIMEOUT_MAX)) ); /* don't cache them too long */

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

/* invalidate the node attribute and file cache caches for parent_node's children */
static void invalidate_level(struct node_entry *parent_node)
{
	struct node_entry *node;
	
	/* invalidate each child node */
	LIST_FOREACH(node, &(parent_node->children), entries)
	{
		node->attr_time = 0;
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
	node->file_validated_time = 0;
	/* invalidate this node's children (if any) */
	invalidate_level(node);
	
	unlock_node_cache();
}

/*****************************************************************************/

#ifdef DEBUG

static int gtabs;

static void display_node_tree_level(struct node_entry *parent_node)
{
	struct node_entry *node;
	
	LIST_FOREACH(node, &(parent_node->children), entries)
	{
		++gtabs;
		syslog(LOG_ERR, "%*s%s: %ld %s, node ino %ld, attr ino %ld", gtabs*3, "", node->name, (unsigned long)node, node->node_type == WEBDAV_DIR_TYPE ? "d" : "f", node->fileid, node->attr_stat.st_ino);
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
	syslog(LOG_ERR, "%*s%s: %ld %s, node ino %ld, attr ino %ld", gtabs*3, "", node->name, (unsigned long)node, node->node_type == WEBDAV_DIR_TYPE ? "d" : "f", node->fileid, node->attr_stat.st_ino);
	display_node_tree_level(node);
	syslog(LOG_ERR, "-----");
	
	unlock_node_cache();
}

void nodecache_display_file_cache(void)
{
	struct node_entry *node;
	unsigned long count;
	
	count = 0;
	
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
}

#endif

/*****************************************************************************/

static struct node_entry *g_next_file_cache_node = NULL;

struct node_entry *nodecache_get_next_file_cache_node(int get_first)
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

static void remove_file_cache_internal(struct node_entry *node)
{
	if ( NODE_FILE_IS_CACHED(node) )
	{
		if (open_cache_files > 0)
		{
			--open_cache_files;
		}
		else
		{
			debug_string("remove_file_cache_internal: open_cache_files was zero");
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

int nodecache_add_file_cache(
	struct node_entry *node,		/* the node_entry to add a file_cache_entry to */
	int fd)							/* the file descriptor of the cache file */
{
	int error;
	
	error = 0;
	
	lock_node_cache();
	
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

		remove_file_cache_internal(victim_node);
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
	
	unlock_node_cache();
	
	return ( error );
}

/*****************************************************************************/

void nodecache_remove_file_cache(struct node_entry *node)
{
	lock_node_cache();
	
	remove_file_cache_internal(node);
	
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
	g_root_node->fileid = g_next_fileid++;
	g_root_node->node_type = WEBDAV_DIR_TYPE;
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
	/* attribute fields are already zeroed */
	/* file cache fields are already zeroed - set file_fd to indicate there is no cache file */
	g_deleted_root_node->file_fd = -1;
	
	/* initialize g_file_list header */
	LIST_INIT(&g_file_list);

	error = init_node_cache_lock();

malloc_g_deleted_root_node_name:
calloc_g_deleted_root_node:
malloc_name:
calloc_g_root_node:
	
	return ( error );
}

/*****************************************************************************/

/*
 * nodecache_get_node finds a node by name in the parent node's children. If the node is
 * not found and make_entry is TRUE, then nodecache_get_node creates a new node.
 */
int nodecache_get_node(
	struct node_entry *parent,		/* the parent node_entry */
	size_t name_length,				/* length of name */
	const char *name,				/* the utf8 name of the node */
	int make_entry,					/* TRUE if a new node_entry should be created if needed*/
	webdav_filetype_t node_type,	/* if make_entry is TRUE, the type of node to create */
	struct node_entry **node)		/* the found (or new) node_entry */
{
	struct node_entry *node_ptr;
	int error;
	
	lock_node_cache();
	
	node_ptr = NULL;
	error = 0;
		
	/* search for an existing node_entry */
	LIST_FOREACH(node_ptr, &(parent->children), entries)
	{
		if ( (node_ptr->name_length == name_length) && !memcmp(node_ptr->name, name, name_length) )
		{
			break;
		}
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
			require_action(node_ptr != NULL, malloc_name, node_ptr = NULL; error = ENOMEM; webdav_kill(-1));
			
			/* initialize the node_entry */
			node_ptr->parent = parent;
			LIST_INIT(&node_ptr->children);
			node_ptr->name_length = name_length;
			memcpy(node_ptr->name, name, name_length);
			node_ptr->name[name_length] = '\0';
			node_ptr->fileid = g_next_fileid++;
			node_ptr->node_type = node_type;
			
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
	
malloc_name:
calloc_node_ptr:

	/* return node_entry */
	*node = node_ptr;
	
	unlock_node_cache();

	return ( error );
}

/*****************************************************************************/

/*
 * nodecache_free_nodes
 * This function frees uncached nodes on the deleted list.
 */
void nodecache_free_nodes(void)
{
	struct node_entry *node;

	lock_node_cache();
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
			/* free memory used by the node */
			free(node->name);
			(void) nodecache_remove_attributes(node);
			free(node);
		}
		node = next_node;
	}
	unlock_node_cache();
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
	
	error = 0;
	
	/* new name? */
	if ( (new_name_length != 0) && (new_name != NULL) )
	{
		if ( (new_name_length != node->name_length) || (memcmp(new_name, node->name, new_name_length) != 0) )
		{
			char *name;
			
			/* allocate space for new name */
			name = malloc(new_name_length + 1);
			require_action(name != NULL, malloc_name, error = errno; webdav_kill(-1));
			
			free(node->name);
			node->name = name;
			memcpy(node->name, new_name, new_name_length);
			node->name[new_name_length] = '\0';
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

	unlock_node_cache();
	
	return ( error );
}

/*****************************************************************************/

int nodecache_delete_node(
		struct node_entry *node)
{
	int error;
	
	if ( node != NULL )
	{
		require_action((node->children).lh_first == NULL, has_children, error = EBUSY);

		/* mark node as deleted and move it over to the deleted list */
		node->flags |= nodeDeletedMask;
		error = nodecache_move_node(node, g_deleted_root_node, 0, NULL);
	}
	else
	{
		error = 0;
	}

has_children:

	return ( error );
}

/*****************************************************************************/

/*
 * nodecache_get_path_from_node
 *
 * Builds a UTF-8 slash '/' delimited NULL terminated path from
 * the root node (but not including the root node) to target_node.
 * If the node is directory, the path MUST end with a slash.
 * The path will MUST NOT start with a slash since this is always a relative path.
 * If result is 0 (no error), then a newly allocated buffer containing the
 * path is returned and the caller is responsible for freeing it.
 */
int nodecache_get_path_from_node(
	struct node_entry *target_node,
	char **path)
{
	int error;
	struct node_entry *cur_node;
	char *cur_ptr;
	char *pathbuf;
	size_t path_len;
	
	lock_node_cache();
	
	error = 0;
	pathbuf = NULL;
	path_len = 0;
	
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

	unlock_node_cache();
	
	return ( error );
}

/*****************************************************************************/
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

static void lock_node_cache(void)
{
	int error;
	
	error = pthread_mutex_lock(&g_node_cache_lock);
	require_noerr_action(error, pthread_mutex_lock, webdav_kill(-1));

pthread_mutex_lock:
	
	return;
}

/*****************************************************************************/

static void unlock_node_cache(void)
{
	int error;
	
	error = pthread_mutex_unlock(&g_node_cache_lock);
	require_noerr_action(error, pthread_mutex_unlock, webdav_kill(-1));

pthread_mutex_unlock:
	
	return;
}

/*****************************************************************************/
