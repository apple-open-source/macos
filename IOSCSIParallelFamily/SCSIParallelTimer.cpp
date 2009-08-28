/*
 * Copyright (c) 2002-2008 Apple Inc. All rights reserved.
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


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <IOKit/IOTypes.h>
#include "SCSIParallelTimer.h"


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SPI TIMER"

#if DEBUG
#define SCSI_PARALLEL_TIMER_DEBUGGING_LEVEL					0
#endif


#include "IOSCSIParallelFamilyDebugging.h"


#if ( SCSI_PARALLEL_TIMER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
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


//-----------------------------------------------------------------------------
//	CreateTimerEventSource									   [STATIC][PUBLIC]
//-----------------------------------------------------------------------------

SCSIParallelTimer *
SCSIParallelTimer::CreateTimerEventSource ( OSObject * owner, Action action )
{
	
	SCSIParallelTimer *		timer = NULL;
	
	timer = OSTypeAlloc ( SCSIParallelTimer );
	require_nonzero ( timer, ErrorExit );
	
	require ( timer->Init ( owner, action ), FreeTimer );
	
	return timer;
	
	
FreeTimer:
	
	
	require_nonzero ( timer, ErrorExit );
	timer->release ( );
	timer = NULL;
	
	
ErrorExit:
	
	
	return timer;
	
}


//-----------------------------------------------------------------------------
//	Enable - Enables timer.											   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTimer::Init ( OSObject * owner, Action action )
{
	
	queue_init ( &fListHead );
	return super::init ( owner, action );
	
}


//-----------------------------------------------------------------------------
//	Enable - Enables timer.											   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTimer::Enable ( void )
{
	super::enable ( );
}


//-----------------------------------------------------------------------------
//	Disable - Disables timer.										   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTimer::Disable ( void )
{
	super::disable ( );
}


//-----------------------------------------------------------------------------
//	CancelTimeout - Cancels timeout.								   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTimer::CancelTimeout ( void )
{
	super::cancelTimeout ( );
}


//-----------------------------------------------------------------------------
//	BeginTimeoutContext - Begins context.							   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTimer::BeginTimeoutContext ( void )
{
	
	closeGate ( );
	fHandlingTimeout = true;
	openGate ( );
	
}


//-----------------------------------------------------------------------------
//	EndTimeoutContext - Ends context.								   [PUBLIC]
//-----------------------------------------------------------------------------


void
SCSIParallelTimer::EndTimeoutContext ( void )
{
	
	closeGate ( );
	fHandlingTimeout = false;
	openGate ( );
	
}


//-----------------------------------------------------------------------------
//	CompareDeadlines - Compares absolute times.						   [PUBLIC]
//-----------------------------------------------------------------------------

SInt32
SCSIParallelTimer::CompareDeadlines ( AbsoluteTime time1, AbsoluteTime time2 )
{
	
	return CMP_ABSOLUTETIME ( &time1, &time2 );
	
}


//-----------------------------------------------------------------------------
//	GetDeadline - Gets the deadline from the task.					   [PUBLIC]
//-----------------------------------------------------------------------------

AbsoluteTime
SCSIParallelTimer::GetDeadline ( SCSIParallelTask * task )
{
	
	check ( task != NULL );
	return task->GetTimeoutDeadline ( );
	
}


//-----------------------------------------------------------------------------
//	GetTimeoutDuration - Gets the timeout from the task.			   [PUBLIC]
//-----------------------------------------------------------------------------

UInt32
SCSIParallelTimer::GetTimeoutDuration ( SCSIParallelTask * task )
{
	
	check ( task != NULL );
	return task->GetTimeoutDuration ( );
	
}


//-----------------------------------------------------------------------------
//	GetExpiredTask - Gets the task which timed out.					   [PUBLIC]
//-----------------------------------------------------------------------------

SCSIParallelTaskIdentifier
SCSIParallelTimer::GetExpiredTask ( void )
{
	
	SCSIParallelTask *	expiredTask = NULL;
	
	closeGate ( );
	
	if ( queue_empty ( &fListHead ) == false )
	{
		
		uint64_t			now;
		AbsoluteTime		deadline1;
		AbsoluteTime		deadline2;
		SCSIParallelTask *	task;
		
        task		= ( SCSIParallelTask * ) queue_first ( &fListHead );
        now 		= mach_absolute_time ( );
        deadline1	= *( AbsoluteTime * ) &now;
		deadline2 	= GetDeadline ( task );
		
		if ( CompareDeadlines ( deadline1, deadline2 ) == 1 )
		{
			
			queue_remove_first ( &fListHead, expiredTask, SCSIParallelTask *, fTimeoutChain );
			
		}
		
	}
	
	openGate ( );
	
	return ( SCSIParallelTaskIdentifier ) expiredTask;
	
}


//-----------------------------------------------------------------------------
//	SetTimeout - Sets timeout.										   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
SCSIParallelTimer::SetTimeout ( SCSIParallelTaskIdentifier	taskIdentifier,
								UInt32						inTimeoutMS )
{
	
	SCSIParallelTask *	task 		= ( SCSIParallelTask * ) taskIdentifier;
	IOReturn			status		= kIOReturnBadArgument;
	AbsoluteTime		deadline;
	
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
	
	clock_interval_to_deadline ( inTimeoutMS, kMillisecondScale, &deadline );
	task->SetTimeoutDeadline ( deadline );
	
	// 1) Check if we have a list head. If not, put this
	// element at the beginning.
	// 2) Check if the task has a shorter timeout than the list head
	if ( ( queue_empty ( &fListHead ) == true ) ||
		 ( CompareDeadlines ( GetDeadline ( ( SCSIParallelTask * ) queue_first ( &fListHead ) ), deadline ) == 1 ) )
	{
		
		queue_enter_first ( &fListHead, task, SCSIParallelTask *, fTimeoutChain );
		Rearm ( );
		
	}
	
	// 3) In the normal case, I/Os are coming down with standard timeout intervals (30s). In this
	// case, we try to check against the last I/O on the timeout list (to avoid walking the entire
	// list in the normal case).
	else if ( CompareDeadlines ( deadline, GetDeadline ( ( SCSIParallelTask * ) queue_last ( &fListHead ) ) ) == 1 )
	{
		
		queue_enter ( &fListHead, task, SCSIParallelTask *, fTimeoutChain );
		
	}
	
	// 4) Walk the entire list looking for the proper slot. <sigh>
	else
	{

		SCSIParallelTask *	currentTask = NULL;
		bool				slotFound	= false;
		
		queue_iterate ( &fListHead, currentTask, SCSIParallelTask *, fTimeoutChain )
		{
			
			// Check if the next deadline is greater or not.
			if ( CompareDeadlines ( GetDeadline ( currentTask ), deadline ) == 1 )
			{
				
				// Found the slot. This task should be ahead of currentTask.
				queue_insert_before ( &fListHead, task, currentTask, SCSIParallelTask *, fTimeoutChain );
				slotFound = true;
				
				// We're done. Break out.
				break;
				
			}
			
		}
		
		if ( slotFound == false )
		{
			
			// Found the slot (end of the list).
			queue_enter ( &fListHead, task, SCSIParallelTask *, fTimeoutChain );
			
		}
		
	}
	
	openGate ( );
	status = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return status;
	
}


//-----------------------------------------------------------------------------
//	RemoveTask - Removes a task from the timeout list.				   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTimer::RemoveTask ( SCSIParallelTaskIdentifier parallelRequest )
{
	
	SCSIParallelTask *	task		= NULL;
	bool				headOfList	= false;
	
	task = OSDynamicCast ( SCSIParallelTask, parallelRequest );
	
	require_nonzero ( task, Exit );
	require_nonzero ( ( task->fTimeoutChain.next ), Exit );
	require_nonzero ( ( task->fTimeoutChain.prev ), Exit );
	
	closeGate ( );
	
	require ( ( queue_empty ( &fListHead ) == false ), ExitGate );
	
	if ( task == ( SCSIParallelTask * ) queue_first ( &fListHead ) )
		headOfList = true;
	
	queue_remove ( &fListHead, task, SCSIParallelTask *, fTimeoutChain );
	
	// Special case for parallelRequest being the list head.
	if ( headOfList == true )
	{
		
		Rearm ( );
		
	}
	
	
ExitGate:
	
	
	openGate ( );
	
	
Exit:
	
	
	return;
	
}


//-----------------------------------------------------------------------------
//	Rearm - Arms the timeout timer.									   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTimer::Rearm ( void )
{
	
	bool	result = false;
	
	closeGate ( );
	
	if ( ( queue_empty ( &fListHead ) == false ) && ( fHandlingTimeout == false ) )
	{
		
		// Re-arm the timer with new timeout deadline
		wakeAtTime ( GetDeadline ( ( SCSIParallelTask * ) queue_first ( &fListHead ) ) );
		result = true;
		
	}
	
	else
	{
		
		// No list head, cancel the timer.
		cancelTimeout ( );
		
	}
	
	openGate ( );
	
	return result;
	
}