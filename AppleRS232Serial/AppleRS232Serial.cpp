/* 
 *Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 *@APPLE_LICENSE_HEADER_START@
 *
 *The contents of this file constitute Original Code as defined in and
 *are subject to the Apple Public Source License Version 1.1 (the
 *"License").  You may not use this file except in compliance with the
 *License.  Please obtain a copy of the License at
 *http://www.apple.com/publicsource and read it before using this file.
 *
 *This Original Code and all software distributed under the License are
 *distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *License for the specific language governing rights and limitations
 *under the License.
 *
 *@APPLE_LICENSE_HEADER_END@
 */
 
#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/serial/IOSerialKeys.h>

#include "AppleRS232Serial.h"

    // Globals

#if USE_ELG
    com_apple_iokit_XTrace	*gXTrace = 0;
    UInt32			gTraceID;
#endif

//#define super IORS232SerialStreamSync
#define super IOSerialDriverSync

//OSDefineMetaClassAndStructors(AppleRS232Serial, IORS232SerialStreamSync);
OSDefineMetaClassAndStructors(AppleRS232Serial, IOSerialDriverSync);

#if USE_ELG
#define DEBUG_NAME "AppleRS232Serial"

/****************************************************************************************************/
//
//		Function:	KernelDebugFindKernelLogger
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Just like the name says
//
/****************************************************************************************************/

IOReturn KernelDebugFindKernelLogger()
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
    
}/* end KernelDebugFindKernelLogger */
#endif

#if LOG_DATA
#define dumplen		32		// Set this to the number of bytes to dump and the rest should work out correct

#define buflen		((dumplen*2)+dumplen)+3
#define Asciistart	(dumplen*2)+3

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
 
UInt8 Asciify(UInt8 i)
{

    i &= 0xF;
    if ( i < 10 )
        return( '0' + i );
    else return( 55  + i );
	
}/* end Asciify */

/****************************************************************************************************/
//
//		Function:	SerialLogData
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

void SerialLogData(UInt8 Dir, UInt32 Count, char *buf)
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
        case kSerialIn:
#if USE_ELG
            XTRACE2(buf, Count, "SerialLogData - Read Complete, address, size");
