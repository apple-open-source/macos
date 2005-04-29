/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>

#include "AppleRS232Serial.h"

//#include <kern/cpu_data.h>
//#include <sys/kdebug.h>
//#include <kern/thread.h>


#if USE_ELG
extern com_apple_iokit_XTrace	*gXTrace;
extern UInt32			gTraceID;
#endif


/****************************************************************************************************/
//
//		Function:	AddBytetoQueue
//
//		Inputs:		Queue - the queue to be added to
//				Value - Byte to be added
//
//		Outputs:	Queue status - full or no error
//
//		Desc:		Add a byte to the circular queue.
//				Check to see if there is space by comparing the next pointer,
//				with the last, If they match we are either Empty or full, so
//				check InQueue for zero.
//
/****************************************************************************************************/

QueueStatus AddBytetoQueue(CirQueue *Queue, char Value)
{
    ELG(Queue->InQueue, Queue->Workloop->inGate(), "AddBytetoQueue - InQueue, inGate");
    
//    mutex_lock(Queue->InUse);
    
    if ((Queue->NextChar == Queue->LastChar) && Queue->InQueue)
    {
//        mutex_unlock(Queue->InUse);
		ELG(Queue->NextChar, Queue->LastChar, "AddBytetoQueue - but queue is full!");
        return queueFull;
    }

    *Queue->NextChar++ = Value;
    Queue->InQueue++;

        // Check to see if we need to wrap the pointer.
		
    if (Queue->NextChar >= Queue->End)
        Queue->NextChar =  Queue->Start;

//    mutex_unlock(Queue->InUse);
    return queueNoError;
	
}/* end AddBytetoQueue */

/****************************************************************************************************/
//
//		Function:	GetBytetoQueue
//
//		Inputs:		Queue - the queue to be removed from
//
//		Outputs:	Value - where to put the byte
//				Queue status - empty or no error
//
//		Desc:		Remove a byte from the circular queue.
//
/****************************************************************************************************/

QueueStatus GetBytetoQueue(CirQueue *Queue, UInt8 *Value)
{
    ELG(Queue->InQueue, Queue->Workloop->inGate(), "GetBytetoQueue - InQueue, inGate");
	
//    mutex_lock(Queue->InUse);
    
    if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue)
    {
//        mutex_unlock(Queue->InUse);
        return queueEmpty;
    }

    *Value = *Queue->LastChar++;
    Queue->InQueue--;

        // Check to see if we need to wrap the pointer.
        
    if (Queue->LastChar >= Queue->End)
        Queue->LastChar =  Queue->Start;

//    mutex_unlock(Queue->InUse);
    return queueNoError;
	
}/* end GetBytetoQueue */

/****************************************************************************************************/
//
//		Function:	InitQueue
//
//		Inputs:		Queue - the queue to be initialized
//				Buffer - the buffer
//				size - length of buffer
//
//		Outputs:	Queue status - queueNoError.
//
//		Desc:		Pass a buffer of memory and this routine will set up the internal 
//				data structures.
//
/****************************************************************************************************/

QueueStatus InitQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size, IOWorkLoop *workloop)
{
    ELG(0, 0, "InitQueue");
    
    Queue->Start	= Buffer;
    Queue->End		= (UInt8*)((size_t)Buffer + Size);
    Queue->Size		= Size;
    Queue->NextChar	= Buffer;
    Queue->LastChar	= Buffer;
    Queue->InQueue	= 0;
//    Queue->InUse	= mutex_alloc(ETAP_IO_AHA);
    Queue->Workloop	= workloop;

    IOSleep(1);		// JDG: what is this doing here??
	
    return queueNoError;
	
}/* end InitQueue */

/****************************************************************************************************/
//
//		Function:	CloseQueue
//
//		Inputs:		Queue - the queue to be closed
//
//		Outputs:	Queue status - queueNoError.
//
//		Desc:		Clear out all of the data structures.
//
/****************************************************************************************************/

QueueStatus CloseQueue(CirQueue *Queue)
{
    ELG(0, 0, "CloseQueue");
    
    Queue->Start	= 0;
    Queue->End		= 0;
    Queue->NextChar	= 0;
    Queue->LastChar	= 0;
    Queue->Size		= 0;
    
//    mutex_free(Queue->InUse);

    return queueNoError;
	
}/* end CloseQueue */

/****************************************************************************************************/
//
//		Function:	ResetQueue
//
//		Inputs:		Queue - the queue to be reset
//
//		Outputs:	
//
//		Desc:		Make sure the queue pointers etc are all reset
//
/****************************************************************************************************/

void ResetQueue(CirQueue *Queue)
{
    ELG(Queue->InQueue, Queue->Workloop->inGate(), "ResetQueue - InQueue, inGate");

    Queue->NextChar	= Queue->Start;
    Queue->LastChar	= Queue->Start;
    Queue->InQueue	= 0;
	
}/* end InitQueue */

/****************************************************************************************************/
//
//		Function:	AddtoQueue
//
//		Inputs:		Queue - the queue to be added to
//				Buffer - data to add
//				Size - length of data
//
//		Outputs:	BytesWritten - Number of bytes actually put in the queue.
//
//		Desc:		Add an entire buffer to the queue.
//
/****************************************************************************************************/

size_t AddtoQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size)
{
    size_t	BytesWritten = 0;

    ELG(Queue->InQueue, Queue->Workloop->inGate(), "AddtoQueue - InQueue, inGate");

    while (FreeSpaceinQueue(Queue) && (Size > BytesWritten))
    {
        AddBytetoQueue(Queue, *Buffer++);
        BytesWritten++;
    }
    LogData(kSerialIn, BytesWritten, Buffer - BytesWritten);
    return BytesWritten;
	
}/* end AddtoQueue */

