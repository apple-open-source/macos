/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2006 Apple Computer, Inc.  All Rights Reserved.
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
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/pccard/IOPCCard.h>

#include "AppleUSBEHCI.h"
#include "AppleEHCIedMemoryBlock.h"
#include "AppleEHCItdMemoryBlock.h"
#include "AppleEHCIitdMemoryBlock.h"
#include "AppleEHCIsitdMemoryBlock.h"

#define super IOUSBControllerV2
#define self this

#define NUM_BUFFER_PAGES	9   // 54
#define NUM_TDS			255 // 1500
#define NUM_EDS			256 // 1500
#define NUM_ITDS		192 // 1300

// TDs  per page == 85
// EDs  per page == 128
// ITDs per page == 64


OSDefineMetaClassAndStructors(AppleUSBEHCI, IOUSBControllerV2)

bool 
AppleUSBEHCI::init(OSDictionary * propTable)
{
    if (!super::init(propTable)) 
		return false;
	
    _ehciBusState = kEHCIBusStateOff;
    _ehciAvailable = true;
    _hasPCIPwrMgmt = false;
	
    _intLock = IOLockAlloc();
    if (!_intLock)
        return(false);
	
    _controllerSpeed = kUSBDeviceSpeedHigh;	// This needs to be set before start.
											// super uses it during start method
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
    
    return (true);
}



void
AppleUSBEHCI::free()
{
    // Free our locks
    //
    IOLockFree( _intLock );
    IOSimpleLockFree( _wdhLock );
    IOSimpleLockFree( _isochScheduleLock );

    super::free();
}



void AppleUSBEHCI::showRegisters(char *s)
{
    int i;
	
    USBLog(3,"EHCIUIM -- showRegisters %s version: 0x%x", s, USBToHostWord(_pEHCICapRegisters->HCIVersion));
    USBLog(3,"USBCMD:  %p", (void*)USBToHostLong(_pEHCIRegisters->USBCMD));
    USBLog(3,"USBSTS:  %p", (void*)USBToHostLong(_pEHCIRegisters->USBSTS));
    USBLog(3,"USBIntr: %p", (void*)USBToHostLong(_pEHCIRegisters->USBIntr));
    USBLog(3,"FRIndex: %p", (void*)USBToHostLong(_pEHCIRegisters->FRIndex));
    USBLog(3,"CTRLDSSeg:    %p", (void*)USBToHostLong(_pEHCIRegisters->CTRLDSSegment));
    USBLog(3,"PerListBase:  %p", (void*)USBToHostLong(_pEHCIRegisters->PeriodicListBase));
    USBLog(3,"AsyncListAd:  %p", (void*)USBToHostLong(_pEHCIRegisters->AsyncListAddr));
    USBLog(3,"ConfFlg: %p", (void*)USBToHostLong(_pEHCIRegisters->ConfigFlag));
    for(i=0;i<5;i++)
    {
        UInt32 x;
        x = USBToHostLong(_pEHCIRegisters->PortSC[i]);
        if(x != 0x1000)
            USBLog(3,"PortSC[%d]: 0x%lx", i+1, x);
    }
}



