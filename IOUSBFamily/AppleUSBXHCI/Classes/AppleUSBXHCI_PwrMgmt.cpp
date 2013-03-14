/*
 *  AppleUSBXHCI_PwrMgmt.cpp
 *
 *  Copyright © 2011 Apple Inc. All Rights Reserved.
 *
 */

//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>

#include "AppleUSBXHCIUIM.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#ifndef XHCI_USE_KPRINTF 
#define XHCI_USE_KPRINTF 0
#endif

#if XHCI_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= XHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#if (DEBUG_REGISTER_READS == 1)
#define Read32Reg(registerPtr, ...) Read32RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read32RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read32Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)


#define Read64Reg(registerPtr, ...) Read64RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read64RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read64Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)
#endif

#define super							IOUSBControllerV3
#define _controllerCanSleep				_expansionData->_controllerCanSleep

#pragma mark •••••••• Power Management ••••••••

//================================================================================================
//
//   CheckSleepCapability
//
//================================================================================================
//
void											
AppleUSBXHCI::CheckSleepCapability(void)
{
	// *****************
	// This stuff used to be done in initForPM. I could not coalesce the two methods, but I need the _controllerCanSleep calculation
	// earlier than that time, so I will do the calculaton here instead of there
	// *****************
	// assume that sleep is OK at first
	USBLog(2, "AppleUSBXHCI[%p]::CheckSleepCapability - assuming that I can sleep", this);
	_controllerCanSleep = true;
	_hasPCIPwrMgmt = false;
	
	//   We need to determine which XHCI controllers don't survive sleep.  These fall into 2 categories:
	//
	//   1.  ExpressCards
	//	 2.  PCI Cards that lose power (right now because of a bug in the PCI Family, USB PCI cards do not prevent
	//	     sleep, so even cards that don't support the PCI Power Mgmt stuff get their power removed.
	//
	//  So here, we look at all those cases and set the _unloadUIMAcrossSleep boolean to true.  As it turns out,
	//  if a controller does not have the "AAPL,clock-id" property, then it means that it cannot survive sleep.  We
	//  might need to refine this later once we figure how to deal with PCI cards that can go into PCI sleep mode.
	//  An exception is the B&W G3, that does not have this property but can sleep.  Sigh...
	
	//  Now, look at PCI cards.  Note that the onboard controller's provider is an IOPCIDevice so we cannot use that
	//  to distinguish between USB PCI cards and the on board controller.  Instead, we use the existence of the
	//  "AAPL,clock-id" property in the provider.  If it does not exist, then we are a XHCI controller on a USB PCI card.
	//
	if ( !_device->getProperty("AAPL,clock-id") )
	{
		// Added check for Thunderbolt attached controllers too here, as them may support D3 but may not be tagged in
		// the IO registry as built-in. If we don't do this then _hasPCIPowerMgmt flag would be set to false and on
		// system sleep it would be considered equal to a restart and we tear down the stack on every sleep.
		if (_device->getProperty("built-in") || (_device->getProperty(kIOPCITunnelledKey) == kOSBooleanTrue))
		{			
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
            USBError(1, "AppleUSBXHCI[%p]::CheckSleepCapability - controller will be unloaded across sleep",this);
            _controllerCanSleep = false;
            setProperty("Card Type","PCI");
        }
	}
	else
	{
		// old Apple ASICs come in here
		setProperty("Card Type","Built-in");
	}
}

//================================================================================================
//
//   ResetControllerState 
//
//      Called on system wake from Hibernate
//
//================================================================================================
//
IOReturn				
AppleUSBXHCI::ResetControllerState(void)
{
    IOReturn status = kIOReturnSuccess;
    int      i = 0;

    USBLog(2, "AppleUSBXHCI[%p]::ResetControllerState (num interrupts: %d, num primary: %d, inactive:%d, unavailable:%d, is controller available:%d lost register access:%d)", this, (int)_numInterrupts, (int)_numPrimaryInterrupts, (int)_numInactiveInterrupts, (int)_numUnavailableInterrupts, (int)_controllerAvailable, (int)_lostRegisterAccess);
    USBLog(2, "AppleUSBXHCI[%p]::ResetControllerState _myPowerState: %d _stateSaved %d", this, (uint32_t)_myPowerState, _stateSaved);

    if(_lostRegisterAccess)
    {
        return(kIOReturnNotResponding);
    }
    else
    {
        // Confirm that controller is Halted
        status = StopUSBBus();
        
        if( status != kIOReturnSuccess )
        {
            USBLog(1, "AppleUSBXHCI[%p]::ResetControllerState  StopUSBBus returned: %x", this, (uint32_t)status);
            return status;
        }
        
        _uimInitialized = false;
    }

	return(kIOReturnSuccess);
}


