/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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


#include <TargetConditionals.h>

#include <libkern/OSByteOrder.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>

#if !TARGET_OS_EMBEDDED
	#include <IOKit/pccard/IOPCCard.h>
#endif

#include <IOKit/usb/IOUSBRootHubDevice.h>

#include "AppleUSBEHCI.h"
#include "AppleEHCIedMemoryBlock.h"
#include "AppleEHCItdMemoryBlock.h"
#include "AppleEHCIitdMemoryBlock.h"
#include "AppleEHCIsitdMemoryBlock.h"

#define super IOUSBControllerV3
#define self this

#define NUM_BUFFER_PAGES	9   // 54
#define NUM_TDS			255 // 1500
#define NUM_EDS			256 // 1500
#define NUM_ITDS		192 // 1300

// TDs  per page == 85
// EDs  per page == 128
// ITDs per page == 64


// Convert USBLog to use kprintf debugging
// The switch is in the header file, but the work is done here because the header is included by the companion controllers
#if EHCI_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= EHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

OSDefineMetaClassAndStructors(AppleUSBEHCI, IOUSBControllerV3)

// this is a static variable (system wide global)
AppleEHCIExtraPower		AppleUSBEHCI::_extraPower;						// this is static as currently it is share by all machines

bool 
AppleUSBEHCI::init(OSDictionary * propTable)
{
    if (!super::init(propTable)) 
		return false;
	
    _myBusState = kUSBBusStateReset;
    _hasPCIPwrMgmt = false;
	
    _wdhLock = IOSimpleLockAlloc();
    if (!_wdhLock)
		goto ErrorExit;

	_isochScheduleLock = IOSimpleLockAlloc();
    if (!_isochScheduleLock)
		goto ErrorExit;

    _uimInitialized = false;
    _controllerSpeed = kUSBDeviceSpeedHigh;	// This needs to be set before start.
											// super uses it during start method
    
    // Initialize our consumer and producer counts.  
    //
    _producerCount = 1;
    _consumerCount = 1;
    
    return true;
	
ErrorExit:
		
	if ( _wdhLock )
		IOSimpleLockFree(_wdhLock);
	
	if (_isochScheduleLock)
		IOSimpleLockFree(_isochScheduleLock);
	
	return false;
}



void
AppleUSBEHCI::free()
{
    // Free our locks
    //
    IOSimpleLockFree( _wdhLock );
    IOSimpleLockFree( _isochScheduleLock );

    super::free();
}



