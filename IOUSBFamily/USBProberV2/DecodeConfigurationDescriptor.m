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


#import "DecodeConfigurationDescriptor.h"


@implementation DecodeConfigurationDescriptor

+ (void)decodeBytes:(IOUSBConfigurationDescHeader *)cfgHeader forDevice:(BusProbeDevice *)thisDevice deviceInterface:(IOUSBDeviceRef)deviceIntf configNumber:(int)iconfig currentConfig:(int)cconfig isOtherSpeedDesc:(BOOL)isOtherSpeedDesc {
    /*	struct IOUSBConfigurationDescriptor {
    UInt8 			bLength;
    UInt8 			bDescriptorType;
    UInt16 			wTotalLength;
    UInt8 			bNumInterfaces;
    UInt8 			bConfigurationValue;
    UInt8 			iConfiguration;
    UInt8 			bmAttributes;
    UInt8 			MaxPower;
    }; */
    
    IOUSBConfigurationDescriptor *   cfg;
    Byte                            *configBuf;
    Byte                            *p, *pend;
    char                            *cstr1;
    char                            str[500];
    char							buf[128];
    IOReturn                        ret;
    UInt8                           descType = isOtherSpeedDesc ? kUSBOtherSpeedConfDesc : kUSBConfDesc;
	UInt32							maxPower = 0;
	UInt8							lastEPType = 0;
    
    if ( cfgHeader->wTotalLength == 0 )
    {
		IOReturn	actErr;
		
		configBuf = malloc(sizeof(cfgHeader));
	    (void) GetDescriptor(deviceIntf, descType, iconfig, configBuf, sizeof(cfgHeader), &actErr);

        // The device did not respond to a request for the first x bytes of the Configuration Descriptor  (We
        // encoded the value in the bDescriptorType field.
        // This description will be shown in the UI
        //
        sprintf(str, "Device gave an error %s (0x%x) when asked for first %u bytes of descriptor", USBErrorToString(actErr), actErr, cfgHeader->bDescriptorType);
        [thisDevice addProperty:"Configuration Descriptor" withValue:str atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1];
		free(configBuf);
        return;
    }
    // We only have the Configuration Descriptor Header.  We need to get the full descriptor first:
    //
    Swap16(&cfgHeader->wTotalLength);
    configBuf = malloc(cfgHeader->wTotalLength*sizeof(Byte));
    
    ret = GetDescriptor(deviceIntf, descType, iconfig, configBuf, cfgHeader->wTotalLength, nil);
    if ( ret != kIOReturnSuccess )
        return;
    
    // Save a copy of a full Configuration Buffer
    //
    p = configBuf;

    //  Display the standard fields of the config descriptor
    //
    cfg = (IOUSBConfigurationDescriptor *)configBuf;
    Swap16(&cfg->wTotalLength);
    
    
    cstr1 = GetStringFromIndex((UInt8)cfg->iConfiguration, deviceIntf);
    
    if (strcmp(cstr1, "0x00") != 0) {
        if (!isOtherSpeedDesc)
		{
			if (cfg->bConfigurationValue == cconfig)	// Current config
			{
				[thisDevice addProperty:"Configuration Descriptor (current config): ......................" withValue:cstr1 atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1];
			}
			else
			{
				[thisDevice addProperty:"Configuration Descriptor: ......................................." withValue:cstr1 atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1];
			}
        }
		else
            [thisDevice addProperty:"Other Speed Configuration Descriptor: ......................................." withValue:cstr1 atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1];
    }
    else {
        if (!isOtherSpeedDesc)
			if(cfg->bConfigurationValue==cconfig)	// Current config
			{
				[thisDevice addProperty:"Configuration Descriptor (current config)" withValue:"" atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1];
			}
			else
			{
				[thisDevice addProperty:"Configuration Descriptor" withValue:"" atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1];
			}
        else
            [thisDevice addProperty:"Other Speed Configuration Descriptor" withValue:"" atDepth:CONFIGURATION_DESCRIPTOR_LEVEL-1];
    }
    
    FreeString(cstr1);
    
    // Print the Length and contents of this descriptor
    //
    sprintf(str, "%u", cfg->wTotalLength);
    [thisDevice addProperty:"Length (and contents):" withValue:str atDepth:CONFIGURATION_DESCRIPTOR_LEVEL];
    [DescriptorDecoder dumpRawConfigDescriptor:(IOUSBConfigurationDescriptor *)cfg forDevice:thisDevice atDepth:CONFIGURATION_DESCRIPTOR_LEVEL+1];

    
    [thisDevice addNumberProperty:"Number of Interfaces:" value: cfg->bNumInterfaces size:sizeof(cfg->bNumInterfaces) atDepth:CONFIGURATION_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
    [thisDevice addNumberProperty:"Configuration Value:" value: cfg->bConfigurationValue size:sizeof(cfg->bConfigurationValue) atDepth:CONFIGURATION_DESCRIPTOR_LEVEL usingStyle:kIntegerOutputStyle];
    
    sprintf(str, "0x%02X", cfg->bmAttributes);
    if (cfg->bmAttributes & 0x40) {
        strcat(str, " (self-powered");
    }
    else {
        strcat(str, " (bus-powered");
    }
    if (cfg->bmAttributes & 0x20) {
        strcat(str, ", remote wakeup");
    }
    strcat(str, ")");
    [thisDevice addProperty:"Attributes:" withValue:str atDepth:CONFIGURATION_DESCRIPTOR_LEVEL];
    
//    cstr1 = GetStringFromNumber(cfg->MaxPower, sizeof(cfg->MaxPower), kIntegerOutputStyle);
//    sprintf(str, "%d ma", [[NSString stringWithCString:cstr1 encoding:NSUTF8StringEncoding] intValue]*2);
//    [thisDevice addProperty:"MaxPower:" withValue:str atDepth:CONFIGURATION_DESCRIPTOR_LEVEL];
//    FreeString(cstr1);
    

   //  The MaxPower field of a configuration in USB 3 depends on whether the device is operating in SuperSpeed mode or 
	//  not.  If it is not, then the units are 2ma.  If it is, they are 8mA.  USB3 does not require an OtherSpeed configuration descriptor
    if ( ([thisDevice usbRelease] >= kUSBRel30) )
    {
        if ([thisDevice speed] == kUSBDeviceSpeedSuper)
			maxPower = 8 * cfg->MaxPower;
	}
	else 
	{
		maxPower = 2 * cfg->MaxPower;
	}
	sprintf(buf, "%u mA", (uint32_t)maxPower);
	[thisDevice addProperty:"MaxPower:" withValue:buf atDepth:CONFIGURATION_DESCRIPTOR_LEVEL];

    
    
    pend = p + cfg->wTotalLength;
    p += cfg->bLength;
    
    
    // Dump the descriptors in the Configuration Descriptor
    //
    while (p < pend)
    {
        UInt8 descLen = p[0];
        UInt8 descType = p[1];

        if ( descLen == 0 )
        {
            [thisDevice addProperty:"Illegal Descriptor:" withValue: "Length of 0" atDepth:CONFIGURATION_DESCRIPTOR_LEVEL];
            break;
        }
        else
        {
            //  If this is an interface descriptor, save the interface class and subclass
            //
            if ( descType == kUSBInterfaceDesc )
            {
                [thisDevice setLastInterfaceClassInfo:[BusProbeClass withClass:((IOUSBInterfaceDescriptor *)p)->bInterfaceClass subclass:((IOUSBInterfaceDescriptor *)p)->bInterfaceSubClass protocol:((IOUSBInterfaceDescriptor *)p)->bInterfaceProtocol]];
                [thisDevice setCurrentInterfaceNumber:(int)((IOUSBInterfaceDescriptor *)p)->bInterfaceNumber];
            }

            [DescriptorDecoder decodeBytes:p forDevice:thisDevice deviceInterface:deviceIntf userInfo:(void *)&lastEPType isOtherSpeedDesc:isOtherSpeedDesc isinCurrentConfig:cfg->bConfigurationValue==cconfig];

			if ( descType == kUSBEndpointDesc )
				lastEPType = ((IOUSBEndpointDescriptor *)p)->bmAttributes & 0x03;

            p += descLen;
        }
    }
	
	// Release our malloc'd buffer
    free(configBuf);
}

@end
