/*
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
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

// General OS Services header files
#include <libkern/OSByteOrder.h>

// This class' header file
#include "IOUSBMassStorageClass.h"

#include "Debugging.h"

#include <IOKit/scsi/IOSCSIPeripheralDeviceNub.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>


//--------------------------------------------------------------------------------------------------
//	Defines
//--------------------------------------------------------------------------------------------------

// Masks for GetDeviceInformation response. GetDeviceInformation returns a UInt32. 
enum
{
	kUSBDeviceInfoIsCaptiveMask             =  	( 1 << kUSBInformationDeviceIsCaptiveBit ),
	kUSBDeviceInfoIsAttachedToRootHubMask   =  	( 1 << kUSBInformationDeviceIsAttachedToRootHubBit ),
	kUSBDeviceInfoIsInternalMask            =  	( 1 << kUSBInformationDeviceIsInternalBit ),
	kUSBDeviceInfoIsConnectedMask           =  	( 1 << kUSBInformationDeviceIsConnectedBit ),
	kUSBDeviceInfoIsEnabledMask             =  	( 1 << kUSBInformationDeviceIsEnabledBit ),
	kUSBDeviceInfoIsSuspendedMask           =  	( 1 << kUSBInformationDeviceIsSuspendedBit ),
	kUSBDeviceInfoIsInResetMask             =  	( 1 << kUSBInformationDeviceIsInResetBit ),
	kUSBDeviceInfoOvercurrentMask          	= 	( 1 << kUSBInformationDeviceOvercurrentBit ),
	kUSBDeviceInfoIsePortIsInTestModeMask   = 	( 1 << kUSBInformationDevicePortIsInTestModeBit )
};


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

// Macros for printing debugging information
#if (USB_MASS_STORAGE_DEBUG == 1)
#define PANIC_NOW(x)	IOPanic x
// Override the debug level for USBLog to make sure our logs make it out and then import
// the logging header.
#define DEBUG_LEVEL		1
#include <IOKit/usb/IOUSBLog.h>
#define STATUS_LOG(x)	USBLog x
#else
#define STATUS_LOG(x)
#define PANIC_NOW(x)
#endif

#define DEBUGGING_LEVEL 0
#define DEBUGLOG kprintf

#define super IOSCSIProtocolServices

OSDefineMetaClassAndStructors( IOUSBMassStorageClass, IOSCSIProtocolServices )

//-----------------------------------------------------------------------------
//	- init - Called at initialization time							   [PUBLIC]
//-----------------------------------------------------------------------------

bool 
IOUSBMassStorageClass::init( OSDictionary * propTable )
{
    if( super::init( propTable ) == false)
    {
		return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
//	- start - Called at services start time	(after successful matching)
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

bool 
IOUSBMassStorageClass::start( IOService * provider )
{
    IOUSBFindEndpointRequest 	request;
	OSDictionary * 				characterDict 	= NULL;
	OSObject *					obj				= NULL;
    IOReturn                    result          = kIOReturnError;
    
    
    if( super::start( provider ) == false )
    {
    	STATUS_LOG(( 1, "%s[%p]: superclass start failure.", getName(), this));
        return false;
    }
	
	// Allocate data for our expansion data.
	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	bzero ( reserved, sizeof ( ExpansionData ) );
	
    // Save the reference to the interface on the device that will be
    // the provider for this object.
    SetInterfaceReference( OSDynamicCast( IOUSBInterface, provider) );
    if ( GetInterfaceReference() == NULL )
    {
    	STATUS_LOG(( 1, "%s[%p]: the provider is not an IOUSBInterface object",
    				getName(), this ));
    	// If our provider is not a IOUSBInterface object, return false
    	// to indicate that the object could not be correctly 
		// instantiated.
    	// The USB Mass Storage Class specification requires that all 
		// devices be a composite device with a Mass Storage interface
		// so this object will always be an interface driver.
        return false;
    }

    STATUS_LOG(( 6, "%s[%p]: USB Mass Storage @ %d", 
    			getName(), this,
                GetInterfaceReference()->GetDevice()->GetAddress()));

    if ( GetInterfaceReference()->open( this ) == false) 
    {
    	STATUS_LOG(( 1, "%s[%p]: could not open the interface", getName(), this ));
   		return false;
    }

	// Set the IOUSBPipe object pointers to NULL so that the driver can 
	// release these objects if instantition is not successful.
    fBulkInPipe 	= NULL;
    fBulkOutPipe	= NULL;
    fInterruptPipe	= NULL;
	
	// Default is to have no clients
	fClients		= NULL;
	
	// Default is to have a max lun of 0.
	SetMaxLogicalUnitNumber( 0 );
	
	// Initialize all Bulk Only related member variables to their default 	
	// states.
	fBulkOnlyCommandTag = 0;
	fBulkOnlyCommandStructInUse = false;
	
	// Initialize all CBI related member variables to their default 	
	// states.
	fCBICommandStructInUse = false;

    // Flag we use to indicate whether or not the device requires the standard
    // USB device reset instead of the BO reset. This applies to BO devices only.
    fUseUSBResetNotBOReset = false;
    
    // Certain Bulk-Only device are subject to erroneous CSW tags.
    fKnownCSWTagMismatchIssues = false;
	
	// We shouldn't be waiting for a reset and reconfiguration yet.
	fWaitingForReconfigurationMessage = false;
    fTerminating = false;
    
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
		
		characterDict = OSDynamicCast( OSDictionary, 
							getProperty( kIOUSBMassStorageCharacteristics ));
		
		// Check if the personality for this device specifies a preferred
		// protocol
		if ( characterDict->getObject( kIOUSBMassStoragePreferredProtocol ) == NULL )
		{
			// This device does not specify a preferred protocol, use the
			// protocol defined in the interface descriptor.
			fPreferredProtocol = 
					GetInterfaceReference()->GetInterfaceProtocol();
		}
		else
		{
	    	OSNumber *	preferredProtocol;
			
			preferredProtocol = OSDynamicCast( OSNumber, 
				characterDict->getObject( kIOUSBMassStoragePreferredProtocol ));
			
			// This device has a preferred protocol, use that.
			fPreferredProtocol = preferredProtocol->unsigned32BitValue();
		}
		
		// Check if this device is known not to support the bulk-only USB reset.
        if ( characterDict->getObject( kIOUSBMassStorageDoNotOperate ) != NULL )
        {
            goto abortStart;
        }
		
        // Check if this device is known not to support the bulk-only USB reset.
        if ( characterDict->getObject( kIOUSBMassStorageUseStandardUSBReset ) != NULL )
        {
            fUseUSBResetNotBOReset = true;
        }
        
        // Is this a device which has CBW/CSW tag issues?
        if ( characterDict->getObject( kIOUSBKnownCSWTagIssues ) != NULL )
        {
            fKnownCSWTagMismatchIssues = true;
        }
                    
		// Check if the personality for this device specifies a preferred 
		// subclass
		if ( characterDict->getObject( kIOUSBMassStoragePreferredSubclass ) == NULL )
		{
			// This device does not specify a preferred subclass, use the 
			// subclass defined in the interface descriptor.
			fPreferredSubclass = 
					GetInterfaceReference()->GetInterfaceSubClass();
		}
		else
		{
	    	OSNumber *	preferredSubclass;
			
			preferredSubclass = OSDynamicCast( OSNumber, characterDict->getObject( kIOUSBMassStoragePreferredSubclass ));
			
			// This device has a preferred protocol, use that.
			fPreferredSubclass = preferredSubclass->unsigned32BitValue();
		}
 
	}
		
	STATUS_LOG(( 6, "%s[%p]: Preferred Protocol is: %d", getName(), this, fPreferredProtocol));
    STATUS_LOG(( 6, "%s[%p]: Preferred Subclass is: %d", getName(), this, fPreferredSubclass));

	// Verify that the device has a supported interface type and configure that
	// Interrupt pipe if the protocol requires one.
    STATUS_LOG(( 7, "%s[%p]: Configure the Storage interface", getName(), this));
    switch ( GetInterfaceProtocol() )
    {
    	case kProtocolControlBulkInterrupt:
    	{
        
            // Find the interrupt pipe for the device
	        request.type = kUSBInterrupt;
	        request.direction = kUSBIn;
 			fInterruptPipe = GetInterfaceReference()->FindNextPipe(NULL, &request);

	        STATUS_LOG(( 7, "%s[%p]: find interrupt pipe", getName(), this));
	        if(( GetInterfaceProtocol() == kProtocolControlBulkInterrupt) 
	        		&& (fInterruptPipe == 0))
	        {
	            // This is a CBI device and must have an interrupt pipe, 
	            // halt configuration since one could not be found
	            STATUS_LOG((1, "%s[%p]: No interrupt pipe for CBI, abort", getName(), this));
	            goto abortStart;
	        }
			
			fCBIMemoryDescriptor = IOMemoryDescriptor::withAddress(
											&fCBICommandRequestBlock.cbiGetStatusBuffer, 
											kUSBStorageAutoStatusSize, 
											kIODirectionIn);
            require_nonzero ( fCBIMemoryDescriptor, abortStart );

            result = fCBIMemoryDescriptor->prepare();
            require_success ( result, abortStart );
            
	    }
    	break;
    	
    	case kProtocolControlBulk:
    	// Since all the CB devices I have seen do not use the interrupt
    	// endpoint, even if it exists, ignore it if present.
    	case kProtocolBulkOnly:
    	{
        
	        STATUS_LOG(( 7, "%s[%p]: Bulk Only - skip interrupt pipe", getName(), this));
	        // Since this is a Bulk Only device, do not look for
	        // interrupt and set the pipe object to NULL so that the
	        // driver can not try to use it.
	        fInterruptPipe 	= NULL;
            
            // Allocate the memory descriptor needed to send the CBW out.
            fBulkOnlyCBWMemoryDescriptor = IOMemoryDescriptor::withAddress( 
                                                &fBulkOnlyCommandRequestBlock.boCBW, 
                                                kByteCountOfCBW, 
                                                kIODirectionOut);
            require_nonzero ( fBulkOnlyCBWMemoryDescriptor, abortStart );
            
            result = fBulkOnlyCBWMemoryDescriptor->prepare();
            require_success ( result, abortStart );
            
            // Allocate the memory descriptor needed to retrieve the CSW.
            fBulkOnlyCSWMemoryDescriptor = IOMemoryDescriptor::withAddress( 
                                                &fBulkOnlyCommandRequestBlock.boCSW, 
                                                kByteCountOfCSW, 
                                                kIODirectionIn);
            require_nonzero ( fBulkOnlyCSWMemoryDescriptor, abortStart );

            result = fBulkOnlyCSWMemoryDescriptor->prepare();
            require_success ( result, abortStart );
            
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
    STATUS_LOG(( 7, "%s[%p]: find bulk in pipe", getName(), this));
	request.type = kUSBBulk;
	request.direction = kUSBIn;
	fBulkInPipe = GetInterfaceReference()->FindNextPipe(NULL, &request);
	if ( fBulkInPipe == NULL )
	{
		// We could not find the bulk in pipe, not much a bulk transfer
		// device can do without this, so fail the configuration.
    	STATUS_LOG((1, "%s[%p]: No bulk in pipe found, aborting",  getName(), this));
    	goto abortStart;
	}
	
	// Find the Bulk Out pipe for the device
    STATUS_LOG(( 7, "%s[%p]: find bulk out pipe", getName(), this));
	request.type = kUSBBulk;
	request.direction = kUSBOut;
	fBulkOutPipe = GetInterfaceReference()->FindNextPipe(NULL, &request);
	if ( fBulkOutPipe == NULL )
	{
		// We could not find the bulk out pipe, not much a bulk transfer
		// device can do without this, so fail the configuration.
    	STATUS_LOG(( 1, "%s[%p]: No bulk out pipe found, aborting", getName(), this));
    	goto abortStart;
	}
	
	// Build the Protocol Charactersitics dictionary since not all devices will have a 
	// SCSI Peripheral Device Nub to guarantee its existance.
	characterDict = OSDynamicCast ( OSDictionary, getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
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
        
        obj = getProperty ( kIOPropertyPhysicalInterconnectTypeKey );
        if ( obj != NULL )
        {
            characterDict->setObject ( kIOPropertyPhysicalInterconnectTypeKey, obj );
        }
        
        obj = getProperty ( kIOPropertyPhysicalInterconnectLocationKey );
        if ( obj != NULL )
        {
            characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, obj );
        }
            
        // Check to see if this device is internal or not. The results of this check if successful 
        // will override location data provided in IOKit personalities. 
        if ( IsPhysicalInterconnectLocationInternal ( ) )
        {
        
            OSString *      internalString = NULL;
            
            
            // As this shall be a rarity, log it. Internal devices should only be configured at 
            // boot time so USB Logger isn't an option.
            if ( USB_MASS_STORAGE_DEBUG == 1 ) 
            {
                IOLog ( "%s[%p]: Configuring INTERNAL USB storage device", getName(), this );
            }
            
            internalString = OSString::withCString ( kIOPropertyInternalKey );
            if ( internalString != NULL )
            {
                
                characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, internalString );
                internalString->release ( );
                internalString = NULL;
                
            }

        }
        
        obj = getProperty ( kIOPropertyReadTimeOutDurationKey );	
        if ( obj != NULL );
        {
            characterDict->setObject ( kIOPropertyReadTimeOutDurationKey, obj );
        } 
        
        obj = getProperty ( kIOPropertyWriteTimeOutDurationKey );	
        if ( obj != NULL );
        {
            characterDict->setObject ( kIOPropertyWriteTimeOutDurationKey, obj );
        }

        setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
        characterDict->release ( );
        characterDict = NULL;
        
    }
    
   	STATUS_LOG(( 6, "%s[%p]: successfully configured", getName(), this));


#if defined (__i386__) 
	{
		// As USB booting is only supporting on i386 based, do not compile for PPC. 
		char				usbDeviceAddress[10];
		OSNumber *			usbDeviceID;
		
		sprintf ( usbDeviceAddress, "%x", ( int ) GetInterfaceReference()->GetDevice()->GetAddress() );
		
		usbDeviceID = OSNumber::withNumber ( ( int ) GetInterfaceReference()->GetDevice()->GetAddress(), 64 );
		if ( usbDeviceID != NULL )
		{
		
			setProperty ( kIOPropertyIOUnitKey, usbDeviceID );
			setLocation ( ( const char * ) usbDeviceAddress, gIODTPlane );
			
			usbDeviceID->release ( );
			
		}
	}
#endif




	// Device configured. We're attached.
	fDeviceAttached = true;

	InitializePowerManagement( GetInterfaceReference() );
	BeginProvidedServices();       
    
    return true;


abortStart:

    STATUS_LOG(( 1, "%s[%p]: aborting startup.  Stop the provider.", getName(), this ));

	if ( fCBIMemoryDescriptor != NULL )
	{
		fCBIMemoryDescriptor->complete();
		fCBIMemoryDescriptor->release();
        fCBIMemoryDescriptor = NULL;
	}
	
	if ( fBulkOnlyCBWMemoryDescriptor != NULL )
	{
		fBulkOnlyCBWMemoryDescriptor->complete();
		fBulkOnlyCBWMemoryDescriptor->release();
        fBulkOnlyCBWMemoryDescriptor = NULL;
	}
	
	if ( fBulkOnlyCSWMemoryDescriptor != NULL )
	{
		fBulkOnlyCSWMemoryDescriptor->complete();
		fBulkOnlyCSWMemoryDescriptor->release();
        fBulkOnlyCSWMemoryDescriptor = NULL;
	}
	
	// Call the stop method to clean up any allocated resources.
    stop( provider );
    
    return false;
}


//-----------------------------------------------------------------------------
//	- stop - Called at stop time								   [PUBLIC]
//-----------------------------------------------------------------------------

void 
IOUSBMassStorageClass::stop(IOService * provider)
{
	// I am logging this as a 1 because if anything is logging after this we want to know about it.
	// This should be the last message we see.
    STATUS_LOG(( 1, "%s[%p]: Bye bye!", getName(), this));
	
	EndProvidedServices();

	// Tell the interface object to close all pipes since the driver is 
	// going away.
	
   	fBulkInPipe 	= NULL;
   	fBulkOutPipe 	= NULL;
   	fInterruptPipe 	= NULL;

    super::stop(provider);
}


//-----------------------------------------------------------------------------
//	- free - Called by IOKit to free any resources.					   [PUBLIC]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::free ( void )
{
	
	if ( reserved != NULL )
	{
		
		// Since fClients is defined as reserved->fClients we don't want
		// to dereference it unless reserved is non-NULL.
		if ( fClients != NULL )
		{
			fClients->release();
			fClients = NULL;
		}
		
		if ( fCBIMemoryDescriptor != NULL )
		{
			fCBIMemoryDescriptor->complete();
			fCBIMemoryDescriptor->release();
            fCBIMemoryDescriptor = NULL;
		}
		
		if ( fBulkOnlyCBWMemoryDescriptor != NULL )
		{
			fBulkOnlyCBWMemoryDescriptor->complete();
			fBulkOnlyCBWMemoryDescriptor->release();
            fBulkOnlyCBWMemoryDescriptor = NULL;
		}
		
		if ( fBulkOnlyCSWMemoryDescriptor != NULL )
		{
			fBulkOnlyCSWMemoryDescriptor->complete();
			fBulkOnlyCSWMemoryDescriptor->release();
            fBulkOnlyCSWMemoryDescriptor = NULL;
		}
		
		IOFree ( reserved, sizeof ( ExpansionData ) );
		reserved = NULL;
		
	}
	
	super::free ( );
	
}


//-----------------------------------------------------------------------------
//	- message - Called by IOKit to deliver messages.				   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::message( UInt32 type, IOService * provider, void * argument )
{
	IOReturn	result;
	
	STATUS_LOG ( (4, "%s[%p]: message = %lx called", getName(), this, type ) );
	switch( type )
	{
    
        // Waiting for kIOUSBMessageCompositeDriverReconfigured to be define in USB.h
		case kIOUSBMessageCompositeDriverReconfigured:
		{
			
			STATUS_LOG((2, "%s[%p]: message  kIOUSBMessageCompositeDriverReconfigured.", getName(), this));
		
			if ( fWaitingForReconfigurationMessage )
			{
			
				fWaitingForReconfigurationMessage = false;
				
				FinishDeviceRecovery ( kIOReturnSuccess );
            
			}
			
		}
		break;
					
		default:
		{
            STATUS_LOG((2, "%s[%p]: message default case.", getName(), this));
			result = super::message( type, provider, argument );
		}
        
	}
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	- willTerminate                                                    [PUBLIC]
//-----------------------------------------------------------------------------

bool        
IOUSBMassStorageClass::willTerminate(  IOService *     provider, 
                                        IOOptionBits    options )
{
    IOUSBInterface * currentInterface;
			
    STATUS_LOG((2, "%s[%p]: willTerminate called.", getName(), this ));
    
    currentInterface = GetInterfaceReference();
    if ( currentInterface != NULL )
    {				
    
        STATUS_LOG((2, "%s[%p]: willTerminate interface is non NULL.", getName(), this));
        
        // Mark ourselves as terminating so we don't accept any additional IO.
        fTerminating = true;
        
        // Let the clients know that the device is gone.
        SendNotification_DeviceRemoved( );

    }
    
    return super::willTerminate( provider, options );
}


//-----------------------------------------------------------------------------
//	- didTerminate                                                     [PUBLIC]
//-----------------------------------------------------------------------------

bool
IOUSBMassStorageClass::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to
    // hold on to the device and IOKit will terminate us when we close it later
    //
    STATUS_LOG ((3 , "%s[%p]::didTerminate isInactive = %d", getName(), this, isInactive() ));
    
    // This should already be set to true, but can't hurt ...
    fTerminating = true;
    
    if ( ( GetInterfaceReference() != NULL ) && ( fResetInProgress == false ) ) 
    {

        IOUSBInterface * currentInterface;
        
        currentInterface = GetInterfaceReference();
        
        // We set this to NULL first chance we get to prevent a potential race between
        // our reset thread and our didTeriminate(). 
        SetInterfaceReference( NULL );
        
        // Close and nullify our USB Interface.
        currentInterface->close( this );
        
    }

    return super::didTerminate(provider, options, defer);
}


//-----------------------------------------------------------------------------
//	- BeginProvidedServices											[PROTECTED]
//-----------------------------------------------------------------------------

bool	
IOUSBMassStorageClass::BeginProvidedServices( void )
{
    
 	// If this is a BO device that supports multiple LUNs, we will need 
	// to spawn off a nub for each valid LUN.  If this is a CBI/CB
	// device or a BO device that only supports LUN 0, this object can
	// register itself as the nub.  
    STATUS_LOG(( 7, "%s[%p]: Determine the maximum LUN", getName(), this ));
	
  	if( GetInterfaceProtocol() == kProtocolBulkOnly )
    {
    	IOReturn	status;
    	bool		maxLUNDetermined = false;
		
		// Before we issue the GetMaxLUN call let's check if this device
		// specifies a MaxLogicalUnitNumber as part of its personality.
		if( getProperty( kIOUSBMassStorageCharacteristics ) != NULL )
		{
			OSDictionary * characterDict = OSDynamicCast( OSDictionary, 
														  getProperty( kIOUSBMassStorageCharacteristics ));
			
			// Check if this device is known to have problems when waking from sleep
			if( characterDict->getObject( kIOUSBMassStorageMaxLogicalUnitNumber ) != NULL )
			{
				OSNumber *	maxLUN = OSDynamicCast( OSNumber,
													characterDict->getObject( kIOUSBMassStorageMaxLogicalUnitNumber ) );
				if( maxLUN != NULL )
				{
					SetMaxLogicalUnitNumber( maxLUN->unsigned8BitValue() );
					STATUS_LOG(( 4, "%s[%p]: Number of LUNs %u.", getName(), this, maxLUN->unsigned8BitValue() ));
					maxLUNDetermined = true;
				}
			}
		}
		
		if( maxLUNDetermined == false )
		{
			// The device is a Bulk Only transport device, issue the
			// GetMaxLUN call to determine what the maximum value is.
			bool		triedReset = false;
			UInt8		clearPipeAttempts = 0;		

			// We want to loop until we get a satisfactory response to GetMaxLUN, either an answer or failure.
			while ( status != kIOReturnSuccess )
			{
				
				// Build the USB command
				fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
				fUSBDeviceRequest.bRequest 			= 0xFE;
				fUSBDeviceRequest.wValue			= 0;
				fUSBDeviceRequest.wIndex			= GetInterfaceReference()->GetInterfaceNumber();
				fUSBDeviceRequest.wLength			= 1;
				fUSBDeviceRequest.pData				= &fMaxLogicalUnitNumber;
				
				STATUS_LOG(( 4, "%s[%p]: Issuing device request to find max LUN", getName(), this ));
				
				// Send the command over the control endpoint
				status = GetInterfaceReference()->DeviceRequest( &fUSBDeviceRequest );
				
				STATUS_LOG(( 4, "%s[%p]: DeviceRequest GetMaxLUN returned status = %x", getName(), this, status ));
				
				if ( status != kIOReturnSuccess )
				{
					
					SetMaxLogicalUnitNumber( 0 );
					if( ( status == kIOUSBPipeStalled ) && ( clearPipeAttempts < 3 ) )
					{
						UInt8		eStatus[2];
						
						STATUS_LOG(( 4, "%s[%p]: calling GetStatusEndpointStatus to clear stall", getName(), this ));
						
						// Throw in an extra Get Status to clear up devices that stall the
						// control pipe like the early Iomega devices.
						GetStatusEndpointStatus( GetControlPipe(), &eStatus[0], NULL);
						
						clearPipeAttempts++;
					}
					else if ( ( status == kIOReturnNotResponding ) && ( triedReset == false ) )
					{
						// The device isn't responding. Let us reset the device, and try again.
				
						// The endpoint status could not be retrieved meaning that the device has
						// stopped responding. Or this could be a device we know needs a reset.
						// Begin the device reset sequence.
						
						STATUS_LOG(( 4, "%s[%p]: BeginProvidedServices: device not responding, reseting.", getName(), this ));
						
						// Reset the device on its own thread so we don't deadlock.
						ResetDeviceNow ( true );

						triedReset = true;
					}
					else
					{
						break;
					}
					
				}
					
			}
		}
			
    }
    else
    {
    	// CBI and CB protocols do not support LUNs so for these the 
    	// maximum LUN will always be zero.
        SetMaxLogicalUnitNumber( 0 );
    }
    

    STATUS_LOG(( 5, "%s[%p]: Maximum supported LUN is: %d", getName(), this, GetMaxLogicalUnitNumber() ));
	
   	STATUS_LOG(( 7, "%s[%p]: successfully configured", getName(), this ));

 	// If this is a BO device that supports multiple LUNs, we will need 
	// to spawn off a nub for each valid LUN.  If this is a CBI/CB
	// device or a BO device that only supports LUN 0, this object can
	// register itself as the nub.  
 	if ( GetMaxLogicalUnitNumber() == 0 )
 	{    
		registerService(kIOServiceAsynchronous);
		
		fClients = NULL;
    }
    else
    {
		// Allocate space for our set that will keep track of the LUNs.
		fClients = OSSet::withCapacity( GetMaxLogicalUnitNumber() + 1 );
	
        for( int loopLUN = 0; loopLUN <= GetMaxLogicalUnitNumber(); loopLUN++ )
        {
		    STATUS_LOG(( 6, "%s[%p]::CreatePeripheralDeviceNubForLUN entering.", getName(), this ));

			IOSCSILogicalUnitNub * 	nub = OSTypeAlloc( IOSCSILogicalUnitNub );
			
			if( nub == NULL )
			{
				PANIC_NOW(( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN failed" ));
				return false;
			}
			
			if ( nub->init( 0 ) == false )
            {
                // Release our nub before we return so we don't leak...
                nub->release();
                // We didn't init successfully so we should return false.
                return false;
            }
            
            if( nub->attach( this ) == false )
            {
                if( isInactive() == false )
                {
                    // panic since the nub can't attach and we are active
                    PANIC_NOW(( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN unable to attach nub" ));
                }
                
                // Release our nub before we return so we don't leak...
                nub->release();
                // We didn't attach so we should return false.
                return false;
            }
                        
            nub->SetLogicalUnitNumber( loopLUN );
            if( nub->start( this ) == false )
            {
                nub->detach( this );
            }
            else
            {
                nub->registerService( kIOServiceAsynchronous );
            }
            
            nub->release();
			nub = NULL;
            
			STATUS_LOG(( 6, "%s[%p]::CreatePeripheralDeviceNubForLUN exiting.", getName(), this ));
		}
    }

	return true;
    
}


//-----------------------------------------------------------------------------
//	- EndProvidedServices											[PROTECTED]
//-----------------------------------------------------------------------------

bool	
IOUSBMassStorageClass::EndProvidedServices( void )
{
	return true;
}


#pragma mark -
#pragma mark *** CDB Transport Methods ***
#pragma mark -


//-----------------------------------------------------------------------------
//	- SendSCSICommand												[PROTECTED]
//-----------------------------------------------------------------------------

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
	
   	STATUS_LOG(( 6, "%s[%p]: SendSCSICommand was called", getName(), this ));

	// If we have been marked as inactive, or no longer have the device, return false.
	if ( isInactive () || ( fDeviceAttached == false ) || ( fTerminating == true ) )
	{	
		return false;
	}
    

#if (USB_MASS_STORAGE_DEBUG == 1)
	SCSICommandDescriptorBlock	cdbData;
	
	STATUS_LOG(( 4, "%s[%p]: SendSCSICommand CDB data: ", getName(), this ));
	GetCommandDescriptorBlock( request, &cdbData );
	
	if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_6Byte )
		STATUS_LOG(( 4, "%s[%p]: %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5]));
	else if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_10Byte )
		STATUS_LOG(( 4, "%s[%p]: %X : %X : %X : %X : %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5], 
                    cdbData[6], cdbData[7], cdbData[8], cdbData[9]));
	else if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_12Byte )
		STATUS_LOG(( 4, "%s[%p]: %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5], 
                    cdbData[6], cdbData[7], cdbData[8], cdbData[9], 
                    cdbData[10], cdbData[11]));
	else if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_16Byte )
		STATUS_LOG(( 4, "%s[%p]: %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5], 
                    cdbData[6], cdbData[7], cdbData[8], cdbData[9], 
                    cdbData[10], cdbData[11], cdbData[12], cdbData[13], 
                    cdbData[14], cdbData[15]));
#endif
	
   	if( GetInterfaceProtocol() == kProtocolBulkOnly)
	{
		if ( fBulkOnlyCommandStructInUse == true )
		{
			return false;
		}
		
		fBulkOnlyCommandStructInUse = true;
   		STATUS_LOG(( 6, "%s[%p]: SendSCSICommandforBulkOnlyProtocol sent", getName(), this ));
		status = SendSCSICommandForBulkOnlyProtocol( request );
		if( status != kIOReturnSuccess )
		{
			// If the command fails we want to make sure that we
			// don't hold up other commands.
			fBulkOnlyCommandStructInUse = false;
		}
   		STATUS_LOG(( 5, "%s[%p]: SendSCSICommandforBulkOnlyProtocol returned %x", getName(), this, status ));
	}
	else
	{
		if ( fCBICommandStructInUse == true )
		{
			return false;
		}
		
		fCBICommandStructInUse = true;
		status = SendSCSICommandForCBIProtocol( request );
		if( status != kIOReturnSuccess )
		{
			// If the command fails we want to make sure that we
			// don't hold up other commands.
			fCBICommandStructInUse = false;
		}
   		STATUS_LOG(( 5, "%s[%p]: SendSCSICommandforCBIProtocol returned %x", getName(), this, status ));
	}

	if ( status == kIOReturnSuccess )
	{	
		*serviceResponse = kSCSIServiceResponse_Request_In_Process;
	}

	return true;
}


//-----------------------------------------------------------------------------
//	- CompleteSCSICommand											[PROTECTED]
//-----------------------------------------------------------------------------

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
  		STATUS_LOG(( 2, "%s[%p]: CompleteSCSICommand kSCSITaskStatus_CHECK_CONDITION", getName(), this ));
		CommandCompleted( request, kSCSIServiceResponse_TASK_COMPLETE, kSCSITaskStatus_CHECK_CONDITION );
	}
}


//-----------------------------------------------------------------------------
//	- AbortSCSICommand												[PROTECTED]
//-----------------------------------------------------------------------------

SCSIServiceResponse
IOUSBMassStorageClass::AbortSCSICommand( SCSITaskIdentifier abortTask )
{
 	IOReturn	status = kIOReturnSuccess;
 	
  	STATUS_LOG(( 6, "%s[%p]: AbortSCSICommand was called", getName(), this ));
 	if ( abortTask == NULL )
 	{
 		// We were given an invalid SCSI Task object.  Let the client know.
  		STATUS_LOG(( 1, "%s[%p]: AbortSCSICommand was called with a NULL CDB object", getName(), this ));
 		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
 	}
	
	if ( fTerminating == true )
	{
 		// We have an invalid interface, the device has probably been removed.
 		// Nothing else to do except to report an error.
  		STATUS_LOG(( 1, "%s[%p]: AbortSCSICommand was called with a NULL interface", getName(), this ));
 		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	}
	
	if(GetInterfaceReference()->GetInterfaceProtocol() == kProtocolBulkOnly)
	{
		status = AbortSCSICommandForBulkOnlyProtocol( abortTask );
   		STATUS_LOG(( 5, "%s[%p]: abortCDBforBulkOnlyProtocol returned %x", getName(), this, status ));
	}
	else
	{
		status = AbortSCSICommandForCBIProtocol( abortTask );
   		STATUS_LOG(( 5, "%s[%p]: abortCDBforCBIProtocol returned %x", getName(), this, status ));
	}

	// Since the driver currently does not support abort, return an error	
	return kSCSIServiceResponse_FUNCTION_REJECTED;
}


//-----------------------------------------------------------------------------
//	- IsProtocolServiceSupported									[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOUSBMassStorageClass::IsProtocolServiceSupported( 
										SCSIProtocolFeature 	feature, 
										void * 					serviceValue )
{
	bool	isSupported = false;
	
	STATUS_LOG(( 6,  "%s[%p]::IsProtocolServiceSupported called", getName(), this ));
	
    // all features just return false
	switch( feature )
	{
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
		{
			*((UInt32 *) serviceValue) = GetMaxLogicalUnitNumber();
			isSupported = true;
		}
		break;
	
		case kSCSIProtocolFeature_MaximumReadBlockTransferCount:
		case kSCSIProtocolFeature_MaximumWriteBlockTransferCount:
		case kSCSIProtocolFeature_MaximumReadTransferByteCount:
		case kSCSIProtocolFeature_MaximumWriteTransferByteCount:
		{
        
            // Do we have SCSI Device Characteristics dictionary which may state preferences for
            // max transfer sizes?
            if ( ( getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) != NULL )
            {
            
                OSDictionary *	characterDict	= NULL;
                OSNumber    *	number			= NULL;
                UInt32          maxBlockCount   = 0;
                UInt64          maxByteCount    = 0;
                
                
                // Get the SCSI Characteristics Dictionary.
                characterDict = OSDynamicCast ( OSDictionary, ( getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
             
                if ( feature == kSCSIProtocolFeature_MaximumReadBlockTransferCount )
                {
					// Does the IOKit personality specify a maximum number of blocks?
					number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumBlockCountReadKey ) );
					require ( ( number != NULL ), Exit );
							
					maxBlockCount = number->unsigned32BitValue ( );
					require ( ( maxBlockCount != 0 ), Exit );

					*((UInt32 *) serviceValue) = maxBlockCount;
					isSupported = true;
					
                }
                
                if ( feature == kSCSIProtocolFeature_MaximumWriteBlockTransferCount )
                {
					// Does the IOKit personality specify a maximum number of blocks?
					number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumBlockCountWriteKey ) );
					require ( number, Exit );
	
					maxBlockCount = number->unsigned32BitValue ( );
					require ( maxBlockCount, Exit );
					
					*((UInt32 *) serviceValue) = maxBlockCount;
					isSupported = true;
                
                }
                
                if ( feature == kSCSIProtocolFeature_MaximumReadTransferByteCount )
                {
					// Does the IOKit personality specify a maximum number of bytes?
					number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumByteCountReadKey ) );
					require ( number, Exit );
							
					maxByteCount = number->unsigned64BitValue ( );
					require ( maxByteCount, Exit );
					
					*((UInt64 *) serviceValue) = maxByteCount;
					isSupported = true;
			
                }
                
                if ( feature == kSCSIProtocolFeature_MaximumWriteTransferByteCount )
                {
					// Does the IOKit personality specify a maximum number of bytes?
					number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumByteCountWriteKey ) );
					require ( number, Exit );
	
					maxByteCount = number->unsigned64BitValue ( );
					require ( maxByteCount, Exit );
					
					*((UInt64 *) serviceValue) = maxByteCount;
					isSupported = true;
                
                }
            
            }

		}
		break;
                                
		default:
		{
			isSupported = false;
		}
		break;

	}
    
    
Exit:

	return isSupported;
}


//-----------------------------------------------------------------------------
//	- HandleProtocolServiceFeature									[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOUSBMassStorageClass::HandleProtocolServiceFeature( 
										SCSIProtocolFeature 	feature, 
										void * 					serviceValue )
{
	UNUSED( feature );
	UNUSED( serviceValue );

	return false;
}


#pragma mark -
#pragma mark *** Standard USB Command Methods ***
#pragma mark -


//-----------------------------------------------------------------------------
//	- ClearFeatureEndpointStall -	Method to do the CLEAR_FEATURE command for
//									an ENDPOINT_STALL feature.		
//																	[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::ClearFeatureEndpointStall( 
								IOUSBPipe *			thePipe,
								IOUSBCompletion	*	completion )
{
	IOReturn			status;
	
	if ( ( fTerminating == true ) ||
		 ( thePipe == NULL ) )
	{
 		// We're terminating, the device has probably been removed.
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
   	STATUS_LOG(( 5, "%s[%p]: ClearFeatureEndpointStall returned %x", getName(), this, status ));
				
	return status;
}


//-----------------------------------------------------------------------------
//	- GetStatusEndpointStatus -	Method to do the GET_STATUS command for the
//								endpoint that the IOUSBPipe is connected to.		
//																	[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::GetStatusEndpointStatus( 
								IOUSBPipe *			thePipe,
								void *				endpointStatus,
								IOUSBCompletion	*	completion ) 
{
	IOReturn			status;

	if ( ( fTerminating == true ) ||
		 ( thePipe == NULL ) )
	{
 		// We're terminating, the device has probably been removed.
 		// Nothing else to do except to report an error.
 		return kIOReturnDeviceError;
	}
	
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
   	STATUS_LOG(( 5, "%s[%p]: GetStatusEndpointStatus returned %x", getName(), this, status ));
	return status;
}

#pragma mark -
#pragma mark *** Accessor Methods For All Protocol Variables ***
#pragma mark -


/* The following methods are for use only by this class */


