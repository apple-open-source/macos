/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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


//—————————————————————————————————————————————————————————————————————————————
//	Includes
//—————————————————————————————————————————————————————————————————————————————

// Core Foundation includes
#include <CoreFoundation/CoreFoundation.h>

// IOSCSIArchitectureModelFamily includes
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>

// Private includes
#include "SCSITaskDeviceClass.h"
#include "SCSITaskClass.h"

// C Library includes
#include <string.h>

// Since mach headers don’t have C++ wrappers we have to
// declare extern “C” before including them.
#ifdef __cplusplus
extern "C" {
#endif

#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>

#ifdef __cplusplus
}
#endif


//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define DEBUG									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SCSITaskDeviceClass"

#if DEBUG
#define PRINT(x)		printf x
#else
#define PRINT(x)
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


//—————————————————————————————————————————————————————————————————————————————
//	Static variable initialization
//—————————————————————————————————————————————————————————————————————————————

IOCFPlugInInterface
SCSITaskDeviceClass::sIOCFPlugInInterface = 
{
	0,
	&SCSITaskIUnknown::sQueryInterface,
	&SCSITaskIUnknown::sAddRef,
	&SCSITaskIUnknown::sRelease,
	1, 0, // version/revision
	&SCSITaskDeviceClass::sProbe,
	&SCSITaskDeviceClass::sStart,
	&SCSITaskDeviceClass::sStop
};

SCSITaskDeviceInterface
SCSITaskDeviceClass::sSCSITaskDeviceInterface =
{
	0,
	&SCSITaskDeviceClass::sQueryInterface,
	&SCSITaskDeviceClass::sAddRef,
	&SCSITaskDeviceClass::sRelease,
	1, 0, // version/revision
	&SCSITaskDeviceClass::sIsExclusiveAccessAvailable,
	&SCSITaskDeviceClass::sAddCallbackDispatcherToRunLoop,
	&SCSITaskDeviceClass::sRemoveCallbackDispatcherFromRunLoop,
	&SCSITaskDeviceClass::sObtainExclusiveAccess,
	&SCSITaskDeviceClass::sReleaseExclusiveAccess,
	&SCSITaskDeviceClass::sCreateSCSITask,
};


#if 0
#pragma mark -
#pragma mark Public Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• alloc - Called to allocate an instance of the class			[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

