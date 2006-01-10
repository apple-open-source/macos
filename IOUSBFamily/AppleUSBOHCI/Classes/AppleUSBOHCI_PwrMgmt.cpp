/*
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

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/platform/ApplePlatformExpert.h>

#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBLog.h>

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

// USB bus has two power states, off and on
#define number_of_power_states 2

#define kAppleCurrentAvailable	"AAPL,current-available"

// Note: This defines two states. off and on. In the off state, the bus is suspended. We
// really should have three state, off (reset), suspended (suspend), and on (operational)
//

// Power States for devices without a clock,ID property (e.g. B&W G3, PC Cards, PCI Cards)
//
static IOPMPowerState ourPowerStates[number_of_power_states] = {
	{ 	
		// State 0
		1,	// version
		0,	// capabilityFlags
		0,	// outputPowerCharacter
		0,	// inputPowerRequirement
		0,	// staticPower
		0,	// unbudgeted Power
		0,	// power to attain
		0,	// time to attain
		0,	// settle up time
		0,	// time to lower
		0,	// settle down time
		0	// power domain budget
	},
	{ 	
		// State 1
		1,			// version
		IOPMDeviceUsable,	// capabilityFlags
		IOPMPowerOn,		// outputPowerCharacter
		IOPMPowerOn,		// inputPowerRequirement
		0,			// staticPower
		0,			// unbudgeted Power
		0,			// power to attain
		0,			// time to attain
		0,			// settle up time
		0,			// time to lower
		0,			// settle down time
		0			// power domain budget
	}
};

// Power States for Key Largo systems
//
static IOPMPowerState ourPowerStatesKL[number_of_power_states] = {
	{ 	
		// State 0
		1,	// version
		0,	// capabilityFlags
		0,	// outputPowerCharacter
		0,	// inputPowerRequirement
		0,	// staticPower
		0,	// unbudgeted Power
		0,	// power to attain
		0,	// time to attain
		0,	// settle up time
		0,	// time to lower
		0,	// settle down time
		0	// power domain budget
	},
	{ 	
		// State 1
		1,				// version
		IOPMDeviceUsable,		// capabilityFlags
		IOPMPowerOn,			// outputPowerCharacter
		IOPMPowerOn | IOPMClockNormal,	// inputPowerRequirement
		0,				// staticPower
		0,				// unbudgeted Power
		0,				// power to attain
		0,				// time to attain
		0,				// settle up time
		0,				// time to lower
		0,				// settle down time
		0				// power domain budget
	}
};


// this section needs to get moved to the IOPCIFamily
#define DEBUG_PCI_PWR_MGMT 1

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void AppleUSBOHCI::initForPM (IOPCIDevice *provider)
{
    UInt8	pciPMCapOffset;
    UInt16	pciPMCapReg;
    UInt16	requiredSleepState = 0;
    OSObject	*anObject;
    OSData	*aString;
    
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
    _onCardBus = (0 != provider->metaCast("IOCardBusDevice"));
	
    //  Now, look at PCI cards.  Note that the onboard controller's provider is an IOPCIDevice so we cannot use that
    //  to distinguish between USB PCI cards and the on board controller.  Instead, we use the existence of the
    //  "AAPL,clock-id" property in the provider.  If it does not exist, then we are a OHCI controller on a USB PCI card.
    //
    if ( !provider->getProperty("AAPL,clock-id") && !_onCardBus && !((getPlatform()->getChipSetType() == kChipSetTypeGossamer) && getPlatform()->getMachineType() == kGossamerTypeYosemite) )
    {
		if (provider->hasPCIPowerManagement() && (provider->enablePCIPowerManagement() == kIOReturnSuccess))
		{
			_hasPCIPwrMgmt = true;
            setProperty("Card Type","Built-in");
		}
        else
        {
            USBError(1, "%s[%p]::start OHCI controller will be unloaded across sleep",getName(),this);
            _unloadUIMAcrossSleep = true;
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
        _unloadUIMAcrossSleep = true;
    }
    
    // callPlatformFunction symbols
    _usb_remote_wakeup = OSSymbol::withCString("usb_remote_wakeup");
    registerService();  //needed to find ::callPlatformFunction and then to wake Yosemite
	
    // register ourselves with superclass policy-maker
    if ( provider->getProperty("AAPL,clock-id")) 
    {
		USBLog(2, "%s[%p]:: registering controlling driver with clock", getName(), this);
        registerPowerDriver(this,ourPowerStatesKL,number_of_power_states);
    }
    else 
    {
		USBLog(2, "%s[%p]:: registering controlling driver without clock", getName(), this);
        registerPowerDriver(this,ourPowerStates,number_of_power_states);
    }
    changePowerStateTo(1);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// Overrides superclass implementation, because kIOPMDoze is not in
// the power state array.
// Return that we can be in the On state if the system is On or in Doze.
// Otherwise return that we will be Off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long AppleUSBOHCI::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
{
    if ( getProvider()->getProperty("AAPL,clock-id")) {
        if ( ((domainState & IOPMPowerOn) && (domainState & IOPMClockNormal) ) ||
			 (domainState & kIOPMDoze) && (domainState & IOPMClockNormal) ) {
            return 1;
        }
        else {
            return 0;
        }
    }
    else {					// non-keylargo system
        if ( (domainState & IOPMPowerOn) ||
			 (domainState & kIOPMDoze) ) {
            return 1;
        }
        else {
            return 0;
        }
    }
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// Overrides superclass implementation, because the OHCI has multiple
// parents that start up at different times.
// Return that we are in the On state at startup time.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long AppleUSBOHCI::initialPowerStateForDomainState ( IOPMPowerFlags domainState )
{
    return 1;
}


//=============================================================================================
//
//	setPowerState
//
//	Called by the superclass to turn the controller on and off.  There are actually 3 different
// 	states: 
//		0 = suspended
//		1 = running
//		2 = idle suspend (suspended if nothing connected to the root hub but system is running.
//
//	The system will call us to go into state 0 or state 1.  We have an timer that looks for root hub
//	inactivity and when it sees such inactivity, it will call us with a level of 3.  When we then
//	detect a "resume" interrupt, we call setPowerState with a level of 1, running.
//
//=============================================================================================
//
IOReturn 
AppleUSBOHCI::setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice )
{
    IOReturn			sleepRes;
#ifndef kIOHibernateStateKey
#define kIOHibernateStateKey	"IOHibernateState"
#endif
    static uint32_t *		pHibernateState;
    
    USBLog(4,"%s[%p]::setPowerState (%ld) bus %d", getName(), this, powerStateOrdinal, _busNumber );
    IOSleep(5);
	
    //	If we are not going to sleep, then we need to take the gate, otherwise, we need to wake up    
    //
    if (_ohciBusState != kOHCIBusStateSuspended)
    {
        _workLoop->CloseGate();
    }
    else
    {
        sleepRes = _workLoop->wake(&_ohciBusState);
        if(sleepRes != kIOReturnSuccess) 
        {
            USBError(1, "%s[%p] setPowerState - Can't wake  workloop, error 0x%x", getName(), this, sleepRes);
        }
        else
        {
            USBLog(5, "%s[%p :setPowerState - workLoop successfully awakened", getName(), this);
        }
    }
    
    if ( powerStateOrdinal == kOHCISetPowerLevelSuspend ) 
    {
		if ( !pHibernateState )
		{
			OSData * data = OSDynamicCast(OSData, (IOService::getPMRootDomain())->getProperty(kIOHibernateStateKey));
			if (data)
				pHibernateState = (uint32_t *) data->getBytesNoCopy();
		}
		
        if ( _unloadUIMAcrossSleep )
        {
            USBLog(3,"%s[%p] Unloading UIM for bus %d before going to sleep",getName(),this, _busNumber );
            
            if ( _rootHubDevice )
            {
                USBLog(2, "%s[%p] Terminating root hub in setPowerState()", getName(), this);
                _rootHubDevice->terminate(kIOServiceRequired | kIOServiceSynchronous);
                _rootHubDevice->detachAll(gIOUSBPlane);
                _rootHubDevice->release();
                _rootHubDevice = NULL;
                USBLog(2, "%s[%p] Terminated root hub in setPowerState()", getName(), this);
            }
            SuspendUSBBus();
            UIMFinalizeForPowerDown();
            _ohciAvailable = false;									// tell the interrupt filter routine that we are off
        }
        else 
        {
            USBLog(2, "%s[%p] suspending the bus", getName(), this);
            _remote_wakeup_occurred = false;
			
            SuspendUSBBus();
            USBLog(2, "%s[%p] The bus is now suspended", getName(), this);
        }
        _ohciBusState = kOHCIBusStateSuspended;
        _idleSuspend = false;
		
        // on PCI PM machines, arm the PME and go to state D3
        if (_hasPCIPwrMgmt)
        {
            _pOHCIRegisters->hcInterruptDisable = HostToUSBLong (kOHCIHcInterrupt_MIE);			// disable interrupts during D3 state
            _ohciAvailable = false;									// tell the interrupt filter routine that we are off
        }
    }
    
    if ( powerStateOrdinal == kOHCISetPowerLevelIdleSuspend )
    {
        USBLog(2, "%s[%p] Suspending the bus due to inactivity", getName(), this);
        _idleSuspend = true;
        
        SuspendUSBBus();
		
        USBLog(2, "%s[%p] The bus is now suspended due to inactivity", getName(), this);
		
    }
    
    if ( powerStateOrdinal == kOHCISetPowerLevelRunning ) 
    {
        // If we were just idle suspended, we did not unload the UIM, so we need to check that here
        //
        if ( !_uimInitialized )
        {
            // If we are inactive OR if we are a PC Card and we have been ejected, then we don't need to do anything here
            //
            if ( isInactive() || (_onCardBus && _pcCardEjected) )
            {
                _ohciBusState = kOHCIBusStateRunning;
                USBLog(3,"%s[%p] isInactive (or pccardEjected) while setPowerState (%d,%d)",getName(),this, isInactive(), _pcCardEjected);
            }
            else
            {
                IOReturn	err = kIOReturnSuccess;
				
                USBLog(5, "%s[%p]: Re-loading UIM if necessary (%d)", getName(), this, _uimInitialized );
				
                // Initialize the hardware
                //
                UIMInitializeForPowerUp();
				
                _ohciAvailable = true;                          // tell the interrupt filter routine that we are on
				_ohciBusState = kOHCIBusStateRunning;
				
                if ( _rootHubDevice == NULL )
                {
                    err = CreateRootHubDevice( _device, &_rootHubDevice );
                    if ( err != kIOReturnSuccess )
                    {
                        USBError(1,"%s[%p] Could not create root hub device upon wakeup (%x)!",getName(), this, err);
                    }
                    else
                    {
                        _rootHubDevice->registerService(kIOServiceRequired | kIOServiceSynchronous);
                    }
                }
            }
        }
        else 
        {
            USBLog(2, "%s[%p] setPowerState powering on USB", getName(), this);
			
			// at this point, interrupts are disabled, and we are waking up. If the Port Change interrupt is active
			// then it is likely that we are responsible for the system issuing the wakeup
			if (USBToHostLong(_pOHCIRegisters->hcInterruptStatus) & kOHCIHcInterrupt_RD)
			{
				IOLog("USB caused wake event (OHCI)\n");
			}
			
            _remote_wakeup_occurred = true;	//doesn't matter how we woke up
			
            if (_hasPCIPwrMgmt)
            {
                _ohciAvailable = true;										// tell the interrupt filter routine that we are on
                _pOHCIRegisters->hcInterruptEnable = HostToUSBLong (kOHCIHcInterrupt_MIE);			// enable interrupts now that we are out of D3 state
                USBLog(4, "%s[%p]::setPowerState - after reenabling interrupts, hcInterruptEnable = %p", getName(), this, USBToHostLong(_pOHCIRegisters->hcInterruptEnable));
            }
            ResumeUSBBus();
            _ohciBusState = kOHCIBusStateRunning;

			// If we woke from hibernate, unload the UIM
			
			if ( _uimInitialized && pHibernateState && *pHibernateState )
			{
				USBLog(3,"%s[%p] Unloading UIM for bus %d after hibernate",getName(),this, _busNumber );
				
				if ( _rootHubDevice )
				{
					USBLog(2, "%s[%p] Terminating root hub in setPowerState()", getName(), this);
					_rootHubDevice->terminate(kIOServiceRequired | kIOServiceSynchronous);
					_rootHubDevice->detachAll(gIOUSBPlane);
					_rootHubDevice->release();
					_rootHubDevice = NULL;
					USBLog(2, "%s[%p] Terminated root hub in setPowerState()", getName(), this);
				}
				SuspendUSBBus();
				UIMFinalizeForPowerDown();
				_ohciAvailable = false;					// tell the interrupt filter routine that we are off

				USBLog(3,"%s[%p] Would create root hub here, postponing it! (%d)",getName(), this, _needToCreateRootHub);
				_needToCreateRootHub = TRUE;
				
			}
		}

        LastRootHubPortStatusChanged(true);
        _idleSuspend = false;
    }
	
	
    // if we are now suspended, then we need to sleep our workloop, otherwise, we need to release the gate on it
    //
    if (_ohciBusState == kOHCIBusStateSuspended)
    {
        sleepRes = _workLoop->sleep(&_ohciBusState);
        if(sleepRes != kIOReturnSuccess) 
        {
            USBError(1, "%s[%p] setPowerState - Can't sleep workloop, error 0x%x", getName(), this, sleepRes);
        }
        else
		{
            USBLog(5, "%s[%p :setPowerState - workLoop successfully slept", getName(), this);
        }
    }
    else
    {
        _workLoop->OpenGate();
    }
	
    USBLog(4,"%s[%p]::setPowerState done", getName(), this );
    return IOPMAckImplied;
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
        USBError(1,"%s[%p] Processing WDH before suspending", getName(), this);
        PollInterrupts();
    }
    
    //This line is necessary even though UIMInitialize sets kOHCIHcInterrupt_RD
    //  with kOHCIDefaultInterrupts.  Is something clobbering this register?
    _pOHCIRegisters->hcInterruptEnable = _pOHCIRegisters->hcInterruptEnable |  HostToUSBLong(kOHCIHcInterrupt_RD);
    
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
			USBLog(2, "%s[%p]:: Resuming bus from Suspend state", getName(), this);
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
				USBLog(2, "%s[%p]:: Delaying 35 milliseconds in resume state", getName(), this);
				IOSleep(35);
			}
			else
			{
				USBLog(2, "%s[%p]:: Delaying 20 milliseconds in resume state", getName(), this);
				IOSleep(20);
			}
			// intentional fall through
        case kOHCIFunctionalState_Reset:
			// Place the USB bus into the operational State
			USBLog(2, "%s[%p]: Changing bus to operational", getName(), this);
			_pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase);
			IOSleep(3);			// wait the required 3 ms before turning on the lists
			_pOHCIRegisters->hcControl =  HostToUSBLong((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
														| kOHCIHcControl_CLE | (_OptiOn ? kOHCIHcControl_Zero : kOHCIHcControl_BLE) 
														| kOHCIHcControl_PLE | kOHCIHcControl_IE);
			break;
        default:
            USBLog(2, "%s[%p]: Bus already operational", getName(), this);
            break;
    }
}