#else
            IOLog( "AppleRS232Serial: SerialLogData - Read Complete, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kSerialOut:
#if USE_ELG
            XTRACE2(buf, Count, "SerialLogData - Write, address, size");
#else
            IOLog( "AppleRS232Serial: SerialLogData - Write, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
#endif
            break;
        case kSerialOther:
#if USE_ELG
            XTRACE2(buf, Count, "SerialLogData - Other, address, size");
#else
            IOLog( "AppleRS232Serial: SerialLogData - Other, address = %8x, size = %8d\n", (UInt)buf, (UInt)Count );
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
        XTRACE2(0, Count, "SerialLogData - No data, Count=0");
#else
        IOLog( "AppleRS232Serial: SerialLogData - No data, Count=0\n" );
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
            case kSerialIn:
                XTRACE2((w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "SerialLogData - Rx buffer dump");
                break;
            case kSerialOut:
                XTRACE2((w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "SerialLogData - Tx buffer dump");
                break;
            case kSerialOther:
                XTRACE2((w[0] << 24 | w[1] << 16 | w[2] << 8 | w[3]), (w[4] << 24 | w[5] << 16 | w[6] << 8 | w[7]), "SerialLogData - Misc buffer dump");
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

}/* end SerialLogData */
#endif

/****************************************************************************************************/
//
//		Function:	getDebugFlagsTable
//
//		Inputs:		props - IOKit debug dictionary
//
//		Outputs:	return code - debug flag
//
//		Desc:		Gets the state of the global debug flag. 
//
/****************************************************************************************************/

static inline UInt64 getDebugFlagsTable(OSDictionary *props)
{
    OSNumber *debugProp;
    UInt64    debugFlags = gIOKitDebug;

    debugProp = OSDynamicCast(OSNumber, props->getObject(gIOKitDebugKey));
    if (debugProp)
	debugFlags = debugProp->unsigned64BitValue();

    return debugFlags;
    
}/* end getDebugFlagsTable */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but...)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleRS232Serial::start(IOService *provider)
{
    IOMemoryMap 	*map;
    IOByteCount		temp;
    
#if USE_ELG
    XTraceLogInfo	*logInfo;
    
    KernelDebugFindKernelLogger();
    if (gXTrace)
    {
        gXTrace->retain();		// don't let it unload ...
        gTraceID = gXTrace->GetNewId();
        ELG((UInt32)this, 0xbeefbeef, "AppleRS232Serial: Hello from start");
        logInfo = gXTrace->LogGetInfo();
        IOLog("AppleRS232Serial::start - Log is at %x\n", (unsigned int)logInfo);
    } else {
        return false;
    }
#endif
    
    ELG(this, provider, "AppleRS232Serial::start - this, provider");

    if (!super::start(provider))
    {
        ALERT(0, 0, "AppleRS232Serial::start - super failed");
        return false;
    }

    fProvider = OSDynamicCast(AppleMacIODevice, provider);
    
    if (!fProvider)
    {
        ALERT(0, 0, "AppleRS232Serial::start - provider invalid");
        return false;
    }

/*** matching on AAPL,connector set to DB9, don't need the other checks

        // Check if the parent is escc or escc-legacy - don't need to match on escc-legacy nodes
        
    parentEntry = provider->getParentEntry(gIODTPlane);
    if (!parentEntry || IODTMatchNubWithKeys(parentEntry, "'escc-legacy'"))
    {
        ELG(0, 0, "AppleRS232Serial::start - escc-legacy not supported");
		return false;
    }

    ok = false;
            
	// If "AAPL,connector" is there with a value of "DB9", then we're ok
        
    cProp = OSDynamicCast(OSData, provider->getProperty("AAPL,connector"));
    if (cProp != NULL)
    {
        tmp = (char *)cProp->getBytesNoCopy();
        if (strcmp(tmp, "DB9") == 0)
        {
            ok = true;
        }
    }
        
        
    if (!ok)
    {
        ALERT(0, 0, "AppleRS232Serial::start - returning false early, Connector or machine incorrect");
        return false;
    }
	*****/

	/* This section will stop the driver from loading on machines that have debugging enabled */
	/* This is to address <rdar://problem/3543234> */

	{
        UInt32	debugFlags;
        if (!PE_parse_boot_arg("debug", &debugFlags))
            debugFlags = 0;
		if (debugFlags & 8)
		{
			ALERT(0, 0, "AppleRS232Serial::start - returning false early, Serial debugging is enabled");
			return false;
		}
    }
	
	/**** this returns NULL now that we're not matching by name anymore
    matched = (OSString *)getProperty(gIONameMatchedKey);
	if (matched) {
		if (matched->isEqualTo("ch-a"))
		{
			ELG(0, 0, "AppleRS232Serial::start - escc Port A");
			fPort.whichPort = serialPortA;
		} else {
			if (matched->isEqualTo("ch-b"))
			{
				ELG(0, 0, "AppleRS232Serial::start - escc Port B");
				fPort.whichPort = serialPortB;
			} else {
				ALERT(0, 0, "AppleRS232Serial::start - Port invalid");
				return false;
			}
		}
	}
	*****/
    fPort.whichPort = serialPortA;	// jdg - this driver only matches on sccA

    
        // get workloop
        
    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
    {
        ALERT(0, 0, "AppleRS232Serial::start - getWorkLoop failed");
        return false;
    }
    
    fWorkLoop->retain();
    
    fCommandGate = IOCommandGate::commandGate(this);
    if (!fCommandGate)
    {
        ALERT(0, 0, "AppleRS232Serial::start - commandGate failed");
        shutDown();
        return false;
    }
    
    if (fWorkLoop->addEventSource(fCommandGate) != kIOReturnSuccess)
    {
        ALERT(0, 0, "AppleRS232Serial::start - addEventSource(commandGate) failed");
        shutDown();
        return false;
    }

    fCommandGate->enable();

    fPort.DTRAsserted = true;					// Data Terminal Ready, true unless doing DTR flow control
    fPort.xOffSent = false;					// set true only if we're doing software flow control and have send xoff
    fPort.RTSAsserted = true;					// RTS.  true unless doing RTS flow control and have lowered to slow down rx
    fPort.aboveRxHighWater = false;
	
    
        // Init the Port structure
        
    if (!initializePort(&fPort))					// Done at start and should not be done again
    {
        ALERT(0, 0, "AppleRS232Serial::start - Initialize port failed");
        shutDown();
        return false;
    }
    
    setStructureDefaults(&fPort);  
    fPort.RS232 = this;							// So some of our boys can find their way home
    
        // Get chip access addresses
        
    map = provider->mapDeviceMemoryWithIndex(0);
    if (!map) 
    {
        ALERT(0, 0, "AppleRS232Serial::start - Mapping device memory failed");
        shutDown();
        return false;
    }
    
    fPort.ChipBaseAddress	    = map->getVirtualAddress();				// was fPort.Base;
    fPort.ChipBaseAddressPhysical   = map->getPhysicalSegment(0, &temp);
    ALERT(fPort.ChipBaseAddress, fPort.ChipBaseAddressPhysical, "chip base, virtual, physical");

        
    if (!SccSetDMARegisters(&fPort, provider))
    {
        ALERT(0, 0, "AppleRS232Serial::start - DMA setup failed");
        shutDown();
        return false;
    }
   
    fPort.TXStats.BufferSize = BUFFER_SIZE_DEFAULT;
    fPort.RXStats.BufferSize = BUFFER_SIZE_DEFAULT;
    
    initChip(&fPort);

    sccInterruptSource = IOInterruptEventSource::interruptEventSource(this, (IOInterruptEventAction)&AppleRS232Serial::interruptHandler,
                                                                                                                    provider, kIntChipSet);	
    if (!sccInterruptSource)
    {    
	ALERT(0, 0, "AppleRS232Serial::start - Interrupt event source failed");
        shutDown();
        return false;
    }
	
    if (fWorkLoop->addEventSource(sccInterruptSource) != kIOReturnSuccess)
    {
	ALERT(0, 0, "AppleRS232Serial::start - Add interrupt source to work loop failed");
        shutDown();
    }
	
#if USE_FILTER_EVENT_SOURCES
    txDMAInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this, (IOInterruptEventAction) &AppleRS232Serial::interruptHandler,
                                                                        (IOFilterInterruptAction) &AppleRS232Serial::interruptFilter, provider, kIntTxDMA);	
#else
    txDMAInterruptSource = IOInterruptEventSource::interruptEventSource(this, (IOInterruptEventAction)&AppleRS232Serial::interruptHandler, provider, kIntTxDMA);
#endif	

    if (!txDMAInterruptSource)
    {    
	ALERT(0, 0, "AppleRS232Serial::start - TX DMA interrupt event source failed");
        shutDown();
        return false;
    }
	
    if (fWorkLoop->addEventSource(txDMAInterruptSource) != kIOReturnSuccess)
    {
	ALERT(0, 0, "AppleRS232Serial::start - Add TX DMA interrupt event source to work loop failed");
        shutDown();  
        return false;
    }
	

#if USE_FILTER_EVENT_SOURCES
    rxDMAInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this, (IOInterruptEventAction) &AppleRS232Serial::interruptHandler,
                                                                        (IOFilterInterruptAction) &AppleRS232Serial::interruptFilter, provider, kIntRxDMA);
#else
    rxDMAInterruptSource = IOInterruptEventSource::interruptEventSource(this, (IOInterruptEventAction)&AppleRS232Serial::interruptHandler, provider, kIntRxDMA);
#endif	
	
    if (!rxDMAInterruptSource)
    {    
	ALERT(0, 0, "AppleRS232Serial::start - RX DMA interrupt event source failed");
        shutDown();
        return false;
    }
	
    if (fWorkLoop->addEventSource(rxDMAInterruptSource) != kIOReturnSuccess)
    {
	ALERT(0, 0, "AppleRS232Serial::start - Add RX DMA interrupt event source to work loop failed");
        shutDown();  
        return false;
    }
	
	// Get a timer and set our timeout handler to be called when it fires
        
    fPort.rxTimer = IOTimerEventSource::timerEventSource(this, rxTimeoutHandler);
	
    if( !fPort.rxTimer )
    {
	ALERT(0, 0, "AppleRS232Serial::start - Timer event source failed");
        shutDown();
        return false;
    }

	// Add the timer to the workloop
        
    if (fWorkLoop->addEventSource(fPort.rxTimer) != kIOReturnSuccess)
    {
	ALERT(0, 0, "AppleRS232Serial::start - Add timer event source to work loop failed");
        shutDown();
        return false;
    }

        // Create and set up the ring buffers
            
    if (!allocateRingBuffer(&(fPort.TX), fPort.TXStats.BufferSize, fWorkLoop) ||
	!allocateRingBuffer(&(fPort.RX), fPort.RXStats.BufferSize, fWorkLoop))
    {
        ELG(0, 0, "AppleRS232Serial::start - Allocate for ring buffers failed");
        shutDown();
        return false;
    }
    
        // Enable the hardware (important for powerbooks and core 99)
        
    callPlatformFunction("EnableSCC", false, (void *)true, 0, 0, 0);
    
//rcs Tiger Fixes..
    fdmaStartTransmissionThread = thread_call_allocate (
        &SccStartTransmissionDelayedHandler, ( thread_call_param_t ) this);
	if ( fdmaStartTransmissionThread == NULL )
	{
        return false;
	}


    dmaRxHandleCurrentPositionThread = thread_call_allocate (
        &SccCurrentPositionDelayedHandler, ( thread_call_param_t ) this);
	if ( dmaRxHandleCurrentPositionThread == NULL )
	{
        return false;
	}

        // Finished all initialization so start service matching
    if (!createSerialStream(provider))
    {
        ALERT(0, 0, "AppleRS232Serial::start - Create serial stream failed");
        shutDown();
        return false;
    }
    
    // register for power changes
    fCurrentPowerState = 1;		// we default to power on
    if (!initForPM(provider)) {
        ALERT(0, 0, "AppleRS232Serial::start - failed to init for power management");
        shutDown();
	return false;
    }
        
    ELG(0, 0, "AppleRS232Serial::start - Successful");
    return true;
    
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	
//
//		Desc:		We're done so clean up and release everything.
//
/****************************************************************************************************/

void AppleRS232Serial::stop(IOService *provider)
{

    ELG(0, 0, "AppleRS232Serial::stop");
    
    PMstop();		// unhook from power tree
    
    shutDown();
   
    super::stop(provider);
    
}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::getWorkLoop
//
//		Inputs:	
//
//		Outputs:	
//
//		Desc:		create our own workloop if we don't have one already.
//
/****************************************************************************************************/
IOWorkLoop* AppleRS232Serial::getWorkLoop() const
{
    IOWorkLoop *w;
    
    if (fWorkLoop) w = fWorkLoop;
    else	   w = IOWorkLoop::workLoop();
    
    ELG(0, w, "get workloop, workloop=");
    return w;
    
}/* end getWorkLoop */


/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::acquirePort
//
//		Inputs:		sleep - true (wait for it), false (don't)
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnExclusiveAccess, kIOReturnIOError and various others
//
//		Desc:		Set up for gated acquirePort call.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::acquirePort(bool sleep, void *refCon)
{
    IOReturn	ret;
    
    ELG(0, sleep, "AppleRS232Serial::acquirePort");
    
    retain();
    ret = fCommandGate->runAction(acquirePortAction, (void *)sleep);
    release();
    
    return ret;

}/* end acquirePort */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::acquirePortAction
//
//		Desc:		Dummy pass through for acquirePortGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::acquirePortAction(OSObject *owner, void *arg0, void *, void *, void *)
{
    ELG(owner, arg0, "acquirePortAction");
    return ((AppleRS232Serial *)owner)->acquirePortGated((bool)arg0);
    
}/* end acquirePortAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::acquirePortGated
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

IOReturn AppleRS232Serial::acquirePortGated(bool sleep)
{
    UInt32 	busyState = 0;
    IOReturn	rtn = kIOReturnSuccess;

    ELG(0, sleep, "AppleRS232Serial::acquirePortGated");
    
    retain(); 								// Hold reference till releasePort(), unless we fail to acquire
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
                ELG(0, 0, "AppleRS232Serial::acquirePortGated - Busy exclusive access");
                release();
            	return kIOReturnExclusiveAccess;
            } else {
            	busyState = 0;
            	rtn = watchStateGated(&busyState, PD_S_ACQUIRED);
            	if ((rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess))
                {
                    continue;
            	} else {
                    ELG( 0, 0, "AppleRS232Serial::acquirePortGated - Interrupted!" );
                    release();
                    return rtn;
                }
            }
        }
    }

        // Take control of the port
        
    do
    {
        if (!fProvider->open(this))
        {
            rtn = kIOReturnBusy;
            ELG(0, rtn, "AppleRS232Serial::acquirePortGated - Open for provider failed");
            break;
        }

            // Initialize the structure and reset the queues
            
        setStructureDefaults(&fPort);
        ResetQueue(&fPort.TX);
        ResetQueue(&fPort.RX);

            // OK, Let's actually setup the chip
            
        OpenScc(&fPort);
		
            // Enables all the DMA transfers
            
        SccSetupReceptionChannel(&fPort, 0);
        //SccdbdmaDefineReceptionCommands(&fPort, 0);	    // called now in start rx
        SccSetupReceptionChannel(&fPort, 1);
        //SccdbdmaDefineReceptionCommands(&fPort, 1);	    // called now in start rx
        SccSetupTansmissionChannel(&fPort);
        SccdbdmaDefineTansmissionCommands(&fPort);

            // Enable all the interrupts
            
	sccInterruptSource->enable();
	txDMAInterruptSource->enable();
	rxDMAInterruptSource->enable();

	// Begin to monitor the channel
            
        SccdbdmaStartReception(&fPort, fPort.activeRxChannelIndex, true);
        
        portOpened = true;
//        changePowerStateTo(1);
        clock_get_uptime(&startingTime);
        
        ELG(0, 0, "AppleRS232Serial::acquirePortGated - Successful");
        
        return kIOReturnSuccess;
        
    } while (0);

        // Acquire failed for some reason

    fProvider->close(this);
    setStateGated(0, STATE_ALL);					// Clear the entire state word
    release();
    
    ELG(0, rtn, "AppleRS232Serial::acquirePortGated - failed");

    return rtn;
    
}/* end acquirePortGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::releasePort
//
//		Inputs:		refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Set up for gated acquirePort call.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::releasePort(void *refCon)
{
    IOReturn	ret;
    
    ELG(0, 0, "AppleRS232Serial::releasePort");
    
    retain();
    ret = fCommandGate->runAction(releasePortAction);
    release();
    
    return ret;
    
}/* end releasePort */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::releasePortAction
//
//		Desc:		Dummy pass through for releasePortGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::releasePortAction(OSObject *owner, void *, void *, void *, void *)
{
    ELG(0, 0, "releasePortAction");
    return ((AppleRS232Serial *)owner)->releasePortGated();
    
}/* end releasePortAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::releasePortGated
//
//		Inputs:		
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		releasePort returns all the resources and does clean up.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::releasePortGated()
{
    UInt32 	busyState = 0;
    int 	i;

    ELG(0, 0, "AppleRS232Serial::releasePortGated");
    
    busyState = fPort.State & PD_S_ACQUIRED;
    if (!busyState)
    {
        ELG( 0, 0, "AppleRS232Serial::releasePortGated - NOT OPEN" );
        return kIOReturnNotOpen;
    }

        // Default everything
        
    for (i=0; i<(256>>SPECIAL_SHIFT); i++)
    {
        fPort.SWspecial[i] = 0;
    }
    
    fPort.CharLength = 8;
    fPort.XONchar = '\x11';
    fPort.XOFFchar = '\x13';
    fPort.StopBits = 1<<1;
    fPort.TX_Parity = PD_RS232_PARITY_NONE;
    fPort.RX_Parity = PD_RS232_PARITY_DEFAULT;
    fPort.RXOstate = IDLE_XO;
    fPort.BaudRate = kDefaultBaudRate;
    fPort.FlowControl = DEFAULT_NOTIFY;
    fPort.FlowControlState = CONTINUE_SEND;
    fPort.DCDState = false;
    fPort.BreakState = false;
    
    fPort.xOffSent = false;
    fPort.RTSAsserted = true;
    fPort.DTRAsserted = true;    
    
    SccCloseChannel(&fPort);						// Turn the chip off (just in case - it should already have been done)

        // Disables the interrupts
        
    sccInterruptSource->disable();
    txDMAInterruptSource->disable();
    rxDMAInterruptSource->disable();

    fProvider->close(this);
    setStateGated(0, STATE_ALL);					// Clear the entire state word
    
    portOpened = false;
        
//    changePowerStateTo(0);

    release(); 								// Dispose of the self-reference we took in acquirePort()
    
    ELG( 0, 0, "AppleRS232Serial::releasePortGated - OK" );

    return kIOReturnSuccess;
    
}/* end releasePortGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::getState
//
//		Inputs:		refCon - the Port (not used)
//
//		Outputs:	Return value - port state
//
//		Desc:		Set up for gated getState call.
//
/****************************************************************************************************/

UInt32 AppleRS232Serial::getState(void *refCon)
{
    UInt32 currState;
    
    ELG(0, 0, "AppleRS232Serial::getState");
    
    retain();
    currState = fCommandGate->runAction(getStateAction);
    release();
    
    return currState;
    
}/* end getState */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::getStateAction
//
//		Desc:		Dummy pass through for getStateGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::getStateAction(OSObject *owner, void *, void *, void *, void *)
{
    UInt32	newState;
    
    ELG(0, 0, "getStateAction");

    newState = ((AppleRS232Serial *)owner)->getStateGated();
    
    return newState;
    
}/* end getStateAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::getState
//
//		Inputs:		
//
//		Outputs:	Return value - port state
//
//		Desc:		Get the state of the port.
//
/****************************************************************************************************/

UInt32 AppleRS232Serial::getStateGated()
{
    UInt32 	state;
	
    ELG(0, 0, "AppleRS232Serial::getStateGated");
    
    CheckQueues(&fPort);
		
//    state = readPortState(&fPort);
    state = fPort.State;
	
    ELG(0, state, "AppleRS232Serial::getStateGated - Exit");
	
    return state;
	
}/* end getStateGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::setState
//
//		Inputs:		state - state to set
//				mask - state mask
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - See setStateGated
//
//		Desc:		Set up for gated setState call.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::setState(UInt32 state, UInt32 mask, void *refCon)
{
    IOReturn	ret;
    
    ELG(state, mask, "AppleRS232Serial::setState");

        // Cannot acquire or activate via setState
    
    if (mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK)))
    {
        return kIOReturnBadArgument;
    }
        
        // ignore any bits that are read-only
        
    mask &= (~fPort.FlowControl & PD_RS232_A_MASK) | PD_S_MASK;
    if (mask)
    {
        retain();
        ret = fCommandGate->runAction(setStateAction, (void *)state, (void *)mask);
        release();
        return ret;
    }
    return kIOReturnSuccess;
    
}/* end setState */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::setStateAction
//
//		Desc:		Dummy pass through for setStateGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::setStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    ELG(0, 0, "setStateAction");
    return ((AppleRS232Serial *)owner)->setStateGated((UInt32)arg0, (UInt32)arg1);
    
}/* end setStateAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::setStateGated
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

IOReturn AppleRS232Serial::setStateGated(UInt32 state, UInt32 mask)
{
    UInt32	delta;
	
    ELG(state, mask, "AppleRS232Serial::setStateGated");

        // Check if it's being acquired or already acquired

    if ((state & PD_S_ACQUIRED) || (fPort.State & PD_S_ACQUIRED))
    {
        if ((mask & PD_RS232_S_DTR) && ((fPort.FlowControl & PD_RS232_A_DTR) != PD_RS232_A_DTR))
        {
            if ((state & PD_RS232_S_DTR) != (fPort.State & PD_RS232_S_DTR))
            {
                if (state & PD_RS232_S_DTR)
                {
                    SccSetDTR(&fPort, true);
                } else {
                    SccSetDTR(&fPort, false);
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
    }
    
    return kIOReturnNotOpen;
	
}/* end setStateGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::watchState
//
//		Inputs:		state - state to watch for
//				mask - state mask bits
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from ::watchState
//
//		Desc:		Set up for gated watchState call.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::watchState(UInt32 *state, UInt32 mask, void *refCon)
{
    IOReturn 	ret;

    ELG(*state, mask, "AppleRS232Serial::watchState");
    
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
//		Method:		AppleRS232Serial::watchStateAction
//
//		Desc:		Dummy pass through for watchStateGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::watchStateAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    ELG(0, 0, "watchStateAction");
    return ((AppleRS232Serial *)owner)->watchStateGated((UInt32 *)arg0, (UInt32)arg1);
    
}/* end watchStateAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::watchStateGated
//
//		Inputs:		state - state to watch for
//				mask - state mask bits
//
//		Outputs:	Return Code - kIOReturnSuccess or value returned from ::watchState
//
//		Desc:		Wait for the at least one of the state bits defined in mask to be equal
//				to the value defined in state. Check on entry then sleep until necessary,
//				A return value of kIOReturnSuccess means that at least one of the port state
//				bits specified by mask is equal to the value passed in by state.  A return
//				value of kIOReturnIOError indicates that the port went inactive.  A return
//				value of kIOReturnIPCError indicates sleep was interrupted by a signal.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::watchStateGated(UInt32 *state, UInt32 mask)
{
    unsigned 	watchState, foundStates;
    bool 	autoActiveBit = false;
    IOReturn 	ret = kIOReturnNotOpen;

    ELG(*state, mask, "AppleRS232Serial::watchStateGated");

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
        
            retain();							// Just to make sure all threads are awake
            fCommandGate->retain();					// before we're released
        
            ret = fCommandGate->commandSleep((void *)&fPort.State);
        
            fCommandGate->retain();

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
    
        fCommandGate->commandWakeup((void *)&fPort.State);
 
        *state &= EXTERNAL_MASK;
    }
	
    ELG(0, ret, "AppleRS232Serial::watchStateGated - Return code on exit" );
    
    return ret;
	
}/* end watchStateGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::nextEvent
//
//		Inputs:		refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess
//
//		Desc:		Not used by this driver.
//
/****************************************************************************************************/

