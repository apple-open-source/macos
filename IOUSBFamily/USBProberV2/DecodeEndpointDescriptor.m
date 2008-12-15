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


#import "DecodeEndpointDescriptor.h"


@implementation DecodeEndpointDescriptor

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice isOtherSpeedDesc:(BOOL)isOtherSpeedDesc {
    /*	struct IOUSBEndpointDescriptor {
    UInt8 			bLength;
    UInt8 			bDescriptorType;
    UInt8 			bEndpointAddress;
    UInt8 			bmAttributes;
    UInt16 			wMaxPacketSize;
    UInt8 			bInterval;
    }; */
    static int 	xferTypes2[] 	= { 0, 1, 2, 3 };
    IOUSBEndpointDescriptor     endpointDescriptor;
    char                        endpointHeading[500];
    char                        str[500];
    char                        temporaryString[500];
    bool                        controlOrBulk = false;
    bool                        isoch = false;
    bool                        interrupt = false;
    bool                        useHighSpeedDefinition = false;
    
    // The bInterval field of an endpoint descriptor changes depending if it is a high speed endpoint or not.  When parsing
    // the "Other Speed Configuration Descriptor", we need to know the speed of the device and whether this is an "other" speed
    // descriptor.  Hence the following code:
    //
    if ( ([thisDevice usbRelease] >= 0x200) )
    {
        if ( (([thisDevice speed] == kUSBDeviceSpeedHigh ) && !isOtherSpeedDesc) ||
             (([thisDevice speed] != kUSBDeviceSpeedHigh ) && isOtherSpeedDesc) )
            useHighSpeedDefinition = true;
    }

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
        [thisDevice addProperty:"Address:" withValue:temporaryString atDepth:ENDPOINT_LEVEL];
    }
    
    sprintf(str, "0x%02X", endpointDescriptor.bmAttributes);
    switch ( endpointDescriptor.bmAttributes & 0x03)
    {
        case 0: strcat(str, "  (Control ");      controlOrBulk = true;  break;
        case 1: strcat(str, "  (Isochronous ");  isoch = true;          break;
        case 2: strcat(str, "  (Bulk ");         controlOrBulk = true;  break;
        case 3: strcat(str, "  (Interrupt ");    interrupt = true;      break;
    }
    
    switch ( (endpointDescriptor.bmAttributes & 0x0C) >> 2)
    {
        case 0: strcat(str, "no synchronization ");     break;
        case 1: strcat(str, "asynchronous ");           break;
        case 2: strcat(str, "adaptive ");               break;
        case 3: strcat(str, "synchronous ");            break;
    }
    
    switch ( (endpointDescriptor.bmAttributes & 0x30) >> 4)
    {
        case 0: strcat(str, "data endpoint)");      break;
        case 1: strcat(str, "feedback endpoint)");  break;
        case 2: strcat(str, "implicit feedback data endpoint)");         break;
        case 3: strcat(str, "(error:  bit 5 is Reserved))");    break;
    }
    
    [thisDevice addProperty:"Attributes:" withValue:str atDepth:ENDPOINT_LEVEL];

	sprintf(temporaryString, "%d", endpointDescriptor.wMaxPacketSize);

	if (useHighSpeedDefinition)
    {
        // If we have a USB 2.0 compliant device running at high speed, then the wMaxPacketSize calculation is funky
        //
        if ( interrupt || isoch)
		{
			UInt32	transPerMicroFrame = (endpointDescriptor.wMaxPacketSize & 0x1800) >> 11;
			// If bits 15..13 are not 0, it's illegal.  If bits 12..11 are 11b, it's illegal
            if ( ( (endpointDescriptor.wMaxPacketSize & 0xe000) != 0) || ( transPerMicroFrame == 3) )
                sprintf(temporaryString, "0x%x: Illegal value for wMaxPacketSize for a hi-speed Interrupt endpoint", endpointDescriptor.bInterval);
            else
                sprintf(temporaryString, "%d  (%d x %d  transactions opportunities per microframe)", endpointDescriptor.wMaxPacketSize, endpointDescriptor.wMaxPacketSize & 0x07ff,  (uint32_t)transPerMicroFrame + 1);
        }
    }
	
    [thisDevice addProperty:"Max Packet Size:" withValue:temporaryString atDepth:ENDPOINT_LEVEL];
    
    if (useHighSpeedDefinition)
    {
        // If we have a USB 2.0 compliant device running at high speed, then the bInterval calculation is funky
        //
        if ( controlOrBulk)
        {
            if ( endpointDescriptor.bInterval == 0)
                sprintf(temporaryString, "%d ( Endpoint never NAKs)", endpointDescriptor.bInterval);
            else
                sprintf(temporaryString, "%d ( At most 1 NAK every %d microframe(s) )", endpointDescriptor.bInterval, endpointDescriptor.bInterval);
        }
        
        if ( interrupt )
            if ( (endpointDescriptor.bInterval == 0) || (endpointDescriptor.bInterval > 16) )
                sprintf(temporaryString, "%d: Illegal value for bInterval for a hi-speed Interrupt endpoint", endpointDescriptor.bInterval);
            else
                sprintf(temporaryString, "%d (%d %s (%d %s) )", endpointDescriptor.bInterval, (1 << (endpointDescriptor.bInterval-1)), endpointDescriptor.bInterval==1?"microframe":"microframes", 
						(endpointDescriptor.bInterval > 3 ? (1 << (endpointDescriptor.bInterval-1))/8 : endpointDescriptor.bInterval * 125), endpointDescriptor.bInterval > 3?"msecs":"microsecs" );
        
        if ( isoch )
            if ( (endpointDescriptor.bInterval == 0) || (endpointDescriptor.bInterval > 16) )
                sprintf(temporaryString, "Illegal value for bInterval for a hi-speed isoch endpoint: %d", endpointDescriptor.bInterval);
            else
                sprintf(temporaryString, "%d (%d %s (%d %s) )", endpointDescriptor.bInterval, (1 << (endpointDescriptor.bInterval-1)), endpointDescriptor.bInterval==1?"microframe":"microframes", 
						endpointDescriptor.bInterval > 3 ? (1 << (endpointDescriptor.bInterval-1))/8 : endpointDescriptor.bInterval * 125, endpointDescriptor.bInterval > 3?"msecs":"microsecs" );
    }
    else
    {
        if ( isoch )
            if ( (endpointDescriptor.bInterval == 0) || (endpointDescriptor.bInterval > 16) )
                sprintf(temporaryString, "Illegal value for bInterval for a full speed isoch endpoint: %d", endpointDescriptor.bInterval);
            else
                sprintf(temporaryString, "%d ms", (1 << (endpointDescriptor.bInterval-1)));
        else
            sprintf(temporaryString, "%d ms", endpointDescriptor.bInterval);
    }

        [thisDevice addProperty:"Polling Interval:" withValue:temporaryString atDepth:ENDPOINT_LEVEL];
}

@end
