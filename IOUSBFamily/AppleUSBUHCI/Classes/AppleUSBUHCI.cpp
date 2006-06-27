/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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



#include <libkern/OSByteOrder.h>

extern "C" {
#include <kern/clock.h>
}


#include <IOKit/IOMessage.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pccard/IOPCCard.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>

#include <IOKit/IOBufferMemoryDescriptor.h>

#include <libkern/OSAtomic.h>

#include "AppleUSBUHCI.h"
#include "AppleUHCItdMemoryBlock.h"
#include "AppleUHCIqhMemoryBlock.h"


#define super IOUSBControllerV2


/*
 * TODO:
 *  - generalize root hub code to use root hub endpoint, rather than having
 *    separate calls and checks in each function (e.g. RHAbort...)
 *  - support for synthetic suspend change status bit in roo thub
 */

OSDefineMetaClassAndStructors(AppleUSBUHCI, IOUSBControllerV2)
OSDefineMetaClassAndStructors(UHCIMemoryBuffer, IOBufferMemoryDescriptor)


UHCIMemoryBuffer *
UHCIMemoryBuffer::newBuffer(bool dmaable)
{
    UHCIMemoryBuffer *bp;
    
    bp = new UHCIMemoryBuffer;
    if (bp == NULL) 
	{
        return NULL;
    }
    
    if (!bp->initWithOptions(kIOMemoryUnshared | kIODirectionInOut, PAGE_SIZE, PAGE_SIZE))
	{
        bp->release();
        return NULL;
    }
    if (dmaable) 
	{
        bp->prepare();
    }
    
    return bp;
}



void
UHCIMemoryBuffer::free()
{
    complete();
    IOBufferMemoryDescriptor::free();
}



// ========================================================================
#pragma mark Initialization
// ========================================================================

bool 
AppleUSBUHCI::init(OSDictionary * propTable)
{
    if (!super::init(propTable))  
		return false;
	
    //USBLog(3, "Debug level before: %d", (int)KernelDebugGetLevel());
    //KernelDebugSetLevel(5);
    //USBLog(3, "AppleUSBUHCI[%p]::init", this);
	
    _uhciBusState = kUHCIBusStateOff;
    _uhciAvailable = true;
    USBLog(7, "AppleUSBUHCI::init: %s", _deviceName);
    
    _intLock = IOLockAlloc();
    if (!_intLock)
        return(false);
	
    _wdhLock = IOSimpleLockAlloc();
    if (!_wdhLock)
        return(false);
	
	_isochScheduleLock = IOSimpleLockAlloc();
    if (!_isochScheduleLock)
        return(false);
	
    _uimInitialized = false;
	
    // Initialize our consumer and producer counts.  
    //
    _producerCount = 1;
    _consumerCount = 1;
    
    return true;
}



bool
AppleUSBUHCI::start( IOService * provider )
{
	OSIterator				*siblings = NULL;
    mach_timespec_t			t;
    OSDictionary			*matching;
    IOService				*service;
    IORegistryEntry			*entry;
    bool					ehciPresent = false;
    
    // Check my provide (_device) parent (a PCI bridge) children (sibling PCI functions)
    // to see if any of them is an EHCI controller - if so, wait for it..
    
	if (provider)
		siblings = provider->getParentEntry(gIOServicePlane)->getChildIterator(gIOServicePlane);
	
	if( siblings ) 
	{
		while( (entry = OSDynamicCast(IORegistryEntry, siblings->getNextObject())))
		{
			UInt32			classCode;
			OSData			*obj = OSDynamicCast(OSData, entry->getProperty("class-code"));
			if (obj) 
			{
				classCode = *(UInt32 *)obj->getBytesNoCopy();
				if (classCode == 0x0c0320) 
				{
					ehciPresent = true;
					break;
				}
			}
		}
		siblings->release();
	}
	
    if (ehciPresent) 
	{
        t.tv_sec = 5;
        t.tv_nsec = 0;
        USBLog(7, "AppleUSBUHCI[%p]::start waiting for EHCI", this);
		setProperty("Companion", "yes");
        service = waitForService( serviceMatching("AppleUSBEHCI"), &t );
        USBLog(7, "AppleUSBUHCI[%p]::start got EHCI service %p", this, service);
    }
    
	// Set a property indicating that we need contiguous memory for isoch transfers
	//
	setProperty(kUSBControllerNeedsContiguousMemoryForIsoch, kOSBooleanTrue);
	
    USBLog(7, "AppleUSBUHCI[%p]::start", this);
    if (!super::start(provider)) 
	{
        return false;
    }
    
    initForPM(_device);

    return true;
}



void
AppleUSBUHCI::stop( IOService * provider )
{
    USBLog(3, "AppleUSBUHCI[%p]::stop", this);
    super::stop(provider);
}



bool
AppleUSBUHCI::finalize(IOOptionBits options)
{
    USBLog(3, "AppleUSBUHCI[%p]::finalize", this);
    return super::finalize(options);
}




void
AppleUSBUHCI::EnableUSBInterrupt(bool enableInterrupt)
{
    UInt16		value;
	
    USBLog(7, "AppleUSBUHCI[%p]::EnableUSBInterrupt(%s) - Legacy register[%p]", this, enableInterrupt ? "true" : "false", (void*)_device->configRead16(kUHCI_PCI_LEGKEY));
    
	// The master interrupt for the UHCI controller is actually in the Legacy Support register (section 5.2.1)
    if (enableInterrupt) 
	{
        value = kUHCI_LEGKEY_INTR_ENABLE;
    } else 
	{
        value = 0;
    }
    _device->configWrite16(kUHCI_PCI_LEGKEY, value);
}