//-----------------------------------------------------------------------------
//	- GetInterfaceReference -	Method to do the GET_STATUS command for the
//								endpoint that the IOUSBPipe is connected to.		
//																	[PROTECTED]
//-----------------------------------------------------------------------------

IOUSBInterface *
IOUSBMassStorageClass::GetInterfaceReference( void )
{
	// Making this a 7 since it gets called A LOT.
   	STATUS_LOG(( 7, "%s[%p]: GetInterfaceReference", getName(), this ));
   	if ( fInterface == NULL )
   	{
   		STATUS_LOG(( 2, "%s[%p]: GetInterfaceReference - Interface is NULL.", getName(), this ));
   	}
   	
	return fInterface;
}


//-----------------------------------------------------------------------------
//	- SetInterfaceReference											[PROTECTED]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::SetInterfaceReference( IOUSBInterface * newInterface )
{
	fInterface = newInterface;
}


//-----------------------------------------------------------------------------
//	- GetInterfaceSubClass											[PROTECTED]
//-----------------------------------------------------------------------------

UInt8
IOUSBMassStorageClass::GetInterfaceSubclass( void )
{
	return fPreferredSubclass;
}


//-----------------------------------------------------------------------------
//	- GetInterfaceProtocol											[PROTECTED]
//-----------------------------------------------------------------------------