UInt32 AppleRS232Serial::nextEvent(void *refCon)
{
    UInt32 ret = kIOReturnSuccess;

    ELG( 0, 0, "AppleRS232Serial::nextEvent" );

    return ret;
	
}/* end nextEvent */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::executeEvent
//
//		Inputs:		event - The event
//				data - any data associated with the event
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnNotOpen or kIOReturnBadArgument
//
//		Desc:		Set up for gated executeEvent call.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::executeEvent(UInt32 event, UInt32 data, void *refCon)
{
    IOReturn 	ret;
    
    ELG(event, data, "AppleRS232Serial::executeEvent");
    
    retain();
    ret = fCommandGate->runAction(executeEventAction, (void *)event, (void *)data);
    release();

    return ret;
    
}/* end executeEvent */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::executeEventAction
//
//		Desc:		Dummy pass through for executeEventGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::executeEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    ELG(0, 0, "executeEventAction");
    return ((AppleRS232Serial *)owner)->executeEventGated((UInt32)arg0, (UInt32)arg1);
    
}/* end executeEventAction */


/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::executeEventGated
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

IOReturn AppleRS232Serial::executeEventGated(UInt32 event, UInt32 data)
{
    IOReturn	ret = kIOReturnSuccess;
    UInt32 	state, delta, old;
        
    delta = 0;
    state = fPort.State;	
    ELG(0, state, "AppleRS232Serial::executeEventGated");
	
    if ((state & PD_S_ACQUIRED) == 0)
        return kIOReturnNotOpen;

    switch ( event )
    {
	case PD_RS232_E_XON_BYTE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_RS232_E_XON_BYTE");
            fPort.XONchar = data;
            break;
	case PD_RS232_E_XOFF_BYTE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_RS232_E_XOFF_BYTE");
            fPort.XOFFchar = data;
            break;
	case PD_E_SPECIAL_BYTE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_SPECIAL_BYTE");
            fPort.SWspecial[data >> SPECIAL_SHIFT] |= (1 << (data & SPECIAL_MASK));
            break;
	case PD_E_VALID_DATA_BYTE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_VALID_DATA_BYTE");
            fPort.SWspecial[data >> SPECIAL_SHIFT] &= ~(1 << (data & SPECIAL_MASK));
            break;
	case PD_E_FLOW_CONTROL:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_FLOW_CONTROL");
            old = fPort.FlowControl;				    // save old modes for unblock checks
            fPort.FlowControl = data & (CAN_BE_AUTO | CAN_NOTIFY);  // new values, trimmed to legal values
	    
	    // now cleanup if we've blocked RX or TX with the previous style flow control and we're switching to a different kind
	    // we have 5 different flow control modes to check and unblock; 3 on rx, 2 on tx
	    if (portOpened && old && (old ^ fPort.FlowControl))		// if had some modes, and some modes are different
	    {
		#define SwitchingAwayFrom(flag) ((old & flag) && !(fPort.FlowControl & flag))
		
		// if switching away from rx xon/xoff and we've sent an xoff, unblock
		if (SwitchingAwayFrom(PD_RS232_A_RXO) && fPort.xOffSent)
		{
		    AddBytetoQueue(&(fPort.TX), fPort.XONchar);
		    fPort.xOffSent = false;
		    SetUpTransmit(&fPort);
		}
		
		// if switching away from RTS flow control and we've lowered RTS, need to raise it to unblock
		if (SwitchingAwayFrom(PD_RS232_A_RTS) && !fPort.RTSAsserted)
		{
		    fPort.RTSAsserted = true;
		    SccSetRTS(&fPort, true);			    // raise RTS again
		}

		// if switching away from DTR flow control and we've lowered DTR, need to raise it to unblock
		if (SwitchingAwayFrom(PD_RS232_A_DTR) && !fPort.DTRAsserted)
		{
		    fPort.DTRAsserted = true;
		    SccSetDTR(&fPort, true);			    // raise DTR again
		}
		
		// If switching away from CTS and we've paused tx, continue it
		if (SwitchingAwayFrom(PD_RS232_S_CTS) && fPort.FlowControlState != CONTINUE_SEND)
		{
		    fPort.FlowControlState = CONTINUE_SEND;
		    IODBDMAContinue(fPort.TxDBDMAChannel.dmaBase);		// Continue transfer
		}
		
		// If switching away from TX xon/xoff and we've paused tx, continue it
		if (SwitchingAwayFrom(PD_RS232_S_TXO) && fPort.RXOstate == NEEDS_XON)
		{
		    fPort.RXOstate = NEEDS_XOFF;
		    fPort.FlowControlState = CONTINUE_SEND;
		    IODBDMAContinue(fPort.TxDBDMAChannel.dmaBase);		// Continue transfer
		}
	    }
		
	    break;
	case PD_E_ACTIVE:
            ELG( data, event, "AppleRS232Serial::executeEventGated - PD_E_ACTIVE");
            if ((bool)data)
            {
                if (!(fPort.State & PD_S_ACTIVE))
                {
                    setStructureDefaults(&fPort);
                    setStateGated(PD_S_ACTIVE, PD_S_ACTIVE);
                }
            } else {
                setStateGated(0, PD_S_ACTIVE);				// Clear active state and wake all sleepers
                SccCloseChannel(&fPort);				// Turn the chip off
            }
            break;
	case PD_E_DATA_LATENCY:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_DATA_LATENCY");
            fPort.DataLatInterval = long2tval(data * 1000);
            break;
	case PD_RS232_E_MIN_LATENCY:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_RS232_E_MIN_LATENCY");
            fPort.MinLatency = bool(data);
            //fPort.DLRimage = 0x0000;
            break;
	case PD_E_DATA_INTEGRITY:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_DATA_INTEGRITY");
            if ((data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.TX_Parity = data;
                fPort.RX_Parity = PD_RS232_PARITY_DEFAULT;
		if (!SccSetParity(&fPort, (ParityType)data))
                {
                    ret = kIOReturnBadArgument;
                    ELG(0, 0, "AppleRS232Serial::executeEventGated - SccSetParity failed");
                }
            }
            break;
	case PD_E_DATA_RATE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_DATA_RATE");
            data >>= 1;								// For API compatiblilty with Intel
            ELG(0, data, "AppleRS232Serial::executeEventGated - actual data rate");
            if ((data < MIN_BAUD) || (data > kMaxBaudRate))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.BaudRate = data;
		if (!SccSetBaud(&fPort, data))
                {
                    ret = kIOReturnBadArgument;
                    ELG(0, 0, "AppleRS232Serial::executeEventGated - SccSetBaud failed");
                }			
            }		
            break;
	case PD_E_DATA_SIZE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_DATA_SIZE");
            data >>= 1;								// For API compatiblilty with Intel
            ELG(0, data, "AppleRS232Serial::executeEventGated - actual data size");
            if ((data < 5) || (data > 8))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.CharLength = data;
		if (!SccSetDataBits(&fPort, data))
                {
                    ret = kIOReturnBadArgument;
                    ELG(0, 0, "AppleRS232Serial::executeEventGated - SccSetDataBits failed");
                }			
            }
            break;
	case PD_RS232_E_STOP_BITS:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_RS232_E_STOP_BITS");
            if ((data < 0) || (data > 20))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.StopBits = data;
		if (!SccSetStopBits(&fPort, data))
                {
                    ret = kIOReturnBadArgument;
                    ELG(0, 0, "AppleRS232Serial::executeEventGated - SccSetStopBits failed");
                }
            }
            break;
	case PD_E_RXQ_FLUSH:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_RXQ_FLUSH");
            break;
	case PD_E_RX_DATA_INTEGRITY:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_RX_DATA_INTEGRITY");
            if ((data != PD_RS232_PARITY_DEFAULT) &&  (data != PD_RS232_PARITY_ANY))
            {
                ret = kIOReturnBadArgument;
            } else {
                fPort.RX_Parity = data;
            }
            break;
	case PD_E_RX_DATA_RATE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_RX_DATA_RATE");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_RX_DATA_SIZE:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_RX_DATA_SIZE");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_RS232_E_RX_STOP_BITS:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_RS232_E_RX_STOP_BITS");
            if (data)
            {
                ret = kIOReturnBadArgument;
            }
            break;
	case PD_E_TXQ_FLUSH:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_TXQ_FLUSH");
            break;
	case PD_RS232_E_LINE_BREAK:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_RS232_E_LINE_BREAK");
            state &= ~PD_RS232_S_BRK;
            delta |= PD_RS232_S_BRK;
            if (data)
            {
                fPort.BreakState = true;
            } else {
                fPort.BreakState = false;
            }
            SccSetBreak(&fPort, data);
            setStateGated(state, delta);
            break;
	case PD_E_DELAY:
            ELG(data, event, "AppleRS232Serial::executeEventGated - PD_E_DELAY");
            if (fPort.BreakState)					// It's the break delay in micro seconds
            {
                IOSleep(data/1000);
            } else {
                fPort.CharLatInterval = long2tval(data * 1000);
            }
            break;
	case PD_E_RXQ_SIZE:
            ELG( 0, event, "AppleRS232Serial::executeEventGated - PD_E_RXQ_SIZE" );
            break;
	case PD_E_TXQ_SIZE:
            ELG( 0, event, "AppleRS232Serial::executeEventGated - PD_E_TXQ_SIZE" );
            break;
	case PD_E_RXQ_HIGH_WATER:
            ELG( data, event, "AppleRS232Serial::executeEventGated - PD_E_RXQ_HIGH_WATER" );
            break;
	case PD_E_RXQ_LOW_WATER:
            ELG( data, event, "AppleRS232Serial::executeEventGated - PD_E_RXQ_LOW_WATER" );
            break;
	case PD_E_TXQ_HIGH_WATER:
            ELG( data, event, "AppleRS232Serial::executeEventGated - PD_E_TXQ_HIGH_WATER" );
            break;
	case PD_E_TXQ_LOW_WATER:
            ELG( data, event, "AppleRS232Serial::executeEventGated - PD_E_TXQ_LOW_WATER" );
            break;
	default:
            ELG( data, event, "AppleRS232Serial::executeEventGated - unrecognized event" );
            ret = kIOReturnBadArgument;
            break;
    }

    return ret;
	
}/* end executeEventGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::requestEvent
//
//		Inputs:		event - The event
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument
//				data - any data associated with the event
//
//		Desc:		call requestEventGated through the command gate.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::requestEvent(UInt32 event, UInt32 *data, void *refCon)
{
    IOReturn 	ret;
    
    ELG(event, data, "AppleRS232Serial::requestEvent");
    
    retain();
    ret = fCommandGate->runAction(requestEventAction, (void *)event, (void *)data);
    release();
    
    return ret;
    
}/* end requestEvent */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::requestEventAction
//
//		Desc:		Dummy pass through for requestEventGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::requestEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    ELG(0, 0, "requestEventAction");
    return ((AppleRS232Serial *)owner)->requestEventGated((UInt32)arg0, (UInt32 *)arg1);
    
}/* end requestEventAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::requestEventGated
//
//		Inputs:		event - The event
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument
//				data - any data associated with the event
//
//		Desc:		requestEvent processes the specified event as an immediate request and
//				returns the results in data.  This is primarily used for getting link
//				status information and verifying baud rate and such.
//
//				Queue access requires this be on the command gate.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::requestEventGated(UInt32 event, UInt32 *data)
{
    IOReturn	returnValue = kIOReturnSuccess;

    ELG(0, 0, "AppleRS232Serial::requestEventGated");

    if (data == NULL) 
    {
        ELG(0, event, "AppleRS232Serial::requestEvent - Invalid parameter, data is null");
        returnValue = kIOReturnBadArgument;
    } else {
        switch (event)
        {
            case PD_E_ACTIVE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_ACTIVE");
                *data = bool(getState(&fPort) & PD_S_ACTIVE);	
                break;
            case PD_E_FLOW_CONTROL:
                ELG(fPort.FlowControl, event, "AppleRS232Serial::requestEvent - PD_E_FLOW_CONTROL");
                *data = fPort.FlowControl;							
                break;
            case PD_E_DELAY:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_DELAY");
                *data = tval2long(fPort.CharLatInterval) / 1000;	
                break;
            case PD_E_DATA_LATENCY:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_DATA_LATENCY");
                *data = tval2long(fPort.DataLatInterval) / 1000;	
                break;
            case PD_E_TXQ_SIZE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_TXQ_SIZE");
                *data = GetQueueSize(&fPort.TX);	
                break;
            case PD_E_RXQ_SIZE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_RXQ_SIZE");
                *data = GetQueueSize(&fPort.RX);	
                break;
            case PD_E_TXQ_LOW_WATER:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_TXQ_LOW_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_LOW_WATER:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_RXQ_LOW_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_HIGH_WATER:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_TXQ_HIGH_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_RXQ_HIGH_WATER:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_RXQ_HIGH_WATER");
                *data = 0; 
                returnValue = kIOReturnBadArgument; 
                break;
            case PD_E_TXQ_AVAILABLE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_TXQ_AVAILABLE");
                *data = FreeSpaceinQueue(&fPort.TX);	 
                break;
            case PD_E_RXQ_AVAILABLE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_RXQ_AVAILABLE");
                *data = UsedSpaceinQueue(&fPort.RX); 	
                break;
            case PD_E_DATA_RATE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_DATA_RATE");
                *data = fPort.BaudRate << 1;		
                break;
            case PD_E_RX_DATA_RATE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_RX_DATA_RATE");
                *data = 0x00;					
                break;
            case PD_E_DATA_SIZE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_DATA_SIZE");
                *data = fPort.CharLength << 1;	
                break;
            case PD_E_RX_DATA_SIZE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_RX_DATA_SIZE");
                *data = 0x00;					
                break;
            case PD_E_DATA_INTEGRITY:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_DATA_INTEGRITY");
                *data = fPort.TX_Parity;			
                break;
            case PD_E_RX_DATA_INTEGRITY:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_E_RX_DATA_INTEGRITY");
                *data = fPort.RX_Parity;			
                break;
            case PD_RS232_E_STOP_BITS:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_RS232_E_STOP_BITS");
                *data = fPort.StopBits << 1;		
                break;
            case PD_RS232_E_RX_STOP_BITS:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_RS232_E_RX_STOP_BITS");
                *data = 0x00;					
                break;
            case PD_RS232_E_XON_BYTE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_RS232_E_XON_BYTE");
                *data = fPort.XONchar;			
                break;
            case PD_RS232_E_XOFF_BYTE:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_RS232_E_XOFF_BYTE");
                *data = fPort.XOFFchar;			
                break;
            case PD_RS232_E_LINE_BREAK:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_RS232_E_LINE_BREAK");
                *data = bool(getState(&fPort) & PD_RS232_S_BRK);
                break;
            case PD_RS232_E_MIN_LATENCY:
                ELG(0, event, "AppleRS232Serial::requestEvent - PD_RS232_E_MIN_LATENCY");
                *data = bool(fPort.MinLatency);		
                break;
            default:
                ELG(0, event, "AppleRS232Serial::requestEvent - unrecognized event");
                returnValue = kIOReturnBadArgument; 			
                break;
        }
    }

    return kIOReturnSuccess;
	
}/* end requestEventGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::enqueueEvent
//
//		Inputs:		event - The event
//				data - any data associated with the event, 
//				sleep - true (wait for it), false (don't)
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Not used by this driver.
//				Events are passed on to executeEvent for immediate action.	
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon)
{
    IOReturn 	ret;
    
    ELG(data, event, "AppleRS232Serial::enqueueEvent");
    
    retain();
    ret = fCommandGate->runAction(enqueueEventAction, (void *)event, (void *)data);
    release();

    return ret;
	
}/* end enqueueEvent */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::enqueueEventAction
//
//		Desc:		Dummy pass through for executeEventGated (events are not queued).
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::enqueueEventAction(OSObject *owner, void *arg0, void *arg1, void *, void *)
{
    ELG(0, 0, "enqueueEventAction");
    return ((AppleRS232Serial *)owner)->executeEventGated((UInt32)arg0, (UInt32)arg1);
    
}/* end enqueueEventAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::dequeueEvent
//
//		Inputs:		event - the event to queue
//				data - associated data	
//				sleep - true (wait for it), false (don't)
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess or kIOReturnNotOpen
//
//		Desc:		Not used by this driver (no events are queued).		
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon)
{
	
    ELG(0, 0, "AppleRS232Serial::dequeueEvent");

    if ((event == NULL) || (data == NULL))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
	
}/* end dequeueEvent */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::enqueueData
//
//		Inputs:		buffer - the data
//				size - number of bytes
//				sleep - true (wait for it), false (don't)
//				refCon - the Port (not used)
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument or value returned from watchState
//				count - bytes transferred  
//
//		Desc:		set up for enqueueDataGated call.	
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon)
{
    IOReturn 	ret;

    ELG(0, sleep, "AppleRS232Serial::enqueueData");

    if (count == NULL || buffer == NULL)
        return kIOReturnBadArgument;
        
    retain();
    ret = fCommandGate->runAction(enqueueDataAction, (void *)buffer, (void *)size, (void *)count, (void *)sleep);
    release();

    return ret;
        
}/* end enqueueData */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::enqueueDatatAction
//
//		Desc:		Dummy pass through for equeueDataGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::enqueueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    ELG(0, 0, "enqueueDataAction");
    return ((AppleRS232Serial *)owner)->enqueueDataGated((UInt8 *)arg0, (UInt32)arg1, (UInt32 *)arg2, (bool)arg3);
    
}/* end enqueueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::enqueueDataGated
//
//		Inputs:		buffer - the data
//				size - number of bytes
//				sleep - true (wait for it), false (don't)
//
//		Outputs:	Return Code - kIOReturnSuccess, kIOReturnBadArgument or value returned from watchState
//				count - bytes transferred  
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
//				arguments were invalid.	
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::enqueueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
    UInt32 	state = PD_S_TXQ_LOW_WATER;
    IOReturn 	rtn = kIOReturnSuccess;

    ELG(0, sleep, "AppleRS232Serial::enqueueDataGated");

    *count = 0;

    if (!(fPort.State & PD_S_ACTIVE))
        return kIOReturnNotOpen;

        // OK, go ahead and try to add something to the buffer
        
    *count = AddtoQueue(&fPort.TX, buffer, size);
    CheckQueues(&fPort);
    
    if (fPort.FlowControlState == PAUSE_SEND)
    {
        return kIOReturnSuccess;
    }

        // Let the tranmitter know that we have something ready to go
    
    SetUpTransmit(&fPort);

        // If we could not queue up all of the data on the first pass and
        // the user wants us to sleep until it's all out then sleep

    while ((*count < size) && sleep)
    {
        state = PD_S_TXQ_LOW_WATER;
        rtn = watchStateGated(&state, PD_S_TXQ_LOW_WATER);
        if ( rtn != kIOReturnSuccess )
        {
            ELG( 0, rtn, "AppleRS232Serial::enqueueDataGated - interrupted" );
            return rtn;
        }

        *count += AddtoQueue(&fPort.TX, buffer + *count, size - *count);
        CheckQueues(&fPort);

            // Let the tranmitter know that we have something ready to go again.

        SetUpTransmit(&fPort);
    }

    ELG(*count, size, "AppleRS232Serial::enqueueDataGated - Count on exit");

    return kIOReturnSuccess;
	
}/* end enqueueDataGated */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::dequeueData
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

