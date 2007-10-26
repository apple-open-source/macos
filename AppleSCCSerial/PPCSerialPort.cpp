/*
 *Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 *AppleSCCSerial.cpp
 *
 *MacOSX implementation of Serial Port driver
 *
 *
 *Humphrey Looney	MacOSX IOKit ppc
 *Elias Keshishoglou 	MacOSX Server ppc
 *Dean Reece		Original Next Intel  version
 *
 * 18-Apr-01	David Laupmanis	Update executeEvent to recognize MIDI clock mode, and call
 *								SccConfigureForMIDI if MIDI clock mode is active
 *
 * 26-Feb-01    Paul Sun 	allocateRingBuffer(), now uses passed in buffer size which is default to BUFFER_SIZE_DEFAULT
 *
 * 07-Feb-01	Paul Sun	Implemented hardware and software flow control.
 *
 * 22-Jan-01	Paul Sun	Fixed bug # 2560437 -- took out the enableScc() function
 *				which is now supported by PlatformExpert.
 *				Fixed bug # 2606888 -- added code to AppleSCCSerial::start()
 *				function for not setting 'irda' port on all the machines.
 *
 *
 *Copyright ©: 	1999 Apple Computer, Inc.  all rights reserved.
 */
#include <machine/limits.h>			/* UINT_MAX */
#include <IOKit/assert.h>

#include <sys/kdebug.h>

#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IORegistryEntry.h>


#include "PPCSerialPort.h"
#include "SccChipPrimatives.h"

#define IOSS_HALFBIT_BRD 1 
//#define USE_TIMER_EVENT_SOURCE_DEBUGGING 1
//#define kTimerTimeout 3000

//#define SHOW_DEBUG_STRINGS  1	
#ifdef  SHOW_DEBUG_STRINGS
#define DLOG(fmt, args...)      IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

extern void flush_dcache(vm_offset_t addr, unsigned count, int phys);
extern UInt32 gTimerCanceled;

// This is to check the status of the debug flags
#include <pexpert/pexpert.h>

class AppleSCCRS232SerialStreamSync : public IORS232SerialStreamSync
{
    OSDeclareDefaultStructors(AppleSCCRS232SerialStreamSync)

public:
    virtual bool compareName(OSString *name, OSString **matched = 0) const;
};

OSDefineMetaClassAndStructors
    (AppleSCCRS232SerialStreamSync, IORS232SerialStreamSync)

// Steal Device Tree matching from the AppleMacIODevice compareName function.
bool AppleSCCRS232SerialStreamSync::
compareName(OSString *name, OSString ** matched) const
{
    return IODTCompareNubName(this, name, matched)
        || IORegistryEntry::compareName(name, matched);
}

OSDefineMetaClassAndStructors(AppleSCCSerial, IOSerialDriverSync)

#define super IOSerialDriverSync

/*
 *AppleSCCSerial::start
 *note returning false currently causes a crash
 */
bool AppleSCCSerial::start(IOService *provider)
{
    IOMemoryMap *map;

    if (!super::start(provider))
        return false;

    fProvider = OSDynamicCast(AppleMacIODevice, provider);
    if (!fProvider)
        return false;

    // get our workloop
    myWorkLoop = getWorkLoop();

    if (!myWorkLoop)
    {
		return false;
    }
	
    fCommandGate = IOCommandGate::commandGate(this);
    if (!fCommandGate)
    {
		return false;
    }


    if (myWorkLoop->addEventSource(fCommandGate) != kIOReturnSuccess)
    {
        return false;
    }

    fCommandGate->enable();

    
#if USE_TIMER_EVENT_SOURCE_DEBUGGING        
		// get a timer and set our timeout handler to be called when it fires
		myTimer = IOTimerEventSource::timerEventSource( this, (IOTimerEventSource::Action)&AppleSCCSerial::timeoutHandler );
		
		// make sure we got a timer
		if( !myTimer )
		{
			//IOLog( "%s: Failed to create timer event source\n", getName() );
			result = false;
			return result;
		}

		// add the timer to the workloop
		if(!myWorkLoop || (myWorkLoop->addEventSource(myTimer) != kIOReturnSuccess) )
		{
			//IOLog( "%s: Failed to add timer event source to workloop\n", getName() );
			result = false;
			return result;
		}

		// now set the timeout
		// this example uses 1 seconds
//		myTimer->setTimeoutMS( kTimerTimeout );

		// zero counter
		counter = 0;	// this is used to count the number of times that timeoutHandler is called
#endif;

    // Never, never match if the user is debugging with kprintf:
    {
        UInt32	debugFlags;
        if (!PE_parse_boot_arg("debug", &debugFlags))
            debugFlags = 0;

        if (debugFlags & 8)
            return NULL;
    }

    // Figure out what kind of serial device nub to make.
    // first check if the parent is escc or escc-legacy
    // don't need to match on escc-legacy nodes
    {
        IORegistryEntry *parentEntry = provider->getParentEntry(gIODTPlane);
        if (!parentEntry || IODTMatchNubWithKeys(parentEntry, "'escc-legacy'"))
            return false;
    }
    
    {
        OSString *matched = (OSString *) getProperty(gIONameMatchedKey);
        if (matched->isEqualTo("ch-a"))
            Port.whichPort = serialPortA;
        else if (matched->isEqualTo("ch-b"))
            Port.whichPort = serialPortB;
        else
            return false;
    }

	// for DCP -- begin
	// need to do the DCP stuff for port a only
	// find out is the modem a DCP modem or not
	if (Port.whichPort == serialPortA)
	{
//		IOLog ("AppleSCCSerial::start -- Calling LookForInternalModem\n");
		Port.gDCPModemFound = LookForInternalModem (kDCPModem);
/*
		if (Port.gDCPModemFound)
		{
			IOLog ("AppleSCCSerial::start -- Found DCP modem\n");
		}
		else
		{
			IOLog ("AppleSCCSerial::start -- Did not find DCP modem\n");
		}
	}
	else
	{
		IOLog ("AppleSCCSerial::start -- It is not for serial Port A\n");
*/
	}
	// for DCP -- end

    // sets the name for this port:
    setPortName(provider);
	
	OSString *name = OSDynamicCast(OSString, getProperty(kIOTTYBaseNameKey));
	if ((name != NULL) && (name->isEqualTo("none")))
		return false;
	
	/* Begin fixed bug # 2550140 & 2553750 */
	/* init the DBDMA memory lock */
	Port.IODBDMARxLock = IOLockAlloc();
	Port.IODBDMATrLock = IOLockAlloc();
	/* End fixed bug # 2550140 & 2553750 */


	Port.DTRAsserted = true;	//we are Data Terminal Ready
	Port.aboveRxHighWater = false;
	
//	Port.SCCWriteLock = IOLockAlloc();
	Port.SCCAccessLock = IOLockAlloc();

    // init the WatchStateLock
//rs!    Port.WatchLock = IOSimpleLockAlloc();
    Port.WatchLock = IOLockAlloc();
    if (!Port.WatchLock)
        return false;
//rs!    IOSimpleLockInit(Port.WatchLock);
    
    // init lock used to protect code on MP
//rs    if (!(Port.serialRequestLock = IOLockAlloc()))
    if (!(Port.serialRequestLock = IORecursiveLockAlloc()))
        return false;  
//rs!    IOSimpleLockInit(Port.serialRequestLock);

    //init the Port structure
    SetStructureDefaults(&Port, true);  
    Port.Instance = Port.whichPort;
    
    // Get chip access addresses
    if (!(map = provider->mapDeviceMemoryWithIndex(0))) return false;
//    Port.Base = map->getPhysicalAddress();
    Port.Base = map->getVirtualAddress();
    Port.ChipBaseAddress = Port.Base;

    // Calls the DMA function that knows how to
    // handle all the different hardware:
    SccSetDMARegisters(&Port, provider);
    if ((Port.TxDBDMAChannel.dmaChannelAddress == NULL) ||
        (Port.TxDBDMAChannel.dmaBase == NULL) ||
        (Port.RxDBDMAChannel.dmaChannelAddress == NULL) ||
        (Port.RxDBDMAChannel.dmaBase == NULL))
        return false;
   
    Port.TXStats.BufferSize= BUFFER_SIZE_DEFAULT;
    Port.RXStats.BufferSize= BUFFER_SIZE_DEFAULT;
    initChip(&Port);

	Port.fAppleSCCSerialInstance = (void *)this;

#if USE_WORK_LOOPS
//	myWorkLoop = (IOWorkLoop *)getWorkLoop();
	
	sccInterruptSource = IOInterruptEventSource::interruptEventSource(
		this,
		(IOInterruptEventAction)&AppleSCCSerial::interruptHandler,
		provider,
		kIntChipSet);
	
	if (!sccInterruptSource)
	{    
		//IOLog("%s: Failed to create SCC interrupt event source!\n", getName());
		return false;
	}
	
	if (myWorkLoop->addEventSource(sccInterruptSource) != kIOReturnSuccess)
	{
		//IOLog("%s: Failed to add SCC interrupt event source to work loop!\n", getName());  
		return false;
	}
	
	
#if USE_FILTER_EVENT_SOURCES
    txDMAInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                    (IOInterruptEventAction) &AppleSCCSerial::interruptHandler,
                    (IOFilterInterruptAction) &AppleSCCSerial::interruptFilter,
                    provider,
					kIntTxDMA);	
#else
	txDMAInterruptSource = IOInterruptEventSource::interruptEventSource(
		this,
		(IOInterruptEventAction)&AppleSCCSerial::interruptHandler,
		provider,
		kIntTxDMA);
#endif	

	if (!txDMAInterruptSource)
	{    
		//IOLog("%s: Failed to create Tx DMA interrupt event source!\n", getName());
		return false;
	}
	
	if (myWorkLoop->addEventSource(txDMAInterruptSource) != kIOReturnSuccess)
	{
		//IOLog("%s: Failed to add Tx DMA interrupt event source to work loop!\n", getName());  
		return false;
	}
	

#if USE_FILTER_EVENT_SOURCES
    rxDMAInterruptSource =   IOFilterInterruptEventSource::filterInterruptEventSource(this,
                    (IOInterruptEventAction) &AppleSCCSerial::interruptHandler,
                    (IOFilterInterruptAction) &AppleSCCSerial::interruptFilter,
                    provider,
					kIntRxDMA);
#else
	rxDMAInterruptSource = IOInterruptEventSource::interruptEventSource(
		this,
		(IOInterruptEventAction)&AppleSCCSerial::interruptHandler,
		provider,
		kIntRxDMA);
#endif	
	
	if (!rxDMAInterruptSource)
	{    
		//IOLog("%s: Failed to create Rx DMA interrupt event source!\n", getName());
		return false;
	}
	
	if (myWorkLoop->addEventSource(rxDMAInterruptSource) != kIOReturnSuccess)
	{
		//IOLog("%s: Failed to add Rx DMA interrupt event source to work loop!\n", getName());  
		return false;
	}
        
	
	// get a timer and set our timeout handler to be called when it fires
	portPtr()->rxTimer = IOTimerEventSource::timerEventSource( this, rxTimeoutHandler );
	
	// make sure we got a timer
	if( !portPtr()->rxTimer )
	{
		//IOLog( "%s: Failed to create timer event source\n", getName() );
		return false;
	}

	// add the timer to the workloop
	if(!myWorkLoop || (myWorkLoop->addEventSource(portPtr()->rxTimer) != kIOReturnSuccess) )
	{
		//IOLog( "%s: Failed to add timer event source to workloop\n", getName() );
		return false;
	}

    if (OSCompareAndSwap(1,0,&gTimerCanceled) ) //Set gTimerCanceled to zero
		IOLog("%s: gTimerCanceled set to Zero\n", getName());  
    
#else
    // Register all the interrupts. We need the chip interrupts to handle the
    // errors and the DMA ... well to transfer the data obviously.
    if (provider->registerInterrupt(kIntChipSet, this, handleInterrupt, 0) != kIOReturnSuccess) {
        return false;
    }

    if (provider->registerInterrupt(kIntTxDMA, this, handleDBDMATxInterrupt, 0) != kIOReturnSuccess) {
        DLOG("AppleSCCSerial: Didn't register kIntTxDMA\n");
        return false;
    }

    if (provider->registerInterrupt(kIntRxDMA, this, handleDBDMARxInterrupt, 0) != kIOReturnSuccess) {
        DLOG("AppleSCCSerial: Didn't register kIntRxDMA\n");
        return false;
    }
