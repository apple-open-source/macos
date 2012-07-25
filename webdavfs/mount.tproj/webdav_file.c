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

#include "webdavd.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include "webdav_cache.h"
#include "webdav_network.h"
#include "OpaqueIDs.h"
#include "LogMessage.h"

/*****************************************************************************/
// The maximum size of an upload or download to allow the
// system to cache.
extern uint64_t webdavCacheMaximumSize;

/*****************************************************************************/

#define WEBDAV_STATFS_TIMEOUT 60	/* Number of seconds statfs_cache_buffer is valid */
static time_t statfs_cache_time;
static struct statfs statfs_cache_buffer;

int g_vfc_typenum;

static pthread_mutex_t webdav_cachefile_lock;	/* this mutex protects webdav_cachefile */
static int webdav_cachefile;	/* file descriptor for an empty, unlinked cache file or -1 */

/*****************************************************************************/

static int get_cachefile(int *fd);
static void save_cachefile(int fd);
static int associate_cachefile(int ref, int fd);

/*****************************************************************************/

#define TMP_CACHE_DIR _PATH_TMP ".webdavcache"		/* Directory for local file cache */
#define CACHEFILE_TEMPLATE "webdav.XXXXXX"			/* template for cache files */

/* get_cachefile returns the fd for a cache file. If webdav_cachefile is not
 * storing a cache file fd, open/create a new temp file and return it.
 * Otherwise, return the stored cache file fd.
 */