void AppleUSBEHCI::showRegisters(UInt32 level, const char *s)
{
    int i, numPorts;

	if (!_controllerAvailable)
		return;
		
    USBLog(level,"AppleUSBEHCI[%p]::showRegisters - called from %s - version: 0x%x", this, s, USBToHostWord(_pEHCICapRegisters->HCIVersion));
#define SHOW_PCI_REGS
#ifdef SHOW_PCI_REGS
    USBLog(level,"PCI: kIOPCIConfigVendorID=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigVendorID));
    USBLog(level,"     kIOPCIConfigRevisionID=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigRevisionID));
    USBLog(level,"     kIOPCIConfigCacheLineSize=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigCacheLineSize));
    USBLog(level,"     kIOPCIConfigBaseAddress0=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigBaseAddress0));
    USBLog(level,"     kIOPCIConfigBaseAddress1=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigBaseAddress1));
    USBLog(level,"     kIOPCIConfigExpansionROMBase=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigExpansionROMBase));
    USBLog(level,"     kIOPCIConfigInterruptLine=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigInterruptLine));
    USBLog(level,"     kIOPCIConfigInterruptLine+4=0x%x", (uint32_t)_device->configRead32(kIOPCIConfigInterruptLine+4));
    USBLog(level,"     kIOPCIConfigCommand=%p", (void*)_device->configRead16(kIOPCIConfigCommand));
    USBLog(level,"     kIOPCIConfigStatus=%p", (void*)_device->configRead16(kIOPCIConfigStatus));
#endif

    USBLog(level,"EHCI: USBCMD:  %p", (void*)USBToHostLong(_pEHCIRegisters->USBCMD));
    USBLog(level,"   USBSTS:  %p", (void*)USBToHostLong(_pEHCIRegisters->USBSTS));
    USBLog(level,"   USBIntr: %p", (void*)USBToHostLong(_pEHCIRegisters->USBIntr));
    USBLog(level,"   FRIndex: %p", (void*)USBToHostLong(_pEHCIRegisters->FRIndex));
    USBLog(level,"   CTRLDSSeg:    %p", (void*)USBToHostLong(_pEHCIRegisters->CTRLDSSegment));
    USBLog(level,"   PerListBase:  %p", (void*)USBToHostLong(_pEHCIRegisters->PeriodicListBase));
    USBLog(level,"   AsyncListAd:  %p", (void*)USBToHostLong(_pEHCIRegisters->AsyncListAddr));
    USBLog(level,"   ConfFlg: %p", (void*)USBToHostLong(_pEHCIRegisters->ConfigFlag));
    numPorts = USBToHostLong(_pEHCICapRegisters->HCSParams) & kEHCINumPortsMask;
    for(i=0;i<numPorts;i++)
    {
        UInt32 x;
        x = USBToHostLong(_pEHCIRegisters->PortSC[i]);
        if(x != 0x1000)
		{
            USBLog(level,"    PortSC[%d]: 0x%x", i+1, (uint32_t)x);
		}
    }
}



bool
AppleUSBEHCI::start( IOService * provider )
{	
    USBLog(7, "AppleUSBEHCI[%p]::start",  this);
	
    if( !super::start(provider))
        return (false);
    
    USBLog(7, "AppleUSBEHCI[%p]::start",  this);
    return true;
}



IOReturn 
AppleUSBEHCI::UIMInitialize(IOService * provider)
{
    UInt32				CapLength, USBCmd, hccparams, ist;
    IOReturn			err = kIOReturnSuccess;
    UInt16				lvalue;
	UInt8				bValue;
    int					i, numPorts;
	bool				gotTimerThreads;
    
    USBLog(7, "AppleUSBEHCI[%p]::UIMInitialize",  this);
	
    _device = OSDynamicCast(IOPCIDevice, provider);
    if(_device == NULL)
        return kIOReturnBadArgument;
	
    do {
		
        if (!(_deviceBase = _device->mapDeviceMemoryWithIndex(0)))
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - unable to get device memory",  this);
            err = kIOReturnNoResources;
            break;
        }
		
        USBLog(3, "AppleUSBEHCI[%p]::UIMInitialize config @ %x (%x)\n", this,
			   (uint32_t)_deviceBase->getVirtualAddress(),
			   (uint32_t)_deviceBase->getPhysicalAddress());
		
        SetVendorInfo();
		
        // Set up a filter interrupt source (this process both primary (thru filter function) and secondary (thru action function)
        // interrupts.
        //
        _filterInterruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
																						  AppleUSBEHCI::InterruptHandler,	
																						  AppleUSBEHCI::PrimaryInterruptFilter,
																						  _device );
		
        if ( !_filterInterruptSource )
        {
            USBError(1,"AppleUSBEHCI[%p]: unable to get filterInterruptEventSource",  this);
            err = kIOReturnNoResources;
            break;
        }
        
        err = _workLoop->addEventSource(_filterInterruptSource);
        if ( err != kIOReturnSuccess )
        {
            USBError(1,"AppleUSBEHCI[%p]: unable to add filter event source: 0x%x",  this, err);
            err = kIOReturnNoResources;
            break;
        }
		
        /*
         * Initialize my data and the hardware
         */
        _errataBits = GetErrataBits(_vendorID, _deviceID, _revisionID);
		setProperty("Errata", _errataBits, 32);
    
        USBLog(7, "AppleUSBEHCI[%p]::UIMInitialize - errata bits=%p",  this,  (void*)_errataBits);
		
        _pEHCICapRegisters = (EHCICapRegistersPtr) _deviceBase->getVirtualAddress();
		
        // enable the card registers
        _device->configWrite16(kIOPCIConfigCommand, kIOPCICommandMemorySpace);
		
        err = AcquireOSOwnership();
        if ( err != kIOReturnSuccess )
        {
            USBError(1,"AppleUSBEHCI[%p]: unable to obtain ownership: 0x%x",  this, err);
            break;
        }
		
		// determine whether this is a 32 bit or 64 bit machine
		hccparams = USBToHostLong(_pEHCICapRegisters->HCCParams);
		if (!(_errataBits & kErrataUse32bitEHCI) && (hccparams & kEHCI64Bit))
			_is64bit = true;
		else
			_is64bit = false;
		
		setProperty("64bit", _is64bit);
		
		ist = (hccparams & kEHCIISTMask) >> kEHCIISTPhase;
		
		if (!ist)
			_istKeepAwayFrames = 1;
		else
			_istKeepAwayFrames = 2;

	   	CapLength  = _pEHCICapRegisters->CapLength;
		_pEHCIRegisters = (EHCIRegistersPtr) ( ((uintptr_t)_pEHCICapRegisters) + CapLength);
				
		// Enable the interrupt delivery.
		_workLoop->enableAllInterrupts();
		
		_rootHubFuncAddress = 1;
		
		// reset the chip and make sure that all is well
		if (_is64bit)
			_pEHCIRegisters->CTRLDSSegment = 0;					// Set upper address bits to 0

		_pEHCIRegisters->USBCMD = 0;  							// this sets r/s to stop
        IOSync();
		
		for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++)
			IOSleep(1);

		if (i >= 100)
		{
            USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - could not get chip to halt within 100 ms",  this);
			err = kIOReturnInternalError;
			break;
		}
		
		_pEHCIRegisters->USBCMD = HostToUSBLong(kEHCICMDHCReset);		// set the reset bit
        IOSync();
		for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBCMD) & kEHCICMDHCReset); i++)
			IOSleep(1);
		if (i >= 100)
		{
            USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - could not get chip to come out of reset within 100 ms",  this);
			err = kIOReturnInternalError;
			break;
		}
		
		_pEHCIRegisters->PeriodicListBase = 0;						// no periodic list as yet
		_pEHCIRegisters->AsyncListAddr = 0;							// no async list as yet
        IOSync();
                        
		USBCmd = USBToHostLong(_pEHCIRegisters->USBCMD);
		// check to make sure we have the correct Frame List Size
		_frameListSize = (USBCmd & kEHCICMDFrameListSizeMask) >> kEHCICMDFrameListSizeOffset;
		if (_frameListSize)
		{
			// at this time we only support a 1K frame list
			USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - bad _frameListSize",  this);
			err = kIOReturnInternalError;
			break;
		}
		_frameListSize = 1024;
		_myBusState = kUSBBusStateRunning;
		USBCmd &= ~kEHCICMDIntThresholdMask;
		USBCmd |= 1 << kEHCICMDIntThresholdOffset;						// Interrupt every micro frame as needed (4745296)
		_pEHCIRegisters->USBCMD = USBToHostLong(USBCmd);
		
		_pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);  		// Route ports to EHCI
		IOSync();
		IOSleep(1);
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			UInt32		newValue = 0, count = 0;
			newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			while ((count++ < 10) && (newValue != kEHCIPortRoutingBit))
			{
				USBError(1, "EHCI driver: UIMInitialize - ConfigFlag bit not sticking. Retrying.");
				_pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);
				IOSync();
				newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			}
		}
		
		_isochBandwidthAvail = 5 *1024;
		_outSlot = kEHCIPeriodicListEntries+1;			// No Isoc transactions currently
		_frameNumber = 0;
		_expansionData->_isochMaxBusStall = 25000;		// we need a requireMaxBusStall of 25 microseconds for EHCI
		
		if ((err = InterruptInitialize()))
			continue;

		CheckSleepCapability();

		numPorts = USBToHostLong(_pEHCICapRegisters->HCSParams) & kEHCINumPortsMask;
		gotTimerThreads = true;
		for (i=0; i < numPorts; i++)
		{
			_rhResumePortTimerThread[i] = thread_call_allocate((thread_call_func_t)RHResumePortTimerEntry, (thread_call_param_t)this);
			if (!_rhResumePortTimerThread[i])
			{
				gotTimerThreads = false;
				break;
			}
		}
		if (!gotTimerThreads)
			continue;
		
		_uimInitialized = true;

		registerService();									// allows the UHCI driver to know we are here
		return kIOReturnSuccess;
		
    } while (false);
	
    USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - Error occurred (0x%x)",  this, err);
    UIMFinalize();

    if (_filterInterruptSource)
		_filterInterruptSource->release();
	
    return(err);
}



