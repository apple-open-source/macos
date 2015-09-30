/*
 * Copyright (c) 2000-2001,2003-2004,2011-2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_POWERWATCH
#define _H_POWERWATCH

#include <security_utilities/machserver.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>


namespace Security {
namespace MachPlusPlus {


//
// PowerWatcher embodies the ability to respond to power events.
// By itself, it is inert - nobody will call its virtual methods.
// Use one of it subclasses, which take care of "hooking" into an
// event delivery mechanism.
//
class PowerWatcher {
public:
    virtual ~PowerWatcher();
    
public:
    virtual void systemWillSleep();
    virtual void systemIsWaking();
    virtual void systemWillPowerDown();
	virtual void systemWillPowerOn();

    bool inDarkWake() { return mInDarkWake; }

 protected:
    bool mInDarkWake;
};


//
// A PowerWatcher that is dispatches events from an IOKit message
//
class IOPowerWatcher : public PowerWatcher {
public:
	IOPowerWatcher();
	~IOPowerWatcher();
    
protected:
    io_connect_t mKernelPort;
    IONotificationPortRef mPortRef;
    io_object_t mHandle;
    IOPMConnection mIOPMconn;
    dispatch_queue_t mIOPMqueue;
    dispatch_group_t mDarkWakeGroup;
    IOPMNotificationHandle mUserActiveHandle;
    
    static void ioCallback(void *refCon, io_service_t service,
        natural_t messageType, void *argument);

    static void
    iopmcallback(void * param,  IOPMConnection connection,
		 IOPMConnectionMessageToken token, 
		 IOPMSystemPowerStateCapabilities capabilities);

    void setupDarkWake();

};


//
// Hook into a "raw" MachServer object for event delivery
//
class PortPowerWatcher : public IOPowerWatcher, public MachServer::NoReplyHandler {
public:
    PortPowerWatcher();
    
    boolean_t handle(mach_msg_header_t *in);    
};


//
// Someone should add a RunLoopPowerWatcher class here, I suppose.
// Well, if you need one: Tag, You're It!
//


} // end namespace MachPlusPlus

} // end namespace Security

#endif //_H_POWERWATCH
