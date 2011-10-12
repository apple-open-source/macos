/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright й 1998-2011 Apple Inc.  All rights reserved.
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


//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/IOTimerEventSource.h>

#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>
#include <IOKit/usb/IOUSBLog.h>
#include <libkern/libkern.h>

#include "AppleUSBEHCI.h"
#include "USBTracepoints.h"

//================================================================================================
//
//   Globals
//
//================================================================================================
//
// Declare a statically-initialized instance of the class so that its constructor will be called on driver load 
// and its destructor will be called on unload.
static class AppleUSBEHCI_IOLockClass gEHCI_GlobalLock;

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBControllerV3

#define _controllerCanSleep				_expansionData->_controllerCanSleep

// From the file Gossamer.h that is not available
enum {
    kGossamerTypeGossamer = 1,
    kGossamerTypeSilk,
    kGossamerTypeWallstreet,
    kGossamerTypeiMac,
    kGossamerTypeYosemite,
    kGossamerType101
};


//================================================================================================
//
//   kprintf logging
//
// Convert USBLog to use kprintf debugging
// The switch is in the header file, but the work is done here because the header is included by the companion controllers
//
//================================================================================================
//
#if EHCI_USE_KPRINTF
#define EHCIPWRMGMT_USE_KPRINTF EHCI_USE_KPRINTF
#else
#define EHCIPWRMGMT_USE_KPRINTF 0
#endif

#if EHCIPWRMGMT_USE_KPRINTF
#undef USBLog
#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= EHCIPWRMGMT_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif


//================================================================================================
//
//   externals
//
//================================================================================================
//
extern UInt32 getPortSCForWriting(EHCIRegistersPtr _pEHCIRegisters, short port);


//================================================================================================
//
//   AppleUSBEHCI_IOLockClass methods
//
//================================================================================================
//
AppleUSBEHCI_IOLockClass::AppleUSBEHCI_IOLockClass() 
{
	lock = IOLockAlloc();
}

AppleUSBEHCI_IOLockClass::~AppleUSBEHCI_IOLockClass() 
{
	IOLockFree(lock);
}		


//================================================================================================
//
//   CheckSleepCapability
//
//================================================================================================
//
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
	_hasPCIPwrMgmt = false;
	
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
	
	//  Now, look at PCI cards.  Note that the onboard controller's provider is an IOPCIDevice so we cannot use that
	//  to distinguish between USB PCI cards and the on board controller.  Instead, we use the existence of the
	//  "AAPL,clock-id" property in the provider.  If it does not exist, then we are a EHCI controller on a USB PCI card.
	//
	if ( !_device->getProperty("AAPL,clock-id") && !((getPlatform()->getChipSetType() == kChipSetTypeGossamer) && getPlatform()->getMachineType() == kGossamerTypeYosemite) )
	{
		if (_device->getProperty("built-in"))
		{
			if (_errataBits & kErrataNECIncompleteWrite)
			{
				FixupNECControllerConfigRegisters();
			}
			
			// rdar://5769508 - if we are on a built in PCI device, then assume the system supports D3cold
			if (_device->hasPCIPowerManagement(kPCIPMCPMESupportFromD3Cold) && (_device->enablePCIPowerManagement(kPCIPMCSPowerStateD3) == kIOReturnSuccess))
			{
				_hasPCIPwrMgmt = true;
				setProperty("Card Type","Built-in");
			}
		}
		else
		{
			// rdar://5856545 - on older machines without the built-in property, we need to use the "default" case in the IOPCIDevice code
			if (_device->hasPCIPowerManagement() && (_device->enablePCIPowerManagement() == kIOReturnSuccess))
			{
				_hasPCIPwrMgmt = true;
				setProperty("Card Type","Built-in");
			}
		}
		
        if (!_hasPCIPwrMgmt)
        {
            USBError(1, "AppleUSBEHCI[%p]::CheckSleepCapability - controller will be unloaded across sleep",this);
            _controllerCanSleep = false;
            setProperty("Card Type","PCI");
        }
	}
	else
	{
		// old Apple ASICs come in here
		setProperty("Card Type","Built-in");
	}
		
	// if we have an ExpressCard attached (non-zero port), then we will need to disable port resume 
	// for that port (some cards disconnect when the ExpressCard power goes away and we would like to ignore these extra detach events.
	_ExpressCardPort = ExpressCardPort(_device);	
	_badExpressCardAttached = false;
}