UInt8
IOUSBMassStorageClass::GetInterfaceProtocol( void )
{
	return fPreferredProtocol;
}


//-----------------------------------------------------------------------------
//	- GetControlPipe												[PROTECTED]
//-----------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetControlPipe( void )
{
	if ( fTerminating == true )
	{
		return NULL;
	}
	
	return GetInterfaceReference()->GetDevice()->GetPipeZero();
}


//-----------------------------------------------------------------------------
//	- GetBulkInPipe													[PROTECTED]
//-----------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetBulkInPipe( void )
{
	return fBulkInPipe;
}


//-----------------------------------------------------------------------------
//	- GetBulkOutPipe												[PROTECTED]
//-----------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetBulkOutPipe( void )
{
	return fBulkOutPipe;
}


//-----------------------------------------------------------------------------
//	- GetInterruptPipe												[PROTECTED]
//-----------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetInterruptPipe( void )
{
	return fInterruptPipe;
}


//-----------------------------------------------------------------------------
//	- GetMaxLogicalUnitNumber										[PROTECTED]
//-----------------------------------------------------------------------------

UInt8
IOUSBMassStorageClass::GetMaxLogicalUnitNumber( void ) const
{
	return fMaxLogicalUnitNumber;
}


//-----------------------------------------------------------------------------
//	- SetMaxLogicalUnitNumber										[PROTECTED]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::SetMaxLogicalUnitNumber( UInt8 maxLUN )
{
	fMaxLogicalUnitNumber = maxLUN;
}


