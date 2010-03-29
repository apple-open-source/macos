/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "DecodeDeviceDescriptor.h"


@implementation DecodeDeviceDescriptor

+ (void)decodeBytes:(IOUSBDeviceDescriptor *)dev forDevice:(BusProbeDevice *)thisDevice deviceInterface:(IOUSBDeviceRef)deviceIntf wasSuspended:(BOOL)wasSuspended {
    NSString                *tempString1, *tempString2, *tempString3;
    BusProbeClass *         deviceClass;
    char                    *cstr1, *cstr2, *cstr3;
    
    Swap16(&dev->bcdUSB);
    Swap16(&dev->idVendor);
    Swap16(&dev->idProduct);
    Swap16(&dev->bcdDevice);
    
    [thisDevice addProperty:"Device Descriptor" withValue:"" atDepth:DEVICE_DESCRIPTOR_LEVEL-1];
    
    [thisDevice addNumberProperty:"Descriptor Version Number:" value: dev->bcdUSB size:sizeof(dev->bcdUSB) atDepth:DEVICE_DESCRIPTOR_LEVEL usingStyle:kHexOutputStyle];
    [thisDevice setUSBRelease:dev->bcdUSB];
    
    deviceClass = GetDeviceClassAndSubClass(&dev->bDeviceClass);
    [thisDevice setDeviceClassInfo:deviceClass];
    [thisDevice addProperty:"Device Class:" withValue:(char *)[[deviceClass classDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:DEVICE_DESCRIPTOR_LEVEL];
    [thisDevice addProperty:"Device Subclass:" withValue:(char *)[[deviceClass subclassDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:DEVICE_DESCRIPTOR_LEVEL];
    
    [thisDevice addProperty:"Device Protocol:" withValue:(char *)[[deviceClass protocolDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:DEVICE_DESCRIPTOR_LEVEL];
    [thisDevice addNumberProperty:"Device MaxPacketSize:" value: dev->bMaxPacketSize0 size:sizeof(dev->bMaxPacketSize0) atDepth:DEVICE_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
    
    cstr1 = GetStringFromNumber(dev->idVendor, sizeof(dev->idVendor), kHexOutputStyle);
    cstr2 = GetStringFromNumber(dev->idProduct, sizeof(dev->idProduct), kHexOutputStyle);
    cstr3 = GetStringFromNumber(dev->idVendor, sizeof(dev->idVendor), kIntegerOutputStyle);
    [thisDevice setVendorID: (UInt32)dev->idVendor];
    [thisDevice setProductID: (UInt32)dev->idProduct];
    
    [thisDevice addProperty:"Device VendorID/ProductID:" withValue:(char *)[[NSString stringWithFormat:@"%s/%s   (%@)", cstr1, cstr2, VendorNameFromVendorID([NSString stringWithCString:cstr3 encoding:NSUTF8StringEncoding])]  cStringUsingEncoding:NSUTF8StringEncoding] atDepth:DEVICE_DESCRIPTOR_LEVEL];
    FreeString(cstr1);
    FreeString(cstr2);
    FreeString(cstr3);
    
    [thisDevice addNumberProperty:"Device Version Number:" value: dev->bcdDevice size:sizeof(dev->bcdDevice) atDepth:DEVICE_DESCRIPTOR_LEVEL usingStyle:kHexOutputStyle];
    [thisDevice addNumberProperty:"Number of Configurations:" value: dev->bNumConfigurations size:sizeof(dev->bNumConfigurations) atDepth:DEVICE_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
    [thisDevice addStringProperty:"Manufacturer String:" fromStringIndex: (UInt8)dev->iManufacturer fromDeviceInterface:deviceIntf atDepth:DEVICE_DESCRIPTOR_LEVEL];
    [thisDevice addStringProperty:"Product String:" fromStringIndex: (UInt8)dev->iProduct fromDeviceInterface:deviceIntf atDepth:DEVICE_DESCRIPTOR_LEVEL];
    [thisDevice addStringProperty:"Serial Number String:" fromStringIndex: (UInt8)dev->iSerialNumber fromDeviceInterface:deviceIntf atDepth:DEVICE_DESCRIPTOR_LEVEL];
    
    // Add the string for the kind of device that it is.  We look at the class of the device
    // and then add the product name.  If the product name is blank (iProduct is 0), then we
    // put the vendor name from the database in the string
    //
    // Examples:
    //
    // 	Composite Device: "Apple Extended USB Keyboard"
    // 	Hub device from Atmel Corporation
    //	Vendor-specific device from unknown vendor
    //
    
    
    // If our subclass name is different than our class name, then add the sub class to the description
    // following a "/"
    //
    if ( ! [[deviceClass subclassName] isEqualToString:@""] &&
        ! [[deviceClass subclassName] isEqualToString:[deviceClass className]] ) {
        tempString1 = [NSString stringWithFormat:@"%@/%@", [deviceClass className], [deviceClass subclassName]]; 
    } else {
        tempString1 = [deviceClass className];
    }
    
    cstr1 = GetStringFromIndex((UInt8)dev->iProduct, deviceIntf);
    cstr2 = GetStringFromNumber(dev->idVendor, sizeof(dev->idVendor), kIntegerOutputStyle);
    
	if ( wasSuspended )
		tempString3 = [NSString stringWithFormat:@" (Suspended)"];
	else
		tempString3 = [NSString stringWithFormat:@""];

    if (strcmp(cstr1,"0x00") == 0) {
        tempString2 = [NSString stringWithFormat:@"%@ device from %@%@", tempString1, VendorNameFromVendorID([NSString stringWithCString:cstr2 encoding:NSUTF8StringEncoding]), tempString3];
    } else {
        tempString2 = [NSString stringWithFormat:@"%@ device: %s%@", tempString1, cstr1, tempString3];
    }
    
    FreeString(cstr1);
    FreeString(cstr2);
    
    [thisDevice setDeviceDescription:tempString2];
}

@end
