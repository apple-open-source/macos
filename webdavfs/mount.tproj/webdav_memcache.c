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

#include <sys/types.h>
#include <sys/syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "webdavd.h"
#include "webdav_memcache.h"

static
void webdav_free_remaining(webdav_memcache_element_t *current_item);
static
int create_webdav_memcache_element(const char *uri, int uri_length, struct vattr *vap,
	char *appledoubleheader, time_t now, webdav_memcache_element_t **element);

/*****************************************************************************/

/* webdav_free_remaining() 
 * This routine free's the memory of a chain of cache elements starting
 * from the given argument.	 It assumes that the caller will fix up the
 * link pointers and have the cache header lock.
 */

static
void webdav_free_remaining(current_item)
	webdav_memcache_element_t *current_item;
{
	webdav_memcache_element_t * next_item;

	next_item = current_item->next;

	while (current_item)
	{
		free(current_item->uri);
		if (current_item->appledoubleheader)
		{
			free(current_item->appledoubleheader);
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
		syslog(LOG_ERR, "webdav_memcache_init: pthread_mutexattr_init() failed: %s", strerror(error));
		return (error);
	}

	error = pthread_mutex_init(&(cache_header->lock), &mutexattr);
	if (error)
	{
		syslog(LOG_ERR, "webdav_memcache_init: pthread_mutex_init() failed: %s", strerror(error));
		return (error);
	}
	for (i = 0; i < WEBDAV_NUMCACHEDUIDS; i++)
	{
		(cache_header->uid_array)[i].uid = (uid_t)-1;
	}
	
	return (error);
}

/*****************************************************************************/

static
int create_webdav_memcache_element(const char *uri, int uri_length, struct vattr *vap,
	char *appledoubleheader, time_t now, webdav_memcache_element_t **element)
{
	int error;
	webdav_memcache_element_t *new_item;
	
	/* Make the new item to insert */
	new_item = malloc(sizeof(webdav_memcache_element_t));
	if (!new_item)
	{
		/* We ignore errors from this because it just won't be cached */
#ifdef DEBUG
		syslog(LOG_INFO, "create_webdav_memcache_element: could not allocate new_item");
#endif
		error = ENOMEM;
		goto exit;
	}
	
	new_item->uri = malloc((size_t)(uri_length + 1));
	if (!(new_item->uri))
	{
		/* We ignore errors from this because it just won't be cached */
#ifdef DEBUG
		syslog(LOG_INFO, "create_webdav_memcache_element: could not allocate new_item->uri");
#endif
		error = ENOMEM;
		goto free_element;
	}
	new_item->uri_length = uri_length;
	strcpy(new_item->uri, uri);

	bcopy(vap, &(new_item->vap), sizeof(struct vattr));

	if (appledoubleheader)
	{
		new_item->appledoubleheader = malloc(APPLEDOUBLEHEADER_LENGTH);
		if (!(new_item->appledoubleheader))
		{
			/* We ignore errors from this because it just won't be cached */
#ifdef DEBUG
			syslog(LOG_INFO, "create_webdav_memcache_element: could not allocate new_item->vap");
#endif
			error = ENOMEM;
			goto free_all;
		}
		bcopy(appledoubleheader, new_item->appledoubleheader, APPLEDOUBLEHEADER_LENGTH);
	}
	else
	{
		new_item->appledoubleheader = NULL;
	}

	new_item->next = 0;
	new_item->time_received = now;
	
	error = 0;
	goto exit;

free_all:
	
	if (new_item->uri)
	{
		free(new_item->uri);
	}

free_element:

	free(new_item);
	new_item = NULL;
	
exit:

	*element = new_item;
	return (error);
}

/*****************************************************************************/

int webdav_memcache_insert(uid_t uid, const char *uri, webdav_memcache_header_t *cache_header,
	struct vattr *vap, char *appledoubleheader)
{
	webdav_memcache_element_t * new_item;
	webdav_memcache_element_t * current_item;
	webdav_memcache_element_t * previous_item;
	int error = 0, error2 = 0;
	int i = 0;
	int uidslot = 0;
	int uri_length;
	time_t now;
	
	now = time(0);
	if ( now == -1 )
	{
#ifdef DEBUG
		syslog(LOG_INFO, "webdav_memcache_insert: time(): %s", strerror(errno));
#endif
		return (errno);
	}
	
	uri_length = strlen(uri);
	
	/* Lock the cache */
	error = pthread_mutex_lock(&(cache_header->lock));
	if (error)
	{
		syslog(LOG_ERR, "webdav_memcache_insert: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		return (error);
	}

	/* find the uid in the list. If there isn't one grab an empty slot. */
	/* If they are all taken, purge one and take it over. */
	for (i = 0; i < WEBDAV_NUMCACHEDUIDS; ++i)
	{
		if (cache_header->uid_array[i].uid == uid)
		{
			uidslot = i;
			break;
		}
	}
	
	if (i == WEBDAV_NUMCACHEDUIDS)
	{
		/* Make the new item to insert before freeing up a slot
		 * since we know that we must insert. If this fails, we leave 
		 * the existing slots alone.
		 */
		error = create_webdav_memcache_element(uri, uri_length, vap, appledoubleheader,
			now, &new_item);
		if (error)
		{
			goto unlock;
		}
		
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
		
		/* first look for duplicates and purge any timed out elements */
		current_item = cache_header->uid_array[uidslot].item_head;
		previous_item = NULL;
		while (current_item)
		{
			if ( now > (current_item->time_received + WEBDAV_MEMCACHE_TIMEOUT))
			{
				/* current_item is past our time out. Since we always
				 * insert at the head, that means that everything else is also overdue so
				 * we'll purge the whole list
				 */
				if (!previous_item)
				{
					/* we are at the head of the list and are wiping out the whole thing */
					cache_header->uid_array[i].item_head = NULL;
				}
				else
				{
					previous_item->next = NULL;
				}
				cache_header->uid_array[i].item_tail = previous_item;
				webdav_free_remaining(current_item);
				break;	/* no match found */
			}
			else
			{
				/* We haven't timed out this element so let's see if it is the one we want */
				if ((uri_length == current_item->uri_length) &&
					(!memcmp(uri, current_item->uri, (size_t)uri_length)))
				{
					/* we found it */
					/* unlink it */
					if (!previous_item)
					{
						/* we are at the head of the list */
						cache_header->uid_array[uidslot].item_head = current_item->next;
					}
					else
					{
						previous_item->next = current_item->next;
					}
					if (cache_header->uid_array[i].item_tail == current_item)
					{
						/* we were at the tail of the list */
						cache_header->uid_array[i].item_tail = previous_item;
					}
					
					/* and remove it */
					current_item->next = NULL;
					webdav_free_remaining(current_item);
					break;	/* break after removing match */
				}
				else
				{
					/* we didn't find it so move on */
					previous_item = current_item;
					current_item = previous_item->next;
				}
			}
		}
		
		/* make the new item to insert */
		error = create_webdav_memcache_element(uri, uri_length, vap, appledoubleheader,
			now, &new_item);
		if (error)
		{
			goto unlock;
		}
		
		/* link new_item in at the head */
		new_item->next = cache_header->uid_array[uidslot].item_head;
		cache_header->uid_array[uidslot].item_head = new_item;
		if (!cache_header->uid_array[i].item_tail)
		{
			cache_header->uid_array[i].item_tail = new_item;
		}
	}

unlock:

	error2 = pthread_mutex_unlock(&(cache_header->lock));
	if (error2)
	{
		syslog(LOG_ERR, "webdav_memcache_insert: pthread_mutex_unlock(): %s", strerror(error2));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		if ( !error )
		{
			error = error2;
		}
	}

	return (error);
}

/*****************************************************************************/

int webdav_memcache_retrieve(uid_t uid, char *uri,
	webdav_memcache_header_t *cache_header, struct vattr *vap, char *appledoubleheader,
	int32_t *lastvalidtime)
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
		syslog(LOG_ERR, "webdav_memcache_retrieve: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
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
				(!memcmp(uri, current_item->uri, (size_t)length)))
			{
				/* we found it, hurray */
				/* copy it out of the cache while we have the lock */
				bcopy(&(current_item->vap), vap, sizeof(struct vattr));
				if (lastvalidtime)
				{
					*lastvalidtime = current_item->time_received;
				}
				if (appledoubleheader)
				{
					/* called because appledoubleheader was required */
					if (current_item->appledoubleheader)
					{
						bcopy(current_item->appledoubleheader, appledoubleheader, APPLEDOUBLEHEADER_LENGTH);
						result = TRUE;
						goto done;
					}
					else
					{
						/* We didn't find appledoubleheader so move on.
						 * Another element might have it.
						 */
						previous_item = current_item;
						current_item = previous_item->next;
					}
				}
				else
				{
					/* caller didn't require appledoubleheader so return with just the stat info */
					result = TRUE;
					goto done;
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

done:

	error = pthread_mutex_unlock(&(cache_header->lock));
	if ( error )
	{
		syslog(LOG_ERR, "webdav_memcache_retrieve: pthread_mutex_unlock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}

	return (result);
}

/*****************************************************************************/

int webdav_memcache_remove(uid, uri, cache_header)
	uid_t uid;										/* uid is not used */
	char *uri;
	webdav_memcache_header_t *cache_header;
{
	#pragma unused(uid)
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
		syslog(LOG_ERR, "webdav_memcache_remove: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		return (error);
	}

	/* find the uid in the list */
	for (i = 0; i < WEBDAV_NUMCACHEDUIDS; ++i)
	{

		if (cache_header->uid_array[i].uid == (uid_t)-1)
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
					(!memcmp(uri, current_item->uri, (size_t)length)))
				{
					/* we found it, hurray */
					free(current_item->uri);
					if (current_item->appledoubleheader)
					{
						free(current_item->appledoubleheader);
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
	if ( error )
	{
		syslog(LOG_ERR, "webdav_memcache_remove: pthread_mutex_unlock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}

	return (error);
}

/*****************************************************************************/

int webdav_memcache_invalidate(webdav_memcache_header_t *cache_header)
{
	int error;
	int index;

	/* lock the cache */
	error = pthread_mutex_lock(&(cache_header->lock));
	if (error)
	{
		syslog(LOG_ERR, "webdav_memcache_invalidate: pthread_mutex_lock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
		goto done;
	}
	
	/* find the uid in the list */
	for (index = 0; index < WEBDAV_NUMCACHEDUIDS; ++index)
	{
		/* if this uid_array is valid */
		if (cache_header->uid_array[index].uid != (uid_t)-1)
		{
			/* then free all of its elements (if any) */
			if (cache_header->uid_array[index].item_head != NULL)
			{
				webdav_free_remaining(cache_header->uid_array[index].item_head);
				cache_header->uid_array[index].item_head = cache_header->uid_array[index].item_tail = NULL;
			}
		}
	}
	
	/* unlock the cache */
	error = pthread_mutex_unlock(&(cache_header->lock));
	if ( error )
	{
		syslog(LOG_ERR, "webdav_memcache_invalidate: pthread_mutex_unlock(): %s", strerror(error));
		webdav_kill(-1);	/* tell the main select loop to force unmount */
	}

done:

	return (error);
}

/*****************************************************************************/
