/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBLog.h>

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

#include "AppleUSBEHCI.h"

// USB bus has two power states, off and on
#define number_of_power_states 2

// Note: This defines two states. off and on. In the off state, the bus is suspended. We
// really should have three state, off (reset), suspended (suspend), and on (operational)
static IOPMPowerState ourPowerStates[number_of_power_states] = {
  {1,0,0,0,0,0,0,0,0,0,0,0},
{1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

static IOPMPowerState ourPowerStatesKL[number_of_power_states] = {
  {1,0,0,0,0,0,0,0,0,0,0,0},
{1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn | IOPMClockNormal,0,0,0,0,0,0,0,0}
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void AppleUSBEHCI::initForPM (IOPCIDevice *provider)
{
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
    _onCardBus = (0 != provider->metaCast("IOCardBusDevice"));
    //  Now, look at PCI cards.  Note that the onboard controller's provider is an IOPCIDevice so we cannot use that
    //  to distinguish between USB PCI cards and the on board controller.  Instead, we use the existence of the
    //  "AAPL,clock-id" property in the provider.  If it does not exist, then we are a EHCI controller on a USB PCI card.
    //
    if ( !provider->getProperty("AAPL,clock-id") && !((getPlatform()->getChipSetType() == kChipSetTypeGossamer) && getPlatform()->getMachineType() == kGossamerTypeYosemite) )
    {
	if (provider->hasPCIPowerManagement() && (provider->enablePCIPowerManagement() == kIOReturnSuccess))
	{
	    _hasPCIPwrMgmt = true;
            setProperty("Card Type","Built-in");
	}
        else
        {
            USBLog(1, "%s[%p]::start EHCI controller will be unloaded across sleep",getName(),this);
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
    usb_remote_wakeup = OSSymbol::withCString("usb_remote_wakeup");
    registerService();  		//needed to find ::callPlatformFunction and then to wake Yosemite

    // register ourselves with superclass policy-maker
    if ( provider->getProperty("AAPL,clock-id")) 
    {
	USBLog(2, "%s(%p):: registering controlling driver with clock", getName(), this);
        registerPowerDriver(this,ourPowerStatesKL,number_of_power_states);
    }
    else 
    {
	USBLog(2, "%s(%p):: registering controlling driver without clock", getName(), this);
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
unsigned long AppleUSBEHCI::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
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
// Overrides superclass implementation, because the EHCI has multiple
// parents that start up at different times.
// Return that we are in the On state at startup time.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
unsigned long AppleUSBEHCI::initialPowerStateForDomainState ( IOPMPowerFlags domainState )
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
AppleUSBEHCI::setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice )
{
    IOReturn			sleepRes;
    
    USBLog(4,"%s[%p]::setPowerState (%ld) bus %d", getName(), this, powerStateOrdinal, _busNumber );
    IOSleep(5);

    //	If we are not going to sleep, then we need to take the gate, otherwise, we need to wake up    
    //
    if (_ehciBusState != kEHCIBusStateSuspended)
    {
        _workLoop->CloseGate();
    }
    else
    {
        sleepRes = _workLoop->wake(&_ehciBusState);
        if(sleepRes != kIOReturnSuccess) 
        {
            USBError(1, "%s[%p] setPowerState - Can't wake  workloop, error 0x%x", getName(), this, sleepRes);
        }
        else
        {
            USBLog(5, "%s[%p :setPowerState - workLoop successfully awakened", getName(), this);
        }
    }
    
    if ( powerStateOrdinal == kEHCISetPowerLevelSuspend ) 
    {

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
            _ehciAvailable = false;					// tell the interrupt filter routine that we are off
        }
        else 
        {
            USBLog(2, "%s[%p] suspending the bus", getName(), this);
            _remote_wakeup_occurred = false;

            // Make sure that we have the USB Bus running before we go into suspend
            if (_idleSuspend)
            {
                USBLog(5, "%s[%p]::setPowerState - in _idleSuspend -  restarting USB before suspending", getName(), this);
                RestartUSBBus();
            }
            SuspendUSBBus();
            USBLog(2, "%s[%p] The bus is now suspended", getName(), this);
        }
        
        _ehciBusState = kEHCIBusStateSuspended;
        _idleSuspend = false;

        // on PCI PM machines, arm the PME and go to state D3
        if (_hasPCIPwrMgmt)
        {
            _savedUSBIntr = _pEHCIRegisters->USBIntr;			// save currently enabled interrupts
            _pEHCIRegisters->USBIntr = 0;					// disable all interrupts
            _ehciAvailable = false;						// tell the interrupt filter routine that we are off
        }
        
    }
    
    if ( powerStateOrdinal == kEHCISetPowerLevelIdleSuspend )
    {
        USBLog(2, "%s[%p] halting the bus due to inactivity", getName(), this);
        _idleSuspend = true;
        
        StopUSBBus();

        USBLog(2, "%s[%p] The bus is now halted due to inactivity", getName(), this);

    }
    
    if ( powerStateOrdinal == kEHCISetPowerLevelRunning ) 
    {
	
        // If we were just idle suspended, we did not unload the UIM, so we need to check that here
        //
        if ( !_uimInitialized )
        {
            // If we are inactive OR if we are a PC Card and we have been ejected, then we don't need to do anything here
            //
            if ( isInactive() || (_onCardBus && _pcCardEjected) )
            {
                _ehciBusState = kEHCIBusStateRunning;
                USBLog(3,"%s[%p] isInactive (or pccardEjected) while setPowerState (%d,%d)",getName(),this, isInactive(), _pcCardEjected);
            }
            else
            {
                IOReturn	err = kIOReturnSuccess;
		
                USBLog(5, "%s[%p]: Re-loading UIM if necessary (%d)", getName(), this, _uimInitialized );
		
                // Initialize our hardware
                //
                UIMInitializeForPowerUp();
		
		_ehciBusState = kEHCIBusStateRunning;
                _ehciAvailable = true;										// tell the interrupt filter routine that we are on
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
	else if (_idleSuspend)
	{
            USBLog(1, "%s[%p]::setPowerState - in _idleSuspend -  restarting USB", getName(), this);
	    RestartUSBBus();
	}
        else 
        {
            USBLog(2, "%s[%p] setPowerState powering on USB", getName(), this);
	    
	    // at this point, interrupts are disabled, and we are waking up. If the Port Change interrupt is active
	    // then it is likely that we are responsible for the system issuing the wakeup
	    if (USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCIPortChangeIntBit)
	    {
		IOLog("USB caused wake event (EHCI)\n");
	    }
	    
            _remote_wakeup_occurred = true;	//doesn't matter how we woke up
	    
            if (_hasPCIPwrMgmt)
            {
                _ehciAvailable = true;										// tell the interrupt filter routine that we are on
                if (_savedUSBIntr)
                    _pEHCIRegisters->USBIntr = _savedUSBIntr;							// enable all interrupts
                USBLog(4, "%s[%p]::setPowerState - after reenabling interrupts, USBIntr = %p", getName(), this, USBToHostLong(_pEHCIRegisters->USBIntr));
            }
            ResumeUSBBus();
            _ehciBusState = kEHCIBusStateRunning;
        }
        LastRootHubPortStatusChanged(true);
        _idleSuspend = false;
    }


    // if we are now suspended, then we need to sleep our workloop, otherwise, we need to release the gate on it
    //
    if (_ehciBusState == kEHCIBusStateSuspended)
    {
        sleepRes = _workLoop->sleep(&_ehciBusState);
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


IOReturn AppleUSBEHCI::callPlatformFunction(const OSSymbol *functionName,
						    bool waitForFunction,
						    void *param1, void *param2,
						    void *param3, void *param4)
{  
    IOLog("EHCIUIM -- callPlatformFunction unimplimented \n");
    if (functionName == usb_remote_wakeup)
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
AppleUSBEHCI::ResumeUSBBus()
{
    UInt8	numPorts;
    int 	i;
    bool	enabledports = false;
    
    // suspend all enabled ports which we own
    numPorts = USBToHostLong(_pEHCICapRegisters->HCSParams) & kEHCINumPortsMask;
    USBLog(7, "%s[%p]::ResumeUSBBus - resuming %d ports", getName(), this, numPorts);
    for (i=0; i < numPorts; i++)
    {
	UInt32 portStat;
	portStat = USBToHostLong(_pEHCIRegisters->PortSC[i]);
	if (portStat & kEHCIPortSC_Owner)
	{
	    USBLog(7, "%s[%p]::ResumeUSBBus - port %d owned by OHCI", getName(), this, i);
	}
	else if (portStat & kEHCIPortSC_Enabled)
	{
	    portStat |= kEHCIPortSC_Resume;
	    _pEHCIRegisters->PortSC[i] = HostToUSBLong(portStat);
	    USBLog(7, "%s[%p]::ResumeUSBBus - port %d now resumed", getName(), this, i);
	    enabledports = true;
	}
	else
	{
	    USBLog(7, "%s[%p]::ResumeUSBBus - port %d not enabled", getName(), this, i);
	}
    }
    
    if (enabledports)
    {
	USBLog(2, "%s[%p]::ResumeUSBBus Delaying 20 milliseconds in resume state", getName(), this);
	IODelay(20000);
	for (i=0; i < numPorts; i++)
	{
	    UInt32 portStat;
	    portStat = USBToHostLong(_pEHCIRegisters->PortSC[i]);
	    if (portStat & kEHCIPortSC_Owner)
	    {
		USBLog(7, "%s[%p]::ResumeUSBBus - port %d owned by OHCI", getName(), this, i);
	    }
	    else if (portStat & kEHCIPortSC_Enabled)
	    {
		portStat &= ~kEHCIPortSC_Resume;
		_pEHCIRegisters->PortSC[i] = HostToUSBLong(portStat);
		USBLog(7, "%s[%p]::ResumeUSBBus - port %d now resumed", getName(), this, i);
		enabledports = true;
	    }
	    else
	    {
		USBLog(7, "%s[%p]::ResumeUSBBus - port %d not enabled", getName(), this, i);
	    }
	}
    }
    
    if (_savedUSBCMD)
    {
	USBLog(7, "%s[%p]::ResumeUSBBus - USBCMD is <%x> will be <%x>", getName(), this, _pEHCIRegisters->USBCMD, _savedUSBCMD);
	_pEHCIRegisters->USBCMD = _savedUSBCMD;
    }

}



void			
AppleUSBEHCI::SuspendUSBBus()
{
    UInt8	numPorts;
    int		i;
    UInt32	usbcmd, usbsts;
    
    // save the USBCMD register and disable the list processing
    _savedUSBCMD = _pEHCIRegisters->USBCMD;
    USBLog(7, "%s[%p]::SuspendUSBBus - got _savedUSBCMD <%x>", getName(), this, _savedUSBCMD);
    // disable list processing
    usbcmd = USBToHostLong(_savedUSBCMD);
    usbcmd &= ~kEHCICMDAsyncEnable;
    usbcmd &= ~kEHCICMDPeriodicEnable;
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);
    
    // suspend all enabled ports which we own
    GetNumberOfPorts( &numPorts );
    USBLog(7, "%s[%p]::SuspendUSBBus - suspending %d ports", getName(), this, numPorts);
    for (i=0; i < numPorts; i++)
    {
	UInt32 portStat;
	portStat = USBToHostLong(_pEHCIRegisters->PortSC[i]);
	if (portStat & kEHCIPortSC_Owner)
	{
	    USBLog(7, "%s[%p]::SuspendUSBBus - port %d owned by OHCI", getName(), this, i);
	}
	else if (portStat & kEHCIPortSC_Enabled)
	{
	    portStat |= kEHCIPortSC_Suspend;
	    _pEHCIRegisters->PortSC[i] = HostToUSBLong(portStat);
	    USBLog(7, "%s[%p]::SuspendUSBBus - port %d now suspended", getName(), this, i);
	}
	else
	{
	    USBLog(7, "%s[%p]::SuspendUSBBus - port %d not enabled", getName(), this, i);
	}
    }

    // clear run/stop
    usbcmd &= ~kEHCICMDRunStop;
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);
    _ehciBusState = kEHCIBusStateOff;
    USBLog(7, "%s[%p]::SuspendUSBBus - ports suspended, HC stop set, waiting for halted", getName(), this);
    
    // wait for halted bit
    do
    {
	usbsts = USBToHostLong(_pEHCIRegisters->USBSTS);
    } while (!(usbsts & kEHCIHCHaltedBit));
    
    USBLog(3, "%s[%p]::SuspendUSBBus - HC halted", getName(), this);
}



void			
AppleUSBEHCI::StopUSBBus()
{
    UInt32	usbcmd;
    
    usbcmd = USBToHostLong(_pEHCIRegisters->USBCMD);
    // clear run/stop
    usbcmd &= ~kEHCICMDRunStop;
    _pEHCIRegisters->USBCMD = HostToUSBLong(usbcmd);
    _ehciBusState = kEHCIBusStateOff;
    USBLog(1, "%s[%p]::StopUSBBus - HC halted", getName(), this);
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
    _ehciBusState = kEHCIBusStateRunning;
    USBLog(1, "%s[%p]::RestartUSBBus - HC restarted", getName(), this);
}


