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


// SCSI Parallel Family includes
#include "IOSCSIParallelInterfaceController.h"
#include "IOSCSIParallelInterfaceDevice.h"
#include "SCSIParallelTask.h"
#include "SCSIParallelTimer.h"

// Generic IOKit includes
#include <IOKit/IOService.h>
#include <IOKit/IOCommandPool.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SPI Controller"

#if DEBUG
#define SCSI_PARALLEL_INTERFACE_CONTROLLER_DEBUGGING_LEVEL	3
#endif

#include "IOSCSIParallelFamilyDebugging.h"

#if ( SCSI_PARALLEL_INTERFACE_CONTROLLER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)		
#endif

#if ( SCSI_PARALLEL_INTERFACE_CONTROLLER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)		
#endif

#if ( SCSI_PARALLEL_INTERFACE_CONTROLLER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)		
#endif


#define super IOService
OSDefineMetaClass ( IOSCSIParallelInterfaceController, IOService );
OSDefineAbstractStructors ( IOSCSIParallelInterfaceController, IOService );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kIOPropertySCSIInitiatorIdentifierKey		"SCSI Initiator Identifier"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Static initialization
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SInt32	IOSCSIParallelInterfaceController::fSCSIParallelDomainCount = 0;


#if 0
#pragma mark -
#pragma mark IOKit Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	handleOpen - Handles opens on the object						  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::handleOpen ( IOService *		client,
												IOOptionBits	options,
												void *			arg )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "+IOSCSIParallelInterfaceController::handleOpen\n" ) );
	
	result = fClients->setObject( client );
	
	STATUS_LOG ( ( "-IOSCSIParallelInterfaceController::handleOpen\n" ) );
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	handleClose - Handles closes on the object						  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::handleClose (
							IOService *		client,
							IOOptionBits	options )
{
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceController::handleClose\n" ) );
	fClients->removeObject ( client );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	handleIsOpen - Figures out if there are any opens on this object. [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::handleIsOpen ( const IOService * client ) const
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "+IOSCSIParallelInterfaceController::handleIsOpen\n" ) );

	// Are they asking if a specific object has us open?
	if ( client != NULL )
	{
		result = fClients->containsObject ( client );
	}
	
	// They're asking if we are open for any client
	else
	{
		result = ( fClients->getCount ( ) > 0 ) ? true : false;
	}
	
	STATUS_LOG ( ( "-IOSCSIParallelInterfaceController::handleIsOpen\n" ) );
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	start - Begins provided services.								  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::start ( IOService * provider )
{
	
	OSDictionary *	protocolDict 		= NULL;
	OSNumber *		initiator	 		= NULL;
	bool			result				= false;
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceController start.\n" ) );
	
	fSCSIDomainIdentifier = OSIncrementAtomic ( &fSCSIParallelDomainCount );
	
	result = super::start ( provider );
	require ( result, PROVIDER_START_FAILURE );
	
	require_nonzero ( provider, PROVIDER_START_FAILURE );
	
	result = provider->open ( this );
	require ( result, PROVIDER_START_FAILURE );
	
	fProvider					= provider;
	fWorkLoop					= NULL;
	fTimerEvent					= NULL;
	fDispatchEvent				= NULL;
	fControllerGate				= NULL;
	fOutstandingRequests 		= 0;
	fHBAHasBeenInitialized 		= false;
	fHBACanAcceptClientRequests = false;
	fClients					= OSSet::withCapacity ( 1 );
	
	fDeviceLock = IOSimpleLockAlloc ( );
	require_nonzero ( fDeviceLock, DEVICE_LOCK_ALLOC_FAILURE );
	
	result = CreateWorkLoop ( provider );
	require ( result, WORKLOOP_CREATE_FAILURE );
	
	// See if a protocol characteristics property already exists for the controller
	protocolDict = OSDynamicCast ( OSDictionary, getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( protocolDict == NULL )
	{
		
		// A Protocol Characteristics dictionary could not be retrieved, so one
		// will be created.		
		protocolDict = OSDictionary::withCapacity ( 1 );
		
	}
	
	else
	{
		
		// Since the object did not need to create the protocol 
		// dictionary, issue a retain to balance out the release that will be 
		// done later.
		protocolDict->retain ( );
		
	}
	
	if ( protocolDict != NULL )
	{
		
		OSString *		string = NULL;
		OSNumber *		domainID = NULL;
		
		// Set the Physical Interconnect property if it doesn't already exist
		string = OSDynamicCast ( OSString, getProperty ( kIOPropertyPhysicalInterconnectTypeKey ) );
		if ( string == NULL )
		{
			
			string = OSString::withCString ( kIOPropertySCSIParallelInterfaceKey );
			if ( string != NULL )
			{
				
				protocolDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, string );
				string->release ( );
				string = NULL;
				
			}
			
		}
		
		else
		{
			
			protocolDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, string );
			
		}
		
		string = OSDynamicCast ( OSString, getProperty ( kIOPropertyPhysicalInterconnectLocationKey ) );
		if ( string == NULL )
		{
			
			string = OSString::withCString ( kIOPropertyInternalExternalKey );
			if ( string != NULL )
			{
				
				protocolDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, string );
				string->release ( );
				string = NULL;
				
			}
			
		}
		
		else
		{
			
			protocolDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, string );
			
		}
		
		domainID = OSNumber::withNumber ( fSCSIDomainIdentifier, 32 );
		if ( domainID != NULL )
		{
			
			protocolDict->setObject ( kIOPropertySCSIDomainIdentifierKey, domainID );
			domainID->release ( );
			domainID = NULL;
			
		}
		
		setProperty ( kIOPropertyProtocolCharacteristicsKey, protocolDict );
		
		// Release it since we either called retain() or created it above.
		protocolDict->release ( );
		
	}
	
	// All the necessary preparation work has been done
	// for this superclass, now Initialize the chip driver.
	result = InitializeController ( );
	require ( result, INIT_CONTROLLER_FAILURE );
	
	// Retrieve the Initiator Identifier for this HBA.
	fInitiatorIdentifier = ReportInitiatorIdentifier ( );
	initiator = OSNumber::withNumber ( fInitiatorIdentifier, 64 );
	if ( initiator != NULL )
	{
		
		setProperty ( kIOPropertySCSIInitiatorIdentifierKey, initiator );
		initiator->release ( );
		initiator = NULL;
		
	}
	
	// Now that the controller has been succesfully initialized, retrieve the
	// necessary HBA specific information.
	fHighestSupportedDeviceID = ReportHighestSupportedDeviceID ( );
	
	// Set the Device List structure to an initial value
	InitializeDeviceList ( );
	
	fSupportedTaskCount = ReportMaximumTaskCount ( );
	
	// Allocate the SCSIParallelTasks and the pool
	result = AllocateSCSIParallelTasks ( );
	require ( result, TASK_ALLOCATE_FAILURE );
	
	// The HBA has been fully initialized and is now ready to provide
	// its services to the system.
	fHBAHasBeenInitialized = true;
	
	result = StartController ( );
	require ( result, START_CONTROLLER_FAILURE );
	
	// The controller is now ready to accept requests, set the flag so
	// that the commands will be accepted
	fHBACanAcceptClientRequests = true;
	
	// Enable interrupts for the work loop as the
	// HBA child class may need it to start the controller.
	fWorkLoop->enableAllInterrupts ( );
	
	// Now create SCSI Device objects
	result = DoesHBAPerformDeviceManagement ( );
	
	if ( result == false )
	{
		
		// This HBA does not support a mechansim for device attach/detach 
		// notification, go ahead and create target devices.
		for ( UInt32 index = 0; index <= fHighestSupportedDeviceID; index++ )
		{
			
			CreateTargetForID ( index );
			
		}
		
	}
	
	registerService ( );
	result = true;
	
	// The controller has been initialized and can accept requests.  Target 
	// devices have either been created, or the HBA will create them as needed.	
	return result;
	
	
