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


#import "BusProbeDevice.h"


@implementation BusProbeDevice

- init {
    if (self = [super init]) {
        _rootNode = [[OutlineViewNode alloc] initWithName:@"A USB Device" value:@"<Description not set>"];
        _speed = -1;
        _address = -1;
        _locationID = -1;
        _vendorID = -1;
        _productID = -1;
        _deviceClassInfo = [[BusProbeClass alloc] init];
        _lastInterfaceClassInfo = [[BusProbeClass alloc] init];
        _currentInterfaceNumber = -1;
		_portInfo = -1;
    }
    return self;
}

- (void)dealloc {
    [_rootNode release];
    [_deviceClassInfo release];
    [_lastInterfaceClassInfo release];
    [super dealloc];
}

- (OutlineViewNode *)rootNode {
    return _rootNode;
}

- (void)addProperty:(char *)property withValue:(char *)value atDepth:(int)depth {
    [_rootNode addNodeWithName:property value:value atDepth:depth];
}

- (void)addNumberProperty:(char *)property value:(UInt32)value size:(int)sizeInBytes atDepth:(int)depth usingStyle:(int)style {
    char *valstr = GetStringFromNumber(value, sizeInBytes, style);    
    [self addProperty:property withValue:valstr atDepth:depth];
    
    FreeString(valstr);
}

- (void)addStringProperty:(char *)property fromStringIndex:(UInt8)strIndex fromDeviceInterface:(IOUSBDeviceRef)deviceIntf atDepth:(int)depth {
    Byte buf[256];
    char *str2 =  malloc(500 * sizeof(char));
    memset(str2,'\0', 500 * sizeof(char));
    
    if (strIndex > 0)
    {
        int len;
        buf[0] = 0;
        len = GetStringDescriptor(deviceIntf, strIndex, buf, sizeof(buf), 0);
        
        if (len > 2)
        {
            Byte *p;
            CFStringRef str;
            for (p = buf + 2; p < buf + len; p += 2)
            {
                Swap16(p);
            }
            
            str = CFStringCreateWithCharacters(NULL, (const UniChar *)(buf+2), (len-2)/2);
            CFStringGetCString(str, (char *)buf, 256, kCFStringEncodingNonLossyASCII);
            CFRelease(str);
            sprintf(str2, "%d \"%s\"", strIndex, buf);
        }
        else
        {
            sprintf(str2,"%d (none)",strIndex);
        }
    }
    else
    {
        sprintf(str2,"%d (none)",strIndex);
    }
    
    [self addProperty:property withValue:str2 atDepth:depth];
    FreeString(str2);
}

- (NSString *)deviceName {
    return [_rootNode name];
}

- (void)setDeviceName:(NSString *)name {
    [_rootNode setName:name];
}

- (NSString *)deviceDescription {
    return [_rootNode value];
}

- (void)setDeviceDescription:(NSString *)description {
    [_rootNode setValue:description];
}

- (UInt8)speed {
    return _speed;
}

- (void)setSpeed:(UInt8)speed {
    _speed = speed;
}


- (USBDeviceAddress)address {
    return _address;
}

- (void)setAddress:(USBDeviceAddress)address {
    _address = address;
}

- (uint32_t)portInfo {
    return _portInfo;
}

- (void)setPortInfo:(uint32_t)portInfo {
    _portInfo = portInfo;
}

- (UInt32)locationID {
    return _locationID;
}

- (void)setLocationID:(UInt32)locationID {
    _locationID = locationID;
}

- (UInt32)vendorID {
    return _vendorID;
}

- (void)setVendorID:(UInt32)vendorID {
    _vendorID = vendorID;
}

- (UInt32)productID {
    return _productID;
}

- (void)setProductID:(UInt32)productID {
    _productID = productID;
}

- (BusProbeClass *)deviceClassInfo {
    return _deviceClassInfo;
}

- (void)setDeviceClassInfo:(BusProbeClass *)classInfo {
    [_deviceClassInfo release];
    _deviceClassInfo = [classInfo retain];
}

- (BusProbeClass *)lastInterfaceClassInfo {
    return _lastInterfaceClassInfo;
}

- (void)setLastInterfaceClassInfo:(BusProbeClass *)classInfo {
    [_lastInterfaceClassInfo release];
    _lastInterfaceClassInfo = [classInfo retain];
}

- (int)currentInterfaceNumber {
    return _currentInterfaceNumber;
}

- (void)setCurrentInterfaceNumber:(int)currentNum {
    _currentInterfaceNumber = currentNum;
}

- (UInt16)usbRelease {
    return _usbRelease;
}

- (void)setUSBRelease:(UInt16)usbRelease {
    _usbRelease = usbRelease;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@   %@\n%@",[self deviceName],[self deviceDescription],[[self rootNode] stringRepresentation:0]];
}

- (NSString *)descriptionForName:(NSString*)name {
    return [NSString stringWithFormat:@"%@\n",[[self rootNode] stringRepresentation:name:0]];
}

@end
