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
//
//  GetEntry.h
//  IrDA Status
//
//  Created by jwilcox on Mon Apr 23 2001.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>

#include <IOKit/IOTypes.h>
#include <IOKit/iokitmig_c.h>
#include <IOKit/IOKitLib.h>

#include "IrDAUserClient.h"

typedef struct IrDAEventDesc				// Each entry in the saved log looks like this
{
	UInt16				data1;				// two 16 bit numbers for the log
	UInt16				data2;
	UInt32				timeStamp;			// timestamp of the log entry - in microseconds
	char				*msg;				// pointer to copy of event msg
} IrDAEventDesc, *IrDAEventDescPtr;

typedef struct IrDALogHdr					// The one global log header to keep track of the log buffer
{
	IrDAEventDescPtr	fEventBuffer;		// pointer to base of the event log
	UInt32				fEventIndex;		// index of next available log entry
	UInt32				fPrintIndex;		// set by dcmd to keep track of what's pretty printed already
	UInt32				fNumEntries;		// kEntryCount -- let dcmd know when to wrap
	Boolean				fTracingOn;			// true if allowing adds
	Boolean				fWrapped;			// true if adding log entries wrapped past the printing ptr
} IrDALogHdr, *IrDAEventLogPtr;

typedef struct IrDALogInfo					// The pointers and buffers passed by to the dumplog application
{
	IrDALogHdr		*hdr;						// the global log header (points to event array)
	UInt32			hdrSize;					// size of the log hdr
	IrDAEventDesc	*eventLog;					// the event buffer
	UInt32			eventLogSize;				// size of the event log array
	char			*msgBuffer;					// pointer buffer of messages
	UInt32			msgBufferSize;				// size of above
} IrDALogInfo;

@interface GetEntry : NSObject {

}

kern_return_t doCommand(io_connect_t con, unsigned char commandID, void *inputData, unsigned long inputDataSize, void *outputData, mach_msg_type_number_t *outputDataSize);

+ (NSString *) GetCurrentDriverName;
+ (void) getEntry: (NSMutableArray *)logItems;

@end