bool
AppleUSBEHCI::start( IOService * provider )
{
    USBLog(7, "AppleUSBEHCI[%p]::start",  this);
    
    if( !super::start(provider))
        return (false);
    
    // initialize power management
    initForPM(_device);
	
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
    int					i;
    
    USBLog(7, "AppleUSBEHCI[%p]::UIMInitialize",  this);
	
    _device = OSDynamicCast(IOPCIDevice, provider);
    if(_device == NULL)
        return kIOReturnBadArgument;
	
    do {
		
        if (!(_deviceBase = provider->mapDeviceMemoryWithIndex(0)))
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - unable to get device memory",  this);
            err = kIOReturnNoResources;
            break;
        }
		
        USBLog(3, "AppleUSBEHCI[%p]::UIMInitialize config @ %lx (%lx)\n", this,
			   (long)_deviceBase->getVirtualAddress(),
			   _deviceBase->getPhysicalAddress());
		
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
        
        USBLog(7, "AppleUSBEHCI[%p]::UIMInitialize - errata bits=%p",  this,  (void*)_errataBits);
		
        _pageSize = PAGE_SIZE;
        _pEHCICapRegisters = (EHCICapRegistersPtr) _deviceBase->getVirtualAddress();
		
        // enable the card registers
        lvalue = _device->configRead16(cwCommand);
        _device->configWrite16(cwCommand, lvalue | cwCommandEnableMemorySpace);
		
        err = AcquireOSOwnership();
        if ( err != kIOReturnSuccess )
        {
            USBError(1,"AppleUSBEHCI[%p]: unable to obtain ownership: 0x%x",  this, err);
            break;
        }
		
		// determine whether this is a 32 bit or 64 bit machine
		hccparams = USBToHostLong(_pEHCICapRegisters->HCCParams);
		if (hccparams & kEHCI64Bit)
			_is64bit = true;
		else
			_is64bit = false;
		
		ist = (hccparams & kEHCIISTMask) >> kEHCIISTPhase;
		
		if (!ist)
			_istKeepAwayFrames = 1;
		else
			_istKeepAwayFrames = 2;

	   	CapLength  = _pEHCICapRegisters->CapLength;
		_pEHCIRegisters = (EHCIRegistersPtr) ( ((UInt32)_pEHCICapRegisters) + CapLength);
		
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
                        
        // enable card bus mastering
        lvalue = _device->configRead16(cwCommand);
         _device->configWrite16(cwCommand, lvalue  | cwCommandEnableBusMaster | cwCommandEnableMemorySpace);

		// turn on transaction completion/error interrupts
		_pEHCIRegisters->USBIntr = HostToUSBLong(kEHCICompleteIntBit | kEHCIErrorIntBit | kEHCIHostErrorIntBit | kEHCIFrListRolloverIntBit);
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
			USBCmd |= kEHCICMDRunStop;
			_ehciBusState = kEHCIBusStateRunning;
			// this command will change the park mode from 3 to 2
			//USBCmd &= ~kEHCICMDAsyncParkModeCountMask;
			// USBCmd |= (2 << kEHCICMDAsyncParkModeCountMaskPhase);
			// this line will eliminate park mode completely
			// USBCmd &= ~kEHCICMDAsyncParkModeEnable;
			USBCmd |= 8 << kEHCICMDIntThresholdOffset;					// Interrupt every 8 micro frames (1 frame)
			_pEHCIRegisters->USBCMD = USBToHostLong(USBCmd);				// start your engines
			
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
			
			// _pEHCIRegisters->PortSC   // Let root hub play with this.
			
			// Should revisit the Isoc bandwidth issue sometime.
			_isochBandwidthAvail = 5 *1024;
			_outSlot = kEHCIPeriodicListEntries+1;	/* No Isoc transactions currently. */
			_frameNumber = 0;
			if ((err = InterruptInitialize()))
				continue;
			
			_uimInitialized = true;
			
			// showRegisters("UIMInitialize");
			
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
			USBLog(2, "acquireOSOwnership done - value %p", (void*)_device->configRead32(eecp));
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
    int 			i;
    UInt32 			termBit, *list; 
    IOUSBControllerListElement	**logical;
    IOPhysicalAddress 		physPtr;
	
    _periodicList = (UInt32 *)IOMallocContiguous(kEHCIPeriodicFrameListsize, kEHCIPageSize, &physPtr);
	
    if(_periodicList == NULL)
    {
		return kIOReturnNoResources;
    }
	
    _pEHCIRegisters->PeriodicListBase = HostToUSBLong(physPtr);
	
    _logicalPeriodicList = (IOUSBControllerListElement **)IOMalloc(kEHCIPeriodicFrameListsize);
    if(_logicalPeriodicList == NULL)
    {
		IOFreeContiguous(_periodicList, kEHCIPeriodicFrameListsize);
        _periodicList = NULL;
		return kIOReturnNoResources;
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
        USBLog (3, "AppleUSBEHCI[%p]: @ %lx (%lx)(shutting down HW)", this, (long)_deviceBase->getVirtualAddress(), _deviceBase->getPhysicalAddress());
    
    // Disable the interrupt delivery
    //
    if ( _workLoop )
        _workLoop->disableAllInterrupts();
	
    // Wait for the interrupts to propagate
    //
    IOSleep(2);
	
    // Clean up our power down notifier.  That will release it.
    //
    if ( _powerDownNotifier ) {
        _powerDownNotifier->remove();
        _powerDownNotifier = NULL;
    }
    
    USBLog(1, "AppleUSBEHCI[%p]: UIMFinalize %x %p %p",  this, isInactive(), _pEHCIRegisters, _device);
	
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
		_ehciBusState = kEHCIBusStateOff;

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

        // Take away the controllers ability be a bus master.
        //
        _device->configWrite32(cwCommand, cwCommandEnableMemorySpace);
		
        _pEHCIRegisters->PeriodicListBase = 0;		// no periodic list as yet
		_pEHCIRegisters->AsyncListAddr = 0;			// no async list as yet
		IOSync();
			
    }
	
    // Free the memory allocated in the InterruptInitialize()
    //
    if ( _periodicList )
	{
        IOFreeContiguous( _periodicList, kEHCIPeriodicFrameListsize );
		_periodicList = NULL;
	}
	
    if ( _logicalPeriodicList )
	{
        IOFree( _logicalPeriodicList, kEHCIPeriodicFrameListsize );
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


//=============================================================================================
//
//  UIMInitializeForPowerUp
//
//  This routine is called for a controller that cannot survive sleep (mostly because it loses
//  power across sleep.  It will re-intialize the EHCI registers and do everything needed to get
//  the card up.  The one thing that it does not do is do any memory allocations, as those have
//  already been done and we don't need to do it again.  This routine is essentially the same as
//  UIMInitialize w/out the memory allocations.
//
//  To Do:  Verify that we do need to call the ControlInintialize(), BulkInitialize(), etc.  I'm
//	    not sure that we do.
//
//=============================================================================================
//
IOReturn 
AppleUSBEHCI::UIMInitializeForPowerUp(void)
{
    UInt32		CapLength, USBCmd;
    IOReturn	err = kIOReturnSuccess;
    UInt16		lvalue;
    int			i;
	
    USBLog(5, "AppleUSBEHCI[%p]:UIMInitializeForPowerUp -  @ %p (%p)",  this, (void*)_deviceBase->getVirtualAddress(), (void*)_deviceBase->getPhysicalAddress());
    
    do {
		
        // enable the card registers
        lvalue = _device->configRead16(cwCommand);
        _device->configWrite16(cwCommand, lvalue | cwCommandEnableMemorySpace);
		
        CapLength  = _pEHCICapRegisters->CapLength;
        _pEHCIRegisters = (EHCIRegistersPtr) ( ((UInt32)_pEHCICapRegisters) + CapLength);
		
        // Enable the interrupt delivery.
        _workLoop->enableAllInterrupts();
		
        _rootHubFuncAddress = 1;
		
        // reset the chip and make sure that all is well
        //
        _pEHCIRegisters->USBCMD = 0;  			// this sets r/s to stop
        IOSync();
        for ( i = 0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++ )
            IOSleep(1);
        
        if ( i >= 100 )
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - could not get chip to halt within 100 ms",  this);
            err = kIOReturnInternalError;
            break;
        }
		
        // set the reset bit
        //
        _pEHCIRegisters->USBCMD = HostToUSBLong(kEHCICMDHCReset);		
        IOSync();
        
        for ( i = 0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBCMD) & kEHCICMDHCReset); i++ )
            IOSleep(1);
        
        if ( i >= 100 )
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMInitializeForPowerUp - could not get chip to come out of reset within 100 ms",  this);
            err = kIOReturnInternalError;
            break;
        }
		
        // restore saved periodic list
        //
		if (_pEHCIRegisters->PeriodicListBase != _savedPeriodicListBase)
		{
			USBLog(1, "AppleUSBEHCI[%p]::UIMInitializeForPowerUp - restoring PeriodicListBase[from 0x%lx to 0x%lx]",  this, _pEHCIRegisters->PeriodicListBase, _savedPeriodicListBase);
			_pEHCIRegisters->PeriodicListBase = _savedPeriodicListBase;
			IOSync();
		}
		
		if (_pEHCIRegisters->AsyncListAddr != _savedAsyncListAddr)
		{
			USBLog(1, "AppleUSBEHCI[%p]::UIMInitializeForPowerUp - restoring AsyncListAddr[from 0x%lx to 0x%lx]",  this, _pEHCIRegisters->AsyncListAddr, _savedAsyncListAddr);
			_pEHCIRegisters->AsyncListAddr = _savedAsyncListAddr;
			IOSync();
		}
		
        // enable card bus mastering
        lvalue = _device->configRead16(cwCommand);
        _device->configWrite16(cwCommand, lvalue | (cwCommandEnableBusMaster | cwCommandEnableMemorySpace));
		
        // turn on transaction completion/error interrupts
        //
        _pEHCIRegisters->USBIntr = HostToUSBLong(kEHCICompleteIntBit | kEHCIErrorIntBit | kEHCIHostErrorIntBit | kEHCIFrListRolloverIntBit);
        IOSync();
		
        USBCmd = USBToHostLong(_pEHCIRegisters->USBCMD);
        
        // check to make sure we have the correct Frame List Size
        //
        _frameListSize = (USBCmd & kEHCICMDFrameListSizeMask) >> kEHCICMDFrameListSizeOffset;
        if (_frameListSize)
        {
            // at this time we only support a 1K frame list
            //
            USBError(1, "AppleUSBEHCI[%p]::UIMInitialize - bad _frameListSize",  this);
            err = kIOReturnInternalError;
            break;
        }
        
        _frameListSize = 1024;
        USBCmd |= kEHCICMDRunStop;
		_ehciBusState = kEHCIBusStateRunning;
		//USBCmd &= ~kEHCICMDAsyncParkModeCountMask;
		//USBCmd |= (2 << kEHCICMDAsyncParkModeCountMaskPhase);
		// this line will eliminate park mode completely
		// USBCmd &= ~kEHCICMDAsyncParkModeEnable;
        USBCmd |= 8 << kEHCICMDIntThresholdOffset;					// Interrupt every 8 micro frames (1 frame)
        _pEHCIRegisters->USBCMD = HostToUSBLong(USBCmd);				// start your engines
		
        _pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);  		// Route ports to EHCI
        IOSync();
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			UInt32		newValue = 0, count = 0;
			newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			while ((count++ < 10) && (newValue != kEHCIPortRoutingBit))
			{
				USBError(1, "EHCI driver: UIMInitializeForPowerUp - ConfigFlag bit not sticking. Retrying.");
				_pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);
				IOSync();
				newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			}
		}
		
        // _pEHCIRegisters->PortSC   // Let root hub play with this.
		
        // Should revisit the Isoc bandwidth issue sometime.
        //
        _isochBandwidthAvail = 5 *1024;
        _outSlot = kEHCIPeriodicListEntries+1;	/* No Isoc transactions currently. */
        _frameNumber = 0;
		
        _uimInitialized = true;
		
		//  showRegisters("After");
        
        return kIOReturnSuccess;
		
    } while (false);
	
    USBError(1, "AppleUSBEHCI[%p]::UIMInitializeForPowerUp - Error occurred (0x%x)",  this, err);
    UIMFinalize();
    if (_filterInterruptSource)
        _filterInterruptSource->release();
	
    return err;
}


