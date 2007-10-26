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
 *  PCSCDevice.cpp
 *  SmartCardServices
 *
 */

#include "PCSCDevice.h"
#include <security_utilities/debugging.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>

namespace PCSCD {

Device::~Device()  throw()
{
}

void Device::dump()
{
	//, serial: %s", // always empty for known readers, mSerialNumber.c_str());
	secdebug("device", "  Service: 0x%04X, Address: 0x%08X, vendor/product: 0x%04X/0x%04X, vendor/product: %s/%s", 
		ioObject(), mAddress, mVendorid, mProductid, mVendorName.c_str(), mProductName.c_str());
	secdebug("device", "     path: %s", path().c_str());
}

/*
Device::Device(const Device& x) throw()				// copy constructor
{
	*this = x;
}

Device& Device::operator= (const Device& x) throw()	// assignment operator
{
	mAddress = x.mAddress;
	mName = x.mName;
	mLibPath = x.mLibPath;
	return *this;
}
*/

} // end namespace PCSCD

