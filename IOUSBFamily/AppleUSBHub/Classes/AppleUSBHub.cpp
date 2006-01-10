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

#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMessage.h>

#include <UserNotification/KUNCUserNotifications.h>

#include <IOKit/usb/IOUSBLog.h>

#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBControllerV2.h>

#include "AppleUSBHub.h"
#include "AppleUSBHubPort.h"

#define super IOService
#define self this

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

// from the EHCI driver
enum
{
    kEHCITestMode_Off		= 0,
    kEHCITestMode_J_State	= 1,
    kEHCITestMode_K_State 	= 2,
    kEHCITestMode_SE0_NAK	= 3,
    kEHCITestMode_Packet	= 4,
    kEHCITestMode_ForceEnable	= 5,
    kEHCITestMode_Start		= 10,
    kEHCITestMode_End		= 11
};

#define errataListLength (sizeof(errataList)/sizeof(ErrataListEntry))

OSDefineMetaClassAndStructors(AppleUSBHub, IOService)

enum
{
	kWatchdogTimerPeriod	=	30000
};

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
    OSNumber 			* errataProperty;
    const IORegistryPlane	*usbPlane = NULL;
    OSNumber			*locationIDProperty;
    
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
    }

    
    _gate = IOCommandGate::commandGate(this);

    if(!_gate)
    {
	USBError(1, "%s[%p]::start - unable to create command gate", getName(), this);
        goto ErrorExit;
    }

   _workLoop = getWorkLoop();
    if ( !_workLoop )
    {
        USBError(1, "%s::start Couldn't get provider's workloop", getName());
        goto ErrorExit;
    }

    // Keep a reference to our workloop
    //
    _workLoop->retain();

    if ( _workLoop->addEventSource( _timerSource ) != kIOReturnSuccess )
    {
        USBError(1, "%s::start Couldn't add timer event source", getName());
        goto ErrorExit;
    }

    if ( _workLoop->addEventSource( _gate ) != kIOReturnSuccess )
    {
        USBError(1, "%s::start Couldn't add gate event source", getName());
        goto ErrorExit;
    }
    
    // remember my device
    _device		= (IOUSBDevice *) provider;
    _address		= _device->GetAddress();
    _bus		= _device->GetBus();

    // Merge any properties in the IOProviderMergeProperties dictionary into our 
    // provider's dictionary
    //
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
	if (_hsHub)
	    registerService();					// for the benefit of a user client
                                
        rootHub = OSDynamicCast(IOUSBRootHubDevice, provider);
        if (rootHub)
        {
	    _isRootHub = true;
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
            USBError(1, "%s[%p] could not allocate all thread functions.  Aborting start", getName(), this);
            goto ErrorExit;
        }

        locationIDProperty = (OSNumber *) provider->getProperty(kUSBDevicePropertyLocationID);
        if ( locationIDProperty )
        {
            _locationID = locationIDProperty->unsigned32BitValue();
        }
        USBLog(1, "[%p] USB Generic Hub @ %d (0x%x)", this, _address, _locationID);
        
	_inStartMethod = false;
        DecrementOutstandingIO();
	err = RearmInterruptRead();
	if (err == kIOReturnSuccess)
	    return true;
    }
    else
    {
    
        USBError(1,"%s[%p]::start Aborting startup: error 0x%x", getName(), this, err);
        if ( _device && _device->isOpen(this) )
            _device->close(this);
        stop(provider);
    }

ErrorExit:

    if ( _timerSource )
    {
        if ( _workLoop )
            _workLoop->removeEventSource(_timerSource);
        _timerSource->release();
        _timerSource = NULL;
    }

    if (_gate)
    {
	if (_workLoop)
	    _workLoop->removeEventSource(_gate);
	    
	_gate->release();
	_gate = NULL;
    }

    if ( _workLoop != NULL )
    {
        _workLoop->release();
        _workLoop = NULL;
    }

    _inStartMethod = false;
    DecrementOutstandingIO();
    return false;
}



