/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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


#import <Foundation/Foundation.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/usb/USB.h>
#import <IOKit/usb/USBSpec.h>
#import <IOKit/IOCFPlugIn.h>
#import "BusProbeClass.h"

typedef struct 	IOUSBDeviceStruct320**	 			IOUSBDeviceRef ;
typedef struct 	IOUSBInterfaceStruct220**	 		IOUSBInterfaceRef ;

#define ROOT_LEVEL                              0
#define DEVICE_DESCRIPTOR_LEVEL			ROOT_LEVEL + 1
#define CONFIGURATION_DESCRIPTOR_LEVEL          ROOT_LEVEL + 1
#define INTERFACE_LEVEL                         CONFIGURATION_DESCRIPTOR_LEVEL + 1
#define ENDPOINT_LEVEL                          INTERFACE_LEVEL + 1
#define HID_DESCRIPTOR_LEVEL			INTERFACE_LEVEL + 1
#define DFU_DESCRIPTOR_LEVEL			INTERFACE_LEVEL + 1
#define HUB_DESCRIPTOR_LEVEL			ROOT_LEVEL + 1
#define DEVICE_QUAL_DESCRIPTOR_LEVEL            ROOT_LEVEL + 1

enum {
    kIntegerOutputStyle = 0,
    kHexOutputStyle = 1
};

IOReturn GetPortInformation( IOUSBDeviceRef deviceIntf, uint32_t * portInfo );
int GetDeviceLocationID( IOUSBDeviceRef deviceIntf, UInt32 * locationID );
int GetDeviceSpeed( IOUSBDeviceRef deviceIntf, UInt8 * speed );
int GetDeviceAddress( IOUSBDeviceRef deviceIntf, USBDeviceAddress * address );
int SuspendDevice( IOUSBDeviceRef deviceIntf, BOOL suspend );
IOReturn GetDescriptor(IOUSBDeviceRef deviceIntf, UInt8 descType, UInt8 descIndex, void *buf, UInt16 len, IOReturn *actError);
int GetStringDescriptor(IOUSBDeviceRef deviceIntf, UInt8 descIndex, void *buf, UInt16 len, UInt16 lang);
int GetClassDescriptor(IOUSBDeviceRef deviceIntf, UInt8 descType, UInt8 descIndex, void *buf, UInt16 len);
int GetDescriptorFromInterface(IOUSBDeviceRef deviceIntf, UInt8 descType, UInt8 descIndex, UInt16 wIndex, void *buf, UInt16 len);
//BusProbeClass * GetClassAndSubClass(UInt8 * pcls);
BusProbeClass * GetDeviceClassAndSubClass(UInt8 * pcls);
BusProbeClass * GetInterfaceClassAndSubClass(UInt8 * pcls);
char * GetStringFromNumber(UInt32 value, int sizeInBytes, int style);
char * GetStringFromIndex(UInt8 strIndex, IOUSBDeviceRef deviceIntf);
NSString * VendorNameFromVendorID(NSString * intValueAsString);
NSString * GetUSBProductNameFromRegistry(io_registry_entry_t entry);
void FreeString(char * cstr);
UInt16 Swap16(void *p);
uint32_t	Swap32(void *p);
uint64_t	Swap64(void *p);
uint32_t	Swap24(void *p);
const char *	USBErrorToString(IOReturn status);