IOReturn AppleRS232Serial::dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon)
{
    IOReturn 	ret;

    ELG(0, 0, "AppleRS232Serial::dequeueData");

    if ((count == NULL) || (buffer == NULL) || (min > size))
        return kIOReturnBadArgument;
        
    retain();
    ret = fCommandGate->runAction(dequeueDataAction, (void *)buffer, (void *)size, (void *)count, (void *)min);
    release();

    return ret;


}/* end dequeueData */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::dequeueDatatAction
//
//		Desc:		Dummy pass through for equeueDataGated.
//
/****************************************************************************************************/

IOReturn AppleRS232Serial::dequeueDataAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
    ELG(0, 0, "dequeueDataAction");
    return ((AppleRS232Serial *)owner)->dequeueDataGated((UInt8 *)arg0, (UInt32)arg1, (UInt32 *)arg2, (UInt32)arg3);
    
}/* end dequeueDataAction */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::dequeueDataGated
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

IOReturn AppleRS232Serial::dequeueDataGated(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
{
    IOReturn 	rtn = kIOReturnSuccess;
    UInt32 	state = 0;

    ELG(size, min, "AppleRS232Serial::dequeueDataGated");
	
        // If the port is not active then there should not be any chars.
        
    *count = 0;
    if (!(fPort.State & PD_S_ACTIVE))
        return kIOReturnNotOpen;

        // Get any data living in the queue.
        
    *count = RemovefromQueue(&fPort.RX, buffer, size);
    CheckQueues(&fPort);

    while ((min > 0) && (*count < min))
    {
            // Figure out how many bytes we have left to queue up
            
        state = 0;

        rtn = watchStateGated(&state, PD_S_RXQ_EMPTY);

        if (rtn != kIOReturnSuccess)
        {
            ELG(0, rtn, "AppleRS232Serial::dequeueData - Interrupted!");
            return rtn;
        }
        
            // Try and get more data starting from where we left off
            
        *count += RemovefromQueue(&fPort.RX, buffer + *count, (size - *count));
        CheckQueues(&fPort);
		
    }
    
    LogData(kSerialIn, *count, buffer);

    ELG(*count, size, "AppleRS232Serial::dequeueData - Count on exit");

    return rtn;
	
}/* end dequeueData */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::CheckQueues
//
//		Inputs:		port - The port
//
//		Outputs:	
//
//		Desc:		Checks the various queue's etc and manipulates the state(s) accordingly.
//				Must be called from a gated method.
//
/****************************************************************************************************/

void AppleRS232Serial::CheckQueues(PortInfo_t *port)
{
    UInt32	Used;
    UInt32	Free;
    UInt32	OldState;
    UInt32	DeltaState;
    UInt32	SW_FlowControl;
    UInt32	RTS_FlowControl;
    UInt32	DTR_FlowControl;
    
    ELG(0, 0, "CheckQueues");

    // This lock is the fix for 2977847.  The PD_S_TX_BUSY flag was getting cleared during
    // the execution of this routine by tx interrupt code, and but then this routine was
    // incorrectly restoring OldState (and the PD_S_TX_BUSY bit) at exit.

    OldState = port->State;

        // Check to see if there is anything in the Transmit queue
        
    Used = UsedSpaceinQueue(&(port->TX));
    Free = FreeSpaceinQueue(&(port->TX));

    if (Free == 0) {
        OldState |= PD_S_TXQ_FULL;
        OldState &= ~PD_S_TXQ_EMPTY;
    } else {
        if (Used == 0) 
        {
            OldState &= ~PD_S_TXQ_FULL;
            OldState |= PD_S_TXQ_EMPTY;
        } else { 
            OldState &= ~PD_S_TXQ_FULL;
            OldState &= ~PD_S_TXQ_EMPTY;
        }
    }
    
        // Check to see if we are below the low water mark

    if (Used < port->TXStats.LowWater)
    {
        OldState |= PD_S_TXQ_LOW_WATER;
    } else {
        OldState &= ~PD_S_TXQ_LOW_WATER;
    }

    if (Used > port->TXStats.HighWater)
    {
        OldState |= PD_S_TXQ_HIGH_WATER;
    } else {
        OldState &= ~PD_S_TXQ_HIGH_WATER;
    }

        // Check to see if there is anything in the Receive queue

    Used = UsedSpaceinQueue(&(port->RX));
    Free = FreeSpaceinQueue(&(port->RX));

    if (Free == 0)
    {
        OldState |= PD_S_RXQ_FULL;
        OldState &= ~PD_S_RXQ_EMPTY;
    } else {
        if (Used == 0)
        {
            OldState &= ~PD_S_RXQ_FULL;
            OldState |= PD_S_RXQ_EMPTY;
        } else {
            OldState &= ~PD_S_RXQ_FULL;
            OldState &= ~PD_S_RXQ_EMPTY;
        }
    }

    SW_FlowControl = port->FlowControl & PD_RS232_A_RXO;
    RTS_FlowControl = port->FlowControl & PD_RS232_A_RTS;
    DTR_FlowControl = port->FlowControl & PD_RS232_A_DTR;

        // Check to see if we are below the low water mark

    if (Used < port->RXStats.LowWater)			    // if under low water mark, release any active flow control
    {
        if ((SW_FlowControl) && (port->xOffSent))	    // unblock xon/xoff flow control
        {
            port->xOffSent = false;
            AddBytetoQueue(&(port->TX), port->XONchar);
            SetUpTransmit(port);
        }
	if (RTS_FlowControl && !port->RTSAsserted)	    // unblock RTS flow control
        {
            port->RTSAsserted = true;
            SccSetRTS(port, true);
        }	
	if (DTR_FlowControl && !port->DTRAsserted)	    // unblock DTR flow control
        {
            port->DTRAsserted = true;
            SccSetDTR(port, true);
        }	
        OldState |= PD_S_RXQ_LOW_WATER;
    } else {
        OldState &= ~PD_S_RXQ_LOW_WATER;
    }

        // Check to see if we are above the high water mark
        
    if (Used > port->RXStats.HighWater)			    // if over highwater mark, block w/any flow control thats enabled
    {
        if ((SW_FlowControl) && (!port->xOffSent))
        {
            port->xOffSent = true;
            AddBytetoQueue(&(port->TX), port->XOFFchar);
            SetUpTransmit(port);
        }

        if (RTS_FlowControl && port->RTSAsserted)
        {
	    port->RTSAsserted = false;
	    SccSetRTS(port, false);			    // lower RTS to hold back more rx data
        }

        if (DTR_FlowControl && port->DTRAsserted)
        {
            port->DTRAsserted = false;
            SccSetDTR(port, false);
        }
        OldState |= PD_S_RXQ_HIGH_WATER;
    } else {
        OldState &= ~PD_S_RXQ_HIGH_WATER;
	port->aboveRxHighWater = false;
    }
    
        // Figure out what has changed to get mask
        
    DeltaState = OldState ^ port->State;
    setStateGated(OldState, DeltaState);
    
}/* end CheckQueues */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::createSerialStream
//
//		Inputs:		provider - Our provider
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the nub
//
/****************************************************************************************************/

bool AppleRS232Serial::createSerialStream(IOService *provider)
{
    IORS232SerialStreamSync	*pNub = new IORS232SerialStreamSync;
    bool 			ret;

    ELG(0, 0, "AppleRS232Serial::createSerialStream");

    if (!pNub)
    {
        ELG( 0, 0, "AppleRS232Serial::createSerialStream - Create nub failed" );
        return false;
    }
    
        // Either we attached and should get rid of our reference
    	// or we failed in which case we should get rid our reference as well.
        // This just makes sure the reference count is correct.
    
    ret = (pNub->init(0, &fPort) && pNub->attach(this));
    
    pNub->release();
    
    if (!ret)
    {
        ALERT( ret, 0, "AppleRS232Serial::createSerialStream - Didn't attach to the nub properly" );
        return false;
    }
    
        // Set the name for this port and register it
        
    pNub->setProperty(kIOTTYBaseNameKey, "serial");
        
    pNub->registerService();

    return true;
    
}/* end createSerialStream */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::initializePort
//
//		Inputs:		port - the port
//
//		Outputs:	return code - true(initialized), false(it didn't)
//
//		Desc:		Initializes the port
//
/****************************************************************************************************/

bool AppleRS232Serial::initializePort(PortInfo_t *port)
{
    SerialDBDMAStatusInfo	*dmaInfo;
    IODBDMADescriptor		*poolBase;
    IODBDMADescriptor		*poolPhysical;
    IOByteCount			temp;
    int i;
    
    ELG(0, sizeof(IODBDMADescriptor), "AppleRS232Serial::initializePort");
    
    // allocate the shared dbdma command pool
    {
	UInt32 bytes_needed = sizeof(IODBDMADescriptor) * (2*kNumberOfRxDBDMACommands + kNumberOfTxDBDMACommands);
	port->dmaChannelCommandAreaMDP = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous,	// options
									       bytes_needed,			// size
									       sizeof(IODBDMADescriptor));	// alignment, needs what?
	if (port->dmaChannelCommandAreaMDP == NULL)
	{
	    ALERT(0, 0, "AppleRS232Serial::initializePort - DBDMA command descriptor pool not allocated");
	    return false;
	}
	port->dmaChannelCommandAreaMDP->prepare();
	poolBase     = (IODBDMADescriptor *)port->dmaChannelCommandAreaMDP->getBytesNoCopy();
	poolPhysical = (IODBDMADescriptor *)port->dmaChannelCommandAreaMDP->getPhysicalSegment(0, &temp);
	if (temp < bytes_needed) return false;
	bzero(poolBase, bytes_needed);
    }
    
    
    // Set up the transmission channel
        
    dmaInfo = &port->TxDBDMAChannel;
    ELG(0, dmaInfo, "AppleRS232Serial::initializePort - dmaInfo for TX");
        
    // allocate the transmit Command Area buffer from the pool
    
    dmaInfo->dmaChannelCommandArea         = poolBase;
    dmaInfo->dmaChannelCommandAreaPhysical = poolPhysical;
    dmaInfo->dmaNumberOfDescriptors = kNumberOfTxDBDMACommands;		
    poolBase	 += dmaInfo->dmaNumberOfDescriptors;
    poolPhysical += dmaInfo->dmaNumberOfDescriptors;
    ELG(dmaInfo->dmaChannelCommandArea, kNumberOfTxDBDMACommands, "AppleRS232Serial::initializePort - TX DMA Channel Command address, count");
    
    // Create the transmit buffer

    dmaInfo->dmaTransferBufferMDP = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kTxDBDMABufferSize, 1);
    if (dmaInfo->dmaTransferBufferMDP == NULL)
    {
        ALERT(0, kTxDBDMABufferSize, "AppleRS232Serial::initializePort - TX DMA Transfer buffer descriptor not allocated");
        return false;
    }
    dmaInfo->dmaTransferBufferMDP->prepare();
    dmaInfo->dmaTransferBuffer = (UInt8 *)dmaInfo->dmaTransferBufferMDP->getBytesNoCopy();
    bzero(dmaInfo->dmaTransferBuffer, kTxDBDMABufferSize);
    dmaInfo->dmaTransferSize = 0;
    ELG(0, dmaInfo->dmaTransferBufferMDP, "AppleRS232Serial::initializePort - TX DMA transfer buffer MDP");
    ELG(dmaInfo->dmaTransferBuffer, kTxDBDMABufferSize, "AppleRS232Serial::initializePort - TX DMA transfer buffer and size");
 

    // Set up the reception channelsl, two of them now
    
    for (i = 0; i < 2; i++) {
        
	dmaInfo = &port->rxDBDMAChannels[i];
	ELG(i, dmaInfo, "AppleRS232Serial::initializePort - dmaInfo for RX");
    
	// allocate the receive Command Area buffer from the pool

	dmaInfo->dmaChannelCommandArea         = poolBase;
	dmaInfo->dmaChannelCommandAreaPhysical = poolPhysical;
	dmaInfo->dmaNumberOfDescriptors = kNumberOfRxDBDMACommands;
	poolBase     += dmaInfo->dmaNumberOfDescriptors;
	poolPhysical += dmaInfo->dmaNumberOfDescriptors;
	ELG(dmaInfo->dmaChannelCommandArea, kNumberOfRxDBDMACommands, "AppleRS232Serial::initializePort - RX DMA Channel Command address, count");
        
	// Create the receive buffer
    
	dmaInfo->dmaTransferBufferMDP = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kRxDBDMABufferSize, 1);
	if (dmaInfo->dmaTransferBufferMDP == NULL)
	{
	    ALERT(0, i, "AppleRS232Serial::initializePort - RX DMA Transfer buffer descriptor not allocated");
	    return false;
	}
	dmaInfo->dmaTransferBufferMDP->prepare();
	dmaInfo->dmaTransferBuffer = (UInt8 *)dmaInfo->dmaTransferBufferMDP->getBytesNoCopy();
	bzero(dmaInfo->dmaTransferBuffer, kRxDBDMABufferSize);
	dmaInfo->dmaTransferSize = 0;
    
	ELG(i, dmaInfo->dmaTransferBufferMDP, "AppleRS232Serial::initializePort - RX DMA transfer buffer MDP");
	ELG(dmaInfo->dmaTransferBuffer, dmaInfo->dmaNumberOfDescriptors, "AppleRS232Serial::initializePort - RX DMA transfer buffer and size");
    }
    port->activeRxChannelIndex = 0;

    port->DataLatInterval.tv_sec = 0;
    port->DataLatInterval.tv_nsec = 0;
    port->CharLatInterval.tv_sec = 0;
    port->CharLatInterval.tv_nsec = 0;
    port->State = (PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER);
    port->WatchStateMask = 0x00000000;
    
    return true;

}/* end initializePort */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::SetStructureDefaults
//
//		Inputs:		port - The port
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the nub
//
/****************************************************************************************************/

