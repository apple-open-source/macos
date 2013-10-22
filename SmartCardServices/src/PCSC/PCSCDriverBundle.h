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

/*
 *  PCSCDriverBundle.h
 *  SmartCardServices
 */

#ifndef _H_XPCSCDRIVERBUNDLE
#define _H_XPCSCDRIVERBUNDLE

#include <string>
#include <vector>
#include <MacTypes.h>
#include <security_utilities/refcount.h>
#include <security_utilities/osxcode.h>
#include "PCSCDevice.h"

#if defined(__cplusplus)

namespace PCSCD {

class DeviceDescription
{
public:

	DeviceDescription() { }
	DeviceDescription(uint16_t vendor, uint16_t product, std::string name) :
		mVendor(vendor), mProduct(product),
		mDeviceClass(0), mDeviceSubClass(0), mDeviceProtocol(0),
		mFriendlyName(name) {}
	DeviceDescription(uint8_t deviceClass, uint8_t deviceSubClass, uint8_t protocol, std::string name) :
		mVendor(0), mProduct(0),
		mDeviceClass(deviceClass), mDeviceSubClass(deviceSubClass), mDeviceProtocol(protocol),
		mFriendlyName(name) {}

	bool operator < (const DeviceDescription &other) const throw();

	uint8_t interfaceClass() const	{ return mDeviceClass; }
	uint16_t vendorid() const { return mVendor; }
	uint16_t productid() const { return mProduct; }
	std::string name() const { return mFriendlyName; }

	void dump();

protected:
	// Match types from <IOKit/USB.h> for IOUSBDeviceDescriptor
	
	uint16_t mVendor;			// Unique vendor's manufacturer code assigned by the USB-IF
	uint16_t mProduct;			// Manufacturer's unique product code

	uint8_t mDeviceClass;
	uint8_t mDeviceSubClass;
	uint8_t mDeviceProtocol;

	std::string mFriendlyName;	// Manufacturer's name for device
};

/*
 * An aggregation of useful information on a driver bundle in the
 * drop directory.
 */

class DriverBundle : public LoadableBundle
{
private:
	DriverBundle(const char *pathname) : LoadableBundle(pathname) { }

public:
	DriverBundle(CFBundleRef bundle);

	virtual ~DriverBundle() throw();

	bool operator < (const DriverBundle &other) const throw();

	void addProduct(DeviceDescription *dev) { mDeviceDescriptions.push_back(dev); }

	uint32_t matches(const Device &device, std::string &name) const;
	
	enum 
	{
		eMatchNone = 0,
		eMatchInterfaceClass,	// must be less than eMatchVendorSpecific
		eMatchVendorSpecific
	};

protected:
	void initialize(CFDictionaryRef dict);

private:

	typedef std::vector<DeviceDescription *> DeviceDescriptions;
    typedef DeviceDescriptions::iterator DeviceDescriptionIterator;
    typedef DeviceDescriptions::const_iterator ConstDeviceDescriptionIterator;
	DeviceDescriptions mDeviceDescriptions;

	std::string getStringAttr(CFDictionaryRef dict, CFStringRef key);
	std::string getStringAttr(CFArrayRef arr, CFIndex idx);
	void dump();
};

} // end namespace PCSCD

#endif /* __cplusplus__ */

#endif /* _H_XPCSCDRIVERBUNDLE */