#endif

    // enables the hardware (important for powerbooks and core 99)
    callPlatformFunction("EnableSCC", false, (void *)true, 0, 0, 0);

    fPollingThread = thread_call_allocate (
        &AppleSCCSerial::callCarrierhack, ( thread_call_param_t ) this);
	if ( fPollingThread == NULL )
	{
        return false;
	}
    
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


    
    // Finished all initialisation so start service matching

    return createSerialStream();
}

#if USE_TIMER_EVENT_SOURCE_DEBUGGING
// this function is called when the timer fires
void AppleSCCSerial::timeoutHandler(OSObject *owner, IOTimerEventSource *sender)
{
	AppleSCCSerial*	serialPortPtr;
	
	// make sure that the owner of the timer is us
	serialPortPtr = OSDynamicCast( AppleSCCSerial, owner );
	if( serialPortPtr )	// it's us
	{		
		// increment the counter
		serialPortPtr->counter++;

		// indicate we were called
		// print the counter as well
		//IOLog( "%s: In timeoutHandler (%ld)\n", serialPortPtr->getName(), serialPortPtr->counter );        
        //let's display the entire DBDMA Command Struct
        HandleRxIntTimeout(serialPortPtr->portPtr());
		
		// reset the timer for 1 seconds
		sender->setTimeoutMS( kTimerTimeout );
	}
} 
#endif

void AppleSCCSerial::stop(IOService *provider)
{
    //rcs we now move the Deallocation stuff here...
    // Disables the dma channles
    SccFreeReceptionChannel(portPtr());
    SccFreeTansmissionChannel(portPtr());

#if USE_WORK_LOOPS
    if (txDMAInterruptSource)
	{
        //txDMAInterruptSource->disable();
        myWorkLoop->removeEventSource(txDMAInterruptSource);
        txDMAInterruptSource->release();
        txDMAInterruptSource = 0;
    }

    if (rxDMAInterruptSource)
	{
        //rxDMAInterruptSource->disable();
        myWorkLoop->removeEventSource(rxDMAInterruptSource);
        rxDMAInterruptSource->release();
        rxDMAInterruptSource = 0;
    }

    if (sccInterruptSource)
	{
        //sccInterruptSource->disable();
        myWorkLoop->removeEventSource(sccInterruptSource);
        sccInterruptSource->release();
        sccInterruptSource = 0;
    }

	if( portPtr()->rxTimer )
	{
        if (OSCompareAndSwap(1,0,&gTimerCanceled) ) //Set gTimerCanceled to zero
    
        OSIncrementAtomic((SInt32 *) &gTimerCanceled);			//Set the timer cancelled flag

		portPtr()->rxTimer->cancelTimeout();					// stop the timer
		portPtr()->rxTimer->disable();							// stop the timer
		myWorkLoop->removeEventSource( portPtr()->rxTimer );	// remove the timer from the workloop
		portPtr()->rxTimer->release();							// release the timer
		portPtr()->rxTimer = NULL;								//
	}

#else
   // And unregisters them ...
    provider->unregisterInterrupt(kIntChipSet);
    provider->unregisterInterrupt(kIntTxDMA);
    provider->unregisterInterrupt(kIntRxDMA);
#endif


#if USE_TIMER_EVENT_SOURCE_DEBUGGING
	if( myTimer )
	{
		myTimer->cancelTimeout();					// stop the timer
		myWorkLoop->removeEventSource( myTimer );	// remove the timer from the workloop
		myTimer->release();							// release the timer
		myTimer = NULL;								//
	}
#endif
    // deactivates the port
    deactivatePort(&Port);

    if ( fPollingThread != NULL)
    {
        while (fCarrierHackCount > 0) {
            if (thread_call_cancel(fPollingThread))
                break;
            else
                IOSleep(1);
        }
        thread_call_free ( fPollingThread );
        fPollingThread = NULL;
    }

	if (fdmaStartTransmissionThread != NULL)
	{
		while (fTransmissionCount > 0) {
			if (thread_call_cancel(fdmaStartTransmissionThread))
				break;
			else
				IOSleep(1);
		}
			
		thread_call_free(fdmaStartTransmissionThread);
		fdmaStartTransmissionThread = NULL;
	}
	
	if (dmaRxHandleCurrentPositionThread != NULL)
	{
		while (fCurrentPositionCount > 0) {
			if (thread_call_cancel(dmaRxHandleCurrentPositionThread))
				break;
			else
				IOSleep(1);
		}
		
		thread_call_free(dmaRxHandleCurrentPositionThread);
		dmaRxHandleCurrentPositionThread = NULL;
		
	}

    /* Turn the chip off after closing a port */
    if (Port.ControlRegister != NULL)
        SccCloseChannel(&Port);

    if (Port.FrameTOEntry != NULL) {
        //calloutEntryRemove(Port.FrameTOEntry);
        //calloutEntryFree(Port.FrameTOEntry);
    }
    if (Port.DataLatTOEntry != NULL) {
        //calloutEntryRemove(Port.DataLatTOEntry);
        //calloutEntryFree(Port.DataLatTOEntry);
    }
    if (Port.DelayTOEntry != NULL) {
        //calloutEntryRemove(Port.DelayTOEntry);
        //calloutEntryFree(Port.DelayTOEntry);
    }
    if (Port.HeartBeatTOEntry != NULL) {
        //calloutEntryRemove(Port.HeartBeatTOEntry);
        //calloutEntryFree(Port.HeartBeatTOEntry);
    }
	/* Begin fixed bug # 2550140 & 2553750 */
	IOLockFree (Port.IODBDMARxLock);
	IOLockFree (Port.IODBDMATrLock);
	/* End fixed bug # 2550140 & 2553750 */
    
//	IOLockFree (Port.SCCWriteLock);
	IOLockFree (Port.SCCAccessLock);


// 	IOSimpleLockFree (Port.WatchLock);	//hsjb 5/4/01, this was missing in SU2
 	IOLockFree (Port.WatchLock);	//hsjb 5/4/01, this was missing in SU2
//rs 7/29/02 	IOLockFree (Port.serialRequestLock);
 	IORecursiveLockFree (Port.serialRequestLock);	//hsjb 5/4/01, this was missing in SU2
   

    super::stop(provider);
}

void AppleSCCSerial::setPortName(IOService *provider)
{
    IOService *topProvider = NULL;
    IOService *myProvider;
    OSData *s;

    // if the argument is missing I've to get from myself:
    if (provider == NULL)
        provider = getProvider();

    // We need to keep track of who is our provider:
    myProvider = provider;

    // This is the way it was encoded in the old machines:
    s = OSDynamicCast(OSData, myProvider->getProperty("AAPL,connector"));
    if (s) {
        // we are going to use it as it is
        char *tmpName = (char *) s->getBytesNoCopy();
        setProperty(kIOTTYBaseNameKey, tmpName);

        // and exits:
        return;
    }

    // See if we find the top of the tree (with the machine type)
    // iterating all the way up:
    while (provider) {
        topProvider = provider;
        provider = topProvider->getProvider();
    }

    // If we are here we did not find a name, so let's try the method used
    // on the newer hardware:
    s = OSDynamicCast(OSData, myProvider->getProperty("slot-names"));
    if (s != NULL) {
        UInt32 *nWords = (UInt32*)s->getBytesNoCopy();

        // If there is more than one entry
        if (*nWords > 0) {
            char *tmpPtr, *tmpName;

            tmpName = (char *) s->getBytesNoCopy() + sizeof(UInt32);

            // To make parsing easy, sets the sting in low case:
            for (tmpPtr = tmpName; *tmpPtr != 0; tmpPtr++)
                *tmpPtr |= 32;

            // and sets the property
			if (IODTMatchNubWithKeys(myProvider, "'ch-b'"))
			{
				if (topProvider != NULL)
				{
					if (IODTMatchNubWithKeys(topProvider, "'PowerMac1,1'"))		// B&W G3
					{
						setProperty(kIOTTYBaseNameKey, "none");
						return;
					}
				}
			}

			setProperty(kIOTTYBaseNameKey, tmpName);

            // and exits:
            return;
        }
    }


    // Again nothing. At this point we can be missing the name or we may
    // just have the modem disconnected. If it is an old machine we give
    // to the port the same name of the scc channel:
    if (topProvider != NULL) {
        if (IODTMatchNubWithKeys(topProvider, "'AAPL,9500'")
            ||  IODTMatchNubWithKeys(topProvider, "'AAPL,Gossamer'")
            ||  IODTMatchNubWithKeys(topProvider, "'AAPL,PowerBook1998'")
            ||  IODTMatchNubWithKeys(topProvider, "'AAPL,PowerMac G3'")) {

            // as last attempt we use the channel name:
            s = OSDynamicCast(OSData, myProvider->getProperty("name"));
            if (s != NULL) {
                // we are going to use it as it is
                char *tmpName = (char*)s->getBytesNoCopy();
                setProperty(kIOTTYBaseNameKey, tmpName);

                // and exits:
                return;
            }
        }
    }

	// ****** Need to add code for irda support for the PowerBooks.
	
	// ****** Also need special code for irda support on WallStreet.
	
    // In all the other cases (PowerMacX,X and iMac,X) I assume ch-a to be
    // the modem port and there is no ch-b support.
    if (IODTMatchNubWithKeys(myProvider, "'ch-a'")) 
	{
		if (topProvider != NULL)
		{
			if (IODTMatchNubWithKeys(topProvider, "'PowerMac1,1'"))	// B&W G3
			{
				setProperty(kIOTTYBaseNameKey, "modem");
			}
			else
			{
				OSString *tmpName = OSDynamicCast(OSString, getProperty("kSCCDebugKey"));
				if (tmpName != NULL)
					setProperty(kIOTTYBaseNameKey, "serial");
				else
					setProperty(kIOTTYBaseNameKey, "none");
			}
		}
		else
			setProperty(kIOTTYBaseNameKey, "none");
	}
    else if (IODTMatchNubWithKeys(myProvider, "'ch-b'"))
        setProperty(kIOTTYBaseNameKey, "none");
}

bool AppleSCCSerial::createSerialStream()
{
    AppleSCCRS232SerialStreamSync *rs232Stream;
    OSObject *dtProperty;
    bool ret;

    rs232Stream = new AppleSCCRS232SerialStreamSync;
    if (!rs232Stream)
        return false;

    // Either we attach and should get rid of our reference
    // or we have fail in which case we should get rid our reference.
    ret = (rs232Stream->init(0, &Port) && rs232Stream->attach(this));
    rs232Stream->release();
    if (!ret)
        return false;

    // Copy the provider's DeviceTree properties into our nub
    dtProperty = fProvider->getProperty("device_type");
    if (dtProperty)
        rs232Stream->setProperty("device_type", dtProperty);

    dtProperty = fProvider->getProperty("compatible");
    if (dtProperty)
        rs232Stream->setProperty("compatible", dtProperty);

    dtProperty = fProvider->getProperty("slot-names");
    if (dtProperty)
        rs232Stream->setProperty("slot-names", dtProperty);

    dtProperty = fProvider->getProperty("AAPL,connector");
    if (dtProperty)
        rs232Stream->setProperty("AAPL,connector", dtProperty);

    // Make sure we copy the PORTNAME up as well.
    dtProperty = getProperty(gIOTTYBaseNameKey);
    if (dtProperty)
        rs232Stream->setProperty(gIOTTYBaseNameKey, dtProperty);

    rs232Stream->registerService();

    return true;
} // end of function AppleSCCSerial::createSerialStream

void AppleSCCSerial::
handleInterrupt(OSObject *target, void *refCon, IOService *nub, int source)
{
    AppleSCCSerial *serialPortPtr = (AppleSCCSerial*)target;

    if (serialPortPtr != NULL) {
    #if ! USE_WORK_LOOPS
        serialPortPtr->fProvider->disableInterrupt(kIntChipSet);
    #endif
    
        PPCSerialISR(target, refCon, serialPortPtr->portPtr());

     #if ! USE_WORK_LOOPS
        serialPortPtr->fProvider->enableInterrupt(kIntChipSet);
    #endif    
    }
}