void 
AppleUSBHub::stop(IOService * provider)
{
    IOUSBControllerV2		*v2Bus;

    if (_buffer) 
    {
        _buffer->release();
		_buffer = NULL;
    }
	
	if (_hsHub)
	{
		v2Bus = OSDynamicCast(IOUSBControllerV2, _device->GetBus());
		if (v2Bus)
			v2Bus->RemoveHSHub(_address);
	}
	
    if(_hubInterface) 
    {
        _hubInterface->close(this);
        _hubInterface->release();
        _hubInterface = NULL;
    }
    
    if (_timerSource)
    {
        if ( _workLoop )
            _workLoop->removeEventSource(_timerSource);

        _timerSource->release();
        _timerSource = NULL;
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
    IOReturn							err = kIOReturnSuccess;
    IOUSBFindInterfaceRequest			req;
    const IOUSBConfigurationDescriptor	*cd;
    IOUSBControllerV2					*v2Bus;

    // Reset some of our variables that so that when we reconfigure due to a reset
    // we don't reuse old values
    //
    _busPowerGood = false;
    _powerForCaptive = 0;
    _numCaptive = 0;

    // Find the first config/interface
    if (_device->GetNumConfigurations() < 1)
    {
        USBError(1,"%s[%p]::ConfigureHub No hub configurations", getName(), this);
        err = kIOReturnNoResources;		// Need better error
        goto ErrorExit;
    }

    // set the configuration to the first config
    cd = _device->GetFullConfigurationDescriptor(0);
    if (!cd)
    {
        USBError(1,"%s[%p]::ConfigureHub No config descriptor", getName(), this);
        err = kIOUSBConfigNotFound;
        goto ErrorExit;
    }

    err = _device->SetConfiguration(this, cd->bConfigurationValue, false);
    
    if (err)
    {
        USBError(1,"%s[%p]::ConfigureHub SetConfiguration failed. Error 0x%x", getName(), this, err);
        goto ErrorExit;
    }
        
    // Set the remote wakeup feature if it's supported
    //
    if (cd->bmAttributes & kUSBAtrRemoteWakeup)
    {
        USBLog(3,"%s[%p]::ConfigureHub Setting kUSBFeatureDeviceRemoteWakeup for Hub device (%p)", getName(), this, _device);
        err = _device->SetFeature(kUSBFeatureDeviceRemoteWakeup);
        if ( err)
            USBError(1,"%s[%p]::ConfigureHub SetFeature(kUSBFeatureDeviceRemoteWakeup) failed. Error 0x%x", getName(), this, err);
    }

    // Find the interface for our hub -- there's only one
    //
    req.bInterfaceClass = kUSBHubClass;
    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    if ((_hubInterface = _device->FindNextInterface(NULL, &req)) == 0)
    {
        USBError(1,"%s[%p]::ConfigureHub no interface found", getName(), this);
        err = kIOUSBInterfaceNotFound;
        goto ErrorExit;
    }
    
    _hubInterface->retain();
    
    
    _busPowered = (cd->bmAttributes & kUSBAtrBusPowered) ? TRUE : FALSE;	//FIXME
    _selfPowered = (cd->bmAttributes & kUSBAtrSelfPowered) ? TRUE : FALSE;

    if( !(_busPowered || _selfPowered) )
    {
        USBError(1,"%s[%p]::ConfigureHub illegal device config - no power", getName(), this);
        err = kIOReturnNoPower;		// Need better error code here.
        goto ErrorExit;
    }

    // Get the hub descriptor
    if ( (err = GetHubDescriptor(&_hubDescriptor)) )
    {
        USBError(1,"%s[%p]::ConfigureHub could not get hub descriptor (0x%x)", getName(), this, err);
        goto ErrorExit;
    }
    
    if(_hubDescriptor.numPorts < 1)
    {
        USBLog(1,"%s[%p]::ConfigureHub there are no ports on this hub", getName(), this);
    }
    if(_hubDescriptor.numPorts > 7)
    {
        USBLog(1,"%s[%p]::ConfigureHub there are an awful lot of ports (%d) on this hub", getName(), this, _hubDescriptor.numPorts);
    }
    _readBytes = ((_hubDescriptor.numPorts + 1) / 8) + 1;
    
    // Setup for reading status pipe
    _buffer = IOBufferMemoryDescriptor::withCapacity(_readBytes, kIODirectionIn);

    if (!_hubInterface->open(this))
    {
        USBError(1," %s[%p]::ConfigureHub could not open hub interface", getName(), this);
        err = kIOReturnNotOpen;
        goto ErrorExit;
    }
    
    // after opening the interface, but before we get the pipe, we need to see if this is a 2.0
    // capabale hub, and if so, we need to set the multiTT status if possible
    _multiTTs = false;
    _hsHub = false;
    
    if (_device->GetbcdUSB() >= 0x200)
    {
		v2Bus = OSDynamicCast(IOUSBControllerV2, _device->GetBus());
		if (v2Bus)
		{
			switch (_device->GetProtocol())
			{
			case 0:
				USBLog(5, "%s[%p]::ConfigureHub - found FS/LS only hub", getName(), this);
				break;
				
			case 1:
				USBLog(5, "%s[%p]::ConfigureHub - found single TT hub", getName(), this);
				v2Bus->AddHSHub(_address, 0);
				_hsHub = true;
				break;
				
			case 2:
				USBLog(5, "%s[%p]::ConfigureHub - found multi TT hub", getName(), this);
				_hsHub = true;

				if ((err = _hubInterface->SetAlternateInterface(this, 1))) 		// pick the multi-TT setting
				{
					USBError(1, "%s[%p]::ConfigureHub - err (%x) setting alt interface", getName(), this, err);
					v2Bus->AddHSHub(_address, 0);
				}
				else
					v2Bus->AddHSHub(_address, kUSBHSHubFlagsMultiTT);
				
				_multiTTs = true;
				break;
				
			default:
				USBError(1, "%s[%p]::ConfigureHub - unknown protocol (%d)", getName(), this, _device->GetProtocol());
				break;
			}
		}
		else
		{
			USBLog(5, "%s[%p]::ConfigureHub - not on a V2 controller", getName(), this);
		}
    }
    

    IOUSBFindEndpointRequest request;
    request.type = kUSBInterrupt;
    request.direction = kUSBIn;
    _interruptPipe = _hubInterface->FindNextPipe(NULL, &request);

    if(!_interruptPipe)
    {
        USBError(1,"%s[%p]::ConfigureHub could not find interrupt pipe", getName(), this);
        err = kIOUSBNotEnoughPipesErr;		// Need better error code here.
        goto ErrorExit;
    } 
    
    // prepare the ports
    UnpackPortFlags();
    CountCaptivePorts();
    err = CheckPortPowerRequirements();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"%s[%p]::ConfigureHub CheckPortPowerRequirements failed with 0x%x", getName(), this, err);
        goto ErrorExit;
    }
    
    err = AllocatePortMemory();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"%s[%p]::ConfigureHub AllocatePortMemory failed with 0x%x", getName(), this, err);
        goto ErrorExit;
    }
    
    if (_hsHub)
    {
		// with a HS hub, we will put a property in our own object specifying the number
		// of TTs in the hub. We will also specify the name of the UserClient, which is only
		// applicable to HS hubs
		if (!_multiTTs)
			setProperty("High Speed", (unsigned long long)1, 8);		// 8 bits
		else
			setProperty("High Speed", (unsigned long long)_hubDescriptor.numPorts, 8);	// 8 bits
		setProperty("IOUserClientClass", "AppleUSBHSHubUserClient");
    }
    
    err = StartPorts();
    if ( err != kIOReturnSuccess )
    {
        USBError(1,"%s[%p]::ConfigureHub StartPorts failed with 0x%x", getName(), this, err);
        goto ErrorExit;
    }
    // Start the timeout Timer
    if (_timerSource)
    {
        // retain();
        _timerSource->setTimeoutMS(kWatchdogTimerPeriod); 
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
                ExtPower   ExtPower
                Good       off

Bus  Self

0     0     	Illegal config
1     0     	Always 100mA per port
0     1     	500mA     0 (dead)
1     1     	500       100
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
            // Don't put up an alert here.  The Adaptec 2.0 Hub claims that it needs 250mA. We will catch this later
            //
            USBLog(3, "%s [%p] Hub claims to need more power (%d > %d) than available", getName(), this, hubPower, busPower);
            _busPowerGood = false;
            _powerForCaptive = 0;
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
        
        _selfPowerGood = false;
        
        if (_selfPowered)
        {
            // Check the status of the power source
            //
            USBStatus	status = 0;
            IOReturn	localErr;

            _powerForCaptive = kUSB100mAAvailable;

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
                USBLog(3,"%s[%p] Hub attached - Self/Bus powered, power supply good", getName(), this);
            }
            else
            {
                USBLog(3,"%s[%p] Hub attached - Self/Bus powered, no external power", getName(), this);
            }
        }
        else
        {
            /* Single power hub */
            if(_selfPowered)
            {
                if(_selfPowerGood)
                {
                    USBLog(3,"%s[%p] Hub attached - Self powered, power supply good", getName(), this);
                }
                else
                {
                    USBLog(3,"%s[%p] Hub attached - Self powered, no external power", getName(), this);
                }
            }
            else
            {
                USBLog(3,"%s[%p] Hub attached - Bus powered", getName(), this);
            }

        }
        startExternal = (_busPowerGood || _selfPowerGood);
        if( !startExternal )
        {	/* not plugged in or bus powered on a bus powered hub */
            err = kIOReturnNoPower;
            _device->DisplayUserNotification(kUSBNotEnoughPowerNotificationType);
            IOLog("USB Low Power Notice:  The hub \"%s\" cannot be used because there is not enough power for all its ports\n",_device->getName());
            USBLog(1,"%s[%p]: insufficient power to turn on ports", getName(), this);
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
            power = _selfPowerGood ? (UInt32)kUSB500mAAvailable : _powerForCaptive;
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
    AppleUSBHubPort *		port;
    AppleUSBHubPort **	 	cachedPorts;
    int				currentPort;

    USBLog(5, "%s [%p]: stopping ports (%d)", getName(), this, _hubDescriptor.numPorts);

    if( _ports)
    {
        cachedPorts = _ports;
        _ports = NULL;

        for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
        {
            port = cachedPorts[currentPort-1];
            if (port)
            {
                cachedPorts[currentPort-1] = NULL;
                port->stop();
                port->release();
            }
        }
        IOFree(cachedPorts, sizeof(AppleUSBHubPort *) * _hubDescriptor.numPorts);
    }

    return kIOReturnSuccess;
}


