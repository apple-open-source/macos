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

#include <IOKit/scsi-commands/IOSCSITargetDevice.h>

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSI Target Device"

#if DEBUG
#define SCSI_TARGET_DEVICE_DEBUGGING_LEVEL			3
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

#define kTURMaxRetries				1
#define kMaxInquiryAttempts			2

#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


bool
IOSCSITargetDevice::handleOpen( IOService *		client,
								   IOOptionBits		options,
								   void *			arg )
{
	bool	result = false;
	
	// It's an open from a multi-LUN client
	require_nonzero ( fClients, ErrorExit );
	require_nonzero ( OSDynamicCast ( IOSCSILogicalUnitNub, client ), ErrorExit );
	result = fClients->setObject ( client );
	
ErrorExit:
	return result;
}


void
IOSCSITargetDevice::handleClose( IOService *		client,
									IOOptionBits	options )
{
	require_nonzero ( fClients, Exit );
	
	if ( fClients->containsObject( client ) )
	{
		fClients->removeObject( client );
		
		if ( ( fClients->getCount( ) == 0 ) && isInactive( ) )
		{
			message( kIOMessageServiceIsRequestingClose, getProvider( ), 0 );
		}
	}
	
Exit:
	return;
}

	
bool
IOSCSITargetDevice::handleIsOpen( const IOService * client ) const
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

bool
IOSCSITargetDevice::InitializeDeviceSupport ( void )
{
	bool			result			= false;

	STATUS_LOG ( ( "%s: InitializeDeviceSupport started\n", getName ( ) ) );

	// Set all appropriate Registry Properties so that they are available if needed.
	STATUS_LOG ( ( "%s: RetrieveCharacteristicsFromProvider\n", getName ( ) ) );
	RetrieveCharacteristicsFromProvider();
		
	// Determine the SCSI Target Device characteristics for the target
	// that is represented by this object.
	STATUS_LOG ( ( "%s: DetermineTargetCharacteristics\n", getName ( ) ) );
	result = DetermineTargetCharacteristics();

	STATUS_LOG ( ( "%s: InitializeDeviceSupport completed\n", getName ( ) ) );
	return result;
}

void
IOSCSITargetDevice::StartDeviceSupport ( void )
{
	UInt64	countLU, loopLU;
	bool	supportsREPORTLUNS = false;
	//UInt8 *	reportLUNData;
		
	STATUS_LOG ( ( "%s: StartDeviceSupport started\n", getName ( ) ) );
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
	countLU = DetermineMaximumLogicalUnitNumber();

	// Allocate space for our set that will keep track of the LUNs.
	fClients = OSSet::withCapacity( countLU + 1 );
		
#if 0
	// Check to see the specification that this device claims compliance with
	// and if it is after SPC (SCSI-3), see if it supports the REPORT_LUNS
	// command.
	if( fTargetANSIVersion >= kINQUIRY_ANSI_VERSION_SCSI_SPC_Compliant )
	{
		UInt32		luCount[2];
		
		// Check 
		if( RetrieveReportLUNsData( 0, (UInt8 *) luCount, 8 ) == true )
		{
		}
	}		
#endif

	if( supportsREPORTLUNS == false )
	{
		for( loopLU = 0; loopLU <= countLU; loopLU++)
		{
			CreateLogicalUnit( loopLU );
		}
	}
	
	STATUS_LOG ( ( "%s: StartDeviceSupport is complete\n", getName ( ) ) );
}

void 					
IOSCSITargetDevice::SuspendDeviceSupport ( void )
{
}

void 					
IOSCSITargetDevice::ResumeDeviceSupport ( void )
{
}

void
IOSCSITargetDevice::StopDeviceSupport ( void )
{
}

void 					
IOSCSITargetDevice::TerminateDeviceSupport ( void )
{
}

UInt32
IOSCSITargetDevice::GetNumberOfPowerStateTransitions ( void )
{
	return 0;
}

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

UInt32		
IOSCSITargetDevice::GetInitialPowerState ( void )
{
	return 0;
}

void		
IOSCSITargetDevice::HandlePowerChange ( void )
{
}

void		
IOSCSITargetDevice::HandleCheckPowerState ( void )
{
}

void		
IOSCSITargetDevice::TicklePowerManager ( void )
{
}

