/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>

#include <UserNotification/KUNCUserNotifications.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBPipe.h>

#include "AppleUSBHub.h"
#include "AppleUSBHubPort.h"

#define super IOService
#define self this
#define DEBUGGING_LEVEL 0

static ErrataListEntry	errataList[] = {

/* For the Cherry 4 port KB, From Cherry:
We use the bcd_releasenumber-highbyte for hardware- and the lowbyte for
firmwarestatus. We have 2 different for the hardware 03(eprom) and
06(masked microcontroller). Firmwarestatus is 05 today.
So high byte can be 03 or 06 ----  low byte can be 01, 02, 03, 04, 05

Currently we are working on a new mask with the new descriptors. The
firmwarestatus will be higher than 05.
*/
      {0x046a, 0x003, 0x0301, 0x0305, kErrataCaptiveOKBit}, // Cherry 4 port KB
      {0x046a, 0x003, 0x0601, 0x0605, kErrataCaptiveOKBit}  // Cherry 4 port KB
};

#define errataListLength (sizeof(errataList)/sizeof(ErrataListEntry))


OSDefineMetaClassAndStructors(AppleUSBHub, IOService)

bool 
AppleUSBHub::init( OSDictionary * propTable )
{
    if( !super::init(propTable))
        return (false);

    _numCaptive = 0;
    _startupDelay = 0;
    _timerSource = NULL;
    _gate = NULL;
    _portSuspended = false;
    _hubHasBeenDisconnected = false;
    _hubIsDead = false;
    _workThread = NULL;
    _resetPortZeroThread = NULL;
    _hubDeadCheckThread =  NULL;
    _busPowerGood = false;
    _powerForCaptive = 0;
    _numCaptive = 0;
    _outstandingIO = 0;
    _needToClose = false;
   
    return(true);
}



bool 
AppleUSBHub::start(IOService * provider)
{
    IOReturn			err = 0;
    IOUSBRootHubDevice		*rootHub;
    OSDictionary		*providerDict;
    IOWorkLoop 			*workLoop = NULL;    
    OSNumber 			* errataProperty;
    const IORegistryPlane	*usbPlane = NULL;
    
    _inStartMethod = true;
    IncrementOutstandingIO();		// make sure we don't close until start is done
    
    if( !super::start(provider))
    {	
        goto ErrorExit;
    }

    // Create the timeout event source
    //
    _timerSource = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action) TimeoutOccurred);

    if ( _timerSource == NULL )
    {
        USBError(1, "%s::start Couldn't allocate timer event source", getName());
        goto ErrorExit;
       // return kIOReturnInternalError;
    }

    _gate = IOCommandGate::commandGate(this);

    if(!_gate)
    {
	USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
        goto ErrorExit;
    }

   workLoop = getWorkLoop();
    if ( !workLoop )
    {
        USBError(1, "%s::start Couldn't get provider's workloop", getName());
        goto ErrorExit;
    }
   
    if ( workLoop->addEventSource( _timerSource ) != kIOReturnSuccess )
    {
        USBError(1, "%s::start Couldn't add timer event source", getName());
        goto ErrorExit;
    }
    
    if ( workLoop->addEventSource( _gate ) != kIOReturnSuccess )
    {
        USBError(1, "%s::start Couldn't add gate event source", getName());
        goto ErrorExit;
    }
    
    // remember my device
    _device		= (IOUSBDevice *) provider;
    _address		= _device->GetAddress();
    _bus		= _device->GetBus();

    providerDict = (OSDictionary*)getProperty("IOProviderMergeProperties");
    if (providerDict)
             provider->getPropertyTable()->merge(providerDict);		// merge will verify that this really is a dictionary

    if (!_device->open(this))
    {
        USBError(1, "%s::start unable to open provider", getName());
        goto ErrorExit;
    }

    // Check to see if we have an errata for the startup delay and if so,
    // then sleep for that amount of time.
    //
    errataProperty = (OSNumber *)getProperty("kStartupDelay");
    if ( errataProperty )
    {
        _startupDelay = errataProperty->unsigned32BitValue();
        IOSleep( _startupDelay );
    }
    
    // Get the other errata that are not personality based
    //
    _errataBits = GetHubErrataBits();
    
    // Go ahead and configure the hub
    //
    err = ConfigureHub();
    
    if ( err == kIOReturnSuccess )
    {
        rootHub = OSDynamicCast(IOUSBRootHubDevice, provider);
        if (rootHub)
        {
            // if my provider is an IOUSBRootHubDevice nub, then I should attach this hub device nub to the root.
            //
            usbPlane = getPlane(kIOUSBPlane);
            
            if (usbPlane)
            {
                rootHub->attachToParent(getRegistryRoot(), usbPlane);
            }
            
        }
        
        // allocate a thread_call structure
        _workThread = thread_call_allocate((thread_call_func_t)ProcessStatusChangedEntry, (thread_call_param_t)this);
        _resetPortZeroThread = thread_call_allocate((thread_call_func_t)ResetPortZeroEntry, (thread_call_param_t)this);
        _hubDeadCheckThread = thread_call_allocate((thread_call_func_t)CheckForDeadHubEntry, (thread_call_param_t)this);
        _clearFeatureEndpointHaltThread = thread_call_allocate((thread_call_func_t)ClearFeatureEndpointHaltEntry, (thread_call_param_t)this);
        
        if ( !_workThread || !_resetPortZeroThread || !_hubDeadCheckThread || !_clearFeatureEndpointHaltThread )
        {
            USBError(1, "[%p] %s could not allocate all thread functions.  Aborting start", this, getName());
            goto ErrorExit;
        }
   
        USBError(1, "[%p] USB Generic Hub @ %d (0x%x)", this, _address, strtol(_device->getLocation(), (char **)NULL, 16));

	_inStartMethod = false;
        DecrementOutstandingIO();
	err = RearmInterruptRead();
	if (err == kIOReturnSuccess)
	    return true;
    }
    else
    {
    
        USBError(1,"[%p] %s::start Aborting startup: error 0x%x", this, getName(), err);
        if ( _device && _device->isOpen(this) )
            _device->close(this);
        stop(provider);
    }

ErrorExit:
    
    if ( _timerSource )
    {
        if ( workLoop )
            workLoop->removeEventSource(_timerSource);
        _timerSource->release();
        _timerSource = NULL;
    }

    if (_gate)
    {
	if (workLoop)
	    workLoop->removeEventSource(_gate);
	    
	_gate->release();
	_gate = NULL;
    }
    
    _inStartMethod = false;
    DecrementOutstandingIO();
    return false;
}