//================================================================================================
//
//   RestartControllerFromReset 
//
//      Called on system wake from Hibernate after ResetControllerState
//
//      Called after ::UIMInitialize and if _myPowerState == 0 from super class
//
//================================================================================================
//
IOReturn				
AppleUSBXHCI::RestartControllerFromReset(void)
{     
    USBLog(2, "AppleUSBXHCI[%p]::RestartControllerFromReset (num interrupts: %d, num primary: %d, inactive:%d, unavailable:%d, is controller available:%d lost register access:%d)", this, (int)_numInterrupts, (int)_numPrimaryInterrupts, (int)_numInactiveInterrupts, (int)_numUnavailableInterrupts, (int)_controllerAvailable, (int)_lostRegisterAccess);
    
    USBLog(2, "AppleUSBXHCI[%p]::RestartControllerFromReset _myPowerState: %d _stateSaved %d", this, (uint32_t)_myPowerState, _stateSaved);

    // TODO:: Similar to UIMInitialize, we have to collapse both together.
    if (!_uimInitialized)
    {
        IOReturn status = ResetController();

        if( status != kIOReturnSuccess )
        {
            USBLog(2, "AppleUSBXHCI[%p]::RestartControllerFromReset ResetController returned 0x%x", this, status);
            return status;
        }
        		
        UInt8    portIndex = 0, deviceIndex = 0;

		// If the WRC is set, clear it
		for ( portIndex=1; portIndex<=_rootHubNumPorts; portIndex++ )
		{
			UInt32		portSC = Read32Reg(&_pXHCIRegisters->PortReg[portIndex-1].PortSC);
			
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
			if (portSC & kXHCIPortSC_WRC)
			{
				portSC |= (UInt32) kXHCIPortSC_WRC;
				Write32Reg(&_pXHCIRegisters->PortReg[portIndex-1].PortSC, portSC);
				IOSync();
			}
			
			if ((portSC & kXHCIPortSC_CCS) && !(portSC & kXHCIPortSC_CSC))
			{
				// Intel Errata (rdar://10403564):  After a HRST, if we have a connection but no connection status change, then we need to fake it
				USBLog(1, "AppleUSBXHCI[%p]::UIMInitialize - PortReg[%d].PortSC: 0x%08x, has a CCS but no CSC", this, portIndex-1, (uint32_t)portSC);
				_synthesizeCSC[portIndex-1] = true;
			}
		}
		
        EnableXHCIPorts();
        
        _stateSaved = false;
        
        //
        // Deallocate all rings and reset all slots
        //
        UInt16 slot, endp;
        XHCIRing *ring = NULL;

        for( slot = 0; slot < _numDeviceSlots; slot++ )
        {
            if( _slots[slot].buffer != NULL )
            {
                for( endp = 1; endp < kXHCI_Num_Contexts; endp++ )
                {
                    // All other endpoints
                    ring = GetRing(slot, endp, 0);
                    
                    if(IsStreamsEndpoint(slot, endp))
                    {
                        if( _slots[slot].maxStream[endp] != 0 )
                        {
                            for( UInt16 streamsID = 1; streamsID <= _slots[slot].maxStream[endp]; streamsID++ )
                            {
                                XHCIRing *streamsRing = GetRing(slot, endp, streamsID);
                                
                                if(streamsRing != NULL)
                                {
                                    // DeallocRing - frees all IOUSBCommands
                                    DeallocRing(streamsRing);
                                }
                            }
                        }
                    }

                    if( ring != NULL )
                    {
                        // TODO:: Do we have any outstanding IOUSBCommands
                        
                        // DeallocRing - frees the array for holding IOUSBCommands
                        DeallocRing(ring);
                        IOFree(ring, sizeof(XHCIRing)* (_slots[slot].maxStream[endp]+1));
                        _slots[slot].potentialStreams[endp] = 0;
                        _slots[slot].maxStream[endp] = 0;
                        _slots[slot].rings[endp] = NULL;
                    }                        
                    
                }
                
                // Do this first to mark its deleted.
                _slots[slot].deviceContext = NULL;    
                _slots[slot].deviceContext64 = NULL;
				
                // Relase the output context
                _slots[slot].buffer->complete();
                _slots[slot].buffer->release();
                _slots[slot].buffer = 0;
                _slots[slot].deviceContextPhys = 0;
            }
        } // end of deallocate slot and endpoint 
        
        
        // Initialise some state variables
		for ( deviceIndex = 0; deviceIndex < kMaxDevices; deviceIndex++ )
		{
			_devHub[deviceIndex] = 0;
			_devPort[deviceIndex] = 0;
			_devMapping[deviceIndex] = 0;
			_devEnabled[deviceIndex] = false;
		}
        
		for ( portIndex = 0 ; portIndex < kMaxPorts; portIndex++ )
		{
			_prevSuspend[portIndex] = false;
			_suspendChangeBits[portIndex] = false;
		}
        
		UInt32 configReg = Read32Reg(&_pXHCIRegisters->Config);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
        configReg =  (configReg & ~kXHCINumDevsMask) | _numDeviceSlots;
		
		Write32Reg(&_pXHCIRegisters->Config, configReg);
        
        // Turn on all device notifications, they'll be logged in PollEventring
		Write32Reg(&_pXHCIRegisters->DNCtrl, 0xFFFF);
        
        USBLog(3, "AppleUSBXHCI[%p]::RestartControllerFromReset - DCBAA - pPhysical[%p] pLogical[%p]", this, (void*)_DCBAAPhys, _DCBAA);
        
        Write64Reg(&_pXHCIRegisters->DCBAAP, _DCBAAPhys);
        
        USBLog(3, "AppleUSBXHCI[%p]::RestartControllerFromReset - CMD Ring - pPhysical[%p] pLogical[%p], num CMDs: %d", this, (void*)_CMDRingPhys, _CMDRing, _numCMDs);
        
        InitCMDRing();
        
        USBLog(3, "AppleUSBXHCI[%p]::RestartControllerFromReset - _ERSTMax  %d", this, _ERSTMax);

        USBLog(3, "AppleUSBXHCI[%p]::RestartControllerFromReset - Event Ring %d - pPhysical[%p] pLogical[%p], num Events: %d", this, kPrimaryInterrupter, (void*)_events[kPrimaryInterrupter].EventRingPhys, _events[kPrimaryInterrupter].EventRing, _events[kPrimaryInterrupter].numEvents);        
        
        InitEventRing(kPrimaryInterrupter, true);
        
        USBLog(3, "AppleUSBXHCI[%p]::RestartControllerFromReset - Event Ring %d - pPhysical[%p] pLogical[%p], num Events: %d", this, kTransferInterrupter, (void*)_events[kTransferInterrupter].EventRingPhys, _events[kTransferInterrupter].EventRing, _events[kTransferInterrupter].numEvents);        
        
        InitEventRing(kTransferInterrupter, true);
        
        if(_numScratchpadBufs != 0)
        {
            _DCBAA[0] = _SBAPhys;
        }
        
		_CCEPhysZero = 0;
		_CCEBadIndex = 0;
		_EventChanged = 0;
		_IsocProblem = 0;

		_stateSaved = false;
		_fakedSetaddress = false;
		
		_filterInterruptActive = false;
		_frameNumber64 = 0;
		_numInterrupts = 0;
		_numPrimaryInterrupts = 0;
		_numInactiveInterrupts = 0;
		_numUnavailableInterrupts = 0;
        
        
        _uimInitialized = true;
    }
    
    return(kIOReturnSuccess);
}