void AppleSCCSerial::
handleDBDMATxInterrupt(OSObject *target, void *refCon, IOService *nub, int source)
{
    AppleSCCSerial *serialPortPtr = OSDynamicCast(AppleSCCSerial, target);

    if (serialPortPtr != NULL) {
     #if ! USE_WORK_LOOPS
        serialPortPtr->fProvider->disableInterrupt(kIntTxDMA);
    #endif    
        PPCSerialTxDMAISR(serialPortPtr, NULL,  serialPortPtr->portPtr());
        
     #if ! USE_WORK_LOOPS
        serialPortPtr->fProvider->enableInterrupt(kIntTxDMA);
    #endif    
    }
}

void AppleSCCSerial::
handleDBDMARxInterrupt(OSObject *target, void *refCon, IOService *nub, int source)
{
    AppleSCCSerial *serialPortPtr = OSDynamicCast(AppleSCCSerial, target);
    

    if (serialPortPtr != NULL) {
#if USE_TIMER_EVENT_SOURCE_DEBUGGING    
        serialPortPtr->myTimer->cancelTimeout(); //cancel timeout
        serialPortPtr->myTimer->setTimeoutMS( kTimerTimeout ); //arm a 1 second timer
#endif    

     #if ! USE_WORK_LOOPS
        serialPortPtr->fProvider->disableInterrupt(kIntRxDMA);
    #endif

        PPCSerialRxDMAISR(serialPortPtr, NULL,  serialPortPtr->portPtr());

     #if ! USE_WORK_LOOPS
        serialPortPtr->fProvider->enableInterrupt(kIntRxDMA);
     #endif
    }
}

// =================================================
// New implementation of PortDevice protocol
// =================================================
/* acquirePort tests and sets the state of the port object.  If the port was
 *available, then the state is set to busy, and kIOReturnSuccess is returned.
 *If the port was already busy and sleep is YES, then the thread will sleep
 *until the port is freed, then re-attempts the acquire.  If the port was
 *already busy and sleep is NO, then IO_R_EXCLUSIVE_ACCESS is returned.
 */
IOReturn AppleSCCSerial::acquirePort(bool sleep, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    UInt32 	busyState = 0;
    IOReturn	rtn = kIOReturnSuccess;


    if (OSCompareAndSwap(1,0,&gTimerCanceled) ) //Set gTimerCanceled to zero
		IOLog("%s: gTimerCanceled set to Zero\n", getName());  

    DLOG("[acquirePort ");
    
    for (;;) {
        busyState = (readPortState(port) & PD_S_ACQUIRED);

        if (!busyState) {
            // Set busy bit, and clear everything else
            changeState(port, PD_S_ACQUIRED | DEFAULT_STATE, STATE_ALL);
            break;
        } else if (!sleep) {
            DLOG("Busy!]");
            return kIOReturnExclusiveAccess;
        } else {
            busyState = 0;
            rtn = watchState(&busyState, PD_S_ACQUIRED, 0);
            if ((rtn == kIOReturnIOError) || (rtn == kIOReturnSuccess)) {
                continue;
            } else {
                DLOG("Interrupted!]");
                return rtn;
            }
        }
    }

    /* We now own the port. */
    do {
        if (!fProvider->open(this)) {
            rtn = kIOReturnBusy;
            break;
        }

        /* Creates and sets up the ring buffers */
        if (!allocateRingBuffer(&(port->TX), port->TXStats.BufferSize)
        ||  !allocateRingBuffer(&(port->RX), port->RXStats.BufferSize)) {
            rtn = kIOReturnNoMemory;
            break;
        }

		// for DCP -- Begin

		if ((port->whichPort == serialPortA) && (port->gDCPModemFound)) 
		{
//			IOLog ("AppleSCCSerial::acquirePort -- callPlatFormFunction for Port.DCPModemSupportPtr\n");
			port->DCPModemSupportPtr = NULL;
			
			void *tmp = NULL;
			tmp = (void *) callPlatformFunction("GetDCPObject", false, 0, 0, 0, 0);
			if (tmp != NULL)
			{
//				IOLog ("AppleSCCSerial::acquirePort -- Port.DCPModemSupportPtr != NULL\n");
				port->gDCPUserClientSet = true;
				port->DCPModemSupportPtr = (DCPModemSupport *) tmp;
			}
			else
			{
				port->gDCPUserClientSet = false;
//				IOLog ("AppleSCCSerial::acquirePort -- Port.DCPModemSupportPtr == NULL\n");
			}

			if ((port->gDCPModemFound) && (port->gDCPUserClientSet))
			{
				port->DCPModemSupportPtr->setDCPBufferSize (port->RXStats.BufferSize);
			}
		}

		// for DCP -- End

        /* Initialize all the structures */
        SetStructureDefaults(port, FALSE);

        /* OK, Let's actually setup the chip. */
        OpenScc(port);
		
        // Enables all the DMA transfers:
        SccSetupReceptionChannel(port);
        SccdbdmaDefineReceptionCommands(port);
        SccSetupTansmissionChannel(port);
        SccdbdmaDefineTansmissionCommands(port);

        // Enable all the interrupts
     #if USE_WORK_LOOPS
	sccInterruptSource->enable();
	txDMAInterruptSource->enable();
	rxDMAInterruptSource->enable();
     #else
        rtn = fProvider->enableInterrupt(kIntChipSet);
        if (rtn != kIOReturnSuccess)
            break;

        rtn = fProvider->enableInterrupt(kIntTxDMA);
        if (rtn != kIOReturnSuccess) {
//            IOLog("AppleSCCSerial: Didn't enable kIntTxDMA interrupt\n");
            break;
        }

        rtn = fProvider->enableInterrupt(kIntRxDMA);
        if (rtn != kIOReturnSuccess) {
//            IOLog ("AppleSCCSerial: Didn't enable kIntRxDMA interrupt\n");
            break;
        }
    #endif
        // also begins to monitor the channel
        SccdbdmaStartReception(port);

        DLOG("Early]\n");
        
        return kIOReturnSuccess;
    } while (0);

    // We failed for some reason
    freeRingBuffer(&(port->TX));
    freeRingBuffer(&(port->RX));

    fProvider->close(this);
    changeState(port, 0, STATE_ALL);	// Clear the entire state word

    return rtn;
}

/* release sets the state of the port object to available and wakes up and
 *threads sleeping for access to this port.  It will return IO_R_SUCCESS
 *if the port was in a busy state, and IO_R_NOT_OPEN if it was available.
 */
IOReturn AppleSCCSerial::releasePort(void *refCon)
{
    IOReturn	ret;
	
    retain();
    ret = fCommandGate->runAction(releasePortAction);
    release();
    
    return ret;
}

IOReturn AppleSCCSerial::releasePortAction(OSObject *owner, void *, void *, void *, void *)
{
    return ((AppleSCCSerial *)owner)->releasePortGated();
}

IOReturn AppleSCCSerial::releasePortGated()
{
//    PortInfo_t *port = (PortInfo_t *) refCon;
    PortInfo_t *port	= &Port;
    UInt32		busyState = 0;
    int 		i;

	OSIncrementAtomic((SInt32 *) &gTimerCanceled);			//Set the timer cancelled flag

    DLOG("[release ");
    busyState = (readPortState(port) & PD_S_ACQUIRED);
    if (!busyState) {
        DLOG("NOT OPEN]");
        return kIOReturnNotOpen;
    }

	// for DCP -- Begin
	if (port->whichPort == serialPortA)
	{
//		IOLog ("AppleSCCSerial::releasePort -- For serial port A\n");
		if (port->gDCPModemFound)
		{
//			IOLog ("AppleSCCSerial::releasePort -- calling DCPModemSupportFunction to quit DCP\n");
			if (port->gDCPUserClientSet)
				Port.DCPModemSupportPtr->callDCPModemSupportFunctions (DCPFunction_QuitDCP, NULL, NULL, NULL);
/*
			else
				IOLog ("AppleSCCSerial::releasePort -- Cannot call DCPModemSupportFunction due to DCPUserClient is not set\n");
		}

		else
		{
			IOLog ("AppleSCCSerial::releasePort -- Not a DCP modem\n");
*/
		}
	}
/*
	else
	{
		IOLog ("AppleSCCSerial::releasePort -- Not for serial port A\n");
	}
*/
	// for DCP -- End


    //calloutEntryRemove(port->HeartBeatTOEntry);
    //calloutEntryRemove(port->DelayTOEntry);
    //calloutEntryRemove(port->DataLatTOEntry);
    //HuL*4calloutEntryRemove(port->FrameTOEntry);
    //IOExitThread(port->HeartBeatTOEntry);
    //IOExitThread(port->DelayTOEntry);
    //IOExitThread(port->DataLatTOEntry);
    //IOExitThread(port->FrameTOEntry);

    // default everything
    for (i=0; i<(256>>SPECIAL_SHIFT); i++)
        port->SWspecial[i] = 0;

    port->CharLength 		= 8;
    port->BreakLength 		= 1;
    port->XONchar			= '\x11';
    port->XOFFchar			= '\x13';
    port->StopBits			= 1<<1;
    port->TX_Parity 		= PD_RS232_PARITY_NONE;
    port->RX_Parity 		= PD_RS232_PARITY_DEFAULT;
    port->RXOstate			= IDLE_XO;
    port->BaudRate 			= kDefaultBaudRate;
    port->FlowControl 		= DEFAULT_NOTIFY;
    port->FlowControlState	= CONTINUE_SEND;
    port->DCDState			= false;
    fModemObject = 0;
    deactivatePort(port);

    // Disables the interrupts:
#if USE_WORK_LOOPS
    sccInterruptSource->disable();
    txDMAInterruptSource->disable();
    rxDMAInterruptSource->disable();
#else
    fProvider->disableInterrupt(kIntChipSet);
    fProvider->disableInterrupt(kIntTxDMA);
    fProvider->disableInterrupt(kIntRxDMA);    
#endif

    // Disables the dma channles
/* rcs!!!
    SccFreeReceptionChannel(port);
    SccFreeTansmissionChannel(port);
*/    
    // Now that we do not get intrrupts anymore we can remove all the
    // buffers.
    freeRingBuffer(&(port->TX));
    freeRingBuffer(&(port->RX));

    DLOG("OK]\n");

    fProvider->close(this);
    changeState(port, 0, STATE_ALL);	// Clear the entire state word

    return kIOReturnSuccess;
}



/*
 *Set the state for the port device.  The lower 16 bits are used to set the
 *state of various flow control bits (this can also be done by enqueueing an
 *PD_E_FLOW_CONTROL event).  If any of the flow control bits have been set
 *for automatic control, then they can't be changed by setState.  For flow
 *control bits set to manual (that are implemented in hardware), the lines
 *will be changed before this method returns.  The one weird case is if RXO
 *is set for manual, then an XON or XOFF character may be placed at the end
 *of the TXQ and transmitted later.
 */
