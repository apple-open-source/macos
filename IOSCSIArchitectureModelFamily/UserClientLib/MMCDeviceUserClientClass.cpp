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
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CoreFoundation.h>

// IOKit includes
#include <IOKit/IOKitLib.h>

// IOSCSIArchitectureModelFamily includes
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>

// Authorization includes
#include <Security/Authorization.h>

// Private includes
#include "MMCDeviceUserClientClass.h"
#include "SCSITaskDeviceClass.h"

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
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"MMCDeviceClass"

#if DEBUG
#define PRINT(x)		printf x
#else
#define PRINT(x)
#endif

#include "IOSCSIArchitectureModelFamilyDebugging.h"

#define CHECK_AUTHORIZATION_RIGHTS 1

#if CHECK_AUTHORIZATION_RIGHTS

#define kSystemWideBurnAuthorizationRight		"system.burn"
#define kBurningAuthorizationFlags				kAuthorizationFlagDefaults | \
												kAuthorizationFlagInteractionAllowed | \
												kAuthorizationFlagExtendRights | \
												kAuthorizationFlagDestroyRights
#endif /* CHECK_AUTHORIZATION_RIGHTS */

//—————————————————————————————————————————————————————————————————————————————
//	Static variable initialization
//—————————————————————————————————————————————————————————————————————————————

IOCFPlugInInterface
MMCDeviceUserClientClass::sIOCFPlugInInterface =
{
	0,
	&MMCDeviceUserClientClass::sQueryInterface,
	&MMCDeviceUserClientClass::sAddRef,
	&MMCDeviceUserClientClass::sRelease,
	1, 0, // version/revision
	&MMCDeviceUserClientClass::sProbe,
	&MMCDeviceUserClientClass::sStart,
	&MMCDeviceUserClientClass::sStop
};


