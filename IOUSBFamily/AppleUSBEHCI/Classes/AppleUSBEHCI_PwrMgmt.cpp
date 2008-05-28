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

#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <libkern/libkern.h>

#ifndef kACPIDevicePathKey
#define kACPIDevicePathKey			"acpi-path"
#endif

#include "AppleUSBEHCI.h"

// From the file Gossamer.h that is not available
enum {
    kGossamerTypeGossamer = 1,
    kGossamerTypeSilk,
    kGossamerTypeWallstreet,
    kGossamerTypeiMac,
    kGossamerTypeYosemite,
    kGossamerType101
};

// Convert USBLog to use kprintf debugging
// The switch is in the header file, but the work is done here because the header is included by the companion controllers
#if EHCI_USE_KPRINTF
#define EHCIPWRMGMT_USE_KPRINTF EHCI_USE_KPRINTF
#else
#define EHCIPWRMGMT_USE_KPRINTF 0
#endif
#if EHCIPWRMGMT_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= EHCIPWRMGMT_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#define super IOUSBControllerV3

extern UInt32 getPortSCForWriting(EHCIRegistersPtr _pEHCIRegisters, short port);

#define _controllerCanSleep				_expansionData->_controllerCanSleep

void											
AppleUSBEHCI::CheckSleepCapability(void)
{
	// *****************
	// This stuff used to be done in initForPM. I could not coalesce the two methods, but I need the _controllerCanSleep calculation
	// earlier than that time, so I will do the calculaton here instead of there
	// *****************
	// assume that sleep is OK at first
	USBLog(2, "AppleUSBEHCI[%p]::CheckSleepCapability - assuming that I can sleep", this);
	_controllerCanSleep = true;
	
	//   We need to determine which EHCI controllers don't survive sleep.  These fall into 2 categories:
	//
	//   1.  CardBus cards
	//	 2.  PCI Cards that lose power (right now because of a bug in the PCI Family, USB PCI cards do not prevent
	//	     sleep, so even cards that don't support the PCI Power Mgmt stuff get their power removed.
	//
	//  So here, we look at all those cases and set the _unloadUIMAcrossSleep boolean to true.  As it turns out,
	//  if a controller does not have the "AAPL,clock-id" property, then it means that it cannot survive sleep.  We
	//  might need to refine this later once we figure how to deal with PCI cards that can go into PCI sleep mode.
	//  An exception is the B&W G3, that does not have this property but can sleep.  Sigh...
	
	//  Deal with CardBus USB cards.  Their provider will be a "IOCardBusDevice", as opposed to a "IOPCIDevice"
	//
	_onCardBus = (0 != _device->metaCast("IOCardBusDevice"));
	//  Now, look at PCI cards.  Note that the onboard controller's provider is an IOPCIDevice so we cannot use that
	//  to distinguish between USB PCI cards and the on board controller.  Instead, we use the existence of the
	//  "AAPL,clock-id" property in the provider.  If it does not exist, then we are a EHCI controller on a USB PCI card.
	//
	if ( !_device->getProperty("AAPL,clock-id") && !((getPlatform()->getChipSetType() == kChipSetTypeGossamer) && getPlatform()->getMachineType() == kGossamerTypeYosemite) )
	{
		bool			hasSupport = false;
		
		// ICH6 is only in the Developer Transition machines, and we will assume that it can support D3Cold
		if (_errataBits & kErrataICH6PowerSequencing)
			hasSupport = _device->hasPCIPowerManagement(kPCIPMCPMESupportFromD3Cold);
		else
			hasSupport = _device->hasPCIPowerManagement();
		
		if (hasSupport)
		{
			if (_errataBits & kErrataICH6PowerSequencing)
				hasSupport = (_device->enablePCIPowerManagement(kPCIPMCPMESupportFromD3Cold) == kIOReturnSuccess);
			else
				hasSupport = (_device->enablePCIPowerManagement() == kIOReturnSuccess);
		}
		if (hasSupport)
		{
			_hasPCIPwrMgmt = true;
			setProperty("Card Type","Built-in");
		}
		else
		{
			USBLog(2, "AppleUSBEHCI[%p]::CheckSleepCapability EHCI controller will be unloaded across sleep",this);
			_controllerCanSleep = false;
			setProperty("Card Type","PCI");
		}
	}
	else
	{
		setProperty("Card Type","Built-in");
	}
	
	if ( _onCardBus )
	{
		setProperty("Card Type","CardBus");
		USBLog(1, "AppleUSBEHCI[%p]::CheckSleepCapability - i CANNOT sleep(CardBus)", this);
		_controllerCanSleep = false;
	}
	
	// if we have an ExpressCard attached (non-zero port), then we will need to disable port resume 
	// for that port (some cards disconnect when the ExpressCard power goes away and we would like to ignore these extra detach events.
	_ExpressCardPort = ExpressCardPort(_device);	
	_badExpressCardAttached = false;
}