IOReturn
AppleUSBEHCI::AcquireOSOwnership(void)
{
    UInt32			hccparams = USBToHostLong(_pEHCICapRegisters->HCCParams & kEHCIEECPMask);
    UInt32			eecp;
    UInt32			data;

	USBLog(2, "Attempting to get EHCI Controller from BIOS");
    eecp = (hccparams & kEHCIEECPMask) >> kEHCIEECPPhase;
	
    // if there is no eecp register, then we can assume we have ownership
    if (eecp < 0x40) 
		return kIOReturnSuccess;

    data = _device->configRead32(eecp);

    if ((data & 0xFF) == kEHCI_USBLEGSUP_ID)
    {
		USBLog(2, "Found USBLEGSUP_ID - value %p - writing OSOwned", (void*)data);
        _device->configWrite32(eecp, data | kEHCI_USBLEGSUP_OSOwned);

        // wait for kEHCI_USBLEGSUP_BIOSOwned bit to clear
        for (int i = 0; i < 25; i++)
        {
            data = _device->configRead32(eecp);
            if ((data & kEHCI_USBLEGSUP_BIOSOwned) == 0)
                break;
            IOSleep(10);
        }
		if ((_device->configRead32(eecp) & kEHCI_USBLEGSUP_BIOSOwned) != 0) 
		{
			USBError(1, "EHCI controller unable to take control from BIOS");
			return kIOReturnNoResources;
		}
		else
		{
			USBLog(2, "acquireOSOwnership done - value %p", (void*)_device->configRead32(eecp));
		}
    }
	else
	{
		// on the NEC controllers, the eecp contains a 0, so we assume we have ownership
		USBLog(2, "EHCI controller has wrong value in EECP register");
	}
	return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::AsyncInitialize (void)
{
    _AsyncHead = NULL;
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::InterruptInitialize (void)
{
    int								i;
    UInt32							termBit, *list; 
    IOUSBControllerListElement		**logical;
    IOPhysicalAddress				physPtr;
	IOReturn						status;
	UInt64							offset = 0;
	IODMACommand::Segment32			segments;
	UInt32							numSegments = 1;
	IODMACommand					*dmaCommand = NULL;
	
    // Set up periodic list
	_periodicListBuffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryUnshared | kIODirectionInOut, kEHCIPeriodicFrameListsize, kEHCIStructureAllocationPhysicalMask);
    if (_periodicListBuffer == NULL) 
	{
		USBError(1, "AppleUSBEHCI[%p]::InterruptInitialize - IOBufferMemoryDescriptor::inTaskWithPhysicalMask failed", this);
        return kIOReturnNoMemory;
    }
    status = _periodicListBuffer->prepare();
	if (status)
	{
		USBError(1, "AppleUSBEHCI[%p]::InterruptInitialize - could not prepare buffer err(%p)", this, (void*)status);
		_periodicListBuffer->release();
		_periodicListBuffer = NULL;
        return status;
	}

	_periodicList = (USBPhysicalAddress32 *)_periodicListBuffer->getBytesNoCopy();
	
	// Use IODMACommand to get the physical address
	dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
	if (!dmaCommand)
	{
		USBError(1, "AppleUSBEHCI[%p]::InterruptInitialize - could not get IODMACommand", this);
		_periodicListBuffer->complete();
		_periodicListBuffer->release();
		_periodicListBuffer = NULL;
        return kIOReturnNoMemory;
	}

	USBLog(6, "AppleUSBEHCI[%p]::InterruptInitialize - got IODMACommand %p", this, dmaCommand);
	status = dmaCommand->setMemoryDescriptor(_periodicListBuffer);
	if (status)
	{
		USBError(1, "AppleUSBEHCI[%p]::InterruptInitialize - setMemoryDescriptor returned err (%p)", this, (void*)status);
		dmaCommand->release();
		_periodicListBuffer->complete();
		_periodicListBuffer->release();
		_periodicListBuffer = NULL;
        return status;
	}
	
	status = dmaCommand->gen32IOVMSegments(&offset, &segments, &numSegments);
	if (status || (numSegments != 1) || (segments.fLength != PAGE_SIZE))
	{
		USBError(1, "AppleUSBEHCI[%p]::InterruptInitialize - could not generate segments err (%p) numSegments (%d) fLength (%d)", this, (void*)status, (int)numSegments, (int)segments.fLength);
		status = status ? status : kIOReturnInternalError;
		dmaCommand->clearMemoryDescriptor();
		dmaCommand->release();
		_periodicListBuffer->complete();
		_periodicListBuffer->release();
		_periodicListBuffer = NULL;
        return status;
	}
	
	physPtr = segments.fIOVMAddr;

	USBLog(7, "AppleUSBEHCI[%p]::InterruptInitialize - frame list pPhysical[%p] frames[%p]", this, (void*)physPtr, _periodicList);

    _physPeriodicListBase = HostToUSBLong(physPtr);
	dmaCommand->clearMemoryDescriptor();
	dmaCommand->release();
	
    _logicalPeriodicList = IONew(IOUSBControllerListElement *, kEHCIPeriodicListEntries);
    if(_logicalPeriodicList == NULL)
    {
		_periodicListBuffer->complete();
		_periodicListBuffer->release();
		_periodicListBuffer = NULL;
		return kIOReturnNoMemory;
    }
	
	
    // Set all the entries in the periodic list to invalid
    termBit = HostToUSBLong(kEHCITermFlag);
    list = (UInt32 *)_periodicList;
    logical = _logicalPeriodicList;
    for(i= 0; i<kEHCIPeriodicListEntries; i++)
    {
        *(list++) = termBit;
        *(logical++) = NULL;
    }
    for (i=0; i<kEHCIMaxPoll; i++)
		_periodicBandwidth[i] = kEHCIMaxPeriodicBandwidth;
    
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::UIMFinalize(void)
{
    int 	i;
    IOReturn	err = kIOReturnSuccess;
	
    if ( _deviceBase )
	{
        USBLog (3, "AppleUSBEHCI[%p]: @ %x (%x)(shutting down HW)", this, (uint32_t)_deviceBase->getVirtualAddress(), (uint32_t)_deviceBase->getPhysicalAddress());
	}
    
#if 0
	// 4930013: JRH - This is a bad thing to do with shared interrupts, and since we turn off interrupts at the source
	// it should be redundant. Let's stop doing it..
	// Disable the interrupt delivery
    //
    if ( _workLoop )
        _workLoop->disableAllInterrupts();
#endif
	
    // Wait for the interrupts to propagate
    //
    IOSleep(2);
	
    // Clean up our power down notifier.  That will release it.
    //
    if ( _powerDownNotifier ) {
        _powerDownNotifier->remove();
        _powerDownNotifier = NULL;
    }
    
    USBLog(1, "AppleUSBEHCI[%p]::UIMFinalize isInactive(%x) _pEHCIRegisters(%p) _device(%p)",  this, isInactive(), _pEHCIRegisters, _device);
	
    // If we are NOT being terminated, then talk to the EHCI controller and
    // set up all the registers to be off
    //
    if ( !isInactive() && _pEHCIRegisters && _device )
    {
        // Disable All EHCI Interrupts
        //
        _pEHCIRegisters->USBIntr = 0x0; 
        IOSync();
		
        // reset the chip and make sure that all is well
        if (_is64bit)
	        _pEHCIRegisters->CTRLDSSegment = 0;             // Set upper address bits to 0
        _pEHCIRegisters->USBCMD = 0;  						// this sets r/s to stop
        IOSync();
        
        for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++)
            IOSleep(1);
        if (i >= 100)
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMFinalize - could not get chip to halt within 100 ms",  this);
            err = kIOReturnInternalError;
            goto ErrorExit;
        }
		_myBusState = kUSBBusStateReset;

        _pEHCIRegisters->USBCMD = HostToUSBLong(kEHCICMDHCReset);		// set the reset bit
        IOSync();
        for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBCMD) & kEHCICMDHCReset); i++)
            IOSleep(1);
        
        if (i >= 100)
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMFinalize - could not get chip to come out of reset within 100 ms",  this);
            err = kIOReturnInternalError;
            goto ErrorExit;
        }
		
        // Route ports back to companion controller
        _pEHCIRegisters->ConfigFlag = 0;
        IOSync();
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			UInt32		newValue = 0, count = 0;
			newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			while ((count++ < 10) && (newValue != 0))
			{
				USBError(1, "EHCI driver: UIMFinalize - ConfigFlag bit not sticking. Retrying.");
				_pEHCIRegisters->ConfigFlag = 0;
				IOSync();
				newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			}
		}

        // disable bus master
        _device->configWrite16(kIOPCIConfigCommand, kIOPCICommandMemorySpace);
		
        _pEHCIRegisters->PeriodicListBase = 0;		// no periodic list as yet
		_pEHCIRegisters->AsyncListAddr = 0;			// no async list as yet
		IOSync();
			
    }
	
    // Free the memory allocated in the InterruptInitialize()
    //
    if ( _periodicListBuffer )
	{
		_periodicListBuffer->complete();
		_periodicListBuffer->release();
		_periodicListBuffer = NULL;
	}
	
    if ( _logicalPeriodicList )
	{
        IODelete( _logicalPeriodicList, IOUSBControllerListElement *, kEHCIPeriodicListEntries );
		_logicalPeriodicList = NULL;
	}
	
    // Need to Free any Isoch Endpoints
    //
	
    // Remove the interruptEventSource we created
    //
    if ( _filterInterruptSource && _workLoop )
    {
        _workLoop->removeEventSource(_filterInterruptSource);
        _filterInterruptSource->release();
        _filterInterruptSource = NULL;
    }
	
	
