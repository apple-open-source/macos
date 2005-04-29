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
 * SccQueuePrimatives.cpp
 *
 * Writers:
 * Elias Keshishoglou
 *
 * Copyright (c) 	1999 Apple Computer, Inc.  all rights reserved.
 */

#include <IOKit/IOLib.h>
#include "SccQueuePrimatives.h"

//#include <kern/cpu_data.h>
//#include <sys/kdebug.h>
//#include <kern/thread.h>

#ifdef ASSERTS_ALL
#include <IOKit/assert.h>
#include "SccTypes.h"
#endif

extern "C"
{
//    void _enable_preemption(void);
//    void _disable_preemption(void);
	#include <kern/lock.h>
}

/*---------------------------------------------------------------------
*		PrivateAddBytetoQueue
* ejk		Sun, Jan 11, 1998	20:48
* 		Add a byte to the circular queue.
*
*-------------------------------------------------------------------*/
static QueueStatus	PrivateAddBytetoQueue(CirQueue *Queue, char Value) {

    QueueStatus returnVal = queueFull;

	IOLockLock(Queue->InUse);

    /* Check to see if there is space by comparing the next pointer,
    with the last, If they match we are either Empty or full, so
    check the InQueue of being zero.
    */
    if ((Queue->NextChar == Queue->LastChar) && Queue->InQueue) {
#ifdef TRACE
        KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 0x0A)) | DBG_FUNC_NONE, (UInt)Queue, 0, 0, 0, 0); //0x06080028
#endif
        returnVal = queueFull;
    }
    else {
#ifdef	ASSERT_ALL
        assert(Queue->InQueue >= 0 &&  Queue->InQueue <= kMaxCirBufferSize);
#endif

        *Queue->NextChar++ = Value;
        Queue->InQueue++;

        /* Check to see if we need to wrap the pointer.
            */
        if (Queue->NextChar >= Queue->End)
            Queue->NextChar = Queue->Start;

        returnVal = queueNoError;
    }

	IOLockUnlock(Queue->InUse);

    return( returnVal);
}

/*---------------------------------------------------------------------
*		PrivateGetBytetoQueue
* ejk		Sun, Jan 11, 1998	20:49
* 		Remove a byte of the circular queue.
*
*-------------------------------------------------------------------*/
static QueueStatus	PrivateGetBytetoQueue(CirQueue *Queue, u_char *Value) {

    QueueStatus returnVal = queueEmpty;

	IOLockLock(Queue->InUse);

    /* Check to see if the queue has something in it.
    */
    if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue) {
#ifdef TRACE
        KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL,0x0B)) | DBG_FUNC_NONE, (UInt)Queue, 0, 0, 0, 0); //0x0608002C
#endif
        returnVal = queueEmpty;
    }
    else {
        *Value = *Queue->LastChar++;
        Queue->InQueue--;

        /* Check to see if we need to wrap the pointer.
            */
        if (Queue->LastChar >= Queue->End)
            Queue->LastChar = Queue->Start;

#ifdef	ASSERT_ALL
        assert(Queue->InQueue >= 0 &&  Queue->InQueue <= kMaxCirBufferSize);
#endif

        returnVal = queueNoError;
    }

	IOLockUnlock(Queue->InUse);

    return( returnVal);
}


/*---------------------------------------------------------------------
*		InitQueue
* ejk		Sun, Jan 11, 1998	20:44
* 		Pass a buffer of memeory and this routine will set up the
*	internal data structures.
*
*-------------------------------------------------------------------*/
QueueStatus	InitQueue(CirQueue *Queue, u_char	*Buffer, size_t Size) {

    Queue->Start = Buffer;
    Queue->End = (u_char *)((size_t)Buffer + Size);
    Queue->Size = Size;
    Queue->NextChar = Buffer;
    Queue->LastChar = Buffer;
    Queue->InQueue = 0;
//rcs	Queue->InUse = mutex_alloc(ETAP_IO_AHA);
	Queue->InUse = IOLockAlloc();
    IOSleep(1);
    return(queueNoError);
}


/*---------------------------------------------------------------------
*		CloseQueue
* ejk		Sun, Jan 11, 1998	20:47
* 		Clear out all of the data structures.
*
*-------------------------------------------------------------------*/
QueueStatus	CloseQueue(CirQueue *Queue) {

    Queue->Start = (u_char *)0;
    Queue->End = (u_char *)0;
    Queue->NextChar = (u_char *)0;
    Queue->LastChar = (u_char *)0;
    Queue->Size = 0;
//rcs	mutex_free(Queue->InUse);
	IOLockFree(Queue->InUse);

    return(queueNoError);
}


/*---------------------------------------------------------------------
*		AddtoQueue
* ejk		Sun, Jan 11, 1998	22:07
* 		Add an entire buffer to the queue.
OPTIMIZE THIS
*
*-------------------------------------------------------------------*/
size_t	AddtoQueue(CirQueue	*Queue, u_char  *Buffer, size_t Size) {
    size_t	BytesWritten = 0;
#ifdef ASSERTS_ALL
    assert( Size<=kMaxCirBufferSize && Size!=0 );
#endif
    
    /* this makes the call atomic */
//    disable_preemption();
	

    while( FreeSpaceinQueue(Queue) && (Size > BytesWritten)) {
        PrivateAddBytetoQueue(Queue, *Buffer++);
        BytesWritten++;
    }
#ifdef ASSERTS_ALL
    assert( BytesWritten == Size );	//not disallowed but need to know if it happens
    assert(Queue->InQueue >= 0);
#endif

    /* end of the atomic code */
//    enable_preemption();

    return(BytesWritten);
}

