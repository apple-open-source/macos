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


    /* AppleUSBCDCDriver.cpp - MacOSX implementation of		*/
    /* USB Communication Device Class (CDC) Driver.		*/

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

#include "AppleUSBCDCDriver.h"

#define MIN_BAUD (50 << 1)

    // Globals

#if USE_ELG
    com_apple_iokit_XTrace	*gXTrace = 0;
    UInt32			gTraceID;
#endif

#define super IOSerialDriverSync

OSDefineMetaClassAndStructors(AppleUSBCDCDriver, IOSerialDriverSync);

/****************************************************************************************************/
//
//		Function:	Asciify
//
//		Inputs:		i - the nibble
//
//		Outputs:	return byte - ascii byte
//
//		Desc:		Converts to ascii. 
//
/****************************************************************************************************/
 
static UInt8 Asciify(UInt8 i)
{

    i &= 0xF;
    if (i < 10)
        return('0' + i);
    else return(55  + i);
	
}/* end Asciify */

#if USE_ELG
#define DEBUG_NAME "AppleUSBCDCDriver"

/****************************************************************************************************/
//
//		Function:	findKernelLogger
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Just like the name says
//
/****************************************************************************************************/

IOReturn findKernelLogger()
{
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    IOReturn		error = 0;
	
	// Get matching dictionary
	
    matchingDictionary = IOService::serviceMatching("com_apple_iokit_XTrace");
    if (!matchingDictionary)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[FindKernelLogger] Couldn't create a matching dictionary.\n");
        goto exit;
    }
	
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[FindKernelLogger] No XTrace logger found.\n");
        goto exit;
    }
	
	// User iterator to find each com_apple_iokit_XTrace instance. There should be only one, so we
	// won't iterate
	
    gXTrace = (com_apple_iokit_XTrace*)iterator->getNextObject();
    if (gXTrace)
    {
        IOLog(DEBUG_NAME "[FindKernelLogger] Found XTrace logger at %p.\n", gXTrace);
    }
	
exit:
	
    if (error != kIOReturnSuccess)
    {
        gXTrace = NULL;
        IOLog(DEBUG_NAME "[FindKernelLogger] Could not find a logger instance. Error = %X.\n", error);
    }
	
    if (matchingDictionary)
        matchingDictionary->release();
            
    if (iterator)
        iterator->release();
		
    return error;
    
}/* end findKernelLogger */
#endif

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

void USBLogData(UInt8 Dir, UInt32 Count, char *buf, PortInfo_t *port)
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
            XTRACE2(port, buf, Count, "USBLogData - Read Complete, address, size");
