/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * SccQueuePrimatives.h
 *
 * Writers:
 * Elias Keshishoglou 
 *
 * Copyright (c) 	1999 Apple Computer, Inc.  all rights reserved.
 */

#ifndef scc_queue_primatives_h
#define scc_queue_primatives_h

#include "sys/types.h"

extern "C" 
{
	#include <kern/lock.h>
}

typedef struct CirQueue {
    u_char	*Start;
    u_char	*End;
    u_char	*NextChar;
    u_char	*LastChar;
    size_t	Size;
    size_t	InQueue;
//rcs Eliminate for Tiger	mutex_t	*InUse;
	IOLock	*InUse;
} CirQueue;


typedef enum QueueStatus {
    queueNoError = 0,
    queueFull,
    queueEmpty,
    queueMaxStatus
} QueueStatus;

/*
 * Place Global prototypes here.
 */

QueueStatus	InitQueue(CirQueue *Queue, u_char	*Buffer, size_t Size);
QueueStatus	CloseQueue(CirQueue *Queue);
size_t		AddtoQueue(CirQueue	*Queue, u_char  *Buffer, size_t Size);
size_t		RemovefromQueue(CirQueue	*Queue, u_char	*Buffer, size_t MaxSize);
size_t		FreeSpaceinQueue(CirQueue *Queue);
size_t		UsedSpaceinQueue(CirQueue *Queue);
size_t		GetQueueSize( CirQueue *Queue);
QueueStatus	AddBytetoQueue(CirQueue *Queue, char Value);
QueueStatus	GetBytetoQueue(CirQueue *Queue, u_char *Value);
QueueStatus     GetQueueStatus(CirQueue *Queue);
u_char* BeginDirectReadFromQueue(CirQueue *Queue, size_t* size, Boolean* queueWrapped);
void EndDirectReadFromQueue(CirQueue *Queue, size_t size);

#endif