ErrorExit:
		
		_uimInitialized = false;
    return err;
}



UInt32 
AppleUSBEHCI::GetBandwidthAvailable()
{
    USBLog(7, "AppleUSBEHCI[%p]::GetBandwidthAvailable -- returning %d",  this, (int)_isochBandwidthAvail);
    return _isochBandwidthAvail;
}



UInt32 
AppleUSBEHCI::GetFrameNumber32()
{
    UInt32	temp1, temp2;
    register 	UInt32	frindex;
    
	// If the controller is halted, then we should just bail out
	if ( USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit)
	{
		USBLog(1, "AppleUSBEHCI[%p]::GetFrameNumber32 called but controller is halted",  this);
		return 0;
	}
	
    // First, get a snapshot but make sure that we haven't wrapped around and not processed the new value.  Note that the
    // rollover bit is processed at primary interrupt time, so we don't need to take that into account in this calculation.
    //
    do
    {
		temp1 = (UInt32)_frameNumber;
		frindex = HostToUSBLong(_pEHCIRegisters->FRIndex);
        IOSync();
		temp2 = (UInt32)_frameNumber;
    } while ((temp1 != temp2) || (frindex ==  0) );
	
    // Shift out the microframes
    //
    frindex = frindex >> 3;
    
    USBLog(7, "AppleUSBEHCI[%p]::GetFrameNumber32 -- returning %d (%p)",  this, (int)(temp1+frindex), (void*)(temp1+frindex));
    
    return (UInt32)(temp1 + frindex);
}


UInt64 
AppleUSBEHCI::GetFrameNumber()
{
    UInt64	temp1, temp2;
    register 	UInt32	frindex;
    
	// If the controller is halted, then we should just bail out
	if ( USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit)
	{
		USBLog(1, "AppleUSBEHCI[%p]::GetFrameNumber called but controller is halted",  this);
		return 0;
	}
	
    // First, get a snapshot but make sure that we haven't wrapped around and not processed the new value.  Note that the
    // rollover bit is processed at primary interrupt time, so we don't need to take that into account in this calculation.
    //
    do
    {
		temp1 = _frameNumber;
		frindex = HostToUSBLong(_pEHCIRegisters->FRIndex);
        // IOSync();
		temp2 = _frameNumber;
    } while ( (temp1 != temp2) || (frindex == 0) );
	
    // Shift out the microframes
    //
    frindex = frindex >> 3;		
    
    // USBLog(7, "AppleUSBEHCI[%p]::GetFrameNumber -- returning %Ld (0x%Lx)",  this, temp1+frindex, temp1+frindex);
	
    return (temp1 + frindex);
}



