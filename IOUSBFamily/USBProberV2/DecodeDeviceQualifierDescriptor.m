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


#import "DecodeDeviceQualifierDescriptor.h"


@implementation DecodeDeviceQualifierDescriptor

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice {
    IOUSBDeviceQualifierDescriptor 	devQualDescriptor;
    BusProbeClass *                     deviceClass;
    
    devQualDescriptor = *(IOUSBDeviceQualifierDescriptor *)p;
    
    Swap16(&devQualDescriptor.bcdUSB);
    
    [thisDevice addProperty:"Device Qualifier Descriptor" withValue:"" atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL-1];
    [thisDevice addNumberProperty:"Descriptor Version Number:" value: devQualDescriptor.bcdUSB size:sizeof(devQualDescriptor.bcdUSB) atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL usingStyle:kHexOutputStyle];
    
    deviceClass = GetDeviceClassAndSubClass(&devQualDescriptor.bDeviceClass);
    [thisDevice addProperty:"Device Class" withValue:(char *)[[deviceClass classDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL];
    [thisDevice addProperty:"Device Subclass" withValue:(char *)[[deviceClass subclassDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL];
    [thisDevice addProperty:"Device Protocol" withValue:(char *)[[deviceClass protocolDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL];
    
 //   [thisDevice addNumberProperty:"Device Protocol" value: devQualDescriptor.bDeviceProtocol size:sizeof(devQualDescriptor.bDeviceProtocol) atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
    [thisDevice addNumberProperty:"Device MaxPacketSize:" value: devQualDescriptor.bMaxPacketSize0 size:sizeof(devQualDescriptor.bMaxPacketSize0) atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
    [thisDevice addNumberProperty:"Number of Configurations:" value: devQualDescriptor.bNumConfigurations size:sizeof(devQualDescriptor.bNumConfigurations) atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
    [thisDevice addNumberProperty:"bReserved:" value: devQualDescriptor.bReserved size:sizeof(devQualDescriptor.bReserved) atDepth:DEVICE_QUAL_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
}

@end
