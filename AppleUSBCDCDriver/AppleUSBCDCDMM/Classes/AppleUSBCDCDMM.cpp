/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2004 Apple Computer, Inc.  All Rights Reserved.
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

    /* AppleUSBCDCDMM.cpp - MacOSX implementation of		*/
    /* USB Communication Device Class (CDC) Driver, DMM Interface.	*/

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

#define DEBUG_NAME "AppleUSBCDCDMM"

#include "AppleUSBCDCDMM.h"

#define MIN_BAUD (50 << 1)

    // Globals

#define super IOSerialDriverSync

OSDefineMetaClassAndStructors(AppleUSBCDCDMM, IOSerialDriverSync);

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

void AppleUSBCDCDMM::USBLogData(UInt8 Dir, SInt32 Count, char *buf)
{    
    SInt32	wlen;
    SInt32	llen, rlen;
    SInt16	i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];
    
    switch (Dir)
    {
        case kDataIn:
            Log( "AppleUSBCDCDMM: USBLogData - Read Complete, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
            break;
        case kDataOut:
            Log( "AppleUSBCDCDMM: USBLogData - Write, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
            break;
        case kDataOther:
            Log( "AppleUSBCDCDMM: USBLogData - Other, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
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
        Log( "AppleUSBCDCDMM: USBLogData - No data, Count=0\n" );
        return;
    }

    rlen = 0;
    do
    {
        memset(LocBuf, 0x20, buflen);
        
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
		
		Log("%s\n", LocBuf);
#if USE_IOL
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
#endif       
        rlen += llen;
        buf = &buf[rlen];
    } while (wlen != 0);

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

QueueStatus AppleUSBCDCDMM::AddBytetoQueue(CirQueue *Queue, char Value)
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

QueueStatus AppleUSBCDCDMM::GetBytetoQueue(CirQueue *Queue, UInt8 *Value)
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

QueueStatus AppleUSBCDCDMM::InitQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size)
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

QueueStatus AppleUSBCDCDMM::CloseQueue(CirQueue *Queue)
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

size_t AppleUSBCDCDMM::AddtoQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size)
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

size_t AppleUSBCDCDMM::RemovefromQueue(CirQueue *Queue, UInt8 *Buffer, size_t MaxSize)
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

size_t AppleUSBCDCDMM::FreeSpaceinQueue(CirQueue *Queue)
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

size_t AppleUSBCDCDMM::UsedSpaceinQueue(CirQueue *Queue)
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

size_t AppleUSBCDCDMM::GetQueueSize(CirQueue *Queue)
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

QueueStatus AppleUSBCDCDMM::GetQueueStatus(CirQueue *Queue)
{
    if ((Queue->NextChar == Queue->LastChar) && Queue->InQueue)
        return queueFull;
    else if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue)
        return queueEmpty;
		
    return queueNoError ;
	
}/* end GetQueueStatus */

/****************************************************************************************************/
//
//		Method:		isCRinQueue
//
//		Inputs:		Queue - the queue to be looked at
//
//		Outputs:	return code - number of bytes
//
//		Desc:		Is there a cr in the queue.
//
/****************************************************************************************************/

UInt16 AppleUSBCDCDMM::isCRinQueue(CirQueue *Queue)
{
    UInt8	*last;
	bool	done = false;
	UInt16  i = 1;
	
	if ((Queue->NextChar == Queue->LastChar) && !Queue->InQueue)
    {
        return 0;
    }
	
	last = Queue->LastChar;
	
	while (!done)
	{
		if (*last == 0x0D)
		{
			done = true;
		} else {
			last++;
			i++;
			if (last >= Queue->End)
			{
				last = Queue->Start;
			}
			if (last == Queue->NextChar)
			{
				i = 0;
				done = true;
			}
		}
	}

    return i;
	
}/* end isCRinQueue */

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

