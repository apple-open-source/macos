/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
#include "SCSIParallelWorkLoop.h"


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SPI WorkLoop"

#if DEBUG
#define SCSI_PARALLEL_WORKLOOP_DEBUGGING_LEVEL				0
#endif


#include "IOSCSIParallelFamilyDebugging.h"


#if ( SCSI_PARALLEL_WORKLOOP_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PARALLEL_WORKLOOP_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PARALLEL_WORKLOOP_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOWorkLoop
OSDefineMetaClassAndStructors ( SCSIParallelWorkLoop, IOWorkLoop );


#if 0
#pragma mark -
#pragma mark IOKit Member Routines
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	Create													   [STATIC][PUBLIC]
//-----------------------------------------------------------------------------

SCSIParallelWorkLoop *
SCSIParallelWorkLoop::Create ( const char *	lockGroupName )
{
	
	SCSIParallelWorkLoop *		workLoop = NULL;
	
	workLoop = OSTypeAlloc ( SCSIParallelWorkLoop );
	require_nonzero ( workLoop, ErrorExit );
	
	require ( workLoop->InitWithLockGroupName ( lockGroupName ), ReleaseWorkLoop );
	
	return workLoop;
	
	
ReleaseWorkLoop:
	
	
	require_nonzero ( workLoop, ErrorExit );
	workLoop->release ( );
	workLoop = NULL;
	
	
ErrorExit:
	
	
	return workLoop;
	
}


//-----------------------------------------------------------------------------
//	InitWithLockGroupName											[PROTECTED]
//-----------------------------------------------------------------------------

bool
SCSIParallelWorkLoop::InitWithLockGroupName ( const char * lockGroupName )
{
	
	bool	result = false;
	
	fLockGroup = lck_grp_alloc_init ( lockGroupName, LCK_GRP_ATTR_NULL );
	require_nonzero ( fLockGroup, ErrorExit );
	
	// Allocate the gateLock before calling the super class. This allows
	// us to profile contention on our workloop lock.
	gateLock = IORecursiveLockAllocWithLockGroup ( fLockGroup );
	
	result = super::init ( );
	
	
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	free															[PROTECTED]
//-----------------------------------------------------------------------------

void
SCSIParallelWorkLoop::free ( void )
{
	
	// NOTE: IOWorkLoop::free() gets called multiple times!
	if ( fLockGroup != NULL )
	{
		
		lck_grp_free ( fLockGroup );
		fLockGroup = NULL;
		
	}
	
	super::free ( );
	
}