IOReturn AppleSCCSerial::setState(UInt32 state, UInt32 mask, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    DLOG("++>setState %d %x\n",(int)state, (int)mask);

    if (mask & (PD_S_ACQUIRED | PD_S_ACTIVE | (~EXTERNAL_MASK)))
        return kIOReturnBadArgument;

    if ((readPortState(port) & PD_S_ACQUIRED) != 0) {
        // ignore any bits that are read-only
        mask &= (~port->FlowControl & PD_RS232_A_MASK) | PD_S_MASK;

        if (mask != 0)
            changeState(port, state, mask);

        DLOG("-->setState\n");
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
}

/*
 *Get the state for the port device.
 */
UInt32 AppleSCCSerial::getState(void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    CheckQueues(port);
    return (readPortState(port) & EXTERNAL_MASK);
}

/*
 *Wait for the at least one of the state bits defined in mask to be equal
 *to the value defined in state. Check on entry then sleep until necessary.
 */
IOReturn AppleSCCSerial::watchState(UInt32 *state, UInt32 mask, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    IOReturn 		ret = kIOReturnNotOpen;

    DLOG("watchState\n");

    if ((readPortState(port) & PD_S_ACQUIRED) != 0) {
        ret = kIOReturnSuccess;
        mask &= EXTERNAL_MASK;
        ret = watchState(port, state, mask);
        (*state) &= EXTERNAL_MASK;
    }
    return ret;
}


/* nextEvent returns the type of the next event on the RX queue.  If no
 *events are present on the RX queue, then PD_E_EOQ is returned.
 */
UInt32 AppleSCCSerial::nextEvent(void *refCon)
{
    UInt32 	ret = kIOReturnSuccess;

    DLOG("nextEvent\n");

//    PortInfo_t *port = (PortInfo_t *) refCon;
//	ret=peekEvent(&(port->RX), 0);

    return ret;
}

/* executeEvent causes the specified event to be processed immediately.
 *This is primarily used for channel control commands like START & STOP
 */
IOReturn AppleSCCSerial::executeEvent(UInt32 event, UInt32 data, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    IOReturn 		ret = kIOReturnSuccess;
    UInt32 	state, delta;
 
    DLOG("executeEvent\n");

    if ((readPortState(port) & PD_S_ACQUIRED) != 0) {
        switch (event) {
            case PD_E_FLOW_CONTROL:
				UInt32 tmp;
				tmp = FLOW_RX_AUTO & (data ^ port->FlowControl);
				port->FlowControl = data & (CAN_BE_AUTO | CAN_NOTIFY);

				if (tmp) {
					if (tmp & PD_RS232_A_RXO)
					{
						port->RXOstate = NEEDS_XOFF;
//						port->RXOstate = IDLE_XO;
					}
					//changeState(port, flowMachine(port), FLOW_RX_AUTO);
				}
				
                //#if 0 /* ejk FIXTHIS	*/
                //            tmp = FLOW_RX_AUTO & (data ^ port->FlowControl);
                //            port->FlowControl = data & (CAN_BE_AUTO | CAN_NOTIFY);
                //           if (tmp) {
                //               if (tmp & PD_RS232_A_RXO)
                //                   port->RXOstate = IDLE_XO;
                //              //changeState(port, flowMachine(port), FLOW_RX_AUTO);
                //          }
                //#endif
                //           ×××break;
                break;
            case PD_E_DELAY:
                port->CharLatInterval = long2tval(data *1000);
                break;
            case PD_E_RXQ_SIZE:
#ifdef IntelTest
                if (readPortState(port) & PD_S_ACTIVE) {
                    ret = IO_R_BUSY;
                }
                else {
                    port->RX.Size = validateRingBufferSize(data, &(port->RX));
                    if (port->RX.HighWater > (port->RX.Size - BIGGEST_EVENT))
                        port->RX.HighWater = (port->RX.Size - BIGGEST_EVENT);
                    if (port->RX.LowWater > (port->RX.HighWater - BIGGEST_EVENT))
                        port->RX.LowWater = (port->RX.HighWater - BIGGEST_EVENT);
                }
#endif
                break;
            case PD_E_TXQ_SIZE:
#ifdef IntelTest
                if (readPortState(port) & PD_S_ACTIVE) {
                    ret = IO_R_BUSY;
                }
                else {
                    port->TX.Size = validateRingBufferSize(data, &(port->RX));
                    if (port->TX.HighWater > (port->TX.Size - BIGGEST_EVENT))
                        port->TX.HighWater = (port->TX.Size - BIGGEST_EVENT);
                    if (port->TX.LowWater > (port->TX.HighWater - BIGGEST_EVENT))
                        port->TX.LowWater = (port->TX.HighWater - BIGGEST_EVENT);
                }
#endif
                break;
            default:
                delta = 0;
                state = readPortState(port);
                ret = executeEvent(port, event, data, &state, &delta);
                changeState(port, state, delta);
                break;
        }
    }

    return ret;
}

/* requestEvent processes the specified event as an immediate request and
 *returns the results in data.  This is primarily used for getting link
 *status information and verifying baud rate and such.
 *
 *Author's note:  This was one of my favorite routines to code up!
 */
IOReturn AppleSCCSerial::requestEvent(UInt32 event, UInt32 *data, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    IOReturn returnValue = kIOReturnSuccess;
    DLOG("call requestEvent %ld\n", event);

   if (data == NULL)
        returnValue = kIOReturnBadArgument;
    else {
        switch (event) {
			case PD_E_ACTIVE				: 	*data = bool(readPortState(port)&PD_S_ACTIVE);		break;
            case PD_E_FLOW_CONTROL			:	*data = port->FlowControl;							break;
            case PD_E_DELAY					:	*data = tval2long(port->CharLatInterval)/1000;		break;
            case PD_E_DATA_LATENCY			:	*data = tval2long(port->DataLatInterval)/1000;		break;
            case PD_E_TXQ_SIZE				:	*data = GetQueueSize(&(port->TX));					break;
            case PD_E_RXQ_SIZE				:	*data = GetQueueSize(&(port->RX));					break;
#if 0
            case PD_E_TXQ_LOW_WATER			:	*data = port->TX.LowWater;							break;
            case PD_E_RXQ_LOW_WATER			:	*data = port->RX.LowWater;							break;
            case PD_E_TXQ_HIGH_WATER		:	*data = port->TX.HighWater;							break;
            case PD_E_RXQ_HIGH_WATER		:	*data = port->RX.HighWater;							break;
#else
            case PD_E_TXQ_LOW_WATER			:
			case PD_E_RXQ_LOW_WATER			:
			case PD_E_TXQ_HIGH_WATER		:
			case PD_E_RXQ_HIGH_WATER		:	*data = 0; returnValue = kIOReturnBadArgument;		break;                
#endif
            case PD_E_TXQ_AVAILABLE			:	*data = FreeSpaceinQueue(&(port->TX));				break;
            case PD_E_RXQ_AVAILABLE			:	*data = UsedSpaceinQueue(&(port->RX));				break;
            case PD_E_DATA_RATE				:	*data = port->BaudRate << 1;						break;
            case PD_E_RX_DATA_RATE			:	*data = 0x00;										break;
            case PD_E_DATA_SIZE				: 	*data = port->CharLength << 1; 						break;
            case PD_E_RX_DATA_SIZE			:	*data = 0x00;										break;
            case PD_E_DATA_INTEGRITY		:	*data = port->TX_Parity;							break;
            case PD_E_RX_DATA_INTEGRITY		:	*data = port->RX_Parity;							break;
            case PD_RS232_E_STOP_BITS		:	*data = port->StopBits << 1;						break;
            case PD_RS232_E_RX_STOP_BITS	:	*data = 0x00;										break;
            case PD_RS232_E_XON_BYTE		:	*data = port->XONchar;								break;
            case PD_RS232_E_XOFF_BYTE		:	*data = port->XOFFchar;								break;
            case PD_RS232_E_LINE_BREAK		:	*data = bool(readPortState(port)&PD_RS232_S_BRK);	break;
            case PD_RS232_E_MIN_LATENCY		:	*data = bool(port->MinLatency);						break;
            default 						:	returnValue = kIOReturnBadArgument; 				break;
        }
    }

    DLOG("end requestEvent %ld\n", event);

    return kIOReturnSuccess;
}

/* enqueueEvent will place the specified event into the TX queue.  The
 *sleep argument allows the caller to specify the enqueueEvent's
 *behaviour when the TX queue is full.  If sleep is true, then this
 *method will sleep until the event is enqueued.  If sleep is false,
 *then enqueueEvent will immediatly return IO_R_RESOURCE.
 */
IOReturn AppleSCCSerial::
enqueueEvent(UInt32 event, UInt32 data, bool sleep, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    DLOG("enqueueEvent (write)\n");

    if (readPortState(port) & PD_S_ACTIVE != 0) {
        //	rtn = TX_enqueueEvent(port, event, data, sleep);

#ifdef IntelTest
        if ((rtn == kIOReturnSuccess) && (!((readPortState(port))&PD_S_TX_BUSY)))
            calloutEntryDispatch(port->FrameTOEntry);	// restart TX engine
#endif
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
}

/* dequeueEvent will remove the oldest event from the RX queue and return
 *it in event & data.  The sleep argument defines the behavior if the RX
 *queue is empty.  If sleep is true, then this method will sleep until an
 *event is available.  If sleep is false, then an PD_E_EOQ event will be
 *returned.  In either case IO_R_SUCCESS is returned.
 */
IOReturn AppleSCCSerial::
dequeueEvent(UInt32 *event, UInt32 *data, bool sleep, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    DLOG("dequeueEvent (read)\n");

    if ((event == NULL) || (data == NULL))
        return kIOReturnBadArgument;

    if (readPortState(port) & PD_S_ACTIVE !=0) {
        //	rtn = RX_dequeueEvent(port, &e, data, sleep);
        //	*event = (UInt32)e;
        return kIOReturnSuccess;
    }

    return kIOReturnNotOpen;
}

/* enqueueData will attempt to copy data from the specified buffer to the
 *TX queue as a sequence of VALID_DATA events.  The argument bufferSize
 *specifies the number of bytes to be sent.  The actual number of bytes
 *transferred is returned in transferCount.  If sleep is true, then this
 *method will sleep until all bytes can be transferred.  If sleep is
 *false, then as many bytes as possible will be copied to the TX queue.
 *
 *Note that the caller should ALWAYS check the transferCount unless the
 *return value was IO_R_INVALID_ARG, indicating one or more arguments
 *were not valid.  Other possible return values are IO_R_SUCCESS if all
 *requirements were met; IO_R_IPC_FAILURE if sleep was interrupted by
 *a signal; IO_R_IO if the port was deactivated.
 */
IOReturn AppleSCCSerial::
enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    UInt32   state = PD_S_TXQ_LOW_WATER;
    IOReturn rtn   = kIOReturnSuccess;

    DLOG("++>In Enqueue %d %d sleep-%d\n", size, *count, sleep);
    if (count == NULL || buffer == NULL)
        return kIOReturnBadArgument;

    (*count) = 0;

    if (!(readPortState(port) & PD_S_ACTIVE)) {
        return kIOReturnNotOpen;
    }

#ifdef TRACE
    /* The debug code consists of the following
        *
        *----------------------------------------------------------------------
        *|              |               |                               |Func   |
        *| Class (8)    | SubClass (8)  |          Code (14)            |Qual(2)|
        *----------------------------------------------------------------------
        *Class     = drivers = 0x06
        *Sub Class = serial  = 0x08
        *Code      = func ID = 1
        *FuncQulif: it is
        *DBG_FUNC_START          1
        *DBG_FUNC_END            2
        *DBG_FUNC_NONE           0
        *0x06080005
        *how to trace:
        *trace -i enables the tracing and sets up the kernel buffer for tracing
        *trace -g shows the trace setup.
        *trace -e start tracing (but I use trace -e -c 6 -s 8 to trace only the calls in the
                                  *         serial driver.
                                  *trace -d stop the tracer.
                                  *trace -t codefile >result dumps the content of the trace buffer in the file "result"
                                  */

    KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 2)) | DBG_FUNC_START, size, sleep, 0, 0, 0); //0x06080009
#endif
	
    /* OK, go ahead and try to add something to the buffer.
        */
    *count = AddtoQueue(&(port->TX), buffer, size);
    CheckQueues(port);
    
 	if (port->FlowControlState == PAUSE_SEND)
	{
		return kIOReturnSuccess;
	}

        /* Let the tranmitter know that we have something ready to go.
            */
        SetUpTransmit(port);

        /* If we could not queue up all of the data on the first pass
            and the user wants us to sleep until until it's all out then sleep.
            */
        while ((*count < size) && sleep) {
            state = PD_S_TXQ_LOW_WATER;
            rtn = watchState(&state, PD_S_TXQ_LOW_WATER, 0);
            /* if ((rtn == IO_R_IO) || (rtn == kIOReturnSuccess)) */
            if (rtn != kIOReturnSuccess) {
                DLOG("Interrupted!]");
#ifdef TRACE
                KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 2)) | DBG_FUNC_END, size, sleep, *count, -1, 0);  //0x0608000A
