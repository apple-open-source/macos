/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>
#include "SCSITaskDeviceClass.h"
#include "SCSITaskClass.h"

#include <string.h>

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#define SCSI_TASK_DEVICE_CLASS_DEBUGGING_LEVEL 0

#if ( SCSI_TASK_DEVICE_CLASS_DEBUGGING_LEVEL > 0 )
#define PRINT(x)	printf x
#else
#define PRINT(x)
#endif


IOCFPlugInInterface
SCSITaskDeviceClass::sIOCFPlugInInterface = 
{
	0,
	&SCSITaskDeviceClass::staticQueryInterface,
	&SCSITaskDeviceClass::staticAddRef,
	&SCSITaskDeviceClass::staticRelease,
	1, 0, // version/revision
	&SCSITaskDeviceClass::staticProbe,
	&SCSITaskDeviceClass::staticStart,
	&SCSITaskDeviceClass::staticStop
};

SCSITaskDeviceInterface
SCSITaskDeviceClass::sSCSITaskDeviceInterface =
{
	0,
	&SCSITaskDeviceClass::staticQueryInterface,
	&SCSITaskDeviceClass::staticAddRef,
	&SCSITaskDeviceClass::staticRelease,
	1, 0, // version/revision
	&SCSITaskDeviceClass::staticIsExclusiveAccessAvailable,
	&SCSITaskDeviceClass::staticAddCallbackDispatcherToRunLoop,
	&SCSITaskDeviceClass::staticRemoveCallbackDispatcherFromRunLoop,
	&SCSITaskDeviceClass::staticObtainExclusiveAccess,
	&SCSITaskDeviceClass::staticReleaseExclusiveAccess,
	&SCSITaskDeviceClass::staticCreateSCSITask,
};


#pragma mark Public Methods

IOCFPlugInInterface **
SCSITaskDeviceClass::alloc ( void )
{
	
	SCSITaskDeviceClass * 		userClient;
	IOCFPlugInInterface ** 		interface = NULL;
	IOReturn					status;
	
	PRINT ( ( "SCSITaskDeviceClass::alloc called\n" ) );
	
	userClient = new SCSITaskDeviceClass;
	if ( userClient != NULL )
	{
		
		status = userClient->init ( );
		if ( status == kIOReturnSuccess )
		{
		
			userClient->AddRef ( );
			interface = ( IOCFPlugInInterface ** ) &userClient->fSCSITaskDeviceInterfaceMap.pseudoVTable;
		
		}
		
		else
		{
			delete userClient;
		}
		
	}
	
	return interface;
	
}


SCSITaskDeviceInterface **
SCSITaskDeviceClass::alloc ( io_service_t service, io_connect_t connection )
{
	
	SCSITaskDeviceClass * 		userClient;
	SCSITaskDeviceInterface ** 	interface = NULL;
	IOReturn					status = kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskDeviceClass::alloc called\n" ) );
	
	userClient = new SCSITaskDeviceClass;
	if ( userClient != NULL )
	{
		
		status = userClient->initWithConnection ( service, connection );
		if ( status == kIOReturnSuccess )
		{
			
			userClient->AddRef ( );
			interface = ( SCSITaskDeviceInterface ** ) &userClient->fSCSITaskDeviceInterfaceMap.pseudoVTable;
			
		}
		
		else
		{
			
			delete userClient;
			
		}
		
	}
	
	return interface;
	
}


IOReturn
SCSITaskDeviceClass::init ( void )
{
	
	fTaskSet = CFSetCreateMutable ( kCFAllocatorDefault, 0, NULL );
	if ( fTaskSet == NULL )
		return kIOReturnNoMemory;
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskDeviceClass::initWithConnection ( io_service_t service, io_connect_t connection )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	if ( ( service == NULL ) || ( connection == NULL ) )
	{
		return kIOReturnBadArgument;
	}
	
	status = init ( );
	if ( status != kIOReturnSuccess )
	{
		return status;
	}
	
	fService 					= service;
	fConnection 				= connection;
	fIsServicesLayerInterface 	= true;
	
	status = IOConnectAddRef ( fConnection );
	fAddedConnectRef = true;
	
	PRINT ( ( "IOConnectAddRef status = 0x%08x\n", status ) );
	
	return kIOReturnSuccess;
	
}


