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
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/platform/ApplePlatformExpert.h>

#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBLog.h>

#define super IOUSBControllerV3


// From the file Gossamer.h that is not available
enum {
    kGossamerTypeGossamer = 1,
    kGossamerTypeSilk,
    kGossamerTypeWallstreet,
    kGossamerTypeiMac,
    kGossamerTypeYosemite,
    kGossamerType101
};



#include "AppleUSBOHCI.h"

#define kAppleCurrentAvailable	"AAPL,current-available"

// this section needs to get moved to the IOPCIFamily
#define DEBUG_PCI_PWR_MGMT 1
#define _controllerCanSleep				_expansionData->_controllerCanSleep

void
AppleUSBOHCI::CheckSleepCapability(void)
{
	// assume that sleep is OK
	_controllerCanSleep = true;
	
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
		if (_device->hasPCIPowerManagement() && (_device->enablePCIPowerManagement() == kIOReturnSuccess))
		{
			_hasPCIPwrMgmt = true;
            setProperty("Card Type","Built-in");
		}
        else
        {
            USBError(1, "AppleUSBOHCI[%p]::CheckSleepCapability - OHCI controller will be unloaded across sleep",this);
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


void
AppleUSBOHCI::SuspendUSBBus()
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
    hcControl = kOHCIFunctionalState_Suspend << kOHCIHcControl_HCFSPhase;
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

void
AppleUSBOHCI::ResumeUSBBus()
{
    switch ((USBToHostLong(_pOHCIRegisters->hcControl) & kOHCIHcControl_HCFS) >> kOHCIHcControl_HCFSPhase )
    {
        case kOHCIFunctionalState_Suspend:
			// Place the USB bus into the resume State
			USBLog(2, "AppleUSBOHCI[%p]:: Resuming bus from Suspend state", this);
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
				USBLog(2, "AppleUSBOHCI[%p]:: Delaying 35 milliseconds in resume state", this);
				IOSleep(35);
			}
			else
			{
				USBLog(2, "AppleUSBOHCI[%p]:: Delaying 20 milliseconds in resume state", this);
				IOSleep(20);
			}
			// intentional fall through
        case kOHCIFunctionalState_Reset:
			// Place the USB bus into the operational State
			USBLog(2, "AppleUSBOHCI[%p]: Changing bus to operational", this);
			_pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase);
			IOSleep(3);			// wait the required 3 ms before turning on the lists
			_pOHCIRegisters->hcControl =  HostToUSBLong((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
														| kOHCIHcControl_CLE | (_OptiOn ? kOHCIHcControl_Zero : kOHCIHcControl_BLE) 
														| kOHCIHcControl_PLE | kOHCIHcControl_IE);
			break;
        default:
            USBLog(2, "AppleUSBOHCI[%p]: Bus already operational", this);
            break;
    }
}



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


IOReturn				
AppleUSBOHCI::SaveControllerStateForSleep(void)
{	
	USBLog(2, "AppleUSBOHCI[%p]::setPowerState - suspending the bus", this);
	_remote_wakeup_occurred = false;
	
	SuspendUSBBus();
	USBLog(2, "AppleUSBOHCI[%p]::setPowerState - The bus is now suspended", this);
	_myBusState = kUSBBusStateSuspended;
	
	// on PCI PM machines, arm the PME and go to state D3
	if (_hasPCIPwrMgmt)
	{
		_pOHCIRegisters->hcInterruptDisable = HostToUSBLong (kOHCIHcInterrupt_MIE);			// disable interrupts during D3 state
	}
	
	return kIOReturnSuccess;
}



IOReturn				
AppleUSBOHCI::RestoreControllerStateFromSleep(void)
{
	USBLog(2, "AppleUSBOHCI[%p]::RestoreControllerStateFromSleep - powering on USB", this);
	
	// at this point, interrupts are disabled, and we are waking up. If the Port Change interrupt is active
	// then it is likely that we are responsible for the system issuing the wakeup
	if (USBToHostLong(_pOHCIRegisters->hcInterruptStatus) & kOHCIHcInterrupt_RD)
	{
		IOLog("USB caused wake event (OHCI)\n");
	}
	
	_remote_wakeup_occurred = true;	//doesn't matter how we woke up
	
	ResumeUSBBus();
	_myBusState = kUSBBusStateRunning;
	

	LastRootHubPortStatusChanged(true);

	return kIOReturnSuccess;
}



IOReturn
AppleUSBOHCI::ResetControllerState(void)
{
	
    // Disable All OHCI Interrupts
    _pOHCIRegisters->hcInterruptDisable = HostToUSBLong(kOHCIHcInterrupt_MIE);
    IOSync();
    
    // Place the USB bus into the Reset State
    _pOHCIRegisters->hcControl = HostToUSBLong((kOHCIFunctionalState_Reset << kOHCIHcControl_HCFSPhase));
    IOSync();
	
    //  need to wait at least 1ms here
    IOSleep(2);
	
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



IOReturn
AppleUSBOHCI::RestartControllerFromReset(void)
{
	UInt32		hcDoneHead;
	
	USBLog(3, "AppleUSBOHCI[%p]::RestartControllerFromReset - Re-loading UIM if necessary (%d)", this, _uimInitialized );
	
    // Check to see if the hcDoneHead is not NULL.  If so, then we need to reset the controller
    //
    hcDoneHead = USBToHostLong(_pOHCIRegisters->hcDoneHead);
    if ( hcDoneHead != NULL )
    {
        USBError(1,"AppleUSBOHCI[%p]::RestartControllerFromReset Non-NULL hcDoneHead: 0x%lx", this, hcDoneHead );
		
        // Reset it now
        //
        _pOHCIRegisters->hcCommandStatus = USBToHostLong(kOHCIHcCommandStatus_HCR);  // Reset OHCI
        IOSleep(3);
    }
    
    // Restore the Control and Bulk head pointers
    //
    _pOHCIRegisters->hcControlCurrentED = 0;
    _pOHCIRegisters->hcControlHeadED = HostToUSBLong ((UInt32) _pControlHead->pPhysical);
    _pOHCIRegisters->hcBulkHeadED = HostToUSBLong ((UInt32) _pBulkHead->pPhysical);
    IOSync();
	
    // Write the HCCA
    //
    OSWriteLittleInt32(&_pOHCIRegisters->hcHCCA, 0, _hccaPhysAddr);
    IOSync();
	
    // Set the HC to write the donehead to the HCCA, and enable interrupts
    _pOHCIRegisters->hcInterruptStatus = USBToHostLong(kOHCIHcInterrupt_WDH);
    IOSync();
	
    // Set up hcFmInterval.
    UInt32	hcFSMPS;				// in register hcFmInterval
    UInt32	hcFI;					// in register hcFmInterval
    UInt32	hcPS;					// in register hcPeriodicStart
	
    hcFI = USBToHostLong(_pOHCIRegisters->hcFmInterval) & kOHCIHcFmInterval_FI;
    // this formula is from the OHCI spec, section 5.4
    hcFSMPS = ((((hcFI-kOHCIMax_OverHead) * 6)/7) << kOHCIHcFmInterval_FSMPSPhase);
    hcPS = (hcFI * 9) / 10;			// per spec- 90%
    _pOHCIRegisters->hcFmInterval = HostToUSBLong(hcFI | hcFSMPS);
    _pOHCIRegisters->hcPeriodicStart = HostToUSBLong(hcPS);
    IOSync();
	
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		// check hcFmInterval
		newValue = USBToHostLong(_pOHCIRegisters->hcFmInterval);
		while ((count++ < 10) && (newValue != (hcFI | hcFSMPS)))
		{
			USBError(1, "OHCI driver: RestartControllerFromReset - hcFmInterval not sticking. Retrying.");
			_pOHCIRegisters->hcFmInterval = HostToUSBLong(hcFI | hcFSMPS);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcFmInterval);
		}
		count = 0;						// reset
										// check hcPeriodicStart
		newValue = USBToHostLong(_pOHCIRegisters->hcPeriodicStart);
		while ((count++ < 10) && (newValue != hcPS))
		{
			USBError(1, "OHCI driver: RestartControllerFromReset - hcPeriodicStart not sticking. Retrying.");
			_pOHCIRegisters->hcPeriodicStart = HostToUSBLong(hcPS);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcPeriodicStart);
		}
		
	}
	
    // Initialize the Root Hub registers
    if (_errataBits & kErrataDisableOvercurrent)
        _pOHCIRegisters->hcRhDescriptorA |= HostToUSBLong(kOHCIHcRhDescriptorA_NOCP);
	
    _pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE); // should be SRWE which should be identical to DRWE
    
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);			// this bit SHOULD now be set
		while ((count++ < 10) && !(newValue & kOHCIHcRhStatus_DRWE))
		{
			USBError(1, "OHCI driver: RestartControllerFromReset - DRWE bit not sticking. Retrying.");
			_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC | kOHCIHcRhStatus_DRWE);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhStatus);
		}
	}
	
    // Set OHCI to operational state and enable processing of control list.
    _pOHCIRegisters->hcControl = HostToUSBLong ((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
                                                | kOHCIHcControl_CLE | kOHCIHcControl_BLE
                                                | kOHCIHcControl_PLE | kOHCIHcControl_IE);
    IOSync();
	
    OHCIRootHubPower(1 /* kOn */);
	_myBusState = kUSBBusStateRunning;

	return kIOReturnSuccess;
}



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



IOReturn
AppleUSBOHCI::DozeController(void)
{
		
	SuspendUSBBus();
	
	return kIOReturnSuccess;
}



IOReturn				
AppleUSBOHCI::WakeControllerFromDoze(void)
{
	ResumeUSBBus();
	_myBusState = kUSBBusStateRunning;

	return kIOReturnSuccess;
}



void
AppleUSBOHCI::powerChangeDone ( unsigned long fromState)
{
	unsigned long newState = getPowerState();
	
	USBLog((fromState == newState) ? 7 : 5, "AppleUSBOHCI[%p]::powerChangeDone from state (%d) to state (%d) _controllerAvailable(%s)", this, (int)fromState, (int)newState, _controllerAvailable ? "true" : "false");
	if (_controllerAvailable)
		showRegisters(7, "powerChangeDone");
	super::powerChangeDone(fromState);
}