IOCFPlugInInterface **
SCSITaskDeviceClass::alloc ( void )
{
	
	SCSITaskDeviceClass * 		userClient	= NULL;
	IOCFPlugInInterface ** 		interface 	= NULL;
	IOReturn					status		= kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskDeviceClass::alloc called\n" ) );
	
	userClient = new SCSITaskDeviceClass;
	require_nonzero ( userClient, Error_Exit );
	
	status = userClient->Init ( );
	require_success_action_string ( status, Error_Exit, delete userClient, "Init failed" );
	
	userClient->AddRef ( );
	interface = ( IOCFPlugInInterface ** ) &userClient->fInterfaceMap.pseudoVTable;
	
	
Error_Exit:
	
	
	return interface;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• alloc - Called to allocate an instance of the class			[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskDeviceInterface **
SCSITaskDeviceClass::alloc ( io_service_t service, io_connect_t connection )
{
	
	SCSITaskDeviceClass * 		userClient	= NULL;
	SCSITaskDeviceInterface ** 	interface 	= NULL;
	IOReturn					status 		= kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskDeviceClass::alloc called\n" ) );
	
	userClient = new SCSITaskDeviceClass;
	require_nonzero ( userClient, Error_Exit );
	
	status = userClient->InitWithConnection ( service, connection );
	require_success_action_string ( status, Error_Exit, delete userClient, "InitWithConnection failed" );
	
	userClient->AddRef ( );
	interface = ( SCSITaskDeviceInterface ** ) &userClient->fSCSITaskDeviceInterfaceMap.pseudoVTable;
	
	
Error_Exit:
	
	
	return interface;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Init - Called to initialize an instance of the class			[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::Init ( void )
{
	
	IOReturn	status = kIOReturnNoResources;
	
	PRINT ( ( "SCSITaskDeviceClass::Init called\n" ) );
	
	fTaskSet = CFSetCreateMutable ( kCFAllocatorDefault, 0, NULL );
	require_nonzero ( fTaskSet, Error_Exit );
	
	status = kIOReturnSuccess;
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Init - 	Called to initialize an instance of the class with a connection
//				established by another object						[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::InitWithConnection ( 	io_service_t	service,
											io_connect_t	connection )
{
	
	IOReturn	status = kIOReturnBadArgument;
	
	PRINT ( ( "SCSITaskDeviceClass::InitWithConnection called\n" ) );
	
	require_nonzero ( service, Error_Exit );
	require_nonzero ( connection, Error_Exit );
	
	status = Init ( );
	require_success ( status, Error_Exit );
	
	fService 					= service;
	fConnection 				= connection;
	fIsServicesLayerInterface 	= true;
	status						= kIOReturnSuccess;
	
	
Error_Exit:
	
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark Protected Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• Default Constructor - Called on allocation					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskDeviceClass::SCSITaskDeviceClass ( void ) :
			SCSITaskIUnknown ( &sIOCFPlugInInterface )
{
	
	PRINT ( ( "SCSITaskDeviceClass constructor called\n" ) );
	
	// Init callback stuff
	fAsyncPort 					= MACH_PORT_NULL;
	fCFRunLoop 					= 0;
	fCFRunLoopSource 			= 0;
	fTaskSet					= 0;
	
	// init user client connection
	fConnection 				= MACH_PORT_NULL;
	fService 					= MACH_PORT_NULL;
	fHasExclusiveAccess 		= false;
	fIsServicesLayerInterface 	= false;
	
	// create driver interface map
	fSCSITaskDeviceInterfaceMap.pseudoVTable = ( IUnknownVTbl * ) &sSCSITaskDeviceInterface;
	fSCSITaskDeviceInterfaceMap.obj 		 = this;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Default Destructor - Called on deallocation					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskDeviceClass::~SCSITaskDeviceClass ( void )
{
	
	PRINT ( ( "SCSITaskDeviceClass : Destructor called\n" ) );
	
	if ( fAsyncPort != MACH_PORT_NULL )
	{
		
		PRINT ( ( "SCSITaskDeviceClass : fAsyncPort != MACH_PORT_NULL\n" ) );
		IOObjectRelease ( fAsyncPort );
		fAsyncPort = MACH_PORT_NULL;
		
	}
	
	if ( fTaskSet != 0 )
	{
		
		PRINT ( ( "SCSITaskDeviceClass : fTaskSet != 0\n" ) );
		CFRelease ( fTaskSet );
		
	}
	
}


#if 0
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• QueryInterface - Called to obtain the presence of an interface
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

HRESULT
SCSITaskDeviceClass::QueryInterface ( REFIID iid, void ** ppv )
{
	
	CFUUIDRef 	uuid 	= CFUUIDCreateFromUUIDBytes ( NULL, iid );
	HRESULT 	result 	= S_OK;
	
	PRINT ( ( "SCSITaskDeviceClass : QueryInterface called\n" ) );
	
	if ( CFEqual ( uuid, IUnknownUUID ) ||
		 CFEqual ( uuid, kIOCFPlugInInterfaceID ) ) 
	{
        
		*ppv = &fInterfaceMap;
        AddRef ( );
		
    }
	
	else if ( CFEqual ( uuid, kIOSCSITaskDeviceInterfaceID ) ) 
	{
		
		PRINT ( ( "kSCSITaskDeviceUserClientInterfaceID requested\n" ) );
		
		*ppv = &fSCSITaskDeviceInterfaceMap;
        AddRef ( );
		
    }
	
    else
	{
		
		*ppv = 0;
		result = E_NOINTERFACE;
		
	}
	
	CFRelease ( uuid );
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Probe - 	Called by IOKit to ascertain whether we can drive the provided
//				io_service_t										[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::Probe ( CFDictionaryRef propertyTable,
							 io_service_t inService,
							 SInt32 * order )
{
	
	CFMutableDictionaryRef	dict	= 0;
	IOReturn				status 	= kIOReturnBadArgument;
	Boolean					ok		= false;
	
	PRINT ( ( "SCSITaskDeviceClass::Probe called\n" ) );
	
	// Sanity check
	require_nonzero ( inService, Error_Exit );
	
	status = IORegistryEntryCreateCFProperties ( inService, &dict, NULL, 0 );
	require_success ( status, Error_Exit );
	
	ok = CFDictionaryContainsKey ( dict, CFSTR ( "IOCFPlugInTypes" ) );
	require_action ( ok, ReleaseDictionary, status = kIOReturnNoDevice );
	
	status = kIOReturnSuccess;
	
	
ReleaseDictionary:
	
	
	require_nonzero ( dict, Error_Exit );
	CFRelease ( dict );
	
	
Error_Exit:
	
	
	return status;
	
}
	

//—————————————————————————————————————————————————————————————————————————————
//	• Start - Called to start providing our services.				[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::Start ( CFDictionaryRef	propertyTable,
							 io_service_t		service )
{
	
	IOReturn 	status = kIOReturnNoDevice;
	
	PRINT ( ( "SCSITaskDeviceClass : Start\n" ) );
	
	fService = service;
	status = IOServiceOpen ( 	fService,
								mach_task_self ( ),
								kSCSITaskLibConnection,
								&fConnection );
	
	require_success ( status, Error_Exit );
	require_nonzero_action ( fConnection, Error_Exit, status = kIOReturnNoDevice );
	
	fHasExclusiveAccess = true;
	
	PRINT ( ( "SCSITaskDeviceClass : IOServiceOpen status = 0x%08lx, connection = %d\n",
			( UInt32 ) status, fConnection ) );
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Stop - Called to stop providing our services.					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::Stop ( void )
{
	
	PRINT ( ( "SCSITaskDeviceClass : Stop\n" ) );
	
	if ( fTaskSet != 0 )
	{
		
//		CFSetApplyFunction ( fTaskSet, &SCSITaskClass::sAbortAndReleaseTasks, 0 );
		CFRelease ( fTaskSet );
		fTaskSet = 0;
		
	}
	
	if ( fConnection != 0 )
	{
		
		PRINT ( ( "SCSITaskDeviceClass : IOServiceClose connection = %d\n", fConnection ) );
        IOServiceClose ( fConnection );
        fConnection = MACH_PORT_NULL;
		
	}
	
	return kIOReturnSuccess;
	
}


#if 0
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• IsExclusiveAccessAvailable - 	Called to ascertain if exclusive access is
//									available.						[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

Boolean
SCSITaskDeviceClass::IsExclusiveAccessAvailable ( void )
{
	
	Boolean			result 	= false;
	IOReturn		status 	= kIOReturnSuccess;

	PRINT ( ( "SCSITaskDeviceClass : IsExclusiveAccessAvailable\n" ) );
	
	require_nonzero ( fConnection, Error_Exit );
	
	// We only have to do this if the services layer
	// is involved. Otherwise, its handled in Start.
	if ( fIsServicesLayerInterface )
	{
		
		status = IOConnectMethodScalarIScalarO ( fConnection, 					 
												 kSCSITaskUserClientIsExclusiveAccessAvailable, 
												 0,
												 0 );
		
		if ( status == kIOReturnSuccess )
		{
			
			result = true;
			
		}
		
	}
	
	else
	{
		result = true;
	}
	
	
Error_Exit:
	
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• CreateDeviceAsyncEventSource - 	Called to create an async event source
//										in the form of a CFRunLoopSourceRef.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::CreateDeviceAsyncEventSource ( CFRunLoopSourceRef * source )
{
	
	IOReturn 			status 		= kIOReturnNoDevice;
	CFMachPortRef		cfMachPort 	= NULL;
	CFMachPortContext	context;
	Boolean				shouldFreeInfo; // zzz what's this for?
	
	PRINT ( ( "SCSITaskDeviceClass : CreateDeviceAsyncEventSource\n" ) );
	
	require_nonzero ( fConnection, Error_Exit );
	
	status = CreateDeviceAsyncPort ( NULL );
	require ( ( status == kIOReturnNotPermitted ) || ( status == kIOReturnSuccess ), Error_Exit );
	
	context.version 		= 1;
	context.info 			= this;
	context.retain 			= NULL;
	context.release 		= NULL;
	context.copyDescription = NULL;
	
	cfMachPort = CFMachPortCreateWithPort (
							kCFAllocatorDefault,
							fAsyncPort,
							( CFMachPortCallBack ) IODispatchCalloutFromMessage,
							&context,
							&shouldFreeInfo );
	
	require_nonzero_action ( cfMachPort, Error_Exit, status = kIOReturnNoMemory );
	
	PRINT ( ( "CFMachPortCreateWithPort\n" ) );
	
	fCFRunLoopSource = CFMachPortCreateRunLoopSource ( kCFAllocatorDefault, cfMachPort, 0 );
	require_nonzero_action ( fCFRunLoopSource, Error_Exit, status = kIOReturnNoMemory );
	
	PRINT ( ( "CFMachPortCreateRunLoopSource\n" ) );
	
	status = kIOReturnSuccess;
	
	require_nonzero ( source, Error_Exit );
	*source = fCFRunLoopSource;
	
	
Error_Exit:
	
	
	if ( cfMachPort != 0 )
	{
		CFRelease ( cfMachPort );
	}
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetDeviceAsyncEventSource - 	Called to get the CFRunLoopSourceRef.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

CFRunLoopSourceRef
SCSITaskDeviceClass::GetDeviceAsyncEventSource ( void )
{
	
	PRINT ( ( "GetDeviceAsyncEventSource, %p\n", fCFRunLoopSource ) );
	return fCFRunLoopSource;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• CreateDeviceAsyncPort - 	Called to create a mach port on which
//								to receive an async callback notification.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::CreateDeviceAsyncPort ( mach_port_t * port )
{
	
	IOReturn					status = kIOReturnNotPermitted;
	MyConnectionAndPortContext	context;	

	PRINT ( ( "CreateDeviceAsyncPort called\n" ) );
	
	require ( ( fAsyncPort == MACH_PORT_NULL ), Error_Exit );
	
	IOCreateReceivePort ( kOSAsyncCompleteMessageID, &fAsyncPort );
	context.connection 	= fConnection;
	context.asyncPort 	= fAsyncPort;
	CFSetApplyFunction ( fTaskSet, &SCSITaskClass::sSetConnectionAndPort, &context );
	
	status = kIOReturnSuccess;
	
	require_nonzero ( port, Error_Exit );
	*port = fAsyncPort;
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetDeviceAsyncPort - 	Called to get the mach port used for async
//							callback notifications.					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

mach_port_t
SCSITaskDeviceClass::GetDeviceAsyncPort ( void )
{
	
	PRINT ( ( "GetDeviceAsyncPort : %d\n", fAsyncPort ) );
	return fAsyncPort;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• AddCallbackDispatcherToRunLoop - 	Called to add an async callback
//										dispatch mechanism to the runloop
//										provided.					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::AddCallbackDispatcherToRunLoop ( CFRunLoopRef cfRunLoopRef )
{
	
	IOReturn 	status 	= kIOReturnNoDevice;
	
	PRINT ( ( "SCSITaskDeviceClass : AddCallbackDispatcherToRunLoop\n" ) );
	
	require_nonzero ( fConnection, Error_Exit );
	
	status = CreateDeviceAsyncEventSource ( NULL );
	require_success ( status, Error_Exit );
	
	fCFRunLoop = cfRunLoopRef;
	CFRunLoopAddSource ( fCFRunLoop, fCFRunLoopSource, kCFRunLoopCommonModes );
	
	PRINT ( ( "CFRunLoopAddSource\n" ) );
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• RemoveCallbackDispatcherFromRunLoop - Called to remove an async callback
//											dispatch mechanism from the runloop.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskDeviceClass::RemoveCallbackDispatcherFromRunLoop ( void )
{
	
	PRINT ( ( "SCSITaskDeviceClass : RemoveCallbackDispatcherFromRunLoop\n" ) );
	
	if ( fCFRunLoopSource != 0 )
	{
		
		CFRunLoopRemoveSource ( fCFRunLoop, fCFRunLoopSource, kCFRunLoopCommonModes );
		fCFRunLoopSource = 0;
		
	}
	
	if ( fAsyncPort != MACH_PORT_NULL )
	{
		
		IOObjectRelease ( fAsyncPort );
		fAsyncPort = MACH_PORT_NULL;
		
	}
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ObtainExclusiveAccess - Called to obtain exclusive access to the device.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::ObtainExclusiveAccess ( void )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskDeviceClass : ObtainExclusiveAccess\n" ) );
	
	// We only have to do this if the services layer
	// is involved. Otherwise, it's handled in Start.
	require ( fIsServicesLayerInterface, Exit );
	
	status = IOConnectMethodScalarIScalarO ( fConnection,
											 kSCSITaskUserClientObtainExclusiveAccess,
											 0,
											 0 );
	
	require_success ( status, Error_Exit );
	
	PRINT ( ( "we have exclusive access\n" ) );
	fHasExclusiveAccess = true;
	
	
Error_Exit:	
Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ReleaseExclusiveAccess - Called to release exclusive access to the device.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::ReleaseExclusiveAccess ( void )
{
	
	IOReturn	status = kIOReturnExclusiveAccess;
	
	PRINT ( ( "SCSITaskDeviceClass : ReleaseExclusiveAccess\n" ) );
	
	// Sanity check: make sure we have exclusive access already
	require ( fHasExclusiveAccess, Error_Exit );
	
	// If this isn't the services layer, short-circuit out
	require_action ( fIsServicesLayerInterface, Exit, status = kIOReturnSuccess );
	
	status = IOConnectMethodScalarIScalarO (	fConnection,
												kSCSITaskUserClientReleaseExclusiveAccess,
												0,
												0 );
	
	PRINT ( ( "ReleaseExclusiveAccess status = 0x%08x\n", status ) );
	
	
Error_Exit:
Exit:	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• CreateSCSITask - Called to create a SCSITaskInterface object.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskInterface **
SCSITaskDeviceClass::CreateSCSITask ( void )
{
	
	SCSITaskInterface **	interface = NULL;

	PRINT ( ( "SCSITaskDeviceClass : CreateSCSITask\n" ) );
	
	require ( fHasExclusiveAccess, Error_Exit );
	
	interface = SCSITaskClass::alloc ( this, fConnection, fAsyncPort );
	require_nonzero ( interface, Error_Exit );
	
	CFSetAddValue ( fTaskSet, interface );
	
	
Error_Exit:
	
		
	return interface;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• RemoveTaskFromTaskSet - 	Called to remove a SCSITaskInterface object
//								from the set of tasks.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskDeviceClass::RemoveTaskFromTaskSet ( SCSITaskInterface ** task )
{
	
	PRINT ( ( "SCSITaskDeviceClass : RemoveTaskFromTaskSet called\n" ) );
	check ( task );
	
	if ( fTaskSet )
	{
		CFSetRemoveValue ( fTaskSet, task );
	}
	
}


#if 0
#pragma mark -
#pragma mark Static C->C++ Glue Functions
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• sProbe - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sProbe ( void *			self,
							  CFDictionaryRef	propertyTable,
							  io_service_t		service,
							  SInt32 *			order )
{
	
	check ( self );
	return getThis ( self )->Probe ( propertyTable, service, order );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sStart - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sStart ( void *			self,
							  CFDictionaryRef	propertyTable,
							  io_service_t		service )
{
	
	check ( self );
	return getThis ( self )->Start ( propertyTable, service );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sStop - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sStop ( void * self )
{
	
	check ( self );
	return getThis ( self )->Stop ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sIsExclusiveAccessAvailable - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

Boolean
SCSITaskDeviceClass::sIsExclusiveAccessAvailable ( void * self )
{
	
	check ( self );
	return getThis ( self )->IsExclusiveAccessAvailable ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sCreateDeviceAsyncEventSource - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sCreateDeviceAsyncEventSource (
						void *					self,
						CFRunLoopSourceRef *	source )
{
	
	check ( self );
	return getThis ( self )->CreateDeviceAsyncEventSource ( source );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetDeviceAsyncEventSource - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

CFRunLoopSourceRef
SCSITaskDeviceClass::sGetDeviceAsyncEventSource ( void * self )
{
	
	check ( self );
	return getThis ( self )->GetDeviceAsyncEventSource ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sCreateDeviceAsyncPort - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sCreateDeviceAsyncPort ( void * self, mach_port_t * port )
{
	
	check ( self );
	return getThis ( self )->CreateDeviceAsyncPort ( port );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetDeviceAsyncPort - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

mach_port_t
SCSITaskDeviceClass::sGetDeviceAsyncPort ( void * self )
{
	
	check ( self );
	return getThis ( self )->GetDeviceAsyncPort ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sAddCallbackDispatcherToRunLoop - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sAddCallbackDispatcherToRunLoop (
									void * 			self,
									CFRunLoopRef	cfRunLoopRef )
{
	
	check ( self );
	return getThis ( self )->AddCallbackDispatcherToRunLoop ( cfRunLoopRef );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sRemoveCallbackDispatcherFromRunLoop - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskDeviceClass::sRemoveCallbackDispatcherFromRunLoop ( void * self )
{
	
	check ( self );
	return getThis ( self )->RemoveCallbackDispatcherFromRunLoop ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sObtainExclusiveAccess - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sObtainExclusiveAccess ( void * self )
{
	
	check ( self );
	return getThis ( self )->ObtainExclusiveAccess ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sReleaseExclusiveAccess - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskDeviceClass::sReleaseExclusiveAccess ( void * self )
{
	
	check ( self );
	return getThis ( self )->ReleaseExclusiveAccess ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sCreateSCSITask - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskInterface **
SCSITaskDeviceClass::sCreateSCSITask ( void * self )
{
	
	check ( self );
	return getThis ( self )->CreateSCSITask ( );
	
}