#endif
                return rtn;
            }
            DLOG("Adding More to Queue %d\n",  size - *count);

            *count += AddtoQueue(&(port->TX), buffer + *count, size - *count);
            CheckQueues(port);

            /* Let the tranmitter know that we have something ready to go.
                */
            SetUpTransmit(port);
        }
        DLOG("Enqueue Check %x\n",(int)UsedSpaceinQueue(&(port->TX)));

        DLOG("chars sent = %d of %d\n", *count,size);
        DLOG("-->Out Enqueue\n");

#ifdef TRACE
        KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 2)) | DBG_FUNC_END, size, sleep, *count, -2, 0);  //0x0608000A
#endif

        return kIOReturnSuccess;
}        

/* dequeueData will attempt to copy data from the RX queue to the specified
 *buffer.  No more than bufferSize VALID_DATA events will be transferred.
 *In other words, copying will continue until either a non-data event is
 *encountered or the transfer buffer is full.  The actual number of bytes
 *transferred is returned in transferCount.
 *
 *The sleep semantics of this method are slightly more complicated than
 *other methods in this API:  Basically, this method will continue to
 *sleep until either minCount characters have been received or a non
 *data event is next in the RX queue.  If minCount is zero, then this
 *method never sleeps and will return immediatly if the queue is empty.
 *
 *Note that the caller should ALWAYS check the transferCount unless the
 *return value was IO_R_INVALID_ARG, indicating one or more arguments
 *were not valid.  Other possible return values are IO_R_SUCCESS if all
 *requirements were met; IO_R_IPC_FAILURE if sleep was interrupted by
 *a signal; IO_R_IO if the port was deactivated.
 */
IOReturn AppleSCCSerial::
dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min, void *refCon)
{
    PortInfo_t *port = (PortInfo_t *) refCon;
    IOReturn 		rtn = kIOReturnSuccess;
    UInt32 	state = 0;

    /* Set up interrupt variable.*/
    DLOG("++>In Dequeue buf %x bufSize %d txCount %d minCount %d\n",(int)buffer, size, *count, min);

    /* Check to make sure we have good arguments.
        */
    if ((count == NULL) || (buffer == NULL) || (min > size))
        return kIOReturnBadArgument;

    /* If the port is not active then there should not be any chars.
        */
    *count = 0;
    if (!(readPortState(port) & PD_S_ACTIVE))
        return(kIOReturnNotOpen);


#ifdef TRACE
    /* The debug code consists of the following
        *
        *----------------------------------------------------------------------
        *|              |               |                               |Func   |
        *| Class (8)    | SubClass (8)  |          Code (14)            |Qual(2)|
        *----------------------------------------------------------------------
        *Class     = drivers = 0x06
        *Sub Class = serial  = 0x08
        *Code      = func ID = 1
        *FuncQulif: it is
        *DBG_FUNC_START          1
        *DBG_FUNC_END            2
        *DBG_FUNC_NONE           0
        *0x06080005
        *how to trace:
        *trace -i enables the tracing and sets up the kernel buffer for tracing
        *trace -g shows the trace setup.
        *trace -e start tracing (but I use trace -e -c 6 -s 8 to trace only the calls in the
                                 *         serial driver).
        *trace -d stop the tracer.
        *trace -t codefile >result dumps the content of the trace buffer in the file "result"
        */

    KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 1)) | DBG_FUNC_START, size, min, 0, 0, 0); //0x06080005
#endif

    /* Get any data living in the queue.
        */
    *count = RemovefromQueue(&(port->RX), buffer, size);
    CheckQueues(port);
    DLOG("-->chars receivedA = %d of needed %d\n", *count, size);

#if 1 	/*EJK REMOVE THIS*/
    while ((min > 0) && (*count < min)) {
        //#define CHAR_DELAY
#ifdef CHAR_DELAY
        UInt32 bitInterval = 1000000L/port->BaudRate; 	// microseconds
        UInt32 delay = (100L*bitInterval)/1000L;		// numChars*numBits*bitInterval / 1000
        IOSleep(delay);
#endif
        /* Figure out how many bytes we have left to queue up */
        state = 0;

        rtn = watchState(port, &state, PD_S_RXQ_EMPTY);

        // Test This                if ((rtn == IO_R_IO) || (rtn == kIOReturnSuccess)) {
        if (rtn == kIOReturnSuccess) {
            //this may cause the queues to get corrupted so leaving ints enabled for now
            //IOSimpleLockUnlockEnableInterrupt(port->serialRequestLock, previousInterruptState);
        } else {
            DLOG("Interrupted!]");

#ifdef TRACE
            KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 1)) | DBG_FUNC_END, size, min, *count, -1, 0);  //0x06080006
#endif
            return rtn;
        }
        /* Try and get more data starting from where we left off */
        *count += RemovefromQueue(&(port->RX), buffer + *count, (size - *count));
        CheckQueues(port);
		DLOG("-->chars receivedB = %d of needed %d\n", *count, size);
        }
#endif

    /* Now let's check our receive buffer to see if we need to stop */
//    bool goXOIdle = (UsedSpaceinQueue(&(port->RX)) < port->RXStats.LowWater) && (port->RXOstate == SENT_XOFF);

//    if (goXOIdle) {
//        port->RXOstate = IDLE_XO;
//        AddBytetoQueue(&(port->TX), port->XOFFchar);
//        SetUpTransmit(port);
//    }

     //DLOG("-->chars received = %d of needed %d\n", *count, size);
    // DLOG("-->Out Dequeue\n");

#ifdef TRACE
    KERNEL_DEBUG_CONSTANT((DRVDBG_CODE(DBG_DRVSERIAL, 1)) | DBG_FUNC_END, size, min, *count, -2, 0);  //0x06080006
#endif

    return rtn;
}
/* _______________________End Protocol________________________  */


/*---------------------------------------------------------------------
 *		activatePort
 *ejk		Sat, Dec 27, 1997	16:47
 *		Comment Me
 *
 *-------------------------------------------------------------------*/
IOReturn AppleSCCSerial::activatePort(PortInfo_t *port) {
    DLOG(" activatePort\n");

    if (readPortState(port) & PD_S_ACTIVE)
        return kIOReturnSuccess;
/*
    if (!allocateRingBuffer(&(port->TX), port->TXStats.BufferSize)) {
        freeRingBuffer(&(port->TX));
        return kIOReturnNoResources;
    }

    if (!allocateRingBuffer(&(port->RX) , port->RXStats.BufferSize)) {
        freeRingBuffer(&(port->TX));
        return kIOReturnNoResources;
    }
*/
    SetStructureDefaults(port, FALSE);

    //CirQueue	*aQueue = NULL;
    //InitQueue(aQueue, (UInt8*)&port->TX, kMaxCirBufferSize);
    //InitQueue(aQueue, (UInt8*)&port->RX, kMaxCirBufferSize);
        // Clear stats
//	programChip(port);
#ifdef IntelTest
    if ((port->Type) > NS16550)
        outb(FIFO_CTRL, ((port->FCRimage) | TX_FIFO_RESET | RX_FIFO_RESET));
#endif

    changeState(port, PD_S_ACTIVE, PD_S_ACTIVE); // activate port
    //changeState(port, flowMachine(port), FLOW_RX_AUTO);	 // set up HW Flow Control
    //changeState(port, generateTXQState(port) | generateRXQState(port),
    //			PD_S_TXQ_MASK | PD_S_RXQ_MASK | FLOW_RX_AUTO);

#ifdef IntelTest
    if (!(readPortState(port) & PD_S_TX_BUSY)) // restart TX engine if idle
        calloutEntryDispatch(port->FrameTOEntry);

    outb(IRQ_ENABLE, (port->IERmask) & (RX_DATA_AVAIL_IRQ_EN | TX_DATA_EMPTY_IRQ_EN
                | LINE_STAT_IRQ_EN | MODEM_STAT_IRQ_EN));
#endif
    DLOG("End Act State %x\n", (int)readPortState(port));
    return kIOReturnSuccess;
}

/*---------------------------------------------------------------------
 *		deactivatePort
 *ejk		Sat, Dec 27, 1997	16:48
 *		Comment Me
 *
 *-------------------------------------------------------------------*/
void AppleSCCSerial::deactivatePort(PortInfo_t *port)
{
    DLOG("deactivatePort");

    if (readPortState(port) & PD_S_ACTIVE) {
// FIXTHIS	outb(IRQ_ENABLE, (port->IERmask) & MODEM_STAT_IRQ_EN);
        // Clear active and wakeup all sleepers
        changeState(port, 0, PD_S_ACTIVE);
        //freeRingBuffer(&(port->TX));
        //freeRingBuffer(&(port->RX));

        //changeState(port, flowMachine(port), FLOW_RX_AUTO);

        /* Tell the chip to turn itself off.*/
        SccCloseChannel(port);
        //CloseQueue(&port->TX);
        //CloseQueue(&port->RX);
#ifdef DEBUG
        DLOG("%s: Ints=%ld TXInts=%ld RXInts=%ld mdmInts=%ld\n",
                    port->PortName, port->Stats.ints, port->Stats.txInts,
                    port->Stats.rxInts, port->Stats.mdmInts);
        DLOG("%s: txChars=%ld rxChars=%ld\n",
                port->PortName, port->Stats.txChars, port->Stats.rxChars);
#endif
    }
}

/* Below this line added by Elias
*/
void AppleSCCSerial::CheckQueues(PortInfo_t *port)
{
    UInt32	Used;
    UInt32	Free;
    UInt32	OldState;
    UInt32	DeltaState;
	

    // This lock is the fix for 2977847.  The PD_S_TX_BUSY flag was getting cleared during
    // the execution of this routine by tx interrupt code, and but then this routine was
    // incorrectly restoring OldState (and the PD_S_TX_BUSY bit) at exit.
    
    IORecursiveLockLock(port->serialRequestLock);
	
    OldState = readPortState(port);

    /* Check to see if there is anything in the Transmit buffer. */
    Used = UsedSpaceinQueue(&(port->TX));
    Free = FreeSpaceinQueue(&(port->TX));

    if (Free == 0) {
        OldState |= PD_S_TXQ_FULL;
        OldState &= ~PD_S_TXQ_EMPTY;
    }
    else if (Used == 0)  {
        OldState &= ~PD_S_TXQ_FULL;
        OldState |= PD_S_TXQ_EMPTY;
    }
    else { 
        OldState &= ~PD_S_TXQ_FULL;
        OldState &= ~PD_S_TXQ_EMPTY;
    }
    
    /* Check to see if we are below the low water mark.
        */
    if (Used < port->TXStats.LowWater)
        OldState |= PD_S_TXQ_LOW_WATER;
    else
        OldState &= ~PD_S_TXQ_LOW_WATER;

    if (Used > port->TXStats.HighWater)
        OldState |= PD_S_TXQ_HIGH_WATER;
    else
        OldState &= ~PD_S_TXQ_HIGH_WATER;


    /* Check to see if there is anything in the Receive buffer.
        */
    Used = UsedSpaceinQueue(&(port->RX));
    Free = FreeSpaceinQueue(&(port->RX));

    if (Free == 0) {
        OldState |= PD_S_RXQ_FULL;
        OldState &= ~PD_S_RXQ_EMPTY;
    }
    else if (Used == 0)  {
        OldState &= ~PD_S_RXQ_FULL;
        OldState |= PD_S_RXQ_EMPTY;
    }
    else {
        OldState &= ~PD_S_RXQ_FULL;
        OldState &= ~PD_S_RXQ_EMPTY;
    }

	//**************************************************/
	// Begin software flow control code
	//**************************************************/

	UInt32	SW_FlowControl = port->FlowControl & PD_RS232_A_RXO;
	UInt32	CTS_FlowControl = port->FlowControl & PD_RS232_A_CTS;
	UInt32	DTR_FlowControl = port->FlowControl & PD_RS232_A_DTR;
	
	//**************************************************/
	// End software flow control code
	//**************************************************/

    /* Check to see if we are below the low water mark.
    */
    if (Used < port->RXStats.LowWater)
	{
		//**************************************************/
		// Begin software flow control code
		//**************************************************/
	
		if ((SW_FlowControl) && (port->xOffSent))
		{
			port->xOffSent = false;
			AddBytetoQueue(&(port->TX), port->XONchar);
			SetUpTransmit(port);
		}

		//**************************************************/
		// End software flow control code
		//**************************************************/
			
        OldState |= PD_S_RXQ_LOW_WATER;
	}
    else
        OldState &= ~PD_S_RXQ_LOW_WATER;

    /* Check to see if we are above the high water mark.
    */
    if (Used > port->RXStats.HighWater)
	{
		//**************************************************/
		// Begin software flow control code
		//**************************************************/
	
		if ((SW_FlowControl) && (!port->xOffSent))
		{
			port->xOffSent = true;
			AddBytetoQueue(&(port->TX), port->XOFFchar);
			SetUpTransmit(port);
		}

		if (CTS_FlowControl)
		{
			// Need to set a software overrun error flag
		}

		if (DTR_FlowControl && port->DTRAsserted)
		{
			DLOG("Deassert DTR\n");
			port->DTRAsserted = false;
			SccSetDTR(port, false);
		}

		//**************************************************/
		// End software flow control code
		//**************************************************/
	}
    else
   {
		if (DTR_FlowControl && !port->DTRAsserted)
		{
			DLOG("Assert DTR\n");
			port->DTRAsserted = true;
			SccSetDTR(port, true);
		}

        OldState &= ~PD_S_RXQ_HIGH_WATER;

		if (port->aboveRxHighWater)
		{
			port->aboveRxHighWater = false;
		}
    }
    /* Figure out what has changed to get mask.*/
    DeltaState = OldState ^ readPortState(port);
    changeState(port, OldState, DeltaState);
    
    IORecursiveLockUnlock(port->serialRequestLock);

}

