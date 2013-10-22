/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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


//
// powerwatch - hook into system notifications of power events
//
#include "powerwatch.h"
#include <IOKit/IOMessage.h>


namespace Security {
namespace MachPlusPlus {


//
// The obligatory empty virtual destructor
//
PowerWatcher::~PowerWatcher()
{ }


//
// The default NULL implementations of the callback virtuals.
// We define these (rather than leaving them abstract) since
// many users want only one of these events.
//
void PowerWatcher::systemWillSleep()
{ }

void PowerWatcher::systemIsWaking()
{ }

void PowerWatcher::systemWillPowerDown()
{ }

void PowerWatcher::systemWillPowerOn()
{ }

//
// IOPowerWatchers
//

void
IOPowerWatcher::iopmcallback(void *				param, 
			     IOPMConnection                    connection,
			     IOPMConnectionMessageToken        token, 
			     IOPMSystemPowerStateCapabilities	capabilities)
{
    IOPowerWatcher *me = (IOPowerWatcher *)param;

    if (SECURITY_DEBUG_LOG_ENABLED()) {
	secdebug("powerwatch", "powerstates");
	if (capabilities & kIOPMSystemPowerStateCapabilityDisk)
	    secdebug("powerwatch", "disk");
	if (capabilities & kIOPMSystemPowerStateCapabilityNetwork)
	    secdebug("powerwatch", "net");
	if (capabilities & kIOPMSystemPowerStateCapabilityAudio) 
	    secdebug("powerwatch", "audio");
	if (capabilities & kIOPMSystemPowerStateCapabilityVideo)
	    secdebug("powerwatch", "video");
    }

    /* if cpu and no display -> in DarkWake */
    if ((capabilities & (kIOPMSystemPowerStateCapabilityCPU|kIOPMSystemPowerStateCapabilityVideo)) == kIOPMSystemPowerStateCapabilityCPU) {
        secdebug("powerwatch", "enter DarkWake");
	me->mInDarkWake = true;
    } else if (me->mInDarkWake) {
        secdebug("powerwatch", "exit DarkWake");
	me->mInDarkWake = false;
    }

    (void)IOPMConnectionAcknowledgeEvent(connection, token);

    return;
}


void
IOPowerWatcher::setupDarkWake()
{
    IOReturn            ret;
    
    mInDarkWake = false;

    ret = ::IOPMConnectionCreate(CFSTR("IOPowerWatcher"),
				 kIOPMSystemPowerStateCapabilityDisk 
				 | kIOPMSystemPowerStateCapabilityNetwork
				 | kIOPMSystemPowerStateCapabilityAudio 
				 | kIOPMSystemPowerStateCapabilityVideo,
				 &mIOPMconn);
    if (ret == kIOReturnSuccess) {
        ret = ::IOPMConnectionSetNotification(mIOPMconn, this,
                          (IOPMEventHandlerType)iopmcallback);
        if (ret == kIOReturnSuccess) {
            ::IOPMConnectionSetDispatchQueue(mIOPMconn, mIOPMqueue);
        }
    }

    dispatch_group_leave(mDarkWakeGroup);
}

IOPowerWatcher::IOPowerWatcher()
{
	if (!(mKernelPort = ::IORegisterForSystemPower(this, &mPortRef, ioCallback, &mHandle)))
		UnixError::throwMe(EINVAL);	// no clue

	mIOPMqueue = dispatch_queue_create("com.apple.security.IOPowerWatcher", NULL);
	if (mIOPMqueue == NULL)
		return;

	// Running in background since this will wait for the power
	// management in configd and we are not willing to block on
	// that, power events will come in when they do.
	mDarkWakeGroup = dispatch_group_create();
	dispatch_group_enter(mDarkWakeGroup);
	dispatch_async(mIOPMqueue, ^ { setupDarkWake(); });
}

IOPowerWatcher::~IOPowerWatcher()
{
	// Make sure to wait until the asynchronous method
	// finishes, to avoid <rdar://problem/14355434>
	if (mDarkWakeGroup) {
		::dispatch_group_wait(mDarkWakeGroup, DISPATCH_TIME_FOREVER);
		::dispatch_release(mDarkWakeGroup);
	}
	if (mKernelPort)
		::IODeregisterForSystemPower(&mHandle);

	if (mIOPMconn) {
		::IOPMConnectionSetDispatchQueue(mIOPMconn, NULL);
		::IOPMConnectionRelease(mIOPMconn);
	}
	if (mIOPMqueue)
		::dispatch_release(mIOPMqueue);
}


//
// The callback dispatcher
//
void IOPowerWatcher::ioCallback(void *refCon, io_service_t service,
    natural_t messageType, void *argument)
{
    IOPowerWatcher *me = (IOPowerWatcher *)refCon;
    enum { allow, refuse, ignore } reaction;
    switch (messageType) {
    case kIOMessageSystemWillSleep:
        secdebug("powerwatch", "system will sleep");
        me->systemWillSleep();
        reaction = allow;
        break;
    case kIOMessageSystemHasPoweredOn:
        secdebug("powerwatch", "system has powered on");
        me->systemIsWaking();
        reaction = ignore;
        break;
    case kIOMessageSystemWillPowerOff:
        secdebug("powerwatch", "system will power off");
        me->systemWillPowerDown();
        reaction = allow;
        break;
    case kIOMessageSystemWillNotPowerOff:
        secdebug("powerwatch", "system will not power off");
        reaction = ignore;
        break;
    case kIOMessageCanSystemSleep:
        secdebug("powerwatch", "can system sleep");
        reaction = allow;
        break;
    case kIOMessageSystemWillNotSleep:
        secdebug("powerwatch", "system will not sleep");
        reaction = ignore;
        break;
    case kIOMessageCanSystemPowerOff:
        secdebug("powerwatch", "can system power off");
        reaction = allow;
        break;
	case kIOMessageSystemWillPowerOn:
        secdebug("powerwatch", "system will power on");
		me->systemWillPowerOn();
        reaction = ignore;
        break;
    default:
        secdebug("powerwatch",
            "type 0x%x message received (ignored)", messageType);
        reaction = ignore;
        break;
    }
    
    // handle acknowledgments
    switch (reaction) {
    case allow:
		secdebug("powerwatch", "calling IOAllowPowerChange");
        IOAllowPowerChange(me->mKernelPort, long(argument));
        break;
    case refuse:
		secdebug("powerwatch", "calling IOCancelPowerChange");
        IOCancelPowerChange(me->mKernelPort, long(argument));
        break;
    case ignore:
		secdebug("powerwatch", "sending no response");
        break;
    }
}


//
// The MachServer hookup
//
PortPowerWatcher::PortPowerWatcher()
{
    port(IONotificationPortGetMachPort(mPortRef));
}

boolean_t PortPowerWatcher::handle(mach_msg_header_t *in)
{
    IODispatchCalloutFromMessage(NULL, in, mPortRef);
    return TRUE;
}


} // end namespace MachPlusPlus

} // end namespace Security
