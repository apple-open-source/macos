/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

    /* AppleUSBCDCACMData.cpp - MacOSX implementation of		*/
    /* USB Communication Device Class (CDC) Driver, ACM Data Interface.	*/

#include <machine/limits.h>			/* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/pwr_mgt/RootDomain.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/IOSerialDriverSync.h>
#include <IOKit/serial/IOModemSerialStreamSync.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>

#include <UserNotification/KUNCUserNotifications.h>

#define DEBUG_NAME "AppleUSBCDCACMData"

#include "AppleUSBCDCACM.h"
#include "AppleUSBCDCACMData.h"

#define MIN_BAUD (50 << 1)

    // Globals

#if USE_ELG
    com_apple_iokit_XTrace	*gXTrace = 0;
#endif

//AppleUSBCDCACMControl		*gControlDriver = NULL;			// Our Control driver

#define super IOSerialDriverSync

OSDefineMetaClassAndStructors(AppleUSBCDCACMData, IOSerialDriverSync);

#if USE_ELG
/****************************************************************************************************/
//
//		Function:	findKernelLoggerAD
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Just like the name says
//
/****************************************************************************************************/

IOReturn findKernelLoggerAD()
{
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    IOReturn		error = 0;
	
	// Get matching dictionary
	
    matchingDictionary = IOService::serviceMatching("com_apple_iokit_XTrace");
    if (!matchingDictionary)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerAD] Couldn't create a matching dictionary.\n");
        goto exit;
    }
	
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerAD] No XTrace logger found.\n");
        goto exit;
    }
	
	// Use iterator to find each com_apple_iokit_XTrace instance. There should be only one, so we
	// won't iterate
	
    gXTrace = (com_apple_iokit_XTrace*)iterator->getNextObject();
    if (gXTrace)
    {
        IOLog(DEBUG_NAME "[findKernelLoggerAD] Found XTrace logger at %p.\n", gXTrace);
    }
	
exit:
	
    if (error != kIOReturnSuccess)
    {
        gXTrace = NULL;
        IOLog(DEBUG_NAME "[findKernelLoggerAD] Could not find a logger instance. Error = %X.\n", error);
    }
	
    if (matchingDictionary)
        matchingDictionary->release();
            
    if (iterator)
        iterator->release();
		
    return error;
    
}/* end findKernelLoggerAD */
#endif

/****************************************************************************************************/
//
//		Function:	findCDCDriverAD
//
//		Inputs:		dataAddr - my address
//				dataInterfaceNum - the data interface number
//
//		Outputs:	
//
//		Desc:		Finds the initiating CDC driver and confirms the interface number
//
/****************************************************************************************************/

IOReturn findCDCDriverAD(void *dataAddr, UInt8 dataInterfaceNum)
{
    AppleUSBCDCACMData	*me = (AppleUSBCDCACMData *)dataAddr;
    AppleUSBCDC		*CDCDriver = NULL;
    bool		driverOK = false;
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    
    XTRACE(me, 0, 0, "findCDCDriverAD");
        
        // Get matching dictionary
       	
    matchingDictionary = IOService::serviceMatching("AppleUSBCDC");
    if (!matchingDictionary)
    {
        XTRACE(me, 0, 0, "findCDCDriverAD - Couldn't create a matching dictionary");
        return kIOReturnError;
    }
    
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        XTRACE(me, 0, 0, "findCDCDriverAD - No AppleUSBCDC driver found!");
        matchingDictionary->release();
        return kIOReturnError;
    }

#if 0    
	// Use iterator to find driver (there's only one so we won't bother to iterate)
                
    CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    if (CDCDriver)
    {
        driverOK = CDCDriver->confirmDriver(kUSBAbstractControlModel, dataInterfaceNum);
    }

    matchingDictionary->release();
    iterator->release();
#endif
    
    	// Iterate until we find our matching CDC driver
                
    CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    while (CDCDriver)
    {
        XTRACE(me, 0, CDCDriver, "findCDCDriverAD - CDC driver candidate");
        
        if (me->fDataInterface->GetDevice() == CDCDriver->getCDCDevice())
        {
            XTRACE(me, 0, CDCDriver, "findCDCDriverAD - Found our CDC driver");
            driverOK = CDCDriver->confirmDriver(kUSBAbstractControlModel, dataInterfaceNum);
            break;
        }
        CDCDriver = (AppleUSBCDC *)iterator->getNextObject();
    }

    matchingDictionary->release();
    iterator->release();
    
    if (!CDCDriver)
    {
        XTRACE(me, 0, 0, "findCDCDriverAD - CDC driver not found");
        return kIOReturnError;
    }
   
    if (!driverOK)
    {
        XTRACE(me, kUSBAbstractControlModel, dataInterfaceNum, "findCDCDriverAD - Not my interface");
        return kIOReturnError;
    }
    
    me->fConfigAttributes = CDCDriver->fbmAttributes;

    return kIOReturnSuccess;
    
}/* end findCDCDriverAD */

/****************************************************************************************************/
//
//		Function:	findControlDriverAD
//
//		Inputs:		me - my address
//
//		Outputs:	AppleUSBCDCACMControl
//
//		Desc:		Finds our matching control driver
//
/****************************************************************************************************/

AppleUSBCDCACMControl *findControlDriverAD(void *me)
{
    Boolean                      worked             = false;
    AppleUSBCDCACMControl	*tempDriver         = NULL;
    OSIterator			*iterator           = NULL;
    OSDictionary		*matchingDictionary = NULL;
    
    XTRACE(me, 0, 0, "findControlDriverAD");
    
        // Get matching dictionary
       	
    matchingDictionary = IOService::serviceMatching("AppleUSBCDCACMControl");
    if (!matchingDictionary)
    {
        XTRACE(me, 0, 0, "findControlDriverAD - Couldn't create a matching dictionary");
        return NULL;
    }
    
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        XTRACE(me, 0, 0, "findControlDriverAD - No AppleUSBCDCACMControl drivers found (iterator)");
        matchingDictionary->release();
        return NULL;
    }
    
	// Iterate until we find our matching driver
                
    tempDriver = (AppleUSBCDCACMControl *)iterator->getNextObject();
    while (tempDriver)
    {
        XTRACE(me, 0, tempDriver, "findControlDriverAD - Data driver candidate");
        if (tempDriver->checkInterfaceNumber((AppleUSBCDCACMData *)me))
        {
            XTRACE(me, 0, tempDriver, "findControlDriverAD - Found our data driver");
            worked = true;
            break;
        }
        tempDriver = (AppleUSBCDCACMControl *)iterator->getNextObject();
    }

    matchingDictionary->release();
    iterator->release();

    if (worked)
        return tempDriver;
    else
    {
        XTRACE(me, 0, 0, "findControlDriverAD - Failed");
        return NULL;
    }
}/* end findControlDriverAD */

#if LOG_DATA
#define dumplen		32		// Set this to the number of bytes to dump and the rest should work out correct

#define buflen		((dumplen*2)+dumplen)+3
#define Asciistart	(dumplen*2)+3

/****************************************************************************************************/
//
//		Function:	USBLogData
//
//		Inputs:		Dir - direction
//				Count - number of bytes
//				buf - the data
//
//		Outputs:	
//
//		Desc:		Puts the data in the log. 
//
/****************************************************************************************************/

void AppleUSBCDCACMData::USBLogData(UInt8 Dir, UInt32 Count, char *buf)
{    
    SInt32	wlen;
#if USE_ELG
    UInt8 	*b;
    UInt8 	w[8];
#else
    UInt32	llen, rlen;
    UInt16	i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];