void
IOSCSITargetDevice::RetrieveCharacteristicsFromProvider( void )
{
	OSDictionary * 	characterDict 	= NULL;
	OSObject *		obj				= NULL;

	STATUS_LOG ( ( "%s: Check for the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
	// Check if the personality for this device specifies a preferred protocol
	if ( ( GetProtocolDriver()->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) == NULL )
	{
		
		STATUS_LOG ( ( "%s: No SCSI Device Characteristics property, set defaults.\n", getName ( ) ) );
		fDefaultInquiryCount = 0;
		
	}
	else
	{
		
		STATUS_LOG ( ( "%s: Get the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
		characterDict = OSDynamicCast ( OSDictionary, ( GetProtocolDriver()->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
		
		STATUS_LOG ( ( "%s: set this SCSI Device Characteristics property.\n", getName ( ) ) );
		setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );
		
		// Check if the personality for this device specifies a preferred Inquiry count
		STATUS_LOG ( ( "%s: check for the Inquiry Length property.\n", getName ( ) ) );
		if ( characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) == NULL )
		{
			
			STATUS_LOG ( ( "%s: No Inquiry Length property, use default.\n", getName ( ) ) );
			fDefaultInquiryCount = 0;
			
		}
		else
		{
			
			OSNumber *	defaultInquiry;
			
			STATUS_LOG ( ( "%s: Get Inquiry Length property.\n", getName ( ) ) );
			defaultInquiry = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) );
			
			// This device has a preferred protocol, use that.
			fDefaultInquiryCount = defaultInquiry->unsigned32BitValue ( );
		
		}
	
	}
		
	STATUS_LOG ( ( "%s: default inquiry count is: %d\n", getName ( ), fDefaultInquiryCount ) );

	characterDict = OSDynamicCast ( OSDictionary, GetProtocolDriver()->getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( characterDict == NULL )
	{
		
		characterDict = OSDictionary::withCapacity ( 1 );
		
	}
	else
	{
		characterDict->retain ( );
	}
	
	obj = GetProtocolDriver()->getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
	}
	
	obj = GetProtocolDriver()->getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
	
#if 0
	// Set default device usage based on protocol characteristics
	obj = GetProtocolDriver()->getProperty ( kIOPropertySCSIProtocolMultiInitKey );
	if ( obj != NULL )
	{
		// The protocol allows for multiple initiators in the SCSI Domain, set device
		// usage to "shared" to prevent power down a drive that appears idle to this system
		// but may be in use by others.
		
	}
#endif
	
	characterDict->release ( );
}

#if 0
#pragma mark -
#pragma mark ¥ Target Related Member Routines
#pragma mark -
#endif

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineTargetCharacteristics - Method to interrogate device.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// The InterrogateTarget member routine will validate the existance of a SCSI Target
// Device at the SCSI Target Identifier that the instantiation of the IOSCSITargetDevice
// represents.  If no target is found, this routine will return false and the instantiation
// will be destroed.
// If a target if found, this routine will perform the following steps:
// 1.) Determine the size of the standard INQUIRY data that the device has
// 2.) Retrieve that amount of standard INQUIRY data from the target
// 3.) Determine the available Logical Units by either the REPORT_LUNS command
// or by sending an INQUIRY to each Logical Unit to see if it is valid.
// 4.) Create a Logical Unit node for each that was determined valid.
bool
IOSCSITargetDevice::DetermineTargetCharacteristics( void )
{
	UInt8			inqData[255]	= {0};
	UInt8			inqDataCount	= 0;
	bool			result			= false;

	STATUS_LOG ( ( "%s: DetermineTargetCharacteristics started \n", getName ( ) ) );

	// Verify that a target is indeed connected for this instantiation.
	if( VerifyTargetPresence() == false )
	{
		STATUS_LOG ( ( "%s: Target presence could not be verified \n", getName ( ) ) );
		// Appearently there is no target connected and no need to try to 
		// retrieve INQUIRY data.
		return false;
	}	

	// Determine the total amount of data that this target has available for
	// the INQUIRY command.
	if( fDefaultInquiryCount != 0 )
	{
		// There is a default INQUIRY size for this device, just report that
		// value back.
		inqDataCount = fDefaultInquiryCount;
	}
	else
	{
		// Since there is not a default INQUIRY size for this device, determine what 
		// the INQUIRY data size should be by sending an INQUIRY command for 6 bytes
		// (In actuality, only 5 bytes are needed, but asking for 6 alleviates the 
		// problem that some FireWire and USB to ATAPI bridges exhibit).
		result = RetrieveDefaultINQUIRYData( 0, inqData, 6 );	
		if ( result == false )
		{
			STATUS_LOG ( ( "%s: Target INQUIRY data could not be retrieved \n", getName ( ) ) );
			return false;
		}
		
		inqDataCount = ((SCSICmd_INQUIRY_StandardDataAll *) inqData)->ADDITIONAL_LENGTH + 5;
	}

   	// Before we register ourself as a nub, we need to find out what 
   	// type of device we want to connect to us
	// Do an Inquiry command and parse the data to determine the peripheral
	// device type.

	result = RetrieveDefaultINQUIRYData( 0, inqData, inqDataCount );	
	if ( result == false )
	{
		STATUS_LOG ( ( "%s: Target INQUIRY data could not be retrieved \n", getName ( ) ) );
		return false;
	}
	 	
	// Check if the protocol layer driver needs the inquiry data for
	// any reason. SCSI Parallel uses this to determine Wide,
	// Sync, DT, QAS, IU, etc.
	if ( IsProtocolServiceSupported ( kSCSIProtocolFeature_SubmitDefaultInquiryData, NULL ) == true )
	{
		HandleProtocolServiceFeature ( kSCSIProtocolFeature_SubmitDefaultInquiryData, (void *) inqData );
	}
	
	SetCharacteristicsFromINQUIRY( (SCSICmd_INQUIRY_StandardDataAll *) inqData );
		
	if( RetrieveINQUIRYDataPage( 0, inqData, 0, 4 ) == true )
	{
		UInt8 	loop;
		bool 	pagefound = false;
		
		RetrieveINQUIRYDataPage( 0, inqData, 0, inqData[3] + 4 );

		// If Vital Data Page zero was successfully retrieved, check to see if page 83h
		// the Device Identification page is supported and if so, retireve that data
		
		for( loop= 4; loop < (inqData[3] + 4); loop++)
		{
			if( inqData[loop] == 0x83 )
			{
				pagefound = true;
			}
		}
		
		if( pagefound == true )
		{
			PublishDeviceIdentification();
		}
	}
		
	return true;
}

bool
IOSCSITargetDevice::VerifyTargetPresence( void )
{
	SCSITaskIdentifier		request;
	bool					presenceVerified = false;
	UInt8					TURCount = 0;
		
	request = GetSCSITask();	
	if( request == NULL )
	{
		// No SCSI Task object could be allocated and therefore the presence
		// of a target could not be verified.
		return false;
	}
	
	do
	{
		SCSIServiceResponse		serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		
		ResetForNewTask( request );
		
		TEST_UNIT_READY( request, 0x00 );
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand( request, 0 );
		if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			if( GetTaskStatus( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				bool 						validSense = false;
				SCSI_Sense_Data				senseBuffer = { 0 };
				IOMemoryDescriptor *		bufferDesc 		= NULL;
				
				validSense = GetAutoSenseData( request, &senseBuffer, sizeof( senseBuffer ) );
				if( validSense == false )
				{
					ResetForNewTask(request );
					REQUEST_SENSE( request,
								   bufferDesc,
								   kSenseDefaultSize,
								   0 );
					serviceResponse = SendCommand( request, 0 );
				}
			}
			
			// The SCSI Task completed with status meaning that a target was found
			// set that the presence was verified.
			presenceVerified = true;
		}
		
		TURCount++;
	} while(( presenceVerified == false ) && ( TURCount < kTURMaxRetries ));
	
	ReleaseSCSITask( request );
	
	return presenceVerified;
}


bool 
IOSCSITargetDevice::SetCharacteristicsFromINQUIRY( SCSICmd_INQUIRY_StandardDataAll * inquiryBuffer )
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

bool
IOSCSITargetDevice::RetrieveReportLUNsData(
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								dataBuffer,  
						UInt8									dataSize )
{
	SCSIServiceResponse 			serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *			bufferDesc 		= NULL;
	SCSITaskIdentifier				request 		= NULL;
	bool							result = false; 

 	request = GetSCSITask();
 	if( request == NULL )
 	{
 		return false;
 	}
 		
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) dataBuffer,
												   dataSize,
												   kIODirectionIn );
		
	if( REPORT_LUNS ( 
			request,
			bufferDesc,
			dataSize,
			0 ) == true )
	{
		serviceResponse = SendCommand ( request, 0 );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			result = true;	
		}
	}
	else
	{
		result = false;
	}

	bufferDesc->release();
	
	ReleaseSCSITask( request );
	
	return result;
}

