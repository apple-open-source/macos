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
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/platform/ApplePlatformExpert.h>

#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBOHCI.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBControllerV3

#define _controllerCanSleep				_expansionData->_controllerCanSleep

#ifndef kACPIDevicePathKey
	#define kIOPCIPMEOptionsKey			"IOPCIPMEOptions"
#endif

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
//	Convert USBLog to use kprintf debugging
//
//================================================================================================
//
#if OHCI_USE_KPRINTF
	#define OHCIPWRMGMT_USE_KPRINTF OHCI_USE_KPRINTF
#else
	#define OHCIPWRMGMT_USE_KPRINTF 0
#endif

#if OHCIPWRMGMT_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= OHCIPWRMGMT_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif


//================================================================================================
//
//   CheckSleepCapability
//
//================================================================================================
//
void
AppleUSBOHCI::CheckSleepCapability(void)
{
	// assume that sleep is OK
	_controllerCanSleep = true;
	_hasPCIPwrMgmt = false;
	
    //   We need to determine which OHCI controllers don't survive sleep.  These fall into 2 categories:
    //
    //   1.  CardBus cards
    //	 2.  PCI Cards that lose power (right now because of a bug in the PCI Family, USB PCI cards do not prevent
    //	     sleep, so even cards that don't support the PCI Power Mgmt stuff get their power removed.
    //
    //  Additionally, the PowerBook 101 controller cannot survive across sleep (I doesn't support remote wakeup).
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
    //  "AAPL,clock-id" property in the provider.  If it does not exist, then we are a OHCI controller on a USB PCI card.
    //
    if ( !_device->getProperty("AAPL,clock-id") && !_onCardBus && !((getPlatform()->getChipSetType() == kChipSetTypeGossamer) && getPlatform()->getMachineType() == kGossamerTypeYosemite) )
    {
		if (_device->getProperty("built-in"))
		{
			// rdar://5769508 - if we are on a built in PCI device, then assume the system supports D3cold
			if (_device->hasPCIPowerManagement(kPCIPMCPMESupportFromD3Cold))
			{
				_device->setProperty(kIOPCIPMEOptionsKey, kOSBooleanTrue);
				if (_device->enablePCIPowerManagement(kPCIPMCSPowerStateD3) == kIOReturnSuccess)
				{
					_hasPCIPwrMgmt = true;
					setProperty("Card Type","Built-in");
				}
			}
		}
		else
		{
			// rdar://5856545 - on older machines without the built-in property, we need to use the "default" case in the IOPCIDevice code
			if (_device->hasPCIPowerManagement())
			{
				_device->setProperty(kIOPCIPMEOptionsKey, kOSBooleanTrue);
				if (_device->enablePCIPowerManagement() == kIOReturnSuccess)
				{
					_hasPCIPwrMgmt = true;
					setProperty("Card Type","Built-in");
				}
			}
		}
		
        if (!_hasPCIPwrMgmt)
        {
            USBError(1, "AppleUSBOHCI[%p]::CheckSleepCapability - controller will be unloaded across sleep",this);
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
        _controllerCanSleep = false;
    }
	
	// Call registerService() so that the IOUSBController object is published and clients (like Prober) can find it
	registerService();
}


//================================================================================================
//
//   callPlatformFunction
//
//================================================================================================
//
IOReturn 
AppleUSBOHCI::callPlatformFunction(const OSSymbol *functionName,
								   bool waitForFunction,
								   void *param1, void *param2,
								   void *param3, void *param4)
{  
    if (functionName == _usb_remote_wakeup)
    {
		bool	*wake;
		
		wake = (bool *)param1;
        
		if (_remote_wakeup_occurred)
		{
			*wake = true;
		}
		else
		{
			*wake = false;
		}
    	return kIOReturnSuccess;
    }
	
    return kIOReturnBadArgument;
}


//================================================================================================
//
//   SuspendUSBBus
//
//================================================================================================
//
void
AppleUSBOHCI::SuspendUSBBus(bool goingToSleep)
{
    UInt32			something;
    UInt32			hcControl;
	
    // 1st turn off all list processing
    //
    hcControl = USBToHostLong(_pOHCIRegisters->hcControl);
    hcControl &= ~(kOHCIHcControl_CLE | kOHCIHcControl_BLE | kOHCIHcControl_PLE | kOHCIHcControl_IE);
	
    _pOHCIRegisters->hcControl = HostToUSBLong(hcControl);
    
    // We used to wait for a SOF interrupt here.  Now just sleep for 1 ms.
    //
    IOSleep(1);
    
    // check for the WDH register to see if we need to process is [2405732]
    //
    if ( _writeDoneHeadInterrupt )
    {
        USBError(1,"AppleUSBOHCI[%p] Processing WDH before suspending", this);
        PollInterrupts();
    }
    
    // now tell the controller to put the bus into suspend mode
	if (goingToSleep && (_errataBits & kErrataOHCINoGlobalSuspendOnSleep))
	{
		UInt32			port;
		hcControl = kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase;
		for (port=0; port < _rootHubNumPorts; port++)
		{
			_savedHcRhPortStatus[port] = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
			USBLog(5, "AppleUSBOHCI[%p]::SuspendUSBBus - port %d _savedHcRhPortStatus(%p)", this, (int)port+1, (void*)_savedHcRhPortStatus[port]);
		}
	}
	else
	{
		hcControl = kOHCIFunctionalState_Suspend << kOHCIHcControl_HCFSPhase;
	}
		
    if (_hasPCIPwrMgmt)
		hcControl |= kOHCIHcControl_RWC | kOHCIHcControl_RWE;
		
    _pOHCIRegisters->hcControl = HostToUSBLong(hcControl);
    IOSleep(3);	// wait 3 milliseconds for things to settle
    
    /*
	 switch ((USBToHostLong(_pOHCIRegisters->hcControl) & kOHCIHcControl_HCFS) >> kOHCIHcControl_HCFSPhase )
	 {
		 case kOHCIFunctionalState_Suspend:
			 // Place the USB bus into the resume State
			 kprintf("AppleUSBOHCI: Bus in suspend mode as expected\n");
			 break;
		 case kOHCIFunctionalState_Resume:
			 kprintf("AppleUSBOHCI: Bus in Resume state???\n");
			 break;
		 case kOHCIFunctionalState_Reset:
			 // Place the USB bus into the operational State
			 kprintf("AppleUSBOHCI: Bus in reset state???\n");
			 break;
		 default:
			 kprintf("AppleUSBOHCI: Bus operational???\n");
			 break;
	 }
	 */
}


//================================================================================================
//
//   ResumeUSBBus
//
//================================================================================================
//
void
AppleUSBOHCI::ResumeUSBBus(bool wakingFromSleep)
{
	UInt32		newValue;
	
	// move this to here because on some OHCI controllers we don't put the controller into suspend mode on sleep
	if (wakingFromSleep)
	{
		UInt32		port, portSC;
		
		IOSleep(1);
		for (port=0; port < _rootHubNumPorts; port++)
		{
			UInt32	portSC = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
			if (portSC & kOHCIHcRhPortStatus_CSC)
			{
				if (portSC & kOHCIHcRhPortStatus_PES)
				{
					USBError(1, "USB (OHCI):Port %d on bus 0x%x has connect status change but is still enabled. setting clear port enable. hcRhPortStatus(%p)", (int)port+1, (uint32_t)_busNumber, (void*)portSC);
					_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(kOHCIHcRhPortStatus_CCS);				// CCS when writing is CPE
					IOSleep(1);
					portSC = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
				}
				else
				{
					IOLog("USB (OHCI):Port %d on bus 0x%x connected or disconnected. portSC(%p)\n", (int)port+1, (uint32_t)_busNumber, (void*)portSC);
				}
			}
			else if (portSC & kOHCIHcRhPortStatus_PSSC)
			{
				IOLog("USB (OHCI):Port %d on bus 0x%x has remote wakeup from some device\n", (int)port+1, (uint32_t)_busNumber);
			}
			else if ((_errataBits & kErrataOHCINoGlobalSuspendOnSleep)					// if we are on these controllers
					 && (portSC & kOHCIHcRhPortStatus_CCS)								// and we are currently connected
					 && !(portSC & kOHCIHcRhPortStatus_PES)								// and we are not currently enabled
					 &&  (_savedHcRhPortStatus[port] & kOHCIHcRhPortStatus_PES))		// and we were enabled before we went to sleep
			{
				USBError(1, "USB (OHCI):Port %d on bus 0x%x is connected but not enabled. trying to set port enable. hcRhPortStatus(%p) _savedHcRhPortStatus(%p)", (int)port+1, (uint32_t)_busNumber, (void*)portSC, (void*)_savedHcRhPortStatus[port]);
				_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(kOHCIHcRhPortStatus_PES);				// CCS when writing is CPE
				IOSleep(1);
				portSC = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
				USBLog(2, "AppleUSBOHCI[%p}::ResumeUSBBus - new hcRhPortStatus(%p)", this, (void*)portSC);
			}
			_savedHcRhPortStatus[port] = 0;												// clear this out to be safe once we have no more need for it
		}
	}
    switch ((USBToHostLong(_pOHCIRegisters->hcControl) & kOHCIHcControl_HCFS) >> kOHCIHcControl_HCFSPhase )
    {
        case kOHCIFunctionalState_Suspend:
			// Place the USB bus into the resume State
			USBLog(2, "AppleUSBOHCI[%p]::ResumeUSBBus - Resuming bus from Suspend state", this);
			_pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Resume << kOHCIHcControl_HCFSPhase);
			// intentional fall through
        case kOHCIFunctionalState_Resume:
			// Complete the resume by waiting for the required delay
			if(_errataBits & kErrataLucentSuspendResume)
                // JRH 08-27-99
                // this is a very simple yet clever hack for working around a bug in the Lucent controller
                // By using 35 instead of 20, we overflow an internal 5 bit counter by exactly 3ms, which 
                // stops an errant 3ms suspend from appearing on the bus
			{
				USBLog(2, "AppleUSBOHCI[%p]::ResumeUSBBus- Delaying 35 milliseconds in resume state", this);
				IOSleep(35);
			}
			else
			{
				USBLog(2, "AppleUSBOHCI[%p]::ResumeUSBBus - Delaying 20 milliseconds in resume state", this);
				IOSleep(20);
			}
			// intentional fall through
        case kOHCIFunctionalState_Reset:
			// Place the USB bus into the operational State
			USBLog(2, "AppleUSBOHCI[%p]::ResumeUSBBus - Changing bus to operational", this);
			_pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase);
			IOSync();
			IOSleep(3);			// wait the required 3 ms before turning on the lists
			
			// <rdar://problem/5981624> We need to make sure that the DRWE bit is properly set any time we go to the operational state
			newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);
			if (!(newValue & kOHCIHcRhStatus_DRWE))
			{
				_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE);
				IOSync();
				
				if (_errataBits & kErrataNECIncompleteWrite)
				{
					UInt32		count = 0;
					newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);			// this bit SHOULD now be set
					while ((count++ < 10) && !(newValue & kOHCIHcRhStatus_DRWE))
					{
						USBError(1, "OHCI driver::ResumeUSBBus - DRWE bit not sticking. Retrying.");
						_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE);
						IOSync();
						newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);
					}
				}
			}
			_pOHCIRegisters->hcControl =  HostToUSBLong((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
														| kOHCIHcControl_CLE | (_OptiOn ? kOHCIHcControl_Zero : kOHCIHcControl_BLE) 
														| kOHCIHcControl_PLE | kOHCIHcControl_IE);
			IOSync();
			break;
			
        default:
            USBLog(2, "AppleUSBOHCI[%p]: Bus already operational - turning on the lists", this);
			_pOHCIRegisters->hcControl =  HostToUSBLong((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
														| kOHCIHcControl_CLE | (_OptiOn ? kOHCIHcControl_Zero : kOHCIHcControl_BLE) 
														| kOHCIHcControl_PLE | kOHCIHcControl_IE);
			IOSync();
			
            break;
    }
}



