/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_IO_STORAGE_DEVICE_CHARACTERISTICS_H_
#define _IOKIT_IO_STORAGE_DEVICE_CHARACTERISTICS_H_


//
//	Protocol Characteristics - Characteristics defined for protocols.
//

// This key is used to define Protocol Characteristics for a particular protocol (e.g. FireWire, USB)
// and it has an associated dictionary which lists the protocol characteristics.
#define	kIOPropertyProtocolCharacteristicsKey		"Protocol Characteristics"

// An identifier that will uniquely identify this SCSI Domain for the Physical Interconnect type.
// This identifier is only guaranteed to be unique for any given Physical Interconnect and is
// not guaranteed to be the same across restarts or shutdowns.
#define kIOPropertySCSIDomainIdentifierKey			"SCSI Domain Identifier"

// This is the SCSI Target Identifier for a given SCSI Target Device
#define kIOPropertySCSITargetIdentifierKey			"SCSI Target Identifier"

// This key is the SCSI Logical Unit Number for the device server controlled
// by the driver
#define kIOPropertySCSILogicalUnitNumberKey			"SCSI Logical Unit Number"

// This key is used to define the Physical Interconnect to which a device is
// attached (e.g. ATAPI, FireWire).
#define kIOPropertyPhysicalInterconnectTypeKey		"Physical Interconnect"

// This key is used to define the Physical Interconnect Location (e.g. Internal).
#define kIOPropertyPhysicalInterconnectLocationKey	"Physical Interconnect Location"

// This key defines the location of Internal. If the device is connected to an internal
// bus, this key should be set.
#define kIOPropertyInternalKey						"Internal"

// This key defines the location of External. If the device is connected to an external
// bus, this key should be set.
#define kIOPropertyExternalKey						"External"

// This key defines the location of Internal/External. If the device is connected to
// a bus and it is indeterminate whether it it internal or external, this key should be set.
#define kIOPropertyInternalExternalKey				"Internal/External"

// This protocol characteristics key is used to inform the system that the protocol
// supports having multiple devices that act as initiators.
#define kIOPropertySCSIProtocolMultiInitKey			"Multiple Initiators"


//
//	Device Characteristics - Characteristics defined for devices.
//

// This key is used to define Device Characteristics for a particular device (e.g. CD-ROM drive)
// and it has an associated dictionary which lists the device characteristics. The device
// characteristics are Command Set specific and are listed in the header files for each command
// set.
#define kIOPropertyDeviceCharacteristicsKey			"Device Characteristics"


// This key is used to define the Vendor Name for a particular device (e.g. Apple)
// and it has an associated string.
#define kIOPropertyVendorNameKey					"Vendor Name"

// This key is used to define the Product Name for a particular device (e.g. CD-600)
// and it has an associated string.
#define kIOPropertyProductNameKey					"Product Name"

// This key is used to define the Product Revision Level for a particular device
// (e.g. 1.0a) and it has an associated string.
#define kIOPropertyProductRevisionLevelKey			"Product Revision Level"


#endif	/* _IOKIT_IO_STORAGE_DEVICE_CHARACTERISTICS_H_ */