UInt64 
AppleUSBEHCI::GetMicroFrameNumber()
{
    UInt64	temp1, temp2;
    register 	UInt32	frindex;
    
    // First, get a snapshot but make sure that we haven't wrapped around and not processed the new value.  Note that the
    // rollover bit is processed at primary interrupt time, so we don't need to take that into account in this calculation.
    //
    do
    {
		temp1 = _frameNumber;
		frindex = HostToUSBLong(_pEHCIRegisters->FRIndex);
        IOSync();
		temp2 = _frameNumber;
    } while ( (temp1 != temp2) || (frindex == 0) );
	
    // since this frindex has the lower 3 bits of the microframe, we need to adjust the upper 61 bits
    // by shifting them up
    temp1 = temp1 << 3;
    
    USBLog(7, "AppleUSBEHCI[%p]::GetMicroFrameNumber -- returning %Ld (0x%Lx)",  this, temp1+frindex, temp1+frindex);
	
    return (temp1 + frindex);
}



void 
AppleUSBEHCI::SetVendorInfo(void)
{
    OSData		*vendProp, *deviceProp, *revisionProp;
	
    // get this chips vendID, deviceID, revisionID
    vendProp     = OSDynamicCast(OSData, _device->getProperty( "vendor-id" ));
    if (vendProp)
        _vendorID = *((UInt32 *) vendProp->getBytesNoCopy());
    deviceProp   = OSDynamicCast(OSData, _device->getProperty( "device-id" ));
    if (deviceProp)
        _deviceID = *((UInt32 *) deviceProp->getBytesNoCopy());
    revisionProp = OSDynamicCast(OSData, _device->getProperty( "revision-id" ));
    if (revisionProp)
        _revisionID = *((UInt32 *) revisionProp->getBytesNoCopy());
}



AppleEHCIQueueHead * 
AppleUSBEHCI::AllocateQH(void)
{
    AppleEHCIQueueHead *freeQH;
	
    // Pop a ED off the freeQH list
    // If freeQH == NULL return Error
    freeQH = _pFreeQH;
	
    if (freeQH == NULL)
    {
		// i need to allocate another page of EDs
		AppleEHCIedMemoryBlock 		*memBlock;
		UInt32				numEDs, i;
		
		memBlock = AppleEHCIedMemoryBlock::NewMemoryBlock();
		if (!memBlock)
		{
			USBLog(1, "AppleUSBEHCI[%p]::AllocateED - unable to allocate a new memory block!",  this);
			return NULL;
		}
		// link it in to my list of ED memory blocks
		memBlock->SetNextBlock(_edMBHead);
		_edMBHead = memBlock;
		numEDs = memBlock->NumEDs();
		_pLastFreeQH = AppleEHCIQueueHead::WithSharedMemory(memBlock->GetLogicalPtr(0), memBlock->GetPhysicalPtr(0));
		_pFreeQH = _pLastFreeQH;
		for (i=1; i < numEDs; i++)
		{
			freeQH = AppleEHCIQueueHead::WithSharedMemory(memBlock->GetLogicalPtr(i), memBlock->GetPhysicalPtr(i));
			if (!freeQH)
			{
				USBLog(1, "AppleUSBEHCI[%p]::AllocateED - hmm. ran out of EDs in a memory block",  this);
				freeQH = _pFreeQH;
				break;
			}
			freeQH->_logicalNext = _pFreeQH;
			_pFreeQH = freeQH;
			// in a normal loop termination, freeQH and _pFreeQH are the same, just like when we don't use this code
		}
    }
    if (freeQH)
    {
		_pFreeQH = OSDynamicCast(AppleEHCIQueueHead, freeQH->_logicalNext);
		// if we use the last one, then we need to zero out the end pointer as well
		if (!_pFreeQH)
			_pLastFreeQH = NULL;
		freeQH->_logicalNext = NULL;
    }
    return freeQH;
}



