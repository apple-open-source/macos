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

#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>
#include "MMCDeviceUserClientClass.h"
#include "SCSITaskDeviceClass.h"
#include <string.h>

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#define MMC_DEVICE_CLASS_DEBUGGING_LEVEL 0

#if ( MMC_DEVICE_CLASS_DEBUGGING_LEVEL > 0 )
#define PRINT(x)	printf x
#else
#define PRINT(x)
#endif

IOCFPlugInInterface
MMCDeviceUserClientClass::sIOCFPlugInInterface =
{
	0,
	&MMCDeviceUserClientClass::staticQueryInterface,
	&MMCDeviceUserClientClass::staticAddRef,
	&MMCDeviceUserClientClass::staticRelease,
	1, 0, // version/revision
	&MMCDeviceUserClientClass::staticProbe,
	&MMCDeviceUserClientClass::staticStart,
	&MMCDeviceUserClientClass::staticStop
};

MMCDeviceInterface
MMCDeviceUserClientClass::sMMCDeviceInterface =
{
	0,
	&MMCDeviceUserClientClass::staticQueryInterface,
	&MMCDeviceUserClientClass::staticAddRef,
	&MMCDeviceUserClientClass::staticRelease,
	1, 0, // version/revision
	&MMCDeviceUserClientClass::staticInquiry,
	&MMCDeviceUserClientClass::staticTestUnitReady,
	&MMCDeviceUserClientClass::staticGetPerformance,
	&MMCDeviceUserClientClass::staticGetConfiguration,
	&MMCDeviceUserClientClass::staticModeSense10,
	&MMCDeviceUserClientClass::staticSetWriteParametersModePage,
	&MMCDeviceUserClientClass::staticGetTrayState,
	&MMCDeviceUserClientClass::staticSetTrayState,
	&MMCDeviceUserClientClass::staticReadTableOfContents,
	&MMCDeviceUserClientClass::staticReadDiscInformation,
	&MMCDeviceUserClientClass::staticReadTrackInformation,
	&MMCDeviceUserClientClass::staticReadDVDStructure,
	&MMCDeviceUserClientClass::staticGetSCSITaskDeviceInterface
};


#pragma mark Public Methods

IOCFPlugInInterface **
MMCDeviceUserClientClass::alloc ( void )
{
	
	MMCDeviceUserClientClass * 	userClient;
	IOCFPlugInInterface ** 		interface = NULL;

	PRINT ( ( "MMCDeviceUserClientClass::alloc called\n" ) );
	
	userClient = new MMCDeviceUserClientClass;
	if ( userClient != NULL )
	{
		
		userClient->AddRef ( );
		interface = ( IOCFPlugInInterface ** ) &userClient->fMMCDeviceInterfaceMap.pseudoVTable;
		
	}
	else
	{
		PRINT ( ( "userClient is null\n" ) );
	}
	
	PRINT ( ( "returning interface\n" ) );

	return interface;
	
}


#pragma mark -
#pragma mark Protected Methods

// Constructor
MMCDeviceUserClientClass::MMCDeviceUserClientClass ( void )
{
	
	PRINT ( ( "MMCDeviceUserClientClass constructor called\n" ) );
		
	// init cf plugin ref counting
	fRefCount = 0;
	
	// init user client connection
	fConnection = MACH_PORT_NULL;
	fService 	= MACH_PORT_NULL;
		
	// create plugin interface map
    fIOCFPlugInInterface.pseudoVTable 	= ( IUnknownVTbl * ) &sIOCFPlugInInterface;
    fIOCFPlugInInterface.obj 			= this;
	
	// create test driver interface map
	fMMCDeviceInterfaceMap.pseudoVTable = ( IUnknownVTbl * ) &sMMCDeviceInterface;
	fMMCDeviceInterfaceMap.obj 			= this;
	
	fFactoryId = kIOSCSITaskLibFactoryID;
	CFRetain ( fFactoryId );
	CFPlugInAddInstanceForFactory ( fFactoryId );
	
}


