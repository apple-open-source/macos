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
/*		@(#)webdav_inode.c		*
 *		(c) 2001   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_inode.c -- routines for managing the inode hash
 *
 *		MODIFICATION HISTORY:
 *				3-Jan-01	 Clark Warner	   File Creation
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <c.h>
#include <errno.h>
#include "webdav_inode.h"
#include "../webdav_fs.kextproj/webdav_fs.kmodproj/webdav.h"

/*****************************************************************************/

/* Generate a hash value for a null terminated
 * string
 */

int hashuri(const char *string, int length)
{
	/* We'll always take the last characters of the URI
	 * since they are the most unique.	We have a constant
	 * for this */

	int i, hash_val = 0;
	char *start;
	int numchars = MIN(length, WEBDAV_UNIQUE_HASH_CHARS);

	start = (char *)string + length - numchars;

	hash_val = 0;
	for (i = 1; *start != 0 && i <= numchars; i++, start++)
	{
		hash_val += (unsigned char)*start * i;
	}

	/* Now push the value over eight bits and put in 256
	  bits of length.	This will help files with the same
	  name in different directories hashs differently.
	*/

	hash_val = (hash_val << 8) + length%256;
	return (hash_val);

}

/*****************************************************************************/

/* Initialize the inode array */

int webdav_inode_init(char *uri, int urilen)
{
	int error;
	pthread_mutexattr_t mutexattr;
	webdav_file_record_t * filerec_ptr;

	/* Zero the table */

	bzero(&ginode_hashtbl, sizeof(ginode_hashtbl));

	/* Set up the mutex */

	error = pthread_mutexattr_init(&mutexattr);
	if (error)
	{
		return (error);
	}
	
	error = pthread_mutex_init(&ginode_lock, &mutexattr);
	if (error)
	{
		return (error);
	}

	/* Now that everything has been built,	put the initial URI
	 * in as the root file id.	We don't use the lock yet since
	 * we are still in the init path */

	filerec_ptr = malloc(sizeof(webdav_file_record_t));

	if (!filerec_ptr)
	{
		return (ENOMEM);
	}

	filerec_ptr->uri = malloc(urilen + 1);
	if (!filerec_ptr->uri)
	{
		free(filerec_ptr);
		return (ENOMEM);
	}

	bcopy(uri, filerec_ptr->uri, urilen);
	filerec_ptr->uri[urilen] = '\0';

	filerec_ptr->uri_length = urilen;
	filerec_ptr->next = 0;
	filerec_ptr->inode = WEBDAV_ROOTFILEID;
	filerec_ptr->file_handle = -1;

	ginode_hashtbl[hashuri((const char *)uri, urilen) % WEBDAV_FILE_RECORD_HASH_BUCKETS] = filerec_ptr;

	return (0);
}

/*****************************************************************************/

/* Given a uri and a length, search the inode hash table
   and get the inode out for this uri.	If there is no
   entry, assign a new inode and make the entry */

int webdav_get_inode(const char *uri, int length, int make_entry, int *inode)
{
	int hash_num, error = 0, error2 = 0, found = 0;
	webdav_file_record_t * filerec_ptr,  *head_ptr;

	error = pthread_mutex_lock(&ginode_lock);
	if (error)
	{
		return (error);
	}

	hash_num = hashuri(uri, length);
	filerec_ptr = head_ptr = ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS];

	while (filerec_ptr && !found)
	{
		if ((filerec_ptr->uri_length == length) && !memcmp(uri, filerec_ptr->uri, length))
		{
			*inode = filerec_ptr->inode;
			goto unlock;
		}
		else
		{
			filerec_ptr = filerec_ptr->next;
		}
	}

	/* if we are here then we did not find the entry, so make a new
	  one and put it on the head of the list if we have been asked to*/

	if (make_entry)
	{

		filerec_ptr = malloc(sizeof(webdav_file_record_t));

		if (!filerec_ptr)
		{
			error = ENOMEM;
			goto unlock;
		}

		filerec_ptr->uri = malloc(length + 1);
		if (!filerec_ptr->uri)
		{
			free(filerec_ptr);
			error = ENOMEM;
			goto unlock;
		}

		bcopy(uri, filerec_ptr->uri, length);
		filerec_ptr->uri[length] = '\0';
		filerec_ptr->uri_length = length;
		filerec_ptr->inode = ginode_cntr++;
		filerec_ptr->file_handle = -1;

		/* XXX, we could wrap inodes and that would suck
		  in the future we should keep a list of inodes of deleted
		  files so that we can reuse those numbers in case we
		  process more than 4 billion files.
		*/

		filerec_ptr->next = head_ptr;
		ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS] = filerec_ptr;
		*inode = filerec_ptr->inode;

	}
	else
	{
		*inode = 0;
	}

unlock:

	error2 = pthread_mutex_unlock(&ginode_lock);
	if (!error)
	{
		error = error2;
	}

	return (0);
}

/*****************************************************************************/

/* Given a uri and a length, search the inode hash table
   and replace its inode number with the one specified.	 If there is no
   entry, make one with the given inode number */

