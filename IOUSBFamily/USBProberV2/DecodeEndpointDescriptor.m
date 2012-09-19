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
	bool						printPollingInterval = true;
    
    // The bInterval field of an endpoint descriptor changes depending if it is a high speed endpoint or not.  When parsing
    // the "Other Speed Configuration Descriptor", we need to know the speed of the device and whether this is an "other" speed
    // descriptor.  Hence the following code:
    //
    if ( ([thisDevice usbRelease] >= kUSBRel20) )
    {
        if ( (([thisDevice speed] == kUSBDeviceSpeedHigh ) && !isOtherSpeedDesc) ||
             (([thisDevice speed] != kUSBDeviceSpeedHigh ) && isOtherSpeedDesc) )
            useHighSpeedDefinition = true;
    }

	bool						useUSB3Definition = false;
	if ( ([thisDevice usbRelease] >= kUSBRel30) )
	{
		useUSB3Definition = true;
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
        case 0: strcat(str, "  (Control");      controlOrBulk = true;  break;
        case 1: strcat(str, "  (Isochronous");  isoch = true;          break;
        case 2: strcat(str, "  (Bulk");         controlOrBulk = true;  break;
        case 3: strcat(str, "  (Interrupt");    interrupt = true;      break;
    }
    
	if ( isoch )
	{
    switch ( (endpointDescriptor.bmAttributes & 0x0C) >> 2)
    {
			case 0: strcat(str, " no synchronization");     break;
			case 1: strcat(str, " asynchronous");           break;
			case 2: strcat(str, " adaptive");               break;
			case 3: strcat(str, " synchronous");            break;
    }
    
    switch ( (endpointDescriptor.bmAttributes & 0x30) >> 4)
    {
			case 0: strcat(str, " data endpoint");      break;
			case 1: strcat(str, " feedback endpoint");  break;
			case 2: strcat(str, " implicit feedback data endpoint");         break;
			case 3: strcat(str, " (error:  bit 5 is Reserved)");    break;
		}
    }
	else if ( useUSB3Definition && interrupt )
	{
		switch ( (endpointDescriptor.bmAttributes & 0x30) >> 4)
		{
			case 0: strcat(str, " periodic");     break;
			case 1: strcat(str, " notification");           break;
			case 2: strcat(str, " reserved");               break;
			case 3: strcat(str, " reserved");            break;
		}
    }
    
	strcat(str, ")");
	
    [thisDevice addProperty:"Attributes:" withValue:str atDepth:ENDPOINT_LEVEL];

	sprintf(temporaryString, "%d", USBToHostWord(endpointDescriptor.wMaxPacketSize));

	if (useUSB3Definition)
	{
		UInt32	maxPacketSize = USBToHostWord(endpointDescriptor.wMaxPacketSize);
		
		if ( (endpointDescriptor.bmAttributes & 0x03) == 0 )
			if (maxPacketSize != 512 )
				sprintf(temporaryString, " 0x%x: Illegal value for wMaxPacketSize for a SuperSpeed Control endpoint", (uint32_t)maxPacketSize);
		
		if ( (endpointDescriptor.bmAttributes & 0x03) == 1 )
			if (maxPacketSize > 1024 )
				sprintf(temporaryString, " 0x%x: Illegal value for wMaxPacketSize for a SuperSpeed Isochronous endpoint", (uint32_t)maxPacketSize);

		if ( (endpointDescriptor.bmAttributes & 0x03) == 2 )
			if (maxPacketSize != 1024 )
				sprintf(temporaryString, " 0x%x: Illegal value for wMaxPacketSize for a SuperSpeed Bulk endpoint", (uint32_t)maxPacketSize);
		
		if ( (endpointDescriptor.bmAttributes & 0x03) == 3 )
			if (maxPacketSize > 1024 )
				sprintf(temporaryString, " 0x%x: Illegal value for wMaxPacketSize for a SuperSpeed Interrupt endpoint", (uint32_t)maxPacketSize);
	}
	else if (useHighSpeedDefinition)
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
    
	if (useHighSpeedDefinition || useUSB3Definition)
    {
        // If we have a USB compliant device running at high speed, then the bInterval calculation is funky
        //
        if ( controlOrBulk)
        {
			if ( useHighSpeedDefinition)
			{
            if ( endpointDescriptor.bInterval == 0)
                sprintf(temporaryString, "%d ( Endpoint never NAKs)", endpointDescriptor.bInterval);
            else
                sprintf(temporaryString, "%d ( At most 1 NAK every %d microframe(s) )", endpointDescriptor.bInterval, endpointDescriptor.bInterval);
        }
			else
			{
				printPollingInterval = false;
			}
        }
        
        if ( interrupt )
            if ( (endpointDescriptor.bInterval == 0) || (endpointDescriptor.bInterval > 16) )
                sprintf(temporaryString, "%d: Illegal value for bInterval for a hi-speed/SuperSpeed Interrupt endpoint", endpointDescriptor.bInterval);
            else
                sprintf(temporaryString, "%d (%d %s (%d %s) )", endpointDescriptor.bInterval, (1 << (endpointDescriptor.bInterval-1)), endpointDescriptor.bInterval==1?"microframe":"microframes", 
						(endpointDescriptor.bInterval > 3 ? (1 << (endpointDescriptor.bInterval-1))/8 : endpointDescriptor.bInterval * 125), endpointDescriptor.bInterval > 3?"msecs":"microsecs" );
        
        if ( isoch )
            if ( (endpointDescriptor.bInterval == 0) || (endpointDescriptor.bInterval > 16) )
                sprintf(temporaryString, "Illegal value for bInterval for a hi-speed/SuperSpeed isoch endpoint: %d", endpointDescriptor.bInterval);
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
	if ( printPollingInterval )
        [thisDevice addProperty:"Polling Interval:" withValue:temporaryString atDepth:ENDPOINT_LEVEL];
}