/*---------------------------------------------------------------------
 *		SetStructureDefaults
 *ejk		Mon, Jan 12, 1998	11:22 PM
 *		Comment Me
 *
 *-------------------------------------------------------------------*/
void AppleSCCSerial::SetStructureDefaults(PortInfo_t *port, bool Init)
{
    UInt32	tmp;
	
    /* These are set up at probe and connot get reset during execution.
    */
    if (Init) {
        //SccSetupTansmissionChannel
        SerialDBDMAStatusInfo *dmaInfo = &port->TxDBDMAChannel;
        
        /* creates the transmit buffer */
        dmaInfo->dmaTransferSize = 0;
        
        dmaInfo->dmaTransferBufferMDP = IOBufferMemoryDescriptor::withOptions(0, PAGE_SIZE, PAGE_SIZE);
        if (dmaInfo->dmaTransferBufferMDP == NULL)
            return;
		
        dmaInfo->dmaTransferBufferMDP->prepare();
        dmaInfo->dmaTransferBuffer = (UInt8 *)dmaInfo->dmaTransferBufferMDP->getBytesNoCopy();
        bzero(dmaInfo->dmaTransferBuffer, PAGE_SIZE);
        
//        dmaInfo->dmaTransferBuffer = (UInt8*)IOMallocContiguous(PAGE_SIZE, PAGE_SIZE,NULL);
//        if (dmaInfo->dmaTransferBuffer == NULL) {
//            return;
//        }
//        else
//            bzero(dmaInfo->dmaTransferBuffer , PAGE_SIZE);

        dmaInfo->dmaNumberOfDescriptors = 2;
        
        dmaInfo->dmaChannelCommandAreaMDP = IOBufferMemoryDescriptor::withOptions(0, PAGE_SIZE, PAGE_SIZE);
        if (dmaInfo->dmaChannelCommandAreaMDP == NULL)
        {
            dmaInfo->dmaNumberOfDescriptors = 0;
            return;
        }
		
        dmaInfo->dmaChannelCommandAreaMDP->prepare();
        dmaInfo->dmaChannelCommandArea = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getBytesNoCopy();
        bzero(dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors);

//        dmaInfo->dmaChannelCommandArea =  (IODBDMADescriptor *)IOMallocContiguous(PAGE_SIZE, PAGE_SIZE,NULL);
//        if (dmaInfo->dmaChannelCommandArea == NULL) {
//            dmaInfo->dmaNumberOfDescriptors = 0;
//            return;
//        }
//        else
//            bzero(dmaInfo->dmaChannelCommandArea , sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors);

        //SccSetupReceptionChannel
    dmaInfo = &port->RxDBDMAChannel;
    dmaInfo->dmaNumberOfDescriptors = PAGE_SIZE / sizeof(IODBDMADescriptor);
    
    dmaInfo->dmaChannelCommandAreaMDP = IOBufferMemoryDescriptor::withOptions(0, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, PAGE_SIZE);
    if (dmaInfo->dmaChannelCommandAreaMDP == NULL)
    {
        dmaInfo->dmaNumberOfDescriptors = 0;
        return;
    }
    
    dmaInfo->dmaChannelCommandAreaMDP->prepare();
    dmaInfo->dmaChannelCommandArea = (IODBDMADescriptor *)dmaInfo->dmaChannelCommandAreaMDP->getBytesNoCopy();
    bzero(dmaInfo->dmaChannelCommandArea, sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors);
    
//    dmaInfo->dmaChannelCommandArea =  (IODBDMADescriptor *)IOMallocContiguous(sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors, PAGE_SIZE, NULL);
    
    //IOLog("Number of Descriptors: %d. Total Size: %d \n",dmaInfo->dmaNumberOfDescriptors, (sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors));
    //IOLog("Size of IODBDMADescriptor: %d. \n",sizeof(IODBDMADescriptor));
    
//    if (dmaInfo->dmaChannelCommandArea == NULL) {
//        dmaInfo->dmaNumberOfDescriptors = 0;
//        return;
//    }
//    else
//        bzero(dmaInfo->dmaChannelCommandArea , sizeof(IODBDMADescriptor) * dmaInfo->dmaNumberOfDescriptors);

    /* creates the receive buffer, since each command transfers one byte, we need
       as many bytes as commands */
    dmaInfo->dmaTransferSize = dmaInfo->dmaNumberOfDescriptors;
    
    dmaInfo->dmaTransferBufferMDP = IOBufferMemoryDescriptor::withOptions(0, dmaInfo->dmaNumberOfDescriptors, PAGE_SIZE);
    if (dmaInfo->dmaTransferBufferMDP == NULL)
    {
        return;
    }
		
    dmaInfo->dmaTransferBufferMDP->prepare();
    dmaInfo->dmaTransferBuffer = (UInt8 *)dmaInfo->dmaTransferBufferMDP->getBytesNoCopy();
    bzero(dmaInfo->dmaTransferBuffer, dmaInfo->dmaNumberOfDescriptors);
    
//    dmaInfo->dmaTransferBuffer = (UInt8*)IOMallocContiguous(dmaInfo->dmaNumberOfDescriptors, PAGE_SIZE,NULL);
//    if (dmaInfo->dmaTransferBuffer == NULL) {
//        return;
//    }
//    else
//        bzero(dmaInfo->dmaTransferBuffer , PAGE_SIZE);


        port->Instance 					= 0;
        port->PortName 					= NULL;
        port->MasterClock 				= CHIP_CLOCK_DEFAULT;
        port->DLRimage 					= 0x0000;
        port->LCRimage 					= 0x00;
        port->FCRimage 					= 0x00;
        port->IERmask 					= 0x00;
        port->RBRmask 					= 0x00;
        port->Base 						= 0x0000;
        port->DataLatInterval.tv_sec 	= 0;
        port->DataLatInterval.tv_nsec 	= 0;
        port->CharLatInterval.tv_sec 	= 0;
        port->CharLatInterval.tv_nsec 	= 0;
        port->HeartBeatInterval.tv_sec 	= 0;
        port->HeartBeatInterval.tv_nsec = 0;
        port->State 					= (PD_S_TXQ_EMPTY | PD_S_TXQ_LOW_WATER | PD_S_RXQ_EMPTY | PD_S_RXQ_LOW_WATER);
        port->WatchStateMask 			= 0x00000000;
        
        
    }
    port->BreakLength 			= 0;
    port->BaudRate 				= 0;
    port->CharLength 			= 0;
    port->BreakLength 			= 0;
    port->StopBits 				= 0;
    port->TX_Parity 			= 0;
    port->RX_Parity 			= 0;
    port->WaitingForTXIdle		= false;
    port->MinLatency 			= false;
    port->XONchar 				= '\x11';
    port->XOFFchar 				= '\x13';
    port->FlowControl 			= 0x00000000;
	port->FlowControlState		= CONTINUE_SEND;
    port->RXOstate 				= IDLE_XO;
    port->TXOstate 				= IDLE_XO;
    port->FrameTOEntry 			= NULL;
    port->DataLatTOEntry 		= NULL;
    port->FrameInterval.tv_sec 	= 0;
    port->FrameInterval.tv_nsec = 0;

    port->RXStats.BufferSize 	= BUFFER_SIZE_DEFAULT;
    port->RXStats.HighWater 	= (port->RXStats.BufferSize << 1) / 3;
    port->RXStats.LowWater 		= port->RXStats.HighWater >> 1;

    port->TXStats.BufferSize 	= BUFFER_SIZE_DEFAULT;
    port->TXStats.HighWater 	= (port->RXStats.BufferSize << 1) / 3;
    port->TXStats.LowWater 		= port->RXStats.HighWater >> 1;

    port->FlowControl 			= DEFAULT_NOTIFY;

    port->AreTransmitting 		= FALSE;

    for (tmp=0; tmp<(256>>SPECIAL_SHIFT); tmp++)
            port->SWspecial[tmp] = 0;

    port->Stats.ints 		= 0;
    port->Stats.txInts 		= 0;
    port->Stats.rxInts 		= 0;
    port->Stats.mdmInts 	= 0;
    port->Stats.txChars 	= 0;
    port->Stats.rxChars 	= 0;
    port->DCDState 			= false;
    
    port->lastCTSTime = 0;
    port->ctsTransitionCount = 0;
}

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **
 *This is where the bulk of the chip driver logic lives.  It isn't pretty.
 *This section includes the interrupt handler and a couple callouts
 *that are used to simulate interrupts:
 *	dataLatTOHandler(port)
 *	frameTOHandler(port)
 *	signalTOHandler(port)
 ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */
void AppleSCCSerial::dataLatTOHandler(PortInfo_t *port)
{
    DLOG("dataLatTOHandler\n");
	
    //IOSpinlockLock(port->serialRequestLock);
    //RX_enqueueLongEvent(port, PD_E_DATA_LATENCY, 0);
    //if (port->RX.Count >= port->RX.Enqueue)
    //changeState(port, generateRXQState(port), PD_S_RXQ_MASK | FLOW_RX_AUTO);
    //IOSpinlockUnlock(port->serialRequestLock);
}

/*
 *Frame TimeOut:
 *This timeout is used to simulate an interrupt after the transmitter
 *shift-register goes idle.  Hardware interrupts are only available when
 *the holding register is empty.  When setting the baud rate, or a line
 *break, it is important to wait for the xmit shift register to finish
 *its job first.
 */
void AppleSCCSerial::frameTOHandler(PortInfo_t *port)
{
    DLOG("frameToHandler\n");

    //IOSpinlockLock(port->serialRequestLock);
    port->WaitingForTXIdle = false;
    PPCSerialISR(NULL, NULL, port);
    //IOSpinlockUnlock(port->serialRequestLock);
}

void AppleSCCSerial::delayTOHandler(PortInfo_t *port)
{
    UInt32 currentState = readPortState(port);
    DLOG("delayTOHandler\n");

    assert (currentState & INTERNAL_DELAY);
    //IOSpinlockLock(port->serialRequestLock);
    currentState &= ~INTERNAL_DELAY;
    changeState(port, currentState, currentState);
    PPCSerialISR(NULL, NULL, port);
    //IOSpinlockUnlock(port->serialRequestLock);
}

void AppleSCCSerial::heartBeatTOHandler(PortInfo_t *port)
{
    DLOG("heartBeatTOHandler\n");

    assert (tval2long(port->HeartBeatInterval) > 0);
    //IOSpinlockLock(port->serialRequestLock);

//	if (!port->JustDoneInterrupt) {
    PPCSerialISR(NULL, NULL, port);
//	}
//	port->JustDoneInterrupt = NO;
        //calloutEntryDispatchDelayed(	port->HeartBeatTOEntry,
        //        			calloutDeadlineFromInterval(port->HeartBeatInterval)); //*** thread related problems?
    ////IOSpinlockUnlock(port->serialRequestLock);
}