void AppleUSBCDCDMM::CheckQueues()
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
//		Method:		AppleUSBCDCDMM::intReadComplete
//
//		Inputs:		obj - me
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Interrupt pipe (DMM interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDMM::intReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCDMM	*me = (AppleUSBCDCDMM*)obj;
    IOReturn			ior;
    UInt32			dLen;
    
    XTRACE(me, rc, 0, "intReadComplete");
    
    if (me->fStopping)
        return;

    if (rc == kIOReturnSuccess)					// If operation returned ok
    {
        dLen = me->fIntBufferSize - remaining;
        XTRACE(me, me->fIntPipeBuffer[1], dLen, "intReadComplete - Notification and length");
		
            // Now look at the notification
		
        if (me->fIntPipeBuffer[1] == kUSBRESPONSE_AVAILABLE)
        {
			rc = me->sendMERRequest(kUSBGET_ENCAPSULATED_RESPONSE, 0, me->fMax_Command, me->fInBuffer, &me->fRspCompletionInfo);
			if (rc != kIOReturnSuccess)
			{
				XTRACE(me, 0, rc, "intReadComplete - sendMERRequest failed");
			}
		}
    } else {
        XTRACE(me, 0, rc, "intReadComplete - error");
	}
    
        // Queue the next read only if not aborted
	
    if (rc != kIOReturnAborted)
    {
        ior = me->fIntPipe->Read(me->fIntPipeMDP, &me->fIntCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(me, 0, rc, "intReadComplete - Read io error");
			me->fReadDead = true;
        }
    } else {
		me->fReadDead = true;
	}
	
}/* end intReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::merWriteComplete
//
//		Inputs:		obj - me
//				param - MER 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Management Element Request write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDMM::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
#if LDEBUG
	AppleUSBCDCDMM	*me = (AppleUSBCDCDMM*)obj;
#endif
    IOUSBDevRequest		*MER = (IOUSBDevRequest *)param;
    UInt16			dataLen;
    
    XTRACE(0, 0, remaining, "merWriteComplete");
        
    if (MER)
    {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, MER->bRequest, remaining, "merWriteComplete - request");
        } else {
            XTRACE(me, MER->bRequest, rc, "merWriteComplete - io err");
        }
		
        dataLen = MER->wLength;
        XTRACE(me, 0, dataLen, "merWriteComplete - data length");
        if ((dataLen != 0) && (MER->pData))
        {
            IOFree(MER->pData, dataLen);
        }
        IOFree(MER, sizeof(IOUSBDevRequest));
		
    } else {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, 0, remaining, "merWriteComplete (request unknown)");
        } else {
            XTRACE(me, 0, rc, "merWriteComplete (request unknown) - io err");
        }
    }
	
}/* end merWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::rspComplete
//
//		Inputs:		obj - me
//				param - MER 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Encapsulated response (MER) completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDMM::rspComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
	AppleUSBCDCDMM  *me = (AppleUSBCDCDMM*)obj;
    IOUSBDevRequest *MER = (IOUSBDevRequest *)param;
    UInt16			dataLen;
    
    XTRACE(me, 0, remaining, "rspComplete");
        
    if (MER)
    {
		dataLen = MER->wLength;
        if (rc == kIOReturnSuccess)
        {
			XTRACE(me, MER->bRequest, dataLen, "rspComplete - request and data length");
			if ((MER->bRequest == kUSBGET_ENCAPSULATED_RESPONSE) && (dataLen > 0))
			{
				meLogData(kDataIn, dataLen, MER->pData);
	
					// Move the incoming bytes to the ring buffer
            
				me->AddtoQueue(&me->fPort.RX, (UInt8 *)MER->pData, dataLen);
        
				me->CheckQueues();
			}
        } else {
            XTRACE(me, MER->bRequest, rc, "rspComplete - io err");
        }
		
        if ((dataLen != 0) && (MER->pData))
        {
            IOFree(MER->pData, dataLen);
        }
        IOFree(MER, sizeof(IOUSBDevRequest));
		
    } else {
        if (rc == kIOReturnSuccess)
        {
            XTRACE(me, 0, remaining, "rspComplete (request unknown)");
        } else {
            XTRACE(me, 0, rc, "rspComplete (request unknown) - io err");
        }
    }
	
}/* end rspComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::probe
//
//		Inputs:		provider - my provider
//
//		Outputs:	IOService - from super::probe, score - probe score
//
//		Desc:		Modify the probe score if necessary (we don't  at the moment)
//
/****************************************************************************************************/