IOReturn 
AppleUSBEHCI::DeallocateTD (EHCIGeneralTransferDescriptorPtr pTD)
{
    UInt32		physical;
	
    //zero out all unnecessary fields
    physical = pTD->pPhysical;
    // bzero(pTD, sizeof(*pTD));
    pTD->pLogicalNext = NULL;
    pTD->pPhysical = physical;
	
    if (_pLastFreeTD)
    {
        _pLastFreeTD->pLogicalNext = pTD;
        _pLastFreeTD = pTD;
    } 
    else 
    {
        // list is currently empty
        _pLastFreeTD = pTD;
        _pFreeTD = pTD;
    }
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::DeallocateED (AppleEHCIQueueHead *pED)
{
    USBLog(7, "AppleUSBEHCI[%p]::DeallocateED - deallocating %p",  this, pED);
    pED->_logicalNext = NULL;
	
    if (_pLastFreeQH)
    {
        _pLastFreeQH->_logicalNext = pED;
        _pLastFreeQH = pED;
    } 
    else 
    {
        // list is currently empty
        _pLastFreeQH = pED;
        _pFreeQH = pED;
    }
    return (kIOReturnSuccess);
}



EHCIGeneralTransferDescriptorPtr 
AppleUSBEHCI::AllocateTD(void)
{
    EHCIGeneralTransferDescriptorPtr freeTD;
	
    // Pop a ED off the FreeED list
    // If FreeED == NULL return Error
    freeTD = _pFreeTD;
	
    if (freeTD == NULL)
    {
		// i need to allocate another page of EDs
		AppleEHCItdMemoryBlock 		*memBlock;
		UInt32				numTDs, i;
		
		memBlock = AppleEHCItdMemoryBlock::NewMemoryBlock();
		if (!memBlock)
		{
			USBError(1, "AppleUSBEHCI[%p]::AllocateTD - unable to allocate a new memory block!",  this);
			return NULL;
		}
		// link it in to my list of ED memory blocks
		memBlock->SetNextBlock(_tdMBHead);
		_tdMBHead = memBlock;
		numTDs = memBlock->NumTDs();
		_pLastFreeTD = memBlock->GetTD(0);				// the first will be last..
		for (i=0; i < numTDs; i++)
		{
			freeTD = memBlock->GetTD(i);
			if (!freeTD)
			{
				USBError(1, "AppleUSBEHCI[%p]::AllocateTD - hmm. ran out of TDs in a memory block",  this);
				freeTD = _pFreeTD;
				break;
			}
			freeTD->pLogicalNext = _pFreeTD;
			_pFreeTD = freeTD;
			// in a normal loop termination, freeED and _pFreeED are the same, just like when we don't use this code
		}
    }
    if (freeTD)
    {
		_pFreeTD = freeTD->pLogicalNext;
		// if we use the last one, then we need to zero out the end pointer as well
		if (!_pFreeTD)
			_pLastFreeTD = NULL;
		freeTD->pLogicalNext = NULL;
		freeTD->lastFrame = 0;
		freeTD->lastRemaining = 0;
		freeTD->command = NULL;
		freeTD->callbackOnTD = false;
		freeTD->multiXferTransaction = false;
		freeTD->finalXferInTransaction = false;
    }
    return freeTD;
}



AppleEHCIIsochTransferDescriptor * 
AppleUSBEHCI::AllocateITD(void)
{
    AppleEHCIIsochTransferDescriptor *freeITD;
	
    // Pop a ED off the FreeED list
    // If FreeED == NULL return Error
    freeITD = _pFreeITD;
	
    if (freeITD == NULL)
    {
		// i need to allocate another page of EDs
		AppleEHCIitdMemoryBlock 	*memBlock;
		UInt32				numTDs, i;
		
		memBlock = AppleEHCIitdMemoryBlock::NewMemoryBlock();
		if (!memBlock)
		{
			USBError(1, "AppleUSBEHCI[%p]::AllocateITD - unable to allocate a new memory block!",  this);
			return NULL;
		}
		// link it in to my list of ED memory blocks
		memBlock->SetNextBlock(_itdMBHead);
		_itdMBHead = memBlock;
		numTDs = memBlock->NumTDs();
		_pLastFreeITD = AppleEHCIIsochTransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(0), memBlock->GetPhysicalPtr(0));
		_pFreeITD = _pLastFreeITD;
		for (i=1; i < numTDs; i++)
		{
			freeITD = AppleEHCIIsochTransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(i), memBlock->GetPhysicalPtr(i));
			if (!freeITD)
			{
				USBError(1, "AppleUSBEHCI[%p]::AllocateTD - hmm. ran out of TDs in a memory block",  this);
				freeITD = _pFreeITD;
				break;
			}
			freeITD->_logicalNext = _pFreeITD;
			_pFreeITD = freeITD;
			// in a normal loop termination, freeED and _pFreeED are the same, just like when we don't use this code
		}
    }
    if (freeITD)
    {
		_pFreeITD = OSDynamicCast(AppleEHCIIsochTransferDescriptor, freeITD->_logicalNext);
		freeITD->_logicalNext = NULL;
        if (!_pFreeITD)
            _pLastFreeITD = NULL;
    }
	
	// initialize the page pointers to zero length
	//
    bzero(&freeITD->GetSharedLogical()->Transaction0, sizeof(EHCIIsochTransferDescriptorShared)-sizeof(USBPhysicalAddress32) );

    USBLog(7, "AppleUSBEHCI[%p]::AllocateITD - returning %p",  this, freeITD);
    return freeITD;
}



IOReturn 
AppleUSBEHCI::DeallocateITD (AppleEHCIIsochTransferDescriptor *pTD)
{
    USBLog(7, "AppleUSBEHCI[%p]::DeallocateITD - deallocating %p",  this, pTD);
    pTD->_logicalNext = NULL;
	
    if (_pLastFreeITD)
    {
        _pLastFreeITD->_logicalNext = pTD;
        _pLastFreeITD = pTD;
    } 
    else 
    {
        // list is currently empty
        _pLastFreeITD = pTD;
        _pFreeITD = pTD;
    }
    return kIOReturnSuccess;
}


AppleEHCISplitIsochTransferDescriptor * 
AppleUSBEHCI::AllocateSITD(void)
{
    AppleEHCISplitIsochTransferDescriptor *freeSITD;
	
    // Pop a ED off the FreeED list
    // If FreeED == NULL return Error
    freeSITD = _pFreeSITD;
	
    if (freeSITD == NULL)
    {
		// i need to allocate another page of EDs
		AppleEHCIsitdMemoryBlock 	*memBlock;
		UInt32				numTDs, i;
		
		memBlock = AppleEHCIsitdMemoryBlock::NewMemoryBlock();
		if (!memBlock)
		{
			USBError(1, "AppleUSBEHCI[%p]::AllocateTD - unable to allocate a new memory block!",  this);
			return NULL;
		}
		// link it in to my list of ED memory blocks
		memBlock->SetNextBlock(_sitdMBHead);
		_sitdMBHead = memBlock;
		numTDs = memBlock->NumTDs();
		USBLog(3, "AppleUSBEHCI[%p]::AllocateSITD - got new memory block (%p) with %d SITDs in it",  this, memBlock, (int)numTDs);
		_pLastFreeSITD = AppleEHCISplitIsochTransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(0), memBlock->GetPhysicalPtr(0));
		_pFreeSITD = _pLastFreeSITD;
		for (i=1; i < numTDs; i++)
		{
			freeSITD = AppleEHCISplitIsochTransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(i), memBlock->GetPhysicalPtr(i));
			if (!freeSITD)
			{
				USBError(1, "AppleUSBEHCI[%p]::AllocateTD - hmm. ran out of TDs in a memory block",  this);
				freeSITD = _pFreeSITD;
				break;
			}
			freeSITD->_logicalNext = _pFreeSITD;
			_pFreeSITD = freeSITD;
			// in a normal loop termination, freeED and _pFreeED are the same, just like when we don't use this code
		}
    }
    if (freeSITD)
    {
		_pFreeSITD = OSDynamicCast(AppleEHCISplitIsochTransferDescriptor, freeSITD->_logicalNext);
		if (!_pFreeSITD)
			_pLastFreeSITD = NULL;
		freeSITD->_logicalNext = NULL;
		freeSITD->_isDummySITD = false;
    }
    USBLog(7, "AppleUSBEHCI[%p]::AllocateSITD - returning %p",  this, freeSITD);
    return freeSITD;
}