static int get_cachefile(int *fd)
{
	int error, mutexerror;
	char pathbuf[MAXPATHLEN];
	int retrycount;
	
	error = 0;

	error = pthread_mutex_lock(&webdav_cachefile_lock);
	require_noerr_action(error, pthread_mutex_lock, webdav_kill(-1));
	
	/* did a previous call leave a cache file for us to use? */
	if ( webdav_cachefile < 0 )
	{
		/* no, so create a temp file */
		retrycount = 0;
		/* don't get stuck here forever */
		while ( retrycount < 5 )
		{
			++retrycount;
			error = 0;
			if ( *gWebdavCachePath == '\0' )
			{
				/* create a template with our pid */
				snprintf(gWebdavCachePath, MAXPATHLEN + 1, "%s.%lu.XXXXXX", TMP_CACHE_DIR, (unsigned long)getpid());
				
				/* create the cache directory */
				require_action(mkdtemp(gWebdavCachePath) != NULL, mkdtemp, error = errno);
			}
			
			/* create a template for the cache file */
			snprintf(pathbuf, MAXPATHLEN, "%s/%s", gWebdavCachePath, CACHEFILE_TEMPLATE);
			
			/* create and open the cache file */
			*fd = mkstemp(pathbuf);
			if ( *fd != -1 )
			{
				/* unlink it so the last close will delete it */
				verify_noerr(unlink(pathbuf));
				break;	/* break with success */
			}
			else
			{
				error = errno;
				if ( ENOENT == error )
				{
					/* the gWebdavCachePath directory is missing, clear the old one and try again */
					*gWebdavCachePath = '\0';
					continue;
				}
				else
				{
					debug_string("mkstemp failed");
					break;	/* break with error */
				}
			}
		}
	}
	else
	{
		/* yes, so grab it */
		*fd = webdav_cachefile;
		webdav_cachefile = -1;
	}

mkdtemp:
	
	mutexerror = pthread_mutex_unlock(&webdav_cachefile_lock);
	require_noerr_action(mutexerror, pthread_mutex_unlock, error = mutexerror; webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:

	return ( error );
}

/*****************************************************************************/

/* save_cachefile saves a cache file fd that wasn't needed. If there is already
 * stored a cache file fd, then the input fd is closed (closing will only
 * happen when there there is a race between multiple open requests so it
 * should be rare).
 */
static void save_cachefile(int fd)
{
	int mutexerror;
	
	mutexerror = pthread_mutex_lock(&webdav_cachefile_lock);
	require_noerr_action(mutexerror, pthread_mutex_lock, webdav_kill(-1));
	
	/* are we already storing a cache file that wasn't used? */
	if ( webdav_cachefile < 0 )
	{
		/* no, so store this one */
		webdav_cachefile = fd;
	}
	else
	{
		/* yes, so close this one */
		close(fd);
	}

	mutexerror = pthread_mutex_unlock(&webdav_cachefile_lock);
	require_noerr_action(mutexerror, pthread_mutex_unlock, webdav_kill(-1));

pthread_mutex_unlock:
pthread_mutex_lock:
	
	return;
}

/*****************************************************************************/

static int associate_cachefile(int ref, int fd)
{
	int error = 0;
	int mib[5];
	
	/* setup mib for the request */
	mib[0] = CTL_VFS;
	mib[1] = g_vfc_typenum;
	mib[2] = WEBDAV_ASSOCIATECACHEFILE_SYSCTL;
	mib[3] = ref;
	mib[4] = fd;
	
	require_noerr_action(sysctl(mib, 5, NULL, NULL, NULL, 0), sysctl, error = errno);

sysctl:

	return ( error );
}

/*****************************************************************************/

int filesystem_init(int typenum)
{
	pthread_mutexattr_t mutexattr;
	int error;
	
	g_vfc_typenum = typenum;
	
	/* Set up the statfs timeout & buffer */
	bzero(&statfs_cache_buffer, sizeof(statfs_cache_buffer));
	statfs_cache_time = 0;
	
	webdav_cachefile = -1;	/* closed */
				
	/* set up the lock on the queues */
	error = pthread_mutexattr_init(&mutexattr);
	require_noerr(error, pthread_mutexattr_init);
	
	error = pthread_mutex_init(&webdav_cachefile_lock, &mutexattr);
	require_noerr(error, pthread_mutex_init);

pthread_mutex_init:
pthread_mutexattr_init:

	return ( error );
}

/*****************************************************************************/

int filesystem_open(struct webdav_request_open *request_open,
		struct webdav_reply_open *reply_open)
{
	int error;
	struct node_entry *node;
	int theCacheFile;
	char *locktoken;
	
	reply_open->pid = 0;
	
	locktoken = NULL;
	
	error = RetrieveDataFromOpaqueID(request_open->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);
	
	require_action_quiet(!NODE_IS_DELETED(node), deleted_node, error = ESTALE);
	
	/* get a cache file */
	theCacheFile = -1;
	error = get_cachefile(&theCacheFile);
	require_noerr_quiet(error, get_cachefile);
	
	if (node->node_type == WEBDAV_FILE_TYPE)
	{
		int write_mode;
		
		if ( NODE_FILE_IS_CACHED(node) )
		{
			/* save the cache file we didn't need */
			save_cachefile(theCacheFile);
		
			/* mark the old cache file active again */
			node->file_inactive_time = 0;
		}
		else
		{
			error = nodecache_add_file_cache(node, theCacheFile);
			require_noerr_action_quiet(error, nodecache_add_file_cache, save_cachefile(theCacheFile));
			
			/* If we get an error beyond this point we need to call nodecache_remove_file_cache() */
		}
		
		write_mode = ((request_open->flags & O_ACCMODE) != O_RDONLY);
		
		if ( write_mode )
		{
			/* If we are opening this file for write access, lock it first,
			  before we copy it into the cache file from the server, 
			  or truncate the cache file. */
			error = network_lock(request_open->pcr.pcr_uid, FALSE, node);
			if ( error == ENOENT )
			{
				/* the server says it's gone so delete it and its descendants */
				(void) nodecache_delete_node(node, TRUE);
				error = ESTALE;
				goto bad_obj_id;
			}
		}
		
		if ( !error )
		{
			if ( write_mode && (request_open->flags & O_TRUNC) )
			{
				/*
				 * If opened for write and truncate, we can set the length to zero
				 * and not get it from the server.
				 */
				require_noerr_action(fchflags(node->file_fd, 0), fchflags, error = errno);
				require_noerr_action(ftruncate(node->file_fd, 0LL), ftruncate, error = errno);
				node->file_status = WEBDAV_DOWNLOAD_FINISHED;
			}
			else
			{
				error = network_open(request_open->pcr.pcr_uid, node, write_mode);
				if ( error == ENOENT )
				{
					/* the server says it's gone so delete it and its descendants */
					(void) nodecache_delete_node(node, TRUE);
					error = ESTALE;
					goto bad_obj_id;
				}
			}
		}
	}
	else
	{
		/* it's a directory */
		
		error = nodecache_add_file_cache(node, theCacheFile);
		require_noerr_action_quiet(error, nodecache_add_file_cache, save_cachefile(theCacheFile));
		/* If we get an error beyond this point we need to call nodecache_remove_file_cache() */
		
		/* Directory opens are always done in the foreground so set the
		 * download status to done
		 */
		node->file_status = WEBDAV_DOWNLOAD_FINISHED;
	}

	if ( !error )
	{
		/* all good so far -- associate the cachefile with the webdav file */
		error = associate_cachefile(request_open->ref, node->file_fd);
		if ( 0 == error )
		{
			reply_open->pid = getpid();
		}
		else
		{
			error = errno;
		}
	}

fchflags:
ftruncate:

	/* clean up if error */
	if ( error )
	{
		/* unlock it if it was locked */
		if ( node->file_locktoken != NULL )
		{
			 /* nothing we can do network_unlock fails -- the lock will time out eventually */
			(void) network_unlock(node);
		}

		/* remove it from the file cache */
		nodecache_remove_file_cache(node);
	}

nodecache_add_file_cache:
get_cachefile:
deleted_node:
bad_obj_id:
	
	return ( error );
}

/*****************************************************************************/

int filesystem_close(struct webdav_request_close *request_close)
{
	int error = 0;
	struct node_entry *node;
	
	error = RetrieveDataFromOpaqueID(request_close->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);
	
	/* Trying to close something we did not open? */
	require_action(NODE_FILE_IS_CACHED(node), not_open, error = EBADF);
	
	/* Kill any threads that may be downloading data for this file */
	if ( (node->file_status & WEBDAV_DOWNLOAD_STATUS_MASK) == WEBDAV_DOWNLOAD_IN_PROGRESS )
	{
		node->file_status |= WEBDAV_DOWNLOAD_TERMINATED;
	}

	while ( (node->file_status & WEBDAV_DOWNLOAD_STATUS_MASK) == WEBDAV_DOWNLOAD_IN_PROGRESS )
	{
		/* wait for the downloading thread to acknowledge that we stopped.*/
		usleep(10000);	/* 10 milliseconds */
	}

	/* set the file_inactive_time  */
	time(&node->file_inactive_time);

	/* if file was written sequentially, clean up the context that's hanging around */
	if ( node->put_ctx != NULL ) {
		error = cleanup_seq_write(node->put_ctx);
		free (node->put_ctx);
		node->put_ctx = NULL;
	}
	
	/* if the file was locked, unlock it */
	if ( node->file_locktoken  )
	{
		/* if it is deleted and locked (which should not happen), leave it locked and let the lock expire */
		if ( !NODE_IS_DELETED(node) )
		{
			error = network_unlock(node);
			if ( error == ENOENT )
			{
				/* the server says it's gone so delete it and its descendants */
				(void) nodecache_delete_node(node, TRUE);
				error = ESTALE;
				goto bad_obj_id;
			}
		}
	}

	/*
	 * If something went wrong with this file, it was deleted, or it is
	 * a directory, then remove it from the file cache.
	 */
	if ( error ||  NODE_IS_DELETED(node) || (node->node_type == WEBDAV_DIR_TYPE) )
	{
		(void)nodecache_remove_file_cache(node);
	}

not_open:
bad_obj_id:
	
	return (error);
}

/*****************************************************************************/

int filesystem_lookup(struct webdav_request_lookup *request_lookup, struct webdav_reply_lookup *reply_lookup)
{
	int error;
	struct node_entry *node;
	struct node_entry *parent_node;
	struct webdav_stat_attr statbuf;
	int lookup;
	CFStringRef name_string;
	
	node = NULL;

	// First make sure the name being looked up is valid UTF-8
	if ( request_lookup->name != NULL && request_lookup->name_length != 0)
	{
		name_string = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)request_lookup->name,
												request_lookup->name_length,
												kCFStringEncodingUTF8, false);
		require_action(name_string != NULL, out, error = EINVAL);
		CFRelease(name_string);
	}
	
	error = RetrieveDataFromOpaqueID(request_lookup->dir_id, (void **)&parent_node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);
	
	if ( request_lookup->force_lookup )
	{
		lookup = TRUE;
	}
	else
	{
		/* see if we already have a node */
		error = nodecache_get_node(parent_node, request_lookup->name_length, request_lookup->name, FALSE, FALSE, 0, &node);
		if ( error )
		{
			/* no node, ask the server */
			lookup = TRUE;
		}
		else
		{
			/* can we used the cached node? */
			lookup = !node_attributes_valid(node, request_lookup->pcr.pcr_uid);
		}
	}
	
	if ( lookup )
	{
		/* look it up on the server */
		error = network_lookup(request_lookup->pcr.pcr_uid, parent_node,
			request_lookup->name, request_lookup->name_length, &statbuf);
		if ( !error )
		{
			/* create a new node */
			error = nodecache_get_node(parent_node, request_lookup->name_length, request_lookup->name, TRUE, FALSE,
				S_ISREG(statbuf.attr_stat.st_mode) ? WEBDAV_FILE_TYPE : WEBDAV_DIR_TYPE, &node);
			if ( !error )
			{
				/* network_lookup gets all of the struct stat fields except for st_ino so fill it in here with the fileid of the new node */
				statbuf.attr_stat.st_ino = node->fileid;
				/* cache the attributes */
				error = nodecache_add_attributes(node, request_lookup->pcr.pcr_uid, &statbuf, NULL);
			}
		}
		else if ( (error == ENOENT) && (node != NULL) )
		{
			/* the server says it's gone so delete it and its descendants */
			(void) nodecache_delete_node(node, TRUE);
			node = NULL;
		}
	}
	else if ( node == NULL )
	{
		/* we recently created/read the parent directory so assume the object doesn't exist */
		error = ENOENT;
	}
	/* else use the cache node */

	if ( !error )
	{
		/* we have the attributes cached */
		reply_lookup->obj_id = node->nodeid;
		reply_lookup->obj_fileid = node->fileid;
		reply_lookup->obj_type = node->node_type;
		
		reply_lookup->obj_atime.tv_sec = node->attr_stat_info.attr_stat.st_atimespec.tv_sec;
		reply_lookup->obj_atime.tv_nsec = node->attr_stat_info.attr_stat.st_atimespec.tv_nsec;
		
		reply_lookup->obj_mtime.tv_sec = node->attr_stat_info.attr_stat.st_mtimespec.tv_sec;
		reply_lookup->obj_mtime.tv_nsec = node->attr_stat_info.attr_stat.st_mtimespec.tv_nsec;
		
		reply_lookup->obj_ctime.tv_sec = node->attr_stat_info.attr_stat.st_ctimespec.tv_sec;
		reply_lookup->obj_ctime.tv_nsec = node->attr_stat_info.attr_stat.st_ctimespec.tv_nsec;
		
		reply_lookup->obj_createtime.tv_sec = node->attr_stat_info.attr_create_time.tv_sec;
		reply_lookup->obj_createtime.tv_nsec = node->attr_stat_info.attr_create_time.tv_nsec;
		
		reply_lookup->obj_filesize = node->attr_stat_info.attr_stat.st_size;
	}

