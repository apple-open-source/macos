/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#import "IrDAActions.h"
#import "IrDAStatus.h"
#import <sys/stat.h>
#import "Preferences.h"

@implementation IrDAActions
// IsCheetahNetworkPrefs
- (NSString *) GetCurrentDriverName{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	return ([defaults objectForKey: DefaultDriverKey]);
}
- (IBAction)StartIrDA:(id)sender
{
    kern_return_t	kr;
    io_object_t		netif;
    io_connect_t	conObj;
    mach_port_t		masterPort;
	NSString		*driverName = [self GetCurrentDriverName];
	const char		*driverCStringName = [driverName cString];

	NSLog(@"StartIrDA");
    // Get master device port
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
		printf("IOMasterPort() failed: %08lx\n", (unsigned long)kr);
		return;
    }
    netif = getInterfaceWithName(masterPort, driverCStringName);
    if (netif) {
		kr = openDevice(netif, &conObj);
		if (kr == kIOReturnSuccess) {
			mach_msg_type_number_t infosize = 0;

			kr = doCommand(conObj, kIrDAUserCmd_Enable, nil, 0, nil, &infosize);
			if (kr == kIOReturnSuccess) {
				NSLog(@"StartIrDA: kIrDAUserCmd_Enable worked!");
			}
			else{
				printf("kr is %d\n", kr);
			}
			closeDevice(conObj);
      	}
		IOObjectRelease(netif);
    }
	else{
		NSLog(@"Unable to find the Requested Driver");
	}
}
- (IBAction)StopIrDA:(id)sender
{
    kern_return_t	kr;
    io_object_t		netif;
    io_connect_t	conObj;
    mach_port_t		masterPort;
	NSString		*driverName = [self GetCurrentDriverName];
	const char		*driverCStringName = [driverName cString];

	NSLog(@"StopIrDA");
    // Get master device port
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
		printf("IOMasterPort() failed: %08lx\n", (unsigned long)kr);
		return;
    }
    netif = getInterfaceWithName(masterPort, driverCStringName);
    if (netif) {
		kr = openDevice(netif, &conObj);
		if (kr == kIOReturnSuccess) {
			mach_msg_type_number_t infosize = 0;

			kr = doCommand(conObj, kIrDAUserCmd_Disable, nil, 0, nil, &infosize);
			if (kr == kIOReturnSuccess) {
				NSLog(@"StopIrDA: kIrDAUserCmd_Disable worked!");
			}
			else{
				printf("kr is %d\n", kr);
			}
			closeDevice(conObj);
      	}
		IOObjectRelease(netif);
    }
	else{
		NSLog(@"Unable to find the Requested Driver");
	}
}

@end
