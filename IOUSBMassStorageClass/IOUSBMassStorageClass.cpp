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

// General OS Services header files
#include <libkern/OSByteOrder.h>

// This class' header file
#include <IOKit/usb/IOUSBMassStorageClass.h>

#include <IOKit/scsi-commands/IOSCSIPeripheralDeviceNub.h>

// Macros for printing debugging information
#if (USB_MASS_STORAGE_DEBUG == 1)
#define STATUS_LOG(x)	IOLog x
#define PANIC_NOW(x)	IOPanic x
#else
#define STATUS_LOG(x)
#define PANIC_NOW(x)
#endif

#define DEBUGGING_LEVEL 0
#define DEBUGLOG kprintf

#define super IOSCSIProtocolServices

OSDefineMetaClassAndStructors( IOUSBMassStorageClass, IOSCSIProtocolServices )

bool 
IOUSBMassStorageClass::init( OSDictionary * propTable )
{
    if( super::init( propTable ) == false)
    {
        return false;
    }

    return true;
}

bool 
IOUSBMassStorageClass::start( IOService * provider )
{
    //IOReturn					status = kIOReturnSuccess;
    IOUSBFindEndpointRequest 	request;
    
    if( super::start( provider ) == false )
    {
    	STATUS_LOG(("%s: superclass start failure.\n", 
    				getName()));
        return false;
    }

    // remember my device
    SetInterfaceReference( OSDynamicCast( IOUSBInterface, provider) );
    if ( GetInterfaceReference() == NULL )
    {
    	STATUS_LOG(("%s: the provider is not an IOUSBInterface object\n", 
    				getName()));
    	// If our provider is not a IOUSBInterface object, return false
    	// to indicate that the object could not be correctly 
		// instantiated.
    	// The USB Mass Storage Class specification requires that all 
		// devices be a composite device with a Mass Storage interface
		// so this object will always be an interface driver.
        return false;
    }

    STATUS_LOG(("%s: USB Mass Storage @ %d\n", 
    			getName(),
                GetInterfaceReference()->GetDevice()->GetAddress()));

    if ( GetInterfaceReference()->open( this ) == false) 
    {
    	STATUS_LOG(("%s: could not open the interface\n", getName()));
   		return false;
    }

	// Set the IOUSBPipe object pointers to NULL so that the driver can 
	// release these objects if instantition is not successful.
    fBulkInPipe 	= NULL;
    fBulkOutPipe	= NULL;
    fInterruptPipe	= NULL;

	// Initialize all Bulk Only related member variables to their default 	
	// states.
	fBulkOnlyCommandTag = 0;
	fBulkOnlyCommandStructInUse = false;
	
	// Initialize all CBI related member variables to their default 	
	// states.
	fCBICommandStructInUse = false;

	// Check if the personality for this device specifies a preferred protocol
	if ( getProperty( kIOUSBMassStorageCharacteristics ) == NULL )
	{
		// This device does not specify a preferred protocol, use the protocol
		// defined in the descriptor.
		fPreferredProtocol = GetInterfaceReference()->GetInterfaceProtocol();
		fPreferredSubclass = GetInterfaceReference()->GetInterfaceSubClass();
	}
	else
	{
		OSDictionary * characterDict;
		
		characterDict = OSDynamicCast( OSDictionary, getProperty( kIOUSBMassStorageCharacteristics ));
		
		// Check if the personality for this device specifies a preferred protocol
		if ( characterDict->getObject( kIOUSBMassStoragePreferredProtocol ) == NULL )
		{
			// This device does not specify a preferred protocol, use the protocol
			// defined in the descriptor.
			fPreferredProtocol = GetInterfaceReference()->GetInterfaceProtocol();
		}
		else
		{
	    	OSNumber *	preferredProtocol;
			
			preferredProtocol = OSDynamicCast( OSNumber, characterDict->getObject( kIOUSBMassStoragePreferredProtocol ));
			
			// This device has a preferred protocol, use that.
			fPreferredProtocol = preferredProtocol->unsigned32BitValue();
		}
		
		// Check if the personality for this device specifies a preferred subclass
		if ( characterDict->getObject( kIOUSBMassStoragePreferredSubclass ) == NULL )
		{
			// This device does not specify a preferred subclass, use the subclass
			// defined in the descriptor.
			fPreferredSubclass = GetInterfaceReference()->GetInterfaceSubClass();
		}
		else
		{
	    	OSNumber *	preferredSubclass;
			
			preferredSubclass = OSDynamicCast( OSNumber, characterDict->getObject( kIOUSBMassStoragePreferredSubclass ));
			
			// This device has a preferred protocol, use that.
			fPreferredSubclass = preferredSubclass->unsigned32BitValue();
		}
	}
		
	STATUS_LOG(("%s: Preferred Protocol is: %d\n", getName(), fPreferredProtocol));
    STATUS_LOG(("%s: Preferred Subclass is: %d\n", getName(), fPreferredSubclass));

	// Verify that the device has a supported interface type and configure that
	// Interrupt pipe if the protocol requires one.
    STATUS_LOG(("%s: Configure the Storage interface\n", getName()));
    switch ( GetInterfaceProtocol() )
    {
    	case kProtocolControlBulkInterrupt:
    	{
	         // Find the interrupt pipe for the device
	        request.type = kUSBInterrupt;
	        request.direction = kUSBIn;
 			fInterruptPipe = GetInterfaceReference()->FindNextPipe(NULL, &request);

	        STATUS_LOG(("%s: find interrupt pipe\n", getName()));
	        if(( GetInterfaceProtocol() == kProtocolControlBulkInterrupt) && (fInterruptPipe == 0))
	        {
	            // This is a CBI device and must have an interrupt pipe, 
	            // halt configuration since one could not be found
	            STATUS_LOG(("%s: No interrupt pipe for CBI, abort\n", getName()));
	            goto abortStart;
	        }
	    }
    	break;
    	
    	case kProtocolControlBulk:
    	// Since all the CB devices I have seen do not use the interrupt
    	// endpoint, even if it exists, ignore it if present.
    	case kProtocolBulkOnly:
    	{
	        STATUS_LOG(("%s: Bulk Only - skip interrupt pipe\n",
	        		 	getName()));
	        // Since this is a Bulk Only device, do not look for
	        // interrupt and set the pipe object to NULL so that the
	        // driver can not try to use it.
	        fInterruptPipe 	= NULL;
	    }
	    break;
	    
	    default:
	    {
	    	// The device has a protocol that the driver does not
	    	// support. Return false to indicate that instantiation was
	    	// not successful.
    		goto abortStart;
	    }
	    break;
    }

	// Find the Bulk In pipe for the device
    STATUS_LOG(("%s: find bulk in pipe\n", getName()));
	request.type = kUSBBulk;
	request.direction = kUSBIn;
	fBulkInPipe = GetInterfaceReference()->FindNextPipe(NULL, &request);
	if ( fBulkInPipe == NULL )
	{
		// We could not find the bulk in pipe, not much a bulk transfer
		// device can do without this, so fail the configuration.
    	STATUS_LOG(("%s: No bulk in pipe found, aborting\n",
    				getName()));
    	goto abortStart;
	}
	
	// Find the Bulk Out pipe for the device
    STATUS_LOG(("%s: find bulk out pipe\n", getName()));
	request.type = kUSBBulk;
	request.direction = kUSBOut;
	fBulkOutPipe = GetInterfaceReference()->FindNextPipe(NULL, &request);
	if ( fBulkOutPipe == NULL )
	{
		// We could not find the bulk out pipe, not much a bulk transfer
		// device can do without this, so fail the configuration.
    	STATUS_LOG(("%s: No bulk out pipe found, aborting\n",
    			 	getName()));
    	goto abortStart;
	}

	// Next step, get our descriptor strings starting with the 
	// manufacturer string
/*	status = GetInterfaceReference()->GetStringDescriptor( 
					GetInterfaceReference()->deviceDescriptor()->manuIdx, 
					fManufacturerString, 
					255);
    STATUS_LOG(("%s: Manufacturer's name: %s\n", getName(), 
				fManufacturerString));

	// Now get the product name string
	status = GetInterfaceReference()->GetStringDescriptor( 
					GetInterfaceReference()->deviceDescriptor()->prodIdx, 
					fProductString, 
					255);
    STATUS_LOG(("%s: Product name: %s\n", getName(), fProductString));
*/

   	STATUS_LOG(("%s: successfully configured\n", getName()));

	InitializePowerManagement( GetInterfaceReference() );
	BeginProvidedServices();       
    
    return true;

abortStart:
    STATUS_LOG(("%s: aborting startup.  Stop the provider.\n", getName() ));

	// Call the stop method to clean up any allocated resources.
    stop( provider );
    
    return false;
}

