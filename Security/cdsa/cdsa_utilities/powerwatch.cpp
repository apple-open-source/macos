/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
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
PowerWatcher::PowerWatcher()
{
    if (!(mKernelPort = IORegisterForSystemPower(this, &mPortRef, ioCallback, &mHandle)))
        UnixError::throwMe(EINVAL);	// no clue
}

PowerWatcher::~PowerWatcher()
{
    if (mKernelPort)
        IODeregisterForSystemPower(&mHandle);
}


//
// The callback dispatcher
//
void PowerWatcher::ioCallback(void *refCon, io_service_t service,
    natural_t messageType, void *argument)
{
    PowerWatcher *me = (PowerWatcher *)refCon;
    switch (messageType) {
    case kIOMessageSystemWillSleep:
        debug("powerwatch", "system will sleep");
        me->systemWillSleep();
        break;
    case kIOMessageSystemHasPoweredOn:
        debug("powerwatch", "system has powered on");
        me->systemIsWaking();
        break;
    case kIOMessageSystemWillPowerOff:
        debug("powerwatch", "system will power off");
        me->systemWillPowerDown();
        break;

#if !defined(NDEBUG)
    case kIOMessageSystemWillNotPowerOff:
        debug("powerwatch", "system will not power off");
        break;
    case kIOMessageCanSystemSleep:
        debug("powerwatch", "can system sleep");
        break;
    case kIOMessageSystemWillNotSleep:
        debug("powerwatch", "system will not sleep");
        break;
    case kIOMessageCanSystemPowerOff:
        debug("powerwatch", "can system power off");
        break;
    default:
        debug("powerwatch",
            "type 0x%x message received (ignored)", messageType);
        break;
#endif //NDEBUG
    }
    
    // always confirm
    IOAllowPowerChange(me->mKernelPort, long(argument));
}


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


//
// The MachServer hookup
//
PortPowerWatcher::PortPowerWatcher()
{
    port(IONotificationPortGetMachPort(mPortRef));
}

PortPowerWatcher::~PortPowerWatcher()
{
}

boolean_t PortPowerWatcher::handle(mach_msg_header_t *in)
{
    IODispatchCalloutFromMessage(NULL, in, mPortRef);
    return TRUE;
}


} // end namespace MachPlusPlus

} // end namespace Security