#endif
    
    switch (Dir)
    {
        case kDataIn:
#if USE_ELG
            XTRACE2(this, buf, Count, "USBLogData - Read Complete, address, size");
#else
            IOLog( "AppleUSBCDCACMData: USBLogData - Read Complete, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kDataOut:
#if USE_ELG
            XTRACE2(this, buf, Count, "USBLogData - Write, address, size");
#else
            IOLog( "AppleUSBCDCACMData: USBLogData - Write, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kDataOther:
#if USE_ELG
            XTRACE2(this, buf, Count, "USBLogData - Other, address, size");
#else
            IOLog( "AppleUSBCDCACMData: USBLogData - Other, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
    }

#if DUMPALL
    wlen = Count;
#else
    if (Count > dumplen)
    {
        wlen = dumplen;
    } else {
        wlen = Count;
    }
#endif

    if (wlen == 0)
    {
#if USE_ELG
        XTRACE2(this, 0, Count, "USBLogData - No data, Count=0");
#else
        IOLog( "AppleUSBCDCACMData: USBLogData - No data, Count=0\n" );
#endif
        return;
    }

#if (USE_ELG)
    b = (UInt8 *)buf;
    while (wlen > 0)							// loop over the buffer
    {
        bzero(w, sizeof(w));						// zero it
        bcopy(b, w, min(wlen, 8));					// copy bytes over
    
        switch (Dir)
        {
            case kDataIn:
                XTRACE2(this, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Rx buffer dump");
                break;
            case kDataOut:
                XTRACE2(this, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Tx buffer dump");
                break;
            case kDataOther:
                XTRACE2(this, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Misc buffer dump");
                break;
        }
        wlen -= 8;							// adjust by 8 bytes for next time (if have more)
        b += 8;
    }
#else
    rlen = 0;
    do
    {
        for (i=0; i<=buflen; i++)
        {
            LocBuf[i] = 0x20;
        }
        LocBuf[i] = 0x00;
        
        if (wlen > dumplen)
        {
            llen = dumplen;
            wlen -= dumplen;
        } else {
            llen = wlen;
            wlen = 0;
        }
        Aspnt = Asciistart;
        Hxpnt = 0;
        for (i=1; i<=llen; i++)
        {
            wchr = buf[i-1];
            LocBuf[Hxpnt++] = Asciify(wchr >> 4);
            LocBuf[Hxpnt++] = Asciify(wchr);
            if ((wchr < 0x20) || (wchr > 0x7F)) 		// Non printable characters
            {
                LocBuf[Aspnt++] = 0x2E;				// Replace with a period
            } else {
                LocBuf[Aspnt++] = wchr;
            }
        }
        LocBuf[(llen + Asciistart) + 1] = 0x00;
        IOLog(LocBuf);
        IOLog("\n");
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
       
        rlen += llen;
        buf = &buf[rlen];
    } while (wlen != 0);
#endif 

}/* end USBLogData */
#endif

/****************************************************************************************************/
//
//		Method:		AddBytetoQueue
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

QueueStatus AppleUSBCDCACMData::AddBytetoQueue(CirQueue *Queue, char Value)
{
    
    if ((Queue->NextChar == Queue->LastChar) && Queue->InQueue)
    {
        return queueFull;
    }

    *Queue->NextChar++ = Value;
    Queue->InQueue++;

        // Check to see if we need to wrap the pointer.
		
    if (Queue->NextChar >= Queue->End)
        Queue->NextChar =  Queue->Start;

    return queueNoError;
	
}/* end AddBytetoQueue */

/****************************************************************************************************/
//
//		Method:		GetBytetoQueue
//
//		Inputs:		Queue - the queue to be removed from
//
//		Outputs:	Value - where to put the byte
//				QueueStatus - empty or no error
//
//		Desc:		Remove a byte from the circular queue.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCACMData::GetBytetoQueue(CirQueue *Queue, UInt8 *Value)
{
    
    if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue)
    {
        return queueEmpty;
    }

    *Value = *Queue->LastChar++;
    Queue->InQueue--;

        // Check to see if we need to wrap the pointer.
        
    if (Queue->LastChar >= Queue->End)
        Queue->LastChar =  Queue->Start;

    return queueNoError;
	
}/* end GetBytetoQueue */

/****************************************************************************************************/
//
//		Method:		InitQueue
//
//		Inputs:		Queue - the queue to be initialized
//				Buffer - the buffer
//				size - length of buffer
//
//		Outputs:	QueueStatus - queueNoError.
//
//		Desc:		Pass a buffer of memory and this routine will set up the internal data structures.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCACMData::InitQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size)
{
    Queue->Start	= Buffer;
    Queue->End		= (UInt8*)((size_t)Buffer + Size);
    Queue->Size		= Size;
    Queue->NextChar	= Buffer;
    Queue->LastChar	= Buffer;
    Queue->InQueue	= 0;

    IOSleep(1);
	
    return queueNoError ;
	
}/* end InitQueue */

/****************************************************************************************************/
//
//		Method:		CloseQueue
//
//		Inputs:		Queue - the queue to be closed
//
//		Outputs:	QueueStatus - queueNoError.
//
//		Desc:		Clear out all of the data structures.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCACMData::CloseQueue(CirQueue *Queue)
{

    Queue->Start	= 0;
    Queue->End		= 0;
    Queue->NextChar	= 0;
    Queue->LastChar	= 0;
    Queue->Size		= 0;

    return queueNoError;
	
}/* end CloseQueue */

/****************************************************************************************************/
//
//		Method:		AddtoQueue
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

size_t AppleUSBCDCACMData::AddtoQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size)
{
    size_t	BytesWritten = 0;

    while (FreeSpaceinQueue(Queue) && (Size > BytesWritten))
    {
        AddBytetoQueue(Queue, *Buffer++);
        BytesWritten++;
    }

    return BytesWritten;
	
}/* end AddtoQueue */

/****************************************************************************************************/
//
//		Method:		RemovefromQueue
//
//		Inputs:		Queue - the queue to be removed from
//				Size - size of buffer
//
//		Outputs:	Buffer - Where to put the data
//				BytesReceived - Number of bytes actually put in Buffer.
//
//		Desc:		Get a buffers worth of data from the queue.
//
/****************************************************************************************************/

size_t AppleUSBCDCACMData::RemovefromQueue(CirQueue *Queue, UInt8 *Buffer, size_t MaxSize)
{
    size_t	BytesReceived = 0;
    UInt8	Value;

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
//		Method:		FreeSpaceinQueue
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	Return Value - Free space left
//
//		Desc:		Return the amount of free space left in this buffer.
//
/****************************************************************************************************/

size_t AppleUSBCDCACMData::FreeSpaceinQueue(CirQueue *Queue)
{
    size_t	retVal = 0;
    
    retVal = Queue->Size - Queue->InQueue;

    return retVal;
	
}/* end FreeSpaceinQueue */

/****************************************************************************************************/
//
//		Method:		UsedSpaceinQueue
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	UsedSpace - Amount of data in buffer
//
//		Desc:		Return the amount of data in this buffer.
//
/****************************************************************************************************/

size_t AppleUSBCDCACMData::UsedSpaceinQueue(CirQueue *Queue)
{
    return Queue->InQueue;
	
}/* end UsedSpaceinQueue */

/****************************************************************************************************/
//
//		Method:		GetQueueSize
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	QueueSize - The size of the queue.
//
//		Desc:		Return the total size of the queue.
//
/****************************************************************************************************/

size_t AppleUSBCDCACMData::GetQueueSize(CirQueue *Queue)
{
    return Queue->Size;
	
}/* end GetQueueSize */

/****************************************************************************************************/
//
//		Method:		GetQueueStatus
//
//		Inputs:		Queue - the queue to be queried
//
//		Outputs:	Queue status - full, empty or no error
//
//		Desc:		Returns the status of the circular queue.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCACMData::GetQueueStatus(CirQueue *Queue)
{
    if ((Queue->NextChar == Queue->LastChar) && Queue->InQueue)
        return queueFull;
    else if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue)
        return queueEmpty;
		
    return queueNoError ;
	
}/* end GetQueueStatus */

/****************************************************************************************************/
//
//		Method:		CheckQueues
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Checks the various queue's etc and manipulates the state(s) accordingly
//				Must be called from a gated method or completion routine.
//
/****************************************************************************************************/

void AppleUSBCDCACMData::CheckQueues()
{
    unsigned long	Used;
    unsigned long	Free;
    unsigned long	QueuingState;
    unsigned long	DeltaState;

	// Initialise the QueueState with the current state.
        
    QueuingState = fPort.State;

        // Check to see if there is anything in the Transmit buffer.
        
    Used = UsedSpaceinQueue(&fPort.TX);
    Free = FreeSpaceinQueue(&fPort.TX);
    
    XTRACE(this, Free, Used, "CheckQueues");
    
    if (Free == 0)
    {
        QueuingState |=  PD_S_TXQ_FULL;
        QueuingState &= ~PD_S_TXQ_EMPTY;
    } else {
        if (Used == 0)
	{
            QueuingState &= ~PD_S_TXQ_FULL;
            QueuingState |=  PD_S_TXQ_EMPTY;
        } else {
            QueuingState &= ~PD_S_TXQ_FULL;
            QueuingState &= ~PD_S_TXQ_EMPTY;
        }
    }

    	// Check to see if we are below the low water mark.
        
    if (Used < fPort.TXStats.LowWater)
         QueuingState |=  PD_S_TXQ_LOW_WATER;
    else QueuingState &= ~PD_S_TXQ_LOW_WATER;

    if (Used > fPort.TXStats.HighWater)
         QueuingState |= PD_S_TXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_TXQ_HIGH_WATER;


        // Check to see if there is anything in the Receive buffer.
        
    Used = UsedSpaceinQueue(&fPort.RX);
    Free = FreeSpaceinQueue(&fPort.RX);

    if (Free == 0)
    {
        QueuingState |= PD_S_RXQ_FULL;
        QueuingState &= ~PD_S_RXQ_EMPTY;
    } else {
        if (Used == 0)
	{
            QueuingState &= ~PD_S_RXQ_FULL;
            QueuingState |= PD_S_RXQ_EMPTY;
        } else {
            QueuingState &= ~PD_S_RXQ_FULL;
            QueuingState &= ~PD_S_RXQ_EMPTY;
        }
    }

        // Check to see if we are below the low water mark.
    
    if (Used < fPort.RXStats.LowWater)
         QueuingState |= PD_S_RXQ_LOW_WATER;
    else QueuingState &= ~PD_S_RXQ_LOW_WATER;

    if (Used > fPort.RXStats.HighWater)
         QueuingState |= PD_S_RXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_RXQ_HIGH_WATER;

        // Figure out what has changed to get mask.
        
    DeltaState = QueuingState ^ fPort.State;
    setStateGated(QueuingState, DeltaState);
	
}/* end CheckQueues */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::dataReadComplete
//
//		Inputs:		obj - me
//				param - the buffer pool pointer
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkIn pipe read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCACMData::dataReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCACMData	*me = (AppleUSBCDCACMData*)obj;
    inPipeBuffers		*buffs = (inPipeBuffers *)param;
    IOReturn		ior;
    size_t		length;
    
    XTRACE(me, rc, 0, "dataReadComplete");
    
    if (me->fStopping)
        return;

    if (rc == kIOReturnSuccess)				// If operation returned ok
    {
        length = DATA_BUFF_SIZE - remaining;
        XTRACE(me, me->fPort.State, length, "dataReadComplete - data length");
		
        meLogData(kDataIn, length, buffs->pipeBuffer);
	
            // Move the incoming bytes to the ring buffer
            
        me->AddtoQueue(&me->fPort.RX, buffs->pipeBuffer, length);
        
        me->CheckQueues();
        
    } else {
        XTRACE(me, 0, rc, "dataReadComplete - error");
		if (rc != kIOReturnAborted)
        {
			if (rc == kIOUSBPipeStalled)
			{
				rc = me->checkPipe(me->fPort.InPipe, true);
			} else {
				rc = me->checkPipe(me->fPort.InPipe, false);
			}
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataReadComplete - clear stall failed (trying to continue)");
            }
        }

    }
    
        // Queue the read only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fPort.InPipe->Read(buffs->pipeMDP, &buffs->completionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(me, 0, rc, "dataReadComplete - Read io err");
			buffs->dead = true;
        }
    }
	
}/* end dataReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::dataWriteComplete
//
//		Inputs:		obj - me
//				param - the buffer pool pointer
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkOut pipe write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCACMData::dataWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCACMData	*me = (AppleUSBCDCACMData *)obj;
    outPipeBuffers		*buffs = (outPipeBuffers *)param;
    SInt32		dLen;
    UInt16		i;
    bool		busy = false;
    
    XTRACE(me, rc, 0, "dataWriteComplete");
    
    if (me->fStopping)
        return;

    if (rc == kIOReturnSuccess)
    {	
        dLen = buffs->count - remaining;
        XTRACE(me, 0, dLen, "dataWriteComplete - data length");
        if (dLen > 0)						// Check if it was a zero length write
        {
            if ((dLen % me->fPort.OutPacketSize) == 0)		// If it was a multiple of max packet size then we need to do a zero length write
            {
                XTRACE(me, rc, dLen, "dataWriteComplete - writing zero length packet");
                buffs->count = 0;
                buffs->pipeMDP->setLength(0);
                
                me->fPort.OutPipe->Write(buffs->pipeMDP, &buffs->completionInfo);
                return;
            } else {
                buffs->avail = true;
            }
        } else {
            buffs->avail = true;
        }

        me->CheckQueues();
        
            // If any of the buffers are unavailable then we're still busy

        for (i=0; i<me->fOutBufPool; i++)
        {
            if (!me->fPort.outPool[i].avail)
            {
                busy = true;
                break;
            }
        }

        if (!busy)
        {
            me->setStateGated(0, PD_S_TX_BUSY);
        }

        me->setUpTransmit();						// just to keep it going??

    } else {
        XTRACE(me, 0, rc, "dataWriteComplete - io error");
		if (rc != kIOReturnAborted)
        {
			if (rc == kIOUSBPipeStalled)
			{
				rc = me->checkPipe(me->fPort.InPipe, true);
			} else {
				rc = me->checkPipe(me->fPort.InPipe, false);
			}
            if (rc != kIOReturnSuccess)
            {
                XTRACE(me, 0, rc, "dataWriteComplete - clear stall failed (trying to continue)");
            }
        }

        
        buffs->avail = true;
        
             // If any of the buffers are unavailable then we're still busy

        for (i=0; i<me->fOutBufPool; i++)
        {
            if (!me->fPort.outPool[i].avail)
            {
                busy = true;
                break;
            }
        }

        if (!busy)
        {
            me->setStateGated(0, PD_S_TX_BUSY);
        }
    }
	
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has been determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleUSBCDCACMData::start(IOService *provider)
{
    OSNumber		*bufNumber = NULL;
    UInt16		bufValue = 0;
    
    fSessions = 0;
    fTerminate = false;
    fStopping = false;
    
    initStructure();
    
#if USE_ELG
    XTraceLogInfo	*logInfo;
    
    findKernelLoggerAD();
    if (gXTrace)
    {
        gXTrace->retain();		// don't let it unload ...
        XTRACE(this, 0, 0xbeefbeef, "Hello from start");
        logInfo = gXTrace->LogGetInfo();
        IOLog("AppleUSBCDCACMData: start - Log is at %x\n", (unsigned int)logInfo);
    } else {
        return false;
    }
#endif

    XTRACE(this, 0, provider, "start - provider.");
    
    if(!super::start(provider))
    {
        ALERT(0, 0, "start - super failed");
        return false;
    }

	// Get my USB provider - the interface

    fDataInterface = OSDynamicCast(IOUSBInterface, provider);
    if(!fDataInterface)
    {
        ALERT(0, 0, "start - provider invalid");
        return false;
    }
    
    // If our IOUSBInterface has a "do not match" property, it means that we should not match and need 
    // to bail.  See rdar://3716623
    
    OSBoolean * boolObj = OSDynamicCast( OSBoolean, provider->getProperty("kDoNotClassMatchThisInterface") );
    if ( boolObj && boolObj->isTrue() )
    {
        ALERT(0, 0, "start - provider doesn't want us to match");
        return false;
    }

    fPort.DataInterfaceNumber = fDataInterface->GetInterfaceNumber();
    
    if (findCDCDriverAD(this, fPort.DataInterfaceNumber) != kIOReturnSuccess)
    {
        XTRACE(this, 0, 0, "start - Find CDC driver failed");
        return false;
    }
    
        // get workloop
        
    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
    {
        ALERT(0, 0, "start - getWorkLoop failed");
        return false;
    }
    
    fCommandGate = IOCommandGate::commandGate(this);
    if (!fCommandGate)
    {
        ALERT(0, 0, "start - commandGate failed");
        return false;
    }
    
    if (fWorkLoop->addEventSource(fCommandGate) != kIOReturnSuccess)
    {
        ALERT(0, 0, "start - addEventSource(commandGate) failed");
        return false;
    }
    
         // Set up the values for the input buffer pool
    
    bufNumber = (OSNumber *)getProperty(inputTag);
    if (bufNumber)
    {
        bufValue = bufNumber->unsigned16BitValue();
        XTRACE(this, 0, bufValue, "start - Number of input buffers requested");
        if (bufValue <= kMaxInBufPool)
        {
            fInBufPool = bufValue;
        } else {
            fInBufPool = kMaxInBufPool;
        }
    } else {
        fInBufPool = kInBufPool;
    }
    
        // Set up the values for the output buffer pool
    
    bufNumber = NULL;
    bufNumber = (OSNumber *)getProperty(outputTag);
    if (bufNumber)
    {
        bufValue = bufNumber->unsigned16BitValue();
        XTRACE(this, 0, bufValue, "start - Number of output buffers requested");
        if (bufValue <= kMaxOutBufPool)
        {
            fOutBufPool = bufValue;
        } else {
            fOutBufPool = kMaxOutBufPool;
        }
    } else {
        fOutBufPool = kOutBufPool;
    }
    
    XTRACE(this, fInBufPool, fOutBufPool, "start - Buffer pools (input, output)");

    if (!createSerialStream())					// Publish SerialStream services
    {
        ALERT(0, 0, "start - createSerialStream failed");
        return false;
    }
    
    if (!allocateResources()) 
    {
        ALERT(0, 0, "start - allocateResources failed");
        return false;
    }
            
        // Looks like we're ok
    
    fDataInterface->retain();
    fWorkLoop->retain();
    fCommandGate->enable();

	if (fConfigAttributes & kUSBAtrRemoteWakeup)
    {
        getPMRootDomain()->publishFeature("WakeOnRing");
		setWakeFeature();
	} else {
        XTRACE(this, 0, 0, "start - Remote wake up not supported");
    }

    XTRACE(this, 0, 0, "start - successful and IOModemSerialStreamSync created");
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCACMData::stop(IOService *provider)
{
    IOReturn	ret;
    
    XTRACE(this, 0, 0, "stop");
    
    fStopping = true;
    
    retain();
    ret = fCommandGate->runAction(stopAction);
    release();
        
    removeProperty((const char *)propertyTag);
    
    super::stop(provider);

}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::stopAction
//
//		Desc:		Dummy pass through for stopGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::stopAction(OSObject *owner, void *, void *, void *, void *)
{

    ((AppleUSBCDCACMData *)owner)->stopGated();
    
    return kIOReturnSuccess;
    
}/* end stopAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::stopGated
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Releases the resources 
//
/****************************************************************************************************/