// Destructor
MMCDeviceUserClientClass::~MMCDeviceUserClientClass ( void )
{
	
	kern_return_t	err = 0;
	
	PRINT ( ( "MMCDeviceUserClientClass destructor called\n" ) );
	
	if ( fConnection != MACH_PORT_NULL )
	{
		
		err = IOServiceClose ( fConnection );
		fConnection = MACH_PORT_NULL;
		
		PRINT ( ( "MMCDeviceUserClientClass : IOServiceClose returned err = 0x%08x\n", err ) );
		
	}
	
	PRINT ( ( "MMCDeviceUserClientClass : calling CFPlugInRemoveInstanceForFactory\n" ) );
	
	CFPlugInRemoveInstanceForFactory ( fFactoryId );
	PRINT ( ( "MMCDeviceUserClientClass : CFPlugInRemoveInstanceForFactory called\n" ) );

	CFRelease ( fFactoryId );
	PRINT ( ( "MMCDeviceUserClientClass : CFRelease called\n" ) );
	
}



//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// staticQueryInterface
//
//

//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// staticQueryInterface
//

HRESULT
MMCDeviceUserClientClass::staticQueryInterface ( void * self, REFIID iid, void ** ppv )
{
	return getThis ( self )->QueryInterface ( iid, ppv );
}


// QueryInterface
//

HRESULT
MMCDeviceUserClientClass::QueryInterface ( REFIID iid, void ** ppv )
{
	
	CFUUIDRef 	uuid 	= CFUUIDCreateFromUUIDBytes ( NULL, iid );
	HRESULT 	result 	= S_OK;
	
	PRINT ( ( "MMCDeviceUserClientClass QueryInterface called\n" ) );
	
	if ( CFEqual ( uuid, IUnknownUUID ) || CFEqual ( uuid, kIOCFPlugInInterfaceID ) ) 
	{
		
		PRINT ( ( "UUID recognized, unknownUUID or IOCFPlugin UUID\n" ) );
		
		*ppv = &fIOCFPlugInInterface;
        AddRef ( );
		
    }
	
	else if ( CFEqual ( uuid, kIOMMCDeviceInterfaceID ) ) 
	{
		PRINT ( ( "MMC UUID recognized\n" ) );
		*ppv = &fMMCDeviceInterfaceMap;
		AddRef ( );
    }

    else
	{
		PRINT ( ( "UUID unrecognized\n" ) );
		*ppv = 0;
	}
	
	if ( !*ppv )
		result = E_NOINTERFACE;
	
	CFRelease ( uuid );
	
	return result;
	
}


// staticAddRef
//

UInt32
MMCDeviceUserClientClass::staticAddRef ( void * self )
{
	return getThis ( self )->AddRef ( );
}


// AddRef
//

UInt32
MMCDeviceUserClientClass::AddRef ( void )
{

	PRINT ( ( "MMCDeviceUserClientClass AddRef called\n" ) );
	
	fRefCount += 1;

	PRINT ( ( "fRefCount = %ld\n", fRefCount ) );

	return fRefCount;
	
}

// staticRelease
//

UInt32
MMCDeviceUserClientClass::staticRelease ( void * self )
{
	return getThis ( self )->Release ( );
}


// Release
//

UInt32
MMCDeviceUserClientClass::Release ( void )
{
	
	UInt32		retVal = fRefCount;

	PRINT ( ( "MMCDeviceUserClientClass Release called\n" ) );
	
	if ( 1 == fRefCount-- ) 
	{
		delete this;
    }
	
    else if ( fRefCount < 0 )
	{
        fRefCount = 0;
	}

	PRINT ( ( "fRefCount = %ld\n", fRefCount ) );
	
	return retVal;
	
}


IOReturn
MMCDeviceUserClientClass::staticProbe ( void * self, CFDictionaryRef propertyTable, 
										io_service_t service, SInt32 * order )
{
	return getThis ( self )->Probe ( propertyTable, service, order );
}