#pragma mark -
#pragma mark *** Accessor Methods For CBI Protocol Variables ***
#pragma mark -


//-----------------------------------------------------------------------------
//	- GetCBIRequestBlock											[PROTECTED]
//-----------------------------------------------------------------------------

CBIRequestBlock *	
IOUSBMassStorageClass::GetCBIRequestBlock( void )
{
	// Return a pointer to the CBIRequestBlock
	return &fCBICommandRequestBlock;
}


//-----------------------------------------------------------------------------
//	- ReleaseCBIRequestBlock										[PROTECTED]
//-----------------------------------------------------------------------------

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
#pragma mark *** Accessor Methods For Bulk Only Protocol Variables ***
#pragma mark -


//-----------------------------------------------------------------------------
//	- GetBulkOnlyRequestBlock										[PROTECTED]
//-----------------------------------------------------------------------------

BulkOnlyRequestBlock *	
IOUSBMassStorageClass::GetBulkOnlyRequestBlock( void )
{
	// Return a pointer to the BulkOnlyRequestBlock
	return &fBulkOnlyCommandRequestBlock;
}


//-----------------------------------------------------------------------------
//	- ReleaseBulkOnlyRequestBlock									[PROTECTED]
//-----------------------------------------------------------------------------

void	
IOUSBMassStorageClass::ReleaseBulkOnlyRequestBlock( BulkOnlyRequestBlock * boRequestBlock )
{
	// Clear the request and completion to avoid possible double callbacks.
	boRequestBlock->request = NULL;

	// Since we only allow one command and the BulkOnlyRequestBlock is
	// a member variable, no need to do anything.
	return;
}