/****************************************************************************************************/
//
//		Function:	RemovefromQueue
//
//		Inputs:		Queue - the queue to be removed from
//				Size - size of buffer
//
//		Outputs:	Buffer - Where to put the data
//				BytesReceived - Number of bytes actually put in Buffer
//
//		Desc:		Get a buffers worth of data from the queue.
//
/****************************************************************************************************/

size_t RemovefromQueue(CirQueue *Queue, UInt8 *Buffer, size_t MaxSize)
{
    size_t	BytesReceived = 0;
    UInt8	Value;

    ELG(Queue->InQueue, Queue->Workloop->inGate(), "RemovefromQueue - InQueue, inGate");

    //  while((GetBytetoQueue(Queue, &Value) == queueNoError) && (MaxSize >= BytesReceived))
    while((MaxSize > BytesReceived) && (GetBytetoQueue(Queue, &Value) == queueNoError)) 
    {
        *Buffer++ = Value;
        BytesReceived++;
    }/* end while */

    return BytesReceived;
	
}/* end RemovefromQueue */

/****************************************************************************************************/
//
//		Function:	FreeSpaceinQueue
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	Return Value - Free space left
//
//		Desc:		Return the amount of free space left in this queue.
//
/****************************************************************************************************/

size_t FreeSpaceinQueue(CirQueue *Queue)
{
    size_t	retVal = 0;

    ELG(Queue->InQueue, Queue->Workloop->inGate(), "FreeSpaceinQueue - InQueue, inGate");
    
//    mutex_lock(Queue->InUse);
    
    retVal = Queue->Size - Queue->InQueue;

//    mutex_unlock(Queue->InUse);
    
    return retVal;
	
}/* end FreeSpaceinQueue */

/****************************************************************************************************/
//
//		Function:	UsedSpaceinQueue
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	UsedSpace - Amount of data in queue
//
//		Desc:		Return the amount of data in this queue.
//
/****************************************************************************************************/

size_t UsedSpaceinQueue(CirQueue *Queue)
{
    size_t	returnVal;

    ELG(Queue->InQueue, Queue->Workloop->inGate(), "UsedSpaceinQueue - InQueue, inGate");

//    mutex_lock(Queue->InUse);
    
    returnVal = Queue->InQueue;
    
//    mutex_unlock(Queue->InUse);
    
    return returnVal;
	
}/* end UsedSpaceinQueue */

/****************************************************************************************************/
//
//		Function:	GetQueueSize
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	QueueSize - The size of the queue.
//
//		Desc:		Return the total size of the queue.
//
/****************************************************************************************************/

size_t GetQueueSize(CirQueue *Queue)
{
    ELG(Queue->InQueue, Queue->Workloop->inGate(), "GetQueueSize - InQueue, inGate");

    return Queue->Size;
	
}/* end GetQueueSize */

/****************************************************************************************************/
//
//		Function:	GetQueueStatus
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	Queue status - full, empty or no error
//
//		Desc:		Returns the status of the circular queue.
//
/****************************************************************************************************/

QueueStatus GetQueueStatus(CirQueue *Queue)
{
    QueueStatus	returnVal = queueNoError;
    
    ELG(Queue->InQueue, Queue->Workloop->inGate(), "GetQueueStatus - InQueue, inGate");

    //    mutex_lock(Queue->InUse);
    
    if ((Queue->NextChar == Queue->LastChar) && Queue->InQueue)
    {
        returnVal = queueFull;
    } else {
        if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue)
        {
            returnVal = queueEmpty;
        }
    }
	
//    mutex_unlock(Queue->InUse);
    return returnVal ;
	
}/* end GetQueueStatus */

/****************************************************************************************************/
//
//		Function:	BeginDirectReadFromQueue
//
//		Inputs:		Queue - the queue to be read from
//				Size - size of data (updated)
//
//		Outputs:	queueWrapped - true(queue wrapped), false(queue didn't)
//				Queue pointer - to the last character
//
//		Desc:		Begins reading directly from the circular queue.
//
/****************************************************************************************************/

UInt8* BeginDirectReadFromQueue(CirQueue *Queue, size_t *size, bool *queueWrapped)
{
    UInt8	*queuePtr = NULL;

    ELG(Queue->InQueue, Queue->Workloop->inGate(), "BeginDirectReadFromQueue - InQueue, inGate");

    //    mutex_lock(Queue->InUse);
	
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
	
//    mutex_unlock(Queue->InUse);
    return queuePtr;
    
}/* end BeginDirectReadFromQueue */

/****************************************************************************************************/
//
//		Function:	EndDirectReadFromQueue
//
//		Inputs:		Queue - the queue to be read from
//				Size - size of data
//
//		Outputs:	
//
//		Desc:		Ends the direct read from the circular queue.
//
/****************************************************************************************************/

void EndDirectReadFromQueue(CirQueue *Queue, size_t size)
{

    ELG(Queue->InQueue, Queue->Workloop->inGate(), "EndDirectReadFromQueue - InQueue, inGate");

    //    mutex_lock(Queue->InUse);
	
    Queue->LastChar += size;
    Queue->InQueue -= size;

        // Check to see if we need to wrap the pointer.
        
    if (Queue->LastChar >= Queue->End)
        Queue->LastChar = Queue->Start;

//    mutex_unlock(Queue->InUse);

}/* end EndDirectReadFromQueue */
