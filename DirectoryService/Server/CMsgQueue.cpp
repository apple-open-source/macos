/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*!
 * @header CMsgQueue
 */

/*
	Object linking:
	                         tail---+
                                    |
		 __       __       __       __
		|  | --> |  | --> |  | --> |  | 
		|__|     |__|     |__|     |__|
		 ^--------------------------
 */

#include "DirServicesTypes.h"

#include "CMsgQueue.h"
#include "CLog.h"

#include <time.h>

//--------------------------------------------------------------------------------------------------
//	* CMsgQueue()
//
//--------------------------------------------------------------------------------------------------

CMsgQueue::CMsgQueue ( void )
{
	fListTail		= nil;

	fMsgCount		= 0;
	fTotalMsgCnt	= 0;
} // CMsgQueue


//--------------------------------------------------------------------------------------------------
//	* ~CMsgQueue()
//
//--------------------------------------------------------------------------------------------------

CMsgQueue::~CMsgQueue()
{

} // ~CMsgQueue


//--------------------------------------------------------------------------------------------------
//	* QueueMessage()
//
//--------------------------------------------------------------------------------------------------

sInt32 CMsgQueue::QueueMessage ( void *inMsgData )
{
	sInt32			result	= eDSNoErr;
	sQueueItem	   *newObj	= nil;

	// Wait for our turn
	fMutex.Wait();

	// Create our message object
	newObj = new sQueueItem;
	if ( newObj != nil )
	{
		newObj->msgData = inMsgData;

		if ( fListTail == nil )
		{
			fListTail = newObj;
			fListTail->next = fListTail;

			fMsgCount = 1;
		}
		else
		{
			newObj->next = fListTail->next;
			fListTail->next = newObj;
			fListTail = newObj;

			fMsgCount++;
		}
	}
	else
	{
		result = kMemoryErr;
		DBGLOG2( kLogMsgQueue, "File: %s. Line: %d", __FILE__, __LINE__ );
		DBGLOG( kLogMsgQueue, "  Memory error." );
	}

	fMutex.Signal();

	return( result );

} // QueueMessage


//--------------------------------------------------------------------------------------------------
//	* DequeueMessage()
//
//--------------------------------------------------------------------------------------------------

sInt32 CMsgQueue::DequeueMessage ( void **outMsgData )
{
	sInt32			result		= eDSNoErr;
	sQueueItem	   *tmpObj		= nil;

	// Wait for our turn
	fMutex.Wait();

	if ( fListTail != nil )
	{
		if ( fListTail == fListTail->next )
		{
			*outMsgData = fListTail->msgData;
			delete( fListTail );
			fListTail = nil;
			fMsgCount = 0;
		}
		else
		{
			// Get the first object in the list
			//	the last object points to the first one
			tmpObj = fListTail->next;
			fListTail->next = tmpObj->next;
			*outMsgData = tmpObj->msgData;
			delete( tmpObj );
			tmpObj = nil;
			fMsgCount--;
		}
	}
	else
	{
		if ( fMsgCount != 0 )
		{
			DBGLOG2( kLogMsgQueue, "File: %s. Line: %d", __FILE__, __LINE__ );
			DBGLOG1( kLogMsgQueue, "  *** Bad message count.  Should be 0 but is %l.", fMsgCount );
		}
		result = kNoMessages;
	}

	// Let it //free
	fMutex.Signal();

	return( result );

} // DequeueMessage


//--------------------------------------------------------------------------------------------------
//	* GetMsgCount()
//
//--------------------------------------------------------------------------------------------------

uInt32 CMsgQueue::GetMsgCount ( void )
{
	uInt32		result = 0;

	// Wait for our turn
	fMutex.Wait();

	result = fMsgCount;

	// Let it //free
	fMutex.Signal();

	return( result );

} // GetMsgCount


//--------------------------------------------------------------------------------------------------
//	* ResetMsgCount()
//
//--------------------------------------------------------------------------------------------------

void CMsgQueue::ResetMsgCount ( void )
{
	uInt32			count		= 0;
	sQueueItem	   *queueObj	= nil;

	// Wait for our turn
	fMutex.Wait();

	if ( fListTail != nil )
	{
		queueObj = fListTail;

		do {
			count++;
			queueObj = queueObj->next;
		} while ( queueObj != fListTail );
	}

	fMsgCount = count;

	// Let it //free
	fMutex.Signal();

} // ResetMsgCount

