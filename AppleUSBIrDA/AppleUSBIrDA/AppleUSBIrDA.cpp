
    /* Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

    /* AppleUSBIrDA.cpp - MacOSX implementation of USB IrDA Driver. */

#include <machine/limits.h>         /* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/IOModemSerialStreamSync.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>

#include <UserNotification/KUNCUserNotifications.h>

#include "AppleUSBIrDA.h"
#include "IrDAComm.h"
#include "IrDAUser.h"
#include "IrDALog.h"
#include "IrDADebugging.h"


#if (hasTracing > 0 && hasAppleUSBIrDATracing > 0)

enum tracecodes
{
    kLogInit = 1,
    kLogFree,
    kLogProbe,
    kLogStart,
    kLogStop,
    
    kLogNewNub,
    kLogNewPort,
    kLogDestroyNub,
    
    kLogSetSpeed,
    kLogGetState,
    kLogXmitLen,
    kLogXmitData,
    
    kLogAddRxBytes,
    kLogSetBofCount,
    kLogGetIrDAStatus,
    
    kLogSetIrDAState,
    kLogInterruptRead,
    kLogDataReadComplete,
    
    kLogInputData,
    kLogDataWriteComplete,
    kLogDataWriteCompleteZero,
    
    kLogAllocateResources,
    kLogReleaseResources,
    kLogConfigureDevice,
    kLogCreateSerialStream,
    
    kLogAcquirePort,
    kLogReleasePort,
    kLogSetState,
    kLogWatchState,
    
    kLogExecEvent,
    kLogExecEventData,
    kLogReqEvent,
    kLogReqEventData,
    
    kLogSetupTransmit,
    kLogSetStructureDefaults,
    kLogMessage,
    
    kLogWorkAround,
    kLogWorkAroundComplete,
    
    kLogInitForPM,
    kLogInitialPowerState,
    kLogSetPowerState
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kLogInit,          "AppleUSBIrDADriver: init"},
    {kLogFree,          "AppleUSBIrDADriver: free"},
    {kLogProbe,         "AppleUSBIrDADriver: probe, provider="},
    {kLogStart,         "AppleUSBIrDADriver: start, provider="},
    {kLogStop,          "AppleUSBIrDADriver: stop"},
    
    {kLogNewNub,        "AppleUSBIrDADriver: new nub"},
    {kLogNewPort,       "AppleUSBIrDADriver: new port"},
    {kLogDestroyNub,    "AppleUSBIrDADriver: destroy nub"},
	    
    {kLogSetSpeed,      "AppleUSBIrDADriver: set irda speed"},
    {kLogGetState,      "AppleUSBIrDADriver: get state"},
    {kLogXmitLen,       "AppleUSBIrDADriver: xmit length"},
    {kLogXmitData,      "AppleUSBIrDADriver: xmit data"},
    
    {kLogAddRxBytes,    "AppleUSBIrDADriver: add rx bytes, count="},
    {kLogSetBofCount,   "AppleUSBIrDADriver: set bof count, input="},
    {kLogGetIrDAStatus, "AppleUSBIrDADriver: get irda status, on="},
    
    {kLogSetIrDAState,          "AppleUSBIrDADriver: set irda state, current=, want="},
    {kLogInterruptRead,         "AppleUSBIrDADriver: interrupt read complete"},
    {kLogDataReadComplete,      "AppleUSBIrDADriver: data read complete, rc, len="},
    
    {kLogInputData,             "AppleUSBIrDADriver: data read buffer"},
    {kLogDataWriteComplete,     "AppleUSBIrDADriver: data write complete, rc, len="},
    {kLogDataWriteCompleteZero, "AppleUSBIrDADriver: data write complete sending zero length packet"},
    
    {kLogAllocateResources,     "AppleUSBIrDADriver: allocate resources"},
    {kLogReleaseResources,      "AppleUSBIrDADriver: release resources"},
    {kLogConfigureDevice,       "AppleUSBIrDADriver: configure device"},
    {kLogCreateSerialStream,    "AppleUSBIrDADriver: create serial stream"},
    
    {kLogAcquirePort,       "AppleUSBIrDADriver: acquire port"},
    {kLogReleasePort,       "AppleUSBIrDADriver: release port"},
    {kLogSetState,          "AppleUSBIrDADriver: set state"},
    {kLogWatchState,        "AppleUSBIrDADriver: watch state"},
    
    {kLogExecEvent,         "AppleUSBIrDADriver: execute event"},
    {kLogExecEventData,     "AppleUSBIrDADriver: execute event, data="},
    {kLogReqEvent,          "AppleUSBIrDADriver: request event"},
    {kLogReqEventData,      "AppleUSBIrDADriver: request event, data="},
    
    {kLogSetupTransmit,         "AppleUSBIrDADriver: setup transmit"},
    {kLogSetStructureDefaults,  "AppleUSBIrDADriver: set structure defaults"},
    {kLogMessage,               "AppleUSBIrDADriver: message"},
    
    {kLogWorkAround,            "AppleUSBIrDADriver: workaround called"},
    {kLogWorkAroundComplete,    "AppleUSBIrDADriver: workaround complete"},
    
    {kLogInitForPM,             "AppleUSBIrDADriver: init power management"},
    {kLogInitialPowerState,     "AppleUSBIrDADriver: get initial power state, flags="},
    {kLogSetPowerState,         "AppleUSBIrDADriver: set power state, ordinal="}

};


#define XTRACE(x, y, z) IrDALogAdd ( x, y, ((uintptr_t)z & 0xffff), gTraceEvents, true)
#else
#define XTRACE(x, y, z) ((void)0)
#endif


enum {
    kUseInterruptsForRead   = true
};

enum {
    kIrDAPowerOffState  = 0,
    kIrDAPowerOnState   = 1,
    kNumIrDAStates = 2
};

static IOPMPowerState gOurPowerStates[kNumIrDAStates] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};


    static IrDAglobals      g;  /**** Instantiate the globals ****/

#define super IOSerialDriverSync

    OSDefineMetaClassAndStructors( AppleUSBIrDADriver, IOSerialDriverSync );
    
/****************************************************************************************************/
//
//      Function:   Asciify
//
//      Inputs:     i - the nibble
//
//      Outputs:    return byte - ascii byte
//
//      Desc:       Converts to ascii. 
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
//      Function:   AllocateEventLog
//
//      Inputs:     size - amount of memory to allocate
//
//      Outputs:    None
//
//      Desc:       Allocates the event log buffer
//
/****************************************************************************************************/

void AllocateEventLog( UInt32 size )
{
    if ( g.evLogBuf )
	return;

    g.evLogFlag = 0;            /* assume insufficient memory   */
    g.evLogBuf = (UInt8*)IOMalloc( size );
    if ( !g.evLogBuf )
    {
	kprintf( "AppleUSBIrDA: evLog allocation failed" );
	return;
    }

    bzero( g.evLogBuf, size );
    g.evLogBufp = g.evLogBuf;
    g.evLogBufe = g.evLogBufp + kEvLogSize - 0x20; // ??? overran buffer?
    g.evLogFlag  = 0xFEEDBEEF;  // continuous wraparound
//  g.evLogFlag  = 'step';      // stop at each ELG
//  g.evLogFlag  = 0x0333;      // any nonzero - don't wrap - stop logging at buffer end

    IOLog( "AppleUSBIrDA: AllocateEventLog - &USBglobals=%p buffer=%p", &g, (uintptr_t)g.evLogBuf );

    return;
    
}/* end AllocateEventLog */

/****************************************************************************************************/
//
//      Function:   EvLog
//
//      Inputs:     a - anything, b - anything, ascii - 4 charater tag, str - any info string           
//
//      Outputs:    None
//
//      Desc:       Writes the various inputs to the event log buffer
//
/****************************************************************************************************/

void EvLog( UInt32 a, UInt32 b, UInt32 ascii, char* str )
{
    register UInt32     *lp;           /* Long pointer      */
    mach_timespec_t     time;

    if ( g.evLogFlag == 0 )
	return;

    IOGetTime( &time );

    lp = (UInt32*)g.evLogBufp;
    g.evLogBufp += 0x10;

    if ( g.evLogBufp >= g.evLogBufe )       /* handle buffer wrap around if any */
    {    g.evLogBufp  = g.evLogBuf;
	if ( g.evLogFlag != 0xFEEDBEEF )    // make 0xFEEDBEEF a symbolic ???
	    g.evLogFlag = 0;                /* stop tracing if wrap undesired   */
    }

	/* compose interrupt level with 3 byte time stamp:  */

    *lp++ = (g.intLevel << 24) | ((time.tv_nsec >> 10) & 0x003FFFFF);   // ~ 1 microsec resolution
    *lp++ = a;
    *lp++ = b;
    *lp   = ascii;

    if( g.evLogFlag == 'step' )
    {   static char code[ 5 ] = {0,0,0,0,0};
	*(UInt32*)&code = ascii;
	IOLog( "AppleUSBIrDA: %8x %8x %8x %s\n", time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code );
    }

    return;
    
}/* end EvLog */
#endif // USE_ELG

#if LOG_DATA
#define dumplen     32      // Set this to the number of bytes to dump and the rest should work out correct

#define buflen      ((dumplen*2)+dumplen)+3
#define Asciistart  (dumplen*2)+3

/****************************************************************************************************/
//
//      Function:   DEVLogData
//
//      Inputs:     Dir - direction, Count - number of bytes, buf - the data
//
//      Outputs:    None
//
//      Desc:       Puts the data in the log. 
//
/****************************************************************************************************/

void DEVLogData(UInt8 Dir, UInt32 Count, char *buf)
{
    UInt8       wlen, i, Aspnt, Hxpnt;
    UInt8       wchr;
    char        LocBuf[buflen+1];

    for ( i=0; i<=buflen; i++ )
    {
	LocBuf[i] = 0x20;
    }
    LocBuf[i] = 0x00;
    
    if ( Dir == kUSBIn )
    {
	IOLog( "AppleUSBIrDA: USBLogData - Received, size = %8lx\n", (long unsigned int)Count );
    } else {
	if ( Dir == kUSBOut )
	{
	    IOLog( "AppleUSBIrDA: USBLogData - Write, size = %8lx\n", (long unsigned int)Count );
	} else {
	    if ( Dir == kUSBAnyDirn )
	    {
		IOLog( "AppleUSBIrDA: USBLogData - Other, size = %8lx\n", (long unsigned int)Count );
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
	    if (( wchr < 0x20) || (wchr > 0x7F ))       // Non printable characters
	    {
		LocBuf[Aspnt++] = 0x2E;                 // Replace with a period
	    } else {
		LocBuf[Aspnt++] = wchr;
	    }
	}
	LocBuf[(wlen + Asciistart) + 1] = 0x00;
	IOLog( "%s\n", LocBuf );
    } else {
	IOLog( "AppleUSBIrDA: USBLogData - No data, Count = 0\n" );
    }
    
}/* end DEVLogData */
#endif // LOG_DATA

/* QueuePrimatives  */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::AddBytetoQueue
//
//      Inputs:     Queue - the queue to be added to
//
//      Outputs:    Value - Byte to be added, Queue status - full or no error
//
//      Desc:       Add a byte to the circular queue.
//
/****************************************************************************************************/

QueueStatus AppleUSBIrDADriver::AddBytetoQueue( CirQueue *Queue, char Value )
{
    /* Check to see if there is space by comparing the next pointer,    */
    /* with the last, If they match we are either Empty or full, so     */
    /* check the InQueue of being zero.                 */

    require(fPort && fPort->serialRequestLock, Fail);
    IOLockLock( fPort->serialRequestLock );

    if ( (Queue->NextChar == Queue->LastChar) && Queue->InQueue ) {
	IOLockUnlock( fPort->serialRequestLock);
	return queueFull;
    }

    *Queue->NextChar++ = Value;
    Queue->InQueue++;

	/* Check to see if we need to wrap the pointer. */
	
    if ( Queue->NextChar >= Queue->End )
	Queue->NextChar =  Queue->Start;

    IOLockUnlock( fPort->serialRequestLock);
    return queueNoError;
    
Fail:
    return queueFull;       // for lack of a better error
    
}/* end AddBytetoQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::GetBytetoQueue
//
//      Inputs:     Queue - the queue to be removed from
//
//      Outputs:    Value - where to put the byte, Queue status - empty or no error
//
//      Desc:       Remove a byte from the circular queue.
//
/****************************************************************************************************/

QueueStatus AppleUSBIrDADriver::GetBytetoQueue( CirQueue *Queue, UInt8 *Value )
{

    require(fPort && fPort->serialRequestLock, Fail);
    IOLockLock( fPort->serialRequestLock );

	/* Check to see if the queue has something in it.   */
	
    if ( (Queue->NextChar == Queue->LastChar) && !Queue->InQueue ) {
	IOLockUnlock(fPort->serialRequestLock);
	return queueEmpty;
    }

    *Value = *Queue->LastChar++;
    Queue->InQueue--;

	/* Check to see if we need to wrap the pointer. */
	
    if ( Queue->LastChar >= Queue->End )
	Queue->LastChar =  Queue->Start;

    IOLockUnlock(fPort->serialRequestLock);
    return queueNoError;
    
Fail:
    return queueEmpty;          // can't get to it, pretend it's empty
    
}/* end GetBytetoQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::InitQueue
//
//      Inputs:     Queue - the queue to be initialized, Buffer - the buffer, size - length of buffer
//
//      Outputs:    Queue status - queueNoError.
//
//      Desc:       Pass a buffer of memory and this routine will set up the internal data structures.
//
/****************************************************************************************************/

QueueStatus AppleUSBIrDADriver::InitQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size )
{
    Queue->Start    = Buffer;
    Queue->End      = (UInt8*)((size_t)Buffer + Size);
    Queue->Size     = Size;
    Queue->NextChar = Buffer;
    Queue->LastChar = Buffer;
    Queue->InQueue  = 0;

    IOSleep( 1 );
    
    return queueNoError ;
    
}/* end InitQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::CloseQueue
//
//      Inputs:     Queue - the queue to be closed
//
//      Outputs:    Queue status - queueNoError.
//
//      Desc:       Clear out all of the data structures.
//
/****************************************************************************************************/

