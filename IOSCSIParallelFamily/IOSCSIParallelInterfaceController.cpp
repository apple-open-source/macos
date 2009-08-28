/*
 * Copyright (c) 2002-2007 Apple Inc. All rights reserved.
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


// SCSI Parallel Family includes
#include "IOSCSIParallelInterfaceController.h"
#include "IOSCSIParallelInterfaceDevice.h"
#include "SCSIParallelTask.h"
#include "SCSIParallelTimer.h"
#include "SCSIParallelWorkLoop.h"

// Libkern includes
#include <libkern/OSAtomic.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>

// Generic IOKit includes
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------


#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SPI Controller"

#if DEBUG
#define SCSI_PARALLEL_INTERFACE_CONTROLLER_DEBUGGING_LEVEL	0
#endif

#include "IOSCSIParallelFamilyDebugging.h"

#if ( SCSI_PARALLEL_INTERFACE_CONTROLLER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
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


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

#define kIOPropertySCSIInitiatorManagesTargets		"Manages Targets"
#define kIOPropertyControllerCharacteristicsKey		"Controller Characteristics"
#define kIOPropertyDeviceTreeEntryKey				"IODeviceTreeEntry"

enum
{
	kWorldWideNameDataSize 				= 8,
	kAddressIdentifierDataSize 			= 3,
	kALPADataSize						= 1
};

enum
{
	kPhysicalInterconnectDictionaryEntryCount	= 3,
	kHBAContraintsDictionaryEntryCount			= 7
};


//-----------------------------------------------------------------------------
//	Static initialization
//-----------------------------------------------------------------------------

SInt32	IOSCSIParallelInterfaceController::fSCSIParallelDomainCount = 0;


//-----------------------------------------------------------------------------
//	Prototypes
//-----------------------------------------------------------------------------

static IOSCSIParallelInterfaceDevice *
GetDevice (	SCSIParallelTaskIdentifier 	parallelTask );

static void
CopyProtocolCharacteristicsProperties ( OSDictionary * dict, IOService * service );


#if 0
#pragma mark -
#pragma mark IOKit Member Routines
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	handleOpen - Handles opens on the object						  [PRIVATE]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::handleOpen ( IOService *		client,
												IOOptionBits	options,
												void *			arg )
{
	
	bool	result = false;
	
	STATUS_LOG ( ( "+IOSCSIParallelInterfaceController::handleOpen\n" ) );
	
	result = fClients->setObject ( client );
	
	STATUS_LOG ( ( "-IOSCSIParallelInterfaceController::handleOpen\n" ) );
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	handleClose - Handles closes on the object						  [PRIVATE]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::handleClose (
							IOService *		client,
							IOOptionBits	options )
{
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceController::handleClose\n" ) );
	fClients->removeObject ( client );
	
}


//-----------------------------------------------------------------------------
//	handleIsOpen - Figures out if there are any opens on this object. [PRIVATE]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	start - Begins provided services.								  [PRIVATE]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::start ( IOService * provider )
{
	
	OSDictionary *	dict		= NULL;
	OSDictionary *	copyDict	= NULL;
	OSNumber *		number		= NULL;
	bool			result		= false;
	
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
	fHBAHasBeenInitialized 		= false;
	fHBACanAcceptClientRequests = false;
	fClients					= OSSet::withCapacity ( 1 );
	
	fDeviceLock = IOSimpleLockAlloc ( );
	require_nonzero ( fDeviceLock, DEVICE_LOCK_ALLOC_FAILURE );
	
	result = CreateWorkLoop ( provider );
	require ( result, WORKLOOP_CREATE_FAILURE );
	
	dict = OSDictionary::withCapacity ( 1 );
	require_nonzero ( dict, CONTROLLER_DICT_FAILURE );
	
	setProperty ( kIOPropertyControllerCharacteristicsKey, dict );
	dict->release ( );
	dict = NULL;
	
	// See if a protocol characteristics property already exists for the controller
	copyDict = OSDynamicCast ( OSDictionary, copyProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( copyDict != NULL )
	{
		
		// Create a deep copy of the dictionary.
		dict = ( OSDictionary * ) copyDict->copyCollection ( );
		copyDict->release ( );
		
	}
	
	else
	{
		
		// A Protocol Characteristics dictionary could not be retrieved, so one
		// will be created.		
		dict = OSDictionary::withCapacity ( kPhysicalInterconnectDictionaryEntryCount );
		
	}
	
	if ( dict != NULL )
	{
		
		OSNumber *	number = NULL;
		
		// Copy any relevant properties from the IOService into the dictionary.
		CopyProtocolCharacteristicsProperties ( dict, this );
		
		// Set the domain ID.
		number = OSNumber::withNumber ( fSCSIDomainIdentifier, 32 );
		if ( number != NULL )
		{
			
			dict->setObject ( kIOPropertySCSIDomainIdentifierKey, number );
			number->release ( );
			number = NULL;
			
		}
		
		setProperty ( kIOPropertyProtocolCharacteristicsKey, dict );
		
		// Release it since we created it.
		dict->release ( );
		
	}
	
	// All the necessary preparation work has been done
	// for this superclass, now Initialize the chip driver.
	result = InitializeController ( );
	require ( result, INIT_CONTROLLER_FAILURE );
	
	// Retrieve the Initiator Identifier for this HBA.
	fInitiatorIdentifier = ReportInitiatorIdentifier ( );
	number = OSNumber::withNumber ( fInitiatorIdentifier, 64 );
	if ( number != NULL )
	{
		
		setProperty ( kIOPropertySCSIInitiatorIdentifierKey, number );
		number->release ( );
		number = NULL;
		
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
	
	// Set the property
	setProperty ( kIOPropertySCSIInitiatorManagesTargets, result );
	
	if ( result == false )
	{
		
		// This HBA does not support a mechanism for device attach/detach 
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
	// If execution jumped to this label, the HBA child class failed to
	// properly initialize.	Nothing to clean up, so fall through this label to
	// next exception handling point.
	
	
CONTROLLER_DICT_FAILURE:
	// If execution jumped to this label, the HBA child class failed to
	// create its controller characteristics dictionary.
	
	// Release the workloop and associated objects.
	ReleaseWorkLoop ( );
	
	
WORKLOOP_CREATE_FAILURE:
	// WORKLOOP_CREATE_FAILURE:
	// If execution jumped to this label, the workloop or associated objects
	// could not be allocated.
	IOSimpleLockFree ( fDeviceLock );
	fDeviceLock = NULL;
	
	
DEVICE_LOCK_ALLOC_FAILURE:
	// DEVICE_LOCK_ALLOC_FAILURE:
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


//-----------------------------------------------------------------------------
//	stop - Ends provided services.									  [PRIVATE]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::stop ( IOService * provider )
{
	
	// Halt all services from the subclass.
	StopController ( );
	
	if ( fHBAHasBeenInitialized == true )
	{
		
		fHBAHasBeenInitialized = false;
		
		// Inform the subclass to terminate all allocated resources
		TerminateController ( );
		
	}
	
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


//-----------------------------------------------------------------------------
//	willTerminate - Prevent HBA from accepting any further commands.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::willTerminate (
	IOService * 		provider,
	IOOptionBits		options )
{
	
	// Prevent any new requests from being sent to the controller.
	fHBACanAcceptClientRequests = false;
	return true;
	
}


//-----------------------------------------------------------------------------
//	didTerminate - Closes provider if all outstanding I/O is complete.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::didTerminate (
	IOService * 		provider,
	IOOptionBits		options,
	bool *				defer )
{
	
	if ( fProvider->isOpen ( this ) == true )
	{
		fProvider->close ( this );
	}
	
	return true;
	
}


//-----------------------------------------------------------------------------
//	getWorkLoop - Gets the workloop.								  [PRIVATE]
//-----------------------------------------------------------------------------

IOWorkLoop *
IOSCSIParallelInterfaceController::getWorkLoop ( void ) const
{
	return GetWorkLoop ( );
}


//-----------------------------------------------------------------------------
//	GetProvider - Gets the provider object.							[PROTECTED]
//-----------------------------------------------------------------------------

IOService *
IOSCSIParallelInterfaceController::GetProvider ( void )
{
	return fProvider;
}


//-----------------------------------------------------------------------------
//	GetSCSIDomainIdentifier - Gets the domain identifier.			[PROTECTED]
//-----------------------------------------------------------------------------

SInt32
IOSCSIParallelInterfaceController::GetSCSIDomainIdentifier ( void )
{
	return fSCSIDomainIdentifier;
}


#if 0
#pragma mark -
#pragma mark Property Management
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	SetHBAProperty - Sets a property for this object.	 			   [PUBLIC]
//-----------------------------------------------------------------------------

bool	
IOSCSIParallelInterfaceController::SetHBAProperty (
									const char *	key,
									OSObject *	 	value )
{
	
	bool			result 		= false;
	OSDictionary *	hbaDict		= NULL;
	OSDictionary *	copyDict	= NULL;
	
	require_nonzero ( key, ErrorExit );
	require_nonzero ( value, ErrorExit );
	
	// We should be within a synchronized context (i.e. holding the workloop lock),
	if ( fWorkLoop->inGate ( ) == false )
	{
		
		// Let's make sure to grab the lock and call this routine again.
		result = fControllerGate->runAction (
					OSMemberFunctionCast (
						IOCommandGate::Action,
						this,
						&IOSCSIParallelInterfaceController::SetHBAProperty ),
					( void * ) key,
					( void * ) value );
		
		goto ErrorExit;
		
	}

	copyDict = OSDynamicCast ( OSDictionary, copyProperty ( kIOPropertyControllerCharacteristicsKey ) );
	require_nonzero ( copyDict, ErrorExit );
	
	hbaDict = ( OSDictionary * ) copyDict->copyCollection ( );
	copyDict->release ( );
	
	require_nonzero ( hbaDict, ErrorExit );
	
	if ( strcmp ( key, kIOPropertyVendorNameKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else if ( strcmp ( key, kIOPropertyProductNameKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else if ( strcmp ( key, kIOPropertyProductRevisionLevelKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else if ( strcmp ( key, kIOPropertyPortDescriptionKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else if ( strcmp ( key, kIOPropertyPortSpeedKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else if ( strcmp ( key, kIOPropertySCSIParallelSignalingTypeKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else if ( strcmp ( key, kIOPropertyFibreChannelCableDescriptionKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else if ( strcmp ( key, kIOPropertyFibreChannelPortWorldWideNameKey ) == 0 )
	{
		
		OSData * data = OSDynamicCast ( OSData, value );
		
		require_nonzero ( data, ErrorExit );
		require ( ( data->getLength ( ) == kWorldWideNameDataSize ), ErrorExit );
		result = hbaDict->setObject ( key, value );
		
	}
	
	else if ( strcmp ( key, kIOPropertyFibreChannelNodeWorldWideNameKey ) == 0 )
	{
		
		OSData * data = OSDynamicCast ( OSData, value );
		
		require_nonzero ( data, ErrorExit );
		require ( ( data->getLength ( ) == kWorldWideNameDataSize ), ErrorExit );
		result = hbaDict->setObject ( key, value );
		
	}
	
	else if ( strcmp ( key, kIOPropertyFibreChannelAddressIdentifierKey ) == 0 )
	{
		
		OSData * data = OSDynamicCast ( OSData, value );
		
		require_nonzero ( data, ErrorExit );
		require ( ( data->getLength ( ) == kAddressIdentifierDataSize ), ErrorExit );
		result = hbaDict->setObject ( key, value );
		
	}
	
	else if ( strcmp ( key, kIOPropertyFibreChannelALPAKey ) == 0 )
	{
		
		OSData * data = OSDynamicCast ( OSData, value );
		
		require_nonzero ( data, ErrorExit );
		require ( ( data->getLength ( ) == kALPADataSize ), ErrorExit );
		result = hbaDict->setObject ( key, value );
		
	}
	
	else if ( strcmp ( key, kIOPropertyPortTopologyKey ) == 0 )
	{
		result = hbaDict->setObject ( key, value );
	}
	
	else
	{
		ERROR_LOG ( ( "SetHBAProperty: Unrecognized property key = %s", key ) );
	}
	
	setProperty ( kIOPropertyControllerCharacteristicsKey, hbaDict );
	hbaDict->release ( );
	hbaDict = NULL;
	
	messageClients ( kIOMessageServicePropertyChange );
	
	
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	RemoveHBAProperty - Removes a property for this object. 		   [PUBLIC]
//-----------------------------------------------------------------------------

void	
IOSCSIParallelInterfaceController::RemoveHBAProperty ( const char * key )
{
	
	OSDictionary *	hbaDict		= NULL;
	OSDictionary *	copyDict	= NULL;
	
	require_nonzero ( key, ErrorExit );
	
	// We should be within a synchronized context (i.e. holding the workloop lock),
	if ( fWorkLoop->inGate ( ) == false )
	{
		
		// Let's make sure to grab the lock and call this routine again.
		fControllerGate->runAction (
			OSMemberFunctionCast (
				IOCommandGate::Action,
				this,
				&IOSCSIParallelInterfaceController::RemoveHBAProperty ),
			( void * ) key );
		
		goto ErrorExit;
		
	}
	
	copyDict = OSDynamicCast ( OSDictionary, copyProperty ( kIOPropertyControllerCharacteristicsKey ) );
	require_nonzero ( copyDict, ErrorExit );
	
	hbaDict = ( OSDictionary * ) copyDict->copyCollection ( );
	copyDict->release ( );
	
	require_nonzero ( hbaDict, ErrorExit );
	
	if ( hbaDict->getObject ( key ) != NULL )
	{
		
		hbaDict->removeObject ( key );
		
	}
	
	setProperty ( kIOPropertyControllerCharacteristicsKey, hbaDict );
	hbaDict->release ( );
	hbaDict = NULL;
	
	messageClients ( kIOMessageServicePropertyChange );
	
	
ErrorExit:
	
	
	return;
	
}


#if 0
#pragma mark -
#pragma mark WorkLoop Management
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	GetWorkLoop - Gets the workloop.								[PROTECTED]
//-----------------------------------------------------------------------------

IOWorkLoop *
IOSCSIParallelInterfaceController::GetWorkLoop ( void ) const
{
	return fWorkLoop;
}


//-----------------------------------------------------------------------------
//	GetCommandGate - Gets the command gate.							[PROTECTED]
//-----------------------------------------------------------------------------

IOCommandGate *
IOSCSIParallelInterfaceController::GetCommandGate ( void )
{
	return fControllerGate;
}


//-----------------------------------------------------------------------------
//	CreateWorkLoop - Creates the workloop and associated objects.	  [PRIVATE]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::CreateWorkLoop ( IOService * provider )
{
	
	bool		result = false;
	IOReturn	status = kIOReturnSuccess;
	char		lockGroupName[64];
	
	bzero ( lockGroupName, sizeof ( lockGroupName ) );
	
	snprintf ( lockGroupName, 64, "SCSI Domain %d", ( int ) fSCSIDomainIdentifier );
	
	if ( fWorkLoop == NULL )
	{
		
		fWorkLoop = SCSIParallelWorkLoop::Create ( lockGroupName );
		require_nonzero ( fWorkLoop, CREATE_WORKLOOP_FAILURE );
		
	}
	
	// Create a timer.
	fTimerEvent = SCSIParallelTimer::CreateTimerEventSource ( this,
			( IOTimerEventSource::Action ) &IOSCSIParallelInterfaceController::TimeoutOccurred );
	require_nonzero ( fTimerEvent, TIMER_CREATION_FAILURE );
	
	// Add the timer to the workloop.
	status = fWorkLoop->addEventSource ( fTimerEvent );
	require_success ( status, ADD_TES_FAILURE );
	
	// Ask the subclass to create the device interrupt.
	fDispatchEvent = CreateDeviceInterrupt (
		&IOSCSIParallelInterfaceController::ServiceInterrupt,
		&IOSCSIParallelInterfaceController::FilterInterrupt,
		provider );
	
	// Virtual HBAs or HBAs that want to handle interrupts differently might
	// return NULL. We'll allow that, but they'll have to deal with things
	// themselves.
	if ( fDispatchEvent != NULL )
	{
		
		// Add the interrupt event source to the workloop.
		status = fWorkLoop->addEventSource ( fDispatchEvent );
		require_success ( status, ADD_ISR_EVENT_FAILURE );
		
	}
	
	// Create a command gate.
	fControllerGate = IOCommandGate::commandGate ( this, NULL );
	require_nonzero ( fControllerGate, ALLOCATE_COMMAND_GATE_FAILURE );
	
	// Add the command gate to the workloop.
	status = fWorkLoop->addEventSource ( fControllerGate );
	require_success ( status,  ADD_GATE_EVENT_FAILURE );
	
	result = true;
	
	return result;
	
	
ADD_GATE_EVENT_FAILURE:
	
	
	require_nonzero_quiet ( fControllerGate, ALLOCATE_COMMAND_GATE_FAILURE );
	fControllerGate->release ( );
	fControllerGate = NULL;
	
	
ALLOCATE_COMMAND_GATE_FAILURE:
	
	
	if ( fDispatchEvent != NULL )
	{
		fWorkLoop->removeEventSource ( fDispatchEvent );
	}
	
	
ADD_ISR_EVENT_FAILURE:
	
	
	if ( fDispatchEvent != NULL )
	{
		
		fDispatchEvent->release ( );
		fDispatchEvent = NULL;
		
	}
	
	fWorkLoop->removeEventSource ( fTimerEvent );
	
	
ADD_TES_FAILURE:
	
	
	require_nonzero_quiet ( fTimerEvent, TIMER_CREATION_FAILURE );
	fTimerEvent->release ( );
	fTimerEvent = NULL;
	
	
TIMER_CREATION_FAILURE:
	
	
	require_nonzero_quiet ( fWorkLoop, CREATE_WORKLOOP_FAILURE );
	fWorkLoop->release ( );
	fWorkLoop = NULL;
	
	
CREATE_WORKLOOP_FAILURE:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	ReleaseWorkLoop - Releases the workloop and associated objects.	  [PRIVATE]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::ReleaseWorkLoop ( void )
{
	
	// Make sure we have a workloop.
	if ( fWorkLoop != NULL )
	{
		
		// Remove all the event sources from the workloop
		// and deallocate them.
		
		if ( fControllerGate != NULL )
		{
			
			fWorkLoop->removeEventSource ( fControllerGate );
			fControllerGate->release ( );
			fControllerGate = NULL;
			
		}
		
		if ( fTimerEvent != NULL ) 	
		{
			
			fWorkLoop->removeEventSource ( fTimerEvent );
			fTimerEvent->release ( );
			fTimerEvent = NULL;
			
		}
		
		if ( fDispatchEvent != NULL )   
		{
			
			fWorkLoop->removeEventSource ( fDispatchEvent );
			fDispatchEvent->release ( );
			fDispatchEvent = NULL;
			
		}
		
		fWorkLoop->release ( );
		fWorkLoop = NULL;
		
	}
	
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Task Management
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	GetSCSIParallelTask - Gets a parallel task from the pool.		   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	FreeSCSIParallelTask - Returns a parallel task to the pool.		   [PUBLIC]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::FreeSCSIParallelTask (
							SCSIParallelTaskIdentifier returnTask )
{

	SCSIParallelTask *	parallelTask	= NULL;
	IOReturn			status			= kIOReturnSuccess;

	parallelTask = OSDynamicCast ( SCSIParallelTask, returnTask );

	require_nonzero ( parallelTask, ERROR_EXIT_NULL_TASK );

	parallelTask->SetSCSITaskIdentifier ( NULL );

	status = parallelTask->clearMemoryDescriptor ( false );

	if ( status != kIOReturnSuccess )
	{

		ERROR_LOG ( ( "FreeSCSIParallelTask: Task %p seems to be still active. "
					"IODMACommand::complete ( ) may not have been called for "
					"this task.", returnTask ) );

	}	

	fParallelTaskPool->returnCommand ( ( IOCommand * ) returnTask );
	
	return;
	
	
ERROR_EXIT_NULL_TASK:


	STATUS_LOG ( ( "FreeSCSIParallelTask: Encountered a NULL task pointer!\n" ) );

	return;

}


//-----------------------------------------------------------------------------
//	AllocateSCSIParallelTasks - Allocates parallel tasks for the pool.
//																	  [PRIVATE]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::AllocateSCSIParallelTasks ( void )
{
	
	bool				result			= false;
	SCSIParallelTask *	parallelTask 	= NULL;
	UInt32				taskSize		= 0;
	UInt32				index			= 0;
	UInt64				mask			= 0;
	OSNumber *			value			= NULL;
	OSDictionary *		constraints		= NULL;
	
	// Default alignment is 16-byte aligned, 32-bit memory only.
	taskSize 	= ReportHBASpecificTaskDataSize ( );
	constraints = OSDictionary::withCapacity ( kHBAContraintsDictionaryEntryCount );
	
	require_nonzero ( constraints, ERROR_EXIT );
	
	ReportHBAConstraints ( constraints );
	
	// Set constraints for cluster layer / IOBlockStorageDriver.
	value = OSDynamicCast ( OSNumber, constraints->getObject ( kIOMaximumSegmentCountReadKey ) );
	if ( value != NULL )
	{
		setProperty ( kIOMaximumSegmentCountReadKey, value );
	}

	value = OSDynamicCast ( OSNumber, constraints->getObject ( kIOMaximumSegmentCountWriteKey ) );
	if ( value != NULL )
	{
		setProperty ( kIOMaximumSegmentCountWriteKey, value );
	}

	value = OSDynamicCast ( OSNumber, constraints->getObject ( kIOMaximumSegmentByteCountReadKey ) );
	if ( value != NULL )
	{
		setProperty ( kIOMaximumSegmentByteCountReadKey, value );
	}

	value = OSDynamicCast ( OSNumber, constraints->getObject ( kIOMaximumSegmentByteCountWriteKey ) );
	if ( value != NULL )
	{
		setProperty ( kIOMaximumSegmentByteCountWriteKey, value );
	}

	value = OSDynamicCast ( OSNumber, constraints->getObject ( kIOMinimumSegmentAlignmentByteCountKey ) );
	if ( value != NULL )
	{
		setProperty ( kIOMinimumSegmentAlignmentByteCountKey, value );
	}

	value = OSDynamicCast ( OSNumber, constraints->getObject ( kIOMaximumSegmentAddressableBitCountKey ) );
	if ( value != NULL )
	{
		setProperty ( kIOMaximumSegmentAddressableBitCountKey, value );
	}
	
	value = OSDynamicCast ( OSNumber, constraints->getObject ( kIOMinimumHBADataAlignmentMaskKey ) );
	mask  = value->unsigned64BitValue ( );
	
	constraints->release ( );
	constraints = NULL;
	
	fParallelTaskPool = IOCommandPool::withWorkLoop ( fWorkLoop );
	require_nonzero ( fParallelTaskPool, POOL_CREATION_FAILURE );
	
	// As long as a single SCSI Parallel Task can be allocated, the HBA
	// can function.  Check to see if the first one can be allocated.
	parallelTask = SCSIParallelTask::Create ( taskSize, mask );
	require_nonzero ( parallelTask, TASK_CREATION_FAILURE );
	
	result = InitializeDMASpecification ( parallelTask );
	require ( result, TASK_INIT_FAILURE );
	
	// Send the single command into the pool.
	fParallelTaskPool->returnCommand ( parallelTask );
	
	// Now try to allocate the remaining Tasks that the HBA reports that it
	// can support.
	for ( index = 1; index < fSupportedTaskCount; index++ )
	{
		
		// Allocate the command with enough space for the HBA specific data
		parallelTask = SCSIParallelTask::Create ( taskSize, mask );
		if ( parallelTask != NULL )
		{
			
			result = InitializeDMASpecification ( parallelTask );
			if ( result == false )
			{
				
				parallelTask->release ( );
				break;
				
			}
			
			// Send the next command into the pool.
			fParallelTaskPool->returnCommand ( parallelTask );
			
		}
		
	}
	
	// Did the subclass override the command pool size?
	value = OSDynamicCast ( OSNumber, getProperty ( kIOCommandPoolSizeKey ) );
	if ( value == NULL )
	{
		
		// No, set the default to be the number of commands we allocated.
		setProperty ( kIOCommandPoolSizeKey, index, 32 );
		
	}
	
	// Since at least a single SCSI Parallel Task was allocated, this
	// HBA can function.
	result = true;
	
	return result;
	
	
TASK_INIT_FAILURE:
	
	
	require_nonzero ( parallelTask, TASK_CREATION_FAILURE );
	parallelTask->release ( );
	parallelTask = NULL;
	
	
TASK_CREATION_FAILURE:
	
	
	require_nonzero ( fParallelTaskPool, POOL_CREATION_FAILURE );
	fParallelTaskPool->release ( );
	fParallelTaskPool = NULL;
	
	
POOL_CREATION_FAILURE:
ERROR_EXIT:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	DeallocateSCSIParallelTasks - Deallocates parallel tasks in the pool.
//																	  [PRIVATE]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	ExecuteParallelTask - Executes a parallel task.					   [PUBLIC]
//-----------------------------------------------------------------------------

SCSIServiceResponse 
IOSCSIParallelInterfaceController::ExecuteParallelTask ( 
							SCSIParallelTaskIdentifier 			parallelRequest )
{
	
	SCSIServiceResponse	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	// If the controller has requested a suspend,
	// return the command and let it add it back to the queue.
	if ( fHBACanAcceptClientRequests == true )
	{
		
		// If the controller has not suspended, send the task now.
		serviceResponse = ProcessParallelTask ( parallelRequest );
		
	}
	
	return serviceResponse;
	
}


//-----------------------------------------------------------------------------
//	CompleteParallelTask - Completes a parallel task.				[PROTECTED]
//-----------------------------------------------------------------------------

void	
IOSCSIParallelInterfaceController::CompleteParallelTask ( 
							SCSIParallelTaskIdentifier 	parallelRequest,
							SCSITaskStatus	 			completionStatus,
							SCSIServiceResponse 		serviceResponse )
{
	
	IOSCSIParallelInterfaceDevice *		target = NULL;
	
	STATUS_LOG ( ( "+IOSCSIParallelInterfaceController::CompleteParallelTask\n" ) );
	
	// We should be within a synchronized context (i.e. holding the workloop lock),
	// but some subclassers aren't so bright. <sigh>
	if ( fWorkLoop->inGate ( ) == false )
	{
		
		// Let's make sure to grab the lock and call this routine again.
		fControllerGate->runAction (
			OSMemberFunctionCast (
				IOCommandGate::Action,
				this,
				&IOSCSIParallelInterfaceController::CompleteParallelTask ),
			parallelRequest,
			( void * ) completionStatus,
			( void * ) serviceResponse );
		
		goto Exit;
		
	}
	
	// Remove the task from the timeout list.
	( ( SCSIParallelTimer * ) fTimerEvent )->RemoveTask ( parallelRequest );
	
	target = GetDevice ( parallelRequest );
	require_nonzero ( target, Exit );
	
	// Complete the command
	target->CompleteSCSITask (	parallelRequest, 
								serviceResponse, 
								completionStatus );
	
	
Exit:
	
	
	STATUS_LOG ( ( "-IOSCSIParallelInterfaceController::CompleteParallelTask\n" ) );
	return;
	
}


//-----------------------------------------------------------------------------
//	FindTaskForAddress - Finds a task by its address (ITLQ nexus)	   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	FindTaskForControllerIdentifier - Finds a task by its unique ID	   [PUBLIC]
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
//	CompleteAbortTask - Completes the AbortTaskRequest.	 			[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::CompleteAbortTask ( 	
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSITaggedTaskIdentifier	theQ,
					SCSIServiceResponse 		serviceResponse )
{
}


//-----------------------------------------------------------------------------
//	CompleteAbortTaskSet - Completes the AbortTaskSetRequest.		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::CompleteAbortTaskSet (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//-----------------------------------------------------------------------------
//	CompleteClearACA - Completes the ClearACARequest.				[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::CompleteClearACA (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//-----------------------------------------------------------------------------
//	CompleteClearTaskSet - Completes the ClearTaskSetRequest.		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::CompleteClearTaskSet (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//-----------------------------------------------------------------------------
//	CompleteLogicalUnitReset - Completes the LogicalUnitResetRequest.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::CompleteLogicalUnitReset (
					SCSITargetIdentifier 		theT,
					SCSILogicalUnitNumber		theL,
					SCSIServiceResponse 		serviceResponse )
{
}


//-----------------------------------------------------------------------------
//	CompleteTargetReset - Completes the TargetResetRequest  		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::CompleteTargetReset (
					SCSITargetIdentifier 		theT,
					SCSIServiceResponse 		serviceResponse )
{
}


//-----------------------------------------------------------------------------
//	ServiceInterrupt - Calls the registered interrupt handler. 		  [PRIVATE]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::ServiceInterrupt ( 
							OSObject *					theObject, 
							IOInterruptEventSource *	theSource,
							int							count  )
{
	( ( IOSCSIParallelInterfaceController * ) theObject )->HandleInterruptRequest ( );
}


//-----------------------------------------------------------------------------
//	FilterInterrupt - Calls the registered interrupt filter. 		  [PRIVATE]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::FilterInterrupt ( 
							OSObject *						theObject, 
							IOFilterInterruptEventSource *	theSource  )
{
	return ( ( IOSCSIParallelInterfaceController * ) theObject )->FilterInterruptRequest ( );
}


//-----------------------------------------------------------------------------
//	FilterInterruptRequest - Default filter routine. 				[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::FilterInterruptRequest ( void )
{
	return true;
}


//-----------------------------------------------------------------------------
//	EnableInterrupt - Enables the interrupt event source.	  		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::EnableInterrupt ( void )
{
	
	if ( fDispatchEvent != NULL )
	{
		fDispatchEvent->enable ( );
	}
	
}


//-----------------------------------------------------------------------------
//	DisableInterrupt - Disables the interrupt event source.	  		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::DisableInterrupt ( void )
{
	
	if ( fDispatchEvent != NULL )
	{
		fDispatchEvent->disable ( );
	}
	
}


//-----------------------------------------------------------------------------
//	SignalInterrupt - Cause the work loop to schedule the interrupt action
//					  even if the filter routine returns false.		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::SignalInterrupt ( void )
{
	
	if ( fDispatchEvent != NULL )
	{
		( ( IOFilterInterruptEventSource * ) fDispatchEvent )->signalInterrupt ( );
	}
	
}


#if 0
#pragma mark -
#pragma mark Timeout Management
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	SetTimeoutForTask - Sets the timeout.					  		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::SetTimeoutForTask (
							SCSIParallelTaskIdentifier 		parallelTask,
							UInt32							timeoutOverride )
{
	
	( ( SCSIParallelTimer * ) fTimerEvent )->SetTimeout (	parallelTask,
															timeoutOverride );
	
}


//-----------------------------------------------------------------------------
//	TimeoutOccurred - Calls the timeout handler.			  		  [PRIVATE]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::TimeoutOccurred ( 
							OSObject *					theObject, 
							IOTimerEventSource * 		theSender )
{
	
	SCSIParallelTimer *			timer		= NULL;
	SCSIParallelTaskIdentifier	expiredTask	= NULL;
	
	timer = OSDynamicCast ( SCSIParallelTimer, theSender );
	if ( timer != NULL )
	{
		
		timer->BeginTimeoutContext ( );
		
		expiredTask = timer->GetExpiredTask ( );
		while ( expiredTask != NULL )
		{
			
			( ( IOSCSIParallelInterfaceController * ) theObject )->HandleTimeout ( expiredTask );
			expiredTask = timer->GetExpiredTask ( );
			
		}
		
		timer->EndTimeoutContext ( );
		
		// Rearm the timer
		timer->Rearm ( );
		
	}
	
}


//-----------------------------------------------------------------------------
//	HandleTimeout - Generic timeout handler. Subclasses should override.
//															  		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::HandleTimeout (
						SCSIParallelTaskIdentifier			parallelRequest )
{
	
	check ( parallelRequest != NULL );
	CompleteParallelTask ( 	parallelRequest,
							kSCSITaskStatus_TaskTimeoutOccurred,
							kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
	
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Device Management
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	CreateTargetForID - Creates a target device for the ID specified.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::CreateTargetForID (
										SCSITargetIdentifier	targetID )
{
	
	OSDictionary *	dict 	= NULL;
	bool			result	= false;
	
	dict = OSDictionary::withCapacity ( 0 );
	require_nonzero ( dict, DICT_CREATION_FAILED );
	
	result = CreateTargetForID ( targetID, dict );
	
	dict->release ( );
	dict = NULL;
	
	
DICT_CREATION_FAILED:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	CreateTargetForID - Creates a target device for the ID specified.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::CreateTargetForID (
										SCSITargetIdentifier	targetID,
										OSDictionary * 			properties )
{
	
	IOSCSIParallelInterfaceDevice *		newDevice 	= NULL;
	IORegistryEntry *					entry		= NULL;
	bool								result		= false;
	
	// Some subclasses like to create targets from within the StartController() override.
	// In those cases, fHBACanAcceptClientRequests is still false. We force it to true
	// in this case since a target is getting created - the HBA must be ready for I/O
	// by that time...
	if ( fHBACanAcceptClientRequests == false )
		fHBACanAcceptClientRequests = true;
	
	// Verify that the device ID is not that of the initiator.
	require ( ( targetID != fInitiatorIdentifier ), INVALID_PARAMETER_EXIT );
	
	// First check to see if this device already exists
	require ( ( GetTargetForID ( targetID ) == NULL ), INVALID_PARAMETER_EXIT );
	
	// See if the controller has a device tree entry it wants us to hook this
	// target upto (e.g. to get io-device-location keys).
	entry = OSDynamicCast ( IORegistryEntry, properties->getObject ( kIOPropertyDeviceTreeEntryKey ) );
	
	// Create the IOSCSIParallelInterfaceDevice object
	newDevice = IOSCSIParallelInterfaceDevice::CreateTarget (
									targetID,
									ReportHBASpecificDeviceDataSize ( ),
									entry );
	require_nonzero ( newDevice, DEVICE_CREATION_FAILED_EXIT );
	
	AddDeviceToTargetList ( newDevice );
	
	result = newDevice->attach ( this );
	require ( result, ATTACH_FAILED_EXIT );
	
	result = newDevice->SetInitialTargetProperties ( properties );
	require ( result, START_FAILED_EXIT );
	
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


//-----------------------------------------------------------------------------
//	DestroyTargetForID - Destroys a target device for the ID specified.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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
	
	// Remove the IOSCSIParallelInterfaceDevice from the device list.
	RemoveDeviceFromTargetList ( victimDevice );
	
	// The device can now be destroyed.
	victimDevice->DestroyTarget ( );
	victimDevice->terminate ( );
	
}


//-----------------------------------------------------------------------------
//	SetTargetProperty - Sets a property for the specified target.	[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::SetTargetProperty (
					SCSIDeviceIdentifier 				targetID,
					const char * 						key,
					OSObject *							value )
{
	
	bool								result	= false;
	IOSCSIParallelInterfaceDevice * 	device	= NULL;
	
	device = GetTargetForID ( targetID );
	
	require_nonzero ( device, ErrorExit );
	result = device->SetTargetProperty ( key, value );
	
	
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	RemoveTargetProperty - Removes a property from the specified target.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::RemoveTargetProperty (
					SCSIDeviceIdentifier 				targetID,
					const char * 						key )
{
	
	IOSCSIParallelInterfaceDevice * 	device	= NULL;
	
	device = GetTargetForID ( targetID );
	
	require_nonzero ( device, ErrorExit );
	device->RemoveTargetProperty ( key );
	
	
ErrorExit:
	
	
	return;
	
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


//-----------------------------------------------------------------------------
//	InitializeDeviceList - Initializes device list.					  [PRIVATE]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::InitializeDeviceList ( void )
{
	
	// Initialize the SCSI Parallel Device Array to all NULL pointers
	for ( UInt32 i = 0; i < kSCSIParallelDeviceListArrayCount; i++ )
	{
		fParallelDeviceList[i] = NULL;
	}
	
}		


//-----------------------------------------------------------------------------
//	GetTargetForID - Gets the device object for the specified targetID.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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
	
	// Hash the index.
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


//-----------------------------------------------------------------------------
//	AddDeviceToTargetList - Adds a device to the target list.		  [PRIVATE]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::AddDeviceToTargetList (
							IOSCSIParallelInterfaceDevice *				newDevice )
{
	
	UInt8					indexID		= 0;
	IOInterruptState		lockState	= 0;
	
	STATUS_LOG ( ( "+IOSCSIParallelInterfaceController::AddDeviceToTargetList\n" ) );
	
	lockState = IOSimpleLockLockDisableInterrupt ( fDeviceLock );
	
	// Hash the index.
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


//-----------------------------------------------------------------------------
//	RemoveDeviceFromTargetList - Removes a device from the target list.
//																	  [PRIVATE]
//-----------------------------------------------------------------------------

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
		
		// Hash the index.
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


//-----------------------------------------------------------------------------
//	DoesHBAPerformAutoSense - Default implementation.				   [PUBLIC]
//-----------------------------------------------------------------------------

bool 
IOSCSIParallelInterfaceController::DoesHBAPerformAutoSense ( void )
{
	return false;
}


//-----------------------------------------------------------------------------
//	ReportHBAConstraints - Default implementation.				 	   [PUBLIC]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::ReportHBAConstraints (
	OSDictionary *		constraints )
{
	
	UInt64			value	= 0;
	OSNumber *		number	= NULL;
	
	// Default alignment is 16-byte aligned, 32-bit memory only.
	value = 0x00000000FFFFFFF0ULL;
	
	number = OSNumber::withNumber ( value, 64 );
	if ( number != NULL )
	{
		
		constraints->setObject ( kIOMinimumHBADataAlignmentMaskKey, number );
		number->release ( );
		
	}
	
	// 32-bit addressing by default.
	value = 32;
	number = OSNumber::withNumber ( value, 64 );
	if ( number != NULL )
	{
		
		constraints->setObject ( kIOMaximumSegmentAddressableBitCountKey, number );
		number->release ( );
		
	}
	
	// 4-byte alignment by default.
	value = 4;
	number = OSNumber::withNumber ( value, 64 );
	if ( number != NULL )
	{
		
		constraints->setObject ( kIOMinimumSegmentAlignmentByteCountKey, number );
		number->release ( );
		
	}
	
}


//-----------------------------------------------------------------------------
//	InitializeDMASpecification - Default implementation.		 	[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceController::InitializeDMASpecification (
	IODMACommand * command )
{
	
	bool	result = false;
	
	result = command->initWithSpecification (
		kIODMACommandOutputHost32,
		32,			// addressBits
		4096,		// PAGE_SIZE
		IODMACommand::kMapped,
		1048576,	// 1MB I/O
		4			// 4-byte aligned segments
		);
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	CreateDeviceInterrupt - Default implementation.				 	[PROTECTED]
//-----------------------------------------------------------------------------

IOInterruptEventSource *
IOSCSIParallelInterfaceController::CreateDeviceInterrupt (
	IOInterruptEventSource::Action			action,
	IOFilterInterruptEventSource::Filter	filter,
	IOService *								provider )
{
	
	IOInterruptEventSource *	ies = NULL;
	
	ies = IOFilterInterruptEventSource::filterInterruptEventSource (
		this,
		action,
		filter,
		provider,
		0 );
	
	check ( ies != NULL );
	
	return ies;
	
}


//-----------------------------------------------------------------------------
//	SuspendServices - Suspends services temporarily.				[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::SuspendServices ( void )
{
	
	// The HBA child class has requested the suspension of tasks.
	fHBACanAcceptClientRequests = false;
	
}


//-----------------------------------------------------------------------------
//	ResumeServices - Resume services temporarily.					[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::ResumeServices ( void )
{
	
	// The HBA child class has allowed the submission of tasks.
	fHBACanAcceptClientRequests = true;
	
}


//-----------------------------------------------------------------------------
//	NotifyClientsOfBusReset - Notifies clients of bus resets.		[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::NotifyClientsOfBusReset ( void )
{
	messageClients ( kSCSIControllerNotificationBusReset );
}


//-----------------------------------------------------------------------------
//	NotifyClientsOfPortStatusChange - Notifies clients of port status changes.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

void
IOSCSIParallelInterfaceController::NotifyClientsOfPortStatusChange (
												SCSIPortStatus newStatus )
{
	
	OSDictionary *	hbaDict		= NULL;
	OSDictionary *	copyDict	= NULL;
	OSString *		string 		= NULL;
	char *			linkStatus	= NULL;
	
	copyDict = OSDynamicCast ( OSDictionary, copyProperty ( kIOPropertyControllerCharacteristicsKey ) );
	require_nonzero ( copyDict, ErrorExit );
	
	hbaDict = ( OSDictionary * ) copyDict->copyCollection ( );
	copyDict->release ( );
	
	require_nonzero ( hbaDict, ErrorExit );
	
	switch ( newStatus )
	{
		
		case kSCSIPort_StatusOnline:
			linkStatus = ( char * ) kIOPropertyPortStatusLinkEstablishedKey;
			break;
		
		case kSCSIPort_StatusOffline:
			linkStatus = ( char * ) kIOPropertyPortStatusNoLinkEstablishedKey;
			break;
		
		case kSCSIPort_StatusFailure:
			linkStatus = ( char * ) kIOPropertyPortStatusLinkFailedKey;
			break;
		
		default:
			break;
		
	}
	
	string = OSString::withCString ( linkStatus );
	if ( string != NULL )
	{
		
		hbaDict->setObject ( kIOPropertyPortStatusKey, string );
		string->release ( );
		string = NULL;
		
	}
	
	setProperty ( kIOPropertyControllerCharacteristicsKey, hbaDict );
	hbaDict->release ( );
	hbaDict = NULL;
	
	
ErrorExit:
	
	
	messageClients ( kSCSIPort_NotificationStatusChange, ( void * ) newStatus );
	
}


#if 0
#pragma mark -
#pragma mark SCSI Parallel Task Object Accessors
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	GetSCSITaskIdentifier - Gets SCSITaskIdentifier for task.		[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetTargetIdentifier - Gets SCSITargetIdentifier for task.		[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetDevice - Gets Device for task.								   [STATIC]
//-----------------------------------------------------------------------------

static IOSCSIParallelInterfaceDevice *
GetDevice (	SCSIParallelTaskIdentifier 	parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return NULL;
	}
	
	return tempTask->GetDevice ( );
	
}


//-----------------------------------------------------------------------------
//	CopyProtocolCharacteristicsProperties - Copies properties from object
//											to dictionary.			   [STATIC]
//-----------------------------------------------------------------------------

static void
CopyProtocolCharacteristicsProperties ( OSDictionary * dict, IOService * service )
{
	
	OSString *		string = NULL;
	OSNumber *		number = NULL;
	
	// Set the Physical Interconnect property if it doesn't already exist.
	if ( dict->getObject ( kIOPropertyPhysicalInterconnectTypeKey ) == NULL )
	{
		
		string = OSDynamicCast ( OSString, service->getProperty ( kIOPropertyPhysicalInterconnectTypeKey ) );
		if ( string == NULL )
		{
			
			string = OSString::withCString ( kIOPropertyPhysicalInterconnectTypeSCSIParallel );
			if ( string != NULL )
			{
				
				dict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, string );
				string->release ( );
				string = NULL;
				
			}
			
		}
		
		else
		{
			
			dict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, string );
			
		}
		
	}

	// Set the Physical Interconnect Location property if it doesn't already exist.
	if ( dict->getObject ( kIOPropertyPhysicalInterconnectLocationKey ) == NULL )
	{
		
		string = OSDynamicCast ( OSString, service->getProperty ( kIOPropertyPhysicalInterconnectLocationKey ) );
		if ( string == NULL )
		{
			
			string = OSString::withCString ( kIOPropertyInternalExternalKey );
			if ( string != NULL )
			{
				
				dict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, string );
				string->release ( );
				string = NULL;
				
			}
			
		}
		
		else
		{
			
			dict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, string );
			
		}
		
	}
	
	// Set the Timeout Duration properties if they don't already exist.
	if ( dict->getObject ( kIOPropertyReadTimeOutDurationKey ) == NULL )
	{
		
		number = OSDynamicCast ( OSNumber, service->getProperty ( kIOPropertyReadTimeOutDurationKey ) );
		if ( number != NULL )
		{
			
			dict->setObject ( kIOPropertyReadTimeOutDurationKey, number );
			
		}
		
	}
	
	if ( dict->getObject ( kIOPropertyWriteTimeOutDurationKey ) == NULL )
	{
		
		number = OSDynamicCast ( OSNumber, service->getProperty ( kIOPropertyWriteTimeOutDurationKey ) );
		if ( number != NULL )
		{
			
			dict->setObject ( kIOPropertyWriteTimeOutDurationKey, number );
			
		}
		
	}
	
}


// ---- Methods for Accessing data in the client's SCSI Task Object ----	
// Method to retrieve the LUN that identifies the Logical Unit whose Task
// Set to which this task is to be added.
// --> Currently this only supports Level 1 Addressing, complete
// Hierachal LUN addressing will need to be added to the SCSI Task object
// and the Peripheral Device Type objects which will represent Logical Units.
// Since that will be completed before this is released, this method will be
// changed at that time.


//-----------------------------------------------------------------------------
//	GetLogicalUnitNumber - Gets SCSILogicalUnitNumber for task.		[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetTaggedTaskIdentifier - Gets SCSITaggedTaskIdentifier for task.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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
							

//-----------------------------------------------------------------------------
//	GetTaskAttribute - Gets SCSITaskAttribute for task. 			[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetCommandDescriptorBlockSize - Gets CDB size for task. 		[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetCommandDescriptorBlock - This will always return a 16 Byte CDB.
//														 			[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetDataTransferDirection - Gets data transfer direction.		[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetRequestedDataTransferCount - Gets requested transfer count.	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetRealizedDataTransferCount - Gets realized transfer count.	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	SetRealizedDataTransferCount - Sets realized transfer count.	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	IncrementRealizedDataTransferCount - Adjusts realized transfer count.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetDataBuffer - Gets data buffer associated with this task.		[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetDataBufferOffset - Gets data buffer offset associated with this task.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetDMACommand - Gets IODMACommand associated with this task.	[PROTECTED]
//-----------------------------------------------------------------------------

IODMACommand *
IOSCSIParallelInterfaceController::GetDMACommand ( 
							SCSIParallelTaskIdentifier 		parallelTask )
{
	return ( IODMACommand * ) parallelTask;
}


//-----------------------------------------------------------------------------
//	GetTimeoutDuration - 	Gets timeout duration in milliseconds associated.
//							with this task.							[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	SetAutoSenseData - 	Sets autosense data in task.				[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetAutoSenseData - 	Gets autosense data in task.				[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetAutoSenseDataSize - 	Gets autosense data size.				[PROTECTED]
//-----------------------------------------------------------------------------

UInt8
IOSCSIParallelInterfaceController::GetAutoSenseDataSize (
 							SCSIParallelTaskIdentifier 		parallelTask )
{
	
	SCSIParallelTask *	tempTask = ( SCSIParallelTask * ) parallelTask;
	
	if ( tempTask == NULL )
	{
		return 0;
	}
	
	return tempTask->GetAutoSenseDataSize ( );
	
}


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiation - Gets SCSIParallelFeatureRequest status
//										for specified feature.		[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiationCount - 	Gets SCSIParallelFeatureRequest
//												count.				[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	SetSCSIParallelFeatureNegotiationResult - 	Sets SCSIParallelFeatureResult
//												status for specified feature.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiationResult - 	Gets SCSIParallelFeatureResult
//												for requested feature.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiationResultCount - 	Gets SCSIParallelFeatureResult
//													count.			[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	SetControllerTaskIdentifier - Sets the unique identifier for the task.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetControllerTaskIdentifier - Gets the unique identifier for the task.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetHBADataSize - Gets size of HBA data allocated in the task.	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetHBADataPointer - Gets pointer to HBA data.					[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetHBADataDescriptor - Gets IOMemoryDescriptor for HBA data.	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetHBATargetDataSize - Gets size of HBA data for the target.	[PROTECTED]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	GetHBATargetDataPointer - Gets size of HBA data for the target.	[PROTECTED]
//-----------------------------------------------------------------------------

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
OSMetaClassDefineReservedUsed ( IOSCSIParallelInterfaceController,  1 );		// Used for DoesHBAPerformAutoSense
OSMetaClassDefineReservedUsed ( IOSCSIParallelInterfaceController,  2 );		// Used for ReportHBAConstraints

OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  3 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  4 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  5 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController,  8 );

OSMetaClassDefineReservedUsed ( IOSCSIParallelInterfaceController,  9 );		// Used for HandleTimeout
OSMetaClassDefineReservedUsed ( IOSCSIParallelInterfaceController, 10 );		// Used for FilterInterruptRequest
OSMetaClassDefineReservedUsed ( IOSCSIParallelInterfaceController, 11 );		// Used for InitializeDMASpecification
OSMetaClassDefineReservedUsed ( IOSCSIParallelInterfaceController, 12 );		// Used for CreateDeviceInterrupt

OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIParallelInterfaceController, 16 );