IOReturn 
AppleUSBEHCI::DeallocateSITD (AppleEHCISplitIsochTransferDescriptor *pTD)
{
    USBLog(7, "AppleUSBEHCI[%p]::DeallocateSITD - deallocating %p",  this, pTD);
    pTD->_logicalNext = NULL;
	
    if (_pLastFreeSITD)
    {
        _pLastFreeSITD->_logicalNext = pTD;
        _pLastFreeSITD = pTD;
    } 
    else 
    {
        // list is currently empty
        _pLastFreeSITD = pTD;
        _pFreeSITD = pTD;
    }
    return kIOReturnSuccess;
}



/*
 * got an error on a TD with no completion routine.
 * Search for a later TD on the same end point which does have one,
 * so we can tell upper layes of the error.
 */
void 
AppleUSBEHCI::doCallback(EHCIGeneralTransferDescriptorPtr nextTD,
						 UInt32			    transferStatus,
						 UInt32			    bufferSizeRemaining)
{
    USBError(1, "AppleUSBEHCI[%p]::doCallback unimemented *************",  this);
}



IOReturn 
AppleUSBEHCI::EnableAsyncSchedule(bool waitForON)
{
    int			i;
    IOReturn	stat = kIOReturnSuccess;
	
	if (!_pEHCIRegisters->AsyncListAddr)
	{
		USBLog(1, "AppleUSBEHCI[%p]::EnableAsyncSchedule.. AsyncListAddr is NULL!! We shouldn't be doing this", this);
	}
    if (!(_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDAsyncEnable)))
    {
		USBLog(7, "AppleUSBEHCI[%p]::EnableAsyncSchedule: enabling schedule",  this);
		// 4627732 - we used to wait for the bits to get synched up, but there doesn't seem to be
		// a compelling reason to do so according to the EHCI spec, so we no longer do that
		if (_errataBits & kErrataAgereEHCIAsyncSched)
		{
			_pEHCIRegisters->USBCMD &= HostToUSBLong(~kEHCICMDRunStop);
			while (!(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit))
				;
			_pEHCIRegisters->USBCMD |= HostToUSBLong(kEHCICMDAsyncEnable | kEHCICMDRunStop);
		}
		else
		{
			_pEHCIRegisters->USBCMD |= HostToUSBLong(kEHCICMDAsyncEnable);
		}
		
		if (waitForON)			// 4627732 - only wait if we explicitly say to
		{
			for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSAsyncScheduleStatus); i++)
				IOSleep(1);
			if (i)
			{
				if (i >= 100)
				{
					USBError(1, "AppleUSBEHCI[%p]::EnableAsyncSchedule: ERROR: USBCMD and USBSTS won't synchronize ON - cannot enable USB list processing",  this);
					stat = kIOReturnInternalError;
				}
				else
				{
					USBLog(7, "AppleUSBEHCI[%p]::EnableAsyncSchedule: had to wait %d ms for CMD and STS to synch up ON",  this, i);
				}
			}
		}
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::EnableAsyncSchedule: schedule was already enabled",  this);
    }
    
    if (stat)
    {
		USBLog(1, "AppleUSBEHCI[%p]::EnableAsyncSchedule: returning status %x",  this, stat);
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::EnableAsyncSchedule: schedule enabled cleanly",  this);
    }
    
    return stat;
}



IOReturn 
AppleUSBEHCI::DisableAsyncSchedule(bool waitForOFF)
{
    int		i;
    IOReturn	stat = kIOReturnSuccess;
	
    if (_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDAsyncEnable))
    {
		USBLog(7, "AppleUSBEHCI[%p]::DisableAsyncSchedule: disabling schedule",  this);
		// 4627732 - we used to wait for the bits to get synched up, but there doesn't seem to be
		// a compelling reason to do so according to the EHCI spec, so we no longer do that
		_pEHCIRegisters->USBCMD &= HostToUSBLong(~kEHCICMDAsyncEnable);
			
		if (waitForOFF)					// 4627732 - only wait if we explicitly say to
		{
			for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSAsyncScheduleStatus); i++)
				IOSleep(1);
			if (i)
			{
				if (i >= 100)
				{
					USBLog(1, "AppleUSBEHCI[%p]::DisableAsyncSchedule: ERROR: USBCMD and USBSTS won't synchronize OFF",  this);
					stat = kIOReturnInternalError;
				}
				else
				{
					USBLog(7, "AppleUSBEHCI[%p]::DisableAsyncSchedule: had to wait %d ms for CMD and STS to synch up OFF",  this, i);
				}
			}
		}
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::DisableAsyncSchedule: schedule was already disabled",  this);
    }
    
    if (stat)
    {
		USBLog(1, "AppleUSBEHCI[%p]::DisableAsyncSchedule: returning status %x",  this, stat);
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::DisableAsyncSchedule: schedule disabled cleanly",  this);
    }
    
    return stat;
}