//=============================================================================================
//
//  UIMFinalizeForPowerDown
//
//  This routine is called for a controller that cannot survive sleep (mostly because the power
//  is turned off across sleep.  It will disable the OHCI controller and turn off its power.
//  It will NOT deallocate any memory.  This routine is very similar to UIMFinalize(), except that
//  it does not deallocate memory.
//
//  To do:  Make sure that all transactions have been completed or aborted.  This should happen
//	    as a result of the terminate issued to the root hub device, but we need to make sure.
//
//=============================================================================================
//
IOReturn 
AppleUSBEHCI::UIMFinalizeForPowerDown(void)
{
    int 	i;
    IOReturn	err;
	
    if ( _deviceBase )
        USBLog (3, "AppleUSBEHCI[%p]: @ %lx (%lx)(turning off HW)", this, (long)_deviceBase->getVirtualAddress(), _deviceBase->getPhysicalAddress());
    
    // showRegisters("Before");
    
    // Disable the interrupt delivery
    //
    if ( _workLoop )
        _workLoop->disableAllInterrupts();
	
    // Wait for the interrupts to propagate
    //
    IOSleep(2);
	
    if ( _pEHCIRegisters && _device )
    {
        // If we are NOT being terminated, then talk to the OHCI controller and
        // set up all the registers to be off
        //
        // Disable All EHCI Interrupts
        //
        _pEHCIRegisters->USBIntr = 0x0;
        IOSync();
		
        // reset the chip and make sure that all is well
        _pEHCIRegisters->USBCMD = 0;  			// this sets r/s to stop
        for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++)
            IOSleep(1);
        if (i >= 100)
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMInitializeForPowerUp - could not get chip to halt within 100 ms",  this);
            err = kIOReturnInternalError;
            goto ErrorExit;
        }
		_ehciBusState = kEHCIBusStateOff;
		
        _pEHCIRegisters->USBCMD = HostToUSBLong(kEHCICMDHCReset);		// set the reset bit
        for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBCMD) & kEHCICMDHCReset); i++)
            IOSleep(1);
        if (i >= 100)
        {
            USBError(1, "AppleUSBEHCI[%p]::UIMInitializeForPowerUp - could not get chip to come out of reset within 100 ms",  this);
            err = kIOReturnInternalError;
            goto ErrorExit;
        }
        
        // Route ports back to companion controller
        _pEHCIRegisters->ConfigFlag = 0;
        IOSync();
		IOSleep(1);
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			UInt32		newValue = 0, count = 0;
			newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			while ((count++ < 10) && (newValue != 0))
			{
				USBError(1, "EHCI driver: UIMFinalizeForPowerDown - ConfigFlag bit not sticking. Retrying.");
				_pEHCIRegisters->ConfigFlag = 0;
				IOSync();
				newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			}
		}
		
		// Take away the controllers ability be a bus master.
        //
        _device->configWrite16(cwCommand, cwCommandEnableMemorySpace);
		
        _pEHCIRegisters->PeriodicListBase = 0;	// no periodic list as yet
		_pEHCIRegisters->AsyncListAddr = 0;		// no async list as yet
		IOSync();
    }
	
