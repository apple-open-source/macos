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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "SCSIParallelTimer.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SPI TIMER"

#if DEBUG
#define SCSI_PARALLEL_TIMER_DEBUGGING_LEVEL					0
#endif


#include "IOSCSIParallelFamilyDebugging.h"


#if ( SCSI_PARALLEL_TIMER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PARALLEL_TIMER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PARALLEL_TIMER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOTimerEventSource
OSDefineMetaClassAndStructors ( SCSIParallelTimer, IOTimerEventSource );


#if 0
#pragma mark -
#pragma mark IOKit Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateTimerEventSource									   [STATIC][PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelTimer *
SCSIParallelTimer::CreateTimerEventSource ( OSObject * owner, Action action )
{
	
	SCSIParallelTimer *		timer = NULL;
	
	timer = OSTypeAlloc ( SCSIParallelTimer );
	require_nonzero ( timer, ErrorExit );
	
	require ( timer->init ( owner, action ), FreeTimer );
	
	return timer;
	
	
FreeTimer:
	
	
	require_nonzero ( timer, ErrorExit );
	timer->release ( );
	timer = NULL;
	
	
ErrorExit:
	
	
	return timer;
	
}
	

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Enable - Enables timer.											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSIParallelTimer::Enable ( void )
{
	super::enable ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Disable - Disables timer.										   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSIParallelTimer::Disable ( void )
{
	super::disable ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CancelTimeout - Cancels timeout.								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSIParallelTimer::CancelTimeout ( void )
{
	super::cancelTimeout ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompareDeadlines - Compares absolute times.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SInt32
SCSIParallelTimer::CompareDeadlines ( AbsoluteTime time1, AbsoluteTime time2 )
{
	
	return CMP_ABSOLUTETIME ( &time1, &time2 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetDeadline - Gets the deadline from the task.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

AbsoluteTime
SCSIParallelTimer::GetDeadline ( SCSIParallelTask * task )
{
	
	check ( task != NULL );
	return task->GetTimeoutDeadline ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetDeadline - Gets the deadline from the task.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSIParallelTimer::SetDeadline ( SCSIParallelTask * task, UInt32 inTimeoutMS )
{
	
	AbsoluteTime		delta;
	AbsoluteTime		deadline;
	
	check ( task != NULL );
	
	// Compute the deadline starting now.
	clock_interval_to_absolutetime_interval ( inTimeoutMS, kMillisecondScale, &delta );
	clock_get_uptime ( &deadline );
	ADD_ABSOLUTETIME ( &deadline, &delta );
	
	return task->SetTimeoutDeadline ( deadline );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetNextTask - Gets the next task in timeout list from the task.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelTask *
SCSIParallelTimer::GetNextTask ( SCSIParallelTask * task )
{
	
	check ( task != NULL );
	return task->GetNextTimeoutTaskInList ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetNextTask - Sets the next task in timeout list from the task.	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSIParallelTimer::SetNextTask ( SCSIParallelTask * task, SCSIParallelTask * next )
{
	
	check ( task != NULL );
	return task->SetNextTimeoutTaskInList ( next );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetTimeoutDuration - Gets the timeout from the task.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
SCSIParallelTimer::GetTimeoutDuration ( SCSIParallelTask * task )
{
	
	check ( task != NULL );
	return task->GetTimeoutDuration ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetExpiredTask - Gets the task which timed out.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelTask *
SCSIParallelTimer::GetExpiredTask ( void )
{
	
	SCSIParallelTask *	task = NULL;
	
	closeGate ( );
	
	if ( fTimeoutTaskListHead != NULL )
	{
		
		AbsoluteTime			now;
		AbsoluteTime			deadline;
		
        clock_get_uptime ( &now );
		deadline = GetDeadline ( fTimeoutTaskListHead );
		
		if ( CompareDeadlines ( now, deadline ) == 1 )
		{
			
			SCSIParallelTask *		newHead = NULL;
			
			newHead = GetNextTask ( fTimeoutTaskListHead );
			task = fTimeoutTaskListHead;
			SetNextTask ( task, NULL );
			fTimeoutTaskListHead = newHead;
			
		}
		
	}
	
	if ( fTimeoutTaskListHead == NULL )
	{
		
		cancelTimeout ( );
		
	}
	
	openGate ( );
	
	return task;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetTimeout - Sets timeout.										   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSIParallelTimer::SetTimeout ( SCSIParallelTaskIdentifier	taskIdentifier,
								UInt32						inTimeoutMS )
{
	
	SCSIParallelTask *	task 		= ( SCSIParallelTask * ) taskIdentifier;
	IOReturn			status		= kIOReturnBadArgument;
	
	require_nonzero ( task, ErrorExit );
	
	// Close the gate in order to ensure single-threaded access to list
	closeGate ( );
	
	// Did the HBA override the timeout value in the task?
	if ( inTimeoutMS == kTimeoutValueNone )
	{
		
		// No, use the timeout value in the task (in milliseconds)
		inTimeoutMS = GetTimeoutDuration ( task );
		
		// Is the timeout set to infinite?
		if ( inTimeoutMS == kTimeoutValueNone )
		{
			
			// Yes, set to longest possible timeout (ULONG_MAX)
			inTimeoutMS = 0xFFFFFFFF;
			
		}
		
	}
	
	SetDeadline ( task, inTimeoutMS );
	
	// Now move command down list to keep list sorted
	
	// 1) Check if we have a list head. If not, put this
	// element at the beginning.
	// 2) Check if the task has a shorter timeout than the list head
	if ( ( fTimeoutTaskListHead == NULL ) ||
		 ( CompareDeadlines ( GetDeadline ( task ), GetDeadline ( fTimeoutTaskListHead ) ) == 1 ) )
	{
		
		SCSIParallelTask *	oldHead = fTimeoutTaskListHead;
		
		fTimeoutTaskListHead = task;
		task->SetNextTimeoutTaskInList ( oldHead );
		
		Rearm ( );
		
	}
	
	else
	{
		
		SCSIParallelTask *	prev = fTimeoutTaskListHead;
		SCSIParallelTask *	next = NULL;
		
		next = GetNextTask ( fTimeoutTaskListHead );
		while ( next != NULL )
		{
			
			// Check if the next deadline is greater or not.
			if ( CompareDeadlines ( GetDeadline ( next ), GetDeadline ( task ) ) == 1 )
			{
				
				// Found the slot. This task should be ahead of next
				SetNextTask ( task, next );
				SetNextTask ( prev, task );
				
				// We're done. Break out.
				break;
				
			}
			
			prev = next;
			next = GetNextTask ( next );
			
		}
		
		if ( next == NULL )
		{
			
			// Found the slot (end of the list).
			SetNextTask ( task, NULL );
			SetNextTask ( prev, task );
			
		}
		
	}
	
	openGate ( );
	status = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Rearm - Arms the timeout timer.									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSIParallelTimer::Rearm ( void )
{
	
	bool	result = false;
	
	closeGate ( );
	
	if ( fTimeoutTaskListHead != NULL )
	{
	
		// Re-arm the timer with new timeout deadline
		wakeAtTime ( GetDeadline ( fTimeoutTaskListHead ) );
		result = true;
		
	}
	
	openGate ( );
	
	return result;
	
}