void 
AppleUSBHub::stop(IOService * provider)
{
    IOWorkLoop * workLoop = getWorkLoop();
   
    if (_buffer) 
    {
        _buffer->release();
	_buffer = 0;
    }
    if(_hubInterface) 
    {
        _hubInterface->close(this);
        _hubInterface->release();
        _hubInterface = NULL;
    }
    if (_timerSource)
    {
        if ( workLoop )
            workLoop->removeEventSource(_timerSource);
            
        _timerSource->release();
        _timerSource = NULL;
    }
    
    if (_gate)
    {
	if (workLoop)
	    workLoop->removeEventSource(_gate);
	    
	_gate->release();
	_gate = NULL;
    }
    
    if (_workThread)
    {
        thread_call_cancel(_workThread);
        thread_call_free(_workThread);
    }
    
    if (_resetPortZeroThread)
    {
        thread_call_cancel(_resetPortZeroThread);
        thread_call_free(_resetPortZeroThread);
    }
    
    if (_hubDeadCheckThread)
    {
        thread_call_cancel(_hubDeadCheckThread);
        thread_call_free(_hubDeadCheckThread);
    }
    
    if (_clearFeatureEndpointHaltThread)
    {
        thread_call_cancel(_clearFeatureEndpointHaltThread);
        thread_call_free(_clearFeatureEndpointHaltThread);
    }

    if (_device)
    {
        // Set it to NULL, since our provider will go away after this stop call
        //
        _device = 0;
    }

    super::stop(provider);
}

IOReturn
AppleUSBHub::ConfigureHub()
{
    IOReturn 				err = kIOReturnSuccess;
    IOUSBFindInterfaceRequest		req;
    const IOUSBConfigurationDescriptor *cd;

    // Reset some of our variables that so that when we reconfigure due to a reset
    // we don't reuse old values
    //
    _busPowerGood = false;
    _powerForCaptive = 0;
    _numCaptive = 0;

    // Find the first config/interface
    if (_device->GetNumConfigurations() < 1)
    {
        USBError(1,"[%p] %s::ConfigureHub No hub configurations", this, getName());
        err = kIOReturnNoResources;		// Need better error
        goto ErrorExit;
    }

    // set the configuration to the first config
    cd = _device->GetFullConfigurationDescriptor(0);
    if (!cd)
    {
        USBError(1,"[%p] %s::ConfigureHub No config descriptor", this, getName());
        err = kIOUSBConfigNotFound;
        goto ErrorExit;
    }

    err = _device->SetConfiguration(this, cd->bConfigurationValue, false);
    
    if (err)
    {
        USBError(1,"[%p] %s::ConfigureHub SetConfiguration failed. Error 0x%x", this, getName(), err);
        goto ErrorExit;
    }
        

    // Find the interface for our hub -- there's only one
    //
    req.bInterfaceClass = kUSBHubClass;
    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    if ((_hubInterface = _device->FindNextInterface(NULL, &req)) == 0)
    {
        USBError(1,"[%p] %s::ConfigureHub no interface found", this, getName());
        err = kIOUSBInterfaceNotFound;
        goto ErrorExit;
    }
    
    _hubInterface->retain();
    
    
    _busPowered = (cd->bmAttributes & kUSBAtrBusPowered) ? TRUE : FALSE;	//FIXME
    _selfPowered = (cd->bmAttributes & kUSBAtrSelfPowered) ? TRUE : FALSE;

    if( !(_busPowered || _selfPowered) )
    {
        USBError(1,"[%p] %s::ConfigureHub illegal device config - no power", this, getName());
        err = kIOReturnNoPower;		// Need better error code here.
        goto ErrorExit;
    }

    // Get the hub descriptor
    if ( (err = GetHubDescriptor(&_hubDescriptor)) )
    {
        USBError(1,"[%p] %s::ConfigureHub could not get hub descriptor (0x%x)", this, getName(), err);
        goto ErrorExit;
    }
    
    if(_hubDescriptor.numPorts < 1)
    {
        USBLog(1,"[%p] %s::start there are no ports on this hub", this, getName());
    }
    if(_hubDescriptor.numPorts > 7)
    {
        USBLog(1,"[%p] %s::start there are an aweful lot of ports (%d) on this hub", this, getName(), _hubDescriptor.numPorts);
    }
    _readBytes = ((_hubDescriptor.numPorts + 1) / 8) + 1;
    
    // Setup for reading status pipe
    _buffer = IOBufferMemoryDescriptor::withCapacity(_readBytes, kIODirectionIn);

    if (!_hubInterface->open(this))
    {
        USBError(1,"[%p] %s::ConfigureHub could not open hub interface", this, getName());
        err = kIOReturnNotOpen;
        goto ErrorExit;
    }

    IOUSBFindEndpointRequest request;
    request.type = kUSBInterrupt;
    request.direction = kUSBIn;
    _interruptPipe = _hubInterface->FindNextPipe(NULL, &request);

    if(!_interruptPipe)
    {
        USBError(1,"[%p] %s::ConfigureHub could not find interrupt pipe", this, getName());
        err = kIOUSBNotEnoughPipesErr;		// Need better error code here.
        goto ErrorExit;
    } 
    
    // prepare the ports
    UnpackPortFlags();
    CountCaptivePorts();
#if (DEBUGGING_LEVEL > 0)
    PrintHubDescriptor(&_hubDescriptor);
#endif
    err = CheckPortPowerRequirements();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"[%p] %s::ConfigureHub CheckPortPowerRequirements failed with 0x%x", this, getName(), err);
        goto ErrorExit;
    }
    
    err = AllocatePortMemory();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"[%p] %s::ConfigureHub AllocatePortMemory failed with 0x%x", this, getName(), err);
        goto ErrorExit;
    }
        
    err = StartPorts();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"[%p] %s::ConfigureHub StartPorts failed with 0x%x", this, getName(), err);
        goto ErrorExit;
    }
        
    // Start the timeout Timer
    if (_timerSource)
    {
        // retain();
        _timerSource->setTimeoutMS(5000); 
    }

    // start an async read
    //
    _hubIsDead = FALSE;
                                
ErrorExit:

    return err;
}


/**********************************************************************
 **
 ** HUB FUNCTIONS
 **
 **********************************************************************/
void 
AppleUSBHub::UnpackPortFlags(void)
{
    int i;

    int numFlags = ((_hubDescriptor.numPorts + 1) / 8) + 1;
    for(i = 0; i < numFlags; i++)
    {
        _hubDescriptor.pwrCtlPortFlags[i] = _hubDescriptor.removablePortFlags[numFlags+i];
        _hubDescriptor.removablePortFlags[numFlags+i] = 0;
    }
}