void 
IOUSBMassStorageClass::stop(IOService * provider)
{
    STATUS_LOG(("%s: Bye bye!\n", getName()));

	EndProvidedServices();

	// Tell the interface object to close all pipes since the driver is 
	// going away.
   	fBulkInPipe 	= NULL;
   	fBulkOutPipe 	= NULL;
   	fInterruptPipe 	= NULL;

    super::stop(provider);
}

IOReturn
IOUSBMassStorageClass::message( UInt32 type, IOService * provider, void * argument = 0 )
{
	IOReturn	result;
	
	STATUS_LOG ( ("%s: message = %lx called\n", getName(), type ) );
	IOLog("%s: message = %lx called\n", getName(), type );
	switch( type )
	{
		case kIOMessageServiceIsTerminated:
		{
			IOUSBInterface * currentInterface;

			STATUS_LOG(("%s: message  kIOMessageServiceIsTerminated.\n", getName() ));
			
			currentInterface = GetInterfaceReference();
			if ( currentInterface != NULL )
			{				
				SetInterfaceReference( NULL );

				// Close our interface
			    currentInterface->close(this);

				// Let the clients know that the device is gone.
				SendNotification_DeviceRemoved( );
			}
			
			result = kIOReturnSuccess;
		}
		break;

		case kIOMessageServiceIsRequestingClose:
		{
			IOUSBInterface * currentInterface;

    		STATUS_LOG(("%s: message  kIOMessageServiceIsRequestingClose.\n", getName() ));
			
			// Let the clients know that the device is gone.
			SendNotification_DeviceRemoved( );

			currentInterface = GetInterfaceReference();
			if ( currentInterface != NULL )
			{				
				SetInterfaceReference( NULL );

				// Close our interface
			    currentInterface->close(this);
			}
			
			result = kIOReturnSuccess;
		}
		break;
					
		default:
		{
			result = super::message( type, provider, argument );
		}
	}
	
	return result;
}