//================================================================================================
//
//   AllocatePowerStateArray
//
//================================================================================================
//
IOReturn
AppleUSBOHCI::AllocatePowerStateArray(void)
{
	IOReturn		err;
	
	err = super::AllocatePowerStateArray();
	
	if (!err)
	{
		// Exception for KeyLargo based systems, which have two power parents and get the kIOPMClockNormal from one of them (USBKeyLargo)
		if ( _device->getProperty("AAPL,clock-id")) 
		{
			// if I am on a KeyLargo system, I need to add the Key Largo clocks to two of my input requirements
			_myPowerStates[kUSBPowerStateLowPower].inputPowerRequirement |= kIOPMClockNormal;
			_myPowerStates[kUSBPowerStateOn].inputPowerRequirement |= kIOPMClockNormal;
		}
	}
	return err;
}


//================================================================================================
//
//   SaveControllerStateForSleep
//
//================================================================================================
//
IOReturn				
AppleUSBOHCI::SaveControllerStateForSleep(void)
{	
	UInt8			pciPMCapOffset = 0;
	UInt16			pmControlStatus = 0;
	
	// <rdar://problem/6623922>
	// The PCI family will have cleared the kPCIPMCSPMEStatus at this point. However, some OHCI controllers will apparently 
	// set the bit again, probably when we actually put the individual ports into suspend. So we need to clear it before we
	// put the controller into global suspend.
	_device->findPCICapability(kIOPCIPowerManagementCapability, &pciPMCapOffset);
	if (pciPMCapOffset > 0x3f)					// must be > 3f, section 3.1
	{
		pmControlStatus = pciPMCapOffset + 4;
	}	
	
	if (pmControlStatus)
	{
		UInt16			pmcsr = _device->configRead16(pmControlStatus);
		USBLog(7, "AppleUSBOHCI[%p]::SaveControllerStateForSleep before PMCS for device (%p) is (%p)", this, _device, (void*)pmcsr);
		if (pmcsr & kPCIPMCSPMEStatus)
		{
			// this one bit (kPCIPMCSPMEStatus) is Read/Write Clear. All other bits are R/W, so we write back the same value we 
			// read so that it will be clear after the write
			_device->configWrite16(pmControlStatus, pmcsr);
			IOSleep(2);
		}
	}
	
	USBLog(2, "AppleUSBOHCI[%p]::SaveControllerStateForSleep - suspending the bus", this);
	_remote_wakeup_occurred = false;
	
	SuspendUSBBus(true);

	USBLog(2, "AppleUSBOHCI[%p]::SaveControllerStateForSleep - The bus is now suspended", this);
	_myBusState = kUSBBusStateSuspended;
	
	// on PCI PM machines, arm the PME and go to state D3
	if (_hasPCIPwrMgmt)
	{
		_pOHCIRegisters->hcInterruptDisable = HostToUSBLong (kOHCIHcInterrupt_MIE);			// disable interrupts during D3 state
	}
	
	return kIOReturnSuccess;
}