bool 
AppleUSBHub::HubStatusChanged(void)
{
    IOReturn	err = kIOReturnSuccess;

	USBLog(6,"+%s[%p]::HubStatusChanged ", getName(), this);
    do
    {
        if ((err = GetHubStatus(&_hubStatus)))
        {
            FatalError(err, "get status (first in hub status change)");
            break;
        }
        _hubStatus.statusFlags = USBToHostWord(_hubStatus.statusFlags);
        _hubStatus.changeFlags = USBToHostWord(_hubStatus.changeFlags);

        USBLog(3,"%s[%p]: hub status = %x/%x", getName(), this, _hubStatus.statusFlags, _hubStatus.changeFlags);

        if (_hubStatus.changeFlags & kHubLocalPowerStatusChange)
        {
            USBLog(3, "%s[%p]: Hub Local Power Status Change detected", getName(), this);
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
			
			USBLog(3,"%s[%p]: hub status after clearing LocalPowerChange = %x/%x", getName(), this, _hubStatus.statusFlags, _hubStatus.changeFlags);
			// Need to check whether we successfully cleared the change
        }

        if (_hubStatus.changeFlags & kHubOverCurrentIndicatorChange)
        {
            USBLog(3, "%s[%p]: Hub OverCurrent Indicator Change detected", getName(), this);
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
			USBLog(3,"%s[%p]: hub status after clearing HubOvercurrent = %x/%x", getName(), this, _hubStatus.statusFlags, _hubStatus.changeFlags);
        }

        // See if we have the kResetOnPowerStatusChange errata. This means that upon getting a hub status change, we should do
        // a device reset
        //
        OSBoolean * boolObj = OSDynamicCast( OSBoolean, getProperty("kResetOnPowerStatusChange") );
        if ( boolObj && boolObj->isTrue() )
        {
            // Set an error so that we cause our port to be reset. The overcurrent and power status changes might disable the ports downstream
            // so we need a reset to recover. Do this ONLY if the change flag indicated that the status was a change to ON.
            //
			if ( _hubStatus.statusFlags & ( kUSBHubLocalPowerChangeFeature || kHubOverCurrentIndicatorChange ) )
			{
				USBLog(3,"%s[%p]: Hub status was a change to ON (0x%x)", getName(), this, _hubStatus.statusFlags);
				err = kIOReturnBusy;
			}
        }

    } while(false);

    if ( err == kIOReturnSuccess )
	{
		USBLog(6,"+%s[%p]::HubStatusChanged returning true", getName(), this);
        return true;
	}
    else
    {
        // If we get an error, then we better reset our hub
        //
        _hubIsDead = TRUE;

        retain();
        ResetMyPort();
        release();
        
        // Return false, which will cause us to NOT rearm the interrupt thread.  The reset
        // will take care of reconfiguring the hub and rearming the interrupt.
        //
		USBLog(6,"-%s[%p]::HubStatusChanged returning false", getName(), this);
        return false;
    }

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
    USBError(1, "%s[%p]::FatalError 0x%x: %s", getName(), this, err, str);
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
        USBLog(5,"%s[%p]: GetHubDescriptor w/ type = %X returned error: 0x%x",this, getName(), kUSBHubDescriptorType, err);
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

    if (err && (err != kIOUSBDeviceNotHighSpeed))
    {
        USBLog(1, "%s[%p]::SetPortFeature (%d) to port %d got error (%x) from DoDeviceRequest", getName(), this, feature, port, err);
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

    USBLog(5,"+%s[%p]::DoPortAction(0x%x) for port (%d), options (0x%x)", getName(), this, type, portNumber, options);

    if ( _ports == NULL )
        return kIOReturnNoDevice;
    
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
            case kIOUSBMessageHubPortClearTT:
                // ClearTT is only supported on HS Hubs
                //
                if ( _hsHub )
                    err = port->ClearTT(_multiTTs, options);
                else
                    err = kIOReturnUnsupported;
                break;
        }
    }

    USBLog(5,"-%s[%p]::DoPortAction(0x%x) for port (%d) returning 0x%x",getName(), this, type, portNumber, err);
    
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
    IOReturn		err = kIOReturnSuccess;
    
    switch (status)
    {
        case kIOReturnOverrun:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // This is an interesting error, as we have the data that we wanted and more...  We will use this
            // data but first we need to clear the stall and reset the data toggle on the device.  We then just 
            // fall through to the kIOReturnSuccess case.
            
	    if (!isInactive())
            {
                //
                // First, clear the halted bit in the controller
                //
		if ( _interruptPipe )
		{
		    _interruptPipe->ClearStall();

		    // And call the device to reset the endpoint as well
		    //
		    IncrementOutstandingIO();
		    thread_call_enter(_clearFeatureEndpointHaltThread);
		}
            }
            
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
            }
            else
            {
                USBLog(3, "%s[%p]::InterruptReadHandler Checking to see if hub is still connected", getName(), this);
                
		CallCheckForDeadHub();

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
            // to clear the stall at the controller and at the device.
            //
            USBLog(3, "%s[%p]::InterruptReadHandler OHCI error (0x%x) reading interrupt pipe", getName(), this, status);
            
	    // 01-28-02 JRH If we are inactive, then we can ignore this
            //
	    if (!isInactive())
            {
                // First, clear the halted bit in the controller
                //
		if ( _interruptPipe )
		{
		    _interruptPipe->ClearStall();
		    
		    // And call the device to reset the endpoint as well
		    //
		    IncrementOutstandingIO();
		    thread_call_enter(_clearFeatureEndpointHaltThread);
		}
            }
            
            queueAnother = false;
            break;

        case kIOUSBHighSpeedSplitError:
        default:
            USBLog(3,"%s[%p]::InterruptReadHandler error 0x%x reading interrupt pipe", getName(), this, status);
            if (isInactive())
                queueAnother = false;
            else
            {
                // Clear the halted bit in the controller
                //
		if ( _interruptPipe )
		    _interruptPipe->ClearStall();
            }
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

    if (isInactive() || !_buffer || !_ports)
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
        if ( statusChangedBitmapPtr[0] == 0xff)
        {
            USBLog(5,"%s[%p]::ProcessStatusChanged found (0x%8.8x) in statusChangedBitmap", getName(), this, statusChangedBitmapPtr[0]);
        }
        else
        {
            if ((statusChangedBitmapPtr[0] & 1) != 0)
            {
                hubStatusSuccess = HubStatusChanged();
            }

            if ( hubStatusSuccess )
            {
                USBLog(5,"%s[%p]::ProcessStatusChanged found (0x%8.8x) in statusChangedBitmap", getName(), this, statusChangedBitmapPtr[0]);
                for (portIndex = 1; portIndex <= _hubDescriptor.numPorts; portIndex++)
                {
                    if ((statusChangedBitmapPtr[portByte] & portMask) != 0)
                    {
                        port = _ports[portIndex-1];
						if ( port )
						{
							USBLog(5,"%s[%p]::ProcessStatusChanged port number %d, calling port->StatusChanged", getName(), this, portIndex);
							portSuccess = port->StatusChanged();
							if (! portSuccess )
							{
								USBLog(1,"%s[%p]::ProcessStatusChanged port->StatusChanged() returned false", getName(), this);
							}
						}
                    }

                    portMask <<= 1;
                    if (portMask > 0x80)
                    {
                        portMask = 1;
                        portByte++;
                    }
                }

                // now re-arm the read
                (void) RearmInterruptRead();
            }
        }
    }
}