IOReturn 
AppleUSBEHCI::EnablePeriodicSchedule(bool waitForON)
{
    int			i;
    IOReturn	stat = kIOReturnSuccess;
	
	if (_inAbortIsochEP)
	{
		USBLog(2, "EnablePeriodicSchedule - not doing in AbortIsochEP");
		return stat;
	}
    if (!(_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDPeriodicEnable)))
    {
		USBLog(7, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: enabling schedule",  this);
		// 4627732 - we used to wait for the bits to get synched up, but there doesn't seem to be
		// a compelling reason to do so according to the EHCI spec, so we no longer do that
		
		_pEHCIRegisters->USBCMD |= HostToUSBLong(kEHCICMDPeriodicEnable);
			
		if (waitForON)					// 4627732 - only wait if we explicitly say to
		{
			for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSPeriodicScheduleStatus); i++)
				IOSleep(1);
			if (i)
			{
				if (i >= 100)
				{
					USBLog(1, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: ERROR: USBCMD and USBSTS won't synchronize ON",  this);
					stat = kIOReturnInternalError;
				}
				else
				{
					USBLog(7, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: had to wait %d ms for CMD and STS to synch up ON",  this, i);
				}
			}
		}
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: schedule was already enabled",  this);
    }
    
    if (stat)
    {
		USBLog(1, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: returning status %x",  this, stat);
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: schedule enabled cleanly",  this);
    }
    
    return stat;
}



IOReturn 
AppleUSBEHCI::DisablePeriodicSchedule(bool waitForOFF)
{
    int		i;
    IOReturn	stat = kIOReturnSuccess;
	
    if (_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDPeriodicEnable))
    {
		USBLog(7, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: disabling schedule",  this);
		// 4627732 - we used to wait for the bits to get synched up, but there doesn't seem to be
		// a compelling reason to do so according to the EHCI spec, so we no longer do that
		_pEHCIRegisters->USBCMD &= HostToUSBLong(~kEHCICMDPeriodicEnable);
			
		if (waitForOFF)					// 4627732 - only wait if we explicitly say to
		{
			for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSPeriodicScheduleStatus); i++)
				IOSleep(1);

			if (i)
			{
				if (i >= 100)
				{
					USBLog(1, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: ERROR: USBCMD and USBSTS won't synchronize OFF",  this);
					stat = kIOReturnInternalError;
				}
				else
				{
					USBLog(7, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: had to wait %d ms for CMD and STS to synch up OFF",  this, i);
				}
			}
		}
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: schedule was already disabled",  this);
    }
    
    if (stat)
    {
		USBLog(1, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: returning status %x",  this, stat);
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: schedule disabled cleanly",  this);
    }
    
    return stat;
}



IOReturn
AppleUSBEHCI::message( UInt32 type, IOService * provider,  void * argument )
{
	IOService *					nub = NULL;
	const IORegistryPlane *		usbPlane = NULL;
	IOUSBRootHubDevice *		parentHub = NULL;
	
    USBLog(6, "AppleUSBEHCI[%p]::message type: %p, isInactive = %d",  this, (void*)type, isInactive());

	switch (type)
	{
		case kIOUSBMessageExpressCardCantWake:
			nub = (IOService*)argument;
			usbPlane = getPlane(kIOUSBPlane);
			parentHub = OSDynamicCast(IOUSBRootHubDevice, nub->getParentEntry(usbPlane));

			nub->retain();
			USBLog(1, "AppleUSBEHCI[%p]::message - got kIOUSBMessageExpressCardCantWake from driver %s[%p] argument is %s[%p]", this, provider->getName(), provider, nub->getName(), nub);
			if (parentHub == _rootHubDevice)
			{
				USBLog(1, "AppleUSBEHCI[%p]::message - device is attached to my root hub!!", this);
				_badExpressCardAttached = true;
			}
			nub->release();
			return kIOReturnSuccess;  // this message was handled
			break;
			
#if !TARGET_OS_EMBEDDED
		case kIOPCCardCSEventMessage:
			cs_event_t	pccardevent;
			pccardevent = (uintptr_t) argument;
			
			if ( pccardevent == CS_EVENT_CARD_REMOVAL )
			{
				// Should return all transactions in any endpoints
				//
				USBLog(5,"AppleUSBEHCI[%p]: Received kIOPCCardCSEventMessage Need to return all transactions",this);
				_pcCardEjected = true;
			}
			// let the super-class run as well...
			break;
#endif
			
	}
	

    // Let our superclass decide handle this method
    // messages
    //
    return super::message( type, provider, argument );
	
}


UInt32
AppleUSBEHCI::AllocateExtraRootHubPortPower(UInt32 extraPowerRequested)
{
	if (_extraPower.version != kAppleEHCIExtraPowerVersion)
	{
		USBLog(2, "AppleUSBEHCI[%p]::AllocateExtraRootHubPortPower - extra power not available on this controller", this);
		return 0;
	}
	
	// 0 - that seems silly
	if (!extraPowerRequested)
	{
		USBLog(7, "AppleUSBEHCI[%p]::AllocateExtraRootHubPortPower(kIOUSBMessageRequestExtraPower) - no extra power requested - fine with me", this);
		return 0;
	}
	
	// requesting new power
	
	// check to make sure there is enough aggregate
	if ((UInt32)extraPowerRequested > _extraPower.aggregate)
	{
		USBLog(2, "AppleUSBEHCI[%p]::AllocateExtraRootHubPortPower - no extra power available", this);
		return 0;
	}
	
	// now check to make sure there is enough on each port
	if ((UInt32)extraPowerRequested > _extraPower.perPort)
	{
		USBLog(2, "AppleUSBEHCI[%p]::AllocateExtraRootHubPortPower - no per port power available", this);
		return 0;
	}
	
	_extraPower.aggregate = _extraPower.aggregate - extraPowerRequested;
	USBLog(2, "AppleUSBEHCI[%p]::AllocateExtraRootHubPortPower - requested(%d) - aggregate now at (%d)", this, (int)extraPowerRequested, (int)_extraPower.aggregate);
	return extraPowerRequested;
}



void
AppleUSBEHCI::ReturnExtraRootHubPortPower(UInt32 extraPowerReturned)
{
	// someone is giving the extra power back
	_extraPower.aggregate = _extraPower.aggregate + extraPowerReturned;
	USBLog(2, "AppleUSBEHCI[%p]::ReturnExtraRootHubPortPower - returned(%d) - aggregate now at (%d)", this, (int)extraPowerReturned, (int)_extraPower.aggregate);
	return;
}



IOUSBControllerIsochEndpoint*			
AppleUSBEHCI::AllocateIsochEP()
{
	AppleEHCIIsochEndpoint		*pEP;
	
	pEP = new AppleEHCIIsochEndpoint;
	if (pEP)
	{
		if (!pEP->init())
		{
			pEP->release();
			pEP = NULL;
		}
	}
	return pEP;
}



void
AppleUSBEHCI::ReturnIsochDoneQueue(IOUSBControllerIsochEndpoint* isochEP)
{
	super::ReturnIsochDoneQueue(isochEP);
}



IODMACommand*
AppleUSBEHCI::GetNewDMACommand()
{
	USBLog(7, "AppleUSBEHCI[%p]::GetNewDMACommand - creating %d bit IODMACommand", this, _is64bit ? 64 : 32);
	return IODMACommand::withSpecification(kIODMACommandOutputHost64, _is64bit ? 64 : 32, 0);
}



IOReturn
AppleUSBEHCI::GetLowLatencyOptionsAndPhysicalMask(IOOptionBits *pOptionBits, mach_vm_address_t *pPhysicalMask)
{
	super::GetLowLatencyOptionsAndPhysicalMask(pOptionBits, pPhysicalMask);				// get the defaults
	if (_is64bit)
		*pPhysicalMask = 0xFFFFFFFFFFFFF000ULL;											// can allow upper pages if the controller supports 64 bit
	return kIOReturnSuccess;
}



