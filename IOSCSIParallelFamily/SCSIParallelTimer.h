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

#ifndef __IOKIT_SCSI_PARALLEL_TIMER_H__
#define __IOKIT_SCSI_PARALLEL_TIMER_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// General IOKit includes
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>

// SCSI Parallel Family includes
#include "SCSIParallelTask.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kTimeoutValueNone	0


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSIParallelTimer : public IOTimerEventSource
{
	
	OSDeclareDefaultStructors ( SCSIParallelTimer )
	
public:
	
	static SCSIParallelTimer *
	CreateTimerEventSource ( OSObject * owner, Action action = 0 );
	
	void 				Enable ( void );
	void 				Disable ( void );
	void 				CancelTimeout ( void );
	bool				Rearm ( void );
	
	IOReturn 			SetTimeout (
							SCSIParallelTaskIdentifier	taskIdentifier,
						  	UInt32					 	timeoutInMS = kTimeoutValueNone );
	
	void				RemoveTask (
							SCSIParallelTaskIdentifier	taskIdentifier );
	
	SCSIParallelTaskIdentifier	GetExpiredTask ( void );
	
	
protected:
	
	SInt32				CompareDeadlines ( AbsoluteTime time1, AbsoluteTime time2 );
	AbsoluteTime		GetDeadline ( SCSIParallelTask * task );
	void				SetDeadline ( SCSIParallelTask * task, UInt32 inTimeoutMS );
	SCSIParallelTask *	GetNextTask ( SCSIParallelTask * task );
	void				SetNextTask ( SCSIParallelTask * task, SCSIParallelTask * next );
	UInt32				GetTimeoutDuration ( SCSIParallelTask * task );
	
	
private:
	
	SCSIParallelTask *		fTimeoutTaskListHead;
	
};


#endif	/* __IOKIT_SCSI_PARALLEL_TIMER_H__ */