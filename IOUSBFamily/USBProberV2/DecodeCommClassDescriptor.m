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

#import "DecodeCommClassDescriptor.h"


@implementation DecodeCommClassDescriptor

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice {
    char 	temporaryString[500];
    
    switch ( ((GenericAudioDescriptorPtr)p)->descSubType )
    {
        case 0:
            sprintf((char *)temporaryString, "Comm Class Header Functional Descriptor");
            break;
        case 1:
            sprintf((char *)temporaryString, "Comm Class Call Management Functional Descriptor");
            break;
        case 2:
            sprintf((char *)temporaryString, "Comm Class Abstract Control Management Functional Descriptor");
            break;
        case 3:
            sprintf((char *)temporaryString, "Comm Class Direct Line Management Functional Descriptor");
            break;
        case 4:
            sprintf((char *)temporaryString, "Comm Class Telephone Ringer Functional Descriptor");
            break;
        case 5:
            sprintf((char *)temporaryString, "Comm Class Call and LIne State Reporting Functional Descriptor");
            break;
        case 6:
            sprintf((char *)temporaryString, "Comm Class Union Functional Descriptor");
            break;
        case 7:
            sprintf((char *)temporaryString, "Comm Class Country Selection Functional Descriptor");
            break;
        case 8:
            sprintf((char *)temporaryString, "Comm Class Telephone Operational Modes Functional Descriptor");
            break;
        case 9:
            sprintf((char *)temporaryString, "Comm Class USB Terminal Functional Descriptor");
            break;
        case 10:
            sprintf((char *)temporaryString, "Comm Class Network Channel Terminal Functional Descriptor");
            break;
        case 11:
            sprintf((char *)temporaryString, "Comm Class Protocol Unit Functional Descriptor");
            break;
        case 12:
            sprintf((char *)temporaryString, "Comm Class Extension Unit Functional Descriptor");
            break;
        case 13:
            sprintf((char *)temporaryString, "Comm Class Multi-Channel Management Functional Descriptor");
            break;
        case 14:
            sprintf((char *)temporaryString, "Comm Class CAPI Control Management Functional Descriptor");
            break;
        case 15:
            sprintf((char *)temporaryString, "Comm Class Ethernet Networking Functional Descriptor");
            break;
        case 16:
            sprintf((char *)temporaryString, "Comm Class ATM Networking Functional Descriptor");
            break;
        default:
            sprintf((char *)temporaryString, "Comm Class Reserved Functional Descriptor (%d)",((GenericAudioDescriptorPtr)p)->descSubType);
            break;
    }
    
    [thisDevice addProperty:temporaryString withValue:"" atDepth:CONFIGURATION_DESCRIPTOR_LEVEL+1];
    
    [DescriptorDecoder dumpRawDescriptor:p forDevice:thisDevice atDepth:CONFIGURATION_DESCRIPTOR_LEVEL+2];
}

@end
