/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include <sys/time.h>	// for struct timespec and gettimeofday();
#include <errno.h>

//--------------------------------------------------------------------------------------------------
//	* CMsgQueue()
//
//--------------------------------------------------------------------------------------------------

CMsgQueue::CMsgQueue ( void )
{
	fListTail           = nil;

	fMsgCount           = 0;
	fTotalMsgCnt        = 0;
    fHandlersAvailable  = 0;
    
    pthread_mutex_init( &fMutex, NULL );
    pthread_cond_init( &fCondition, NULL );
} // CMsgQueue


//--------------------------------------------------------------------------------------------------
//	* ~CMsgQueue()
//
//--------------------------------------------------------------------------------------------------

CMsgQueue::~CMsgQueue()
{
    pthread_mutex_destroy( &fMutex );
    pthread_cond_destroy( &fCondition );
} // ~CMsgQueue


//--------------------------------------------------------------------------------------------------
//	* QueueMessage()
//
//--------------------------------------------------------------------------------------------------

bool CMsgQueue::QueueMessage ( void *inMsgData )
{
	bool           result	= false;
	sQueueItem	   *newObj	= nil;

	// Wait for our turn
    fQueueLock.WaitLock();

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
		DbgLog( kLogMsgQueue, "File: %s. Line: %d", __FILE__, __LINE__ );
		DbgLog( kLogMsgQueue, "  Memory error." );
	}

    fQueueLock.SignalLock();
    
    // signal one of the threads there is something to do
    pthread_mutex_lock( &fMutex );
    if( fHandlersAvailable > 0 )
    {
        result = true;
        pthread_cond_signal( &fCondition );
    }
    pthread_mutex_unlock( &fMutex );    
    
	return( result );

} // QueueMessage


//--------------------------------------------------------------------------------------------------
//	* DequeueMessage()
//
//--------------------------------------------------------------------------------------------------

bool CMsgQueue::DequeueMessage ( void **outMsgData )
{
	bool			result		= false;
	sQueueItem	   *tmpObj		= nil;

	// Wait for our turn
    fQueueLock.WaitLock();

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
        
		result = true;
	}
	else
	{
		if ( fMsgCount != 0 )
		{
			DbgLog( kLogMsgQueue, "File: %s. Line: %d", __FILE__, __LINE__ );
			DbgLog( kLogMsgQueue, "  *** Bad message count.  Should be 0 but is %l.", fMsgCount );
		}
	}

    fQueueLock.SignalLock();
    
    return( result );

} // DequeueMessage


//--------------------------------------------------------------------------------------------------
//	* GetMsgCount()
//
//--------------------------------------------------------------------------------------------------

UInt32 CMsgQueue::GetMsgCount ( void )
{
	UInt32		result = 0;

	// Wait for our turn
    fQueueLock.WaitLock();

	result = fMsgCount;

	// Let it //free
    fQueueLock.SignalLock();

	return( result );

} // GetMsgCount


//--------------------------------------------------------------------------------------------------
//	* ResetMsgCount()
//
//--------------------------------------------------------------------------------------------------

void CMsgQueue::ClearMsgQueue ( void )
{
	UInt32			count		= 0;
	sQueueItem	   *queueObj	= nil;

	// Wait for our turn
    fQueueLock.WaitLock();

	if ( fListTail != nil )
	{
		queueObj = fListTail;

		do {
			count++;
			queueObj = queueObj->next;
		} while ( queueObj != fListTail );
	}

	fMsgCount = count;

    // flushed queue, let's broadcast so all waiting threads go away
    pthread_mutex_lock( &fMutex );
    pthread_cond_broadcast( &fCondition );
    pthread_mutex_unlock( &fMutex );    
    
    // Let it free
    fQueueLock.SignalLock();

} // ResetMsgCount

bool CMsgQueue::WaitOnMessage( UInt32 inMilliSecs )
{
    bool bReturn = true;

    pthread_mutex_lock( &fMutex );
    
    fHandlersAvailable++;
    
    // we only lock if we didn't have a broadcast
    if( inMilliSecs == 0 ) // wait forever
    {
        pthread_cond_wait( &fCondition, &fMutex );
    }
    else if( inMilliSecs > 0 )
    {
        struct timeval	tvNow;
        struct timespec	tsTimeout;
        
        gettimeofday( &tvNow, NULL );
        TIMEVAL_TO_TIMESPEC ( &tvNow, &tsTimeout );
        tsTimeout.tv_sec += (inMilliSecs / 1000);
        tsTimeout.tv_nsec += ((inMilliSecs % 1000) * 1000000);
        
        if( pthread_cond_timedwait(&fCondition, &fMutex, &tsTimeout) == ETIMEDOUT )
        {
            // we timed out, our return is now false, nothing to do
            bReturn = false;
        }
    }
    
    fHandlersAvailable--;
    pthread_mutex_unlock( &fMutex );

    // now return whether there is something to do or not
    return bReturn;
}

