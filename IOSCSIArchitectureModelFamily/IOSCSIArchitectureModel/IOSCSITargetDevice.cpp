/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Libkern includes
#include <libkern/OSByteOrder.h>

// Generic IOKit related headers
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>

// SCSI Architecture Model Family includes
#include "SCSITaskDefinition.h"
#include "SCSIPrimaryCommands.h"
#include "IOSCSITargetDevice.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSI Target Device"

#if DEBUG
#define SCSI_TARGET_DEVICE_DEBUGGING_LEVEL					3
#endif

#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_TARGET_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_TARGET_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_TARGET_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClassAndStructors ( IOSCSITargetDevice, IOSCSIPrimaryCommandsDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kTURMaxRetries							1
#define kMaxInquiryAttempts						2
#define kStandardInquiryDataHeaderSize			5
#define kVitalProductsInquiryDataHeaderSize		4

#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleOpen - Handles opens on the object.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


bool
IOSCSITargetDevice::handleOpen ( IOService *		client,
								 IOOptionBits		options,
								 void *				arg )
{
	
	bool	result = false;
	
	// It's an open from a multi-LUN client
	require_nonzero ( fClients, ErrorExit );
	require_nonzero ( OSDynamicCast ( IOSCSILogicalUnitNub, client ), ErrorExit );
	result = fClients->setObject ( client );
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleClose - Handles closes on the object.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::handleClose ( IOService *	client,
								  IOOptionBits	options )
{
	
	require_nonzero ( fClients, Exit );
	
	if ( fClients->containsObject ( client ) )
	{
		
		fClients->removeObject ( client );
		
		if ( ( fClients->getCount ( ) == 0 ) && isInactive ( ) )
		{
			message ( kIOMessageServiceIsRequestingClose, getProvider ( ), 0 );
		}
		
	}
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleIsOpen - Figures out if there are any opens on this object.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::handleIsOpen ( const IOService * client ) const
{
	
	bool	result = false;
		
	require_nonzero ( fClients, CallSuperClassError );
	
	// General case (is anybody open)
	if ( ( client == NULL ) && ( fClients->getCount ( ) != 0 ) )
	{
		result = true;
	}
	
	else
	{
		// specific case (is this client open)
		result = fClients->containsObject ( client );
	}
	
	return result;
	
	
CallSuperClassError:
	
	
	result = super::handleIsOpen ( client );
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods - Methods used by this class and subclasses
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InitializeDeviceSupport - Initializes device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::InitializeDeviceSupport ( void )
{
	
	bool	result = false;
	
	// Set all appropriate Registry Properties so that they are available if needed.
	RetrieveCharacteristicsFromProvider ( );
	
	// Determine the SCSI Target Device characteristics for the target
	// that is represented by this object.
	result = DetermineTargetCharacteristics ( );
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StartDeviceSupport - Starts device support.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::StartDeviceSupport ( void )
{
	
	UInt64		countLU;
	UInt64		loopLU;
	bool		supportsREPORTLUNS = false;
	//UInt8 *	reportLUNData;
	
	// Try to determine the available Logical Units by using the REPORT_LUNS
	// command.
	
	// Create the Logical Unit nodes for each valid LUN.  If the number
	// of valid LUNs could not be determine, should we create a Logical Unit
	// node for each possible LUN and have it determine if there is a valid
	// Logical Unit for its LUN?
	
	// Once the Logical Unit stuff is in, this will go away.
	// Register this object as a nub for the Logical Unit Driver.
	// registerService ( );

	// Determine the maximum number of Logical Units supported by the device
	// and protocol
	countLU = DetermineMaximumLogicalUnitNumber ( );
	
	// Allocate space for our set that will keep track of the LUNs.
	fClients = OSSet::withCapacity ( countLU + 1 );
	
#if 0
	// Check to see the specification that this device claims compliance with
	// and if it is after SPC (SCSI-3), see if it supports the REPORT_LUNS
	// command.
	if ( fTargetANSIVersion >= kINQUIRY_ANSI_VERSION_SCSI_SPC_Compliant )
	{
		
		UInt32		luCount[2];
		
		// Check 
		if ( RetrieveReportLUNsData ( 0, ( UInt8 * ) luCount, 8 ) == true )
		{
		}
		
	}
#endif
	
	if ( supportsREPORTLUNS == false )
	{
		
		for ( loopLU = 0; loopLU <= countLU; loopLU++ )
		{
			CreateLogicalUnit ( loopLU );
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SuspendDeviceSupport - Suspends device support.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 					
IOSCSITargetDevice::SuspendDeviceSupport ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResumeDeviceSupport - Resumes device support.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 					
IOSCSITargetDevice::ResumeDeviceSupport ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StopDeviceSupport - Stops device support.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::StopDeviceSupport ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TerminateDeviceSupport - Terminates device support.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 					
IOSCSITargetDevice::TerminateDeviceSupport ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetNumberOfPowerStateTransitions - Unused.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSITargetDevice::GetNumberOfPowerStateTransitions ( void )
{
	return 0;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearNotReadyStatus - Unused.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::ClearNotReadyStatus ( void )
{
	return true;
}


#if 0
#pragma mark -
#pragma mark ¥ Power Management Utility Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetInitialPowerState - Unused.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32		
IOSCSITargetDevice::GetInitialPowerState ( void )
{
	return 0;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandlePowerChange - Unused.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void		
IOSCSITargetDevice::HandlePowerChange ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleCheckPowerState - Unused.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void		
IOSCSITargetDevice::HandleCheckPowerState ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TicklePowerManager - Unused.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void		
IOSCSITargetDevice::TicklePowerManager ( void )
{
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveCharacteristicsFromProvider - Gets characteristics from the
//											provider.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::RetrieveCharacteristicsFromProvider ( void )
{
	
	OSDictionary * 	characterDict 	= NULL;
	OSObject *		obj				= NULL;
	
	fDefaultInquiryCount = 0;
	
	// Check if the personality for this device specifies a preferred protocol
	obj = GetProtocolDriver ( )->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey );
	if ( obj != NULL )
	{
		
		characterDict = OSDynamicCast ( OSDictionary, obj );
		if ( characterDict != NULL )
		{
			
			setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );
			
			// Check if the personality for this device specifies a preferred Inquiry count
			if ( characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) != NULL )
			{
				
				OSNumber *	defaultInquiry = NULL;
				
				STATUS_LOG ( ( "%s: Get Inquiry Length property.\n", getName ( ) ) );
				
				obj	= characterDict->getObject ( kIOPropertySCSIInquiryLengthKey );
				
				defaultInquiry = OSDynamicCast ( OSNumber, obj );
				if ( defaultInquiry != NULL )
				{
					
					// This device has a preferred protocol, use that.
					fDefaultInquiryCount = defaultInquiry->unsigned32BitValue ( );
					
				}
				
			}
			
		}
		
	}
	
	STATUS_LOG ( ( "%s: default inquiry count is: %d\n", getName ( ), fDefaultInquiryCount ) );
	
	obj = GetProtocolDriver ( )->getProperty ( kIOPropertyProtocolCharacteristicsKey );
	characterDict = OSDynamicCast ( OSDictionary, obj );
	if ( characterDict == NULL )
	{
		characterDict = OSDictionary::withCapacity ( 1 );
	}
	
	else
	{
		characterDict->retain ( );
	}
	
	if ( characterDict != NULL )
	{
		
		obj = GetProtocolDriver ( )->getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
		if ( obj != NULL )
		{
			characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
		}
		
		obj = GetProtocolDriver ( )->getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
		if ( obj != NULL )
		{
			characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
		}
		
		setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
		
	#if 0
		// Set default device usage based on protocol characteristics
		obj = GetProtocolDriver ( )->getProperty ( kIOPropertySCSIProtocolMultiInitKey );
		if ( obj != NULL )
		{
			
			// The protocol allows for multiple initiators in the SCSI Domain, set device
			// usage to "shared" to prevent power down a drive that appears idle to this system
			// but may be in use by others.
			
		}
	#endif
		
		characterDict->release ( );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark ¥ Target Related Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineTargetCharacteristics - Method to interrogate device.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// The DetermineTargetCharacteristics method will validate the existence of
// a SCSI Target Device at the SCSI Target Identifier that the instantiation
// of the IOSCSITargetDevice represents.  If no target is found, this routine
// will return false and the instantiation will be destroyed.
//
// If a target if found, this routine will perform the following steps:
// 1.) Determine the size of the standard INQUIRY data that the device has
// 2.) Retrieve that amount of standard INQUIRY data from the target
// 3.) Determine the available Logical Units by either the REPORT_LUNS command
// or by sending an INQUIRY to each Logical Unit to see if it is valid.
// 4.) Create a Logical Unit node for each that was determined valid.

bool
IOSCSITargetDevice::DetermineTargetCharacteristics ( void )
{
	
	UInt8		inqData[255]	= { 0 };
	UInt8		inqDataCount	= 0;
	bool		result			= false;
	
	// Verify that a target is indeed connected for this instantiation.	
	result = VerifyTargetPresence ( );
	require_string ( result, ErrorExit, "Target presence could not be verified" );
	
	// Determine the total amount of data that this target has available for
	// the INQUIRY command.
	if ( fDefaultInquiryCount != 0 )
	{
		
		// There is a default INQUIRY size for this device, just report that
		// value back.
		inqDataCount = fDefaultInquiryCount;
		
	}
	
	else
	{
		
		SCSICmd_INQUIRY_StandardDataAll *	stdData = NULL;
		
		// Since there is not a default INQUIRY size for this device, determine what 
		// the INQUIRY data size should be by sending an INQUIRY command for 6 bytes
		// (In actuality, only 5 bytes are needed, but asking for 6 alleviates the 
		// problem that some FireWire and USB to ATAPI bridges exhibit).
		result = RetrieveDefaultINQUIRYData ( 0, inqData, kStandardInquiryDataHeaderSize + 1 );	
		require_string ( result, ErrorExit, "Target INQUIRY data could not be retrieved" );
		
		stdData = ( SCSICmd_INQUIRY_StandardDataAll * ) inqData;
		inqDataCount = stdData->ADDITIONAL_LENGTH + kStandardInquiryDataHeaderSize;
		
	}
	
   	// Before we register ourself as a nub, we need to find out what 
   	// type of device we want to connect to us
	// Do an Inquiry command and parse the data to determine the peripheral
	// device type.
	result = RetrieveDefaultINQUIRYData ( 0, inqData, inqDataCount );	
	require_string ( result, ErrorExit, "Target INQUIRY data could not be retrieved" );
	
	// Check if the protocol layer driver needs the inquiry data for
	// any reason. SCSI Parallel uses this to determine Wide,
	// Sync, DT, QAS, IU, etc.
	result = IsProtocolServiceSupported ( kSCSIProtocolFeature_SubmitDefaultInquiryData, NULL );
	if ( result == true )
	{
		
		HandleProtocolServiceFeature ( kSCSIProtocolFeature_SubmitDefaultInquiryData,
									   ( void * ) inqData );
		
	}
	
	SetCharacteristicsFromINQUIRY ( ( SCSICmd_INQUIRY_StandardDataAll * ) inqData );
	
	result = RetrieveINQUIRYDataPage ( 0, inqData, 0, kVitalProductsInquiryDataHeaderSize );
	if ( result == true )
	{
		
		UInt8 	loop;
		bool 	pagefound = false;
		
		RetrieveINQUIRYDataPage ( 0, inqData, 0, inqData[3] + kVitalProductsInquiryDataHeaderSize );
		
		// If Vital Data Page zero was successfully retrieved, check to see if page 83h
		// the Device Identification page is supported and if so, retireve that data
		for ( loop = kVitalProductsInquiryDataHeaderSize; loop < ( inqData[3] + kVitalProductsInquiryDataHeaderSize ); loop++ )
		{
			
			if ( inqData[loop] == kINQUIRY_Page83_PageCode )
			{
				
				pagefound = true;
				break;
				
			}
			
		}
		
		if ( pagefound == true )
		{
			PublishDeviceIdentification ( );
		}
		
	}
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VerifyTargetPresence - Verifies target presence.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::VerifyTargetPresence ( void )
{
	
	SCSITaskIdentifier		request				= NULL;
	bool					presenceVerified 	= false;
	UInt8					TURCount			= 0;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	do
	{
		
		SCSIServiceResponse		serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		
		TEST_UNIT_READY ( request, 0x00 );
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				bool 						validSense	= false;
				SCSI_Sense_Data				senseBuffer = { 0 };
				
				validSense = GetAutoSenseData ( request, &senseBuffer, sizeof ( senseBuffer ) );
				if ( validSense == false )
				{

					IOMemoryDescriptor *		bufferDesc	= NULL;
					
					bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																sizeof ( SCSI_Sense_Data ),
																kIODirectionIn );
					
					if ( bufferDesc != NULL )
					{
						
						REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 );
						serviceResponse = SendCommand ( request, 0 );
						
						bufferDesc->release ( );
						
					}
					
				}
				
			}
			
			// The SCSI Task completed with status meaning that a target was found
			// set that the presence was verified.
			presenceVerified = true;
			
		}
		
		TURCount++;
		
	} while ( ( presenceVerified == false ) && ( TURCount < kTURMaxRetries ) );
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return presenceVerified;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCharacteristicsFromINQUIRY - 	Sets characteristics from INQUIRY
//										data.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::SetCharacteristicsFromINQUIRY (
							SCSICmd_INQUIRY_StandardDataAll * inquiryBuffer )
{
	
	OSString *		string			= NULL;
	char			tempString[17]; // Maximum + 1 for null char
	int				index			= 0;
	
	// Set target characteristics
	// Save the target's Peripheral Device Type
	fTargetPeripheralDeviceType = ( inquiryBuffer->PERIPHERAL_DEVICE_TYPE & kINQUIRY_PERIPHERAL_TYPE_Mask );
	
	// Save the SCSI ANSI version that the device to which the device claims compliance.
	fTargetANSIVersion = ( inquiryBuffer->VERSION & kINQUIRY_ANSI_VERSION_Mask );
	
	// Set the other supported features
#if 0
	fTargetHasHiSup = ( inquiryBuffer->RESPONSE_DATA_FORMAT & ;
	fTargetHasSCCS;
	fTargetHasEncServs;
	fTargetHasMultiPorts;
	fTargetHasMChanger;
#endif
	
   	// Set the Peripheral Device Type property for the device.
   	setProperty ( kIOPropertySCSIPeripheralDeviceType,
   				( UInt64 ) fTargetPeripheralDeviceType,
   				kIOPropertySCSIPeripheralDeviceTypeSize );
	
   	// Set the Vendor Identification property for the device.
   	for ( index = 0; index < kINQUIRY_VENDOR_IDENTIFICATION_Length; index++ )
   	{
   		tempString[index] = inquiryBuffer->VENDOR_IDENTIFICATION[index];
   	}
	tempString[index] = 0;
	
   	for ( index = kINQUIRY_VENDOR_IDENTIFICATION_Length - 1; index != 0; index-- )
   	{
   		
   		if ( tempString[index] != ' ' )
   		{
   			
   			// Found a real character
   			tempString[index + 1] = '\0';
   			break;
   			
   		}
   		
   	}
   	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIVendorIdentification, string );
		string->release ( );
		
	}
	
   	// Set the Product Indentification property for the device.
   	for ( index = 0; index < kINQUIRY_PRODUCT_IDENTIFICATION_Length; index++ )
   	{
   		tempString[index] = inquiryBuffer->PRODUCT_INDENTIFICATION[index];
   	}
   	tempString[index] = 0;
	
   	for ( index = kINQUIRY_PRODUCT_IDENTIFICATION_Length - 1; index != 0; index-- )
   	{
   		
   		if ( tempString[index] != ' ' )
   		{
   			
   			// Found a real character
   			tempString[index+1] = '\0';
   			break;
   			
   		}
   		
   	}
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIProductIdentification, string );
		string->release ( );
		
	}
	
   	// Set the Product Revision Level property for the device.
   	for ( index = 0; index < kINQUIRY_PRODUCT_REVISION_LEVEL_Length; index++ )
   	{
   		tempString[index] = inquiryBuffer->PRODUCT_REVISION_LEVEL[index];
   	}
   	tempString[index] = 0;
	
   	for ( index = kINQUIRY_PRODUCT_REVISION_LEVEL_Length - 1; index != 0; index-- )
   	{
		
		if ( tempString[index] != ' ' )
		{
			
			// Found a real character
			tempString[index+1] = '\0';
			break;
			
		}
		
	}
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIProductRevisionLevel, string );
		string->release ( );
		
	}
	
	return true;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Logical Unit Related Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveReportLUNsData - 	Gets REPORT_LUNS data from device.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::RetrieveReportLUNsData (
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								dataBuffer,  
						UInt8									dataSize )
{
	
	SCSIServiceResponse 	serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *	bufferDesc 		= NULL;
	SCSITaskIdentifier		request 		= NULL;
	bool					result			= false; 
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) dataBuffer,
												   dataSize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
 	request = GetSCSITask ( );
 	require_nonzero ( request, ReleaseBuffer );
	
	if ( REPORT_LUNS ( request, bufferDesc, dataSize, 0 ) == true )
	{
		
		serviceResponse = SendCommand ( request, 0 );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			result = true;	
		}
		
	}
	
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMaximumLogicalUnitNumber - Determins Max LUN supported.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64 
IOSCSITargetDevice::DetermineMaximumLogicalUnitNumber ( void )
{
	
	UInt32		protocolLUNCount 	= 0;
	bool		supported			= false;
	
	// If this is a SCSI-3 device or later, issue the REPORT_LUNS command
	// to determine the possible Logical Units supported by the device.
	// else get the maxmium supported value from the Protocol Services
	// driver and scan to that limit.
	supported = IsProtocolServiceSupported (
							kSCSIProtocolFeature_GetMaximumLogicalUnitNumber,
							( void * ) &protocolLUNCount );
	
	if ( supported == false )
	{
		
		// Since the protocol does not report that it supports a maximum
		// Logical Unit Number, it does not support Logical Units at all
		// meaning that only LUN 0 is valid.
		protocolLUNCount = 0;
		
	}
	
	return protocolLUNCount;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VerifyLogicalUnitPresence - Verifies the presence of a logical unit.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::VerifyLogicalUnitPresence (
								SCSILogicalUnitNumber		theLogicalUnit )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateLogicalUnit - Creates a logical unit.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::CreateLogicalUnit ( SCSILogicalUnitNumber theLogicalUnit )
{
	
	bool						result	= false;
	IOSCSILogicalUnitNub * 		nub		= NULL;
	
	nub = OSTypeAlloc ( IOSCSILogicalUnitNub );
	require_nonzero ( nub, ErrorExit );
	
	result = nub->init ( 0 );
	require ( result, ReleaseNub );
	
	nub->SetLogicalUnitNumber ( ( UInt8 ) theLogicalUnit );
	
	result = nub->attach ( this );
	require ( result, ReleaseNub );
	
	result = nub->start ( this );
	require_action ( result, ReleaseNub, nub->detach ( this ) );
	
	nub->registerService ( );
	
	
ReleaseNub:
	
	
	require_nonzero_quiet ( nub, ErrorExit );
	nub->release ( );
	nub = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Utility Member Routines
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveDefaultINQUIRYData - 	Retrieves as much INQUIRY data as
//									requested.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOSCSITargetDevice::RetrieveDefaultINQUIRYData ( 
							SCSILogicalUnitNumber	logicalUnit,
							UInt8 * 				inquiryBuffer,  
							UInt8					inquirySize )
{
	
	SCSIServiceResponse 		serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *		bufferDesc 		= NULL;
	SCSITaskIdentifier			request 		= NULL;
 	int 						index			= 0;
	bool						result			= false;
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inquiryBuffer,
												   inquirySize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
 	request = GetSCSITask ( );
 	require_nonzero ( request, ReleaseBuffer );
	
	for ( index = 0; ( index < kMaxInquiryAttempts ); index++ )
	{
		
		result = INQUIRY ( request, bufferDesc, 0, 0, 0, inquirySize, 0 );
		require ( result, ReleaseTask );
		
		serviceResponse = SendCommand ( request, 0 );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{	
			
			result = true;
			break;	
			
		}
		
		else
		{
			
			result = false;
			IOSleep ( 1000 );
			
		}
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseBuffer );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ReleaseBuffer );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RetrieveINQUIRYDataPage - Retrieves as much INQUIRY data for a given
//								page as requested.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDevice::RetrieveINQUIRYDataPage (
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								inquiryBuffer,  
						UInt8									inquiryPage,
						UInt8									inquirySize )
{
	
	SCSIServiceResponse 	serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request 		= NULL;
	IOMemoryDescriptor *	bufferDesc 		= NULL;
	bool					result			= false; 
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inquiryBuffer,
												   inquirySize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ErrorExit );
	
 	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseBuffer );
	
	if ( INQUIRY (
			request,
			bufferDesc,
			0,
		  	1,
			inquiryPage,
			inquirySize,
			0 ) == true )
	{
		
		serviceResponse = SendCommand ( request, 0 );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			result = true;	
		}
		
	}
	
	ReleaseSCSITask ( request );
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ErrorExit );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PublishDeviceIdentification - Publishes device ID page (page 0x83) data.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDevice::PublishDeviceIdentification ( void )
{
	
	OSArray *		deviceIDs 		= NULL;
	UInt8			inqData[255]	= { 0 };
	UInt8			inqDataCount	= 0;
	UInt8			deviceIDCount	= 0;
	bool			result			= false;
	
	// This device reports that it supports Inquiry Page 83
	// determine the length of all descriptors
	result = RetrieveINQUIRYDataPage ( 0, inqData, kINQUIRY_Page83_PageCode, kVitalProductsInquiryDataHeaderSize );
	require ( result, ErrorExit );
	
	// Verify that the device does indeed have Device Identification information, by checking
	// that the additional length field is not zero.
	require_nonzero ( inqData[3], ErrorExit );
	
	result = RetrieveINQUIRYDataPage ( 0, inqData, kINQUIRY_Page83_PageCode, inqData[3] + kVitalProductsInquiryDataHeaderSize );
	require ( result, ErrorExit );
	
	// Create the array to hold the ID dictionaries
	deviceIDs = OSArray::withCapacity ( 1 );
	require_nonzero ( deviceIDs, ErrorExit );
	
	// Set the index to the first ID byte
	inqDataCount = kVitalProductsInquiryDataHeaderSize;
	
	while ( inqDataCount < ( inqData[3] + kVitalProductsInquiryDataHeaderSize ) )
	{
		
		UInt8				idSize;
		OSDictionary * 		idDictionary;
		OSNumber *			numString = NULL;
		UInt8				codeSet = 0;		
		
		deviceIDCount++;
		
		// Create the dictionary for the current device ID
		idDictionary = OSDictionary::withCapacity ( 1 );
		require_nonzero ( idDictionary, ReleaseDeviceIDs );
		
		// Process identification header
		codeSet = inqData[inqDataCount] & 0x0F;
		numString = OSNumber::withNumber ( codeSet, 8 );
		if ( numString != NULL )
		{
			
			idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdCodeSet, numString );
			numString->release ( );
			numString = NULL;
			
		}
		
		numString = OSNumber::withNumber ( inqData[inqDataCount + 1] & kINQUIRY_Page83_CodeSetMask, 8 );
		if ( numString != NULL )
		{
			
			idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdType, numString );
			numString->release ( );
			numString = NULL;
			
		}
		
		numString = OSNumber::withNumber ( (inqData[inqDataCount + 1] >> 4 ) & 0x03, 8 );
		if ( numString != NULL )
		{
			
			idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdAssociation, numString );
			numString->release ( );
			numString = NULL;
			
		}
		
		// Increment length for the header info
		inqDataCount += 3;
		
		// Get the size of the ID
		idSize = inqData[inqDataCount];
		
		// Increment length for the length byte and so
		// that the index is now at the IDENTIFIER field
		inqDataCount++;
		
		if ( codeSet == kINQUIRY_Page83_CodeSetASCIIData )
		{
			
			OSString *		charString		= NULL;
			char			idString[255]	= { 0 };
			
			// Add the ASCII bytes to the C string
			for ( UInt8 i = 0; i< idSize; i++ )
			{
				idString[i] = inqData[inqDataCount + i];
			}
			
			charString = OSString::withCString ( idString );
			if ( charString != NULL )
			{
				
				idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdentifier, charString );
				charString->release ( );
				charString = NULL;
				
			}
			
		}
		
		else
		{
			
			OSData *	idData = NULL;
			
			idData = OSData::withBytes ( &inqData[inqDataCount], idSize );
			if ( idData != NULL )
			{
				
				idDictionary->setObject ( kIOPropertySCSIINQUIRYDeviceIdentifier, idData );
				idData->release ( );
				idData = NULL;
				
			}
			
		}
		
		// Increment length for the ID length byte
		inqDataCount +=idSize;
		
		if ( deviceIDs->ensureCapacity ( deviceIDCount ) == deviceIDCount )
		{
			deviceIDs->setObject ( idDictionary );
		}
		
		idDictionary->release ( );
		idDictionary = NULL;
		
	}
	
	setProperty ( kIOPropertySCSIINQUIRYDeviceIdentification, deviceIDs );
	
	
ReleaseDeviceIDs:
	
	
	require_nonzero_quiet ( deviceIDs, ErrorExit );
	deviceIDs->release ( );
	deviceIDs = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  1 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  2 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  3 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  4 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  5 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  6 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  7 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  8 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice,  9 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 10 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 11 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 12 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 13 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 14 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 15 );
OSMetaClassDefineReservedUnused ( IOSCSITargetDevice, 16 );