#else
            IOLog( "AppleUSBCDCDriver: USBLogData - Read Complete, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kDataOut:
#if USE_ELG
            XTRACE2(port, buf, Count, "USBLogData - Write, address, size");
#else
            IOLog( "AppleUSBCDCDriver: USBLogData - Write, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kDataOther:
#if USE_ELG
            XTRACE2(port, buf, Count, "USBLogData - Other, address, size");
#else
            IOLog( "AppleUSBCDCDriver: USBLogData - Other, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
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
        XTRACE2(port, 0, Count, "USBLogData - No data, Count=0");
#else
        IOLog( "AppleUSBCDCDriver: USBLogData - No data, Count=0\n" );
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
                XTRACE2(port, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Rx buffer dump");
                break;
            case kDataOut:
                XTRACE2(port, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Tx buffer dump");
                break;
            case kDataOther:
                XTRACE2(port, (w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "USBLogData - Misc buffer dump");
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

QueueStatus AppleUSBCDCDriver::AddBytetoQueue(CirQueue *Queue, char Value)
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

QueueStatus AppleUSBCDCDriver::GetBytetoQueue(CirQueue *Queue, UInt8 *Value)
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

QueueStatus AppleUSBCDCDriver::InitQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size)
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

QueueStatus AppleUSBCDCDriver::CloseQueue(CirQueue *Queue)
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

size_t AppleUSBCDCDriver::AddtoQueue(CirQueue *Queue, UInt8 *Buffer, size_t Size)
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

size_t AppleUSBCDCDriver::RemovefromQueue(CirQueue *Queue, UInt8 *Buffer, size_t MaxSize)
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

size_t AppleUSBCDCDriver::FreeSpaceinQueue(CirQueue *Queue)
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

size_t AppleUSBCDCDriver::UsedSpaceinQueue(CirQueue *Queue)
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

size_t AppleUSBCDCDriver::GetQueueSize(CirQueue *Queue)
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

QueueStatus AppleUSBCDCDriver::GetQueueStatus(CirQueue *Queue)
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
//		Inputs:		port - the port to check
//
//		Outputs:	None
//
//		Desc:		Checks the various queue's etc and manipulates the state(s) accordingly
//				Must be called from a gated method or completion routine.
//
/****************************************************************************************************/

void AppleUSBCDCDriver::CheckQueues(PortInfo_t *port)
{
    unsigned long	Used;
    unsigned long	Free;
    unsigned long	QueuingState;
    unsigned long	DeltaState;

	// Initialise the QueueState with the current state.
        
    QueuingState = port->State;

        // Check to see if there is anything in the Transmit buffer.
        
    Used = UsedSpaceinQueue(&port->TX);
    Free = FreeSpaceinQueue(&port->TX);
    
    XTRACE(port, Free, Used, "CheckQueues");
    
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
        
    if (Used < port->TXStats.LowWater)
         QueuingState |=  PD_S_TXQ_LOW_WATER;
    else QueuingState &= ~PD_S_TXQ_LOW_WATER;

    if (Used > port->TXStats.HighWater)
         QueuingState |= PD_S_TXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_TXQ_HIGH_WATER;


        // Check to see if there is anything in the Receive buffer.
        
    Used = UsedSpaceinQueue(&port->RX);
    Free = FreeSpaceinQueue(&port->RX);

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
    
    if (Used < port->RXStats.LowWater)
         QueuingState |= PD_S_RXQ_LOW_WATER;
    else QueuingState &= ~PD_S_RXQ_LOW_WATER;

    if (Used > port->RXStats.HighWater)
         QueuingState |= PD_S_RXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_RXQ_HIGH_WATER;

        // Figure out what has changed to get mask.
        
    DeltaState = QueuingState ^ port->State;
    setStateGated(QueuingState, DeltaState, port);
	
    return;
	
}/* end CheckQueues */

	// Encode the 4 modem status bits (so we only make one call to changeState below)

static UInt32 sMapModemStates[16] = 
{
	             0 |              0 |              0 |              0, // 0000
	             0 |              0 |              0 | PD_RS232_S_DCD, // 0001
	             0 |              0 | PD_RS232_S_DSR |              0, // 0010
	             0 |              0 | PD_RS232_S_DSR | PD_RS232_S_DCD, // 0011
	             0 | PD_RS232_S_BRK |              0 |              0, // 0100
	             0 | PD_RS232_S_BRK |              0 | PD_RS232_S_DCD, // 0101
	             0 | PD_RS232_S_BRK | PD_RS232_S_DSR |              0, // 0110
	             0 | PD_RS232_S_BRK | PD_RS232_S_DSR | PD_RS232_S_DCD, // 0111
	PD_RS232_S_RNG |              0 |              0 |              0, // 1000
	PD_RS232_S_RNG |              0 |              0 | PD_RS232_S_DCD, // 1001
	PD_RS232_S_RNG |              0 | PD_RS232_S_DSR |              0, // 1010
	PD_RS232_S_RNG |              0 | PD_RS232_S_DSR | PD_RS232_S_DCD, // 1011
	PD_RS232_S_RNG | PD_RS232_S_BRK |              0 |              0, // 1100
	PD_RS232_S_RNG | PD_RS232_S_BRK |              0 | PD_RS232_S_DCD, // 1101
	PD_RS232_S_RNG | PD_RS232_S_BRK | PD_RS232_S_DSR |              0, // 1110
	PD_RS232_S_RNG | PD_RS232_S_BRK | PD_RS232_S_DSR | PD_RS232_S_DCD, // 1111
};

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::commReadComplete
//
//		Inputs:		obj - me
//				param - the Port
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Interrupt pipe (Comm interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::commReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCDriver	*me = (AppleUSBCDCDriver*)obj;
    PortInfo_t 		*port = (PortInfo_t*)param;
    IOReturn		ior;
    UInt32		dLen;
    UInt16		*tState;
    UInt32		tempS, value, mask;
    
    XTRACE(port, rc, 0, "commReadComplete");
    
    if (me->fStopping)
        return;

    if (rc == kIOReturnSuccess)					// If operation returned ok
    {
        dLen = COMM_BUFF_SIZE - remaining;
        XTRACE(port, 0, dLen, "commReadComplete - data length");
		
            // Now look at the state stuff
            
//        LogData(kDataOther, dLen, port->CommPipeBuffer, port);
		
        if ((dLen > 7) && (port->CommPipeBuffer[1] == kUSBSERIAL_STATE))
        {
            tState = (UInt16 *)&port->CommPipeBuffer[8];
            tempS = USBToHostWord(*tState);
            XTRACE(port, 0, tempS, "commReadComplete - kUSBSERIAL_STATE");
			
            mask = sMapModemStates[15];				// All 4 on
            value = sMapModemStates[tempS & 15];		// now the status bits
            me->setStateGated(value, mask, port);
        }
    } else {
        XTRACE(port, 0, rc, "commReadComplete - error");
        if (rc != kIOReturnAborted)
        {
            rc = me->clearPipeStall(port, port->CommPipe);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(port, 0, rc, "dataReadComplete - clear stall failed (trying to continue)");
            }
        }
    }
    
        // Queue the next read only if not aborted
        
    if (rc != kIOReturnAborted)
    {
        ior = port->CommPipe->Read(port->CommPipeMDP, &port->fCommCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(port, 0, rc, "commReadComplete - Read io error");
        }
    }

    return;
	
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dataReadComplete
//
//		Inputs:		obj - me
//				param - the Port
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkIn pipe (Data interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::dataReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCDriver	*me = (AppleUSBCDCDriver*)obj;
    PortInfo_t 		*port = (PortInfo_t*)param;
    IOReturn		ior;
    size_t		length;
    
    XTRACE(port, rc, 0, "dataReadComplete");
    
    if (me->fStopping)
        return;

    if (rc == kIOReturnSuccess)				// If operation returned ok
    {
        length = DATA_BUFF_SIZE - remaining;
        XTRACE(port, port->State, length, "dataReadComplete - data length");
		
        LogData(kDataIn, length, port->PipeInBuffer, port);
	
            // Move the incoming bytes to the ring buffer
            
        me->AddtoQueue(&port->RX, port->PipeInBuffer, length);
        
        me->CheckQueues(port);
        
    } else {
        XTRACE(port, 0, rc, "dataReadComplete - error");
        if (rc != kIOReturnAborted)
        {
            rc = me->clearPipeStall(port, port->InPipe);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(port, 0, rc, "dataReadComplete - clear stall failed (trying to continue)");
            }
        }
    }
	
        // Queue the next read only if not aborted
        
    if (rc != kIOReturnAborted)
    {
        ior = port->InPipe->Read(port->PipeInMDP, &port->fReadCompletionInfo, NULL);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(port, 0, rc, "dataReadComplete - Read io err");
        }
    }

    return;
	
}/* end dataReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dataWriteComplete
//
//		Inputs:		obj - me
//				param - the Port
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkOut pipe (Data interface) write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::dataWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCDriver	*me = (AppleUSBCDCDriver *)obj;
    PortInfo_t 		*port = (PortInfo_t *)param;
    
    XTRACE(port, rc, 0, "dataWriteComplete");
    
    if (me->fStopping)
        return;

    if (rc == kIOReturnSuccess)	// If operation returned ok
    {	
        XTRACE(port, 0, (port->Count - remaining), "dataWriteComplete - data length");
        if (port->Count > 0)						// Check if it was a zero length write
        {
            if ((port->Count % port->OutPacketSize) == 0)		// If it was a multiple of max packet size then we need to do a zero length write
            {
                XTRACE(port, rc, (port->Count - remaining), "dataWriteComplete - writing zero length packet");
                port->PipeOutMDP->setLength(0);
                port->Count = 0;
                port->OutPipe->Write(port->PipeOutMDP, &port->fWriteCompletionInfo);
                return;
            }
        }

        me->CheckQueues(port);

        port->AreTransmitting = false;
        me->setStateGated(0, PD_S_TX_BUSY, port);
        me->setUpTransmit(port);					// just to keep it going??
        
//        if (!port->AreTransmitting)
//        {
//            me->setStateGated(0, PD_S_TX_BUSY, port);
//        }

    } else {
        XTRACE(port, 0, rc, "dataWriteComplete - io error");
        if (rc != kIOReturnAborted)
        {
            rc = me->clearPipeStall(port, port->OutPipe);
            if (rc != kIOReturnSuccess)
            {
                XTRACE(port, 0, rc, "dataWriteComplete - clear stall failed (trying to continue)");
            }
        }
        port->AreTransmitting = false;
        me->setStateGated(0, PD_S_TX_BUSY, port);
    }

    return;
	
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::merWriteComplete
//
//		Inputs:		obj - me
//				param - port 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Management Element Request write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    AppleUSBCDCDriver	*me = (AppleUSBCDCDriver *)obj;
    IOUSBDevRequest	*MER = (IOUSBDevRequest *)param;
    UInt16		dataLen;
    
    XTRACE(me, 0, remaining, "merWriteComplete");
    
    if (me->fStopping)
    {
        if (MER)
        {
            dataLen = MER->wLength;
            if ((dataLen != 0) && (MER->pData))
            {
                IOFree(MER->pData, dataLen);
            }
            IOFree(MER, sizeof(IOUSBDevRequest));
        }
        return;
    }
    
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
	
    return;
	
}/* end merWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::start(IOService *provider)
{
    UInt8	configs;	// number of device configurations
    UInt8	i;
	
    for (i=0; i<numberofPorts; i++)
    {
        fPorts[i] = NULL;
    }
    
    fSessions = 0;
    fTerminate = false;
    fStopping = false;
    
#if USE_ELG
    XTraceLogInfo	*logInfo;
    
    findKernelLogger();
    if (gXTrace)
    {
        gXTrace->retain();		// don't let it unload ...
        XTRACE(this, 0, 0xbeefbeef, "Hello from start");
        logInfo = gXTrace->LogGetInfo();
        IOLog("AppleUSBCDCDriver: start - Log is at %x\n", (unsigned int)logInfo);
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

	// Get my USB device provider - the device

    fpDevice = OSDynamicCast(IOUSBDevice, provider);
    if(!fpDevice)
    {
        ALERT(0, 0, "start - provider invalid");
        stop(provider);
        return false;
    }

	// Let's see if we have any configurations to play with
		
    configs = fpDevice->GetNumConfigurations();
    if (configs < 1)
    {
        ALERT(0, 0, "start - no configurations");
        stop(provider);
        return false;
    }
	
	// Now take control of the device and configure it
		
    if (!fpDevice->open(this))
    {
        ALERT(0, 0, "start - unable to open device");
        stop(provider);
        return false;
    }
    
        // get workloop
        
    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
    {
        ALERT(0, 0, "start - getWorkLoop failed");
        fpDevice->close(this);
        stop(provider);
        return false;
    }
    
    fWorkLoop->retain();
	
    if (!configureDevice(configs))
    {
        ALERT(0, 0, "start - configureDevice failed");
        cleanUp();
        fpDevice->close(this);
        stop(provider);
        return false;
    }
            
    XTRACE(this, 0, 0, "start - successful and IOModemSerialStreamSync created");
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCDriver::stop(IOService *provider)
{
    IOReturn	ret;
    UInt16	i;
    parmList	parms;
    
    XTRACE(this, 0, 0, "stop");
    
    fStopping = true;
    
    for (i=0; i<numberofPorts; i++)
    {
        if (fPorts[i] != NULL)
        {
            parms.arg1 = NULL;
            parms.arg2 = NULL;
            parms.arg3 = NULL;
            parms.arg4 = NULL;
            parms.port = fPorts[i];
    
            retain();
            ret = fPorts[i]->commandGate->runAction(stopAction, (void *)&parms);
            release();
            
            IOFree(fPorts[i], sizeof(PortInfo_t));
            fPorts[i] = NULL;
        }
    }
    
    removeProperty((const char *)propertyTag);
    
    if (fpDevice)
    {
        fpDevice->close(this);
        fpDevice = NULL;
    }
    
    super::stop(provider);
    
    return;

}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::stopAction
//
//		Desc:		Dummy pass through for stopGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::stopAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    ((AppleUSBCDCDriver *)owner)->stopGated(parms->port);
    
    return kIOReturnSuccess;
    
}/* end stopAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::stopGated
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Releases the interfaces, memory and general housekeeping 
//
/****************************************************************************************************/

void AppleUSBCDCDriver::stopGated(PortInfo_t *port)
{
    
    XTRACE(port, 0, 0, "stopGated");
    
    releaseResources(port);           
           
    return;
	
}/* end stopGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::cleanUp
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Clean up the ports.
//
/****************************************************************************************************/

void AppleUSBCDCDriver::cleanUp(void)
{
    UInt8	i;
    
    XTRACE(this, 0, 0, "cleanUp");

    for (i=0; i<numberofPorts; i++)
    {
        if (fPorts[i] != NULL)
        {
            releaseResources(fPorts[i]);
            IOFree(fPorts[i], sizeof(PortInfo_t));
            fPorts[i] = NULL;
        }
    }

    return;
	
}/* end cleanUp */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::configureDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (device configured), false (device not configured)
//
//		Desc:		Finds the configurations and then the appropriate interfaces etc.
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::configureDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;			// device request
    IOUSBInterface			*Comm;
    IOUSBInterface			*Dataintf = NULL;
    IOReturn				ior = kIOReturnSuccess;
    UInt8				i, dataindx;
    bool				portok = false;
    bool				goodCDC = false;
    PortInfo_t 				*port = NULL;
    
    XTRACE(this, 0, numConfigs, "configureDevice");
    	
        // Initialize and "configure" the device
        
    if (!initDevice(numConfigs))
    {
        XTRACE(this, 0, 0, "configureDevice - initDevice failed");
        return false;
    }

    OSBoolean *boolObj = OSDynamicCast(OSBoolean, fpDevice->getProperty("kDoNotSuspend"));
    if (boolObj && boolObj->isTrue())
    {
        fSuspendOK = false;
        XTRACE(this, 0, 0, "configureDevice - Suspended has been canceled for this device");
    }
    
    req.bInterfaceClass	= kUSBCommClass;
    req.bInterfaceSubClass = kUSBAbstractControlModel;
    req.bInterfaceProtocol = kUSBv25;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    Comm = fpDevice->FindNextInterface(NULL, &req);
    if (!Comm)
    {
        XTRACE(this, 0, 0, "configureDevice - Finding the first CDC interface failed");
    }

    while (Comm)
    {
        port = NULL;
        for (i=0; i<numberofPorts; i++)
        {
            if (fPorts[i] == NULL)
            {
                port = (PortInfo_t*)IOMalloc(sizeof(PortInfo_t));
                fPorts[i] = port;
                XTRACE(port, 0, i, "configureDevice - Port allocated");
                initStructure(port);
                break;
            }
        }
        if (!port)
        {
            XTRACE(this, 0, i, "configureDevice - No ports available or allocate failed");
        } else {
            port->CommInterface = Comm;
            port->CommInterface->retain();
            port->CommInterfaceNumber = Comm->GetInterfaceNumber();
            XTRACE(port, 0, port->CommInterfaceNumber, "configureDevice - Comm interface number.");
    	
            portok = getFunctionalDescriptors(port);
            
            if (portok)
            {
                req.bInterfaceClass = kUSBDataClass;
                req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
                req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
                req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
                do
                {
                    Dataintf = fpDevice->FindNextInterface(Dataintf, &req);
                    if (Dataintf)
                    {
                        dataindx = Dataintf->GetInterfaceNumber();
                        XTRACE(port, dataindx, port->DataInterfaceNumber, "configureDevice - finding Data interface");
                        if (dataindx == port->DataInterfaceNumber)
                        {
                            port->DataInterface = Dataintf;				// We've found our man
                            XTRACE(port, 0, 0, "configureDevice - found matching Data interface");
                            break;
                        }
                    }
                } while (Dataintf);
                
                if (!port->DataInterface)
                {
                    port->DataInterface = fpDevice->FindNextInterface(NULL, &req);	// Go with the first one (is this a good idea??)
                    if (!port->DataInterface)
                    {
                        XTRACE(port, 0, 0, "configureDevice - Find next interface for the Data Class failed");
                        portok = false;
                    } else {
                        XTRACE(port, 0, 0, "configureDevice - going with the first (only?) Data interface");
                    }
                }
                if (port->DataInterface)
                {
                    port->DataInterface->retain();
            
                        // Found both so now set the name for this port
	
                    if (createSerialStream(port))					// Publish SerialStream services
                    {
                        portok = initPort(port);					// Initilaize the Port structure
                        if (portok)
                        {
                            goodCDC = true;
                        } else {
                            XTRACE(port, 0, 0, "configureDevice - initPort failed");
                        }
                    } else {
                        XTRACE(port, 0, 0, "configureDevice - createSerialStream failed");
                        portok = false;
                    }
                }
            }
        }
        if (!portok)
        {
            if (port)
            {
                releaseResources(port);
                IOFree(port, sizeof(PortInfo_t));
                fPorts[i] = NULL;
            }
        }

            // see if there's another CDC interface
            
        req.bInterfaceClass = kUSBCommClass;
	req.bInterfaceSubClass = kUSBAbstractControlModel;
	req.bInterfaceProtocol = kUSBv25;
	req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            
        Comm = fpDevice->FindNextInterface(Comm, &req);
        if (!Comm)
        {
            XTRACE(port, 0, 0, "configureDevice - No more CDC interfaces");
        }
        portok = false;
    }
    
    if (fSuspendOK)
    {
        ior = fpDevice->SuspendDevice(true);         // Suspend the device (if supported, bus powered ONLY and not canceled)
        if (ior)
        {
            XTRACE(port, 0, ior, "AppleUSBCDCDriver::configureDevice - SuspendDevice error");
        }
    }
	
    return goodCDC;

}/* end configureDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::initDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (CDC present), false (CDC not present)
//
//		Desc:		Determines if this is a CDC compliant device and then sets the configuration
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::initDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;
    IOUSBDevRequest			devreq;
    const IOUSBConfigurationDescriptor	*cd = NULL;		// configuration descriptor
    IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
    IOReturn				ior = kIOReturnSuccess;
    UInt8				cval;
    UInt8				config = 0;
    bool				goodconfig = false;
       
    XTRACE(this, 0, numConfigs, "initDevice");
    	
        // Make sure we have a CDC interface to play with
        
    for (cval=0; cval<numConfigs; cval++)
    {
    	XTRACE(this, 0, cval, "initDevice - Checking Configuration");
		
     	cd = fpDevice->GetFullConfigurationDescriptor(cval);
     	if (!cd)
    	{
            XTRACE(this, 0, 0, "initDevice - Error getting the full configuration descriptor");
        } else {
            req.bInterfaceClass	= kUSBCommClass;
            req.bInterfaceSubClass = kUSBAbstractControlModel;
            req.bInterfaceProtocol = kUSBv25;
            req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
            if (ior == kIOReturnSuccess)
            {
                if (intf)
                {
                    XTRACE(this, 0, config, "initDevice - Interface descriptor found");
                    config = cd->bConfigurationValue;
                    goodconfig = true;					// We have at least one CDC interface in this configuration
                    break;
                } else {
                    XTRACE(this, 0, config, "initDevice - That's weird the interface was null");
                }
            } else {
                XTRACE(this, 0, cval, "initDevice - No CDC interface found this configuration");
            }
        }
    }
    
    if (goodconfig)
    {
        ior = fpDevice->SetConfiguration(this, config);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(this, 0, ior, "initDevice - SetConfiguration error");
            goodconfig = false;			
        }
    }
    
    if (!goodconfig)					// If we're not good - bail
        return false;
    
    fbmAttributes = cd->bmAttributes;
    XTRACE(this, fbmAttributes, kUSBAtrRemoteWakeup, "initDevice - Configuration bmAttributes");
    
    fSuspendOK = false;
    if (!(fbmAttributes & kUSBAtrSelfPowered))
    {
        if (fbmAttributes & kUSBAtrBusPowered)
        {
            fSuspendOK = true;
        }
    }
    if (fSuspendOK)
    {
        XTRACE(this, 0, 0, "initDevice - Suspend/Resume is active");
    } else {
        XTRACE(this, 0, 0, "initDevice - Suspend/Resume is inactive");
    }
    
    if (fbmAttributes & kUSBAtrRemoteWakeup)
    {
        getPMRootDomain()->publishFeature("WakeOnRing");
    
            // Clear the feature if wake-on-ring is not set (SetConfiguration sets the feature 
            // automatically if the device supports remote wake up)
    
        if (!WakeonRing())				
        {
            devreq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
            devreq.bRequest = kUSBRqClearFeature;
            devreq.wValue = kUSBFeatureDeviceRemoteWakeup;
            devreq.wIndex = 0;
            devreq.wLength = 0;
            devreq.pData = 0;

            ior = fpDevice->DeviceRequest(&devreq);
            if (ior == kIOReturnSuccess)
            {
                XTRACE(this, 0, ior, "initDevice - Clearing remote wake up feature successful");
            } else {
                XTRACE(this, 0, ior, "initDevice - Clearing remote wake up feature failed");
            }
        }
    } else {
        XTRACE(this, 0, 0, "initDevice - Remote wake up not supported");
    }
        
    return goodconfig;

}/* end initDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::getFunctionalDescriptors
//
//		Inputs:		port - the port
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::getFunctionalDescriptors(PortInfo_t *port)
{
    bool				gotDescriptors = false;
    bool				configok = true;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    CMFunctionalDescriptor		*CMFDesc;		// call management functional descriptor
    ACMFunctionalDescriptor		*ACMFDesc;		// abstract control management functional descriptor
       
    XTRACE(port, 0, 0, "getFunctionalDescriptors");
    
        // Set some defaults just in case and then get the associated functional descriptors
	
    port->CMCapabilities = CM_ManagementData + CM_ManagementOnData;
    port->ACMCapabilities = ACM_DeviceSuppControl + ACM_DeviceSuppBreak;
    port->DataInterfaceNumber = 0xff;
    
    do
    {
        (IOUSBDescriptorHeader*)funcDesc = port->CommInterface->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case Header_FunctionalDescriptor:
                    XTRACE(port, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Header Functional Descriptor");
                    break;
                case CM_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)CMFDesc = funcDesc;
                    XTRACE(port, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - CM Functional Descriptor");
                    port->DataInterfaceNumber = CMFDesc->bDataInterface;
                    port->CMCapabilities = CMFDesc->bmCapabilities;
				
                        // Check the configuration supports data management on the data interface (that's all we support)
				
                    if (!(port->CMCapabilities & CM_ManagementData))
                    {
                        XTRACE(port, 0, 0, "getFunctionalDescriptors - Interface doesn't support Call Management");
                        configok = false;
                    }
                    if (!(port->CMCapabilities & CM_ManagementOnData))
                    {
                        XTRACE(port, 0, 0, "getFunctionalDescriptors - Interface doesn't support Call Management on Data Interface");
                       //  configok = false;				// Some devices get this wrong so we'll let it slide
                    }
                    break;
                case ACM_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)ACMFDesc = funcDesc;
                    XTRACE(port, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - ACM Functional Descriptor");
                    port->ACMCapabilities = ACMFDesc->bmCapabilities;
                    break;
                case Union_FunctionalDescriptor:
                    XTRACE(port, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Union Functional Descriptor");
                    break;
                case CS_FunctionalDescriptor:
                    XTRACE(port, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - CS Functional Descriptor");
                    break;
                default:
                    XTRACE(port, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - unknown Functional Descriptor");
                    break;
            }
        }
    } while(!gotDescriptors);

    return configok;
    
}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::createSuffix
//
//		Inputs:		port - the port
//
//		Outputs:	return Code - true (suffix created), false (suffix not create)
//				sufKey - the key
//
//		Desc:		Creates the suffix key. It attempts to use the serial number string from the device
//				if it's reasonable i.e. less than 8 bytes ascii. Remember it's stored in unicode 
//				format. If it's not present or not reasonable it will generate the suffix based 
//				on the location property tag. At least this remains the same across boots if the
//				device is plugged into the same physical location. In the latter case trailing
//				zeros are removed. The interface number is also added to make it unique for
//				multiple CDC configuration devices.
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::createSuffix(PortInfo_t *port, unsigned char *sufKey)
{
    
    IOReturn	rc;
    UInt8	serBuf[12];		// arbitrary size > 8
    OSNumber	*location;
    UInt32	locVal;
    UInt8	*rlocVal;
    UInt16	offs, i, sig = 0;
    UInt8	indx;
    bool	keyOK = false;			
	
    XTRACE(port, 0, 0, "createSuffix");
	
    indx = fpDevice->GetSerialNumberStringIndex();	
    if (indx != 0)
    {	
            // Generate suffix key based on the serial number string (if reasonable <= 8 and > 0)	

        rc = fpDevice->GetStringDescriptor(indx, (char *)&serBuf, sizeof(serBuf));
        if (!rc)
        {
            if ((strlen((char *)&serBuf) < 9) && (strlen((char *)&serBuf) > 0))
            {
                strcpy((char *)sufKey, (const char *)&serBuf);
                sig = strlen((char *)sufKey);
                keyOK = true;
            }			
        } else {
            XTRACE(port, 0, rc, "createSuffix error reading serial number string");
        }
    }
	
    if (!keyOK)
    {
            // Generate suffix key based on the location property tag
	
        location = (OSNumber *)fpDevice->getProperty(kUSBDevicePropertyLocationID);
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
    
        // Make it unique if there's more than one configuration and config value is none zero
    
    if (keyOK)
    {
        sufKey[sig] = Asciify((UInt8)port->CommInterfaceNumber >> 4);
        if (sufKey[sig] != '0')
            sig++;
        sufKey[sig] = Asciify((UInt8)port->CommInterfaceNumber);
        if (sufKey[sig] != '0')
            sig++;			
        sufKey[sig] = 0x00;
    }
	
    return keyOK;

}/* end createSuffix */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::createSerialStream
//
//		Inputs:		port - the port
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the nub and port structure
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::createSerialStream(PortInfo_t *port)
{
    IOModemSerialStreamSync	*pNub = new IOModemSerialStreamSync;
    bool			ret;
    UInt8			indx;
    IOReturn			rc;
    unsigned char		rname[20];
    const char			*suffix = (const char *)&rname;
	
    XTRACE(port, 0, pNub, "createSerialStream");
    if (!pNub)
    {
        return false;
    }
		
    	// Either we attached and should get rid of our reference
    	// or we failed in which case we should get rid our reference as well.
        // This just makes sure the reference count is correct.
	
    ret = (pNub->init(0, port) && pNub->attach(this));
	
    pNub->release();
    if (!ret)
    {
        XTRACE(port, ret, 0, "createSerialStream - Failed to attach to the nub");
        return false;
    }

    // Report the base name to be used for generating device nodes
	
    pNub->setProperty(kIOTTYBaseNameKey, baseName);
	
    // Create suffix key and set it
	
    if (createSuffix(port, (unsigned char *)suffix))
    {		
        pNub->setProperty(kIOTTYSuffixKey, suffix);
    }

    pNub->registerService();
	
	// Save the Product String (at least the first productNameLength's worth). This is done (same name) per stream for the moment.

    indx = fpDevice->GetProductStringIndex();	
    if (indx != 0)
    {	
        rc = fpDevice->GetStringDescriptor(indx, (char *)&fProductName, sizeof(fProductName));
        if (!rc)
        {
            if (strlen((char *)fProductName) == 0)		// believe it or not this sometimes happens (null string with an index defined???)
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
//		Method:		AppleUSBCDCDriver::acquirePort
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnExclusiveAccess, kIOReturnIOError and various others
//
//		Desc:		Set up for gated acquirePort call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::acquirePort(bool sleep, void *refCon)
{
    IOReturn	ret;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    
    XTRACE(port, 0, sleep, "acquirePort");
    
    parms.arg1 = (void *)sleep;
    parms.arg2 = NULL;
    parms.arg3 = NULL;
    parms.arg4 = NULL;
    parms.port = port;
    
    retain();
    ret = port->commandGate->runAction(acquirePortAction, (void *)&parms);
    release();
    
    return ret;

}/* end acquirePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::acquirePortAction
//
//		Desc:		Dummy pass through for acquirePortGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::acquirePortAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    return ((AppleUSBCDCDriver *)owner)->acquirePortGated((bool)parms->arg1, parms->port);
    
}/* end acquirePortAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::acquirePortGated
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				port - the Port
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

IOReturn AppleUSBCDCDriver::acquirePortGated(bool sleep, PortInfo_t *port)
{
    UInt32 	busyState = 0;
    IOReturn 	rtn = kIOReturnSuccess;

    XTRACE(port, 0, sleep, "acquirePortGated");

    retain(); 							// Hold reference till releasePortgated, unless we fail to acquire
    while (true)
    {
        busyState = port->State & PD_S_ACQUIRED;
        if (!busyState)
        {		
                // Set busy bit (acquired), and clear everything else
                
            setStateGated((UInt32)PD_S_ACQUIRED | DEFAULT_STATE, (UInt32)STATE_ALL, port);
            break;
        } else {
            if (!sleep)
            {
                XTRACE(port, 0, 0, "acquirePortGated - Busy exclusive access");
                release();
            	return kIOReturnExclusiveAccess;
            } else {
            	busyState = 0;
            	rtn = watchStateGated(&busyState, PD_S_ACQUIRED, port);
            	if ((rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess))
                {
                    continue;
            	} else {
                    XTRACE(port, 0, 0, "acquirePortGated - Interrupted!");
                    release();
                    return rtn;
                }
            }
        }
    }
    
    do
    {
        if (fSuspendOK)
        {
            rtn = fpDevice->SuspendDevice(false);		// Resume the device
            if (rtn != kIOReturnSuccess)
            {
                break;
            }
        }
    
        IOSleep(50);
    	
        setStructureDefaults(port);			// Set the default values
	
            // Read the comm interrupt pipe for status:
		
        port->fCommCompletionInfo.target = this;
        port->fCommCompletionInfo.action = commReadComplete;
        port->fCommCompletionInfo.parameter = port;
		
        rtn = port->CommPipe->Read(port->CommPipeMDP, &port->fCommCompletionInfo, NULL);
        if (rtn == kIOReturnSuccess)
        {
        	// Read the data-in bulk pipe:
			
            port->fReadCompletionInfo.target = this;
            port->fReadCompletionInfo.action = dataReadComplete;
            port->fReadCompletionInfo.parameter = port;
		
            rtn = port->InPipe->Read(port->PipeInMDP, &port->fReadCompletionInfo, NULL);
            if (rtn == kIOReturnSuccess)
            {
                    // Set up the data-out bulk pipe:
			
                port->fWriteCompletionInfo.target = this;
                port->fWriteCompletionInfo.action = dataWriteComplete;
                port->fWriteCompletionInfo.parameter = port;
		
                    // Set up the management element request completion routine:

                port->fMERCompletionInfo.target = this;
                port->fMERCompletionInfo.action = merWriteComplete;
                port->fMERCompletionInfo.parameter = NULL;
            } else {
                break;
            }
        } else {
            break;
        }
        
        fSessions++;				// Bump number of active sessions and turn on clear to send
        setStateGated(PD_RS232_S_CTS, PD_RS232_S_CTS, port);
        
        return kIOReturnSuccess;
        
    } while (0);

    	// We failed for some reason

    setStateGated(0, STATE_ALL, port);		// Clear the entire state word
    release();
    
    return rtn;
	
}/* end acquirePortGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::releasePort
//
//		Inputs:		refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Set up for gated releasePort call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::releasePort(void *refCon)
{
    IOReturn	ret = kIOReturnSuccess;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    UInt16	i;
    bool	portok = false;
    
    XTRACE(port, 0, 0, "releasePort");
    
        // In some odd situations releasePort can occur after stop has been issued.
        // We need to check here to make sure the refcon/port is still valid.
        
    for (i=0; i<numberofPorts; i++)
    {
        if (fPorts[i] != NULL)
        {
            if (port = fPorts[i])
            {
                portok = true;
                break;
            }
        }
    }

    if (portok)
    {
        parms.arg1 = NULL;
        parms.arg2 = NULL;
        parms.arg3 = NULL;
        parms.arg4 = NULL;
        parms.port = port;
    
        retain();
        ret = port->commandGate->runAction(releasePortAction, (void *)&parms);
        release();
    } else {
        XTRACE(port, 0, 0, "releasePort - port is invalid");
    }
    
    return ret;

}/* end releasePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::releasePortAction
//
//		Desc:		Dummy pass through for releasePortGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::releasePortAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    return ((AppleUSBCDCDriver *)owner)->releasePortGated(parms->port);
    
}/* end acquirePortAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::releasePortGated
//
//		Inputs:		port - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		releasePort returns all the resources and does clean up.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::releasePortGated(PortInfo_t *port)
{
    UInt32 	busyState;
    IOReturn	ior;

    XTRACE(port, 0, 0, "releasePortGated");
    
    busyState = (port->State & PD_S_ACQUIRED);
    if (!busyState)
    {
        XTRACE(port, 0, 0, "AppleUSBCDCDriver::releasePort - NOT OPEN");
        if (fTerminate)
            return kIOReturnOffline;

        return kIOReturnNotOpen;
    }
	
    if (!fTerminate)
        USBSetControlLineState(port, false, false);		// clear RTS and clear DTR only if not terminated
	
    setStateGated(0, (UInt32)STATE_ALL, port);			// Clear the entire state word - which also deactivates the port
    
    port->CommPipe->Abort();					// Abort any outstanding reads
    port->InPipe->Abort();
    
    if (!fTerminate)
    {
        if (fSuspendOK)
        {
            ior = fpDevice->SuspendDevice(true);         // Suspend the device again (if supported and not unplugged)
            if (ior)
            {
                XTRACE(port, 0, ior, "releasePort - SuspendDevice error");
            }
        }
    }

    fSessions--;					// reduce number of active sessions
    if ((fTerminate) && (fSessions == 0))		// if it's the result of a terminate and session count is zero we also need to close the device
    {
    	fpDevice->close(this);
        fpDevice = NULL;
    } else {
        fpDevice->ReEnumerateDevice(0);			// Re-enumerate for now to clean up various problems
    }
    
    release(); 						// Dispose of the self-reference we took in acquirePortGated()
    
    XTRACE(port, 0, 0, "releasePort - Exit");
    
    return kIOReturnSuccess;
	
}/* end releasePortGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::getState
//
//		Inputs:		refCon - the Port
//
//		Outputs:	Return value - port state
//
//		Desc:		Set up for gated getState call.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDriver::getState(void *refCon)
{
    UInt32	currState;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    
    XTRACE(port, 0, 0, "getState");
    
    parms.arg1 = NULL;
    parms.arg2 = NULL;
    parms.arg3 = NULL;
    parms.arg4 = NULL;
    parms.port = port;
    
    retain();
    currState = port->commandGate->runAction(getStateAction, (void *)&parms);
    release();
    
    return currState;
    
}/* end getState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::getStateAction
//
//		Desc:		Dummy pass through for getStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::getStateAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    UInt32	newState;
    parmList	*parms = (parmList *)arg0;

    newState = ((AppleUSBCDCDriver *)owner)->getStateGated(parms->port);
    
    return newState;
    
}/* end getStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::getStateGated
//
//		Inputs:		port - the Port
//
//		Outputs:	return value - port state
//
//		Desc:		Get the state for the port.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDriver::getStateGated(PortInfo_t *port)
{
    UInt32 	state;
	
    XTRACE(port, 0, 0, "getStateGated");
	
    CheckQueues(port);
	
    state = port->State & EXTERNAL_MASK;
	
    XTRACE(port, state, EXTERNAL_MASK, "getState - Exit");
	
    return state;
	
}/* end getStateGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::setState
//
//		Inputs:		state - the state
//				mask - the mask
//				refCon - the Port
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnBadArgument
//
//		Desc:		Set up for gated setState call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::setState(UInt32 state, UInt32 mask, void *refCon)
{
    IOReturn	ret = kIOReturnSuccess;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    
    XTRACE(port, 0, 0, "setState");
    
        // Cannot acquire or activate via setState
    
    if (mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK)))
    {
        ret = kIOReturnBadArgument;
    } else {
        
            // ignore any bits that are read-only
        
        mask &= (~port->FlowControl & PD_RS232_A_MASK) | PD_S_MASK;
        if (mask)
        {
            parms.arg1 = (void *)state;
            parms.arg2 = (void *)mask;
            parms.arg3 = NULL;
            parms.arg4 = NULL;
            parms.port = port;
    
            retain();
            ret = port->commandGate->runAction(setStateAction, (void *)&parms);
            release();
        }
    }
    
    return ret;
    
}/* end setState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::setStateAction
//
//		Desc:		Dummy pass through for setStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::setStateAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    return ((AppleUSBCDCDriver *)owner)->setStateGated((UInt32)parms->arg1, (UInt32)parms->arg2, parms->port);
    
}/* end setStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::setStateGated
//
//		Inputs:		state - state to set
//				mask - state mask
//				port - the Port
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

IOReturn AppleUSBCDCDriver::setStateGated(UInt32 state, UInt32 mask, PortInfo_t *port)
{
    UInt32	delta;
	
    XTRACE(port, state, mask, "setStateGated");
    
    if (fTerminate)
        return kIOReturnOffline;
    
        // Check if it's being acquired or already acquired

    if ((state & PD_S_ACQUIRED) || (port->State & PD_S_ACQUIRED))
    {
        if (mask & PD_RS232_S_DTR)
        {
            if ((state & PD_RS232_S_DTR) != (port->State & PD_RS232_S_DTR))
            {
                if (state & PD_RS232_S_DTR)
                {
                    XTRACE(port, 0, 0, "setState - DTR TRUE");
                    USBSetControlLineState(port, false, true);
                } else {
                    XTRACE(port, 0, 0, "setState - DTR FALSE");
                    USBSetControlLineState(port, false, false);
                }
            }
        }
        
        state = (port->State & ~mask) | (state & mask); 		// compute the new state
        delta = state ^ port->State;		    			// keep a copy of the diffs
        port->State = state;

	    // Wake up all threads asleep on WatchStateMask
		
        if (delta & port->WatchStateMask)
        {
            port->commandGate->commandWakeup((void *)&port->State);
        }
        
        return kIOReturnSuccess;

    } else {
        XTRACE(port, port->State, 0, "setStateGated - Not Acquired");
    }
    
    return kIOReturnNotOpen;
	
}/* end setStateGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::watchState
//
//		Inputs:		state - state to watch for
//				mask - state mask bits
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from ::watchState
//
//		Desc:		Set up for gated watchState call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::watchState(UInt32 *state, UInt32 mask, void *refCon)
{
    IOReturn 	ret;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;

    XTRACE(port, *state, mask, "watchState");
    
    if (!state) 
        return kIOReturnBadArgument;
        
    if (!mask)
        return kIOReturnSuccess;
        
    parms.arg1 = (void *)state;
    parms.arg2 = (void *)mask;
    parms.arg3 = NULL;
    parms.arg4 = NULL;
    parms.port = port;

    retain();
    ret = port->commandGate->runAction(watchStateAction, (void *)&parms);
    release();
    return ret;

}/* end watchState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::watchStateAction
//
//		Desc:		Dummy pass through for watchStateGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::watchStateAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    return ((AppleUSBCDCDriver *)owner)->watchStateGated((UInt32 *)parms->arg1, (UInt32)parms->arg2, parms->port);
    
}/* end watchStateAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::watchStateGated
//
//		Inputs:		state - state to watch for
//				mask - state mask bits
//				port - the Port
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

IOReturn AppleUSBCDCDriver::watchStateGated(UInt32 *state, UInt32 mask, PortInfo_t *port)
{
    unsigned 	watchState, foundStates;
    bool 	autoActiveBit = false;
    IOReturn 	ret = kIOReturnNotOpen;

    XTRACE(port, *state, mask, "watchStateGated");
    
    if (fTerminate)
        return kIOReturnOffline;
        
    if (port->State & PD_S_ACQUIRED)
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
			
            foundStates = (watchState ^ ~port->State) & mask;

            if (foundStates)
            {
                *state = port->State;
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
			
            port->WatchStateMask |= mask;
            
            XTRACE(port, port->State, port->WatchStateMask, "watchStateGated - Thread sleeping");
            
            retain();								// Just to make sure all threads are awake
            port->commandGate->retain();					// before we're released
        
            ret = port->commandGate->commandSleep((void *)&port->State);
        
//            port->commandGate->retain();
            port->commandGate->release();
            
            XTRACE(port, port->State, ret, "watchStateGated - Thread restart");

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
		
        port->WatchStateMask = 0;
        XTRACE(port, *state, 0, "watchStateGated - Thread wakeing others");
        port->commandGate->commandWakeup((void *)&port->State);
 
        *state &= EXTERNAL_MASK;
    }
	
    XTRACE(port, ret, 0, "watchState - Exit");
    
    return ret;
	
}/* end watchStateGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::nextEvent
//
//		Inputs:		refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnOffline
//
//		Desc:		Not used by this driver.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDriver::nextEvent(void *refCon)
{
    PortInfo_t 	*port = (PortInfo_t *)refCon;

    XTRACE(port, 0, 0, "nextEvent");
    
    if (fTerminate)
        return kIOReturnOffline;
        
    if (getState(port) & PD_S_ACTIVE)
    {
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
	
}/* end nextEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::executeEvent
//
//		Inputs:		event - The event
//				data - any data associated with the event
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen or kIOReturnBadArgument
//
//		Desc:		Set up for gated executeEvent call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::executeEvent(UInt32 event, UInt32 data, void *refCon)
{
    IOReturn 	ret;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    
    XTRACE(port, data, event, "executeEvent");
    
    parms.arg1 = (void *)event;
    parms.arg2 = (void *)data;
    parms.arg3 = NULL;
    parms.arg4 = NULL;
    parms.port = port;
    
    retain();
    ret = port->commandGate->runAction(executeEventAction, (void *)&parms);
    release();

    return ret;
    
}/* end executeEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::executeEventAction
//
//		Desc:		Dummy pass through for executeEventGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::executeEventAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    return ((AppleUSBCDCDriver *)owner)->executeEventGated((UInt32)parms->arg1, (UInt32)parms->arg2, parms->port);
    
}/* end executeEventAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::executeEventGated
//
//		Inputs:		event - The event
//				data - any data associated with the event
//				port - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen or kIOReturnBadArgument
//
//		Desc:		executeEvent causes the specified event to be processed immediately.
//				This is primarily used for channel control commands like START & STOP
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::executeEventGated(UInt32 event, UInt32 data, PortInfo_t *port)
{
    IOReturn	ret = kIOReturnSuccess;
    UInt32 	state, delta;
	
    if (fTerminate)
        return kIOReturnOffline;
        
    delta = 0;
    state = port->State;	
    XTRACE(port, state, event, "executeEventGated");
	
    if ((state & PD_S_ACQUIRED) == 0)
        return kIOReturnNotOpen;

    switch (event)
    {
	case PD_RS232_E_XON_BYTE:
            XTRACE(port, data, event, "executeEventGated - PD_RS232_E_XON_BYTE");
            port->XONchar = data;
            break;
	case PD_RS232_E_XOFF_BYTE:
            XTRACE(port, data, event, "executeEventGated - PD_RS232_E_XOFF_BYTE");
            port->XOFFchar = data;
            break;
	case PD_E_SPECIAL_BYTE:
            XTRACE(port, data, event, "executeEventGated - PD_E_SPECIAL_BYTE");
            port->SWspecial[ data >> SPECIAL_SHIFT ] |= (1 << (data & SPECIAL_MASK));
            break;
	case PD_E_VALID_DATA_BYTE:
            XTRACE(port, data, event, "executeEventGated - PD_E_VALID_DATA_BYTE");
            port->SWspecial[ data >> SPECIAL_SHIFT ] &= ~(1 << (data & SPECIAL_MASK));
            break;
	case PD_E_FLOW_CONTROL:
            XTRACE(port, data, event, "executeEventGated - PD_E_FLOW_CONTROL");
            break;
	case PD_E_ACTIVE:
            XTRACE(port, data, event, "executeEventGated - PD_E_ACTIVE");
            if ((bool)data)
            {
                if (!(state & PD_S_ACTIVE))
                {
                    setStructureDefaults(port);
                    setStateGated((UInt32)PD_S_ACTIVE, (UInt32)PD_S_ACTIVE, port); 		// activate port
				
                    USBSetControlLineState(port, true, true);					// set RTS and set DTR
                }
            } else {
                if ((state & PD_S_ACTIVE))
                {
                    setStateGated(0, (UInt32)PD_S_ACTIVE, port);				// deactivate port
				
                    USBSetControlLineState(port, false, false);					// clear RTS and clear DTR
                }
            }
            break;
	case PD_E_DATA_LATENCY:
            XTRACE(port, data, event, "executeEventGated - PD_E_DATA_LATENCY");
            port->DataLatInterval = long2tval(data * 1000);
            break;
	case PD_RS232_E_MIN_LATENCY:
            XTRACE(port, data, event, "executeEventGated - PD_RS232_E_MIN_LATENCY");
            port->MinLatency = bool(data);
            break;
	case PD_E_DATA_INTEGRITY:
            XTRACE(port, data, event, "executeEventGated - PD_E_DATA_INTEGRITY");
            if ((data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE))
            {
                ret = kIOReturnBadArgument;
            } else {
                port->TX_Parity = data;
                port->RX_Parity = PD_RS232_PARITY_DEFAULT;
			
                USBSetLineCoding(port);			
            }
            break;
	case PD_E_DATA_RATE:
            XTRACE(port, data, event, "executeEventGated - PD_E_DATA_RATE");
            
                // For API compatiblilty with Intel.
                
            data >>= 1;
            XTRACE(port, data, 0, "executeEventGated - actual data rate");
            if ((data < MIN_BAUD) || (data > kMaxBaudRate))
            {
                ret = kIOReturnBadArgument;
            } else {
                port->BaudRate = data;
			
                USBSetLineCoding(port);			
            }		
            break;
	case PD_E_DATA_SIZE:
            XTRACE(port, data, event, "executeEventGated - PD_E_DATA_SIZE");
            
                // For API compatiblilty with Intel.
                
            data >>= 1;
            XTRACE(port, data, 0, "executeEventGated - actual data size");
            if ((data < 5) || (data > 8))
            {
                ret = kIOReturnBadArgument;
            } else {
                port->CharLength = data;
			
                USBSetLineCoding(port);			
            }
            break;
	case PD_RS232_E_STOP_BITS:
            XTRACE(port, data, event, "executeEventGated - PD_RS232_E_STOP_BITS");
            if ((data < 0) || (data > 20))
            {
                ret = kIOReturnBadArgument;
            } else {
                port->StopBits = data;
			
                USBSetLineCoding(port);
            }
            break;
	case PD_E_RXQ_FLUSH:
            XTRACE(port, data, event, "executeEventGated - PD_E_RXQ_FLUSH");
            break;
	case PD_E_RX_DATA_INTEGRITY:
            XTRACE(port, data, event, "executeEventGated - PD_E_RX_DATA_INTEGRITY");
            if ((data != PD_RS232_PARITY_DEFAULT) &&  (data != PD_RS232_PARITY_ANY))
            {
                ret = kIOReturnBadArgument;
            } else {
                port->RX_Parity = data;
            }
            break;
	case PD_E_RX_DATA_RATE:
            XTRACE(port, data, event, "executeEventGated - PD_E_RX_DATA_RATE");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_RX_DATA_SIZE:
            XTRACE(port, data, event, "executeEventGated - PD_E_RX_DATA_SIZE");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_RS232_E_RX_STOP_BITS:
            XTRACE(port, data, event, "executeEventGated - PD_RS232_E_RX_STOP_BITS");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_TXQ_FLUSH:
            XTRACE(port, data, event, "executeEventGated - PD_E_TXQ_FLUSH");
            break;
	case PD_RS232_E_LINE_BREAK:
            XTRACE(port, data, event, "executeEventGated - PD_RS232_E_LINE_BREAK");
            state &= ~PD_RS232_S_BRK;
            delta |= PD_RS232_S_BRK;
            setStateGated(state, delta, port);
            break;
	case PD_E_DELAY:
            XTRACE(port, data, event, "executeEventGated - PD_E_DELAY");
            port->CharLatInterval = long2tval(data * 1000);
            break;
	case PD_E_RXQ_SIZE:
            XTRACE(port, data, event, "executeEventGated - PD_E_RXQ_SIZE");
            break;
	case PD_E_TXQ_SIZE:
            XTRACE(port, data, event, "executeEventGated - PD_E_TXQ_SIZE");
            break;
	case PD_E_RXQ_HIGH_WATER:
            XTRACE(port, data, event, "executeEventGated - PD_E_RXQ_HIGH_WATER");
            break;
	case PD_E_RXQ_LOW_WATER:
            XTRACE(port, data, event, "executeEventGated - PD_E_RXQ_LOW_WATER");
            break;
	case PD_E_TXQ_HIGH_WATER:
            XTRACE(port, data, event, "executeEventGated - PD_E_TXQ_HIGH_WATER");
            break;
	case PD_E_TXQ_LOW_WATER:
            XTRACE(port, data, event, "executeEventGated - PD_E_TXQ_LOW_WATER");
            break;
	default:
            XTRACE(port, data, event, "executeEventGated - unrecognized event");
            ret = kIOReturnBadArgument;
            break;
    }
	
    return ret;
	
}/* end executeEventGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::requestEvent
//
//		Inputs:		event - The event
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument
//				data - any data associated with the event
//
//		Desc:		requestEvent processes the specified event as an immediate request and
//				returns the results in data.  This is primarily used for getting link
//				status information and verifying baud rate etc.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::requestEvent(UInt32 event, UInt32 *data, void *refCon)
{
    PortInfo_t	*port = (PortInfo_t *) refCon;
    IOReturn	returnValue = kIOReturnSuccess;

    XTRACE(port, 0, event, "requestEvent");
    
    if (fTerminate)
        return kIOReturnOffline;

    if (data == NULL) 
    {
        XTRACE(port, 0, event, "requestEvent - data is null");
        returnValue = kIOReturnBadArgument;
    } else {
        switch (event)
        {
            case PD_E_ACTIVE:
                XTRACE(port, 0, event, "requestEvent - PD_E_ACTIVE");
                *data = bool(getState(port) & PD_S_ACTIVE);	
                break;
            case PD_E_FLOW_CONTROL:
                XTRACE(port, port->FlowControl, event, "requestEvent - PD_E_FLOW_CONTROL");
                *data = port->FlowControl;							
                break;
            case PD_E_DELAY:
                XTRACE(port, 0, event, "requestEvent - PD_E_DELAY");
                *data = tval2long(port->CharLatInterval)/ 1000;	
                break;
            case PD_E_DATA_LATENCY:
                XTRACE(port, 0, event, "requestEvent - PD_E_DATA_LATENCY");
                *data = tval2long(port->DataLatInterval)/ 1000;	
                break;
            case PD_E_TXQ_SIZE:
                XTRACE(port, 0, event, "requestEvent - PD_E_TXQ_SIZE");
                *data = GetQueueSize(&port->TX);	
                break;
            case PD_E_RXQ_SIZE:
                XTRACE(port, 0, event, "requestEvent - PD_E_RXQ_SIZE");
                *data = GetQueueSize(&port->RX);	
                break;
            case PD_E_TXQ_LOW_WATER:
                XTRACE(port, 0, event, "requestEvent - PD_E_TXQ_LOW_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_LOW_WATER:
                XTRACE(port, 0, event, "requestEvent - PD_E_RXQ_LOW_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_HIGH_WATER:
                XTRACE(port, 0, event, "requestEvent - PD_E_TXQ_HIGH_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_HIGH_WATER:
                XTRACE(port, 0, event, "requestEvent - PD_E_RXQ_HIGH_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_AVAILABLE:
                XTRACE(port, 0, event, "requestEvent - PD_E_TXQ_AVAILABLE");
                *data = FreeSpaceinQueue(&port->TX);	 
                break;
            case PD_E_RXQ_AVAILABLE:
                XTRACE(port, 0, event, "requestEvent - PD_E_RXQ_AVAILABLE");
                *data = UsedSpaceinQueue(&port->RX); 	
                break;
            case PD_E_DATA_RATE:
                XTRACE(port, 0, event, "requestEvent - PD_E_DATA_RATE");
                *data = port->BaudRate << 1;		
                break;
            case PD_E_RX_DATA_RATE:
                XTRACE(port, 0, event, "requestEvent - PD_E_RX_DATA_RATE");
                *data = 0x00;					
                break;
            case PD_E_DATA_SIZE:
                XTRACE(port, 0, event, "requestEvent - PD_E_DATA_SIZE");
                *data = port->CharLength << 1;	
                break;
            case PD_E_RX_DATA_SIZE:
                XTRACE(port, 0, event, "requestEvent - PD_E_RX_DATA_SIZE");
                *data = 0x00;					
                break;
            case PD_E_DATA_INTEGRITY:
                XTRACE(port, 0, event, "requestEvent - PD_E_DATA_INTEGRITY");
                *data = port->TX_Parity;			
                break;
            case PD_E_RX_DATA_INTEGRITY:
                XTRACE(port, 0, event, "requestEvent - PD_E_RX_DATA_INTEGRITY");
                *data = port->RX_Parity;			
                break;
            case PD_RS232_E_STOP_BITS:
                XTRACE(port, 0, event, "requestEvent - PD_RS232_E_STOP_BITS");
                *data = port->StopBits << 1;		
                break;
            case PD_RS232_E_RX_STOP_BITS:
                XTRACE(port, 0, event, "requestEvent - PD_RS232_E_RX_STOP_BITS");
                *data = 0x00;					
                break;
            case PD_RS232_E_XON_BYTE:
                XTRACE(port, 0, event, "requestEvent - PD_RS232_E_XON_BYTE");
                *data = port->XONchar;			
                break;
            case PD_RS232_E_XOFF_BYTE:
                XTRACE(port, 0, event, "requestEvent - PD_RS232_E_XOFF_BYTE");
                *data = port->XOFFchar;			
                break;
            case PD_RS232_E_LINE_BREAK:
                XTRACE(port, 0, event, "requestEvent - PD_RS232_E_LINE_BREAK");
                *data = bool(getState(port) & PD_RS232_S_BRK);
                break;
            case PD_RS232_E_MIN_LATENCY:
                XTRACE(port, 0, event, "requestEvent - PD_RS232_E_MIN_LATENCY");
                *data = bool(port->MinLatency);		
                break;
            default:
                XTRACE(port, 0, event, "requestEvent - unrecognized event");
                returnValue = kIOReturnBadArgument; 			
                break;
        }
    }

    return kIOReturnSuccess;
	
}/* end requestEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::enqueueEvent
//
//		Inputs:		event - The event
//				data - any data associated with the event, 
//				sleep - true (wait for it), false (don't)
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Not used by this driver.
//				Events are passed on to executeEvent for immediate action.	
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon)
{
    IOReturn 	ret;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    
    XTRACE(port, data, event, "enqueueEvent");
    
    parms.arg1 = (void *)event;
    parms.arg2 = (void *)data;
    parms.arg3 = NULL;
    parms.arg4 = NULL;
    parms.port = port;
    
    retain();
    ret = port->commandGate->runAction(executeEventAction, (void *)&parms);
    release();

    return ret;
	
}/* end enqueueEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dequeueEvent
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen
//
//		Desc:		Not used by this driver.		
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
	
    XTRACE(port, 0, 0, "dequeueEvent");
    
    if (fTerminate)
        return kIOReturnOffline;

    if ((event == NULL) || (data == NULL))
        return kIOReturnBadArgument;

    if (getState(port) & PD_S_ACTIVE)
    {
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
	
}/* end dequeueEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::enqueueData
//
//		Inputs:		buffer - the data
//				size - number of bytes
//				sleep - true (wait for it), false (don't)
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument or value returned from watchState
//				count - bytes transferred  
//
//		Desc:		set up for enqueueDataGated call.	
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon)
{
    IOReturn 	ret;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    
    XTRACE(port, size, sleep, "enqueueData");
    
    if (count == NULL || buffer == NULL)
        return kIOReturnBadArgument;
    
    parms.arg1 = (void *)buffer;
    parms.arg2 = (void *)size;
    parms.arg3 = (void *)count;
    parms.arg4 = (void *)sleep;
    parms.port = port;
        
    retain();
    ret = port->commandGate->runAction(enqueueDataAction, (void *)&parms);
    release();

    return ret;
        
}/* end enqueueData */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::enqueueDatatAction
//
//		Desc:		Dummy pass through for equeueDataGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::enqueueDataAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    return ((AppleUSBCDCDriver *)owner)->enqueueDataGated((UInt8 *)parms->arg1, (UInt32)parms->arg2, (UInt32 *)parms->arg3, (bool)parms->arg4, parms->port);
    
}/* end enqueueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::enqueueDataGated
//
//		Inputs:		buffer - the data
//				size - number of bytes
//				sleep - true (wait for it), false (don't)
//				port - the Port
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

IOReturn AppleUSBCDCDriver::enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, PortInfo_t 	*port)
{
    UInt32 	state = PD_S_TXQ_LOW_WATER;
    IOReturn 	rtn = kIOReturnSuccess;

    XTRACE(port, size, sleep, "enqueueDataGated");

    if (fTerminate)
        return kIOReturnOffline;

    *count = 0;

    if (!(port->State & PD_S_ACTIVE))
        return kIOReturnNotOpen;

    XTRACE(port, port->State, size, "enqueueDataGated - current State");	
//    LogData(kDataOther, size, buffer, port);

        // OK, go ahead and try to add something to the buffer
        
    *count = AddtoQueue(&port->TX, buffer, size);
    CheckQueues(port);

        // Let the tranmitter know that we have something ready to go
    
    setUpTransmit(port);

        // If we could not queue up all of the data on the first pass and
        // the user wants us to sleep until it's all out then sleep

    while ((*count < size) && sleep)
    {
        state = PD_S_TXQ_LOW_WATER;
        rtn = watchStateGated(&state, PD_S_TXQ_LOW_WATER, port);
        if (rtn != kIOReturnSuccess)
        {
            XTRACE(port, 0, rtn, "enqueueDataGated - interrupted");
            return rtn;
        }

        *count += AddtoQueue(&port->TX, buffer + *count, size - *count);
        CheckQueues(port);

            // Let the tranmitter know that we have something ready to go.

        setUpTransmit(port);
    }

    XTRACE(port, *count, size, "enqueueDataGated - Exit");

    return kIOReturnSuccess;
	
}/* end enqueueDataGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dequeueData
//
//		Inputs:		size - buffer size
//				min - minimum bytes required
//				refCon - the Port (not used)
//
//		Outputs:	buffer - data returned
//				min - number of bytes
//				Return Code - kIOReturnSuccess, kIOReturnBadArgument, kIOReturnNotOpen, or value returned from watchState
//
//		Desc:		set up for enqueueDataGated call.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon)
{
    IOReturn 	ret;
    PortInfo_t	*port = (PortInfo_t *)refCon;
    parmList	parms;
    
    XTRACE(port, size, min, "dequeueData");
    
    if ((count == NULL) || (buffer == NULL) || (min > size))
        return kIOReturnBadArgument;
    
    parms.arg1 = (void *)buffer;
    parms.arg2 = (void *)size;
    parms.arg3 = (void *)count;
    parms.arg4 = (void *)min;
    parms.port = port;

    retain();
    ret = port->commandGate->runAction(dequeueDataAction, (void *)&parms);
    release();

    return ret;

}/* end dequeueData */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dequeueDatatAction
//
//		Desc:		Dummy pass through for equeueDataGated.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::dequeueDataAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    parmList	*parms = (parmList *)arg0;

    return ((AppleUSBCDCDriver *)owner)->dequeueDataGated((UInt8 *)parms->arg1, (UInt32)parms->arg2, (UInt32 *)parms->arg3, (UInt32)parms->arg4, parms->port);
    
}/* end dequeueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dequeueDataGated
//
//		Inputs:		size - buffer size
//				min - minimum bytes required
//				port - the Port
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

IOReturn AppleUSBCDCDriver::dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, PortInfo_t *port)
{
    IOReturn 	rtn = kIOReturnSuccess;
    UInt32 	state = 0;
    bool	goXOIdle;

    XTRACE(port, size, min, "dequeueDataGated");
    
    if (fTerminate)
        return kIOReturnOffline;
	
        // If the port is not active then there should not be any chars.
        
    *count = 0;
    if (!(port->State & PD_S_ACTIVE))
        return kIOReturnNotOpen;

        // Get any data living in the queue.
        
    *count = RemovefromQueue(&port->RX, buffer, size);
    CheckQueues(port);

    while ((min > 0) && (*count < min))
    {
            // Figure out how many bytes we have left to queue up
            
        state = 0;

        rtn = watchStateGated(&state, PD_S_RXQ_EMPTY, port);

        if (rtn != kIOReturnSuccess)
        {
            XTRACE(port, 0, rtn, "dequeueDataGated - Interrupted!");
            return rtn;
        }
        
            // Try and get more data starting from where we left off
            
        *count += RemovefromQueue(&port->RX, buffer + *count, (size - *count));
        CheckQueues(port);
		
    }

        // Now let's check our receive buffer to see if we need to stop
        
    goXOIdle = (UsedSpaceinQueue(&port->RX) < port->RXStats.LowWater) && (port->RXOstate == SENT_XOFF);

    if (goXOIdle)
    {
        port->RXOstate = IDLE_XO;
        AddBytetoQueue(&port->TX, port->XOFFchar);
        setUpTransmit(port);
    }

    XTRACE(port, *count, size, "dequeueData - Exit");

    return rtn;
	
}/* end dequeueDataGated */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::setUpTransmit
//
//		Inputs:		port - the port
//
//		Outputs:	return code - true (transmit started), false (transmission already in progress)
//
//		Desc:		Setup and then start transmisson on the port specified
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::setUpTransmit(PortInfo_t *port)
{

    XTRACE(port, 0, port->AreTransmitting, "setUpTransmit");
    
            // As a precaution just check we've not been terminated (may be a woken thread)
    
    if (fTerminate)
    {
        XTRACE(port, 0, 0, "setUpTransmit - terminating");
        return false;
    }
	
        //  If we are already in the cycle of transmitting characters,
        //  then we do not need to do anything.
		
    if (port->AreTransmitting == TRUE)
        return FALSE;

    if (UsedSpaceinQueue(&port->TX) > 0)
    {
        startTransmission(port);
    }

    return TRUE;
	
}/* end setUpTransmit */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::startTransmission
//
//		Inputs:		port - the port
//
//		Outputs:	None
//
//		Desc:		Start the transmisson on the port specified
//				Must be called from a gated method
//
/****************************************************************************************************/

void AppleUSBCDCDriver::startTransmission(PortInfo_t *port)
{
    size_t	count;
    IOReturn	ior;
    
    XTRACE(port, 0, 0, "startTransmission");

        // Sets up everything as we are running so as not to start this
        // port twice if a call occurs twice to this Method:
		
    port->AreTransmitting = TRUE;
    setStateGated(PD_S_TX_BUSY, PD_S_TX_BUSY, port);

        // Fill up the buffer with characters from the queue
		
    count = RemovefromQueue(&port->TX, port->PipeOutBuffer, MAX_BLOCK_SIZE);

        // If there are no bytes to send just exit:
		
    if (count <= 0)
    {
            // Updates all the status flags:
			
        CheckQueues(port);
        port->AreTransmitting = FALSE;
        setStateGated(0, PD_S_TX_BUSY, port);
        return;
    }
    
    XTRACE(port, port->State, count, "startTransmission - Bytes to write");
    LogData(kDataOut, count, port->PipeOutBuffer, port);	
    port->Count = count;

    port->PipeOutMDP->setLength(count);
    ior = port->OutPipe->Write(port->PipeOutMDP, &port->fWriteCompletionInfo);

        // We just removed a bunch of stuff from the
        // queue, so see if we can free some thread(s)
        // to enqueue more stuff.
		
    CheckQueues(port);

    return;
	
}/* end startTransmission */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::USBSetLineCoding
//
//		Inputs:		port - the port
//
//		Outputs:	None
//
//		Desc:		Set up and send SetLineCoding Management Element Request(MER) for all settings.
//
/****************************************************************************************************/

void AppleUSBCDCDriver::USBSetLineCoding(PortInfo_t *port)
{
    LineCoding		*lineParms;
    IOUSBDevRequest	*MER;
    IOReturn		rc;
    UInt16		lcLen = sizeof(LineCoding)-1;
	
    XTRACE(port, 0, port->CommInterfaceNumber, "USBSetLineCoding");
	
	// First check that Set Line Coding is supported
	
    if (!(port->ACMCapabilities & ACM_DeviceSuppControl))
    {
        return;
    }
	
	// Check for changes and only do it if something's changed
	
    if ((port->BaudRate == port->LastBaudRate) && (port->StopBits == port->LastStopBits) && 
        (port->TX_Parity == port->LastTX_Parity) && (port->CharLength == port->LastCharLength))
    {
        return;
    }
    	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(port, 0, 0, "USBSetLineCoding - allocate MER failed");
        return;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
    lineParms = (LineCoding*)IOMalloc(lcLen);
    if (!lineParms)
    {
        XTRACE(port, 0, 0, "USBSetLineCoding - allocate lineParms failed");
        IOFree(MER, sizeof(IOUSBDevRequest));
        return;
    }
    bzero(lineParms, lcLen); 
	
        // convert BaudRate - intel format doubleword (32 bits) 
		
    OSWriteLittleInt32(lineParms, dwDTERateOffset, port->BaudRate);
    port->LastBaudRate = port->BaudRate;
    lineParms->bCharFormat = port->StopBits - 2;
    port->LastStopBits = port->StopBits;
    lineParms->bParityType = port->TX_Parity - 1;
    port->LastTX_Parity = port->TX_Parity;
    lineParms->bDataBits = port->CharLength;
    port->LastCharLength = port->CharLength;
	
//    LogData(kDataOther, lcLen, lineParms, port);
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSET_LINE_CODING;
    MER->wValue = 0;
    MER->wIndex = port->CommInterfaceNumber;
    MER->wLength = lcLen;
    MER->pData = lineParms;
    
    port->fMERCompletionInfo.parameter = MER;
	
    rc = fpDevice->DeviceRequest(MER, &port->fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(port, MER->bRequest, rc, "USBSetLineCoding - error issueing DeviceRequest");
        IOFree(MER->pData, lcLen);
        IOFree(MER, sizeof(IOUSBDevRequest));
    }

}/* end USBSetLineCoding */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::USBSetControlLineState
//
//		Inputs:		port - the port
//				RTS - true(set RTS), false(clear RTS)
//				DTR - true(set DTR), false(clear DTR)
//
//		Outputs:	None
//
//		Desc:		Set up and send SetControlLineState Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCDriver::USBSetControlLineState(PortInfo_t *port, bool RTS, bool DTR)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt16		CSBitmap = 0;
	
    XTRACE(port, 0, port->CommInterfaceNumber, "USBSetControlLineState");
	
	// First check that Set Control Line State is supported
	
    if (!(port->ACMCapabilities & ACM_DeviceSuppControl))
    {
        return;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(port, 0, 0, "USBSetControlLineState - allocate MER failed");
        return;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSET_CONTROL_LINE_STATE;
    if (RTS)
        CSBitmap |= kRTSOn;
    if (DTR)
        CSBitmap |= kDTROn;
    MER->wValue = CSBitmap;
    MER->wIndex = port->CommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
    
    port->fMERCompletionInfo.parameter = MER;
	
    rc = fpDevice->DeviceRequest(MER, &port->fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(port, MER->bRequest, rc, "USBSetControlLineState - error issueing DeviceRequest");
        IOFree(MER, sizeof(IOUSBDevRequest));
    }

}/* end USBSetControlLineState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::USBSendBreak
//
//		Inputs:		port - the port
//				sBreak - true(set Break), false(clear Break)
//
//		Outputs:	None
//
//		Desc:		Set up and send SendBreak Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCDriver::USBSendBreak(PortInfo_t *port, bool sBreak)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
	
    XTRACE(port, 0, port->CommInterfaceNumber, "USBSendBreak");
	
	// First check that Send Break is supported
	
    if (!(port->ACMCapabilities & ACM_DeviceSuppBreak))
    {
        return;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
    {
        XTRACE(port, 0, 0, "USBSendBreak - allocate MER failed");
        return;
    }
    bzero(MER, sizeof(IOUSBDevRequest));
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSEND_BREAK;
    if (sBreak)
    {
        MER->wValue = 0xFFFF;
    } else {
        MER->wValue = 0;
    }
    MER->wIndex = port->CommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
    
    port->fMERCompletionInfo.parameter = MER;
	
    rc = fpDevice->DeviceRequest(MER, &port->fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
    {
        XTRACE(port, MER->bRequest, rc, "USBSendBreak - error issueing DeviceRequest");
        IOFree(MER, sizeof(IOUSBDevRequest));
    }

}/* end USBSendBreak */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::clearPipeStall
//
//		Inputs:		port - the port
//				thePipe - the pipe
//
//		Outputs:	
//
//		Desc:		Clear a stall (by reset) on the specified pipe. All outstanding I/O
//				is returned as aborted.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::clearPipeStall(PortInfo_t *port, IOUSBPipe *thePipe)
{
    UInt8	pipeStatus;
    IOReturn 	rtn = kIOReturnSuccess;
    
    XTRACE(port, 0, thePipe, "clearPipeStall");
    
    pipeStatus = thePipe->GetStatus();
    if (pipeStatus == kPipeStalled)
    {
        rtn = thePipe->Reset();
        if (rtn == kIOReturnSuccess)
        {
            XTRACE(port, 0, 0, "clearPipeStall - Successful");
        } else {
            XTRACE(port, 0, rtn, "clearPipeStall - Failed");
        }
    } else {
        XTRACE(port, 0, 0, "clearPipeStall - Pipe not stalled");
    }
    
    return rtn;

}/* end clearPipeStall */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::initPort
//
//		Inputs:		port - the port
//
//		Outputs:	return code - true (initialized), false (failed)
//
//		Desc:		Initialize the rest of port data
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::initPort(PortInfo_t *port)
{
	
    XTRACE(port, 0, 0, "initPort");
    
    port->commandGate = IOCommandGate::commandGate(this);
    if (!port->commandGate)
    {
        XTRACE(port, 0, 0, "initPort - commandGate failed");
        return false;
    }
    
    if (fWorkLoop->addEventSource(port->commandGate) != kIOReturnSuccess)
    {
        XTRACE(port, 0, 0, "initPort - addEventSource(commandGate) failed");
        return false;
    }

    port->commandGate->enable();
            
    if (!allocateResources(port)) 
    {
        XTRACE(port, 0, 0, "initPort - allocateResources failed");
        return false;
    }

    return true;

}/* end initPort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::initStructure
//
//		Inputs:		port - the port
//
//		Outputs:	none
//
//		Desc:		Initialize the port structure
//
/****************************************************************************************************/

void AppleUSBCDCDriver::initStructure(PortInfo_t *port)
{
	
    XTRACE(port, 0, 0, "initStructure");

        // These are set up at start and should not be reset during execution.
        
    port->FCRimage = 0x00;
    port->IERmask = 0x00;

    port->State = (PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER);
    port->WatchStateMask = 0x00000000;
    port->PipeOutMDP = NULL;
    port->PipeInMDP = NULL;
    port->CommPipeMDP = NULL;
    port->CommInterface = NULL;
    port->DataInterface = NULL;
    port->InPipe = NULL;
    port->OutPipe = NULL;
    port->CommPipe = NULL;
    port->CommPipeBuffer = NULL;
    port->PipeInBuffer = NULL;
    port->PipeOutBuffer = NULL;
    port->Count = -1;
    
}/* end initStructure */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::setStructureDefaults
//
//		Inputs:		port - the port
//
//		Outputs:	None
//
//		Desc:		Sets the defaults for the specified port structure
//
/****************************************************************************************************/

void AppleUSBCDCDriver::setStructureDefaults(PortInfo_t *port)
{
    UInt32	tmp;
	
    XTRACE(port, 0, 0, "setStructureDefaults");

    port->BaudRate = kDefaultBaudRate;			// 9600 bps
    port->LastBaudRate = 0;
    port->CharLength = 8;				// 8 Data bits
    port->LastCharLength = 0;
    port->StopBits = 2;					// 1 Stop bit
    port->LastStopBits = 0;
    port->TX_Parity = 1;				// No Parity
    port->LastTX_Parity	= 0;
    port->RX_Parity = 1;				// --ditto--
    port->MinLatency = false;
    port->XONchar = '\x11';
    port->XOFFchar = '\x13';
    port->FlowControl = 0x00000000;
    port->RXOstate = IDLE_XO;
    port->TXOstate = IDLE_XO;
    port->FrameTOEntry = NULL;

    port->RXStats.BufferSize = kMaxCirBufferSize;
    port->RXStats.HighWater = (port->RXStats.BufferSize << 1) / 3;
    port->RXStats.LowWater = port->RXStats.HighWater >> 1;
    port->TXStats.BufferSize = kMaxCirBufferSize;
    port->TXStats.HighWater = (port->RXStats.BufferSize << 1) / 3;
    port->TXStats.LowWater = port->RXStats.HighWater >> 1;

    port->FlowControl = (DEFAULT_AUTO | DEFAULT_NOTIFY);

    port->AreTransmitting = FALSE;

    for (tmp=0; tmp < (256 >> SPECIAL_SHIFT); tmp++)
        port->SWspecial[ tmp ] = 0;

    return;
	
}/* end setStructureDefaults */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::allocateResources
//
//		Inputs:		port - the Port
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::allocateResources(PortInfo_t *port)
{
    IOUSBFindEndpointRequest	epReq;		// endPoint request
    bool			goodCall;

    XTRACE(port, 0, 0, "allocateResources.");

        // Open all the end points and get the buffers

    goodCall = port->DataInterface->open(this);
    if (!goodCall)
    {
        XTRACE(port, 0, 0, "allocateResources - open data interface failed.");
        port->DataInterface->release();
        port->DataInterface = NULL;
        return false;
    }

    goodCall = port->CommInterface->open(this);
    if (!goodCall)
    {
        XTRACE(port, 0, 0, "allocateResources - open comm interface failed.");
        port->CommInterface->release();
        port->CommInterface = NULL;
        return false;
    }
    
        // Bulk In pipe

    epReq.type = kUSBBulk;
    epReq.direction = kUSBIn;
    epReq.maxPacketSize	= 0;
    epReq.interval = 0;
    port->InPipe = port->DataInterface->FindNextPipe(0, &epReq);
    if (!port->InPipe)
    {
        XTRACE(port, 0, 0, "allocateResources - no bulk input pipe.");
        return false;
    }
    port->InPacketSize = epReq.maxPacketSize;
    XTRACE(port, epReq.maxPacketSize << 16 |epReq.interval, port->InPipe, "allocateResources - bulk input pipe.");
    
        // Allocate Memory Descriptor Pointer with memory for the bulk in pipe:

    port->PipeInMDP = IOBufferMemoryDescriptor::withCapacity(DATA_BUFF_SIZE, kIODirectionIn);
    if (!port->PipeInMDP)
    {
        XTRACE(port, 0, 0, "allocateResources - Couldn't allocate MDP for bulk in pipe");
        return false;
    }
		
//    port->PipeInMDP->setLength(DATA_BUFF_SIZE);
    port->PipeInBuffer = (UInt8*)port->PipeInMDP->getBytesNoCopy();
    XTRACE(port, 0, port->PipeInBuffer, "allocateResources - input buffer");
    
        // Bulk Out pipe

    epReq.direction = kUSBOut;
    port->OutPipe = port->DataInterface->FindNextPipe(0, &epReq);
    if (!port->OutPipe)
    {
        XTRACE(port, 0, 0, "allocateResources - no bulk output pipe.");
        return false;
    }
    port->OutPacketSize = epReq.maxPacketSize;
    XTRACE(port, epReq.maxPacketSize << 16 |epReq.interval, port->OutPipe, "allocateResources - bulk output pipe.");
    
        // Allocate Memory Descriptor Pointer with memory for the bulk out pipe:

    port->PipeOutMDP = IOBufferMemoryDescriptor::withCapacity(MAX_BLOCK_SIZE, kIODirectionOut);
    if (!port->PipeOutMDP)
    {
        XTRACE(port, 0, 0, "allocateResources - Couldn't allocate MDP for bulk out pipe");
        return false;
    }
		
//    port->PipeOutMDP->setLength(MAX_BLOCK_SIZE);
    port->PipeOutBuffer = (UInt8*)port->PipeOutMDP->getBytesNoCopy();
    XTRACE(port, 0, port->PipeOutBuffer, "allocateResources - output buffer");

        // Interrupt pipe

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    port->CommPipe = port->CommInterface->FindNextPipe(0, &epReq);
    if (!port->CommPipe)
    {
        XTRACE(port, 0, 0, "allocateResources - no comm pipe.");
        return false;
    }
    XTRACE(port, epReq.maxPacketSize << 16 |epReq.interval, port->CommPipe, "allocateResources - comm pipe.");

        // Allocate Memory Descriptor Pointer with memory for the Interrupt pipe:

    port->CommPipeMDP = IOBufferMemoryDescriptor::withCapacity(COMM_BUFF_SIZE, kIODirectionIn);
    if (!port->CommPipeMDP)
    {
        XTRACE(port, 0, 0, "allocateResources - Couldn't allocate MDP for interrupt pipe");
        return false;
    }
		
//    port->CommPipeMDP->setLength(COMM_BUFF_SIZE);
    port->CommPipeBuffer = (UInt8*)port->CommPipeMDP->getBytesNoCopy();
    XTRACE(port, 0, port->CommPipeBuffer, "allocateResources - comm buffer");
    
        // Now the ring buffers
        
    if (!allocateRingBuffer(port, &port->TX, port->TXStats.BufferSize))
    {
        XTRACE(port, 0, 0, "allocateResources - Couldn't allocate TX ring buffer");
        return false;
    }
    
    XTRACE(port, 0, port->TX.Start, "allocateResources - TX ring buffer");
    
    if (!allocateRingBuffer(port, &port->RX, port->RXStats.BufferSize)) 
    {
        XTRACE(port, 0, 0, "allocateResources - Couldn't allocate RX ring buffer");
        return false;
    }
    
    XTRACE(port, 0, port->RX.Start, "allocateResources - RX ring buffer");

    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::releaseResources
//
//		Inputs:		port - the Port
//
//		Outputs:	None
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCDriver::releaseResources(PortInfo_t *port)
{
    XTRACE(port, 0, 0, "releaseResources");
	
    if (port->CommInterface)	
    {
        port->CommInterface->close(this);
        port->CommInterface->release();
        port->CommInterface = NULL;		
    }
	
    if (port->DataInterface)	
    { 
        port->DataInterface->close(this);
        port->DataInterface->release();
        port->DataInterface = NULL;
    }

    if (port->PipeOutMDP )	
    { 
        port->PipeOutMDP->release();	
        port->PipeOutMDP = 0; 
    }
	
    if (port->PipeInMDP  )	
    { 
        port->PipeInMDP->release();	
        port->PipeInMDP = 0; 
    }
	
    if (port->CommPipeMDP)	
    { 
        port->CommPipeMDP->release();	
        port->CommPipeMDP = 0; 
    }
    
    freeRingBuffer(port, &port->TX);
    freeRingBuffer(port, &port->RX);

    return;
	
}/* end releaseResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::freeRingBuffer
//
//		Inputs:		port - the port
//				Queue - the specified queue to free
//
//		Outputs:	None
//
//		Desc:		Frees all resources assocated with the queue, then sets all queue parameters 
//				to safe values.
//
/****************************************************************************************************/

void AppleUSBCDCDriver::freeRingBuffer(PortInfo_t *port, CirQueue *Queue)
{
    XTRACE(port, 0, Queue, "freeRingBuffer");

    if (Queue)
    {
        if (Queue->Start)
        {
            IOFree(Queue->Start, Queue->Size);
        }
        CloseQueue(Queue);
    }
    
    return;
	
}/* end freeRingBuffer */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::allocateRingBuffer
//
//		Inputs:		port - the port
//				Queue - the specified queue to allocate
//				BufferSize - size to allocate
//
//		Outputs:	return Code - true (buffer allocated), false (it failed)
//
//		Desc:		Allocates resources needed by the queue, then sets up all queue parameters. 
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::allocateRingBuffer(PortInfo_t *port, CirQueue *Queue, size_t BufferSize)
{
    UInt8	*Buffer;

        // Size is ignored and kMaxCirBufferSize, which is 4096, is used.
		
    XTRACE(port, 0, BufferSize, "allocateRingBuffer");
    Buffer = (UInt8*)IOMalloc(kMaxCirBufferSize);

    InitQueue(Queue, Buffer, kMaxCirBufferSize);

    if (Buffer)
        return true;

    return false;
	
}/* end allocateRingBuffer */

/****************************************************************************************************/
//
//		Function:	AppleUSBCDCDriver::WakeonRing
//
//		Inputs:		none
//
//		Outputs:	return code - true(Wake-on-Ring enabled), false(disabled)	
//
//		Desc:		Find the PMU entry and checks the wake-on-ring flag
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::WakeonRing(void)
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
//		Method:		AppleUSBCDCDriver::message
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

IOReturn AppleUSBCDCDriver::message(UInt32 type, IOService *provider, void *argument)
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
			"/System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/AppleUSBCDCDriver.kext",		// localizationPath
			"Unplug Header",		// the header
			"Unplug Notice",		// the notice - look in Localizable.strings
			"OK"); 
                }
            } else {
            
                    // Clean up before closing the device
            
                cleanUp();
                                
            	fpDevice->close(this); 	// Need to close so we can get the stop call (only if no sessions active - see releasePortGated)
                fpDevice = NULL;
            }
			
            fTerminate = true;		// We're being terminated (unplugged)
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
            break;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
        case kIOUSBMessagePortHasBeenReset:
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenReset");
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */
