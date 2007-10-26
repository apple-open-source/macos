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

#ifndef __CMsgQueue_h__
#define __CMsgQueue_h__ 1

#include <DirectoryServiceCore/DSSemaphore.h>

class	CMsgQueue;
extern	CMsgQueue	*gMsgQueue;		

typedef struct sQueueItem sQueueItem;

typedef struct sQueueItem 
{
	void		   *msgData;
	sQueueItem	   *next;
} sQueueItem;


class CMsgQueue
{
public:
					CMsgQueue			( void );
	virtual		   ~CMsgQueue			( void );

    // returns false if no handlers for the queue
	bool			QueueMessage			( void *inMsgData );
    
    // returns true if message available
	bool			DequeueMessage			( void **outMsgData );

	UInt32			GetMsgCount();

    // returns false if times out, expecting that it will remove itself
    bool            WaitOnMessage           ( UInt32 inMilliSecs );
	void			ClearMsgQueue			( void );

private:

	sQueueItem		   *fListTail;

    pthread_mutex_t		fMutex;
    pthread_cond_t		fCondition;
    
    DSSemaphore         fQueueLock;

    UInt32              fHandlersAvailable;
	UInt32				fMsgCount;
	UInt32				fTotalMsgCnt;
};

#endif