ErrorExit:
        _uimInitialized = false;
	
	return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EnableAsyncSchedule(void)
{
    int			i;
    IOReturn	stat = kIOReturnSuccess;
	
    if (!(_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDAsyncEnable)))
    {
		USBLog(7, "AppleUSBEHCI[%p]::EnableAsyncSchedule: enabling schedule",  this);
		// first make certain that it really is disabled
		for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSAsyncScheduleStatus); i++)
			IOSleep(1);
		if (i)
		{
			if (i >= 100)
			{
				USBLog(1, "AppleUSBEHCI[%p]::EnableAsyncSchedule: ERROR: USBCMD and USBSTS won't synchronize OFF",  this);
				stat = kIOReturnInternalError;
			}
			else
			{
				USBLog(7, "AppleUSBEHCI[%p]::EnableAsyncSchedule: had to wait %d ms for CMD and STS to synch up OFF",  this, i);
			}
		}
		
		if (!stat)
		{
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
AppleUSBEHCI::DisableAsyncSchedule(void)
{
    int		i;
    IOReturn	stat = kIOReturnSuccess;
	
    if (_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDAsyncEnable))
    {
		// first make certain that it really is enabled
		for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSAsyncScheduleStatus); i++)
			IOSleep(1);
		if (i)
		{
			if (i >= 100)
			{
				USBLog(1, "AppleUSBEHCI[%p]::DisableAsyncSchedule: ERROR: USBCMD and USBSTS won't synchronize ON",  this);
				stat = kIOReturnInternalError;
			}
			else
			{
				USBLog(7, "AppleUSBEHCI[%p]::DisableAsyncSchedule: had to wait %d ms for CMD and STS to synch up ON",  this, i);
			}
		}
		
		if (!stat)
		{
			_pEHCIRegisters->USBCMD &= HostToUSBLong(~kEHCICMDAsyncEnable);
			
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
AppleUSBEHCI::EnablePeriodicSchedule(void)
{
    int		i;
    IOReturn	stat = kIOReturnSuccess;
	
	if (_inAbortIsochEP)
	{
		USBLog(2, "EnablePeriodicSchedule - not doing in AbortIsochEP");
		return stat;
	}
    if (!(_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDPeriodicEnable)))
    {
		// first make certain that it really is disabled
		for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSPeriodicScheduleStatus); i++)
			IOSleep(1);
		if (i)
		{
			if (i >= 100)
			{
				USBLog(1, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: ERROR: USBCMD and USBSTS won't synchronize OFF",  this);
				stat = kIOReturnInternalError;
			}
			else
			{
				USBLog(7, "AppleUSBEHCI[%p]::EnablePeriodicSchedule: had to wait %d ms for CMD and STS to synch up OFF",  this, i);
			}
		}
		
		if (!stat)
		{
			_pEHCIRegisters->USBCMD |= HostToUSBLong(kEHCICMDPeriodicEnable);
			
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
AppleUSBEHCI::DisablePeriodicSchedule(void)
{
    int		i;
    IOReturn	stat = kIOReturnSuccess;
	
    if (_pEHCIRegisters->USBCMD & HostToUSBLong(kEHCICMDPeriodicEnable))
    {
		// first make certain that it really is enabled
		for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSPeriodicScheduleStatus); i++)
			IOSleep(1);
			
		if (i)
		{
			if (i >= 100)
			{
				USBLog(1, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: ERROR: USBCMD and USBSTS won't synchronize ON",  this);
				stat = kIOReturnInternalError;
			}
			else
			{
				USBLog(7, "AppleUSBEHCI[%p]::DisablePeriodicSchedule: had to wait %d ms for CMD and STS to synch up ON",  this, i);
			}
		}
		
		if (!stat)
		{
			_pEHCIRegisters->USBCMD &= HostToUSBLong(~kEHCICMDPeriodicEnable);
			
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
    cs_event_t	pccardevent;
	
    // Let our superclass decide handle this method
    // messages
    //
    if ( type == kIOPCCardCSEventMessage)
    {
        pccardevent = (UInt32) argument;
		
        if ( pccardevent == CS_EVENT_CARD_REMOVAL )
        {
            // Should return all transactions in any endpoints
            //
            USBLog(5,"AppleUSBEHCI[%p]: Received kIOPCCardCSEventMessage Need to return all transactions",this);
            _pcCardEjected = true;
        }
    }
	
    USBLog(6, "AppleUSBEHCI[%p]::message type: %p, isInactive = %d",  this, (void*)type, isInactive());
    return super::message( type, provider, argument );
	
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