//================================================================================================
//
//   RestoreControllerStateFromSleep
//
//================================================================================================
//
IOReturn				
AppleUSBOHCI::RestoreControllerStateFromSleep(void)
{
	USBLog(2, "AppleUSBOHCI[%p]::RestoreControllerStateFromSleep - powering on USB", this);
	
	_remote_wakeup_occurred = true;	//doesn't matter how we woke up
	
	// before resuming, make sure the op regs are inited
	InitializeOperationalRegisters(false);

	// at this point, interrupts are disabled, and we are waking up. If the Port Change interrupt is active
	// then it is likely that we are responsible for the system issuing the wakeup
	// 5922768 - we used to use the interrupt status register here, but some OHCI controllers don't have that bit set when waking from
	// a remote wakeup. However, it appears that all controllers we know of will be in the Resume state by this time if we are waking because
	// of a remote wakeup - after review, remove this completely and let the per port code handle it
	//if (((USBToHostLong(_pOHCIRegisters->hcControl) & kOHCIHcControl_HCFS) >> kOHCIHcControl_HCFSPhase ) == kOHCIFunctionalState_Resume)
	//{
	//	UInt32			port;
	//
	//	IOLog("USB caused wake event (OHCI @ 0x%x)\n", (uint32_t)_busNumber);
	//}
	
	ResumeUSBBus(true);
	_myBusState = kUSBBusStateRunning;
	

	LastRootHubPortStatusChanged(true);

	return kIOReturnSuccess;
}



