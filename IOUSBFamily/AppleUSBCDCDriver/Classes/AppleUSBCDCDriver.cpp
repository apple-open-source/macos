/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
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

    static globals	g;	// Instantiate the globals

#define super IOSerialDriverSync

    OSDefineMetaClassAndStructors( AppleUSBCDCDriver, IOSerialDriverSync );

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
    if ( i < 10 )
        return( '0' + i );
    else return( 55  + i );
	
}/* end Asciify */

#if USE_ELG
/****************************************************************************************************/
//
//		Function:	AllocateEventLog
//
//		Inputs:		size - amount of memory to allocate
//
//		Outputs:	None
//
//		Desc:		Allocates the event log buffer
//
/****************************************************************************************************/

static void AllocateEventLog( UInt32 size )
{
    if ( g.evLogBuf )
        return;

    g.evLogFlag = 0;            // assume insufficient memory
    g.evLogBuf = (UInt8*)IOMalloc( size );
    if ( !g.evLogBuf )
    {
        kprintf( "AppleUSBCDCDriver evLog allocation failed " );
        return;
    }

    bzero( g.evLogBuf, size );
    g.evLogBufp	= g.evLogBuf;
    g.evLogBufe	= g.evLogBufp + kEvLogSize - 0x20; // ??? overran buffer?
    g.evLogFlag  = 0xFEEDBEEF;	// continuous wraparound
//	g.evLogFlag  = 'step';		// stop at each ELG
//	g.evLogFlag  = 0x0333;		// any nonzero - don't wrap - stop logging at buffer end

    IOLog( "AllocateEventLog - &globals=%8x buffer=%8x", (unsigned int)&g, (unsigned int)g.evLogBuf );

    return;
	
}/* end AllocateEventLog */

/****************************************************************************************************/
//
//		Function:	EvLog
//
//		Inputs:		a - anything, b - anything, ascii - 4 charater tag, str - any info string			
//
//		Outputs:	None
//
//		Desc:		Writes the various inputs to the event log buffer
//
/****************************************************************************************************/

static void EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
    register UInt32	*lp;           // Long pointer
    mach_timespec_t	time;

    if ( g.evLogFlag == 0 )
        return;

    IOGetTime( &time );

    lp = (UInt32*)g.evLogBufp;
    g.evLogBufp += 0x10;

    if ( g.evLogBufp >= g.evLogBufe )       // handle buffer wrap around if any
    {    
        g.evLogBufp  = g.evLogBuf;
        if ( g.evLogFlag != 0xFEEDBEEF )    // make 0xFEEDBEEF a symbolic ???
            g.evLogFlag = 0;                // stop tracing if wrap undesired
    }

        // compose interrupt level with 3 byte time stamp:

    *lp++ = (g.intLevel << 24) | ((time.tv_nsec >> 10) & 0x003FFFFF);   // ~ 1 microsec resolution
    *lp++ = a;
    *lp++ = b;
    *lp   = ascii;

    if( g.evLogFlag == 'step' )
    {	
        static char	code[ 5 ] = {0,0,0,0,0};
        *(UInt32*)&code = ascii;
        IOLog( "%8x AppleUSBCDCDriver: %8x %8x %s\n", time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code );
    }

    return;
	
}/* end EvLog */
#endif // USE_ELG

#if LOG_DATA
#define dumplen		32		// Set this to the number of bytes to dump and the rest should work out correct

#define buflen		((dumplen*2)+dumplen)+3
#define Asciistart	(dumplen*2)+3

/****************************************************************************************************/
//
//		Function:	USBLogData
//
//		Inputs:		Dir - direction, Count - number of bytes, buf - the data
//
//		Outputs:	None
//
//		Desc:		Puts the data in the log. 
//
/****************************************************************************************************/

static void USBLogData(UInt8 Dir, UInt32 Count, char *buf)
{
    UInt8	wlen, i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];

    for ( i=0; i<=buflen; i++ )
    {
        LocBuf[i] = 0x20;
    }
    LocBuf[i] = 0x00;
	
    if ( Dir == kUSBIn )
    {
        IOLog( "AppleUSBCDCDriver: USBLogData - Read Complete, size = %8x\n", Count );
    } else {
        if ( Dir == kUSBOut )
        {
            IOLog( "AppleUSBCDCDriver: USBLogData - Write, size = %8x\n", Count );
        } else {
            if ( Dir == kUSBAnyDirn )
            {
                IOLog( "AppleUSBCDCDriver: USBLogData - Other, size = %8x\n", Count );
            }
        }			
    }

    if ( Count > dumplen )
    {
        wlen = dumplen;
    } else {
        wlen = Count;
    }
	
    if ( wlen > 0 )
    {
        Aspnt = Asciistart;
        Hxpnt = 0;
        for ( i=1; i<=wlen; i++ )
        {
            wchr = buf[i-1];
            LocBuf[Hxpnt++] = Asciify( wchr >> 4 );
            LocBuf[Hxpnt++] = Asciify( wchr );
            if (( wchr < 0x20) || (wchr > 0x7F )) 		// Non printable characters
            {
                LocBuf[Aspnt++] = 0x2E;				// Replace with a period
            } else {
                LocBuf[Aspnt++] = wchr;
            }
        }
        LocBuf[(wlen + Asciistart) + 1] = 0x00;
        IOLog( LocBuf );
        IOLog( "\n" );
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
    } else {
        IOLog( "AppleUSBCDCDriver: USBLogData - No data, Count=0\n" );
    }
	
}/* end USBLogData */
#endif // LOG_DATA

/****************************************************************************************************/
//
//		Method:		AddBytetoQueue
//
//		Inputs:		Queue - the queue to be added to, Value - Byte to be added, queueRequestLock - queue lock
//
//		Outputs:	Queue status - full or no error
//
//		Desc:		Add a byte to the circular queue.
//				Check to see if there is space by comparing the next pointer,
//				with the last, If they match we are either Empty or full, so
//				check InQueue for zero.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCDriver::AddBytetoQueue( CirQueue *Queue, char Value, IOLock *queueRequestLock )
{

    if ( !queueRequestLock )
    {
        return queueFull;		// for lack of a better error
    }
    IOLockLock( queueRequestLock );
    
    if ( (Queue->NextChar == Queue->LastChar) && Queue->InQueue )
    {
        IOLockUnlock( queueRequestLock );
        return queueFull;
    }

    *Queue->NextChar++ = Value;
    Queue->InQueue++;

        // Check to see if we need to wrap the pointer.
		
    if ( Queue->NextChar >= Queue->End )
        Queue->NextChar =  Queue->Start;

    IOLockUnlock( queueRequestLock );
    return queueNoError;
	
}/* end AddBytetoQueue */

/****************************************************************************************************/
//
//		Method:		GetBytetoQueue
//
//		Inputs:		Queue - the queue to be removed from, queueRequestLock - queue lock
//
//		Outputs:	Value - where to put the byte, Queue status - empty or no error
//
//		Desc:		Remove a byte from the circular queue.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCDriver::GetBytetoQueue( CirQueue *Queue, UInt8 *Value, IOLock *queueRequestLock )
{
	
    if ( !queueRequestLock )
    {
        return queueEmpty;		// pretend it's empty
    }
    IOLockLock( queueRequestLock );
    
    if ( (Queue->NextChar == Queue->LastChar) && !Queue->InQueue )
    {
        IOLockUnlock( queueRequestLock );
        return queueEmpty;
    }

    *Value = *Queue->LastChar++;
    Queue->InQueue--;

        // Check to see if we need to wrap the pointer.
        
    if ( Queue->LastChar >= Queue->End )
        Queue->LastChar =  Queue->Start;

    IOLockUnlock( queueRequestLock );
    return queueNoError;
	
}/* end GetBytetoQueue */

/****************************************************************************************************/
//
//		Method:		InitQueue
//
//		Inputs:		Queue - the queue to be initialized, Buffer - the buffer, size - length of buffer
//
//		Outputs:	Queue status - queueNoError.
//
//		Desc:		Pass a buffer of memory and this routine will set up the internal data structures.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCDriver::InitQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size )
{
    Queue->Start	= Buffer;
    Queue->End		= (UInt8*)((size_t)Buffer + Size);
    Queue->Size		= Size;
    Queue->NextChar	= Buffer;
    Queue->LastChar	= Buffer;
    Queue->InQueue	= 0;

    IOSleep( 1 );
	
    return queueNoError ;
	
}/* end InitQueue */

/****************************************************************************************************/
//
//		Method:		CloseQueue
//
//		Inputs:		Queue - the queue to be closed
//
//		Outputs:	Queue status - queueNoError.
//
//		Desc:		Clear out all of the data structures.
//
/****************************************************************************************************/

QueueStatus AppleUSBCDCDriver::CloseQueue( CirQueue *Queue )
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
//		Inputs:		Queue - the queue to be added to, Buffer - data to add, Size - length of data
//
//		Outputs:	BytesWritten - Number of bytes actually put in the queue.
//
//		Desc:		Add an entire buffer to the queue.
//
/****************************************************************************************************/

size_t AppleUSBCDCDriver::AddtoQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size, IOLock *queueRequestLock )
{
    size_t	BytesWritten = 0;

    while ( FreeSpaceinQueue( Queue, queueRequestLock ) && (Size > BytesWritten) )
    {
        AddBytetoQueue( Queue, *Buffer++, queueRequestLock );
        BytesWritten++;
    }

    return BytesWritten;
	
}/* end AddtoQueue */

/****************************************************************************************************/
//
//		Method:		RemovefromQueue
//
//		Inputs:		Queue - the queue to be removed from, Size - size of buffer, queueRequestLock - queue lock
//
//		Outputs:	Buffer - Where to put the data, BytesReceived - Number of bytes actually put in Buffer.
//
//		Desc:		Get a buffers worth of data from the queue.
//
/****************************************************************************************************/