//================================================================================================
//
//   SaveAnInterrupter 
//
//      Save an interruptor set (hardware registers)
//
//================================================================================================
//
void AppleUSBXHCI::SaveAnInterrupter(int IRQ)
{
 	_savedInterrupter[IRQ].ERSTSZ = Read32Reg(&_pXHCIRuntimeReg->IR[IRQ].ERSTSZ);
	_savedInterrupter[IRQ].ERSTBA = Read64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERSTBA);
	_savedInterrupter[IRQ].ERDP = Read64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERDP);
	_savedInterrupter[IRQ].IMAN = Read32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMAN);
	_savedInterrupter[IRQ].IMOD = Read32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMOD);
   
}

//================================================================================================
//
//   RestoreAnInterruptor 
//
//      Restore an interruptor set (hardware registers)
//
//================================================================================================
//
void AppleUSBXHCI::RestoreAnInterrupter(int IRQ)
{
    Write32Reg(&_pXHCIRuntimeReg->IR[IRQ].ERSTSZ, _savedInterrupter[IRQ].ERSTSZ);
    Write64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERSTBA, _savedInterrupter[IRQ].ERSTBA);
    Write64Reg(&_pXHCIRuntimeReg->IR[IRQ].ERDP, _savedInterrupter[IRQ].ERDP);
    Write32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMAN, _savedInterrupter[IRQ].IMAN);
    Write32Reg(&_pXHCIRuntimeReg->IR[IRQ].IMOD, _savedInterrupter[IRQ].IMOD);
}