void AppleRS232Serial::setStructureDefaults(PortInfo_t *port)
{
    UInt32	tmp;
    
    ELG(0, 0, "AppleRS232Serial::setStructureDefaults");

    port->BaudRate = 0;
    port->CharLength = 0;
    port->StopBits = 0;
    port->TX_Parity = 0;
    port->RX_Parity = 0;
    port->MinLatency = false;
    port->XONchar = '\x11';
    port->XOFFchar = '\x13';
    port->FlowControl = 0x00000000;
    port->FlowControlState = CONTINUE_SEND;
    port->RXOstate = IDLE_XO;
    port->TXOstate = IDLE_XO;
    
    port->xOffSent = false;
    port->RTSAsserted = true;
    port->DTRAsserted = true;
    

    port->RXStats.BufferSize = BUFFER_SIZE_DEFAULT;
    port->RXStats.HighWater = (port->RXStats.BufferSize << 1) / 3;
    port->RXStats.LowWater = port->RXStats.HighWater >> 1;

    port->TXStats.BufferSize = BUFFER_SIZE_DEFAULT;
    port->TXStats.HighWater = (port->RXStats.BufferSize << 1) / 3;
    port->TXStats.LowWater = port->RXStats.HighWater >> 1;

    port->FlowControl = DEFAULT_NOTIFY;

    port->AreTransmitting = FALSE;

    for (tmp=0; tmp<(256>>SPECIAL_SHIFT); tmp++)
    {
	port->SWspecial[tmp] = 0;
    }

    //port->Stats.ints = 0;
    //port->Stats.txInts = 0;
    //port->Stats.rxInts = 0;
    //port->Stats.mdmInts = 0;
    //port->Stats.txChars = 0;
    //port->Stats.rxChars = 0;
    port->DCDState = false;
    port->BreakState = false;
    
    port->lastCTSTime = 0;
    port->ctsTransitionCount = 0;
    
}/* end setStructureDefaults */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::freeRingBuffer
//
//		Inputs:		Queue - the specified queue to free
//
//		Outputs:	
//
//		Desc:		Frees the resources assocated with the queue
//
/****************************************************************************************************/