bad_obj_id:
out:	
	return (error);
}

/*****************************************************************************/

int filesystem_getattr(struct webdav_request_getattr *request_getattr, struct webdav_reply_getattr *reply_getattr)
{
	int error;
	struct node_entry *node;
	struct webdav_stat_attr statbuf;
	struct webdav_stat *wstat;
	
	error = RetrieveDataFromOpaqueID(request_getattr->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);
	
	require_action_quiet(!NODE_IS_DELETED(node), deleted_node, error = ESTALE);
	
	/* see if we have valid attributes */
	if ( !node_attributes_valid(node, request_getattr->pcr.pcr_uid) )
	{
		/* no... look it up on the server */
		error = network_getattr( request_getattr->pcr.pcr_uid, node, &statbuf);
		if ( !error )
		{
			/* cache the attributes */
			error = nodecache_add_attributes(node, request_getattr->pcr.pcr_uid, &statbuf, NULL);
		}
	}
	else
	{
		error = 0;
	}

	if ( !error )
	{
		/* we have the attributes cached */
		wstat = &reply_getattr->obj_attr;
		
		wstat->st_dev = node->attr_stat_info.attr_stat.st_dev;
		wstat->st_ino = (webdav_ino_t) node->attr_stat_info.attr_stat.st_ino;
		wstat->st_mode = node->attr_stat_info.attr_stat.st_mode;
		wstat->st_nlink = node->attr_stat_info.attr_stat.st_nlink;
		wstat->st_uid = node->attr_stat_info.attr_stat.st_uid;
		wstat->st_gid = node->attr_stat_info.attr_stat.st_gid;
		wstat->st_rdev = node->attr_stat_info.attr_stat.st_rdev;
		
		wstat->st_atimespec.tv_sec = node->attr_stat_info.attr_stat.st_atimespec.tv_sec;
		wstat->st_atimespec.tv_nsec = node->attr_stat_info.attr_stat.st_atimespec.tv_nsec;
		
		wstat->st_mtimespec.tv_sec = node->attr_stat_info.attr_stat.st_mtimespec.tv_sec;
		wstat->st_mtimespec.tv_nsec = node->attr_stat_info.attr_stat.st_mtimespec.tv_nsec;
		
		wstat->st_ctimespec.tv_sec = node->attr_stat_info.attr_stat.st_ctimespec.tv_sec;
		wstat->st_ctimespec.tv_nsec = node->attr_stat_info.attr_stat.st_ctimespec.tv_nsec;
		
		wstat->st_createtimespec.tv_sec = node->attr_stat_info.attr_create_time.tv_sec;
		wstat->st_createtimespec.tv_nsec = node->attr_stat_info.attr_create_time.tv_nsec;		
		
		wstat->st_size = node->attr_stat_info.attr_stat.st_size;
		wstat->st_blocks = node->attr_stat_info.attr_stat.st_blocks;
		wstat->st_blksize = node->attr_stat_info.attr_stat.st_blksize;
		wstat->st_flags = node->attr_stat_info.attr_stat.st_flags;
		wstat->st_gen = node->attr_stat_info.attr_stat.st_gen;
	}
	
deleted_node:
bad_obj_id:
	
	return (error);
}

/*****************************************************************************/

int filesystem_statfs(struct webdav_request_statfs *request_statfs,
		struct webdav_reply_statfs *reply_statfs)
{
	int error;
	time_t thetime;
	int call_server;
	struct node_entry *node;
	
	error = RetrieveDataFromOpaqueID(request_statfs->root_obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE); /* this should never fail since the root node never goes away */
	