void			
AppleUSBEHCI::ResumeUSBBus()
{
    UInt8	numPorts;
	int		i;
    bool	enabledports = false;

    // restore volatile registers saved in SuspendUSBBus()
	if (USBToHostLong(_pEHCIRegisters->ConfigFlag) != kEHCIPortRoutingBit)
	{
		USBLog(5, "AppleUSBEHCI[%p]::ResumeUSBBus - restoring ConfigFlag[from 0x%x]",  this, (unsigned int) USBToHostLong(_pEHCIRegisters->ConfigFlag));
		_pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);
		IOSync();
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			UInt32		newValue = 0, count = 0;
			newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			while ((count++ < 10) && (newValue != kEHCIPortRoutingBit))
			{
				USBError(1, "EHCI driver: ResumeUSBBus - ConfigFlag bit not sticking. Retrying.");
				_pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);
				IOSync();
				newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
			}
		}
	}
	
	_pEHCIRegisters->PeriodicListBase = _physPeriodicListBase;
	IOSync();
	
    if (_savedAsyncListAddr && (_pEHCIRegisters->AsyncListAddr != _savedAsyncListAddr))
    {
		USBLog(4, "AppleUSBEHCI[%p]::ResumeUSBBus - restoring AsyncListAddr[from 0x%x to 0x%x]",  this,  (unsigned int)USBToHostLong(_pEHCIRegisters->AsyncListAddr),  (unsigned int)USBToHostLong(_savedAsyncListAddr));
        _pEHCIRegisters->AsyncListAddr = _savedAsyncListAddr;
        IOSync();
    }

	if (_is64bit)	
	    _pEHCIRegisters->CTRLDSSegment = 0;

	// 09-15-2005 rdar://4041217
	// make sure that the HC is actually turned on (as long as it was before we suspended)
	// without this, we were resuming the ports and taking them out of resume and not sending SOF tokens
	// which was confusing lots of devices.
	if (USBToHostLong(_savedUSBCMD) & kEHCICMDRunStop)
	{
		// if the controller was running before, go ahead and turn it on now, but leave the schedules off
		_pEHCIRegisters->USBCMD |= HostToUSBLong(kEHCICMDRunStop);
		IOSync();
		for (i=0; (i< 100) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++)
			IODelay(100);
		if (i>1)
		{
			USBError(1, "AppleUSBEHCI[%p]::ResumeUSBBus - controller took (%d) turns to get going", this, i);
		}
	}
    // resume all enabled ports which we own
    numPorts = USBToHostLong(_pEHCICapRegisters->HCSParams) & kEHCINumPortsMask;
    USBLog(7, "AppleUSBEHCI[%p]::ResumeUSBBus - resuming %d ports",  this, numPorts);
    for (i=0; i < numPorts; i++)
    {
		UInt32 portStat;
		portStat = getPortSCForWriting(_pEHCIRegisters, i+1);
		if (portStat & kEHCIPortSC_Owner)
		{
			USBLog(4, "AppleUSBEHCI[%p]::ResumeUSBBus - port %d owned by OHCI",  this, i+1);
		}
		else if (portStat & kEHCIPortSC_Enabled)
		{
			// is this an ExpressCard port that we disabled resume enable?  - if so, put it back!
			if (_badExpressCardAttached && ((int)_ExpressCardPort == (i+1)))
			{
				portStat |= (kEHCIPortSC_WKCNNT_E|kEHCIPortSC_WKDSCNNT_E);
				_pEHCIRegisters->PortSC[i] = USBToHostLong(portStat);
				IOSync();
			}

			// I used to resume any ports I had previously suspended here. Now this is handled by the hub driver
			// which will issue the suspends before sleep and the resumes on wakeup.
			enabledports = true;
		}
		else
		{
			USBLog(7, "AppleUSBEHCI[%p]::ResumeUSBBus - port %d not enabled",  this, i);
		}
    }
        
    if (_savedUSBCMD)
    {
		USBLog(5, "AppleUSBEHCI[%p]::ResumeUSBBus - USBCMD is <%p> will be <%p>",  this, (void*)_pEHCIRegisters->USBCMD, (void*)_savedUSBCMD);
		_pEHCIRegisters->USBCMD = _savedUSBCMD;
    }
	
}



