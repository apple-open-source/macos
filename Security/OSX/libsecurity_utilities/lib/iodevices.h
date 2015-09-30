/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_IODEVICES
#define _H_IODEVICES

#include <security_utilities/mach++.h>
#include <security_utilities/machserver.h>
#include <security_utilities/cfutilities.h>
#include <IOKit/pwr_mgt/IOPMLib.h>


namespace Security {
namespace IOKit {


//
// An IOKit master port
//
class MasterPort : public MachPlusPlus::AutoPort {
public:
	MasterPort();
};


//
// An IOKit device
//
class Device {
public:
	Device() : mService(MACH_PORT_NULL) { }
	Device(io_service_t d) : mService(d) { }
	~Device();
	
	io_service_t ioObject() const { return mService; }
	
	operator bool () const		{ return mService; }
	bool operator ! () const	{ return !mService; }
	
	std::string name() const;
	std::string name(const char *plane) const;
	std::string path(const char *plane = kIOServicePlane) const;
	CFDictionaryRef properties() const;
	CFTypeRef property(const char *name) const;
	
	template <class T>
	T property(const char *name) const
	{ return static_cast<T>(property(name)); }

private:
	io_service_t mService;
};


//
// A device iterator.
// This isn't an STL iterator. It's not important enough.
//
class DeviceIterator {
public:
	DeviceIterator(io_iterator_t iter) : mIterator(iter), mAtEnd(false) { }
	~DeviceIterator();

	io_service_t operator () ();

private:
	io_iterator_t mIterator;		// iterator port (handle)
	bool mAtEnd;					// already hit end
};


//
// An IOKit-style device matching dictionary, with convenient helper methods
//
class DeviceMatch : public CFRef<CFMutableDictionaryRef> {
	NOCOPY(DeviceMatch)
public:
	DeviceMatch(CFMutableDictionaryRef dict) : CFRef<CFMutableDictionaryRef>(dict) { }		// this dictionary
	DeviceMatch();							// empty dictionary
	DeviceMatch(const char *cls);			// this class
	DeviceMatch(const char *cls, const char *name, uint32_t value, ...); // class plus value pairs
	
	CFRef<CFMutableDictionaryRef> &dict() { return static_cast<CFRef<CFMutableDictionaryRef> &>(*this); }
	operator bool () const { return this->get() != NULL; }
	bool operator ! () const { return this->get() == NULL; }
	
	void add(const char *name, uint32_t value);	// add one name/value pair
};


//
// An IOKit notification port object
//
class NotificationPort {
public:
	NotificationPort();
	NotificationPort(const MasterPort &master);
	~NotificationPort();
	
	mach_port_t port() const;
	operator mach_port_t () const	{ return port(); }
	
	CFRunLoopSourceRef source() const;
	
	class Receiver {
	public:
		virtual ~Receiver();
		
		virtual void ioChange(DeviceIterator &iterator) = 0;
		virtual void ioServiceChange(void *refCon, io_service_t service,	//IOServiceInterestCallback
			natural_t messageType, void *messageArgument) {}
	};
	
	void add(const DeviceMatch &match, Receiver &receiver, const char *type = kIOFirstMatchNotification);
	void addInterestNotification(Receiver &receiver, io_service_t service,
		const io_name_t interestType = kIOGeneralInterest);

private:

	static void ioNotify(void *refCon, io_iterator_t iterator);
	static void ioDeviceNotification(void *refCon, io_service_t service,
		natural_t messageType, void *messageArgument);

protected:
	IONotificationPortRef mPortRef;
};


class MachPortNotificationPort : public NotificationPort, public MachPlusPlus::MachServer::NoReplyHandler {
public:
	MachPortNotificationPort();
	virtual ~MachPortNotificationPort();
    
private:
    boolean_t handle(mach_msg_header_t *in);    
};


} // end namespace MachPlusPlus
} // end namespace Security

#endif //_H_IODEVICES