bool	
IOUSBMassStorageClass::BeginProvidedServices( void )
{
 	// If this is a BO device that supports multiple LUNs, we will need 
	// to spawn off a nub for each valid LUN.  If this is a CBI/CB
	// device or a BO device that only supports LUN 0, this object can
	// register itself as the nub.  
    STATUS_LOG(("%s: Determine the maximum LUN\n", getName()));
    if( GetInterfaceProtocol() == kProtocolBulkOnly )
    {
    	IOReturn	status;
    	
        // The device is a Bulk Only transport device, issue the
        // GetMaxLUN call to determine what the maximum value is.

		// Build the USB command
		fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
	   	fUSBDeviceRequest.bRequest 			= 0xFE;
	   	fUSBDeviceRequest.wValue			= 0;
		fUSBDeviceRequest.wIndex			= 0;

		fUSBDeviceRequest.wLength			= 1;
	   	fUSBDeviceRequest.pData				= &fMaxLogicalUnitNumber;
		
		// Send the command over the control endpoint
		status = GetInterfaceReference()->DeviceRequest( &fUSBDeviceRequest );
		if ( status != kIOReturnSuccess )
		{
        	fMaxLogicalUnitNumber = 0;
        	if( status == kIOUSBPipeStalled )
        	{
        		UInt8	eStatus[2];

        		// Throw in an extra Get Status to clear up devices that stall the
        		// control pipe like the early Iomega devices.
        		GetStatusEndpointStatus( GetControlPipe(), &eStatus[0], NULL);
        	}
        }
    }
    else
    {
    	// CBI and CB protocols do not support LUNs so for these the 
    	// maximum LUN will always be zero.
        fMaxLogicalUnitNumber = 0;
    }

    STATUS_LOG(("%s: Maximum supported LUN is: %d\n", 
    			getName(), 
    			fMaxLogicalUnitNumber));
    	
   	STATUS_LOG(("%s: successfully configured\n", getName()));

 	// If this is a BO device that supports multiple LUNs, we will need 
	// to spawn off a nub for each valid LUN.  If this is a CBI/CB
	// device or a BO device that only supports LUN 0, this object can
	// register itself as the nub.  
 	if ( fMaxLogicalUnitNumber == 0 )
 	{    
		registerService();
    }
    else
    {
        for ( int loopLUN = 0; loopLUN <= fMaxLogicalUnitNumber; loopLUN++)
        {
		    STATUS_LOG ( ( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN entering.\n" ) );

			IOSCSILogicalUnitNub * 	nub = new IOSCSILogicalUnitNub;
			
			if ( nub == NULL )
			{
				PANIC_NOW(( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN failed\n" ));
				return false;
			}
			
			nub->init( 0 );
			
			if ( nub->attach( this ) == false )
			{
				// panic since the nub can't attach
				PANIC_NOW(( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN unable to attach nub" ));
				return false;
			}
						
			nub->SetLogicalUnitNumber( loopLUN );
			if( nub->start( this ) == false )
			{
				nub->detach( this );
			}
			else
			{
				nub->registerService( kIOServiceSynchronous );
			}

			nub->release();
			
			STATUS_LOG ( ( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN exiting.\n" ) );
		}
    }

	return true;
}

bool	
IOUSBMassStorageClass::EndProvidedServices( void )
{
	return true;
}

#pragma mark -
#pragma mark CDB Transport Methods
bool 
IOUSBMassStorageClass::SendSCSICommand( 	
									SCSITaskIdentifier 			request, 
									SCSIServiceResponse *		serviceResponse,
									SCSITaskStatus *			taskStatus )
{
	IOReturn status;

	// Set the defaults to an error state.		
	*taskStatus = kSCSITaskStatus_No_Status;
	*serviceResponse =  kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
   	STATUS_LOG(("%s: SendSCSICommand was called\n", getName()));
 
  	if ( request == NULL )
 	{
 		// We were given an invalid SCSI Task object.  Let the client know.
  		STATUS_LOG(("%s: SendSCSICommand was called with a NULL CDB \n", 
					getName()));
		return true;
 	}

	if ( GetInterfaceReference() == NULL )
	{
		return false;
	}

	SCSICommandDescriptorBlock	cdbData;
	
	STATUS_LOG(("%s: SendSCSICommand CDB data: ", getName()));
	GetCommandDescriptorBlock( request, &cdbData );
	for ( int i=0; i < GetCommandDescriptorBlockSize(request); i ++ )
	{
		STATUS_LOG(("%X : ", cdbData[i]));
	}
	STATUS_LOG(( "\n" ));
	
   	if( GetInterfaceProtocol() == kProtocolBulkOnly)
	{
		if ( fBulkOnlyCommandStructInUse == true )
		{
			return false;
		}
		
		fBulkOnlyCommandStructInUse = true;
   		STATUS_LOG(("%s: SendSCSICommandforBulkOnlyProtocol sent \n", getName() ));
		status = SendSCSICommandForBulkOnlyProtocol( request );
   		STATUS_LOG(("%s: SendSCSICommandforBulkOnlyProtocol returned %d\n",
   			 		getName(), 
   			 		status));
	}
	else
	{
		if ( fCBICommandStructInUse == true )
		{
			return false;
		}
		
		fCBICommandStructInUse = true;
		status = SendSCSICommandForCBIProtocol( request );
   		STATUS_LOG(("%s: SendSCSICommandforCBIProtocol returned %d\n", 
   					getName(), 
   					status));
	}

	if ( status == kIOReturnSuccess )
	{	
		*serviceResponse = kSCSIServiceResponse_Request_In_Process;
	}

	return true;
}

void 
IOUSBMassStorageClass::CompleteSCSICommand( SCSITaskIdentifier request, IOReturn status )
{
	fBulkOnlyCommandStructInUse = false;
	fCBICommandStructInUse = false;
		
	if ( status == kIOReturnSuccess )
	{	
		CommandCompleted( request, kSCSIServiceResponse_TASK_COMPLETE, kSCSITaskStatus_GOOD );
	}
	else
	{
		CommandCompleted( request, kSCSIServiceResponse_TASK_COMPLETE, kSCSITaskStatus_CHECK_CONDITION );
	}
}

SCSIServiceResponse 
IOUSBMassStorageClass::AbortSCSICommand( SCSITaskIdentifier abortTask )
{
 	IOReturn	status = kIOReturnSuccess;
 	
  	STATUS_LOG(("%s: AbortSCSICommand was called\n", getName()));
 	if ( abortTask == NULL )
 	{
 		// We were given an invalid SCSI Task object.  Let the client know.
  		STATUS_LOG(("%s: AbortSCSICommand was called with a NULL CDB object\n", 
					getName()));
 		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
 	}
	
	if ( GetInterfaceReference() == NULL )
	{
 		// We have an invalid interface, the device has probably been removed.
 		// Nothing else to do except to report an error.
  		STATUS_LOG(("%s: AbortSCSICommand was called with a NULL interface\n", 
					getName()));
 		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	}
	
	if(GetInterfaceReference()->GetInterfaceProtocol() == kProtocolBulkOnly)
	{
		status = AbortSCSICommandForBulkOnlyProtocol( abortTask );
   		STATUS_LOG(("%s: abortCDBforBulkOnlyProtocol returned %d\n", 
					getName(), 
					status));
	}
	else
	{
		status = AbortSCSICommandForCBIProtocol( abortTask );
   		STATUS_LOG(("%s: abortCDBforCBIProtocol returned %d\n", 
   					getName(), 
   					status));
	}

	// Since the driver currently does not support abort, return an error	
	return kSCSIServiceResponse_FUNCTION_REJECTED;
}
    
bool
IOUSBMassStorageClass::IsProtocolServiceSupported( 
										SCSIProtocolFeature 	feature, 
										void * 					serviceValue )
{
	bool	isSupported;
	
	STATUS_LOG(( "IOUSBMassStorageClass::IsProtocolServiceSupported called\n"));
	
    // all features just return false
	switch( feature )
	{
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
		{
			*((UInt32 *) serviceValue) = fMaxLogicalUnitNumber;
			isSupported = true;
		}
		break;
		
		default:
		{
			isSupported = false;
		}
		break;

	}

	return isSupported;
}

bool
IOUSBMassStorageClass::HandleProtocolServiceFeature( 
										SCSIProtocolFeature 	feature, 
										void * 					serviceValue )
{
	return false;
}


#pragma mark -
#pragma mark Standard USB Command Methods

// Method to do the CLEAR_FEATURE command for an ENDPOINT_STALL feature.
IOReturn 
IOUSBMassStorageClass::ClearFeatureEndpointStall( 
								IOUSBPipe *			thePipe,
								IOUSBCompletion	*	completion )
{
	IOReturn			status;

	if ( GetInterfaceReference() == NULL )
	{
 		// We have an invalid interface, the device has probably been removed.
 		// Nothing else to do except to report an error.
 		return kIOReturnDeviceError;
	}
	
	// Make sure that the Data Toggles are reset before doing the 
	// Clear Stall.
	thePipe->Reset();

	// Clear out the structure for the request
	bzero( &fUSBDeviceRequest, sizeof(IOUSBDevRequest));

	// Build the USB command
	fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBStandard, kUSBEndpoint);
   	fUSBDeviceRequest.bRequest 			= kUSBRqClearFeature;
   	fUSBDeviceRequest.wValue			= 0;	// Zero is EndpointStall
	fUSBDeviceRequest.wIndex			= thePipe->GetEndpointNumber();
	if ( thePipe == GetBulkInPipe() )
	{
		fUSBDeviceRequest.wIndex		|= 0x80;
	}

	// Send the command over the control endpoint
	status = GetInterfaceReference()->DeviceRequest( &fUSBDeviceRequest, completion );
   	STATUS_LOG(("%s: ClearFeatureEndpointStall returned %d\n", 
   				getName(), 
   				status));
	return status;
}

// Method to do the GET_STATUS command for the endpoint that the
// IOUSBPipe is connected to.
IOReturn 
IOUSBMassStorageClass::GetStatusEndpointStatus( 
								IOUSBPipe *			thePipe,
								void *				endpointStatus,
								IOUSBCompletion	*	completion ) 
{
	IOReturn			status;

	if ( GetInterfaceReference() == NULL )
	{
 		// We have an invalid interface, the device has probably been removed.
 		// Nothing else to do except to report an error.
 		return kIOReturnDeviceError;
	}

	// Make sure that the Data Toggles are reset before doing the 
	// Clear Stall.
	thePipe->Reset();

	// Clear out the structure for the request
	bzero( &fUSBDeviceRequest, sizeof(IOUSBDevRequest));

	// Build the USB command	
    fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType( kUSBIn, kUSBStandard, kUSBEndpoint);	
   	fUSBDeviceRequest.bRequest 			= kUSBRqGetStatus;
   	fUSBDeviceRequest.wValue			= 0;	// Zero is EndpointStall
	fUSBDeviceRequest.wIndex			= thePipe->GetEndpointNumber();

	if ( thePipe == GetBulkInPipe() )
	{
		fUSBDeviceRequest.wIndex		|= 0x80;
	}
	
	fUSBDeviceRequest.wLength			= 2;
   	fUSBDeviceRequest.pData				= endpointStatus;

	// Send the command over the control endpoint
	status = GetInterfaceReference()->DeviceRequest( &fUSBDeviceRequest, completion );
   	STATUS_LOG(("%s: GetStatusEndpointStatus returned %d\n", 
   				getName(), 
   				status));
	return status;
}

#pragma mark -
#pragma mark Accessor Methods For All Protocol Variables
/* The following methods are for use only by this class */
inline IOUSBInterface *
IOUSBMassStorageClass::GetInterfaceReference( void )
{
   	STATUS_LOG(("%s: GetInterfaceReference \n", getName() ));
   	if ( fInterface == NULL )
   	{
   		STATUS_LOG(("%s: GetInterfaceReference - Interface is NULL.\n", getName() ));
   	}
   	
	return fInterface;
}

inline void
IOUSBMassStorageClass::SetInterfaceReference( IOUSBInterface * newInterface )
{
	fInterface = newInterface;
}

inline UInt8
IOUSBMassStorageClass::GetInterfaceSubclass( void )
{
	return fPreferredSubclass;
}

inline UInt8
IOUSBMassStorageClass::GetInterfaceProtocol( void )
{
	return fPreferredProtocol;
}

inline IOUSBPipe *
IOUSBMassStorageClass::GetControlPipe( void )
{
	if ( GetInterfaceReference() == NULL )
	{
		return NULL;
	}
	
	return GetInterfaceReference()->GetDevice()->GetPipeZero();
}

inline IOUSBPipe *
IOUSBMassStorageClass::GetBulkInPipe( void )
{
	return fBulkInPipe;
}

inline IOUSBPipe *
IOUSBMassStorageClass::GetBulkOutPipe( void )
{
	return fBulkOutPipe;
}

inline IOUSBPipe *
IOUSBMassStorageClass::GetInterruptPipe( void )
{
	return fInterruptPipe;
}

#pragma mark -
#pragma mark Accessor Methods For CBI Protocol Variables

CBIRequestBlock *	
IOUSBMassStorageClass::GetCBIRequestBlock( void )
{
	// Return a pointer to the CBIRequestBlock
	return &fCBICommandRequestBlock;
}

void	
IOUSBMassStorageClass::ReleaseCBIRequestBlock( CBIRequestBlock * cbiRequestBlock )
{
	// Clear the request and completion to avoid possible double callbacks.
	cbiRequestBlock->request = NULL;

	// Since we only allow one command and the CBIRequestBlock is
	// a member variable, no need to do anything.
	return;
}

#pragma mark -
#pragma mark Accessor Methods For Bulk Only Protocol Variables

BulkOnlyRequestBlock *	
IOUSBMassStorageClass::GetBulkOnlyRequestBlock( void )
{
	// Return a pointer to the BulkOnlyRequestBlock
	return &fBulkOnlyCommandRequestBlock;
}

void	
IOUSBMassStorageClass::ReleaseBulkOnlyRequestBlock( BulkOnlyRequestBlock * boRequestBlock )
{
	// Clear the request and completion to avoid possible double callbacks.
	boRequestBlock->request = NULL;

	// Since we only allow one command and the BulkOnlyRequestBlock is
	// a member variable, no need to do anything.
	return;
}

UInt32	
IOUSBMassStorageClass::GetNextBulkOnlyCommandTag( void )
{
	fBulkOnlyCommandTag++;
	
	return fBulkOnlyCommandTag;
}

// Will get called when a device has been resumed
IOReturn
IOUSBMassStorageClass::HandlePowerOn ( void )
{
#if 0
	UInt8	eStatus[2];
        	
	// The USB hub port that the device is connected to has been resumed,
	// check to see if the device is still responding correctly and if not, 
	// fix it so that it is. 
	STATUS_LOG(("%s: HandlePowerOn", getName() ));
	if ( GetStatusEndpointStatus( GetBulkInPipe(), &eStatus[0], NULL) != kIOReturnSuccess)
	{
		// The endpoint status could not be retrieved meaning that the device has
		// stopped responding.  Begin the device reset sequence.
		
    			STATUS_LOG(("%s: kIOMessageServiceIsResumed GetStatusEndpointStatus error.\n", getName() ));
	
		(GetInterfaceReference()->GetDevice())->ResetDevice();
		
		// Once the device has been reset, send notification to the client so that the
		// device can be reconfigured for use.
		SendNotification_VerifyDeviceState();
	}
#endif
	return kIOReturnSuccess;
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 1 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 2 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 3 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 4 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 5 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 6 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 7 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 8 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 9 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 10 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 11 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 12 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 13 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 14 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 15 );
OSMetaClassDefineReservedUnused( IOUSBMassStorageClass, 16 );
