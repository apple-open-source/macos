/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
 
#include <libkern/OSByteOrder.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>

// SCSI Architecture definitions header files
#include <IOKit/scsi-commands/SCSITask.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>

// Definition for this class
#include <IOKit/scsi-commands/IOSCSIPeripheralDeviceNub.h>
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"

// For debugging, set SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG
#define SCSI_PERIPHERAL_DEVICE_NUB_DEBUGGING_LEVEL 0


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

#define kMaxInquiryAttempts 8

#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors ( IOSCSIPeripheralDeviceNub, IOSCSIProtocolServices );


bool
IOSCSIPeripheralDeviceNub::init ( OSDictionary * propTable )
{
	
	// Pass to super class for inspection
	if ( super::init ( propTable ) == false )
	{
		// Super didn't like it, return false
		return false;
	}
	
	return true;
	
}


bool
IOSCSIPeripheralDeviceNub::start ( IOService * provider )
{
	
	OSDictionary * 	characterDict = NULL;
	
	if ( !super::start ( provider ) )
	{
		return false;
   	}

	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceNub, provider );
	if ( fProvider != NULL )
	{
		
		// Our provider is one of us, don't load again.
		return false;
		
	}
	
	fProvider = OSDynamicCast ( IOSCSIProtocolServices, provider );
	if ( fProvider == NULL )
	{
		
		ERROR_LOG ( ( "%s: Provider was not a IOSCSIProtocolServices object\n", getName ( ) ) );
		return false;
		
	}
	
	fSCSIPrimaryCommandObject = new SCSIPrimaryCommands;
	if ( fSCSIPrimaryCommandObject == NULL )
	{
		
		ERROR_LOG ( ( "%s: Could not allocate a SCSIPrimaryCommands object\n", getName ( ) ) );
		return false;
		
	}
	
	if ( fProvider->open ( this ) == false )
	{
		
		ERROR_LOG ( ( "%s: Could not open provider\n", getName ( ) ) );
		if ( fSCSIPrimaryCommandObject != NULL )
		{
			
			fSCSIPrimaryCommandObject->release ( );
			fSCSIPrimaryCommandObject = NULL;
			
		}
		
		return false;
		
	}
	
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
	
	if ( InterrogateDevice ( ) == true )
	{
				
		OSObject *	obj;
		
		STATUS_LOG ( ( "%s: The device has been interrogated, register services.\n", getName ( ) ) );
		
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
   		return true;
		
	}
	
	// The setup was unsuccessful, return false to allow this object to
	// get removed.	
	stop ( provider );
	
	return false;
	
}


void
IOSCSIPeripheralDeviceNub::stop ( IOService * provider )
{
	
	if ( fProvider && ( fProvider == provider ) )
	{
		
		STATUS_LOG ( ("%s: stop called\n", getName( ) ) );
		if (  fSCSIPrimaryCommandObject != NULL )
		{
			
			fSCSIPrimaryCommandObject->release ( );
			fSCSIPrimaryCommandObject = NULL;
			
		}
		
		super::stop ( provider );
		
	}
	
}


IOReturn
IOSCSIPeripheralDeviceNub::message (	UInt32 		type,
										IOService *	nub,
										void *		arg )
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
			
			messageClients ( kSCSIProtocolNotification_VerifyDeviceState );
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


#pragma mark -
#pragma mark Power Management Utility Methods


void
IOSCSIPeripheralDeviceNub::joinPMtree ( IOService * driver )
{

	STATUS_LOG ( ( "%s%s::%s called%s\n", "\033[33m",
					getName ( ), __FUNCTION__, "\033[0m" ) );
	
	fProvider->joinPMtree ( driver );
	
	STATUS_LOG ( ( "%s%s::%s finished%s\n", "\033[33m",
					getName ( ), __FUNCTION__, "\033[0m" ) );
	
}


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



#pragma mark -
#pragma mark Property Table Utility Methods


