/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#import <Cocoa/Cocoa.h>
#import "Node.h"
#import <Foundation/Foundation.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/usb/USB.h>
#import <IOKit/usb/USBSpec.h>
#import <IOKit/IOCFPlugIn.h>
#import "USBClass.h"
#import "DecodeHIDReport.h"
#import "NodeOutput.h"
#import "DecodeAudioInterfaceDescriptor.h"

#define RETURNNUM(obj, fld, integer) \
[self returnNum:(obj.fld) size:(sizeof(obj.fld)) asInt:integer];

#define NUM(obj, heading, fld, deviceNum, depth, integer) \
[self printNum:heading value:(obj.fld) size:(sizeof(obj.fld)) forDevice:deviceNum atDepth:depth asInt:integer];

#define STR(obj, heading, fld, deviceNum, depth) \
[self PrintStr:deviceIntf name:heading strIndex:(UInt8)obj.fld forDevice:deviceNum atDepth:depth];

#define RETURNSTR(obj, fld) \
[self ReturnStr:deviceIntf strIndex:(UInt8)obj.fld];

#define HID_DESCRIPTOR					0x21
#define DFU_FUNCTIONAL_DESCRIPTOR		0x21

#define ROOT_LEVEL						0
#define DEVICE_DESCRIPTOR_LEVEL			ROOT_LEVEL + 1
#define CONFIGURATION_DESCRIPTOR_LEVEL	ROOT_LEVEL + 1
#define INTERFACE_LEVEL					CONFIGURATION_DESCRIPTOR_LEVEL + 1
#define ENDPOINT_LEVEL					INTERFACE_LEVEL + 1
#define HID_DESCRIPTOR_LEVEL			INTERFACE_LEVEL + 1
#define DFU_DESCRIPTOR_LEVEL			INTERFACE_LEVEL + 1
#define HUB_DESCRIPTOR_LEVEL			ROOT_LEVEL + 1
#define DEVICE_QUAL_DESCRIPTOR_LEVEL			ROOT_LEVEL + 1

enum ClassSpecific {
    CS_INTERFACE		= 0x24,
    CS_ENDPOINT			= 0x25
};
struct IOUSBHubDescriptor {
    UInt8 	length;
    UInt8 	hubType;
    UInt8 	numPorts;
    UInt16 	characteristics __attribute__((packed));
    UInt8 	powerOnToGood;	/* Port settling time, in 2ms */
    UInt8 	hubCurrent;
    
	/* These are received packed, will have to be unpacked */
	UInt8 	removablePortFlags[8];
	UInt8 	pwrCtlPortFlags[8];
};

typedef struct IOUSBHubDescriptor IOUSBHubDescriptor;

enum {
    kEndpointAddressBit = 7,
    kEndpointAddressMask = ( 1 << kEndpointAddressBit )
};

@interface BusProbeClass : NodeOutput {
    IBOutlet id ov;
}

+(Node *)busprobeRootNode;
+(void)USBProbe;
+(void)outputDevice:(IOUSBDeviceInterface **)deviceIntf locationID:(UInt32)locationID deviceNumber:(int)deviceNumber;
    int GetClassDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descType, UInt8 descIndex, void *buf, UInt16 len);
    int GetDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descType, UInt8 descIndex, void *buf, UInt16 len);
    int GetDeviceSpeed(IOUSBDeviceInterface **deviceIntf, UInt8 *speed);
    int GetDeviceAddress(IOUSBDeviceInterface **deviceIntf, USBDeviceAddress *address);
int GetDescriptorFromInterface(IOUSBDeviceInterface **deviceIntf, UInt8 descType, UInt8 descIndex, UInt16 wIndex, void *buf, UInt16 len);
int GetStringDescriptor(IOUSBDeviceInterface **deviceIntf, UInt8 descIndex, void *buf, UInt16 len, UInt16 lang);
+(USBClass *)ClassAndSubClass:(const char *)scope pcls:(UInt8 *)pcls forDevice:(int)deviceNumber atDepth:(int)depth;
+(void)PrintDescLenAndType:(void *)desc forDevice:(int)deviceNumber atDepth:(int)depth;
+(void)printNum:(char *)name value:(UInt32)value size:(int)sizeInBytes forDevice:(int)deviceNumber atDepth:(int)depth asInt:(int)asInt;
+(NSString *)returnNum:(UInt32)value size:(int)sizeInBytes asInt:(int)asInt;
+(void)PrintNumStr:(char *)name value:(UInt32)value size:(int)sizeInBytes interpret:(char *)interpret forDevice:(int)deviceNumber atDepth:(int)depth asInt:(int)asInt;
UInt16 Swap16(void *p);
+(void)PrintStr:(IOUSBDeviceInterface **)deviceIntf name:(char *)name strIndex:(UInt8)strIndex forDevice:(int)deviceNumber atDepth:(int)depth;
+(NSString *)ReturnStr:(IOUSBDeviceInterface **)deviceIntf strIndex:(UInt8)strIndex;
+(void)dump:(int)n byte:(Byte *)p forDevice:(int)deviceNumber atDepth:(int)depth;
+(void)DumpRawDescriptor:(Byte *)p forDevice:(int)deviceNumber atDepth:(int)depth;
//+(void)DumpDescriptor:(IOUSBDeviceInterface **)deviceIntf p:(Byte *)p forDevice:(int)deviceNumber;
+(void)DumpDescriptor:(IOUSBDeviceInterface **)deviceIntf p:(Byte *)p forDevice:(int)deviceNumber lastInterfaceClass:(UInt8)lastInterfaceClass lastInterfaceSubClass:(UInt8)lastInterfaceSubClass currentInterfaceNum:(int)currentInterfaceNum ;
+(void)DoRegularCSInterface:(Byte *)p deviceClass:(USBClass *)interfaceClass forDevice:(int)deviceNumber atDepth:(int)depth;
+(void)DoRegularCSEndpoint:(Byte *)p deviceClass:(USBClass *)interfaceClass forDevice:(int)deviceNumber atDepth:(int)depth;

- (void)loadVendorNamesFromFile;
+ (NSString *)vendorNameFromVendorID:(NSString *)intValueAsString;

-(id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
-(BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
-(int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
-(id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;

@end
