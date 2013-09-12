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


#import "DescriptorDecoder.h"
#import "DecodeHubDescriptor.h"


@implementation DecodeHubDescriptor

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice {
    IOUSBHubDescriptor 		hubDescriptor;
    UInt16			hubChar;
    char                        temporaryString[500];
    char                        buf[128];
    
    hubDescriptor = *(IOUSBHubDescriptor *)p;
    IOUSB3HubDescriptor 		hub3Descriptor;
    hub3Descriptor = *(IOUSB3HubDescriptor *)p;

    [thisDevice addProperty:"Hub Descriptor" withValue:"" atDepth:HUB_DESCRIPTOR_LEVEL-1];
    
    sprintf(buf, "%u", p[0]);
    [thisDevice addProperty:"Length (and contents):" withValue:buf atDepth:HUB_DESCRIPTOR_LEVEL];

	[DescriptorDecoder dumpRawDescriptor:(Byte *)p forDevice:thisDevice atDepth:HUB_DESCRIPTOR_LEVEL+1];
	
    [thisDevice addNumberProperty:"Number of Ports:" value: hubDescriptor.numPorts size:sizeof(hubDescriptor.numPorts) atDepth:HUB_DESCRIPTOR_LEVEL usingStyle:kHexOutputStyle];
//    NUM(hubDescriptor, "Number of Ports:", numPorts, deviceNumber, HUB_DESCRIPTOR_LEVEL, 0);
    
    hubChar = Swap16(&hubDescriptor.characteristics);
    
    sprintf(temporaryString, "0x%x (%sswitched %s hub with %s overcurrent protection", hubChar,
            (((hubChar & 3) == 0) ? "Gang " : ((hubChar & 3) == 1) ? "Individually " : "Non-"),
            ((hubChar & 4) == 4) ? "compound" : "standalone",
            ((hubChar & 0x18) == 0) ? "global" :
            ((hubChar & 0x18) == 0x8) ? "individual port" : "no");
    
    if ( [thisDevice usbRelease] == kUSBRel20 )
    {
        sprintf(buf, " requiring %d FS bit times and %s port indicators)", 
              (((hubChar & 0x60) >> 5) + 1 ) * 8,
               (hubChar & 0x80) ? "having" : " no");
        strcat(temporaryString, buf);
    }
    else
        strcat(temporaryString, ")");
               
    [thisDevice addProperty:"Hub Characteristics:" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
    
    sprintf(temporaryString, "%d ms", hubDescriptor.powerOnToGood*2);
    [thisDevice addProperty:"PowerOnToGood time:" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
    
    sprintf(temporaryString, "%d mA", hubDescriptor.hubCurrent);
    [thisDevice addProperty:"Controller current:" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
    
	if(hubDescriptor.hubType == kUSB3HUBDesc)
	{
		sprintf(temporaryString, "0.%d microsecs", hub3Descriptor.hubHdrDecLat);
		[thisDevice addProperty:"Header decode latency:" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
		sprintf(temporaryString, "%d ns", hub3Descriptor.hubDelay);
		[thisDevice addProperty:"Hub delay time:" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
		if (hubDescriptor.numPorts < 8)
		{
			sprintf(temporaryString, "0x%x", hub3Descriptor.removablePortFlags[0]);
			[thisDevice addProperty:"Device Removable (byte):" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
		}
		else if (hubDescriptor.numPorts < 16)
		{
			sprintf(temporaryString, "0x%x", (uint32_t)Swap16( &( (UInt16 *)hub3Descriptor.removablePortFlags)[0]));
			[thisDevice addProperty:"Device Removable (word):" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
		}
	}
	else
	{
    if (hubDescriptor.numPorts < 8)
    {
        sprintf(temporaryString, "0x%x", hubDescriptor.removablePortFlags[0]);
        [thisDevice addProperty:"Device Removable (byte):" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];

        sprintf(temporaryString, "0x%x", hubDescriptor.removablePortFlags[1]);
        [thisDevice addProperty:"Port Power Control Mask (byte):" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
    }
    else if (hubDescriptor.numPorts < 16)
    {
        sprintf(temporaryString, "0x%x", (uint32_t)Swap16( &( (UInt16 *)hubDescriptor.removablePortFlags)[0]));
        [thisDevice addProperty:"Device Removable (word):" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];

        sprintf(temporaryString, "0x%x", (uint32_t)Swap16(&((UInt16 *)hubDescriptor.removablePortFlags)[1]));
        [thisDevice addProperty:"Port Power Control Mask (word):" withValue:temporaryString atDepth:HUB_DESCRIPTOR_LEVEL];
    }
}

}

@end