void AppleUSBCDCACMData::stopGated()
{
    
    XTRACE(this, 0, 0, "stopGated");
    
    releaseResources();
	
}/* end stopGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::createSuffix
//
//		Inputs:		
//
//		Outputs:	return Code - true (suffix created), false (suffix not create)
//				sufKey - the key
//
//		Desc:		Creates the suffix key. It attempts to use the serial number string from the device
//				if it's reasonable i.e. less than 8 bytes ascii. Remember it's stored in unicode 
//				format. If it's not present or not reasonable it will generate the suffix based 
//				on the location property tag. At least this remains the same across boots if the
//				device is plugged into the same physical location. In the latter case trailing
//				zeros are removed.
//				The interface number is also added to make it unique for
//				multiple CDC configuration devices.
//
/****************************************************************************************************/

bool AppleUSBCDCACMData::createSuffix(unsigned char *sufKey)
{
    
    IOReturn	rc;
    UInt8	serBuf[12];		// arbitrary size > 8
    OSNumber	*location;
    UInt32	locVal;
    UInt8	*rlocVal;
    UInt16	offs, i, sig = 0;
    UInt8	indx;
    bool	keyOK = false;			
	
    XTRACE(this, 0, 0, "createSuffix");
	
    indx = fDataInterface->GetDevice()->GetSerialNumberStringIndex();	
    if (indx != 0)
    {	
            // Generate suffix key based on the serial number string (if reasonable <= 8 and > 0)	

        rc = fDataInterface->GetDevice()->GetStringDescriptor(indx, (char *)&serBuf, sizeof(serBuf));
        if (!rc)
        {
            if ((strlen((char *)&serBuf) < 9) && (strlen((char *)&serBuf) > 0))
            {
                strcpy((char *)sufKey, (const char *)&serBuf);
                sig = strlen((char *)sufKey);
                keyOK = true;
            }			
        } else {
            XTRACE(this, 0, rc, "createSuffix error reading serial number string");
        }
    }
	
    if (!keyOK)
    {
            // Generate suffix key based on the location property tag
	
        location = (OSNumber *)fDataInterface->GetDevice()->getProperty(kUSBDevicePropertyLocationID);
        if (location)
        {
            locVal = location->unsigned32BitValue();		
            offs = 0;
            rlocVal = (UInt8*)&locVal;
            for (i=0; i<4; i++)
            {
                sufKey[offs] = Asciify(rlocVal[i] >> 4);
                if (sufKey[offs++] != '0')
                    sig = offs;
                sufKey[offs] = Asciify(rlocVal[i]);
                if (sufKey[offs++] != '0')
                    sig = offs;
            }
            keyOK = true;
        }
    }
    
        // Make it unique just in case there's more than one CDC configuration on this device
    
    if (keyOK)
    {
        sufKey[sig] = Asciify((UInt8)fPort.DataInterfaceNumber >> 4);
        if (sufKey[sig] != '0')
            sig++;
        sufKey[sig] = Asciify((UInt8)fPort.DataInterfaceNumber);
        if (sufKey[sig] != '0')
            sig++;			
        sufKey[sig] = 0x00;
    }
	
    return keyOK;

}/* end createSuffix */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::createSerialStream
//
//		Inputs:		
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the nub
//
/****************************************************************************************************/