QueueStatus AppleUSBIrDADriver::CloseQueue( CirQueue *Queue )
{

    Queue->Start    = 0;
    Queue->End      = 0;
    Queue->NextChar = 0;
    Queue->LastChar = 0;
    Queue->Size     = 0;

    return queueNoError;
    
}/* end CloseQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::AddtoQueue
//
//      Inputs:     Queue - the queue to be added to, Buffer - data to add, Size - length of data
//
//      Outputs:    BytesWritten - Number of bytes actually put in the queue.
//
//      Desc:       Add an entire buffer to the queue.
//
/****************************************************************************************************/

size_t AppleUSBIrDADriver::AddtoQueue( CirQueue *Queue, UInt8 *Buffer, size_t Size )
{
    size_t      BytesWritten = 0;

    while ( FreeSpaceinQueue( Queue ) && (Size > BytesWritten) )
    {
	AddBytetoQueue( Queue, *Buffer++ );
	BytesWritten++;
    }

    return BytesWritten;
    
}/* end AddtoQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::RemovefromQueue
//
//      Inputs:     Queue - the queue to be removed from, Size - size of buffer
//
//      Outputs:    Buffer - Where to put the data, BytesReceived - Number of bytes actually put in Buffer.
//
//      Desc:       Get a buffers worth of data from the queue.
//
/****************************************************************************************************/

size_t AppleUSBIrDADriver::RemovefromQueue( CirQueue *Queue, UInt8 *Buffer, size_t MaxSize )
{
    size_t      BytesReceived = 0;
    UInt8       Value;
    
    while( (MaxSize > BytesReceived) && (GetBytetoQueue(Queue, &Value) == queueNoError) ) 
    {
	*Buffer++ = Value;
	BytesReceived++;
    }/* end while */

    return BytesReceived;
    
}/* end RemovefromQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::FreeSpaceinQueue
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    Return Value - Free space left
//
//      Desc:       Return the amount of free space left in this buffer.
//
/****************************************************************************************************/

size_t AppleUSBIrDADriver::FreeSpaceinQueue( CirQueue *Queue )
{
    size_t  retVal = 0;

    require(fPort && fPort->serialRequestLock, Fail);
    IOLockLock( fPort->serialRequestLock );

    retVal = Queue->Size - Queue->InQueue;
    
    IOLockUnlock(fPort->serialRequestLock);
    
Fail:
    return retVal;
    
}/* end FreeSpaceinQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::UsedSpaceinQueue
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    UsedSpace - Amount of data in buffer
//
//      Desc:       Return the amount of data in this buffer.
//
/****************************************************************************************************/

size_t AppleUSBIrDADriver::UsedSpaceinQueue( CirQueue *Queue )
{
    return Queue->InQueue;
    
}/* end UsedSpaceinQueue */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::GetQueueSize
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    QueueSize - The size of the queue.
//
//      Desc:       Return the total size of the queue.
//
/****************************************************************************************************/

size_t AppleUSBIrDADriver::GetQueueSize( CirQueue *Queue )
{
    return Queue->Size;
    
}/* end GetQueueSize */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::GetQueueStatus
//
//      Inputs:     Queue - the queue to be queried
//
//      Outputs:    Queue status - full, empty or no error
//
//      Desc:       Returns the status of the circular queue.
//
/****************************************************************************************************/
/*
QueueStatus AppleUSBIrDADriver::GetQueueStatus( CirQueue *Queue )
{
    if ( (Queue->NextChar == Queue->LastChar) && Queue->InQueue )
	return queueFull;
    else if ( (Queue->NextChar == Queue->LastChar) && !Queue->InQueue )
	return queueEmpty;
	
    return queueNoError ;
    
}*/ /* end GetQueueStatus */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::CheckQueues
//
//      Inputs:     port - the port to check
//
//      Outputs:    None
//
//      Desc:       Checks the various queue's etc and manipulates the state(s) accordingly
//
/****************************************************************************************************/

void AppleUSBIrDADriver::CheckQueues( PortInfo_t *port )
{
    unsigned long   Used;
    unsigned long   Free;
    unsigned long   QueuingState;
    unsigned long   DeltaState;

    // Initialise the QueueState with the current state.
    QueuingState = readPortState( port );

	/* Check to see if there is anything in the Transmit buffer. */
    Used = UsedSpaceinQueue( &port->TX );
    Free = FreeSpaceinQueue( &port->TX );
//  ELG( Free, Used, 'CkQs', "CheckQueues" );
    if ( Free == 0 )
    {
	QueuingState |=  PD_S_TXQ_FULL;
	QueuingState &= ~PD_S_TXQ_EMPTY;
    }
    else if ( Used == 0 )
    {
	QueuingState &= ~PD_S_TXQ_FULL;
	QueuingState |=  PD_S_TXQ_EMPTY;
    }
    else
    {
	QueuingState &= ~PD_S_TXQ_FULL;
	QueuingState &= ~PD_S_TXQ_EMPTY;
    }

	/* Check to see if we are below the low water mark. */
    if ( Used < port->TXStats.LowWater )
	 QueuingState |=  PD_S_TXQ_LOW_WATER;
    else QueuingState &= ~PD_S_TXQ_LOW_WATER;

    if ( Used > port->TXStats.HighWater )
	 QueuingState |= PD_S_TXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_TXQ_HIGH_WATER;


	/* Check to see if there is anything in the Receive buffer. */
    Used = UsedSpaceinQueue( &port->RX );
    Free = FreeSpaceinQueue( &port->RX );

    if ( Free == 0 )
    {
	QueuingState |= PD_S_RXQ_FULL;
	QueuingState &= ~PD_S_RXQ_EMPTY;
    }
    else if ( Used == 0 )
    {
	QueuingState &= ~PD_S_RXQ_FULL;
	QueuingState |= PD_S_RXQ_EMPTY;
    }
    else
    {
	QueuingState &= ~PD_S_RXQ_FULL;
	QueuingState &= ~PD_S_RXQ_EMPTY;
    }

	/* Check to see if we are below the low water mark. */
    if ( Used < port->RXStats.LowWater )
	 QueuingState |= PD_S_RXQ_LOW_WATER;
    else QueuingState &= ~PD_S_RXQ_LOW_WATER;

    if ( Used > port->RXStats.HighWater )
	 QueuingState |= PD_S_RXQ_HIGH_WATER;
    else QueuingState &= ~PD_S_RXQ_HIGH_WATER;

	/* Figure out what has changed to get mask.*/
    DeltaState = QueuingState ^ readPortState( port );
    changeState( port, QueuingState, DeltaState );
    
    return;
    
}/* end CheckQueues */

/* end of QueuePrimatives */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::Add_RXBytes
//
//      Inputs:     Buffer - the raw input data, Size - the length
//
//      Outputs:
//
//      Desc:       Adds data to the circular receive queue 
//
/****************************************************************************************************/

void AppleUSBIrDADriver::Add_RXBytes( UInt8 *Buffer, size_t Size )
{
    XTRACE(kLogAddRxBytes, 0, Size);
    ELG( 0, Size, 'AdRB', "Add_RXBytes" );
    
    AddtoQueue( &fPort->RX, Buffer, Size );
    CheckQueues( fPort );
}/* end Add_RXBytes */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::SetBofCount
//
//      Inputs:     bof_count - the requested number of Beginning Of Frames
//
//      Outputs:    return word - the actual count (not bofs)
//
//      Desc:       Encode the requested number of BOF bytes to the first value that's big enough 
//
/****************************************************************************************************/  

SInt16 AppleUSBIrDADriver::SetBofCount( SInt16 bof_count )
{
    SInt16 counts[] = { 0, 1, 2, 3, 6, 12, 24, 48, -1};     // the bof counts that are encoded below
    SInt16  codes[] = { 8, 7, 6, 5, 4,  3,  2,  1,  1};     // could use an f(i), but this is easier to match to spec
    int i, sz;
    
    ELG( 0, bof_count, 'Sbof', "SetBofCount" );
    XTRACE(kLogSetBofCount, 0, bof_count);
    
    // input is desired bof count at the current speed, but the usb hardware wants the
    // unadjusted bof count (i.e. xbofs at 115k bps), so we have to adjust back to 115k.
    
    if (fCurrentBaud < 115200) {
	bof_count = bof_count * (115200 / fCurrentBaud);
	XTRACE(kLogSetBofCount, 1, bof_count);
    }
    
    sz = sizeof(counts) / sizeof(counts[0]);
    
    // note that the input bof counts can be computed, so we do an 'at least' test instead
    // of insisting upon an exact match to one of the supported bof counts.
    
    for (i = 0 ; i < sz; i++) 
    {
	if (counts[i] >= bof_count || counts[i] < 0)        // if table entry at least what's wanted (no smaller), or end-of-table
	{       
	    fBofsCode = codes[i];                           // then save the encoded version
	    return counts[i];                               // and return raw count used to caller
	}
    }
    
    ELG( 0, i, 'Sbf-', "SetBofCount - logic error" );
    return 0;
}/* end SetBofCount */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::SetSpeed
//
//      Inputs:     brate - the requested baud rate
//
//      Outputs:    return word - baud coding
//
//      Desc:       Set the baudrate for the device 
//
/****************************************************************************************************/  

UInt16 AppleUSBIrDADriver::SetSpeed( UInt32 brate )
{
    XTRACE(kLogSetSpeed, brate >> 16, (short)brate);
    ELG( 0, brate, 'Sbof', "SetSpeed" );
    
    fCurrentBaud = brate;
    
    switch (brate)
    {
	case 2400: 
	    fBaudCode = kLinkSpeed2400;     // 0x01
	    break;
	    
	case 9600: 
	    fBaudCode = kLinkSpeed9600;     // 0x02
	    break;
	    
	case 19200: 
	    fBaudCode = kLinkSpeed19200;    // 0x03
	    break;
	    
	case 38400: 
	    fBaudCode = kLinkSpeed38400;    // 0x04
	    break;
	    
	case 57600: 
	    fBaudCode = kLinkSpeed57600;    // 0x05
	    break;
	    
	case 115200:
	    fBaudCode = kLinkSpeed115200;   // 0x06
	    break;
	    
	case 576000:
	    fBaudCode = kLinkSpeed576000;   // 0x07
	    break;
	    
	case 1152000:
	    fBaudCode = kLinkSpeed1152000;  // 0x08
	    break;
	    
	case 4000000:
	    fBaudCode = kLinkSpeed4000000;  // 0x09
	    break;
	    
	case 300: 
	case 600: 
	case 1200: 
	case 1800: 
	case 3600: 
	case 4800: 
	case 7200: 
	default:
	    ELG( 0, brate, 'SSp-', "SetSpeed - Unsupported baud rate");
	    fBaudCode = 0;
	    break;
    }
    
    // start a one-byte transmit to set the speed in the device
    StartTransmit(0, NULL, 0, NULL);            // no control or data bytes, just the mode byte please
						// note SetSpeedComplete is called out of transmit complete
    return fBaudCode;
}/* end SetSpeed */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::GetIrDAComm
//
//      Inputs: 
//
//      Outputs:    IrDAComm - Address of the IrDA object
//
//      Desc:       Returns the address of the IrDA object 
//
/****************************************************************************************************/

IrDAComm* AppleUSBIrDADriver::GetIrDAComm( void )
{
    return fIrDA;
}/* end GetIrDAComm */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::GetIrDAQoS
//
//      Inputs: 
//
//      Outputs:    USBIrDAQoS - Address of the QoS structure
//
//      Desc:       Returns the address of the Quality of Service structure
//
/****************************************************************************************************/

USBIrDAQoS* AppleUSBIrDADriver::GetIrDAQoS( void )
{
    return &fQoS;
}/* end GetIrDAQoS */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::GetIrDAStatus
//
//      Inputs:     status - status structure
//
//      Outputs:    
//
//      Desc:       Sets the connection state and CRC errors of the status structure
//
/****************************************************************************************************/

void AppleUSBIrDADriver::GetIrDAStatus( IrDAStatus *status )
{
    //int review_get_irda_status;     // check w/irda on/off logic
    
    ELG( 0, 0, 'GIrS', "GetIrDAStatus" );
    XTRACE(kLogGetIrDAStatus, 0, fIrDAOn);
    
    if ( !fIrDAOn )
    {
	    //bzero( status, sizeof(IrDAStatus) );
	    status->connectionState = kIrDAStatusOff;
    } else {
	    if ( status->connectionState == kIrDAStatusOff )
	    {
		status->connectionState = kIrDAStatusIdle;
	    }
	    status->crcErrors = 0;                  // Unavailable
	}
    
}/* end GetIrDAStatus */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::CheckIrDAState
//
//      Inputs:     open session count (fSessions)
//                  user-client start request (fStartStopUserClient)
//                  usb start/stop (fStartStopUSB) -- replace with fTerminate?
//
//      Outputs:    
//
//      Desc:       Turns IrDA on or off if appropriate
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::CheckIrDAState( void )
{
    IOReturn    ior = kIOReturnSuccess;
    Boolean     newState = fUSBStarted &    // usb must have started, and 
			(fPowerState == kIrDAPowerOnState) &&   // powered on by the power manager, and
			(fUserClientStarted | (fSessions > 0)); // one of the clients too

    ELG( 0, 0, 'SIrS', "CheckIrDAState" );
    XTRACE(kLogSetIrDAState, fIrDAOn, newState);
    
    if ( newState && !fIrDAOn )         // Turn IrDA on if needed
    {
	fIrDAOn = true;
	fTerminate = false;
	
	if (!fSuspendFail) {            // if previous suspend worked, then
					// resume it and startIrDA will run from message().
	    ior = fpDevice->SuspendDevice( false );     // Ask to resume the device
	    if ( ior != kIOReturnSuccess )
	    {
		ELG( 0, ior, 'SIR-', "SetIrDAState - Resume failed" );
		IOLog("AppleUSBIrDA: failed to resume device\n");
		fIrDAOn = false;            // We're basically dead at this point
		fTerminate = true;
	    }
	}
	else{                   // earlier suspend failed, just start irda here
	    if ( !startIrDA() )
	    {
		fIrDAOn = false;
		fTerminate = true;
		IOLog("AppleUSBIrDADriver: SetIrDAState - startIrDA failed" );
	    } else {
		ELG( 0, 0, 'msc+', "SetIrDAState - startIrDA successful" );
		//IOLog("AppleUSBIrDADriver: message - startIrDA successful\n" );
	    }
	}
    }
    else if (!newState && fIrDAOn)      // Turn IrDA off if needed
    {
	fIrDAOn = false;
	fTerminate = true;              // Make it look like we've been terminated
	    
	stopIrDA();                     // stop irda and stop pipe i/o
	
	ior = fpDevice->SuspendDevice( true );  // Try to suspend the device
	if ( ior != kIOReturnSuccess )
	{
	    ELG( 0, 0, 'SIS-', "SetIrDAState - Suspend failed" );
	    IOLog("AppleUSBIrDA: failed to suspend device\n");
	}
    }
    
    return ior;
}/* end CheckIrDAState */

