/*
 * Copyright © 1998-2010, 2012 Apple Inc.  All rights reserved.
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


#import <Cocoa/Cocoa.h>
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

struct klog64_timeval{
	uint64_t	tv_sec;
	uint64_t	tv_usec;
};

// KLog Message defines
//
#define _T_STAMP 	sizeof(struct klog64_timeval)
#define _LEVEL 		sizeof(uint32_t)
#define _TAG		sizeof(uint32_t)
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

@protocol USBLoggerListener <NSObject>

- (void)usbLoggerTextAvailable:(NSString *)text forLevel:(int)level;

@end

@interface USBLogger : NSObject {
    id              _listener;
    int             _loggingLevel;
    BOOL            _isLogging;
    mach_port_t     _gMasterPort;
    io_connect_t    _gControllerUserClientPort;
    io_connect_t    _gKLogUserClientPort;
    mach_port_t     _gQPort;
    IODataQueueMemory *	_gMyQueue;
}

- initWithListener:(id <USBLoggerListener>)listener level:(int)level;

- (kern_return_t)OpenUSBControllerUserClient;
- (kern_return_t)setDebuggerOptions:(int)shouldLogFlag setLevel:(bool)setLevel level:(UInt32)level setType:(bool)setType type:(UInt32)type;

- (void)beginLogging;
- (void)invalidate;
- (void)setLevel:(int)level;

- (kern_return_t)callUSBControllerUserClient:(io_connect_t)port methodIndex:(UInt32)methodIndex inParam:(UInt32)inParam;
@end