UInt64 
IOSCSITargetDevice::DetermineMaximumLogicalUnitNumber( void )
{
	UInt32		protocolLUNCount;
	
	// If this is a SCSI-3 device or later, issue the REPORT_LUNS command
	// to determine the possible Logical Units supported by the device.
	// else get the maxmium supported value from the Protocol Services driver
	// and scan to that limit.
	if( IsProtocolServiceSupported( kSCSIProtocolFeature_GetMaximumLogicalUnitNumber, (void *) &protocolLUNCount ) == false )
	{
		// Since the protocol does not report that it supports a maximum Logical Unit Number,
		// it does not support Logical Units at all meaning that only LUN 0 is valid.
		return 0;
	}


	return protocolLUNCount;
}

bool
IOSCSITargetDevice::VerifyLogicalUnitPresence( SCSILogicalUnitNumber theLogicalUnit )
{
	return false;
}

bool
IOSCSITargetDevice::CreateLogicalUnit( SCSILogicalUnitNumber theLogicalUnit )
{
    STATUS_LOG ( ( "IOSCSITargetDevice::CreatePeripheralDeviceNubForLUN entering.\n" ) );

	IOSCSILogicalUnitNub * 	nub = new IOSCSILogicalUnitNub;
	
	if ( nub == NULL )
	{
		PANIC_NOW(( "IOSCSITargetDevice::CreatePeripheralDeviceNubForLUN failed\n" ));
		return false;
	}
	
	nub->init( 0 );
	
	nub->SetLogicalUnitNumber( (UInt8) theLogicalUnit );
	if ( nub->attach( this ) == false )
	{
		// panic since the nub can't attach
		PANIC_NOW(( "IOSCSITargetDevice::CreatePeripheralDeviceNubForLUN unable to attach nub" ));
		return false;
	}
				
	if( nub->start( this ) == false )
	{
		nub->detach( this );
	}
	else
	{
		nub->registerService( kIOServiceSynchronous );
	}

	nub->release();
			
	return false;
}