+ (void)decodeBytesCompanion:(Byte *)p forDevice:(BusProbeDevice *)thisDevice endpoint:(UInt8) epType {

    IOUSBSuperSpeedEndpointCompanionDescriptor *    epCompanionDescriptor = (IOUSBSuperSpeedEndpointCompanionDescriptor*)p;
    char                        buf[128];

    // Only defined for USB 3.0
    if ( ([thisDevice usbRelease] != kUSBRel30) )
    {
		return;
	}
	
    [thisDevice addProperty:"SuperSpeed Endpoint Companion" withValue:"" atDepth:ENDPOINT_LEVEL-1];

    if (epCompanionDescriptor->bMaxBurst > 15)
	{
		sprintf(buf, "Illegal value of %u", epCompanionDescriptor->bMaxBurst);
		[thisDevice addProperty:"bMaxBurst:" withValue:buf atDepth:ENDPOINT_LEVEL];
	}
	else
	{
		sprintf(buf, "%u", epCompanionDescriptor->bMaxBurst);
		[thisDevice addProperty:"bMaxBurst:" withValue:buf atDepth:ENDPOINT_LEVEL];
	}

	switch (epType)
	{
		case kUSBBulk:
			if ( epCompanionDescriptor->bmAttributes > 16 )
			{
				sprintf(buf, "Illegal value of %u for bulk endpoint", epCompanionDescriptor->bmAttributes);
				[thisDevice addProperty:"bmAttributes:" withValue:buf atDepth:ENDPOINT_LEVEL];
			}
			else
			{
				sprintf(buf, "%u (%u MaxStreams)", epCompanionDescriptor->bmAttributes, 1 << epCompanionDescriptor->bmAttributes);
				[thisDevice addProperty:"bmAttributes:" withValue:buf atDepth:ENDPOINT_LEVEL];
			}
			break;
		case kUSBControl:
			if ( epCompanionDescriptor->bmAttributes != 0 )
			{
				sprintf(buf, "Illegal value of %u control endpoint", epCompanionDescriptor->bmAttributes);
				[thisDevice addProperty:"bmAttributes:" withValue:buf atDepth:ENDPOINT_LEVEL];
			}
			else
			{
				[thisDevice addProperty:"bmAttributes:" withValue:"0" atDepth:ENDPOINT_LEVEL];
			}
		case kUSBInterrupt:
		case kUSBIsoc:
			if ( epCompanionDescriptor->bmAttributes > 2 )
			{
				sprintf(buf, "Illegal value of %u for isoch/interrupt endpoint", epCompanionDescriptor->bmAttributes);
				[thisDevice addProperty:"bmAttributes:" withValue:buf atDepth:ENDPOINT_LEVEL];
			}
			else
			{
				sprintf(buf, "%u (Mult: %u,  max number of packets: %u)", epCompanionDescriptor->bmAttributes, epCompanionDescriptor->bmAttributes, epCompanionDescriptor->bMaxBurst * (epCompanionDescriptor->bmAttributes + 1));
				[thisDevice addProperty:"bmAttributes:" withValue:buf atDepth:ENDPOINT_LEVEL];
			}
	}
	
	if ( epType == kUSBInterrupt || epType == kUSBIsoc )
	{
		sprintf(buf, "%u", USBToHostLong(epCompanionDescriptor->wBytesPerInterval));
		[thisDevice addProperty:"wBytesPerInterval:" withValue:buf atDepth:ENDPOINT_LEVEL];
	}
}

@end