IOReturn
AppleUSBHub::RearmInterruptRead()
{
    IOReturn		err = kIOReturnSuccess;
    IOUSBCompletion	comp;

	USBLog(5,"+%s[%p]::RearmInterruptRead", getName(), this);

    if ( (_buffer == NULL) || ( _interruptPipe == NULL ) )
        return err;

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




IOReturn 
AppleUSBHub::message( UInt32 type, IOService * provider,  void * argument )
{
    IOReturn				err = kIOReturnSuccess;
    IOUSBHubPortStatus			status;
    IOUSBHubPortReEnumerateParam *	params ;
    IOUSBHubPortClearTTParam *      	ttParams;
    
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

        case kIOUSBMessageHubPortClearTT:
            ttParams = (IOUSBHubPortClearTTParam *) argument;
            err = DoPortAction( type, ttParams->portNumber, ttParams->options );
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
                    USBError(1, "[%p] (Reset) USB Generic Hub @ %d (0x%x)", this, _address, _locationID);
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
			USBLog(3, "-%s[%p]  Received kIOUSBMessagePortHasBeenReset -- finishded reconfiguring hub", getName(), this);
    
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
    
    // Paranoia:  if we don't have a device ('cause it was stop'ped), then don't send
    // the request.
    //
    if ( _device && !_device->isInactive() && _device->isOpen(this))
        err = _device->DeviceRequest(request, 5000, 0);
    else
        err = kIOReturnNoDevice;
        
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
        me->_timerSource->setTimeoutMS(kWatchdogTimerPeriod);
    }
    // me->release();

}/* end timeoutOccurred */