#if 0
#pragma mark -
#pragma mark ¥ INQUIRY Utility Member Routines
#pragma mark -
#endif

// This member routine will return the requested in INQUIRY data in the provided
// buffer which will at a minimum be of inquirySize.
bool 
IOSCSITargetDevice::RetrieveDefaultINQUIRYData( 
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								inquiryBuffer,  
						UInt8									inquirySize )
{
	SCSIServiceResponse 			serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *			bufferDesc 		= NULL;
	SCSITaskIdentifier				request 		= NULL;
 	int 							index;
	bool							result = false;
	 
 	request = GetSCSITask();
 	if( request == NULL )
 	{
 		return false;
 	}
 		
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inquiryBuffer,
												   inquirySize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, RELEASE_TASK );
		
 	IOSleep ( 100 );
 	
   	for ( index = 0; ( index < kMaxInquiryAttempts ); index++ )
	{
		
		if (INQUIRY (
				request,
				bufferDesc,
				0,
			  	0,
				0,
				inquirySize,
				0 ) == false )
		{
			result = false;
			break;
		}
			
		serviceResponse = SendCommand ( request, 0 );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{	
			result = true;
			break;	
		}
		else
		{
			IOSleep ( 1000 );
		}
		
	}
	
	bufferDesc->release();
	
RELEASE_TASK:
	ReleaseSCSITask( request );
	
	return true;
}