//-----------------------------------------------------------------------------
//	- GetNextBulkOnlyCommandTag										[PROTECTED]
//-----------------------------------------------------------------------------

UInt32	
IOUSBMassStorageClass::GetNextBulkOnlyCommandTag( void )
{
	fBulkOnlyCommandTag++;
	
	return fBulkOnlyCommandTag;
}


//-----------------------------------------------------------------------------
//	- HandlePowerOn - Will get called when a device has been resumed   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::HandlePowerOn( void )
{
	UInt8	eStatus[2];
	bool	knownResetOnResumeDevice = false;

	// The USB hub port that the device is connected to has been resumed,
	// check to see if the device is still responding correctly and if not, 
	// fix it so that it is.
	STATUS_LOG(( 6, "%s[%p]: HandlePowerOn", getName(), this ));
	
	if ( getProperty( kIOUSBMassStorageCharacteristics ) != NULL )
	{
		OSDictionary * characterDict = OSDynamicCast(
											OSDictionary, 
											getProperty( kIOUSBMassStorageCharacteristics ));
		
		// Check if this device is known to have problems when waking from sleep
		if ( characterDict->getObject( kIOUSBMassStorageResetOnResume ) != NULL )
		{
			STATUS_LOG(( 4, "%s[%p]: knownResetOnResumeDevice", getName(), this ));
			knownResetOnResumeDevice = true;
		}
	}
	
	
	if ( ( GetStatusEndpointStatus( GetBulkInPipe(), &eStatus[0], NULL) != kIOReturnSuccess ) ||
		 ( knownResetOnResumeDevice == true ) )
	{   
        ResetDeviceNow( true );
	}
	
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- handleOpen													   [PUBLIC]
//-----------------------------------------------------------------------------

bool
IOUSBMassStorageClass::handleOpen( IOService *		client,
								   IOOptionBits		options,
								   void *			arg )
{
	
	bool	result = false;
	
	// If this is a normal open on a single LUN device.
	if ( GetMaxLogicalUnitNumber() == 0 )
	{
		
		result = super::handleOpen ( client, options, arg );
		goto Exit;
		
	}
	
	// It's an open from a multi-LUN client
	require_nonzero ( fClients, ErrorExit );
	require_nonzero ( OSDynamicCast ( IOSCSILogicalUnitNub, client ), ErrorExit );
	result = fClients->setObject ( client );
	
	
Exit:
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	- handleClose													   [PUBLIC]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::handleClose( IOService *		client,
									IOOptionBits	options )
{
		
	if ( GetMaxLogicalUnitNumber() == 0 )
	{
		super::handleClose( client, options );
		return;
	}
	
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


//-----------------------------------------------------------------------------
//	- handleIsOpen													   [PUBLIC]
//-----------------------------------------------------------------------------

bool
IOUSBMassStorageClass::handleIsOpen( const IOService * client ) const
{
	
	bool	result = false;
	UInt8	lun = GetMaxLogicalUnitNumber();
	
	require_nonzero ( lun, CallSuperClass );
	require_nonzero ( fClients, CallSuperClass );
	
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
	
	
CallSuperClass:
	
	
	result = super::handleIsOpen ( client );	
	return result;
	
}


//-----------------------------------------------------------------------------
//	- sWaitForReset											[STATIC][PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::sWaitForReset( void * refcon )
{
	
	return (( IOUSBMassStorageClass * ) refcon )->GatedWaitForReset();
	
}


//-----------------------------------------------------------------------------
//	- GatedWaitForReset												[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::GatedWaitForReset( void )
{
	
	IOReturn status = kIOReturnSuccess;
	
	while ( fResetInProgress == true )
	{
		status = fCommandGate->commandSleep( &fResetInProgress, THREAD_UNINT );
	}
	
	return status;
	
}


//-----------------------------------------------------------------------------
//	- sWaitForTaskAbort										[STATIC][PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::sWaitForTaskAbort( void * refcon )
{
	
	return (( IOUSBMassStorageClass * ) refcon )->GatedWaitForTaskAbort();
	
}


//-----------------------------------------------------------------------------
//	- GatedWaitForTaskAbort											[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::GatedWaitForTaskAbort( void )
{
	
	IOReturn status = kIOReturnSuccess;
	
	while ( fAbortCurrentSCSITaskInProgress == true )
	{
		status = fCommandGate->commandSleep( &fAbortCurrentSCSITaskInProgress, THREAD_UNINT );
	}
	
	return status;
	
}


//-----------------------------------------------------------------------------
//	- sResetDevice											[STATIC][PROTECTED]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::sResetDevice( void * refcon )
{
	
	IOUSBMassStorageClass *		driver;
	IOReturn					status = kIOReturnError;
    IOUSBInterface *			interfaceRef	= NULL;
	IOUSBDevice *				deviceRef		= NULL;
    
	driver = ( IOUSBMassStorageClass * ) refcon;
	
	STATUS_LOG(( 4, "%s[%p]: sResetDevice", driver->getName(), driver ));
	
	// Check if we should bail out because we are
	// being terminated.
	if ( ( driver->fTerminating == true ) ||
		 ( driver->isInactive( ) == true ) )
	{
		STATUS_LOG(( 2, "%s[%p]: sResetDevice - We are being terminated!", driver->getName(), driver ));
		goto ErrorExit;
	}
    
    interfaceRef = driver->GetInterfaceReference();
    require ( ( interfaceRef != NULL ), ErrorExit );
	
	deviceRef = interfaceRef->GetDevice();
	require ( ( deviceRef != NULL ), ErrorExit );
	
	// Are we still connected to the hub? We only need to check this for full and low speed devices. 
	status = deviceRef->message ( kIOUSBMessageHubIsDeviceConnected, NULL, 0 );
	STATUS_LOG(( 5, "%s[%p]: kIOUSBMessageHubIsDeviceConnected returned = %x", driver->getName(), driver, status ));
	
	if ( status != kIOReturnNoDevice )
	{
		
		// Device is still attached. Lets try resetting.
		status = deviceRef->ResetDevice();
		STATUS_LOG(( 5, "%s[%p]: ResetDevice() returned = %x", driver->getName(), driver, status ));
		
	}
	
	if ( status != kIOReturnSuccess )
	{
		
		// Device reset failed, or the device has been disconnected.
		
		// Were we attempting to recover the device after a major device error?
		if ( driver->fWaitingForReconfigurationMessage == true )
		{
		
			// Yes, so we need to abort the command we'll not be able to retry. 
			driver->fWaitingForReconfigurationMessage = false;
        
		}
		
		goto ErrorExit;
		
	}
    
	// We may get terminated during the call to ResetDevice() since it is synchronous.  We should
	// check again whether we should bail or not.
	if ( ( driver->fTerminating == true ) ||
		 ( driver->isInactive( ) == true ) )
	{
    
		STATUS_LOG(( 2, "%s[%p]: sResetDevice - We are being terminated (ii) !", driver->getName(), driver ));

		goto ErrorExit;
        
	}

	// Once the device has been reset, send notification to the client so that the
	// device can be reconfigured for use.
	driver->SendNotification_VerifyDeviceState();
	
	
ErrorExit:
	
	if ( status != kIOReturnSuccess )
	{
    
        // We set the device state to detached so the proper status for the 
        // device is returned along with the aborted SCSITask.
        driver->fDeviceAttached = false;
		driver->AbortCurrentSCSITask();
        
        // Let the clients know that the device is gone.
        driver->SendNotification_DeviceRemoved( );
        
	}
    
    if ( ( driver->fTerminating == true ) && ( driver->GetInterfaceReference() != NULL ) ) 
    {

        IOUSBInterface * currentInterface;
        
        currentInterface = driver->GetInterfaceReference();
        
        // We set this to NULL first chance we get to prevent a potential race between
        // our reset thread and our didTeriminate(). 
        driver->SetInterfaceReference( NULL );
        
        // Close and nullify our USB Interface.
        currentInterface->close( driver );
        
    }
    	
	driver->fResetInProgress = false;
        
	if ( driver->fWaitingForReconfigurationMessage == false )
	{
        // Unblock our main thread.
		driver->fCommandGate->commandWakeup( &driver->fResetInProgress, false );
        
	}
	
	STATUS_LOG(( 6, "%s[%p]: sResetDevice exiting.", driver->getName(), driver ));
    	
	// We retained the driver in ResetDeviceNow() when
	// we created a thread for sResetDevice().
	driver->release();
    
	return;
	
}

//-----------------------------------------------------------------------------
//	- sAbortCurrentSCSITask									[STATIC][PROTECTED]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::sAbortCurrentSCSITask( void * refcon )
{
	
	IOUSBMassStorageClass *		driver;
	SCSITaskIdentifier			currentTask = NULL;
	
	
	driver = ( IOUSBMassStorageClass * ) refcon;
	
	STATUS_LOG(( 4, "%s[%p]: sAbortCurrentSCSITask", driver->getName(), driver ));
	
	if( driver->fBulkOnlyCommandStructInUse == true )
	{
		currentTask = driver->fBulkOnlyCommandRequestBlock.request;
	}
	else if( driver->fCBICommandStructInUse == true )
	{
		currentTask = driver->fCBICommandRequestBlock.request;
   	}
	
	if ( currentTask != NULL )
	{
	
		if ( driver->fDeviceAttached == false )
		{ 
			STATUS_LOG(( 1, "%s[%p]: sAbortCurrentSCSITask Aborting current SCSITask with device not present.", driver->getName(), driver ));
			driver->CommandCompleted( currentTask, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE, kSCSITaskStatus_DeviceNotPresent );
		}
		else
		{
			STATUS_LOG(( 1, "%s[%p]: sAbortCurrentSCSITask Aborting current SCSITask with delivery failure.", driver->getName(), driver ));
			driver->CommandCompleted( currentTask, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE, kSCSITaskStatus_DeliveryFailure );
		}
	
	}
	
	driver->fBulkOnlyCommandStructInUse = false;
	driver->fCBICommandStructInUse = false;
	driver->fAbortCurrentSCSITaskInProgress = false;
	driver->fCommandGate->commandWakeup( &driver->fAbortCurrentSCSITaskInProgress, false );
	
	return;
	
}


OSMetaClassDefineReservedUsed( IOUSBMassStorageClass, 1 );


//-----------------------------------------------------------------------------
//	- StartDeviceRecovery - The recovery sequence to restore functionality for
//							devices that stop responding (like many devices
//							after a Suspend/Resume).				[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::StartDeviceRecovery( void )
{
	// First check to see if the device is still connected.
	UInt8		eStatus[2];
	IOReturn	status = kIOReturnError;
	
	// The USB hub port that the device is connected to has been resumed,
	// check to see if the device is still responding correctly and if not, 
	// fix it so that it is. 
	STATUS_LOG(( 5, "%s[%p]: StartDeviceRecovery", getName(), this ));
	
	if( fBulkOnlyCommandStructInUse == true )
	{
		// Set up the IOUSBCompletion structure
		fBulkOnlyCommandRequestBlock.boCompletion.target 		= this;
		fBulkOnlyCommandRequestBlock.boCompletion.action 		= &this->DeviceRecoveryCompletionAction;
		status = GetStatusEndpointStatus( GetBulkInPipe(), &eStatus[0], &fBulkOnlyCommandRequestBlock.boCompletion);
	}
	else if( fCBICommandStructInUse == true )
	{
		// Set up the IOUSBCompletion structure
		fCBICommandRequestBlock.cbiCompletion.target 		= this;
		fCBICommandRequestBlock.cbiCompletion.action 		= &this->DeviceRecoveryCompletionAction;
		status = GetStatusEndpointStatus( GetBulkInPipe(), &eStatus[0], &fCBICommandRequestBlock.cbiCompletion);
   	}
	
	return status;
}

OSMetaClassDefineReservedUsed( IOUSBMassStorageClass, 2 );


//-----------------------------------------------------------------------------
//	- FinishDeviceRecovery											[PROTECTED]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::FinishDeviceRecovery( IOReturn status )
{
	
	SCSITaskIdentifier	tempTask					=	NULL;
    
	
	STATUS_LOG(( 4, "%s[%p]: + IOUSBMassStorageClass::FinishDeviceRecovery. Status = %x", getName(), this, status ));
	
	if( fBulkOnlyCommandStructInUse == true )
	{
		tempTask = fBulkOnlyCommandRequestBlock.request;
	}
	else if ( fCBICommandStructInUse == true )
	{
		tempTask = fCBICommandRequestBlock.request;
	}
	
    if ( ( fTerminating == true ) || isInactive() )
    {
        
        // We're being terminated. Abort the outstanding command so the system can clean up.
		fDeviceAttached = false;
		
        goto ErrorExit;
        
    }
        
	if ( status != kIOReturnSuccess)
	{
    
		// The endpoint status could not be retrieved meaning that the device has
		// stopped responding.  Begin the device reset sequence.
		
		STATUS_LOG(( 4, "%s[%p]: FinishDeviceRecovery reseting device on separate thread.", getName(), this ));
		
		ResetDeviceNow( false );

	}

	// If the device is responding correctly or has been reset, retry the command.
	if ( status == kIOReturnSuccess )
	{
		
		// Once the device has been reset, send notification to the client so that the
		// device can be reconfigured for use.
		SendNotification_VerifyDeviceState();
		
		if( fBulkOnlyCommandStructInUse == true )
		{
			
			STATUS_LOG(( 6, "%s[%p]: FinishDeviceRecovery SendSCSICommandforBulkOnlyProtocol sent", getName(), this ));
			status = SendSCSICommandForBulkOnlyProtocol( tempTask );
	   		STATUS_LOG(( 5, "%s[%p]: FinishDeviceRecovery SendSCSICommandforBulkOnlyProtocol returned %x", getName(), this, status));
			
			// SendSCSICommandForBulkOnlyProtocol clears out fBulkOnlyCommandRequestBlock.request if the command fails
			// to be sent correctly. We need to reset this value so we can abort the command. 
			fBulkOnlyCommandRequestBlock.request = tempTask;
			
            require ( ( status == kIOReturnSuccess ), ErrorExit );
			
		}
		else if ( fCBICommandStructInUse == true )
		{
		
			STATUS_LOG(( 6, "%s[%p]: FinishDeviceRecovery SendSCSICommandforCBIProtocol sent", getName(), this ));
			status = SendSCSICommandForCBIProtocol( tempTask );
	   		STATUS_LOG(( 5, "%s[%p]: FinishDeviceRecovery SendSCSICommandforCBIProtocol returned %x", getName(), this, status));
			
			// SendSCSICommandForCBIProtocol clears out fBulkOnlyCommandRequestBlock.request if the command fails
			// to be sent correctly. We need to reset this value so we can abort the command. 
			fCBICommandRequestBlock.request = tempTask;
			
			require ( ( status == kIOReturnSuccess ), ErrorExit );
			
	   	}
		
	}
	
	STATUS_LOG(( 4, "%s[%p]: - IOUSBMassStorageClass::FinishDeviceRecovery", getName(), this ));
	
	return;
	
	
ErrorExit:
	
	
	if ( tempTask != NULL )
	{
		AbortCurrentSCSITask();
	}
	
	STATUS_LOG(( 4, "%s[%p]: - IOUSBMassStorageClass::FinishDeviceRecovery - AbortCurrentSCSITask", getName(), this ));
	
}


//-----------------------------------------------------------------------------
//	- DeviceRecoveryCompletionAction								[PROTECTED]
//-----------------------------------------------------------------------------

void 
IOUSBMassStorageClass::DeviceRecoveryCompletionAction(
					                void *			target,
					                void *			parameter,
					                IOReturn		status,
					                UInt32			bufferSizeRemaining)
{
	UNUSED( parameter );
	UNUSED( bufferSizeRemaining );
	
	IOUSBMassStorageClass *		theMSC;
	
	theMSC = (IOUSBMassStorageClass *) target;
	theMSC->FinishDeviceRecovery( status );
}


//-----------------------------------------------------------------------------
//	- ResetDeviceNow                                                [PROTECTED]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::ResetDeviceNow( bool waitForReset )
{
    
    // Make sure we aren't terminating. 
	if ( ( fTerminating == false ) && ( isInactive() == false ) )
	{
	
		// We call retain here so that the driver will stick around long enough for
		// sResetDevice() to do it's thing in case we are being terminated.  The
		// retain() is balanced with a release in sResetDevice().
		retain();
		
		// The endpoint status could not be retrieved meaning that the device has
		// stopped responding. Or this could be a device we know needs a reset.
		// Begin the device reset sequence.
		
		STATUS_LOG(( 4, "%s[%p]: ResetDeviceNow", getName(), this ));
		
		// Reset the device on its own thread so we don't deadlock.
		fResetInProgress = true;
        fWaitingForReconfigurationMessage = false;
        
        // If we aren't going to block on the reset thread, we need to wait for the 
        // reconfiguration message. 
        if ( waitForReset == false ) 
        {
            fWaitingForReconfigurationMessage = true;
        }
        
		IOCreateThread( IOUSBMassStorageClass::sResetDevice, this );
		
		if ( waitForReset == true )
		{
		
			STATUS_LOG(( 4, "%s[%p]: kIOMessageServiceIsResumed Waiting for Reset.", getName(), this ));
			fCommandGate->runAction ( ( IOCommandGate::Action ) &IOUSBMassStorageClass::sWaitForReset );
	
		}
		
	}
    
}

//-----------------------------------------------------------------------------
//	- AbortCurrentSCSITask                                          [PROTECTED]
//-----------------------------------------------------------------------------

void
IOUSBMassStorageClass::AbortCurrentSCSITask( void )
{

	// We call retain here so that the driver will stick around long enough for
	// sAbortCurrentSCSITask() to do it's thing in case we are being terminated.  
	retain();
	
	// The endpoint status could not be retrieved meaning that the device has
	// stopped responding. Or this could be a device we know needs a reset.
	// Begin the device reset sequence.
	
	STATUS_LOG(( 4, "%s[%p]: AbortCurrentSCSITask called!", getName(), this ));
	
	// Abort the SCSITask on a separate thread so we don't deadlock.
	fAbortCurrentSCSITaskInProgress = true;
	
	IOCreateThread( IOUSBMassStorageClass::sAbortCurrentSCSITask, this );
	fCommandGate->runAction ( ( IOCommandGate::Action ) &IOUSBMassStorageClass::sWaitForTaskAbort );
	
	
Exit:

    // We retained ourselves earlier in this method, time to balance that out.
	release();
	
	STATUS_LOG(( 4, "%s[%p]: AbortCurrentSCSITask Exiting", getName(), this ));
	
}


//-----------------------------------------------------------------------------
//	- IsPhysicalInterconnectLocationInternal                        [PROTECTED]
//-----------------------------------------------------------------------------

bool				
IOUSBMassStorageClass::IsPhysicalInterconnectLocationInternal ( void )
{

	IOReturn				status				= kIOReturnError;
	IOUSBInterface *		usbInterface		= NULL;
	IOUSBDevice *			usbDevice			= NULL;
	UInt32					deviceInformation	= 0;
	bool                    internal            = false;
    
	
	// We acquire our references individually to avoid panics. 
	
	// Get a reference to our USB interface.
	usbInterface = GetInterfaceReference();
	require ( ( usbInterface != NULL ), ErrorExit );
	
	// Get a reference to our USB device. 
	usbDevice = usbInterface->GetDevice();
	require ( ( usbDevice != NULL ), ErrorExit );
	
	status = usbDevice->GetDeviceInformation ( &deviceInformation );
	require_success ( status, ErrorExit );
	
	if ( deviceInformation & ( 1 << kUSBDeviceInfoIsInternalMask ) )
	{
		internal = true;
	}	
	
	
ErrorExit:

	return status;
	
}

// Space reserved for future expansion.
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
