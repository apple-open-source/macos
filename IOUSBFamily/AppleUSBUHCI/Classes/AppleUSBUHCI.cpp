/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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


#define super IOUSBController


/*
 * TODO:
 *  - generalize root hub code to use root hub endpoint, rather than having
 *    separate calls and checks in each function (e.g. RHAbort...)
 *  - support for synthetic suspend change status bit in roo thub
 */

OSDefineMetaClassAndStructors(AppleUSBUHCI, IOUSBControllerV2)


class UHCIMemoryBuffer : public IOBufferMemoryDescriptor
{
protected:
    virtual void free();
public:
    queue_chain_t _chain;
    static UHCIMemoryBuffer *newBuffer(bool dmaable = true);
};

UHCIMemoryBuffer *
UHCIMemoryBuffer::newBuffer(bool dmaable)
{
    UHCIMemoryBuffer *bp;
    
    bp = new UHCIMemoryBuffer;
    if (bp == NULL) {
        return NULL;
    }
    
    if (!bp->initWithOptions(kIOMemoryUnshared | kIODirectionInOut, PAGE_SIZE, PAGE_SIZE)) {
        bp->release();
        return NULL;
    }
    if (dmaable) {
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
    if (!super::init(propTable))  return false;

    //USBLog(3, "Debug level before: %d", (int)KernelDebugGetLevel());
    //KernelDebugSetLevel(5);
    //USBLog(3, "%s[%p]::init", getName(), this);

    USBLog(3, "AppleUSBUHCI::init: %s", _deviceName);
    
    _uimInitialized = false;

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
        USBLog(3, "%s[%p]::start waiting for EHCI", getName(), this);
		setProperty("Companion", "yes");
        service = waitForService( serviceMatching("AppleUSBEHCI"), &t );
        USBLog(3, "%s[%p]::start got EHCI service %p", getName(), this, service);
    }
    
    USBLog(3, "%s[%p]::start", getName(), this);
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
    USBLog(3, "%s[%p]::stop", getName(), this);
    super::stop(provider);
}

bool
AppleUSBUHCI::finalize(IOOptionBits options)
{
    USBLog(3, "%s[%p]::finalize", getName(), this);
    return super::finalize(options);
}


void
AppleUSBUHCI::EnableUSBInterrupt(bool enableInterrupt)
{
    UInt16 value;
    USBLog(3, "%s[%p]::EnableUSBInterrupt - Legacy register: 0x%x", getName(), this, _device->configRead16(kUHCI_PCI_LEGKEY));
    
	// The master interrupt for the UHCI controller is actually in the Legacy Support register (section 5.2.1)
    if (enableInterrupt) 
	{
        value = kUHCI_LEGKEY_INTR_ENABLE;
    } else {
        value = 0;
    }
    _device->configWrite16(kUHCI_PCI_LEGKEY, value);
}


