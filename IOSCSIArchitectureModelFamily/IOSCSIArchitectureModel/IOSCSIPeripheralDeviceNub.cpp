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
#include <IOKit/scsi-commands/SCSITask.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi-commands/IOSCSIPeripheralDeviceNub.h>
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "SCSIPrimaryCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSI Peripheral Device Nub"

#if DEBUG
#define SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors ( IOSCSIPeripheralDeviceNub, IOSCSIProtocolServices );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kMaxInquiryAttempts 					8


#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ init - Called by IOKit to initialize us.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::init ( OSDictionary * propTable )
{
	
	bool	result = false;
	
	require ( super::init ( propTable ), ErrorExit );
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::start ( IOService * provider )
{
	
	OSDictionary * 	characterDict 	= NULL;
	OSObject *		obj				= NULL;
	bool			result			= false;
	
	// Call our super class' start routine so that all inherited
	// behavior is initialized.	
	require ( super::start ( provider ), ErrorExit );
	
	// Check if our provider is one of us. If it is, we don't want to
	// load again.
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceNub, provider );
	require_quiet ( ( fProvider == NULL ), ErrorExit );
	
	// Make sure our provider is a protocol services driver.
	fProvider = OSDynamicCast ( IOSCSIProtocolInterface, provider );
	require_nonzero_quiet ( fProvider, ErrorExit );
	
	fSCSIPrimaryCommandObject = new SCSIPrimaryCommands;
	require_nonzero ( fSCSIPrimaryCommandObject, ErrorExit );
	
	require ( fProvider->open ( this ), ReleaseCommandObject );
	
	STATUS_LOG ( ( "%s: Check for the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
	// Check if the personality for this device specifies a preferred protocol
	if ( ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) == NULL )
	{
		
		STATUS_LOG ( ( "%s: No SCSI Device Characteristics property, set defaults.\n", getName ( ) ) );
		fDefaultInquiryCount = 0;
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "%s: Get the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
		characterDict = OSDynamicCast ( OSDictionary, ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
		
		STATUS_LOG ( ( "%s: set this SCSI Device Characteristics property.\n", getName ( ) ) );
		setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );
		
		// Check if the personality for this device specifies a preferred protocol
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
	
	require ( InterrogateDevice ( ), CloseProvider );
	
	setProperty ( kIOMatchCategoryKey, kSCSITaskUserClientIniterKey );
	
	characterDict = OSDynamicCast ( OSDictionary, fProvider->getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( characterDict == NULL )
	{
		
		characterDict = OSDictionary::withCapacity ( 1 );
		
	}
	
	else
	{
		characterDict->retain ( );
	}
	
	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
	}
	
	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
	
	characterDict->release ( );
	
	// Register this object as a nub for the Logical Unit Driver.
	registerService ( );
	
	STATUS_LOG ( ( "%s: Registered and setup is complete\n", getName ( ) ) );
	// Setup was successful, return true.
	result = true;
	
	return result;
	
	
CloseProvider:
	
	
	fProvider->close ( this );
	
	
ReleaseCommandObject:
	
	
	require_nonzero_quiet ( fSCSIPrimaryCommandObject, ErrorExit );
	fSCSIPrimaryCommandObject->release ( );
	fSCSIPrimaryCommandObject = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ stop - Called by IOKit to stop our services.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIPeripheralDeviceNub::stop ( IOService * provider )
{
	
	STATUS_LOG ( ("%s: stop called\n", getName( ) ) );
	
	require_nonzero ( fProvider, ErrorExit );
	require ( ( fProvider == provider ), ErrorExit );
	
	require_nonzero ( fSCSIPrimaryCommandObject, ErrorExit );
	
	fSCSIPrimaryCommandObject->release ( );
	fSCSIPrimaryCommandObject = NULL;
	
	super::stop ( provider );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Called by IOKit to deliver messages.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIPeripheralDeviceNub::message ( UInt32 		type,
									 IOService *	nub,
									 void *			arg )
{
	
	IOReturn status = kIOReturnSuccess;
	
	switch ( type )
	{
		
		// The device is no longer attached to the bus
		// close provider so message will continue
		// to propagate up the driver stack.
		case kIOMessageServiceIsRequestingClose:
		{
			
			STATUS_LOG ( ("%s: kIOMessageServiceIsRequestingClose called\n", getName( ) ) );
			if ( fProvider )
			{	
				STATUS_LOG ( ("%s: closing provider\n", getName( ) ) );
				fProvider->close ( this );
			}
			
		}
		break;
		
		case kSCSIProtocolNotification_VerifyDeviceState:
		{
			
			messageClients ( kSCSIProtocolNotification_VerifyDeviceState, NULL, NULL );
			status = kIOReturnSuccess;
			
		}
		break;
		
		default:
		{
			STATUS_LOG ( ("%s: some message = %ld called\n", getName ( ), type ) );
			status = super::message ( type, nub, arg );
		}
		break;
		
	}
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Power Management Utility Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ joinPMtree - Relays joinPMtree call to provider.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIPeripheralDeviceNub::joinPMtree ( IOService * driver )
{
	
	STATUS_LOG ( ( "%s%s::%s called%s\n", "\033[33m",
					getName ( ), __FUNCTION__, "\033[0m" ) );
	
	fProvider->joinPMtree ( driver );
	
	STATUS_LOG ( ( "%s%s::%s finished%s\n", "\033[33m",
					getName ( ), __FUNCTION__, "\033[0m" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InitializePowerManagement - Does nothing.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIPeripheralDeviceNub::InitializePowerManagement ( IOService * provider )
{
	
	// This piece does not get any power management
	// since it is just a relay class. Do NOT call registerPowerDriver
	// and do not call super::InitializePowerManagement
	// which does that.
	STATUS_LOG ( ( "%s%s::%s not doing anything%s\n", "\033[33m",
					getName ( ), __FUNCTION__, "\033[0m" ) );
	
}


#if 0
#pragma mark -
#pragma mark ¥ Property Table Utility Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ matchPropertyTable - Handles passive matching.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::matchPropertyTable (	OSDictionary *	table,
												SInt32 *		score )
{
	
	bool 	returnValue 	= true;
	bool 	isMatch 		= false;
	SInt32	propertyScore	= *score;
	
	// Clamp the copy of the score to 4000 to prevent us from promoting
	// a device driver a full ranking order. Our increments for ranking
	// order increase by 5000 and we add propertyScore to *score on exit
	// if the exiting value of *score is non-zero.
	if ( propertyScore >= 5000 )
	{
		propertyScore = 4000;
	}
	
	if ( sCompareIOProperty ( this, table, kIOPropertySCSIPeripheralDeviceType, &isMatch ) )
	{
		
		if ( isMatch )
		{
			*score = kDefaultProbeRanking;
		}
		
		else
		{
			
			*score = kPeripheralDeviceTypeNoMatch;
			returnValue = false;
		
		}

		if ( sCompareIOProperty ( this, table, kIOPropertySCSIVendorIdentification, &isMatch ) )
		{
			
			if ( isMatch )
			{
				
				*score = kFirstOrderRanking;

				if ( sCompareIOProperty ( this, table, 
						kIOPropertySCSIProductIdentification, &isMatch ) )
				{
					
					if ( isMatch )
					{
						
						*score = kSecondOrderRanking;

						if ( sCompareIOProperty ( this, table,
							kIOPropertySCSIProductRevisionLevel, &isMatch ) )
						{
							
							if ( isMatch ) 
							{
								*score = kThirdOrderRanking;
							}
							
							else
							{
							
								*score = kPeripheralDeviceTypeNoMatch;
								returnValue = false;							

							}
							
						}
					
					}
					else
					{
						
						*score = kPeripheralDeviceTypeNoMatch;
						returnValue = false;
						
					}
				
				}
			
			}
			
			else
			{
				
				*score = kPeripheralDeviceTypeNoMatch;
				returnValue = false;
			
			}
		
		}
	
	}
	
	else
	{
		
		// We need to special case the SCSITaskUserClient "driver"
		// because if no driver matches, then we want it to match on the device
		
		OSString *	string;
		
		string = OSDynamicCast ( OSString, table->getObject ( kIOMatchCategoryKey ) );
		if ( string != NULL )
		{
			
			if ( strcmp ( string->getCStringNoCopy ( ), kIOPropertySCSITaskUserClientDevice ) )
			{
				*score = kDefaultProbeRanking - 100;
			}
		
		}
	
		else
		{
			
			*score = kPeripheralDeviceTypeNoMatch;
			returnValue = false;
		
		}
		
	}
	
	if ( *score != 0 )
		*score += propertyScore;
	
	return returnValue;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sCompareIOProperty - Compares properties.				[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::sCompareIOProperty (
								IOService * 		object,
								OSDictionary * 		table,
								char * 				propertyKeyName,
								bool * 				matches )
{
	
	OSObject *	tableObject		= NULL;
	OSObject *	deviceObject	= NULL;
	bool 		returnValue		= false;
	
	*matches = false;
	
	tableObject  = table->getObject ( propertyKeyName );
	deviceObject = object->getProperty ( propertyKeyName );
	
	require_nonzero_quiet ( deviceObject, ErrorExit );
	require_nonzero_quiet ( tableObject, ErrorExit );
	
	returnValue = true;
	*matches = deviceObject->isEqualTo ( tableObject );
	
	
ErrorExit:
	
	
	return returnValue;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Provided Services Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendSCSICommand - Implements required method from IOSCSIProtocolServices.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::SendSCSICommand (
							SCSITaskIdentifier		request,
							SCSIServiceResponse * 	serviceResponse,
							SCSITaskStatus *		taskStatus )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortSCSICommand - Implements required method from IOSCSIProtocolServices.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::AbortSCSICommand ( SCSITaskIdentifier request )
{
	return kSCSIServiceResponse_FUNCTION_REJECTED;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ExecuteCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIPeripheralDeviceNub::ExecuteCommand ( SCSITaskIdentifier request )
{
	fProvider->ExecuteCommand ( request );
}


// The Task Management function to allow the SCSI Application Layer client to request
// that a specific task be aborted.
SCSIServiceResponse		
IOSCSIPeripheralDeviceNub::AbortTask( UInt8 theLogicalUnit, SCSITaggedTaskIdentifier theTag )
{
	return 	fProvider->AbortTask ( theLogicalUnit, theTag );
}

// The Task Management function to allow the SCSI Application Layer client to request
// that a all tasks curerntly in the task set be aborted.
SCSIServiceResponse		
IOSCSIPeripheralDeviceNub::AbortTaskSet( UInt8 theLogicalUnit )
{
	return 	fProvider->AbortTaskSet ( theLogicalUnit );
}

SCSIServiceResponse		
IOSCSIPeripheralDeviceNub::ClearACA( UInt8 theLogicalUnit )
{
	return 	fProvider->ClearACA ( theLogicalUnit );
}

SCSIServiceResponse		
IOSCSIPeripheralDeviceNub::ClearTaskSet( UInt8 theLogicalUnit )
{
	return 	fProvider->ClearTaskSet( theLogicalUnit );
}
    
SCSIServiceResponse		
IOSCSIPeripheralDeviceNub::LogicalUnitReset( UInt8 theLogicalUnit )
{
	return 	fProvider->LogicalUnitReset( theLogicalUnit );
}

SCSIServiceResponse		
IOSCSIPeripheralDeviceNub::TargetReset( void )
{
	return 	fProvider->TargetReset();
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::AbortCommand ( SCSITaskIdentifier abortTask )
{
	return fProvider->AbortCommand ( abortTask );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsProtocolServiceSupported - Relays command to protocol services driver.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::IsProtocolServiceSupported (
								SCSIProtocolFeature		feature,
								void *					serviceValue )
{
	return fProvider->IsProtocolServiceSupported ( feature, serviceValue );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleProtocolServiceFeature - Relays command to protocol services driver.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::HandleProtocolServiceFeature (
								SCSIProtocolFeature		feature,
								void *					serviceValue )
{
	return fProvider->HandleProtocolServiceFeature ( feature, serviceValue );
}


#if 0
#pragma mark -
#pragma mark ¥ Private method declarations
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TaskCallback - Callback method for synchronous commands.		  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOSCSIPeripheralDeviceNub::TaskCallback ( SCSITaskIdentifier completedTask )
{
	
	IOSyncer *				fSyncLock		= NULL;
	SCSITask *				scsiRequest		= NULL;
	SCSIServiceResponse 	serviceResponse;
	
	STATUS_LOG ( ( "IOSCSIPeripheralDeviceNub::TaskCallback called\n.") );
		
	scsiRequest = OSDynamicCast ( SCSITask, completedTask );
	require_nonzero_string ( scsiRequest, ErrorExit, "SCSITask is NULL\n" );
	
	fSyncLock = ( IOSyncer * ) scsiRequest->GetApplicationLayerReference ( );
	require_nonzero_string ( fSyncLock, ErrorExit, "fSyncLock is NULL\n" );
	
	serviceResponse = scsiRequest->GetServiceResponse ( );
	fSyncLock->signal ( serviceResponse, false );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendTask - Method to send synchronous commands.				  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSIPeripheralDeviceNub::SendTask ( SCSITask * request )
{
	
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOSyncer *				fSyncLock		= NULL;
	
	fSyncLock = IOSyncer::create ( false );
	require_nonzero_string ( fSyncLock, ErrorExit, "IOSyncer is NULL\n" );
	
	fSyncLock->signal ( kIOReturnSuccess, false );
	
	request->SetTaskCompletionCallback ( &IOSCSIPeripheralDeviceNub::TaskCallback );
	request->SetApplicationLayerReference ( ( void * ) fSyncLock );
	fSyncLock->reinit ( );
	
	ExecuteCommand ( request );
	
	// Wait for the completion routine to get called
	serviceResponse = ( SCSIServiceResponse ) fSyncLock->wait ( false );
	fSyncLock->release ( );
	
	
ErrorExit:
	
	
	return serviceResponse;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InterrogateDevice - Method to interrogate device.				  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIPeripheralDeviceNub::InterrogateDevice ( void )
{
	
	OSString *						string			= NULL;
	SCSICmd_INQUIRY_StandardData * 	inqData 		= NULL;
	SCSIServiceResponse 			serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSI_Sense_Data					senseBuffer		= { 0 };
	IOMemoryDescriptor *			bufferDesc 		= NULL;
	SCSITask *						request 		= NULL;
	UInt8							inqDataCount	= 0;
	int								index			= 0;
	bool							result			= false;
	char							tempString[17]; // Maximum + 1 for null char
	
   	// Before we register ourself as a nub, we need to find out what 
   	// type of device we want to connect to us
	// Do an Inquiry command and parse the data to determine the peripheral
	// device type.
	if ( fDefaultInquiryCount == 0 )
	{
		
		// There is no default Inquiry count for this device, use the standard
		// structure size.
		STATUS_LOG ( ( "%s: use sizeof(SCSICmd_INQUIRY_StandardData) for Inquiry.\n", getName ( ) ) );
		inqDataCount = sizeof ( SCSICmd_INQUIRY_StandardData );
		
	}
	
	else
	{
		
		// This device has a default inquiry count, use it.
		STATUS_LOG ( ( "%s: use fDefaultInquiryCount for Inquiry.\n", getName ( ) ) );
		inqDataCount = fDefaultInquiryCount;
		
	}
	
	inqData = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc ( inqDataCount );
	require_nonzero ( inqData, ErrorExit );
	bzero ( inqData, inqDataCount );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inqData,
												   inqDataCount,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseInquiryBuffer );
	
	request = new SCSITask;	
	require_nonzero ( request, ReleaseBuffer );
	request->ResetForNewTask ( );
	
	fSCSIPrimaryCommandObject->TEST_UNIT_READY ( request, 0x00 );
	
	// The command was successfully built, now send it
	serviceResponse = SendTask ( request );
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		bool validSense = false;

		if ( request->GetTaskStatus ( ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			validSense = request->GetAutoSenseData ( &senseBuffer, sizeof ( senseBuffer ) );
			if ( validSense == false )
			{
				
				request->ResetForNewTask ( );
				fSCSIPrimaryCommandObject->REQUEST_SENSE ( request,
														   bufferDesc,
														   kSenseDefaultSize,
														   0 );
				serviceResponse = SendTask ( request );
				
			}
			
		}
		
	}
	
 	IOSleep ( 100 );
 	
 	// Check if we got terminated. If so, bail early.
 	require ( ( isInactive ( ) == false ), ReleaseTask );
 	
   	for ( index = 0; ( index < kMaxInquiryAttempts ) && ( isInactive ( ) == false ); index++ )
	{
		
		fSCSIPrimaryCommandObject->INQUIRY (
									request,
									bufferDesc,
									0,
								  	0,
									0,
									inqDataCount,
									0 );
			
		serviceResponse = SendTask ( request );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( request->GetTaskStatus ( ) == kSCSITaskStatus_GOOD ) )
		{
			break;	
		}
		
		else
		{
			IOSleep ( 1000 );
		}
		
	}
	
	// Check if we have exhausted our max number of attempts. If so, bail early.
	require ( ( index != kMaxInquiryAttempts ), ReleaseTask );
 	
 	// Check if we got terminated. If so, bail early.
 	require ( isInactive ( ) == false, ReleaseTask );
	
   	// Set the Peripheral Device Type property for the device.
   	setProperty ( kIOPropertySCSIPeripheralDeviceType,
   				( UInt64 ) ( inqData->PERIPHERAL_DEVICE_TYPE & kINQUIRY_PERIPHERAL_TYPE_Mask ),
   				kIOPropertySCSIPeripheralDeviceTypeSize );
	
   	// Set the Vendor Identification property for the device.
   	for ( index = 0; index < kINQUIRY_VENDOR_IDENTIFICATION_Length; index++ )
   	{
   		tempString[index] = inqData->VENDOR_IDENTIFICATION[index];
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
   		tempString[index] = inqData->PRODUCT_INDENTIFICATION[index];
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
   		tempString[index] = inqData->PRODUCT_REVISION_LEVEL[index];
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
	
	result = true;
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseBuffer );
	request->release ( );
	request = NULL;
	
	
ReleaseBuffer:
	
	
	require_nonzero_quiet ( bufferDesc, ReleaseInquiryBuffer );
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseInquiryBuffer:
	
	
	require_nonzero_quiet ( inqData, ErrorExit );
	require_nonzero_quiet ( inqDataCount, ErrorExit );
	IOFree ( ( void * ) inqData, inqDataCount );
	inqData = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  1 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  2 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  3 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  4 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  5 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  8 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub,  9 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIPeripheralDeviceNub, 16 );


#if 0
#pragma mark -
#pragma mark ¥ IOSCSILogicalUnitNub
#pragma mark -
#endif


OSDefineMetaClassAndStructors ( IOSCSILogicalUnitNub, IOSCSIPeripheralDeviceNub );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSILogicalUnitNub::start ( IOService * provider )
{
	
	OSDictionary * 	characterDict 	= NULL;
	OSObject *		obj				= NULL;
	bool			result			= false;
	
	require ( super::start ( provider ), ErrorExit );
	
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceNub, provider );
	require_nonzero_quiet ( ( fProvider == NULL ), ErrorExit );
	
	fProvider = OSDynamicCast ( IOSCSIProtocolInterface, provider );
	require_nonzero_quiet ( fProvider, ErrorExit );
	
	fSCSIPrimaryCommandObject = new SCSIPrimaryCommands;
	require_nonzero ( fSCSIPrimaryCommandObject, ErrorExit );
	
	require ( fProvider->open ( this ), ReleaseCommandObject );
	
	STATUS_LOG ( ( "%s: Check for the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
	// Check if the personality for this device specifies a preferred protocol
	if ( ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) == NULL )
	{
		
		STATUS_LOG ( ( "%s: No SCSI Device Characteristics property, set defaults.\n", getName ( ) ) );
		fDefaultInquiryCount = 0;
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "%s: Get the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
		characterDict = OSDynamicCast ( OSDictionary, ( fProvider->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
		
		STATUS_LOG ( ( "%s: set this SCSI Device Characteristics property.\n", getName ( ) ) );
		setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );
		
		// Check if the personality for this device specifies a preferred protocol
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
	
	require ( InterrogateDevice ( ), CloseProvider );
		
	setProperty ( kIOMatchCategoryKey, kSCSITaskUserClientIniterKey );
	
	characterDict = OSDynamicCast ( OSDictionary, fProvider->getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( characterDict == NULL )
	{
		
		characterDict = OSDictionary::withCapacity ( 1 );
		
	}
	
	else
	{
		characterDict->retain ( );
	}
	
	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
	}

	obj = fProvider->getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
	
	characterDict->release ( );
	
	// Register this object as a nub for the Logical Unit Driver.
	registerService ( );
	
	STATUS_LOG ( ( "%s: Registered and setup is complete\n", getName ( ) ) );
	// Setup was successful, return true.
	result = true;
	
	return result;
	
	
CloseProvider:
	
	
	fProvider->close ( this );
	
	
ReleaseCommandObject:
	
	
	require_nonzero_quiet ( fSCSIPrimaryCommandObject, ErrorExit );
	fSCSIPrimaryCommandObject->release ( );
	fSCSIPrimaryCommandObject = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetLogicalUnitNumber - Sets the logical unit number for this device.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSILogicalUnitNub::SetLogicalUnitNumber ( UInt8 newLUN )
{
	
	STATUS_LOG ( ( "%s: SetLogicalUnitNumber to %d\n", getName ( ), newLUN ) );
	fLogicalUnitNumber = newLUN;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ExecuteCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSILogicalUnitNub::ExecuteCommand ( SCSITaskIdentifier request )
{
	
	STATUS_LOG ( ( "%s: ExecuteCommand for %d\n", getName ( ),
				 fLogicalUnitNumber ) );
	
	if ( fLogicalUnitNumber != 0 )
	{
		
		SCSITask *	scsiRequest;
		
	    scsiRequest = OSDynamicCast ( SCSITask, request );
	    if ( scsiRequest != NULL )
	    {
			
			scsiRequest->SetLogicalUnitNumber ( fLogicalUnitNumber );
			
		}
		
	}
	
	IOSCSIPeripheralDeviceNub::ExecuteCommand ( request );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortCommand - Relays command to protocol services driver.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOSCSILogicalUnitNub::AbortCommand ( SCSITaskIdentifier abortTask )
{
	return IOSCSIPeripheralDeviceNub::AbortCommand ( abortTask );
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  1 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  2 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  3 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  4 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  5 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  6 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  7 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  8 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub,  9 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 10 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 11 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 12 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 13 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 14 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 15 );
OSMetaClassDefineReservedUnused ( IOSCSILogicalUnitNub, 16 );