//================================================================================================
//
//   SaveControllerStateForSleep 
//
//      Called on the way to system Sleep and Hibernate
//
//================================================================================================
//
IOReturn				
AppleUSBXHCI::SaveControllerStateForSleep(void)
{
    USBLog(2, "AppleUSBXHCI[%p]::SaveControllerStateForSleep  _myPowerState: %d", this, (uint32_t)_myPowerState);
    
    IOReturn status = kIOReturnSuccess;
	UInt32	 CMD, count=0;
        
	// Confirm that controller is Halted
	status = StopUSBBus();
	
	if( status != kIOReturnSuccess )
    {
        USBLog(1, "AppleUSBXHCI[%p]::SaveControllerStateForSleep  StopUSBBus returned: %x", this, (uint32_t)status);
		return status;
	}
    
	_savedRegisters.USBCMD = Read32Reg(&_pXHCIRegisters->USBCMD);
	_savedRegisters.DNCtrl = Read32Reg(&_pXHCIRegisters->DNCtrl);
	_savedRegisters.DCBAAP = Read64Reg(&_pXHCIRegisters->DCBAAP);
	_savedRegisters.Config = Read32Reg(&_pXHCIRegisters->Config);
	
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
    
	SaveAnInterrupter(kPrimaryInterrupter);
	SaveAnInterrupter(kTransferInterrupter);
    
    // Section 4.23.2 of XHCI doesn't require us to save/restore the CRCR state.
	//_savedRegisters.CRCR = Read64Reg(&_pXHCIRegisters->CRCR);
	
	CMD = Read32Reg(&_pXHCIRegisters->USBCMD);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	CMD |= kXHCICMDCSS;
	Write32Reg(&_pXHCIRegisters->USBCMD, CMD);
	
	count = 0;
	UInt32 usbSts = Read32Reg(&_pXHCIRegisters->USBSTS);
	while((!_lostRegisterAccess) && (usbSts & kXHCISSS))
	{
		IOSleep(1);
		if(count++ >100)
		{
			USBLog(1, "AppleUSBXHCI[%p]::SaveControllerStateForSleep - Controller not saved state after 100ms", this);
			return (kIOReturnInternalError);
		}
		
		usbSts = Read32Reg(&_pXHCIRegisters->USBSTS);
	}
    
    if (_lostRegisterAccess)
    {
        return kIOReturnNoDevice;
    }
	
	
	usbSts = Read32Reg(&_pXHCIRegisters->USBSTS);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	if(usbSts & kXHCISRE)
	{
		USBLog(1, "AppleUSBXHCI[%p]::SaveControllerStateForSleep - Error saving controller state", this);
		return (kIOReturnInternalError);
	}
	
	_stateSaved = true;
	USBLog(2, "AppleUSBXHCI[%p]::SaveControllerStateForSleep - state saved", this);
	PrintRuntimeRegs();
    
    return kIOReturnSuccess;
}


