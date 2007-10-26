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
 *  PCSCDevice.h
 *  SmartCardServices
 *
 */

#ifndef _H_PCSCDEVICE
#define _H_PCSCDEVICE

#include <security_utilities/iodevices.h>
#include <security_utilities/refcount.h>

#if defined(__cplusplus)

namespace PCSCD {

class Device : public IOKit::Device, public RefCount
{
public:
//	Device() :  { }
	Device(io_service_t d) : IOKit::Device(d) { }

	virtual ~Device() throw();

	bool operator < (const Device &other) const { return this->address() < other.address(); }

	void setAddress(uint32_t address)  { mAddress = address; }
	void setInterfaceClass(uint32_t interfaceClass)  { mInterfaceClass = interfaceClass; }
	void setDeviceClass(uint32_t deviceClass)  { mDeviceClass = deviceClass; }
	void setVendorid(uint32_t vendorid)  { mVendorid = vendorid; }
	void setProductid(uint32_t productid)  { mProductid = productid; }
	void setPath(const std::string path)  { mLibPath = path; }
	void setName(const std::string name)  { mName = name; }
	void setIsPCCard(bool isPCCard)  { mIsPCCard = isPCCard; }

	uint32_t address() const { return mAddress; }
	uint32_t interfaceClass() const { return mInterfaceClass; }
	uint32_t deviceClass() const { return mDeviceClass; }
	uint32_t vendorid() const { return mVendorid; }
	uint32_t productid() const { return mProductid; }
	std::string path() const { return mLibPath; }
	std::string name() const { return mName; }
	bool isPCCard() const { return mIsPCCard; }
	
	std::string vendorName() const { return mVendorName; }
	std::string productName() const { return mProductName; }
	std::string serialNumber() const { return mSerialNumber; }

	void setDebugParams(const std::string vendorName, const std::string productName,
		const std::string serialNumber)
		{ mVendorName = vendorName; mProductName = productName; mSerialNumber = serialNumber;}
	
	void dump();
	
private:

	uint32_t mAddress;

	std::string mName;			// Manufacturer's name for device
	std::string mLibPath;		// path to driver bundle from PCSCDriverBundle

	uint32_t mInterfaceClass;	// If present, one of kUSBChipSmartCardInterfaceClass/kUSBVendorSpecificInterfaceClass
	uint32_t mDeviceClass;		// If == kUSBVendorSpecificClass, check vendor/product
	uint32_t mVendorid;
	uint32_t mProductid;

	bool mIsPCCard;

	// Mainly for debugging
	std::string mVendorName, mProductName, mSerialNumber;
};

} // end namespace PCSCD

#endif /* __cplusplus__ */

#endif // _H_PCSCDEVICE
