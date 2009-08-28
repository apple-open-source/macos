/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <pthread.h>

#include "OpaqueIDs.h"

enum
{
	/*
	 * An opaque id is composed of two parts.  The lower half is an index into
	 * a table which holds the actual data for the opaque id.  The upper half is a
	 * counter which insures that a particular opaque id value isn't reused for a long
	 * time after it has been disposed.  Currently, with 20 bits as the index, there
	 * can be (2^20)-2 opaque ids in existence at any particular point in time
	 * (index 0 is not used), and no opaque id value will be re-issued more frequently than every
	 * 2^12th (4096) times. (although, in practice many more opaque ids will be issued
	 * before one is re-used).
	 */
	kOpaqueIDIndexBits = 20,
	kOpaqueIDIndexMask = ((1 << kOpaqueIDIndexBits) - 1),
	kOpaqueIDMaximumCount = (1 << kOpaqueIDIndexBits) - 1, /* all 0 bits are never a valid index */

	/*
	 * This keeps a little 'extra room' in the table, so that if it gets
	 * nearly full that a dispose and re-allocate won't quickly run thru the couple
	 * couple free slots in the table.  So, if there are less than this number of
	 * available items, the table will get grown.
	 */
	kOpaqueIDMinimumFree = 1024,
	/* When the table is grown, grow by this many entries */
	kOpaqueIDGrowCount = 2048
};

/*
 * Create an MPOpaqeID by masking together a count and and index.
 */
#define CreateOpaqueID(counter, index) (opaque_id)(((counter) << kOpaqueIDIndexBits) | ((index) & kOpaqueIDIndexMask))

/*
 * Get the count part of an opaque_id
 */
#define GetOpaqueIDCounterPart(id) (((uint32_t)(id)) >> kOpaqueIDIndexBits)

/*
 * Get the index part of an opaque_id
 */
#define GetOpaqueIDIndexPart(id) (((uint32_t)(id)) & kOpaqueIDIndexMask)

/*****************************************************************************/

/*
 * Keep a 'list' of free records in the gOpaqueEntryArray table, using the .nextIndex field as
 * the link to the next one.  Nodes are put into this list at the end, and removed
 * from the front, to try harder to keep from re-allocating a particular opaque id
 * anytime soon after it has been disposed of.
 */
 
struct OpaqueEntry
{
	opaque_id id;	/* when in use, this is the opaque ID; when not in use, the index part is zero */
	uint32_t nextIndex;	/* linkage for the free list */
	void *data;		/* when in use, this is pointer to the data; it is NULL otherwise */
};
typedef struct OpaqueEntry *OpaqueEntryArrayPtr;

static pthread_mutex_t gOpaqueEntryMutex = PTHREAD_MUTEX_INITIALIZER;
static u_int32_t gOpaqueEntriesAllocated = 0;
static u_int32_t gOpaqueEntriesUsed = 0;
static OpaqueEntryArrayPtr gOpaqueEntryArray = NULL;

static u_int32_t gIndexOfFreeOpaqueEntryHead = 0;
static u_int32_t gIndexOfFreeOpaqueEntryTail = 0;

/*****************************************************************************/

/*
 * AddToFreeList adds the OpaqueEntry at the specified index in the gOpaqueEntryArray array
 * to the free list.
 */
static void AddToFreeList(u_int32_t indexToFree)
{
	OpaqueEntryArrayPtr freeEntry;

	/* don't add the OpaqueEntry at index 0 to free list -- it just won't be used */
	if ( indexToFree != 0 )
	{
		freeEntry = &gOpaqueEntryArray[indexToFree];
		freeEntry->data = 0;
		freeEntry->nextIndex = 0;
		
		/* Add this OpaqueEntry to the tail of the free list */
		if ( gIndexOfFreeOpaqueEntryTail != 0 )
		{
			gOpaqueEntryArray[gIndexOfFreeOpaqueEntryTail].nextIndex = indexToFree;
		}
		gIndexOfFreeOpaqueEntryTail = indexToFree;

		/* If the head of the free list is 0, then this OpaqueEntry is also the head */
		if ( gIndexOfFreeOpaqueEntryHead == 0 )
		{
			gIndexOfFreeOpaqueEntryHead = indexToFree;
		}
	}
}

/*****************************************************************************/

/*
 * RemoveFromFreeList removes a OpaqueEntry from the free list and returns
 * its index in the gOpaqueEntryArray array.
 */