IOReturn
MMCDeviceUserClientClass::Probe ( CFDictionaryRef propertyTable,
								  io_service_t inService,
								  SInt32 * order )
{
	
	CFMutableDictionaryRef	dict;
	kern_return_t			kernErr;
	
	PRINT ( ( "MMCDeviceUserClientClass::Probe called\n" ) );
	
	// Sanity check
	if ( inService == NULL )
		return kIOReturnBadArgument;
	
	kernErr = IORegistryEntryCreateCFProperties ( inService, &dict, NULL, 0 );
	if ( kernErr != KERN_SUCCESS )
	{
		return kIOReturnBadArgument;
	}
	
	if ( CFDictionaryContainsKey ( dict, CFSTR ( kIOPropertySCSITaskUserClientInstanceGUID ) ) )
	{
		
		// Check to see if it conforms to CD or DVD services layer object names
		if ( !IOObjectConformsTo ( inService, "IOCompactDiscServices" ) &&
			!IOObjectConformsTo ( inService, "IODVDServices" ) )
		{
			
			PRINT ( ( "MMCDeviceUserClientClass::Probe not our service type\n" ) );
			return kIOReturnBadArgument;
			
		}
		
	}
	
	else
	{
		return kIOReturnBadArgument;
	}
	
	return kIOReturnSuccess;
	
}
	

IOReturn
MMCDeviceUserClientClass::staticStart ( void * self,
										CFDictionaryRef propertyTable,
										io_service_t service )
{
	return getThis ( self )->Start ( propertyTable, service );
}


IOReturn
MMCDeviceUserClientClass::Start ( CFDictionaryRef propertyTable, io_service_t service )
{
	
	IOReturn 	status 		= kIOReturnSuccess;
	
	PRINT ( ( "MMCDeviceUserClientClass : start\n" ) );
	
	fService = service;
	status = IOServiceOpen ( fService, mach_task_self ( ), 
							 kSCSITaskLibConnection, &fConnection );
	
	if ( !fConnection )
		status = kIOReturnNoDevice;
		
	PRINT ( ( "MMCDeviceUserClientClass : IOServiceOpen status = 0x%08lx, connection = %d\n",
			( UInt32 ) status, fConnection ) );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticStop ( void * self )
{
	return getThis ( self )->Stop ( );
}


IOReturn
MMCDeviceUserClientClass::Stop ( void )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	PRINT ( ( "MMCDeviceUserClientClass : stop\n" ) );
	
	if ( fConnection )
	{
		
		PRINT ( ( "MMCDeviceUserClientClass : IOServiceClose connection = %d\n", fConnection ) );
        status = IOServiceClose ( fConnection );
        fConnection = MACH_PORT_NULL;
        
        PRINT ( ( "IOServiceClose status = 0x%08x\n", status ) );
    	
	}
	
	return kIOReturnSuccess;
	
}


//////////////////////////////////////////////////////////////////
// MMCDeviceInterface methods
//

IOReturn
MMCDeviceUserClientClass::staticInquiry ( void * self,
										  SCSICmd_INQUIRY_StandardData * inquiryBuffer,
										  UInt32 inqBufferSize,
										  SCSITaskStatus * outTaskStatus,
										  SCSI_Sense_Data * senseDataBuffer )
{
	return getThis ( self )->Inquiry ( inquiryBuffer, inqBufferSize, outTaskStatus, senseDataBuffer );
}


IOReturn
MMCDeviceUserClientClass::Inquiry ( SCSICmd_INQUIRY_StandardData * inquiryBuffer,
									UInt32 inqBufferSize,
									SCSITaskStatus * outTaskStatus,
									SCSI_Sense_Data * senseDataBuffer  )
{
	
	IOReturn					status;
	UInt32						params[3];
	UInt32						outParams[1];
	mach_msg_type_number_t 		len = 1;
	
	params[0] = ( UInt32 ) inqBufferSize;
	params[1] = ( UInt32 ) inquiryBuffer;
	params[2] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceInquiry,
												 ( int * ) params, 3,
												 ( int * ) outParams, &len );
	
	PRINT ( ( "MMCDeviceUserClientClass : Inquiry status = 0x%08x\n", status ) );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticTestUnitReady ( void * self,
												SCSITaskStatus * outTaskStatus,
												SCSI_Sense_Data * senseDataBuffer  )
{
	return getThis ( self )->TestUnitReady ( outTaskStatus, senseDataBuffer );	
}