//=============================================================================================
//
//  CallCheckForDeadHub
//  This is called by the hub driver if the interrupt pipe comes back with a NotResponding error
//  or by the hub port driver if somethine has gone wrong trying to talk to the hub (e.g. a port reset
//  fails) It increments the IO count and spins off a new thread to actually do the checking
//
//=============================================================================================
//
void
AppleUSBHub::CallCheckForDeadHub(void)
{
    IncrementOutstandingIO();
    thread_call_enter(_hubDeadCheckThread);
}



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
    
    // Are we still connected?
    //
    if ( _device && !_hubIsDead)
    {
        err = _device->message(kIOUSBMessageHubIsDeviceConnected, NULL, 0);
    
        if ( kIOReturnSuccess == err)
        {
            _hubIsDead = TRUE;
            USBLog(3, "%s[%p]::CheckForDeadHub - Still connected. Resetting port", getName(), this);
            
            retain();
            ResetMyPort();
            release();
            
        }
        else
        {
            // Device is not connected -- our device has gone away.  The message kIOServiceIsTerminated
            // will take care of shutting everything down.  
            //
            _hubHasBeenDisconnected = TRUE;
            USBLog(3, "%s[%p]::CheckForDeadHub - device has been unplugged", getName(), this);
        }
    }
    else
    {
        USBLog(3,"%s[%p]::CheckForDeadHub -- already resetting hub",getName(), this);
    }

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
    IOReturn			status = kIOReturnSuccess;
    IOUSBDevRequest		request;
    UInt32			retries = 2;
    
    // Clear out the structure for the request
    //
    bzero( &request, sizeof(IOUSBDevRequest));
    
    while ( (retries > 0) && (_interruptPipe) )
    {
        retries--;
        
        // Build the USB command to clear the ENDPOINT_HALT feature for our interrupt endpoint
        //
        request.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBStandard, kUSBEndpoint);
        request.bRequest        = kUSBRqClearFeature;
        request.wValue		= kUSBFeatureEndpointStall;
        request.wIndex		= _interruptPipe->GetEndpointNumber() | 0x80 ; // bit 7 sets the direction of the endpoint to IN
        request.wLength		= 0;
        request.pData 		= NULL;
        
        // Send the command over the control endpoint
        //
        status = _device->DeviceRequest(&request, 5000, 0);
        
        if ( status != kIOReturnSuccess )
        {
            USBLog(3, "%s[%p]::ClearFeatureEndpointHalt -  DeviceRequest returned: 0x%x, retries = %d", getName(), this, status, retries);
            IOSleep(100);
        }
        else
            break;
    }
    
    // Now that we've sent the ENDPOINT_HALT clear feature, we need to requeue the interrupt read.  Note
    // that we are doing this even if we get an error from the DeviceRequest.
    //
    status = RearmInterruptRead();
}

void 
AppleUSBHub::ResetMyPort()
{
    // Call willTerminate on ourselves (will abort the interrupt pipe)
    //
    willTerminate(this, 0);
        
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
    IOReturn		err;
    int			currentPort;
    AppleUSBHubPort *	port;
    
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());

    if ( _interruptPipe )
    {
		err = _interruptPipe->Abort();
        if ( err != kIOReturnSuccess )
        {
            USBLog(1, "%s[%p]::willTerminate interruptPipe->Abort returned 0x%x", getName(), this, err);
        }
        _interruptPipe = NULL;
    }

    // JRH 09/19/2003 rdar://problem/3290312
    // make sure that none of our ports has the dev zero lock held. if they do, it is safe to go ahead
    // and release it since the hub is now gone (we are terminating)
    if ( _ports)
    {
        for (currentPort = 1; currentPort <= _hubDescriptor.numPorts; currentPort++)
        {
            port = _ports[currentPort-1];
            if (port)
            {
                if (port->_devZero)
                {
                    USBLog(1, "%s[%p]::StopPorts - port %d had the dev zero lock", getName(), this, currentPort);
                }
                port->willTerminate(this, 0);
            }
        }
    }

    // We are going to be terminated, so clean up! Make sure we don't get any more status change interrupts.
    // Note that if we are terminated before we set up our interrupt pipe, then we better not call it!
    //
    if (_timerSource) 
    {
		_timerSource->cancelTimeout();
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

    if (_gate)
    {
        if (_workLoop)
        {
            (void) _workLoop->removeEventSource(_gate);
            _workLoop->release();
            _workLoop = NULL;
        }
        else
        {
            USBError(1, "%s[%p]::free - have gate, but no valid workloop (%p)!", getName(), this, _workLoop);
        }
        _gate->release();
        _gate = NULL;
    }
    
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


//================================================================================================
//
//  MergeDictionaryIntoProvider
//
//  We will iterate through the dictionary that we want to merge into our provider.  If
//  the dictionary entry is not an OSDictionary, we will set that property into our provider.  If it is a
//  OSDictionary, we will get our provider's entry and merge our entry into it, recursively.
//
//================================================================================================
//
bool
AppleUSBHub::MergeDictionaryIntoProvider(IOService * provider, OSDictionary * dictionaryToMerge)
{
    const OSSymbol * 		dictionaryEntry = NULL;
    OSCollectionIterator * 	iter = NULL;
    bool			result = false;

    USBLog(6,"+%s[%p]::MergeDictionary(%p)IntoProvider(%p)", getName(), this, dictionaryToMerge, provider);

    if (!provider || !dictionaryToMerge)
        return false;

    // Get the dictionary whose entries we need to merge into our provider and get
    // an iterator to it.
    //
    iter = OSCollectionIterator::withCollection((OSDictionary *)dictionaryToMerge);
    if ( iter != NULL )
    {
        // Iterate through the dictionary until we run out of entries
        //
        while ( NULL != (dictionaryEntry = (const OSSymbol *)iter->getNextObject()) )
        {
            const char *	str = NULL;
            OSDictionary *	sourceDictionary = NULL;
            OSDictionary *	providerDictionary = NULL;
            OSObject *		providerProperty = NULL;

            // Get the symbol name for debugging
            //
            str = dictionaryEntry->getCStringNoCopy();
            USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  merging \"%s\"", getName(), this, str);

            // Check to see if our destination already has the same entry.
            //
            providerProperty = provider->getProperty(dictionaryEntry);
            if ( providerProperty )
            {
                USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  provider already had property %s", getName(), this, str);
                providerDictionary = OSDynamicCast(OSDictionary, providerProperty);
                if ( providerDictionary )
                {
                    USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  provider's %s is also a dictionary (%p)", getName(), this, str, providerDictionary);
                }
            }

            // See if our source entry is also a dictionary
            //
            sourceDictionary = OSDynamicCast(OSDictionary, dictionaryToMerge->getObject(dictionaryEntry));
            if ( sourceDictionary )
            {
                USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  source dictionary had %s as a dictionary (%p)", getName(), this, str, sourceDictionary);
            }

            if ( providerDictionary &&  sourceDictionary )
            {
                // Need to merge our entry into the provider's dictionary.  However, we don't have a copy of our dictionary, just
                // a reference to it.  So, we need to make a copy of our provider's dictionary so that we don't modify our provider's
                // dictionary using non-synchronize calls.
                //
                OSDictionary *		localCopyOfProvidersDictionary;
                UInt32			providerSize;
                UInt32			providerSizeAfterMerge;

                // A capacity of 0 indicates that the dictionary should have the same size as the source
                //
                localCopyOfProvidersDictionary = OSDictionary::withDictionary( providerDictionary, 0);
                if ( localCopyOfProvidersDictionary == NULL )
                {
                    USBError(1,"%s[%p]::MergeDictionaryIntoProvider  could not copy our provider's dictionary",getName(), this);
                    break;
                }

                // Get the size of our provider's dictionary so that we can check later whether it changed
                //
                providerSize = providerDictionary->getCapacity();
                USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  Created a local copy(%p) of dictionary (%p), size %d", getName(), this, localCopyOfProvidersDictionary, providerDictionary, providerSize);

                USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  need to merge a dictionary (%s)", getName(), this, str);

                // Recursively merge the two dictionaries
                //
                result = MergeDictionaryIntoDictionary(  sourceDictionary, localCopyOfProvidersDictionary);
                if ( result )
                {
                    // Get the size of our provider's dictionary so to see if it's changed  (Yes, the size could remain the same but the contents
                    // could have changed, but this gives us a first approximation.  We're not doing anything with this result, although we could
                    // remerge if the size changed)
                    //
                    providerSizeAfterMerge = providerDictionary->getCapacity();
                    if ( providerSizeAfterMerge != providerSize )
                    {
                        USBError(1,"%s[%p]::MergeDictionaryIntoProvider  our provider's dictionary size changed (%d,%d)",getName(), this, providerSize, providerSizeAfterMerge);
                    }

                    USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  setting  property %s from merged dictionary (%p)", getName(), this, str, providerDictionary);

                    // OK, now we can just set the property in our provider
                    //
                    result = provider->setProperty( dictionaryEntry, localCopyOfProvidersDictionary );
                    if ( !result )
                    {
                        USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  setProperty %s , returned false", getName(), this, str);
                        break;
                    }
                }
                else
                {
                    // If we got an error merging dictionaries, then just bail out without doing anything
                    //
                    USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  MergeDictionaryIntoDictionary(%p,%p) returned false", getName(), this, sourceDictionary, providerDictionary);
                    break;
                }
            }
            else
            {
                // Not a dictionary, so just set the property
                //
                USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  setting property %s", getName(), this, str);
                result = provider->setProperty(dictionaryEntry, dictionaryToMerge->getObject(dictionaryEntry));
                if ( !result )
                {
                    USBLog(6,"%s[%p]::MergeDictionaryIntoProvider  setProperty %s, returned false", getName(), this, str);
                    break;
                }
            }
        }
        iter->release();
    }
    USBLog(6,"-%s[%p]::MergeDictionaryIntoProvider(%p, %p)  result %d", getName(), this, provider, dictionaryToMerge, result);

    return result;
}


//================================================================================================
//
//  MergeDictionaryIntoDictionary( parentSourceDictionary, parentTargetDictionary)
//
//  This routine will merge the contents of parentSourceDictionary into the parentTargetDictionary, recursively.
//  Note that we are only modifying copies of the parentTargetDictionary, so we don't expect anybody
//  else to be accessing them at the same time.
//
//================================================================================================
//
bool
AppleUSBHub::MergeDictionaryIntoDictionary(OSDictionary * parentSourceDictionary,  OSDictionary * parentTargetDictionary)
{
    OSCollectionIterator*	srcIterator = NULL;
    OSSymbol*			keyObject = NULL ;
    OSObject*			targetObject = NULL ;
    bool			result = false;

    USBLog(6,"+%s[%p]::MergeDictionaryIntoDictionary(%p => %p)", getName(), this, parentSourceDictionary, parentTargetDictionary);

    if (!parentSourceDictionary || !parentTargetDictionary)
        return false ;

    // Get our source dictionary
    //
    srcIterator = OSCollectionIterator::withCollection(parentSourceDictionary) ;

    while (NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())))
    {
        const char *	str;
        OSDictionary *	childSourceDictionary = NULL;
        OSDictionary *	childTargetDictionary = NULL;
        OSObject *	childTargetObject = NULL;

        // Get the symbol name for debugging
        //
        str = keyObject->getCStringNoCopy();
        USBLog(6,"%s[%p]::MergeDictionaryIntoDictionary  merging \"%s\"", getName(), this, str);

        // Check to see if our destination already has the same entry.
        //
        childTargetObject = parentTargetDictionary->getObject(keyObject);
        if ( childTargetObject )
        {
            childTargetDictionary = OSDynamicCast(OSDictionary, childTargetObject);
            if ( childTargetDictionary )
                USBLog(6,"%s[%p]::MergeDictionaryIntoDictionary  target object %s is a dictionary (%p)", getName(), this, str, childTargetDictionary);
        }

        // See if our source entry is also a dictionary
        //
        childSourceDictionary = OSDynamicCast(OSDictionary, parentSourceDictionary->getObject(keyObject));
        if ( childSourceDictionary )
        {
            USBLog(6,"%s[%p]::MergeDictionaryIntoDictionary  source dictionary had %s as a dictionary (%p)", getName(), this, str, childSourceDictionary);
        }

        if ( childTargetDictionary && childSourceDictionary)
        {
            // Our destination dictionary already has the entry for this same object AND our
            // source is also a dcitionary, so we need to recursively add it.
            //
            USBLog(6,"%s[%p]::MergeDictionaryIntoDictionary  recursing(%p,%p)", getName(), this, childSourceDictionary, childTargetDictionary);
            result = MergeDictionaryIntoDictionary(childSourceDictionary, childTargetDictionary) ;
            if ( !result )
            {
                USBLog(6,"%s[%p]::MergeDictionaryIntoDictionary  recursing (%p,%p) failed", getName(), this, childSourceDictionary, childTargetDictionary);
                break;
            }
        }
        else
        {
            // We have a property that we need to merge into our parent dictionary.
            //
            USBLog(6,"%s[%p]::MergeDictionaryIntoDictionary  setting object %s into dictionary %p", getName(), this, str, parentTargetDictionary);
            result = parentTargetDictionary->setObject(keyObject, parentSourceDictionary->getObject(keyObject)) ;
            if ( !result )
            {
                USBLog(6,"%s[%p]::MergeDictionaryIntoDictionary  setObject %s, returned false", getName(), this, str);
                break;
            }
        }

    }

    srcIterator->release();

    USBLog(6,"-%s[%p]::MergeDictionaryIntoDictionary(%p=>(%p)  result %d", getName(), this, parentSourceDictionary, parentTargetDictionary, result);
    return result;
}


IOReturn
AppleUSBHub::EnterTestMode()
{
    IOUSBControllerV2 	*con;
    int			currentPort;
    AppleUSBHubPort	*port;
    
    if (!_hsHub)
		return kIOReturnBadArgument;
	
    if (_isRootHub)
    {
	con = OSDynamicCast(IOUSBControllerV2, _bus);
	if (!con)
	    return kIOReturnBadArgument;
	    
	USBLog(1, "%s[%p]::EnterTestMode - root hub", getName(), this);    
	_inTestMode = true;
	return con->SetTestMode(kEHCITestMode_Start, 0);
    }
    // not a root hub
    _inTestMode = true;
    if ( _ports)
    {
	USBLog(1, "%s[%p]::EnterTestMode - external hub - suspending ports", getName(), this);    
        for (currentPort = 0; currentPort < _hubDescriptor.numPorts; currentPort++)
        {
            port = _ports[currentPort];
            if (port)
            {
		port->SuspendPort(true);
            }
        }
    }
    return kIOReturnSuccess;
}


IOReturn
AppleUSBHub::LeaveTestMode()
{
    IOUSBControllerV2 	*con;

    if (!_hsHub)
		return kIOReturnBadArgument;

    if (_isRootHub)
    {
		con = OSDynamicCast(IOUSBControllerV2, _bus);
		if (!con)
			return kIOReturnBadArgument;
	    
		USBLog(1, "%s[%p]::LeaveTestMode - root hub", getName(), this);    
		return con->SetTestMode(kEHCITestMode_End, 0);
    }
    // not a root hub - just reset my port - this will terminate me
    USBLog(1, "%s[%p]::LeaveTestMode - external hub", getName(), this);    
    retain();
    ResetMyPort();
    release();
    return kIOReturnSuccess;
}


bool
AppleUSBHub::IsHSRootHub()
{
    if (_hsHub && _isRootHub)
		return true;
    else
		return false;
}


IOReturn
AppleUSBHub::PutPortIntoTestMode(UInt32 port, UInt32 mode)
{
    IOUSBControllerV2 	*con;
    IOUSBDevRequest	request;

    if (!_hsHub || !_inTestMode)
		return kIOReturnBadArgument;

    if (_isRootHub)
    {
		con = OSDynamicCast(IOUSBControllerV2, _bus);
		if (!con)
			return kIOReturnBadArgument;
		USBLog(1, "%s[%p]::PutPortIntoTestMode - putting root hub port %d into mode %x", getName(), this, port, mode);    
		return con->SetTestMode(mode, port);
    }
    
    USBLog(1, "%s[%p]::PutPortIntoTestMode - putting external hub port %d into mode %x", getName(), this, port, mode);    
    return SetPortFeature(kUSBHubPortTestFeature, (mode << 8) + port);
}