void AppleRS232Serial::freeRingBuffer(CirQueue *Queue)
{
    ELG(0, Queue, "AppleRS232Serial::freeRingBuffer");

    if (Queue->Start)
    {
        IOFree(Queue->Start, Queue->Size);
        CloseQueue(Queue);
    }
	
}/* end freeRingBuffer */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::allocateRingBuffer
//
//		Inputs:		Queue - the specified queue to allocate
//				BufferSize - size to allocate
//
//		Outputs:	return Code - true(buffer allocated), false(it failed)
//
//		Desc:		Allocates resources needed by the queue 
//
/****************************************************************************************************/

bool AppleRS232Serial::allocateRingBuffer(CirQueue *Queue, size_t BufferSize, IOWorkLoop *workloop)
{
    UInt8	*Buffer;
		
    ELG(0, BufferSize, "AppleRS232Serial::allocateRingBuffer");
    
    Buffer = (UInt8*)IOMalloc(BufferSize);

    InitQueue(Queue, Buffer, BufferSize, workloop);

    if (Buffer)
        return true;

    return false;
	
}/* end allocateRingBuffer */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::shutDown
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Clean up and release everything.
//
/****************************************************************************************************/

void AppleRS232Serial::shutDown()
{

    ELG(0, 0, "AppleRS232Serial::shutDown");

        // Disable the dma channles
        
    SccFreeReceptionChannel(&fPort, 0);
    SccFreeReceptionChannel(&fPort, 1);
    SccFreeTansmissionChannel(&fPort);
    
    if (fPort.dmaChannelCommandAreaMDP != NULL)
    {
	fPort.dmaChannelCommandAreaMDP->complete();
	fPort.dmaChannelCommandAreaMDP->release();
	fPort.dmaChannelCommandAreaMDP = NULL;
    }
    

    if (txDMAInterruptSource)
    {
        fWorkLoop->removeEventSource(txDMAInterruptSource);
        txDMAInterruptSource->release();
        txDMAInterruptSource = 0;
    }

    if (rxDMAInterruptSource)
    {
        fWorkLoop->removeEventSource(rxDMAInterruptSource);
        rxDMAInterruptSource->release();
        rxDMAInterruptSource = 0;
    }

    if (sccInterruptSource)
    {
        fWorkLoop->removeEventSource(sccInterruptSource);
        sccInterruptSource->release();
        sccInterruptSource = 0;
    }

    if(fPort.rxTimer)
    {
        fPort.rxTimer->cancelTimeout();						// stop the timer
        fWorkLoop->removeEventSource(fPort.rxTimer );				// remove the timer from the workloop
        fPort.rxTimer->release();						// release the timer
        fPort.rxTimer = NULL;
    }

        // Turn the chip off after closing the port
        
    if (fPort.ControlRegister != NULL)
    {
        SccCloseChannel(&fPort);
    }

    	// Begin fixed bug # 2550140 & 2553750
     
    //if (fPort.IODBDMARxLock)
    //{
    //    IOLockFree(fPort.IODBDMARxLock);
    //    fPort.IODBDMARxLock = NULL;
    //}
    //if (fPort.IODBDMATrLock)
    //{
    //    IOLockFree(fPort.IODBDMATrLock);
    //    fPort.IODBDMATrLock = NULL;
    //}
    
	// End fixed bug # 2550140 & 2553750
    
    //if (fPort.SCCAccessLock)
    //{
    //    IOLockFree(fPort.SCCAccessLock);
    //    fPort.SCCAccessLock = NULL;
    //}
    
    freeRingBuffer(&(fPort.TX));
    freeRingBuffer(&(fPort.RX));
    
    if (fCommandGate)
    {
        fCommandGate->release();
        fCommandGate = NULL;
    }
    if (fWorkLoop)
    {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }
        
	//rcs TBD??? We may need to incorporate the AppleSCCSerial::stop semantics where we explicitly check to see that the
	//thread is not running? 

	if (fdmaStartTransmissionThread != NULL)
	{
			thread_call_free(fdmaStartTransmissionThread);
			fdmaStartTransmissionThread = NULL;
	}
		
	if (dmaRxHandleCurrentPositionThread != NULL)
	{
		thread_call_free(dmaRxHandleCurrentPositionThread);
		dmaRxHandleCurrentPositionThread = NULL;
	}



}/* end shutDown */