/* executeEvent causes the specified event to be processed immediately.
 */
IOReturn AppleSCCSerial::
executeEvent(PortInfo_t *port, UInt32 event, UInt32 data, UInt32 *state, UInt32 *delta)
{

    IOReturn ret = kIOReturnSuccess;
    UInt32 tmp;
    unsigned char 		clockMode; 	// for MIDI clock mode

    DLOG("ExecuteEvent %d %d\n",(int)event,(int)data);

    if ((readPortState(port) & PD_S_ACQUIRED) == 0)
        return kIOReturnNotOpen;

    switch (event) {
        case PD_RS232_E_XON_BYTE:
            port->XONchar = data;
            break;
        case PD_RS232_E_XOFF_BYTE:
            port->XOFFchar = data;
            break;
        case PD_E_SPECIAL_BYTE:
            port->SWspecial[data>>SPECIAL_SHIFT] |= (1 << (data & SPECIAL_MASK));
            break;

        case PD_E_VALID_DATA_BYTE:
            port->SWspecial[data>>SPECIAL_SHIFT] &= ~(1 << (data & SPECIAL_MASK));
            break;

// Enqueued flow control event allows the user to change the state of any flow control
// signals set on Manual (its Auto bit in exec/req event is cleared)
        case PD_E_FLOW_CONTROL :
DLOG("-ExecuteEvent PD_E_FLOW_CONTROL\n");
            tmp = data & (PD_RS232_D_RFR | PD_RS232_D_DTR | PD_RS232_D_RXO);
            tmp >>= PD_RS232_D_SHIFT;
            tmp &= ~(port->FlowControl);
            *delta |= tmp;
            *state &= ~tmp;
            *state |= (tmp & data);
            if (tmp & PD_RS232_S_RXO) {
                if (data & PD_RS232_S_RXO)
				{
                    port->RXOstate = NEEDS_XON;
				}
                else
				{
                    port->RXOstate = NEEDS_XOFF;
				}
            }
#if 0
            programMCR(port, *state);
#endif
            break;

        case PD_E_ACTIVE:
DLOG("-ExecuteEvent PD_E_ACTIVE\n");
            if ((bool)data)
                ret = activatePort(port);
            else
                deactivatePort(port);
            break;

          //Fix for IRDA and MIDI which want to know about bytes as soon as it arrives      
        case PD_E_DATA_LATENCY:
                port->DataLatInterval = long2tval(data *1000);
               // IOLog("Setting PD_E_DATA_LATENCY to: %ld \n",data);
            break;

        case PD_RS232_E_MIN_LATENCY:
            port->MinLatency = bool(data);
            port->DLRimage = 0x0000;
            programChip(port);
            break;

        case PD_E_DATA_INTEGRITY:
DLOG("-ExecuteEvent PD_E_DATA_INTEGRITY\n");
            if ((data < PD_RS232_PARITY_NONE) || (data > PD_RS232_PARITY_SPACE)) {
                ret = kIOReturnBadArgument;
            }
            else {
                port->TX_Parity = data;
                port->RX_Parity = PD_RS232_PARITY_DEFAULT;
                if (!SccSetParity(port, (ParityType)data))
                    ret = kIOReturnBadArgument;
            }
            break;

        case PD_E_DATA_RATE:
            /* For API compatiblilty with Intel.
            */
            data >>=1;
DLOG("-ExecuteEvent PD_E_DATA_RATE %d\n",(int)data);
            if ((data < MIN_BAUD) || (data > kMaxBaudRate))
                ret = kIOReturnBadArgument;
            else {
                port->BaudRate = data;
                // for DCP -- begin
				if ((port->whichPort == serialPortA) && (port->gDCPModemFound))
				{	// force the connect to be 115k for DCP
//					IOLog ("AppleSCCSerial::executeEvent -- forcing the baudrate to be 115k\n");
					if (!SccSetBaud(port, 115200))
					{
//						IOLog ("AppleSCCSerial::executeEvent -- forcing the baudrate to be 115k failed\n");
						ret = kIOReturnBadArgument;
					}
				}	// for DCP -- end
                else
				{
                if (!SccSetBaud(port, data))
                    ret = kIOReturnBadArgument;
            }

            }
            break;

		case PD_E_EXTERNAL_CLOCK_MODE:
			/*  For compatiblilty with MIDI.
			*/
DLOG("-ExecuteEvent PD_E_EXTERNAL_CLOCK_MODE data = %d\n", data);
			switch (data) {
				case kX1UserClockMode:				// PPCSerialPort.h for value
					clockMode = kX1ClockMode;		// Z85C30.h for value
					break;
				case kX16UserClockMode:
					clockMode = kX16ClockMode;
					break;
				case kX32UserClockMode:
					clockMode = kX32ClockMode;
					break;
				case kX64UserClockMode:
					clockMode = kX64ClockMode;
					break;
				default :
					clockMode = kX32ClockMode;
					break;
			}	
			if (!SccConfigureForMIDI(port, clockMode))
				ret = kIOReturnBadArgument;
			break;

        case PD_E_DATA_SIZE:
            /* For API compatiblilty with Intel.
            */
            data >>=1;
            DLOG("-ExecuteEvent PD_E_DATA_SIZE data = %d\n", data);
            if ((data < 5) || (data > 8))
                ret = kIOReturnBadArgument;
            else {
                port->CharLength = data;
                if (!SccSetDataBits(port, data))
                    ret = kIOReturnBadArgument;
            }
            break;
        case PD_RS232_E_STOP_BITS:
            DLOG("-ExecuteEvent PD_RS232_E_STOP_BITS\n");
            if ((data < 0) || (data > 20))
                ret = kIOReturnBadArgument;
            else {
                port->StopBits = data;
                if (!SccSetStopBits(port, data))
                    ret = kIOReturnBadArgument;
            }
            break;
        case PD_E_RXQ_FLUSH:
#ifdef IntelTest
                port->RX.Input = port->RX.Output = port->RX.Base;
                port->RX.Count = port->RX.OverRun = 0;
                if ((port->Type) > NS16550)
                        outb(FIFO_CTRL, ((port->FCRimage) | RX_FIFO_RESET));
                *state &= ~(PD_S_RXQ_MASK | FLOW_RX_AUTO);
//		*state |= generateRXQState(port);
                *delta |= PD_S_RXQ_MASK | FLOW_RX_AUTO;
#endif
                break;

        case PD_E_RX_DATA_INTEGRITY:
            if ((data != PD_RS232_PARITY_DEFAULT) && (data != PD_RS232_PARITY_ANY))
                ret = kIOReturnBadArgument;
            else
                port->RX_Parity = data;
            break;
        case PD_E_RX_DATA_RATE:
            if (data != 0)
                ret = kIOReturnBadArgument;
            break;
        case PD_E_RX_DATA_SIZE:
            if (data != 0)
                ret = kIOReturnBadArgument;
            break;
        case PD_RS232_E_RX_STOP_BITS:
            if (data != 0)
                ret = kIOReturnBadArgument;
            break;
        case PD_E_TXQ_FLUSH:
#ifdef IntelTest
                port->TX.Input = port->TX.Output = port->TX.Base;
                port->TX.Count = 0;
                if ((port->Type) > NS16550)
                        outb(FIFO_CTRL, ((port->FCRimage) | TX_FIFO_RESET));
                *state &= ~PD_S_TXQ_MASK;
                *state |= generateTXQState(port);
                *delta |= PD_S_TXQ_MASK;
#endif
            break;
        case PD_RS232_E_LINE_BREAK :
            *state &= ~PD_RS232_S_BRK;
//		*state |= lineBreak(port, ((BOOL)data));
            *delta |= PD_RS232_S_BRK;
            break;
        case PD_E_DELAY:
            assert(!((*state) & INTERNAL_DELAY));
            if (data > 0) {
                if (data > (UINT_MAX/1000))
                    data = (UINT_MAX/1000);
                *state |= INTERNAL_DELAY;
                //calloutEntryDispatchDelayed(port->DelayTOEntry, calloutDeadlineFromInterval(long2tval(data*1000)));
            }
            break;
        case PD_E_RXQ_HIGH_WATER:
#if 0
                port->RX.HighWater = data;
        if ((port->RX.HighWater) > ((port->RX.Size) - BIGGEST_EVENT))
                (port->RX.HighWater) = ((port->RX.Size) - BIGGEST_EVENT);
                if ((port->RX.HighWater) < (BIGGEST_EVENT *2))
                        (port->RX.HighWater) = (BIGGEST_EVENT *2);
        if ((port->RX.LowWater) > ((port->RX.HighWater) - BIGGEST_EVENT))
                (port->RX.LowWater) = ((port->RX.HighWater) - BIGGEST_EVENT);
                *state &= ~(PD_S_RXQ_MASK | FLOW_RX_AUTO);
//		*state |= generateRXQState(port);
                *delta |= PD_S_RXQ_MASK | FLOW_RX_AUTO;
#endif
                break;

          case PD_E_RXQ_LOW_WATER:
#if 0
                port->RX.LowWater = data;
        if ((port->RX.LowWater) > ((port->RX.HighWater) - BIGGEST_EVENT))
                (port->RX.LowWater) = ((port->RX.HighWater) - BIGGEST_EVENT);
                if ((port->RX.LowWater) < BIGGEST_EVENT)
                        (port->RX.LowWater) = BIGGEST_EVENT;
                *state &= ~(PD_S_RXQ_MASK | FLOW_RX_AUTO);
//		*state |= generateRXQState(port);
                *delta |= PD_S_RXQ_MASK | FLOW_RX_AUTO;
#endif
                break;

          case PD_E_TXQ_HIGH_WATER:
#if 0
                port->RX.HighWater = data;
        if ((port->TX.HighWater) > ((port->TX.Size) - BIGGEST_EVENT))
                (port->TX.HighWater) = ((port->TX.Size) - BIGGEST_EVENT);
                if ((port->TX.HighWater) < (BIGGEST_EVENT *2))
                        (port->TX.HighWater) = (BIGGEST_EVENT *2);
        if ((port->TX.LowWater) > ((port->TX.HighWater) - BIGGEST_EVENT))
                (port->TX.LowWater) = ((port->TX.HighWater) - BIGGEST_EVENT);
                *state &= ~PD_S_TXQ_MASK;
//		*state |= generateTXQState(port);
                *delta |= PD_S_TXQ_MASK;
#endif
                break;

          case PD_E_TXQ_LOW_WATER:
#if 0
                port->TX.LowWater = data;
        if ((port->TX.LowWater) > ((port->TX.HighWater) - BIGGEST_EVENT))
                (port->TX.LowWater) = ((port->TX.HighWater) - BIGGEST_EVENT);
                if ((port->TX.LowWater) < BIGGEST_EVENT)
                        (port->TX.LowWater) = BIGGEST_EVENT;
                *state &= ~PD_S_TXQ_MASK;
//		*state |= generateTXQState(port);
                *delta |= PD_S_TXQ_MASK;
#endif
                break;
                
          default:
                ret = kIOReturnBadArgument;
                break;
        }
/* ejk for compiler warnings.
*/
*state |= *state;
return ret;
}

/* freeRingBuffer() : Accepts a queue as its argument.
 *First, it frees all resources assocated with the queue,
 *then sets all queue parameters to safe values.
 */
void AppleSCCSerial::freeRingBuffer(CirQueue *Queue)
{

    DLOG("In freeRingBuffer\n");

    if (Queue->Start) {
        IOFree(Queue->Start, Queue->Size);
        CloseQueue(Queue);
    }
}

/* allocateRingBuffer() : Accepts a queue as its argument.
 *First, it allocates resources needed by the queue,
 *then sets up all queue parameters.
 */
bool AppleSCCSerial::allocateRingBuffer(CirQueue *Queue, size_t BufferSize)
{
    u_char		*Buffer;

    DLOG("In allocateRingBuffer\n");
//    Buffer = (u_char *)IOMalloc(kMaxCirBufferSize);
    Buffer = (u_char *)IOMalloc(BufferSize);

//    InitQueue(Queue, Buffer, kMaxCirBufferSize);
    InitQueue(Queue, Buffer, BufferSize);

    if (Buffer)
        return true;
    else
        return false;
}

