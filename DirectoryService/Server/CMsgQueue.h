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

#include "DSMutexSemaphore.h"

class	CMsgQueue;
extern	CMsgQueue	*gTCPMsgQueue;		
extern	CMsgQueue	*gMsgQueue;		
extern	CMsgQueue	*gInternalMsgQueue;		
extern	CMsgQueue	*gCheckpwMsgQueue;		

typedef struct sQueueItem sQueueItem;

typedef struct sQueueItem 
{
	void		   *msgData;
	sQueueItem	   *next;
} sQueueItem;


class CMsgQueue
{
public:

enum {
	kMemoryErr		= -128,
	kNoMessages
} eObjErrors;

					CMsgQueue			( void );
	virtual		   ~CMsgQueue			( void );

	sInt32			QueueMessage			( void *inMsgData );
	sInt32			DequeueMessage			( void **outMsgData );

	uInt32			GetMsgCount();

private:
	void			ResetMsgCount			( void );

	sQueueItem		   *fListTail;

	uInt32				fMsgCount;
	uInt32				fTotalMsgCnt;

	DSMutexSemaphore		fMutex;
};

#endif