bool AppleUSBCDCACMData::createSerialStream()
{
    IOModemSerialStreamSync	*pNub = new IOModemSerialStreamSync;
    bool			ret;
    UInt8			indx;
    IOReturn			rc;
    unsigned char		rname[20];
    const char			*suffix = (const char *)&rname;
	
    XTRACE(this, 0, pNub, "createSerialStream");
    if (!pNub)
    {
        return false;
    }
		
    	// Either we attached and should get rid of our reference
    	// or we failed in which case we should get rid our reference as well.
        // This just makes sure the reference count is correct.
	
    ret = (pNub->init(0, 0) && pNub->attach(this));
	
    pNub->release();
    if (!ret)
    {
        XTRACE(this, ret, 0, "createSerialStream - Failed to attach to the nub");
        return false;
    }

        // Report the base name to be used for generating device nodes
	
    pNub->setProperty(kIOTTYBaseNameKey, baseName);
	
        // Create suffix key and set it
	
    if (createSuffix((unsigned char *)suffix))
    {		
        pNub->setProperty(kIOTTYSuffixKey, suffix);
    }

    pNub->registerService();
	
	// Save the Product String (at least the first productNameLength's worth).

    indx = fDataInterface->GetDevice()->GetProductStringIndex();	
    if (indx != 0)
    {	
        rc = fDataInterface->GetDevice()->GetStringDescriptor(indx, (char *)&fProductName, sizeof(fProductName));
        if (!rc)
        {
            if (strlen((char *)fProductName) == 0)		// Believe it or not this sometimes happens - null string with an index defined???
            {
                strcpy((char *)fProductName, defaultName);
            }
            pNub->setProperty((const char *)propertyTag, (const char *)fProductName);
        }
    }

    return true;
	
}/* end createSerialStream */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::acquirePort
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnExclusiveAccess, kIOReturnIOError and various others
//
//		Desc:		Set up for gated acquirePort call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::acquirePort(bool sleep, void *refCon)
{
    IOReturn                ret;
//    AppleUSBCDCACMControl   *temp;
    
    XTRACE(this, refCon, sleep, "acquirePort");
    
        // Find the matching control driver first (if we don't already have it)
        
//    IOLog("acquirePort: = %x\n", (unsigned int)gControlDriver);
//    if (!gControlDriver)
//    {
//        temp = findControlDriverAD(this);
//        if (temp == NULL)
//        {
//            XTRACE(this, 0, 0, "acquirePort - Cannot find control driver, trying to continue...");
//        }
//    }

    retain();
    ret = fCommandGate->runAction(acquirePortAction, (void *)sleep);
    release();
    
    return ret;

}/* end acquirePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::acquirePortAction
//
//		Desc:		Dummy pass through for acquirePortGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::acquirePortAction(OSObject *owner, void *arg0, void *, void *, void *)
{

    return ((AppleUSBCDCACMData *)owner)->acquirePortGated((bool)arg0);
    
}/* end acquirePortAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::acquirePortGated
//
//		Inputs:		sleep - true (wait for it), false (don't)
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnExclusiveAccess, kIOReturnIOError and various others
//
//		Desc:		acquirePort tests and sets the state of the port object.  If the port was
// 				available, then the state is set to busy, and kIOReturnSuccess is returned.
// 				If the port was already busy and sleep is YES, then the thread will sleep
// 				until the port is freed, then re-attempts the acquire.  If the port was
// 				already busy and sleep is NO, then kIOReturnExclusiveAccess is returned.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::acquirePortGated(bool sleep)
{
    UInt32 	busyState = 0;
    IOReturn 	rtn = kIOReturnSuccess;
    UInt16	i;

    XTRACE(this, 0, sleep, "acquirePortGated");

    retain(); 							// Hold reference till releasePortGated, unless we fail to acquire
    while (true)
    {
        busyState = fPort.State & PD_S_ACQUIRED;
        if (!busyState)
        {		
                // Set busy bit (acquired), and clear everything else
                
            setStateGated((UInt32)PD_S_ACQUIRED | DEFAULT_STATE, (UInt32)STATE_ALL);
            break;
        } else {
            if (!sleep)
            {
                XTRACE(this, 0, 0, "acquirePortGated - Busy exclusive access");
                release();
            	return kIOReturnExclusiveAccess;
            } else {
            	busyState = 0;
            	rtn = watchStateGated(&busyState, PD_S_ACQUIRED);
            	if ((rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess))
                {
                    continue;
            	} else {
                    XTRACE(this, 0, 0, "acquirePortGated - Interrupted!");
                    release();
                    return rtn;
                }
            }
        }
    }
    
    do
    {    	
        setStructureDefaults();				// Set the default values
        
            // Set up and read the data-in bulk pipe
        
        for (i=0; i<fInBufPool; i++)
        {
            if (fPort.inPool[i].pipeMDP)
            {
                fPort.inPool[i].completionInfo.target = this;
                fPort.inPool[i].completionInfo.action = dataReadComplete;
                fPort.inPool[i].completionInfo.parameter = (void *)&fPort.inPool[i];
                rtn = fPort.InPipe->Read(fPort.inPool[i].pipeMDP, &fPort.inPool[i].completionInfo, NULL);
                if (rtn != kIOReturnSuccess)
                {
                    XTRACE(this, i, rtn, "acquirePortGated - Read for bulk-in pipe failed");
					fPort.inPool[i].dead = true;
                    break;
                }
            }
        }
        if (rtn == kIOReturnSuccess)
        {
        
                // Set up the data-out bulk pipe
		
            for (i=0; i<fOutBufPool; i++)
            {
                if (fPort.outPool[i].pipeMDP)
                {
                    fPort.outPool[i].completionInfo.target = this;
                    fPort.outPool[i].completionInfo.action = dataWriteComplete;
                    fPort.outPool[i].completionInfo.parameter = (void *)&fPort.outPool[i];
                }
            }
        } else {
            break;
        }

        fSessions++;					// Bump number of active sessions and turn on clear to send
        setStateGated(PD_RS232_S_CTS, PD_RS232_S_CTS);
        
            // Tell the Control driver we're good to go
        
        {
            AppleUSBCDCACMControl   *temp = NULL;
    
            temp = findControlDriverAD(this);
            if (temp == NULL)
            {
		XTRACE(this, 0, 0, "acquirePortGated - Cannot find control driver, trying to continue...");
            }
            else
            {
                if (!temp->dataAcquired())
                {
                    XTRACE(this, 0, 0, "acquirePortGated - dataAcquired to Control failed");
                    break;
                }
            }		
        }
        
        return kIOReturnSuccess;
        
    } while (0);

    	// We failed for some reason

    setStateGated(0, STATE_ALL);			// Clear the entire state word
    release();
    
    return rtn;
	
}/* end acquirePortGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::releasePort
//
//		Inputs:		refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Set up for gated releasePort call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::releasePort(void *refCon)
{
    IOReturn	ret = kIOReturnSuccess;
    
    XTRACE(this, 0, 0, "releasePort");
    
    retain();
    ret = fCommandGate->runAction(releasePortAction);
    release();

#if 0        
        // Check the pipes before we leave (only if we're not terminated)
   
    if (!fTerminate)
    {
        if (fPort.InPipe)
            checkPipe(fPort.InPipe, true);
    
        if (fPort.OutPipe)
            checkPipe(fPort.OutPipe, true);
    }
#endif
        
    return ret;

}/* end releasePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::releasePortAction
//
//		Desc:		Dummy pass through for releasePortGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::releasePortAction(OSObject *owner, void *, void *, void *, void *)
{

    return ((AppleUSBCDCACMData *)owner)->releasePortGated();
    
}/* end releasePortAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::releasePortGated
//
//		Inputs:		
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		releasePort returns all the resources and does clean up.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::releasePortGated()
{
    UInt32 	busyState;

    XTRACE(this, 0, 0, "releasePortGated");
    
    busyState = (fPort.State & PD_S_ACQUIRED);
    if (!busyState)
    {
        if (fTerminate || fStopping)
        {
            XTRACE(this, 0, 0, "releasePortGated - Offline");
            return kIOReturnOffline;
        }
        
        XTRACE(this, 0, 0, "releasePortGated - Not open");
        return kIOReturnNotOpen;
    }
	
    if (!fTerminate)
        setControlLineState(false, false);		// clear RTS and clear DTR only if not terminated
	
    setStateGated(0, (UInt32)STATE_ALL);		// Clear the entire state word - which also deactivates the port

//#if 0    
        // Abort any outstanding I/O
        
    if (fPort.InPipe)
        fPort.InPipe->Abort();
    if (fPort.OutPipe)
        fPort.OutPipe->Abort();
//#endif
        
        // Tell the Control driver the port's been released
    {
	AppleUSBCDCACMControl   *temp = NULL;
    
	temp = findControlDriverAD(this);
	if (temp == NULL)
	{
            XTRACE(this, 0, 0, "releasePortGated - Cannot find control driver, trying to continue...");
	}
	else
	{
            temp->dataReleased();
	}		
    }
    
    fSessions--;					// reduce number of active sessions
            
    release(); 						// Dispose of the self-reference we took in acquirePortGated()
    
    XTRACE(this, 0, 0, "releasePort - Exit");
    
    return kIOReturnSuccess;
	
}/* end releasePortGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::getState
//
//		Inputs:		refCon - unused
//
//		Outputs:	Return value - port state
//
//		Desc:		Set up for gated getState call.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCACMData::getState(void *refCon)
{
    UInt32	currState;
    
    XTRACE(this, 0, 0, "getState");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "getState - Offline");
        return 0;
    }
    
    retain();
    currState = fCommandGate->runAction(getStateAction);
    release();
    
    return currState;
    
}/* end getState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::getStateAction
//
//		Desc:		Dummy pass through for getStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::getStateAction(OSObject *owner, void *, void *, void *, void *)
{
    UInt32	newState;

    newState = ((AppleUSBCDCACMData *)owner)->getStateGated();
    
    return newState;
    
}/* end getStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::getStateGated
//
//		Inputs:		port - unused
//
//		Outputs:	return value - port state
//
//		Desc:		Get the state for the port.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCACMData::getStateGated()
{
    UInt32 	state;
	
    XTRACE(this, 0, 0, "getStateGated");
    
    if (fTerminate || fStopping)
        return 0;
	
    CheckQueues();
	
    state = fPort.State & EXTERNAL_MASK;
	
    XTRACE(this, state, EXTERNAL_MASK, "getStateGated - Exit");
	
    return state;
	
}/* end getStateGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::setState
//
//		Inputs:		state - the state
//				mask - the mask
//				refCon - unused
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnBadArgument
//
//		Desc:		Set up for gated setState call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::setState(UInt32 state, UInt32 mask, void *refCon)
{
    IOReturn	ret = kIOReturnSuccess;
    
    XTRACE(this, 0, 0, "setState");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "setState - Offline");
        return 0;
    }
    
        // Cannot acquire or activate via setState
    
    if (mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK)))
    {
        ret = kIOReturnBadArgument;
    } else {
        
            // ignore any bits that are read-only
        
        mask &= (~fPort.FlowControl & PD_RS232_A_MASK) | PD_S_MASK;
        if (mask)
        {
            retain();
            ret = fCommandGate->runAction(setStateAction, (void *)state, (void *)mask);
            release();
        }
    }
    
    return ret;
    
}/* end setState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::setStateAction
//
//		Desc:		Dummy pass through for setStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::setStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{

    return ((AppleUSBCDCACMData *)owner)->setStateGated((UInt32)arg0, (UInt32)arg1);
    
}/* end setStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::setStateGated
//
//		Inputs:		state - state to set
//				mask - state mask
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnBadArgument
//
//		Desc:		Set the state for the port device.  The lower 16 bits are used to set the
//				state of various flow control bits (this can also be done by enqueueing a
//				PD_E_FLOW_CONTROL event).  If any of the flow control bits have been set
//				for automatic control, then they can't be changed by setState.  For flow
//				control bits set to manual (that are implemented in hardware), the lines
//				will be changed before this method returns.  The one weird case is if RXO
//				is set for manual, then an XON or XOFF character may be placed at the end
//				of the TXQ and transmitted later.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::setStateGated(UInt32 state, UInt32 mask)
{
    UInt32	delta;
	
    XTRACE(this, state, mask, "setStateGated");
    
    if (fStopping)
        return kIOReturnOffline;
    
        // Check if it's being acquired or already acquired

    if ((state & PD_S_ACQUIRED) || (fPort.State & PD_S_ACQUIRED))
    {
        if (mask & PD_RS232_S_DTR)
        {
            if ((state & PD_RS232_S_DTR) != (fPort.State & PD_RS232_S_DTR))
            {
                if (state & PD_RS232_S_DTR)
                {
                    XTRACE(this, 0, 0, "setState - DTR TRUE");
                    setControlLineState(false, true);
                } else {
                    if (!fTerminate)
                    {
                        XTRACE(this, 0, 0, "setState - DTR FALSE");
                        setControlLineState(false, false);
                    }
                }
            }
        }
        
        state = (fPort.State & ~mask) | (state & mask); 		// compute the new state
        delta = state ^ fPort.State;		    			// keep a copy of the diffs
        fPort.State = state;

	    // Wake up all threads asleep on WatchStateMask
		
        if (delta & fPort.WatchStateMask)
        {
            fCommandGate->commandWakeup((void *)&fPort.State);
        }
        
        return kIOReturnSuccess;

    } else {
        XTRACE(this, fPort.State, 0, "setStateGated - Not Acquired");
    }
    
    return kIOReturnNotOpen;
	
}/* end setStateGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::watchState
//
//		Inputs:		state - state to watch for
//				mask - state mask bits
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from ::watchState
//
//		Desc:		Set up for gated watchState call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::watchState(UInt32 *state, UInt32 mask, void *refCon)
{
    IOReturn 	ret;

    XTRACE(this, *state, mask, "watchState");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "watchState - Offline");
        return kIOReturnOffline;
    }
    
    if (!state) 
        return kIOReturnBadArgument;
        
    if (!mask)
        return kIOReturnSuccess;

    retain();
    ret = fCommandGate->runAction(watchStateAction, (void *)state, (void *)mask);
    release();
    
    return ret;

}/* end watchState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::watchStateAction
//
//		Desc:		Dummy pass through for watchStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::watchStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{

    return ((AppleUSBCDCACMData *)owner)->watchStateGated((UInt32 *)arg0, (UInt32)arg1);
    
}/* end watchStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::watchStateGated
//
//		Inputs:		state - state to watch for
//				mask - state mask bits
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from privateWatchState
//
//		Desc:		Wait for the at least one of the state bits defined in mask to be equal
//				to the value defined in state. Check on entry then sleep until necessary,
//				A return value of kIOReturnSuccess means that at least one of the port state
//				bits specified by mask is equal to the value passed in by state.  A return
//				value of kIOReturnIOError indicates that the port went inactive.  A return
//				value of kIOReturnIPCError indicates sleep was interrupted by a signal.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::watchStateGated(UInt32 *state, UInt32 mask)
{
    unsigned 	watchState, foundStates;
    bool 	autoActiveBit = false;
    IOReturn 	ret = kIOReturnNotOpen;

    XTRACE(this, *state, mask, "watchStateGated");
    
    if (fTerminate || fStopping)
        return kIOReturnOffline;
        
    if (fPort.State & PD_S_ACQUIRED)
    {
        ret = kIOReturnSuccess;
        mask &= EXTERNAL_MASK;
        
        watchState = *state;
        if (!(mask & (PD_S_ACQUIRED | PD_S_ACTIVE)))
        {
            watchState &= ~PD_S_ACTIVE;				// Check for low PD_S_ACTIVE
            mask |=  PD_S_ACTIVE;				// Register interest in PD_S_ACTIVE bit
            autoActiveBit = true;
        }

        while (true)
        {
                // Check port state for any interesting bits with watchState value
                // NB. the '^ ~' is a XNOR and tests for equality of bits.
			
            foundStates = (watchState ^ ~fPort.State) & mask;

            if (foundStates)
            {
                *state = fPort.State;
                if (autoActiveBit && (foundStates & PD_S_ACTIVE))
                {
                    ret = kIOReturnIOError;
                } else {
                    ret = kIOReturnSuccess;
                }
                break;
            }

                // Everytime we go around the loop we have to reset the watch mask.
                // This means any event that could affect the WatchStateMask must
                // wakeup all watch state threads.  The two events are an interrupt
                // or one of the bits in the WatchStateMask changing.
			
            fPort.WatchStateMask |= mask;
            
            XTRACE(this, fPort.State, fPort.WatchStateMask, "watchStateGated - Thread sleeping");
            
            retain();								// Just to make sure all threads are awake
            fCommandGate->retain();					// before we're released
        
            ret = fCommandGate->commandSleep((void *)&fPort.State);
        
            fCommandGate->release();
            
            XTRACE(this, fPort.State, ret, "watchStateGated - Thread restart");

            if (ret == THREAD_TIMED_OUT)
            {
                ret = kIOReturnTimeout;
                break;
            } else {
                if (ret == THREAD_INTERRUPTED)
                {
                    ret = kIOReturnAborted;
                    break;
                }
            }
            release();
        }       
        
            // As it is impossible to undo the masking used by this
            // thread, we clear down the watch state mask and wakeup
            // every sleeping thread to reinitialize the mask before exiting.
		
        fPort.WatchStateMask = 0;
        XTRACE(this, *state, 0, "watchStateGated - Thread wakeing others");
        fCommandGate->commandWakeup((void *)&fPort.State);
 
        *state &= EXTERNAL_MASK;
    }
	
    XTRACE(this, ret, 0, "watchState - Exit");
    
    return ret;
	
}/* end watchStateGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::nextEvent
//
//		Inputs:		refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnOffline
//
//		Desc:		Not used by this driver.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCACMData::nextEvent(void *refCon)
{

    XTRACE(this, 0, 0, "nextEvent");
    
    if (fTerminate || fStopping)
        return kIOReturnOffline;
        
    if (getState(&fPort) & PD_S_ACTIVE)
    {
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
	
}/* end nextEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::executeEvent
//
//		Inputs:		event - The event
//				data - any data associated with the event
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen or kIOReturnBadArgument
//
//		Desc:		Set up for gated executeEvent call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::executeEvent(UInt32 event, UInt32 data, void *refCon)
{
    IOReturn 	ret;
    
    XTRACE(this, data, event, "executeEvent");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "executeEvent - Offline");
        return kIOReturnOffline;
    }
    
    retain();
    ret = fCommandGate->runAction(executeEventAction, (void *)event, (void *)data);
    release();

    return ret;
    
}/* end executeEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::executeEventAction
//
//		Desc:		Dummy pass through for executeEventGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::executeEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{

    return ((AppleUSBCDCACMData *)owner)->executeEventGated((UInt32)arg0, (UInt32)arg1);
    
}/* end executeEventAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::executeEventGated
//
//		Inputs:		event - The event
//				data - any data associated with the event
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen or kIOReturnBadArgument
//
//		Desc:		executeEvent causes the specified event to be processed immediately.
//				This is primarily used for channel control commands like START & STOP
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::executeEventGated(UInt32 event, UInt32 data)
{
    IOReturn	ret = kIOReturnSuccess;
    UInt32 	state, delta;
	
    if (fTerminate || fStopping)
        return kIOReturnOffline;
        
    delta = 0;
    state = fPort.State;	
    XTRACE(this, state, event, "executeEventGated");
	
    if ((state & PD_S_ACQUIRED) == 0)
        return kIOReturnNotOpen;

    switch (event)
    {
	case PD_RS232_E_XON_BYTE:
            XTRACE(this, data, event, "executeEventGated - PD_RS232_E_XON_BYTE");
            fPort.XONchar = data;
            break;
	case PD_RS232_E_XOFF_BYTE:
            XTRACE(this, data, event, "executeEventGated - PD_RS232_E_XOFF_BYTE");
            fPort.XOFFchar = data;
            break;
	case PD_E_SPECIAL_BYTE:
            XTRACE(this, data, event, "executeEventGated - PD_E_SPECIAL_BYTE");
            fPort.SWspecial[ data >> SPECIAL_SHIFT ] |= (1 << (data & SPECIAL_MASK));
            break;
	case PD_E_VALID_DATA_BYTE:
            XTRACE(this, data, event, "executeEventGated - PD_E_VALID_DATA_BYTE");
            fPort.SWspecial[ data >> SPECIAL_SHIFT ] &= ~(1 << (data & SPECIAL_MASK));
            break;
	case PD_E_FLOW_CONTROL:
            XTRACE(this, data, event, "executeEventGated - PD_E_FLOW_CONTROL");
            break;
	case PD_E_ACTIVE:
            XTRACE(this, data, event, "executeEventGated - PD_E_ACTIVE");
            if ((bool)data)
            {
                if (!(state & PD_S_ACTIVE))
                {
                    setStructureDefaults();
                    setStateGated((UInt32)PD_S_ACTIVE, (UInt32)PD_S_ACTIVE); 			// activate port
				
                    setControlLineState(true, true);						// set RTS and set DTR
                }
            } else {
                if ((state & PD_S_ACTIVE))
                {
                    setStateGated(0, (UInt32)PD_S_ACTIVE);					// deactivate port
				
                    setControlLineState(false, false);						// clear RTS and clear DTR
                }
            }
            break;
	case PD_E_DATA_LATENCY:
            XTRACE(this, data, event, "executeEventGated - PD_E_DATA_LATENCY");
            fPort.DataLatInterval = long2tval(data * 1000);
            break;
	case PD_RS232_E_MIN_LATENCY:
            XTRACE(this, data, event, "executeEventGated - PD_RS232_E_MIN_LATENCY");
            fPort.MinLatency = bool(data);
            break;
	case PD_E_DATA_INTEGRITY:
            XTRACE(this, data, event, "executeEventGated - PD_E_DATA_INTEGRITY");
            if ((data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.TX_Parity = data;
                fPort.RX_Parity = PD_RS232_PARITY_DEFAULT;
			
                setLineCoding();			
            }
            break;
	case PD_E_DATA_RATE:
            XTRACE(this, data, event, "executeEventGated - PD_E_DATA_RATE");
            
                // For API compatiblilty with Intel.
                
            data >>= 1;
            XTRACE(this, data, 0, "executeEventGated - actual data rate");
            if ((data < MIN_BAUD) || (data > kMaxBaudRate))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.BaudRate = data;
			
                setLineCoding();			
            }		
            break;
	case PD_E_DATA_SIZE:
            XTRACE(this, data, event, "executeEventGated - PD_E_DATA_SIZE");
            
                // For API compatiblilty with Intel.
                
            data >>= 1;
            XTRACE(this, data, 0, "executeEventGated - actual data size");
            if ((data < 5) || (data > 8))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.CharLength = data;
			
                setLineCoding();			
            }
            break;
	case PD_RS232_E_STOP_BITS:
            XTRACE(this, data, event, "executeEventGated - PD_RS232_E_STOP_BITS");
            if ((data < 0) || (data > 20))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.StopBits = data;
			
                setLineCoding();
            }
            break;
	case PD_E_RXQ_FLUSH:
            XTRACE(this, data, event, "executeEventGated - PD_E_RXQ_FLUSH");
            break;
	case PD_E_RX_DATA_INTEGRITY:
            XTRACE(this, data, event, "executeEventGated - PD_E_RX_DATA_INTEGRITY");
            if ((data != PD_RS232_PARITY_DEFAULT) &&  (data != PD_RS232_PARITY_ANY))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.RX_Parity = data;
            }
            break;
	case PD_E_RX_DATA_RATE:
            XTRACE(this, data, event, "executeEventGated - PD_E_RX_DATA_RATE");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_RX_DATA_SIZE:
            XTRACE(this, data, event, "executeEventGated - PD_E_RX_DATA_SIZE");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_RS232_E_RX_STOP_BITS:
            XTRACE(this, data, event, "executeEventGated - PD_RS232_E_RX_STOP_BITS");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_TXQ_FLUSH:
            XTRACE(this, data, event, "executeEventGated - PD_E_TXQ_FLUSH");
            break;
	case PD_RS232_E_LINE_BREAK:
            XTRACE(this, data, event, "executeEventGated - PD_RS232_E_LINE_BREAK");
            state &= ~PD_RS232_S_BRK;
            delta |= PD_RS232_S_BRK;
            setStateGated(state, delta);
            break;
	case PD_E_DELAY:
            XTRACE(this, data, event, "executeEventGated - PD_E_DELAY");
            fPort.CharLatInterval = long2tval(data * 1000);
            break;
	case PD_E_RXQ_SIZE:
            XTRACE(this, data, event, "executeEventGated - PD_E_RXQ_SIZE");
            break;
	case PD_E_TXQ_SIZE:
            XTRACE(this, data, event, "executeEventGated - PD_E_TXQ_SIZE");
            break;
	case PD_E_RXQ_HIGH_WATER:
            XTRACE(this, data, event, "executeEventGated - PD_E_RXQ_HIGH_WATER");
            break;
	case PD_E_RXQ_LOW_WATER:
            XTRACE(this, data, event, "executeEventGated - PD_E_RXQ_LOW_WATER");
            break;
	case PD_E_TXQ_HIGH_WATER:
            XTRACE(this, data, event, "executeEventGated - PD_E_TXQ_HIGH_WATER");
            break;
	case PD_E_TXQ_LOW_WATER:
            XTRACE(this, data, event, "executeEventGated - PD_E_TXQ_LOW_WATER");
            break;
	default:
            XTRACE(this, data, event, "executeEventGated - unrecognized event");
            ret = kIOReturnBadArgument;
            break;
    }
	
    return ret;
	
}/* end executeEventGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::requestEvent
//
//		Inputs:		event - The event
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument
//				data - any data associated with the event
//
//		Desc:		requestEvent processes the specified event as an immediate request and
//				returns the results in data.  This is primarily used for getting link
//				status information and verifying baud rate etc.
//				For the most part this can be done immediately without being gated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::requestEvent(UInt32 event, UInt32 *data, void *refCon)
{
    IOReturn	returnValue = kIOReturnSuccess;

    XTRACE(this, 0, event, "requestEvent");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "requestEvent - Offline");
        return kIOReturnOffline;
    }

    if (data == NULL) 
    {
        XTRACE(this, 0, event, "requestEvent - data is null");
        returnValue = kIOReturnBadArgument;
    } else {
        switch (event)
        {
            case PD_E_ACTIVE:
                XTRACE(this, 0, event, "requestEvent - PD_E_ACTIVE");
                *data = bool(getState(&fPort) & PD_S_ACTIVE);			// Just to be safe put this through the gate
                break;
            case PD_E_FLOW_CONTROL:
                XTRACE(this, fPort.FlowControl, event, "requestEvent - PD_E_FLOW_CONTROL");
                *data = fPort.FlowControl;							
                break;
            case PD_E_DELAY:
                XTRACE(this, 0, event, "requestEvent - PD_E_DELAY");
                *data = tval2long(fPort.CharLatInterval)/ 1000;	
                break;
            case PD_E_DATA_LATENCY:
                XTRACE(this, 0, event, "requestEvent - PD_E_DATA_LATENCY");
                *data = tval2long(fPort.DataLatInterval)/ 1000;	
                break;
            case PD_E_TXQ_SIZE:
                XTRACE(this, 0, event, "requestEvent - PD_E_TXQ_SIZE");
                *data = GetQueueSize(&fPort.TX);	
                break;
            case PD_E_RXQ_SIZE:
                XTRACE(this, 0, event, "requestEvent - PD_E_RXQ_SIZE");
                *data = GetQueueSize(&fPort.RX);	
                break;
            case PD_E_TXQ_LOW_WATER:
                XTRACE(this, 0, event, "requestEvent - PD_E_TXQ_LOW_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_LOW_WATER:
                XTRACE(this, 0, event, "requestEvent - PD_E_RXQ_LOW_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_HIGH_WATER:
                XTRACE(this, 0, event, "requestEvent - PD_E_TXQ_HIGH_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_HIGH_WATER:
                XTRACE(this, 0, event, "requestEvent - PD_E_RXQ_HIGH_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_AVAILABLE:
                XTRACE(this, 0, event, "requestEvent - PD_E_TXQ_AVAILABLE");
                *data = FreeSpaceinQueue(&fPort.TX);	 
                break;
            case PD_E_RXQ_AVAILABLE:
                XTRACE(this, 0, event, "requestEvent - PD_E_RXQ_AVAILABLE");
                *data = UsedSpaceinQueue(&fPort.RX); 	
                break;
            case PD_E_DATA_RATE:
                XTRACE(this, 0, event, "requestEvent - PD_E_DATA_RATE");
                *data = fPort.BaudRate << 1;		
                break;
            case PD_E_RX_DATA_RATE:
                XTRACE(this, 0, event, "requestEvent - PD_E_RX_DATA_RATE");
                *data = 0x00;					
                break;
            case PD_E_DATA_SIZE:
                XTRACE(this, 0, event, "requestEvent - PD_E_DATA_SIZE");
                *data = fPort.CharLength << 1;	
                break;
            case PD_E_RX_DATA_SIZE:
                XTRACE(this, 0, event, "requestEvent - PD_E_RX_DATA_SIZE");
                *data = 0x00;					
                break;
            case PD_E_DATA_INTEGRITY:
                XTRACE(this, 0, event, "requestEvent - PD_E_DATA_INTEGRITY");
                *data = fPort.TX_Parity;			
                break;
            case PD_E_RX_DATA_INTEGRITY:
                XTRACE(this, 0, event, "requestEvent - PD_E_RX_DATA_INTEGRITY");
                *data = fPort.RX_Parity;			
                break;
            case PD_RS232_E_STOP_BITS:
                XTRACE(this, 0, event, "requestEvent - PD_RS232_E_STOP_BITS");
                *data = fPort.StopBits << 1;		
                break;
            case PD_RS232_E_RX_STOP_BITS:
                XTRACE(this, 0, event, "requestEvent - PD_RS232_E_RX_STOP_BITS");
                *data = 0x00;					
                break;
            case PD_RS232_E_XON_BYTE:
                XTRACE(this, 0, event, "requestEvent - PD_RS232_E_XON_BYTE");
                *data = fPort.XONchar;			
                break;
            case PD_RS232_E_XOFF_BYTE:
                XTRACE(this, 0, event, "requestEvent - PD_RS232_E_XOFF_BYTE");
                *data = fPort.XOFFchar;			
                break;
            case PD_RS232_E_LINE_BREAK:
                XTRACE(this, 0, event, "requestEvent - PD_RS232_E_LINE_BREAK");
                *data = bool(getState(&fPort) & PD_RS232_S_BRK);			// This should be gated too
                break;
            case PD_RS232_E_MIN_LATENCY:
                XTRACE(this, 0, event, "requestEvent - PD_RS232_E_MIN_LATENCY");
                *data = bool(fPort.MinLatency);		
                break;
            default:
                XTRACE(this, 0, event, "requestEvent - unrecognized event");
                returnValue = kIOReturnBadArgument; 			
                break;
        }
    }

    return kIOReturnSuccess;
	
}/* end requestEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::enqueueEvent
//
//		Inputs:		event - The event
//				data - any data associated with the event, 
//				sleep - true (wait for it), false (don't)
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Not used by this driver.
//				Events are passed on to executeEvent for immediate action.	
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon)
{
    IOReturn 	ret;
    
    XTRACE(this, data, event, "enqueueEvent");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "enqueueEvent - Offline");
        return kIOReturnOffline;
    }
    
    retain();
    ret = fCommandGate->runAction(executeEventAction, (void *)event, (void *)data);
    release();

    return ret;
	
}/* end enqueueEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::dequeueEvent
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen
//
//		Desc:		Not used by this driver.		
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon)
{
	
    XTRACE(this, 0, 0, "dequeueEvent");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "dequeueEvent - Offline");
        return kIOReturnOffline;
    }

    if ((event == NULL) || (data == NULL))
        return kIOReturnBadArgument;

    if (getState(&fPort) & PD_S_ACTIVE)
    {
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
	
}/* end dequeueEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::enqueueData
//
//		Inputs:		buffer - the data
//				size - number of bytes
//				sleep - true (wait for it), false (don't)
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument or value returned from watchState
//				count - bytes transferred  
//
//		Desc:		set up for enqueueDataGated call.	
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon)
{
    IOReturn 	ret;
    
    XTRACE(this, size, sleep, "enqueueData");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "enqueueData - Offline");
        return kIOReturnOffline;
    }
    
    if (count == NULL || buffer == NULL)
        return kIOReturnBadArgument;
        
    retain();
    ret = fCommandGate->runAction(enqueueDataAction, (void *)buffer, (void *)size, (void *)count, (void *)sleep);
    release();

    return ret;
        
}/* end enqueueData */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::enqueueDatatAction
//
//		Desc:		Dummy pass through for enqueueDataGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::enqueueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{

    return ((AppleUSBCDCACMData *)owner)->enqueueDataGated((UInt8 *)arg0, (UInt32)arg1, (UInt32 *)arg2, (bool)arg3);
    
}/* end enqueueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::enqueueDataGated
//
//		Inputs:		buffer - the data
//				size - number of bytes
//				sleep - true (wait for it), false (don't)
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from watchState
//				count - bytes transferred,  
//
//		Desc:		enqueueData will attempt to copy data from the specified buffer to
//				the TX queue as a sequence of VALID_DATA events.  The argument
//				bufferSize specifies the number of bytes to be sent.  The actual
//				number of bytes transferred is returned in count.
//				If sleep is true, then this method will sleep until all bytes can be
//				transferred.  If sleep is false, then as many bytes as possible
//				will be copied to the TX queue.
//				Note that the caller should ALWAYS check the transferCount unless
//				the return value was kIOReturnBadArgument, indicating one or more
//				arguments were not valid.  Other possible return values are
//				kIOReturnSuccess if all requirements were met or kIOReturnOffline
//				if the device was unplugged.		
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
    UInt32 	state = PD_S_TXQ_LOW_WATER;
    IOReturn 	rtn = kIOReturnSuccess;

    XTRACE(this, size, sleep, "enqueueDataGated");

    if (fTerminate || fStopping)
        return kIOReturnOffline;

    *count = 0;

    if (!(fPort.State & PD_S_ACTIVE))
        return kIOReturnNotOpen;

    XTRACE(this, fPort.State, size, "enqueueDataGated - current State");	
//    LogData(kDataOther, size, buffer);

        // Go ahead and try to add something to the buffer
        
    *count = AddtoQueue(&fPort.TX, buffer, size);
    CheckQueues();

        // Let the tranmitter know that we have something ready to go
    
    setUpTransmit();

        // If we could not queue up all of the data on the first pass and
        // the user wants us to sleep until it's all out then sleep

    while ((*count < size) && sleep)
    {
        state = PD_S_TXQ_LOW_WATER;
        rtn = watchStateGated(&state, PD_S_TXQ_LOW_WATER);
        if (rtn != kIOReturnSuccess)
        {
            XTRACE(this, 0, rtn, "enqueueDataGated - interrupted");
            return rtn;
        }

        *count += AddtoQueue(&fPort.TX, buffer + *count, size - *count);
        CheckQueues();

            // Let the tranmitter know that we have something ready to go.

        setUpTransmit();
    }

    XTRACE(this, *count, size, "enqueueDataGated - Exit");

    return kIOReturnSuccess;
	
}/* end enqueueDataGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::dequeueData
//
//		Inputs:		size - buffer size
//				min - minimum bytes required
//				refCon - the Port
//
//		Outputs:	buffer - data returned
//				min - number of bytes
//				Return Code - kIOReturnSuccess, kIOReturnBadArgument, kIOReturnNotOpen, or value returned from watchState
//
//		Desc:		set up for dequeueDataGated call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon)
{
    IOReturn 	ret;
    
    XTRACE(this, size, min, "dequeueData");
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, kIOReturnOffline, "dequeueData - Offline");
        return kIOReturnOffline;
    }
    
    if ((count == NULL) || (buffer == NULL) || (min > size))
        return kIOReturnBadArgument;

    retain();
    ret = fCommandGate->runAction(dequeueDataAction, (void *)buffer, (void *)size, (void *)count, (void *)min);
    release();

    return ret;

}/* end dequeueData */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::dequeueDatatAction
//
//		Desc:		Dummy pass through for dequeueDataGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::dequeueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{

    return ((AppleUSBCDCACMData *)owner)->dequeueDataGated((UInt8 *)arg0, (UInt32)arg1, (UInt32 *)arg2, (UInt32)arg3);
    
}/* end dequeueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::dequeueDataGated
//
//		Inputs:		size - buffer size
//				min - minimum bytes required
//
//		Outputs:	buffer - data returned
//				min - number of bytes
//				Return Code - kIOReturnSuccess, kIOReturnBadArgument, kIOReturnNotOpen, or value returned from watchState
//
//		Desc:		dequeueData will attempt to copy data from the RX queue to the
//				specified buffer.  No more than bufferSize VALID_DATA events
//				will be transferred. In other words, copying will continue until
//				either a non-data event is encountered or the transfer buffer
//				is full.  The actual number of bytes transferred is returned
//				in count.
//				The sleep semantics of this method are slightly more complicated
//				than other methods in this API. Basically, this method will
//				continue to sleep until either min characters have been
//				received or a non data event is next in the RX queue.  If
//				min is zero, then this method never sleeps and will return
//				immediately if the queue is empty.
//				Note that the caller should ALWAYS check the transferCount
//				unless the return value was kIOReturnBadArgument, indicating one or
//				more arguments were not valid.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
{
    IOReturn 	rtn = kIOReturnSuccess;
    UInt32 	state = 0;
    bool	goXOIdle;

    XTRACE(this, size, min, "dequeueDataGated");
    
    if (fTerminate || fStopping)
        return kIOReturnOffline;
	
        // If the port is not active then there should not be any chars.
        
    *count = 0;
    if (!(fPort.State & PD_S_ACTIVE))
        return kIOReturnNotOpen;

        // Get any data living in the queue.
        
    *count = RemovefromQueue(&fPort.RX, buffer, size);
    CheckQueues();

    while ((min > 0) && (*count < min))
    {
            // Figure out how many bytes we have left to queue up
            
        state = 0;

        rtn = watchStateGated(&state, PD_S_RXQ_EMPTY);

        if (rtn != kIOReturnSuccess)
        {
            XTRACE(this, 0, rtn, "dequeueDataGated - Interrupted!");
            return rtn;
        }
        
            // Try and get more data starting from where we left off
            
        *count += RemovefromQueue(&fPort.RX, buffer + *count, (size - *count));
        CheckQueues();
		
    }

        // Now let's check our receive buffer to see if we need to stop
        
    goXOIdle = (UsedSpaceinQueue(&fPort.RX) < fPort.RXStats.LowWater) && (fPort.RXOstate == SENT_XOFF);

    if (goXOIdle)
    {
        fPort.RXOstate = IDLE_XO;
        AddBytetoQueue(&fPort.TX, fPort.XOFFchar);
        setUpTransmit();
    }

    XTRACE(this, *count, size, "dequeueData - Exit");

    return rtn;
	
}/* end dequeueDataGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::setUpTransmit
//
//		Inputs:		
//
//		Outputs:	return code - true (transmit started), false (transmission already in progress)
//
//		Desc:		Setup and then start transmisson
//
/****************************************************************************************************/

