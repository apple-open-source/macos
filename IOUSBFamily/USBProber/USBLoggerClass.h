/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#import <Cocoa/Cocoa.h>
#import "MainController.h"

#import <Foundation/Foundation.h>
#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>
#import <sys/time.h>

#import <CoreServices/CoreServices.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/usb/IOUSBLib.h>
#import <IOKit/IODataQueueClient.h>
#import <IOKit/IODataQueueShared.h>
#import <mach/mach_port.h>

//================================================================================================
//   Defines
//================================================================================================
//

// Index for user client methods (from IOUSBLog.h)
enum
{
    kUSBControllerUserClientOpen = 0,
    kUSBControllerUserClientClose,
    kUSBControllerUserClientEnableLogger,
    kUSBControllerUserClientSetDebuggingLevel,
    kUSBControllerUserClientSetDebuggingType,
    kUSBControllerUserClientGetDebuggingLevel,
    kUSBControllerUserClientGetDebuggingType,
    kNumUSBControllerMethods
};

// KLog Message defines
//
#define _T_STAMP 	sizeof(struct timeval)
#define _LEVEL 		sizeof(UInt32)
#define _TAG		sizeof(UInt32)
#define _OFFSET 	(_T_STAMP + _LEVEL + _TAG)
#define _MSG		BUFSIZE - (_T_STAMP + _LEVEL + _TAG)

#define BUFSIZE 1024 	//entries
#define ENTRYSIZE 1200 	//bytes

#define Q_ON	1
#define Q_OFF	0

// Info Debug Output Types (from the KLog user client)
//
typedef UInt32		KernelDebuggingOutputType;
enum
{
    kKernelDebugOutputIOLogType		= 0x00000001,
    kKernelDebugOutputKextLoggerType	= 0x00000002
};


@interface usbLoggerClass : NSObject
{
    IBOutlet id loggingLevelPopup;
    IBOutlet id startLoggingButton;
    IBOutlet id usbloggerOutput;
    IBOutlet id usbloggerDumpToFile;
    IBOutlet id filterTextField1;
    IBOutlet id filterTextField2;
    IBOutlet id usbloggerFilteredOutput;
    IBOutlet id filterAndOrSelector;

    volatile BOOL loggingThreadIsRunning;
    volatile BOOL shouldDisplayOutput;
    volatile BOOL loggingLevelChanged;
    BOOL shouldDumpToFile;
}

- (IBAction)clearOutput:(id)sender;
- (IBAction)filterOutput:(id)sender;
- (IBAction)startLogging:(id)sender;
- (IBAction)timeStamp:(id)sender;

-(kern_return_t)OpenUSBControllerUserClient;
    kern_return_t SetDebuggerOptions( bool disableLogging, bool setLevel, UInt32 level, bool setType, UInt32 type );
-(kern_return_t)DumpUSBLog;
-(void)CleanUp;
+(void)CleanUp;  // So logger can be cleaned up externally (i.e. from the main controller @ Quit);
- (void)outputString:(NSString *)string unconditionally:(BOOL)unconditionally;
@end
