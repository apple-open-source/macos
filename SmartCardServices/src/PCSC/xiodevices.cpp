/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// xiodevices - additional code for finding and tracking devices via IOKit
// >>> move this iodevices.cpp when final
//
#include "xiodevices.h"
#include <security_utilities/cfutilities.h>
#include <security_utilities/mach++.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBLib.h>

using namespace MachPlusPlus;

namespace Security {
namespace IOKit {

void XNotificationPort::add(DeviceMatch match, XReceiver &receiver, const char *type)
{
	// The kIOProviderClassKey key is required in a matching dictionary. We extract it
	// here only for debugging purposes

	CFTypeRef valueRef = NULL;
	const char *pclass = "";
	CFRef<CFMutableDictionaryRef> theDict = match.dict();
	if (theDict && CFDictionaryGetValueIfPresent(theDict, CFSTR(kIOProviderClassKey), &valueRef) && 
		CFGetTypeID(valueRef) == CFStringGetTypeID())
		pclass = cfString(static_cast<CFStringRef>(valueRef)).c_str();
		
	// type is usually IOServiceMatched
	mach_port_t pp = NotificationPort::port();
	secdebug("iokit", "XNotificationPort::add - type: %s [port: %p (0x%08X), class: %s]",
		type, mPortRef, pp, pclass);	
		
//	CFShow(match.dict());
	// p (void)CFShow(match.dict())
	io_iterator_t iterator;
 	Error::check(::IOServiceAddMatchingNotification(mPortRef, type,
		match, ioNotify, &receiver, &iterator));
	CFRetain(match);	// compensate for IOSAMN not retaining its argument

	// run initial iterator to process existing devices
	secdebug("iokit", "dispatching INITIAL device match iterator %p", reinterpret_cast<void *>(iterator));
	DeviceIterator it(iterator);
	receiver.ioChange(it);
}

void XNotificationPort::addInterestNotification(XReceiver &receiver, io_service_t service, 
	const io_name_t interestType)
{
	io_iterator_t iterator;
	mach_port_t pp = NotificationPort::port();
//	MachPlusPlus::Port(pp).dump(0);
	secdebug("iokit", "XNotificationPort::addInterest - type: %s [port: %p (0x%08X), service: 0x%08X]",
		interestType, mPortRef, pp, service);	// IOServiceMatched
#if 1
	CFRunLoopSourceRef notificationRunLoopSource = IONotificationPortGetRunLoopSource(mPortRef);
	CFRunLoopSourceRef classRunLoopSource = NotificationPort::source();
//    IONotificationPortRef r_notify_port = IONotificationPortCreate(0);
	kern_return_t kr = ::IOServiceAddInterestNotification(mPortRef,	//,r_notify_port
		service, interestType, ioDeviceNotification, &receiver, &iterator);
	const char *msgstr = mach_error_string(kr);
	const char *msgtyp = mach_error_type(kr);
	if (msgstr && msgtyp)
		secdebug("iokit", " msg: %s, typ: %s", msgstr, msgtyp);
//	Error::check(kr);
//    if(r_notify_port) IOObjectRelease((io_object_t)r_notify_port);
#else
	Error::check(::IOServiceAddInterestNotification(mPortRef,
		service, interestType, ioDeviceNotification, &receiver, &iterator));
#endif
}

// callbacks

void XNotificationPort::ioNotify(void *refCon, io_iterator_t iterator)
{
	secdebug("iokit", "dispatching NEW device match iterator %p", reinterpret_cast<void *>(iterator));
	DeviceIterator it(iterator);
	reinterpret_cast<XReceiver *>(refCon)->ioChange(it);
}

void XNotificationPort::ioDeviceNotification(void *refCon, io_service_t service,
	natural_t messageType, void *messageArgument)
{
	secdebug("iokit", "dispatching NEW device notification iterator, service 0x%08X, msg: 0x%04X, arg: %p", 
		service, messageType, messageArgument);

	const char *msgstr = mach_error_string(messageType);
	const char *msgtyp = mach_error_type(messageType);
	if (msgstr && msgtyp)
		secdebug("iokit", " msg: %s, typ: %s", msgstr, msgtyp);
	
#if 0
	secdebug("iokit", "kIOMessageServiceIsTerminated: 0x%04X", kIOMessageServiceIsTerminated);
	secdebug("iokit", "kIOMessageServiceIsSuspended: 0x%04X", kIOMessageServiceIsSuspended);
	secdebug("iokit", "kIOMessageServiceIsResumed: 0x%04X", kIOMessageServiceIsResumed);
	secdebug("iokit", "kIOMessageServiceIsRequestingClose: 0x%04X", kIOMessageServiceIsRequestingClose);
	secdebug("iokit", "kIOMessageServiceIsAttemptingOpen: 0x%04X", kIOMessageServiceIsAttemptingOpen);
	secdebug("iokit", "kIOMessageServiceWasClosed: 0x%04X", kIOMessageServiceWasClosed);
	secdebug("iokit", "kIOMessageServiceBusyStateChange: 0x%04X", kIOMessageServiceBusyStateChange);
	secdebug("iokit", "kIOMessageServicePropertyChange: 0x%04X", kIOMessageServicePropertyChange);
	secdebug("iokit", "kIOMessageCanDevicePowerOff: 0x%04X", kIOMessageCanDevicePowerOff);
	secdebug("iokit", "kIOMessageDeviceWillPowerOff: 0x%04X", kIOMessageDeviceWillPowerOff);
	secdebug("iokit", "kIOMessageDeviceWillNotPowerOff: 0x%04X", kIOMessageDeviceWillNotPowerOff);
	secdebug("iokit", "kIOMessageDeviceHasPoweredOn: 0x%04X", kIOMessageDeviceHasPoweredOn);
	secdebug("iokit", "kIOMessageCanSystemPowerOff: 0x%04X", kIOMessageCanSystemPowerOff);
	secdebug("iokit", "iokit_vendor_specific_msg(0x000A): 0x%04X", iokit_vendor_specific_msg(0x000A));
#endif	

//	assert(service!=io_service_t(-1));
	if (service!=io_service_t(-1))
		reinterpret_cast<XReceiver *>(refCon)->ioServiceChange(refCon, service, messageType, messageArgument);
}


} // end namespace IOKit
} // end namespace Security