IOReturn
AppleUSBUHCI::HardwareInit(void)
{
    IOReturn									status;
    UInt32 *									frames;
    IOPhysicalAddress							pPhysical;
    int											i, j, frame_period;
    AppleUHCITransferDescriptor					*pTD;
    AppleUHCIQueueHead							*lastQH, *bulkQH, *fsQH, *lsQH, *pQH;
    UHCIMemoryBuffer							*bp;
    
    ioWrite16(kUHCI_INTR, 0);					// Disable interrupts
    
    GlobalReset();
    status = Reset();
    if (status != kIOReturnSuccess) 
	{
        return status;
    }
    
    // Set up frame array
    bp = UHCIMemoryBuffer::newBuffer();
    if (bp == NULL) 
	{
        return kIOReturnNoMemory;
    }
    queue_enter(&_allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
    
    frames = (UInt32 *)bp->getBytesNoCopy();
    pPhysical = bp->getPhysicalAddress();
	USBLog(7, "AppleUSBUHCI[%p]::HardwareInit - frame list pPhysical[%p] frames[%p]", this, (void*)pPhysical, frames);
    _framesPaddr = pPhysical;
	_frameList = frames;								// pointer to the list of physical addresses
    
    // Set frame number and physical frame address
    ioWrite16(kUHCI_FRNUM, 0);
    ioWrite32(kUHCI_FRBASEADDR, pPhysical);
    USBLog(7, "AppleUSBUHCI[%p]::HardwareInit - Setting physical frame address to %p", this, (void*)pPhysical);
    
    //============= Set up queue heads =======================//
    
    // Dummy QH at the end of the list
    lastQH = AllocateQH(0, 0, 0, 0, 0, kQHTypeDummy);
    if (lastQH == NULL)
        return kIOReturnNoMemory;
	
    lastQH->_logicalNext = NULL;
    lastQH->SetPhysicalLink(kUHCI_QH_T);
    lastQH->firstTD = NULL;
    lastQH->GetSharedLogical()->elink = HostToUSBLong(kUHCI_QH_T);
    _lastQH = lastQH;
    
    // Bulk traffic queue.
    bulkQH = AllocateQH(0, 0, 0, 0, 0, kQHTypeDummy);
    if (bulkQH == NULL)
        return kIOReturnNoMemory;
	
    bulkQH->_logicalNext = lastQH;
    bulkQH->SetPhysicalLink(lastQH->GetPhysicalAddrWithType());
    bulkQH->firstTD = NULL;
    bulkQH->GetSharedLogical()->elink = HostToUSBLong(kUHCI_QH_T);
    _bulkQHStart = _bulkQHEnd = bulkQH;
    
    // Full speed control queue.
    fsQH = AllocateQH(0, 0, 0, 0, 0, kQHTypeDummy);
    if (fsQH == NULL)
        return kIOReturnNoMemory;
	
    fsQH->_logicalNext = bulkQH;
    fsQH->SetPhysicalLink(bulkQH->GetPhysicalAddrWithType());
    fsQH->firstTD = NULL;
    fsQH->GetSharedLogical()->elink = HostToUSBLong(kUHCI_QH_T);
    _fsControlQHStart = _fsControlQHEnd = fsQH;
	
    // Low speed control queue.
    lsQH = AllocateQH(0, 0, 0, 0, 0, kQHTypeDummy);
    if (lsQH == NULL)
        return kIOReturnNoMemory;
	
    lsQH->_logicalNext = fsQH;
    lsQH->SetPhysicalLink(fsQH->GetPhysicalAddrWithType());
    lsQH->firstTD = NULL;
    lsQH->GetSharedLogical()->elink = HostToUSBLong(kUHCI_QH_T);
    _lsControlQHStart = _lsControlQHEnd = lsQH;
    
    // Interrupt QH tree.
	// For 2^n virtual frames, there are n+1 interrupt QHs,
	// representing each 1/m frame polling rate.
	// The rate of _intrQH[i] is 1/2^i.
    assert( (1 << (kUHCI_NINTR_QHS - 1)) <= kUHCI_NVFRAMES );
    lastQH = lsQH;
    for (i=0; i < kUHCI_NINTR_QHS; i++) 
	{
        pQH = AllocateQH(0, 0, 0, 0, 0, kQHTypeDummy);
        if (pQH == NULL) 
		{
            return kIOReturnNoMemory;
        }
        _intrQH[i] = pQH;
        pQH->firstTD = NULL;
        pQH->GetSharedLogical()->elink = HostToUSBLong(kUHCI_QH_T);
		pQH->_logicalNext = lastQH;
		pQH->SetPhysicalLink(lastQH->GetPhysicalAddrWithType());
		// insert the queue head into the frame list as appropriate
		// note that this depends on creating the QHs with the most frequent first - _intrQH[0]
		// because that one will originally get placed into every slot, and the next one will get
		// inserted every 2nd slot, then every 4th, etc and will overwrite some of the ones places in the 
		// list in an earlier value of "i"
		frame_period = (1 << i);
		for (j=frame_period-1; j < kUHCI_NVFRAMES; j += frame_period)
		{
			frames[j] = HostToUSBLong(pQH->GetPhysicalAddrWithType());
			_logicalFrameList[j] = pQH;
		}
        lastQH = pQH;
    }
    
    // For "bandwidth reclamation", point the hardware link
	//	for the last QH back to the full speed queue head.
	//	Don't link the software pointer.
	//
    _lastQH->SetPhysicalLink(fsQH->GetPhysicalAddrWithType() | kUHCI_QH_T);					// start with a terminated list

	// Use 64-byte packets, and mark controller as configured
	Command(kUHCI_CMD_MAXP | kUHCI_CMD_CF);
	USBLog(7, "AppleUSBUHCI[%p]::HardwareInit - Command register reports %x", this, ioRead16(kUHCI_CMD));

	// Enable interrupts
	ioWrite16(kUHCI_INTR, kUHCI_INTR_TIE | kUHCI_INTR_RIE | kUHCI_INTR_IOCE | kUHCI_INTR_SPIE);
	USBLog(7, "AppleUSBUHCI[%p]::HardwareInit - Interrupt register reports %x", this, ioRead16(kUHCI_INTR));

	// Start the controller
	return Run(true);
}



IOReturn
AppleUSBUHCI::UIMInitialize(IOService * provider)
{
    IOReturn status;
    UInt32   value;
    
    USBLog(7, "+AppleUSBUHCI[%p]::UIMInitialize", this);
    
    if (!_uimInitialized) 
	{
        _device = OSDynamicCast(IOPCIDevice, provider);
        if(_device == NULL) 
		{
            return kIOReturnBadArgument;
        }
        
        // Disable the master interrupt
        EnableUSBInterrupt(false);
        
        //      _device->configWrite32(0x20, 0xFFFFFFFF);
        //      UInt32 val = _device->configRead32(0x20);
        //
        //      USBLog(3, "Read of config at 0x20 = %08x", val);
        //      return kIOReturnBadArgument;
        
        _ioMap = _device->mapDeviceMemoryWithIndex(0);
		
        USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize - _ioMap = %p", this, _ioMap);
        if (_ioMap) 
		{
            USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize - _ioMap vaddr %p, pPhysical %p", this, (void*)_ioMap->getVirtualAddress(), (void*)_ioMap->getPhysicalAddress());
        } else 
		{
            USBError(1, "AppleUSBUHCI[%p]::UIMInitialize - ioMap is NULL", this);
            return kIOReturnNoMemory;
        }
        _ioPhysAddress = _ioMap->getPhysicalAddress();
        _ioVirtAddress = _ioMap->getVirtualAddress();
        
        _frameLock = IOLockAlloc();
        if (_frameLock == NULL) 
		{
            return kIOReturnNoMemory;
        }
        
        queue_init(&_allocatedBuffers);
		
        _isocBandwidth = kUSBMaxFSIsocEndpointReqCount;
        _uhciBusState = kUHCIBusStateRunning;
		
        clock_get_uptime(&_lastTime);
        
        SetVendorInfo();
        SetDeviceName();
        
        // Do not use standardized errata bits yet
        _errataBits = GetErrataBits(_vendorID, _deviceID, _revisionID);
        
		USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize - there are %d interrupt sources", this, _numInterruptSources);
        
        //_interruptSource = IOInterruptEventSource::interruptEventSource(this, &InterruptHandler, _device);
        
        _interruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this, &InterruptHandler, &PrimaryInterruptFilter, _device);
        
        if (!_interruptSource || (_workLoop->addEventSource(_interruptSource) != kIOReturnSuccess)) 
		{
            return kIOReturnBadArgument;
        }
        
        USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize - Getting config registers:", this);
        USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize -   CLASSC: %08x", this, (unsigned int)_device->configRead32(0x08));
        USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize -   USBBASE: %08x", this, (unsigned int)_device->configRead32(0x20));
        USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize -   SBRN: %02x", this, _device->configRead8(0x60));
        
        // enable the card
        value = _device->configRead32(kIOPCIConfigCommand) & 0xFFFF0000;
        value |= (kIOPCICommandBusMaster | kIOPCICommandMemorySpace | kIOPCICommandIOSpace);
        _device->configWrite32(kIOPCIConfigCommand, value);
        
        USBLog(7, "AppleUSBUHCI[%p]::UIMInitialize - calling HardwareInit:", this);
        
        status = HardwareInit();
        
        USBLog(7, "AppleUSBUHCI[%p]:: UIMInitialize - status after init: %p", this, (void*)status);
        
        // Set up a periodic timer to check the root hub status
        _rhTimer = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action) RHTimerFired);
        
        if ( _rhTimer == NULL )
        {
            USBError(1, "AppleUSBUHCI[%p]::UIMInitialize - couldn't allocate timer event source", this);
            return kIOReturnNoMemory;
        }
        
        if ( _workLoop->addEventSource( _rhTimer ) != kIOReturnSuccess )
        {
            USBError(1, "AppleUSBUHCI[%p]::UIMInitialize - couldn't add timer event source", this);
            return kIOReturnError;
        }
		
		_uhciBusState = kUHCIBusStateRunning;
		
        // Enable interrupts
        EnableUSBInterrupt(true);
        
        // Note that the timer isn't scheduled to send events yet.
        
        // enable interrupt delivery
        _workLoop->enableAllInterrupts();
		
        _uimInitialized = true;
    }
    
    USBLog(7, "-AppleUSBUHCI[%p]::UIMInitialize", this);
    
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::UIMFinalize()
{
    AppleUHCIQueueHead			*pQH;
    
    USBLog(3, "AppleUSBUHCI[%p]::UIMFinalize", this);
    
    // Turn off ports.
    RHEnablePort(1, false);
    RHEnablePort(2, false);
	
    // Stop and suspend controller.
    SuspendController();
    
    _workLoop->disableAllInterrupts();
    
    if (!isInactive()) 
	{
        // Disable controller in PCI space.
        // XXX
        
        // Release I/O resources.
        if (_ioMap) 
		{
            _ioMap->release();
            _ioMap = NULL;
        }
    }
	
    // Clean up our power down notifier.  That will release it.
    //
    if ( _powerDownNotifier ) 
	{
        _powerDownNotifier->remove();
        _powerDownNotifier = NULL;
    }
	
    USBLog(3, "AppleUSBUHCI[%p]::UIMFinalize freeing memory", this);
    
    // Free allocated TD, QH, transaction and frame memory.
	
    while (!queue_empty(&_allocatedBuffers)) 
	{
        UHCIMemoryBuffer *bp;
        
        queue_remove_first(&_allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
        bp->release();
    }
    
    // TODO: free the transfer descriptor memory blocks
    // TODO: free the queue head memory blocks
	
    if (_rhTimer)
    {
        if ( _workLoop )
            _workLoop->removeEventSource(_rhTimer);
        
        _rhTimer->release();
        _rhTimer = NULL;
    }
	
    USBLog(3, "AppleUSBUHCI[%p]::UIMFinalize - removing interrupt source", this);
	
    if (_interruptSource) 
	{
        _workLoop->removeEventSource(_interruptSource);
        _interruptSource->release();
        _interruptSource = NULL;
    }
	
    IOLockFree(_frameLock);
    _frameLock = NULL;
    
    if (_deviceNameLen) 
	{
        IOFree((void *)_deviceName, _deviceNameLen);
        _deviceName = NULL;
        _deviceNameLen = 0;
    }
    
    _uimInitialized = false;
    
    USBLog(3, "AppleUSBUHCI[%p]::UIMFinalize done", this);
    
    return kIOReturnSuccess;
}



// Initialize the controller hardware after powering up (e.g. from sleep).
// Does not start the controller.
IOReturn 
AppleUSBUHCI::UIMInitializeForPowerUp()
{
    UInt32 value;
    
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp", this);
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp before: kUHCI_FRBASEADDR[%p] _saveFrameAddress[%p]", this, (void*)ioRead32(kUHCI_FRBASEADDR), (void*)_saveFrameAddress);
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp kUHCI_FRNUM[%p] _saveFrameNumber[%p]", this, (void*)ReadFrameNumberRegister(), (void*)_saveFrameNumber);
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp kUHCI_INTR[%p] _saveInterrupts[%p]", this, (void*)ioRead16(kUHCI_INTR), (void*)_saveInterrupts);
    ioWrite32(kUHCI_FRBASEADDR, _saveFrameAddress);
    ioWrite16(kUHCI_FRNUM, _saveFrameNumber);
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp after: kUHCI_FRBASEADDR[%p]", this, (void*)ioRead32(kUHCI_FRBASEADDR));
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp after: kUHCI_FRNUM[%p]", this,  (void*)ReadFrameNumberRegister());
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp after: kUHCI_INTR[%p]", this, (void*)ioRead16(kUHCI_INTR));
	_saveFrameNumber = 0;
	_saveFrameAddress = 0;
    
    Command(kUHCI_CMD_MAXP | kUHCI_CMD_CF | ioRead16(kUHCI_CMD));
    USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp Command register reports %p", this, (void*)ioRead16(kUHCI_CMD));
    
    // Enable bus mastering
    value = _device->configRead32(kIOPCIConfigCommand) & 0xFFFF0000;
    value |= (kIOPCICommandBusMaster | kIOPCICommandMemorySpace | kIOPCICommandIOSpace);
    _device->configWrite32(kIOPCIConfigCommand, value);
	_uhciAvailable = true;

	// Enable interrupts
	ioWrite16(kUHCI_INTR, _saveInterrupts);
	_saveInterrupts = 0;
    if (_rootHubPollingRate && _outstandingTrans[0].completion.action)
	{
		USBLog(2, "AppleUSBUHCI[%p]::UIMInitalizeForPowerUp starting rhTimer(%d)", this, _rootHubPollingRate);
        _rhTimer->setTimeoutMS(_rootHubPollingRate);
	}
	USBLog(2, "AppleUSBUHCI[%p]::UIMInitializeForPowerUp - enabling master interrupt INTR[%p]", this, (void*)ioRead16(kUHCI_INTR));
	EnableUSBInterrupt(true);
    
    return kIOReturnSuccess;
}



// Finalize controller hardware for powering down.
// Assumes that the controller is stopped.
IOReturn
AppleUSBUHCI::UIMFinalizeForPowerDown()
{
    UInt32 value;
    
    USBLog(2, "AppleUSBUHCI[%p]::UIMFinalizeForPowerDown", this);
    
    _saveFrameAddress = ioRead32(kUHCI_FRBASEADDR);
    USBLog(2, "AppleUSBUHCI[%p]::UIMFinalizeForPowerDown _saveFrameAddress[%p]", this, (void*)_saveFrameAddress);
    _saveFrameNumber = ioRead16(kUHCI_FRNUM);
    USBLog(2, "AppleUSBUHCI[%p]::UIMFinalizeForPowerDown _saveFrameNumber[%p]", this, (void*)_saveFrameNumber);
    _saveInterrupts = ioRead16(kUHCI_INTR);
    USBLog(2, "AppleUSBUHCI[%p]::UIMFinalizeForPowerDown _saveInterrupts[%p]", this, (void*)_saveInterrupts);

    // Disable interrupts
    ioWrite16(kUHCI_INTR, 0);
    USBLog(2, "AppleUSBUHCI[%p]::UIMFinalizeForPowerDown cancelling rhTimer", this);
    _rhTimer->cancelTimeout();										// This is the root hub status change interrupt

    // Disable bus mastering
	_uhciAvailable = false;
    value = _device->configRead32(kIOPCIConfigCommand) & 0xFFFF0000;
    value |= (kIOPCICommandMemorySpace | kIOPCICommandIOSpace);
    _device->configWrite32(kIOPCIConfigCommand, value);
    
	USBLog(2, "AppleUSBUHCI[%p]::UIMFinalizeForPowerDown - disabling master interrupt - INTR[%p]", this, (void*)ioRead16(kUHCI_INTR));
	EnableUSBInterrupt(false);
		
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::message( UInt32 type, IOService * provider,  void * argument )
{
    return super::message( type, provider, argument );
}


void 
AppleUSBUHCI::SetVendorInfo(void)
{
    OSData		*vendProp, *deviceProp, *revisionProp;
	
    // Get this chip's vendID, deviceID, revisionID.
    vendProp     = OSDynamicCast(OSData, _device->getProperty( "vendor-id" ));
	
    if (vendProp)
        _vendorID = *((UInt32 *) vendProp->getBytesNoCopy());
	
    USBLog(7, "AppleUSBUHCI[%p]::SetVendorInfo - vendorID = %p", this, (void*)_vendorID);
    
    deviceProp   = OSDynamicCast(OSData, _device->getProperty( "device-id" ));
    if (deviceProp)
        _deviceID   = *((UInt32 *) deviceProp->getBytesNoCopy());
	
    revisionProp = OSDynamicCast(OSData, _device->getProperty( "revision-id" ));
    if (revisionProp)
        _revisionID = *((UInt32 *) revisionProp->getBytesNoCopy());
    
    if (_vendorID == 0x1106) 
	{
        // VIA controllers.
		// After a BABBLE error, the controller seems to lock up.
        _errataBits = kUHCIResetAfterBabble;
    }
}



UInt32
AppleUSBUHCI::GetBandwidthAvailable()
{
    USBLog(7, "AppleUSBUHCI[%p]::GetBandwidthAvailable returns %d", this, (int)_isocBandwidth);
    return _isocBandwidth;
}


// ========================================================================
#pragma mark Hardware control
// ========================================================================
void
AppleUSBUHCI::GlobalReset(void)
{
    USBLog(4, "+AppleUSBUHCI[%p]::GlobalReset", this);
    Command(kUHCI_CMD_GRESET);
    IOSleep(kUHCI_RESET_DELAY);
    Command(0);
    USBLog(4, "-AppleUSBUHCI[%p]::GlobalReset", this);
}



IOReturn
AppleUSBUHCI::Reset(bool enableInterrupts)
{
    int					i;
    
    USBLog(2, "+AppleUSBUHCI[%p]::Reset", this);
    Command(kUHCI_CMD_HCRESET);
    for(i=0; (i < kUHCI_RESET_DELAY) && (ioRead16(kUHCI_CMD) & kUHCI_CMD_HCRESET); i++) 
	{
        IOSleep(1);
    }
    if (i >= kUHCI_RESET_DELAY) 
	{
        USBError(1, "%s: controller reset failed", getName());
        return kIOReturnTimeout;
    }
    USBLog(2, "AppleUSBUHCI[%p]::Reset - reset done after %d spins", this, i);
    
    if (_framesPaddr != NULL) 
	{
        ioWrite32(kUHCI_FRBASEADDR, _framesPaddr);
        USBLog(2, "AppleUSBUHCI[%p]::Reset - Command register reports %x", this, ioRead16(kUHCI_CMD));
        
        ioWrite16(kUHCI_FRNUM, (UInt16)(_lastFrameNumberLow & kUHCI_FRNUM_MASK));
		
        // Use 64-byte packets, and mark controller as configured
        Command(kUHCI_CMD_MAXP | kUHCI_CMD_CF);
        USBLog(2, "AppleUSBUHCI[%p]::Reset - Interrupt register before reports %x", this, ioRead16(kUHCI_INTR));
        if (enableInterrupts) 
		{
            // Enable interrupts
            ioWrite16(kUHCI_INTR, kUHCI_INTR_TIE | kUHCI_INTR_RIE | kUHCI_INTR_IOCE | kUHCI_INTR_SPIE);
            USBLog(2, "AppleUSBUHCI[%p]::Reset - Interrupt register after reports %x", this, ioRead16(kUHCI_INTR));        
        }
    }
	return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::Run(bool run)
{
    bool				state;
    UInt16				cmd;
    int					i;
    IOReturn			status = kIOReturnTimeout;
    
    USBLog(2, "AppleUSBUHCI[%p]::Run(%s)", this, run ? "true" : "false");
    
    //_workLoop->disableAllInterrupts();
    cmd = ioRead16(kUHCI_CMD);
    if (run) 
	{
        cmd = cmd | kUHCI_CMD_RS;
    } else 
	{
        cmd = cmd & ~kUHCI_CMD_RS;
    }
    USBLog(2, "AppleUSBUHCI[%p]::Run - About to write command 0x%x", this, cmd);
    Command(cmd);
    USBLog(2, "AppleUSBUHCI[%p]::Run - Waiting for controller to come ready", this);
    for (i=0; i<20; i++) 
	{
        state = ((ioRead16(kUHCI_STS) & kUHCI_STS_HCH) == 0);
        if (run == state) 
		{
            status = kIOReturnSuccess;
            break;
        }
        IOSleep(1);
    }
    USBLog(2, "AppleUSBUHCI[%p]::Run - Finished waiting with result %d", this, status);
    //if (run) {
    //    _workLoop->enableAllInterrupts();
    //}
    
	USBLog(2, "AppleUSBUHCI[%p]::Run - run resulted in status %d, command port %x", this, status, ioRead16(kUHCI_CMD));
	return status;
}



// For now, the frame number is really only 32 bits
UInt64
AppleUSBUHCI::GetFrameNumber()
{
    UInt32				lastFrameNumber;
    UInt32				lastFrame;
    UInt32				thisFrame;
    UInt32				overflow;
    UInt32				newFrame;
    
	// If the controller is halted, then we should just bail out
	if (ioRead16(kUHCI_STS) & kUHCI_STS_HCH)
	{
		if (!_idleSuspend)
		{
			USBLog(1, "AppleUSBUHCI[%p]::GetFrameNumber called but controller is halted",  this);
		}
		return 0;
	}
	
    if (_lastFrameNumberLow >= (UInt32)(~kUHCI_FRNUM_MASK)) 
	{
        USBLog(7, "AppleUSBUHCI[%p]::GetFrameNumber - locking to check frame number", this);
        IOLockLock(_frameLock);
        lastFrameNumber = _lastFrameNumberLow;
		
        overflow = lastFrameNumber & (~kUHCI_FRNUM_MASK);
        lastFrame = lastFrameNumber & kUHCI_FRNUM_MASK;
		
        thisFrame = ReadFrameNumberRegister();
        if (lastFrame <= thisFrame) 
		{
            // No 11-bit overflow
            newFrame = overflow + thisFrame;
        } else 
		{
            // 11-bit and 32-bit overflow
            _lastFrameNumberHigh++;
            newFrame = overflow + thisFrame + kUHCI_FRNUM_COUNT;
            USBLog(7, "AppleUSBUHCI[%p]::GetFrameNumber - 64-bit frame number overflow (low %p)", this, (void*)newFrame);
        }
        _lastFrameNumberLow = newFrame;
        IOLockUnlock(_frameLock);
    } else do 
	{
        lastFrameNumber = _lastFrameNumberLow;
        
        overflow = lastFrameNumber & (~kUHCI_FRNUM_MASK);
        lastFrame = lastFrameNumber & kUHCI_FRNUM_MASK;
        
        thisFrame = ReadFrameNumberRegister();
        if (lastFrame <= thisFrame) 
		{
            // No 11-bit overflow
            newFrame = overflow + thisFrame;
        } else // if (overflow < (~kUHCI_FRNUM_MASK))
		{
            // 11-bit overflow, but no 32-bit overflow
            newFrame = overflow + thisFrame + kUHCI_FRNUM_COUNT;
            USBLog(7, "AppleUSBUHCI[%p]::GetFrameNumber - 11-bit frame number overflow", this);
        }
        
    } while (!OSCompareAndSwap(lastFrameNumber, newFrame, &_lastFrameNumberLow));
    
    USBLog(7, "AppleUSBUHCI[%p]:: GetFrameNumber - frame number is %qx", this, (UInt64)newFrame | ((UInt64)_lastFrameNumberHigh << 32));
    return (UInt64)newFrame | ((UInt64)_lastFrameNumberHigh << 32);
}



UInt32
AppleUSBUHCI::GetFrameNumber32()
{
    return (UInt32)GetFrameNumber();
}


// ========================================================================
#pragma mark I/O
// ========================================================================

#if defined(__ppc__)
void 
AppleUSBUHCI::ioWrite8(UInt16 offset, UInt8 value) {
    ((volatile UInt8 *)_ioVirtAddress)[ offset ] = value;
    eieio();
    IODelay(10);
}
void
AppleUSBUHCI::ioWrite16(UInt16 offset, UInt16 value) {
    OSWriteSwapInt16((volatile void *)_ioVirtAddress, offset, value);
    eieio();
    IODelay(10);
}
void 
AppleUSBUHCI::ioWrite32(UInt16 offset, UInt32 value) {
    OSWriteSwapInt32((volatile void *)_ioVirtAddress, offset, value);
    eieio();
    IODelay(10);
}
UInt8
AppleUSBUHCI::ioRead8(UInt16 offset) {
    UInt8 value = ((volatile UInt8 *)_ioVirtAddress)[ offset ];
    eieio();
    return value;
}
UInt16 
AppleUSBUHCI::ioRead16(UInt16 offset) {
    UInt16 value = OSReadSwapInt16((volatile void *)_ioVirtAddress, offset);
    eieio();
    return value;
}
UInt32
AppleUSBUHCI::ioRead32(UInt16 offset) {
    UInt32 value =  OSReadSwapInt32((volatile void *)_ioVirtAddress, offset);
    eieio();
    return value;
}

#elif defined(__i386__)
extern __inline__ unsigned long inl(
                                    UInt16 port)
{
    UInt32 value;
    __asm__ volatile("inl %1, %0" : "=a" (value) : "d" (port));
    return (value);
}

extern __inline__ unsigned short inw(
                                     UInt16 port)
{
    UInt16 value;
    __asm__ volatile(".byte 0x66; inl %1, %0" : "=a" (value) : "d" (port));
    return (value);
}

extern __inline__ unsigned char inb(
                                    UInt16 port)
{
    UInt8 value;
    __asm__ volatile("inb %1, %0" : "=a" (value) : "d" (port));
    return (value);
}

extern __inline__ void outl(
                            UInt16 port,
                            UInt32 value)
{
    __asm__ volatile("outl %0, %1" : : "a" (value), "d" (port));
}

extern __inline__ void outw(
                            UInt16 port,
                            UInt16 value)
{
    __asm__ volatile(".byte 0x66; outl %0, %1" : : "a" (value), "d" (port));
}

extern __inline__ void outb(
                            UInt16 port,
                            UInt8 value)
{
    __asm__ volatile("outb %0, %1" : : "a" (value), "d" (port));
}

void 
AppleUSBUHCI::ioWrite8(UInt16 offset, UInt8 value) {
    outb(_ioPhysAddress + offset, value);
}
void
AppleUSBUHCI::ioWrite16(UInt16 offset, UInt16 value) {
    outw(_ioPhysAddress + offset, value);
}
void 
AppleUSBUHCI::ioWrite32(UInt16 offset, UInt32 value) {
    outl(_ioPhysAddress + offset, value);
}
UInt8
AppleUSBUHCI::ioRead8(UInt16 offset) {
    return inb(_ioPhysAddress + offset);
}
UInt16 
AppleUSBUHCI::ioRead16(UInt16 offset) {
    return inw(_ioPhysAddress + offset);
}
UInt32
AppleUSBUHCI::ioRead32(UInt16 offset) {
    return inl(_ioPhysAddress + offset);
}
#else
#error Unknown architecture
#endif

struct UHCIDeviceInfo {
	UInt16 device_id;
	const char *device_name;
};

static struct UHCIDeviceInfo UHCI_Intel_devices[] = {
	{0x2412, "82801AA (ICH)"},
	{0x2422, "82801AB (ICH0)"},
	{0x2442, "82801BA/BAM (ICH2) USB-A"},
	{0x2444, "82801BA/BAM (ICH2) USB-B"},
	{0x2452, "82801E"},
	{0x2482, "82801CA/CAM (ICH3) USB-A"},
	{0x2484, "82801CA/CAM (ICH3) USB-B"},
	{0x2487, "82801CA/CAM (ICH3) USB-C"},
	{0x24c2, "82801DB (ICH4) USB-A"},
	{0x24c4, "82801DB (ICH4) USB-B"},
	{0x24c7, "82801DB (ICH4) USB-C"},
	{0x24d2, "82801EB/ER (ICH5/ICH5R) USB-A"},
	{0x24d4, "82801EB/ER (ICH5/ICH5R) USB-B"},
	{0x24d7, "82801EB/ER (ICH5/ICH5R) USB-C"},
	{0x24de, "82801EB/ER (ICH5/ICH5R) USB-D"},
	{0x25a9, "6300ESB"},
	{0x24aa, "6300ESB"},
	{0x7020, "82371SB (PIIX3)"},
	{0x7112, "82371AB/EB/MB (PIIX4)"},
	{0x719a, "82443MX"},
	{0x7602, "82372FB/82468GX (PIIX5)"},
	{0, 0}
};

static struct UHCIDeviceInfo UHCI_VIA_devices[] = {
	{0x3038, "VT83C572, VT6202"},
	{0, 0}
};

static struct UHCIVendorInfo 
{
	UInt16 vendor_id;
	const char *vendor_name;
	struct UHCIDeviceInfo *devices;
} UHCIVendorInfo[] = 
{
	{0x8086, "Intel", UHCI_Intel_devices},
	{0x1106, "VIA", UHCI_VIA_devices},
	{0, 0, 0}
};



void
AppleUSBUHCI::SetDeviceName(void)
{
	struct UHCIVendorInfo *vi;
	struct UHCIDeviceInfo *di, *di_found = NULL;
	
	USBLog(7, "AppleUSBUHCI[%p]::SetDeviceName", this);
	for (vi = &UHCIVendorInfo[0]; vi->vendor_name != NULL; vi++) 
	{
		USBLog(7, "AppleUSBUHCI[%p]::SetDeviceName - vendor: %s", this, vi->vendor_name);
		if (vi->vendor_id == _vendorID) 
		{
			for (di = vi->devices; di->device_name != NULL; di++) 
			{
				USBLog(7, "AppleUSBUHCI[%p]::SetDeviceName - device: %s", this, di->device_name);
				if (di->device_id == _deviceID) 
				{
					di_found = di;
					break;
				}
			}
		}
		if (di_found != NULL) 
		{
			break;
		}
	}
	if (di_found == NULL) 
	{
		_deviceNameLen = 0;
		_deviceName = "Generic UHCI USB Controller";
	} else 
	{
		_deviceNameLen = strlen(vi->vendor_name) + strlen(di_found->device_name) + strlen("UHCI USB Controller") + 4;
		char *str = (char *)IOMalloc(_deviceNameLen);
		sprintf(str, "%s %s UHCI USB Controller", vi->vendor_name, di_found->device_name);
		_deviceName = str;
	}
	USBLog(7, "AppleUSBUHCI[%p]::SetDeviceName: %s", this, _deviceName);
}



void
AppleUSBUHCI::ProcessCompletedTransactions(void)
{
    IOReturn 			err, err1;
    int 			i;
		
    err = scavengeIsochTransactions();
    if(err != kIOReturnSuccess)
    {
		USBLog(3, "AppleUSBUHCI[%p]::ProcessCompletedTransactions err isoch list %x", this, err);
    }
    err = scavengeQueueHeads(_intrQH[kUHCI_NINTR_QHS - 1]);
    if(err != kIOReturnSuccess)
    {
		USBLog(3, "AppleUSBUHCI[%p]::ProcessCompletedTransactions -  err queue heads %x", this, err);
    }
	
}



IOReturn						
AppleUSBUHCI::scavengeIsochTransactions(void)
{
    AppleUHCIIsochTransferDescriptor 	*pDoneEl;
    UInt32								cachedProducer;
    UInt32								cachedConsumer;
    IOUSBControllerIsochEndpoint*		pEP;
    AppleUHCIIsochTransferDescriptor	*prevEl;
    AppleUHCIIsochTransferDescriptor	*nextEl;
    IOInterruptState					intState;
	
    // Get the values of the Done Queue Head and the producer count.  We use a lock and disable interrupts
    // so that the filter routine does not preempt us and updates the values while we're trying to read them.
    //
    intState = IOSimpleLockLockDisableInterrupt( _wdhLock );
    
    pDoneEl = (AppleUHCIIsochTransferDescriptor*)_savedDoneQueueHead;
    cachedProducer = _producerCount;
    
    IOSimpleLockUnlockEnableInterrupt( _wdhLock, intState );
    
    cachedConsumer = _consumerCount;
	
    if (pDoneEl && (cachedConsumer != cachedProducer))
    {
		// there is real work to do - first reverse the list
		prevEl = NULL;
		USBLog(7, "AppleUSBUHCI[%p]::scavengeIsocTransactions - before reversal, cachedConsumer[%d] cachedProducer[%d]", this, (int)cachedConsumer, (int)cachedProducer);
		while (true)
		{
			pDoneEl->_logicalNext = prevEl;
			prevEl = pDoneEl;
			cachedConsumer++;
			if (pDoneEl->_pEndpoint)
			{
				pDoneEl->_pEndpoint->onProducerQ--;
				pDoneEl->_pEndpoint->onReversedList++;
			}
			if ( cachedProducer == cachedConsumer)
				break;
			
			pDoneEl = OSDynamicCast(AppleUHCIIsochTransferDescriptor, pDoneEl->_doneQueueLink);
		}
		
		// update the consumer count
		_consumerCount = cachedConsumer;
		
		USBLog(7, "AppleUSBUHCI[%p]::scavengeIsocTransactions - after reversal, cachedConsumer[0x%lx]", this, cachedConsumer);
		// now cachedDoneQueueHead points to the head of the done queue in the right order
		while (pDoneEl)
		{
			nextEl = OSDynamicCast(AppleUHCIIsochTransferDescriptor, pDoneEl->_logicalNext);
			pDoneEl->_logicalNext = NULL;
			if (pDoneEl->_pEndpoint)
			{
				pDoneEl->_pEndpoint->onReversedList--;
			}
			USBLog(7, "AppleUSBUHCI[%p]::scavengeIsocTransactions - about to scavenge TD %p", this, pDoneEl);
			scavengeAnIsochTD(pDoneEl);
			pDoneEl = nextEl;
		}
    }
    
    pEP = _isochEPList;
    while (pEP)
    {
		if (pEP->onReversedList)
		{
			USBLog(1, "AppleUSBUHCI[%p]::scavengeIsocTransactions - EP (%p) still had %ld TDs on the reversed list!!", this, pEP, pEP->onReversedList);
		}
		ReturnIsochDoneQueue(pEP);
		AddIsochFramesToSchedule(pEP);
		pEP = pEP->nextEP;
    }
    return kIOReturnSuccess;
	
}



IOReturn
AppleUSBUHCI::scavengeAnIsochTD(AppleUHCIIsochTransferDescriptor *pTD)
{
    IOUSBControllerIsochEndpoint*			pEP;
    IOReturn								ret;
    AbsoluteTime							timeStamp;
	
    pEP = pTD->_pEndpoint;
    clock_get_uptime(&timeStamp);
    if(pEP == NULL)
    {
		USBError(1, "AppleUSBUHCI[%p]::scavengeAnIsochTD - could not find endpoint associated with iTD (%p)", this, pTD->_pEndpoint);
    }
    else
    {	
		if (!pTD->_lowLatency)
			ret = pTD->UpdateFrameList(timeStamp);		// TODO - accumulate the return values
		
		if (pTD->frStatus)
		{
			if ( pTD->frStatus == kIOReturnUnderrun )
			{
				USBLog(7, "AppleUSBUHCI[%p]::scavengeAnIsochTD - frStatus is %p - _frameNumber %Ld - _frameIndex %d", this, (void*)pTD->frStatus, pTD->_frameNumber, (int)pTD->_frameIndex);
			}
			else
			{
				USBLog(3, "AppleUSBUHCI[%p]::scavengeAnIsochTD - frStatus is %p - _frameNumber %Ld - _frameIndex %d", this, (void*)pTD->frStatus, pTD->_frameNumber, (int)pTD->_frameIndex);
			}

		}
		PutTDonDoneQueue(pEP, pTD, true);
    }

    return(kIOReturnSuccess);
}



IOReturn						
AppleUSBUHCI::scavengeQueueHeads(IOUSBControllerListElement *pLE)
{
    AppleUHCITransferDescriptor			*doneQueue = NULL, *doneTail= NULL, *qHead, *qTD, *qEnd;
    UInt32								ctrlStatus, leCount = 0, tdCount = 0, lastToggle = 0;
	UInt16								actLength;
    Boolean								TDisHalted, shortTransfer;
    AppleUHCIQueueHead					*pQH;
	bool								logging = false;
    
    while( (pLE != NULL) && (leCount++ < 150000) )
    {
		pQH = OSDynamicCast(AppleUHCIQueueHead, pLE);
		if(pQH && (pQH->type != kQHTypeDummy) && (!pQH->stalled))
		{
			bool	foundInactive = false;
			
			qTD = qHead = pQH->firstTD;
			qEnd = pQH->lastTD;
			if (((qTD == NULL) || (qEnd == NULL)) && (qTD != qEnd))
			{
				USBError(1, "The UHCI driver found a device queue with invalid head (%p) or tail (%p)", qTD, qEnd);
			}
			TDisHalted = false;
			shortTransfer = false;
			if (qTD && (qTD != qEnd))
			{
				USBLog(7, "AppleUSBUHCI[%p]::scavengeQueueHeads - looking at pQH[%p]=========================================", this, pQH);
				logging = true;
			}
				
			while( qTD && (qTD != qEnd) && (tdCount++ < 150000) )
			{	
				// This end point has transactions
				ctrlStatus = USBToHostLong(qTD->GetSharedLogical()->ctrlStatus);
				actLength = UHCI_TD_GET_ACTLEN(ctrlStatus);
				if(!TDisHalted && !shortTransfer)
				{
					if((ctrlStatus & kUHCI_TD_ACTIVE) != 0)
					{	// Command is still alive, go to next queue
						if (foundInactive)
						{
							USBLog(7, "scavengeQueueHeads - found still active TD %p at the end", qTD);
							qTD->print(7);
						}
						break;
					}
					if (!foundInactive)
					{
						USBLog(7, "scavengeQueueHeads - found non-active TD %p in QH %p", qTD, pQH);
						pQH->print(7);
						qTD->print(7);
						foundInactive = true;
					}
					// check for halted
					TDisHalted = ((ctrlStatus & kUHCI_TD_STALLED) ? true : false) ;
					if (!TDisHalted)
					{
						// this TD is not active, and was not halted, so check to see if it was short
						// if so - we can ignore that state of the remaining TDs until the lastTD
						// since the harwdare skipped them
						if ((ctrlStatus & kUHCI_TD_SPD) && (actLength < UHCI_TD_GET_MAXLEN(USBToHostLong(qTD->GetSharedLogical()->token))))
						{
							USBLog(6, "scavengeQueueHeads - found short TD %p is short", qTD);
							shortTransfer = true;
							lastToggle = USBToHostLong(qTD->GetSharedLogical()->token) & kUHCI_TD_D;			// will be used later
						}
					}
					else
					{
						USBLog(6, "scavengeQueueHeads - found stalled TD %p", qTD);
						pQH->stalled = true;
					}
				}
				if (qTD->buffer && (qTD->direction == kUSBIn) && actLength)
				{
					USBLog(1, "AppleUSBUHCI[%p]::scavengeQueueHeads - writing back from alignment buffer", this);
					qTD->buffer->userBuffer->writeBytes(qTD->buffer->userOffset, (void*)qTD->buffer->vaddr, actLength);
				}
				if (qTD->lastTDofTransaction)
				{
					// We have the complete command
					USBLog(7, "AppleUSBUHCI[%p]::scavengeQueueHeads - TD (%p) is last of transaction", this, qTD);
					qTD->print(7);
					if(doneQueue == NULL)
					{
						doneQueue = qHead;
					}
					else
					{
						doneTail->_logicalNext = qHead;
					}
					doneTail = qTD;
					qTD = OSDynamicCast(AppleUHCITransferDescriptor, qTD->_logicalNext);					// qTD now points to the next TD AFTER the last TD of the trasnaction
					qHead = qTD;
					doneTail->_logicalNext = NULL;
					if (qTD == NULL)
					{
						USBError(1, "The UHCI driver found a NULL Transfer Descriptor");
						break;
					}
					// at this point we need to update pQH->GetSharedLogical()->elink with the new qTD
					// however, before we do that, we might need to adjust active bits or D bits in the rest of the queue
					// if halted, we need to make them all inactive
					// is short, we might need to flip all of the DBits
					if(!TDisHalted && shortTransfer)
					{
						// we don't need to flip toggle bits on control queues, since each phase is a separate "transaction"
						// and each phase controls its own toggle state
						if ((pQH->type != kUSBControl) && ((USBToHostLong(qTD->GetSharedLogical()->token) & kUHCI_TD_D) == lastToggle))
						{
							AppleUHCITransferDescriptor		*tempTD = qTD;
							// if the toggle bits are the same, then we need to swap them all
							while (tempTD)
							{
								UInt32 token = tempTD->GetSharedLogical()->token;
								lastToggle = lastToggle ? 0 : HostToUSBLong(kUHCI_TD_D);
								token &= ~HostToUSBLong(kUHCI_TD_D);
								tempTD->GetSharedLogical()->token = token | lastToggle;
								tempTD = OSDynamicCast(AppleUHCITransferDescriptor, tempTD->_logicalNext);
							}
						}
						// need to set the elink, which was not advanced on the short packet
						pQH->GetSharedLogical()->elink = HostToUSBLong(qTD->GetPhysicalAddrWithType());
					}
					else if (TDisHalted)
					{
						// on a halted TD, which is an error, qTD now points to either the dummy TD (which is inactive)
						// or the next TD after the last TD in the chain which caused the error. In that case, we are going to
						// set the hardware elink to TERMINATED so that we don't see the possibly active TD which is next
						// but we won't actually ever process that TD until after a ClearEndpointHalt or an Abort
						pQH->GetSharedLogical()->elink = HostToUSBLong(kUHCI_QH_T);
					}
					// we are going to return the TDs between the curent firstTD and the new qTD, so change the firstTD
					pQH->firstTD = qTD;
					
                    // Reset our loop variables
                    //
                    TDisHalted = false;
                    shortTransfer = false;
                } 
				else
				{
					USBLog(7, "AppleUSBUHCI[%p]::scavengeQueueHeads - looking past TD (%p) to TD (%p)", this, qTD, qTD->_logicalNext); 
					qTD = OSDynamicCast(AppleUHCITransferDescriptor, qTD->_logicalNext);
					if (qTD == NULL)
					{
						USBError(1, "The UHCI driver found a NULL Transfer Descriptor");
						break;
					}
					else
						qTD->print(7);
				}
			}
			if (logging)
			{
				USBLog(7, "AppleUSBUHCI[%p]::scavengeQueueHeads - done with pQH[%p]=========================================", this, pQH);
				logging = false;
			}
		}
		pLE = pLE->_logicalNext;
    }

    if(doneQueue != NULL)
    {
		UHCIUIMDoDoneQueueProcessing(doneQueue, kIOReturnSuccess, NULL);
    }
    if(leCount > 1000)
    {
		USBLog(1, "AppleUSBUHCI[%p]::scavengeQueueHeads looks like bad ed queue (%d)", this, (int)leCount);
    }
    
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::UHCIUIMDoDoneQueueProcessing(AppleUHCITransferDescriptor *pHCDoneTD, OSStatus forceErr, AppleUHCITransferDescriptor *stopAt)
{
    UInt32							ctrlStatus, token;
    UInt32							bufferSizeRemaining = 0;
    AppleUHCITransferDescriptor		*nextTD;
    OSStatus						accumErr = kIOReturnSuccess;
	
    USBLog(7, "+AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing", this);
    while (pHCDoneTD != NULL)
    {
        IOReturn	errStatus;
        if(pHCDoneTD == stopAt)
        {
            // Don't process this one or any further
            USBLog(7, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing stop at %p", this, pHCDoneTD);
            break;
        }
		
        nextTD	= OSDynamicCast(AppleUHCITransferDescriptor, pHCDoneTD->_logicalNext);
        ctrlStatus = USBToHostLong(pHCDoneTD->GetSharedLogical()->ctrlStatus);
        token = USBToHostLong(pHCDoneTD->GetSharedLogical()->token);
		if (forceErr != kIOReturnSuccess)
		{
            errStatus = forceErr;
		}
		else if (accumErr != kIOReturnSuccess)
        {
            errStatus = accumErr;
        }
        else
        {
            errStatus = TDToUSBError(ctrlStatus);
            accumErr = errStatus;
            if (errStatus)
            {
                USBLog(4, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing - got errStatus 0x%x on TD %p", this, errStatus, pHCDoneTD);
				pHCDoneTD->print(4);
				
				// If we have a BABBLE on this TD, then call the UIMRootHubStatusChange immediately
				UInt32 value = USBToHostLong(pHCDoneTD->GetSharedLogical()->ctrlStatus);
				if ( value & kUHCI_TD_BABBLE )
				{
					USBLog(4, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing - TD (%p) had the BABBLE bit on (0x%x), calling UIMRootHubStatusChange directly()", this, pHCDoneTD, value);
					UIMRootHubStatusChange();
				}
				
            }
        }
		
		bufferSizeRemaining += (UHCI_TD_GET_MAXLEN(token) - UHCI_TD_GET_ACTLEN(ctrlStatus));
		
		if (pHCDoneTD->lastTDofTransaction)
		{
			if ( pHCDoneTD->command == NULL )
			{
				// IOPanic("pHCDoneTD->command is NULL in UHCIUIMDoneQueueProcessing");
				USBError (1, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing pHCDoneTD->command is NULL (%p)", this, pHCDoneTD);
			}
			else
			{
				IOUSBCompletion completion = pHCDoneTD->command->GetUSLCompletion();
				if(completion.action)
				{
					// remove flag before completing
					pHCDoneTD->lastTDofTransaction = false;
					if (errStatus)
						USBLog(3, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing - calling completion routine - err[%p] remain[%p]", this, (void*)errStatus, (void*)bufferSizeRemaining);
					Complete(completion, errStatus, bufferSizeRemaining);
					if ((pHCDoneTD->pQH->type == kUSBControl) || (pHCDoneTD->pQH->type == kUSBBulk))
					{
						if (!_controlBulkTransactionsOut)
						{
							USBError(1, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing - _controlBulkTransactionsOut underrun!", this);
						}
						else
						{
							_controlBulkTransactionsOut--;
							USBLog(6, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing - _controlBulkTransactionsOut(%p) pHCDoneTD(%p)", this, (void*)_controlBulkTransactionsOut, pHCDoneTD);
							if (!_controlBulkTransactionsOut)
							{
								UInt32 link;
								link = _lastQH->GetPhysicalLink();
								USBLog(6, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing - no more _controlBulkTransactionsOut - terminating list (%p to %p)", this, (void*)link, (void*)(link | kUHCI_QH_T));
								_lastQH->SetPhysicalLink(link | kUHCI_QH_T);
							}
						}
					}
					bufferSizeRemaining = 0;	// So next transaction starts afresh.
					accumErr = kIOReturnSuccess;
				}
				else
				{	
					USBError(1, "The UHCI driver has detected an error [completion.action == NULL]");
				}
			}
		}
		pHCDoneTD->logicalBuffer = NULL;
		USBLog(7, "AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing - deallocating TD (%p)", this, pHCDoneTD);
		DeallocateTD(pHCDoneTD);
        pHCDoneTD = nextTD;	// New qHead
    }
	
    USBLog(7, "-AppleUSBUHCI[%p]::UHCIUIMDoDoneQueueProcessing", this);
    return(kIOReturnSuccess);
}


// ========================================================================
#pragma mark Memory management
// ========================================================================

AppleUHCITransferDescriptor* 
AppleUSBUHCI::AllocateTD(AppleUHCIQueueHead *pQH)
{
    AppleUHCITransferDescriptor		*freeTD;
	
    // Pop a ED off the FreeED list
    // If FreeED == NULL return Error
    freeTD = _pFreeTD;
	
    if (freeTD == NULL)
    {
		// i need to allocate another page of EDs
		AppleUHCItdMemoryBlock 		*memBlock;
		UInt32						numTDs, i;
		
		memBlock = AppleUHCItdMemoryBlock::NewMemoryBlock();
		if (!memBlock)
		{
			USBError(1, "AppleUSBUHCI[%p]::AllocateTD - unable to allocate a new memory block!",  this);
			return NULL;
		}
		// link it in to my list of ED memory blocks
		memBlock->SetNextBlock(_tdMBHead);
		_tdMBHead = memBlock;
		numTDs = memBlock->NumTDs();
		_pLastFreeTD = AppleUHCITransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(0), memBlock->GetPhysicalPtr(0));
        _pFreeTD = _pLastFreeTD;
		for (i=1; i < numTDs; i++)
		{
			freeTD = AppleUHCITransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(i), memBlock->GetPhysicalPtr(i));
			if (!freeTD)
			{
				USBError(1, "AppleUSBUHCI[%p]::AllocateTD - hmm. ran out of TDs in a memory block",  this);
				freeTD = _pFreeTD;
				break;
			}
			freeTD->_logicalNext = _pFreeTD;
			_pFreeTD = freeTD;
			// in a normal loop termination, freeED and _pFreeED are the same, just like when we don't use this code
		}
    }
    if (freeTD)
    {
		_pFreeTD = OSDynamicCast(AppleUHCITransferDescriptor, freeTD->_logicalNext);
		// if we use the last one, then we need to zero out the end pointer as well
		if (!_pFreeTD)
			_pLastFreeTD = NULL;
		freeTD->_logicalNext = NULL;
		freeTD->buffer = NULL;										// no alignment buffer yet
		freeTD->lastFrame = 0;
		freeTD->lastRemaining = 0;
		freeTD->command = NULL;
		// zero out the shared data
		freeTD->GetSharedLogical()->ctrlStatus = 0;
		freeTD->SetPhysicalLink(0);
		freeTD->GetSharedLogical()->token = 0;
		freeTD->GetSharedLogical()->buffer = 0;
		freeTD->pQH = pQH;
    }
    return freeTD;
}



IOReturn 
AppleUSBUHCI::DeallocateTD(AppleUHCITransferDescriptor *pTD)
{
    UInt32						physical;
	AppleUHCIQueueHead			*pQH = OSDynamicCast(AppleUHCIQueueHead, pTD->pQH);
	
	pTD->GetSharedLogical()->ctrlStatus = 0;
    pTD->_logicalNext = NULL;
	
	if (pTD->buffer)
	{
		USBLog(3, "AppleUSBUHCI[%p]::DeallocateTD - have alignment buffer %p and pQH %p", this, pTD->buffer, pQH);
		if (pQH)
			pQH->ReleaseAlignmentBuffer(pTD->buffer);
		pTD->buffer = NULL;
	}
	
    if (_pLastFreeTD)
    {
        _pLastFreeTD->_logicalNext = pTD;
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



AppleUHCIIsochTransferDescriptor* 
AppleUSBUHCI::AllocateITD(void)
{
    AppleUHCIIsochTransferDescriptor		*freeITD;
	
    // Pop a ED off the FreeED list
    // If FreeED == NULL return Error
    freeITD = _pFreeITD;
	
    if (freeITD == NULL)
    {
		// i need to allocate another page of EDs
		AppleUHCItdMemoryBlock 		*memBlock;
		UInt32						numTDs, i;
		
		memBlock = AppleUHCItdMemoryBlock::NewMemoryBlock();
		if (!memBlock)
		{
			USBError(1, "AppleUSBUHCI[%p]::AllocateITD - unable to allocate a new memory block!",  this);
			return NULL;
		}
		// link it in to my list of ED memory blocks
		memBlock->SetNextBlock(_tdMBHead);
		_tdMBHead = memBlock;
		numTDs = memBlock->NumTDs();
		_pLastFreeITD = AppleUHCIIsochTransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(0), memBlock->GetPhysicalPtr(0));
        _pFreeITD = _pLastFreeITD;
		for (i=1; i < numTDs; i++)
		{
			freeITD = AppleUHCIIsochTransferDescriptor::WithSharedMemory(memBlock->GetLogicalPtr(i), memBlock->GetPhysicalPtr(i));
			if (!freeITD)
			{
				USBError(1, "AppleUSBUHCI[%p]::AllocateITD - hmm. ran out of TDs in a memory block",  this);
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
		_pFreeITD = OSDynamicCast(AppleUHCIIsochTransferDescriptor, freeITD->_logicalNext);
		// if we use the last one, then we need to zero out the end pointer as well
		if (!_pFreeITD)
			_pLastFreeITD = NULL;
		freeITD->_logicalNext = NULL;
		freeITD->buffer = NULL;											// no alignment buffer
		// zero out the shared data
		freeITD->GetSharedLogical()->ctrlStatus = 0;
		freeITD->SetPhysicalLink(0);
		freeITD->GetSharedLogical()->token = 0;
		freeITD->GetSharedLogical()->buffer = 0;
    }
    return freeITD;
}



IOReturn 
AppleUSBUHCI::DeallocateITD(AppleUHCIIsochTransferDescriptor *pITD)
{
    UInt32					physical;
	AppleUHCIIsochEndpoint	*pEP = OSDynamicCast(AppleUHCIIsochEndpoint, pITD->_pEndpoint);
	
	pITD->GetSharedLogical()->ctrlStatus = 0;
    pITD->_logicalNext = NULL;
	
	if (pITD->buffer)
	{
		USBLog(6, "AppleUSBUHCI[%p]::DeallocateITD - have alignment buffer %p and pEP %p", this, pITD->buffer, pEP);
		if (pEP)
			pEP->ReleaseAlignmentBuffer(pITD->buffer);
		pITD->buffer = NULL;
	}
	
    if (_pLastFreeITD)
    {
        _pLastFreeITD->_logicalNext = pITD;
        _pLastFreeITD = pITD;
    } 
    else 
    {
        // list is currently empty
        _pLastFreeITD = pITD;
        _pFreeITD = pITD;
    }
	
	return kIOReturnSuccess;
}



AppleUHCIQueueHead * 
AppleUSBUHCI::AllocateQH(UInt16 functionNumber, UInt16 endpointNumber, UInt8 direction, UInt16 speed, UInt16 maxPacketSize, UInt8 type)
{
    AppleUHCIQueueHead				*freeQH;
	
    // Pop a ED off the freeQH list
    // If freeQH == NULL return Error
    freeQH = _pFreeQH;
	
    if (freeQH == NULL)
    {
		// i need to allocate another page of EDs
		AppleUHCIqhMemoryBlock 		*memBlock;
		UInt32						numQHs, i;
		
		memBlock = AppleUHCIqhMemoryBlock::NewMemoryBlock();
		if (!memBlock)
		{
			USBLog(1, "AppleUSBUHCI[%p]::AllocateQH - unable to allocate a new memory block!",  this);
			return NULL;
		}
		// link it in to my list of ED memory blocks
		memBlock->SetNextBlock(_qhMBHead);
		_qhMBHead = memBlock;
		numQHs = memBlock->NumQHs();
		_pLastFreeQH = AppleUHCIQueueHead::WithSharedMemory(memBlock->GetLogicalPtr(0), memBlock->GetPhysicalPtr(0));
		_pFreeQH = _pLastFreeQH;
		for (i=1; i < numQHs; i++)
		{
			freeQH = AppleUHCIQueueHead::WithSharedMemory(memBlock->GetLogicalPtr(i), memBlock->GetPhysicalPtr(i));
			if (!freeQH)
			{
				USBLog(1, "AppleUSBUHCI[%p]::AllocateED - hmm. ran out of EDs in a memory block",  this);
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
		_pFreeQH = OSDynamicCast(AppleUHCIQueueHead, freeQH->_logicalNext);
		// if we use the last one, then we need to zero out the end pointer as well
		if (!_pFreeQH)
			_pLastFreeQH = NULL;
		freeQH->_logicalNext = NULL;
		freeQH->buffersInUse = 0;
		freeQH->functionNumber = functionNumber;
		freeQH->endpointNumber = endpointNumber;
		freeQH->direction = direction;
		freeQH->speed = speed;
		freeQH->maxPacketSize = maxPacketSize;
		freeQH->type = type;
        freeQH->stalled = false;
		queue_init(&freeQH->allocatedBuffers);
		queue_init(&freeQH->freeBuffers);
	}
    return freeQH;
}





void 
AppleUSBUHCI::DeallocateQH(AppleUHCIQueueHead *pQH)
{
    UInt32		physical;
	
    //zero out all unnecessary fields
    pQH->_logicalNext = NULL;
	
    if (_pFreeQH)
	{
        _pLastFreeQH->_logicalNext = pQH;
        _pLastFreeQH = pQH;
    } else
	{
        // list is currently empty
        _pLastFreeQH = pQH;
        _pFreeQH = pQH;
    }
}



IOUSBControllerIsochEndpoint*			
AppleUSBUHCI::AllocateIsochEP()
{
	AppleUHCIIsochEndpoint		*pEP;
	
	pEP = new AppleUHCIIsochEndpoint;
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



// ========================================================================
#pragma mark Debugging
// ========================================================================


#if SINGLE_STEP


// Single step for debugging
void
AppleUSBUHCI::SingleStep(int count, bool runAfter)
{
	UInt16		cmd;
	UInt16		frame;
	UInt16		status;
	int			i;
	QH *		qh;
	
	Run(false);
	
	cmd = ioRead16(kUHCI_CMD) | kUHCI_CMD_SWDBG;
	ioWrite16(kUHCI_CMD, cmd);
	
	ioWrite16(kUHCI_FRNUM, 0);
	IODelay(10);
	
	while (count--) 
	{
		frame = ioRead16(kUHCI_FRNUM);
		USBLog(3, "AppleUSBUHCI[%p]::SingleStep - single stepping frame %d", this, frame);
		
		i = frame % kUHCI_NVFRAMES;
		qh = _logicalFrameList[i].first_qh;
		DumpQHChain(qh);
		
		cmd = cmd | kUHCI_CMD_RS;
		ioWrite16(kUHCI_CMD, cmd);
		
		do 
		{
			IODelay(10);
			cmd = ioRead16(kUHCI_CMD);
		} while (cmd & kUHCI_CMD_RS);
		
		status = ioRead16(kUHCI_STS);
		
		USBLog(3, "AppleUSBUHCI[%p]::SingleStep - status %x", this, status);
	}
	
	if (runAfter) 
	{
		Run(true);
	}
}

#endif /* SINGLE_STEP */


void
AppleUSBUHCI::PrintFrameList(UInt32 slot, int level)
{
	IOUSBControllerListElement		*pLE;
    AppleUHCIQueueHead				*pQH;
	AppleUHCITransferDescriptor		*pTD;
	int								i;
	
	USBLog(level, "AppleUSBUHCI[%p]::PrintFrameList - raw list", this);
	for (i=0; i< 1024; i++)
	{
		USBLog(level, "*********_frameList[%d]=%p", i, (void*)USBToHostLong(_frameList[i]));
		IOSleep(1);
	}
	USBLog(level, "AppleUSBUHCI[%p]::PrintFrameList(%d) - _frameList@%p[%p] _logicalFrameList[%p]", this, (int)slot, &_frameList[slot], (void*)USBToHostLong(_frameList[slot]), _logicalFrameList[slot]);
	pLE = _logicalFrameList[slot];
	while (pLE)
	{
		pLE->print(level);
		pQH = OSDynamicCast(AppleUHCIQueueHead, pLE);
		if (pQH)
		{
			for (pTD=pQH->firstTD; pTD && (pTD != pQH->lastTD); pTD=OSDynamicCast(AppleUHCITransferDescriptor, pTD->_logicalNext))
			{
				pTD->print(level);
			}
		}
		pLE = pLE->_logicalNext;
	}
}