bool
IOSCSIPeripheralDeviceNub::matchPropertyTable (	OSDictionary *	table,
												SInt32 *		score )
{
	
	bool 	returnValue 	= true;
	bool 	isMatch 		= false;
	SInt32	propertyScore	= *score;
	
	// Clamp the copy of the score to 4000 to prevent us from promoting a device
	// driver a full ranking order. Our increments for ranking order increase by 5000
	// and we add propertyScore to *score on exit if the exiting value of *score is non-zero.
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


bool
IOSCSIPeripheralDeviceNub::sCompareIOProperty (	IOService * 		object,
												OSDictionary * 		table,
												char * 				propertyKeyName,
												bool * 				matches )
{
	
	OSObject *	tableObject;
	OSObject *	deviceObject;
	bool 		returnValue = false;
	
	*matches = false;
	
	tableObject 	= table->getObject ( propertyKeyName );
	deviceObject 	= object->getProperty ( propertyKeyName );
	
	if ( ( deviceObject != NULL ) && ( tableObject != NULL ) )
	{
		
		returnValue = true;
		*matches = deviceObject->isEqualTo( tableObject );
		
	}
	
	return returnValue;
	
}


#pragma mark -
#pragma mark Provided Services Methods


bool
IOSCSIPeripheralDeviceNub::SendSCSICommand ( SCSITaskIdentifier 	request,
											 SCSIServiceResponse * 	serviceResponse,
											 SCSITaskStatus		 * 	taskStatus )
{
	return false;
}


SCSIServiceResponse
IOSCSIPeripheralDeviceNub::AbortSCSICommand ( SCSITaskIdentifier request )
{
	return kSCSIServiceResponse_FUNCTION_REJECTED;
}


// The ExecuteCommand function will take a SCSITask Object and transport
// it across the physical wire(s) to the device
void
IOSCSIPeripheralDeviceNub::ExecuteCommand ( SCSITaskIdentifier request )
{
	fProvider->ExecuteCommand ( request );
}


// The AbortCommand function will abort the indicated SCSITask object,
// if it is possible and the SCSITask has not already completed.
SCSIServiceResponse
IOSCSIPeripheralDeviceNub::AbortCommand ( SCSITaskIdentifier abortTask )
{
	return fProvider->AbortCommand ( abortTask );
}


// The IsProtocolServiceSupported will return true if the specified
// feature is supported by the protocol layer.  If the service has a value that must be
// returned, it will be returned in the serviceValue output parameter.
bool
IOSCSIPeripheralDeviceNub::IsProtocolServiceSupported ( SCSIProtocolFeature feature, void * serviceValue )
{
	return fProvider->IsProtocolServiceSupported ( feature, serviceValue );
}


bool
IOSCSIPeripheralDeviceNub::HandleProtocolServiceFeature ( SCSIProtocolFeature feature, void * serviceValue )
{
	return fProvider->HandleProtocolServiceFeature ( feature, serviceValue );
}


#pragma mark -
#pragma mark Private method declarations


void 
IOSCSIPeripheralDeviceNub::TaskCallback ( SCSITaskIdentifier completedTask )
{
	
	IOSyncer *				fSyncLock;
	SCSIServiceResponse 	serviceResponse;
	SCSITask *				scsiRequest;

	STATUS_LOG ( ( "IOSCSIPeripheralDeviceNub::TaskCallback called\n.") );
		
	scsiRequest = OSDynamicCast ( SCSITask, completedTask );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIPeripheralDeviceNub::TaskCallback scsiRequest==NULL." ) );
		ERROR_LOG ( ( "IOSCSIPeripheralDeviceNub::TaskCallback scsiRequest==NULL." ) );
		return;
		
	}
	
	fSyncLock = ( IOSyncer * ) scsiRequest->GetApplicationLayerReference ( );
	serviceResponse = scsiRequest->GetServiceResponse ( );
	fSyncLock->signal ( serviceResponse, false );
	
}


SCSIServiceResponse
IOSCSIPeripheralDeviceNub::SendTask ( SCSITask * request )
{
	
	SCSIServiceResponse 	serviceResponse;
	IOSyncer *				fSyncLock;
	
	fSyncLock = IOSyncer::create ( false );
	if ( fSyncLock == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIPeripheralDeviceNub::SendTask Allocate fSyncLock failed." ) );
		ERROR_LOG ( ( "IOSCSIPeripheralDeviceNub::TaskCallback scsiRequest==NULL." ) );
		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		
	}
	
	fSyncLock->signal ( kIOReturnSuccess, false );
	
	request->SetTaskCompletionCallback ( &this->TaskCallback );
	request->SetApplicationLayerReference ( ( void * ) fSyncLock );
	fSyncLock->reinit ( );
	
	STATUS_LOG ( ( "%s:SendTask Execute the command.\n", getName ( ) ) );
	ExecuteCommand ( request );
	
	STATUS_LOG ( ( "%s:SendTask wait for the signal.\n", getName ( ) ) );
	
	// Wait for the completion routine to get called
	serviceResponse = ( SCSIServiceResponse ) fSyncLock->wait ( false );
	fSyncLock->release ( );
	
	STATUS_LOG ( ( "%s:SendTask return the service response.\n", getName ( ) ) );
	return serviceResponse;
	
}