//
// User client has asked to start/stop irda.  do it if
// it's ok w/bsd open count and usb start/stop flag.
//
IOReturn AppleUSBIrDADriver::SetIrDAUserClientState( bool IrDAOn )
{
    fUserClientStarted = IrDAOn;
    return CheckIrDAState();
}

/****************************************************************************************************/
//
//      Function:   AppleUSBIrDADriver::init
//
//      Inputs:     dict - Dictionary
//
//      Outputs:    Return code - from super::init
//
//      Desc:       Driver initialization
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::init( OSDictionary *dict )
{
    bool    rc;
    
    rc = super::init( dict );
    IOLogIt( (uintptr_t)IrDALogGetInfo(), rc, 'init', "init" );
    XTRACE(kLogInit, 0, rc);
    return rc;
    
}/* end init */

/****************************************************************************************************/
//
//      Function:   AppleUSBIrDADriver::probe
//
//      Inputs:     provider - my provider
//
//      Outputs:    IOService - from super::probe, score - probe score
//
//      Desc:       Modify the probe score if necessary (we don't  at the moment)
//
/****************************************************************************************************/

IOService* AppleUSBIrDADriver::probe( IOService *provider, SInt32 *score )
{ 
    IOService       *res;

    res = super::probe( provider, score );
    IOLogIt( provider, res, 'prob', "probe" );
    XTRACE(kLogProbe, 0, provider);
    return res;
    
}/* end probe */


/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::start
//
//      Inputs:     provider - my provider
//
//      Outputs:    Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//      Desc:       This is called once it has beed determined I'm probably the best 
//                  driver for this device.
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::start( IOService *provider )
{
    UInt8   configs;    // number of device configurations
    bool    ok;
	
    XTRACE(kLogStart, 0, provider);

    g.evLogBufp = NULL;

    fTerminate = false;     // Make sure we don't think we're being terminated
    fPort = NULL;
    fIrDA = NULL;
    fNub = NULL;
    fIrDAOn = false;
    fSuspendFail = false;
    fpInterface = NULL;
    
    fpinterruptPipeBuffer = NULL;
    fPipeInBuffer = NULL;
    fPipeOutBuffer = NULL;
    
    fpDevice = NULL;
    fpInPipe = NULL;
    fpOutPipe = NULL;
    fpInterruptPipe = NULL;
    
    fUserClientStarted = false;     // user/client has not started us yet
    fUSBStarted = false;            // set to true when start finishes up ok
    fSessions = 0;
    
    fReadActive = false;
    
    fWriteActive = false;
    
    fGate = IOCommandGate::commandGate(this, 0);    // create a new command gate for PM
    require(fGate, Fail);
    getWorkLoop()->addEventSource(fGate);           // add it to the usb workloop
	
#if USE_ELG
    AllocateEventLog( kEvLogSize );
    ELG( &g, g.evLogBufp, 'USBM', "start - event logging set up." );

    waitForService( resourceMatching( "kdp" ) );
#endif /* USE_ELG */

    ELG( this, provider, 'strt', "start - this, provider." );
    if( !super::start( provider ) )
    {
	IOLogIt( 0, 0, 'SS--', "start - super failed" );
	return false;
    }

    /* Get my USB provider - the interface and then get the device */

    fpInterface = OSDynamicCast( IOUSBInterface, provider );
    require(fpInterface, Fail);
    
    fpDevice = fpInterface->GetDevice();
    require(fpDevice, Fail);
    
    /* Let's see if we have any configurations to play with */
	
    configs = fpDevice->GetNumConfigurations();
    require(configs == 1, Fail);
	
    // make our nub (and fPort) now
    ok = createNub();
    require(ok, Fail);
	
    // Now configure it (leaves device suspended)
    ok = configureDevice(configs);
    require(ok, Fail);
    
    // Finally create the bsd tty (serial stream) and leave it there until usb stop
	
    ok = createSerialStream();
    require(ok, Fail);
    
    ok = initForPM(provider);
    require(ok, Fail);
    
    fUSBStarted = true;     // now pay attn to bsd open's and user client start-irda requests

    return true;
    
Fail:

    IOLogIt( 0, 0, 'sts-', "start - failed" );
    stop( provider );
    
    return false;
    
}/* end start */

//
// initForPM
//
// Add ourselves to the power management tree so we
// can do the right thing on sleep/wakeup.
//
bool AppleUSBIrDADriver::initForPM(IOService * provider)
{
    XTRACE(kLogInitForPM, 0, 0);
    
    fPowerThreadCall = thread_call_allocate(handleSetPowerState, (thread_call_param_t) this );
    require(fPowerThreadCall != NULL, Fail);
    
    fPowerState = kIrDAPowerOnState;        // init our power state to be 'on'
    PMinit();                               // init power manager instance variables
    provider->joinPMtree(this);             // add us to the power management tree
    require(pm_vars != NULL, Fail);

    // register ourselves with ourself as policy-maker
    registerPowerDriver(this, gOurPowerStates, kNumIrDAStates);
    return true;
    
Fail:
    return false;
}

//
// request for our initial power state
//
unsigned long AppleUSBIrDADriver::initialPowerStateForDomainState ( IOPMPowerFlags flags)
{
    XTRACE(kLogInitialPowerState, flags >> 16, (short)flags);
    return fPowerState;
}

//
// request to turn device on or off.  since PM doesn't call us on our workloop, we'll
// get it going on another thead and wait for it.
//
IOReturn AppleUSBIrDADriver::setPowerState(unsigned long powerStateOrdinal, IOService * whatDevice)
{
    //UInt32 counter = 0;
    
    if (powerStateOrdinal == fPowerState) return kIOPMAckImplied;
    
    if (fPowerThreadCall) {
	bool ok;
	
	retain();				// paranoia is your friend, make sure we're not freed
	
	fWaitForGatedCmd = true;
	ok = thread_call_enter1(fPowerThreadCall, (void *)powerStateOrdinal);     // invoke handleSetPowerState
	
	while (fWaitForGatedCmd) {		// wait for it now
	    IOSleep(1);				// cycles to gated thread
	    //counter++;
	}
	
        if (ok) {                               // if thread was already pending ...
            release();                          // don't need/want the retain, so undo it
        }					// normally released below
    }
    //IOLog("irda setPowerState %d, waited %d ms\n", (int)powerStateOrdinal, (int)counter);
    return kIOPMAckImplied;
}

// handleSetPowerState()
//
// param0 - the object
// param1 - new power state ordinal
//static
void AppleUSBIrDADriver::handleSetPowerState(thread_call_param_t param0, thread_call_param_t param1 )
{
    AppleUSBIrDADriver *self = OSDynamicCast(AppleUSBIrDADriver, (const OSMetaClassBase *)param0);
    uintptr_t powerStateOrdinal = (uintptr_t)param1;
    
    if (self && self->fGate) {
	self->fGate->runAction(&(self->setPowerStateGated), (void *)powerStateOrdinal, (void *)0, (void *)0, (void *)0);
	self->release();		// offset the retain in setPowerState()
    }
}

// could cast Action directly to setPowerStateGatedPrivate, but not w/out a compiler warning
// static
IOReturn AppleUSBIrDADriver::setPowerStateGated(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    AppleUSBIrDADriver *self = OSDynamicCast(AppleUSBIrDADriver, (const OSMetaClassBase *)owner);
    uintptr_t powerStateOrdinal = (uintptr_t)arg0;
    
    if (self) return self->setPowerStateGatedPrivate(powerStateOrdinal);
    else return -1;
}

//
// setPowerStateGatedPrivate - do the work of setPowerState, now that we are on the workloop
//
IOReturn AppleUSBIrDADriver::setPowerStateGatedPrivate(uintptr_t powerStateOrdinal)
{
    fPowerState = powerStateOrdinal;
    CheckIrDAState();
    fWaitForGatedCmd = false;	    // release caller (power thread)
    return kIOReturnSuccess;
}


/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::interruptReadComplete
//
//      Inputs:     obj - me, param - parameter block(the Port), rc - return code, remaining - what's left
//                                                                                  (whose idea was that?)
//
//      Outputs:    None
//
//      Desc:       Interrupt pipe read completion routine
//
/****************************************************************************************************/

void AppleUSBIrDADriver::interruptReadComplete( void *obj, void *param, IOReturn rc, UInt32 remaining )
{
    AppleUSBIrDADriver  *me = (AppleUSBIrDADriver*)obj;
    //PortInfo_t            *port = (PortInfo_t*)param;
    IOReturn    ior;
    UInt32      dLen;

    XTRACE(kLogInterruptRead, me->fpinterruptPipeBuffer[0], rc);
    if (me->fIrDAOn) {                  // if we're still "up" and haven't been turned off
	check(INTERRUPT_BUFF_SIZE - remaining == 1);
	check(me->fpinterruptPipeBuffer[0] == 1);
    }
    
    if ( rc == kIOReturnSuccess )   /* If operation returned ok:    */
    {
	dLen = INTERRUPT_BUFF_SIZE - remaining;
	ELG( rc, dLen, 'iRC+', "interruptReadComplete" );
	XTRACE(kLogInterruptRead, 1, dLen);
	
	    /* Now look at the data */
//      LogData( kUSBAnyDirn, dLen, me->fpinterruptPipeBuffer );
	
	if (dLen != 1)
	{
	    XTRACE(kLogInterruptRead, 0xdead, 0xbeef);
	    ELG( 0, dLen, 'iRC-', "interruptReadComplete - what was that?" );
	} else {
	    
	    if (kUseInterruptsForRead) {            // We're using interrupts to trigger reads ...
		check(me->fReadActive == false);
		if (me->fReadActive == false) {
		    ior = me->fpInPipe->Read( me->fpPipeInMDP, &me->fReadCompletionInfo, NULL );    // start a read
		    if (ior != kIOReturnSuccess)
		    {
			ELG( 0, ior, 'icf-', "interrupt complete failed to start read");
		    } else {
			me->fReadActive = true;
		    }
		}
	    }
	}
	
	    /* Queue the next interrupt read:   */
    
	ior = me->fpInterruptPipe->Read( me->fpinterruptPipeMDP, &me->finterruptCompletionInfo, NULL );
	if ( ior == kIOReturnSuccess ) {
	    XTRACE(kLogInterruptRead, 0xffff, 0xffff);
	    return;
	}
    }

	/* Read returned with error OR next interrupt read failed to be queued: */

    XTRACE(kLogInterruptRead, 0xdead, 2);
    ELG( 0, rc, 'iRC-', "interruptReadComplete - error" );

    return;
    
}/* end interruptReadComplete */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::dataReadComplete
//
//      Inputs:     obj - me, param - parameter block(the Port), rc - return code, remaining - what's left
//
//      Outputs:    None
//
//      Desc:       BulkIn pipe (Data interface) read completion routine
//
/****************************************************************************************************/

void AppleUSBIrDADriver::dataReadComplete( void *obj, void *param, IOReturn rc, UInt32 remaining )
{
    AppleUSBIrDADriver  *me = (AppleUSBIrDADriver*)obj;
    PortInfo_t      *port = (PortInfo_t*)param;
    UInt16          dtlength;
    IOReturn        ior = kIOReturnSuccess;

    XTRACE(kLogDataReadComplete, rc, USBLapPayLoad - remaining);

#if (hasTracing > 0 && hasAppleUSBIrDATracing > 1)
    if (1) {
	int len = USBLapPayLoad - remaining;
	UInt32 w;
	UInt8 *b = me->fPipeInBuffer;
	int i;
	
	while (len > 0) {
	    w = 0;
	    for (i = 0 ; i < 4; i++) {
		w = w << 8;
		if (len > 0)            // don't run off end (pad w/zeros)
		    w = w | *b;
		b++;
		len--;
	    }
	    XTRACE(kLogInputData, w >> 16, (short)w);
	}
    }
#endif  // tracing high
    
    check(me->fReadActive == true);
    
    if ( rc == kIOReturnSuccess )   /* If operation returned ok:    */
    {
	me->fReadActive = false;
	dtlength = USBLapPayLoad - remaining;
	ELG( port->State, dtlength, 'dRC+', "dataReadComplete" );
	
//      LogData( kUSBIn, dtlength, me->fPipeInBuffer );
	
	if ( dtlength > 1 )
	{
	    if ( me->fIrDA )
	    {
		ior = me->fIrDA->ReadComplete( &me->fPipeInBuffer[1], dtlength-1 );
	    }
	    if ( ior != kIOReturnSuccess )
	    {
		ELG( 0, ior, 'IrR-', "dataReadComplete - IrDA ReadComplete problem" );
	    }
	}

	/* Queue the next read if not using interrupts */
	
	if (kUseInterruptsForRead == false) {

	    ior = me->fpInPipe->Read( me->fpPipeInMDP, &me->fReadCompletionInfo, NULL );
	    
	    if ( ior == kIOReturnSuccess )
	    {
		me->fReadActive = true;
		me->CheckQueues( port );
		return;
	    } else {
		ELG( 0, ior, 'DtQ-', "dataReadComplete - queueing bulk read failed" );
	    }
	}
    } else {
	
	/* Read returned with error */
	ELG( 0, rc, 'DtR-', "dataReadComplete - io err" );
    }

    return;
    
}/* end dataReadComplete */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::dataWriteComplete
//
//      Inputs:     obj - me, param - parameter block(the Port), rc - return code, remaining - what's left
//
//      Outputs:    None
//
//      Desc:       BulkOut pipe (Data interface) write completion routine
//
/****************************************************************************************************/