void			
AppleUSBEHCI::SuspendUSBBus()
{
    UInt8	numPorts;
    int		i;
    UInt32	usbcmd, usbsts;
    
	// Initialize our suspended bitmap to no ports suspended
	_savedSuspendedPortBitmap = 0;
	
    // save the USBCMD register before disabling the list processing
    _savedUSBCMD = _pEHCIRegisters->USBCMD;
    USBLog(7, "AppleUSBEHCI[%p]::SuspendUSBBus - got _savedUSBCMD <%p>",  this, (void*)_savedUSBCMD);
    
	// disable list processing
    usbcmd = USBToHostLong(_savedUSBCMD);
	

	// first make sure that the status register matches the command register
    if (usbcmd & kEHCICMDAsyncEnable)
	{
		// if the status is currently off, we are waiting for it to come on from some previous event - this 
		// should really never happen
		for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSAsyncScheduleStatus); i++)
			IODelay(100);
		if (i)
		{
			USBError(1, "AppleUSBEHCI[%p]::SuspendUSBBus - Async Schedule should have been on but was off for %d loops", this, i);
		}
		usbcmd &= ~kEHCICMDAsyncEnable;
	}
    if (usbcmd & kEHCICMDPeriodicEnable)
	{
		// if the status is currently off, we are waiting for it to come on from some previous event - this 
		// should really never happen
		for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSPeriodicScheduleStatus); i++)
			IODelay(100);

		if (i)
		{
			USBError(1, "AppleUSBEHCI[%p]::SuspendUSBBus - Periodic Schedule should have been on but was off for %d loops", this, i);
		}
		usbcmd &= ~kEHCICMDPeriodicEnable;
	}
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);			// This will turn off both schedules if they aren't already off
    IOSync();
    
	// 09-15-2005 rdar://4041217
	// make sure that the lists are actually off before we start suspending the ports
	// otherwise, we end up trying to process a TD to a suspended (and blocked) port
	// which causes an incorrect error
	for (i=0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSAsyncScheduleStatus); i++)
		IODelay(1000);
		
	if (i > 2)
	{
		USBError(1, "AppleUSBEHCI[%p]::SuspendUSBBus - Async Schedule took %d loops to turn off", this, i);
	}


	for (i=0; (i < 1000) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSPeriodicScheduleStatus); i++)
		IODelay(1000);
		
	if (i > 2)
	{
		USBError(1, "AppleUSBEHCI[%p]::SuspendUSBBus - Periodic Schedule took %d loops to turn off CMD(%p) STS(%p)", this, i, (void*)USBToHostLong(_pEHCIRegisters->USBCMD), (void*)USBToHostLong(_pEHCIRegisters->USBSTS));
	}

	// save these registers per Intel recommendations - but do it AFTER the scheduling has stopped
    _savedAsyncListAddr = _pEHCIRegisters->AsyncListAddr;
	
    // this used to suspend all enabled and not already suspended ports
	// now, we note whether a port was not suspended, but they all should have been suspended by the root hub driver at this point
	// we also check for bad express cards
    GetNumberOfPorts( &numPorts );
    USBLog(numPorts ? 4 : 1, "AppleUSBEHCI[%p]::SuspendUSBBus - suspending %d ports",  this, numPorts);
    for (i=0; i < numPorts; i++)
    {
		UInt32 portStat;
		portStat = getPortSCForWriting(_pEHCIRegisters, i+1);
		if (portStat & kEHCIPortSC_Owner)
		{
			USBLog(4, "AppleUSBEHCI[%p]::SuspendUSBBus - port %d owned by OHCI",  this, i+1);
		}
		else if (portStat & kEHCIPortSC_Enabled)
		{
			// is this an ExpressCard port that needs to turn off resume enable?
			if (_badExpressCardAttached && ((int)_ExpressCardPort == (i+1)))
			{
				portStat &= ~(kEHCIPortSC_WKCNNT_E|kEHCIPortSC_WKDSCNNT_E);
				_pEHCIRegisters->PortSC[i] = USBToHostLong(portStat);
				IOSync();
			}
			
			if (portStat & kEHCIPortSC_Suspend)
			{
				_savedSuspendedPortBitmap |= (1<<i);
				USBLog(4, "AppleUSBEHCI[%p]::SuspendUSBBus - port %d was already suspended (as it should have been)",  this, i+1);
			}
			else
			{
				USBError(1, "AppleUSBEHCI[%p]::SuspendUSBBus - port %d was NOT already suspended (as it should have been) PROBLEMS AHEAD",  this, i+1);
				// EHCIRootHubPortSuspend(i+1, true);
			}
		}
		else
		{
			USBLog(4, "AppleUSBEHCI[%p]::SuspendUSBBus - port %d not enabled",  this, i+1);
		}
    }

    // clear run/stop
    usbcmd &= ~kEHCICMDRunStop;
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);
    IOSync();
    _myBusState = kUSBBusStateReset;
    USBLog(5, "AppleUSBEHCI[%p]::SuspendUSBBus - ports suspended, HC stop set, waiting for halted - USBCMD(%p) USBSTS(%p)",  this, (void*)USBToHostLong(_pEHCIRegisters->USBCMD), (void*)USBToHostLong(_pEHCIRegisters->USBSTS));
    
	i=0;
	
    // wait for halted bit
    do
    {
		if ((++i % 10000) == 0)
		{ 
			USBLog(1, "AppleUSBEHCI[%p]::SuspendUSBBus - HC not halting! USBCMD(%p) USBSTS(%p) i(%d)", this, (void*)USBToHostLong(_pEHCIRegisters->USBCMD), (void*)USBToHostLong(_pEHCIRegisters->USBSTS), (int)i);
		}
		usbsts = USBToHostLong(_pEHCIRegisters->USBSTS);
    } while (!(usbsts & kEHCIHCHaltedBit));
    
    USBLog(5, "AppleUSBEHCI[%p]::SuspendUSBBus - HC halted",  this);
}