size_t AppleUSBCDCDriver::RemovefromQueue( CirQueue *Queue, UInt8 *Buffer, size_t MaxSize, IOLock *queueRequestLock )
{
    size_t	BytesReceived = 0;
    UInt8	Value;

    //  while( (GetBytetoQueue( Queue, &Value, queueRequestLock ) == queueNoError ) && (MaxSize >= BytesReceived) )
    while( (MaxSize > BytesReceived) && (GetBytetoQueue(Queue, &Value, queueRequestLock) == queueNoError) ) 
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
//		Inputs:		Queue - the queue to be queried, queueRequestLock - queue lock
//
//		Outputs:	Return Value - Free space left
//
//		Desc:		Return the amount of free space left in this buffer.
//
/****************************************************************************************************/

size_t AppleUSBCDCDriver::FreeSpaceinQueue( CirQueue *Queue, IOLock *queueRequestLock )
{
    size_t	retVal = 0;

    if ( !queueRequestLock )
    {
        return retVal;
    }
    IOLockLock( queueRequestLock );
    
    retVal = Queue->Size - Queue->InQueue;

    IOLockUnlock( queueRequestLock );
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

size_t AppleUSBCDCDriver::UsedSpaceinQueue( CirQueue *Queue )
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

size_t AppleUSBCDCDriver::GetQueueSize( CirQueue *Queue )
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

QueueStatus AppleUSBCDCDriver::GetQueueStatus( CirQueue *Queue )
{
    if ( (Queue->NextChar == Queue->LastChar) && Queue->InQueue )
        return queueFull;
    else if ( (Queue->NextChar == Queue->LastChar) && !Queue->InQueue )
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
//
/****************************************************************************************************/

void AppleUSBCDCDriver::CheckQueues( PortInfo_t *port )
{
    unsigned long	Used;
    unsigned long	Free;
    unsigned long	QueuingState;
    unsigned long	DeltaState;

	// Initialise the QueueState with the current state.
        
    QueuingState = readPortState( port );

        // Check to see if there is anything in the Transmit buffer.
        
    Used = UsedSpaceinQueue( &port->TX );
    Free = FreeSpaceinQueue( &port->TX, port->TXqueueRequestLock );
//	ELG( Free, Used, 'CkQs', "CheckQueues" );
    if ( Free == 0 )
    {
        QueuingState |=  PD_S_TXQ_FULL;
        QueuingState &= ~PD_S_TXQ_EMPTY;
    } else {
        if ( Used == 0 )
	{
            QueuingState &= ~PD_S_TXQ_FULL;
            QueuingState |=  PD_S_TXQ_EMPTY;
        } else {
            QueuingState &= ~PD_S_TXQ_FULL;
            QueuingState &= ~PD_S_TXQ_EMPTY;
        }
    }

    	// Check to see if we are below the low water mark.
        
    if ( Used < port->TXStats.LowWater )
         QueuingState |=  PD_S_TXQ_LOW_WATER;
    else QueuingState &= ~PD_S_TXQ_LOW_WATER;

    if ( Used > port->TXStats.HighWater )
         QueuingState |= PD_S_TXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_TXQ_HIGH_WATER;


        // Check to see if there is anything in the Receive buffer.
        
    Used = UsedSpaceinQueue( &port->RX );
    Free = FreeSpaceinQueue( &port->RX, port->RXqueueRequestLock );

    if ( Free == 0 )
    {
        QueuingState |= PD_S_RXQ_FULL;
        QueuingState &= ~PD_S_RXQ_EMPTY;
    } else {
        if ( Used == 0 )
	{
            QueuingState &= ~PD_S_RXQ_FULL;
            QueuingState |= PD_S_RXQ_EMPTY;
        } else {
            QueuingState &= ~PD_S_RXQ_FULL;
            QueuingState &= ~PD_S_RXQ_EMPTY;
        }
    }

        // Check to see if we are below the low water mark.
    
    if ( Used < port->RXStats.LowWater )
         QueuingState |= PD_S_RXQ_LOW_WATER;
    else QueuingState &= ~PD_S_RXQ_LOW_WATER;

    if ( Used > port->RXStats.HighWater )
         QueuingState |= PD_S_RXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_RXQ_HIGH_WATER;

        // Figure out what has changed to get mask.
        
    DeltaState = QueuingState ^ readPortState( port );
    changeState( port, QueuingState, DeltaState );
	
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
//		Inputs:		obj - me, param - parameter block(the Port), rc - return code, remaining - what's left
//												(whose idea was that?)
//
//		Outputs:	None
//
//		Desc:		Interrupt pipe (Comm interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::commReadComplete( void *obj, void *param, IOReturn rc, UInt32 remaining )
{
    AppleUSBCDCDriver	*me = (AppleUSBCDCDriver*)obj;
    PortInfo_t 		*port = (PortInfo_t*)param;
    IOReturn		ior;
    UInt32		dLen;
    UInt16		*tState;
    UInt32		tempS, value, mask;

    if ( rc == kIOReturnSuccess )	// If operation returned ok
    {
        dLen = COMM_BUFF_SIZE - remaining;
        ELG( rc, dLen, 'cRC+', "commReadComplete" );
		
            // Now look at the state stuff
            
        LogData( kUSBAnyDirn, dLen, port->CommPipeBuffer );
		
        if ((dLen > 7) && (port->CommPipeBuffer[1] == kUSBSERIAL_STATE))
        {
            tState = (UInt16 *)&port->CommPipeBuffer[8];
            tempS = USBToHostWord(*tState);
            ELG( 0, tempS, 'cRUS', "commReadComplete - kUSBSERIAL_STATE" );
			
            mask = sMapModemStates[15];				// All 4 on
            value = sMapModemStates[tempS & 15];		// now the status bits
            me->changeState(port, value, mask);
        }
		
            // Queue the next read
	
        ior = port->CommPipe->Read( port->CommPipeMDP, &me->fCommCompletionInfo, NULL );
        if ( ior == kIOReturnSuccess )
            return;
    }

        // Read returned with error OR next interrupt read failed to be queued

    ELG( 0, rc, 'cRC-', "commReadComplete - error" );

    return;
	
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dataReadComplete
//
//		Inputs:		obj - me, param - parameter block(the Port), rc - return code, remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkIn pipe (Data interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::dataReadComplete( void *obj, void *param, IOReturn rc, UInt32 remaining )
{
    AppleUSBCDCDriver	*me = (AppleUSBCDCDriver*)obj;
    PortInfo_t 		*port = (PortInfo_t*)param;
    IOReturn		ior;

    if ( rc == kIOReturnSuccess )	// If operation returned ok
    {	
        ELG( port->State, 128 - remaining, 'dRC+', "dataReadComplete" );
		
        LogData( kUSBIn, (128 - remaining), port->PipeInBuffer );
	
            // Move the incoming bytes to the ring buffer
            
        me->AddtoQueue( &port->RX, port->PipeInBuffer, 128 - remaining, port->RXqueueRequestLock );
	
            // Queue the next read
	
        ior = port->InPipe->Read( port->PipeInMDP, &me->fReadCompletionInfo, NULL );
        if ( ior == kIOReturnSuccess )
        {
            me->CheckQueues( port );
            return;
        }
    }

        // Read returned with error OR next bulk read failed to be queued

    ELG( 0, rc, 'dRe-', "AppleUSBCDCDriver::dataReadComplete - io err" );

    return;
	
}/* end dataReadComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dataWriteComplete
//
//		Inputs:		obj - me, param - parameter block(the Port), rc - return code, remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkOut pipe (Data interface) write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::dataWriteComplete( void *obj, void *param, IOReturn rc, UInt32 remaining )
{
    AppleUSBCDCDriver	*me = (AppleUSBCDCDriver*)obj;
    PortInfo_t 		*port = (PortInfo_t*)param;

    if ( rc == kIOReturnSuccess )	// If operation returned ok
    {	
        ELG( rc, (port->Count - remaining), 'dWC+', "dataWriteComplete" );
        if ( port->Count > 0 )							// Check if it was a zero length write
        {
            if ( (port->Count % port->OutPacketSize) == 0 )			// If it was a multiple of max packet size then we need to do a zero length write
            {
                ELG( rc, (port->Count - remaining), 'dWCz', "dataWriteComplete - writing zero length packet" );
                port->PipeOutMDP->setLength( 0 );
                port->Count = 0;
                port->OutPipe->Write( port->PipeOutMDP, &me->fWriteCompletionInfo );
                return;
            }
        }

        me->CheckQueues( port );

        port->AreTransmitting = false;
        me->SetUpTransmit( port );					// just to keep it going??
        if ( !port->AreTransmitting )
            me->changeState( port, 0, PD_S_TX_BUSY );	/// mlj

        return;
    }

    ELG( 0, rc, 'dWe-', "AppleUSBCDCDriver::dataWriteComplete - io err" );
    port->AreTransmitting = false;
    return;
	
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::merWriteComplete
//
//		Inputs:		obj - me, param - parameter block (may or may not be present depending on request), 
//				rc - return code, remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Management element request write completion routine
//
/****************************************************************************************************/

void AppleUSBCDCDriver::merWriteComplete( void *obj, void *param, IOReturn rc, UInt32 remaining )
{

    IOUSBDevRequest	*MER = (IOUSBDevRequest*)param;
    UInt16		dataLen;
	
    if (MER)
    {
        if ( rc == kIOReturnSuccess )
        {
            ELG( MER->bRequest, remaining, 'mWC+', "AppleUSBCDCDriver::merWriteComplete" );
        } else {
            ELG( MER->bRequest, rc, 'mWC-', "AppleUSBCDCDriver::merWriteComplete - io err" );
        }
		
        dataLen = MER->wLength;
        ELG( 0, dataLen, 'mWC ', "AppleUSBCDCDriver::merWriteComplete - data length" );
        if ((dataLen != 0) && (MER->pData))
        {
            IOFree( MER->pData, dataLen );
        }
        IOFree( MER, sizeof(IOUSBDevRequest) );
		
    } else {
        if ( rc == kIOReturnSuccess )
        {
            ELG( 0, remaining, 'mWr+', "AppleUSBCDCDriver::merWriteComplete (request unknown)" );
        } else {
            ELG( 0, rc, 'rWr-', "AppleUSBCDCDriver::merWriteComplete (request unknown) - io err" );
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

bool AppleUSBCDCDriver::start( IOService *provider )
{
    UInt8	configs;	// number of device configurations
    UInt8	i;

    g.evLogBufp = NULL;
	
    for (i=0; i<numberofPorts; i++)
    {
        fPorts[i] = NULL;
    }
    
#if USE_ELG
    AllocateEventLog( kEvLogSize );
    ELG( &g, g.evLogBufp, 'USBM', "AppleUSBCDCDriver::init - event logging set up." );

    waitForService( resourceMatching( "kdp" ) );
#endif /* USE_ELG */

    ELG( this, provider, 'strt', "AppleUSBCDCDriver::start - this, provider." );
    if( !super::start( provider ) )
    {
        IOLogIt( 0, 0, 'SS--', "AppleUSBCDCDriver::start - super failed" );
        return false;
    }

	// Get my USB device provider - the device

    fpDevice = OSDynamicCast( IOUSBDevice, provider );
    if( !fpDevice )
    {
        IOLogIt( 0, 0, 'Dev-', "AppleUSBCDCDriver::start - provider invalid" );
        stop( provider );
        return false;
    }

	// Let's see if we have any configurations to play with
		
    configs = fpDevice->GetNumConfigurations();
    if ( configs < 1 )
    {
        IOLogIt( 0, 0, 'Cfg-', "AppleUSBCDCDriver::start - no configurations" );
        stop( provider );
        return false;
    }
	
	// Now take control of the device and configure it
		
    if (!fpDevice->open(this))
    {
        IOLogIt( 0, 0, 'Opn-', "AppleUSBCDCDriver::start - unable to open device" );
        stop( provider );
        return false;
    }
	
    if ( !configureDevice(configs) )
    {
    IOLogIt( 0, 0, 'Nub-', "AppleUSBCDCDriver::start - failed" );
    fpDevice->close(this);
    stop( provider );
        return false;
    }
    
    ELG( 0, 0, 'Nub+', "AppleUSBCDCDriver::start - successful and IOModemSerialStreamSync created" );
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::free
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Clean up and free the log 
//
/****************************************************************************************************/

void AppleUSBCDCDriver::free()
{

    ELG( 0, 0, 'free', "AppleUSBCDCDriver::free" );
	
#if USE_ELG
    if ( g.evLogBuf )
    	IOFree( g.evLogBuf, kEvLogSize );
#endif /* USE_ELG */

    super::free();
    return;
	
}/* end free */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops
//
/****************************************************************************************************/

void AppleUSBCDCDriver::stop( IOService *provider )
{
    UInt8	i;
    
    ELG( 0, 0, 'stop', "AppleUSBCDCDriver::stop" );

//    if ( fqueueRequestLock )
//    {
//        IOLockFree( fqueueRequestLock );	// free the queue Lock
//        fqueueRequestLock = 0;
//    }

    for (i=0; i<numberofPorts; i++)
    {
        if ( fPorts[i] != NULL )
        {
            if ( fPorts[i]->CommInterface )	
            {
                fPorts[i]->CommInterface->close( this );	
                fPorts[i]->CommInterface->release();
                fPorts[i]->CommInterface = NULL;	
            }
	
            if ( fPorts[i]->DataInterface )	
            { 
                fPorts[i]->DataInterface->close( this );	
                fPorts[i]->DataInterface->release();
                fPorts[i]->DataInterface = NULL;	
            }

            IOFree( fPorts[i], sizeof(PortInfo_t) );
            fPorts[i] = NULL;
        }
    }
	
    removeProperty( (const char *)propertyTag );
    
    if ( fpDevice )
    {
        fpDevice->close( this );
        fpDevice = NULL;
    }
    
    super::stop( provider );
    
    return;
	
}/* end stop */

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

bool AppleUSBCDCDriver::allocateResources( PortInfo_t *port )
{
    IOUSBFindEndpointRequest	epReq;		// endPoint request struct on stack
    bool			goodCall;

    ELG( port, 0, 'Allo', "AppleUSBCDCDriver::allocateResources." );

        // Open all the end points:

    goodCall = port->DataInterface->open( this );
    if ( !goodCall )
    {
        ELG( 0, 0, 'epD-', "AppleUSBCDCDriver::allocateResources - open data interface failed." );
        port->DataInterface->release();
        port->DataInterface = NULL;
        return false;
    }

    goodCall = port->CommInterface->open( this );
    if ( !goodCall )
    {
        ELG( 0, 0, 'epC-', "AppleUSBCDCDriver::allocateResources - open comm interface failed." );
        port->CommInterface->release();
        port->CommInterface = NULL;
        return false;
    }
	
//    port->CommInterfaceNumber = port->CommInterface->GetInterfaceNumber();
//    ELG( 0, port->CommInterfaceNumber, 'CIn#', "AppleUSBCDCDriver::allocateResources - Comm interface number." );

    epReq.type = kUSBBulk;
    epReq.direction = kUSBIn;
    epReq.maxPacketSize	= 0;
    epReq.interval = 0;
    port->InPipe = port->DataInterface->FindNextPipe( 0, &epReq );
    if ( !port->InPipe )
    {
        ELG( 0, 0, 'i P-', "AppleUSBCDCDriver::allocateResources - no bulk input pipe." );
        return false;
    }
    ELG( epReq.maxPacketSize << 16 |epReq.interval, port->InPipe, 'i P+', "AppleUSBCDCDriver::allocateResources - bulk input pipe." );

    epReq.direction = kUSBOut;
    port->OutPipe = port->DataInterface->FindNextPipe( 0, &epReq );
    if ( !port->OutPipe )
    {
        ELG( 0, 0, 'o P-', "AppleUSBCDCDriver::allocateResources - no bulk output pipe." );
        return false;
    }
    port->OutPacketSize = epReq.maxPacketSize;
    ELG( epReq.maxPacketSize << 16 |epReq.interval, port->OutPipe, 'o P+', "AppleUSBCDCDriver::allocateResources - bulk output pipe." );

        // Interrupt pipe - Comm Interface:

    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    port->CommPipe = port->CommInterface->FindNextPipe( 0, &epReq );
    if ( !port->CommPipe )
    {
        ELG( 0, 0, 'c P-', "AppleUSBCDCDriver::allocateResources - no comm pipe." );
        releaseResources( port );
        return false;
    }
    ELG( epReq.maxPacketSize << 16 |epReq.interval, port->CommPipe, 'c P+', "AppleUSBCDCDriver::allocateResources - comm pipe." );

        // Allocate Memory Descriptor Pointer with memory for the Comm pipe:

    port->CommPipeMDP = IOBufferMemoryDescriptor::withCapacity( COMM_BUFF_SIZE, kIODirectionIn );
    if ( !port->CommPipeMDP )
        return false;
		
    port->CommPipeMDP->setLength( COMM_BUFF_SIZE );
    port->CommPipeBuffer = (UInt8*)port->CommPipeMDP->getBytesNoCopy();
    ELG( 0, port->CommPipeBuffer, 'cBuf', "AppleUSBCDCDriver::allocateResources - comm buffer" );

        // Allocate Memory Descriptor Pointer with memory for the data-in bulk pipe:

    port->PipeInMDP = IOBufferMemoryDescriptor::withCapacity( 128, kIODirectionIn );
    if ( !port->PipeInMDP )
        return false;
		
    port->PipeInMDP->setLength( 128 );
    port->PipeInBuffer = (UInt8*)port->PipeInMDP->getBytesNoCopy();
    ELG( 0, port->PipeInBuffer, 'iBuf', "AppleUSBCDCDriver::allocateResources - input buffer" );

        // Allocate Memory Descriptor Pointer with memory for the data-out bulk pipe:

    port->PipeOutMDP = IOBufferMemoryDescriptor::withCapacity( MAX_BLOCK_SIZE, kIODirectionOut );
    if ( !port->PipeOutMDP )
        return false;
		
    port->PipeOutMDP->setLength( MAX_BLOCK_SIZE );
    port->PipeOutBuffer = (UInt8*)port->PipeOutMDP->getBytesNoCopy();
    ELG( 0, port->PipeOutBuffer, 'oBuf', "AppleUSBCDCDriver::allocateResources - output buffer" );

        // Allocate queue locks

    port->RXqueueRequestLock = IOLockAlloc();	// init lock for RX queue
    if ( !port->RXqueueRequestLock )
        return false;
        
    port->TXqueueRequestLock = IOLockAlloc();	// init lock for TX queue
    if ( !port->TXqueueRequestLock )
        return false;
		
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

void AppleUSBCDCDriver::releaseResources( PortInfo_t *port )
{
    ELG( 0, 0, 'rlRs', "AppleUSBCDCDriver::releaseResources" );
	
    if ( port->CommInterface )	
    {
        port->CommInterface->close( this );		
    }
	
    if ( port->DataInterface )	
    { 
        port->DataInterface->close( this );
    }

    if ( port->serialRequestLock )
    {
        IOLockFree( port->serialRequestLock );	// free the State machine Lock
        port->serialRequestLock = 0;
    }
    
    if ( port->RXqueueRequestLock )
    {
        IOLockFree( port->RXqueueRequestLock );
        port->RXqueueRequestLock = 0;
    }
    
    if ( port->TXqueueRequestLock )
    {
        IOLockFree( port->TXqueueRequestLock );
        port->TXqueueRequestLock = 0;
    }
	
    if ( port->PipeOutMDP  )	
    { 
        port->PipeOutMDP->release();	
        port->PipeOutMDP = 0; 
    }
	
    if ( port->PipeInMDP   )	
    { 
        port->PipeInMDP->release();	
        port->PipeInMDP = 0; 
    }
	
    if ( port->CommPipeMDP )	
    { 
        port->CommPipeMDP->release();	
        port->CommPipeMDP = 0; 
    }

    return;
	
}/* end releaseResources */

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

bool AppleUSBCDCDriver::configureDevice( UInt8 numConfigs )
{
    IOUSBFindInterfaceRequest		req;			// device request
    IOUSBInterface			*Comm;
    IOUSBInterface			*Dataintf = NULL;
    IOReturn				ior = kIOReturnSuccess;
    UInt8				i, dataindx;
    bool				portok = false;
    bool				goodCDC = false;
    PortInfo_t 				*port = NULL;
    
    ELG( 0, numConfigs, 'cDev', "AppleUSBCDCDriver::configureDevice" );
    	
        // Initialize and "configure" the device
        
    if ( !initDevice( numConfigs ))
    {
        ELG( 0, 0, 'cDi-', "AppleUSBCDCDriver::configureDevice - initDevice failed" );
        return false;
    }

    OSBoolean *boolObj = OSDynamicCast( OSBoolean, fpDevice->getProperty("kDoNotSuspend") );
    if ( boolObj && boolObj->isTrue() )
    {
        fSuspendOK = false;
        USBLog(5,"%s[%p] Suspend has been canceled for this device", getName(), this);
        ELG( 0, 0, 'cDs-', "AppleUSBCDCDriver::configureDevice - Suspended has been canceled for this device" );
    }

    req.bInterfaceClass	= kUSBCommClass;
    req.bInterfaceSubClass = kUSBAbstractControlModel;
    req.bInterfaceProtocol = kUSBv25;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    Comm = fpDevice->FindNextInterface( NULL, &req );
    if ( !Comm )
    {
        ELG( 0, 0, 'FIC-', "AppleUSBCDCDriver::configureDevice - Finding the first CDC interface failed" );
    }

    while ( Comm )
    {
        port = NULL;
        for (i=0; i<numberofPorts; i++)
        {
            if (fPorts[i] == NULL)
            {
                port = (PortInfo_t*)IOMalloc( sizeof(PortInfo_t) );
                fPorts[i] = port;
                ELG( port, i, 'Port', "AppleUSBCDCDriver::configureDevice - Port allocated" );
                SetStructureDefaults( port, true );			// init the Port structure
                break;
            }
        }
        if ( !port )
        {
            ELG( 0, i, 'Port', "AppleUSBCDCDriver::configureDevice - No ports available or IOMalloc failed" );
        } else {
            port->CommInterface = Comm;
            port->CommInterface->retain();
            port->CommInterfaceNumber = Comm->GetInterfaceNumber();
            ELG( 0, port->CommInterfaceNumber, 'cdI#', "AppleUSBCDCDriver::configureDevice - Comm interface number." );
    	
            portok = getFunctionalDescriptors ( port );
            
            if ( portok )
            {
                req.bInterfaceClass = kUSBDataClass;
                req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
                req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
                req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
                do
                {
                    Dataintf = fpDevice->FindNextInterface( Dataintf, &req );
                    if ( Dataintf )
                    {
                        dataindx = Dataintf->GetInterfaceNumber();
                        ELG( dataindx, port->DataInterfaceNumber, 'cDD#', "AppleUSBCDCDriver::configureDevice - finding Data interface" );
                        if ( dataindx == port->DataInterfaceNumber )
                        {
                            port->DataInterface = Dataintf;				// We've found our man
                            ELG( 0, 0, 'cDDm', "AppleUSBCDCDriver::configureDevice - found matching Data interface" );
                            break;
                        }
                    }
                } while (Dataintf);
                
                if ( !port->DataInterface )
                {
                    port->DataInterface = fpDevice->FindNextInterface( NULL, &req );	// Go with the first one
                    if ( !port->DataInterface )
                    {
                        ELG( 0, 0, 'cDD-', "AppleUSBCDCDriver::configureDevice - Find next interface for the Data Class failed" );
                        portok = false;
                    } else {
                        ELG( 0, 0, 'cDDm', "AppleUSBCDCDriver::configureDevice - going with the first (only?) Data interface" );
                    }
                }
                if ( port->DataInterface )
                {
                    port->DataInterface->retain();
            
                        // Found both so now set the name for this port
	
                    if ( createSerialStream( port ) )	// Publish SerialStream services
                    {
                        goodCDC = true;
                    } else {
                        ELG( 0, 0, 'Nub-', "AppleUSBCDCDriver::configureDevice - createSerialStream failed" );
                        portok = false;
                    }
                }
            }
        }
        if ( !portok )
        {
            if (port)
            {
                IOFree( port, sizeof(PortInfo_t) );
                fPorts[i] = NULL;
            }
        }
        
            // see if there's another CDC interface
            
        req.bInterfaceClass = kUSBCommClass;
	req.bInterfaceSubClass = kUSBAbstractControlModel;
	req.bInterfaceProtocol = kUSBv25;
	req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            
        Comm = fpDevice->FindNextInterface( Comm, &req );
        if ( !Comm )
        {
            ELG( 0, 0, 'cDFI', "AppleUSBCDCDriver::configureDevice - No more CDC interfaces" );
        }
        portok = false;
    }
    
    if ( fSuspendOK )
    {
        ior = fpDevice->SuspendDevice( true );         // Suspend the device (if supported, bus powered ONLY and not canceled)
        if ( ior )
        {
            ELG( 0, ior, 'cCSD', "AppleUSBCDCDriver::configureDevice - SuspendDevice error" );
        }
    }
    
    if ( goodCDC )
    {
        return true;
    }
	
    return false;

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

bool AppleUSBCDCDriver::initDevice( UInt8 numConfigs )
{
    IOUSBFindInterfaceRequest		req;
    IOUSBDevRequest			devreq;
    const IOUSBConfigurationDescriptor	*cd = NULL;		// configuration descriptor
    IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
    IOReturn				ior = kIOReturnSuccess;
    UInt8				cval;
    UInt8				config = 0;
    bool				goodconfig = false;
       
    ELG( 0, numConfigs, 'cDev', "AppleUSBCDCDriver::initDevice" );
    	
        // Make sure we have a CDC interface to play with
        
    for (cval=0; cval<numConfigs; cval++)
    {
    	ELG( 0, cval, 'CkCn', "AppleUSBCDCDriver::initDevice - Checking Configuration" );
		
     	cd = fpDevice->GetFullConfigurationDescriptor(cval);
     	if ( !cd )
    	{
            ELG( 0, 0, 'GFC-', "AppleUSBCDCDriver::initDevice - Error getting the full configuration descriptor" );
        } else {
            req.bInterfaceClass	= kUSBCommClass;
            req.bInterfaceSubClass = kUSBAbstractControlModel;
            req.bInterfaceProtocol = kUSBv25;
            req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
            if ( ior == kIOReturnSuccess )
            {
                if ( intf )
                {
                    ELG( 0, config, 'FNI+', "AppleUSBCDCDriver::initDevice - Interface descriptor found" );
                    config = cd->bConfigurationValue;
                    goodconfig = true;					// We have at least one CDC interface in this configuration
                    break;
                } else {
                    ELG( 0, config, 'FNI-', "AppleUSBCDCDriver::initDevice - That's weird the interface was null" );
                }
            } else {
                ELG( 0, cval, 'FNID', "AppleUSBCDCDriver::initDevice - No CDC interface found this configuration" );
            }
        }
    }
    
    if ( goodconfig )
    {
        ior = fpDevice->SetConfiguration( this, config );
        if ( ior != kIOReturnSuccess )
        {
            ELG( 0, ior, 'SCo-', "AppleUSBCDCDriver::initDevice - SetConfiguration error" );
            goodconfig = false;			
        }
    }
    
    if ( !goodconfig )					// If we're not good - bail
        return false;
    
    fbmAttributes = cd->bmAttributes;
    ELG( fbmAttributes, kUSBAtrRemoteWakeup, 'GFbA', "AppleUSBCDCDriver::initDevice - Configuration bmAttributes" );
    
    fSuspendOK = false;
    if ( !(fbmAttributes & kUSBAtrSelfPowered) )
    {
        if ( fbmAttributes & kUSBAtrBusPowered )
        {
            fSuspendOK = true;
        }
    }
    if ( fSuspendOK )
    {
        ELG( 0, 0, 'SCS+', "AppleUSBCDCDriver::initDevice - Suspend/Resume is active" );
    } else {
        ELG( 0, 0, 'SCS-', "AppleUSBCDCDriver::initDevice - Suspend/Resume is inactive" );
    }
    
    if ( fbmAttributes & kUSBAtrRemoteWakeup )
    {
//        getPMRootDomain()->publishFeature( "WakeOnRing" );
    
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

            ior = fpDevice->DeviceRequest( &devreq );
            if ( ior == kIOReturnSuccess )
            {
                ELG( 0, ior, 'SCCs', "AppleUSBCDCDriver::initDevice - Clearing remote wake up feature successful" );
            } else {
                ELG( 0, ior, 'SCCf', "AppleUSBCDCDriver::initDevice - Clearing remote wake up feature failed" );
            }
        }
    } else {
        ELG( 0, 0, 'SCRw', "AppleUSBCDCDriver::initDevice - Remote wake up not supported" );
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

bool AppleUSBCDCDriver::getFunctionalDescriptors( PortInfo_t *port )
{
    bool				gotDescriptors = false;
    bool				configok = true;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    CMFunctionalDescriptor		*CMFDesc;		// call management functional descriptor
    ACMFunctionalDescriptor		*ACMFDesc;		// abstract control management functional descriptor
       
    ELG( 0, 0, 'gFDs', "AppleUSBCDCDriver::getFunctionalDescriptors" );
    
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
            switch ( funcDesc->bDescriptorSubtype )
            {
                case Header_FunctionalDescriptor:
                    ELG( funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFHd', "AppleUSBCDCDriver::getFunctionalDescriptors - Header Functional Descriptor" );
                    break;
                case CM_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)CMFDesc = funcDesc;
                    ELG( funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFCM', "AppleUSBCDCDriver::getFunctionalDescriptors - CM Functional Descriptor" );
                    port->DataInterfaceNumber = CMFDesc->bDataInterface;
                    port->CMCapabilities = CMFDesc->bmCapabilities;
				
                        // Check the configuration supports data management on the data interface (that's all we support)
				
                    if (!(port->CMCapabilities & CM_ManagementData))
                    {
                        ELG( 0, 0, 'gFC-', "AppleUSBCDCDriver::getFunctionalDescriptors - Interface doesn't support Call Management" );
                        configok = false;
                    }
                    if (!(port->CMCapabilities & CM_ManagementOnData))
                    {
                        ELG( 0, 0, 'gFC-', "AppleUSBCDCDriver::getFunctionalDescriptors - Interface doesn't support Call Management on Data Interface" );
                       //  configok = false;
                    }
                    break;
                case ACM_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)ACMFDesc = funcDesc;
                    ELG( funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFAM', "AppleUSBCDCDriver::getFunctionalDescriptors - ACM Functional Descriptor" );
                    port->ACMCapabilities = ACMFDesc->bmCapabilities;
                    break;
                case Union_FunctionalDescriptor:
                    ELG( funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFUn', "AppleUSBCDCDriver::getFunctionalDescriptors - Union Functional Descriptor" );
                    break;
                case CS_FunctionalDescriptor:
                    ELG( funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFCS', "AppleUSBCDCDriver::getFunctionalDescriptors - CS Functional Descriptor" );
                    break;
                default:
                    ELG( funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, 'gFFD', "AppleUSBCDCDriver::getFunctionalDescriptors - unknown Functional Descriptor" );
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
//		Inputs:		None
//
//		Outputs:	return Code - true (suffix created), false (suffix not create), sufKey - the key
//
//		Desc:		Creates the suffix key. It attempts to use the serial number string from the device
//				if it's reasonable i.e. less than 8 bytes ascii. Remember it's stored in unicode 
//				format. If it's not present or not reasonable it will generate the suffix based 
//				on the location property tag. At least this remains the same across boots if the
//				device is plugged into the same physical location. In the latter case trailing
//				zeros are removed.
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::createSuffix( unsigned char *sufKey )
{
    
    IOReturn	rc;
    UInt8	serBuf[10];		// arbitrary size > 8
    OSNumber	*location;
    UInt32	locVal;
    UInt8	*rlocVal;
    UInt16	offs, i, sig = 0;
    UInt8	indx;
    bool	keyOK = false;			
	
    ELG( 0, 0, 'cSuf', "AppleUSBCDCDriver::createSuffix" );
	
    indx = fpDevice->GetSerialNumberStringIndex();	
    if ( indx != 0 )
    {	
            // Generate suffix key based on the serial number string (if reasonable <= 8 and > 0)	

        rc = fpDevice->GetStringDescriptor(indx, (char *)&serBuf, sizeof(serBuf));
        if ( !rc )
        {
            if ( (strlen((char *)&serBuf) < 9) && (strlen((char *)&serBuf) > 0) )
            {
                strcpy( (char *)sufKey, (const char *)&serBuf);
                keyOK = true;
            }			
        } else {
            ELG( 0, rc, 'Sdt-', "AppleUSBCDCDriver::createSuffix error reading serial number string" );
        }
    }
	
    if ( !keyOK )
    {
            // Generate suffix key based on the location property tag
	
        location = (OSNumber *)fpDevice->getProperty(kUSBDevicePropertyLocationID);
        if ( location )
        {
            locVal = location->unsigned32BitValue();		
            offs = 0;
            rlocVal = (UInt8*)&locVal;
            for (i=0; i<4; i++)
            {
                sufKey[offs] = Asciify(rlocVal[i] >> 4);
                if ( sufKey[offs++] != '0')
                    sig = offs;
                sufKey[offs] = Asciify(rlocVal[i]);
                if ( sufKey[offs++] != '0')
                    sig = offs;
            }			
            sufKey[sig] = 0x00;
            keyOK = true;
        }
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

bool AppleUSBCDCDriver::createSerialStream( PortInfo_t *port )
{
    IOModemSerialStreamSync	*pNub = new IOModemSerialStreamSync;
    bool			ret;
    UInt8			indx;
    IOReturn			rc;
    unsigned char		rname[10];
    const char			*suffix = (const char *)&rname;
	
    ELG( 0, pNub, '=Nub', "AppleUSBCDCDriver::createSerialStream - 0, nub" );
    if ( !pNub )
    {
        return false;
    }
		
    	// Either we attached and should get rid of our reference
    	// or we failed in which case we should get rid our reference as well.
        // This just makes sure the reference count is correct.
	
    ret = (pNub->init(0, port) && pNub->attach( this ));
	
    pNub->release();
    if ( !ret )
    {
        ELG( ret, 0, 'Nub-', "AppleUSBCDCDriver::createSerialStream - Didn't attach to the nub properly" );
        return false;
    }

    // Report the base name to be used for generating device nodes
	
    pNub->setProperty( kIOTTYBaseNameKey, baseName );
	
    // Create suffix key and set it
	
    if ( createSuffix( (unsigned char *)suffix ) )
    {		
        pNub->setProperty( kIOTTYSuffixKey, suffix );
    }

    pNub->registerService();
	
	// Save the Product String (at least the first productNameLength's worth). This is done (same name) per stream for the moment.

    indx = fpDevice->GetProductStringIndex();	
    if ( indx != 0 )
    {	
        rc = fpDevice->GetStringDescriptor( indx, (char *)&fProductName, sizeof(fProductName) );
        if ( !rc )
        {
            if ( strlen((char *)fProductName) == 0 )		// believe it or not this sometimes happens (null string with an index defined???)
            {
                strcpy( (char *)fProductName, defaultName);
            }
            pNub->setProperty( (const char *)propertyTag, (const char *)fProductName );
        }
    }

    return true;
	
}/* end createSerialStream */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::acquirePort
//
//		Inputs:		sleep - true (wait for it), false (don't), refCon - the Port
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

IOReturn AppleUSBCDCDriver::acquirePort( bool sleep, void *refCon )
{
    PortInfo_t 	*port = (PortInfo_t *) refCon;
    UInt32 	busyState = 0;
    IOReturn 	rtn = kIOReturnSuccess;

    ELG( port, sleep, 'acqP', "AppleUSBCDCDriver::acquirePort" );

    if ( !port->serialRequestLock )
    {
        port->serialRequestLock = IOLockAlloc();	// init lock for state machine
        if ( !port->serialRequestLock )
            return kIOReturnNoMemory;
    }

    for (;;)
    {
        busyState = readPortState( port ) & PD_S_ACQUIRED;
        if ( !busyState )
        {		
                // Set busy bit, and clear everything else
                
            changeState( port, (UInt32)PD_S_ACQUIRED | DEFAULT_STATE, (UInt32)STATE_ALL);
            break;
        } else {
            if ( !sleep )
            {
                ELG( 0, 0, 'busy', "AppleUSBCDCDriver::acquirePort - Busy exclusive access" );
            	return kIOReturnExclusiveAccess;
            } else {
            	busyState = 0;
            	rtn = watchState( &busyState, PD_S_ACQUIRED, refCon );
            	if ( (rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess) )
                {
                    continue;
            	} else {
                    ELG( 0, 0, 'int-', "AppleUSBCDCDriver::acquirePort - Interrupted!" );
                    return rtn;
                }
            }
        }
    } /* end for */
    
    if ( fSuspendOK )
    {
    rtn = fpDevice->SuspendDevice( false );		// Resume the device
    if ( rtn != kIOReturnSuccess )
    {
        return rtn;
    }
    }
    
    IOSleep( 50 );
    
    if ( !allocateResources( port ) ) 
    {
    	return kIOReturnNoMemory;
    }
	
    SetStructureDefaults( port, FALSE );	/* Initialize all the structures */
	
    if (!allocateRingBuffer(&(port->TX), port->TXStats.BufferSize) || !allocateRingBuffer(&(port->RX), port->RXStats.BufferSize)) 
    {
        releaseResources( port );
        return kIOReturnNoMemory;
    }
	
        // Read the comm interrupt pipe for status:
		
    fCommCompletionInfo.target = this;
    fCommCompletionInfo.action = commReadComplete;
    fCommCompletionInfo.parameter = port;
		
    rtn = port->CommPipe->Read(port->CommPipeMDP, &fCommCompletionInfo, NULL );
    if ( rtn == kIOReturnSuccess )
    {
        	// Read the data-in bulk pipe:
			
        fReadCompletionInfo.target = this;
        fReadCompletionInfo.action = dataReadComplete;
        fReadCompletionInfo.parameter = port;
		
        rtn = port->InPipe->Read(port->PipeInMDP, &fReadCompletionInfo, NULL );
			
        if ( rtn == kIOReturnSuccess )
        {
        	// Set up the data-out bulk pipe:
			
            fWriteCompletionInfo.target	= this;
            fWriteCompletionInfo.action	= dataWriteComplete;
            fWriteCompletionInfo.parameter = port;
		
                // Set up the management element request completion routine:

            fMERCompletionInfo.target = this;
            fMERCompletionInfo.action = merWriteComplete;
            fMERCompletionInfo.parameter = NULL;				// for now, filled in with parm block when allocated
        }
    }

    if (rtn != kIOReturnSuccess)
    {
    	// We failed for some reason
	
        freeRingBuffer(&(port->TX));
        freeRingBuffer(&(port->RX));

        releaseResources( port );
        changeState(port, 0, STATE_ALL);	// Clear the entire state word
    } else {
        fSessions++;				// bump number of active sessions and turn on clear to send
        changeState( port, PD_RS232_S_CTS, PD_RS232_S_CTS);
    }

    return rtn;
	
}/* end acquirePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::releasePort
//
//		Inputs:		refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		releasePort returns all the resources and does clean up.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::releasePort( void *refCon )
{
    PortInfo_t 	*port = (PortInfo_t *) refCon;
    UInt32 	busyState;
    IOReturn	ior;

    ELG( 0, port, 'relP', "AppleUSBCDCDriver::releasePort" );
    busyState = (readPortState( port ) & PD_S_ACQUIRED);
    if ( !busyState )
    {
        ELG( 0, 0, 'rlP-', "AppleUSBCDCDriver::releasePort - NOT OPEN" );
        if ( fTerminate )
            return kIOReturnOffline;

        return kIOReturnNotOpen;
    }
	
    if ( !fTerminate )
        USBSetControlLineState(port, false, false);		// clear RTS and clear DTR only if not terminated
	
    changeState( port, 0, (UInt32)STATE_ALL );			// Clear the entire state word - which also deactivates the port

        // Remove all the buffers.

    freeRingBuffer( &port->TX );
    freeRingBuffer( &port->RX );
	
        // Release all resources
		
    releaseResources( port );
    
    if ( !fTerminate )
    {
        if ( fSuspendOK )
        {
            ior = fpDevice->SuspendDevice( true );         // Suspend the device again (if supported and not unplugged)
            if ( ior )
            {
                ELG( 0, ior, 'rPSD', "AppleUSBCDCDriver::releasePort - SuspendDevice error" );
            }
        }
    }

    fSessions--;					// reduce number of active sessions
    if ((fTerminate) && (fSessions == 0))		// if it's the result of a terminate and session count is zero we also need to close the device
    {
    	fpDevice->close(this);
        fpDevice = NULL;
    }
    
    ELG( 0, 0, 'RlP+', "AppleUSBCDCDriver::releasePort - OK" );
    
    return kIOReturnSuccess;
	
}/* end releasePort */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::setState
//
//		Inputs:		state - state to set, mask - state mask, refCon - the Port
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

IOReturn AppleUSBCDCDriver::setState( UInt32 state, UInt32 mask, void *refCon )
{
    PortInfo_t *port = (PortInfo_t *) refCon;
	
    ELG( state, mask, 'stSt', "AppleUSBCDCDriver::setState" );
    
    if ( fTerminate )
        return kIOReturnOffline;
	
    if ( mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK)) )
        return kIOReturnBadArgument;
    
    if ( readPortState( port ) & PD_S_ACQUIRED )
    {
            // ignore any bits that are read-only
            
        mask &= (~port->FlowControl & PD_RS232_A_MASK) | PD_S_MASK;

        if ( mask & PD_RS232_S_DTR )
        {
            if ( (state & PD_RS232_S_DTR) != (port->State & PD_RS232_S_DTR) )
            {
                if ( state & PD_RS232_S_DTR )
                {
                    ELG( 0, 0, 'stDT', "AppleUSBCDCDriver::setState - DTR TRUE" );
                    USBSetControlLineState(port, false, true);
                } else {
                    ELG( 0, 0, 'stDF', "AppleUSBCDCDriver::setState - DTR FALSE" );
                    USBSetControlLineState(port, false, false);
                }
            }
        }

        if ( mask)
            changeState( port, state, mask );

        return kIOReturnSuccess;
    }
    
    return kIOReturnNotOpen;
	
}/* end setState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::getState
//
//		Inputs:		refCon - the Port
//
//		Outputs:	state - port state
//
//		Desc:		Get the state for the port.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDriver::getState( void *refCon )
{
    PortInfo_t 	*port = (PortInfo_t *) refCon;
    UInt32 	state;
	
    ELG( 0, port, 'gtSt', "AppleUSBCDCDriver::getState" );
	
    CheckQueues( port );
	
    state = readPortState( port ) & EXTERNAL_MASK;
	
    ELG( state, EXTERNAL_MASK, 'gtSe', "AppleUSBCDCDriver::getState - exit" );
	
    return state;
	
}/* end getState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::watchState
//
//		Inputs:		state - state to watch for, mask - state mask bits, refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from ::watchState
//
//		Desc:		Wait for the at least one of the state bits defined in mask to be equal
//				to the value defined in state. Check on entry then sleep until necessary,
//				see privatewatchState for more details.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::watchState( UInt32 *state, UInt32 mask, void *refCon)
{
    PortInfo_t 	*port = (PortInfo_t *) refCon;
    IOReturn 	ret = kIOReturnNotOpen;

    ELG( *state, mask, 'WatS', "AppleUSBCDCDriver::watchState" );
    
    if ( fTerminate )
        return kIOReturnOffline;

    if ( readPortState( port ) & PD_S_ACQUIRED )
    {
        ret = kIOReturnSuccess;
        mask &= EXTERNAL_MASK;
        ret = privateWatchState( port, state, mask );
        *state &= EXTERNAL_MASK;
    }
	
    ELG( ret, 0, 'Wate', "AppleUSBCDCDriver::watchState - exit" );
    
    return ret;
	
}/* end watchState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::nextEvent
//
//		Inputs:		refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess
//
//		Desc:		Not used by this driver.
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDriver::nextEvent( void *refCon )
{
    UInt32 ret = kIOReturnSuccess;

    ELG( 0, 0, 'NxtE', "AppleUSBCDCDriver::nextEvent" );

    return ret;
	
}/* end nextEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::executeEvent
//
//		Inputs:		event - The event, data - any data associated with the event, refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen or kIOReturnBadArgument
//
//		Desc:		executeEvent causes the specified event to be processed immediately.
//				This is primarily used for channel control commands like START & STOP
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::executeEvent( UInt32 event, UInt32 data, void *refCon )
{
    PortInfo_t 	*port = (PortInfo_t *)refCon;
    IOReturn	ret = kIOReturnSuccess;
    UInt32 	state, delta;
	
    if ( fTerminate )
        return kIOReturnOffline;
        
    delta = 0;
    state = readPortState( port );	
    ELG( port, state, 'ExIm', "AppleUSBCDCDriver::executeEvent" );
	
    if ( (state & PD_S_ACQUIRED) == 0 )
        return kIOReturnNotOpen;

    switch ( event )
    {
	case PD_RS232_E_XON_BYTE:
            ELG( data, event, 'ExIm', "AppleUSBCDCDriver::executeEvent - PD_RS232_E_XON_BYTE" );
            port->XONchar = data;
            break;
	case PD_RS232_E_XOFF_BYTE:
            ELG( data, event, 'ExIm', "AppleUSBCDCDriver::executeEvent - PD_RS232_E_XOFF_BYTE" );
            port->XOFFchar = data;
            break;
	case PD_E_SPECIAL_BYTE:
            ELG( data, event, 'ExIm', "AppleUSBCDCDriver::executeEvent - PD_E_SPECIAL_BYTE" );
            port->SWspecial[ data >> SPECIAL_SHIFT ] |= (1 << (data & SPECIAL_MASK));
            break;
	case PD_E_VALID_DATA_BYTE:
            ELG( data, event, 'ExIm', "AppleUSBCDCDriver::executeEvent - PD_E_VALID_DATA_BYTE" );
            port->SWspecial[ data >> SPECIAL_SHIFT ] &= ~(1 << (data & SPECIAL_MASK));
            break;
	case PD_E_FLOW_CONTROL:
            ELG( data, event, 'ExIm', "AppleUSBCDCDriver::executeEvent - PD_E_FLOW_CONTROL" );
            break;
	case PD_E_ACTIVE:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_ACTIVE" );
            if ( (bool)data )
            {
                if ( !(state & PD_S_ACTIVE) )
                {
                    SetStructureDefaults( port, FALSE );
                    changeState( port, (UInt32)PD_S_ACTIVE, (UInt32)PD_S_ACTIVE ); 	// activate port
				
                    USBSetControlLineState(port, true, true);					// set RTS and set DTR
                }
            } else {
                if ( (state & PD_S_ACTIVE) )
                {
                    changeState( port, 0, (UInt32)PD_S_ACTIVE );			// deactivate port
				
                    USBSetControlLineState(port, false, false);				// clear RTS and clear DTR
                }
            }
            break;
	case PD_E_DATA_LATENCY:
            ELG( data, event, 'ExIm', "AppleUSBCDCDriver::executeEvent - PD_E_DATA_LATENCY" );
            port->DataLatInterval = long2tval( data * 1000 );
            break;
	case PD_RS232_E_MIN_LATENCY:
            ELG( data, event, 'ExIm', "AppleUSBCDCDriver::executeEvent - PD_RS232_E_MIN_LATENCY" );
            port->MinLatency = bool( data );
            break;
	case PD_E_DATA_INTEGRITY:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_DATA_INTEGRITY" );
            if ( (data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE))
            {
                ret = kIOReturnBadArgument;
            } else {
                port->TX_Parity = data;
                port->RX_Parity = PD_RS232_PARITY_DEFAULT;
			
                USBSetLineCoding( port );			
            }
            break;
	case PD_E_DATA_RATE:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_DATA_RATE" );
            
                // For API compatiblilty with Intel.
                
            data >>= 1;
            ELG( 0, data, 'Exlm', "AppleUSBCDCDriver::executeEvent - actual data rate" );
            if ( (data < MIN_BAUD) || (data > kMaxBaudRate) )
            {
                ret = kIOReturnBadArgument;
            } else {
                port->BaudRate = data;
			
                USBSetLineCoding( port );			
            }		
            break;
	case PD_E_DATA_SIZE:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_DATA_SIZE" );
            
                // For API compatiblilty with Intel.
                
            data >>= 1;
            ELG( 0, data, 'Exlm', "AppleUSBCDCDriver::executeEvent - actual data size" );
            if ( (data < 5) || (data > 8) )
            {
                ret = kIOReturnBadArgument;
            } else {
                port->CharLength = data;
			
                USBSetLineCoding( port );			
            }
            break;
	case PD_RS232_E_STOP_BITS:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_RS232_E_STOP_BITS" );
            if ( (data < 0) || (data > 20) )
            {
                ret = kIOReturnBadArgument;
            } else {
                port->StopBits = data;
			
                USBSetLineCoding( port );
            }
            break;
	case PD_E_RXQ_FLUSH:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_RXQ_FLUSH" );
            break;
	case PD_E_RX_DATA_INTEGRITY:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_RX_DATA_INTEGRITY" );
            if ( (data != PD_RS232_PARITY_DEFAULT) &&  (data != PD_RS232_PARITY_ANY) )
            {
                ret = kIOReturnBadArgument;
            } else {
                port->RX_Parity = data;
            }
            break;
	case PD_E_RX_DATA_RATE:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_RX_DATA_RATE" );
            if ( data )
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_RX_DATA_SIZE:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_RX_DATA_SIZE" );
            if ( data )
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_RS232_E_RX_STOP_BITS:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_RS232_E_RX_STOP_BITS" );
            if ( data )
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_TXQ_FLUSH:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_TXQ_FLUSH" );
            break;
	case PD_RS232_E_LINE_BREAK:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_RS232_E_LINE_BREAK" );
            state &= ~PD_RS232_S_BRK;
            delta |= PD_RS232_S_BRK;
            break;
	case PD_E_DELAY:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_DELAY" );
            port->CharLatInterval = long2tval(data * 1000);
            break;
	case PD_E_RXQ_SIZE:
            ELG( 0, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_RXQ_SIZE" );
            break;
	case PD_E_TXQ_SIZE:
            ELG( 0, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_TXQ_SIZE" );
            break;
	case PD_E_RXQ_HIGH_WATER:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_RXQ_HIGH_WATER" );
            break;
	case PD_E_RXQ_LOW_WATER:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_RXQ_LOW_WATER" );
            break;
	case PD_E_TXQ_HIGH_WATER:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_TXQ_HIGH_WATER" );
            break;
	case PD_E_TXQ_LOW_WATER:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - PD_E_TXQ_LOW_WATER" );
            break;
	default:
            ELG( data, event, 'Exlm', "AppleUSBCDCDriver::executeEvent - unrecognized event" );
            ret = kIOReturnBadArgument;
            break;
    }

    state |= state;					// ejk for compiler warnings. ??
    changeState( port, state, delta );
	
    return ret;
	
}/* end executeEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::requestEvent
//
//		Inputs:		event - The event, refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument, data - any data associated with the event
//
//		Desc:		requestEvent processes the specified event as an immediate request and
//				returns the results in data.  This is primarily used for getting link
//				status information and verifying baud rate and such.
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::requestEvent( UInt32 event, UInt32 *data, void *refCon )
{
    PortInfo_t	*port = (PortInfo_t *) refCon;
    IOReturn	returnValue = kIOReturnSuccess;

//    ELG( 0, readPortState( port ), 'ReqE', "AppleUSBCDCDriver::requestEvent" );
    ELG( 0, port, 'ReqE', "AppleUSBCDCDriver::requestEvent" );
    
    if ( fTerminate )
        return kIOReturnOffline;

    if ( data == NULL ) 
    {
        ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - data is null" );
        returnValue = kIOReturnBadArgument;
    } else {
        switch ( event )
        {
            case PD_E_ACTIVE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_ACTIVE" );
                *data = bool(readPortState( port ) & PD_S_ACTIVE);	
                break;
            case PD_E_FLOW_CONTROL:
                ELG( port->FlowControl, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_FLOW_CONTROL" );
                *data = port->FlowControl;							
                break;
            case PD_E_DELAY:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_DELAY" );
                *data = tval2long( port->CharLatInterval )/ 1000;	
                break;
            case PD_E_DATA_LATENCY:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_DATA_LATENCY" );
                *data = tval2long( port->DataLatInterval )/ 1000;	
                break;
            case PD_E_TXQ_SIZE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_TXQ_SIZE" );
                *data = GetQueueSize( &port->TX );	
                break;
            case PD_E_RXQ_SIZE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_RXQ_SIZE" );
                *data = GetQueueSize( &port->RX );	
                break;
            case PD_E_TXQ_LOW_WATER:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_TXQ_LOW_WATER" );
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_LOW_WATER:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_RXQ_LOW_WATER" );
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_HIGH_WATER:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_TXQ_HIGH_WATER" );
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_HIGH_WATER:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_RXQ_HIGH_WATER" );
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_AVAILABLE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_TXQ_AVAILABLE" );
                *data = FreeSpaceinQueue( &port->TX, port->TXqueueRequestLock );	 
                break;
            case PD_E_RXQ_AVAILABLE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_RXQ_AVAILABLE" );
                *data = UsedSpaceinQueue( &port->RX ); 	
                break;
            case PD_E_DATA_RATE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_DATA_RATE" );
                *data = port->BaudRate << 1;		
                break;
            case PD_E_RX_DATA_RATE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_RX_DATA_RATE" );
                *data = 0x00;					
                break;
            case PD_E_DATA_SIZE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_DATA_SIZE" );
                *data = port->CharLength << 1;	
                break;
            case PD_E_RX_DATA_SIZE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_RX_DATA_SIZE" );
                *data = 0x00;					
                break;
            case PD_E_DATA_INTEGRITY:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_DATA_INTEGRITY" );
                *data = port->TX_Parity;			
                break;
            case PD_E_RX_DATA_INTEGRITY:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_E_RX_DATA_INTEGRITY" );
                *data = port->RX_Parity;			
                break;
            case PD_RS232_E_STOP_BITS:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_RS232_E_STOP_BITS" );
                *data = port->StopBits << 1;		
                break;
            case PD_RS232_E_RX_STOP_BITS:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_RS232_E_RX_STOP_BITS" );
                *data = 0x00;					
                break;
            case PD_RS232_E_XON_BYTE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_RS232_E_XON_BYTE" );
                *data = port->XONchar;			
                break;
            case PD_RS232_E_XOFF_BYTE:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_RS232_E_XOFF_BYTE" );
                *data = port->XOFFchar;			
                break;
            case PD_RS232_E_LINE_BREAK:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_RS232_E_LINE_BREAK" );
                *data = bool(readPortState( port ) & PD_RS232_S_BRK);
                break;
            case PD_RS232_E_MIN_LATENCY:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - PD_RS232_E_MIN_LATENCY" );
                *data = bool( port->MinLatency );		
                break;
            default:
                ELG( 0, event, 'ReqE', "AppleUSBCDCDriver::requestEvent - unrecognized event" );
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
//		Inputs:		event - The event, data - any data associated with the event, 
//				sleep - true (wait for it), false (don't), refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen
//
//		Desc:		Not used by this driver.	
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::enqueueEvent( UInt32 event, UInt32 data, bool sleep, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
	
    ELG( data, event, 'EnqE', "AppleUSBCDCDriver::enqueueEvent" );
    
    if ( fTerminate )
        return kIOReturnOffline;

    if ( readPortState( port ) & PD_S_ACTIVE )
    {
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
	
}/* end enqueueEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dequeueEvent
//
//		Inputs:		sleep - true (wait for it), false (don't), refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen
//
//		Desc:		Not used by this driver.		
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::dequeueEvent( UInt32 *event, UInt32 *data, bool sleep, void *refCon )
{
    PortInfo_t *port = (PortInfo_t *) refCon;
	
    ELG( 0, 0, 'DeqE', "dequeueEvent" );
    
    if ( fTerminate )
        return kIOReturnOffline;

    if ( (event == NULL) || (data == NULL) )
        return kIOReturnBadArgument;

    if ( readPortState( port ) & PD_S_ACTIVE )
    {
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
	
}/* end dequeueEvent */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::enqueueData
//
//		Inputs:		buffer - the data, size - number of bytes, sleep - true (wait for it), false (don't),
//				refCon - the Port
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from watchState, count - bytes transferred,  
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

IOReturn AppleUSBCDCDriver::enqueueData( UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon )
{
    PortInfo_t 	*port = (PortInfo_t *) refCon;
    UInt32 	state = PD_S_TXQ_LOW_WATER;
    IOReturn 	rtn = kIOReturnSuccess;

    ELG( 0, sleep, 'eqDt', "AppleUSBCDCDriver::enqueData" );

    if ( fTerminate )
        return kIOReturnOffline;

    if ( count == NULL || buffer == NULL )
        return kIOReturnBadArgument;

    *count = 0;

    if ( !(readPortState( port ) & PD_S_ACTIVE) )
        return kIOReturnNotOpen;

    ELG( port->State, size, 'eqDt', "AppleUSBCDCDriver::enqueData State" );	
    LogData( kUSBAnyDirn, size, buffer );

        // OK, go ahead and try to add something to the buffer
        
    *count = AddtoQueue( &port->TX, buffer, size, port->TXqueueRequestLock );
    CheckQueues( port );

        // Let the tranmitter know that we have something ready to go
    
    SetUpTransmit( port );

        // If we could not queue up all of the data on the first pass and
        // the user wants us to sleep until it's all out then sleep

    while ( (*count < size) && sleep )
    {
        state = PD_S_TXQ_LOW_WATER;
        rtn = watchState( &state, PD_S_TXQ_LOW_WATER, refCon );
        if ( rtn != kIOReturnSuccess )
        {
            ELG( 0, rtn, 'EqD-', "AppleUSBCDCDriver::enqueueData - interrupted" );
            return rtn;
        }

        *count += AddtoQueue( &port->TX, buffer + *count, size - *count, port->TXqueueRequestLock );
        CheckQueues( port );

            // Let the tranmitter know that we have something ready to go.

        SetUpTransmit( port );
    }

    ELG( *count, size, 'enqd', "AppleUSBCDCDriver::enqueueData - Enqueue" );

    return kIOReturnSuccess;
	
}/* end enqueueData */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::dequeueData
//
//		Inputs:		size - buffer size, min - minimum bytes required, refCon - the Port
//
//		Outputs:	buffer - data returned, min - number of bytes
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

IOReturn AppleUSBCDCDriver::dequeueData( UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon )
{
    PortInfo_t 	*port = (PortInfo_t *) refCon;
    IOReturn 	rtn = kIOReturnSuccess;
    UInt32 	state = 0;

    ELG( size, min, 'dqDt', "AppleUSBCDCDriver::dequeueData" );
    
    if ( fTerminate )
        return kIOReturnOffline;
	
        // Check to make sure we have good arguments.
        
    if ( (count == NULL) || (buffer == NULL) || (min > size) )
        return kIOReturnBadArgument;

        // If the port is not active then there should not be any chars.
        
    *count = 0;
    if ( !(readPortState( port ) & PD_S_ACTIVE) )
        return kIOReturnNotOpen;

        // Get any data living in the queue.
        
    *count = RemovefromQueue( &port->RX, buffer, size, port->RXqueueRequestLock );
    CheckQueues( port );

    while ( (min > 0) && (*count < min) )
    {
            // Figure out how many bytes we have left to queue up
            
        state = 0;

        rtn = watchState( &state, PD_S_RXQ_EMPTY, refCon );

        if ( rtn != kIOReturnSuccess )
        {
            ELG( 0, rtn, 'DqD-', "AppleUSBCDCDriver::dequeueData - Interrupted!" );
            return rtn;
        }
        
            // Try and get more data starting from where we left off
            
        *count += RemovefromQueue( &port->RX, buffer + *count, (size - *count), port->RXqueueRequestLock );
        CheckQueues( port );
		
    }

        // Now let's check our receive buffer to see if we need to stop
        
    bool goXOIdle = (UsedSpaceinQueue( &port->RX ) < port->RXStats.LowWater) && (port->RXOstate == SENT_XOFF);

    if ( goXOIdle )
    {
        port->RXOstate = IDLE_XO;
        AddBytetoQueue( &port->TX, port->XOFFchar, port->TXqueueRequestLock );
        SetUpTransmit( port );
    }

    ELG( *count, size, 'deqd', "dequeueData -->Out Dequeue" );

    return rtn;
	
}/* end dequeueData */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::SetUpTransmit
//
//		Inputs:		port - the port to transmit on
//
//		Outputs:	return code - true (transmit started), false (transmission already in progress)
//
//		Desc:		Setup and then start transmisson on the port specified
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::SetUpTransmit( PortInfo_t *port )
{

    ELG( port, port->AreTransmitting, 'upTx', "AppleUSBCDCDriver::SetUpTransmit" );
	
        //  If we are already in the cycle of transmitting characters,
        //  then we do not need to do anything.
		
    if ( port->AreTransmitting == TRUE )
        return FALSE;

//    if ( GetQueueStatus( &port->TX ) != queueEmpty )
    if (UsedSpaceinQueue(&port->TX) > 0)
    {
        StartTransmission( port );
    }

    return TRUE;
	
}/* end SetUpTransmit */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::StartTransmission
//
//		Inputs:		port - the port to transmit on
//
//		Outputs:	None
//
//		Desc:		Start the transmisson on the port specified
//
/****************************************************************************************************/

void AppleUSBCDCDriver::StartTransmission( PortInfo_t *port )
{
    size_t	count;
    IOReturn	ior;

        // Sets up everything as we are running so as not to start this
        // port twice if a call occurs twice to this Method:
		
    port->AreTransmitting = TRUE;
    changeState( port, PD_S_TX_BUSY, PD_S_TX_BUSY );

        // Fill up the buffer with characters from the queue
		
    count = RemovefromQueue( &port->TX, port->PipeOutBuffer, MAX_BLOCK_SIZE, port->TXqueueRequestLock );
    ELG( port->State, count, ' Tx+', "AppleUSBCDCDriver::StartTransmission" );
    LogData( kUSBOut, count, port->PipeOutBuffer );	
    port->Count = count;

        // If there are no bytes to send just exit:
		
    if ( count <= 0 )
    {
            // Updates all the status flags:
			
        CheckQueues( port );
        port->AreTransmitting = FALSE;
        changeState( port, 0, PD_S_TX_BUSY );
        return;
    }

    port->PipeOutMDP->setLength( count );
    ior = port->OutPipe->Write( port->PipeOutMDP, &fWriteCompletionInfo );

        // We just removed a bunch of stuff from the
        // queue, so see if we can free some thread(s)
        // to enqueue more stuff.
		
    CheckQueues( port );

    return;
	
}/* end StartTransmission */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::USBSetLineCoding
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Set up and send SetLineCoding Management Element Request(MER) for all settings.
//
/****************************************************************************************************/

void AppleUSBCDCDriver::USBSetLineCoding( PortInfo_t *port )
{
    LineCoding		*lineParms;
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt16		lcLen = sizeof(LineCoding)-1;
	
    ELG( 0, port, 'USLC', "AppleUSBCDCDriver::USBSetLineCoding" );
	
	// First check that Set Line Coding is supported
	
    if (!(port->ACMCapabilities & ACM_DeviceSuppControl))
    {
        return;
    }
	
	// Check for changes and only do it if something's changed
	
    if ( (port->BaudRate == port->LastBaudRate) && (port->StopBits == port->LastStopBits) && 
        (port->TX_Parity == port->LastTX_Parity) && (port->CharLength == port->LastCharLength) )
    {
        return;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc( sizeof(IOUSBDevRequest) );
    if ( !MER )
    {
        ELG( 0, 0, 'USL-', "AppleUSBCDCDriver::USBSetLineCoding - allocate MER failed" );
        return;
    }
    bzero( MER, sizeof(IOUSBDevRequest) );
	
    lineParms = (LineCoding*)IOMalloc( lcLen );
    if ( !lineParms )
    {
        ELG( 0, 0, 'USL-', "AppleUSBCDCDriver::USBSetLineCoding - allocate lineParms failed" );
        return;
    }
    bzero( lineParms, lcLen ); 
	
        // convert BaudRate - intel format doubleword (32 bits) 
		
    OSWriteLittleInt32( lineParms, dwDTERateOffset, port->BaudRate );
    port->LastBaudRate = port->BaudRate;
    lineParms->bCharFormat = port->StopBits - 2;
    port->LastStopBits = port->StopBits;
    lineParms->bParityType = port->TX_Parity - 1;
    port->LastTX_Parity = port->TX_Parity;
    lineParms->bDataBits = port->CharLength;
    port->LastCharLength = port->CharLength;
	
    LogData( kUSBAnyDirn, lcLen, lineParms );
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSET_LINE_CODING;
    MER->wValue = 0;
    MER->wIndex = port->CommInterfaceNumber;
    MER->wLength = lcLen;
    MER->pData = lineParms;
	
    fMERCompletionInfo.parameter = MER;
	
    rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
    if ( rc != kIOReturnSuccess )
    {
        ELG( MER->bRequest, rc, 'SLER', "AppleUSBCDCDriver::USBSetLineCoding - error issueing DeviceRequest" );
        IOFree( MER->pData, lcLen );
        IOFree( MER, sizeof(IOUSBDevRequest) );
    }

}/* end USBSetLineCoding */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::USBSetControlLineState
//
//		Inputs:		port - the port, RTS - true(set RTS), false(clear RTS), DTR - true(set DTR), false(clear DTR)
//
//		Outputs:	None
//
//		Desc:		Set up and send SetControlLineState Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCDriver::USBSetControlLineState( PortInfo_t *port, bool RTS, bool DTR)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt16		CSBitmap = 0;
	
    ELG( 0, 0, 'USLC', "AppleUSBCDCDriver::USBSetControlLineState" );
	
	// First check that Set Control Line State is supported
	
    if (!(port->ACMCapabilities & ACM_DeviceSuppControl))
    {
        return;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc( sizeof(IOUSBDevRequest) );
    if ( !MER )
    {
        ELG( 0, 0, 'USL-', "AppleUSBCDCDriver::USBSetControlLineState - allocate MER failed" );
        return;
    }
    bzero( MER, sizeof(IOUSBDevRequest) );
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSET_CONTROL_LINE_STATE;
    if ( RTS )
        CSBitmap |= kRTSOn;
    if ( DTR )
        CSBitmap |= kDTROn;
    MER->wValue = CSBitmap;
    MER->wIndex = port->CommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
	
    fMERCompletionInfo.parameter = MER;
	
    rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
    if ( rc != kIOReturnSuccess )
    {
        ELG( MER->bRequest, rc, 'SLER', "AppleUSBCDCDriver::USBSetControlLineState - error issueing DeviceRequest" );
        IOFree( MER, sizeof(IOUSBDevRequest) );
    }

}/* end USBSetControlLineState */


/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::USBSendBreak
//
//		Inputs:		port - the port, sBreak - true(set Break), false(clear Break) - This may change
//
//		Outputs:	None
//
//		Desc:		Set up and send SendBreak Management Element Request(MER).
//
/****************************************************************************************************/

void AppleUSBCDCDriver::USBSendBreak( PortInfo_t *port, bool sBreak)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
	
    ELG( 0, 0, 'USLC', "AppleUSBCDCDriver::USBSendBreak" );
	
	// First check that Send Break is supported
	
    if (!(port->ACMCapabilities & ACM_DeviceSuppBreak))
    {
        return;
    }
	
    MER = (IOUSBDevRequest*)IOMalloc( sizeof(IOUSBDevRequest) );
    if ( !MER )
    {
        ELG( 0, 0, 'USL-', "AppleUSBCDCDriver::USBSendBreak - allocate MER failed" );
        return;
    }
    bzero( MER, sizeof(IOUSBDevRequest) );
	
        // now build the Management Element Request
		
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kUSBSET_CONTROL_LINE_STATE;
    if (sBreak)
    {
        MER->wValue = 0xFFFF;
    } else {
        MER->wValue = 0;
    }
    MER->wIndex = port->CommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
	
    fMERCompletionInfo.parameter = MER;
	
    rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
    if ( rc != kIOReturnSuccess )
    {
        ELG( MER->bRequest, rc, 'SLER', "AppleUSBCDCDriver::USBSendBreak - error issueing DeviceRequest" );
        IOFree( MER, sizeof(IOUSBDevRequest) );
    }

}/* end USBSendBreak */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::SetStructureDefaults
//
//		Inputs:		port - the port to set the defaults, Init - Probe time or not
//
//		Outputs:	None
//
//		Desc:		Sets the defaults for the specified port structure
//
/****************************************************************************************************/

void AppleUSBCDCDriver::SetStructureDefaults( PortInfo_t *port, bool Init )
{
    UInt32	tmp;
	
    ELG( 0, 0, 'StSD', "AppleUSBCDCDriver::SetStructureDefaults" );

        // These are set up at start and cannot get reset during execution.
        
    if ( Init )
    {
        port->FCRimage = 0x00;
        port->IERmask = 0x00;

        port->State = ( PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER );
        port->WatchStateMask = 0x00000000;
        port->serialRequestLock = NULL;
        port->RXqueueRequestLock = NULL;
        port->TXqueueRequestLock = NULL;
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
    }

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

    for ( tmp=0; tmp < (256 >> SPECIAL_SHIFT); tmp++ )
        port->SWspecial[ tmp ] = 0;

    return;
	
}/* end SetStructureDefaults */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::freeRingBuffer
//
//		Inputs:		Queue - the specified queue to free
//
//		Outputs:	None
//
//		Desc:		Frees all resources assocated with the queue, then sets all queue parameters 
//				to safe values.
//
/****************************************************************************************************/

void AppleUSBCDCDriver::freeRingBuffer( CirQueue *Queue )
{
    ELG( 0, Queue, 'f rb', "AppleUSBCDCDriver::freeRingBuffer" );

    IOFree( Queue->Start, Queue->Size );
    CloseQueue( Queue );
    return;
	
}/* end freeRingBuffer */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::allocateRingBuffer
//
//		Inputs:		Queue - the specified queue to allocate, BufferSize - size to allocate
//
//		Outputs:	return Code - true (buffer allocated), false (it failed)
//
//		Desc:		Allocates resources needed by the queue, then sets up all queue parameters. 
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::allocateRingBuffer( CirQueue *Queue, size_t BufferSize )
{
    UInt8	*Buffer;

        // Size is ignored and kMaxCirBufferSize, which is 4096, is used.
		
    ELG( 0, BufferSize, 'alrb', "AppleUSBCDCDriver::allocateRingBuffer" );
    Buffer = (UInt8*)IOMalloc( kMaxCirBufferSize );

    InitQueue( Queue, Buffer, kMaxCirBufferSize );

    if ( Buffer )
        return true;

    return false;
	
}/* end allocateRingBuffer */

/****************************************************************************************************/
//
//		Function:	WakeonRing
//
//		Inputs:		
//
//		Outputs:	return code - true(Wake-on-Ring enabled), false(disabled)	
//
//		Desc:		Find the PMU entry and checks the wake-on-ring flag
//
/****************************************************************************************************/

bool AppleUSBCDCDriver::WakeonRing()
{
    mach_timespec_t	t;
    IOService 		*pmu;
    bool		WoR = false;
	
    ELG( 0, 0, 'WoR ', "AppleUSBCDCDriver::WakeonRing" );
        
    t.tv_sec = 1;
    t.tv_nsec = 0;
    
    pmu = waitForService(IOService::serviceMatching( "ApplePMU" ), &t );
    if (pmu)
    {
        if (kOSBooleanTrue == pmu->getProperty("WakeOnRing"))
        {
            ELG( 0, 0, 'WREn', "AppleUSBCDCDriver::WakeonRing - Enabled" );
            WoR = true;
        } else {
            ELG( 0, 0, 'WRDs', "AppleUSBCDCDriver::WakeonRing - Disabled" );
        }
    } else {
        ELG( 0, 0, 'WRsf', "AppleUSBCDCDriver::WakeonRing - serviceMatching ApplePMU failed" );
    }
    
    return WoR;
    
}/* end WakeonRing */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCDriver::message
//
//		Inputs:		type - message type, provider - my provider, argument - additional parameters
//
//		Outputs:	return Code - kIOReturnSuccess
//
//		Desc:		Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::message( UInt32 type, IOService *provider,  void *argument )
{	
    UInt8	i;
	
    ELG( 0, type, 'mess', "AppleUSBCDCDriver::message" );
	
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
            ELG( fSessions, type, 'mess', "AppleUSBCDCDriver::message - kIOMessageServiceIsTerminated" );
			
            if ( fSessions )
            {
                for (i=0; i<numberofPorts; i++)
                {
                    if ( (fPorts[i] != NULL) && (fPorts[i]->serialRequestLock != 0) )
                    {
//			changeState( fPorts[i], 0, (UInt32)PD_S_ACTIVE );
                    }
                }
                if ( !fTerminate )		// Check if we're already being terminated
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
                for (i=0; i<numberofPorts; i++)
                {
                    if ( fPorts[i] != NULL )
                    {
                        if ( fPorts[i]->CommInterface )	
                        {
                            fPorts[i]->CommInterface->close( this );	
                            fPorts[i]->CommInterface->release();
                            fPorts[i]->CommInterface = NULL;	
                        }
	
                        if ( fPorts[i]->DataInterface )	
                        { 
                            fPorts[i]->DataInterface->close( this );	
                            fPorts[i]->DataInterface->release();
                            fPorts[i]->DataInterface = NULL;	
                        }
                    }
                }
                
            	fpDevice->close(this); 	// need to close so we can get the free and stop calls only if no sessions active (see releasePort)
                fpDevice = NULL;
            }
			
            fTerminate = true;		// we're being terminated (unplugged)
            return kIOReturnSuccess;			
        case kIOMessageServiceIsSuspended: 	
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - kIOMessageServiceIsSuspended" );
            break;			
        case kIOMessageServiceIsResumed: 	
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - kIOMessageServiceIsResumed" );
            break;			
        case kIOMessageServiceIsRequestingClose: 
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - kIOMessageServiceIsRequestingClose" ); 
            break;
        case kIOMessageServiceWasClosed: 	
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - kIOMessageServiceWasClosed" ); 
            break;
        case kIOMessageServiceBusyStateChange: 	
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - kIOMessageServiceBusyStateChange" ); 
            break;
        case kIOUSBMessagePortHasBeenResumed: 	
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - kIOUSBMessagePortHasBeenResumed" );
            break;
        case kIOUSBMessageHubResumePort:
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - kIOUSBMessageHubResumePort" );
            break;
        default:
            ELG( 0, type, 'mess', "AppleUSBCDCDriver::message - unknown message" ); 
            break;
    }
    
    return kIOReturnUnsupported;
}

/****************************************************************************************************/
//
//		Method:		readPortState
//
//		Inputs:		port - the specified port
//
//		Outputs:	returnState - current state of the port
//
//		Desc:		Reads the current Port->State. 
//
/****************************************************************************************************/

UInt32 AppleUSBCDCDriver::readPortState( PortInfo_t *port )
{
    UInt32	returnState = 0;
	
//	ELG( 0, port, 'rPSt', "readPortState" );

    if ( port &&  port->serialRequestLock )
    {
        IOLockLock( port->serialRequestLock );

        returnState = port->State;

        IOLockUnlock( port->serialRequestLock);
    }

//	ELG( returnState, 0, 'rPS-', "readPortState" );

    return returnState;
	
}/* end readPortState */

/****************************************************************************************************/
//
//		Method:		changeState
//
//		Inputs:		port - the specified port, state - new state, mask - state mask (the specific bits)
//
//		Outputs:	None
//
//		Desc:		Change the current Port->State to state using the mask bits.
//				if mask = 0 nothing is changed.
//				delta contains the difference between the new and old state taking the
//				mask into account and it's used to wake any waiting threads as appropriate. 
//
/****************************************************************************************************/

void AppleUSBCDCDriver::changeState( PortInfo_t *port, UInt32 state, UInt32 mask )
{
    UInt32	delta;
	
//	ELG( state, mask, 'chSt', "changeState" );
    if ( port &&  port->serialRequestLock )
    {

        IOLockLock( port->serialRequestLock );
        state = (port->State & ~mask) | (state & mask); 	// compute the new state
        delta = state ^ port->State;		    		// keep a copy of the diffs
        port->State = state;

	    // Wake up all threads asleep on WatchStateMask
		
        if ( delta & port->WatchStateMask )
            thread_wakeup_with_result( &port->WatchStateMask, THREAD_RESTART );

        IOLockUnlock( port->serialRequestLock );

        ELG( port->State, delta, 'chSt', "changeState --> changeState" );
    }
	
}/* end changeState */

/****************************************************************************************************/
//
//		Method:		privateWatchState
//
//		Inputs:		port - the specified port, state - state watching for, mask - state mask (the specific bits)
//
//		Outputs:	IOReturn - kIOReturnSuccess, kIOReturnIOError or kIOReturnIPCError
//
//		Desc:		Wait for the at least one of the state bits defined in mask to be equal
//				to the value defined in state. Check on entry then sleep until necessary.
//				A return value of kIOReturnSuccess means that at least one of the port state
//				bits specified by mask is equal to the value passed in by state.  A return
//				value of kIOReturnIOError indicates that the port went inactive.  A return
//				value of kIOReturnIPCError indicates sleep was interrupted by a signal. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCDriver::privateWatchState( PortInfo_t *port, UInt32 *state, UInt32 mask )
{
    unsigned 	watchState, foundStates;
    bool 	autoActiveBit = false;
    IOReturn 	rtn = kIOReturnSuccess;

//    ELG( mask, *state, 'wsta', "privateWatchState" );

    watchState = *state;
    IOLockLock( port->serialRequestLock );

        // hack to get around problem with carrier detection
		
//    if ( *state & PD_RS232_S_CAR )			/// mlj ???
//    {
//        port->State |= PD_RS232_S_CAR;
//    }

    if ( !(mask & (PD_S_ACQUIRED | PD_S_ACTIVE)) )
    {
        watchState &= ~PD_S_ACTIVE;		// Check for low PD_S_ACTIVE
        mask |=  PD_S_ACTIVE;			// Register interest in PD_S_ACTIVE bit
        autoActiveBit = true;
    }

    for (;;)
    {
            // Check port state for any interesting bits with watchState value
            // NB. the '^ ~' is a XNOR and tests for equality of bits.
			
        foundStates = (watchState ^ ~port->State) & mask;

        if ( foundStates )
        {
            *state = port->State;
            if ( autoActiveBit && (foundStates & PD_S_ACTIVE) )
            {
                rtn = kIOReturnIOError;
            } else {
                rtn = kIOReturnSuccess;
            }
            break;
        }

            // Everytime we go around the loop we have to reset the watch mask.
            // This means any event that could affect the WatchStateMask must
            // wakeup all watch state threads.  The two events are an interrupt
            // or one of the bits in the WatchStateMask changing.
			
        port->WatchStateMask |= mask;

            // note: Interrupts need to be locked out completely here,
            // since as assertwait is called other threads waiting on
            // &port->WatchStateMask will be woken up and spun through the loop.
            // If an interrupt occurs at this point then the current thread
            // will end up waiting with a different port state than assumed
            //  -- this problem was causing dequeueData to wait for a change in
            // PD_E_RXQ_EMPTY to 0 after an interrupt had already changed it to 0.

        assert_wait( &port->WatchStateMask, true );	// assert event

        IOLockUnlock( port->serialRequestLock );
        rtn = thread_block( (void(*)(void))0 );		// block ourselves
        IOLockLock( port->serialRequestLock );

        if ( rtn == THREAD_RESTART )
        {
            continue;
        } else {
            rtn = kIOReturnIPCError;
            break;
        }
    }

        // As it is impossible to undo the masking used by this
        // thread, we clear down the watch state mask and wakeup
        // every sleeping thread to reinitialize the mask before exiting.
		
    port->WatchStateMask = 0;

    thread_wakeup_with_result( &port->WatchStateMask, THREAD_RESTART );
    IOLockUnlock( port->serialRequestLock);
	
    return rtn;
	
}/* end privateWatchState */