	if ( gSuppressAllUI )
	{
		/* If we're suppressing UI, we don't need statfs to get quota info from the server. */
		error = 0;
	}
	else
	{
		thetime = time(0);
		if ( thetime != -1 )
		{
			/* do we need to call the server? */
			call_server = (statfs_cache_time == 0) || (thetime > (statfs_cache_time + WEBDAV_STATFS_TIMEOUT));
		}
		else
		{
			/* if we can't get the time we'll zero thetime and call the server */
			thetime = 0;
			call_server = TRUE;
		}
		
		if ( call_server )
		{
			/* update the cached statfs buffer */
			error = network_statfs(request_statfs->pcr.pcr_uid, node, &statfs_cache_buffer);
			if ( !error )
			{
				/* update the time the cached statfs buffer was updated */
				statfs_cache_time = thetime;
			}
		}
		else
		{
			error = 0;
		}
		
		if ( !error )
		{
			reply_statfs->fs_attr.f_bsize = statfs_cache_buffer.f_bsize;
			reply_statfs->fs_attr.f_iosize = statfs_cache_buffer.f_iosize;
			reply_statfs->fs_attr.f_blocks = statfs_cache_buffer.f_blocks;
			reply_statfs->fs_attr.f_bfree = statfs_cache_buffer.f_bfree;
			reply_statfs->fs_attr.f_bavail = statfs_cache_buffer.f_bavail;
			reply_statfs->fs_attr.f_files = statfs_cache_buffer.f_files;
			reply_statfs->fs_attr.f_ffree = statfs_cache_buffer.f_ffree;
		}
	}

bad_obj_id:
	
	return (error);
}

/*****************************************************************************/

/*
 * The only errors expected from filesystem_mount are:
 *		ECANCELED - the user could not authenticate and cancelled the mount
 *		ENODEV - we could not connect to the server (bad URL, server down, etc.)
 */
int filesystem_mount(int *a_mount_args)
{
	int error;

	error = network_mount(getuid(), a_mount_args);
	
	return (error);
}

/*****************************************************************************/

int filesystem_create(struct webdav_request_create *request_create, struct webdav_reply_create *reply_create)
{
	int error;
	struct node_entry *node;
	struct node_entry *parent_node;
	time_t creation_date;
	int theCacheFile;

	error = RetrieveDataFromOpaqueID(request_create->dir_id, (void **)&parent_node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);
	
	require_action_quiet(!NODE_IS_DELETED(parent_node), deleted_node, error = ESTALE);
	
	error = network_create(request_create->pcr.pcr_uid, parent_node, request_create->name, request_create->name_length, &creation_date);
	
	// Translate ENOENT to workaround VFS bug:
	// <rdar://problem/6965993> 10A383: WebDAV FS hangs on open with Microsoft servers (unsupported characters)
	if (error == ENOENT) {
		error = EIO;
	}
	
	if ( !error )
	{
		/*
		 * we just changed the parent_node so update or remove its attributes
		 */
		if ( (creation_date != -1) &&	/* if we know when the creation occurred */
			 (parent_node->attr_stat_info.attr_stat.st_mtimespec.tv_sec <= creation_date) &&	/* and that time is later than what's cached */
			 node_attributes_valid(parent_node, request_create->pcr.pcr_uid) )	/* and the cache is valid */
		{
			/* update the times of the cached attributes */
			parent_node->attr_stat_info.attr_create_time.tv_sec = creation_date;
			parent_node->attr_stat_info.attr_stat.st_mtimespec.tv_sec = creation_date;
			parent_node->attr_stat_info.attr_stat.st_atimespec = parent_node->attr_stat_info.attr_stat.st_ctimespec = parent_node->attr_stat_info.attr_stat.st_mtimespec;
			parent_node->attr_time = time(NULL);
		}
		else
		{
			/* remove the attributes */
			(void)nodecache_remove_attributes(parent_node);
		}
		
		/* Create a node */
		error = nodecache_get_node(parent_node, request_create->name_length, request_create->name, TRUE, TRUE, WEBDAV_FILE_TYPE, &node);
		if ( !error )
		{
			/* if we have the creation date, we can fill in the attributes because everything else is synthesized and the size is 0 */
			if ( creation_date != -1 )
			{
				struct webdav_stat_attr statbuf;
				
				bzero((void *)&statbuf, sizeof(struct webdav_stat_attr));
				
				statbuf.attr_stat.st_dev = 0;
				statbuf.attr_stat.st_ino = node->fileid;
				statbuf.attr_stat.st_mode = S_IFREG | S_IRWXU;
				/* Why 1 for st_nlink?
				 * Getting the real link count for directories is expensive.
				 * Setting it to 1 lets FTS(3) (and other utilities that assume
				 * 1 means a file system doesn't support link counts) work.
				 */
				statbuf.attr_stat.st_nlink = 1;
				statbuf.attr_stat.st_uid = UNKNOWNUID;
				statbuf.attr_stat.st_gid = UNKNOWNUID;
				statbuf.attr_stat.st_rdev = 0;
				statbuf.attr_create_time.tv_sec = creation_date;
				statbuf.attr_stat.st_mtimespec.tv_sec = creation_date;
				/* set all times to the last modified time since we cannot get the other times */
				statbuf.attr_stat.st_atimespec = statbuf.attr_stat.st_ctimespec = statbuf.attr_stat.st_mtimespec;
				statbuf.attr_stat.st_size = 0;	/* we just created it */
				statbuf.attr_stat.st_blocks = 0;	/* we just created it */
				statbuf.attr_stat.st_blksize = WEBDAV_IOSIZE;
				statbuf.attr_stat.st_flags = 0;
				statbuf.attr_stat.st_gen = 0;
				
				/* cache the attributes */
				error = nodecache_add_attributes(node, request_create->pcr.pcr_uid, &statbuf, NULL);
			}
			
			/*
			 * Since we just created the file and we can add an empty cache file
			 * to the node and set the status to download finished. This will
			 * save time in the next open.
			 */
			theCacheFile = -1;
			error = get_cachefile(&theCacheFile);
			if ( error == 0 )
			{
				/* add the empty file */
				error = nodecache_add_file_cache(node, theCacheFile);
				time(&node->file_validated_time);
				node->file_status = WEBDAV_DOWNLOAD_FINISHED;
				/* set the file_inactive_time  */
				time(&node->file_inactive_time);
			}
			
			reply_create->obj_id = node->nodeid;
			reply_create->obj_fileid = node->fileid;
		}
	}

deleted_node:
bad_obj_id:

	return (error);
}

/*****************************************************************************/

int filesystem_mkdir(struct webdav_request_mkdir *request_mkdir, struct webdav_reply_mkdir *reply_mkdir)
{
	int error;
	struct node_entry *node;
	struct node_entry *parent_node;
	time_t creation_date;

	error = RetrieveDataFromOpaqueID(request_mkdir->dir_id, (void **)&parent_node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(parent_node), deleted_node, error = ESTALE);

	error = network_mkdir(request_mkdir->pcr.pcr_uid, parent_node, request_mkdir->name, request_mkdir->name_length, &creation_date);
	if ( !error )
	{
		/*
		 * we just changed the parent_node so update or remove its attributes
		 */
		if ( (creation_date != -1) &&	/* if we know when the creation occurred */
			 (parent_node->attr_stat_info.attr_stat.st_mtimespec.tv_sec <= creation_date) &&	/* and that time is later than what's cached */
			 node_attributes_valid(parent_node, request_mkdir->pcr.pcr_uid) )	/* and the cache is valid */
		{
			/* update the times of the cached attributes */
			parent_node->attr_stat_info.attr_create_time.tv_sec = creation_date;
			parent_node->attr_stat_info.attr_stat.st_mtimespec.tv_sec = creation_date;
			parent_node->attr_stat_info.attr_stat.st_atimespec = parent_node->attr_stat_info.attr_stat.st_ctimespec = parent_node->attr_stat_info.attr_stat.st_mtimespec;
			parent_node->attr_time = time(NULL);
		}
		else
		{
			/* remove the attributes */
			(void)nodecache_remove_attributes(parent_node);
		}
		
		/* Create a node */
		error = nodecache_get_node(parent_node, request_mkdir->name_length, request_mkdir->name, TRUE, TRUE, WEBDAV_DIR_TYPE, &node);
		if ( !error )
		{
			/* if we have the creation date, we can fill in the attributes because everything else is synthesized */
			if ( creation_date != -1 )
			{
				struct webdav_stat_attr statbuf;
				
				bzero((void *)&statbuf, sizeof(struct webdav_stat_attr));
				
				statbuf.attr_stat.st_dev = 0;
				statbuf.attr_stat.st_ino = node->fileid;
				statbuf.attr_stat.st_mode = S_IFDIR | S_IRWXU;
				/* Why 1 for st_nlink?
				 * Getting the real link count for directories is expensive.
				 * Setting it to 1 lets FTS(3) (and other utilities that assume
				 * 1 means a file system doesn't support link counts) work.
				 */
				statbuf.attr_stat.st_nlink = 1;
				statbuf.attr_stat.st_uid = UNKNOWNUID;
				statbuf.attr_stat.st_gid = UNKNOWNUID;
				statbuf.attr_stat.st_rdev = 0;
				statbuf.attr_create_time.tv_sec = creation_date;
				statbuf.attr_stat.st_mtimespec.tv_sec = creation_date;
				/* set all other times to the last modified time since we cannot get the other times */
				statbuf.attr_stat.st_atimespec = statbuf.attr_stat.st_ctimespec = statbuf.attr_stat.st_mtimespec;
				statbuf.attr_stat.st_size = WEBDAV_DIR_SIZE;	/* fake up the directory size */
				statbuf.attr_stat.st_blocks = ((statbuf.attr_stat.st_size + S_BLKSIZE - 1) / S_BLKSIZE);
				statbuf.attr_stat.st_blksize = WEBDAV_IOSIZE;
				statbuf.attr_stat.st_flags = 0;
				statbuf.attr_stat.st_gen = 0;
				
				/* cache the attributes */
				error = nodecache_add_attributes(node, request_mkdir->pcr.pcr_uid, &statbuf, NULL);
			}
			
			reply_mkdir->obj_id = node->nodeid;
			reply_mkdir->obj_fileid = node->fileid;
		}
	}

deleted_node:
bad_obj_id:

	return (error);
}

/*****************************************************************************/

int filesystem_read(struct webdav_request_read *request_read, char **a_byte_addr, size_t *a_size)
{
	int error;
	struct node_entry *node;
	
	error = RetrieveDataFromOpaqueID(request_read->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(node), deleted_node, error = ESTALE);

	// Note: request_read->count has already been checked for overflow
	error = network_read(request_read->pcr.pcr_uid, node,
		request_read->offset, (size_t)request_read->count, a_byte_addr, a_size);

deleted_node:
bad_obj_id:

	return ( error );
}

/*****************************************************************************/

int filesystem_rename(struct webdav_request_rename *request_rename)
{
	int error = 0;
	struct node_entry *f_node;
	struct node_entry *t_node;
	struct node_entry *parent_node;
	time_t rename_date;

	error = RetrieveDataFromOpaqueID(request_rename->from_obj_id, (void **)&f_node);
	require_noerr_action_quiet(error, bad_from_obj_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(f_node), deleted_node, error = ESTALE);
	
	error = RetrieveDataFromOpaqueID(request_rename->to_dir_id, (void **)&parent_node);
	require_noerr_action_quiet(error, bad_to_dir_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(parent_node), deleted_node, error = ESTALE);

	if ( request_rename->to_obj_id != kInvalidOpaqueID )
	{
		/* "to" exists */
		error = RetrieveDataFromOpaqueID(request_rename->to_obj_id, (void **)&t_node);
		require_noerr_action_quiet(error, bad_to_obj_id, error = ESTALE);
		
		require_action_quiet(!NODE_IS_DELETED(t_node), deleted_node, error = ESTALE);
		
		/* "from" and "to" must be the same file_type */
		if ( f_node->node_type != t_node->node_type )
		{
			/* return the appropriate error */
			error = (f_node->node_type == WEBDAV_FILE_TYPE) ? EISDIR : ENOTDIR;
		}
		else
		{
			error = 0;
		}
	}
	else
	{
		t_node = NULL;
		error = 0;
	}
	
	if ( !error )
	{
		error = network_rename(request_rename->pcr.pcr_uid, f_node, t_node,
			parent_node, request_rename->to_name, request_rename->to_name_length, &rename_date);
		if ( !error )
		{
			/*
			 * we just changed the parent node(s) so update or remove their attributes
			 */
			if ( (rename_date != -1) &&	/* if we know when the creation occurred */
				 (f_node->parent->attr_stat_info.attr_stat.st_mtimespec.tv_sec <= rename_date) &&		/* and that time is later than what's cached */
				 node_attributes_valid(f_node->parent, request_rename->pcr.pcr_uid) )	/* and the cache is valid */
			{
				/* update the times of the cached attributes */
				f_node->parent->attr_stat_info.attr_stat.st_mtimespec.tv_sec = rename_date;
				f_node->parent->attr_stat_info.attr_stat.st_atimespec = f_node->parent->attr_stat_info.attr_stat.st_ctimespec = f_node->parent->attr_stat_info.attr_stat.st_mtimespec;
				f_node->parent->attr_time = time(NULL);
			}
			else
			{
				/* remove the attributes */
				(void)nodecache_remove_attributes(f_node->parent);
			}
			if ( f_node->parent != parent_node )
			{
				if ( (rename_date != -1) &&	/* if we know when the creation occurred */
					 (parent_node->attr_stat_info.attr_stat.st_mtimespec.tv_sec <= rename_date) &&	/* and that time is later than what's cached */
					 node_attributes_valid(parent_node, request_rename->pcr.pcr_uid) )	/* and the cache is valid */
				{
					/* update the times of the cached attributes */
					parent_node->attr_stat_info.attr_stat.st_mtimespec.tv_sec = rename_date;
					parent_node->attr_stat_info.attr_stat.st_atimespec = parent_node->attr_stat_info.attr_stat.st_ctimespec = parent_node->attr_stat_info.attr_stat.st_mtimespec;
					parent_node->attr_time = time(NULL);
				}
				else
				{
					/* remove the attributes */
					(void)nodecache_remove_attributes(parent_node);
				}
			}
						
			if ( t_node != NULL )
			{
				if ( nodecache_delete_node(t_node, FALSE) != 0 )
				{
					debug_string("nodecache_delete_node failed");
				}
			}
			
			/* move "from" node into the destination directory (with a possible rename) */
			if ( nodecache_move_node(f_node, parent_node, request_rename->to_name_length, request_rename->to_name) != 0 )
			{
				debug_string("nodecache_move_node failed");
			}

			statfs_cache_time = 0;
		}
	}

deleted_node:
bad_to_obj_id:
bad_to_dir_id:
bad_from_obj_id:
	
	return (error);
}

/*****************************************************************************/

int filesystem_remove(struct webdav_request_remove *request_remove)
{
	int error;
	struct node_entry *node;
	time_t remove_date;
	
	error = RetrieveDataFromOpaqueID(request_remove->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(node), deleted_node, error = ESTALE);
	
	error = network_remove(request_remove->pcr.pcr_uid, node, &remove_date);
	
	/*
	 *  When connected to an Mac OS X Server, I can delete the main file (ie blah.dmg), but when I try
	 *  to delete the ._ file (ie ._blah.dmg), I get a file not found error since the vfs layer on the
	 *  server already deleted the ._ file.  Since I am deleting the ._ anyways, the ENOENT is ok and I
	 *  should still clean up. 
	 */
	if ( (!error) || (error == ENOENT) )
	{
		/*
		 * we just changed the parent_node so update or remove its attributes
		 */
		if ( (remove_date != -1) &&	/* if we know when the creation occurred */
			 (node->parent->attr_stat_info.attr_stat.st_mtimespec.tv_sec <= remove_date) &&	/* and that time is later than what's cached */
			 node_attributes_valid(node->parent, request_remove->pcr.pcr_uid) )	/* and the cache is valid */
		{
			/* update the times of the cached attributes */
			node->parent->attr_stat_info.attr_stat.st_mtimespec.tv_sec = remove_date;
			node->parent->attr_stat_info.attr_stat.st_atimespec = node->parent->attr_stat_info.attr_stat.st_ctimespec = node->parent->attr_stat_info.attr_stat.st_mtimespec;
			node->parent->attr_time = time(NULL);
		}
		else
		{
			/* remove the attributes */
			(void)nodecache_remove_attributes(node->parent);
		}
				
		if ( nodecache_delete_node(node, FALSE) != 0 )
		{
			debug_string("nodecache_delete_node failed");
		}
		
		statfs_cache_time = 0;
	}

deleted_node:
bad_obj_id:
	
	return (error);
}

/*****************************************************************************/

int filesystem_rmdir(struct webdav_request_rmdir *request_rmdir)
{
	int error;
	struct node_entry *node;
	time_t remove_date;

	error = RetrieveDataFromOpaqueID(request_rmdir->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(node), deleted_node, error = ESTALE);
		
	/*
	 * network_rmdir ensures the directory on the server is empty (which is what really matters)
	 * before deleting the directory on the server. So, if network_rmdir is successful, then
	 * we need to get rid of the directory node and any of its children nodes.
	 */
	error = network_rmdir(request_rmdir->pcr.pcr_uid, node, &remove_date);
	if ( !error )
	{
		/*
		 * we just changed the parent_node so update or remove its attributes
		 */
		if ( (remove_date != -1) &&	/* if we know when the creation occurred */
			 (node->parent->attr_stat_info.attr_stat.st_mtimespec.tv_sec <= remove_date) &&	/* and that time is later than what's cached */
			 node_attributes_valid(node->parent, request_rmdir->pcr.pcr_uid) )	/* and the cache is valid */
		{
			/* update the times of the cached attributes */
			node->parent->attr_stat_info.attr_stat.st_mtimespec.tv_sec = remove_date;
			node->parent->attr_stat_info.attr_stat.st_atimespec = node->parent->attr_stat_info.attr_stat.st_ctimespec = node->parent->attr_stat_info.attr_stat.st_mtimespec;
			node->parent->attr_time = time(NULL);
		}
		else
		{
			/* remove the attributes */
			(void)nodecache_remove_attributes(node->parent);
		}
		
		/* delete node and any children we *thought* we knew about (some other client must have deleted them on the server) */
		if ( nodecache_delete_node(node, TRUE) != 0 )
		{
			debug_string("nodecache_delete_node failed");
		}
		
		statfs_cache_time = 0;
	}
	
deleted_node:
bad_obj_id:

	return (error);
}

/*****************************************************************************/

int filesystem_fsync(struct webdav_request_fsync *request_fsync)
{
	int error;
	struct node_entry *node;
	off_t file_length;
	time_t file_last_modified;
	
	error = RetrieveDataFromOpaqueID(request_fsync->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(node), deleted_node, error = ESTALE);
		
	/* Trying to fsync something that's not open? */
	require_action(NODE_FILE_IS_CACHED(node), not_open, error = EBADF);
	
	/* The kernel should not send us an fsync until the file is downloaded */
	require_action((node->file_status & WEBDAV_DOWNLOAD_STATUS_MASK) == WEBDAV_DOWNLOAD_FINISHED, still_downloading, error = EIO);

	error = network_fsync(request_fsync->pcr.pcr_uid, node, &file_length, &file_last_modified);
	
	if ( (file_length == -1) || (file_last_modified == -1) )
	{
		/* if we didn't get the length or the file_last_modified date, remove its attributes */
		(void)nodecache_remove_attributes(node);
	}
	else
	{
		/* otherwise, update its attributes */
		struct webdav_stat_attr statbuf;

		bzero((void *)&statbuf, sizeof(struct webdav_stat_attr));

		statbuf.attr_stat.st_dev = 0;
		statbuf.attr_stat.st_ino = node->fileid;
		statbuf.attr_stat.st_mode = S_IFREG | S_IRWXU;
		/* Why 1 for st_nlink?
		* Getting the real link count for directories is expensive.
		* Setting it to 1 lets FTS(3) (and other utilities that assume
		* 1 means a file system doesn't support link counts) work.
		*/
		statbuf.attr_stat.st_nlink = 1;
		statbuf.attr_stat.st_uid = UNKNOWNUID;
		statbuf.attr_stat.st_gid = UNKNOWNUID;
		statbuf.attr_stat.st_rdev = 0;
		/* set all times (except create time) to the last modified time since we cannot get the other times. */
		statbuf.attr_stat.st_mtimespec.tv_sec = file_last_modified;
		statbuf.attr_stat.st_atimespec = statbuf.attr_stat.st_ctimespec = statbuf.attr_stat.st_mtimespec;
		statbuf.attr_stat.st_size = file_length;
		statbuf.attr_stat.st_blocks = ((statbuf.attr_stat.st_size + S_BLKSIZE - 1) / S_BLKSIZE);
		statbuf.attr_stat.st_blksize = WEBDAV_IOSIZE;
		statbuf.attr_stat.st_flags = 0;
		statbuf.attr_stat.st_gen = 0;

		/* cache the attributes */
		error = nodecache_add_attributes(node, request_fsync->pcr.pcr_uid, &statbuf, NULL);
	}
	
	/* and we changed the volume so invalidate the statfs cache */
	statfs_cache_time = 0;

still_downloading:
not_open:
deleted_node:
bad_obj_id:
	
	return ( error );
}

/*****************************************************************************/

// This function sends a write request to the write manager.  
// When the request offset is zero, this function also intializes the sequential write engine.
//
int filesystem_write_seq(struct webdav_request_writeseq *request_sq_wr)
{
	int error;
	ssize_t bytesRead; 
	// size_t totalBytesRead;
	off_t totalBytesRead;
	off_t nbytes;
	struct node_entry *node;
	struct stream_put_ctx *ctx = NULL;
	struct seqwrite_mgr_req *mgr_req;
	struct timespec timeout;
	pthread_mutexattr_t mutexattr;
	
	mgr_req = NULL;
	error = 0;
	
	error = RetrieveDataFromOpaqueID(request_sq_wr->obj_id, (void **)&node);
	require_noerr_action_quiet(error, out1, error = ESTALE);

	/* File size is a hint, can't be trusted */
	/* number of bytes to be written can be 0, so no need to check */
	require_action_quiet(!NODE_IS_DELETED(node), out1, error = ESTALE);
		
	/* Trying to write something that's not open? */
	require_action(NODE_FILE_IS_CACHED(node), out1, error = EBADF);
	
	if ( request_sq_wr->offset == 0 ) {
		error = setup_seq_write(request_sq_wr->pcr.pcr_uid, node, request_sq_wr->file_len);
		
		// If file is large, turn off data caching during the upload
		if( request_sq_wr->file_len > webdavCacheMaximumSize)
			fcntl(node->file_fd, F_NOCACHE, 1);
	}
	
	if (error) {
		syslog(LOG_ERR, "%s: setup_seq_write returned %d\n", __FUNCTION__, error);
		goto out1;	
	}
	
	ctx = node->put_ctx;
	if (request_sq_wr->is_retry)
		ctx->is_retry = 1;
	
	// syslog(LOG_DEBUG, "%s: entered. offset %llu, count %lu\n", __FUNCTION__, request_sq_wr->offset, request_sq_wr->count);
	
	pthread_mutex_lock(&ctx->ctx_lock);
	/* Set request data which is common across all requests for this call */
	
	mgr_req = (struct seqwrite_mgr_req *) malloc (sizeof(struct seqwrite_mgr_req));	
	if (mgr_req == NULL) {
		syslog(LOG_ERR, "%s: malloc of seqwrite_mgr_req failed", __FUNCTION__);
		ctx->finalStatus = EIO;
		ctx->finalStatusValid = true;
		error = EIO;
		pthread_mutex_unlock(&ctx->ctx_lock);
		goto out1;
	}
	
	bzero(mgr_req, sizeof(struct seqwrite_mgr_req));
	mgr_req->refCount = 1;	// hold a reference until were done
	mgr_req->req = request_sq_wr;
	mgr_req->is_retry = request_sq_wr->is_retry;	
	mgr_req->data = (unsigned char *)malloc(BODY_BUFFER_SIZE); /* 64K */
	if ( mgr_req->data == NULL ) {
		syslog(LOG_ERR, "%s: malloc of data buffer failed", __FUNCTION__);
		ctx->finalStatus = EIO;
		ctx->finalStatusValid = true;
		error = EIO;
		pthread_mutex_unlock(&ctx->ctx_lock);
		goto out1;
	}
	
	error = pthread_mutexattr_init(&mutexattr);
	if (error) {
		syslog(LOG_ERR, "%s: init ctx mutexattr failed, error %d", __FUNCTION__, error);
		ctx->finalStatus = EIO;
		ctx->finalStatusValid = true;
		error = EIO;
		pthread_mutex_unlock(&ctx->ctx_lock);
		goto out1;
	}
	
	error = pthread_mutex_init(&mgr_req->req_lock, &mutexattr);
	if (error) {
		syslog(LOG_ERR, "%s: init ctx_lock failed, error %d", __FUNCTION__, error);
		ctx->finalStatus = EIO;
		ctx->finalStatusValid = true;
		error = EIO;
		pthread_mutex_unlock(&ctx->ctx_lock);
		goto out1;
	}
	
	error = pthread_cond_init(&mgr_req->req_condvar, NULL);
	if (error) {
		syslog(LOG_ERR, "%s: init ctx_condvar failed, error %d", __FUNCTION__, error);
		ctx->finalStatus = EIO;
		ctx->finalStatusValid = true;
		error = EIO;
		pthread_mutex_unlock(&ctx->ctx_lock);
		goto out1;
	}
	
	if (node->file_fd == -1) 
	{
		/* cache file's no good, today just isn't our day */
		syslog(LOG_ERR, "%s: cache file descriptor is -1, failed.", __FUNCTION__ );
		ctx->finalStatus = EIO;
		ctx->finalStatusValid = true;
		error = EIO;
		pthread_mutex_unlock(&ctx->ctx_lock);
		goto out1;
	}
	
	// Seek to the start of this offset
	if ( lseek(node->file_fd, request_sq_wr->offset, SEEK_SET) < 0) {
			/* seek failed, dammit */
			syslog(LOG_ERR, "%s: lseek errno %d, failed.", __FUNCTION__, errno);
			ctx->finalStatus = EIO;
			ctx->finalStatusValid = true;
			error = EIO;
			pthread_mutex_unlock(&ctx->ctx_lock);
			goto out1;
		}

	pthread_mutex_unlock(&ctx->ctx_lock);
	
	totalBytesRead = 0, bytesRead = 0;
	/* Loop until everything's written or an error occurs */
	while( 1 ) {
		pthread_mutex_lock(&ctx->ctx_lock);
		
		// if the sequential write was cancelled, get outta dodge
		if ( ctx->mgr_status == WR_MGR_DONE || ctx->finalStatusValid == true ) {
			// sequential write was cancelled
			// syslog(LOG_DEBUG, "%s: WRITESEQ: cancelled at top of loop mgr_status %d, finalStatusValid %d",
			//	__FUNCTION__, ctx->mgr_status == WR_MGR_DONE, ctx->finalStatusValid);
			error = ctx->finalStatus;
			pthread_mutex_unlock(&ctx->ctx_lock);
			goto out1;
		}
		
		// If we're done reading, break
		if ( totalBytesRead >= request_sq_wr->count ) {
			pthread_mutex_unlock(&ctx->ctx_lock);
			break;
		}
		
		nbytes = MIN( request_sq_wr->count - totalBytesRead, BODY_BUFFER_SIZE );

		bytesRead = read( node->file_fd, mgr_req->data, (size_t)nbytes );
		
		/* bytesRead < 0 we got an error */
		if ( bytesRead < 0 ) {
			syslog(LOG_ERR, "%s: read() cache file returned error %d, failed.", __FUNCTION__, errno);
			error = errno;
			ctx->finalStatus = error;
			ctx->finalStatusValid = true;
			pthread_mutex_unlock(&ctx->ctx_lock);
			goto out1;
		}
		
		mgr_req->type = SEQWRITE_CHUNK;
		mgr_req->request_done = false;
		mgr_req->chunkLen = bytesRead;
		mgr_req->chunkWritten = false;
		mgr_req->error = 0;
		
		// queue request
		if (queue_writemgr_request_locked(ctx, mgr_req) < 0) {
			syslog(LOG_ERR, "%s: queue_writemgr_request_locked failed.", __FUNCTION__);
			error = EIO;
			ctx->finalStatus = error;
			ctx->finalStatusValid = true;
			pthread_mutex_unlock(&ctx->ctx_lock);
			goto out1;
		}
		
		pthread_mutex_unlock(&ctx->ctx_lock);
		
		timeout.tv_sec = time(NULL) + WEBDAV_WRITESEQ_REQUEST_TIMEOUT;
		timeout.tv_nsec = 0;
		
		// now wait on condition var until mgr is done with this request
		pthread_mutex_lock(&mgr_req->req_lock);
		/* wait for request to finish */
		while (mgr_req->request_done == false && ctx->finalStatusValid == false) {
			error = pthread_cond_timedwait(&mgr_req->req_condvar, &mgr_req->req_lock, &timeout);	
			if ( error != 0 ) {
				syslog(LOG_ERR, "%s: pthread_cond_timedwait returned error %d, failed.", __FUNCTION__, error);
				pthread_mutex_lock(&ctx->ctx_lock);
				if ( error == ETIMEDOUT ) {
					ctx->finalStatus = ETIMEDOUT;
					ctx->finalStatusValid = true;
				} else {
					ctx->finalStatus = EIO;
					ctx->finalStatusValid = true;
					error = EIO;
				}
				pthread_mutex_unlock(&ctx->ctx_lock);
				pthread_mutex_unlock(&mgr_req->req_lock);
				goto out1;
			}
		}
		pthread_mutex_unlock(&mgr_req->req_lock);
		totalBytesRead += bytesRead;
	}	
	
	if (mgr_req->request_done == true) {
		error = mgr_req->error;
	} else {
		error = ctx->finalStatus;
	}
		
	// syslog(LOG_ERR, "%s: WRITE_SEQ: Write at offset %llu done, mgr_req.request_done: %d, error %d",
	//	__FUNCTION__, request_sq_wr->offset, mgr_req.request_done, error);
		
out1:
	if (mgr_req != NULL)
		release_writemgr_request(ctx, mgr_req);
	
	if (!error) {
		// write succeeded, so turn off retry state
		ctx->is_retry = 0;
	}

	return ( error );
}

/*****************************************************************************/

int filesystem_readdir(struct webdav_request_readdir *request_readdir)
{
	int error;
	struct node_entry *node;

	error = RetrieveDataFromOpaqueID(request_readdir->obj_id, (void **)&node);
	require_noerr_action_quiet(error, bad_obj_id, error = ESTALE);

	require_action_quiet(!NODE_IS_DELETED(node), deleted_node, error = ESTALE);
		
	error = network_readdir(request_readdir->pcr.pcr_uid, request_readdir->cache, node);

deleted_node:
bad_obj_id:

	return (error);
}

/*****************************************************************************/

int filesystem_lock(struct node_entry *node)
{
	int error;
	
	require_action(node != NULL, null_node, error = EIO);

	if ( node->file_locktoken != NULL )
	{
		error = network_lock(0, TRUE, node); /* uid for refreshes is ignored */
	}
	else
	{
		error = 0;
	}

null_node:
	
	return ( error );
}

/*****************************************************************************/

int filesystem_invalidate_caches(struct webdav_request_invalcaches *request_invalcaches)
{
	int error;
	
	/* only the owner (mounter) can invalidate */
	require_action(request_invalcaches->pcr.pcr_uid == gProcessUID, not_permitted, error = EPERM);
	
	nodecache_invalidate_caches();
	error = 0;

not_permitted:
	
	return (error);
}

/*****************************************************************************/