IOReturn
MMCDeviceUserClientClass::TestUnitReady ( SCSITaskStatus * outTaskStatus,
										  SCSI_Sense_Data * senseDataBuffer  )
{
	
	IOReturn					status;
	UInt32						params[1];
	UInt32						outParams[1];
	mach_msg_type_number_t 		len = 1;
	
	params[0] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceTestUnitReady, 
												 ( int * ) params, 1,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : TestUnitReady taskStatus = %d\n", *outTaskStatus ) );
	
	if ( *outTaskStatus == kSCSITaskStatus_CHECK_CONDITION )
	{
		
		PRINT ( ( "SENSE_KEY_CODE: 0x%08x, ASC: 0x%08x, ASCQ: 0x%08x\n",
			   senseDataBuffer->SENSE_KEY & kSENSE_KEY_Mask,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE_QUALIFIER ) );
		
	}
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticGetPerformance ( void * self, UInt8 TOLERANCE, UInt8 WRITE, UInt8 EXCEPT,
											   UInt32 STARTING_LBA, UInt16 MAXIMUM_NUMBER_OF_DESCRIPTORS,
											   void * buffer, UInt16 bufferSize, SCSITaskStatus * taskStatus,
											   SCSI_Sense_Data * senseDataBuffer )
{
	
	return getThis ( self )->GetPerformance ( TOLERANCE, WRITE, EXCEPT, STARTING_LBA, MAXIMUM_NUMBER_OF_DESCRIPTORS,
											  buffer, bufferSize, taskStatus, senseDataBuffer );
										 
}


IOReturn
MMCDeviceUserClientClass::GetPerformance ( UInt8 TOLERANCE, UInt8 WRITE, UInt8 EXCEPT, UInt32 STARTING_LBA,
										  UInt16 MAXIMUM_NUMBER_OF_DESCRIPTORS, void * buffer, UInt16 bufferSize,
										  SCSITaskStatus * outTaskStatus, SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn					status;
	mach_msg_type_number_t 		len = 1;
	UInt32 						params[5];
	UInt32						outParams[1];
		
	params[0] = ( UInt32 ) ( ( TOLERANCE << 16 ) | ( WRITE << 8 ) | EXCEPT );
	params[1] = ( UInt32 ) ( STARTING_LBA );
	params[2] = ( UInt32 ) ( ( MAXIMUM_NUMBER_OF_DESCRIPTORS << 16 ) | bufferSize );
	params[3] = ( UInt32 ) buffer;
	params[4] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceGetConfiguration, 
												 ( int * ) params, 5,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : GetConfiguration status = %d\n", status ) );
	
	return status;
	
}

		
IOReturn
MMCDeviceUserClientClass::staticGetConfiguration ( void * self, UInt8 RT, UInt16 STARTING_FEATURE_NUMBER,
												 void * buffer, UInt16 bufferSize, SCSITaskStatus * taskStatus,
												 SCSI_Sense_Data * senseDataBuffer )
{
	
	return getThis ( self )->GetConfiguration ( RT, STARTING_FEATURE_NUMBER, buffer, bufferSize,
												taskStatus, senseDataBuffer );
	
}

IOReturn
MMCDeviceUserClientClass::GetConfiguration ( UInt8 RT, UInt16 STARTING_FEATURE_NUMBER,
											void * buffer, UInt16 bufferSize, SCSITaskStatus * outTaskStatus,
											SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn					status;
	mach_msg_type_number_t 		len = 1;
	UInt32 						params[5];
	UInt32						outParams[1];
	
	if ( RT > 0x02 )
		return kIOReturnBadArgument;
	
	params[0] = ( UInt32 ) ( RT );
	params[1] = ( UInt32 ) ( STARTING_FEATURE_NUMBER );
	params[2] = ( UInt32 ) buffer;
	params[3] = ( UInt32 ) bufferSize;
	params[4] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceGetConfiguration, 
												 ( int * ) params, 5,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : GetConfiguration status = %d\n", status ) );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticModeSense10 ( void * self, UInt8 LLBAA, UInt8 DBD, UInt8 PC,
											  UInt8 PAGE_CODE, void * buffer, UInt16 bufferSize,
											  SCSITaskStatus * outTaskStatus, SCSI_Sense_Data * senseDataBuffer )
{
	return getThis ( self )->ModeSense10 ( LLBAA, DBD, PC, PAGE_CODE, buffer,
										   bufferSize, outTaskStatus, senseDataBuffer );	
}


IOReturn
MMCDeviceUserClientClass::ModeSense10 ( UInt8 LLBAA, UInt8 DBD, UInt8 PC, UInt8 PAGE_CODE,
										void * buffer, UInt16 bufferSize,
										SCSITaskStatus * outTaskStatus,
										SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn					status;
	mach_msg_type_number_t 		len = 1;
	UInt32 						params[5];
	UInt32						outParams[1];
	
	params[0] = ( UInt32 ) ( ( DBD & 0x01 << 2 ) | ( LLBAA & 0x01 << 3 ) );
	params[1] = ( UInt32 ) ( ( PC & 0x04 << 5 ) | ( PAGE_CODE & 0x3F ) );
	params[2] = ( UInt32 ) buffer;
	params[3] = ( UInt32 ) bufferSize;
	params[4] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceModeSense10, 
												 ( int * ) params, 5,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : ModeSense10 status = %d\n", status ) );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticSetWriteParametersModePage ( void * self, void * buffer, UInt8 bufferSize,
														     SCSITaskStatus * taskStatus, SCSI_Sense_Data * senseDataBuffer )
{
	
	return getThis ( self )->SetWriteParametersModePage ( buffer, bufferSize, taskStatus, senseDataBuffer );
	
}


IOReturn
MMCDeviceUserClientClass::SetWriteParametersModePage ( void * buffer, UInt8 bufferSize,
													   SCSITaskStatus * outTaskStatus,
													   SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn					status;
	mach_msg_type_number_t 		len = 1;
	UInt32 						params[3];
	UInt32						outParams[1];
	
	if ( ( ( ( UInt8 * ) buffer )[8] & 0x3F ) != 0x05 )
		return kIOReturnBadArgument;
	
	params[0] = ( UInt32 ) buffer;
	params[1] = bufferSize;
	params[2] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceSetWriteParametersModePage, 
												 ( int * ) params, 3,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : SetWriteParametersModePage status = %d\n", status ) );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticGetTrayState ( void * self, UInt8 * trayState )
{
	return getThis ( self )->GetTrayState ( trayState );	
}


IOReturn
MMCDeviceUserClientClass::GetTrayState ( UInt8 * trayState )
{
	
	IOReturn				status;
	UInt32					outParams[1];
	mach_msg_type_number_t 	len = 1;
		
	status = io_connect_method_scalarI_scalarO ( fConnection, 					 
												 kMMCDeviceGetTrayState, 
												 NULL, 0,
												 ( int * ) outParams, &len );
	
	*trayState = ( UInt8 )( ( outParams[0] != 0 ) ? kMMCDeviceTrayOpen : kMMCDeviceTrayClosed );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticSetTrayState ( void * self, UInt8 trayState )
{
	return getThis ( self )->SetTrayState ( trayState );	
}


IOReturn
MMCDeviceUserClientClass::SetTrayState ( UInt8 trayState )
{
	
	IOReturn				status;
	UInt32					params[1];
	mach_msg_type_number_t 	len = 0;
	
	if ( ( trayState != kMMCDeviceTrayOpen ) &&
		 ( trayState != kMMCDeviceTrayClosed ) )
		return kIOReturnBadArgument;
	
	params[0] = trayState;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 					 
												 kMMCDeviceSetTrayState, 
												 ( int * ) params, 1,
												 NULL, &len );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticReadTableOfContents ( void * self, UInt8 MSF,
													  UInt8 format, UInt8 trackSessionNumber,
													  void * buffer, UInt16 bufferSize,
													  SCSITaskStatus * outTaskStatus,
													  SCSI_Sense_Data * senseDataBuffer )
{
	return getThis ( self )->ReadTableOfContents ( MSF, format, trackSessionNumber,
												   buffer, bufferSize, outTaskStatus,
												   senseDataBuffer );	
}


IOReturn
MMCDeviceUserClientClass::ReadTableOfContents ( UInt8 MSF,
												UInt8 format,
												UInt8 trackSessionNumber,
												void * buffer,
												UInt16 bufferSize,
												SCSITaskStatus * outTaskStatus,
												SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						params[5];
	UInt32						outParams[1];
	
	params[0] = ( UInt32 ) ( ( MSF << 8 ) | format );
	params[1] = ( UInt32 ) trackSessionNumber;
	params[2] = ( UInt32 ) buffer;
	params[3] = ( UInt32 ) bufferSize;
	params[4] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceReadTableOfContents, 
												 ( int * ) params, 5,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : ReadTableOfContents status = 0x%08x\n", status ) );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticReadDiscInformation ( void * self,
													  void * buffer,
													  UInt16 bufferSize,
													  SCSITaskStatus * outTaskStatus,
													  SCSI_Sense_Data * senseDataBuffer )
{
	return getThis ( self )->ReadDiscInformation ( buffer, bufferSize, outTaskStatus, senseDataBuffer );	
}


IOReturn
MMCDeviceUserClientClass::ReadDiscInformation ( void * buffer, UInt16 bufferSize,
												SCSITaskStatus * outTaskStatus,
												SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						params[3];
	UInt32						outParams[1];
	
	params[0] = ( UInt32 ) buffer;
	params[1] = ( UInt32 ) bufferSize;
	params[2] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceReadDiscInformation, 
												 ( int * ) params, 3,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : ReadDiscInformation status = 0x%08x\n", status ) );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticReadTrackInformation ( void * self,
													  UInt8 addressNumberType,
													  UInt32 lbaTrackSessionNumber,
													  void * buffer,
													  UInt16 bufferSize,
													  SCSITaskStatus * outTaskStatus,
													  SCSI_Sense_Data * senseDataBuffer )
{
	return getThis ( self )->ReadTrackInformation ( addressNumberType,
													lbaTrackSessionNumber,
													buffer,
													bufferSize,
													outTaskStatus,
													senseDataBuffer );	
}


IOReturn
MMCDeviceUserClientClass::ReadTrackInformation ( UInt8 addressNumberType,
												 UInt32 lbaTrackSessionNumber,
												 void * buffer,
												 UInt16 bufferSize,
												 SCSITaskStatus * outTaskStatus,
												 SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						params[5];
	UInt32						outParams[1];

	params[0] = ( UInt32 ) addressNumberType;
	params[1] = ( UInt32 ) lbaTrackSessionNumber;
	params[2] = ( UInt32 ) buffer;
	params[3] = ( UInt32 ) bufferSize;
	params[4] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceReadTrackInformation, 
												 ( int * ) params, 5,
												 ( int * ) outParams, &len );
	
	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : ReadTrackInformation status = 0x%08x\n", status ) );
	
	return status;
	
}


IOReturn
MMCDeviceUserClientClass::staticReadDVDStructure ( void * self,
										 		   UInt32 logicalBlockAddress,
										 		   UInt8 layerNumber,
										 		   UInt8 format,
										 		   void * buffer,
										 		   UInt16 bufferSize,
												   SCSITaskStatus * outTaskStatus,
												   SCSI_Sense_Data * senseDataBuffer )
{
	return getThis ( self )->ReadDVDStructure ( logicalBlockAddress,
												layerNumber,
												format,
												buffer,
												bufferSize,
												outTaskStatus,
												senseDataBuffer );	
}


IOReturn
MMCDeviceUserClientClass::ReadDVDStructure ( UInt32 logicalBlockAddress,
											 UInt8 layerNumber,
										 	 UInt8 format,
										 	 void * buffer,
										 	 UInt16 bufferSize,
											 SCSITaskStatus * outTaskStatus,
											 SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						params[5];
	UInt32						outParams[1];
		
	params[0] = ( UInt32 ) logicalBlockAddress;
	params[1] = ( UInt32 ) ( layerNumber << 8 ) | format;
	params[2] = ( UInt32 ) buffer;
	params[3] = ( UInt32 ) bufferSize;
	params[4] = ( UInt32 ) senseDataBuffer;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kMMCDeviceReadDVDStructure, 
												 ( int * ) params, 5,
												 ( int * ) outParams, &len );

	*outTaskStatus = ( SCSITaskStatus ) outParams[0];
	
	PRINT ( ( "MMCDeviceUserClientClass : ReadDVDStructure status = 0x%08x\n", status ) );
	
	return status;
	
}


SCSITaskDeviceInterface **
MMCDeviceUserClientClass::staticGetSCSITaskDeviceInterface ( void * self )
{
	return getThis ( self )->GetSCSITaskDeviceInterface ( );
}


SCSITaskDeviceInterface **
MMCDeviceUserClientClass::GetSCSITaskDeviceInterface ( void )
{
	return ( SCSITaskDeviceInterface ** ) SCSITaskDeviceClass::alloc ( fService, fConnection );
}