void 
AppleUSBHub::CountCaptivePorts(void)
{
    int 		portMask = 2;
    int 		portByte = 0;
    int			currentPort;


    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        /* determine if the port is a captive port */
        if ((_hubDescriptor.removablePortFlags[portByte] & portMask) != 0)
            _numCaptive++;		// save this for power calculations

        portMask <<= 1;
        if(portMask > 0x80)
        {
            portMask = 1;
            portByte++;
        }
    }
}



/*
 ExtPower   Good      off

Bus  Self

0     0     Illegal config
1     0     Always 100mA per port
0     1     500mA     0 (dead)
1     1     500      100
*/
IOReturn 
AppleUSBHub::CheckPortPowerRequirements(void)
{
    IOReturn	err = kIOReturnSuccess;
    /* Note hub current in units of 1mA, everything else in units of 2mA */
    UInt32	hubPower = _hubDescriptor.hubCurrent/2;
    UInt32	busPower = _device->GetBusPowerAvailable();
    UInt32	powerAvailForPorts = 0;
    UInt32	powerNeededForPorts = 0;
    bool	startExternal;

    do
    {
        if (hubPower > busPower)
        {
            USBLog(3, "%s [%p] Hub needs more power (%d > %d) than available", getName(), this, hubPower, busPower);
            _busPowerGood = false;
            _powerForCaptive = 0;
            err = kIOReturnNoPower;
        }
        else
        {
            powerAvailForPorts = busPower - hubPower;
            /* we minimally need make available 100mA per non-captive port */
            powerNeededForPorts = (_hubDescriptor.numPorts - _numCaptive) * kUSB100mA;
            _busPowerGood = (powerAvailForPorts >= powerNeededForPorts);

            if(_numCaptive > 0)
            {
                if(_busPowerGood)
                    _powerForCaptive =
                        (powerAvailForPorts - powerNeededForPorts) / _numCaptive;
                else
                    _powerForCaptive = powerAvailForPorts / _numCaptive;
            }
    
            if( (_errataBits & kErrataCaptiveOKBit) != 0)
                _powerForCaptive = kUSB100mAAvailable;
        }
#if (DEBUGGING_LEVEL > 0)
        IOLog("%s: power:\n", getName());
        IOLog("\tbus power available = %ldmA\n", busPower * 2);
        IOLog("\thub power needed = %ldmA\n", hubPower * 2);
        IOLog("\tport power available = %ldmA\n", powerAvailForPorts * 2);
        IOLog("\tport power needed = %ldmA\n", powerNeededForPorts * 2);
        IOLog("\tpower for captives = %ldmA\n", _powerForCaptive * 2);
        IOLog("\tbus power is %s\n", _busPowerGood?"good" : "insufficient");
#endif
        
        _selfPowerGood = false;
        
        if(_selfPowered)
        {
            USBStatus	status = 0;
            IOReturn	localErr;

            localErr = _device->GetDeviceStatus(&status);
            if ( localErr != kIOReturnSuccess )
            {
                err = localErr;
                break;
            }
            
            status = USBToHostWord(status);
            _selfPowerGood = ((status & 1) != 0);	// FIXME 1?
        }

        if(_selfPowered && _busPowered)
        {
            /* Dual power hub */
            
            if(_selfPowerGood)
            {
                USBLog(3,"[%p] %s Hub attached - Self/Bus powered, power supply good", this, getName());
            }
            else
            {
                USBLog(3,"[%p] %s Hub attached - Self/Bus powered, no external power", this, getName());
            }
        }
        else
        {
            /* Single power hub */
            if(_selfPowered)
            {
                if(_selfPowerGood)
                {
                    USBLog(3,"[%p] %s Hub attached - Self powered, power supply good", this, getName());
                }
                else
                {
                    USBLog(3,"[%p] %s Hub attached - Self powered, no external power", this, getName());
                }
            }
            else
            {
                USBLog(3,"[%p] %s Hub attached - Bus powered", this, getName());
            }

        }
        startExternal = (_busPowerGood || _selfPowerGood);
        if( !startExternal )
        {	/* not plugged in or bus powered on a bus powered hub */
            err = kIOReturnNoPower;
	    DisplayNotEnoughPowerNotice();            
            USBLog(1,"[%p] %s: insufficient power to turn on ports", this, getName());
            if(!_busPowered)
            {
                /* may be able to turn on compound devices */
                break;	/* Now what ?? */
            }
        }
    } while (false);

    return err;
}



IOReturn 
AppleUSBHub::AllocatePortMemory(void)
{
    AppleUSBHubPort 	*port;
    UInt32		power;
    UInt32 		portMask = 2;
    UInt32 		portByte = 0;
    UInt32		currentPort;
    bool		captive;


    _ports = (AppleUSBHubPort **) IOMalloc(sizeof(AppleUSBHubPort *) * _hubDescriptor.numPorts);

    if (!_ports)
        return kIOReturnNoMemory;

    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        /* determine if the port is a captive port */
        if ((_hubDescriptor.removablePortFlags[portByte] & portMask) != 0)
        {
            power = _powerForCaptive;
            captive = true;
        }
        else
        {
            power = _selfPowerGood ? kUSB500mAAvailable : kUSB100mAAvailable;
            captive = false;
        }

        port = new AppleUSBHubPort;
        if (port->init(self, currentPort, power, captive) != kIOReturnSuccess)
        {
            port->release();
            _ports[currentPort-1] = NULL;
        }
        else
            _ports[currentPort-1] = port;

        portMask <<= 1;
        if(portMask > 0x80)
        {
            portMask = 1;
            portByte++;
        }
    }
    
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHub::StartPorts(void)
{
    AppleUSBHubPort 	*port;
    int			currentPort;

    USBLog(5, "%s [%p]: starting ports (%d)", getName(), this, _hubDescriptor.numPorts);

    for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        port = _ports[currentPort-1];
	if (port)
            port->start();

    }
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBHub::StopPorts(void)
{
    AppleUSBHubPort 	*port;
    int			currentPort;

    USBLog(5, "%s [%p]: stopping ports (%d)", getName(), this, _hubDescriptor.numPorts);

    if( _ports) for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
    {
        port = _ports[currentPort-1];
	if (port) {
            port->stop();
            port->release();
            _ports[currentPort-1] = 0;
	}
    }

    IOFree(_ports, sizeof(AppleUSBHubPort *) * _hubDescriptor.numPorts);
    _ports = NULL;
    
    return kIOReturnSuccess;
}