//================================================================================================
//
//   ResetControllerState
//
//================================================================================================
//
IOReturn
AppleUSBOHCI::ResetControllerState(void)
{
	
    // Disable All OHCI Interrupts
    _pOHCIRegisters->hcInterruptDisable = HostToUSBLong(kOHCIHcInterrupt_MIE);
    IOSync();
    
    // Place the USB bus into the Reset State
    _pOHCIRegisters->hcControl = HostToUSBLong((kOHCIFunctionalState_Reset << kOHCIHcControl_HCFSPhase));
    IOSync();
	
	// always make sure we stay in reset for at least 50 ms
    IOSleep(50);
	
    // Clear all Processing Registers
    _pOHCIRegisters->hcHCCA = 0;
    _pOHCIRegisters->hcControlHeadED = 0;
    _pOHCIRegisters->hcControlCurrentED = 0;
    _pOHCIRegisters->hcBulkHeadED = 0;
    _pOHCIRegisters->hcBulkCurrentED = 0;
    IOSync();
	
    // turn off the global power
    OHCIRootHubPower(0 /* kOff */);
	
    // go ahead and reset the controller
    _pOHCIRegisters->hcCommandStatus = HostToUSBLong(kOHCIHcCommandStatus_HCR);  	// Reset OHCI
    IOSync();
    IOSleep(1);			// the spec says 10 microseconds

	return kIOReturnSuccess;
}