void AppleUSBIrDADriver::dataWriteComplete( void *obj, void *param, IOReturn rc, UInt32 remaining )
{
    AppleUSBIrDADriver  *me = (AppleUSBIrDADriver*)obj;
    Boolean done = true;                // write really finished?

    ELG( rc, (me->fCount - remaining), 'dWCt', "dataWriteComplete" );
    XTRACE(kLogDataWriteComplete, rc, me->fCount - remaining);
    
    check(me->fWriteActive);
    me->fWriteActive = false;
    
    // first check for speed change, it's the only time we do a single-byte write
    if (me->fCount == 1) {
	if (me->fIrDA)
	    me->fIrDA->SetSpeedComplete(rc == kIOReturnSuccess);
	return;
    }
    
    // in a transmit complete, but need to manually transmit a zero-length packet
    // if it's a multiple of the max usb packet size for the bulk-out pipe (64 bytes)
    if ( rc == kIOReturnSuccess )   /* If operation returned ok:    */
    {
	if ( me->fCount > 0 )                       // Check if it was not a zero length write
	{
	    if ( (me->fCount % 64) == 0 )               // If was a multiple of 64 bytes then we need to do a zero length write
	    {
		XTRACE(kLogDataWriteCompleteZero, 0, 0);
		LogData( kUSBOut, 0, me->fPipeOutBuffer );
		me->fWriteActive = true;
		me->fpPipeOutMDP->setLength( 0 );
		me->fCount = 0;
		me->fpOutPipe->Write( me->fpPipeOutMDP, &me->fWriteCompletionInfo );
		done = false;               // don't complete back to irda quite yet
	    }
	}
    }
    
    if (done && me->fIrDA )     // if time to let irda know the write has finished
    {
	me->fIrDA->Transmit_Complete( rc == kIOReturnSuccess ); // let IrDA know how write finished
    }
    return;
    
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::free
//
//      Inputs:     None
//
//      Outputs:    None
//
//      Desc:       Clean up and free the log 
//
/****************************************************************************************************/

void AppleUSBIrDADriver::free()
{
    XTRACE(kLogFree, 0, 0);
    ELG( 0, 0, 'free', "free" );
    
    if (fIrDA) {
	fIrDA->release();   // we don't do delete's in the kernal I suppose ...
	fIrDA=NULL;
    }
    
    if (fPowerThreadCall) {
	thread_call_free(fPowerThreadCall);
	fPowerThreadCall = NULL;
    }
    
    if (fGate) {
	IOWorkLoop *work =  getWorkLoop();
	if (work) work->removeEventSource(fGate);
	fGate->release();
	fGate = NULL;
    }
    
    
#if USE_ELG
    if ( g.evLogBuf )
	IOFree( g.evLogBuf, kEvLogSize );
#endif /* USE_ELG */

    super::free();
    
    XTRACE(kLogFree, 0xffff, 0xffff);
    return;
    
}/* end free */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::stop
//
//      Inputs:     provider - my provider
//
//      Outputs:    None
//
//      Desc:       Stops
//
/****************************************************************************************************/

void AppleUSBIrDADriver::stop( IOService *provider )
{
    XTRACE(kLogStop, 0, provider);
    ELG( 0, 0, 'stop', "stop" );
    
    fUSBStarted = false;        // reset usb start/stop flag for CheckIrDAState
    CheckIrDAState();           // turn irda off, release resources
    
    destroySerialStream();      // release the bsd tty
	
    destroyNub();               // delete the nubs and fPort
    
    if ( fpInterface )  
    {
	fpInterface->release();     // retain done in ConfigureDevice
	fpInterface = NULL; 
    }
    
    // release our power manager state
    PMstop();
    
    super::stop( provider );
    XTRACE(kLogStop, 0xffff, 0xffff);
    return;
    
}/* end stop */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::allocateResources
//
//      Inputs:     
//
//      Outputs:    return code - true (allocate was successful), false (it failed)
//
//      Desc:       Finishes up the rest of the configuration and gets all the endpoints open 
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::allocateResources( void )
{
    IOUSBFindEndpointRequest    epReq;      // endPoint request struct on stack
    bool                        goodCall;   // return flag fm Interface call

    ELG( 0, 0, 'Allo', "allocateResources." );
    XTRACE(kLogAllocateResources, 0, 0);

    // Open all the end points
    require(fpInterface, Fail);
	
    goodCall = fpInterface->open( this );       // close done in releaseResources
    if ( !goodCall )
    {
	ELG( 0, 0, 'epD-', "allocateResources - open data interface failed." );
	fpInterface->release();
	fpInterface = NULL;
	return false;
    }

    fpInterfaceNumber = fpInterface->GetInterfaceNumber();
    
    epReq.type          = kUSBBulk;
    epReq.direction     = kUSBIn;
    epReq.maxPacketSize = 0;
    epReq.interval      = 0;
    fpInPipe = fpInterface->FindNextPipe( 0, &epReq );
    require(fpInPipe, Fail);
    ELG( epReq.maxPacketSize << 16 |epReq.interval, fpInPipe, 'i P+', "allocateResources - bulk input pipe." );

    epReq.direction = kUSBOut;
    fpOutPipe = fpInterface->FindNextPipe( 0, &epReq );
    require(fpOutPipe, Fail);
    ELG( epReq.maxPacketSize << 16 |epReq.interval, fpOutPipe, 'o P+', "allocateResources - bulk output pipe." );

    epReq.type          = kUSBInterrupt;
    epReq.direction     = kUSBIn;
    fpInterruptPipe = fpInterface->FindNextPipe( 0, &epReq );
    require(fpInterruptPipe, Fail);
    ELG( epReq.maxPacketSize << 16 |epReq.interval, fpInterruptPipe, 'irP+', "allocateResources - interrupt pipe." );

    // Allocate Memory Descriptor Pointer with memory for the interrupt-in pipe:

    fpinterruptPipeMDP = IOBufferMemoryDescriptor::withCapacity( INTERRUPT_BUFF_SIZE, kIODirectionIn );
    require(fpinterruptPipeMDP, Fail);
    
    fpinterruptPipeMDP->setLength( INTERRUPT_BUFF_SIZE );
    fpinterruptPipeBuffer = (UInt8*)fpinterruptPipeMDP->getBytesNoCopy();
    ELG( 0, fpinterruptPipeBuffer, 'iBuf', "allocateResources - interrupt in buffer" );

    // Allocate Memory Descriptor Pointer with memory for the data-in bulk pipe:

    fpPipeInMDP = IOBufferMemoryDescriptor::withCapacity( USBLapPayLoad, kIODirectionIn );
    require(fpPipeInMDP, Fail);
    
    fpPipeInMDP->setLength( USBLapPayLoad );
    fPipeInBuffer = (UInt8*)fpPipeInMDP->getBytesNoCopy();
    ELG( 0, fPipeInBuffer, 'iBuf', "allocateResources - input buffer" );

    // Allocate Memory Descriptor Pointer with memory for the data-out bulk pipe:

    fpPipeOutMDP = IOBufferMemoryDescriptor::withCapacity( MAX_BLOCK_SIZE, kIODirectionOut );
    require(fpPipeOutMDP, Fail);
    
    fpPipeOutMDP->setLength( MAX_BLOCK_SIZE );
    fPipeOutBuffer = (UInt8*)fpPipeOutMDP->getBytesNoCopy();
    ELG( 0, fPipeOutBuffer, 'oBuf', "allocateResources - output buffer" );
    
    // set up the completion info for all three pipes
    
    require(fPort, Fail);
    finterruptCompletionInfo.target = this;
    finterruptCompletionInfo.action = interruptReadComplete;
    finterruptCompletionInfo.parameter  = fPort;
    
    fReadCompletionInfo.target  = this;
    fReadCompletionInfo.action  = dataReadComplete;
    fReadCompletionInfo.parameter   = fPort;

    fWriteCompletionInfo.target = this;
    fWriteCompletionInfo.action = dataWriteComplete;
    fWriteCompletionInfo.parameter  = fPort;
    

    ELG( 0, 0, 'aRs+', "allocateResources successful" );
    return true;

Fail:
    return false;
    
} // allocateResources


/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::releaseResources
//
//      Inputs:     port - the Port
//
//      Outputs:    None
//
//      Desc:       Frees up the pipe resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBIrDADriver::releaseResources( void )
{
    ELG( 0, 0, 'rlRs', "releaseResources" );
    XTRACE(kLogReleaseResources, 0, 0);
    
    if ( fpInterface )  
    { 
	fpInterface->close( this ); 
    }    
    
    if ( fpPipeOutMDP  )    
    { 
	fpPipeOutMDP->release();    
	fpPipeOutMDP    = 0; 
    }
    
    if ( fpPipeInMDP   )    
    { 
	fpPipeInMDP->release(); 
	fpPipeInMDP     = 0; 
    }
    
    if ( fpinterruptPipeMDP )   
    { 
	fpinterruptPipeMDP->release();  
	fpinterruptPipeMDP  = 0; 
    }

    return;
    
}/* end releaseResources */

//
// start reading on the pipes
//
bool AppleUSBIrDADriver::startPipes( void )
{
    IOReturn                    rtn;
    
    require(fPort, Fail);
    require(fpinterruptPipeMDP, Fail);
    require(fpPipeInMDP, Fail);
    require(fpPipeOutMDP, Fail);


    if (kUseInterruptsForRead) {                // read on interrupt pipe if using interrupts
	rtn = fpInterruptPipe->Read(fpinterruptPipeMDP, &finterruptCompletionInfo, NULL );
    }
    else {                                      // Read the data-in bulk pipe if not using interrupts
	rtn = fpInPipe->Read(fpPipeInMDP, 1000, 1000, &fReadCompletionInfo, NULL );
    }
    require(rtn == kIOReturnSuccess, Fail);
    
    // is this really referenced by anyone??
    fReadActive = (kUseInterruptsForRead == false);     // remember if we did a read

    return true;
    
Fail:
    return false;
}/* end startPipes */

//
// stop i/o on the pipes
//
void AppleUSBIrDADriver::stopPipes()
{
    if (fpInPipe)           fpInPipe->Abort();
    if (fpOutPipe)          fpOutPipe->Abort();
    if (fpInterruptPipe)    fpInterruptPipe->Abort();
}


/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::configureDevice
//
//      Inputs:     numConfigs - number of configurations present
//
//      Outputs:    return Code - true (device configured), false (device not configured)
//
//      Desc:       Finds the configurations and then the appropriate interfaces etc.
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::configureDevice( UInt8 numConfigs )
{
    IOUSBFindInterfaceRequest           req;            // device request Class on stack
    const IOUSBConfigurationDescriptor  *cd = NULL;     // configuration descriptor
    IOUSBInterfaceDescriptor            *intf = NULL;   // interface descriptor
    IOReturn                            ior;
    UInt8                               cval;
    UInt8                               config = 0;
    USBIrDAQoS                          *qos;
       
    ELG( 0, numConfigs, 'cDev', "configureDevice" );
    XTRACE(kLogConfigureDevice, 0, 0);
    
    for (cval=0; cval<numConfigs; cval++)
    {
	ELG( 0, cval, 'CkCn', "configureDevice - Checking Configuration" );
	
	cd = fpDevice->GetFullConfigurationDescriptor(cval);
	if ( !cd )
	{
	    ELG( 0, 0, 'GFC-', "configureDevice - Error getting the full configuration descriptor" );
	} else {
	
		// Find the first one - there may be more to go on in the future
		
	    req.bInterfaceClass = kIOUSBFindInterfaceDontCare;
	    req.bInterfaceSubClass  = kIOUSBFindInterfaceDontCare;
	    req.bInterfaceProtocol  = kIOUSBFindInterfaceDontCare;
	    req.bAlternateSetting   = kIOUSBFindInterfaceDontCare;
	    ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
	    if ( ior == kIOReturnSuccess )
	    {
		if ( intf )
		{
		    config = cd->bConfigurationValue;
		    ELG( cd, config, 'FNI+', "configureDevice - Interface descriptor found" );
		    break;
		} else {
		    ELG( 0, config, 'FNI-', "configureDevice - That's weird the interface was null" );
		    cd = NULL;
		}
	    } else {
		ELG( 0, cval, 'FNID', "configureDevice - No CDC interface found this configuration" );
		cd = NULL;
	    }
	}
    }
	    
    if ( !cd )
    {
	return false;
    }
    
	// Now lets do it for real
	
    req.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceSubClass  = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol  = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting   = kIOUSBFindInterfaceDontCare;
    
    fpInterface = fpDevice->FindNextInterface( NULL, &req );
    if ( !fpInterface )
    {
	ELG( 0, 0, 'FIC-', "configureDevice - Find next interface failed" );
	return false;
    }
    fpInterface->retain();      // release done in stop()
    
	// Get the QoS Functional Descriptor (it's the only one)
    
    qos = (USBIrDAQoS *)fpInterface->FindNextAssociatedDescriptor(NULL, USBIrDAClassDescriptor);
    if (!qos)
    {
	ELG( 0, 0, 'OSF-', "configureDevice - No QOS descriptor" );
	
	/* Set some defaults - qos values need tuning of course */
	
	fQoS.bFunctionLength = 12;
	fQoS.bDescriptorType = USBIrDAClassDescriptor;
	fQoS.version = 0x100;
	fQoS.datasize = 0x1f;       // 1k = 1f, 2k = 3f
	fQoS.windowsize = 1;
	fQoS.minturn = 2;           // review & tune.
	fQoS.baud1 = 0x01;          // 4mbit no mir, all sir
	fQoS.baud2 = 0x3e;
	fQoS.bofs = 4;              // review and tune.
	fQoS.sniff = 0;
	fQoS.unicast = 0;
    } else {
	ELG( qos->bDescriptorType, qos, 'QSFD', "AppleUSBCDCDriver::configureDevice - Got QoS Functional Descriptor" );
	
	/* Save the real values */
	
	fQoS.bFunctionLength = qos->bFunctionLength;
	fQoS.bDescriptorType = qos->bDescriptorType;
	fQoS.version = USBToHostWord(qos->version);
	fQoS.datasize = qos->datasize;
	fQoS.windowsize = qos->windowsize;
	fQoS.minturn = qos->minturn;
	fQoS.baud1 = qos->baud2;        // flipped because of our good friends at you know who
	fQoS.baud2 = qos->baud1;
	fQoS.bofs = qos->bofs;
	fQoS.sniff = qos->sniff;
	fQoS.unicast = qos->unicast;
    }
    
	
    // irda starts up turned off so always try and suspend the hardware
    ior = fpDevice->SuspendDevice( true );         // Suspend the device
    require(ior == kIOReturnSuccess, Fail);
	
    return true;
    
Fail:
    return false;

}/* end configureDevice */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::createSuffix
//
//      Inputs:     None
//
//      Outputs:    return Code - true (suffix created), false (suffix not create), sufKey - the key
//
//      Desc:       Creates the suffix key. It attempts to use the serial number string from the device
//                  if it's reasonable i.e. less than 8 bytes ascii. Remember it's stored in unicode 
//                  format. If it's not present or not reasonable it will generate the suffix based 
//                  on the location property tag. At least this remains the same across boots if the
//                  device is plugged into the same physical location. In the latter case trailing
//                  zeros are removed.
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::createSuffix( unsigned char *sufKey, int sufMaxLen )
{
    
    IOReturn                rc;
    UInt8                   serBuf[10];     // arbitrary size > 8
    OSNumber                *location;
    UInt32                  locVal;
    UInt8                   *rlocVal;
    UInt16                  offs, i, sig = 0;
    UInt8                   indx;
    bool                    keyOK = false;      
    
    ELG( 0, 0, 'cSuf', "createSuffix" );
    
    indx = fpDevice->GetSerialNumberStringIndex();  
    if (indx != 0 )
    {   
	// Generate suffix key based on the serial number string (if reasonable <= 8 and > 0)   

	rc = fpDevice->GetStringDescriptor(indx, (char *)&serBuf, sizeof(serBuf));
	if ( !rc )
	{
	    if ( (strlen((char *)&serBuf) < 9) && (strlen((char *)&serBuf) > 0) )
	    {
		strlcpy( (char *)sufKey, (const char *)&serBuf, sufMaxLen);
		keyOK = true;
	    }           
	} else {
	    ELG( 0, rc, 'Sdt-', "createSuffix error reading serial number string" );
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
//      Method:     AppleUSBIrDADriver::createSerialStream
//
//      Inputs:     None
//
//      Outputs:    return Code - true (created and initialilzed ok), false (it failed)
//
//      Desc:       Creates and initializes the nub and port structure
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::createSerialStream()
{
    UInt8           indx;
    IOReturn            rc;
    unsigned char suffix[10];
    
    ELG( 0, fNub, '=Nub', "createSerialStream" );
    XTRACE(kLogCreateSerialStream, 0, 0);
    
    check(fNub && fPort);
    if (!fNub || !fPort) return false;

    SetStructureDefaults( fPort, true );            // init the Port structure
    
    // Allocate the request lock
    fPort->serialRequestLock = IOLockAlloc();   // init lock used to protect code on MP
    if ( !fPort->serialRequestLock ) {
	return false;
    }
    
    // now the ring buffers
    if (!allocateRingBuffer(&(fPort->TX), fPort->TXStats.BufferSize) ||
	!allocateRingBuffer(&(fPort->RX), fPort->RXStats.BufferSize)) 
    {
	return false;
    }

	
    if ( !fTerminate )
    {
	// Report the base name to be used for generating device nodes
    
	fNub->setProperty( kIOTTYBaseNameKey, baseName );
    
	// Create suffix key and set it
    
	if ( createSuffix(suffix, sizeof(suffix) ) )
	{       
	    fNub->setProperty( kIOTTYSuffixKey, (const char *)suffix );
	}

	
	// Save the Product String  (at least the first productNameLength's worth).
	
	indx = fpDevice->GetProductStringIndex();   
	if ( indx != 0 )
	{   
	    rc = fpDevice->GetStringDescriptor( indx, (char *)&fProductName, sizeof(fProductName) );
	    if ( !rc )
	    {
		if ( strlen((char *)fProductName) == 0 )        // believe it or not this sometimes happens (null string with an index defined???)
		{
		    strlcpy( (char *)fProductName, defaultName, sizeof(fProductName));
		}
		fNub->setProperty( (const char *)propertyTag, (const char *)fProductName );
	    }
	}
	    
	fNub->registerService();
    }
    
    return true;
    
}/* end createSerialStream */

//
// release things created in createSerialStream
//
void
AppleUSBIrDADriver::destroySerialStream(void)
{
    require(fPort, Fail);
    
    if ( fPort->serialRequestLock )
    {
	IOLockFree( fPort->serialRequestLock ); // free the Serial Request Lock
	fPort->serialRequestLock = NULL;
    }

    // Remove all the buffers.

    freeRingBuffer( &fPort->TX );
    freeRingBuffer( &fPort->RX );

    removeProperty( (const char *)propertyTag );    // unhook from BSD
    
Fail:
    return;
}

//
// startIrDA
//
// assumes createSerialStream is called once at usb start time
// calls allocateResources to open endpoints
//
bool
AppleUSBIrDADriver::startIrDA()
{
    bool ok;
    
    require(fIrDA == NULL, Fail);
    require(fNub, Fail);
    require(fUserClientNub, Fail);
    
    Workaround();                           // make chip as sane as can be
    
    ok = allocateResources();               // open the pipe endpoints
    require(ok, Fail);
    
    startPipes();                           // start reading on the usb pipes

    fBaudCode = kLinkSpeed9600;             // the code for 9600 (see BaudRate above)
    fLastChangeByte = 0;                    // no known state of device, force mode to change on first i/o
    fCurrentBaud = 9600;
    SetBofCount(10);                        // start with about 10 bofs (sets fBofsCode)

    fIrDA = IrDAComm::irDAComm(fNub, fUserClientNub);       // create and init and start IrDA
    require(fIrDA, Fail);
	
    return true;

Fail:
    return false;
}

void
AppleUSBIrDADriver::stopIrDA()
{
    require(fIrDA, Fail);
    
    fIrDA->Stop();
    fIrDA->release();
    fIrDA = NULL;

    stopPipes();                            // stop reading on the usb pipes
    
    if (fpPipeOutMDP != NULL)               // better test for releaseResources?
	releaseResources( );
    
Fail:
    return;
}

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::createNub
//
//      Inputs:
//
//      Outputs:    fNub (an AppleUSBIrDA glue object) and fPort
//
//      Desc:       allocates and inits, but doesn't publish the BSD info on the nub yet
//              create serial stream finishes the job later.
//
/****************************************************************************************************/
bool
AppleUSBIrDADriver::createNub(void)
{
    bool ret;
    
    if (fNub == NULL) {
	fNub = new AppleUSBIrDA;
    }
    require(fNub, Failed);
    check(fNub->getRetainCount() == 1);     // testing
    
    if (fPort == NULL) {
	fPort = (PortInfo_t*)IOMalloc( sizeof(PortInfo_t) );
    }
    require(fPort, Failed);
    bzero(fPort, sizeof(PortInfo_t));
				    
    ret = fNub->init(0, fPort);
    require(ret == true, Failed);
    
    ret = fNub->attach( this );
    require(ret == true, Failed);
    check(fNub->getRetainCount() == 2);     // testing
    
    XTRACE(kLogNewNub, 0, fNub);
    XTRACE(kLogNewPort, 0, fPort);
    
    // now make the nub to act as a communication point for user-client
    if (fUserClientNub == NULL)
	fUserClientNub = AppleIrDA::withNub(fNub);      // it talks to the serial nub ...
    require(fUserClientNub, Failed);
    
    fUserClientNub->attach(this);
    
    return true;
	
Failed:
    IOLog("Create nub failed\n");
    // could try and clean up here, but let's start by just not crashing.
    return false;
}

void AppleUSBIrDADriver::destroyNub()
{
    if (fPort != NULL) {
	IOFree( fPort, sizeof(PortInfo_t) );
	fPort = NULL;
    }
    
    if (fUserClientNub) {
	XTRACE(kLogDestroyNub, 1, fUserClientNub->getRetainCount());
	fUserClientNub->detach(this);
	fUserClientNub->release();
	fUserClientNub = NULL;
    }
    
    if (fNub) {
	XTRACE(kLogDestroyNub, 2, fNub->getRetainCount());
	fNub->detach(this);
	fNub->release();    // crash boom?
	fNub = NULL;
    }
}

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::acquirePort
//
//      Inputs:     sleep - true (wait for it), false (don't), refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess, kIOReturnExclusiveAccess, kIOReturnIOError and various others
//
//      Desc:       acquirePort tests and sets the state of the port object.  If the port was
//                  available, then the state is set to busy, and kIOReturnSuccess is returned.
//                  If the port was already busy and sleep is YES, then the thread will sleep
//                  until the port is freed, then re-attempts the acquire.  If the port was
//                  already busy and sleep is NO, then kIOReturnExclusiveAccess is returned.
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::acquirePort( bool sleep, void *refCon )
{
    PortInfo_t          *port = (PortInfo_t *) refCon;
    UInt32              busyState = 0;
    IOReturn            rtn = kIOReturnSuccess;

    ELG( port, sleep, 'acqP', "acquirePort" );
    XTRACE(kLogAcquirePort, 0, 0);
    
    if ( fTerminate ) {
	//int review_fTerminate;
	//return kIOReturnOffline;
    }
    SetStructureDefaults( port, FALSE );    /* Initialize all the structures */
    
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
		ELG( 0, 0, 'busy', "acquirePort - Busy exclusive access" );
		return kIOReturnExclusiveAccess;
	    } else {
		busyState = 0;
		rtn = watchState( &busyState, PD_S_ACQUIRED, refCon );
		if ( (rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess) )
		{
		    continue;
		} else {
		    ELG( 0, 0, 'int-', "acquirePort - Interrupted!" );
		    return rtn;
		}
	    }
	}
    } /* end for */
    
    fSessions++;    //bump number of active sessions and turn on clear to send
    changeState( port, PD_RS232_S_CTS, PD_RS232_S_CTS);

    CheckIrDAState();       // turn irda on/off if appropriate
    
    if (1) {                // wait for initial connect
	int counter = 0;
	//while (fIrDA && fIrDA->Starting()) {
	while (counter++ < (10 * 10)) {     // sanity check limit of 10 seconds
	    if (fIrDA && (fIrDA->Starting() == false)) break;
	    XTRACE(kLogAcquirePort, counter, 1);
	    IOSleep(100);                   // wait 1/10 of a second between polls and yield time
	}
	//IOLog("AppleUSBIrDA: acquire port paused %d ms for initial connection\n", counter*100);
    }
    
    return rtn;
    
}/* end acquirePort */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::releasePort
//
//      Inputs:     refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//      Desc:       releasePort returns all the resources and does clean up.
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::releasePort( void *refCon )
{
    PortInfo_t          *port = (PortInfo_t *) refCon;
    UInt32              busyState;

    ELG( 0, port, 'relP', "releasePort" );
    XTRACE(kLogReleasePort, 0, 0);
    
    busyState = (readPortState( port ) & PD_S_ACQUIRED);
    if ( !busyState )
    {
	ELG( 0, 0, 'rlP-', "releasePort - NOT OPEN" );
	return kIOReturnNotOpen;
    }
    
    changeState( port, 0, (UInt32)STATE_ALL );  // Clear the entire state word which also deactivates the port

    fSessions--;        // reduce number of active sessions
    CheckIrDAState();   // turn irda off if appropriate

    if ((fTerminate) && (fSessions == 0))       // if it's the result of a terminate and session count is zero we also need to close things
    {
	if (0 && fpInterface )      // jdg - this was bogus
	{
	    fpInterface->close( this ); 
	    fpInterface->release();
	    fpInterface = NULL; 
	}
       // else IOLog("appleusbirda - would have released fpInteface here\n");
    }
    
    ELG( 0, 0, 'RlP+', "releasePort - OK" );
    
    return kIOReturnSuccess;
    
}/* end releasePort */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::setState
//
//      Inputs:     state - state to set, mask - state mask, refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess or kIOReturnBadArgument
//
//      Desc:       Set the state for the port device.  The lower 16 bits are used to set the
//                  state of various flow control bits (this can also be done by enqueueing a
//                  PD_E_FLOW_CONTROL event).  If any of the flow control bits have been set
//                  for automatic control, then they can't be changed by setState.  For flow
//                  control bits set to manual (that are implemented in hardware), the lines
//                  will be changed before this method returns.  The one weird case is if RXO
//                  is set for manual, then an XON or XOFF character may be placed at the end
//                  of the TXQ and transmitted later.
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::setState( UInt32 state, UInt32 mask, void *refCon )
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    
    ELG( state, mask, 'stSt', "setState" );
    XTRACE(kLogSetState, 0, 0);
    
    if ( mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK)) )
	return kIOReturnBadArgument;

    if ( readPortState( port ) & PD_S_ACQUIRED )
    {
	    // ignore any bits that are read-only
	mask &= (~port->FlowControl & PD_RS232_A_MASK) | PD_S_MASK;

	if ( mask)
	    changeState( port, state, mask );

	return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
    
}/* end setState */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::getState
//
//      Inputs:     refCon - the Port
//
//      Outputs:    state - port state
//
//      Desc:       Get the state for the port.
//
/****************************************************************************************************/

