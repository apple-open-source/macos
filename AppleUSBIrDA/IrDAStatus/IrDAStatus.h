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
#import <Cocoa/Cocoa.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>

#include <IOKit/IOTypes.h>
#include <IOKit/iokitmig_c.h>
#include <IOKit/IOKitLib.h>

#include "IrDAStats.h"
#include "IrDAUserClient.h"

@interface IrDAStatusObj : NSObject
{
    IBOutlet id connectionSpeed;
    IBOutlet id connectionState;
    IBOutlet id crcErrors;
    IBOutlet id dataPacketsIn;
    IBOutlet id dataPacketsOut;
    IBOutlet id dropped;
    IBOutlet id iFrameRec;
    IBOutlet id iFrameSent;
    IBOutlet id ioErrors;
    IBOutlet id nickName;
    IBOutlet id protocolErrs;
    IBOutlet id recTimeout;
    IBOutlet id rejRec;
    IBOutlet id rejSent;
    IBOutlet id resent;
    IBOutlet id rnrRec;
    IBOutlet id rnrSent;
    IBOutlet id rrRec;
    IBOutlet id rrSent;
    IBOutlet id srejRec;
    IBOutlet id srejSent;
    IBOutlet id uFrameRec;
    IBOutlet id uFrameSent;
    IBOutlet id xmitTimeout;
	NSTimer			*timer;
	Boolean			state;
	IrDAStatus		oldStatus;
	io_connect_t	conObj;
}
kern_return_t doCommand(io_connect_t con, unsigned char commandID, void *inputData, unsigned long inputDataSize, void *outputData, mach_msg_type_number_t *outputDataSize);
io_object_t getInterfaceWithName(mach_port_t masterPort, const char *className);
kern_return_t openDevice(io_object_t obj, io_connect_t * con);
kern_return_t closeDevice(io_connect_t con);

- (NSString *) GetCurrentDriverName;
- (void) InvalidateOldStatus;
- (void) DumpResults:(IrDAStatus *)stats;
- (IBAction)StartTimer:(id)sender;
@end