#pragma mark -
#pragma mark Protected Methods

// Default Constructor
SCSITaskDeviceClass::SCSITaskDeviceClass ( void )
{
	
	PRINT ( ( "SCSITaskDeviceClass constructor called\n" ) );
	
	// init CFPlugIn refcounting
	fRefCount = 0;
	
	// Init callback stuff
	fAsyncPort 			= MACH_PORT_NULL;
	fCFRunLoop 			= 0;
	fCFRunLoopSource 	= 0;
	fTaskSet			= 0;
	
	// init user client connection
	fConnection 				= MACH_PORT_NULL;
	fService 					= MACH_PORT_NULL;
	fAddedConnectRef 			= false;
	fHasExclusiveAccess 		= false;
	fIsServicesLayerInterface 	= false;
	
	// create plugin interface map
    fIOCFPlugInInterface.pseudoVTable = ( IUnknownVTbl * ) &sIOCFPlugInInterface;
    fIOCFPlugInInterface.obj 		  = this;
	
	// create test driver interface map
	fSCSITaskDeviceInterfaceMap.pseudoVTable = ( IUnknownVTbl * ) &sSCSITaskDeviceInterface;
	fSCSITaskDeviceInterfaceMap.obj 		 = this;
	
	fFactoryId = kIOSCSITaskLibFactoryID;
	CFRetain ( fFactoryId );
	CFPlugInAddInstanceForFactory ( fFactoryId );
	
}


// Destructor
SCSITaskDeviceClass::~SCSITaskDeviceClass ( void )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskDeviceClass : Destructor called\n" ) );
	
	if ( fAddedConnectRef )
	{
		
		PRINT ( ( "SCSITaskDeviceClass : fAddedConnectRef = true\n" ) );
		status = IOConnectRelease ( fConnection );
		fAddedConnectRef = false;
		
		PRINT ( ( "IOConnectRelease status = 0x%08x\n", status ) );
		
	}
	
	if ( fAsyncPort != NULL )
	{
		
		PRINT ( ( "SCSITaskDeviceClass : fAsyncPort != NULL\n" ) );
		IOObjectRelease ( fAsyncPort );
		fAsyncPort = MACH_PORT_NULL;
		
	}
	
	if ( fTaskSet != 0 )
	{
		
		PRINT ( ( "SCSITaskDeviceClass : fTaskSet != 0\n" ) );
		CFRelease ( fTaskSet );
		
	}
	
	PRINT ( ( "SCSITaskDeviceClass : calling CFPlugInRemoveInstanceForFactory\n" ) );
	
	CFPlugInRemoveInstanceForFactory ( fFactoryId );
	PRINT ( ( "SCSITaskDeviceClass : CFPlugInRemoveInstanceForFactory called\n" ) );
	
	CFRelease ( fFactoryId );
	PRINT ( ( "SCSITaskDeviceClass : CFRelease called\n" ) );
	
}



//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// staticQueryInterface
//
//

HRESULT
SCSITaskDeviceClass::staticQueryInterface ( void * self, REFIID iid, void ** ppv )
{
	return getThis ( self )->QueryInterface ( iid, ppv );
}


// QueryInterface
//

HRESULT
SCSITaskDeviceClass::QueryInterface ( REFIID iid, void ** ppv )
{
	
	CFUUIDRef 	uuid 	= CFUUIDCreateFromUUIDBytes ( NULL, iid );
	HRESULT 	result 	= S_OK;

	PRINT ( ( "SCSITaskDeviceClass : QueryInterface called\n" ) );

	if ( CFEqual ( uuid, IUnknownUUID ) || CFEqual ( uuid, kIOCFPlugInInterfaceID ) ) 
	{
        
		*ppv = &fIOCFPlugInInterface;
        AddRef ( );
		
    }
	
	else if ( CFEqual ( uuid, kIOSCSITaskDeviceInterfaceID ) ) 
	{

		PRINT ( ( "kSCSITaskDeviceUserClientInterfaceID requested\n" ) );

		*ppv = &fSCSITaskDeviceInterfaceMap;
        AddRef ( );
		
    }

    else
		*ppv = 0;
	
	if ( !*ppv )
		result = E_NOINTERFACE;
	
	CFRelease ( uuid );
	
	return result;
	
}