void			
AppleUSBEHCI::StopUSBBus()
{
    UInt32	usbcmd;
    

    usbcmd = USBToHostLong(_pEHCIRegisters->USBCMD);
    // clear run/stop
    usbcmd &= ~kEHCICMDRunStop;
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);
    _myBusState = kUSBBusStateReset;
    USBLog(5, "AppleUSBEHCI[%p]::StopUSBBus - HC halted",  this);
}



void			
AppleUSBEHCI::RestartUSBBus()
{
    UInt32	usbcmd, usbsts;
    // wait for halted bit
    do
    {
		usbsts = USBToHostLong(_pEHCIRegisters->USBSTS);
    } while (!(usbsts & kEHCIHCHaltedBit));
    usbcmd = USBToHostLong(_pEHCIRegisters->USBCMD);
    // set run/stop
    usbcmd |= kEHCICMDRunStop;
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);
    _myBusState = kUSBBusStateRunning;
    USBLog(5, "AppleUSBEHCI[%p]::RestartUSBBus - HC restarted",  this);
}



static IOACPIPlatformDevice * CopyACPIDevice( IORegistryEntry * device )
{
	IOACPIPlatformDevice *  acpiDevice = 0;
	OSString *				acpiPath;

	if (device)
	{
		acpiPath = (OSString *) device->copyProperty(kACPIDevicePathKey);
		if (acpiPath && !OSDynamicCast(OSString, acpiPath))
		{
			acpiPath->release();
			acpiPath = 0;
		}

		if (acpiPath)
		{
			IORegistryEntry * entry;

			entry = IORegistryEntry::fromPath(acpiPath->getCStringNoCopy());
			acpiPath->release();

			if (entry && entry->metaCast("IOACPIPlatformDevice"))
				acpiDevice = (IOACPIPlatformDevice *) entry;
			else if (entry)
				entry->release();
		}
	}

	return (acpiDevice);
}