bool AppleUSBCDCACMData::setUpTransmit()
{

    XTRACE(this, 0, 0, "setUpTransmit");
    
            // As a precaution just check we've not been terminated (maybe a woken thread)
    
    if (fTerminate || fStopping)
    {
        XTRACE(this, 0, 0, "setUpTransmit - terminated");
        return false;
    }

    if (UsedSpaceinQueue(&fPort.TX) > 0)
    {
        startTransmission();
    }

    return TRUE;
	
}/* end setUpTransmit */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::startTransmission
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Start the transmisson
//				Must be called from a gated method
//
/****************************************************************************************************/

void AppleUSBCDCACMData::startTransmission()
{
    size_t	count;
    IOReturn	ior;
    UInt16	indx;
    
    XTRACE(this, 0, 0, "startTransmission");

        // Fill up a buffer with data from the queue
        
    indx = fPort.outPoolIndex++;
    if (fPort.outPoolIndex >= fOutBufPool)
    {
        fPort.outPoolIndex = 0;
    }
    if (!fPort.outPool[indx].avail)
    {
        XTRACE(this, fOutBufPool, indx, "startTransmission - Output buffer unavailable");
        return;
    }

        // Fill up the buffer with characters from the queue
		
    count = RemovefromQueue(&fPort.TX, fPort.outPool[indx].pipeBuffer, MAX_BLOCK_SIZE);

        // If there are no bytes to send just exit:
		
    if (count <= 0)
    {
            // Updates all the status flags:
			
        CheckQueues();
        return;
    }
    
    setStateGated(PD_S_TX_BUSY, PD_S_TX_BUSY);
    
    XTRACE(this, fPort.State, count, "startTransmission - Bytes to write");
    LogData(kDataOut, count, fPort.outPool[indx].pipeBuffer);
    	
    fPort.outPool[indx].count = count;
    fPort.outPool[indx].avail = false;
    fPort.outPool[indx].completionInfo.parameter = (void *)&fPort.outPool[indx];
    fPort.outPool[indx].pipeMDP->setLength(count);
    
    ior = fPort.OutPipe->Write(fPort.outPool[indx].pipeMDP, &fPort.outPool[indx].completionInfo);
    if (ior != kIOReturnSuccess)
    {
        XTRACE(this, 0, ior, "startTransmission - Write failed");
    } 

        // We just removed a bunch of stuff from the
        // queue, so see if we can free some thread(s)
        // to enqueue more stuff.
		
    CheckQueues();
	
}/* end startTransmission */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::setLineCoding
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Set up and send SetLineCoding Management Element Request(MER) for all settings.
//
/****************************************************************************************************/