UInt32 AppleUSBIrDADriver::getState( void *refCon )
{
    PortInfo_t  *port = (PortInfo_t *) refCon;
    UInt32      state;
    
    ELG( 0, port, 'gtSt', "getState" );
    
    CheckQueues( port );
	
    state = readPortState( port ) & EXTERNAL_MASK;
    
    ELG( state, EXTERNAL_MASK, 'gtS-', "getState-->State" );
	XTRACE(kLogGetState, state >> 16, (short)state);
    
    return state;
    
}/* end getState */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::watchState
//
//      Inputs:     state - state to watch for, mask - state mask bits, refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess or value returned from ::watchState
//
//      Desc:       Wait for the at least one of the state bits defined in mask to be equal
//                  to the value defined in state. Check on entry then sleep until necessary,
//                  see watchState for more details.
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::watchState( UInt32 *state, UInt32 mask, void *refCon)
{
    PortInfo_t  *port = (PortInfo_t *) refCon;
    IOReturn    ret = kIOReturnNotOpen;

    ELG( *state, mask, 'WatS', "watchState" );
    XTRACE(kLogWatchState, 0, 0);

    if ( readPortState( port ) & PD_S_ACQUIRED )
    {
	ret = kIOReturnSuccess;
	mask &= EXTERNAL_MASK;
	ret = privateWatchState( port, state, mask );
	*state &= EXTERNAL_MASK;
    }
    
    ELG( ret, 0, 'WatS', "watchState --> watchState" );
    XTRACE(kLogWatchState, 0xffff, 0xffff);
    return ret;
    
}/* end watchState */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::nextEvent
//
//      Inputs:     refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess
//
//      Desc:       Not used by this driver.
//
/****************************************************************************************************/