bool
IOSCSIPeripheralDeviceNub::InterrogateDevice ( void )
{
	
	OSString *						string;
	SCSICmd_INQUIRY_StandardData * 	inqData = NULL;
	UInt8							inqDataCount;
	int								loopCount;
	char							tempString[17]; // Maximum + 1 for null char
	IOMemoryDescriptor *			bufferDesc = NULL;
	SCSITask *						request = NULL;
	
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
	if ( inqData == NULL )
	{
		
		ERROR_LOG ( ( "%s: Couldn't allocate Inquiry buffer.\n", getName ( ) ) );
		return false;
		
	}
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) inqData, inqDataCount, kIODirectionIn );
	if ( bufferDesc == NULL )
	{
		
		ERROR_LOG ( ( "%s: Couldn't allocate Inquiry buffer descriptor.\n", getName ( ) ) );
		IOFree ( ( void * ) inqData, inqDataCount );
		inqData = NULL;
		return false;
		
	}
	
	bool 					cmdValid;
	SCSIServiceResponse 	serviceResponse;
	SCSI_Sense_Data			senseBuffer;
	
	STATUS_LOG ( ( "%s: Send a Test Unit Ready to prime the device.\n", getName ( ) ) );
	
	request = new SCSITask;	
	cmdValid = fSCSIPrimaryCommandObject->TEST_UNIT_READY ( request, 0x00 );
	if ( cmdValid == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendTask ( request );
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			bool validSense = false;

			STATUS_LOG ( ( "%s: Test Unit Ready completed.\n", getName ( ) ) );
			if ( request->GetTaskStatus ( ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				ERROR_LOG ( ( "%s: Check condition occurred.\n", getName ( ) ) );
				validSense = request->GetAutoSenseData ( &senseBuffer );
				if ( validSense == false )
				{
					
					request->ResetForNewTask ( );
					cmdValid = fSCSIPrimaryCommandObject->REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 );
					if ( cmdValid == true )
					{
						
						ERROR_LOG ( ( "%s: No autosense. Send request Sense.\n", getName ( ) ) );
						serviceResponse = SendTask ( request );
						
					}
					
				}
				
			}
			
		}
		
	}
	
 	IOSleep ( 100 );
 	
 	if ( isInactive ( ) )
 	{
		goto ERROR_EXIT;
	}
 	
	// We have seen cases in which a device will never succeed because the
	// device is wedged or gone - put upper limit to retries.
   	for ( loopCount = 0; loopCount < kMaxInquiryAttempts; loopCount++ )
	{
		
		cmdValid = fSCSIPrimaryCommandObject->INQUIRY (
											request,
											bufferDesc,
											0,
								  			0,
											0,
									 		inqDataCount,
											0 );
		if ( cmdValid == true )
		{
			
			// The command was successfully built, now send it
			STATUS_LOG ( ( "%s: Send the Inquiry command.\n", getName ( ) ) );
			serviceResponse = SendTask ( request );
			
		}
		
		else
		{
			
			ERROR_LOG ( ( "%s: INQUIRY command failed\n", getName ( ) ) );
			request->release ( );
			goto ERROR_EXIT;
			
		}
		
		if ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE )
		{
			IOSleep ( 1000 );
		}
		
		else
		{
			break;
		}
		
		// If we are terminated while spinning in this loop - bail
		if ( isInactive ( ) )
		{
			break;
		}
		
	}
	
	// We are finished with the buffer, release the object.
	bufferDesc->release ( );
	bufferDesc = NULL;
	request->release ( );
	
	if ( isInactive ( ) ) 
	{
		goto ERROR_EXIT;
	}

   	// Set the Peripheral Device Type property for the device.
   	setProperty ( kIOPropertySCSIPeripheralDeviceType,
   				( UInt64 ) ( inqData->PERIPHERAL_DEVICE_TYPE & kINQUIRY_PERIPHERAL_TYPE_Mask ), 8 );
	
   	// Set the Vendor Identification property for the device.
   	for ( loopCount = 0; loopCount < kINQUIRY_VENDOR_IDENTIFICATION_Length; loopCount++ )
   	{
   		tempString[loopCount] = inqData->VENDOR_IDENTIFICATION[loopCount];
   	}
	tempString[loopCount] = 0;
	
   	for ( loopCount = kINQUIRY_VENDOR_IDENTIFICATION_Length - 1; loopCount >= 0; loopCount-- )
   	{
   		
   		if ( tempString[loopCount] != ' ' )
   		{
   			
   			// Found a real character
   			tempString[loopCount+1] = '\0';
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
   	for ( loopCount = 0; loopCount < kINQUIRY_PRODUCT_IDENTIFICATION_Length; loopCount++ )
   	{
   		tempString[loopCount] = inqData->PRODUCT_INDENTIFICATION[loopCount];
   	}
   	tempString[loopCount] = 0;
	
   	for ( loopCount = kINQUIRY_PRODUCT_IDENTIFICATION_Length - 1; loopCount >= 0; loopCount-- )
   	{
   		
   		if ( tempString[loopCount] != ' ' )
   		{
   			
   			// Found a real character
   			tempString[loopCount+1] = '\0';
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
   	for ( loopCount = 0; loopCount < kINQUIRY_PRODUCT_REVISION_LEVEL_Length; loopCount++ )
   	{
   		tempString[loopCount] = inqData->PRODUCT_REVISION_LEVEL[loopCount];
   	}
   	tempString[loopCount] = 0;
	
   	for ( loopCount = kINQUIRY_PRODUCT_REVISION_LEVEL_Length - 1; loopCount >= 0; loopCount-- )
   	{
		
		if ( tempString[loopCount] != ' ' )
		{
			
			// Found a real character
			tempString[loopCount+1] = '\0';
			break;
			
		}
		
	}
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
		
		setProperty ( kIOPropertySCSIProductRevisionLevel, string );
		string->release ( );
		
	}
	
	// This object is done with the Inquiry data, free it now	
	if ( inqData != NULL )	
	{
		
		IOFree ( ( void * ) inqData, inqDataCount );
		inqData = NULL;
		
	}
	
	return true;
	
	
ERROR_EXIT:
	
	ERROR_LOG ( ( "%s: aborting startup.\n", getName ( ) ) );
	
	if ( bufferDesc != NULL )	
	{
		bufferDesc->release ( );
		bufferDesc = NULL;
	}
	
	if ( inqData != NULL )	
	{
		
		IOFree ( ( void * ) inqData, inqDataCount );
		inqData = NULL;
		
	}
	
	return false;
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 1 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 2 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 3 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 4 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 5 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 6 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 7 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 8 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 9 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 10 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 11 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 12 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 13 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 14 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 15 );
OSMetaClassDefineReservedUnused( IOSCSIPeripheralDeviceNub, 16 );

OSDefineMetaClassAndStructors( IOSCSILogicalUnitNub, IOSCSIPeripheralDeviceNub );

bool
IOSCSILogicalUnitNub::start( IOService * provider )
{
	
	OSDictionary * 	characterDict = NULL;
	
	if ( !super::start ( provider ) )
	{
		return false;
   	}

	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceNub, provider );
	if ( fProvider != NULL )
	{
		
		// Our provider is one of us, don't load again.
		return false;
		
	}
	
	fProvider = OSDynamicCast ( IOSCSIProtocolServices, provider );
	if ( fProvider == NULL )
	{
		
		ERROR_LOG ( ( "%s: Provider was not a IOSCSIProtocolServices object\n", getName ( ) ) );
		return false;
		
	}
	
	fSCSIPrimaryCommandObject = new SCSIPrimaryCommands;
	if ( fSCSIPrimaryCommandObject == NULL )
	{
		
		ERROR_LOG ( ( "%s: Could not allocate a SCSIPrimaryCommands object\n", getName ( ) ) );
		return false;
		
	}

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
	
	if ( InterrogateDevice ( ) == true )
	{
				
		OSObject *	obj;
		
		STATUS_LOG ( ( "%s: The device has been interrogated, register services.\n", getName ( ) ) );
		
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
   		return true;
		
	}
	
	// The setup was unsuccessful, return false to allow this object to
	// get removed.	
	stop ( provider );
	
	return false;
	
}

void
IOSCSILogicalUnitNub::SetLogicalUnitNumber( UInt8 newLUN )
{
	STATUS_LOG ( ( "%s: SetLogicalUnitNumber to %d\n", getName(), newLUN ) );
	fLogicalUnitNumber = newLUN;
}

// The ExecuteCommand function will take a SCSITask Object and transport
// it across the physical wire(s) to the device
void
IOSCSILogicalUnitNub::ExecuteCommand( SCSITaskIdentifier request )
{
	STATUS_LOG ( ( "%s: ExecuteCommand for %d\n", getName(), fLogicalUnitNumber ) );
	if( fLogicalUnitNumber != 0 )
	{
		SCSITask *	scsiRequest;
		
	    scsiRequest = OSDynamicCast( SCSITask, request );
	    if( scsiRequest != NULL )
	    {
			scsiRequest->SetLogicalUnitNumber( fLogicalUnitNumber );
		}
	}
	
	IOSCSIPeripheralDeviceNub::ExecuteCommand( request );
}


// The AbortCommand function will abort the indicated SCSITask object,
// if it is possible and the SCSITask has not already completed.
SCSIServiceResponse
IOSCSILogicalUnitNub::AbortCommand( SCSITaskIdentifier abortTask )
{
	return IOSCSIPeripheralDeviceNub::AbortCommand( abortTask );
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 1 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 2 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 3 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 4 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 5 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 6 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 7 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 8 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 9 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 10 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 11 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 12 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 13 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 14 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 15 );
OSMetaClassDefineReservedUnused( IOSCSILogicalUnitNub, 16 );