bool 
AppleUSBHub::HubStatusChanged(void)
{
    IOReturn	err = kIOReturnSuccess;

    do
    {
        if ((err = GetHubStatus(&_hubStatus)))
        {
            FatalError(err, "get status (first in hub status change)");
            break;            
        }
        _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
        _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);

        USBLog(3,"%s [%p]: hub status = %x/%x", getName(), this, _hubStatus.statusFlags, _hubStatus.changeFlags);

        if (_hubStatus.changeFlags & kHubLocalPowerStatusChange)
        {
            USBLog(3, "%s [%p]: Local Power Status Change detected", getName(), this);
            if ((err = ClearHubFeature(kUSBHubLocalPowerChangeFeature)))
            {
                FatalError(err, "clear hub power status feature");
                break;
            }
            if ((err = GetHubStatus(&_hubStatus)))
            {
                FatalError(err, "get status (second in hub status change)");
                break;
            }
            _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
            _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);
        }

        if (_hubStatus.changeFlags & kHubOverCurrentIndicatorChange)
        {
            USBLog(3, "%s [%p]: OverCurrent Indicator Change detected", getName(), this);
            if ((err =
                 ClearHubFeature(kUSBHubOverCurrentChangeFeature)))
            {
                FatalError(err, "clear hub over current feature");
                break;
            }
            if ((err = GetHubStatus(&_hubStatus)))
            {
                FatalError(err, "get status (second in hub status change)");
                break;
            }
            _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
            _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);
        }
    
    // See if we have the kResetOnPowerStatusChange errata. This means that upon getting a hub status change, we should do
    // a device reset
    //
    OSBoolean * boolObj = OSDynamicCast( OSBoolean, getProperty("kResetOnPowerStatusChange") );
    if ( boolObj && boolObj->isTrue() )
    { 
        // Reset our hub, as the overcurrent and power status chagnes might disable the ports downstream
        //
        _hubIsDead = TRUE;
        
        ResetMyPort();
    
        // Set an error so we return false, which will cause us to NOT rearm the interrupt thread.  The reset
        // will take care of reconfiguring the hub and rearming the interrupt.
        //
        err = kIOReturnBusy;
    }

    } while(false);

    return(err == kIOReturnSuccess);
}

UInt32 
AppleUSBHub::GetHubErrataBits()
{
      UInt16		vendID, deviceID, revisionID;
      ErrataListEntry	*entryPtr;
      UInt32		i, errata = 0;

      // get this chips vendID, deviceID, revisionID
      vendID = _device->GetVendorID();
      deviceID = _device->GetProductID();
      revisionID = _device->GetDeviceRelease();

      for(i=0, entryPtr = errataList; i < errataListLength; i++, entryPtr++)
      {
          if (vendID == entryPtr->vendID
              && deviceID == entryPtr->deviceID
              && revisionID >= entryPtr->revisionLo
              && revisionID <= entryPtr->revisionHi)
          {
              errata |= entryPtr->errata;  // we match, add this errata to our list
          }
      }
      return(errata);
}



void 
AppleUSBHub::FatalError(IOReturn err, char *str)
{
    USBError(1, "[%p] %s::FatalError 0x%x: %s", this, getName(), err, str);
}



IOReturn 
AppleUSBHub::GetHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOReturn	err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    if (!desc) return (kIOReturnBadArgument);

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBDevice);
    request.bRequest = kUSBRqGetDescriptor;
    request.wValue = (kUSBHubDescriptorType << 8) + 0;	// Descriptor type goes in high byte, index in low
    request.wIndex = 0;
    request.wLength = sizeof(IOUSBHubDescriptor);
    request.pData = desc;

    err = DoDeviceRequest(&request);

    if (err)
    {
        /*
         * Is this a bogus hub?  Some hubs require 0 for the descriptor type
         * to get their device descriptor.  This is a bug, but it's actually
         * spec'd out in the USB 1.1 docs.
         */
        USBLog(5,"[%p] %s: GetHubDescriptor w/ type = %X returned error: 0x%x",this, getName(), kUSBHubDescriptorType, err);
        request.wValue = 0;
        request.wLength = sizeof(IOUSBHubDescriptor);
        err = DoDeviceRequest(&request);
    }

    if (err)
    {
        USBLog(3, "%s [%p] GetHubDescriptor error = 0x%x", getName(), this, err);
    }        

    return(err);
}



IOReturn 
AppleUSBHub::GetHubStatus(IOUSBHubStatus *status)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBDevice);
    request.bRequest = kUSBRqGetStatus;
    request.wValue = 0;
    request.wIndex = 0;
    request.wLength = sizeof(IOUSBHubStatus);
    request.pData = status;

    err = DoDeviceRequest(&request);

    if (err)
    {
        USBLog(3, "%s [%p] GetHubStatus error = 0x%x", getName(), this, err);
    }        

    return(err);
}



IOReturn 
AppleUSBHub::GetPortState(UInt8 *state, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqGetState;
    request.wValue = 0;
    request.wIndex = port;
    request.wLength = sizeof(*state);
    request.pData = state;

    err = DoDeviceRequest(&request);

    if (err)
    {
        USBLog(3, "%s [%p] GetPortState error = 0x%x", getName(), this, err);
    }        

    return(err);
}



IOReturn 
AppleUSBHub::ClearHubFeature(UInt16 feature)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBDevice);
    request.bRequest = kUSBRqClearFeature;
    request.wValue = feature;
    request.wIndex = 0;
    request.wLength = 0;
    request.pData = NULL;

    err = DoDeviceRequest(&request);

    if (err)
    {
        USBLog(3, "%s [%p] ClearHubFeature error = 0x%x", getName(), this, err);
    }        

    return(err);
}



IOReturn 
AppleUSBHub::GetPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqGetStatus;
    request.wValue = 0;
    request.wIndex = port;
    request.wLength = sizeof(IOUSBHubPortStatus);
    request.pData = status;

    err = DoDeviceRequest(&request);

    if (err)
    {
        USBLog(3, "%s[%p]::GetPortStatus, error (%x) returned from DoDeviceRequest", getName(), this, err);
    }

    // Get things the right way round.
    status->statusFlags = USBToHostWord(status->statusFlags);
    status->changeFlags = USBToHostWord(status->changeFlags);

    if ( err == kIOReturnSuccess)
    {
        USBLog( 5, "%s[%p]::GetPortStatus for port %d, status: 0x%8x, change: 0x%8x - returning kIOReturnSuccess", getName(), this, port, status->statusFlags, status->changeFlags);
    }
    
    return(err);
}



IOReturn 
AppleUSBHub::SetPortFeature(UInt16 feature, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(5, "%s[%p]::SetPortFeature port/feature (%x) - setting", getName(), this, (port << 16) | feature);

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqSetFeature;
    request.wValue = feature;
    request.wIndex = port;
    request.wLength = 0;
    request.pData = NULL;

    err = DoDeviceRequest(&request);

    if (err)
    {
        USBLog(1, "%s[%p]::SetPortFeature got error (%x) to DoDeviceRequest", getName(), this, err);
    }

    return(err);
}