IOReturn
AppleUSBUHCI::HardwareInit(void)
{
    IOReturn status;
    UInt32 * frames;
    IOPhysicalAddress paddr;
    int i, j, frame_period;
    struct AppleUHCITD *td;
    struct AppleUHCIQH *last_qh, *bulk_qh,
        *hs_qh, *ls_qh, *qh;
    UHCIMemoryBuffer *bp;
    
    ioWrite16(kUHCI_INTR, 0);   /* Disable interrupts. */
    
    GlobalReset();
    status = Reset();
    if (status != kIOReturnSuccess) {
        return status;
    }
    
    /* Set up frame array. */
    bp = UHCIMemoryBuffer::newBuffer();
    if (bp == NULL) {
        return kIOReturnNoMemory;
    }
    queue_enter(&_allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
    
    frames = (UInt32 *)bp->getBytesNoCopy();
    paddr = bp->getPhysicalAddress();
    _framesPaddr = paddr;
    
    /* Set frame number and physical frame address. */
    ioWrite16(kUHCI_FRNUM, 0);
    ioWrite32(kUHCI_FRBASEADDR, paddr);
    USBLog(3, "Setting physical frame address to %x", paddr);
    
    /* Set up queue heads. */
    
    /* Dummy TD at the end of all queues. */
    td = AllocTD();
    if (td == NULL)
        return kIOReturnNoMemory;
    td->link = NULL;
    td->hw.link = HostToUSBLong(kUHCI_TD_T);
    td->hw.ctrlStatus = HostToUSBLong(UHCI_TD_SET_ERRCNT(0) |
                                      UHCI_TD_SET_ACTLEN(0));
    td->hw.token = HostToUSBLong(kUHCI_TD_PID_OUT |
                                 UHCI_TD_SET_MAXLEN(0) |
                                 UHCI_TD_SET_ENDPT(0) |
                                 UHCI_TD_SET_ADDR(0));
    td->hw.buffer = 0;
    
    /* Dummy QH at the end of all queues. Put dummy TD in it. */
    last_qh = AllocQH();
    if (last_qh == NULL)
        return kIOReturnNoMemory;
    last_qh->hlink = NULL;
    last_qh->hw.hlink = HostToUSBLong(kUHCI_QH_T);
    last_qh->elink = td;
    last_qh->hw.elink = HostToUSBLong(td->paddr);
    _lastQH = last_qh;
    
    /* Bulk traffic queue. */
    bulk_qh = AllocQH();
    if (bulk_qh == NULL)
        return kIOReturnNoMemory;
    bulk_qh->hlink = _lastQH;
    bulk_qh->hw.hlink = HostToUSBLong(_lastQH->paddr | kUHCI_QH_Q);
    bulk_qh->elink = NULL;
    bulk_qh->hw.elink = HostToUSBLong(kUHCI_QH_T);
    _bulkQHStart = _bulkQHEnd = bulk_qh;
    
    /* High speed control queue. */
    hs_qh = AllocQH();
    if (hs_qh == NULL)
        return kIOReturnNoMemory;
    hs_qh->hlink = bulk_qh;
    hs_qh->hw.hlink = HostToUSBLong(bulk_qh->paddr | kUHCI_QH_Q);
    hs_qh->elink = NULL;
    hs_qh->hw.elink = HostToUSBLong(kUHCI_QH_T);
    _hsControlQHStart = _hsControlQHEnd = hs_qh;
        
    /* Low speed control queue. */
    ls_qh = AllocQH();
    if (ls_qh == NULL)
        return kIOReturnNoMemory;
    ls_qh->hlink = hs_qh;
    ls_qh->hw.hlink = HostToUSBLong(hs_qh->paddr | kUHCI_QH_Q);
    ls_qh->elink = NULL;
    ls_qh->hw.elink = HostToUSBLong(kUHCI_QH_T);
    _lsControlQHStart = _lsControlQHEnd = ls_qh;
    
    /* Interrupt QH tree.
     * For 2^n virtual frames, there are n+1 interrupt QHs,
     * representing each 1/m frame polling rate.
     * The rate of _intrQH[i] is 1/2^i.
     */
    assert( (1 << (kUHCI_NINTR_QHS - 1)) == kUHCI_NVFRAMES );
    last_qh = NULL;
    for (i=(kUHCI_NINTR_QHS - 1); i>=0; i--) {
        qh = AllocQH();
        if (qh == NULL) {
            return kIOReturnNoMemory;
        }
        _intrQH[i] = qh;
        qh->elink = NULL;
        qh->hw.elink = HostToUSBLong(kUHCI_QH_T);
        if (last_qh != NULL) {
            last_qh->hlink = qh;
            last_qh->hw.hlink = HostToUSBLong(qh->paddr | kUHCI_QH_Q);
        }
        last_qh = qh;
    }
    qh->hlink = ls_qh;
    qh->hw.hlink = HostToUSBLong(ls_qh->paddr | kUHCI_QH_Q);
    
    /* For "bandwidth reclamation", point the hardware link
     * for the last QH back to the high speed queue head.
     * Don't link the software pointer.
     */
    _lastQH->hw.hlink = HostToUSBLong(hs_qh->paddr | kUHCI_QH_Q);
    
    /* Set up virtual frames. */
    for (i=0; i<kUHCI_NVFRAMES; i++) {
        /* Each virtual frame points to a TD, which points
         * to the interrupt QH tree.
         */
        td = AllocTD();
        qh = AllocQH();
        if (td == NULL || qh == NULL)
            return kIOReturnNoMemory;
        td->link = (TD*)qh;
        td->hw.link = HostToUSBLong(qh->paddr | kUHCI_QH_Q);
        td->hw.ctrlStatus = HostToUSBLong(kUHCI_TD_ISO |
                                          UHCI_TD_SET_ERRCNT(1) |
                                          UHCI_TD_SET_ACTLEN(0)); /* Isoc, not active. */
        td->hw.token = HostToUSBLong(kUHCI_TD_PID_OUT |
            UHCI_TD_SET_MAXLEN(0) |
            UHCI_TD_SET_ENDPT(0) |
            UHCI_TD_SET_ADDR(0));
        
        /* Point to correct interrupt QH. */
        for (j=kUHCI_NINTR_QHS-1; j>=0; j--) {
            frame_period = 1 << j;
            if (i % frame_period == 0) {
                break;
            }
        }
        if (j<0) {
            /* Should never happen. */
            j = 0;
        }
        qh->hlink = _intrQH[j];
        qh->hw.hlink = HostToUSBLong(_intrQH[j]->paddr | kUHCI_QH_Q);
        qh->elink = NULL;
        qh->hw.elink = HostToUSBLong(kUHCI_QH_T);
        
        _vframes[i].first_qh = qh;
        _vframes[i].last_qh = qh;
        _vframes[i].td = td;
        
        /* Point physical frames to virtual frames. */
        for (j=i; j<kUHCI_FRAME_COUNT; j+=kUHCI_NVFRAMES) {
            frames[j] = HostToUSBLong(td->paddr);
        }
    }
    IOSync();
    DumpFrame(0);
    
    /* Use 64-byte packets, and mark controller as configured. */
    Command(kUHCI_CMD_MAXP | kUHCI_CMD_CF);
    USBLog(3, "%s[%p]: Command register reports %x", getName(), this, ioRead16(kUHCI_CMD));
    
    /* Enable interrupts. */
    ioWrite16(kUHCI_INTR, kUHCI_INTR_TIE | kUHCI_INTR_RIE | kUHCI_INTR_IOCE | kUHCI_INTR_SPIE);
    USBLog(3, "%s[%p]: Interrupt register reports %x", getName(), this, ioRead16(kUHCI_INTR));

    /* Start the controller. */
    return Run(true);
}

IOReturn
AppleUSBUHCI::UIMInitialize(IOService * provider)
{
    IOReturn status;
    UInt32   value;
    
    USBLog(3, "%s[%p]::UIMInitialize", getName(), this);
    
    if (!_uimInitialized) {
        _device = OSDynamicCast(IOPCIDevice, provider);
        if(_device == NULL) {
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
            
        USBLog(3, "map = %08x", (unsigned int)_ioMap);
        if (_ioMap) {
            USBLog(3, "%s[%p] Map vaddr %x, paddr %x",
                   getName(), this,
                   (UInt32)_ioMap->getVirtualAddress(),
                   (UInt32)_ioMap->getPhysicalAddress());
        } else {
            USBError(1, "%s[%p]: ioMap is NULL", getName(), this);
            return kIOReturnNoMemory;
        }
        _ioPhysAddress = _ioMap->getPhysicalAddress();
        _ioVirtAddress = _ioMap->getVirtualAddress();
        
        _frameLock = IOLockAlloc();
        if (_frameLock == NULL) {
            return kIOReturnNoMemory;
        }
        
        queue_init(&_allocatedBuffers);
        queue_init(&_freeTransactions);
        queue_init(&_rhIntrTransactions);
        queue_init(&_activeTransactions);
        queue_init(&_endpoints);

        _isocBandwidth = kUSBMaxFSIsocEndpointReqCount;
        _powerLevel = kUHCIPowerLevelRunning;

        clock_get_uptime(&_lastTime);
        
        _rhEndpoint = AllocEndpoint(0, 0, kUSBAnyDirn, 0, 0, kUSBInterrupt);
        _rhEndpoint->type = kUSBInterrupt;

        queue_remove(&_endpoints, _rhEndpoint, UHCIEndpoint *, chain);
        
        SetVendorInfo();
        SetDeviceName();
        
        /* Do not use standardized errata bits yet. */
        _errataBits = GetErrataBits(_vendorID, _deviceID, _revisionID);
        
       USBLog(3, "%s[%p]: there are %d interrupt sources", getName(), this, _numInterruptSources);
        
        //_interruptSource = IOInterruptEventSource::interruptEventSource(this, &InterruptHandler, _device);
        
        _interruptSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
                                                                                    &InterruptHandler,	
                                                                                    &PrimaryInterruptFilter,
                                                                                    _device);
        
        if (!_interruptSource || (_workLoop->addEventSource(_interruptSource) != kIOReturnSuccess)) {
            return kIOReturnBadArgument;
        }
        
        USBLog(3, "%s[%p]: Getting config registers:", getName(), this);
        USBLog(3, " CLASSC: %08x", (unsigned int)_device->configRead32(0x08));
        USBLog(3, " USBBASE: %08x", (unsigned int)_device->configRead32(0x20));
        USBLog(3, " SBRN: %02x", _device->configRead8(0x60));
        
        // enable the card
        value = _device->configRead32(kIOPCIConfigCommand) & 0xFFFF0000;
        value |= (kIOPCICommandBusMaster | kIOPCICommandMemorySpace | kIOPCICommandIOSpace);
        _device->configWrite32(kIOPCIConfigCommand, value);
        
        USBLog(3, "%s[%p]: calling HardwareInit:", getName(), this);
        
        status = HardwareInit();
        
        USBLog(3, "%s[%p]: status after init: %08x", getName(), this, status);
        
        /* Set up a periodic timer to check the root hub status. */
        _rhTimer = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action) RHTimerFired);
        
        if ( _rhTimer == NULL )
        {
            USBError(1, "%s[%p]::UIMInitialize couldn't allocate timer event source", getName(), this);
            return kIOReturnNoMemory;
        }
        
        if ( _workLoop->addEventSource( _rhTimer ) != kIOReturnSuccess )
        {
            USBError(1, "%s[%p]::UIMInitialize couldn't add timer event source", getName(), this);
            return kIOReturnError;
        }
                
        // Enable interrupts
        EnableUSBInterrupt(true);
        
        /* Note that the timer isn't scheduled to send events yet. */
        
        // enable interrupt delivery
        _workLoop->enableAllInterrupts();
                
        _uimInitialized = true;
    }
    
    USBLog(3, "%s[%p]::UIMInitialize done", getName(), this);
    
    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::UIMFinalize()
{
    UHCIEndpoint *ep;
    
    USBLog(3, "%s[%p]::UIMFinalize", getName(), this);
    
    // Turn off ports.
    RHEnablePort(1, false);
    RHEnablePort(2, false);
        
    // Stop and suspend controller.
    SuspendController();
    
    _workLoop->disableAllInterrupts();
    
    if (!isInactive()) {
        // Disable controller in PCI space.
        // XXX
        
        // Release I/O resources.
        if (_ioMap) {
            _ioMap->release();
            _ioMap = NULL;
        }
    }

    // Clean up our power down notifier.  That will release it.
    //
    if ( _powerDownNotifier ) {
        _powerDownNotifier->remove();
        _powerDownNotifier = NULL;
    }
        
    USBLog(3, "%s[%p]::UIMFinalize freeing memory", getName(), this);
    
    // Free special root hub endpoint.
    FreeEndpoint(_rhEndpoint);
    _rhEndpoint = NULL;
    
    // Free allocated TD, QH, transaction and frame memory.

    while (!queue_empty(&_allocatedBuffers)) {
        UHCIMemoryBuffer *bp;
        
        queue_remove_first(&_allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
        bp->release();
    }
    
    _freeTDs = NULL;
    _freeQHs = NULL;
    
    while (!queue_empty(&_endpoints)) {
        queue_remove_first(&_endpoints, ep, UHCIEndpoint *, chain);
        
        IOFree(ep, sizeof(UHCIEndpoint));
    }

    if (_rhTimer)
    {
        if ( _workLoop )
            _workLoop->removeEventSource(_rhTimer);
        
        _rhTimer->release();
        _rhTimer = NULL;
    }

    USBLog(3, "removing interrupt source");

    if (_interruptSource) {
        _workLoop->removeEventSource(_interruptSource);
        _interruptSource->release();
        _interruptSource = NULL;
    }

    IOLockFree(_frameLock);
    _frameLock = NULL;
    
    if (_deviceNameLen) {
        IOFree((void *)_deviceName, _deviceNameLen);
        _deviceName = NULL;
        _deviceNameLen = 0;
    }
    
    _uimInitialized = false;
    
    USBLog(3, "%s[%p]::UIMFinalize done", getName(), this);
    
    return kIOReturnSuccess;
}

/*
 * Initialize the controller hardware after powering up (e.g. from sleep).
 * Does not start the controller.
 */

IOReturn 
AppleUSBUHCI::UIMInitializeForPowerUp()
{
    UInt32 value;
    
    USBLog(3, "%s[%p]::UIMInitializeForPowerUp", getName(), this);
    USBLog(3, "%s[%p]: before: frame address register is %x", getName(), this, ioRead32(kUHCI_FRBASEADDR));
    USBLog(3, "%s[%p]: frame number register is %x", getName(), this, ReadFrameNumberRegister());
    ioWrite32(kUHCI_FRBASEADDR, _saveFrameAddress);
    ioWrite16(kUHCI_FRNUM, _saveFrameNumber);
    USBLog(3, "%s[%p]: after: frame address register is %x", getName(), this, ioRead32(kUHCI_FRBASEADDR));
    USBLog(3, "%s[%p]: frame number register is %x", getName(), this,  ReadFrameNumberRegister());
    
    Command(kUHCI_CMD_MAXP | kUHCI_CMD_CF);
    USBLog(3, "%s[%p]: Command register reports %x", getName(), this, ioRead16(kUHCI_CMD));
    
    /* Enable bus mastering. */
    value = _device->configRead32(kIOPCIConfigCommand) & 0xFFFF0000;
    value |= (kIOPCICommandBusMaster | kIOPCICommandMemorySpace | kIOPCICommandIOSpace);
    _device->configWrite32(kIOPCIConfigCommand, value);
    
    /* Enable interrupts. */
    ioWrite16(kUHCI_INTR, kUHCI_INTR_TIE | kUHCI_INTR_RIE | kUHCI_INTR_IOCE | kUHCI_INTR_SPIE);
    
    return kIOReturnSuccess;
}

/*
 * Finalize controller hardware for powering down.
 * Assumes that the controller is stopped.
 */

IOReturn
AppleUSBUHCI::UIMFinalizeForPowerDown()
{
    UInt32 value;
    
    USBLog(3, "%s[%p]::UIMFinalizeForPowerDown", getName(), this);
    
    _saveFrameAddress = ioRead32(kUHCI_FRBASEADDR);
    USBLog(3, "%s[%p]: frame address register is %x", getName(), this,
           _saveFrameAddress);
    _saveFrameNumber = ioRead16(kUHCI_FRNUM);
    USBLog(3, "%s[%p]: frame number register is %x", getName(), this,
           _saveFrameNumber);
    
    /* Disable bus mastering. */
    value = _device->configRead32(kIOPCIConfigCommand) & 0xFFFF0000;
    value |= (kIOPCICommandMemorySpace | kIOPCICommandIOSpace);
    _device->configWrite32(kIOPCIConfigCommand, value);
    
    /* Disable interrupts. */
    ioWrite16(kUHCI_INTR, 0);

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
    USBLog(2, "%s[%p]: vendorID = 0x%x", getName(), this, _vendorID);
    
    deviceProp   = OSDynamicCast(OSData, _device->getProperty( "device-id" ));
    if (deviceProp)
        _deviceID   = *((UInt32 *) deviceProp->getBytesNoCopy());
    revisionProp = OSDynamicCast(OSData, _device->getProperty( "revision-id" ));
    if (revisionProp)
        _revisionID = *((UInt32 *) revisionProp->getBytesNoCopy());
    
    if (_vendorID == 0x1106) {
        /* VIA controllers.
         * After a BABBLE error, the controller seems to lock up.
         */
        _errataBits = kUHCIResetAfterBabble;
    }
}

UInt32
AppleUSBUHCI::GetBandwidthAvailable()
{
    USBLog(3, "%s[%p]::GetBandwidthAvailable returns %d", getName(), this, _isocBandwidth);
    return _isocBandwidth;
}


// ========================================================================
#pragma mark Hardware control
// ========================================================================


void
AppleUSBUHCI::GlobalReset(void)
{
    USBLog(4, "%s[%p]: global reset", getName(), this);
    Command(kUHCI_CMD_GRESET);
    IOSleep(kUHCI_RESET_DELAY);
    Command(0);
    USBLog(4, "%s[%p]: global reset done", getName(), this);
}

IOReturn
AppleUSBUHCI::Reset(bool enableInterrupts)
{
    int i;
    
    USBLog(3, "%s[%p]: resetting", getName(), this);
    Command(kUHCI_CMD_HCRESET);
    for(i=0; (i < kUHCI_RESET_DELAY) && (ioRead16(kUHCI_CMD) & kUHCI_CMD_HCRESET); i++) {
        IOSleep(1);
    }
    if (i >= kUHCI_RESET_DELAY) {
        USBError(1, "%s: controller reset failed", getName());
        return kIOReturnTimeout;
    }
    USBLog(3, "%s[%p]: reset done after %d spins", getName(), this, i);
    
    if (_framesPaddr != NULL) {
        ioWrite32(kUHCI_FRBASEADDR, _framesPaddr);
        USBLog(3, "%s[%p]: Command register reports %x", getName(), this, ioRead16(kUHCI_CMD));
        
        ioWrite16(kUHCI_FRNUM, (UInt16)(_lastFrameNumberLow & kUHCI_FRNUM_MASK));

        /* Use 64-byte packets, and mark controller as configured. */
        Command(kUHCI_CMD_MAXP | kUHCI_CMD_CF);
        USBLog(3, "%s[%p]: Interrupt register before reports %x", getName(), this, ioRead16(kUHCI_INTR));
        if (enableInterrupts) {
            /* Enable interrupts. */
            ioWrite16(kUHCI_INTR, kUHCI_INTR_TIE | kUHCI_INTR_RIE | kUHCI_INTR_IOCE | kUHCI_INTR_SPIE);
            USBLog(3, "%s[%p]: Interrupt register after reports %x", getName(), this, ioRead16(kUHCI_INTR));        
        }
    }
    
    
    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::Run(bool run)
{
    bool state;
    UInt16 cmd;
    int i;
    IOReturn status = kIOReturnTimeout;
    
    USBLog(3, "%s[%p]: setting run = %d", getName(), this, run);
    
    //_workLoop->disableAllInterrupts();
    cmd = ioRead16(kUHCI_CMD);
    if (run) {
        cmd = cmd | kUHCI_CMD_RS;
    } else {
        cmd = cmd & ~kUHCI_CMD_RS;
    }
    USBLog(3, "%s[%p]: About to write command 0x%x", getName(), this, cmd);
    Command(cmd);
    USBLog(3, "%s[%p]: Waiting for controller to come ready", getName(), this);
    for (i=0; i<20; i++) {
        state = ((ioRead16(kUHCI_STS) & kUHCI_STS_HCH) == 0);
        if (run == state) {
            status = kIOReturnSuccess;
            break;
        }
        IOSleep(1);
    }
    USBLog(3, "%s[%p]: Finished waiting with result %d", getName(), this, status);
    //if (run) {
    //    _workLoop->enableAllInterrupts();
    //}
    
    USBLog(3, "%s[%p]: run resulted in status %d, command port %x", getName(), this, status, ioRead16(kUHCI_CMD));
    return status;
}

/* For now, the frame number is really only 32 bits. */

UInt64
AppleUSBUHCI::GetFrameNumber()
{
    UInt32 lastFrameNumber;
    
    UInt32 lastFrame;
    UInt32 thisFrame;
    UInt32 overflow;

    UInt32 newFrame;
    
    if (_lastFrameNumberLow >= (UInt32)(~kUHCI_FRNUM_MASK)) {
        USBLog(7, "UHCI[%p]: locking to check frame number", this);
        IOLockLock(_frameLock);
        lastFrameNumber = _lastFrameNumberLow;

        overflow = lastFrameNumber & (~kUHCI_FRNUM_MASK);
        lastFrame = lastFrameNumber & kUHCI_FRNUM_MASK;

        thisFrame = ReadFrameNumberRegister();
        if (lastFrame <= thisFrame) {
            // No 11-bit overflow
            newFrame = overflow + thisFrame;
        } else {
            // 11-bit and 32-bit overflow
            _lastFrameNumberHigh++;
            newFrame = overflow + thisFrame + kUHCI_FRNUM_COUNT;
            USBLog(7, "UHCI[%p]: 64-bit frame number overflow (low %x)", this, newFrame);
        }
        _lastFrameNumberLow = newFrame;
        IOLockUnlock(_frameLock);
    } else do {
        lastFrameNumber = _lastFrameNumberLow;
        
        overflow = lastFrameNumber & (~kUHCI_FRNUM_MASK);
        lastFrame = lastFrameNumber & kUHCI_FRNUM_MASK;
        
        thisFrame = ReadFrameNumberRegister();
        if (lastFrame <= thisFrame) {
            // No 11-bit overflow
            newFrame = overflow + thisFrame;
        } else /* if (overflow < (~kUHCI_FRNUM_MASK)) */ {
            // 11-bit overflow, but no 32-bit overflow
            newFrame = overflow + thisFrame + kUHCI_FRNUM_COUNT;
            USBLog(7, "UHCI[%p]: 11-bit frame number overflow", this);
        }
        
    } while (!OSCompareAndSwap(lastFrameNumber, newFrame, &_lastFrameNumberLow));
    
    USBLog(7, "UHCI[%p]: frame number is %qx", this, (UInt64)newFrame | ((UInt64)_lastFrameNumberHigh << 32));
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

// ========================================================================
#pragma mark Interrupts
// ========================================================================

void 
AppleUSBUHCI::PollInterrupts(IOUSBCompletionAction safeAction)
{
    USBLog(3, "%s[%p]::PollInterrupts", getName(), this);
    /* Not used */
}

/*
 * Called at hardware interrupt time.
 */
bool 
AppleUSBUHCI::PrimaryInterruptFilter(OSObject *owner, IOFilterInterruptEventSource *source)
{
    register AppleUSBUHCI *myself = (AppleUSBUHCI *)owner;
    
    if (!myself || myself->isInactive()) {
        return false;
    }
    
    return myself->FilterInterrupt();
}


void 
AppleUSBUHCI::InterruptHandler(OSObject *owner,
                                    IOInterruptEventSource * /*source*/,
                                    int /*count*/)
{
    AppleUSBUHCI *controller = (AppleUSBUHCI *)owner;
    
    //USBLog(7, "AppleUSBUHCI::InterruptHandler");

    //
    // Interrupt source callouts are not blocked by _workLoop->sleep()
    // and driver checks _powerLevel to prevent touching hardware when
    // UHCI has powered down, and another device sharing the interrupt
    // line has it asserted.
    //
    // Driver must ensure that the device cannot generate an interrupt
    // while in suspend state to prevent an interrupt storm.
    //

    if (!controller->isInactive() &&
        (controller->_powerLevel != kUHCIPowerLevelSuspend)) {
        controller->HandleInterrupt();
    }
}
    
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

static struct UHCIVendorInfo {
    UInt16 vendor_id;
    const char *vendor_name;
    struct UHCIDeviceInfo *devices;
} UHCIVendorInfo[] = {
    {0x8086, "Intel", UHCI_Intel_devices},
    {0x1106, "VIA", UHCI_VIA_devices},
    {0, 0, 0}
};


void
AppleUSBUHCI::SetDeviceName(void)
{
    struct UHCIVendorInfo *vi;
    struct UHCIDeviceInfo *di, *di_found = NULL;

    USBLog(3, "::SetDeviceName");
    for (vi = &UHCIVendorInfo[0]; vi->vendor_name != NULL; vi++) {
        USBLog(3, "vendor: %s", vi->vendor_name);
        if (vi->vendor_id == _vendorID) {
            for (di = vi->devices; di->device_name != NULL; di++) {
                USBLog(3, "device: %s", di->device_name);
                if (di->device_id == _deviceID) {
                    di_found = di;
                    break;
                }
            }
        }
        if (di_found != NULL) {
            break;
        }
    }
    if (di_found == NULL) {
        _deviceNameLen = 0;
        _deviceName = "Generic UHCI USB Controller";
    } else {
        _deviceNameLen = strlen(vi->vendor_name) + strlen(di_found->device_name) + strlen("UHCI USB Controller") + 4;
        char *str = (char *)IOMalloc(_deviceNameLen);
        sprintf(str, "%s %s UHCI USB Controller", vi->vendor_name, di_found->device_name);
        _deviceName = str;
    }
    USBLog(3, "::SetDeviceName: %s", _deviceName);
}

/* Interrupt handling. */


/* Called at hardware interrupt time. */
bool
AppleUSBUHCI::FilterInterrupt(void)
{
    UInt16 hwIntrStatus;
    UHCITransaction *tp;
    TD *td;
    AbsoluteTime timeStamp;
    UInt32 status;
    UInt32 currentFrame;
    UInt32 frame;
    UHCIAlignmentBuffer *bp;
    
    hwIntrStatus = ioRead16(kUHCI_STS) & kUHCI_STS_INTR_MASK;
    
    if (hwIntrStatus == 0) {
        /* Not our interrupt. */
        return false;
    }
    
    /* Acknowledge interrupt. */
    ioWrite16(kUHCI_STS, hwIntrStatus);
    
    OSBitOrAtomic(hwIntrStatus, &_intrStatus);
    
    clock_get_uptime(&timeStamp);
    
    currentFrame = ReadFrameNumber() & kUHCI_NVFRAMES_MASK;
    frame = (currentFrame - 2);
    frame = frame & kUHCI_NVFRAMES_MASK;
    
    while (frame != currentFrame) {
        td = _vframes[frame].td;
    
    while (td && (td->hw.ctrlStatus & HostToUSBLong(kUHCI_TD_ISO))) {
        status = USBToHostLong(td->hw.ctrlStatus);

        if (td->fllp && (status & kUHCI_TD_ACTIVE) == 0) {
            td->fllp->frTimeStamp = timeStamp;
            td->fllp->frStatus = TDToUSBError(status);
            td->fllp->frActCount = UHCI_TD_GET_ACTLEN(status);
            td->fllp = NULL;
        }
        
        td = td->link;
    }
    
        frame++;
        if (frame == kUHCI_NVFRAMES) frame = 0;
    }
    
#if AUDIO_HACK
    // XXX
    // For the "audio hack" operation, the UIM code will construct a list of
    // work items for the filter interrupt routine.  Each work item will specify
    // a frame number, source and destination buffer to copy.
    // When the filter interrupt routine copies the data, it will mark the queue entry done.
    
    for (bp = _interruptAlignmentBuffers; bp != NULL; bp = bp->next) {
        if (bp->copyAtInterruptTime && bp->frameNumber == previousFrame) {
            if ((bp->userAddr != NULL) && (bp->vaddr != NULL)) {
                bcopy((void *)bp->userAddr, (void *)bp->vaddr, bp->userLength);
            bp->copyAtInterruptTime = 0;
        }
    }
    }
#endif
    
    // We will return false from this filter routine,
    // but will indicate that there the action routine should be called
    // by calling _filterInterruptSource->signalInterrupt(). 
    // This is needed because IOKit will disable interrupts for a level interrupt
    // after the filter interrupt is run, until the action interrupt is called.
    // We want to be able to have our filter interrupt routine called
    // before the action routine runs, if needed.  That is what will enable
    // low latency isoch transfers to work, as when the
    // system is under heavy load, the action routine can be delayed for tens of ms.
    //
    _interruptSource->signalInterrupt();
    
    return false;
}

/* Called at software interrupt time. */
void
AppleUSBUHCI::HandleInterrupt(void)
{
    UInt16 status;
    UInt32 intrStatus;
    bool needReset = false;
    UHCIAlignmentBuffer *bp;
    
    do {
        intrStatus = _intrStatus;
    } while (!OSCompareAndSwap(intrStatus, 0, &_intrStatus));

    status = ioRead16(kUHCI_STS);
    
    //USBLog(7, "%s[%p]: HandleInterrupt frame %d value 0x%x intr 0x%x", getName(), this, ReadFrameNumber(), status, intrStatus);
    //USBLog(7, "%s[%p]: Controller status %x", getName(), this, status);
    //USBLog(7, "%s[%p]: Frame %d", getName(), this, ReadFrameNumber());
    
    if (status & kUHCI_STS_HCH) {
        if (_powerLevel == kUHCIPowerLevelRunning) {
            USBLog(1, "%s[%p]: Host controller halted when it should be running", getName(), this);
            DumpFrame(ReadFrameNumber() % kUHCI_NVFRAMES, 5);
            needReset = true;
        }
        
#if DEBUG
        /* Disable interrupts for debugging */
        _workLoop->disableAllInterrupts();
        /* Dump all transactions */
        UHCITransaction *tp;
        USBLog(3, "%s[%p]: halting; dumping all transactions", getName(), this);
        queue_iterate(&_activeTransactions, tp, UHCITransaction *, active_chain) {
            DumpTransaction(tp, 3);
        }
#endif
    }
    if (intrStatus & kUHCI_STS_HCPE) {
        USBLog(1, "%s[%p]: Host controller process error", getName(), this);
        needReset = true;
    }
    if (intrStatus & kUHCI_STS_HSE) {
        USBLog(1, "%s[%p]: Host controller system error", getName(), this);
        needReset = true;
    }
    if (intrStatus & kUHCI_STS_RD) {
        USBLog(7, "%s[%p]: Host controller resume detected", getName(), this);
        if (_powerLevel == kUHCIPowerLevelIdleSuspend) {
            ResumeController();
        }
    }
    if (intrStatus & kUHCI_STS_EI) {
        USBLog(7, "%s[%p]: Host controller error interrupt", getName(), this);
    }
    
    if (needReset) {
        USBError(1, "%s[%p]: Resetting controller due to errors detected at interrupt time (0x%x)",
                 getName(), this, status);
        Reset(true);
        Run(true);
    }
    
#if AUDIO_HACK
    // XXX
    /* Clean up alignment buffer queue. */
    for (bp = _interruptAlignmentBuffers; bp != NULL; bp = bp->next) {
        if (bp->copyAtInterruptTime == 0) {
        }
    }
#endif
    
    /* Process completed transactions. */
    ProcessCompletedTransactions();
    
    /* Check for root hub status change. */
    RHCheckStatus();
}

bool
AppleUSBUHCI::HandleShortPacket(UHCITransaction *tp, TD *td)
{
    UInt32 status;
    TD *last_td;

    DumpQH(tp->qh, 6);
    if (tp->type == kUSBControl) {
        /* Check for the ACK phase of the transaction */
        last_td = tp->last_td;
        status = USBToHostLong(last_td->hw.ctrlStatus);
        
        if (status & kUHCI_TD_ACTIVE) {
            /* Continue the last TD, which should be the ack. */
            tp->qh->hw.elink = HostToUSBLong(last_td->paddr);
            IOSync();
            
            /* Mark previous TDs in the transaction as inactive. */
            for(; td != last_td; td = td->link) {
                td->hw.ctrlStatus &= HostToUSBLong(~kUHCI_TD_ACTIVE);
            }
            IOSync();
            
            USBLog(6, "%s[%p]: restarting at TD %p; transaction not complete.",
                   getName(), this, td);
            return false;  // the transaction will continue
        }
    }
    return true; // this transaction is complete
}

bool
AppleUSBUHCI::IsTransactionComplete(UHCITransaction *tp)
{
    TD *td;
    UInt32 status;
    bool completed;
    
    if (tp->state != kUHCI_TP_STATE_ACTIVE)
        return false;
    
    if (tp->type == kUSBIsoc) {
        /* All frames will eventually be processed;
         * the transaction is only done when the last td is finished.
         */
        if (tp->isoc_num_frames == 0) {
            completed = true;
        } else {
            // The transaction is only complete after the filter interrupt routine
            // has timestamped the last frame.
            completed = (tp->last_td->fllp == NULL);
        }
    } else {
        /* Examine the T bit in the HW queue header;
         * it should be set if the controller is done with the queue.
         */
        if (USBToHostLong(tp->qh->hw.elink) & kUHCI_QH_T) {
            USBLog(7, "%s[%p]: T bit set in queue header for tp %p", getName(), this, tp);
            completed = true;
        } else {
        /* Is the last TD in the chain done? */
        status = USBToHostLong(tp->last_td->hw.ctrlStatus);
        
        if (status & kUHCI_TD_ACTIVE) {
            /* The last TD isn't complete.
            * Look through all the TDs for an error.
            */
            completed = true;
            for (td = tp->first_td; td != NULL; td = td->link) {
                status = USBToHostLong(td->hw.ctrlStatus);
                if (status & kUHCI_TD_ACTIVE) {
                    /* Active TD means transaction isn't complete. */
                    //USBLog(6, "%s[%p]: Active TD %p; transaction not complete.",
                    //       getName(), this, td);
                        //DumpTD(td, 6);
                    completed = false;
                    break;
                }
                if (status & kUHCI_TD_STALLED) {
                    /* Transaction completed with an error. */
                        USBLog(6, "%s[%p]: ======= Stall error TD %p tp %p ==============",
                               getName(), this, td, tp);
                    completed = true;
                    break;
                }
                if ((status & kUHCI_TD_SPD) &&
                    UHCI_TD_GET_ACTLEN(status) <
                    UHCI_TD_GET_MAXLEN(USBToHostLong(td->hw.token))) {
                    /* Short packet; that is OK. */
                        USBLog(6, "%s[%p]: ===============Short packet TD %p tp %p ================",
                               getName(), this, td, tp);
                    completed = HandleShortPacket(tp, td);
                    break;
                }
                if (td == tp->last_td) {
                    break;
                }
            }
        } else {
                /* The last TD in the chain is not active. */
                //USBLog(6, "%s[%p]: last TD inactive for tp %p", getName(), this, tp);
                completed = true;
            }
        }        
    }
    
    return completed;
}

int
AppleUSBUHCI::ProcessCompletedTransactions(void)
{
    UHCITransaction *tp;
    queue_chain_t save;
    queue_head_t complete;
    UInt16 frame;
    int completed;
#if DEBUG
    int processed1 = 0, processed2 = 0;
#endif
    
    //USBLog(5, "%s[%p]::ProcessCompletedTransactions ************", getName(), this);
    
    frame = (ReadFrameNumberRegister() - 1);
    frame = frame % kUHCI_NVFRAMES;
    
    queue_init(&complete);
    
#if DEBUG
    queue_iterate(&_activeTransactions, tp, UHCITransaction *, active_chain) {
        processed1++;
    }
    
    USBLog(5, "%s[%p]: num in queue %d\n", getName(), this, processed1);
#endif
    
    for (tp = (UHCITransaction *)queue_first(&_activeTransactions);
         !queue_end(&_activeTransactions, (queue_entry_t)tp);
         tp = (UHCITransaction *)queue_next(&save)) {
        
        /* Save the next/prev pointer in case we remove it from the chain. */
        save = tp->active_chain;
        
        //USBLog(5, "%s[%p]: *** Processing transaction %p", getName(), this, tp);
        
        if (IsTransactionComplete(tp)) {
            USBLog(7, "****** transaction %p finished completed **********", tp);
            RemoveTransaction(tp);
            tp->state = kUHCI_TP_STATE_COMPLETE;
            queue_enter(&complete, tp, UHCITransaction *, active_chain);
        } else {
            USBLog(6, "****** transaction %p not completed", tp);
            //DumpTransaction(tp, 5);
        }
#if DEBUG
        processed2++;
#endif
    }
    //USBLog(5, "%s[%p]: ***** calling out completed transactions after %d processed", getName(), this, processed2);
    
    /* Complete transactions with upper layer, which might queue more active transactions. */
    completed = 0;
    while (!queue_empty(&complete)) {
        queue_remove_first(&complete, tp, UHCITransaction *, active_chain);
        USBLog(7, "%s[%p]: *** Calling completion on transaction %p", getName(), this, tp);
        DumpTransaction(tp);

        CompleteTransaction(tp, kIOReturnSuccess);
        completed++;
    }

#if DEBUG
    USBLog(5, "%s[%p]: ****** done processing %d transactions ******", getName(), this, completed);
    USBLog(5, "in queue %d examined %d completed %d", processed1, processed2, completed);

    if (processed == 0) {
        USBLog(5, "%s[%p]: **** No transactions processed for frame %d!! ****", getName(), this, frame);
        USBLog(5, "%s[%p]: cmd %x, sts %x", getName(), this,
               ioRead16(kUHCI_CMD), ioRead16(kUHCI_STS));
        DumpFrame(frame);
    }
#endif
    
    return completed;
}

// ========================================================================
#pragma mark Memory management
// ========================================================================

TD *
AppleUSBUHCI::AllocTD(void)
{
    TD *td;
    IOPhysicalAddress paddr;
    UInt32 align;
    
    if (_freeTDs == NULL) {
        unsigned int i, count;
        UHCIMemoryBuffer *bp;
        TD *last_td = NULL;
        
        bp = UHCIMemoryBuffer::newBuffer();
        if (bp == NULL) {
            USBError(1, "Error allocating TD memory");
            return NULL;
        }
        paddr = bp->getPhysicalAddress();
        
        USBLog(4, "%s[%p]::AllocTD got memory at %p (phys %x)", getName(), this, bp->getBytesNoCopy(), paddr);
        
        queue_enter(&_allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);

        align = ((sizeof(TD) + kUHCI_TD_ALIGN - 1) / kUHCI_TD_ALIGN) * kUHCI_TD_ALIGN;
        USBLog(7, "TD alignment is %d", align);
        count = PAGE_SIZE / align;
            
        td = (TD *)bp->getBytesNoCopy();
        for (i=0; i<count; i++) {
            td->paddr = paddr;
            td->link = last_td;
            USBLog(7, "Creating TD %x with paddr %x", td, td->paddr);
            last_td = td;
            td = (TD *)(((UInt8 *)td) + align);
            paddr += align;
        }
        _freeTDs = last_td;
        USBLog(4, "%s[%p]: last td is at %p", getName(), this, last_td);
    }
    td = _freeTDs;
    _freeTDs = td->link;
    bzero(&td->hw, sizeof(td->hw));
    IOSync();
    td->link = NULL;
    td->buffer = NULL;
    td->fllp = NULL;
    return td;
}

void 
AppleUSBUHCI::FreeTD(TD *td)
{
    /* Clearing the status is helpful for debugging. */
    td->hw.ctrlStatus = 0;
    IOSync();
    td->fllp = NULL;
    
    td->link = _freeTDs;
    _freeTDs = td;
}

QH *
AppleUSBUHCI::AllocQH(void)
{
    QH *qh;
    IOPhysicalAddress paddr;
    UInt32 align;
    
    if (_freeQHs == NULL) {
        unsigned int i, count;
        UHCIMemoryBuffer *bp;
        QH *last_qh = NULL;
        
        bp = UHCIMemoryBuffer::newBuffer();
        if (bp == NULL) {
            USBError(1, "Error allocating TD memory");
            return NULL;
        }
        paddr = bp->getPhysicalAddress();

        USBLog(4, "%s[%p]::AllocQH got memory at %p (phys %x)", getName(), this, bp->getBytesNoCopy(), paddr);
        
        queue_enter(&_allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
        
        align = ((sizeof(QH) + kUHCI_QH_ALIGN - 1) / kUHCI_QH_ALIGN) * kUHCI_QH_ALIGN;
        count = PAGE_SIZE / align;

        qh = (QH *)bp->getBytesNoCopy();
        for (i=0; i<count; i++) {
            qh->paddr = paddr;
            qh->hlink = last_qh;
            //USBLog(7, "Creating QH %x with paddr %x", qh, qh->paddr);
            last_qh = qh;
            qh = (QH *)(((UInt8 *)qh) + align);
            paddr += align;
        }
        _freeQHs = last_qh;
        USBLog(4, "%s[%p]: last qh is at %p", getName(), this, last_qh);
    }
    qh = _freeQHs;
    _freeQHs = qh->hlink;
    bzero(&qh->hw, sizeof(qh->hw));
    IOSync();
    qh->hlink = NULL;
    qh->elink = NULL;
    qh->qlink = NULL;
    return qh;
}

void 
AppleUSBUHCI::FreeQH(QH *qh)
{
    /* Clearing the links is helpful for debugging. */
    qh->hw.elink = HostToUSBLong(kUHCI_QH_T);
    qh->hw.hlink = HostToUSBLong(kUHCI_QH_T);
    IOSync();
    
    qh->hlink = _freeQHs;
    _freeQHs = qh;
}

UHCITransaction *
AppleUSBUHCI::AllocTransaction(UHCIEndpoint *ep)
{
    UHCITransaction *tp;
    
    if (ep == NULL) {
        USBError(1, "%s[%p]: NULL endpoint in AllocTransaction!!!!!", getName(), this);
    }
    
    if (queue_empty(&_freeTransactions)) {
        UHCIMemoryBuffer *bp;
        unsigned int i, count;
        
        bp = UHCIMemoryBuffer::newBuffer(false);
        if (bp == NULL) {
            USBError(1, "%s[%p]: Error allocating transaction memory", getName(), this);
            return NULL;
        }

        queue_enter(&_allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
        
        USBLog(4, "%s[%p]::AllocTransaction got buffer at %p", getName(), this, bp->getBytesNoCopy());
        
        count = PAGE_SIZE / sizeof(UHCITransaction);
        
        tp = (UHCITransaction *)bp->getBytesNoCopy();
        for (i=0; i<count; i++) {
            queue_enter(&_freeTransactions, tp, UHCITransaction *, active_chain);
            tp++;
        }
        USBLog(5, "%s[%p]: last chunk at %p", getName(), this, --tp);
    }
    
    queue_remove_first(&_freeTransactions, tp, UHCITransaction *, active_chain);
    bzero(tp, sizeof(UHCITransaction));
    tp->endpoint = ep;
    tp->type = ep->type;
    //queue_enter(&ep->transactions, tp, UHCITransaction *, endpoint_chain);
    return tp;
}


void
AppleUSBUHCI::FreeTransaction(UHCITransaction *tp)
{
    TD *td, *next_td;
    
    USBLog(7, "%s[%p]::FreeTransaction(%p)", getName(), this, tp);
    
    if (tp->state == kUHCI_TP_STATE_ACTIVE) {
        // Freeing an active transaction is erroneous and will likely crash.
        USBError(1, "%s[%p]: freeing active transaction %p", getName(), this, tp);
        DumpTransaction(tp, 2);
        // Better to leak the transaction than to panic.
        return;
    }
    
    //DumpTransaction(tp);
    
    //USBLog(5, "freeing tds");
    
    /* In an isoc transaction, the TDs stay with the endpoint,
     * not the transaction.
     */
    if (tp->type == kUSBIsoc) {
        unsigned int i;
        unsigned int frame = tp->isoc_start_frame;
        TD **isoc_tds = tp->endpoint->isoc_tds;

        for (i=0; i < tp->isoc_num_frames; i++) {
            td = isoc_tds[frame];
            td->fllp = NULL;
            td->hw.ctrlStatus &= HostToUSBLong(~(kUHCI_TD_IOC | kUHCI_TD_ACTIVE));
            IOSync();
            frame++;
            if (frame >= kUHCI_NVFRAMES) {
                frame = 0;
            }
            if (td->buffer != NULL) {
                EndpointFreeBuffer(tp->endpoint, td->buffer);
                td->buffer = NULL;
            }
        }
    } else {
        for (td = tp->first_td; td != NULL; ) {
            next_td = td->link;
            if (td->buffer != NULL) {
                EndpointFreeBuffer(tp->endpoint, td->buffer);
                td->buffer = NULL;
            }
            FreeTD(td);
            td = next_td;
        }
    }
    //USBLog(5, "freeing qh");
    if (tp->qh) {
        FreeQH(tp->qh);
    }
    tp->first_td = NULL;
    tp->last_td = NULL;
    
    if (tp->isoc_map != NULL) {
        tp->isoc_map->release();
        tp->isoc_map = NULL;
    }
    
    tp->endpoint = NULL;
    tp->state = kUHCI_TP_STATE_FREE;
    queue_enter(&_freeTransactions, tp, UHCITransaction *, active_chain);
}



UHCIEndpoint * 
AppleUSBUHCI::AllocEndpoint(UInt16 functionNumber,
                            UInt16 endpointNumber,
                            UInt16 direction,
                            UInt16 speed,
                            UInt16 maxPacketSize,
                            UInt32 type)
{
    UHCIEndpoint *ep;
    int i;
    
    ep = (UHCIEndpoint *)IOMalloc(sizeof(UHCIEndpoint));
    
    if (ep == NULL)
        return NULL;
    
    bzero(ep, sizeof(UHCIEndpoint));
    
    queue_init(&ep->activeTransactions);
    queue_enter(&_endpoints, ep, UHCIEndpoint *, chain);
    queue_init(&ep->allocatedBuffers);
    queue_init(&ep->freeBuffers);

    ep->functionNumber = functionNumber;
    ep->endpointNumber = endpointNumber;
    ep->direction = direction;
    ep->speed = speed;
    ep->maxPacketSize = maxPacketSize;
    ep->maxBufferSize = maxPacketSize;
    ep->type = type;
    ep->lastDBit = true; // Start with DATA0 by default
    
    /* Allocate TDs for Isochronous transaction use. */
    if (type == kUSBIsoc) {
        TD *td;
        ep->isoc_tds = (TD **)IOMalloc(sizeof(TD *) * kUHCI_NVFRAMES);
        if (ep->isoc_tds == NULL) {
            FreeEndpoint(ep);
            return NULL;
        }
        for (i=0; i<kUHCI_NVFRAMES; i++) {
            td = AllocTD();
            if (td == NULL) {
                FreeEndpoint(ep);
                return NULL;
            }
            ep->isoc_tds[i] = td;
        }
    }
    /* If this is an interrupt endpoint,
     * the qhs will be allocated by the caller.
     */
    
    /* Ensure first transaction is DATA0. */
    ep->lastDBit = true;
    
    /* Queue head under which all transactions will go for this endpoint. */
    ep->head_qh = AllocQH();
    if (ep->head_qh == NULL) {
        FreeEndpoint(ep);
        return NULL;
    }
    
    return ep;
}


void
AppleUSBUHCI::FreeEndpoint(UHCIEndpoint *ep)
{

    if (ep != _rhEndpoint) {
        queue_remove(&_endpoints, ep, UHCIEndpoint *, chain);
    }

    if (ep->head_qh) {
        FreeQH(ep->head_qh);
        ep->head_qh = NULL;
    }

    if (ep->isoc_tds) {
        int i;
        for (i=0; i<kUHCI_NVFRAMES; i++) {
            if (ep->isoc_tds[i]) {
                FreeTD(ep->isoc_tds[i]);
            }
        }
        IOFree(ep->isoc_tds, sizeof(TD *) * kUHCI_NVFRAMES);
        ep->isoc_tds = NULL;
    }
    
    if (ep->buffersInUse > 0) {
        USBError(1, "%s[%p]::FreeEndpoint freeing endpoint with active alignment buffers",
                 getName(), this);
    }
    EndpointFreeAllBuffers(ep);
    
    IOFree(ep, sizeof(UHCIEndpoint));
}


UHCIAlignmentBuffer *
AppleUSBUHCI::EndpointAllocBuffer(UHCIEndpoint *ep)
{
    UHCIAlignmentBuffer *ap;
    UInt32 align;
    
    if (queue_empty(&ep->freeBuffers)) {
        unsigned int i, count;
        UHCIMemoryBuffer *bp;
        IOPhysicalAddress paddr;
        IOVirtualAddress  vaddr;
        
        bp = UHCIMemoryBuffer::newBuffer();
        if (bp == NULL) {
            USBError(1, "%s[%p]: Error allocating alignment buffer memory", getName(), this);
            return NULL;
        }
        paddr = bp->getPhysicalAddress();
        vaddr = (IOVirtualAddress)bp->getBytesNoCopy();
        
        queue_enter(&ep->allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
     
        align = ((ep->maxBufferSize + kUHCI_BUFFER_ALIGN - 1) / kUHCI_BUFFER_ALIGN) * kUHCI_BUFFER_ALIGN;
        count = PAGE_SIZE / align;

        for (i=0; i<count; i++) {
            ap = (UHCIAlignmentBuffer *)IOMalloc(sizeof(UHCIAlignmentBuffer));

            ap->paddr = paddr;
            ap->vaddr = vaddr;
            queue_enter(&ep->freeBuffers, ap, UHCIAlignmentBuffer *, chain);
            USBLog(4, "Creating alignment buffer %x size %d with paddr %x", ap, align, ap->paddr);
            ap++;
            paddr += align;
            vaddr += align;
        }
    }
    
    queue_remove_first(&ep->freeBuffers, ap, UHCIAlignmentBuffer *, chain);
    ep->buffersInUse++;
    ap->userBuffer = NULL;
    ap->userOffset = 0;
    ap->userAddr = NULL;
    return ap;
}

void
AppleUSBUHCI::EndpointFreeBuffer(UHCIEndpoint *ep, UHCIAlignmentBuffer *ap)
{
    queue_enter(&ep->freeBuffers, ap, UHCIAlignmentBuffer *, chain);
    ep->buffersInUse--;
}

IOReturn
AppleUSBUHCI::EndpointFreeAllBuffers(UHCIEndpoint *ep)
{
    UHCIAlignmentBuffer *ap;
    
    if (ep->buffersInUse > 0) {
        return kIOReturnBusy;
    }
    
    /* Free all alignment buffer descriptors. */
    while (!queue_empty(&ep->freeBuffers)) {
        queue_remove_first(&ep->freeBuffers, ap, UHCIAlignmentBuffer *, chain);
        IOFree(ap, sizeof(UHCIAlignmentBuffer));
    }
    
    /* Free actual memory used by buffers. */
    /* No need to remove them from the buffer chain. */
    while (!queue_empty(&ep->allocatedBuffers)) {
        UHCIMemoryBuffer *bp;
        
        queue_remove_first(&ep->allocatedBuffers, bp, UHCIMemoryBuffer *, _chain);
        bp->release();
    }
    queue_init(&ep->freeBuffers);
    return kIOReturnSuccess;
}

// ========================================================================
#pragma mark Debugging
// ========================================================================


void
AppleUSBUHCI::DumpTransaction(UHCITransaction *tp, int level)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    char *type_name;
    char *state_name;
    
    USBLog(level, "==== TP %p ====", tp);
    USBLog(level, "buf = %p, bufLen = %d", tp->buf, tp->bufLen);
    USBLog(level, "nCompletions = %d", tp->nCompletions);
    USBLog(level, "command = %p", tp->command);
    switch(tp->type) {
        case kUSBControl:
            type_name = "Control";
            break;
        case kUSBInterrupt:
            type_name = "Interrupt";
            break;
        case kUSBBulk:
            type_name = "Bulk";
            break;
        case kUSBIsoc:
            type_name = "Isoc";
            break;
        default:
            type_name = "Unknown";
            break;
    }    
    switch(tp->state) {
    case kUHCI_TP_STATE_NULL:
        state_name = "NULL";
        break;
    case kUHCI_TP_STATE_FREE:
        state_name = "FREE";
        break;
    case kUHCI_TP_STATE_ACTIVE:
        state_name = "ACTIVE";
        break;
    case kUHCI_TP_STATE_COMPLETE:
        state_name = "COMPLETE";
        break;
    case kUHCI_TP_STATE_ABORTED:
        state_name = "ABORTED";
        break;
    default:
        state_name = "UNKNOWN";
        break;
    }
    USBLog(level, "state = %s, type %s, timeout %d", state_name, type_name, tp->timeout);
    USBLog(level, "endpoint = %p", tp->endpoint);
    USBLog(level, "active_chain = %p,%p", tp->active_chain.prev, tp->active_chain.next);
    USBLog(level, "endpoint_chain = %p,%p", tp->endpoint_chain.prev, tp->endpoint_chain.next);
    USBLog(level, "first_td = %p, last_td = %p", tp->first_td, tp->last_td);
    
    USBLog(level, "qh %p:", tp->qh);
    if (tp->type == kUSBIsoc) {
        unsigned int i, frame;
        TD *td;
        TD **isoc_tds = tp->endpoint->isoc_tds;
        
        USBLog(level, "isoc_start_frame = %d", tp->isoc_start_frame);
        USBLog(level, "isoc_num_frames = %d", tp->isoc_num_frames);
        frame = tp->isoc_start_frame;
        for (i=0; i<tp->isoc_num_frames; i++) {
            td = isoc_tds[frame];
            DumpTD(td, level);
            
            frame++;
            if (frame >= kUHCI_NVFRAMES) {
                frame = 0;
            }
        }
    } else {
        if (tp->qh) {
            DumpQH(tp->qh, level);
        } else {
            DumpTDChain(tp->first_td, false, level);
        }
    }
    USBLog(level, "first_td = %p, last_td = %p", tp->first_td, tp->last_td);
    
    USBLog(level, "==== END TP %p ====", tp);
#endif
}


void
AppleUSBUHCI::DumpTD(TD *td, int level)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    UInt32 value;
    
    USBLog(level, "=== TD %p ====", td);
    USBLog(level, "paddr %x, link %p, buffer %p", td->paddr, td->link, td->buffer);
    value = USBToHostLong(td->hw.link);
    USBLog(level, "HW: link     %08x %s%s%s",
           (value & 0xFFFFFFF0),
           (value & 0x4) ? "Vf " : "",
           (value & 0x2) ? "Q " : "",
           (value & 0x1) ? "T" : "");
    value = USBToHostLong(td->hw.ctrlStatus);
    USBLog(level, "  ctrlStatus %08x ActLen %x Status %x Err %x %s%s%s%s", value,
           UHCI_TD_GET_ACTLEN(value),
           UHCI_TD_GET_STATUS(value),
           UHCI_TD_GET_ERRCNT(value),
           (value & kUHCI_TD_IOC) ? "IOC " : "",
           (value & kUHCI_TD_ISO) ? "ISO " : "",
           (value & kUHCI_TD_LS) ? "LS " : "",
           (value & kUHCI_TD_SPD) ? "SPD " : "");
    USBLog(level, "     status %s%s%s%s%s%s%s",
           (value & kUHCI_TD_ACTIVE) ? "ACTIVE " : "",
           (value & kUHCI_TD_STALLED) ? "STALL " : "",
           (value & kUHCI_TD_DBUF) ? "DBUF " : "",
           (value & kUHCI_TD_BABBLE) ? "BABBLE " : "",
           (value & kUHCI_TD_NAK) ? "NAK " : "",
           (value & kUHCI_TD_CRCTO) ? "CRCTO " : "",
           (value & kUHCI_TD_BITSTUFF) ? "BITSTUFF" : "");
    value = USBToHostLong(td->hw.token);
    char *token_type;
    switch (value & kUHCI_TD_PID) {
    case kUHCI_TD_PID_SETUP:
        token_type = "(SETUP)";
        break;
    case kUHCI_TD_PID_IN:
        token_type = "(IN)";
        break;
    case kUHCI_TD_PID_OUT:
        token_type = "(OUT)";
        break;
    default:
        token_type = "(UNKNOWN)";
        break;
    }
    USBLog(level, "       token %08x %s DevAddr %x EndPt %x MaxLen %x %s", value,
           token_type,
           UHCI_TD_GET_ADDR(value),
           UHCI_TD_GET_ENDPT(value),
           UHCI_TD_GET_MAXLEN(value),
           (value & kUHCI_TD_D) ? "D" : "");
    USBLog(level, "      buffer %08x", USBToHostLong(td->hw.buffer));
    USBLog(level, "=== END TD %p ====", td);
#endif
}

void
AppleUSBUHCI::DumpTDChain(TD *td, bool qhOK, int level)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    int td_count = 0;
    USBLog(level, "============  Dump TD chain ============");
    while (td) {
        if (++td_count > 1024 * 1024) {
            USBLog(level, "[too many TDs in chain, aborting]");
            break;
        }
        DumpTD(td, level);
        if (!qhOK && USBToHostLong(td->hw.link) & kUHCI_TD_Q) {
            USBLog(level, "QH found, chain ended");
            break;
        }
        td = td->link;
    }
    USBLog(level, "==========  End Dump TD chain ==========");
#endif
}

void
AppleUSBUHCI::DumpQH(QH *qh, int level)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    UInt32 value;
    int i;
    
    USBLog(level, "=== QH %p ====", qh);
    if (qh == _lsControlQHStart) {
        USBLog(level, "++++ Low Speed QH Start ++++");
    } 
    if (qh == _lsControlQHEnd) {
        USBLog(level, "++++ Low Speed QH End ++++");
    } 
    if (qh == _hsControlQHStart) {
        USBLog(level, "++++ High Speed QH Start ++++");
    } 
    if (qh == _hsControlQHEnd) {
        USBLog(level, "++++ High Speed QH End ++++");
    } 
    if (qh == _bulkQHStart) {
        USBLog(level, "++++ Bulk QH Start ++++");
    }
    if (qh == _bulkQHEnd) {
        USBLog(level, "++++ Bulk QH End ++++");
    }
    if (qh == _lastQH) {
        USBLog(level, "++++++++ Last QH ++++++++");
    }
    
        for (i=0; i<kUHCI_NINTR_QHS; i++) {
            if (qh == _intrQH[i]) {
                USBLog(level, "++++ Interrupt QH %d ++++", i);
            }
        }
    
    USBLog(level, "paddr %x, hlink %p, elink %p qlink %p",
           qh->paddr, qh->hlink, qh->elink, qh->qlink);
    value = USBToHostLong(qh->hw.hlink);
    USBLog(level, "HW: hlink    %08x %s %s", value,
           (value & kUHCI_QH_Q) ? "QH" : "TD",
           (value & kUHCI_QH_T) ? "T" : "");
    value = USBToHostLong(qh->hw.elink);
    USBLog(level, "    elink    %08x %s %s", value,
           (value & kUHCI_QH_Q) ? "QH" : "TD",
           (value & kUHCI_QH_T) ? "T" : "");
    USBLog(level, "==============");
    if (qh->elink) {
        USBLog(level, "vvvvvv  ELINK  vvvvvv");
        DumpTDChain(qh->elink, false, level);
        USBLog(level, "^^^^^^  ELINK  ^^^^^^");
    }
#if 0
    if (qh->qlink) {
        USBLog(level, "vvvvvv QLINK  vvvvvvv");
        DumpQHChain(qh->qlink, level);
        USBLog(level, "^^^^^^ QLINK  ^^^^^^");
    }
#endif
    USBLog(level, "=== QH %p DONE ====", qh);
#endif
}

void
AppleUSBUHCI::DumpQHChain(QH *qh, int level)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    int qh_count = 0;
    QH *start_qh = qh;
    USBLog(level, "QH chain %p ---> ---> --->", start_qh);
    while (qh) {
        if (++qh_count > 1024 * 1024) {
            USBLog(level, "[too many QHs in chain, aborting]");
            break;
        }
        USBLog(level, "---> ---> continued");
        DumpQH(qh, level);
        qh = qh->hlink;
    }
    USBLog(level, "<--- <--- <--- end of QH chain %p", start_qh);
#endif
}

void
AppleUSBUHCI::DumpFrame(UInt16 frame, int level)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    if (frame >= kUHCI_NVFRAMES) {
        USBLog(level, "** Bogus request for frame %d", frame);
        frame = frame % kUHCI_NVFRAMES;
    }
    USBLog(level, "======== Frame %d dump, starting with TD(s): ========", frame);
    DumpTDChain(_vframes[frame].td, false, level);
    USBLog(level, "Frame %d QHs:", frame);
    if (_vframes[frame].first_qh) {
        DumpQHChain(_vframes[frame].first_qh, level);
    } else {
        USBLog(level, "NULL QH!");
    }
    USBLog(level, "======== Frame %d dump end ========", frame);
#endif
}