UInt32 AppleUSBIrDADriver::nextEvent( void *refCon )
{
    UInt32      ret = kIOReturnSuccess;

    ELG( 0, 0, 'NxtE', "nextEvent" );

    return ret;
    
}/* end nextEvent */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::executeEvent
//
//      Inputs:     event - The event, data - any data associated with the event, refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess, kIOReturnNotOpen or kIOReturnBadArgument
//
//      Desc:       executeEvent causes the specified event to be processed immediately.
//                  This is primarily used for channel control commands like START & STOP
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::executeEvent( UInt32 event, UInt32 data, void *refCon )
{
    PortInfo_t  *port = (PortInfo_t *) refCon;
    IOReturn    ret = kIOReturnSuccess;
    UInt32      state, delta;
    
    delta = 0;
    state = readPortState( port );  
    ELG( port, state, 'ExIm', "executeEvent" );
    XTRACE(kLogExecEvent, event >> 16, (short)event);
    XTRACE(kLogExecEventData, data >> 16, (short)data);
    
    if ( (state & PD_S_ACQUIRED) == 0 )
	return kIOReturnNotOpen;

    switch ( event )
    {
    case PD_RS232_E_XON_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_RS232_E_XON_BYTE" );
	port->XONchar = data;
	break;
    case PD_RS232_E_XOFF_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_RS232_E_XOFF_BYTE" );
	port->XOFFchar = data;
	break;
    case PD_E_SPECIAL_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_SPECIAL_BYTE" );
	port->SWspecial[ data >> SPECIAL_SHIFT ] |= (1 << (data & SPECIAL_MASK));
	break;

    case PD_E_VALID_DATA_BYTE:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_VALID_DATA_BYTE" );
	port->SWspecial[ data >> SPECIAL_SHIFT ] &= ~(1 << (data & SPECIAL_MASK));
	break;

    case PD_E_FLOW_CONTROL:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_FLOW_CONTROL" );
	break;

    case PD_E_ACTIVE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_ACTIVE" );
	if ( (bool)data )
	{
	    if ( !(state & PD_S_ACTIVE) )
	    {
		SetStructureDefaults( port, FALSE );
		changeState( port, (UInt32)PD_S_ACTIVE, (UInt32)PD_S_ACTIVE ); // activate port
	    }
	} else {
	    if ( (state & PD_S_ACTIVE) )
	    {
		changeState( port, 0, (UInt32)PD_S_ACTIVE );
	    }
	}
	break;

    case PD_E_DATA_LATENCY:
	ELG( data, event, 'ExIm', "executeEvent - PD_E_DATA_LATENCY" );
	port->DataLatInterval = long2tval( data * 1000 );
	break;

    case PD_RS232_E_MIN_LATENCY:
	ELG( data, event, 'ExIm', "executeEvent - PD_RS232_E_MIN_LATENCY" );
	port->MinLatency = bool( data );
	break;

    case PD_E_DATA_INTEGRITY:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DATA_INTEGRITY" );
	if ( (data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE))
	{
	    ret = kIOReturnBadArgument;
	}
	else
	{
	    port->TX_Parity = data;
	    port->RX_Parity = PD_RS232_PARITY_DEFAULT;          
	}
	break;

    case PD_E_DATA_RATE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DATA_RATE" );
	    /* For API compatiblilty with Intel.    */
	data >>= 1;
	ELG( 0, data, 'Exlm', "executeEvent - actual data rate" );
	if ( (data < kMinBaudRate) || (data > kMaxBaudRate) )       // Do we really care
	    ret = kIOReturnBadArgument;
	else
	{
	    port->BaudRate = data;
	}       
	break;

    case PD_E_DATA_SIZE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DATA_SIZE" );
	    /* For API compatiblilty with Intel.    */
	data >>= 1;
	ELG( 0, data, 'Exlm', "executeEvent - actual data size" );
	if ( (data < 5) || (data > 8) )
	    ret = kIOReturnBadArgument;
	else
	{
	    port->CharLength = data;            
	}
	break;

    case PD_RS232_E_STOP_BITS:
	ELG( data, event, 'Exlm', "executeEvent - PD_RS232_E_STOP_BITS" );
	if ( (data < 0) || (data > 20) )
	    ret = kIOReturnBadArgument;
	else
	{
	    port->StopBits = data;
	}
	break;

    case PD_E_RXQ_FLUSH:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RXQ_FLUSH" );
	break;

    case PD_E_RX_DATA_INTEGRITY:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RX_DATA_INTEGRITY" );
	if ( (data != PD_RS232_PARITY_DEFAULT) &&  (data != PD_RS232_PARITY_ANY) )
	    ret = kIOReturnBadArgument;
	else
	    port->RX_Parity = data;
	break;

    case PD_E_RX_DATA_RATE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RX_DATA_RATE" );
	if ( data )
	    ret = kIOReturnBadArgument;
	break;

    case PD_E_RX_DATA_SIZE:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RX_DATA_SIZE" );
	if ( data )
	    ret = kIOReturnBadArgument;
	break;

    case PD_RS232_E_RX_STOP_BITS:
	ELG( data, event, 'Exlm', "executeEvent - PD_RS232_E_RX_STOP_BITS" );
	if ( data )
	    ret = kIOReturnBadArgument;
	break;

    case PD_E_TXQ_FLUSH:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_TXQ_FLUSH" );
	break;

    case PD_RS232_E_LINE_BREAK:
	ELG( data, event, 'Exlm', "executeEvent - PD_RS232_E_LINE_BREAK" );
	state &= ~PD_RS232_S_BRK;
	delta |= PD_RS232_S_BRK;
	break;

    case PD_E_DELAY:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_DELAY" );
	port->CharLatInterval = long2tval(data * 1000);
	break;
	
    case PD_E_RXQ_SIZE:
	ELG( 0, event, 'Exlm', "executeEvent - PD_E_RXQ_SIZE" );
	break;

    case PD_E_TXQ_SIZE:
	ELG( 0, event, 'Exlm', "executeEvent - PD_E_TXQ_SIZE" );
	break;

    case PD_E_RXQ_HIGH_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RXQ_HIGH_WATER" );
	break;

    case PD_E_RXQ_LOW_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_RXQ_LOW_WATER" );
	break;

    case PD_E_TXQ_HIGH_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_TXQ_HIGH_WATER" );
	break;

    case PD_E_TXQ_LOW_WATER:
	ELG( data, event, 'Exlm', "executeEvent - PD_E_TXQ_LOW_WATER" );
	break;

    default:
	ELG( data, event, 'Exlm', "executeEvent - unrecognized event" );
	ret = kIOReturnBadArgument;
	break;
    }

    state |= state;/* ejk for compiler warnings. ?? */
    changeState( port, state, delta );
    
    return ret;
    
}/* end executeEvent */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::requestEvent
//
//      Inputs:     event - The event, refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess, kIOReturnBadArgument, data - any data associated with the event
//
//      Desc:       requestEvent processes the specified event as an immediate request and
//                  returns the results in data.  This is primarily used for getting link
//                  status information and verifying baud rate and such.
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::requestEvent( UInt32 event, UInt32 *data, void *refCon )
{
    PortInfo_t  *port = (PortInfo_t *) refCon;
    IOReturn    returnValue = kIOReturnSuccess;

    ELG( 0, readPortState( port ), 'ReqE', "requestEvent" );
    XTRACE(kLogReqEvent, event >> 16, (short)event);

    if ( data == NULL ) {
	ELG( 0, event, 'ReqE', "requestEvent - data is null" );
	returnValue = kIOReturnBadArgument;
    }
    else
    {
	XTRACE(kLogReqEventData, (*data) >> 16, (short)*data);
	switch ( event )
	{
	    case PD_E_ACTIVE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_ACTIVE" );
		*data = bool(readPortState( port ) & PD_S_ACTIVE);  
		break;
	    
	    case PD_E_FLOW_CONTROL:
		ELG( port->FlowControl, event, 'ReqE', "requestEvent - PD_E_FLOW_CONTROL" );
		*data = port->FlowControl;                          
		break;
	    
	    case PD_E_DELAY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DELAY" );
		*data = tval2long( port->CharLatInterval )/ 1000;   
		break;
	    
	    case PD_E_DATA_LATENCY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_LATENCY" );
		*data = tval2long( port->DataLatInterval )/ 1000;   
		break;

	    case PD_E_TXQ_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_SIZE" );
		*data = GetQueueSize( &port->TX );  
		break;
	    
	    case PD_E_RXQ_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_SIZE" );
		*data = GetQueueSize( &port->RX );  
		break;

	    case PD_E_TXQ_LOW_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_LOW_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_RXQ_LOW_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_LOW_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_TXQ_HIGH_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_HIGH_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_RXQ_HIGH_WATER:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_HIGH_WATER" );
		*data = 0; 
		returnValue = kIOReturnBadArgument; 
		break;
	
	    case PD_E_TXQ_AVAILABLE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_TXQ_AVAILABLE" );
		*data = FreeSpaceinQueue( &port->TX );   
		break;
	    
	    case PD_E_RXQ_AVAILABLE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RXQ_AVAILABLE" );
		*data = UsedSpaceinQueue( &port->RX );  
		break;

	    case PD_E_DATA_RATE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_RATE" );
		*data = port->BaudRate << 1;        
		break;
	    
	    case PD_E_RX_DATA_RATE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RX_DATA_RATE" );
		*data = 0x00;                   
		break;
	    
	    case PD_E_DATA_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_SIZE" );
		*data = port->CharLength << 1;  
		break;
	    
	    case PD_E_RX_DATA_SIZE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RX_DATA_SIZE" );
		*data = 0x00;                   
		break;
	    
	    case PD_E_DATA_INTEGRITY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_DATA_INTEGRITY" );
		*data = port->TX_Parity;            
		break;
	    
	    case PD_E_RX_DATA_INTEGRITY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_E_RX_DATA_INTEGRITY" );
		*data = port->RX_Parity;            
		break;

	    case PD_RS232_E_STOP_BITS:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_STOP_BITS" );
		*data = port->StopBits << 1;        
		break;
	    
	    case PD_RS232_E_RX_STOP_BITS:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_RX_STOP_BITS" );
		*data = 0x00;                   
		break;
	    
	    case PD_RS232_E_XON_BYTE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_XON_BYTE" );
		*data = port->XONchar;          
		break;
	    
	    case PD_RS232_E_XOFF_BYTE:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_XOFF_BYTE" );
		*data = port->XOFFchar;         
		break;
	    
	    case PD_RS232_E_LINE_BREAK:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_LINE_BREAK" );
		*data = bool(readPortState( port ) & PD_RS232_S_BRK);
		break;
	    
	    case PD_RS232_E_MIN_LATENCY:
		ELG( 0, event, 'ReqE', "requestEvent - PD_RS232_E_MIN_LATENCY" );
		*data = bool( port->MinLatency );       
		break;

	    default:
		ELG( 0, event, 'ReqE', "requestEvent - unrecognized event" );
		returnValue = kIOReturnBadArgument;             
		break;
	}
    }

    return kIOReturnSuccess;
    
}/* end requestEvent */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::enqueueEvent
//
//      Inputs:     event - The event, data - any data associated with the event, 
//                                              sleep - true (wait for it), false (don't), refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess, kIOReturnNotOpen
//
//      Desc:       Not used by this driver.    
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::enqueueEvent( UInt32 event, UInt32 data, bool sleep, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    
    ELG( data, event, 'EnqE', "enqueueEvent" );

    if ( readPortState( port ) & PD_S_ACTIVE )
    {
	return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
    
}/* end enqueueEvent */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::dequeueEvent
//
//      Inputs:     sleep - true (wait for it), false (don't), refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess, kIOReturnNotOpen
//
//      Desc:       Not used by this driver.        
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::dequeueEvent( UInt32 *event, UInt32 *data, bool sleep, void *refCon )
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    
    ELG( 0, 0, 'DeqE', "dequeueEvent" );

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
//      Method:     AppleUSBIrDADriver::enqueueData
//
//      Inputs:     buffer - the data, size - number of bytes, sleep - true (wait for it), false (don't),
//                                                                                      refCon - the Port
//
//      Outputs:    Return Code - kIOReturnSuccess or value returned from watchState, count - bytes transferred,  
//
//      Desc:       enqueueData will attempt to copy data from the specified buffer to
//                  the TX queue as a sequence of VALID_DATA events.  The argument
//                  bufferSize specifies the number of bytes to be sent.  The actual
//                  number of bytes transferred is returned in count.
//                  If sleep is true, then this method will sleep until all bytes can be
//                  transferred.  If sleep is false, then as many bytes as possible
//                  will be copied to the TX queue.
//                  Note that the caller should ALWAYS check the transferCount unless
//                  the return value was kIOReturnBadArgument, indicating one or more
//                  arguments were not valid.  Other possible return values are
//                  kIOReturnSuccess if all requirements were met.      
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::enqueueData( UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon )
{
    PortInfo_t  *port = (PortInfo_t *) refCon;
    UInt32      state = PD_S_TXQ_LOW_WATER;
    IOReturn    rtn = kIOReturnSuccess;

    ELG( 0, sleep, 'eqDt', "enqueData" );

    if ( fTerminate )
	return kIOReturnOffline;

    if ( count == NULL || buffer == NULL )
	return kIOReturnBadArgument;

    *count = 0;

    if ( !(readPortState( port ) & PD_S_ACTIVE) )
	return kIOReturnNotOpen;
    
    ELG( port->State, size, 'eqDt', "enqueData State" );    
    LogData( kUSBOut, size, buffer );

	/* OK, go ahead and try to add something to the buffer  */
    *count = AddtoQueue( &port->TX, buffer, size );
    CheckQueues( port );

	/* Let the tranmitter know that we have something ready to go   */
    SetUpTransmit( );

	/* If we could not queue up all of the data on the first pass and   */
	/* the user wants us to sleep until it's all out then sleep */

    while ( (*count < size) && sleep )
    {
	state = PD_S_TXQ_LOW_WATER;
	rtn = watchState( &state, PD_S_TXQ_LOW_WATER, refCon );
	if ( rtn != kIOReturnSuccess )
	{
	    ELG( 0, rtn, 'EqD-', "enqueueData - interrupted" );
	    return rtn;
	}

	*count += AddtoQueue( &port->TX, buffer + *count, size - *count );
	CheckQueues( port );

	/* Let the tranmitter know that we have something ready to go.  */

	SetUpTransmit( );
    }/* end while */

    ELG( *count, size, 'enqd', "enqueueData - Enqueue" );

    return kIOReturnSuccess;
    
}/* end enqueueData */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::dequeueData
//
//      Inputs:     size - buffer size, min - minimum bytes required, refCon - the Port
//
//      Outputs:    buffer - data returned, min - number of bytes
//                  Return Code - kIOReturnSuccess, kIOReturnBadArgument, kIOReturnNotOpen, or value returned from watchState
//
//      Desc:       dequeueData will attempt to copy data from the RX queue to the
//                  specified buffer.  No more than bufferSize VALID_DATA events
//                  will be transferred. In other words, copying will continue until
//                  either a non-data event is encountered or the transfer buffer
//                  is full.  The actual number of bytes transferred is returned
//                  in count.
//                  The sleep semantics of this method are slightly more complicated
//                  than other methods in this API. Basically, this method will
//                  continue to sleep until either min characters have been
//                  received or a non data event is next in the RX queue.  If
//                  min is zero, then this method never sleeps and will return
//                  immediately if the queue is empty.
//                  Note that the caller should ALWAYS check the transferCount
//                  unless the return value was kIOReturnBadArgument, indicating one or
//                  more arguments were not valid.
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::dequeueData( UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon )
{
    PortInfo_t  *port = (PortInfo_t *) refCon;
    IOReturn    rtn = kIOReturnSuccess;
    UInt32      state = 0;

    ELG( size, min, 'dqDt', "dequeueData" );
    
	/* Check to make sure we have good arguments.   */
    if ( (count == NULL) || (buffer == NULL) || (min > size) )
	return kIOReturnBadArgument;

	/* If the port is not active then there should not be any chars.    */
    *count = 0;
    if ( !(readPortState( port ) & PD_S_ACTIVE) )
	return kIOReturnNotOpen;

	/* Get any data living in the queue.    */
    *count = RemovefromQueue( &port->RX, buffer, size );
    if (fIrDA)
	fIrDA->ReturnCredit( *count );      // return credit when room in the queue
    
    CheckQueues( port );

    while ( (min > 0) && (*count < min) )
    {
	int count_read;
	
	    /* Figure out how many bytes we have left to queue up */
	state = 0;

	rtn = watchState( &state, PD_S_RXQ_EMPTY, refCon );

	if ( rtn != kIOReturnSuccess )
	{
	    ELG( 0, rtn, 'DqD-', "dequeueData - Interrupted!" );
	    LogData( kUSBIn, *count, buffer );
	    return rtn;
	}
	/* Try and get more data starting from where we left off */
	count_read = RemovefromQueue( &port->RX, buffer + *count, (size - *count) );
	if (fIrDA)
	    fIrDA->ReturnCredit(count_read);        // return credit when room in the queue
	    
	*count += count_read;
	CheckQueues( port );
	
    }/* end while */

    LogData( kUSBIn, *count, buffer );

    ELG( *count, size, 'deqd', "dequeueData -->Out Dequeue" );

    return rtn;
    
}/* end dequeueData */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::SetUpTransmit
//
//      Inputs:
//
//      Outputs:    return code - true (transmit started), false (transmission already in progress)
//
//      Desc:       Setup and then start transmisson on the channel specified
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::SetUpTransmit( void )
{

    size_t      count = 0;
    size_t      data_Length, tCount;
    UInt8       *TempOutBuffer;

    ELG( fPort, fPort->AreTransmitting, 'upTx', "SetUpTransmit" );
    XTRACE(kLogSetupTransmit, 0, fPort->AreTransmitting);
    
	//  If we are already in the cycle of transmitting characters,
	//  then we do not need to do anything.
	
    if ( fPort->AreTransmitting == TRUE )
	return false;

	// First check if we can actually do anything, also if IrDA has no room we're done for now

    //if ( GetQueueStatus( &fPort->TX ) != queueEmpty )
    if (UsedSpaceinQueue(&fPort->TX) > 0)
    {
	data_Length = fIrDA->TXBufferAvailable();
	if ( data_Length == 0 )
	{
	    return false;
	}
    
	if ( data_Length > MAX_BLOCK_SIZE )
	{
	    data_Length = MAX_BLOCK_SIZE;
	}
	
	TempOutBuffer = (UInt8*)IOMalloc( data_Length );
	if ( !TempOutBuffer )
	{
	    ELG( 0, 0, 'STA-', "SetUpTransmit - buffer allocation problem" );
	    return false;
	}
	bzero( TempOutBuffer, data_Length );

	// Fill up the buffer with characters from the queue

	count = RemovefromQueue( &fPort->TX, TempOutBuffer, data_Length );
	ELG( fPort->State, count, ' Tx+', "SetUpTransmit - Sending to IrDA" );
    
	fPort->AreTransmitting = TRUE;
	changeState( fPort, PD_S_TX_BUSY, PD_S_TX_BUSY );

	tCount = fIrDA->Write( TempOutBuffer, count );      // do the "transmit" -- send to IrCOMM

	changeState( fPort, 0, PD_S_TX_BUSY );
	fPort->AreTransmitting = false;

	IOFree( TempOutBuffer, data_Length );
	if ( tCount != count )
	{
	    ELG( tCount, count, 'IrW-', "SetUpTransmit - IrDA write problem, data has been dropped" );
	    return false;
	}

	// We potentially removed a bunch of stuff from the
	// queue, so see if we can free some thread(s)
	// to enqueue more stuff.
	
	CheckQueues( fPort );
    }

    return true;
    
}/* end SetUpTransmit */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::StartTransmission
//
//      Inputs:     control_length - Length of control data
//                  control_buffer - Control data
//                  data_length - Length of raw data
//                  data_buffer - raw data
//
//      Outputs:    Return code - kIOReturnSuccess
//
//      Desc:       Start the transmisson. If both control and data length is zero then only
//                  the change byte will be sent.
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::StartTransmit(UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer)
{
    IOReturn    ior;
    UInt8       changeByte;
    
    ELG( 0, fPort, 'StTx', "StartTransmission" );
    check(fWriteActive == false);                   // bail?
    
    // Sending control and data
	
    changeByte = (fBofsCode << 4) | fBaudCode;      // compute new mode byte
    fPipeOutBuffer[0] = (changeByte != fLastChangeByte) ? changeByte : 0;   // tell hardware new mode if changed

    fLastChangeByte = changeByte;                   // save new mode for next time through
    
    // append the control and data buffers after the mode byte
    if ( control_length != 0 )
    {
	bcopy(control_buffer, &fPipeOutBuffer[1], control_length);
	if ( data_length != 0 )
	{
	    bcopy(data_buffer, &fPipeOutBuffer[control_length+1], data_length);
	}
    }
    
    // add up the total length to send off to the device
    fCount = control_length + data_length + 1;
    fpPipeOutMDP->setLength( fCount );
	    
//  LogData( kUSBOut, fCount, fPipeOutBuffer );
    XTRACE(kLogXmitLen, 0, fCount);
    fWriteActive = true;
    //ior = fpOutPipe->Write( fpPipeOutMDP, &fWriteCompletionInfo );
    ior = fpOutPipe->Write( fpPipeOutMDP, 1000, 1000, &fWriteCompletionInfo );  // 1 second timeouts
    
    return ior;
    
}/* end StartTransmission */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::SetStructureDefaults
//
//      Inputs:     port - the port to set the defaults, Init - Probe time or not
//
//      Outputs:    None
//
//      Desc:       Sets the defaults for the specified port structure
//
/****************************************************************************************************/

void AppleUSBIrDADriver::SetStructureDefaults( PortInfo_t *port, bool Init )
{
    UInt32  tmp;
    
    ELG( 0, 0, 'StSD', "SetStructureDefaults" );
    XTRACE(kLogSetStructureDefaults, 0, Init);

	/* These are initialized when the port is created and shouldn't be reinitialized. */
    if ( Init )
    {
	port->FCRimage          = 0x00;
	port->IERmask           = 0x00;

	port->State             = ( PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER );
	port->WatchStateMask    = 0x00000000;              
 //       port->serialRequestLock = 0;
//      port->readActive        = false;
    }

    port->BaudRate          = kDefaultBaudRate;         // 9600 bps
    port->CharLength        = 8;                        // 8 Data bits
    port->StopBits          = 2;                        // 1 Stop bit
    port->TX_Parity         = 1;                        // No Parity
    port->RX_Parity         = 1;                        // --ditto--
    port->MinLatency        = false;
    port->XONchar           = '\x11';
    port->XOFFchar          = '\x13';
    port->FlowControl       = 0x00000000;
    port->RXOstate          = IDLE_XO;
    port->TXOstate          = IDLE_XO;
    port->FrameTOEntry      = NULL;

//  port->RXStats.BufferSize    = BUFFER_SIZE_DEFAULT;
    port->RXStats.BufferSize    = kMaxCirBufferSize;
//  port->RXStats.HighWater     = port->RXStats.BufferSize - (DATA_BUFF_SIZE*2);
    port->RXStats.HighWater     = (port->RXStats.BufferSize << 1) / 3;
    port->RXStats.LowWater      = port->RXStats.HighWater >> 1;

//  port->TXStats.BufferSize    = BUFFER_SIZE_DEFAULT;
    port->TXStats.BufferSize    = kMaxCirBufferSize;
    port->TXStats.HighWater     = (port->RXStats.BufferSize << 1) / 3;
    port->TXStats.LowWater      = port->RXStats.HighWater >> 1;
    
    port->FlowControl           = (DEFAULT_AUTO | DEFAULT_NOTIFY);
//  port->FlowControl           = DEFAULT_NOTIFY;

    port->AreTransmitting   = FALSE;

    for ( tmp=0; tmp < (256 >> SPECIAL_SHIFT); tmp++ )
	port->SWspecial[ tmp ] = 0;

    return;
    
}/* end SetStructureDefaults */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::freeRingBuffer
//
//      Inputs:     Queue - the specified queue to free
//
//      Outputs:    None
//
//      Desc:       Frees all resources assocated with the queue, then sets all queue parameters 
//                  to safe values.
//
/****************************************************************************************************/

void AppleUSBIrDADriver::freeRingBuffer( CirQueue *Queue )
{
    ELG( 0, Queue, 'f rb', "freeRingBuffer" );
    require(Queue->Start, Bogus);
    
    IOFree( Queue->Start, Queue->Size );
    CloseQueue( Queue );

Bogus:
    return;
    
}/* end freeRingBuffer */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::allocateRingBuffer
//
//      Inputs:     Queue - the specified queue to allocate, BufferSize - size to allocate
//
//      Outputs:    return Code - true (buffer allocated), false (it failed)
//
//      Desc:       Allocates resources needed by the queue, then sets up all queue parameters. 
//
/****************************************************************************************************/

bool AppleUSBIrDADriver::allocateRingBuffer( CirQueue *Queue, size_t BufferSize )
{
    UInt8       *Buffer;

	// Size is ignored and kMaxCirBufferSize, which is 4096, is used.
	
    ELG( 0, BufferSize, 'alrb', "allocateRingBuffer" );
    Buffer = (UInt8*)IOMalloc( kMaxCirBufferSize );

    InitQueue( Queue, Buffer, kMaxCirBufferSize );

    if ( Buffer )
	return true;

    return false;
    
}/* end allocateRingBuffer */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::message
//
//      Inputs:     type - message type, provider - my provider, argument - additional parameters
//
//      Outputs:    return Code - kIOReturnSuccess
//
//      Desc:       Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::message( UInt32 type, IOService *provider,  void *argument)
{
    
    ELG( 0, type, 'mess', "message" );
    XTRACE(kLogMessage, type >> 16, (short)type);
    
    switch ( type )
    {
	case kIOMessageServiceIsTerminated:
	    ELG( 0, type, 'mess', "message - kIOMessageServiceIsTerminated" );
	    
#ifdef old	// don't need to do stops, will be closed shortly
	    
	    if (fIrDA)
	    {
		int REVIEW_fTerminate;  // this isn't right yet.
		stopIrDA();         // stop irda now
	    }
	    
	    if ( fSessions )
	    {
		if ( (fPort != NULL) && (fPort->serialRequestLock != 0) )
		{
//                  changeState( fPort, 0, (UInt32)PD_S_ACTIVE );
		}
				
		KUNCUserNotificationDisplayNotice(
		0,      // Timeout in seconds
		0,      // Flags (for later usage)
		"",     // iconPath (not supported yet)
		"",     // soundPath (not supported yet)
		"",     // localizationPath (not supported  yet)
		"USB IrDA Unplug Notice",       // the header
		"The USB IrDA Pod has been unplugged while an Application was still active. This can result in loss of data.",
		"OK");
	    } else {
		if ( fpInterface )  
		{
		    fpInterface->close( this ); 
		    fpInterface->release();
		    fpInterface = NULL; 
		}
	    }
	    
	    fTerminate = true;      // we're being terminated (unplugged)
#endif // old
	    
	    /* We need to disconnect the user client interface */
	    messageClients(kIrDACallBack_Unplug, 0, 1);
	    break;
	    
	case kIOMessageServiceIsSuspended:  
	    ELG( 0, type, 'mess', "message - kIOMessageServiceIsSuspended" );
	    break;
	    
	case kIOMessageServiceIsResumed:    
	    ELG( 0, type, 'mess', "message - kIOMessageServiceIsResumed" );
	    break;
	    
	case kIOMessageServiceIsRequestingClose: 
	    ELG( 0, type, 'mess', "message - kIOMessageServiceIsRequestingClose" ); 
	    break;
	    
	case kIOMessageServiceWasClosed:    
	    ELG( 0, type, 'mess', "message - kIOMessageServiceWasClosed" ); 
	    break;
	    
	case kIOMessageServiceBusyStateChange:  
	    ELG( 0, type, 'mess', "message - kIOMessageServiceBusyStateChange" ); 
	    break;
	    
	case kIOUSBMessagePortHasBeenResumed:   
	    ELG( 0, type, 'mess', "message - kIOUSBMessagePortHasBeenResumed" );
	    DebugLog("message = kIOUSBMessagePortHasBeenResumed" );
	    
	    if ( !fIrDAOn )         // We tried to suspend, but it failed
	    {
		fSuspendFail = true;
		ELG( 0, 0, 'msS-', "message - Suspend device really failed" );
	    }
	    else {                  // we're trying to resume, so start irda
		if ( !startIrDA() )
		{
		    fIrDAOn = false;
		    fTerminate = true;
		    IOLog("AppleUSBIrDADriver: message - startIrDA failed\n" );
		} else {
		    ELG( 0, 0, 'msc+', "message - startIrDA successful" );
		    //IOLog("AppleUSBIrDADriver: message - startIrDA successful\n" );
		}
	    }
	    break;
	
	case kIOUSBMessageHubResumePort:
	    ELG( 0, type, 'mess', "message - kIOUSBMessageHubResumePort" );
	    DebugLog("message = kIOUSBMessageHubResumePort" );
			
	    if ( !fIrDAOn )         // Means the suspend failed
	    {
		fSuspendFail = true;
	    }
	    else {                  // we're being asked to resume
		if ( !startIrDA() )
		{
		    ELG( 0, 0, 'msc-', "message - startIrDA failed" );
#if 0
		    KUNCUserNotificationDisplayNotice(
		    0,      // Timeout in seconds
		    0,      // Flags (for later usage)
		    "",     // iconPath (not supported yet)
		    "",     // soundPath (not supported yet)
		    "",     // localizationPath (not supported  yet)
		    "USB IrDA Problem Notice",      // the header
		    "The USB IrDA Pod has experienced difficulties. To continue either replug the device (if external) or restart the computer",
		    "OK");
#endif
		
		    fIrDAOn = false;        // We're basically sol at this point
		    fTerminate = true;  
		} else {
		    ELG( 0, 0, 'msc+', "message - createSerialStream successful" );
		    //IOLog("AppleUSBIrDADriver: message - createSerialStream successful\n" );
		}
	    }

	default:
	    ELG( 0, type, 'mess', "message - unknown message" ); 
	    break;
    }
    
    return kIOReturnSuccess;
}

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::readPortState
//
//      Inputs:     port - the specified port
//
//      Outputs:    returnState - current state of the port
//
//      Desc:       Reads the current Port->State. 
//
/****************************************************************************************************/

UInt32 AppleUSBIrDADriver::readPortState( PortInfo_t *port )
{
    UInt32              returnState;
    
//  ELG( 0, port, 'rPSt', "readPortState" );

    IOLockLock( port->serialRequestLock );

    returnState = port->State;

    IOLockUnlock( port->serialRequestLock);

//  ELG( returnState, 0, 'rPS-', "readPortState" );

    return returnState;
    
}/* end readPortState */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::changeState
//
//      Inputs:     port - the specified port, state - new state, mask - state mask (the specific bits)
//
//      Outputs:    None
//
//      Desc:       Change the current Port->State to state using the mask bits.
//                  if mask = 0 nothing is changed.
//                  delta contains the difference between the new and old state taking the
//                  mask into account and it's used to wake any waiting threads as appropriate. 
//
/****************************************************************************************************/

void AppleUSBIrDADriver::changeState( PortInfo_t *port, UInt32 state, UInt32 mask )
{
    UInt32              delta;
    
//  ELG( state, mask, 'chSt', "changeState" );

    IOLockLock( port->serialRequestLock );
    state = (port->State & ~mask) | (state & mask); // compute the new state
    delta = state ^ port->State;                    // keep a copy of the diffs
    port->State = state;

	// Wake up all threads asleep on WatchStateMask
	
    if ( delta & port->WatchStateMask )
    {
	thread_wakeup_with_result( &port->WatchStateMask, THREAD_RESTART );
    }

    IOLockUnlock( port->serialRequestLock );

    ELG( port->State, delta, 'chSt', "changeState - exit" );
    return;
    
}/* end changeState */

/****************************************************************************************************/
//
//      Method:     AppleUSBIrDADriver::privateWatchState
//
//      Inputs:     port - the specified port, state - state watching for, mask - state mask (the specific bits)
//
//      Outputs:    IOReturn - kIOReturnSuccess, kIOReturnIOError or kIOReturnIPCError
//
//      Desc:       Wait for the at least one of the state bits defined in mask to be equal
//                  to the value defined in state. Check on entry then sleep until necessary.
//                  A return value of kIOReturnSuccess means that at least one of the port state
//                  bits specified by mask is equal to the value passed in by state.  A return
//                  value of kIOReturnIOError indicates that the port went inactive.  A return
//                  value of kIOReturnIPCError indicates sleep was interrupted by a signal. 
//
/****************************************************************************************************/

IOReturn AppleUSBIrDADriver::privateWatchState( PortInfo_t *port, UInt32 *state, UInt32 mask )
{
    unsigned            watchState, foundStates;
    bool                autoActiveBit   = false;
    IOReturn            rtn             = kIOReturnSuccess;

//    ELG( mask, *state, 'wsta', "privateWatchState" );

    watchState              = *state;
    IOLockLock( port->serialRequestLock );

	// hack to get around problem with carrier detection
	
    if ( *state | 0x40 )    /// mlj ??? PD_S_RXQ_FULL?
    {
	port->State |= 0x40;
    }

    if ( !(mask & (PD_S_ACQUIRED | PD_S_ACTIVE)) )
    {
	watchState &= ~PD_S_ACTIVE; // Check for low PD_S_ACTIVE
	mask       |=  PD_S_ACTIVE; // Register interest in PD_S_ACTIVE bit
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
//          ELG( rtn, foundStates, 'FndS', "privateWatchState - foundStates" );
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

	assert_wait( &port->WatchStateMask, true ); /* assert event */

	IOLockUnlock( port->serialRequestLock );
	rtn = thread_block( 0 );         /* block ourselves */
	IOLockLock( port->serialRequestLock );

	if ( rtn == THREAD_RESTART )
	{
	    continue;
	} else {
	    rtn = kIOReturnIPCError;
	    break;
	}
    }/* end for */

	    // As it is impossible to undo the masking used by this
	    // thread, we clear down the watch state mask and wakeup
	    // every sleeping thread to reinitialize the mask before exiting.
	
    port->WatchStateMask = 0;

    thread_wakeup_with_result( &port->WatchStateMask, THREAD_RESTART );
    IOLockUnlock( port->serialRequestLock);
    
    //    ELG( rtn, *state, 'wEnd', "privateWatchState end" );
    
    return rtn;
    
}/* end privateWatchState */

#pragma mark -- hardware workaround

/****************************************************************************************************/
//
// Workaround: (re)send the device configuration to put the KC hardware back into a known state
//
/****************************************************************************************************/

void AppleUSBIrDADriver::Workaround(void)
{
    IOReturn        rc;
    IOUSBDevRequest *request;
    
    XTRACE(kLogWorkAround, 0, 0);
    //DebugLog("AppleUSBIrDA: workaround called");
	
    request = (IOUSBDevRequest*)IOMalloc( sizeof(IOUSBDevRequest) );
    require(request, Failed);
    bzero( request, sizeof(IOUSBDevRequest) );
    // allocate pData here if needed
	    
    //request->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    
    request->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);   // long macro for zero
    request->bRequest = kUSBRqSetConfig;        // 9 - set configuration
    request->wValue = 1;                        // new configuration #1
    request->wIndex = 0;
    request->wLength = 0;
    
    check(request->bmRequestType == 0);
    check(request->bRequest == 9);      // sanity eludes
    
    fRequestCompletionInfo.target = this;
    fRequestCompletionInfo.action = workAroundComplete;
    fRequestCompletionInfo.parameter = request;
    
    rc = fpDevice->DeviceRequest(request, &fRequestCompletionInfo);
    check(rc == kIOReturnSuccess);
    
Failed:
    return;
}/* end Workaround */