IOReturn 
AppleUSBHub::ClearPortFeature(UInt16 feature, UInt16 port)
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBDevRequest	request;

    USBLog(5, "%s[%p]::ClearPortFeature port/feature (%x) - clearing", getName(), this, (port << 16) | feature);

    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBOther);
    request.bRequest = kUSBRqClearFeature;
    request.wValue = feature;
    request.wIndex = port;
    request.wLength = 0;
    request.pData = NULL;

    err = DoDeviceRequest(&request);

    if (err)
    {
        USBLog(1, "%s[%p]::ClearPortFeature got error (%x) to DoDeviceRequest", getName(), this, err);
    }

    return(err);
}

IOReturn 
AppleUSBHub::DoPortAction(UInt32 type, UInt32 portNumber, UInt32 options )
{
    AppleUSBHubPort 		*port;
    IOReturn			err = kIOReturnSuccess;

    USBLog(5,"+%s[%p]::DoPortAction for port (%d), options (0x%x)", getName(), this, portNumber, options);
    
    port = _ports[portNumber - 1];
    if (port)
    {
        switch ( type )
        {
            case kIOUSBMessageHubSuspendPort:
                err = port->SuspendPort( true );
                break;
            case kIOUSBMessageHubResumePort:
                err = port->SuspendPort( false );
                break;
            case kIOUSBMessageHubReEnumeratePort:
                err = port->ReEnumeratePort(options);
                break;
            case kIOUSBMessageHubResetPort:
                err = port->ResetPort();
                break;
        }
    }
    
    return err;
}

void 
AppleUSBHub::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);

    if (!me)
        return;
    
    me->InterruptReadHandler(status, bufferSizeRemaining);
    me->DecrementOutstandingIO();
}

void 
AppleUSBHub::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining)
{
    bool		queueAnother = TRUE;
    bool		timeToGoAway = false;
    IOReturn		err = kIOReturnSuccess;
    
    switch (status)
    {
        case kIOReturnOverrun:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // This is an interesting error, as we have the data that we wanted and more...  We will use this
            // data but first we need to clear the stall and reset the data toggle on the device.  We will not
            // requeue another read because our _clearFeatureEndpointHaltThread will requeue it.  We then just 
            // fall through to the kIOReturnSuccess case.
            
	    if (!isInactive())
	    {
                //
                // First, clear the halted bit in the controller
                //
                _interruptPipe->ClearStall();
                
                // And call the device to reset the endpoint as well
                //
                IncrementOutstandingIO();
                thread_call_enter(_clearFeatureEndpointHaltThread);
            }
            queueAnother = false;
            timeToGoAway = false;
            
            // Fall through to process the data.
            
        case kIOReturnSuccess:

            // Handle the data
            //
            if ( !_hubIsDead )
            {
                IncrementOutstandingIO();
                thread_call_enter(_workThread);
            }
            
            // Note that the workThread will requeue the interrupt, so we don't
            // need to do it again
            //
            queueAnother = FALSE;

            break;

        case kIOReturnNotResponding:
            // If our device has been disconnected or we're already processing a
            // terminate message, just go ahead and close the device (i.e. don't
            // queue another read.  Otherwise, go check to see if the device is
            // around or not. 
            //
            USBLog(3, "%s[%p]::InterruptReadHandler error kIOReturnNotResponding", getName(), this);
            
            if ( _hubHasBeenDisconnected || isInactive() )
            {
                  queueAnother = false;
                  timeToGoAway = true;
            }
            else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler Checking to see if hub is still connected", getName(), this);
                
                IncrementOutstandingIO();
                thread_call_enter(_hubDeadCheckThread);
                
                // Note that since we don't do retries on the Hub, if we get a kIOReturnNotResponding error
                // we will either determine that the hub is disconnected or we will reset the hub.  In either
                // case, we will not need to requeue the interrupt, so we don't need to clear the stall.

                queueAnother = false;
                
            }
                
            break;
            
	case kIOReturnAborted:
	    // This generally means that we are done, because we were unplugged, but not always
            //
            if (isInactive() || _hubIsDead )
	    {
                USBLog(3, "%s[%p]::InterruptReadHandler error kIOReturnAborted (expected)", getName(), this);
		queueAnother = false;
                timeToGoAway = true;
	    }
	    else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler error kIOReturnAborted. Try again.", getName(), this);
            }
	    break;

        case kIOReturnUnderrun:
        case kIOUSBPipeStalled:
        case kIOUSBLinkErr:
        case kIOUSBNotSent2Err:
        case kIOUSBNotSent1Err:
        case kIOUSBBufferUnderrunErr:
        case kIOUSBBufferOverrunErr:
        case kIOUSBWrongPIDErr:
        case kIOUSBPIDCheckErr:
        case kIOUSBDataToggleErr:
        case kIOUSBBitstufErr:
        case kIOUSBCRCErr:
            // These errors will halt the endpoint, so before we requeue the interrupt read, we have
            // to clear the stall at the controller and at the device.  We will not requeue the read
            // until after we clear the ENDPOINT_HALT feature.  We need to do a callout thread because
            // we are executing inside the gate here and we cannot issue a synchronous request.
            USBLog(3, "%s[%p]::InterruptReadHandler OHCI error (0x%x) reading interrupt pipe", getName(), this, status);
            
	    // 01-28-02 JRH If we are inactive, then we can ignore this
	    if (!isInactive())
	    {
                // First, clear the halted bit in the controller
                //
                _interruptPipe->ClearStall();
                
                // And call the device to reset the endpoint as well
                //
                IncrementOutstandingIO();
                thread_call_enter(_clearFeatureEndpointHaltThread);
            }
            
            // We don't want to requeue the read here, AND we don't want to indicate that we are done
            //
            queueAnother = false;
            break;

        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            //
            USBLog(3,"%s[%p]::InterruptReadHandler error 0x%x reading interrupt pipe", getName(), this, status);
	    if (isInactive())
		queueAnother = false;
            break;
    }

    if ( queueAnother )
    {
        // RearmInterruptRead will close the device if it fails, so we don't need to do it here
        //
        err = RearmInterruptRead();
    }
}



void
AppleUSBHub::ResetPortZeroEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
    
    if (!me)
        return;
        
    me->ResetPortZero();
    me->DecrementOutstandingIO();
}



