/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
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


#import <Foundation/Foundation.h>
#import <IOKit/usb/USB.h>
#import "BusProberSharedFunctions.h"
#import "OutlineViewNode.h"
#import "BusProbeClass.h"

@interface BusProbeDevice : NSObject {
    OutlineViewNode *   _rootNode;
    UInt8               _speed;
    USBDeviceAddress    _address;
    UInt32              _locationID;
    UInt32              _vendorID;
    UInt32              _productID;
    UInt16              _usbRelease;
    BusProbeClass *     _deviceClassInfo;
    BusProbeClass *     _lastInterfaceClassInfo;
    UInt8               _lastInterfaceSubclass;
    int                 _currentInterfaceNumber;
	uint32_t			_portInfo;
}

- (OutlineViewNode *)rootNode;
- (void)addProperty:(char *)property withValue:(char *)value atDepth:(int)depth;
- (void)addNumberProperty:(char *)property value:(UInt32)value size:(int)sizeInBytes atDepth:(int)depth usingStyle:(int)style;
- (void)addStringProperty:(char *)property fromStringIndex:(UInt8)strIndex fromDeviceInterface:(IOUSBDeviceRef)deviceIntf atDepth:(int)depth;

- (NSString *)deviceName;
- (void)setDeviceName:(NSString *)name;
- (NSString *)deviceDescription;
- (void)setDeviceDescription:(NSString *)description;
- (UInt8)speed;
- (void)setSpeed:(UInt8)speed;
- (USBDeviceAddress)address;
- (void)setAddress:(USBDeviceAddress)address;
- (uint32_t)portInfo;
- (void)setPortInfo:(uint32_t)portInfo;
- (UInt32)locationID;
- (void)setLocationID:(UInt32)locationID;
- (UInt32)vendorID;
- (void)setVendorID:(UInt32)vendorID;
- (UInt32)productID;
- (void)setProductID:(UInt32)productID;
- (UInt32)locationID;
- (void)setLocationID:(UInt32)locationID;
- (BusProbeClass *)deviceClassInfo;
- (void)setDeviceClassInfo:(BusProbeClass *)classInfo;
- (BusProbeClass *)lastInterfaceClassInfo;
- (void)setLastInterfaceClassInfo:(BusProbeClass *)classInfo;
- (int)currentInterfaceNumber;
- (void)setCurrentInterfaceNumber:(int)currentNum;
- (UInt16)usbRelease;
- (void)setUSBRelease:(UInt16)usbRelease;

- (NSString *)descriptionForName:(NSString*)name;
- (NSString *)description;
- (NSMutableDictionary *)dictionaryVersionOfMe;

@end