//================================================================================================
//
//   RestoreControllerStateFromSleep 
//
//      Called on system wake from Sleep
//
//================================================================================================
//
IOReturn				
AppleUSBXHCI::RestoreControllerStateFromSleep(void)
{
    int slot, endp;
	UInt32 CMD, STS, count=0;
    UInt32 portIndex;
	volatile UInt32 val = 0;
	volatile UInt32 * addr;
	
    USBLog(2, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep _myPowerState: %d _stateSaved %d", this, (uint32_t)_myPowerState, _stateSaved);
	PrintRuntimeRegs();

    UInt32		sts = Read32Reg(&_pXHCIRegisters->USBSTS);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	

	
	// At this point, interrupts are disabled, and we are waking up. If the Port Change Detect bit is active
	// then it is likely that we are responsible for the system issuing the wakeup
	if (sts & kXHCIPCD)
	{
		UInt32			port;
		
		for (port=0; port < _rootHubNumPorts; port++)
		{
			UInt32	portSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
			if (portSC & kXHCIPortSC_CSC)
			{
				if (portSC & kXHCIPortSC_PED)
				{
					USBError(1, "USB (XHCI):Port %d on bus 0x%x - connect status changed but still enabled. clearing enable bit: portSC(0x%x)\n", (int)port+1, (uint32_t)_busNumber, (uint32_t)portSC);
					portSC = GetPortSCForWriting(port+1);
					portSC |= (UInt32)kXHCIPortSC_PEC;
					Write32Reg(&_pXHCIRegisters->PortReg[port].PortSC, portSC);
				}
				else
				{
					IOLog("USB (%s):Port %d on bus 0x%x connected or disconnected: portSC(0x%x)\n", _rootHubDevice ? _rootHubDevice->getName() : "XHCI", (int)port+1, (uint32_t)_busNumber, (uint32_t)portSC);
					USBLog(5, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep  Port %d on bus 0x%x (%s)- connected or disconnected, calling EnsureUsability()", this, (int)port+1, (uint32_t)_busNumber, _rootHubDevice ? _rootHubDevice->getName() : "XHCI");
					EnsureUsability();
				}
			}
			else if ( ((portSC & kXHCIPortSC_LinkState_Mask) >> kXHCIPortSC_LinkState_Shift) == kXHCIPortSC_PLS_Resume)
			{
				
				USBLog(5, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep  Port %d on bus 0x%x - has remote wakeup from some device", this, (int)port+1, (uint32_t)_busNumber);
				
				// Because of how XHCI works, the root hub driver might not be able to detect that there was a remote wakeup 
				// on a port if the upper level driver issues a Resume before the root hub interrupt timer runs
				// Let the hub driver know that from here to make sure we get the log
				
				if (_rootHubDevice && _rootHubDevice->GetPolicyMaker())
				{
					_rootHubDevice->GetPolicyMaker()->message(kIOUSBMessageRootHubWakeEvent, this, (void *)(uintptr_t) port);
				}
				else
				{
					IOLog("\tUSB (XHCI):Port %d on bus 0x%x has remote wakeup from some device\n", (int)port+1, (uint32_t)_busNumber);
				}
				
				// Clear the PLC bit if set
				portSC	= GetPortSCForWriting(port+1);
				if (_lostRegisterAccess)
				{
					return kIOReturnNoDevice;
				}
				
				portSC |= (UInt32)kXHCIPortSC_PLC;
				Write32Reg(&_pXHCIRegisters->PortReg[port].PortSC, portSC);
			}
			else if (portSC & kXHCIPortSC_PED)
			{
				USBLog(5, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep  Port %d on bus 0x%x is enabled but not handling portSC of 0x%x", this, (int)port+1, (uint32_t)_busNumber, (uint32_t)portSC);
			}
			
			// In EHCI, we do an "else" check here for a port that is enabled but not suspended.  
            // However, it seems that in XHCI when we get here we are already in resume, so a
			// check for being in PLS of U3 does not make sense
		}
	}
    
    // Restore XHCI run time registers
	if(_stateSaved)
	{
        // Step 4
        USBLog(2, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep - restoring saved state", this);
		Write32Reg(&_pXHCIRegisters->USBCMD, _savedRegisters.USBCMD);
		Write32Reg(&_pXHCIRegisters->DNCtrl, _savedRegisters.DNCtrl);
        
        // Section 4.23.2 of XHCI doesn't require us to save/restore the CRCR state.
		Write64Reg(&_pXHCIRegisters->DCBAAP, _savedRegisters.DCBAAP);
		Write32Reg(&_pXHCIRegisters->Config, _savedRegisters.Config);
		
        RestoreAnInterrupter(kPrimaryInterrupter);
        RestoreAnInterrupter(kTransferInterrupter);
		
        // Step 5
		CMD = Read32Reg(&_pXHCIRegisters->USBCMD);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
		
		CMD |= kXHCICMDCRS;
		Write32Reg(&_pXHCIRegisters->USBCMD, CMD);
		
		count = 0;
		STS = Read32Reg(&_pXHCIRegisters->USBSTS);
		
		while((!_lostRegisterAccess) && (STS & kXHCIRSS))
		{
			IOSleep(1);
			if(count++ >100)
			{
				USBLog(1, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep - Controller state not restored  after 100ms", this);
				break ;
			}
			STS = Read32Reg(&_pXHCIRegisters->USBSTS);
		}
		
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
				
        // Step 6
        InitCMDRing();
	
		STS = Read32Reg(&_pXHCIRegisters->USBSTS);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}

		if(STS & kXHCISRE)
		{
			int i;
			USBLog(1, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep - Error restoring controller state USBSTS = 0x%x", this, STS);
			Write32Reg(&_pXHCIRuntimeReg->IR[kPrimaryInterrupter].IMAN, 0);	// Disable the interrupt.
			Write32Reg(&_pXHCIRuntimeReg->IR[kTransferInterrupter].IMAN, 0);	// Disable the interrupt.
			Write32Reg(&_pXHCIRegisters->USBCMD, 0);  		// this sets r/s to stop
			IOSync();
			
			STS = Read32Reg(&_pXHCIRegisters->USBSTS);
			for (i=0; (!_lostRegisterAccess) && (i < 100) && !(STS & kXHCIHCHaltedBit); i++)
			{
				IOSleep(1);
				STS = Read32Reg(&_pXHCIRegisters->USBSTS);
			}
			
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
			if (i >= 100)
			{
				USBError(1, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep - could not get chip to halt within 100 ms",  this);
				return(kIOReturnInternalError);
			}
		}
        
		_stateSaved = false;
	}
	
	STS = Read32Reg(&_pXHCIRegisters->USBSTS);
	if( (STS & kXHCIHCHaltedBit) || (STS & kXHCIHSEBit) )
	{
        // TODO :: We need to tell the super that controller we could not recover due to HSE error, so we need
        // it to call ResetControllerState
	}
	
    DisableComplianceMode();
        
    // Restart all endpoints
    for(slot = 0; slot<_numDeviceSlots; slot++)
	{
		if(_slots[slot].buffer != NULL)
		{
			for(endp = 1; endp<kXHCI_Num_Contexts; endp++)
			{
				XHCIRing *ring;
				ring = GetRing(slot, endp, 0);
				if( (ring != NULL) && (ring->TRBBuffer != NULL) )
				{
                    if(IsStreamsEndpoint(slot, endp))
					{
                        USBLog(5, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep - restart stream ep=%d", this, endp);
						RestartStreams(slot, endp, 0);
					}
					else
					{
                        USBLog(5, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep - slot=%d doorbell4ep=%d", this, slot, endp);
                        StartEndpoint(slot, endp);
					}
				}
			}
		}
	}
    
    // Deal with any port with CAS (Cold Attach Status) set
	for (portIndex=0; portIndex < _rootHubNumPorts; portIndex++)
	{
        UInt32	portSC;
        
		portSC = Read32Reg(&_pXHCIRegisters->PortReg[portIndex].PortSC);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
		
		if (portSC & kXHCIPortSC_CAS)
		{
			USBLog(5, "AppleUSBXHCI[%p]::RestoreControllerStateFromSleep  Port %d on bus 0x%x has CAS bit set (0x%08x), issuing a Warm Reset", this, (int)portIndex+1, (uint32_t)_busNumber, (uint32_t)portSC);
			
			portSC = GetPortSCForWriting(portIndex+1);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
			portSC |= (UInt32)kXHCIPortSC_WPR;
			Write32Reg(&_pXHCIRegisters->PortReg[portIndex].PortSC, portSC);
			IOSync();
			IOSleep(50);
		}
	}
	
    return kIOReturnSuccess;
}

//================================================================================================
//
//   DozeController 
//
//      
//
//================================================================================================
//
enum  {
	SWAXHCIReg		=	0x40,						// offset in config space
	SWAXHCIValue	=	0x800						// value (bit 11) to mask in/out
};



IOReturn				
AppleUSBXHCI::DozeController(void)
{
	
	// Previous Host Controllers would halt the controller when going to doze
	// That is not allowed with XHCI controllers.
    USBLog(2, "AppleUSBXHCI[%p]::DozeController _myPowerState: %d, _externalDeviceCount %d", this, (uint32_t)_myPowerState, (int)_v3ExpansionData->_externalDeviceCount);
	
	
    return kIOReturnSuccess;
}



//================================================================================================
//
//   WakeControllerFromDoze 
//
//      
//
//================================================================================================
//
IOReturn				
AppleUSBXHCI::WakeControllerFromDoze(void)
{
	int				i;
	UInt32			port;
	bool			somePortNeedsToResume = false;
	
	// Previous Host Controllers would halt the controller when going to doze
	// That is not allowed with XHCI controllers.
    USBLog(2, "AppleUSBXHCI[%p]::WakeControllerFromDoze _myPowerState: %d", this, (uint32_t)_myPowerState);

	// check to see if we have a pending resume on any port and if so, wait for 20ms
	for (port = 0; port < _rootHubNumPorts; port++)
	{
		UInt32 PLS, portSC;
		portSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
		
		PLS = (UInt32)(portSC & kXHCIPortSC_LinkState_Mask) >> kXHCIPortSC_LinkState_Shift;
		if (PLS == kXHCIPortSC_PLS_Resume)
		{
			USBLog(5, "AppleUSBXHCI[%p]::WakeControllerFromDoze - port %d appears to be resuming from a remote wakeup", this, (int)port+1);
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


IOReturn				
AppleUSBXHCI::EnableInterruptsFromController(bool enable)
{
    UInt32      CMD = Read32Reg(&_pXHCIRegisters->USBCMD);

    USBLog(2, "AppleUSBXHCI[%p]::EnableInterruptsFromController (num interrupts: %d, num primary: %d, inactive:%d, unavailable:%d, is controller available:%d lost register access:%d)", this, (int)_numInterrupts, (int)_numPrimaryInterrupts, (int)_numInactiveInterrupts, (int)_numUnavailableInterrupts, (int)_controllerAvailable, (int)_lostRegisterAccess);
	
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
    
	if (enable)
	{
		USBLog(2, "AppleUSBXHCI[%p]::EnableInterruptsFromController - enabling interrupts, USBCMD(%p) INTE(%s)", this, (void*)CMD,
               (CMD & kXHCICMDINTE) ? "true":"false" );
        
		RestartUSBBus();
	}
	else
	{
		CMD &= ~kXHCICMDINTE; 
		Write32Reg(&_pXHCIRegisters->USBCMD, CMD);
		IOSync();
		USBLog(2, "AppleUSBXHCI[%p]::EnableInterruptsFromController - interrupts disabled, USBCMD(%p) INTE(%s)", this, (void*)CMD,
               (CMD & kXHCICMDINTE) ? "true":"false" );
	}
	
	return kIOReturnSuccess;
}

IOReturn
AppleUSBXHCI::QuiesceAllEndpoints ( )
{   
    IOReturn ret = kIOPMAckImplied;
    int slot, endp;
    USBLog(5, "AppleUSBXHCI[%p]::QuiesceAllEndpoints", this);
    
    // radar://10439459 - Clear CSC bit if not set, only for PPT
    if( (_errataBits & kXHCIErrataPPT) != 0 )
    {
        int portIndex = 0;
        
        for (portIndex=0; portIndex < _rootHubNumPorts; portIndex++ )
        {
            UInt32		portSC = Read32Reg(&_pXHCIRegisters->PortReg[portIndex].PortSC);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
            
            // If the CSC is not set on the way to sleep, clear it
            if ( !(portSC & kXHCIPortSC_CSC) )
            {
                portSC = GetPortSCForWriting(portIndex+1);
				if (_lostRegisterAccess)
				{
					return kIOReturnNoDevice;
				}
				
                portSC |= (UInt32) kXHCIPortSC_CSC;
                Write32Reg(&_pXHCIRegisters->PortReg[portIndex].PortSC, portSC);
                IOSync();
            }
        }
    }
    
    // Stop all endpoints
    for(slot = 0; slot<_numDeviceSlots; slot++)
    {
        if(_slots[slot].buffer != NULL)
        {
            for(endp = 1; endp<kXHCI_Num_Contexts; endp++)
            {
                XHCIRing *ring;
                ring = GetRing(slot, endp, 0);
                if( (ring != NULL) && (ring->TRBBuffer != NULL) )
                {
                    USBLog(1, "AppleUSBXHCI[%p]::QuiesceAllEndpoints calling QuiesceEndpoint = %d, %d ", this, slot, endp);
                    QuiesceEndpoint(slot, endp);
                }
            }
        }
    }
    
    int commandRingRunning = (int)Read64Reg(&_pXHCIRegisters->CRCR);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
    
    if( commandRingRunning & kXHCI_CRR )
    {
        
        int count = 0;
        // Stop the command ring
        Write64Reg(&_pXHCIRegisters->CRCR, (_CMDRingPhys & ~kXHCICRCRFlags_Mask) | kXHCI_CS);	
        
        _waitForCommandRingStoppedEvent = true;
        while (_waitForCommandRingStoppedEvent)
        {
            USBLog(1, "AppleUSBXHCI[%p]::QuiesceAllEndpoints waiting for command ring stop, count = %d", this, count);
            PollForCMDCompletions(kPrimaryInterrupter);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
            count++;
            if ( count > 100 )
                break;
            IOSleep(1);
        }
    }
    
    return (ret);
}


void
AppleUSBXHCI::ControllerSleep ( void )
{
    USBLog(5, "AppleUSBXHCI[%p]::ControllerSleep", this);
    if(_myPowerState == kUSBPowerStateLowPower)
        WakeControllerFromDoze();
    
    QuiesceAllEndpoints();
    
    EnableInterruptsFromController(false);
    SaveControllerStateForSleep();
}
    

//================================================================================================
//
//   powerChangeDone
//
//================================================================================================
//
void
AppleUSBXHCI::powerChangeDone ( unsigned long fromState)
{
	unsigned long newState = getPowerState();
	
	USBLog((fromState == newState) || !_controllerAvailable ? 7 : 4, "AppleUSBXHCI[%p]::powerChangeDone from state (%d) to state (%d) _controllerAvailable(%s)", this, (int)fromState, (int)newState, _controllerAvailable ? "true" : "false");
    
	if (_wakingFromHibernation)
	{
        USBLog(5, "AppleUSBXHCI[%p]::powerChangeDone - _wakingFromHibernation is set", this);
	}
    
	super::powerChangeDone(fromState);
}