/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::interruptFilter
//
//		Inputs:		obj - Unused
//				source - Where it came from
//
//		Outputs:	
//
//		Desc:		Interrupt filter
//
/****************************************************************************************************/
bool AppleRS232Serial::interruptFilter(OSObject* obj, IOFilterInterruptEventSource *source)
{
    UInt16	interruptIndex;
    
//    ELG(0, 0, "AppleRS232Serial::interruptFilter");

        // check if this interrupt is ours
        
    interruptIndex = source->getIntIndex();
    if (interruptIndex == kIntRxDMA)
    {
//        ELG(0, 0, "AppleRS232Serial::interruptFilter - RX DMA");
        return true;							// Go ahead and invoke completion routine
    }
	
    if (interruptIndex == kIntTxDMA)
    {
//        ELG(0, 0, "AppleRS232Serial::interruptFilter - TX DMA");
        return true;							// go ahead and invoke completion routine
    }
	
//    ELG(0, 0, "AppleRS232Serial::interruptFilter - Not my interrupt");

    return false;
    
}/* end interruptFilter */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::interruptHandler
//
//		Inputs:		obj - Should be me (just passed on)
//				source - Where it came from
//				count - Unused
//
//		Outputs:	
//
//		Desc:		Initial interrupt handler
//
/****************************************************************************************************/
void AppleRS232Serial::interruptHandler(OSObject* obj, IOInterruptEventSource *source, int count)
{
    UInt16	interruptIndex;
    
    ELG(0, obj, "AppleRS232Serial::interruptHandler");

    interruptIndex = source->getIntIndex();	
    if (interruptIndex == kIntChipSet)
    {
        ELG(0, 0, "AppleRS232Serial::interruptHandler - SCC interrupt");
        handleInterrupt(obj, 0, NULL, 0);
    }
		
    if (interruptIndex == kIntRxDMA)
    {
        ELG(0, 0, "AppleRS232Serial::interruptHandler - RX DMA interrupt");
        handleDBDMARxInterrupt(obj, 0, NULL, 0);
    }
		
    if (interruptIndex == kIntTxDMA)
    {
        ELG(0, 0, "AppleRS232Serial::interruptHandler - TX DMA interrupt");
        handleDBDMATxInterrupt(obj, 0, NULL, 0);
    }
    
}/* end interruptHandler */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::handleInterrupt
//
//		Inputs:		target - Should be me
//				refCon - Just passed on
//				nub - Unused
//				source - Unused (but probably me)
//
//		Outputs:	
//
//		Desc:		Interrupt handler
//
/****************************************************************************************************/

