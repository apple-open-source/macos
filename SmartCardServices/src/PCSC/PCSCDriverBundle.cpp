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
 *  PCSCDriverBundle.cpp
 *  SmartCardServices
 */

/*
	A driver bundle is a standard Mac OS X bundle that usually lives in the directory:
	
		/usr/libexec/SmartCardServices/drivers/
	
	The two major components of this bundle are the executable and the Info.plist. A single
	driver bundle may provide support for multiple readers. See
	
		<rdar://problem/4432039> pcscd crash for multiple VID/PIDs
	and
		<http://pcsclite.alioth.debian.org/ifdhandler-3/node7.html>

	The key that determines if a driver supports only one reader or multiple readers is
	"ifdVendorID", sometimes referred to as the manufacturer name. If this is a
	CFStringRef, then only one reader is supported; if it is a CFArrayRef, then
	multiple readers are supports. There are three fields for each reader:
	
		VendorID		uint32_t
		ProductID		uint32_t
		Friendly name	string

	See e.g. http://pcsclite.alioth.debian.org/ccid.html for a working driver with multiple IDs.

*/

#include "PCSCDriverBundle.h"
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/debugging.h>
#include <security_utilities/errors.h>
#include <IOKit/usb/USBSpec.h>
#include <IOKit/usb/USB.h>

#define DEBUG_BUNDLE_MATCHES 1