// staticAddRef
//

UInt32
SCSITaskDeviceClass::staticAddRef ( void * self )
{
	return getThis ( self )->AddRef ( );
}


// AddRef
//

UInt32
SCSITaskDeviceClass::AddRef ( void )
{

	PRINT ( ( "SCSITaskDeviceClass : AddRef\n" ) );
	
	fRefCount += 1;

	PRINT ( ( "fRefCount = %ld\n", fRefCount ) );

	return fRefCount;
	
}

// staticRelease
//

UInt32
SCSITaskDeviceClass::staticRelease ( void * self )
{
	return getThis ( self )->Release ( );
}


// Release
//

UInt32
SCSITaskDeviceClass::Release ( void )
{
	
	UInt32		retVal = fRefCount;

	PRINT ( ( "SCSITaskDeviceClass : Release\n" ) );
	PRINT ( ( "fRefCount = %ld\n", fRefCount ) );
	
	if ( 1 == fRefCount-- ) 
	{
		delete this;
    }
	
    else if ( fRefCount < 0 )
	{
        fRefCount = 0;
	}
	
	return retVal;
	
}


IOReturn
SCSITaskDeviceClass::staticProbe ( void * self, CFDictionaryRef propertyTable, 
								   io_service_t service, SInt32 * order )
{
	return getThis ( self )->Probe ( propertyTable, service, order );
}


IOReturn
SCSITaskDeviceClass::Probe ( CFDictionaryRef propertyTable,
							 io_service_t inService,
							 SInt32 * order )
{
	
	CFMutableDictionaryRef	dict;
	kern_return_t			kernErr;
	
	PRINT ( ( "SCSITaskDeviceClass::Probe called\n" ) );
	
	// Sanity check
	if ( inService == NULL )
		return kIOReturnBadArgument;
	
	kernErr = IORegistryEntryCreateCFProperties ( inService, &dict, NULL, 0 );
	if ( kernErr != KERN_SUCCESS )
	{
		return kIOReturnBadArgument;
	}
	
	if ( !CFDictionaryContainsKey ( dict, CFSTR ( "IOCFPlugInTypes" ) ) )
	{
		return kIOReturnBadArgument;
	}
		
	return kIOReturnSuccess;
	
}
	

IOReturn
SCSITaskDeviceClass::staticStart ( void * self,
										CFDictionaryRef propertyTable,
										io_service_t service )
{
	return getThis ( self )->Start ( propertyTable, service );
}


IOReturn
SCSITaskDeviceClass::Start ( CFDictionaryRef propertyTable, io_service_t service )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskDeviceClass : Start\n" ) );
	
	fService = service;
	status = IOServiceOpen ( fService, mach_task_self ( ), 
							 kSCSITaskLibConnection, &fConnection );
	
	if ( !fConnection )
		status = kIOReturnNoDevice;
	else
		fHasExclusiveAccess = true;
	
	PRINT ( ( "SCSITaskDeviceClass : IOServiceOpen status = 0x%08lx, connection = %d\n",
			( UInt32 ) status, fConnection ) );
	
	return status;
	
}


IOReturn
SCSITaskDeviceClass::staticStop ( void * self )
{
	return getThis ( self )->Stop ( );
}


IOReturn
SCSITaskDeviceClass::Stop ( void )
{
	
	PRINT ( ( "SCSITaskDeviceClass : Stop\n" ) );
	
	if ( fTaskSet )
	{
		CFRelease ( fTaskSet );
		fTaskSet = 0;
	}
	
	if ( fConnection )
	{
		
		PRINT ( ( "SCSITaskDeviceClass : IOServiceClose connection = %d\n", fConnection ) );
        IOServiceClose ( fConnection );
        fConnection = MACH_PORT_NULL;
		
	}
	
	return kIOReturnSuccess;
	
}