void AppleUSBCDCACMData::setLineCoding()
{

    XTRACE(this, 0, 0, "setLineCoding");
    
    	// Check for changes and only do it if something's changed
	
    if ((fPort.BaudRate == fPort.LastBaudRate) && (fPort.StopBits == fPort.LastStopBits) && 
        (fPort.TX_Parity == fPort.LastTX_Parity) && (fPort.CharLength == fPort.LastCharLength))
    {
        return;
    }

        // Now send it to the control driver
    {
	AppleUSBCDCACMControl   *temp = NULL;
    
	temp = findControlDriverAD(this);
	if (temp == NULL)
	{
            XTRACE(this, 0, 0, "setLineCoding - Cannot find control driver, trying to continue...");
	}
	else
	{
            temp->USBSendSetLineCoding(fPort.BaudRate, fPort.StopBits, fPort.TX_Parity, fPort.CharLength);
	}		
    }
}/* end setLineCoding */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::setControlLineState
//
//		Inputs:		RTS - true(set RTS), false(clear RTS)
//				DTR - true(set DTR), false(clear DTR)
//
//		Outputs:	
//
//		Desc:		Set up and send SetControlLineState Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCACMData::setControlLineState(bool RTS, bool DTR)
{
    AppleUSBCDCACMControl   *temp = NULL;
	
    XTRACE(this, 0, 0, "setControlLineState");
    temp = findControlDriverAD(this);
    if (temp == NULL)
    {
        XTRACE(this, 0, 0, "setControlLineState - Cannot find control driver, trying to continue...");
    }
    else
    {
        temp->USBSendSetControlLineState(RTS, DTR);
    }		
}/* end setControlLineState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::sendBreak
//
//		Inputs:		sBreak - true(set Break), false(clear Break)
//
//		Outputs:	
//
//		Desc:		Set up and send SendBreak Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCACMData::sendBreak(bool sBreak)
{
    AppleUSBCDCACMControl   *temp = NULL;
	
    XTRACE(this, 0, 0, "sendBreak");
    temp = findControlDriverAD(this);
    if (temp == NULL)
    {
        XTRACE(this, 0, 0, "sendBreak - Cannot find control driver, trying to continue...");
    }
    else
    {
        temp->USBSendBreak(sBreak);
    }		
}/* end sendBreak */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::checkPipe
//
//		Inputs:		thePipe - the pipe
//				devReq - true(send CLEAR_FEATURE), false(only if status returns stalled)
//
//		Outputs:	
//
//		Desc:		Clear a stall on the specified pipe. If ClearPipeStall is issued
//				all outstanding I/O is returned with kIOUSBTransactionReturned and
//				a CLEAR_FEATURE Endpoint stall is sent.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::checkPipe(IOUSBPipe *thePipe, bool devReq)
{
    IOReturn 	rtn = kIOReturnSuccess;
    
    XTRACE(this, 0, thePipe, "checkPipe");
    
    if (!devReq)
    {
        rtn = thePipe->GetPipeStatus();
        if (rtn != kIOUSBPipeStalled)
        {
            XTRACE(this, 0, rtn, "checkPipe - Pipe not stalled");
            return rtn;
        }
    }
    
    rtn = thePipe->ClearPipeStall(true);
    if (rtn == kIOReturnSuccess)
    {
        XTRACE(this, 0, 0, "checkPipe - ClearPipeStall Successful");
    } else {
        XTRACE(this, 0, rtn, "checkPipe - ClearPipeStall Failed");
    }
    
    return rtn;

}/* end checkPipe */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::initStructure
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Initialize the port structure
//
/****************************************************************************************************/