//================================================================================================
//
//   ResumeUSBBus
//
//================================================================================================
//
void			
AppleUSBEHCI::ResumeUSBBus()
{
	int		i;
    bool	enabledports = false;

	USBTrace( kUSBTEHCI, kTPEHCIResumeUSBBus, (uintptr_t)this, 0, 0, 0);
	
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
		UInt32	USBCmd = USBToHostLong(_pEHCIRegisters->USBCMD);
		
		// rdar://7315326 - Make sure that the threshold is set back to what we want it.. Some controllers appear to change it during sleep
		USBCmd &= ~kEHCICMDIntThresholdMask;
		USBCmd |= 1 << kEHCICMDIntThresholdOffset;						// Interrupt every micro frame as needed (4745296)

		// same with Async Park Mode
		// get rid of the count as well as the enable bit
		//USBCmd &= ~kEHCICMDAsyncParkModeCountMask;
		
		// this would allow a different count if we stayed enabled
		// USBCmd |= (2 << kEHCICMDAsyncParkModeCountMaskPhase);
		
		// this line will eliminate park mode completely
		//USBCmd &= ~kEHCICMDAsyncParkModeEnable;
		USBCmd |= kEHCICMDRunStop;
		
		USBLog(5, "AppleUSBEHCI[%p]::ResumeUSBBus - initial restart - USBCMD is <%p> will be <%p>",  this, (void*)USBToHostLong(_pEHCIRegisters->USBCMD), (void*)USBCmd);
		_pEHCIRegisters->USBCMD = HostToUSBLong(USBCmd);
		IOSync();
		
		for (i=0; (i< 100) && (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++)
			IODelay(100);
		if (i>1)
		{
			USBLog(2, "AppleUSBEHCI[%p]::ResumeUSBBus - controller took (%d) turns to get going", this, i);
		}
	}
    // resume all enabled ports which we own

    USBLog(7, "AppleUSBEHCI[%p]::ResumeUSBBus - resuming %d ports",  this, _rootHubNumPorts);
    for (i=0; i < _rootHubNumPorts; i++)
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


//================================================================================================
//
//   SuspendUSBBus
//
//================================================================================================
//
void			
AppleUSBEHCI::SuspendUSBBus()
{
    UInt8	numPorts;
    int		i;
    UInt32	usbcmd, usbsts;
    
	USBTrace_Start( kUSBTEHCI, kTPEHCISuspendUSBBus, (uintptr_t)this, 0, 0, 0);
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
    USBLog(numPorts ? 4 : 1, "AppleUSBEHCI[%p]::SuspendUSBBus - bus 0x%x, suspending %d ports",  this, (uint32_t)_busNumber, numPorts);
    for (i=0; i < numPorts; i++)
    {
		UInt32 portStat;
		portStat = getPortSCForWriting(_pEHCIRegisters, i+1);
		if (portStat & kEHCIPortSC_Owner)
		{
			USBLog(4, "AppleUSBEHCI[%p]::SuspendUSBBus - port %d owned by OHCI",  this, i+1);
		}
		else
		{
			// is this an ExpressCard port that needs to turn off resume enable?
			if (_badExpressCardAttached && ((int)_ExpressCardPort == (i+1)))
			{
				USBLog(4, "AppleUSBEHCI[%p]::SuspendUSBBus - port %d had an ExpresCard in it",  this, i+1);
				portStat &= ~(kEHCIPortSC_WKCNNT_E|kEHCIPortSC_WKDSCNNT_E);
				_pEHCIRegisters->PortSC[i] = USBToHostLong(portStat);
				IOSync();
			}
			
			if (portStat & kEHCIPortSC_Enabled)
			{
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
			USBTrace( kUSBTEHCI, kTPEHCISuspendUSBBus, (uintptr_t)this, USBToHostLong(_pEHCIRegisters->USBCMD), USBToHostLong(_pEHCIRegisters->USBSTS), (int)i);
		}
		usbsts = USBToHostLong(_pEHCIRegisters->USBSTS);
    } while (!(usbsts & kEHCIHCHaltedBit));
    
    USBLog(5, "AppleUSBEHCI[%p]::SuspendUSBBus - HC halted",  this);
}


//================================================================================================
//
//   StopUSBBus
//
//================================================================================================
//
void			
AppleUSBEHCI::StopUSBBus()
{
    UInt32	usbcmd;
    
	USBTrace( kUSBTEHCI, kTPEHCIStopUSBBus, (uintptr_t)this, 0, 0, 0);

    usbcmd = USBToHostLong(_pEHCIRegisters->USBCMD);
    // clear run/stop
    usbcmd &= ~kEHCICMDRunStop;
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);
    _myBusState = kUSBBusStateReset;
    USBLog(5, "AppleUSBEHCI[%p]::StopUSBBus - HC halted",  this);
}



//================================================================================================
//
//   RestartUSBBus
//
//================================================================================================
//
void			
AppleUSBEHCI::RestartUSBBus()
{
	UInt32			usbcmd, usbsts;
 
	USBTrace( kUSBTEHCI, kTPEHCIRestartUSBBus, (uintptr_t)this, 0, 0, 0);
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


//================================================================================================
//
//   SaveControllerStateForSleep
//
//================================================================================================
//
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
	printAsyncQueue(7, "SaveControllerStateForSleep", true, false);
	SuspendUSBBus();
	
	USBLog(7, "AppleUSBEHCI[%p]::SaveControllerStateForSleep The bus is now suspended - showing queue", this);
	printAsyncQueue(7, "SaveControllerStateForSleep", true, false);

	_myBusState = kUSBBusStateSuspended;

	showRegisters(7, "-SaveControllerStateForSleep");
	return kIOReturnSuccess;
}


//================================================================================================
//
//   RestoreControllerStateFromSleep
//
//================================================================================================
//
IOReturn				
AppleUSBEHCI::RestoreControllerStateFromSleep(void)
{
	UInt32			sts = USBToHostLong(_pEHCIRegisters->USBSTS);
	
	// if I am trying to restore from sleep and the controller has gone away, then I will just return success (which allows the power change to complete)
	// and wait for a termination
	
	if (sts == kEHCIInvalidRegisterValue)
	{
		_controllerAvailable = false;
		return kIOReturnSuccess;
	}
	
	USBLog(5, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep - setPowerState powering on USB",  this);
	
	showRegisters(7, "+RestoreControllerStateFromSleep");

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		FixupNECControllerConfigRegisters();
	}

	// at this point, interrupts are disabled, and we are waking up. If the Port Change interrupt is active
	// then it is likely that we are responsible for the system issuing the wakeup
	if (( sts & kEHCIPortChangeIntBit) || (_errataBits & kErrataMissingPortChangeInt))
	{
		UInt32			port;
		
		// just have this be done per port..
		// IOLog("\nUSB caused wake event (EHCI @ 0x%x)\n", (uint32_t)_busNumber);

		for (port=0; port < _rootHubNumPorts; port++)
		{
			UInt32	portSC = USBToHostLong(_pEHCIRegisters->PortSC[port]);
			if (portSC & kEHCIPortSC_ConnectChange)
			{
				if (portSC & kEHCIPortSC_Enabled)
				{
					USBError(1, "USB (EHCI):Port %d on bus 0x%x - connect status changed but still enabled. clearing enable bit: portSC(%p)\n", (int)port+1, (uint32_t)_busNumber, (void*)portSC);
					portSC = getPortSCForWriting(_pEHCIRegisters, port+1);
					portSC &= ~kEHCIPortSC_Enabled;
					_pEHCIRegisters->PortSC[port] = HostToUSBLong(portSC);
				}
				else
				{
					IOLog("USB (EHCI):Port %d on bus 0x%x connected or disconnected: portSC(%p)\n", (int)port+1, (uint32_t)_busNumber, (void*)portSC);
					USBLog(5, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep  Port %d on bus 0x%x - connected or disconnected, calling EnsureUsability()", this, (int)port+1, (uint32_t)_busNumber);
					EnsureUsability();
				}
			}
			else if (portSC & kEHCIPortSC_Resume)
			{

				USBLog(5, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep  Port %d on bus 0x%x - has remote wakeup from some device", this, (int)port+1, (uint32_t)_busNumber);
				
				// because of how EHCI works, the root hub driver might not be able to detect that there was a remote wakeup 
				// on a port if the upper level driver issues a Resume before the root hub interrupt timer runs
				// Let the hub driver know that from here to make sure we get the log
				
				if (_rootHubDevice && _rootHubDevice->GetPolicyMaker())
				{
					_rootHubDevice->GetPolicyMaker()->message(kIOUSBMessageRootHubWakeEvent, this, (void *)(uintptr_t) port);
				}
				else
				{
					IOLog("\tUSB (EHCI):Port %d on bus 0x%x has remote wakeup from some device\n", (int)port+1, (uint32_t)_busNumber);
				}
				
				
				if ((_errataBits & kErrataMissingPortChangeInt) && !_portChangeInterrupt && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIPortChangeIntBit))
				{
					USBLog(2, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep - reclaiming missing Port Change interrupt for port %d",  this, (int)port+1);
					_portChangeInterrupt = kEHCIPortChangeIntBit;
				}
			}
			else if ((portSC & kEHCIPortSC_Enabled) && !(portSC & kEHCIPortSC_Suspend))
			{
				USBError(1, "USB (EHCI):Port %d on bus 0x%x - port enabled but not suspended.  clearing enable bit: portSC(%p)\n", (int)port+1, (uint32_t)_busNumber, (void*)portSC);
				portSC = getPortSCForWriting(_pEHCIRegisters, port+1);
				portSC &= ~kEHCIPortSC_Enabled;
				_pEHCIRegisters->PortSC[port] = HostToUSBLong(portSC);
			}
		}
	}
	
	USBLog(7, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep - about to resume bus - showing queue", this);
	printAsyncQueue(7, "RestoreControllerStateFromSleep", true, false);
	ResumeUSBBus();
	USBLog(7, "AppleUSBEHCI[%p]::RestoreControllerStateFromSleep - bus has been resumed - showing queue", this);
	printAsyncQueue(7, "RestoreControllerStateFromSleep", true, false);
	_myBusState = kUSBBusStateRunning;
	
	showRegisters(7, "-RestoreControllerStateFromSleep");
	
	return kIOReturnSuccess;
}


//================================================================================================
//
//   ResetControllerState
//
//================================================================================================
//
IOReturn
AppleUSBEHCI::ResetControllerState(void)
{
	int				i;
	UInt32			asyncListAddr = _pEHCIRegisters->AsyncListAddr;				// it is OK to read this in HC order endian-wise

	if (asyncListAddr == kEHCIInvalidRegisterValue)
	{
		_controllerAvailable = false;
		USBLog(1, "AppleUSBEHCI[%p]::ResetControllerState - registers have gone invalid", this);
	}
	else
	{
		USBTrace( kUSBTEHCI, kTPEHCIResetControllerState, (uintptr_t)this, 0, 0, 0);
		USBLog(5, "AppleUSBEHCI[%p]::ResetControllerState - powering down USB - _pEHCIRegisters(%p) _pEHCICapRegisters(%p) PCIConfigCommand(%p)",  this, _pEHCIRegisters, _pEHCICapRegisters, (void*)_device->configRead16(kIOPCIConfigCommand));
		showRegisters(2, "+ResetControllerState");

		// interrupts were disabled in the superclass
		// stop the controller
		_pEHCIRegisters->USBCMD = 0;  			// this sets r/s to stop
		IOSync();
		
		_myBusState = kUSBBusStateReset;
		
		// if USBSTS comes back 0xffffffff, this loop will drop out because the halted bit will be set
		for (i=0; (i < 100) && !(USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIHCHaltedBit); i++)
			IOSleep(1);
		
		if (i >= 100)
		{
			USBError(1, "AppleUSBEHCI[%p]::ResetControllerState - could not get chip to halt within 100 ms",  this);
			return kIOReturnInternalError;
		}
		

		asyncListAddr = _pEHCIRegisters->AsyncListAddr;
		if (asyncListAddr == kEHCIInvalidRegisterValue)
		{
			_controllerAvailable = false;
			USBLog(1, "AppleUSBEHCI[%p]::ResetControllerState - registers have gone invalid", this);
		}
		else
		{
				// make sure to save the old base registers
			if (asyncListAddr)
			{
				_savedAsyncListAddr = asyncListAddr;
				USBLog(5, "AppleUSBEHCI[%p]::ResetControllerState - got _savedAsyncListAddr(%p)",  this, (void*)_savedAsyncListAddr);
			}
			
			_pEHCIRegisters->PeriodicListBase = 0;		// no periodic list as yet
			_pEHCIRegisters->AsyncListAddr = 0;			// no async list as yet
			IOSync();
			
			USBLog(5, "AppleUSBEHCI[%p]::ResetControllerState - reseting saved status for %d root hub ports",  this, (int)_rootHubNumPorts);
			for (i=0; i < _rootHubNumPorts; i++)
			{
				_rhPrevStatus[i] = 0;
			}
			_uimInitialized = false;
			
			showRegisters(2, "-ResetControllerState");
		}
	}
	return kIOReturnSuccess;
}


//================================================================================================
//
//   RestartControllerFromReset
//
//================================================================================================
//
IOReturn
AppleUSBEHCI::RestartControllerFromReset(void)
{
	int				i;
	UInt32			USBCmd;

	USBTrace( kUSBTEHCI, kTPEHCIRestartControllerFromReset, (uintptr_t)this, 0, 0, 0);
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
		USBLog(5, "AppleUSBEHCI[%p]::RestartControllerFromReset - restoring AsyncListAddr[from 0x%x to 0x%x]",  this, (uint32_t)_pEHCIRegisters->AsyncListAddr, (uint32_t)_savedAsyncListAddr);
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

		// get rid of the count as well as the enable bit
		//USBCmd &= ~kEHCICMDAsyncParkModeCountMask;
		
		// this would allow a different count if we stayed enabled
		// USBCmd |= (2 << kEHCICMDAsyncParkModeCountMaskPhase);
		
		// this line will eliminate park mode completely
		//USBCmd &= ~kEHCICMDAsyncParkModeEnable;
	
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
	
	_outSlot = kEHCIPeriodicListEntries+1;	/* No Isoc transactions currently. */

	return kIOReturnSuccess;
}



//================================================================================================
//
//   DozeController
//
//================================================================================================
//
IOReturn
AppleUSBEHCI::DozeController(void)
{
	USBTrace( kUSBTEHCI, kTPEHCIDozeController, (uintptr_t)this, 0, 0, 0);

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



//================================================================================================
//
//   WakeControllerFromDoze
//
//================================================================================================
//
IOReturn				
AppleUSBEHCI::WakeControllerFromDoze(void)
{
	int				i;
	UInt32			port;
	bool			somePortNeedsToResume = false;

	USBTrace( kUSBTEHCI, kTPEHCIWakeControllerFromDoze, (uintptr_t)this, 0, 0, 0);

	RestartUSBBus();

	// check to see if we have a pending resume on any port and if so, wait for 20ms
	for (port = 0; port < _rootHubNumPorts; port++)
	{
		UInt32		portStatus = USBToHostLong(_pEHCIRegisters->PortSC[port]);
		if (portStatus & kEHCIPortSC_Resume)
		{
			USBLog(5, "AppleUSBEHCI[%p]::WakeControllerFromDoze - port %d appears to be resuming from a remote wakeup", this, (int)port+1);
			_rhPortBeingResumed[port] = true;
			somePortNeedsToResume = true;
		}
	}
	
	if ( somePortNeedsToResume )
	{
		// Now, wait the 20ms for the resume and then call RHResumeAllPorts to finish
		IOSleep(20);
		
		RHCompleteResumeOnAllPorts();
	}
	
	return kIOReturnSuccess;
}



//================================================================================================
//
//   EnableInterruptsFromController
//
//================================================================================================
//
IOReturn
AppleUSBEHCI::EnableInterruptsFromController(bool enable)
{
	bool		armSecondaryInterrupt = false;
	
	USBTrace( kUSBTEHCI, kTPEHCIEnableInterrupts, (uintptr_t)this, enable, 0, 0);

	if (enable)
	{
		if ((_errataBits & kErrataMissingPortChangeInt) && _portChangeInterrupt)
		{
			UInt32					sts = USBToHostLong(_pEHCIRegisters->USBSTS);
			UInt32					intr = USBToHostLong(_pEHCIRegisters->USBIntr);
			
			if (!(intr & kEHCIPortChangeIntBit))					// make sure interrupt is currently masked out
			{
				if (!(sts & kEHCIPortChangeIntBit))
				{
					USBLog(2, "AppleUSBEHCI[%p]::EnableInterruptsFromController - arming for missed port change interrupt", this);
					armSecondaryInterrupt = true;
				}
				else
				{
					USBLog(2, "AppleUSBEHCI[%p]::EnableInterruptsFromController - no need to rearm for port change interrupt since is will arm itself", this);
				}
			}
			else
			{
				USBLog(2, "AppleUSBEHCI[%p]::EnableInterruptsFromController - Port Change int unexpectedly armed in USBIntr register", this);
			}
		}
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
	
	if (armSecondaryInterrupt && _filterInterruptSource)
		_filterInterruptSource->signalInterrupt();
		
	return kIOReturnSuccess;
}


unsigned long
AppleUSBEHCI::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
{
	unsigned long ret = super::maxCapabilityForDomainState(domainState);
	
	// trying to make this a limited thing for EHCI only
	// if we are on a Thunderbolt system and we are not already waking from hibernation and we are waking up from sleep, but the ConfigFlag got cleared
	// then we need to do a wake from hibernation
	if (_v3ExpansionData->_onThunderbolt && !_wakingFromHibernation && (_myPowerState == kUSBPowerStateSleep) && (ret > kUSBPowerStateSleep))
	{
		UInt32	configFlag = USBToHostLong(_pEHCIRegisters->ConfigFlag);
		if (configFlag == kEHCIInvalidRegisterValue)
		{
			_controllerAvailable = false;
			USBLog(1, "AppleUSBEHCI[%p]::maxCapabilityForDomainState - registers have gone invalid", this);
		}
		else if (configFlag != kEHCIPortRoutingBit)
		{
			USBError(1, "EHCI: Thunderbolt controller appears to have been reset - going into wake from hibernation PortSC[0](0x%x)", (int)USBToHostLong(_pEHCIRegisters->PortSC[0]));
			ResetControllerState();
			EnableAllEndpoints(true);
			_wakingFromHibernation = true;
			ret = kUSBPowerStateOff;
		}
	}
	
	return ret;
}



//================================================================================================
//
//   powerStateDidChangeTo
//
//================================================================================================
//
IOReturn
AppleUSBEHCI::powerStateDidChangeTo ( IOPMPowerFlags capabilities, unsigned long stateNumber, IOService* whatDevice)
{
	USBTrace( kUSBTEHCI, kTPEHCIPowerState, (uintptr_t)this, stateNumber, 0, 2);

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


//================================================================================================
//
//   powerChangeDone
//
//================================================================================================
//
void
AppleUSBEHCI::powerChangeDone ( unsigned long fromState)
{
	unsigned long newState = getPowerState();
	
	USBTrace( kUSBTEHCI, kTPEHCIPowerState, (uintptr_t)this, fromState, newState, 1);
	USBLog((fromState == newState) || !_controllerAvailable ? 7 : 4, "AppleUSBEHCI[%p]::powerChangeDone from state (%d) to state (%d) _controllerAvailable(%s)", this, (int)fromState, (int)newState, _controllerAvailable ? "true" : "false");
	if (_wakingFromHibernation)
	{
		USBLog(2, "AppleUSBEHCI[%p]::powerChangeDone - _wakingFromHibernation - _savedAsyncListAddr(%p) AsyncListAddr(%p) _AsyncHead(%p)", this, (void*)USBToHostLong(_savedAsyncListAddr), (void*)_pEHCIRegisters->AsyncListAddr, _AsyncHead);		
		_savedAsyncListAddr = _pEHCIRegisters->AsyncListAddr;
		// at this point, we expect _savedAsyncListAddr to be NULL, since when we are waking from hibernation the AsyncListAddr should be NULL (see the end of ResetControllerState)
		if (_savedAsyncListAddr == kEHCIInvalidRegisterValue)
		{
			_controllerAvailable = false;
			USBLog(1, "AppleUSBEHCI[%p]::powerChangeDone - registers have gone invalid", this);
			_savedAsyncListAddr = 0;
		}
		else
		{
			if (_savedAsyncListAddr)
			{
				USBLog(1, "AppleUSBEHCI[%p]::powerChangeDone - _savedAsyncListAddr is NOT NULL (%p) - UNEXPECTED", this, (void*)_savedAsyncListAddr);
				_savedAsyncListAddr = 0;
			}
			if (_AsyncHead)						// if all of the endpoints have not been aborted yet, then we need to throw them away
			{
				AppleEHCIQueueHead		*pQH = _AsyncHead;
				USBError(1, "AppleUSBEHCI[%p]::powerChangeDone - waking from hibernation with some queue heads on the queue. UNEXPECTED", this);
				while (pQH)
				{
					UInt32					flags = USBToHostLong(pQH->GetSharedLogical()->flags);
					
					USBLog(1, "AppleUSBEHCI[%p]::powerChangeDone - pQH(%p) ADDR(%d) EP(%d) DIR(%d) being throw away", this, pQH, (int)(flags & kEHCIEDFlags_FA), (int)((flags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase), (int)pQH->_direction);
					pQH = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
				}
				_AsyncHead = NULL;
			}
		}
	}
	if (_controllerAvailable)
		showRegisters(7, "powerChangeDone");
		
	super::powerChangeDone(fromState);
}