/*---------------------------------------------------------------------
*		RemovefromQueue
* ejk		Sun, Jan 11, 1998	22:08
* 		Get a buffers worth of data from the queue.
OPTIMIZE THIS
*
*-------------------------------------------------------------------*/
size_t	RemovefromQueue(CirQueue	*Queue, u_char	*Buffer, size_t MaxSize) {
    size_t	BytesReceived = 0;
    u_char	Value;

    /* this makes the call atomic */
//    disable_preemption();

    while( (BytesReceived < MaxSize) && (PrivateGetBytetoQueue(Queue, &Value) == queueNoError)) {
        *Buffer++ = Value;
        BytesReceived++;
    }
#ifdef	ASSERTS_ALL
    assert(Queue->InQueue >= 0 &&  Queue->InQueue <= kMaxCirBufferSize);
#endif

    /* end of the atomic code */
//    enable_preemption();

    return( BytesReceived);
}

/*---------------------------------------------------------------------
*		FreeSpaceinQueue
* ejk		Sun, Jan 11, 1998	21:52
* 		Return the free space left in this buffer.
*
*-------------------------------------------------------------------*/
size_t	FreeSpaceinQueue(CirQueue *Queue) {
    size_t retVal;

    /* this makes the call atomic */
//    disable_preemption();
	IOLockLock(Queue->InUse);

    retVal = Queue->Size - Queue->InQueue;

    /* end of the atomic code */
//    enable_preemption();
	IOLockUnlock(Queue->InUse);

    return(retVal);
}

/*---------------------------------------------------------------------
*		UsedSpaceinQueue
* ejk		Sun, Jan 11, 1998	21:52
* 		Return the amount of data in this buffer.
*
*-------------------------------------------------------------------*/
size_t	UsedSpaceinQueue(CirQueue *Queue) 
{
//rcs Check for deadlock
	IOLockLock(Queue->InUse);
 
    size_t returnValue = (Queue->InQueue);
	
	IOLockUnlock(Queue->InUse);
	
	return returnValue;
}

/*---------------------------------------------------------------------
*		GetQueueSize
* ejk		Sun, Jan 11, 1998	22:25
* 		Return the total size of the queue.
*
*-------------------------------------------------------------------*/
size_t	GetQueueSize( CirQueue *Queue) {
    return(Queue->Size);
}

/*---------------------------------------------------------------------
*		AddBytetoQueue
* ejk		Sun, Jan 11, 1998	20:48
* 		Add a byte to the circular queue.
*
*-------------------------------------------------------------------*/
QueueStatus	AddBytetoQueue(CirQueue *Queue, char Value) {

    QueueStatus returnVal = queueFull;

    /* this makes the call atomic */
//    disable_preemption();

    returnVal = PrivateAddBytetoQueue(Queue, Value);
        
    /* end of the atomic code */
//    enable_preemption();

    return( returnVal);
}

/*---------------------------------------------------------------------
*		GetBytetoQueue
* ejk		Sun, Jan 11, 1998	20:49
* 		Remove a byte of the circular queue.
*
*-------------------------------------------------------------------*/
QueueStatus	GetBytetoQueue(CirQueue *Queue, u_char *Value) {

    QueueStatus returnVal = queueEmpty;

    /* this makes the call atomic */
//    disable_preemption();

    returnVal = PrivateGetBytetoQueue(Queue, Value);
    
    /* end of the atomic code */
//    enable_preemption();

    return( returnVal);
}

/*---------------------------------------------------------------------
*              GetQueueStatus
* ejk          Sun, Jan 11, 1998       20:49
*              Returns the status of the circular queue.
*
*-------------------------------------------------------------------*/
QueueStatus     GetQueueStatus(CirQueue *Queue) {

    QueueStatus returnVal = queueNoError;

    /* this makes the call atomic */
//    disable_preemption();
	IOLockLock(Queue->InUse);

    if ((Queue->NextChar == Queue->LastChar) && Queue->InQueue)
        returnVal = queueFull;
    else if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue)
        returnVal = queueEmpty;

    /* end of the atomic code */
//    enable_preemption();
	IOLockUnlock(Queue->InUse);

    return( returnVal);
}       
/*---------------------------------------------------------------------
*              BeginDirectReadFromQueue
*
*-------------------------------------------------------------------*/
u_char* BeginDirectReadFromQueue(CirQueue *Queue, size_t* size, Boolean* queueWrapped)
{
	u_char* queuePtr = NULL;
	
	/* this makes the call atomic */
//    disable_preemption();
	IOLockLock(Queue->InUse);
	
	*queueWrapped = false;

	if (Queue->InQueue)
	{
        *size = min(*size, Queue->InQueue);
		if ((Queue->LastChar + *size) >= Queue->End)
        {
		    *size = Queue->End - Queue->LastChar;
			*queueWrapped = true;
		}
		queuePtr = Queue->LastChar;
	}
	
    /* end of the atomic code */
//    enable_preemption();
	IOLockUnlock(Queue->InUse);

	return queuePtr;
}

/*---------------------------------------------------------------------
*              EndDirectReadFromQueue
*
*-------------------------------------------------------------------*/
void EndDirectReadFromQueue(CirQueue *Queue, size_t size)
{
	/* this makes the call atomic */
//    disable_preemption();
	IOLockLock(Queue->InUse);
	
	Queue->LastChar += size;
	Queue->InQueue -= size;

	/* Check to see if we need to wrap the pointer.
		*/
	if (Queue->LastChar >= Queue->End)
		Queue->LastChar = Queue->Start;

#ifdef	ASSERT_ALL
	assert(Queue->InQueue >= 0 &&  Queue->InQueue <= Queue->Size);
#endif
 
	   /* end of the atomic code */
//    enable_preemption();
	IOLockUnlock(Queue->InUse);

}
