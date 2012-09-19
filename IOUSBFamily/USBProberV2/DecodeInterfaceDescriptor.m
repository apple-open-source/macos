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


#import "DecodeInterfaceDescriptor.h"


@implementation DecodeInterfaceDescriptor

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice  withDeviceInterface:(IOUSBDeviceRef)deviceIntf{
    /*	struct IOUSBInterfaceDescriptor {
    UInt8 			bLength;
    UInt8 			bDescriptorType;
    UInt8 			bInterfaceNumber;
    UInt8 			bAlternateSetting;
    UInt8 			bNumEndpoints;
    UInt8 			bInterfaceClass;
    UInt8 			bInterfaceSubClass;
    UInt8 			bInterfaceProtocol;
    UInt8 			iInterface;
    }; */
    
    IOUSBInterfaceDescriptor 	interfaceDescriptor;
	NSString					*interfaceName;
    BusProbeClass *             interfaceClass;
    OutlineViewNode *           headingNode;
    NSString                    *tempString1, *tempString2;
	char                        *cstr1;
   
    interfaceDescriptor = *(IOUSBInterfaceDescriptor *)p;
 	cstr1 = GetStringFromIndex((UInt8)interfaceDescriptor.iInterface, deviceIntf);
		// Add property without a name, we'll sort that out later.
	if (strcmp(cstr1, "0x00") != 0) 
		[thisDevice addProperty:"" withValue:cstr1 atDepth:INTERFACE_LEVEL-1];
	else
		[thisDevice addProperty:"" withValue:"" atDepth:INTERFACE_LEVEL-1];
		
    headingNode = [[thisDevice rootNode] deepestChild];
    
    [thisDevice addNumberProperty:"Alternate Setting" value: interfaceDescriptor.bAlternateSetting size:sizeof(interfaceDescriptor.bAlternateSetting) atDepth:INTERFACE_LEVEL usingStyle:kIntegerOutputStyle];
    [thisDevice addNumberProperty:"Number of Endpoints" value: interfaceDescriptor.bNumEndpoints size:sizeof(interfaceDescriptor.bNumEndpoints) atDepth:INTERFACE_LEVEL usingStyle:kIntegerOutputStyle];
    
    interfaceClass = GetInterfaceClassAndSubClass(&interfaceDescriptor.bInterfaceClass);

    [thisDevice addProperty:"Interface Class:" withValue:(char *)[[interfaceClass classDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:INTERFACE_LEVEL];
    [thisDevice addProperty:"Interface Subclass;" withValue:(char *)[[interfaceClass subclassDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:INTERFACE_LEVEL];
    [thisDevice addProperty:"Interface Protocol:" withValue:(char *)[[interfaceClass protocolDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:INTERFACE_LEVEL];
    
	// Sort out interface name
    tempString1 = [interfaceClass className];
    
    // If our subclass name is different than our class name, then add the sub class to the description
    // following a "/"
    //
    if ( ! [[interfaceClass subclassName] isEqualToString:@""] &&
        ! [[interfaceClass subclassName] isEqualToString:[interfaceClass className]] )
    {
        tempString2 = [NSString stringWithFormat:@"%@/%@",tempString1,[interfaceClass subclassName]];
        tempString1 = tempString2;
    }
    
    if ( interfaceDescriptor.bAlternateSetting != 0 )
        interfaceName = [NSString stringWithFormat:@"Interface #%d - %@ (#%d)", (int)interfaceDescriptor.bInterfaceNumber, tempString1, (int)interfaceDescriptor.bAlternateSetting];
    else
        interfaceName = [NSString stringWithFormat:@"Interface #%d - %@", (int)interfaceDescriptor.bInterfaceNumber, tempString1];
    
	// If the interface has a name add the heading
    if (strcmp(cstr1, "0x00") != 0) {
        interfaceName = [NSString stringWithFormat:@"%@ ..............................................", interfaceName];
	}
    FreeString(cstr1);
	[headingNode setName:interfaceName];

   // [thisDevice addNumberProperty:"Interface Protocol" value: interfaceDescriptor.bInterfaceProtocol size:sizeof(interfaceDescriptor.bInterfaceProtocol) atDepth:INTERFACE_LEVEL usingStyle:kIntegerOutputStyle];
}

@end

@implementation DecodeInterfaceAssociationDescriptor

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice withDeviceInterface:(IOUSBDeviceRef)deviceIntf{
    /*	struct IOUSBInterfaceAssociationDescriptor
{
    UInt8 			bLength;
    UInt8 			bDescriptorType;
    UInt8 			bFirstInterface;
    UInt8 			bInterfaceCount;
    UInt8 			bFunctionClass;
    UInt8 			bFunctionSubClass;
    UInt8 			bFunctionProtocol;
    UInt8			iFunction;
};
 */
    
    IOUSBInterfaceAssociationDescriptor 	interfaceAssocDescriptor;
    BusProbeClass *             interfaceAssocClass;
    NSString                    *tempString1, *tempString2;
    
    interfaceAssocDescriptor = *(IOUSBInterfaceAssociationDescriptor *)p;
        
    interfaceAssocClass = GetInterfaceClassAndSubClass(&interfaceAssocDescriptor.bFunctionClass);
    
    tempString1 = [interfaceAssocClass className];
    
    // If our subclass name is different than our class name, then add the sub class to the description
    // following a "/"
    //
    if ( ! [[interfaceAssocClass subclassName] isEqualToString:@""] &&
        ! [[interfaceAssocClass subclassName] isEqualToString:[interfaceAssocClass className]] )
    {
        tempString2 = [NSString stringWithFormat:@"%@/%@",tempString1,[interfaceAssocClass subclassName]];
        tempString1 = tempString2;
    }
    
    [thisDevice addProperty:"Interface Association" withValue:(char *)([tempString1 cStringUsingEncoding:NSUTF8StringEncoding]) atDepth:INTERFACE_LEVEL-1];
    
    [thisDevice addNumberProperty:"First Interface" value: interfaceAssocDescriptor.bFirstInterface size:sizeof(interfaceAssocDescriptor.bFirstInterface) atDepth:INTERFACE_LEVEL usingStyle:kIntegerOutputStyle];
    [thisDevice addNumberProperty:"Interface Count" value: interfaceAssocDescriptor.bInterfaceCount size:sizeof(interfaceAssocDescriptor.bInterfaceCount) atDepth:INTERFACE_LEVEL usingStyle:kIntegerOutputStyle];
    
    [thisDevice addProperty:"Function Class" withValue:(char *)[[interfaceAssocClass classDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:INTERFACE_LEVEL];
    [thisDevice addProperty:"Function Subclass" withValue:(char *)[[interfaceAssocClass subclassDescription] cStringUsingEncoding:NSUTF8StringEncoding] atDepth:INTERFACE_LEVEL];
    
    [thisDevice addNumberProperty:"Interface Protocol" value: interfaceAssocDescriptor.bFunctionProtocol size:sizeof(interfaceAssocDescriptor.bFunctionProtocol) atDepth:INTERFACE_LEVEL usingStyle:kIntegerOutputStyle];

    [thisDevice addStringProperty:"Function String" fromStringIndex: (UInt8)interfaceAssocDescriptor.iFunction fromDeviceInterface:deviceIntf atDepth:INTERFACE_LEVEL];
}

@end