bool 
IOSCSITargetDevice::RetrieveINQUIRYDataPage( 
						SCSILogicalUnitNumber					logicalUnit,
						UInt8 * 								inquiryBuffer,  
						UInt8									inquiryPage,
						UInt8									inquirySize )
{
	SCSIServiceResponse 			serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *			bufferDesc 		= NULL;
	SCSITaskIdentifier				request 		= NULL;
	bool							result = false; 

 	request = GetSCSITask();
 	if( request == NULL )
 	{
 		return false;
 	}
 		
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inquiryBuffer,
												   inquirySize,
												   kIODirectionIn );
	//require_nonzero ( bufferDesc, ReleaseInquiryBuffer );
		
	if( INQUIRY (
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
	else
	{
		result = false;
	}

	bufferDesc->release();
	
	ReleaseSCSITask( request );
	
	return result;
}

void
IOSCSITargetDevice::PublishDeviceIdentification( void )
{
	UInt8			inqData[255]	= {0};
	UInt8			inqDataCount	= 0;
	OSArray *		deviceIDs 		= NULL;
	UInt8			deviceIDCount	= 0;

	// This device reports that it supports Inquiry Page 83
	// determine the length of all descriptors
	if( RetrieveINQUIRYDataPage( 0, inqData, kINQUIRY_Page83_PageCode, 4 ) == false )
	{
		return;
	}

	// Verify that the device does indeed have Device Identification information, by checking
	// that the additional length field is not zero.
	if( inqData[3] == 0 )
	{
		return;
	}
	
	if( RetrieveINQUIRYDataPage( 0, inqData, kINQUIRY_Page83_PageCode, inqData[3] + 4 ) == false )
	{
		return;
	}
	
	// Create the array to hold the ID dictionaries
	deviceIDs = OSArray::withCapacity ( 1 );
	if( deviceIDs == NULL )
	{
		return;
	}

	// Set the index to the first ID byte
	inqDataCount = 4;
	
	while ( inqDataCount < (inqData[3] + 4))
	{
		UInt8				idSize;
		OSDictionary * 		idDictionary;
		OSNumber *			numString = NULL;
		UInt8				codeSet = 0;		

		deviceIDCount++;
				
		// Create the dictionary for the current device ID
		idDictionary = OSDictionary::withCapacity ( 1 );
		
		// Process identification header
		codeSet = inqData[inqDataCount] & 0x0F;
		numString = OSNumber::withNumber( codeSet , 8 );
		if( numString != NULL )
		{
			idDictionary->setObject( kIOPropertySCSIINQUIRYDeviceIdCodeSet, numString);
			numString->release();
		}
		
		numString = OSNumber::withNumber( inqData[inqDataCount + 1] & kINQUIRY_Page83_CodeSetMask, 8 );
		if( numString != NULL )
		{
			idDictionary->setObject( kIOPropertySCSIINQUIRYDeviceIdType, numString);
			numString->release();
		}
		
		numString = OSNumber::withNumber( (inqData[inqDataCount + 1] >> 4) & 0x03, 8 );
		if( numString != NULL )
		{
			idDictionary->setObject( kIOPropertySCSIINQUIRYDeviceIdAssociation, numString);
			numString->release();
		}
				
		// Increment length for the header info
		inqDataCount +=3;
		
		// Get the size of the ID
		idSize = inqData[inqDataCount];
		
		// Increment length for the length byte and so
		// that the index is now at the IDENTIFIER field
		inqDataCount++;
		
		if( codeSet == kINQUIRY_Page83_CodeSetASCIIData )
		{
			OSString *			charString = NULL;
			char				idString[255] = {0};

			// Add the ASCII bytes to the C string
			for( UInt8 i = 0; i< idSize; i++ )
			{
	    		idString[i] = inqData[inqDataCount+i];
			}

			charString = OSString::withCString( idString );
			if( charString != NULL )
			{
				idDictionary->setObject( kIOPropertySCSIINQUIRYDeviceIdentifier, charString);
				charString->release();
			}
					
		}
		else
		{
			OSData *	idData = NULL;
			
			idData = OSData::withBytes( &inqData[inqDataCount], idSize );
			if( idData != NULL )
			{
				idDictionary->setObject( kIOPropertySCSIINQUIRYDeviceIdentifier, idData);
				idData->release();
			}
		}
		
		// Increment length for the ID length byte
		inqDataCount +=idSize;
		
		if( deviceIDs->ensureCapacity( deviceIDCount ) == deviceIDCount )
		{
			deviceIDs->setObject( idDictionary );
		}
		
		idDictionary->release();
	}
		
	setProperty ( kIOPropertySCSIINQUIRYDeviceIdentification, deviceIDs );
	deviceIDs->release();
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