void
AppleUSBUHCI::DumpEndpoint(UHCIEndpoint *ep, int level)
{
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
    UHCITransaction *tp;
    USBLog(level, "\n======== Dumping endpoint %p ========", ep);
    DumpQH(ep->head_qh, level);
    USBLog(level, "\n======== transactions ========");
    queue_iterate(&ep->activeTransactions, tp, UHCITransaction *, endpoint_chain) {
        DumpTransaction(tp, level);
    }
    USBLog(level, "======== DONE dumping endpoint %p ========\n", ep);
#endif
}


#if SINGLE_STEP

/* Single step for debugging. */

void
AppleUSBUHCI::SingleStep(int count, bool runAfter)
{
    UInt16 cmd;
    UInt16 frame;
    UInt16 status;
    int i;
    QH *qh;
    
    Run(false);
    
    cmd = ioRead16(kUHCI_CMD) | kUHCI_CMD_SWDBG;
    ioWrite16(kUHCI_CMD, cmd);
    
    ioWrite16(kUHCI_FRNUM, 0);
    IODelay(10);
    
    while (count--) {
        frame = ioRead16(kUHCI_FRNUM);
        USBLog(3, "%s[%p]: single stepping frame %d", getName(), this, frame);
        
        i = frame % kUHCI_NVFRAMES;
        qh = _vframes[i].first_qh;
        DumpQHChain(qh);
        
        cmd = cmd | kUHCI_CMD_RS;
        ioWrite16(kUHCI_CMD, cmd);
        
        do {
            IODelay(10);
            cmd = ioRead16(kUHCI_CMD);
        } while (cmd & kUHCI_CMD_RS);
        
        status = ioRead16(kUHCI_STS);
        
        USBLog(3, "%s[%p]: status %x", getName(), this, status);
    }
    
    if (runAfter) {
        Run(true);
    }
}

#endif /* SINGLE_STEP */




