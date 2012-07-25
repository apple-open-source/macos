/*
 * Copyright (c) 2004-2007 Apple, Inc. All Rights Reserved.
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
// iodevices - code for finding and tracking devices via IOKit
//
#include "iodevices.h"
#include <security_utilities/cfutilities.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBLib.h>

using namespace MachPlusPlus;


namespace Security {
namespace IOKit {


//
// Master Ports.
// Note that multiple MasterPort objects may refer to the same Mach port.
//
MasterPort::MasterPort()
{
	Error::check(::IOMasterPort(MACH_PORT_NULL, &port()));
}


//
// IOKit Devices (a small subset of functionality)
//
Device::~Device()
{
	::IOObjectRelease(mService);
}

string Device::name() const
{
	io_name_t tName;
	Error::check(::IORegistryEntryGetName(mService, tName));
	return tName;
}

string Device::name(const char *plane) const
{
	io_name_t tName;
	Error::check(::IORegistryEntryGetNameInPlane(mService, plane, tName));
	return tName;
}

string Device::path(const char *plane) const
{
	io_string_t tPath;
	Error::check(::IORegistryEntryGetPath(mService, plane, tPath));
	return tPath;
}

CFDictionaryRef Device::properties() const
{
	CFMutableDictionaryRef dict;
	Error::check(::IORegistryEntryCreateCFProperties(mService, &dict, NULL, 0));
	return dict;
}

CFTypeRef Device::property(const char *name) const
{
	return ::IORegistryEntryCreateCFProperty(mService, CFTempString(name), NULL, 0);
}


//
// DeviceIterators
//
DeviceIterator::~DeviceIterator()
{
	// drain the iterator to avoid port leakage
	while (Device dev = (*this)())
		;
}


io_service_t DeviceIterator::operator () ()
{
	io_service_t dev = ::IOIteratorNext(mIterator);
	mAtEnd = !dev;
	return dev;
}


//
// DeviceMatches
//
DeviceMatch::DeviceMatch()
	: CFRef<CFMutableDictionaryRef>(makeCFMutableDictionary())
{
	CFError::check(*this);
}

DeviceMatch::DeviceMatch(const char *cls)
	: CFRef<CFMutableDictionaryRef>(::IOServiceMatching(cls))
{
	CFError::check(*this);
}

DeviceMatch::DeviceMatch(const char *cls, const char *name, uint32_t value, ...)
	: CFRef<CFMutableDictionaryRef>(::IOServiceMatching(cls))
{
	CFError::check(*this);
	
	va_list args;
	va_start(args, value);
	while (name) {
		add(name, value);
		name = va_arg(args, const char *);
		if (!name)
			break;
		value = va_arg(args, uint32_t);
	}
}

void DeviceMatch::add(const char *name, uint32_t value)
{
	CFRef<CFNumberRef> number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
	CFDictionarySetValue(*this, CFTempString(name), number);
}


//
// NotificationPorts
//
NotificationPort::NotificationPort()
{
	CFError::check(mPortRef = ::IONotificationPortCreate(MasterPort()));
}

NotificationPort::NotificationPort(const MasterPort &master)
{
	CFError::check(mPortRef = ::IONotificationPortCreate(master));
}

NotificationPort::~NotificationPort()
{
	::IONotificationPortDestroy(mPortRef);
}


mach_port_t NotificationPort::port() const
{
	mach_port_t p = ::IONotificationPortGetMachPort(mPortRef);
	CFError::check(p);
	return p;
}

CFRunLoopSourceRef NotificationPort::source() const
{
	CFRunLoopSourceRef rls = ::IONotificationPortGetRunLoopSource(mPortRef);
	CFError::check(rls);
	return rls;
}


void NotificationPort::add(const DeviceMatch &match, Receiver &receiver, const char *type)
{
	io_iterator_t iterator;
	CFRetain(match);	// compensate for IOSAMN not retaining its argument
	Error::check(::IOServiceAddMatchingNotification(mPortRef, type,
		match,
		ioNotify, &receiver,
		&iterator));
	
	// run initial iterator to process existing devices
	secdebug("iokit", "dispatching initial device match iterator %d", iterator);
	DeviceIterator it(iterator);
	receiver.ioChange(it);
}

void NotificationPort::addInterestNotification(Receiver &receiver, io_service_t service, 
	const io_name_t interestType)
{
	io_iterator_t iterator;
	mach_port_t pp = NotificationPort::port();

	secdebug("iokit", "NotificationPort::addInterest - type: %s [port: %p (0x%08X), service: 0x%08X]",
		interestType, mPortRef, pp, service);

	// We cannot throw if we get an error here since we will receive notifications
	// from each plane, and not all planes have the necessary information to be
	// able to add an interest notification
	kern_return_t kr = ::IOServiceAddInterestNotification(mPortRef,
		service, interestType, ioDeviceNotification, &receiver, &iterator);
	const char *msgstr = mach_error_string(kr);
	const char *msgtyp = mach_error_type(kr);
	if (msgstr && msgtyp)
		secdebug("iokit", " msg: %s, typ: %s", msgstr, msgtyp);
}

void NotificationPort::ioNotify(void *refCon, io_iterator_t iterator)
{
	secdebug("iokit", "dispatching new device match iterator %d", iterator);
	DeviceIterator it(iterator);
	try {
		reinterpret_cast<Receiver *>(refCon)->ioChange(it);
	} catch (...) {
		secdebug("iokit", "ioChange callback threw an exception (ignored)");
	}
}

void NotificationPort::ioDeviceNotification(void *refCon, io_service_t service,
	natural_t messageType, void *messageArgument)
{
	secdebug("iokit", "dispatching NEW device notification iterator, service 0x%08X, msg: 0x%04X, arg: %p", 
		service, messageType, messageArgument);

	const char *msgstr = mach_error_string(messageType);
	const char *msgtyp = mach_error_type(messageType);
	if (msgstr && msgtyp)
		secdebug("iokit", " msg: %s, typ: %s", msgstr, msgtyp);
	
	if (service!=io_service_t(-1))
		reinterpret_cast<Receiver *>(refCon)->ioServiceChange(refCon, service, messageType, messageArgument);
}

//
// Abstract NotificationPort::Receivers
//
NotificationPort::Receiver::~Receiver()
{ /* virtual */ }


//
// MachPortNotificationPorts
//
MachPortNotificationPort::MachPortNotificationPort()
{
	NoReplyHandler::port(NotificationPort::port());
}

MachPortNotificationPort::~MachPortNotificationPort()
{
}

boolean_t MachPortNotificationPort::handle(mach_msg_header_t *in)
{
	::IODispatchCalloutFromMessage(NULL, in, mPortRef);
    return TRUE;
}



} // end namespace IOKit
} // end namespace Security
