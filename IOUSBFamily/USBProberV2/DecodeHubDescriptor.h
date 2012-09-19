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


#import <Foundation/Foundation.h>
#import "BusProberSharedFunctions.h"
#import "BusProbeDevice.h"

struct IOUSBHubDescriptor {
    UInt8 	length;
    UInt8 	hubType;
    UInt8 	numPorts;
    UInt16 	characteristics __attribute__((packed));
    UInt8   powerOnToGood;								// Port settling time, in 2ms
    UInt8 	hubCurrent;
    // These are received packed, will have to be unpacked
    UInt8 	removablePortFlags[9];
    UInt8 	pwrCtlPortFlags[9];
};

typedef struct IOUSBHubDescriptor IOUSBHubDescriptor;

#ifdef SUPPORTS_SS_USB
// To cope with the extra fields in a USB3 hub descriptor

struct IOUSB3HubDescriptor {
    UInt8   length;
    UInt8   hubType;
    UInt8   numPorts;
    UInt16  characteristics __attribute__((packed));
    UInt8   powerOnToGood;								// Port settling time, in 2ms
    UInt8   hubCurrent;
	UInt8   hubHdrDecLat;								// Header decode latency, new 3.0 field
    UInt16  hubDelay __attribute__((packed));			// new in 3.0
	
    // These are received packed, will have to be unpacked
    UInt8   removablePortFlags[9];
    UInt8   pwrCtlPortFlags[9];				// This field does not exist in the 3.0 descriptor
};

typedef struct IOUSB3HubDescriptor IOUSB3HubDescriptor;
#endif

@interface DecodeHubDescriptor : NSObject {

}

+ (void)decodeBytes:(Byte *)p forDevice:(BusProbeDevice *)thisDevice;

@end
