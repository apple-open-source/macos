/*
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


#import "DecodeEndpointDescriptor.h"


@implementation DecodeEndpointDescriptor

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice {
    /*	struct IOUSBEndpointDescriptor {
    UInt8 			bLength;
    UInt8 			bDescriptorType;
    UInt8 			bEndpointAddress;
    UInt8 			bmAttributes;
    UInt16 			wMaxPacketSize;
    UInt8 			bInterval;
    }; */
    static char *               xferTypes[] = { "Control", "Isochronous", "Bulk", "Interrupt" };
    static int 	xferTypes2[] 	= { 0, 1, 2, 3 };
    IOUSBEndpointDescriptor     endpointDescriptor;
    char                        endpointHeading[500];
    char                        str[500];
    char                        temporaryString[500];
    
    endpointDescriptor = *(IOUSBEndpointDescriptor *)p;
    
    Swap16(&endpointDescriptor.wMaxPacketSize);
    switch (xferTypes2[endpointDescriptor.bmAttributes & 3])
    {
        case 0:
            sprintf(endpointHeading, "Endpoint 0x%02X - Control Endpoint", endpointDescriptor.bEndpointAddress);
            break;
        case 1:
            if ( (endpointDescriptor.bEndpointAddress & kEndpointAddressMask ) == 0 )
                sprintf(endpointHeading, "Endpoint 0x%02X - Isochronous Output", endpointDescriptor.bEndpointAddress);
            else
                sprintf(endpointHeading, "Endpoint 0x%02X - Isochronous Input", endpointDescriptor.bEndpointAddress);
            break;
        case 2:
            if ( (endpointDescriptor.bEndpointAddress & kEndpointAddressMask ) == 0 )
                sprintf(endpointHeading, "Endpoint 0x%02X - Bulk Output", endpointDescriptor.bEndpointAddress);
            else
                sprintf(endpointHeading, "Endpoint 0x%02X - Bulk Input", endpointDescriptor.bEndpointAddress);
            break;
        case 3:
            if ( (endpointDescriptor.bEndpointAddress & kEndpointAddressMask ) == 0 )
                sprintf(endpointHeading, "Endpoint 0x%02X - Interrupt Output", endpointDescriptor.bEndpointAddress);
            else
                sprintf(endpointHeading, "Endpoint 0x%02X - Interrupt Input", endpointDescriptor.bEndpointAddress);
            break;
        default:
            sprintf(endpointHeading, "Endpoint 0x%02X", endpointDescriptor.bEndpointAddress);
            break;
    }
    
    [thisDevice addProperty:endpointHeading withValue:"" atDepth:ENDPOINT_LEVEL-1];
    
    if (!(xferTypes2[endpointDescriptor.bmAttributes & 3] == 0))
    {
        // we dont need to show direction for Control Endpoints
        //
        if ( (endpointDescriptor.bEndpointAddress & kEndpointAddressMask ) == 0 )
            sprintf(temporaryString, "0x%02X  (OUT)", endpointDescriptor.bEndpointAddress);
        else
            sprintf(temporaryString, "0x%02X  (IN)", endpointDescriptor.bEndpointAddress);
        [thisDevice addProperty:"Attributes:" withValue:temporaryString atDepth:ENDPOINT_LEVEL];
    }
    
    sprintf(str, "0x%02X  (%s)", endpointDescriptor.bmAttributes, xferTypes[endpointDescriptor.bmAttributes & 3]);
    [thisDevice addProperty:"Attributes:" withValue:str atDepth:ENDPOINT_LEVEL];
    
    sprintf(temporaryString, "%d", endpointDescriptor.wMaxPacketSize);
    [thisDevice addProperty:"Max Packet Size:" withValue:temporaryString atDepth:ENDPOINT_LEVEL];
    
    sprintf(temporaryString, "%d ms", endpointDescriptor.bInterval);
    [thisDevice addProperty:"Polling Interval:" withValue:temporaryString atDepth:ENDPOINT_LEVEL];
}

@end