void
AppleUSBHub::ResetPortZero()
{
    AppleUSBHubPort 	*port;
    UInt32		currentPort;

    // Find out which port we have to reset
    //
    if( _ports) 
        for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
        {
            port = _ports[currentPort-1];
            if (port) 
            {
                Boolean	locked;
                locked = port->GetDevZeroLock();
                if ( locked )
                {
                    // If the timeout flag for this port is already set, AND the timestamp is the same as when we
                    // first detected the lock, then is time to release the devZero lock
                    if ( ((_timeoutFlag & (1 << (currentPort-1))) != 0) && (_portTimeStamp[currentPort-1] == port->GetPortTimeStamp()) )
                    {
                        USBLog(1, "%s[%p]::ResetPortZero: - port %d - Releasing devZero lock", getName(), this, currentPort);
                        _timeoutFlag &= ~( 1<<(currentPort-1));
                        port->ReleaseDevZeroLock();
                    }
                }
            }
        }
}

//
// ProcessStatusChanged
// This method will run on one of the shared kernel threads. It is called when an Async read
// on the interrupt pipe returns some data. It needs to issue another read when it is done
// processing, and then just return
//
void 
AppleUSBHub::ProcessStatusChangedEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
    
    if (!me)
        return;
        
    me->ProcessStatusChanged();
    me->DecrementOutstandingIO();
}



void 
AppleUSBHub::ProcessStatusChanged()
{
    const UInt8	*	statusChangedBitmapPtr = 0;
    int 		portMask;
    int 		portByte;
    int 		portIndex;
    AppleUSBHubPort 	*port;
    bool		portSuccess = false;
    bool		hubStatusSuccess = true;

    if (isInactive() || !_buffer)
	return;

    portMask = 2;
    portByte = 0;
    statusChangedBitmapPtr = (const UInt8*)_buffer->getBytesNoCopy();
    if (statusChangedBitmapPtr == NULL)
    {
        USBError(1, "%s[%p]::ProcessStatusChanged: No interrupt pipe buffer!", getName(), this);
    }
    else
    {
        USBLog(5,"%s[%p]::ProcessStatusChanged found (0x%8.8x) in statusChangedBitmap", getName(), this, statusChangedBitmapPtr[0]);
        for (portIndex = 1; portIndex <= _hubDescriptor.numPorts; portIndex++)
        {
            if ((statusChangedBitmapPtr[portByte] & portMask) != 0)
            {
                port = _ports[portIndex-1];
                USBLog(5,"%s[%p]::ProcessStatusChanged port number %d, calling port->StatusChanged", getName(), this, portIndex);
                portSuccess = port->StatusChanged();
                if (! portSuccess )
                {
                    USBLog(1,"%s[%p]::ProcessStatusChanged port->StatusChanged() returned false", getName(), this);
                }
            }
    
            portMask <<= 1;
            if (portMask > 0x80)
            {
                portMask = 1;
                portByte++;
            }
        }
    
        // hub status changed
        if ((statusChangedBitmapPtr[0] & 1) != 0)
        {	
            hubStatusSuccess = HubStatusChanged();
        }
    }
    
    if ( hubStatusSuccess )
    {
        // now re-arm the read 
        (void) RearmInterruptRead();
    }
    
}

IOReturn
AppleUSBHub::RearmInterruptRead()
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion	comp;

    IncrementOutstandingIO();			// retain myself for the callback
    comp.target = this;
    comp.action = (IOUSBCompletionAction) InterruptReadHandlerEntry;
    comp.parameter = NULL;
    _buffer->setLength(_readBytes);

    if ((err = _interruptPipe->Read(_buffer, &comp)))
    {
        USBError(1,"%s[%p]::RearmInterruptRead error %x reading interrupt pipe", getName(), this, err);
        DecrementOutstandingIO();
    }
    
    return err;
}



void 
AppleUSBHub::PrintHubDescriptor(IOUSBHubDescriptor *desc)
{
    int i = 0;
    char *characteristics[] =
        { "ppsw", "nosw", "comp", "ppoc", "nooc", 0 };


    if (desc->length == 0) return;

    IOLog("hub descriptor: (%d bytes)\n", desc->length);
    IOLog("\thubType = %d\n", desc->hubType);
    IOLog("\tnumPorts = %d\n", desc->numPorts);
    IOLog("\tcharacteristics = %x ( ",
                                USBToHostWord(desc->characteristics));
    do
    {
        if (USBToHostWord(desc->characteristics) & (1 << i))
            IOLog("%s ", characteristics[i]);
    } while (characteristics[++i]);
    IOLog(")\n");
    IOLog("\tpowerOnToGood = %d ms\n", desc->powerOnToGood * 2);
    IOLog("\thubCurrent = %d\n", desc->hubCurrent);
    IOLog("\tremovablePortFlags = %lx %lx\n", (UInt32)&desc->removablePortFlags[1], (UInt32)&desc->removablePortFlags[0]);
    IOLog("\tpwrCtlPortFlags    = %lx %lx\n", (UInt32)&desc->pwrCtlPortFlags[1], (UInt32)&desc->removablePortFlags[0]);
}



// This method is in here as well as in the device class, because one day we might want to get fancy
// about hubs with captive devices (like keyboards) - we could turn on some of the hub and not all..
// for now, this is never actually called.
void
AppleUSBHub::DisplayNotEnoughPowerNotice()
{
    KUNCUserNotificationDisplayNotice(
	0,		// Timeout in seconds
	0,		// Flags (for later usage)
	"",		// iconPath (not supported yet)
	"",		// soundPath (not supported yet)
	"/System/Library/Extensions/IOUSBFamily.kext",		// localizationPath
	"Low Power Header",		// the header
	"Low Power Notice",		// the notice - look in Localizable.strings
	"OK"); 
    return;
}