static bool HasExpressCardUSB( IORegistryEntry * acpiDevice, UInt32 * portnum )
{
	const IORegistryPlane *	acpiPlane;
	bool					match = false;
	IORegistryIterator *	iter;
	IORegistryEntry *		entry;

	do {
		acpiPlane = acpiDevice->getPlane( "IOACPIPlane" );
		if (!acpiPlane)
			break;

		// acpiDevice is the USB controller in ACPI plane.
		// Recursively iterate over children of acpiDevice.

		iter = IORegistryIterator::iterateOver(
				/* start */	acpiDevice,
				/* plane */	acpiPlane,
				/* options */ kIORegistryIterateRecursively);

		if (iter)
		{
			while (!match && (entry = iter->getNextObject()))
			{
				// USB port must be a leaf node (no child), and
				// must be an IOACPIPlatformDevice.

				if ((entry->getChildEntry(acpiPlane) == 0) &&
					entry->metaCast("IOACPIPlatformDevice"))
				{
					IOACPIPlatformDevice * port;
					port = (IOACPIPlatformDevice *) entry;

					// Express card port? Is port ejectable?

					if (port->validateObject( "_EJD" ) == kIOReturnSuccess)
					{
						// Determining the USB port number.
						if (portnum)
							*portnum = strtoul(port->getLocation(), NULL, 10);
						match = true;
					}
				}
			}

			iter->release();
		}
	}
	while (false);
	
	return match;
}

// Checks for ExpressCard connected to this controller, and returns the port number (1 based)
// Will return 0 if no ExpressCard is connected to this controller.
//
UInt32 
AppleUSBEHCI::ExpressCardPort( IOService * provider )
{
	IOACPIPlatformDevice *	acpiDevice;
	UInt32					portNum = 0;
	bool					isPCIeUSB;
	
	acpiDevice = CopyACPIDevice( provider );
	if (acpiDevice)
	{
		isPCIeUSB = HasExpressCardUSB( acpiDevice, &portNum );	
		acpiDevice->release();
	}
	return(portNum);
}



IOReturn
AppleUSBEHCI::powerStateDidChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	USBLog(5, "AppleUSBEHCI[%p]::powerStateDidChangeTo - stateNumber(%d)", this, (int)stateNumber);
	//еее LastRootHubPortStatusChanged(true);
	
	// if we are changing TO doze and FROM sleep, then we need to OR in the root hub port change bit into the saved interrupts
	// otherwise, when the superclass restores the interrupts, it will replace it
	if ((_myPowerState == kUSBPowerStateSleep) && (stateNumber == kUSBPowerStateLowPower))
	{
		_savedUSBIntr = _savedUSBIntr | HostToUSBLong(kEHCIPortChangeIntBit);
		USBLog(5, "AppleUSBEHCI[%p]::powerStateDidChangeTo - added port change bit to _savedUSBIntr - now (%p)", this, (void*)_savedUSBIntr);
	}
	return super::powerStateDidChangeTo(capabilities, stateNumber, whatDevice);
}



IOReturn				
AppleUSBEHCI::SaveControllerStateForSleep(void)
{
	showRegisters(7, "+SaveControllerStateForSleep");

	// interrupts have been diabled by the superclass
	
	USBLog(5, "AppleUSBEHCI[%p]::SaveControllerStateForSleep - suspending the bus",  this);
	
	// Make sure that we have the USB Bus running before we go into suspend
	if (_myBusState < kUSBBusStateRunning)
	{
		USBLog(5, "AppleUSBEHCI[%p]::SaveControllerStateForSleep - _myBusState < kUSBBusStateRunning -  restarting USB before suspending", this);
		RestartUSBBus();
	}
	USBLog(7, "AppleUSBEHCI[%p]::SaveControllerStateForSleep - about to suspend bus - showing queue", this);
	printAsyncQueue(7, "SaveControllerStateForSleep");
	SuspendUSBBus();
	
	USBLog(7, "AppleUSBEHCI[%p]::SaveControllerStateForSleep The bus is now suspended - showing queue", this);
	printAsyncQueue(7, "SaveControllerStateForSleep");

	_myBusState = kUSBBusStateSuspended;

	showRegisters(7, "-SaveControllerStateForSleep");

	return kIOReturnSuccess;
}