static u_int32_t RemoveFromFreeList()
{
	u_int32_t result;
	
	/* are there any OpaqueEntries free? */
	if ( gIndexOfFreeOpaqueEntryHead != 0 )
	{
		/* remove the OpaqueEntry from the head */
		result = gIndexOfFreeOpaqueEntryHead;

		if ( gIndexOfFreeOpaqueEntryHead == gIndexOfFreeOpaqueEntryTail )
		{
			gIndexOfFreeOpaqueEntryTail = 0;
		}

		gIndexOfFreeOpaqueEntryHead = gOpaqueEntryArray[gIndexOfFreeOpaqueEntryHead].nextIndex;
	}
	else
	{
		result = 0;
	}

	return ( result );
}

/*****************************************************************************/

int AssignOpaqueID(void *inData, opaque_id *outID)
{
	int error;
	u_int32_t entryToUse;
	
	require_action(outID != NULL, bad_parameter, error = EINVAL);
	
	*outID = kInvalidOpaqueID;
	
	error = pthread_mutex_lock(&gOpaqueEntryMutex);
	require_noerr(error, pthread_mutex_lock);
	
	/*
	 * If there aren't any items in the table, or if the number of free items is
	 * lower than we want, then grow the table.
	 */
	if ( (gIndexOfFreeOpaqueEntryHead == 0) || ((gOpaqueEntriesAllocated - gOpaqueEntriesUsed) < kOpaqueIDMinimumFree) )
	{
		u_int32_t newCount;
		
		newCount = MIN(gOpaqueEntriesAllocated + 2048, kOpaqueIDMaximumCount);

		if ( gOpaqueEntriesAllocated < newCount )
		{
			OpaqueEntryArrayPtr nuids;
			
			nuids = (OpaqueEntryArrayPtr)realloc(gOpaqueEntryArray, sizeof(struct OpaqueEntry) * newCount);

			if ( nuids != NULL )
			{
				u_int32_t i;

				gOpaqueEntryArray = nuids;

				/* Add all the 'new' OpaqueEntry to the free list. */
				for ( i = 0; i < newCount - gOpaqueEntriesAllocated; ++i )
				{
					/* set both count and index to 0 */
					gOpaqueEntryArray[gOpaqueEntriesAllocated + i].id = 0;

					AddToFreeList(gOpaqueEntriesAllocated + i);
				}

				gOpaqueEntriesAllocated = newCount;
			}
		}
	}

	/* get index of an OpaqueEntry to use */
	entryToUse = RemoveFromFreeList();

	/* release the lock */
	pthread_mutex_unlock(&gOpaqueEntryMutex);

	/* did we get an OpaqueEntry? */
	require_action((entryToUse != 0) && (entryToUse < gOpaqueEntriesAllocated), no_opaqueID, error = EINVAL);
		
	/* the new id is created with the previous counter + 1, and the index */
	gOpaqueEntryArray[entryToUse].id = CreateOpaqueID(GetOpaqueIDCounterPart(gOpaqueEntryArray[entryToUse].id) + 1, entryToUse);
	gOpaqueEntryArray[entryToUse].data = inData;
	
	*outID = gOpaqueEntryArray[entryToUse].id;

	++gOpaqueEntriesUsed;

no_opaqueID:
pthread_mutex_lock:
bad_parameter:

	return ( error );
}

/*****************************************************************************/

int DeleteOpaqueID(opaque_id inID)
{
	int error;
	uint32_t index;

	error = pthread_mutex_lock(&gOpaqueEntryMutex);
	require_noerr(error, pthread_mutex_lock);
	
	index = GetOpaqueIDIndexPart(inID);
	if ( (index != 0) && (index < gOpaqueEntriesAllocated) && (gOpaqueEntryArray[index].id == inID) )
	{
		/*
		 * Keep the old counter so that next time we can increment the
		 * generation count and return a 'new' opaque ID which maps to this
		 * same index. The index is set to zero to indicate this entry is not
		 * in use.
		 */
		gOpaqueEntryArray[index].id = CreateOpaqueID(GetOpaqueIDCounterPart(inID), 0);

		AddToFreeList(index);
		--gOpaqueEntriesUsed;
	}
	else
	{
		error = EINVAL;
	}

	pthread_mutex_unlock(&gOpaqueEntryMutex);

pthread_mutex_lock:

	return ( error );
}

/*****************************************************************************/

int RetrieveDataFromOpaqueID(opaque_id inID, void **outData)
{
	int error;
	uint32_t index;

	error = pthread_mutex_lock(&gOpaqueEntryMutex);
	require_noerr(error, pthread_mutex_lock);
	
	index = GetOpaqueIDIndexPart(inID);
	if ( (index != 0) && (index < gOpaqueEntriesAllocated) && (gOpaqueEntryArray[index].id == inID) )
	{
		if (outData)
		{
			*outData = gOpaqueEntryArray[index].data;
		}
	}
	else
	{
		error = EINVAL;
	}
	
	pthread_mutex_unlock(&gOpaqueEntryMutex);

pthread_mutex_lock:

	return ( error );
}

/*****************************************************************************/