void AppleUSBCDCACMData::initStructure()
{
    UInt16	i;
	
    XTRACE(this, 0, 0, "initStructure");

        // These are set up at start and should not be reset during execution.
        
    fPort.FCRimage = 0x00;
    fPort.IERmask = 0x00;

    fPort.State = (PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER);
    fPort.WatchStateMask = 0x00000000;
    fPort.InPipe = NULL;
    fPort.OutPipe = NULL;
    for (i=0; i<kMaxInBufPool; i++)
    {
        fPort.inPool[i].pipeMDP = NULL;
        fPort.inPool[i].pipeBuffer = NULL;
        fPort.inPool[i].dead = false;
    }
    for (i=0; i<kMaxOutBufPool; i++)
    {
        fPort.outPool[i].pipeMDP = NULL;
        fPort.outPool[i].pipeBuffer = NULL;
        fPort.outPool[i].count = -1;
        fPort.outPool[i].avail = false;
    }
    fPort.outPoolIndex = 0;

}/* end initStructure */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::setStructureDefaults
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Sets the defaults for the specified port structure
//
/****************************************************************************************************/

void AppleUSBCDCACMData::setStructureDefaults()
{
    UInt32	tmp;
	
    XTRACE(this, 0, 0, "setStructureDefaults");

    fPort.BaudRate = kDefaultBaudRate;			// 9600 bps
    fPort.LastBaudRate = 0;
    fPort.CharLength = 8;				// 8 Data bits
    fPort.LastCharLength = 0;
    fPort.StopBits = 2;					// 1 Stop bit
    fPort.LastStopBits = 0;
    fPort.TX_Parity = 1;				// No Parity
    fPort.LastTX_Parity	= 0;
    fPort.RX_Parity = 1;				// --ditto--
    fPort.MinLatency = false;
    fPort.XONchar = '\x11';
    fPort.XOFFchar = '\x13';
    fPort.FlowControl = 0x00000000;
    fPort.RXOstate = IDLE_XO;
    fPort.TXOstate = IDLE_XO;
    fPort.FrameTOEntry = NULL;

    fPort.RXStats.BufferSize = kMaxCirBufferSize;
    fPort.RXStats.HighWater = (fPort.RXStats.BufferSize << 1) / 3;
    fPort.RXStats.LowWater = fPort.RXStats.HighWater >> 1;
    fPort.TXStats.BufferSize = kMaxCirBufferSize;
    fPort.TXStats.HighWater = (fPort.RXStats.BufferSize << 1) / 3;
    fPort.TXStats.LowWater = fPort.RXStats.HighWater >> 1;

    fPort.FlowControl = (DEFAULT_AUTO | DEFAULT_NOTIFY);

    for (tmp=0; tmp < (256 >> SPECIAL_SHIFT); tmp++)
        fPort.SWspecial[ tmp ] = 0;
	
}/* end setStructureDefaults */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool AppleUSBCDCACMData::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;
    UInt16			i;

    XTRACE(this, 0, 0, "allocateResources.");

        // Open all the end points and get the buffers

    if (!fDataInterface->open(this))
    {
        XTRACE(this, 0, 0, "allocateResources - open data interface failed.");
        return false;
    }

        // Bulk In pipe

    epReq.type = kUSBBulk;
    epReq.direction = kUSBIn;
    epReq.maxPacketSize	= 0;
    epReq.interval = 0;
    fPort.InPipe = fDataInterface->FindNextPipe(0, &epReq);
    if (!fPort.InPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no bulk input pipe.");
        return false;
    }
    fPort.InPacketSize = epReq.maxPacketSize;
    XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fPort.InPipe, "allocateResources - bulk input pipe.");
    
        // Allocate Memory Descriptor Pointer with memory for the bulk in pipe
    
    for (i=0; i<fInBufPool; i++)
    {
        fPort.inPool[i].pipeMDP = IOBufferMemoryDescriptor::withCapacity(DATA_BUFF_SIZE, kIODirectionIn);
        if (!fPort.inPool[i].pipeMDP)
        {
            XTRACE(this, 0, i, "allocateResources - Allocate input MDP failed");
            return false;
        }
        fPort.inPool[i].pipeBuffer = (UInt8*)fPort.inPool[i].pipeMDP->getBytesNoCopy();
        XTRACE(this, fPort.inPool[i].pipeMDP, fPort.inPool[i].pipeBuffer, "allocateResources - input buffer");
        fPort.inPool[i].dead = false;
    }
    
        // Bulk Out pipe

    epReq.direction = kUSBOut;
    fPort.OutPipe = fDataInterface->FindNextPipe(0, &epReq);
    if (!fPort.OutPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no bulk output pipe.");
        return false;
    }
    fPort.OutPacketSize = epReq.maxPacketSize;
    XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fPort.OutPipe, "allocateResources - bulk output pipe.");
    
        // Allocate Memory Descriptor Pointer with memory for the bulk out pipe

    for (i=0; i<fOutBufPool; i++)
    {
        fPort.outPool[i].pipeMDP = IOBufferMemoryDescriptor::withCapacity(MAX_BLOCK_SIZE, kIODirectionOut);
        if (!fPort.outPool[i].pipeMDP)
        {
            XTRACE(this, 0, i, "allocateResources - Allocate output MDP failed");
            return false;
        }
        fPort.outPool[i].pipeBuffer = (UInt8*)fPort.outPool[i].pipeMDP->getBytesNoCopy();
        XTRACE(this, fPort.outPool[i].pipeMDP, fPort.outPool[i].pipeBuffer, "allocateResources - output buffer");
        fPort.outPool[i].avail = true;
    }
    
        // Now the ring buffers
        
    if (!allocateRingBuffer(&fPort.TX, fPort.TXStats.BufferSize))
    {
        XTRACE(this, 0, 0, "allocateResources - Couldn't allocate TX ring buffer");
        return false;
    }
    
    XTRACE(this, 0, fPort.TX.Start, "allocateResources - TX ring buffer");
    
    if (!allocateRingBuffer(&fPort.RX, fPort.RXStats.BufferSize)) 
    {
        XTRACE(this, 0, 0, "allocateResources - Couldn't allocate RX ring buffer");
        return false;
    }
    
    XTRACE(this, 0, fPort.RX.Start, "allocateResources - RX ring buffer");

    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCACMData::releaseResources()
{
    UInt16	i;
    
    XTRACE(this, 0, 0, "releaseResources");
	
	if (fDataInterface)	
    { 
        fDataInterface->close(this);
        fDataInterface->release();
        fDataInterface = NULL;
    }
    
    for (i=0; i<fInBufPool; i++)
    {
        if (fPort.inPool[i].pipeMDP)	
        { 
            fPort.inPool[i].pipeMDP->release();	
            fPort.inPool[i].pipeMDP = NULL;
            fPort.inPool[i].dead = false;
        }
    }
	
    for (i=0; i<fOutBufPool; i++)
    {
        if (fPort.outPool[i].pipeMDP)	
        { 
            fPort.outPool[i].pipeMDP->release();	
            fPort.outPool[i].pipeMDP = NULL;
            fPort.outPool[i].count = -1;
            fPort.outPool[i].avail = false;
        }
    }
    fPort.outPoolIndex = 0;
    
    if (fWorkLoop)
    {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }

    freeRingBuffer(&fPort.TX);
    freeRingBuffer(&fPort.RX);
	
}/* end releaseResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::freeRingBuffer
//
//		Inputs:		Queue - the specified queue to free
//
//		Outputs:	
//
//		Desc:		Frees all resources assocated with the queue, then sets all queue parameters 
//				to safe values.
//
/****************************************************************************************************/