/****************************************************************************************************/
//
//      Function:   AppleUSBCDCDriver::workAroundComplete
//
//      Inputs:     obj - me, param - request block 
//                  rc - return code, remaining - what's left
//
//      Outputs:    None
//
/****************************************************************************************************/

void AppleUSBIrDADriver::workAroundComplete(void *obj, void *param, IOReturn rc, UInt32 remaining )
{
    //AppleUSBIrDADriver    *me = (AppleUSBIrDADriver*)obj;
    IOUSBDevRequest     *request = (IOUSBDevRequest*)param;
    UInt16              dataLen;

    XTRACE(kLogWorkAroundComplete, 0, 0);
    require(request, Fail);

    dataLen = request->wLength;
    if ((dataLen != 0) && (request->pData)) {   // doesn't happen here
	IOFree(request->pData, dataLen);
    }
    IOFree(request, sizeof(IOUSBDevRequest));
		
Fail:
    return;
}   /* end workAroundComplete */


#pragma mark -- Glue
/****************************************************************************************************/
// Glue to call the actual USB driver over the IOSerialStreamSync hurdle
//
// todo: see if dynamic cast is slow, if so just check for AppleUSBIrDADriver
// once at attach time and maybe save a copy of it.
/****************************************************************************************************/
#undef super
#define super AppleIrDASerial
    OSDefineMetaClassAndStructors( AppleUSBIrDA, AppleIrDASerial );