//////////////////////////////////////////////////////////////////
// SCSITaskDeviceInterface Methods


Boolean
SCSITaskDeviceClass::staticIsExclusiveAccessAvailable ( void * self )
{
	return getThis ( self )->IsExclusiveAccessAvailable ( );
}


Boolean
SCSITaskDeviceClass::IsExclusiveAccessAvailable ( void )
{
	
	Boolean					result 	= false;
	IOReturn				status 	= kIOReturnSuccess;
	mach_msg_type_number_t 	len 	= 0;
	
	if ( !fConnection )
		return result;	
	
	PRINT ( ( "SCSITaskDeviceClass : IsExclusiveAccessAvailable\n" ) );
	
	// We only have to do this if the services layer
	// is involved. Otherwise, its handled in Start.
	if ( fIsServicesLayerInterface )
	{
		
		status = io_connect_method_scalarI_scalarO ( fConnection,
												 kSCSITaskUserClientIsExclusiveAccessAvailable,
												 NULL, 0, NULL, &len );
		
		if ( status == kIOReturnSuccess )
		{
			result = true;
		}
		
	}
		
	else
	{
		result = true;
	}
	
	return result;
	
}


IOReturn
SCSITaskDeviceClass::staticAddCallbackDispatcherToRunLoop ( void * self, CFRunLoopRef cfRunLoopRef )
{
	return getThis ( self )->AddCallbackDispatcherToRunLoop ( cfRunLoopRef );
}


IOReturn
SCSITaskDeviceClass::AddCallbackDispatcherToRunLoop ( CFRunLoopRef cfRunLoopRef )
{
	
	IOReturn 			status = kIOReturnSuccess;
	CFMachPortRef		cfMachPort = NULL;
	
	PRINT ( ( "SCSITaskDeviceClass : AddCallbackDispatcherToRunLoop\n" ) );
	
	if ( !fConnection )		    
		return kIOReturnNoDevice;
	
	if ( status == kIOReturnSuccess )
	{
		
		CFMachPortContext	context;
		Boolean				shouldFreeInfo; // zzz what's this for?
		
		context.version 		= 1;
		context.info 			= this;
		context.retain 			= NULL;
		context.release 		= NULL;
		context.copyDescription = NULL;
		
		if ( fAsyncPort == NULL )
		{
			
			MyConnectionAndPortContext	context;
			IOCreateReceivePort ( kOSAsyncCompleteMessageID, &fAsyncPort );
			context.connection 	= fConnection;
			context.asyncPort 	= fAsyncPort;
			CFSetApplyFunction ( fTaskSet, &SCSITaskClass::sSetConnectionAndPort, &context );
			
		}
		
		cfMachPort = CFMachPortCreateWithPort ( kCFAllocatorDefault, fAsyncPort, 
							( CFMachPortCallBack ) IODispatchCalloutFromMessage,
							&context, &shouldFreeInfo );
		if ( !cfMachPort )
		{
			status = kIOReturnNoMemory;
		}
		
		PRINT ( ( "CFMachPortCreateWithPort\n" ) );
		
	}
		
	if ( status == kIOReturnSuccess )
	{
		
		fCFRunLoopSource = CFMachPortCreateRunLoopSource ( kCFAllocatorDefault, cfMachPort, 0 );
		if ( !fCFRunLoopSource )
			status = kIOReturnNoMemory;
		
		PRINT ( ( "CFMachPortCreateRunLoopSource\n" ) );
		
	}
	
	if ( cfMachPort != NULL )
		CFRelease ( cfMachPort );
		
	if ( status == kIOReturnSuccess )
	{
		
		fCFRunLoop = cfRunLoopRef;
		CFRunLoopAddSource ( fCFRunLoop, fCFRunLoopSource, kCFRunLoopDefaultMode );

		PRINT ( ( "CFRunLoopAddSource\n" ) );

	}
	
	if ( fCFRunLoopSource != NULL )
		CFRelease ( fCFRunLoopSource );
	
	return status;
	
}