START_CONTROLLER_FAILURE:	
	// START_CONTROLLER_FAILURE:
	// If execution jumped to this label, the HBA child class was unsuccessful
	// at starting its sevices.
	
	// First step is to release the allocated SCSI Parallel Tasks
	DeallocateSCSIParallelTasks ( );
	
	
TASK_ALLOCATE_FAILURE:
	// TASK_ALLOCATE_FAILURE:
	// If execution jumped to this label, SCSI Parallel Tasks failed to be 
	// allocated.
	
	// Since the HBA child class was initialized, it needs to be terminated.
	fHBAHasBeenInitialized = false;
	TerminateController ( );
	
	
INIT_CONTROLLER_FAILURE:
	// INIT_CONTROLLER_FAILURE:
	// If execution jumped to this label, the HBA child class failed its
	// initialization.
	
	// Release the workloop and associated objects.
	ReleaseWorkLoop ( );
	
	
WORKLOOP_CREATE_FAILURE:
	// WORKLOOP_CREATE_FAILURE:
	// If execution jumped to this label, the workloop or associated objects
	// could not be allocated.
	IOSimpleLockFree ( fDeviceLock );
	fDeviceLock = NULL;
	
	
DEVICE_LOCK_ALLOC_FAILURE:	
	// Call the superclass to stop.
	super::stop ( provider );
	
	
PROVIDER_START_FAILURE:
	// PROVIDER_START_FAILURE:
	// If execution jumped to this label, the Provider was not successfully
	// started, no cleanup needs to be done.
	
	// Since the start attempt was not successful, report that by 
	// returning false.
	return false;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	stop - Begins provided services.								  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::stop ( IOService * provider )
{
	
	// Prevent any new requests from being sent to the controller
	fHBACanAcceptClientRequests = false;
	
	// Check that there are no commands that are still pending
	// A pending command would be considered a SCSIParallelTask that
	// has been gotten, but has not been freed.
	
	// Destroy all of the Target Device objects.  Doing this before stopping the
	// controller prevents any clients from trying to access the controller 
	// after being stopped.
	
	// Halt all services from the subclass
	StopController ( );
	
	// Halt the reception of interrupts.
	fDispatchEvent->disable ( );
	
	fHBAHasBeenInitialized = false;
	
	// Inform the subclass to terminate all allocated resources
	TerminateController ( );
	
	// Free all of the SCSIParallelTasks and the pool.
	DeallocateSCSIParallelTasks ( );
	
	// Release all WorkLoop related resources
	ReleaseWorkLoop ( );
	
	if ( fDeviceLock != NULL )
	{
		
		IOSimpleLockFree ( fDeviceLock );
		fDeviceLock = NULL;
		
	}
	
	super::stop ( provider );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	getWorkLoop - Gets the workloop.								  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOWorkLoop *
IOSCSIParallelInterfaceController::getWorkLoop ( void ) const
{
	return GetWorkLoop ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetProvider - Gets the provider object.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOService *
IOSCSIParallelInterfaceController::GetProvider ( void )
{
	return fProvider;
}


#if 0
#pragma mark -
#pragma mark WorkLoop Management
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetWorkLoop - Gets the workloop.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOWorkLoop *
IOSCSIParallelInterfaceController::GetWorkLoop ( void ) const
{
	return fWorkLoop;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetCommandGate - Gets the command gate.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOCommandGate *
IOSCSIParallelInterfaceController::GetCommandGate ( void )
{
	return fControllerGate;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateWorkLoop - Creates the workloop and associated objects.	  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::CreateWorkLoop ( IOService * provider )
{
	
	bool		result = false;
	IOReturn	status = kIOReturnSuccess;
	
	if ( fWorkLoop == NULL )
	{
		
		fWorkLoop = IOWorkLoop::workLoop ( );
		require_nonzero ( fWorkLoop, CREATE_WORKLOOP_FAILURE );
		
	}
	
	fTimerEvent = SCSIParallelTimer::CreateTimerEventSource ( this,
			( IOTimerEventSource::Action ) &IOSCSIParallelInterfaceController::TimeoutOccurred );
	require_nonzero ( fTimerEvent, TIMER_CREATION_FAILURE );
	
	status = fWorkLoop->addEventSource ( fTimerEvent );
	require_success ( status, ADD_TES_FAILURE );
	
	fDispatchEvent = IOInterruptEventSource::interruptEventSource (
						this,
						&IOSCSIParallelInterfaceController::ServiceInterrupt,
						provider,
						0 );
	
	require_nonzero ( fDispatchEvent, CREATE_ISR_EVENT_FAILURE );
	
	status = fWorkLoop->addEventSource( fDispatchEvent );
	require_success ( status, ADD_ISR_EVENT_FAILURE );
	
	fControllerGate = IOCommandGate::commandGate ( this, NULL );
	require_nonzero ( fControllerGate, ALLOCATE_COMMAND_GATE_FAILURE );
	
	status = fWorkLoop->addEventSource ( fControllerGate );
	require_success ( status,  ADD_GATE_EVENT_FAILURE );
	
	result = true;
	
	return result;
	
	
ADD_GATE_EVENT_FAILURE:
	
	
	require_nonzero_quiet ( fControllerGate, ALLOCATE_COMMAND_GATE_FAILURE );
	fControllerGate->release ( );
	fControllerGate = NULL;
	
	
ALLOCATE_COMMAND_GATE_FAILURE:
	
	
	fWorkLoop->removeEventSource ( fDispatchEvent );
	
	
ADD_ISR_EVENT_FAILURE:
	
	
	require_nonzero_quiet ( fDispatchEvent, CREATE_ISR_EVENT_FAILURE );
	fDispatchEvent->release ( );
	fDispatchEvent = NULL;
	
	
CREATE_ISR_EVENT_FAILURE:
	
	
	fWorkLoop->removeEventSource ( fTimerEvent );
	
	
ADD_TES_FAILURE:
	
	
	require_nonzero_quiet ( fTimerEvent, TIMER_CREATION_FAILURE );
	fDispatchEvent->release ( );
	fDispatchEvent = NULL;
	
	
TIMER_CREATION_FAILURE:
	
	
	require_nonzero_quiet ( fWorkLoop, CREATE_WORKLOOP_FAILURE );
	fWorkLoop->release ( );
	fWorkLoop = NULL;
	
	
CREATE_WORKLOOP_FAILURE:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	ReleaseWorkLoop - Releases the workloop and associated objects.	  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::ReleaseWorkLoop ( void )
{
	
	if ( fControllerGate != NULL )
	{
		
		fWorkLoop->removeEventSource ( fControllerGate );
		fControllerGate->release ( );
		fControllerGate = NULL;
		
	}
	
	if ( fTimerEvent != NULL ) 	
	{
		
		fTimerEvent->release ( );
		fTimerEvent = NULL;
		
	}
	
	if ( fDispatchEvent != NULL )   
	{
		
		fWorkLoop->removeEventSource ( fDispatchEvent );
		fDispatchEvent->release ( );
		fDispatchEvent = NULL;
		
	}
	
	if ( fWorkLoop != NULL )
	{
		
		fWorkLoop->release ( );
		fWorkLoop = NULL;
		
	}
	
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Task Management
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetSCSIParallelTask - Gets a parallel task from the pool.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelTaskIdentifier
IOSCSIParallelInterfaceController::GetSCSIParallelTask ( bool blockForCommand )
{
	
	SCSIParallelTask *		parallelTask = NULL;
	
	parallelTask = ( SCSIParallelTask * ) fParallelTaskPool->getCommand ( blockForCommand );
	if ( parallelTask != NULL )
	{
		parallelTask->ResetForNewTask ( );
	}
	
	return ( SCSIParallelTaskIdentifier ) parallelTask;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FreeSCSIParallelTask - Returns a parallel task to the pool.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::FreeSCSIParallelTask (
							SCSIParallelTaskIdentifier returnTask )
{
	fParallelTaskPool->returnCommand ( ( IOCommand * ) returnTask );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AllocateSCSIParallelTasks - Allocates parallel tasks for the pool.
//																	  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::AllocateSCSIParallelTasks ( void )
{
	
	bool				result			= false;
	SCSIParallelTask *	parallelTask 	= NULL;
	UInt32				taskSize		= ReportHBASpecificTaskDataSize ( );
	
	fParallelTaskPool = IOCommandPool::withWorkLoop ( fWorkLoop );
	require_nonzero ( fParallelTaskPool, POOL_CREATION_FAILURE );
	
	// As long as a single SCSI Parallel Task can be allocated, the HBA
	// can function.  Check to see if the first one can be allocated.
	parallelTask = SCSIParallelTask::Create ( taskSize );
	require_nonzero ( parallelTask, TASK_CREATION_FAILURE );
	
	// Send the single command into the pool.
	fParallelTaskPool->returnCommand ( parallelTask );
	
	// Now try to allocate the remaining Tasks that the HBA reports that it
	// can support.
	for ( UInt32 index = 1; index < fSupportedTaskCount; index++ )
	{
		
		// Allocate the command with enough space for the HBA specific data
		parallelTask = SCSIParallelTask::Create ( taskSize );
		if ( parallelTask != NULL )
		{
			
			// Send the next command into the pool.
			fParallelTaskPool->returnCommand ( parallelTask );
			
		}
		
	}
	
	// Since at least a single SCSI Parallel Task was allocated, this
	// HBA can function.
	result = true;
	
	return result;
	
	
TASK_CREATION_FAILURE:
	
	
	require_nonzero ( fParallelTaskPool, POOL_CREATION_FAILURE );
	fParallelTaskPool->release ( );
	fParallelTaskPool = NULL;
	
	
POOL_CREATION_FAILURE:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	DeallocateSCSIParallelTasks - Deallocates parallel tasks in the pool.
//																	  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::DeallocateSCSIParallelTasks ( void )
{
	
	SCSIParallelTask *	parallelTask = NULL;
	
	require_nonzero ( fParallelTaskPool, Exit );
	
	parallelTask = ( SCSIParallelTask * ) fParallelTaskPool->getCommand ( false );
	while ( parallelTask != NULL )
	{
		
		parallelTask->release ( );
		parallelTask = ( SCSIParallelTask * ) fParallelTaskPool->getCommand ( false );
		
	}
	
	fParallelTaskPool->release ( );
	fParallelTaskPool = NULL;
	
	
Exit:
	
	
	return;
	
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Task Execution
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	ExecuteParallelTask - Executes a parallel task.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse 
IOSCSIParallelInterfaceController::ExecuteParallelTask ( 
							SCSIParallelTaskIdentifier 			parallelRequest )
{
	
	// If the controller has requested a suspend,
	// return the command and let it add it back to the queue.
	if ( fHBACanAcceptClientRequests == false )
	{
	}
	
	// If the controller has not suspended, send the task now
	return ProcessParallelTask ( parallelRequest );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompleteParallelTask - Completes a parallel task.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void	
IOSCSIParallelInterfaceController::CompleteParallelTask ( 
							SCSIParallelTaskIdentifier 	parallelRequest,
							SCSITaskStatus	 			completionStatus,
							SCSIServiceResponse 		serviceResponse )
{
	
	IOSCSIParallelInterfaceDevice *		target = NULL;
	
	STATUS_LOG ( ( "+IOSCSIParallelInterfaceController::CompleteParallelTask\n" ) );
	
	target = GetTargetForID ( GetTargetIdentifier ( parallelRequest ) );
	require_nonzero ( target, Exit );	
	
	// Complete the command
	target->CompleteSCSITask (	parallelRequest, 
								serviceResponse, 
								completionStatus );
	
Exit:
	
	
	STATUS_LOG ( ( "-IOSCSIParallelInterfaceController::CompleteParallelTask\n" ) );
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FindTaskForAddress - Finds a task by its address (ITLQ nexus)	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelTaskIdentifier
IOSCSIParallelInterfaceController::FindTaskForAddress ( 
							SCSIDeviceIdentifier 		theT,
							SCSILogicalUnitNumber		theL,
							SCSITaggedTaskIdentifier	theQ )
{
	
	SCSIParallelTaskIdentifier			task	= NULL;
	IOSCSIParallelInterfaceDevice *		target 	= NULL;
	
	target = GetTargetForID ( theT );
	require_nonzero ( target, Exit );
	
	// A valid object exists for the target ID, request that it find
	// the task on its outstanding queue.
	task = target->FindTaskForAddress ( theL, theQ );
	
	
Exit:
	
	
	return task;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FindTaskForControllerIdentifier - Finds a task by its unique ID	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelTaskIdentifier	
IOSCSIParallelInterfaceController::FindTaskForControllerIdentifier ( 
							SCSIDeviceIdentifier 		theTarget,
							UInt64						theIdentifier )
{
	
	SCSIParallelTaskIdentifier			task	= NULL;
	IOSCSIParallelInterfaceDevice *		target 	= NULL;
	
	target = GetTargetForID ( theTarget );
	require_nonzero ( target, Exit );
	
	// A valid object exists for the target ID, request that it find
	// the task on its outstanding queue.
	task = target->FindTaskForControllerIdentifier ( theIdentifier );
	
	
Exit:
	
	
	return task;
	
}

// The completion callback for the SAM-2 Task Management functions.
// The implementation for these will be added when the support for these
// are added in the SCSI Parallel Interface Device object

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompleteAbortTask - Completes the AbortTaskRequest.	 			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::CompleteAbortTask ( 	
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSITaggedTaskIdentifier	theQ,
					SCSIServiceResponse 		serviceResponse )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompleteAbortTaskSet - Completes the AbortTaskSetRequest.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::CompleteAbortTaskSet (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompleteClearACA - Completes the ClearACARequest.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::CompleteClearACA (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompleteClearTaskSet - Completes the ClearTaskSetRequest.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::CompleteClearTaskSet (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompleteLogicalUnitReset - Completes the LogicalUnitResetRequest.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::CompleteLogicalUnitReset (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CompleteTargetReset - Completes the TargetResetRequest  		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::CompleteTargetReset (
					SCSITargetIdentifier 		theT,
					SCSIServiceResponse 		serviceResponse )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	ServiceInterrupt - Calls the registered interrupt handler. 		  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::ServiceInterrupt ( 
							OSObject *					theObject, 
							IOInterruptEventSource *	theSource,
							int							count  )
{
	( ( IOSCSIParallelInterfaceController * ) theObject )->HandleInterruptRequest ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	EnableInterrupt - Enables the interrupt event source.	  		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::EnableInterrupt ( void )
{
	fDispatchEvent->enable ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	DisableInterrupt - Disables the interrupt event source.	  		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::DisableInterrupt ( void )
{
	fDispatchEvent->disable ( );
}


#if 0
#pragma mark -
#pragma mark Timeout Management
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetTimeoutForTask - Sets the timeout.					  		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::SetTimeoutForTask (
							SCSIParallelTaskIdentifier 		parallelTask,
							UInt32							timeoutOverride )
{
	
	( ( SCSIParallelTimer * ) fTimerEvent )->SetTimeout (	parallelTask,
															timeoutOverride );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	TimeoutOccurred - Calls the timeout handler.			  		  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::TimeoutOccurred ( 
							OSObject *					theObject, 
							IOTimerEventSource * 		theSender )
{
	
	SCSIParallelTimer *		timer		= OSDynamicCast ( SCSIParallelTimer, theSender );
	SCSIParallelTask *		expiredTask	= NULL;
	
	if ( timer != NULL )
	{
		
		expiredTask = timer->GetExpiredTask ( );
		while ( expiredTask != NULL )
		{
			
			( ( IOSCSIParallelInterfaceController * ) theObject )->HandleTimeout ( expiredTask );
			expiredTask = timer->GetExpiredTask ( );
			
		}
		
		// Rearm the timer
		timer->Rearm ( );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	HandleTimeout - Generic timeout handler. Subclasses should override.
//															  		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::HandleTimeout (
						SCSIParallelTaskIdentifier			parallelRequest )
{
	
	check ( parallelRequest != NULL );
	CompleteParallelTask ( 	parallelRequest,
							kSCSITaskStatus_No_Status,
							kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
	
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Device Management
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CreateTargetForID - Creates a target device for the ID specified.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::CreateTargetForID (
										SCSITargetIdentifier	targetID )
{
	
	IOSCSIParallelInterfaceDevice *		newDevice 	= NULL;
	bool								result		= false;
	
	// Verify that the device ID is not that of the initiator.
	require ( ( targetID != fInitiatorIdentifier ), INVALID_PARAMETER_EXIT );
	
	// First check to see if this device already exists
	require ( ( GetTargetForID ( targetID ) == NULL ), INVALID_PARAMETER_EXIT );
	
	// Create the IOSCSIParallelInterfaceDevice object
	newDevice = IOSCSIParallelInterfaceDevice::CreateTarget (
									targetID,
									ReportHBASpecificDeviceDataSize ( ) );
	require_nonzero ( newDevice, DEVICE_CREATION_FAILED_EXIT );
	
	AddDeviceToTargetList ( newDevice );
	
	// Attach the device
	result = newDevice->init ( 0 );
	require ( result, ATTACH_FAILED_EXIT );
	
	result = newDevice->attach ( this );
	require ( result, ATTACH_FAILED_EXIT );
	
	result = newDevice->start ( this );
	require ( result, START_FAILED_EXIT );
	
	newDevice->release ( );
	
	
	// The SCSI Device was successfully created.
	result = true;
	
	return result;
	
	
START_FAILED_EXIT:
	
	
	// Detach the target device
	newDevice->detach ( this );
	
	
ATTACH_FAILED_EXIT:	
	
	
	RemoveDeviceFromTargetList ( newDevice );
	
	// The device can now be destroyed.
	newDevice->DestroyTarget ( );
	
	
DEVICE_CREATION_FAILED_EXIT:
INVALID_PARAMETER_EXIT:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	DestroyTargetForID - Destroys a target device for the ID specified.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::DestroyTargetForID (
									SCSITargetIdentifier		targetID )
{
	
	IOSCSIParallelInterfaceDevice *		victimDevice = NULL;
	
	victimDevice = GetTargetForID ( targetID );
	if ( victimDevice == NULL )
	{
		
		// There is no object for this target in the device list,
		// so return without doing anything.
		return;
		
	}
	
	// Remove the IOSCSIParallelInterfaceDevice from the device list
	RemoveDeviceFromTargetList ( victimDevice );
	
	// The device can now be destroyed.
	victimDevice->DestroyTarget ( );
	victimDevice->terminate ( );
	
}


#if 0
#pragma mark -
#pragma mark Device List Management
#pragma mark -
#endif

/*
 * The following member routines are used to manage the array of linked lists, the Device
 * List, that allow quick access to the SCSI Parallel Device objects.  These routines
 * have intricate knowledge about the layout of the Device List since they are responsible
 * for managing it and so they are the only ones that are allowed to directly access that 
 * structure.  Any other routine that needs to retrieve an element from the Device List 
 * must use these routines to obtain it so that if necessity causes to the Device List
 * structure to change, they are not broken.
 */


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	InitializeDeviceList - Initializes device list.					  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::InitializeDeviceList ( void )
{
	
	// Initialize the SCSI Parallel Device Array to all NULL pointers
	for ( UInt32 i = 0; i < kSCSIParallelDeviceListArrayCount; i++ )
	{
		fParallelDeviceList[i] = NULL;
	}
	
}		


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetTargetForID - Gets the device object for the specified targetID.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOSCSIParallelInterfaceDevice *
IOSCSIParallelInterfaceController::GetTargetForID ( SCSITargetIdentifier targetID )
{
	
	IOSCSIParallelInterfaceDevice *		device		= NULL;
	IOInterruptState					lockState	= 0;
	UInt8								indexID		= 0;
	
	require ( ( targetID >= 0 ), INVALID_PARAMETER_FAILURE );
	require ( ( targetID <= fHighestSupportedDeviceID ), INVALID_PARAMETER_FAILURE );
	require ( ( targetID != fInitiatorIdentifier ), INVALID_PARAMETER_FAILURE );
	
	lockState = IOSimpleLockLockDisableInterrupt ( fDeviceLock );
	
	// Since the array is made up of 16 elements, do a bitwize and with the 
	// targetID to get the array index.
	indexID = targetID & kSCSIParallelDeviceListIndexMask;
	
	// Walk the list for the indexID
	device = fParallelDeviceList[indexID];
	
	while ( device != NULL )
	{
		
		if ( device->GetTargetIdentifier ( ) == targetID )
		{
			
			// This is the device in which the client is interested, break
			// so that the current pointer will be returned.
			break;
			
		}
		
		// Get the next element in the list
		device = device->GetNextDeviceInList ( );
		
	}
	
	IOSimpleLockUnlockEnableInterrupt ( fDeviceLock, lockState );
	
	
INVALID_PARAMETER_FAILURE:
	
	
	return device;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	AddDeviceToTargetList - Adds a device to the target list.		  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::AddDeviceToTargetList (
							IOSCSIParallelInterfaceDevice *				newDevice )
{
	
	UInt8					indexID		= 0;
	IOInterruptState		lockState	= 0;
	
	STATUS_LOG ( ( "+IOSCSIParallelInterfaceController::AddDeviceToTargetList\n" ) );
	
	lockState = IOSimpleLockLockDisableInterrupt ( fDeviceLock );
	
	// Since the array is made up of 16 elements, do a bitwize and with the 
	// targetID to get the array index.
	indexID = newDevice->GetTargetIdentifier ( ) & kSCSIParallelDeviceListIndexMask;
	
	// Set the pointer in the SCSI Device array
	if ( fParallelDeviceList[indexID] == NULL )
	{
		
		// This is the first device object created for this
		// index, set the array pointer to the new object. 
		fParallelDeviceList[indexID] = newDevice;
		newDevice->SetNextDeviceInList ( NULL );
		newDevice->SetPreviousDeviceInList ( NULL );
		
	}
	
	else
	{
		
		// This is not the first device object for this index, 
		// walk the list at this index and add it to the end.
		IOSCSIParallelInterfaceDevice *	currentDevice;
		
		currentDevice = fParallelDeviceList[indexID];
		while ( currentDevice->GetNextDeviceInList ( ) != NULL )
		{
			currentDevice = currentDevice->GetNextDeviceInList ( );
		}
		
		currentDevice->SetNextDeviceInList ( newDevice );
		newDevice->SetNextDeviceInList ( NULL );
		newDevice->SetPreviousDeviceInList ( currentDevice );
		
	}
	
	IOSimpleLockUnlockEnableInterrupt ( fDeviceLock, lockState );
	
	STATUS_LOG ( ( "-IOSCSIParallelInterfaceController::AddDeviceToTargetList\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	RemoveDeviceFromTargetList - Adds a device to the target list.	  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::RemoveDeviceFromTargetList (
							IOSCSIParallelInterfaceDevice * 	victimDevice )
{
	
	IOSCSIParallelInterfaceDevice *		nextDevice	= NULL;
	IOSCSIParallelInterfaceDevice *		prevDevice	= NULL;
	IOInterruptState					lockState	= 0;
	
	lockState = IOSimpleLockLockDisableInterrupt ( fDeviceLock );
	
	nextDevice = victimDevice->GetNextDeviceInList ( );
	prevDevice = victimDevice->GetPreviousDeviceInList ( );
	
	if ( prevDevice != NULL )
	{
		
		// There is a previous device, set it to the victim's next device
		prevDevice->SetNextDeviceInList ( nextDevice );
		
	}
	
	else
	{
		
		// There is not a previous device, set the pointer in the array
		// to the victim's next device.
		UInt8	indexID = 0;
		
		// Since the array is made up of 16 elements, do a bitwize and with the 
		// targetID to get the array index.
		indexID = victimDevice->GetTargetIdentifier ( ) & kSCSIParallelDeviceListIndexMask;
		
		// Set the Device List element to point at the device object that was following
		// the device object that is being removed.  If there was no next device, 
		// the nextDevice pointer will be NULL causing the Device List no longer have
		// any devices at this index.
		fParallelDeviceList[indexID] = nextDevice;
		
	}
	
	if ( nextDevice != NULL )
	{
		
		// The next device is not NULL, set it to the victim's previous
		nextDevice->SetPreviousDeviceInList ( prevDevice );
		
	}
	
	// Clear out the victim's previous and next pointers
	victimDevice->SetNextDeviceInList ( NULL );
	victimDevice->SetPreviousDeviceInList ( NULL );
	
	IOSimpleLockUnlockEnableInterrupt ( fDeviceLock, lockState );
	
}


#if 0
#pragma mark -
#pragma mark Controller Child Class 
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SuspendServices - Suspends services temporarily.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::SuspendServices ( void )
{
	
	// The HBA child class has requested the suspension of tasks.
	fHBACanAcceptClientRequests = false;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	ResumeServices - Resume services temporarily.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::ResumeServices ( void )
{
	
	// The HBA child class has allowed the submission of tasks.
	fHBACanAcceptClientRequests = true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	NotifyClientsOfBusReset - Notifies clients of bus resets.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::NotifyClientsOfBusReset ( void )
{
	messageClients ( kSCSIControllerNotificationBusReset );
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Task Object Accessors
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetSCSITaskIdentifier - Gets SCSITaskIdentifier for task.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaskIdentifier
IOSCSIParallelInterfaceController::GetSCSITaskIdentifier (
							SCSIParallelTaskIdentifier 	parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return NULL;
	}
	
	return tempTask->GetSCSITaskIdentifier ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetTargetIdentifier - Gets SCSITargetIdentifier for task.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITargetIdentifier
IOSCSIParallelInterfaceController::GetTargetIdentifier (
							SCSIParallelTaskIdentifier 	parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return NULL;
	}
	
	return tempTask->GetTargetIdentifier ( );
	
}


// ---- Methods for Accessing data in the client's SCSI Task Object ----	
// Method to retrieve the LUN that identifies the Logical Unit whose Task
// Set to which this task is to be added.
// --> Currently this only supports Level 1 Addressing, complete
// Hierachal LUN addressing will need to be added to the SCSI Task object
// and the Peripheral Device Type objects which will represent Logical Units.
// Since that will be completed before this is released, this method will be
// changed at that time.


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetLogicalUnitNumber - Gets SCSILogicalUnitNumber for task.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSILogicalUnitNumber
IOSCSIParallelInterfaceController::GetLogicalUnitNumber (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetLogicalUnitNumber ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetTaggedTaskIdentifier - Gets SCSITaggedTaskIdentifier for task.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaggedTaskIdentifier
IOSCSIParallelInterfaceController::GetTaggedTaskIdentifier (
							SCSIParallelTaskIdentifier		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return kSCSIUntaggedTaskIdentifier;
	}
	
	return tempTask->GetTaggedTaskIdentifier ( );
	
}
							

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetTaskAttribute - Gets SCSITaskAttribute for task. 			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaskAttribute
IOSCSIParallelInterfaceController::GetTaskAttribute (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return kSCSITask_SIMPLE;
	}
	
	return tempTask->GetTaskAttribute ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetCommandDescriptorBlockSize - Gets CDB size for task. 		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
IOSCSIParallelInterfaceController::GetCommandDescriptorBlockSize (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetCommandDescriptorBlockSize ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetCommandDescriptorBlock - This will always return a 16 Byte CDB.
//														 			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::GetCommandDescriptorBlock (
							SCSIParallelTaskIdentifier 		parallelTask,
							SCSICommandDescriptorBlock * 	cdbData )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return false;
	}
	
	return tempTask->GetCommandDescriptorBlock ( cdbData );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetDataTransferDirection - Gets data transfer direction.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
IOSCSIParallelInterfaceController::GetDataTransferDirection (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return kSCSIDataTransfer_NoDataTransfer;
	}
	
	return tempTask->GetDataTransferDirection ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetRequestedDataTransferCount - Gets requested transfer count.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIParallelInterfaceController::GetRequestedDataTransferCount ( 
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetRequestedDataTransferCount ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetRealizedDataTransferCount - Gets realized transfer count.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIParallelInterfaceController::GetRealizedDataTransferCount (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetRealizedDataTransferCount ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetRealizedDataTransferCount - Sets realized transfer count.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::SetRealizedDataTransferCount (
				SCSIParallelTaskIdentifier 		parallelTask,
				UInt64 							realizedTransferCountInBytes )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return false;
	}
	
	return tempTask->SetRealizedDataTransferCount ( realizedTransferCountInBytes );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	IncrementRealizedDataTransferCount - Adjusts realized transfer count.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::IncrementRealizedDataTransferCount (
				SCSIParallelTaskIdentifier 		parallelTask,
				UInt64 							realizedTransferCountInBytes )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return;
	}
	
	return tempTask->IncrementRealizedDataTransferCount ( realizedTransferCountInBytes );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetDataBuffer - Gets data buffer associated with this task.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOMemoryDescriptor *
IOSCSIParallelInterfaceController::GetDataBuffer (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return NULL;
	}
	
	return tempTask->GetDataBuffer ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetDataBufferOffset - Gets data buffer offset associated with this task.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIParallelInterfaceController::GetDataBufferOffset (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetDataBufferOffset ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetTimeoutDuration - 	Gets timeout duration in milliseconds associated.
//							with this task.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIParallelInterfaceController::GetTimeoutDuration (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetTimeoutDuration ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetAutoSenseData - 	Sets autosense data in task.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::SetAutoSenseData (
							SCSIParallelTaskIdentifier 		parallelTask,
							SCSI_Sense_Data * 				newSenseData,
							UInt8							senseDataSize )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return false;
	}
	
	return tempTask->SetAutoSenseData ( newSenseData, senseDataSize );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetAutoSenseData - 	Gets autosense data in task.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIParallelInterfaceController::GetAutoSenseData (
 							SCSIParallelTaskIdentifier 		parallelTask,
 							SCSI_Sense_Data * 				receivingBuffer,
 							UInt8							senseDataSize )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return false;
	}
	
	return tempTask->GetAutoSenseData ( receivingBuffer, senseDataSize );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetAutoSenseDataSize - 	Gets autosense data size.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt8
IOSCSIParallelInterfaceController::GetAutoSenseDataSize(
 							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetAutoSenseDataSize ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetSCSIParallelFeatureNegotiation - Gets SCSIParallelFeatureRequest status
//										for specified feature.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelFeatureRequest
IOSCSIParallelInterfaceController::GetSCSIParallelFeatureNegotiation ( 
							SCSIParallelTaskIdentifier 		parallelTask,
							SCSIParallelFeature 			requestedFeature )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return kSCSIParallelFeature_NoNegotiation;
	}
	
	return tempTask->GetSCSIParallelFeatureNegotiation ( requestedFeature );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetSCSIParallelFeatureNegotiationCount - 	Gets SCSIParallelFeatureRequest
//												count.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIParallelInterfaceController::GetSCSIParallelFeatureNegotiationCount ( 
							SCSIParallelTaskIdentifier 		parallelTask)
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetSCSIParallelFeatureNegotiationCount ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetSCSIParallelFeatureNegotiationResult - 	Sets SCSIParallelFeatureResult
//												status for specified feature.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIParallelInterfaceController::SetSCSIParallelFeatureNegotiationResult (
							SCSIParallelTaskIdentifier 		parallelTask,
							SCSIParallelFeature 			requestedFeature, 
							SCSIParallelFeatureResult 		newResult )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return;
	}
	
	return tempTask->SetSCSIParallelFeatureNegotiationResult ( requestedFeature, newResult );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetSCSIParallelFeatureNegotiationResult - 	Gets SCSIParallelFeatureResult
//												for requested feature.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIParallelFeatureResult
IOSCSIParallelInterfaceController::GetSCSIParallelFeatureNegotiationResult (
							SCSIParallelTaskIdentifier 	parallelTask,
							SCSIParallelFeature 		requestedFeature )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return kSCSIParallelFeature_NegotitiationUnchanged;
	}
	
	return tempTask->GetSCSIParallelFeatureNegotiationResult ( requestedFeature );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetSCSIParallelFeatureNegotiationResultCount - 	Gets SCSIParallelFeatureResult
//													count.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIParallelInterfaceController::GetSCSIParallelFeatureNegotiationResultCount (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetSCSIParallelFeatureNegotiationResultCount ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SetControllerTaskIdentifier - Sets the unique identifier for the task.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void	
IOSCSIParallelInterfaceController::SetControllerTaskIdentifier ( 
							SCSIParallelTaskIdentifier 		parallelTask,
							UInt64 							newIdentifier )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return;
	}
	
	return tempTask->SetControllerTaskIdentifier ( newIdentifier );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetControllerTaskIdentifier - Gets the unique identifier for the task.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64	
IOSCSIParallelInterfaceController::GetControllerTaskIdentifier (
							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetControllerTaskIdentifier ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetHBADataSize - Gets size of HBA data allocated in the task.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIParallelInterfaceController::GetHBADataSize (
							SCSIParallelTaskIdentifier 	parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetHBADataSize ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetHBADataPointer - Gets pointer to HBA data.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void *
IOSCSIParallelInterfaceController::GetHBADataPointer ( 
							SCSIParallelTaskIdentifier 	parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return NULL;
	}
	
	return tempTask->GetHBADataPointer ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetHBADataDescriptor - Gets IOMemoryDescriptor for HBA data.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOMemoryDescriptor *
IOSCSIParallelInterfaceController::GetHBADataDescriptor ( 
							SCSIParallelTaskIdentifier 	parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return NULL;
	}
	
	return tempTask->GetHBADataDescriptor ( );
	
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Device Object Accessors
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetHBATargetDataSize - Gets size of HBA data for the target.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIParallelInterfaceController::GetHBATargetDataSize (
								SCSITargetIdentifier 	targetID )
{
	
	IOSCSIParallelInterfaceDevice *	targetDevice;
	
	targetDevice = GetTargetForID ( targetID );
	if ( targetDevice == NULL )
	{
		return 0;
	}
	
	return targetDevice->GetHBADataSize ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetHBATargetDataPointer - Gets size of HBA data for the target.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void *
IOSCSIParallelInterfaceController::GetHBATargetDataPointer (
								SCSITargetIdentifier 	targetID )
{
	
	IOSCSIParallelInterfaceDevice *	targetDevice;
	
	targetDevice = GetTargetForID ( targetID );
	if ( targetDevice == NULL )
	{
		return NULL;
	}
	
	return targetDevice->GetHBADataPointer ( );
	
}


#if 0
#pragma mark -
#pragma mark VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  1 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  2 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  3 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  4 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  5 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  8 );

OSMetaClassDefineReservedUsed ( IOSCSIParallelInterfaceController,  9 );		// Used for HandleTimeout

OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 16 );