bool
AppleUSBIrDA::attach(AppleUSBIrDADriver *provider)
{
    // any reason not to do the type check for AppleUSBIrDADriver at compile time?
    return super::attach(provider);
}

void
AppleUSBIrDA::Add_RXBytes( UInt8 *Buffer, size_t Size )
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->Add_RXBytes(Buffer, Size);
}

SInt16
AppleUSBIrDA::SetBofCount(SInt16 bof_count)
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->SetBofCount(bof_count);
    else
	return -1;
}

UInt16
AppleUSBIrDA::SetSpeed(UInt32 brate)
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->SetSpeed(brate);
    else
	return 0;
}

bool
AppleUSBIrDA::SetUpTransmit( void )
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->SetUpTransmit();
    else
	return false;
}

IOReturn
AppleUSBIrDA::StartTransmit( UInt32 control_length, UInt8 *control_buffer, UInt32 data_length, UInt8 *data_buffer )
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->StartTransmit(control_length, control_buffer, data_length, data_buffer);
    else
	return -1;
}

USBIrDAQoS *
AppleUSBIrDA::GetIrDAQoS( void )
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->GetIrDAQoS();
    else
	return NULL;
}

IrDAComm *
AppleUSBIrDA::GetIrDAComm( void )
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->GetIrDAComm();
    else
	return NULL;
}

void
AppleUSBIrDA::GetIrDAStatus( IrDAStatus *status )
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->GetIrDAStatus(status);
}

IOReturn
AppleUSBIrDA::SetIrDAUserClientState( bool IrDAOn )
{
    AppleUSBIrDADriver *driver;
    driver = OSDynamicCast(AppleUSBIrDADriver, fProvider);
    if (driver)
	return driver->SetIrDAUserClientState(IrDAOn);
    else
	return -1;
}

