/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*		@(#)webdav_memcache.c	   *
 *		(c) 1999   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_memcache.c -- WebDAV in memory cache for stat information
 *
 *		MODIFICATION HISTORY:
 *				12-NOV-99	  Clark Warner		File Creation
 */

#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "webdav_memcache.h"

/*****************************************************************************/

/* webdav_free_remaining() 
 * This routine free's the memory of a chain of cache elements starting
 * from the given argument.	 It assumes that the caller will fix up the
 * link pointers and have the cache header lock.
 */

void webdav_free_remaining(current_item)
	webdav_memcache_element_t *current_item;
{
	webdav_memcache_element_t * next_item;

	next_item = current_item->next;

	while (current_item)
	{
		/* *** workaround *** */
		if (current_item->uri_length < 200)
		{
			free(current_item->vap);
			free(current_item->uri);
		}
		free(current_item);
		current_item = next_item;
		if (current_item)
		{
			next_item = current_item->next;
		}
	}
}

/*****************************************************************************/

int webdav_memcache_init(cache_header)
	webdav_memcache_header_t *cache_header;
{
	int i, error = 0;
	pthread_mutexattr_t mutexattr;

	bzero(cache_header, sizeof(*cache_header));
	cache_header->open_uids = WEBDAV_NUMCACHEDUIDS;
	cache_header->last_uid = 0;

	/* set up the lock on the array */
	error = pthread_mutexattr_init(&mutexattr);
	if (error)
	{
		return (error);
	}

	error = pthread_mutex_init(&(cache_header->lock), &mutexattr);
	if (error)
	{
		return (error);
	}
	for (i = 0; i < WEBDAV_NUMCACHEDUIDS; i++)
	{
		(cache_header->uid_array)[i].uid = -1;
	}

	return (error);
}

/*****************************************************************************/

int webdav_memcache_insert(uid, uri, cache_header, vap)
	uid_t uid;
	char *uri;
	webdav_memcache_header_t *cache_header;
	struct vattr *vap;
{
	webdav_memcache_element_t * new_item;
	webdav_memcache_element_t * current_item = NULL;

	int error = 0;
	int error2;
	int i = 0;
	int uidslot = 0;

	/* Lock the cache */
	error = pthread_mutex_lock(&(cache_header->lock));
	if (error)
	{
		return (error);
	}

	/* Make the new item to insert */

	new_item = malloc(sizeof(webdav_memcache_element_t));
	if (!new_item)
	{
		error = ENOMEM;
		goto unlock;
	}

	new_item->uri = malloc(strlen(uri) + 1);
	if (!(new_item->uri))
	{
		error = ENOMEM;
		goto free_element;
	}
	new_item->uri_length = strlen(uri);
	strcpy(new_item->uri, uri);

	new_item->vap = malloc(sizeof(struct vattr));
	if (!(new_item->vap))
	{
		error = ENOMEM;
		goto free_uri;
	}

	bcopy(vap, new_item->vap, sizeof(struct vattr));

	new_item->next = 0;
	new_item->time_received = time(0);
	if (new_item->time_received == -1)
	{
		error = errno;
		goto free_all;
	}

	/* find the uid in the list.  If there isn't one grab an emptly slot */
	/* If they are all taken, purge one and take it over */

	for (i = 0; i < WEBDAV_NUMCACHEDUIDS; ++i)
	{
		if (cache_header->uid_array[i].uid == uid)
		{
			current_item = cache_header->uid_array[i].item_head;
			uidslot = i;
			break;
		}
	}

	if (i == WEBDAV_NUMCACHEDUIDS)
	{
		/* We didn't find our slot so grab a free one*/

		uidslot = cache_header->last_uid;
		if (cache_header->open_uids == 0)
		{
			/* No slots are open so we will purge the current one and
			 * take it over
			 */
			cache_header->uid_array[uidslot].uid = uid;
			current_item = cache_header->uid_array[uidslot].item_head;

			if (current_item)
			{
				webdav_free_remaining(current_item);
			}

		}
		else
		{
			/* we had an open slot so we'll just use that */

			--cache_header->open_uids;
			cache_header->uid_array[uidslot].uid = uid;
		}

		cache_header->uid_array[uidslot].item_head = new_item;
		cache_header->uid_array[uidslot].item_tail = new_item;

		/* ok, we have finished getting the slot so update the lastuid */

		++cache_header->last_uid;
		if (cache_header->last_uid == WEBDAV_NUMCACHEDUIDS)
		{
			cache_header->last_uid = 0;
		}

	}
	else
	{
		/* we found this uid so just put the item on the head of the list */

		new_item->next = current_item;
		cache_header->uid_array[uidslot].item_head = new_item;
	}

	goto unlock;

free_all:

	free(new_item->vap);

free_uri:

	free(new_item->uri);

free_element:

	free(new_item);

unlock:

	error2 = pthread_mutex_unlock(&(cache_header->lock));
	if (!error)
	{
		error = error2;
	}

	return (error);
}

/*****************************************************************************/

int webdav_memcache_retrieve(uid_t uid, char *uri,
	webdav_memcache_header_t *cache_header, struct vattr *vap)
{
	webdav_memcache_element_t * current_item = 0;
	webdav_memcache_element_t * previous_item = 0;
	int length = strlen(uri);
	time_t now = time(0);

	int i = 0;
	int error = 0;
	int	result = FALSE;

	/* lock the cache */
	error = pthread_mutex_lock(&(cache_header->lock));
	if (error)
	{
		return (result);
	}

	/* find the uid in the list */

	for (i = 0; i < WEBDAV_NUMCACHEDUIDS; ++i)
	{
		if (cache_header->uid_array[i].uid == uid)
		{
			current_item = cache_header->uid_array[i].item_head;
			break;
		}
	}

	previous_item = 0;
	while (current_item)
	{
		if (now > (current_item->time_received + WEBDAV_MEMCACHE_TIMEOUT))
		{
			/* The item we have found is past our time out.	 Since we always
			 * insert at the head, that means that everything else is overdue also so
			 * we'll purge the whole list
			 */
			if (!previous_item)
			{
				/* we are at the head of the list and are wiping out the whole thing */
				cache_header->uid_array[i].item_head = 0;
			}
			else
			{
				previous_item->next = 0;
			}
			cache_header->uid_array[i].item_tail = previous_item;
			webdav_free_remaining(current_item);
			goto done;
		}
		else
		{
			/* We haven't timed out this element so let's see if it is the one we want */
			if ((length == current_item->uri_length) &&
				(!memcmp(uri, current_item->uri, length)))
			{
				/* we found it, hurray */
				/* copy it out of the cache while we have the lock */
				bcopy(current_item->vap, vap, sizeof(struct vattr));
				result = TRUE;
				goto done;
			}
			else
			{
				/* we didn't find it so move on */
				previous_item = current_item;
				current_item = previous_item->next;
			}
		}
	}

done:

	error = pthread_mutex_unlock(&(cache_header->lock));

	return (result);
}

/*****************************************************************************/

int webdav_memcache_remove(uid, uri, cache_header)
	uid_t uid;										/* uid is not used */
	char *uri;
	webdav_memcache_header_t *cache_header;
{
	webdav_memcache_element_t * current_item = 0;
	webdav_memcache_element_t * previous_item = 0;
	int length = strlen(uri);
	time_t now = time(0);

	int i = 0;
	int error = 0;

	/* lock the cache */
	error = pthread_mutex_lock(&(cache_header->lock));
	if (error)
	{
		return (error);
	}

	/* find the uid in the list */

	for (i = 0; i < WEBDAV_NUMCACHEDUIDS; ++i)
	{

		if (cache_header->uid_array[i].uid == -1)
		{
			continue;
		}

		current_item = cache_header->uid_array[i].item_head;

		previous_item = 0;
		while (current_item)
		{
			if (now > (current_item->time_received + WEBDAV_MEMCACHE_TIMEOUT))
			{
				/* The item we have found is past our time out.	 Since we always
				 * insert at the head, that means that everything else is overdue also so
				 * we'll purge the whole list
				 */
				if (!previous_item)
				{
					/* we are at the head of the list and are wiping out the whole thing */
					cache_header->uid_array[i].item_head = 0;
				}
				else
				{
					previous_item->next = 0;
				}
				cache_header->uid_array[i].item_tail = previous_item;
				webdav_free_remaining(current_item);
				current_item = 0;				/* break out of the while loop */
			}
			else
			{
				/* We haven't timed out this element so let's pull it */
				if ((length == current_item->uri_length) &&
					(!memcmp(uri, current_item->uri, length)))
				{
					/* we found it, hurray */
					/* *** workaround *** */
					if (current_item->uri_length < 200)
					{
						free(current_item->uri);
						free(current_item->vap);
					}
					if (!previous_item)
					{
						/* we are at the head of the list so adjust the cache header */
						cache_header->uid_array[i].item_head = current_item->next;
						free(current_item);
						current_item = cache_header->uid_array[i].item_head;
					}
					else
					{
						previous_item->next = current_item->next;
						free(current_item);
						current_item = previous_item->next;
					}
				}
				else
				{
					/* we didn't find it so move on */
					previous_item = current_item;
					current_item = previous_item->next;
				}
			}
		}

	}											/* end for */

	/* if we got here, we either pulled the item or didn't find it;
	  either way it's a success */

	error = pthread_mutex_unlock(&(cache_header->lock));

	return (error);
}

/*****************************************************************************/