int webdav_set_inode(const char *uri, int length, int inode)
{

	int hash_num, error = 0, error2 = 0;
	webdav_file_record_t * filerec_ptr,  *head_ptr;

	error = pthread_mutex_lock(&ginode_lock);
	if (error)
	{
		return (error);
	}

	hash_num = hashuri(uri, length);
	filerec_ptr = head_ptr = ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS];

	while (filerec_ptr)
	{
		if ((filerec_ptr->uri_length == length) && !memcmp(uri, filerec_ptr->uri, length))
		{
			filerec_ptr->inode = inode;
			goto unlock;
		}
		else
		{
			filerec_ptr = filerec_ptr->next;
		}
	}

	/* if we are here then we did not find the entry, so make a new
	  one and put it on the head of the list if we have been asked to*/

	filerec_ptr = malloc(sizeof(webdav_file_record_t));

	if (!filerec_ptr)
	{
		error = ENOMEM;
		goto unlock;
	}

	filerec_ptr->uri = malloc(length + 1);
	if (!filerec_ptr->uri)
	{
		free(filerec_ptr);
		error = ENOMEM;
		goto unlock;
	}

	bcopy(uri, filerec_ptr->uri, length);
	filerec_ptr->uri[length] = '\0';
	filerec_ptr->uri_length = length;
	filerec_ptr->inode = inode;
	filerec_ptr->next = head_ptr;
	filerec_ptr->file_handle = -1;
	ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS] = filerec_ptr;

unlock:

	error2 = pthread_mutex_unlock(&ginode_lock);
	if (!error)
	{
		error = error2;
	}

	return (error);
}

/*****************************************************************************/

/* Given a uri and a length, search the inode hash table
   and remove the entry for this uri.  If there is no
   entry, just return success */

int webdav_remove_inode(const char *uri, int length)
{

	int hash_num, error = 0;
	webdav_file_record_t * filerec_ptr,  *prev_ptr;

	error = pthread_mutex_lock(&ginode_lock);
	if (error)
	{
		return (error);
	}

	hash_num = hashuri(uri, length);
	filerec_ptr = prev_ptr = ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS];

	/* If it is the first one, we will zero out the entry in the table*/

	if (filerec_ptr)
	{
		if ((filerec_ptr->uri_length == length) && !memcmp(uri, filerec_ptr->uri, length))
		{
			ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS] = filerec_ptr->next;
			free(filerec_ptr->uri);
			free(filerec_ptr);
			goto unlock;
		}
		else
		{
			filerec_ptr = filerec_ptr->next;
		}
	}
	else
	{
		/* There was no entry here so get out */
		goto unlock;
	}

	while (filerec_ptr)
	{
		if ((filerec_ptr->uri_length == length) && !memcmp(uri, filerec_ptr->uri, length))
		{
			prev_ptr->next = filerec_ptr->next;
			free(filerec_ptr->uri);
			free(filerec_ptr);
			goto unlock;
		}
		else
		{
			prev_ptr = filerec_ptr;
			filerec_ptr = filerec_ptr->next;
		}
	}

unlock:

	error = pthread_mutex_unlock(&ginode_lock);

	return (error);
}

/*****************************************************************************/

/* Given a uri and a length, search the inode hash table
   and get the file_handle for this uri.  If there is no
   entry, return -1 in a_file_handle, (same as if an entry 
   is found but it has no file handle). */

int webdav_get_file_handle(const char *uri, int length, webdav_filehandle_t *a_file_handle)
{

	int hash_num, error = 0, found = 0;
	webdav_file_record_t * filerec_ptr,  *head_ptr;

	error = pthread_mutex_lock(&ginode_lock);
	if (error)
	{
		return (error);
	}

	*a_file_handle = -1;

	hash_num = hashuri(uri, length);
	filerec_ptr = head_ptr = ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS];

	while (filerec_ptr && !found)
	{
		if ((filerec_ptr->uri_length == length) && !memcmp(uri, filerec_ptr->uri, length))
		{
			*a_file_handle = filerec_ptr->file_handle;
			goto unlock;
		}
		else
		{
			filerec_ptr = filerec_ptr->next;
		}
	}

unlock:

	error = pthread_mutex_unlock(&ginode_lock);

	return (error);
}

/*****************************************************************************/

/* Given a uri and a length, search the inode hash table
   and set the file_handle for this uri. By the time we
   are setting a file handle there should be an entry
   in the hash table.  If there isn't this is an error
   */
   
int webdav_set_file_handle(const char *uri, int length, webdav_filehandle_t file_handle)
{

	int hash_num, error = 0, error2 = 0, found = 0;
	webdav_file_record_t * filerec_ptr,  *head_ptr;

	error = pthread_mutex_lock(&ginode_lock);
	if (error)
	{
		return (error);
	}

	hash_num = hashuri(uri, length);
	filerec_ptr = head_ptr = ginode_hashtbl[hash_num%WEBDAV_FILE_RECORD_HASH_BUCKETS];

	while (filerec_ptr && !found)
	{
		if ((filerec_ptr->uri_length == length) && !memcmp(uri, filerec_ptr->uri, length))
		{
			filerec_ptr->file_handle = file_handle;
			goto unlock;
		}
		else
		{
			filerec_ptr = filerec_ptr->next;
		}
	}

	/* If we are here, we did not find the element in the array
	 * this should not happen as looking up the file should have
	 * caused the element to be created.  It it does happen we'll
	 * just return an error
	 */

	error = ENOENT;
	/* fall through to unlock */

unlock:

	error2 = pthread_mutex_unlock(&ginode_lock);
	if (!error)
	{
		error = error2;
	}

	return (error);
}

/*****************************************************************************/