//================================================================================================
//
//   RestartControllerFromReset
//
//================================================================================================
//
IOReturn
AppleUSBOHCI::RestartControllerFromReset(void)
{
	UInt32		newValue;
	
	USBLog(3, "AppleUSBOHCI[%p]::RestartControllerFromReset - Re-loading UIM if necessary (%d)", this, _uimInitialized );
	
	// first, reinit the op regs
	InitializeOperationalRegisters(true);
	
    // Set OHCI to operational state and enable processing of control list.
	_pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase);
	IOSync();
	IOSleep(3);			// wait the required 3 ms before turning on the lists
	
	// <rdar://problem/5981624> We need to make sure that the DRWE bit is properly set any time we go to the operational state
	newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);
	if (!(newValue & kOHCIHcRhStatus_DRWE))
	{
		_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE);
		IOSync();
		
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			UInt32		count = 0;
			newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);			// this bit SHOULD now be set
			while ((count++ < 10) && !(newValue & kOHCIHcRhStatus_DRWE))
			{
				USBError(1, "OHCI driver::RestartControllerFromReset - DRWE bit not sticking. Retrying.");
				_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE);
				IOSync();
				newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);
			}
		}
	}
	
	_pOHCIRegisters->hcControl =  HostToUSBLong((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
												| kOHCIHcControl_CLE | (_OptiOn ? kOHCIHcControl_Zero : kOHCIHcControl_BLE) 
												| kOHCIHcControl_PLE | kOHCIHcControl_IE);
    IOSync();
	
    OHCIRootHubPower(1 /* kOn */);
	_myBusState = kUSBBusStateRunning;

	return kIOReturnSuccess;
}


//================================================================================================
//
//   EnableInterruptsFromController
//
//================================================================================================
//
IOReturn
AppleUSBOHCI::EnableInterruptsFromController(bool enable)
{
	if (enable)
	{
		USBLog(2, "AppleUSBOHCI[%p]::EnableInterruptsFromController - enabling interrupts", this);
		_pOHCIRegisters->hcInterruptEnable = HostToUSBLong (kOHCIHcInterrupt_MIE | kOHCIDefaultInterrupts);
		IOSync();
	}
	else
	{
		_pOHCIRegisters->hcInterruptDisable = HostToUSBLong (kOHCIHcInterrupt_MIE);			// disable interrupts during D3 state
		IOSync();
		USBLog(2, "AppleUSBOHCI[%p]::EnableInterruptsFromController - interrupts disabled", this);
	}
	
	return kIOReturnSuccess;
}


//================================================================================================
//
//   DozeController
//
//================================================================================================
//
IOReturn
AppleUSBOHCI::DozeController(void)
{
		
	SuspendUSBBus(false);
	
	return kIOReturnSuccess;
}


//================================================================================================
//
//   WakeControllerFromDoze
//
//================================================================================================
//
IOReturn				
AppleUSBOHCI::WakeControllerFromDoze(void)
{
	ResumeUSBBus(false);
	_myBusState = kUSBBusStateRunning;

	return kIOReturnSuccess;
}


//================================================================================================
//
//   powerChangeDone
//
//================================================================================================
//
void
AppleUSBOHCI::powerChangeDone ( unsigned long fromState)
{
	unsigned long newState = getPowerState();
	
	USBLog((fromState == newState) ? 7 : 5, "AppleUSBOHCI[%p]::powerChangeDone from state (%d) to state (%d) _controllerAvailable(%s)", this, (int)fromState, (int)newState, _controllerAvailable ? "true" : "false");
	if (_controllerAvailable)
		showRegisters(7, "powerChangeDone");
	super::powerChangeDone(fromState);
}

