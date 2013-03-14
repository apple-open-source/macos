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


@implementation DescriptorDecoder

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice deviceInterface:(IOUSBDeviceRef)deviceIntf userInfo:(void *)userInfo isOtherSpeedDesc:(BOOL)isOtherSpeedDesc  isinCurrentConfig:(Boolean)inCurrentConfig
{
    switch ((UInt8)p[1]) {
        case kUSBInterfaceDesc:
            [DecodeInterfaceDescriptor decodeBytes:p forDevice:thisDevice withDeviceInterface:deviceIntf];
            break;
        case kUSBEndpointDesc:
            [DecodeEndpointDescriptor decodeBytes:p forDevice:thisDevice isOtherSpeedDesc:isOtherSpeedDesc];
            break;
        case kUSBSuperSpeedEndpointCompanion:
            [DecodeEndpointDescriptor decodeBytesCompanion:p forDevice:thisDevice endpoint:*(UInt8 *)userInfo];
            break;
        case HID_DESCRIPTOR:
			// case DFU_FUNCTIONAL_DESCRIPTOR:  - same value, compiler complains
			// case CCID_DESCRIPTOR: // same value again
            [DecodeHIDDescriptor decodeBytes:p forDevice:thisDevice withDeviceInterface:deviceIntf isinCurrentConfig:inCurrentConfig];
            break;
        case kUSBHUBDesc:
        case kUSB3HUBDesc:
            [DecodeHubDescriptor decodeBytes:p forDevice:thisDevice];
            break;
        case kUSBDeviceQualifierDesc:
            [DecodeDeviceQualifierDescriptor decodeBytes:p forDevice:thisDevice];
            break;
        case kUSBInterfaceAssociationDesc:
            [DecodeInterfaceAssociationDescriptor decodeBytes:p forDevice:thisDevice withDeviceInterface:deviceIntf];
            break;
        default:
            switch([[thisDevice lastInterfaceClassInfo] classNum])
            {
                case 1: /* audio class */
                    [DecodeAudioInterfaceDescriptor decodeBytes:p forDevice:thisDevice];
                    break;
                case 2: /* communication class */
                    [DecodeCommClassDescriptor decodeBytes:p forDevice:thisDevice];
                    break;
                case 0xe: /* video class */
                    [DecodeVideoInterfaceDescriptor decodeBytes:p forDevice:thisDevice withDeviceInterface:deviceIntf];
                    break;
                case 8: /* Mass Storage */
                    [DecodeMassStorageDescriptor decodeBytes:p forDevice:thisDevice];
                    break;
                default:
                    [self dumpRawDescriptor:p forDevice:thisDevice atDepth:CONFIGURATION_DESCRIPTOR_LEVEL];
                    break;
            }
            break;
    }
}

+(void)dumpRawDescriptor:(Byte *)p forDevice:(BusProbeDevice *)thisDevice atDepth:(int)depth {
    [self dump:p[0] byte:p forDevice:thisDevice atDepth:depth];
}    

+(void)dumpRawConfigDescriptor:(IOUSBConfigurationDescriptor*)cfg forDevice:(BusProbeDevice *)thisDevice atDepth:(int)depth {
    [self dump:cfg->wTotalLength byte:(Byte* )cfg forDevice:thisDevice atDepth:depth];
    
}

+(void)dumpRawBOSDescriptor:(IOUSBBOSDescriptor*)bos forDevice:(BusProbeDevice *)thisDevice atDepth:(int)depth {
    [self dump:bos->wTotalLength byte:(Byte* )bos forDevice:thisDevice atDepth:depth];
    
}

+(void)dump:(int)n byte:(Byte *)p forDevice:(BusProbeDevice *)thisDevice atDepth:(int)depth {
#define BYTESPERLINE	16
    
    int 	lineCount = 0;
    int		runningCount = 0;
    int		lastLine = 0;
    char 	str1[BYTESPERLINE * 6] = "";  // 0xXX + 2 spaces
    char 	str2[10];
    char	descriptor[40];
    
    strcat( str1, "0000: ");
    
    while (--n >= 0) 
    {
        sprintf(str2, "%02X ", *p++);
        strcat(str1, str2);
        
        lineCount++;
        runningCount++;
        
        // Add a space in between BYTESPERLINE / 2 and the next one
        //
        if ( (runningCount % (BYTESPERLINE>>1)) == 0 )
            strcat(str1, " ");
        
        // Add the index to the bytes (should they be in hex?) to the text
        //
        sprintf(descriptor, "Raw Descriptor (hex) ");
        
        // Split the descriptor into BYTESPERLINE bytes each line so that it's more readabale
        //
        if ( lineCount == BYTESPERLINE )
        {
            [thisDevice addProperty:descriptor withValue:str1 atDepth:depth];
            lastLine = runningCount;
            lineCount = 0;
            sprintf(str1, "%4.4x: ",runningCount);
            //   strcpy(str1,"");
        }
    }
    
    if ( lineCount != 0 )
    {
        // Don't add an index for descriptors that only occupy one line
        //
        if ( lastLine == 0 )
            strcpy(descriptor,"Raw Descriptor (hex)");
        
        [thisDevice addProperty:descriptor withValue:str1 atDepth:depth];
    }
    else
        [thisDevice addProperty:"Unknown Descriptor" withValue:str1 atDepth:depth];
    
    
    
    return;
}

@end
