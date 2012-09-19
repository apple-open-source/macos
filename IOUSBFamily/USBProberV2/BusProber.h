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


#import <Foundation/Foundation.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/usb/USB.h>
#import <IOKit/usb/USBSpec.h>
#import <IOKit/IOCFPlugIn.h>
#import <mach/mach_port.h>
#import "BusProberSharedFunctions.h"
#import "OutlineViewNode.h"
#import "BusProbeDevice.h"
#import "BusProbeClass.h"

#import "DescriptorDecoder.h"

@interface BusProber : NSObject {
    id      _listener;
    
    NSMutableArray *        _devicesArray;
    CFRunLoopSourceRef      _runLoopSource;
}

- initWithListener:(id)listener devicesArray:(NSMutableArray *)devices;

- (BOOL)registerForUSBNotifications;
- (void)unregisterForUSBNotifications;

- (void)refreshData:(BOOL)shouldForce;

- (void)processDevice:(IOUSBDeviceRef)deviceIntf deviceNumber:(int)deviceNumber usbName:(NSString*)usbName;
- (void)PrintPortInfo: (uint32_t)portInfo forDevice:(BusProbeDevice *)thisDevice;
- (void)GetAndPrintNumberOfEndpoints:(IOUSBDeviceRef)deviceIntf forDevice:(BusProbeDevice *)thisDevice portInfo:(UInt32)portInfo;
@end

@protocol BusProberListener <NSObject>

- (void)busProberInformationDidChange:(BusProber *)aProber;

@end