namespace PCSCD {

// Keys in CFDictionary for bundle's Info.plist
static const CFStringRef kManufacturerName	= CFSTR("ifdVendorID");
static const CFStringRef kProductName		= CFSTR("ifdProductID");
static const CFStringRef kFriendlyName		= CFSTR("ifdFriendlyName");
static const CFStringRef kInterfaceClass	= CFSTR("ifdInterfaceClass");
static const CFStringRef kInterfaceSubClass	= CFSTR("ifdInterfaceSubClass");
static const CFStringRef kInterfaceProtocol	= CFSTR("ifdInterfaceProtocol");

DriverBundle::DriverBundle(CFBundleRef bundle) : LoadableBundle(bundle)
{
	initialize(CFBundleGetInfoDictionary(bundle));
}

void DriverBundle::initialize(CFDictionaryRef dict)
{
	const int radix = 16;

	try
	{
		CFTypeRef vend = CFDictionaryGetValue(dict, kManufacturerName);
		if (!vend)
		{
			// Must be a class driver
			secdebug("pcscd", "Class Driver: %s", path().c_str());
			std::string istr(getStringAttr(dict,kInterfaceClass));
			uint8_t dclass = strtoul(istr.c_str(), NULL, radix);
			std::string sstr(getStringAttr(dict,kInterfaceSubClass));
			uint8_t dsubclass = strtoul(sstr.c_str(), NULL, radix);
			std::string pstr(getStringAttr(dict,kInterfaceProtocol));
			uint8_t dprotocol = strtoul(pstr.c_str(), NULL, radix);
			std::string name(getStringAttr(dict,kFriendlyName));
			DeviceDescription *dev = new DeviceDescription(dclass, dsubclass, dprotocol, name);
			addProduct(dev);
		}
		else
		if (CFGetTypeID(vend) == CFArrayGetTypeID())
		{
			secdebug("pcscd", "Driver with aliases: %s", path().c_str());
			CFTypeRef xprod = CFDictionaryGetValue(dict, kProductName);
			CFTypeRef xname = CFDictionaryGetValue(dict, kFriendlyName);
			if (!xprod || !xname || 
				(CFGetTypeID(xprod) != CFArrayGetTypeID()) || (CFGetTypeID(xname) != CFArrayGetTypeID()))
				CFError::throwMe();
			CFRef<CFArrayRef> products(reinterpret_cast<CFArrayRef>(xprod));
			CFRef<CFArrayRef> names   (reinterpret_cast<CFArrayRef>(xname));
			const int productCount = CFArrayGetCount(reinterpret_cast<CFArrayRef>(vend));
			// Make sure parallel arrays vendor, product, name are same size
			if ((productCount != CFArrayGetCount(products)) ||
				(productCount != CFArrayGetCount(names)))
				CFError::throwMe();

			for (int ix=0;ix<productCount;++ix)
			{
				std::string vstr(getStringAttr(reinterpret_cast<CFArrayRef>(vend), ix));
				uint16_t vendor = strtoul(vstr.c_str(), NULL, radix);
				std::string pstr(getStringAttr(products, ix));
				uint16_t product = strtoul(pstr.c_str(), NULL, radix);
				std::string name(getStringAttr(names, ix));
				DeviceDescription *dev = new DeviceDescription(vendor, product, name);
				addProduct(dev);
			}
		}
		else
		if (CFGetTypeID(vend) == CFStringGetTypeID())
		{
			secdebug("pcscd", "Driver for single product: %s", path().c_str());
			std::string vstr(cfString(reinterpret_cast<CFStringRef>(vend)));
			uint16_t vendor = strtoul(vstr.c_str(), NULL, radix);
			std::string pstr(getStringAttr(dict,kProductName));
			uint16_t product = strtoul(pstr.c_str(), NULL, radix);
			std::string name(getStringAttr(dict,kFriendlyName));
			DeviceDescription *dev = new DeviceDescription(vendor, product, name);
			addProduct(dev);
		}
		else
			CFError::throwMe();
	}
	catch (...)
	{
		secdebug("pcscd", "Malformed Info.plist for: %s", path().c_str());
        secdebug("pcscd", "error getting plugin directory bundles");
		return;
	}

	dump();
}

std::string DriverBundle::getStringAttr(CFDictionaryRef dict, CFStringRef key)
{
	// Do some sanity checking on potential string values in the plist
	CFTypeRef attr = CFDictionaryGetValue(dict, key);
	if (!attr)
		return std::string();
	if (CFGetTypeID(attr) != CFStringGetTypeID())
		CFError::throwMe();
	
	return std::string(cfString(static_cast<CFStringRef>(attr)));
}

std::string DriverBundle::getStringAttr(CFArrayRef arr, CFIndex idx)
{
	// Do some sanity checking on potential string values in the plist
	CFTypeRef attr = CFArrayGetValueAtIndex(arr, idx);
	if (!attr)
		return std::string();
	if (CFGetTypeID(attr) != CFStringGetTypeID())
		CFError::throwMe();
	
	return std::string(cfString(static_cast<CFStringRef>(attr)));
}

DriverBundle::~DriverBundle() throw()
{
	// delete supported devices objects
}

uint32_t DriverBundle::matches(const PCSCD::Device &device, std::string &name) const
{
	// Searches for a driver bundle that matches device. If found,
	// it sets the libpath for the device and returns true.

#ifdef DEBUG_BUNDLE_MATCHES
	secdebug("device", " DEVICE: vendor/product: 0x%04X/0x%04X, interfaceClass: 0x%04X, vendor/product:  %s/%s", 
		device.vendorid(), device.productid(), device.interfaceClass(),
		device.vendorName().c_str(), device.productName().c_str());
#endif

	// Look for a manufacturer-specific driver first
	for (ConstDeviceDescriptionIterator it=mDeviceDescriptions.begin();it!=mDeviceDescriptions.end();++it)
	{
		const DeviceDescription *desc = static_cast<DeviceDescription *>(*it);
#ifdef DEBUG_BUNDLE_MATCHES
		secdebug("device", "   DESC: vendor/product: 0x%04X/0x%04X, interfaceClass: 0x%04X, path: %s", 
			desc->vendorid(), desc->productid(), desc->interfaceClass(), path().c_str());
#endif
		if (desc->vendorid()  && (desc->vendorid()==device.vendorid()) &&
			desc->productid() && (desc->productid()==device.productid()))
		{
			name = desc->name();
			return eMatchVendorSpecific;
		}
	}

	if (device.interfaceClass())
		for (ConstDeviceDescriptionIterator it=mDeviceDescriptions.begin();it!=mDeviceDescriptions.end();++it)
		{
			const DeviceDescription *desc = static_cast<DeviceDescription *>(*it);
			if (desc->interfaceClass() && (desc->interfaceClass()==device.interfaceClass()))
			{
				name = desc->name();
				return eMatchInterfaceClass;
			}
		}

	return eMatchNone;
}		

#pragma mark -------------------- Operators --------------------

bool DriverBundle::operator < (const DriverBundle &other) const throw()
{
	return this->path() < other.path();
}

bool DeviceDescription::operator < (const DeviceDescription &other) const throw()
{
    if (this->mVendor >= other.mVendor)
		return false;

    return (this->mProduct < other.mProduct);
}

#pragma mark -------------------- Debugging Routines --------------------

void DriverBundle::dump()
{
#ifndef NDEBUG
	secdebug("pcscd", "Driver at path: %s", path().c_str());
	for (DeviceDescriptionIterator it = mDeviceDescriptions.begin(); it != mDeviceDescriptions.end();++it)
		(*it)->dump();
#endif
}

void DeviceDescription::dump()
{
#ifndef NDEBUG
	secdebug("pcscd", "   Friendly name: %s", mFriendlyName.c_str());
	if (interfaceClass())
		secdebug("pcscd", "   Class: 0x%02X  SubClass: 0x%02X  Protocol: 0x%02X",
			mDeviceClass,mDeviceSubClass,mDeviceProtocol);
	else
		secdebug("pcscd", "   VendorID: 0x%04X  ProductID: 0x%04X", mVendor, mProduct);
#endif
}

} // end namespace PCSCD