IOReturn 
AppleUSBHub::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn				err = kIOReturnSuccess;
    IOUSBHubPortStatus			status;
    IOUSBHubPortReEnumerateParam *	params ;
      
    switch ( type )
    {
        case kIOUSBMessageHubIsDeviceConnected:
        
            // If we are in the process of terminating, or if we have determined that the hub is dead, then
            // just return as if the device was unplugged.  The hub itself is going away or is already resetting,
            // so it any device that was connected will not be anymore.
            //
            if ( isInactive() || _hubIsDead )
            {
                USBLog(3,"%s[%p] : got kIOUSBMessageHubIsDeviceConnected while isInactive() or _hubIsDead", getName(), this);
                err = kIOReturnNoDevice;
                break;
            }
                        
            // Get the status for the port.  Note that the argument passed into this method is the port number.  If we get an
            // error from the call, then that means that our hub is not 100% so the device is probably not connected.  Otherwise
            // check the kHubPortConnection bit to see whether there is a device connected to the port or not
            //
            err = GetPortStatus(&status, * (UInt32 *) argument );
            if ( err != kIOReturnSuccess )
            {
                err = kIOReturnNoDevice;
            }
            else
            {
                USBLog(5,"%s[%p]::kIOUSBMessageHubIsDeviceConnected - port %d - status(%8x)/change(%8x)", getName(), this, * (UInt32 *) argument, status.statusFlags, status.changeFlags);
                if ( (status.statusFlags & kHubPortConnection) && !(status.changeFlags & kHubPortConnection) )
                    err = kIOReturnSuccess;
                else
                    err = kIOReturnNoDevice;
            }
            break;
            
        case kIOUSBMessageHubSuspendPort:
        case kIOUSBMessageHubResumePort:
        case kIOUSBMessageHubResetPort:
            err = DoPortAction( type, * (UInt32 *) argument, 0 );
            break;
        
        case kIOUSBMessageHubReEnumeratePort:
            params = (IOUSBHubPortReEnumerateParam *) argument;
            err = DoPortAction( type, params->portNumber, params->options );
            break;
            
        case kIOMessageServiceIsTerminated: 	
            USBLog(3,"%s[%p] : Received kIOMessageServiceIsTerminated - ignoring", getName(), this);
            break;
            

        case kIOUSBMessagePortHasBeenReset:
             if ( isInactive() )
            {
                USBLog(5,"%s[%p] : got kIOUSBMessagePortHasBeenReset while isInactive() or _hubIsDead", getName(), this);
                err = kIOReturnSuccess;
                break;
            }
            
           // Should we do something here if we get an error?
            //
	    if (!_inStartMethod)
	    {
		_inStartMethod = true;
		IncrementOutstandingIO();		// make sure we don't close until start is done
		USBLog(3, "%s[%p]  Received kIOUSBMessagePortHasBeenReset -- reconfiguring hub", getName(), this);
		
                // Abort any transactions (should not have any pending at this point)
                //
                if ( _interruptPipe )
                {
                    _interruptPipe->Abort();
                    _interruptPipe = NULL;
                }
            
                // When we reconfigure the hub, we'll recreate the hubInterface, so let's tear it down.
                //
		if(_hubInterface) 
		{
		    _hubInterface->close(this);
		    _hubInterface->release();
		    _hubInterface = NULL;
		}
    
                // Reconfigure our Hub. 
		err = ConfigureHub();
		if ( err ) 
		{
		    USBLog(3, "%s[%p] Reconfiguring hub returned: 0x%x",getName(), this, err);
		}
                else
                {
                    USBError(1, "[%p] (Reset) USB Generic Hub @ %d (0x%x)", this, _address, strtol(_device->getLocation(), (char **)NULL, 16));
                }
                
		_inStartMethod = false;
		DecrementOutstandingIO();

                // Only rearm the interrupt if we successfully configured the
                // hub
                if ( err == kIOReturnSuccess)
                {
                    err = RearmInterruptRead();
                }
	    }
    
            break;

        case kIOUSBMessagePortHasBeenResumed:
            err = kIOReturnSuccess;
            break;
            
        default:
            break;

    }
    
    return err;
}

IOReturn 
AppleUSBHub::DoDeviceRequest(IOUSBDevRequest *request)
{
    IOReturn err;
    
    // If the device is closed, we should not attempt to send any requests to it
    //
    if ( _device && !_device->isOpen(this) )
    {
        err = kIOReturnNoDevice;
        goto exit;
    }
    
    // Paranoia:  if we don't have a device ('cause it was stop'ped), then don't send
    // the request.
    //
    if ( _device )
        err = _device->DeviceRequest(request, 5000, 0);
    else
        err = kIOReturnNoDevice;

exit:
        
    return err;
}

bool 
AppleUSBHub::finalize(IOOptionBits options)
{
    return(super::finalize(options));
}

void 
AppleUSBHub::TimeoutOccurred(OSObject *owner, IOTimerEventSource *sender)
{
    
    AppleUSBHub		*me;
    AppleUSBHubPort 	*port;
    UInt32		currentPort;

    me = OSDynamicCast(AppleUSBHub, owner);
    if (!me)
        return;

    if( me->_ports) 
        for (currentPort = 1; currentPort <= me->_hubDescriptor.numPorts; currentPort++)
        {
            port = me->_ports[currentPort-1];
            if (port) 
            {
                Boolean	locked;
                locked = port->GetDevZeroLock();
                if ( locked )
                {
                    // If the timeout flag for this port is already set, AND the timestamp is the same as when we
                    // first detected the lock, then is time to release the devZero lock
                    if ( ((me->_timeoutFlag & (1 << (currentPort-1))) != 0) && (me->_portTimeStamp[currentPort-1] == port->GetPortTimeStamp()) )
                    {
                        // Need to call through a separate thread because we will end up making synchronous calls
                        // to the USB bus.  That thread will clear the timeoutFlag for this port.
                        USBError(3,"%s[%p]::TimeoutOccurred error", me->getName(), me);
                        me->IncrementOutstandingIO();
                        thread_call_enter(me->_resetPortZeroThread);
                    }
                    else
                    {
                        // Set the timeout flag for this port
                        me->_timeoutFlag |= (1<<(currentPort-1));
                        
                        // Set the timestamp for the port
                        me->_portTimeStamp[currentPort-1] = port->GetPortTimeStamp();
                    }
                }
                else
                {
                    // Port is not locked, make sure that we clear the timeoutFlag for this port and reset the timestamp
                    me->_timeoutFlag &= ~( 1<<(currentPort-1));
                    me->_portTimeStamp[currentPort-1] = 0;
                }
            }
        }

    /*
     * Restart the watchdog timer
     */
    if (me->_timerSource && !me->isInactive() )
    {
        // me->retain();
        me->_timerSource->setTimeoutMS(5000);
    }
    // me->release();

}/* end timeoutOccurred */

//=============================================================================================
//
//  CheckForDeadHub is called when we get a kIODeviceNotResponding error in our interrupt pipe.
//  This can mean that (1) the device was unplugged, or (2) we lost contact
//  with our hub.  In case (1), we just need to close the driver and go.  In
//  case (2), we need to ask if we are still attached.  If we are, then we update 
//  our retry count.  Once our retry count (3 from the 9 sources) are exhausted, then we
//  issue a DeviceReset to our provider, with the understanding that we will go
//  away (as an interface).
//
//=============================================================================================
//
void 
AppleUSBHub::CheckForDeadHubEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
    
    if (!me)
        return;
        
    me->CheckForDeadHub();
    me->DecrementOutstandingIO();
}