/*
 *readPortState
 *
 *Reads the  current Port->State. This is in a specific function so that
 *I know it is interrupt-preotected.
 *
 */
UInt32 AppleSCCSerial::readPortState(PortInfo_t *port)
{
//rs!IOSimpleLockLockDisableInterrupt(port->serialRequestLock);
	IORecursiveLockLock(port->serialRequestLock);
    UInt32 returnState = port->State;
//rs!    IOSimpleLockUnlockEnableInterrupt(port->serialRequestLock, previousInterruptState);
    IORecursiveLockUnlock(port->serialRequestLock);

    return returnState;
}

/*
 *changeState
 *
 *Changes the bits which are indicated by mask of the current Port->State
 *to the bits indicated by mask of state
 *
 *if state = 0 the mask bits are cleared
 *if mask = 0 nothing is changed
 *
 *delta contains the difference between the new and old state taking the
 *mask into account
 *
 */

void AppleSCCSerial::changeState(PortInfo_t *port, UInt32 state, UInt32 mask)
{
    UInt32	delta;

    //DLOG("++>changeState(state%x, mask%x, port->state%x) watchStateMask%x\n",(int)state, (int)mask, (int)port->State, (int)port->WatchStateMask);

    if (port &&  port->serialRequestLock)
    {

        IORecursiveLockLock(port->serialRequestLock);

	//7/13/01-hsjb - do we need to do the same for RTS?

	if ((mask & PD_RS232_S_DTR) && ((port->FlowControl & PD_RS232_A_DTR) != PD_RS232_A_DTR))
    {
		if ((state & PD_RS232_S_DTR) != (port->State & PD_RS232_S_DTR))
		{
			if (state & PD_RS232_S_DTR)
			{
				DLOG("*** DTR ON\n");
				SccSetDTR(port, true);
			}
			else
			{
				DLOG("*** DTR OFF\n");
				SccSetDTR(port, false);
			}
		}
    }

	state = (port->State & ~mask) | (state & mask); // compute the new state
    delta = (state ^ port->State);		    // keep a copy of the diffs
    port->State = state;



    // Wake up all threads asleep on WatchStateMask
    if (delta & port->WatchStateMask) {
        //DLOG("changeState Calling thread_wakeup\n");
        thread_wakeup_with_result (&port->WatchStateMask, THREAD_RESTART);
    }
    IORecursiveLockUnlock(port->serialRequestLock);

 }
    
    //DLOG("-->changeState %x \nstate %x\n p->wsm%x \ndelta %x\nmask %x\n", (int)port->State, state, port->WatchStateMask, delta, mask);
}

/* watchState() :
 *Wait for the at least one of the state bits defined in mask to be equal
 *to the value defined in state. Check on entry then sleep until necessary.
 *A return value of kIOReturnSuccess means that at least one of the port state
 *bits specified by mask is equal to the value passed in by state.  A return
 *value of IO_R_IO indicates that the port went inactive.  A return value of
 *IO_R_IPC_FAILURE indicates sleep was interrupted by a signal.
 */
IOReturn AppleSCCSerial::watchState(PortInfo_t *port, UInt32 *state, UInt32 mask)
{
    unsigned 		watchState, foundStates;
    bool 		autoActiveBit 	= false;
    IOReturn 		rtn 		= kIOReturnSuccess;
//    IOInterruptState	previousInterruptState;
    
    DLOG("[WatchState..");
    watchState = *state;
//    previousInterruptState = IOSimpleLockLockDisableInterrupt(port->serialRequestLock);
    IORecursiveLockLock(port->serialRequestLock);

    // hack to get around problem with carrier detection
/*
    if (*state & PD_RS232_S_CAR) {
        port->State |= PD_RS232_S_CAR;        
#if 0	//7/13/01 hsjb- This is a "watch" state, not a "set" state call, Also this breaks irda!
        SccSetDTR(port, true);
#endif
    }
*/
    if (!(mask & (PD_S_ACQUIRED | PD_S_ACTIVE))) {
        watchState &= ~PD_S_ACTIVE;	// Check for low PD_S_ACTIVE
        mask       |=  PD_S_ACTIVE;	// Register interest in PD_S_ACTIVE bit
        autoActiveBit = true;
    }

    for (;;) {
        // Check port state for any interesting bits with watchState value
        // NB. the '^ ~' is a XNOR and tests for equality of bits.
        foundStates = (watchState ^ ~(port->State)) & mask;

        if (foundStates) {
            *state = port->State;
            if (autoActiveBit && (foundStates & PD_S_ACTIVE))
                rtn = kIOReturnIOError;
            else
                rtn = kIOReturnSuccess;
            break;
        }

        //
        // Everytime we go around the loop we have to reset the watch mask.
        // This means any event that could affect the WatchStateMask must
        // wakeup all watch state threads.  The two event's are an interrupt
        // or one of the bits in the WatchStateMask changing.
        //
//rs!        IOSimpleLockLock(port->WatchLock);
        IOLockLock(port->WatchLock);
        port->WatchStateMask |= mask;

        // note: Interrupts need to be locked out completely here, since as assertwait is called
        // other threads waiting on &port->WatchStateMask will be woken up and spun through the loop
        // if an interrupts occurs at this point then the current thread will end up waiting with a different
        // port state than assumed -- this problem was causing dequeueData to wait for a change
        // in PD_E_RXQ_EMPTY to 0 after an interrupt had already changed it to 0.
        assert_wait(&port->WatchStateMask, true);       /* assert event */
//rs!        IOSimpleLockUnlock(port->WatchLock);          /* release the lock */
        IOLockUnlock(port->WatchLock);          /* release the lock */
//        IOSimpleLockUnlockEnableInterrupt(port->serialRequestLock, previousInterruptState);
        IORecursiveLockUnlock(port->serialRequestLock);
//        rtn = thread_block((void (*)(void)) 0);                       /* block ourselves */
        rtn = thread_block(THREAD_CONTINUE_NULL);                       /* block ourselves */
//rs!        previousInterruptState = IOSimpleLockLockDisableInterrupt(port->serialRequestLock);
        IORecursiveLockLock(port->serialRequestLock);

        if (rtn == THREAD_RESTART) {
            continue;
        }
        else {
            rtn = kIOReturnIPCError;
            break;
        }
    }
    DLOG("]\n");

    // As it is impossible to undo the masking used by this
    // thread, we clear down the watch state mask and wakeup
    // every sleeping thread to reinitialize the mask before exiting.
//rs!    IOSimpleLockLock(port->WatchLock);
    IOLockLock(port->WatchLock);
    port->WatchStateMask = 0;
//    IOSimpleLockUnlock(port->WatchLock);
    IOLockUnlock(port->WatchLock);
    
    thread_wakeup_with_result(&port->WatchStateMask, THREAD_RESTART);
//    IOSimpleLockUnlockEnableInterrupt(port->serialRequestLock, previousInterruptState);
	IORecursiveLockUnlock(port->serialRequestLock);
    return rtn;
}

#if USE_WORK_LOOPS
bool AppleSCCSerial::interruptFilter(OSObject* obj, IOFilterInterruptEventSource * source)
{
    // check if this interrupt belongs to me
	int	interruptIndex = source->getIntIndex();
	if (interruptIndex == kIntRxDMA)
	{
		//IOLog("Rx DMA Interrupt Filtered\n");
		return true;// go ahead and invoke completion routine
	}
	
	if (interruptIndex == kIntTxDMA)
	{
		//IOLog("Tx DMA Interrupt Filtered\n");
		return true;// go ahead and invoke completion routine
	}
	
	//IOLog("NOT Rx or Tx Interrupt Filtered\n");
	return false;
}


void AppleSCCSerial::interruptHandler(OSObject* obj, IOInterruptEventSource * source, int count)
{
    // handle the interrupt
	int	interruptIndex = source->getIntIndex();//who are you? what do you want? who do you serve?
	
	if (interruptIndex == kIntChipSet)
	{
		//IOLog("Handle SCC Interrupt\n");
		handleInterrupt(obj, 0, NULL, 0);
		return;
	}
		
	if (interruptIndex == kIntRxDMA)
	{
		//IOLog("Handle Rx DMA Interrupt\n");
		handleDBDMARxInterrupt(obj, 0, NULL, 0);
		return;
	}
		
	if (interruptIndex == kIntTxDMA)
	{
		//IOLog("Handle Tx DMA Interrupt\n");
		handleDBDMATxInterrupt(obj, 0, NULL, 0);
	}
}
#endif

void AppleSCCSerial::callCarrierhack(thread_call_param_t whichDevice, thread_call_param_t carrier)
{
    AppleSCCSerial *self = (AppleSCCSerial *) whichDevice;

    if (self->modemDCDStateChangeCallback)
        (*self->modemDCDStateChangeCallback)(self->fModemObject, (bool) carrier);
    
//    IOLog("callCarrierhack: carrier %d count: %d\n",carrier,self->fCarrierHackCount );
    OSDecrementAtomic(&self->fCarrierHackCount);
}

void AppleSCCSerial::setCarrierHack(OSObject *target, SCCCarrierHack action)
{
    modemDCDStateChangeCallback = action;
    fModemObject = target;
 }



bool AppleSCCSerial::NonSCCModem()
{
    IORegistryEntry *		next;
    IORegistryIterator * 	iter;
	bool					stopWhile = false;
	bool					found = false;

    iter = IORegistryIterator::iterateOver( gIODTPlane );
    assert( iter );
    iter->reset();
    while ((stopWhile == false) && (next = iter->getNextObjectRecursive()))
	{
		if (0 == strcmp ("i2c-modem", next->getName()))
		{
			stopWhile = true;
			OSData *s2 = OSDynamicCast(OSData, next->getProperty("modem-id"));
			if (s2)
			{
				char *tmpID = (char *) s2->getBytesNoCopy();
				tmpID++;	// need to look at the second byte which contains the modem ID
				switch (*tmpID)
				{
					case 0x04 :
					case 0x05 :
					case 0x07 :
					case 0x08 :
					case 0x0b :
					case 0x0c :
						found = true;
						break;
				}
			}
		}
    }
    iter->release();
	return found;
}	// end of AppleSCCSerial::NonSCCModem

UInt32 SCC_GetSystemTime(void)
{
    AbsoluteTime atTime;
    UInt64       nsTime;

    clock_get_uptime(&atTime);
    absolutetime_to_nanoseconds( atTime, &nsTime);

    return (UInt32)(nsTime/1000000);
}

void SCC_TRACEMSG(char *msg)    
{
    UInt32 dwTime = SCC_GetSystemTime();
    IOLog("%07ld.%03ld: ", dwTime/1000, dwTime%1000);
    IOLog (msg);
}



// for DCP -- begin

// ------------------------------------------------------------------------------
// Search for a specified internal modem
bool AppleSCCSerial::LookForInternalModem(UInt8 modemID = 0)
{
    IORegistryEntry *		next;
    IORegistryIterator * 	iter;
	bool					found = false;

//	IOLog ("AppleSCCSerial::LookForInternalModem -- Entered\n");
    iter = IORegistryIterator::iterateOver( gIODTPlane );
    assert( iter );
    iter->reset();
	found = false;
    while ((found == false) && (next = iter->getNextObjectRecursive()))
	{
		if (0 == strcmp ("i2c-modem", next->getName()))
		{
			OSData *s2 = OSDynamicCast(OSData, next->getProperty("modem-id"));
			if (s2)
			{
				char *tmpID = (char *) s2->getBytesNoCopy();
				tmpID++;	// need to look at the second byte which contains the modem ID
				if (*tmpID == modemID)
				{
//					IOLog ("AppleSCCSerial::LookForInternalModem -- Found Mini Spring DCP Modem\n");
					found = true;
				}
			}
		}
    }

    iter->release();
//	IOLog ("AppleSCCSerial::LookForInternalModem -- Exited\n");
	return found;
}	// end of AppleSCCSerial::LookForInternalModem


// for DCP -- end