MMCDeviceInterface
MMCDeviceUserClientClass::sMMCDeviceInterface =
{
	0,
	&MMCDeviceUserClientClass::sQueryInterface,
	&MMCDeviceUserClientClass::sAddRef,
	&MMCDeviceUserClientClass::sRelease,
	1, 0, // version/revision
	&MMCDeviceUserClientClass::sInquiry,
	&MMCDeviceUserClientClass::sTestUnitReady,
	&MMCDeviceUserClientClass::sGetPerformance,
	&MMCDeviceUserClientClass::sGetConfiguration,
	&MMCDeviceUserClientClass::sModeSense10,
	&MMCDeviceUserClientClass::sSetWriteParametersModePage,
	&MMCDeviceUserClientClass::sGetTrayState,
	&MMCDeviceUserClientClass::sSetTrayState,
	&MMCDeviceUserClientClass::sReadTableOfContents,
	&MMCDeviceUserClientClass::sReadDiscInformation,
	&MMCDeviceUserClientClass::sReadTrackInformation,
	&MMCDeviceUserClientClass::sReadDVDStructure,
	&MMCDeviceUserClientClass::sGetSCSITaskDeviceInterface,
	&MMCDeviceUserClientClass::sGetPerformanceV2,
	&MMCDeviceUserClientClass::sSetCDSpeed,
	&MMCDeviceUserClientClass::sReadFormatCapacities
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
MMCDeviceUserClientClass::alloc ( void )
{
	
	MMCDeviceUserClientClass * 	userClient	= NULL;
	IOCFPlugInInterface ** 		interface 	= NULL;
	
	PRINT ( ( "MMCDeviceUserClientClass::alloc called\n" ) );
	
	userClient = new MMCDeviceUserClientClass;
	require_nonzero_string ( userClient, Error_Exit, "new failed" );
	
	userClient->AddRef ( );
	interface = ( IOCFPlugInInterface ** ) &userClient->fInterfaceMap.pseudoVTable;
	
	
Error_Exit:
	
	
	return interface;
	
}


inline bool
MMCDeviceUserClientClass::IsParameterValid ( 	SCSICmdField1Byte 	param,
												SCSICmdField1Byte 	mask )
{
	
	bool	valid = false;
	
	require ( ( ( param | mask ) == mask ), Error_Exit );
	valid = true;
	
	
Error_Exit:
	
	
	return valid;
	
}


inline bool
MMCDeviceUserClientClass::IsParameterValid ( 	SCSICmdField2Byte 	param,
												SCSICmdField2Byte 	mask )
{
	
	bool	valid = false;
	
	require ( ( ( param | mask ) == mask ), Error_Exit );
	valid = true;
	
	
Error_Exit:
	
	
	return valid;
	
}


inline bool
MMCDeviceUserClientClass::IsParameterValid ( 	SCSICmdField4Byte 	param,
												SCSICmdField4Byte 	mask )
{
	
	bool	valid = false;
	
	require ( ( ( param | mask ) == mask ), Error_Exit );
	valid = true;
	
	
Error_Exit:
	
	
	return valid;
	
}


#if 0
#pragma mark -
#pragma mark Protected Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• Default Constructor - Called on allocation					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

MMCDeviceUserClientClass::MMCDeviceUserClientClass ( void ) :
				SCSITaskIUnknown ( &sIOCFPlugInInterface )
{
	
	PRINT ( ( "MMCDeviceUserClientClass constructor called\n" ) );
	
	// init user client connection
	fConnection = MACH_PORT_NULL;
	fService 	= MACH_PORT_NULL;
	
	// create driver interface map
	fMMCDeviceInterfaceMap.pseudoVTable = ( IUnknownVTbl * ) &sMMCDeviceInterface;
	fMMCDeviceInterfaceMap.obj 			= this;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Default Destructor - Called on deallocation					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

MMCDeviceUserClientClass::~MMCDeviceUserClientClass ( void )
{
	
	PRINT ( ( "MMCDeviceUserClientClass destructor called\n" ) );
	Stop ( );
	
}


#if 0
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• QueryInterface - Called to obtain the presence of an interface
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

HRESULT
MMCDeviceUserClientClass::QueryInterface ( 	REFIID 	iid,
											void ** ppv )
{
	
	CFUUIDRef 	uuid 	= CFUUIDCreateFromUUIDBytes ( NULL, iid );
	HRESULT 	result 	= S_OK;
	
	PRINT ( ( "MMCDeviceUserClientClass QueryInterface called\n" ) );
	
	if ( CFEqual ( uuid, IUnknownUUID ) ||
		 CFEqual ( uuid, kIOCFPlugInInterfaceID ) ) 
	{
		
		PRINT ( ( "UUID recognized, unknownUUID or IOCFPlugin UUID\n" ) );
		
		*ppv = &fInterfaceMap;
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
MMCDeviceUserClientClass::Probe ( CFDictionaryRef 	propertyTable,
								  io_service_t 		inService,
								  SInt32 * 			order )
{
	
	CFMutableDictionaryRef	dict	= 0;
	IOReturn				status 	= kIOReturnBadArgument;
	Boolean					ok		= false;

	PRINT ( ( "MMC::Probe called\n" ) );
	
	// Sanity check
	require_nonzero ( inService, Error_Exit );
		
	status = IORegistryEntryCreateCFProperties ( inService, &dict, NULL, 0 );
	require_success ( status, Error_Exit );
	
	ok = CFDictionaryContainsKey (
				dict,
				CFSTR ( kIOPropertySCSITaskUserClientInstanceGUID ) );
	require_action ( ok, ReleaseDictionary, status = kIOReturnNoDevice );
	
	// Check to see if it conforms to CD or DVD services layer object names
	ok = IOObjectConformsTo ( inService, "IOCDBlockStorageDevice" ) ? true : false;
	require_action ( ok, ReleaseDictionary, status = kIOReturnNoDevice );
	
	
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
MMCDeviceUserClientClass::Start (	CFDictionaryRef 	propertyTable,
									io_service_t 		service )
{
	
	IOReturn 				result	= kIOReturnNoDevice;
	
#if CHECK_AUTHORIZATION_RIGHTS
	
	OSStatus				status	= noErr;
	AuthorizationItem		item	= { 0 };
	AuthorizationItemSet	itemSet = { 0 };
	
	PRINT ( ( "SCSITaskDeviceClass : Start\n" ) );
	
	item.name		= kSystemWideBurnAuthorizationRight;
	itemSet.count	= 1;
	itemSet.items	= &item;
	
	status = AuthorizationCreate ( &itemSet,
								   kAuthorizationEmptyEnvironment,
								   kBurningAuthorizationFlags,
								   NULL );
	
	require_noerr_action ( status, Error_Exit, result = kIOReturnNotPermitted );
	
#endif /* CHECK_AUTHORIZATION_RIGHTS */
	
	fService = service;
	result = IOServiceOpen ( fService,
							 mach_task_self ( ),
							 kSCSITaskLibConnection,
							 &fConnection );
	
	require_success ( result, Error_Exit );
	require_nonzero_action ( fConnection, Error_Exit, result = kIOReturnNoDevice );
	
	
Error_Exit:
	
	
	PRINT ( ( "MMC : IOServiceOpen result = 0x%08lx, connection = %d\n",
			( UInt32 ) result, fConnection ) );
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Stop - Called to stop providing our services.					[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::Stop ( void )
{
	
	PRINT ( ( "MMC : stop\n" ) );
	
	if ( fConnection != 0 )
	{
		
		PRINT ( ( "MMC : IOServiceClose connection = %d\n", fConnection ) );
        IOServiceClose ( fConnection );
        fConnection = MACH_PORT_NULL;
		
	}
	
	return kIOReturnSuccess;
	
}


#if 0
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• Inquiry - Called to issue an INQUIRY command as defined in SPC-2.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::Inquiry (
					SCSICmd_INQUIRY_StandardData *	inquiryBuffer,
					SCSICmdField1Byte 				inqBufferSize,
					SCSITaskStatus *				taskStatus,
					SCSI_Sense_Data *				senseDataBuffer  )
{
	
	IOReturn					status		= kIOReturnSuccess;
	IOByteCount					byteCount	= 0;
	AppleInquiryStruct			inquiryData	= { 0 };
	
	PRINT ( ( "MMC::Inquiry called\n" ) );
	
	check ( inquiryBuffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	inquiryData.buffer 			= ( void * ) inquiryBuffer;
	inquiryData.bufferSize		= inqBufferSize;
	inquiryData.senseDataBuffer	= ( void * ) senseDataBuffer;
	byteCount 					= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
												 	kMMCDeviceInquiry,
												 	sizeof ( inquiryData ),
													&byteCount,
													( void * ) &inquiryData,
													( void * ) taskStatus );
	
	PRINT ( ( "MMC : Inquiry status = 0x%08x\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• TestUnitReady - 	Called to issue a TEST_UNIT_READY command as defined
//						 in SPC-2.									[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::TestUnitReady ( SCSITaskStatus * 	taskStatus,
										  SCSI_Sense_Data * senseDataBuffer  )
{
	
	IOReturn		status 		= kIOReturnSuccess;
	IOByteCount		byteCount	= 0;
	
	PRINT ( ( "MMC::TestUnitReady called\n" ) );
	
	check ( taskStatus );
	check ( senseDataBuffer );
	
	byteCount = sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodScalarIStructureO (  fConnection, 	
												 kMMCDeviceTestUnitReady, 
												 1,
												 &byteCount,
												 ( int ) senseDataBuffer,
												 ( void * ) taskStatus );
	
	PRINT ( ( "MMC : TestUnitReady status = %d\n", status ) );		
	PRINT ( ( "MMC : TestUnitReady taskStatus = %d\n", *taskStatus ) );
	
	if ( *taskStatus == kSCSITaskStatus_CHECK_CONDITION )
	{
		
		PRINT ( ( "SENSE_KEY_CODE: 0x%02x, ASC: 0x%02x, ASCQ: 0x%02x\n",
			   senseDataBuffer->SENSE_KEY & kSENSE_KEY_Mask,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE_QUALIFIER ) );
		
	}
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetPerformance - 	Called to issue a GET_PERFORMANCE command as defined
//						in Mt Fuji 5.								[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::GetPerformance (
						SCSICmdField5Bit 	DATA_TYPE,
						SCSICmdField4Byte	STARTING_LBA,
						SCSICmdField2Byte	MAXIMUM_NUMBER_OF_DESCRIPTORS,
						SCSICmdField1Byte	TYPE,
						void *				buffer,
						SCSICmdField2Byte	bufferSize,
						SCSITaskStatus *	taskStatus,
						SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn					status			= kIOReturnBadArgument;
	IOByteCount					byteCount		= 0;
	AppleGetPerformanceStruct	performanceData = { 0 };
	bool						valid 			= false;
	
	PRINT ( ( "MMC::GetPerformance called\n" ) );
	
	check ( buffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	valid = IsParameterValid ( DATA_TYPE, kSCSICmdFieldMask5Bit );
	require ( valid, Error_Exit );
	
	performanceData.DATA_TYPE						= DATA_TYPE;
	performanceData.STARTING_LBA					= STARTING_LBA;
	performanceData.MAXIMUM_NUMBER_OF_DESCRIPTORS	= MAXIMUM_NUMBER_OF_DESCRIPTORS;
	performanceData.TYPE							= TYPE;
	performanceData.buffer							= buffer;
	performanceData.bufferSize						= bufferSize;
	performanceData.senseDataBuffer					= senseDataBuffer;
	
	byteCount = sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceGetPerformance, 
													sizeof ( performanceData ),
													&byteCount,
													( void * ) &performanceData,
													( void * ) taskStatus );
	
	
Error_Exit:
	
	
	PRINT ( ( "MMC : GetPerformance status = %d\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetConfiguration - 	Called to issue a GET_CONFIGURATION command as
//						 	defined in MMC-2.						[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::GetConfiguration (
						SCSICmdField1Byte	RT,
						SCSICmdField2Byte	STARTING_FEATURE_NUMBER,
						void *				buffer,
						SCSICmdField2Byte	bufferSize,
						SCSITaskStatus *	taskStatus,
						SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn						status 		= kIOReturnBadArgument;
	IOByteCount						byteCount	= 0;
	AppleGetConfigurationStruct		configData	= { 0 };
	
	PRINT ( ( "MMC::GetConfiguration called\n" ) );
	
	check ( buffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	require ( ( RT < 0x03 ), Error_Exit );
	
	configData.RT 						= RT;
	configData.STARTING_FEATURE_NUMBER	= STARTING_FEATURE_NUMBER;
	configData.buffer					= buffer;
	configData.bufferSize				= bufferSize;
	configData.senseDataBuffer			= ( void * ) senseDataBuffer;
	byteCount 							= sizeof ( SCSITaskStatus );

	status = IOConnectMethodStructureIStructureO (  fConnection, 	
												 	kMMCDeviceGetConfiguration,
												 	sizeof ( configData ),
												 	&byteCount,
												 	( void * ) &configData,
												 	( void * ) taskStatus );
	
	
Error_Exit:
	
	
	PRINT ( ( "MMC : GetConfiguration status = %d\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ModeSense10 - Called to issue a MODE_SENSE_10 command as defined in
//					SPC-2.											[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::ModeSense10 (
						SCSICmdField1Bit	LLBAA,
						SCSICmdField1Bit	DBD,
						SCSICmdField2Bit	PC,
						SCSICmdField6Bit	PAGE_CODE,
						void *				buffer,
						SCSICmdField2Byte	bufferSize,
						SCSITaskStatus * 	taskStatus,
						SCSI_Sense_Data * 	senseDataBuffer )
{
	
	IOReturn					status			= kIOReturnBadArgument;
	AppleModeSense10Struct 		modeSenseStruct = { 0 };
	IOByteCount					byteCount		= 0;
	bool						valid			= false;
	
	PRINT ( ( "MMC::ModeSense10 called\n" ) );
	
	check ( buffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	valid = ( 	IsParameterValid ( LLBAA, kSCSICmdFieldMask1Bit ) 	&&
				IsParameterValid ( DBD, kSCSICmdFieldMask1Bit ) 	&&
				IsParameterValid ( PC, kSCSICmdFieldMask2Bit )		&&
				IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ) );
	
	require ( valid, Error_Exit );
	
	modeSenseStruct.LLBAA 				= LLBAA;
	modeSenseStruct.DBD					= DBD;
	modeSenseStruct.PC					= PC;
	modeSenseStruct.PAGE_CODE			= PAGE_CODE;
	modeSenseStruct.buffer				= buffer;
	modeSenseStruct.bufferSize			= bufferSize;
	modeSenseStruct.senseDataBuffer		= senseDataBuffer;
	byteCount 							= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceModeSense10, 
													sizeof ( modeSenseStruct ),
													&byteCount,
													( void * ) &modeSenseStruct,
													( void * ) taskStatus );
	
	
Error_Exit:
	
	
	PRINT ( ( "MMC : ModeSense10 status = %d\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetWriteParametersModePage - 	Called to issue a MODE_SELECT_10 with the
//									Write Parameters Mode Page value set as
//									defined in SPC-2 and MMC-2.		[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::SetWriteParametersModePage (
						void *				buffer,
						SCSICmdField2Byte	bufferSize,
						SCSITaskStatus * 	taskStatus,
						SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn							status		= kIOReturnBadArgument;
	IOByteCount 						byteCount	= 0;
	AppleWriteParametersModePageStruct	paramsData	= { 0 };
	UInt8								pageCode	= 0;
	UInt8 *								buf			= ( UInt8 * ) buffer;
	
	PRINT ( ( "MMC::SetWriteParametersModePage called\n" ) );
	
	check ( buf );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	pageCode = ( buf[8] & 0x3F );
	require ( ( pageCode == 0x05 ), Error_Exit );
	
	paramsData.buffer 			= buffer;
	paramsData.bufferSize		= bufferSize;
	paramsData.senseDataBuffer	= ( void * ) senseDataBuffer;
	byteCount					= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceSetWriteParametersModePage, 
													sizeof ( paramsData ),
													&byteCount,
													( void * ) &paramsData,
													( void * ) taskStatus );
	
	
Error_Exit:
	
	
	PRINT ( ( "MMC : SetWriteParametersModePage status = %d\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetTrayState - Called to determine the status of the drive tray.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::GetTrayState ( UInt8 * trayState )
{
	
	IOReturn		status			= kIOReturnSuccess;
	UInt32			outParams[1]	= { 0 };
	
	PRINT ( ( "MMC::GetTrayState called\n" ) );
	
	status = IOConnectMethodScalarIScalarO ( fConnection, 					 
											 kMMCDeviceGetTrayState, 
											 0,
											 1,
											 ( int * ) outParams );
	
	require_success ( status, ErrorExit );
	
	*trayState = ( UInt8 )( ( outParams[0] != 0 ) ? kMMCDeviceTrayOpen : kMMCDeviceTrayClosed );
	
	
ErrorExit:
	

	PRINT ( ( "MMC::GetTrayState status = 0x%x\n", status) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetTrayState - Called to set the status of the drive tray.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::SetTrayState ( UInt8 trayState )
{
	
	IOReturn		status = kIOReturnBadArgument;
	UInt32			param;
	
	PRINT ( ( "MMC::SetTrayState called\n" ) );
	
	require ( ( trayState == kMMCDeviceTrayOpen ) ||
			  ( trayState == kMMCDeviceTrayClosed ),
			  Error_Exit );
	
	param = trayState;
	
	status = IOConnectMethodScalarIScalarO ( fConnection, 					 
											 kMMCDeviceSetTrayState, 
											 1,
											 0,
											 param );
	
	
Error_Exit:
	

	PRINT ( ( "MMC::SetTrayState status = 0x%x\n", status) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ReadTableOfContents - Called to issue a READ_TOC_PMA_ATIP command to the
//							drive.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::ReadTableOfContents (
						SCSICmdField1Bit 	MSF,
						SCSICmdField4Bit 	FORMAT,
						SCSICmdField1Byte	TRACK_SESSION_NUMBER,
						void *				buffer,
						SCSICmdField2Byte	bufferSize,
						SCSITaskStatus *	taskStatus,
						SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn							status 		= kIOReturnBadArgument;
	AppleReadTableOfContentsStruct 		readTOCData = { 0 };
	IOByteCount							byteCount	= 0;
	bool								valid 		= false;
	
	PRINT ( ( "MMC::ReadTableOfContents called\n" ) );
	
	valid = IsParameterValid ( MSF, kSCSICmdFieldMask1Bit ) &&
			IsParameterValid ( FORMAT, kSCSICmdFieldMask4Bit );
	
	require ( valid, Error_Exit );
	
	readTOCData.MSF 					= MSF;
	readTOCData.FORMAT					= FORMAT;
	readTOCData.TRACK_SESSION_NUMBER	= TRACK_SESSION_NUMBER;
	readTOCData.buffer					= buffer;
	readTOCData.bufferSize				= bufferSize;
	readTOCData.senseDataBuffer			= ( void * ) senseDataBuffer;
	byteCount							= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceReadTableOfContents, 
													sizeof ( readTOCData ),
													&byteCount,
													( void * ) &readTOCData,
													( void * ) taskStatus );
	
	PRINT ( ( "MMC : ReadTableOfContents status = %d\n", status ) );
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ReadDiscInformation - Called to issue a READ_DISC_INFORMATION command
//							to the drive.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::ReadDiscInformation (
						void *				buffer,
						SCSICmdField2Byte	bufferSize,
						SCSITaskStatus *	taskStatus,
						SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn					status			= kIOReturnSuccess;
	AppleReadDiscInfoStruct 	discInfoData	= { 0 };
	IOByteCount					byteCount		= 0;
	
	PRINT ( ( "MMC::ReadDiscInformation called\n" ) );
	
	check ( buffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	discInfoData.buffer				= buffer;
	discInfoData.bufferSize			= bufferSize;
	discInfoData.senseDataBuffer	= ( void * ) senseDataBuffer;
	byteCount						= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceReadDiscInformation, 
													sizeof ( discInfoData ),
													&byteCount,
													( void * ) &discInfoData,
													( void * ) taskStatus );
	
	PRINT ( ( "MMC : ReadDiscInformation status = 0x%08x\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ReadTrackInformation -	Called to issue a READ_TRACK/RZONE_INFORMATION
//								command to the drive.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::ReadTrackInformation (
				SCSICmdField2Bit	ADDRESS_NUMBER_TYPE,
				SCSICmdField4Byte	LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
				void *				buffer,
				SCSICmdField2Byte	bufferSize,
				SCSITaskStatus *	taskStatus,
				SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn					status 			= kIOReturnBadArgument;
	AppleReadTrackInfoStruct 	trackInfoData	= { 0 };
	IOByteCount					byteCount		= 0;
	bool						valid			= false;
	
	PRINT ( ( "MMC::ReadTrackInformation called\n" ) );
	
	check ( buffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	valid = IsParameterValid ( ADDRESS_NUMBER_TYPE, kSCSICmdFieldMask2Bit );
	require ( valid, Error_Exit );
	
	trackInfoData.ADDRESS_NUMBER_TYPE							= ADDRESS_NUMBER_TYPE & 0x03;
	trackInfoData.LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER	= LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER;
	trackInfoData.buffer										= buffer;
	trackInfoData.bufferSize									= bufferSize;
	trackInfoData.senseDataBuffer								= ( void * ) senseDataBuffer;
	byteCount													= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceReadTrackInformation, 
													sizeof ( trackInfoData ),
													&byteCount,
													( void * ) &trackInfoData,
													( void * ) taskStatus );
	
	
Error_Exit:
	
		
	PRINT ( ( "MMC : ReadTrackInformation status = 0x%08x\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ReadDVDStructure -	Called to issue a READ_DVD_STRUCTURE command to
//							the drive.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::ReadDVDStructure (
					SCSICmdField4Byte	ADDRESS,
					SCSICmdField1Byte	LAYER_NUMBER,
					SCSICmdField1Byte	FORMAT,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus *	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn						status			= kIOReturnSuccess;
	AppleReadDVDStructureStruct 	dvdStructData	= { 0 };
	IOByteCount						byteCount		= 0;
	
	PRINT ( ( "MMC::ReadDVDStructure called\n" ) );
	
	check ( buffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	dvdStructData.ADDRESS 				= ADDRESS;
	dvdStructData.LAYER_NUMBER			= LAYER_NUMBER;
	dvdStructData.FORMAT				= FORMAT;
	dvdStructData.buffer				= buffer;
	dvdStructData.bufferSize			= bufferSize;
	dvdStructData.AGID					= 0x00;
	dvdStructData.senseDataBuffer		= senseDataBuffer;
	byteCount 							= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceReadDVDStructure, 
													sizeof ( dvdStructData ),
													&byteCount,
													( void * ) &dvdStructData,
													( void * ) taskStatus );
	
	PRINT ( ( "MMC : ReadDVDStructure status = %d\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetCDSpeed - Called to set the new CD read/write speeds.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::SetCDSpeed ( SCSICmdField2Byte	LOGICAL_UNIT_READ_SPEED,
									   SCSICmdField2Byte	LOGICAL_UNIT_WRITE_SPEED,
									   SCSITaskStatus *		taskStatus,
									   SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn						status				= kIOReturnBadArgument;
	AppleSetCDSpeedStruct		 	setCDSpeedData		= { 0 };
	IOByteCount						byteCount			= 0;
	
	
	PRINT ( ( "MMC::SetCDSpeed called\n" ) );
	
	check ( taskStatus );
	check ( senseDataBuffer );
	
	setCDSpeedData.LOGICAL_UNIT_READ_SPEED	= LOGICAL_UNIT_READ_SPEED;
	setCDSpeedData.LOGICAL_UNIT_WRITE_SPEED	= LOGICAL_UNIT_WRITE_SPEED;
	setCDSpeedData.senseDataBuffer			= senseDataBuffer;
	byteCount 								= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceSetCDSpeed, 
													sizeof ( setCDSpeedData ),
													&byteCount,
													( void * ) &setCDSpeedData,
													( void * ) taskStatus );

	PRINT ( ( "MMC::SetCDSpeed status = 0x%x\n", status) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ReadFormatCapacities - Called to issue a READ_FORMAT_CAPACITIES command
//							 to the drive.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::ReadFormatCapacities ( void *					buffer,
												 SCSICmdField2Byte		bufferSize,
												 SCSITaskStatus *		taskStatus,
												 SCSI_Sense_Data *		senseDataBuffer )
{
	
	IOReturn							status						= kIOReturnBadArgument;
	AppleReadFormatCapacitiesStruct		readFormatCapacitiesData	= { 0 };
	IOByteCount							byteCount					= 0;
	
	PRINT ( ( "MMC::ReadFormatCapacities called\n" ) );
	
	check ( buffer );
	check ( taskStatus );
	check ( senseDataBuffer );
	
	readFormatCapacitiesData.buffer				= buffer;
	readFormatCapacitiesData.bufferSize			= bufferSize;
	readFormatCapacitiesData.senseDataBuffer	= senseDataBuffer;
	byteCount 									= sizeof ( SCSITaskStatus );
	
	status = IOConnectMethodStructureIStructureO ( 	fConnection, 	
													kMMCDeviceReadFormatCapacities, 
													sizeof ( readFormatCapacitiesData ),
													&byteCount,
													( void * ) &readFormatCapacitiesData,
													( void * ) taskStatus );

	PRINT ( ( "MMC::ReadFormatCapacities status = 0x%x\n", status) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetSCSITaskDeviceInterface -	Called to obtain a handle to the
//									SCSITaskDeviceInterface.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskDeviceInterface **
MMCDeviceUserClientClass::GetSCSITaskDeviceInterface ( void )
{
	
	PRINT ( ( "MMC::GetSCSITaskDeviceInterface called\n" ) );
	return ( SCSITaskDeviceInterface ** ) SCSITaskDeviceClass::alloc (
															fService,
															fConnection );
	
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
MMCDeviceUserClientClass::sProbe ( 	void * 			self,
									CFDictionaryRef propertyTable, 
									io_service_t	service,
									SInt32 *		order )
{
	
	check ( self );
	return getThis ( self )->Probe ( propertyTable, service, order );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sStart - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sStart ( 	void * 			self,
									CFDictionaryRef propertyTable,
									io_service_t 	service )
{
	
	check ( self );
	return getThis ( self )->Start ( propertyTable, service );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sStop - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sStop ( void * self )
{
	
	check ( self );
	return getThis ( self )->Stop ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sInquiry - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sInquiry (  
					void *							self,
					SCSICmd_INQUIRY_StandardData *	inquiryBuffer,
					UInt32							inqBufferSize,
					SCSITaskStatus *				taskStatus,
					SCSI_Sense_Data *				senseDataBuffer )
{
	
	IOReturn	status = kIOReturnBadArgument;
	
	require ( ( inqBufferSize <= 0xFF ), Error_Exit );
	
	status = getThis ( self )->Inquiry ( inquiryBuffer,
										 inqBufferSize & 0xFF,
										 taskStatus,
										 senseDataBuffer );
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sTestUnitReady - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sTestUnitReady (
					void * 				self,
					SCSITaskStatus * 	taskStatus,
					SCSI_Sense_Data * 	senseDataBuffer  )
{
	
	check ( self );
	return getThis ( self )->TestUnitReady ( taskStatus, senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetPerformance - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sGetPerformance (
					void *				self,
					SCSICmdField2Bit	TOLERANCE,
					SCSICmdField1Bit	WRITE,
					SCSICmdField2Bit	EXCEPT,
					SCSICmdField4Byte	STARTING_LBA,
					SCSICmdField2Byte	MAXIMUM_NUMBER_OF_DESCRIPTORS,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus * 	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	bool		valid 	= false;
	IOReturn	status 	= kIOReturnBadArgument;
	
	check ( self );

	valid = getThis ( self )->IsParameterValid ( TOLERANCE, kSCSICmdFieldMask2Bit ) &&
			getThis ( self )->IsParameterValid ( WRITE, kSCSICmdFieldMask1Bit ) &&
			getThis ( self )->IsParameterValid ( EXCEPT, kSCSICmdFieldMask2Bit );
	
	require ( valid, Error_Exit );
		
	status = getThis ( self )->GetPerformance ( TOLERANCE << 3 | WRITE << 2 | EXCEPT,
												STARTING_LBA,
												MAXIMUM_NUMBER_OF_DESCRIPTORS,
												0,
												buffer,
												bufferSize,
												taskStatus,
												senseDataBuffer );
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetPerformanceV2 - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sGetPerformanceV2 (
					void * 				self,
					SCSICmdField5Bit 	DATA_TYPE,
					SCSICmdField4Byte	STARTING_LBA,
					SCSICmdField2Byte	MAXIMUM_NUMBER_OF_DESCRIPTORS,
					SCSICmdField1Byte	TYPE,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus *	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->GetPerformance ( 	DATA_TYPE,
												STARTING_LBA,
												MAXIMUM_NUMBER_OF_DESCRIPTORS,
												TYPE,
												buffer,
												bufferSize,
												taskStatus,
												senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetConfiguration - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sGetConfiguration (
					void *				self,
					SCSICmdField1Byte	RT,
					SCSICmdField2Byte	STARTING_FEATURE_NUMBER,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus *	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->GetConfiguration ( RT,
												STARTING_FEATURE_NUMBER,
												buffer,
												bufferSize,
												taskStatus,
												senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sModeSense10 - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sModeSense10 ( 
					void *				self,
					SCSICmdField1Bit	LLBAA,
					SCSICmdField1Bit	DBD,
					SCSICmdField2Bit	PC,
					SCSICmdField6Bit	PAGE_CODE,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus * 	taskStatus,
					SCSI_Sense_Data * 	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->ModeSense10 ( 	LLBAA,
											DBD,
											PC,
											PAGE_CODE,
											buffer,
											bufferSize,
											taskStatus,
											senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetWriteParametersModePage - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sSetWriteParametersModePage (  
					void *				self,
					void *				buffer,
					SCSICmdField1Byte	bufferSize,
					SCSITaskStatus *	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->SetWriteParametersModePage ( 	buffer,
															bufferSize,
															taskStatus,
															senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetTrayState - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sGetTrayState ( void * self, UInt8 * trayState )
{
	
	check ( self );
	return getThis ( self )->GetTrayState ( trayState );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetTrayState - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sSetTrayState ( void * self, UInt8 trayState )
{
	
	check ( self );
	return getThis ( self )->SetTrayState ( trayState );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sReadTableOfContents - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sReadTableOfContents (  
					void *				self,
					SCSICmdField1Bit 	MSF,
					SCSICmdField4Bit 	FORMAT,
					SCSICmdField1Byte	TRACK_SESSION_NUMBER,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus *	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->ReadTableOfContents ( 	MSF,
													FORMAT,
													TRACK_SESSION_NUMBER,
													buffer,
													bufferSize,
													taskStatus,
													senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sReadDiscInformation - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sReadDiscInformation (  
					void *				self,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus *	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->ReadDiscInformation ( 	buffer,
													bufferSize,
													taskStatus,
													senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sReadTrackInformation - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sReadTrackInformation ( 
				void *				self,
				SCSICmdField2Bit	ADDRESS_NUMBER_TYPE,
				SCSICmdField4Byte	LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
				void *				buffer,
				SCSICmdField2Byte	bufferSize,
				SCSITaskStatus *	taskStatus,
				SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->ReadTrackInformation (
									ADDRESS_NUMBER_TYPE,
									LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
									buffer,
									bufferSize,
									taskStatus,
									senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sReadDVDStructure - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sReadDVDStructure (  
					void *				self,
					SCSICmdField4Byte	ADDRESS,
					SCSICmdField1Byte	LAYER_NUMBER,
					SCSICmdField1Byte	FORMAT,
					void *				buffer,
					SCSICmdField2Byte	bufferSize,
					SCSITaskStatus *	taskStatus,
					SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( self );
	return getThis ( self )->ReadDVDStructure ( ADDRESS,
												LAYER_NUMBER,
												FORMAT,
												buffer,
												bufferSize,
												taskStatus,
												senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetCDSpeed - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sSetCDSpeed ( void *				self,
										SCSICmdField2Byte	LOGICAL_UNIT_READ_SPEED,
										SCSICmdField2Byte	LOGICAL_UNIT_WRITE_SPEED,
										SCSITaskStatus *	taskStatus,
										SCSI_Sense_Data *	senseDataBuffer )
{
	
	IOReturn	status;
	
	
	check ( self );
	status = getThis ( self )->SetCDSpeed ( LOGICAL_UNIT_READ_SPEED,
											LOGICAL_UNIT_WRITE_SPEED,
											taskStatus,
											senseDataBuffer );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sReadFormatCapacities - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
MMCDeviceUserClientClass::sReadFormatCapacities ( void *					self,
												  void *					buffer,
												  SCSICmdField2Byte			bufferSize,
												  SCSITaskStatus *			taskStatus,
												  SCSI_Sense_Data *			senseDataBuffer )
{
	
	IOReturn	status;
	
	
	check ( self );
	status = getThis ( self )->ReadFormatCapacities ( buffer,
													  bufferSize,
													  taskStatus,
													  senseDataBuffer );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetSCSITaskDeviceInterface - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskDeviceInterface **
MMCDeviceUserClientClass::sGetSCSITaskDeviceInterface ( void * self )
{
	
	check ( self );
	return getThis ( self )->GetSCSITaskDeviceInterface ( );
	
}