void AppleRS232Serial::handleInterrupt(OSObject *target, void *refCon, IOService *nub, int source)
{
    AppleRS232Serial *serialPortPtr = (AppleRS232Serial *)target;

    ELG(0, serialPortPtr->fWorkLoop->inGate(), "AppleRS232Serial::handleInterrupt, inGate");
    
    if (serialPortPtr != NULL) 
    {
        PPCSerialISR(target, refCon, &serialPortPtr->fPort);
    }
    
}/* end handleInterrupt */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::handleDBDMATxInterrupt
//
//		Inputs:		target - Should be me
//				refCon - Unused
//				nub - Unused
//				source - Unused
//
//		Outputs:	
//
//		Desc:		DMA TX Interrupt handler
//
/****************************************************************************************************/

void AppleRS232Serial::handleDBDMATxInterrupt(OSObject *target, void *refCon, IOService *nub, int source)
{
    AppleRS232Serial *serialPortPtr = OSDynamicCast(AppleRS232Serial, target);

    ELG(serialPortPtr, serialPortPtr->fWorkLoop->inGate(), "AppleRS232Serial::handleDBDMATxInterrupt, obj, inGate");
    
    if (serialPortPtr != NULL)
    {
        PPCSerialTxDMAISR(serialPortPtr, NULL,  &serialPortPtr->fPort);
    }
    
}/* end handleDBDMATxInterrupt */

/****************************************************************************************************/
//
//		Method:		AppleRS232Serial::handleDBDMARxInterrupt
//
//		Inputs:		target - Should be me
//				refCon - Unused
//				nub - Unused
//				source - Unused (but probably me)
//
//		Outputs:	
//
//		Desc:		DMA TX Interrupt handler
//
/****************************************************************************************************/

void AppleRS232Serial::handleDBDMARxInterrupt(OSObject *target, void *refCon, IOService *nub, int source)
{
    AppleRS232Serial *serialPortPtr = OSDynamicCast(AppleRS232Serial, target);
    
    ELG(serialPortPtr, serialPortPtr->fWorkLoop->inGate(), "AppleRS232Serial::handleDBDMARxInterrupt, obj, inGate");

    if (serialPortPtr != NULL)
    {
        PPCSerialRxDMAISR(serialPortPtr, NULL,  &serialPortPtr->fPort);
    }
    
}/* end handleDBDMARxInterrupt */