void AppleUSBCDCACMData::freeRingBuffer(CirQueue *Queue)
{
    XTRACE(this, 0, Queue, "freeRingBuffer");

    if (Queue)
    {
        if (Queue->Start)
        {
            IOFree(Queue->Start, Queue->Size);
        }
        CloseQueue(Queue);
    }
	
}/* end freeRingBuffer */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::allocateRingBuffer
//
//		Inputs:		Queue - the specified queue to allocate
//				BufferSize - size to allocate
//
//		Outputs:	return Code - true (buffer allocated), false (it failed)
//
//		Desc:		Allocates resources needed by the queue, then sets up all queue parameters. 
//
/****************************************************************************************************/

bool AppleUSBCDCACMData::allocateRingBuffer(CirQueue *Queue, size_t BufferSize)
{
    UInt8	*Buffer;

        // Size is ignored and kMaxCirBufferSize, which is 4096, is used.
		
    XTRACE(this, 0, BufferSize, "allocateRingBuffer");
    Buffer = (UInt8*)IOMalloc(kMaxCirBufferSize);

    InitQueue(Queue, Buffer, kMaxCirBufferSize);

    if (Buffer)
        return true;

    return false;
	
}/* end allocateRingBuffer */

/****************************************************************************************************/
//
//		Function:	AppleUSBCDCACMData::WakeonRing
//
//		Inputs:		none
//
//		Outputs:	return code - true(Wake-on-Ring enabled), false(disabled)	
//
//		Desc:		Find the PMU entry and checks the wake-on-ring flag
//
/****************************************************************************************************/

bool AppleUSBCDCACMData::WakeonRing(void)
{
    mach_timespec_t	t;
    IOService 		*pmu;
    bool		WoR = false;

    XTRACE(this, 0, 0, "WakeonRing");
        
    t.tv_sec = 1;
    t.tv_nsec = 0;
    
    pmu = waitForService(IOService::serviceMatching("ApplePMU"), &t);
    if (pmu)
    {
        if (kOSBooleanTrue == pmu->getProperty("WakeOnRing"))
        {
            XTRACE(this, 0, 0, "WakeonRing - Enabled");
            WoR = true;
        } else {
            XTRACE(this, 0, 0, "WakeonRing - Disabled");
        }
    } else {
        XTRACE(this, 0, 0, "WakeonRing - serviceMatching ApplePMU failed");
    }
    
    return WoR;
    
}/* end WakeonRing */

/****************************************************************************************************/
//
//		Function:	AppleUSBCDCACMData::setWakeFeature
//
//		Inputs:		none
//
//		Outputs:	none
//
//		Desc:		Check the wake-on-ring feature and send the device request
//
/****************************************************************************************************/

void AppleUSBCDCACMData::setWakeFeature(void)
{
	IOUSBDevRequest devreq;
	IOReturn		ior;

    XTRACE(this, 0, 0, "setWakeFeature");
	    
		// Set/Clear the Device Remote Wake feature depending upon wake-on-ring
    
	devreq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
	if (!WakeonRing())				
	{
		devreq.bRequest = kUSBRqClearFeature;
	} else {
		devreq.bRequest = kUSBRqSetFeature;
	}
	devreq.wValue = kUSBFeatureDeviceRemoteWakeup;
	devreq.wIndex = 0;
	devreq.wLength = 0;
	devreq.pData = 0;

	ior = fDataInterface->GetDevice()->DeviceRequest(&devreq);
	if (ior == kIOReturnSuccess)
	{
		XTRACE(this, 0, ior, "setWakeFeature - Set/Clear remote wake up feature successful");
	} else {
		XTRACE(this, 0, ior, "setWakeFeature - Set/Clear remote wake up feature failed");
	}

}/* end setWakeFeature */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::resurrectRead
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Try and resurrect any dead reads 
//
/****************************************************************************************************/

void AppleUSBCDCACMData::resurrectRead()
{
    UInt16		i;
	IOReturn	rtn;
	
	XTRACE(this, 0, 0, "resurrectRead");
	
		// Let's check the pipes first
	
	if (fPort.InPipe)
	{
		checkPipe(fPort.InPipe, false);
	}
    
	if (fPort.OutPipe)
	{
		checkPipe(fPort.OutPipe, false);
	}

	for (i=0; i<fInBufPool; i++)
	{
		if (fPort.inPool[i].pipeMDP)
		{
			if (fPort.inPool[i].dead)
			{
				rtn = fPort.InPipe->Read(fPort.inPool[i].pipeMDP, &fPort.inPool[i].completionInfo, NULL);
				if (rtn != kIOReturnSuccess)
				{
					XTRACE(this, i, rtn, "resurrectRead - Read for bulk-in pipe failed, still dead");
				} else {
					fPort.inPool[i].dead = false;
				}
			}
		}
	}
	
}/* end resurrectRead */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCACMData::message
//
//		Inputs:		type - message type
//				provider - my provider
//				argument - additional parameters
//
//		Outputs:	return Code - kIOReturnSuccess
//
//		Desc:		Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCACMData::message(UInt32 type, IOService *provider, void *argument)
{	
    
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, fSessions, type, "message - kIOMessageServiceIsTerminated");
			
            if (fSessions)
            {
                if (!fTerminate)		// Check if we're already being terminated
                { 
		    // NOTE! This call below depends on the hard coded path of this KEXT. Make sure
		    // that if the KEXT moves, this path is changed!
		    KUNCUserNotificationDisplayNotice(
			0,		// Timeout in seconds
			0,		// Flags (for later usage)
			"",		// iconPath (not supported yet)
			"",		// soundPath (not supported yet)
			"/System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCACMData.kext",		// localizationPath
			"Unplug Header",		// the header
			"Unplug Notice",		// the notice - look in Localizable.strings
			"OK"); 
                }
            }
            			
            fTerminate = true;		// We're being terminated (unplugged)
            releaseResources();
            return kIOReturnSuccess;			
        case kIOMessageServiceIsSuspended: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceIsSuspended");
            break;			
        case kIOMessageServiceIsResumed: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceIsResumed");
            break;			
        case kIOMessageServiceIsRequestingClose: 
            XTRACE(this, 0, type, "message - kIOMessageServiceIsRequestingClose"); 
            break;
        case kIOMessageServiceWasClosed: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceWasClosed"); 
            break;
        case kIOMessageServiceBusyStateChange: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceBusyStateChange"); 
            break;
        case kIOUSBMessagePortHasBeenResumed: 	
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenResumed");
            resurrectRead();
            break;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
        case kIOUSBMessagePortHasBeenReset:
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenReset");
			resurrectRead();
			if (fConfigAttributes & kUSBAtrRemoteWakeup)
			{
				setWakeFeature();
			}
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */