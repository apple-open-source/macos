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

#ifndef __SCCQUEUE__
#define __SCCQUEUE__

#include "sys/types.h"

extern "C" 
{
    #include <kern/lock.h>
}

typedef struct CirQueue
{
    UInt8	*Start;
    UInt8	*End;
    UInt8	*NextChar;
    UInt8	*LastChar;
    size_t	Size;
    size_t	InQueue;
//    mutex_t	*InUse;
} CirQueue;

typedef enum QueueStatus
{
    queueNoError = 0,
    queueFull,
    queueEmpty,
    queueMaxStatus
} QueueStatus;

QueueStatus	InitQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size);
QueueStatus	CloseQueue(CirQueue *Queue);
void		ResetQueue(CirQueue *Queue);
size_t		AddtoQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size);
size_t		RemovefromQueue(CirQueue *Queue, UInt8	*Buffer, size_t MaxSize);
size_t		FreeSpaceinQueue(CirQueue *Queue);
size_t		UsedSpaceinQueue(CirQueue *Queue);
size_t		GetQueueSize( CirQueue *Queue);
QueueStatus	AddBytetoQueue(CirQueue *Queue, char Value);
QueueStatus	GetBytetoQueue(CirQueue *Queue, UInt8 *Value);
QueueStatus     GetQueueStatus(CirQueue *Queue);
UInt8*		BeginDirectReadFromQueue(CirQueue *Queue, size_t *size, bool *queueWrapped);
void		EndDirectReadFromQueue(CirQueue *Queue, size_t size);

#endif