IOService* AppleUSBCDCDMM::probe( IOService *provider, SInt32 *score )
{ 
    IOService   *res;
		
		// If our IOUSBInterface has a "do not match" property, it means that we should not match and need 
		// to bail.  See rdar://3716623
    
    OSBoolean *boolObj = OSDynamicCast(OSBoolean, provider->getProperty("kDoNotClassMatchThisInterface"));
    if (boolObj && boolObj->isTrue())
    {
        XTRACE(this, 0, 0, "probe - provider doesn't want us to match");
        return NULL;
    }

    res = super::probe(provider, score);
    
    return res;
    
}/* end probe */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has been determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleUSBCDCDMM::start(IOService *provider)
{
    
    fSessions = 0;
    fTerminate = false;
    fStopping = false;
    fWorkLoop = NULL;
	fMax_Command = 256;
	fIntBufferSize = INT_BUFF_SIZE;
    
    initStructure();
    
    XTRACE(this, 0, provider, "start - provider.");
    
    if(!super::start(provider))
    {
        ALERT(0, 0, "start - super failed");
        return false;
    }

	// Get my USB provider - the interface

    fInterface = OSDynamicCast(IOUSBInterface, provider);
    if(!fInterface)
    {
        ALERT(0, 0, "start - provider invalid");
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
	
	if (!configureDMM())
    {
        ALERT(0, 0, "start - configureDMM failed");
        return false;
    }
    
    if (!allocateResources()) 
    {
        ALERT(0, 0, "start - allocateResources failed");
        return false;
    }
	
	if (!createSerialStream())					// Publish SerialStream services
    {
        ALERT(0, 0, "start - createSerialStream failed");
        return false;
    }
            
        // Looks like we're ok
    
    fInterface->retain();
    fWorkLoop->retain();
    fCommandGate->enable();

    XTRACE(this, 0, 0, "start - successful and IOModemSerialStreamSync created");
	Log(DEBUG_NAME ": Version number - %s\n", VersionNumber);
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCDMM::stop(IOService *provider)
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
//		Method:		AppleUSBCDCDMM::stopAction
//
//		Desc:		Dummy pass through for stopGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::stopAction(OSObject *owner, void *, void *, void *, void *)
{

    ((AppleUSBCDCDMM *)owner)->stopGated();
    
    return kIOReturnSuccess;
    
}/* end stopAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::stopGated
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Releases the resources 
//
/****************************************************************************************************/

void AppleUSBCDCDMM::stopGated()
{
    
    XTRACE(this, 0, 0, "stopGated");
    
    releaseResources();
	
}/* end stopGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::configureDMM
//
//		Inputs:		
//
//		Outputs:	return Code - true (configured), false (not configured)
//
//		Desc:		Configures the Device Management Model interface etc.
//
/****************************************************************************************************/

bool AppleUSBCDCDMM::configureDMM()
{
    
    XTRACE(this, 0, 0, "configureDMM");
    
    fInterfaceNumber = fInterface->GetInterfaceNumber();
    XTRACE(this, 0, fInterfaceNumber, "configureDMM - Interface number.");
    	
    if (!getFunctionalDescriptors())
    {
        XTRACE(this, 0, 0, "configureDMM - getFunctionalDescriptors failed");
        return false;
    }
    
    return true;

}/* end configureDMM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool AppleUSBCDCDMM::getFunctionalDescriptors()
{
    bool				gotDescriptors = false;
    UInt16				vers;
    UInt16				*hdrVers;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    HDRFunctionalDescriptor		*HDRFDesc;		// hearder functional descriptor
    DMMFunctionalDescriptor		*DMMFDesc;		// device management functional descriptor
       
    XTRACE(this, 0, 0, "getFunctionalDescriptors");
    
        // Get the associated functional descriptors

    do
    {
        funcDesc = (const FunctionalDescriptorHeader *)fInterface->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;				// We're done
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case Header_FunctionalDescriptor:
                    HDRFDesc = (HDRFunctionalDescriptor *)funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Header Functional Descriptor");
                    hdrVers = (UInt16 *)&HDRFDesc->bcdCDC1;
                    vers = USBToHostWord(*hdrVers);
                    if (vers > kUSBRel11)
                    {
                        XTRACE(this, vers, kUSBRel11, "getFunctionalDescriptors - Header descriptor version number is incorrect");
                    }
                    break;
				case DMM_FunctionalDescriptor:
                    DMMFDesc = (DMMFunctionalDescriptor *)funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - ACM Functional Descriptor");
                    fMax_Command = USBToHostWord(DMMFDesc->wMaxCommand);
                    break;
                case Union_FunctionalDescriptor:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Union Functional Descriptor");
					break;
                case CS_FunctionalDescriptor:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - CS Functional Descriptor");
                    break;
                default:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - unknown Functional Descriptor");
                    break;
            }
        }
    } while (!gotDescriptors);
    
    return true;
    
}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::createSuffix
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

bool AppleUSBCDCDMM::createSuffix(unsigned char *sufKey)
{
    
    IOReturn	rc;
    UInt8	serBuf[12];		// arbitrary size > 8
    OSNumber	*location;
    UInt32	locVal;
    SInt16	i, sig = 0;
    UInt8	indx;
    bool	keyOK = false;			
	
    XTRACE(this, 0, 0, "createSuffix");
	
    indx = fInterface->GetDevice()->GetSerialNumberStringIndex();	
    if (indx != 0)
    {	
            // Generate suffix key based on the serial number string (if reasonable <= 8 and > 0)	

        rc = fInterface->GetDevice()->GetStringDescriptor(indx, (char *)&serBuf, sizeof(serBuf));
        if (!rc)
        {
            if ((strlen((char *)&serBuf) < 9) && (strlen((char *)&serBuf) > 0))
            {
				strncpy((char *)sufKey, (const char *)&serBuf, strlen((char *)&serBuf));
//                strcpy((char *)sufKey, (const char *)&serBuf);
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
	
        location = (OSNumber *)fInterface->GetDevice()->getProperty(kUSBDevicePropertyLocationID);
        if (location)
        {
            locVal = location->unsigned32BitValue();		
            snprintf((char *)sufKey, (sizeof(locVal)*2)+1, "%x", (unsigned int)locVal);
			sig = strlen((const char *)sufKey)-1;
			for (i=sig; i>=0; i--)
			{
				if (sufKey[i] != '0')
				{
					break;
				}
			}
			sig = i + 1;
            keyOK = true;
        }
    }
    
        // Make it unique just in case there's more than one configuration on this device
    
    if (keyOK)
    {
        sufKey[sig] = Asciify((UInt8)fInterfaceNumber >> 4);
        if (sufKey[sig] != '0')
            sig++;
        sufKey[sig++] = Asciify((UInt8)fInterfaceNumber);
        sufKey[sig] = 0x00;
    }
	
    return keyOK;

}/* end createSuffix */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::createSerialStream
//
//		Inputs:		
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the nub
//
/****************************************************************************************************/

bool AppleUSBCDCDMM::createSerialStream()
{
    IOModemSerialStreamSync	*pNub = new IOModemSerialStreamSync;
    bool			ret;
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
	
	pNub->setProperty((const char *)hiddenTag, true);

    pNub->registerService();

    return true;
	
}/* end createSerialStream */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::acquirePort
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnExclusiveAccess, kIOReturnIOError and various others
//
//		Desc:		Set up for gated acquirePort call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::acquirePort(bool sleep, void *refCon)
{
    IOReturn                ret;
    
    XTRACE(this, refCon, sleep, "acquirePort");
	
		// Check for being acquired after stop has been issued and before start
		
	if (fTerminate || fStopping)
	{
		XTRACE(this, 0, 0, "acquirePort - Offline");
		return kIOReturnOffline;
	}
	
		// Make sure we have a valid workloop
	
	if (!fWorkLoop)
    {
        XTRACE(this, 0, 0, "acquirePort - No workLoop");
        return kIOReturnOffline;
    }

    retain();
    ret = fCommandGate->runAction(acquirePortAction, (void *)sleep);
    release();
    
    return ret;

}/* end acquirePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::acquirePortAction
//
//		Desc:		Dummy pass through for acquirePortGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::acquirePortAction(OSObject *owner, void *arg0, void *, void *, void *)
{

    return ((AppleUSBCDCDMM *)owner)->acquirePortGated((bool)arg0);
    
}/* end acquirePortAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::acquirePortGated
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

IOReturn AppleUSBCDCDMM::acquirePortGated(bool sleep)
{
    UInt32 	busyState = 0;
    IOReturn 	rtn = kIOReturnSuccess;

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
    
    setStructureDefaults();				// Set the default values
        
    fSessions++;					// Bump number of active sessions and turn on clear to send
    setStateGated(PD_RS232_S_CTS, PD_RS232_S_CTS);
        
    return kIOReturnSuccess;
        
}/* end acquirePortGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::releasePort
//
//		Inputs:		refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Set up for gated releasePort call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::releasePort(void *refCon)
{
    IOReturn	ret = kIOReturnSuccess;
    
    XTRACE(this, 0, 0, "releasePort");
    
    retain();
    ret = fCommandGate->runAction(releasePortAction);
    release();
        
    return ret;

}/* end releasePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::releasePortAction
//
//		Desc:		Dummy pass through for releasePortGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::releasePortAction(OSObject *owner, void *, void *, void *, void *)
{

    return ((AppleUSBCDCDMM *)owner)->releasePortGated();
    
}/* end releasePortAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::releasePortGated
//
//		Inputs:		
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		releasePort returns all the resources and does clean up.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::releasePortGated()
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
            
    fSessions--;					// reduce number of active sessions
            
    release(); 						// Dispose of the self-reference we took in acquirePortGated()
    
    XTRACE(this, 0, 0, "releasePort - Exit");
    
    return kIOReturnSuccess;
	
}/* end releasePortGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::getState
//
//		Inputs:		refCon - unused
//
//		Outputs:	Return value - port state
//
//		Desc:		Set up for gated getState call.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDMM::getState(void *refCon)
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
//		Method:		AppleUSBCDCDMM::getStateAction
//
//		Desc:		Dummy pass through for getStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::getStateAction(OSObject *owner, void *, void *, void *, void *)
{
    UInt32	newState;

    newState = ((AppleUSBCDCDMM *)owner)->getStateGated();
    
    return newState;
    
}/* end getStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::getStateGated
//
//		Inputs:		port - unused
//
//		Outputs:	return value - port state
//
//		Desc:		Get the state for the port.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDMM::getStateGated()
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
//		Method:		AppleUSBCDCDMM::setState
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

IOReturn AppleUSBCDCDMM::setState(UInt32 state, UInt32 mask, void *refCon)
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
//		Method:		AppleUSBCDCDMM::setStateAction
//
//		Desc:		Dummy pass through for setStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::setStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{

    return ((AppleUSBCDCDMM *)owner)->setStateGated((UInt32)arg0, (UInt32)arg1);
    
}/* end setStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::setStateGated
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

IOReturn AppleUSBCDCDMM::setStateGated(UInt32 state, UInt32 mask)
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
//		Method:		AppleUSBCDCDMM::watchState
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

IOReturn AppleUSBCDCDMM::watchState(UInt32 *state, UInt32 mask, void *refCon)
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
//		Method:		AppleUSBCDCDMM::watchStateAction
//
//		Desc:		Dummy pass through for watchStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::watchStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{

    return ((AppleUSBCDCDMM *)owner)->watchStateGated((UInt32 *)arg0, (UInt32)arg1);
    
}/* end watchStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::watchStateGated
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

IOReturn AppleUSBCDCDMM::watchStateGated(UInt32 *state, UInt32 mask)
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
//		Method:		AppleUSBCDCDMM::nextEvent
//
//		Inputs:		refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnOffline
//
//		Desc:		Not used by this driver.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDMM::nextEvent(void *refCon)
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
//		Method:		AppleUSBCDCDMM::executeEvent
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

IOReturn AppleUSBCDCDMM::executeEvent(UInt32 event, UInt32 data, void *refCon)
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
//		Method:		AppleUSBCDCDMM::executeEventAction
//
//		Desc:		Dummy pass through for executeEventGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::executeEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{

    return ((AppleUSBCDCDMM *)owner)->executeEventGated((UInt32)arg0, (UInt32)arg1);
    
}/* end executeEventAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::executeEventGated
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

IOReturn AppleUSBCDCDMM::executeEventGated(UInt32 event, UInt32 data)
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
//		Method:		AppleUSBCDCDMM::requestEvent
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

IOReturn AppleUSBCDCDMM::requestEvent(UInt32 event, UInt32 *data, void *refCon)
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
//		Method:		AppleUSBCDCDMM::enqueueEvent
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

IOReturn AppleUSBCDCDMM::enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon)
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
//		Method:		AppleUSBCDCDMM::dequeueEvent
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				refCon - unused
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen
//
//		Desc:		Not used by this driver.		
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon)
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
//		Method:		AppleUSBCDCDMM::enqueueData
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

IOReturn AppleUSBCDCDMM::enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon)
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
//		Method:		AppleUSBCDCDMM::enqueueDatatAction
//
//		Desc:		Dummy pass through for enqueueDataGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::enqueueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{

    return ((AppleUSBCDCDMM *)owner)->enqueueDataGated((UInt8 *)arg0, (UInt32)arg1, (UInt32 *)arg2, (bool)arg3);
    
}/* end enqueueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::enqueueDataGated
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

IOReturn AppleUSBCDCDMM::enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
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
//		Method:		AppleUSBCDCDMM::dequeueData
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

IOReturn AppleUSBCDCDMM::dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon)
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
//		Method:		AppleUSBCDCDMM::dequeueDatatAction
//
//		Desc:		Dummy pass through for dequeueDataGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::dequeueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{

    return ((AppleUSBCDCDMM *)owner)->dequeueDataGated((UInt8 *)arg0, (UInt32)arg1, (UInt32 *)arg2, (UInt32)arg3);
    
}/* end dequeueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::dequeueDataGated
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

IOReturn AppleUSBCDCDMM::dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
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
//		Method:		AppleUSBCDCDMM::setUpTransmit
//
//		Inputs:		
//
//		Outputs:	return code - true (transmit started), false (transmission already in progress)
//
//		Desc:		Setup and then start transmisson
//
/****************************************************************************************************/

bool AppleUSBCDCDMM::setUpTransmit()
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
//		Method:		AppleUSBCDCDMM::startTransmission
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Start the transmisson
//				Must be called from a gated method
//
/****************************************************************************************************/

void AppleUSBCDCDMM::startTransmission()
{
    size_t	count;
    IOReturn	ior;
    
    XTRACE(this, 0, 0, "startTransmission");
	
		// Check if we have a cr, if not just exit
	
	count = isCRinQueue(&fPort.TX);
	
	if (count <= 0)
    {
            // Updates all the status flags
			
        CheckQueues();
        return;
    }

        // Fill up the buffer with characters from the queue
		
    count = RemovefromQueue(&fPort.TX, fOutBuffer, count);
    
    setStateGated(PD_S_TX_BUSY, PD_S_TX_BUSY);
    
    XTRACE(this, fPort.State, count, "startTransmission - Bytes to write");
    LogData(kDataOut, count, fOutBuffer);
    
    ior = sendMERRequest(kUSBSEND_ENCAPSULATED_COMMAND, 0, count, fOutBuffer, &fMERCompletionInfo);
    if (ior != kIOReturnSuccess)
    {
        XTRACE(this, 0, ior, "startTransmission - sendMERRequest failed");
    } 

        // We just removed a bunch of stuff from the
        // queue, so see if we can free some thread(s)
        // to enqueue more stuff.
		
    CheckQueues();
	
}/* end startTransmission */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::sendMERRequest
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Set up and send a Management Element Request(MER).
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDMM::sendMERRequest(UInt8 request, UInt16 val, UInt16 len, UInt8 *buff, IOUSBCompletion *Comp)
{
	IOUSBDevRequest	*MER;
    IOReturn		rc = kIOReturnSuccess;

    XTRACE(this, 0, 0, "sendMERRequest");

	MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(this, 0, 0, "sendMERRequest - allocate MER failed");
        return kIOReturnError;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = request;
    MER->wValue = val;
    MER->wIndex = fInterfaceNumber;
	if (len > 0)
	{
		MER->wLength = len;
		MER->pData = buff;
	} else {
		MER->wLength = 0;
		MER->pData = NULL;
	}
    
    Comp->parameter = MER;
	
    rc = fInterface->GetDevice()->DeviceRequest(MER, Comp);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(this, MER->bRequest, rc, "sendMERRequest - error issueing DeviceRequest");
        IOFree(MER, sizeof(IOUSBDevRequest));
    }
	
	return rc;
	
}
	
/* end sendMERRequest */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::setLineCoding
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Set up and send SetLineCoding Management Element Request(MER) for all settings.
//
/****************************************************************************************************/

void AppleUSBCDCDMM::setLineCoding()
{
	IOReturn	ior;
	LineCoding  *lineParms;
    UInt16		lcLen = sizeof(LineCoding)-1;

    XTRACE(this, 0, 0, "setLineCoding");
	
	return;

    	// Check for changes and only do it if something's changed
	
    if ((fPort.BaudRate == fPort.LastBaudRate) && (fPort.StopBits == fPort.LastStopBits) && 
        (fPort.TX_Parity == fPort.LastTX_Parity) && (fPort.CharLength == fPort.LastCharLength))
    {
        return;
    }
	
	lineParms = (LineCoding *)IOMalloc(lcLen);
    if (!lineParms)
    {
        XTRACE(this, 0, 0, "setLineCoding - allocate lineParms failed");
        return;
    }
    bzero(lineParms, lcLen); 
	
        // Convert BaudRate - intel format doubleword (32 bits) 
		
    OSWriteLittleInt32(lineParms, dwDTERateOffset, fPort.BaudRate);
    lineParms->bCharFormat = fPort.StopBits - 2;
    lineParms->bParityType = fPort.TX_Parity - 1;
    lineParms->bDataBits = fPort.CharLength;

        // Now send it
		
	ior = sendMERRequest(kUSBSET_LINE_CODING, 0, lcLen, (UInt8 *)lineParms, &fMERCompletionInfo);
	if (ior != kIOReturnSuccess)
	{
		XTRACE(this, 0, ior, "setLineCoding - sendMERRequest failed");
		IOFree(lineParms, lcLen);
	}
	
}/* end setLineCoding */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::setControlLineState
//
//		Inputs:		RTS - true(set RTS), false(clear RTS)
//				DTR - true(set DTR), false(clear DTR)
//
//		Outputs:	
//
//		Desc:		Set up and send SetControlLineState Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCDMM::setControlLineState(bool RTS, bool DTR)
{
	IOReturn	ior;
    UInt16		CSBitmap = 0;
	
    XTRACE(this, 0, 0, "setControlLineState");
	
	return;
	
	if (RTS)
        CSBitmap |= kRTSOn;
    if (DTR)
        CSBitmap |= kDTROn;
    
		// Now send it
		
	ior = sendMERRequest(kUSBSET_CONTROL_LINE_STATE, CSBitmap, 0, NULL, &fMERCompletionInfo);
	if (ior != kIOReturnSuccess)
	{
		XTRACE(this, 0, ior, "setControlLineState - sendMERRequest failed");
	}
		
}/* end setControlLineState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::sendBreak
//
//		Inputs:		sBreak - true(set Break), false(clear Break)
//
//		Outputs:	
//
//		Desc:		Set up and send SendBreak Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCDMM::sendBreak(bool sBreak)
{
	IOReturn	ior;
    UInt16		breakVal = 0;
	
    XTRACE(this, 0, 0, "sendBreak");
	
	return;
	
	if (sBreak)
    {
        breakVal = 0xFFFF;
    }

		// Now send it
		
	ior = sendMERRequest(kUSBSEND_BREAK, breakVal, 0, NULL, &fMERCompletionInfo);
	if (ior != kIOReturnSuccess)
	{
		XTRACE(this, 0, ior, "sendBreak - sendMERRequest failed");
	}
			
}/* end sendBreak */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::initStructure
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Initialize the port structure
//
/****************************************************************************************************/

void AppleUSBCDCDMM::initStructure()
{
	
    XTRACE(this, 0, 0, "initStructure");

        // These are set up at start and should not be reset during execution.
        
    fPort.FCRimage = 0x00;
    fPort.IERmask = 0x00;

    fPort.State = (PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER);
    fPort.WatchStateMask = 0x00000000;
    
}/* end initStructure */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::setStructureDefaults
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Sets the defaults for the specified port structure
//
/****************************************************************************************************/

void AppleUSBCDCDMM::setStructureDefaults()
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
//		Method:		AppleUSBCDCDMM::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool AppleUSBCDCDMM::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;
	IOReturn					rtn;

    XTRACE(this, 0, 0, "allocateResources.");

        // Open all the end points and get the buffers

    if (!fInterface->open(this))
    {
        XTRACE(this, 0, 0, "allocateResources - open data interface failed.");
        return false;
    }

           // Interrupt pipe

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    fIntPipe = fInterface->FindNextPipe(0, &epReq);
    if (!fIntPipe)
    {
        XTRACE(this, 0, 0, "allocateResources - no interrrupt pipe.");
        return false;
    }
    XTRACE(this, epReq.maxPacketSize << 16 |epReq.interval, fIntPipe, "allocateResources - interrupt pipe.");
	fIntBufferSize = epReq.maxPacketSize;

        // Allocate Memory Descriptor Pointer with memory for the Interrupt pipe:

    fIntPipeMDP = IOBufferMemoryDescriptor::withCapacity(fIntBufferSize, kIODirectionIn);
    if (!fIntPipeMDP)
    {
        XTRACE(this, 0, 0, "allocateResources - Couldn't allocate MDP for interrupt pipe");
        return false;
    }

    fIntPipeBuffer = (UInt8*)fIntPipeMDP->getBytesNoCopy();
    XTRACE(this, 0, fIntPipeBuffer, "allocateResources - comm buffer");
	
		// Now the input and output buffers
	
	fInBuffer = (UInt8 *)IOMalloc(fMax_Command);
    if (!fInBuffer)
    {
        XTRACE(this, 0, 0, "allocateResources - allocate input buffer failed");
        return false;
    }
    bzero(fInBuffer, fMax_Command);
	
	fOutBuffer = (UInt8 *)IOMalloc(fMax_Command);
    if (!fOutBuffer)
    {
        XTRACE(this, 0, 0, "allocateResources - allocate output buffer failed");
        return false;
    }
    bzero(fOutBuffer, fMax_Command);
	
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

		// Read the interrupt pipe
		
    fIntCompletionInfo.target = this;
    fIntCompletionInfo.action = intReadComplete;
    fIntCompletionInfo.parameter = NULL;
		
    rtn = fIntPipe->Read(fIntPipeMDP, &fIntCompletionInfo, NULL);
    if (rtn != kIOReturnSuccess)
    {
		XTRACE(this, rtn, 0, "allocateResources - Read for interrupt pipe failed");
		return false;
	}
	
		// Set up the MER completion routine
		
    fMERCompletionInfo.target = this;
    fMERCompletionInfo.action = merWriteComplete;
    fMERCompletionInfo.parameter = NULL;
	
		// Set up the response completion routine
		
    fRspCompletionInfo.target = this;
    fRspCompletionInfo.action = rspComplete;
    fRspCompletionInfo.parameter = NULL;
    
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDMM::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCDMM::releaseResources()
{
    
    XTRACE(this, 0, 0, "releaseResources");
	
	if (fIntPipe)
	{
		fIntPipe->Abort();
	}
	
	if (fInterface)	
    { 
        fInterface->close(this);
        fInterface->release();
        fInterface = NULL;
    }
    
    if (fIntPipeMDP)	
    { 
        fIntPipeMDP->release();	
        fIntPipeMDP = 0; 
    }

#if 0	
	if (fInBuffer)
	{
		IOFree(fInBuffer, fMax_Command);
		fInBuffer = 0;
	}
#endif
	    
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
//		Method:		AppleUSBCDCDMM::freeRingBuffer
//
//		Inputs:		Queue - the specified queue to free
//
//		Outputs:	
//
//		Desc:		Frees all resources assocated with the queue, then sets all queue parameters 
//				to safe values.
//
/****************************************************************************************************/

void AppleUSBCDCDMM::freeRingBuffer(CirQueue *Queue)
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
//		Method:		AppleUSBCDCDMM::allocateRingBuffer
//
//		Inputs:		Queue - the specified queue to allocate
//				BufferSize - size to allocate
//
//		Outputs:	return Code - true (buffer allocated), false (it failed)
//
//		Desc:		Allocates resources needed by the queue, then sets up all queue parameters. 
//
/****************************************************************************************************/

bool AppleUSBCDCDMM::allocateRingBuffer(CirQueue *Queue, size_t BufferSize)
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
//		Method:		AppleUSBCDCDMM::message
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

IOReturn AppleUSBCDCDMM::message(UInt32 type, IOService *provider, void *argument)
{
	IOReturn	rtn;
    
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, fSessions, type, "message - kIOMessageServiceIsTerminated");
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
            if (fReadDead)
			{
				rtn = fIntPipe->Read(fIntPipeMDP, &fIntCompletionInfo, NULL);
				if (rtn != kIOReturnSuccess)
				{
					XTRACE(this, 0, rtn, "message - Read for interrupt-in pipe failed, still dead");
				} else {
					fReadDead = false;
				}
			}
            break;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
        case kIOUSBMessagePortHasBeenReset:
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenReset");
			if (fReadDead)
			{
				rtn = fIntPipe->Read(fIntPipeMDP, &fIntCompletionInfo, NULL);
				if (rtn != kIOReturnSuccess)
				{
					XTRACE(this, 0, rtn, "message - Read for interrupt-in pipe failed, still dead");
				} else {
					fReadDead = false;
				}
			}
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */