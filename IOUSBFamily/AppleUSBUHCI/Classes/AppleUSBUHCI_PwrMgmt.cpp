/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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


#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOMessage.h>

#include "AppleUSBUHCI.h"


#define super IOUSBController


// ========================================================================
#pragma mark Global variables
// ========================================================================


class AppleUHCIPwrMgmtGlobals
{
public:
    AppleUHCIPwrMgmtGlobals(); 
    ~AppleUHCIPwrMgmtGlobals();
    
    inline bool isValid( void ) const;
};

#define kUSBRemoteWakeupKey "usb_remote_wakeup"

static const OSSymbol * gUSBRemoteWakeupKey;

static AppleUHCIPwrMgmtGlobals gAppleUHCIPwrMgmtGlobals;

AppleUHCIPwrMgmtGlobals::AppleUHCIPwrMgmtGlobals()
{
    gUSBRemoteWakeupKey =
    OSSymbol::withCStringNoCopy( kUSBRemoteWakeupKey );
}

AppleUHCIPwrMgmtGlobals::~AppleUHCIPwrMgmtGlobals()
{
    if (gUSBRemoteWakeupKey) gUSBRemoteWakeupKey->release();
}

bool AppleUHCIPwrMgmtGlobals::isValid( void ) const
{
    return (gUSBRemoteWakeupKey);
}



// ========================================================================
#pragma mark Public power management interface
// ========================================================================



#define kUHCI_NUM_POWER_STATES 2

static IOPMPowerState powerStates[kUHCI_NUM_POWER_STATES] = {
    {
        1,  // version
        0,  // capability flags
        0,  // output power character
        0,  // input power requirement
        0,  // static power
        0,  // unbudgeted power
        0,  // power to attain this state
        0,  // time to attain this state
        0,  // settle up time
        0,  // time to lower
        0,  // settle down time
        0   // power domain budget
    },
    {
        1,  // version
        IOPMDeviceUsable,  // capability flags
        IOPMPowerOn,  // output power character
        IOPMPowerOn,  // input power requirement
        0,  // static power
        0,  // unbudgeted power
        0,  // power to attain this state
        0,  // time to attain this state
        0,  // settle up time
        0,  // time to lower
        0,  // settle down time
        0   // power domain budget
    }
};


void
AppleUSBUHCI::initForPM (IOPCIDevice *provider)
{
    USBLog(3, "%s[%p]::initForPM %p", getName(), this, provider);
    
    if (provider->getProperty("built-in") && (_errataBits & kErrataICH6PowerSequencing)) 
	{
		// The ICH6 UHCI drivers on a Transition system just magically work on sleep/wake
		// so we will just hard code those. Other systems will have to be evaluated later
        setProperty("Card Type","Built-in");
        _unloadUIMAcrossSleep = false;
    }
    else 
	{
        // This appears to be necessary
    setProperty("Card Type","PCI");
    _unloadUIMAcrossSleep = true;
    }
    
	if (_errataBits & kErrataICH6PowerSequencing)
		_powerDownNotifier = registerPrioritySleepWakeInterest(PowerDownHandler, this, 0);
    
    registerPowerDriver(this, powerStates, kUHCI_NUM_POWER_STATES);
    changePowerStateTo(kUHCIPowerLevelRunning);
}



unsigned long
AppleUSBUHCI::maxCapabilityForDomainState ( IOPMPowerFlags domainState )
{
	if ( (domainState & IOPMPowerOn) || (domainState & kIOPMDoze) ) 
	{
		return kUHCIPowerLevelRunning;
	} else 
	{
		return kUHCIPowerLevelSuspend;
	}
}



unsigned long
AppleUSBUHCI::initialPowerStateForDomainState ( IOPMPowerFlags domainState )
{
    return kUHCIPowerLevelRunning;
}

IOReturn 
AppleUSBUHCI::setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice )
{
    IOReturn result;
    
    USBLog(3, "%s[%p]::setPowerState(%d, %p)", getName(), this, powerStateOrdinal, whatDevice);
    
    if (_powerLevel != kUHCIPowerLevelSuspend) {
        _workLoop->CloseGate();
    } else {
        result = _workLoop->wake(&_powerLevel);
        if (result != kIOReturnSuccess) {
            USBError(1, "%s[%p] setPowerState - Can't wake workloop, error 0x%x", getName(), this, result);
        }
    }
    
    switch (powerStateOrdinal) {
    case kUHCIPowerLevelRunning:
        USBLog(3, "%s[%p]: changing to running state", getName(), this);
        
        if (_powerLevel == kUHCIPowerLevelSuspend) {
            if (!isInactive()) {
                //if (!_uimInitialized) {
                    EnableUSBInterrupt(false);
                    UIMInitializeForPowerUp();
                    EnableUSBInterrupt(true);
                //}
                if (_rootHubDevice == NULL) {
                    result = CreateRootHubDevice(_device, &_rootHubDevice);
                    if (result != kIOReturnSuccess) {
                        USBError(1,"%s[%p] Could not create root hub device on wakeup (%x)!",getName(), this, result);
                    } else {
                        _rootHubDevice->registerService(kIOServiceRequired | kIOServiceSynchronous);
                    }
                }
            }
        }

        ResumeController();

        _remoteWakeupOccurred = true;
        _powerLevel = kUHCIPowerLevelRunning;
        break;
        
    case kUHCIPowerLevelSuspend:
        USBLog(3, "%s[%p]: changing to suspended state", getName(), this);
        
        if (_unloadUIMAcrossSleep) {
            USBLog(3, "%s[%p] Unloading UIM before going to sleep", getName(), this);
            
            if ( _rootHubDevice )
            {
                _rootHubDevice->terminate(kIOServiceRequired | kIOServiceSynchronous);
                _rootHubDevice->detachAll(gIOUSBPlane);
                _rootHubDevice->release();
                _rootHubDevice = NULL;
            }
        }
            
        SuspendController();
            
        UIMFinalizeForPowerDown();
        
        _remoteWakeupOccurred = false;
        _powerLevel = kUHCIPowerLevelSuspend;
        break;
        
    case kUHCIPowerLevelIdleSuspend:
        USBLog(3, "%s[%p]: changing to idle suspended state", getName(), this);

        SuspendController();

        _powerLevel = kUHCIPowerLevelIdleSuspend;
        break;
        
    default:
        USBLog(3, "%s[%p]: unknown power state %d", getName(), this, powerStateOrdinal);
        break;
    }

    if (_powerLevel == kUHCIPowerLevelSuspend) {
        result = _workLoop->sleep(&_powerLevel);
        if (result!= kIOReturnSuccess) {
            USBError(1, "%s[%p] setPowerState - Can't sleep workloop, error 0x%x", getName(), this, result);
        }
    } else {
        _workLoop->OpenGate();
    }
    
    return IOPMAckImplied;
}


IOReturn
AppleUSBUHCI::callPlatformFunction(const OSSymbol *functionName,
                                   bool waitForFunction,
                                   void *param1, void *param2,
                                   void *param3, void *param4)
{
    USBLog(3, "%s[%p]::callPlatformFunction(%s)",
           getName(), this, functionName->getCStringNoCopy());
    
    if (functionName == gUSBRemoteWakeupKey) {
	bool *wake = (bool *)param1;
	
	if (_remoteWakeupOccurred) {
	    *wake = true;
	} else {
	    *wake = false;
	}
    	return kIOReturnSuccess;
    }
    
    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}



// ========================================================================
#pragma mark Internal methods
// ========================================================================



void			
AppleUSBUHCI::ResumeController(void)
{
    UInt16 cmd;
    int i;
    
    USBLog(3, "%s[%p]::ResumeController", getName(), this);
    USBLog(3, "%s[%p]: cmd state %x, status %x", getName(), this,
           ioRead16(kUHCI_CMD), ioRead16(kUHCI_STS));

    
    for (i=0; i<10; i++) {
        cmd = ioRead16(kUHCI_CMD);
        
        if (cmd & kUHCI_CMD_EGSM) {
            /* Global Suspend mode. */
            USBLog(5, "%s[%p]: taking controller out of global suspend mode",
                   getName(), this);
            cmd &= ~kUHCI_CMD_EGSM;
            cmd |= kUHCI_CMD_FGR;
            ioWrite16(kUHCI_CMD, cmd);
            IODelay(20);

            continue;
        }
        
        if (cmd & kUHCI_CMD_FGR) {
            /* Resume is active; assert it for 20 ms. */
            USBLog(3, "%s[%p]: taking controller out of force resume",
                   getName(), this);
            IOSleep(20);
            cmd &= ~kUHCI_CMD_FGR;
            ioWrite16(kUHCI_CMD, cmd);
            IOSleep(3);
            
            continue;
        }
        
        if ((cmd & kUHCI_CMD_RS) == 0) {
            /* Controller is not running. */
            USBLog(3, "%s[%p]: starting controller",
                   getName(), this);
            Run(true);
        }
        
        /* Controller should be running at this point. */
        break;
    }
    
    
    USBLog(3, "%s[%p]: resume done, cmd %x, status %x", getName(), this,
           ioRead16(kUHCI_CMD), ioRead16(kUHCI_STS));
}

void			
AppleUSBUHCI::SuspendController(void)
{
    UInt16 cmd;
    
    USBLog(3, "%s[%p]::SuspendController", getName(), this);
    USBLog(3, "%s[%p]: cmd state %x, status %x", getName(), this,
           ioRead16(kUHCI_CMD), ioRead16(kUHCI_STS));

    /* Stop the controller. */
    Run(false);
    
    /* Put the controller in Global Suspend. */
    cmd = ioRead16(kUHCI_CMD) & ~kUHCI_CMD_FGR;
    cmd |= kUHCI_CMD_EGSM;
    ioWrite16(kUHCI_CMD, cmd);
    IOSleep(3);
    
    USBLog(3, "%s[%p]: suspend done, cmd %x, status %x", getName(), this,
           ioRead16(kUHCI_CMD), ioRead16(kUHCI_STS));
}

void			
AppleUSBUHCI::StopController(void)
{
    /* Stop the controller. */
    Run(false);
}

void			
AppleUSBUHCI::RestartController(void)
{
    /* Start the controller. */
    Run(true);
}

IOReturn 
AppleUSBUHCI::PowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service,
                               void *messageArgument, vm_size_t argSize )
{
    AppleUSBUHCI *	me = OSDynamicCast(AppleUSBUHCI, (OSObject *)target);
    
    if (!me)
        return kIOReturnUnsupported;
    
    USBLog(4, "UHCI: %p: PowerDownHandler %x %x", me, messageType, messageArgument);
    switch (messageType)
    {
        case kIOMessageSystemWillRestart:
        case kIOMessageSystemWillPowerOff:
            if (me->_powerLevel == kUHCIPowerLevelRunning) {
                if ( me->_rootHubDevice )
                {
                    me->_rootHubDevice->terminate(kIOServiceRequired | kIOServiceSynchronous);
                    me->_rootHubDevice->detachAll(gIOUSBPlane);
                    me->_rootHubDevice->release();
                    me->_rootHubDevice = NULL;
                }
                
                me->SuspendController();
                me->_powerLevel = kUHCIPowerLevelSuspend;
            }
            if (me->_powerLevel != kUHCIPowerLevelSuspend) {
                me->UIMFinalizeForPowerDown();
            }
            break;
            
        default:
            // We don't care about any other message that comes in here.
            break;
            
    }
    return kIOReturnSuccess;
}