void
SCSITaskDeviceClass::staticRemoveCallbackDispatcherFromRunLoop ( void * self )
{
	return getThis ( self )->RemoveCallbackDispatcherFromRunLoop ( );
}


void
SCSITaskDeviceClass::RemoveCallbackDispatcherFromRunLoop ( void )
{

	PRINT ( ( "SCSITaskDeviceClass : RemoveCallbackDispatcherFromRunLoop\n" ) );
	
	if ( fCFRunLoopSource )
	{
		
		CFRunLoopRemoveSource ( fCFRunLoop, fCFRunLoopSource, kCFRunLoopDefaultMode );
		fCFRunLoopSource = NULL;
		
		if ( fAsyncPort != NULL )
		{
			
			MyConnectionAndPortContext	context;
			
			IOObjectRelease ( fAsyncPort );
			fAsyncPort = MACH_PORT_NULL;
			
			context.connection 	= fConnection;
			context.asyncPort 	= fAsyncPort;
			
//			CFSetApplyFunction ( fTaskSet, &SCSITaskClass::sSetConnectionAndPort, &context );
			
		}
		
	}
	
}


IOReturn
SCSITaskDeviceClass::staticObtainExclusiveAccess ( void * self )
{
	return getThis ( self )->ObtainExclusiveAccess ( );
}


IOReturn
SCSITaskDeviceClass::ObtainExclusiveAccess ( void )
{
	
	IOReturn				status = kIOReturnSuccess;
	mach_msg_type_number_t 	len = 0;
	
	PRINT ( ( "SCSITaskDeviceClass : ObtainExclusiveAccess\n" ) );
	

	// We only have to do this if the services layer
	// is involved. Otherwise, its handled in Start.
	status = io_connect_method_scalarI_scalarO ( fConnection,
												 kSCSITaskUserClientObtainExclusiveAccess,
												 NULL, 0, NULL, &len );
	if ( status == kIOReturnSuccess )
	{
		
		PRINT ( ( "we have exclusive access\n" ) );
		fHasExclusiveAccess = true;
		
	}
	
	return status;
	
}


IOReturn
SCSITaskDeviceClass::staticReleaseExclusiveAccess ( void * self )
{
	return getThis ( self )->ReleaseExclusiveAccess ( );
}


IOReturn
SCSITaskDeviceClass::ReleaseExclusiveAccess ( void )
{
	
	IOReturn				status = kIOReturnExclusiveAccess;
	mach_msg_type_number_t 	len = 0;
	
	PRINT ( ( "SCSITaskDeviceClass : ReleaseExclusiveAccess\n" ) );
	
	if ( !fHasExclusiveAccess )
		return status;
	
	if ( fIsServicesLayerInterface )
	{
	
		status = io_connect_method_scalarI_scalarO ( fConnection,
												 kSCSITaskUserClientReleaseExclusiveAccess,
												 NULL, 0, NULL, &len );
		
		PRINT ( ( "ReleaseExclusiveAccess status = 0x%08x\n", status ) );
		
	}
	
	return status;
	
}


SCSITaskInterface **
SCSITaskDeviceClass::staticCreateSCSITask ( void * self )
{
	return getThis ( self )->CreateSCSITask ( );
}


SCSITaskInterface **
SCSITaskDeviceClass::CreateSCSITask ( void )
{
	
	SCSITaskInterface **	interface = NULL;

	PRINT ( ( "SCSITaskDeviceClass : CreateSCSITask\n" ) );
	
	if ( !fHasExclusiveAccess )
		return NULL;
	
	interface = SCSITaskClass::alloc ( this, fConnection, fAsyncPort );
	if ( interface != NULL )
	{
		
		CFSetAddValue ( fTaskSet, interface );
		
	}
	
	return interface;
	
}


void
SCSITaskDeviceClass::RemoveTaskFromTaskSet ( SCSITaskInterface ** task )
{
	
	if ( fTaskSet )
	{
		CFSetRemoveValue ( fTaskSet, task );
	}
	
}