void 
AppleUSBHub::CheckForDeadHub()
{
    IOReturn			err = kIOReturnSuccess;
    
    IncrementOutstandingIO();
    
    // Are we still connected?
    //
    if ( _device )
    {
        err = _device->message(kIOUSBMessageHubIsDeviceConnected, NULL, 0);
    
        if ( kIOReturnSuccess == err )
        {
            _hubIsDead = TRUE;
            USBLog(3, "%s[%p]: Detected an kIONotResponding error but still connected.  Resetting port", getName(), this);
            
            ResetMyPort();
            
        }
        else
        {
            // Device is not connected -- our device has gone away.  The message kIOServiceIsTerminated
            // will take care of shutting everything down.  
            //
            _hubHasBeenDisconnected = TRUE;
            USBLog(3, "%s[%p]: CheckForDeadHub: device has been unplugged", getName(), this);
        }
    }

    DecrementOutstandingIO();
}

//=============================================================================================
//
//  ClearFeatureEndpointHaltEntry is called when we get an OHCI error from our interrupt read
//  (except for kIOReturnNotResponding  which will check for a dead device).  In these cases
//  we need to clear the halted bit in the controller AND we need to reset the data toggle on the
//  device.
//
//=============================================================================================
//
void 
AppleUSBHub::ClearFeatureEndpointHaltEntry(OSObject *target)
{
    AppleUSBHub *	me = OSDynamicCast(AppleUSBHub, target);
    
    if (!me)
        return;
        
    me->ClearFeatureEndpointHalt();
    me->DecrementOutstandingIO();
}

void 
AppleUSBHub::ClearFeatureEndpointHalt( )
{
    IOReturn			status;
    IOUSBDevRequest		request;
    
    // Clear out the structure for the request
    //
    bzero( &request, sizeof(IOUSBDevRequest));

    // Build the USB command to clear the ENDPOINT_HALT feature for our interrupt endpoint
    //
    request.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBStandard, kUSBEndpoint);
    request.bRequest 		= kUSBRqClearFeature;
    request.wValue		= kUSBFeatureEndpointStall;
    request.wIndex		= _interruptPipe->GetEndpointNumber() | 0x80 ; // bit 7 sets the direction of the endpoint to IN
    request.wLength		= 0;
    request.pData 		= NULL;

    // Send the command over the control endpoint
    //
    status = _device->DeviceRequest(&request, 5000, 0);

    if ( status )
    {
        USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  DeviceRequest returned: 0x%x", getName(), this, status);
    }
    
    // Now that we've sent the ENDPOINT_HALT clear feature, we need to requeue the interrupt read.  Note
    // that we are doing this even if we get an error from the DeviceRequest.
    //
    status = RearmInterruptRead();
}


void 
AppleUSBHub::ResetMyPort()
{
    // Abort any pending transactions in the interrupt pipe.  Null the pipe because the
    // device reset is going to end up terminating the interface from under us.  (Need 
    // to review that once we change device reset to not terminate interfaces)
    //
    if ( _interruptPipe )
    {
        _interruptPipe->Abort();
        // _interruptPipe = NULL;
    }
    
    // If our timerSource is going, cancel it, as we don't need to
    // timeout our ports anymore
    //
    if (_timerSource) 
    {
        _timerSource->cancelTimeout();
    }
    
    // Stop/close all ports, deallocate our ports
    //
    StopPorts();
        
    // Ask our device to reset our port
    //
    _device->ResetDevice();
}


bool 	
AppleUSBHub::requestTerminate( IOService * provider, IOOptionBits options )
{
    USBLog(3, "%s[%p]::requestTerminate isInactive = %d", getName(), this, isInactive());
    return super::requestTerminate(provider, options);
}


bool
AppleUSBHub::willTerminate( IOService * provider, IOOptionBits options )
{
    IOReturn	err;
    
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    if ( _interruptPipe )
    {
	err = _interruptPipe->Abort();
        if ( err != kIOReturnSuccess )
        {
            USBLog(1, "%s[%p]::willTerminate interruptPipe->Abort returned 0x%x", getName(), this, err);
        }
           
	// _interruptPipe = NULL;
    }
    
    // We are going to be terminated, so clean up! Make sure we don't get any more status change interrupts.
    // Note that if we are terminated before we set up our interrupt pipe, then we better not call it!
    //
    if (_timerSource) 
    {
	_timerSource->cancelTimeout();
	// release();
    }
    
    return super::willTerminate(provider, options);
}


bool
AppleUSBHub::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    USBLog(3, "%s[%p]::didTerminate isInactive = %d", getName(), this, isInactive());
    
    // Stop/close all ports, deallocate our ports
    //
    StopPorts();
            
    if (!_outstandingIO)
    {
	_device->close(this);
    }
    else
    {
	_needToClose = true;
    }
    return super::didTerminate(provider, options, defer);
}


bool
AppleUSBHub::terminate( IOOptionBits options )
{
    USBLog(5, "%s[%p]::terminate isInactive = %d", getName(), this, isInactive());
    return super::terminate(options);
}


void
AppleUSBHub::free( void )
{
    USBLog(5, "%s[%p]::free isInactive = %d", getName(), this, isInactive());
    super::free();
}


bool
AppleUSBHub::terminateClient( IOService * client, IOOptionBits options )
{
    USBLog(5, "%s[%p]::terminateClient isInactive = %d", getName(), this, isInactive());
    return super::terminateClient(client, options);
}



void
AppleUSBHub::DecrementOutstandingIO(void)
{
    if (!_gate)
    {
	USBLog(2, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - no gate", getName(), this, isInactive(), _outstandingIO);
	if (!--_outstandingIO && _needToClose)
	{
	    USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), _outstandingIO);
	    _device->close(this);
	}
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)-1);
}



void
AppleUSBHub::IncrementOutstandingIO(void)
{
    if (!_gate)
    {
	USBLog(2, "%s[%p]::IncrementOutstandingIO isInactive = %d, outstandingIO = %d - no gate", getName(), this, isInactive(), _outstandingIO);
	_outstandingIO++;
	return;
    }
    _gate->runAction(ChangeOutstandingIO, (void*)1);
}



IOReturn
AppleUSBHub::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    AppleUSBHub *me = OSDynamicCast(AppleUSBHub, target);
    UInt32	direction = (UInt32)param1;
    
    if (!me)
    {
	USBLog(1, "AppleUSBHub::ChangeOutstandingIO - invalid target");
	return kIOReturnSuccess;
    }
    switch (direction)
    {
	case 1:
	    me->_outstandingIO++;
	    break;
	    
	case -1:
	    if (!--me->_outstandingIO && me->_needToClose)
	    {
		USBLog(3, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->_outstandingIO);
		me->_device->close(me);
	    }
	    break;
	    
	default:
	    USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }

    return kIOReturnSuccess;
}