IOReturn				
AppleUSBEHCI::RestoreControllerStateFromSleep(void)
{
	USBLog(5, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep - setPowerState powering on USB",  this);
	
	showRegisters(7, "+RestoreControllerStateFromSleep");

	// at this point, interrupts are disabled, and we are waking up. If the Port Change interrupt is active
	// then it is likely that we are responsible for the system issuing the wakeup
	if (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIPortChangeIntBit)
	{
		UInt32			port, numPorts;
		
		IOLog("\nUSB caused wake event (EHCI)\n");
		kprintf("\nUSB caused wake event (EHCI)\n");
	}
	
	USBLog(7, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep - about to resume bus - showing queue", this);
	printAsyncQueue(7, "RestoreControllerStateFromSleep");
	ResumeUSBBus();
	USBLog(7, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep - bus has been resumed - showing queue", this);
	printAsyncQueue(7, "RestoreControllerStateFromSleep");
	_myBusState = kUSBBusStateRunning;
	
	showRegisters(7, "-RestoreControllerStateFromSleep");
	
	// interrupts will be enabled in the superclass
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::ResetControllerState(void)
{
	int			i;

	USBLog(5, "AppleUSBEHCI[%p]::ResetControllerState - powering down USB - _pEHCIRegisters(%p) _pEHCICapRegisters(%p) PCIConfigCommand(%p)",  this, _pEHCIRegisters, _pEHCICapRegisters, (void*)_device->configRead16(kIOPCIConfigCommand));
	showRegisters(2, "+ResetControllerState");

	// interrupts were disabled in the superclass
	// stop the controller
	_pEHCIRegisters->USBCMD = 0;  			// this sets r/s to stop
	IOSync();
	
	_myBusState = kUSBBusStateReset;
	for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++)
		IOSleep(1);
	if (i >= 100)
	{
		USBError(1, "AppleUSBEHCI[%p]::ResetControllerState - could not get chip to halt within 100 ms",  this);
		return kIOReturnInternalError;
	}
	

	// make sure to save the old base registers
	if (_pEHCIRegisters->AsyncListAddr)
	{
		_savedAsyncListAddr = _pEHCIRegisters->AsyncListAddr;
		USBLog(5, "AppleUSBEHCI[%p]::ResetControllerState - got _savedAsyncListAddr(%p)",  this, (void*)_savedAsyncListAddr);
	}
	
	_pEHCIRegisters->PeriodicListBase = 0;		// no periodic list as yet
	_pEHCIRegisters->AsyncListAddr = 0;			// no async list as yet
	IOSync();
	_uimInitialized = false;
	
	showRegisters(2, "-ResetControllerState");
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::RestartControllerFromReset(void)
{
	int				i;
	UInt32			USBCmd;

	USBLog(5, "AppleUSBEHCI[%p]::RestartControllerFromReset - restarting USB _uimInitialized(%s) _savedAsyncListAddr(%p)",  this, _uimInitialized ? "yes" : "no", (void*)_savedAsyncListAddr);
 
	if (!_uimInitialized)
	{
		// set the reset bit, but only if we are not currently initialized
		//
		_pEHCIRegisters->USBCMD = HostToUSBLong(kEHCICMDHCReset);		
		IOSync();

		for ( i = 0; (i < 100) && (USBToHostLong(_pEHCIRegisters->USBCMD) & kEHCICMDHCReset); i++ )
			IOSleep(1);
		
		if ( i >= 100 )
		{
			USBError(1, "AppleUSBEHCI[%p]::RestartControllerFromReset - could not get chip to come out of reset within 100 ms",  this);
			return kIOReturnInternalError;
		}
		
	}
	// restore lists
	_pEHCIRegisters->PeriodicListBase = _physPeriodicListBase;
	IOSync();
	
	if (_pEHCIRegisters->AsyncListAddr != _savedAsyncListAddr)
	{
		USBLog(5, "AppleUSBEHCI[%p]::RestartControllerFromReset - restoring AsyncListAddr[from 0x%lx to 0x%lx]",  this, _pEHCIRegisters->AsyncListAddr, _savedAsyncListAddr);
		_pEHCIRegisters->AsyncListAddr = _savedAsyncListAddr;
		IOSync();
	}
		
	USBCmd = USBToHostLong(_pEHCIRegisters->USBCMD);
	
	// check to make sure we have the correct Frame List Size
	//
	_frameListSize = (USBCmd & kEHCICMDFrameListSizeMask) >> kEHCICMDFrameListSizeOffset;
	if (_frameListSize)
	{
		// at this time we only support a 1K frame list
		//
		USBError(1, "AppleUSBEHCI[%p]::RestartControllerFromReset - bad _frameListSize",  this);
		return kIOReturnInternalError;
	}
	
	_frameListSize = 1024;
	USBCmd |= kEHCICMDRunStop;
	_myBusState = kUSBBusStateRunning;

	USBCmd &= ~kEHCICMDIntThresholdMask;
	USBCmd |= 1 << kEHCICMDIntThresholdOffset;						// Interrupt every micro frame as needed (4745296)
	_pEHCIRegisters->USBCMD = HostToUSBLong(USBCmd);				// start your engines
	
	_pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);  		// Route ports to EHCI
	IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
		while ((count++ < 10) && (newValue != kEHCIPortRoutingBit))
		{
			USBError(1, "EHCI driver: RestartControllerFromReset - ConfigFlag bit not sticking. Retrying.");
			_pEHCIRegisters->ConfigFlag = HostToUSBLong(kEHCIPortRoutingBit);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->ConfigFlag);
		}
	}
	
	// interrupts will be turned on in the superclass by calling in to the enable routine below.
	// get ready for that
	_savedUSBIntr = HostToUSBLong(kEHCICompleteIntBit | kEHCIErrorIntBit | kEHCIHostErrorIntBit | kEHCIFrListRolloverIntBit | kEHCIPortChangeIntBit);	
	
	// Should revisit the Isoc bandwidth issue sometime.
	//
	_isochBandwidthAvail = 5 *1024;
	_outSlot = kEHCIPeriodicListEntries+1;	/* No Isoc transactions currently. */
	_frameNumber = 0;

	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::DozeController(void)
{
	DisableAsyncSchedule(true);
	DisablePeriodicSchedule(true);
	StopUSBBus();
	
	if (_myPowerState == kUSBPowerStateOn)
	{
		// enable the port change interrupt
		_pEHCIRegisters->USBIntr = _pEHCIRegisters->USBIntr | HostToUSBLong(kEHCIPortChangeIntBit);
		IOSync();
	}
		
	return kIOReturnSuccess;
}



IOReturn				
AppleUSBEHCI::WakeControllerFromDoze(void)
{
	RestartUSBBus();
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::EnableInterruptsFromController(bool enable)
{
	if (enable)
	{
		USBLog(5, "AppleUSBEHCI[%p]::EnableInterruptsFromController - enabling interrupts, USBIntr(%p) _savedUSBIntr(%p)", this, (void*)_pEHCIRegisters->USBIntr, (void*)HostToUSBLong(_savedUSBIntr));
		_pEHCIRegisters->USBIntr = _savedUSBIntr;
		IOSync();
		_savedUSBIntr = 0;
	}
	else
	{
		_savedUSBIntr = _pEHCIRegisters->USBIntr;									// save currently enabled interrupts
		_pEHCIRegisters->USBIntr = HostToUSBLong(kEHCIFrListRolloverIntBit);		// disable all interrupts except frame rollover which can be handled in the HW Int routine
		IOSync();
		USBLog(5, "AppleUSBEHCI[%p]::EnableInterruptsFromController - interrupts disabled, _savedUSBIntr(%p)", this, (void*)_savedUSBIntr);
	}
	
	return kIOReturnSuccess;
}



void
AppleUSBEHCI::powerChangeDone ( unsigned long fromState)
{
	unsigned long newState = getPowerState();
	
	USBLog((fromState == newState) ? 7 : 4, "AppleUSBEHCI[%p]::powerChangeDone from state (%d) to state (%d) _controllerAvailable(%s)", this, (int)fromState, (int)newState, _controllerAvailable ? "true" : "false");
	if (_wakingFromHibernation)
	{
		USBLog(2, "AppleUSBEHCI[%p]::powerChangeDone - _wakingFromHibernation - _savedAsyncListAddr(%p) AsyncListAddr(%p)", this, (void*)USBToHostLong(_savedAsyncListAddr), (void*)_pEHCIRegisters->AsyncListAddr);		
		_savedAsyncListAddr = _pEHCIRegisters->AsyncListAddr;
	}
	if (_controllerAvailable)
		showRegisters(7, "powerChangeDone");
		
	super::powerChangeDone(fromState);
}
