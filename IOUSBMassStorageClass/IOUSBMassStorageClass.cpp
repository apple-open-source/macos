/*
 * Copyright (c) 1998-2009 Apple Inc. All rights reserved.
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


//--------------------------------------------------------------------------------------------------
//	Includes
//--------------------------------------------------------------------------------------------------

// General OS Services header files
#include <libkern/OSByteOrder.h>

// Local includes
#include "IOUSBMassStorageClass.h"
#include "IOUSBMassStorageClassTimestamps.h"
#include "Debugging.h"

// IOKit includes
#include <IOKit/scsi/IOSCSIPeripheralDeviceNub.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>

//--------------------------------------------------------------------------------------------------
//	Defines
//--------------------------------------------------------------------------------------------------

// Default timeout values
enum
{
	kDefaultReadTimeoutDuration				=	30000,
	kDefaultWriteTimeoutDuration			=	30000
};

//	Default maximum transfer count, in bytes
enum
{
	kDefaultMaximumByteCountRead			=	131072,
	kDefaultMaximumByteCountWrite			=	131072
};

//	Maximum number of consecutive I/Os which can be aborted with a Device Reset,
//	before the device will be considered removed.
enum
{
	kMaxConsecutiveResets					=	5
};


//--------------------------------------------------------------------------------------------------
//	Macros
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//	Declarations - USBMassStorageClassGlobals
//--------------------------------------------------------------------------------------------------

class USBMassStorageClassGlobals
{

	public:

		// Constructor
		USBMassStorageClassGlobals ( void );
	
		// Desctructor
		virtual ~USBMassStorageClassGlobals ( void );
	
};
	
	
//--------------------------------------------------------------------------------------------------
//	Globals - USBMassStorageClassGlobals
//--------------------------------------------------------------------------------------------------


UInt32								gUSBDebugFlags = 0; // Externally defined in IOUSBMassStorageClass.h
static USBMassStorageClassGlobals 	gUSBGlobals;

static int USBMassStorageClassSysctl ( struct sysctl_oid * oidp, void * arg1, int arg2, struct sysctl_req * req );
SYSCTL_PROC ( _debug, OID_AUTO, USBMassStorageClass, CTLFLAG_RW, 0, 0, USBMassStorageClassSysctl, "USBMassStorageClass", "USBMassStorageClass debug interface" );


//--------------------------------------------------------------------------------------------------
//	USBMassStorageClassSysctl - Sysctl handler.						   						[STATIC]
//--------------------------------------------------------------------------------------------------

static int
USBMassStorageClassSysctl ( struct sysctl_oid * oidp, void * arg1, int arg2, struct sysctl_req * req )
{
	
	int				error = 0;
	USBSysctlArgs	usbArgs;
	
	UNUSED ( oidp );
	UNUSED ( arg1 );
	UNUSED ( arg2 );
	
	STATUS_LOG ( ( 1, "+USBMassStorageClassGlobals: gUSBDebugFlags = 0x%08X\n", ( unsigned int ) gUSBDebugFlags ) );
	
	error = SYSCTL_IN ( req, &usbArgs, sizeof ( usbArgs ) );
	if ( ( error == 0 ) && ( usbArgs.type == kUSBTypeDebug ) )
	{
		
		if ( usbArgs.operation == kUSBOperationGetFlags )
		{
			
			usbArgs.debugFlags = gUSBDebugFlags;
			error = SYSCTL_OUT ( req, &usbArgs, sizeof ( usbArgs ) );
			
		}
		
		else if ( usbArgs.operation == kUSBOperationSetFlags )
		{
			gUSBDebugFlags = usbArgs.debugFlags;			
		}
		
	}
	
	STATUS_LOG ( ( 1, "-USBMassStorageClassGlobals: gUSBDebugFlags = 0x%08X\n", ( unsigned int ) gUSBDebugFlags ) );
	
	return error;
	
}


//--------------------------------------------------------------------------------------------------
//	USBMassStorageClassGlobals - Default Constructor				   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

USBMassStorageClassGlobals::USBMassStorageClassGlobals ( void )
{
	
	int		debugFlags;
	
	STATUS_LOG ( ( 1, "+USBMassStorageClassGlobals::USBMassStorageClassGlobals\n" ) );
	
	if ( PE_parse_boot_argn ( "USB-MassStorage", &debugFlags, sizeof ( debugFlags ) ) )
	{
		gUSBDebugFlags = debugFlags;
	}
	
	// Register our sysctl interface
	sysctl_register_oid ( &sysctl__debug_USBMassStorageClass );
	
	STATUS_LOG ( ( 1, "-USBMassStorageClassGlobals::USBMassStorageClassGlobals\n" ) );
	
}


//--------------------------------------------------------------------------------------------------
//	USBMassStorageClassGlobals - Destructor							   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

USBMassStorageClassGlobals::~USBMassStorageClassGlobals ( void )
{
	
	STATUS_LOG ( ( 1, "+~USBMassStorageClassGlobals::USBMassStorageClassGlobals\n" ) );
	
	// Unregister our sysctl interface
	sysctl_unregister_oid ( &sysctl__debug_USBMassStorageClass );
	
	STATUS_LOG ( ( 1, "-~USBMassStorageClassGlobals::USBMassStorageClassGlobals\n" ) );
	
}


//--------------------------------------------------------------------------------------------------
//	Declarations - IOUSBMassStorageClass
//--------------------------------------------------------------------------------------------------

#define super IOSCSIProtocolServices

OSDefineMetaClassAndStructors( IOUSBMassStorageClass, IOSCSIProtocolServices )
	

//--------------------------------------------------------------------------------------------------
//	init - Called at initialization time							   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageClass::init ( OSDictionary * propTable )
{
    if ( super::init( propTable ) == false)
    {
		return false;
    }

    return true;
	
}

//--------------------------------------------------------------------------------------------------
//	start - Called at services start time	(after successful matching)						[PUBLIC]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageClass::start ( IOService * provider )
{
    IOUSBFindEndpointRequest 	request;
	OSDictionary * 				characterDict 	= NULL;
	OSObject *					obj				= NULL;
    IOReturn                    result          = kIOReturnError;
	bool						retVal			= false;
	OSNumber *					number			= NULL;
	bool						success			= false;
	
	
    if ( super::start( provider ) == false )
    {
    	STATUS_LOG(( 1, "%s[%p]: superclass start failure.", getName(), this));
        return false;
    }
	
	RecordUSBTimeStamp (	UMC_TRACE ( kIOUSBMassStorageClassStart ),
							( uintptr_t ) this, NULL,
							NULL, NULL );
	
	// Allocate data for our expansion data.
	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	bzero ( reserved, sizeof ( ExpansionData ) );
	
    // Save the reference to the interface on the device that will be
    // the provider for this object.
    SetInterfaceReference( OSDynamicCast( IOUSBInterface, provider) );
    if ( GetInterfaceReference() == NULL )
    {
	
    	STATUS_LOG ( ( 1, "%s[%p]: the provider is not an IOUSBInterface object",
    				getName(), this ) );
    	// If our provider is not a IOUSBInterface object, return false
    	// to indicate that the object could not be correctly 
		// instantiated.
    	// The USB Mass Storage Class specification requires that all 
		// devices be a composite device with a Mass Storage interface
		// so this object will always be an interface driver.
 		goto Exit;
		
    }
	
	// Check if a subclass has marked this device as not to be operated at all.
	if ( provider->getProperty( kIOUSBMassStorageDoNotMatch ) != NULL )
	{
		goto abortStart;
	}
	
	RecordUSBTimeStamp (	UMC_TRACE ( kAtUSBAddress ),
							( uintptr_t ) this, ( unsigned int ) GetInterfaceReference()->GetDevice()->GetAddress(),
							NULL, NULL );

    STATUS_LOG ( ( 6, "%s[%p]: USB Mass Storage @ %d", 
				getName(), this,
                GetInterfaceReference()->GetDevice()->GetAddress() ) );

    if ( GetInterfaceReference()->open( this ) == false) 
    {
	
    	STATUS_LOG ( ( 1, "%s[%p]: could not open the interface", getName(), this ) );
		goto Exit;
		
    }

	// Set the IOUSBPipe object pointers to NULL so that the driver can 
	// release these objects if instantition is not successful.
    fBulkInPipe 	= NULL;
    fBulkOutPipe	= NULL;
    fInterruptPipe	= NULL;
	
	// Default is to have no clients
	fClients		= NULL;
	
	// Default is to have a max lun of 0.
	SetMaxLogicalUnitNumber ( 0 );
	
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
	
    // Used to determine if we're going to block on the reset thread or not.
	fWaitingForReconfigurationMessage = false;
    
    // Used to determine where we should close our provider at time of termination.
    fTerminating = false;
    
    // IOSAM may request that we suspend/resume our port instead of spin up/down media.
    fPortSuspendResumeForPMEnabled = false;
    
	// Workaround flag for devices which spin themselves up/down and have problems with driver intervention. 
	fAutonomousSpinDownWorkAround = false;
	
	// Check if the personality for this device specifies a preferred protocol
    characterDict = OSDynamicCast ( OSDictionary, getProperty( kIOUSBMassStorageCharacteristics ) );
	if ( characterDict == NULL )
	{
		// This device does not specify a preferred protocol, use the protocol
		// defined in the descriptor.
		fPreferredProtocol = GetInterfaceReference()->GetInterfaceProtocol();
		fPreferredSubclass = GetInterfaceReference()->GetInterfaceSubClass();
		
	}
	else
	{
		
        OSNumber *	preferredProtocol;
        OSNumber *	preferredSubclass;
            
            
		RecordUSBTimeStamp ( UMC_TRACE ( kIOUMCStorageCharacDictFound ),
							 ( uintptr_t ) this, NULL, NULL, NULL );
		
		// Check if we have a USB storage personality for this particular device.
        preferredProtocol = OSDynamicCast ( OSNumber, characterDict->getObject( kIOUSBMassStoragePreferredProtocol ) );
		if ( preferredProtocol == NULL )
		{
			// This device does not specify a preferred protocol, use the
			// protocol defined in the interface descriptor.
			fPreferredProtocol = GetInterfaceReference()->GetInterfaceProtocol();
					
		}
		else
		{
            // This device has a preferred protocol, use that.
            fPreferredProtocol = preferredProtocol->unsigned32BitValue();
			
		}
		
		// Check if this device is not to be operated at all.
        if ( characterDict->getObject( kIOUSBMassStorageDoNotOperate ) != NULL )
        {
            goto abortStart;
        }
        
        // Check if this device is known not to support the bulk-only USB reset.
        if ( characterDict->getObject ( kIOUSBMassStorageUseStandardUSBReset ) != NULL )
        {
            fUseUSBResetNotBOReset = true;
        }
        
        // Is this a device which has CBW/CSW tag issues?
        if ( characterDict->getObject ( kIOUSBKnownCSWTagIssues ) != NULL )
        {
            fKnownCSWTagMismatchIssues = true;
        }

        if ( characterDict->getObject( kIOUSBMassStorageEnableSuspendResumePM ) != NULL )
        {
            fPortSuspendResumeForPMEnabled = true;
        }
       		
       	// Check if this device is known to have problems when waking from sleep
		if ( characterDict->getObject( kIOUSBMassStorageResetOnResume ) != NULL )
		{
		
			STATUS_LOG ( ( 4, "%s[%p]: knownResetOnResumeDevice", getName(), this ) );
			fRequiresResetOnResume = true;
			
		}
                    
		// Check if the personality for this device specifies a preferred subclass
        preferredSubclass = OSDynamicCast ( OSNumber, characterDict->getObject( kIOUSBMassStoragePreferredSubclass ));
		if ( preferredSubclass == NULL )
		{
			// This device does not specify a preferred subclass, use the 
			// subclass defined in the interface descriptor.
			fPreferredSubclass = GetInterfaceReference()->GetInterfaceSubClass();
					
		}
		else
		{
			// This device has a preferred protocol, use that.
			fPreferredSubclass = preferredSubclass->unsigned32BitValue();
			
		}
 
	}
		
	STATUS_LOG ( ( 6, "%s[%p]: Preferred Protocol is: %d", getName(), this, fPreferredProtocol ) );
    STATUS_LOG ( ( 6, "%s[%p]: Preferred Subclass is: %d", getName(), this, fPreferredSubclass ) );

	// Verify that the device has a supported interface type and configure that
	// Interrupt pipe if the protocol requires one.
    STATUS_LOG ( ( 7, "%s[%p]: Configure the Storage interface", getName(), this ) );
    switch ( GetInterfaceProtocol() )
    {
	
    	case kProtocolControlBulkInterrupt:
    	{
		
			RecordUSBTimeStamp ( UMC_TRACE ( kCBIProtocolDeviceDetected ),
								 ( uintptr_t ) this, NULL,
								 NULL, NULL );
        
            // Find the interrupt pipe for the device
	        request.type = kUSBInterrupt;
	        request.direction = kUSBIn;
 			fInterruptPipe = GetInterfaceReference()->FindNextPipe ( NULL, &request );

	        STATUS_LOG ( ( 7, "%s[%p]: find interrupt pipe", getName(), this ) );
	        if ( ( GetInterfaceProtocol() == kProtocolControlBulkInterrupt ) 
	        		&& ( fInterruptPipe == 0 ) )
	        {
	            // This is a CBI device and must have an interrupt pipe, 
	            // halt configuration since one could not be found
	            STATUS_LOG ( ( 1, "%s[%p]: No interrupt pipe for CBI, abort", getName(), this ) );
	            goto abortStart;
				
	        }
			
			fCBIMemoryDescriptor = IOMemoryDescriptor::withAddress (
											&fCBICommandRequestBlock.cbiGetStatusBuffer, 
											kUSBStorageAutoStatusSize, 
											kIODirectionIn );
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
        
	        STATUS_LOG ( ( 7, "%s[%p]: Bulk Only - skip interrupt pipe", getName(), this ) );
			
			RecordUSBTimeStamp ( UMC_TRACE ( kBODeviceDetected ),
								 ( uintptr_t ) this, NULL, NULL, NULL );
								 
	        // Since this is a Bulk Only device, do not look for
	        // interrupt and set the pipe object to NULL so that the
	        // driver can not try to use it.
	        fInterruptPipe 	= NULL;
            
            // Allocate the memory descriptor needed to send the CBW out.
            fBulkOnlyCBWMemoryDescriptor = IOMemoryDescriptor::withAddress ( 
                                                &fBulkOnlyCommandRequestBlock.boCBW, 
                                                kByteCountOfCBW, 
                                                kIODirectionOut );
            require_nonzero ( fBulkOnlyCBWMemoryDescriptor, abortStart );
            
            result = fBulkOnlyCBWMemoryDescriptor->prepare();
            require_success ( result, abortStart );
            
            // Allocate the memory descriptor needed to retrieve the CSW.
            fBulkOnlyCSWMemoryDescriptor = IOMemoryDescriptor::withAddress ( 
                                                &fBulkOnlyCommandRequestBlock.boCSW, 
                                                kByteCountOfCSW, 
                                                kIODirectionIn );
            require_nonzero ( fBulkOnlyCSWMemoryDescriptor, abortStart );

            result = fBulkOnlyCSWMemoryDescriptor->prepare();
            require_success ( result, abortStart );
            
	    }
	    break;
	    
	    default:
	    {
			RecordUSBTimeStamp ( UMC_TRACE ( kNoProtocolForDevice ),
								 ( uintptr_t ) this, NULL, NULL, NULL );
								 
	    	// The device has a protocol that the driver does not
	    	// support. Return false to indicate that instantiation was
	    	// not successful.
    		goto abortStart;
	    }
	    break;
    }

	// Find the Bulk In pipe for the device
    STATUS_LOG ( ( 7, "%s[%p]: find bulk in pipe", getName(), this ) );
	request.type = kUSBBulk;
	request.direction = kUSBIn;
	fBulkInPipe = GetInterfaceReference()->FindNextPipe ( NULL, &request );
	if ( fBulkInPipe == NULL )
	{
		// We could not find the bulk in pipe, not much a bulk transfer
		// device can do without this, so fail the configuration.
    	STATUS_LOG ( ( 1, "%s[%p]: No bulk in pipe found, aborting",  getName(), this ) );
    	goto abortStart;
		
	}
	
	// Find the Bulk Out pipe for the device
    STATUS_LOG ( ( 7, "%s[%p]: find bulk out pipe", getName(), this ) );
	request.type = kUSBBulk;
	request.direction = kUSBOut;
	fBulkOutPipe = GetInterfaceReference()->FindNextPipe ( NULL, &request );
	if ( fBulkOutPipe == NULL )
	{
		// We could not find the bulk out pipe, not much a bulk transfer
		// device can do without this, so fail the configuration.
    	STATUS_LOG ( ( 1, "%s[%p]: No bulk out pipe found, aborting", getName(), this ) );
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
        

        internalString = OSString::withCString ( kIOPropertyInternalKey );
        if ( internalString != NULL )
        {
            
            characterDict->setObject ( kIOPropertyPhysicalInterconnectLocationKey, internalString );
            internalString->release ( );
            internalString = NULL;
            
        }

    }
	
	obj = getProperty ( kIOPropertyReadTimeOutDurationKey );	
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyReadTimeOutDurationKey, obj );
	} 
	else
	{
		
		number = OSNumber::withNumber ( kDefaultReadTimeoutDuration, 32 );
		if ( number != NULL )
		{
		
			characterDict->setObject ( kIOPropertyReadTimeOutDurationKey, number );
			number->release ( );
			number = NULL;
			
		}
		
	}
	
	obj = getProperty ( kIOPropertyWriteTimeOutDurationKey );	
	if ( obj != NULL )
	{
		characterDict->setObject ( kIOPropertyWriteTimeOutDurationKey, obj );
	}
	else
	{
	
		number = OSNumber::withNumber ( kDefaultWriteTimeoutDuration, 32 );
		if ( number != NULL )
		{
		
			characterDict->setObject ( kIOPropertyWriteTimeOutDurationKey, number );
			number->release ( );
			number = NULL;
			
		}
	
	}
		
	setProperty ( kIOPropertyProtocolCharacteristicsKey, characterDict );
	
	characterDict->release ( );
	characterDict = NULL;
    
   	STATUS_LOG ( ( 6, "%s[%p]: successfully configured", getName(), this ) );

#if defined (__i386__) 
	{
		// As USB booting is only supporting on i386 based, do not compile for PPC. 
		char				usbDeviceAddress [ kUSBDAddressLength ];
		OSNumber *			usbDeviceID;
		
		snprintf ( usbDeviceAddress, kUSBDAddressLength, "%x", ( int ) GetInterfaceReference()->GetDevice()->GetAddress() );
		
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

	InitializePowerManagement ( GetInterfaceReference() );
	
	success = BeginProvidedServices();
	require ( success, abortStart );
   
    retVal = true;
	goto Exit;

abortStart:

    STATUS_LOG ( ( 1, "%s[%p]: aborting startup.  Stop the provider.", getName(), this ) );
	
	if ( IsPowerManagementIntialized() )
	{
	
		PMstop();
		
	}

	// Close and nullify our USB Interface.
	{
        IOUSBInterface * currentInterface;
        
        currentInterface = GetInterfaceReference();
    
		if ( currentInterface != NULL ) 
		{

			SetInterfaceReference( NULL );
			currentInterface->close( this );

		}
	
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
	
	// Call the stop method to clean up any allocated resources.
    stop ( provider );

Exit:
    
    return retVal;
	
}

//--------------------------------------------------------------------------------------------------
//	stop - Called at stop time										  						[PUBLIC]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageClass::stop ( IOService * provider )
{
	// I am logging this as a 1 because if anything is logging after this we want to know about it.
	// This should be the last message we see.
    STATUS_LOG ( ( 1, "%s[%p]: Bye bye!", getName(), this ) );
	
	RecordUSBTimeStamp (	UMC_TRACE ( kIOUSBMassStorageClassStop ), 
							( uintptr_t ) this, NULL, NULL, NULL );
	
	EndProvidedServices();

	// Tell the interface object to close all pipes since the driver is 
	// going away.
	
   	fBulkInPipe 	= NULL;
   	fBulkOutPipe 	= NULL;
   	fInterruptPipe 	= NULL;

    super::stop ( provider );
}


//--------------------------------------------------------------------------------------------------
//	free - Called by IOKit to free any resources.					   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

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


//--------------------------------------------------------------------------------------------------
//	message -	Called by IOKit to deliver messages.				   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::message ( UInt32 type, IOService * provider, void * argument )
{
	IOReturn	result = kIOReturnSuccess;
	
	RecordUSBTimeStamp ( UMC_TRACE ( kMessagedCalled ), ( uintptr_t ) this, type, NULL, NULL );
							
	STATUS_LOG ( (4, "%s[%p]: message = %lx called", getName(), this, type ) );
	switch( type )
	{
		
		case kIOUSBMessagePortHasBeenResumed:
		{
		
			STATUS_LOG((2, "%s[%p]: message  kIOUSBMessagePortHasBeenResumed.", getName(), this));
		
			if ( fRequiresResetOnResume == true )
			{   
				ResetDeviceNow ( true );
			}
			
		}
		break;
					
		default:
		{
            STATUS_LOG ( ( 2, "%s[%p]: message default case.", getName(), this ) );
			result = super::message ( type, provider, argument );
		}
        
	}
	
	return result;
	
}


//--------------------------------------------------------------------------------------------------
//	willTerminate													   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

bool        
IOUSBMassStorageClass::willTerminate (  IOService *     provider, 
                                        IOOptionBits    options )
{

    IOUSBInterface * currentInterface;
			
    STATUS_LOG ( ( 2, "%s[%p]: willTerminate called.", getName(), this ) );
	
	RecordUSBTimeStamp (	UMC_TRACE ( kWillTerminateCalled ),
							( uintptr_t ) this, ( uintptr_t ) GetInterfaceReference(),
							( unsigned int ) isInactive(), NULL );
    
    currentInterface = GetInterfaceReference();
    if ( currentInterface != NULL )
    {				
    
        STATUS_LOG ( ( 2, "%s[%p]: willTerminate interface is non NULL.", getName(), this ) );
        
        // Mark ourselves as terminating so we don't accept any additional I/O.
        fTerminating = true;
        
        // Let clients know that the device is gone.
        SendNotification_DeviceRemoved( );

    }
    
    return super::willTerminate ( provider, options );
	
}


//--------------------------------------------------------------------------------------------------
//	didTerminate													   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageClass::didTerminate ( IOService * provider, IOOptionBits options, bool * defer )
{

    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to
    // hold on to the device and IOKit will terminate us when we close it later
    //
    STATUS_LOG ( ( 3 , "%s[%p]::didTerminate isInactive = %d", getName(), this, isInactive() ) );
    
	RecordUSBTimeStamp (	UMC_TRACE ( kDidTerminateCalled ),
							( uintptr_t ) this, ( uintptr_t ) GetInterfaceReference(), 
							( unsigned int ) isInactive(), ( unsigned int ) fResetInProgress );
							
    // This should already be set to true, but can't hurt ...
    fTerminating = true;
    
    if ( ( GetInterfaceReference() != NULL ) && ( fResetInProgress == false ) ) 
    {

        IOUSBInterface * currentInterface;
        
        currentInterface = GetInterfaceReference();
        
        // We set this to NULL first chance we get to prevent a potential race between
        // our reset thread and our didTerminate(). 
        SetInterfaceReference ( NULL );
        
        // Close and nullify our USB Interface.
        currentInterface->close ( this );
        
    }

    return super::didTerminate ( provider, options, defer );
	
}


//--------------------------------------------------------------------------------------------------
//	BeginProvidedServices																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool	
IOUSBMassStorageClass::BeginProvidedServices ( void )
{
    
 	// If this is a BO device that supports multiple LUNs, we will need 
	// to spawn off a nub for each valid LUN.  If this is a CBI/CB
	// device or a BO device that only supports LUN 0, this object can
	// register itself as the nub.  
    STATUS_LOG ( ( 7, "%s[%p]: Determine the maximum LUN", getName(), this ) );
	
  	if ( GetInterfaceProtocol() == kProtocolBulkOnly )
    {
    	IOReturn        status              = kIOReturnError;
    	bool            maxLUNDetermined    = false;
        OSDictionary *  characterDict       = NULL; 
        
        
        // Before we issue the GetMaxLUN call let's check if this device
		// specifies a MaxLogicalUnitNumber as part of its personality.
        characterDict = OSDynamicCast (	OSDictionary, getProperty ( kIOUSBMassStorageCharacteristics ) );
        if ( characterDict != NULL )
        {
        
            OSNumber *	maxLUN = OSDynamicCast (    OSNumber,
                                                    characterDict->getObject ( kIOUSBMassStorageMaxLogicalUnitNumber ) );
                                                
            if( maxLUN != NULL )
            {
            
                RecordUSBTimeStamp (	UMC_TRACE ( kBOPreferredMaxLUN ),
                                        ( uintptr_t ) this, maxLUN->unsigned8BitValue(), NULL, NULL );	
                
                STATUS_LOG ( ( 4, "%s[%p]: Number of LUNs %u.", getName(), this, maxLUN->unsigned8BitValue() ) );

                SetMaxLogicalUnitNumber ( maxLUN->unsigned8BitValue() );
                maxLUNDetermined = true;
                
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
				fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType( kUSBIn, kUSBClass, kUSBInterface );
				fUSBDeviceRequest.bRequest 			= 0xFE;
				fUSBDeviceRequest.wValue			= 0;
				fUSBDeviceRequest.wIndex			= GetInterfaceReference()->GetInterfaceNumber();
				fUSBDeviceRequest.wLength			= 1;
				fUSBDeviceRequest.pData				= &fMaxLogicalUnitNumber;
				
				STATUS_LOG ( ( 4, "%s[%p]: Issuing device request to find max LUN", getName(), this ) );
				
				// Send the command over the control endpoint
				status = GetInterfaceReference()->DeviceRequest ( &fUSBDeviceRequest );
				
				RecordUSBTimeStamp (	UMC_TRACE ( kBOGetMaxLUNReturned ),
										( uintptr_t ) this, status, fMaxLogicalUnitNumber, ( unsigned int ) triedReset );	
											
				STATUS_LOG ( ( 4, "%s[%p]: DeviceRequest GetMaxLUN returned status = %x", getName(), this, status ) );
				
				if ( status != kIOReturnSuccess )
				{

					SetMaxLogicalUnitNumber( 0 );
					if( ( status == kIOUSBPipeStalled ) && ( clearPipeAttempts < 3 ) )
					{
					
						UInt8		eStatus[2];
						
						STATUS_LOG ( ( 4, "%s[%p]: calling GetStatusEndpointStatus to clear stall", getName(), this ) );
						
						// Throw in an extra Get Status to clear up devices that stall the
						// control pipe like the early Iomega devices.
						GetStatusEndpointStatus( GetControlPipe(), &eStatus[0], NULL);
						
						clearPipeAttempts++;
						
					}
					else if ( ( status == kIOReturnNotResponding ) && ( triedReset == false ) )
					{
					
						// The device isn't responding. Let us reset the device, and try again.
						
						STATUS_LOG ( ( 4, "%s[%p]: BeginProvidedServices: device not responding, reseting.", getName(), this ) );
						
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
    
	RecordUSBTimeStamp (	UMC_TRACE ( kLUNConfigurationComplete ),
							( uintptr_t ) this, GetMaxLogicalUnitNumber ( ), NULL, NULL );	

    STATUS_LOG ( ( 5, "%s[%p]: Configured, Max LUN = %d", getName(), this, GetMaxLogicalUnitNumber() ) );

 	// If this is a BO device that supports multiple LUNs, we will need 
	// to spawn off a nub for each valid LUN.  If this is a CBI/CB
	// device or a BO device that only supports LUN 0, this object can
	// register itself as the nub.  
 	if ( GetMaxLogicalUnitNumber() > 0 )
    {
		// Allocate space for our set that will keep track of the LUNs.
		fClients = OSSet::withCapacity ( GetMaxLogicalUnitNumber() + 1 );
	
        for( int loopLUN = 0; loopLUN <= GetMaxLogicalUnitNumber(); loopLUN++ )
        {
		    STATUS_LOG ( ( 6, "%s[%p]::CreatePeripheralDeviceNubForLUN entering.", getName(), this ) );

			IOSCSILogicalUnitNub * 	nub = OSTypeAlloc( IOSCSILogicalUnitNub );
			
			if( nub == NULL )
			{
				PANIC_NOW ( ( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN failed" ) );
				return false;
			}
			
			if ( nub->init( 0 ) == false )
            {
                // Release our nub before we return so we don't leak...
                nub->release();
                // We didn't init successfully so we should return false.
                return false;
            }
            
            if ( nub->attach( this ) == false )
            {
                if( isInactive() == false )
                {
                    // panic since the nub can't attach and we are active
                    PANIC_NOW ( ( "IOUSBMassStorageClass::CreatePeripheralDeviceNubForLUN unable to attach nub" ) );
                }
                
                // Release our nub before we return so we don't leak...
                nub->release();
                // We didn't attach so we should return false.
                return false;
            }
                        
            nub->SetLogicalUnitNumber ( loopLUN );
            if ( nub->start ( this ) == false )
            {
                nub->detach ( this );
            }
            else
            {
                nub->registerService ( kIOServiceAsynchronous );
            }
            
            nub->release();
			nub = NULL;
            
			STATUS_LOG ( ( 6, "%s[%p]::CreatePeripheralDeviceNubForLUN exiting.", getName(), this ) );
		}
    }

	// Calling registerService() will start driver matching and result in our handleOpen() method
	// being called by an IOSCSIPeripheralDeviceNub object. In the multi-LUN case in which our
	// nubs have already been instantiated (above), that open will fail because our MaxLogicalUnitNumber
	// is nonzero. In the single LUN case, the open will succeed and the rest of the storage stack
	// will be built upon it.
	registerService ( kIOServiceAsynchronous );

	return true;
    
}


//--------------------------------------------------------------------------------------------------
//	EndProvidedServices																	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool	
IOUSBMassStorageClass::EndProvidedServices
( void )
{
	return true;
}


#pragma mark -
#pragma mark *** CDB Transport Methods ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	SendSCSICommand																		 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageClass::SendSCSICommand ( 	
									SCSITaskIdentifier 			request, 
									SCSIServiceResponse *		serviceResponse,
									SCSITaskStatus *			taskStatus )
{

	IOReturn					status;
	SCSICommandDescriptorBlock	cdbData;
	bool						accepted = false;

	// Set the defaults to an error state.		
	*taskStatus = kSCSITaskStatus_No_Status;
	*serviceResponse =  kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
   	STATUS_LOG ( ( 6, "%s[%p]: SendSCSICommand Entered with request=%p", getName(), this, request ) );

	// If we have been marked as inactive, or no longer have the device, return false.
	if ( isInactive () || ( fDeviceAttached == false ) || ( fTerminating == true ) )
	{	
	
        RecordUSBTimeStamp (	UMC_TRACE ( kNewCommandWhileTerminating ),
								( uintptr_t ) this, ( uintptr_t ) request, NULL, NULL );
        
		goto Exit;
		
	}
	
    if ( fPortIsSuspended == true )
    {
        // Our port is suspended, but the client layer would like to send a SCSI
        // command. Time to resume. 
        status = SuspendPort ( false );
        if ( status != kIOReturnSuccess )
        {
			goto Exit;
        }
        
    }
    
	GetCommandDescriptorBlock( request, &cdbData );
	
	RecordUSBTimeStamp (	UMC_TRACE ( kCDBLog1 ),
							( uintptr_t ) this, ( uintptr_t ) request,
							( cdbData[ 0] ) | ( cdbData[ 1] << 8 ) | ( cdbData[ 2] << 16 ) | ( cdbData[ 3] << 24 ),
							( cdbData[ 4] ) | ( cdbData[ 5] << 8 ) | ( cdbData[ 6] << 16 ) | ( cdbData[ 7] << 24 ) );
							
	RecordUSBTimeStamp (	UMC_TRACE ( kCDBLog2 ),
							( uintptr_t ) this, ( uintptr_t ) request,
							( cdbData[ 8] ) | ( cdbData[ 9] << 8 ) | ( cdbData[10] << 16 ) | ( cdbData[11] << 24 ),
							( cdbData[12] ) | ( cdbData[13] << 8 ) | ( cdbData[14] << 16 ) | ( cdbData[15] << 24 ) );
    
#if USB_MASS_STORAGE_DEBUG
	
	STATUS_LOG ( ( 4, "%s[%p]: SendSCSICommand CDB data: ", getName(), this ) );
	
	if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_6Byte )
		STATUS_LOG ( ( 4, "%s[%p]: %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5] ) );
	else if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_10Byte )
		STATUS_LOG ( ( 4, "%s[%p]: %X : %X : %X : %X : %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5], 
                    cdbData[6], cdbData[7], cdbData[8], cdbData[9] ) );
	else if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_12Byte )
		STATUS_LOG ( ( 4, "%s[%p]: %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5], 
                    cdbData[6], cdbData[7], cdbData[8], cdbData[9], 
                    cdbData[10], cdbData[11] ) );
	else if ( GetCommandDescriptorBlockSize ( request ) == kSCSICDBSize_16Byte )
		STATUS_LOG ( ( 4, "%s[%p]: %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X : %X",
                    getName(), this, cdbData[0], cdbData[1], 
                    cdbData[2], cdbData[3], cdbData[4], cdbData[5], 
                    cdbData[6], cdbData[7], cdbData[8], cdbData[9], 
                    cdbData[10], cdbData[11], cdbData[12], cdbData[13], 
                    cdbData[14], cdbData[15] ) );
#endif
	
   	if ( GetInterfaceProtocol() == kProtocolBulkOnly )
	{
	
		if ( fBulkOnlyCommandStructInUse == true  )
		{
		
			RecordUSBTimeStamp (	UMC_TRACE ( kBOCommandAlreadyInProgress ),
									( uintptr_t ) this, ( uintptr_t ) request, NULL, NULL );
			goto Exit;
			
		}
		
		fBulkOnlyCommandStructInUse = true;
   		STATUS_LOG(( 6, "%s[%p]: SendSCSICommandforBulkOnlyProtocol sent", getName(), this ));
		status = SendSCSICommandForBulkOnlyProtocol ( request );
		if ( status != kIOReturnSuccess )
		{
			// If the command fails we want to make sure that we
			// don't hold up other commands.
			fBulkOnlyCommandStructInUse = false;
		}
		
		RecordUSBTimeStamp (	UMC_TRACE ( kBOSendSCSICommandReturned ),
								( uintptr_t ) this, ( uintptr_t ) request, status, NULL );
									
   		STATUS_LOG ( ( 5, "%s[%p]: SendSCSICommandforBulkOnlyProtocol returned %x", getName(), this, status ) );
		
	}
	else
	{
	
		if ( fCBICommandStructInUse == true )
		{
			
			RecordUSBTimeStamp (	UMC_TRACE ( kCBICommandAlreadyInProgress ),
									( uintptr_t ) this, ( uintptr_t ) request, NULL, NULL );
									
			goto Exit;
			
		}
		
		fCBICommandStructInUse = true;
		status = SendSCSICommandForCBIProtocol ( request );
		if( status != kIOReturnSuccess )
		{
			// If the command fails we want to make sure that we
			// don't hold up other commands.

			fCBICommandStructInUse = false;
		}
		
		RecordUSBTimeStamp (	UMC_TRACE ( kCBISendSCSICommandReturned ),
								( uintptr_t ) this, ( uintptr_t ) request, status, NULL );
								
   		STATUS_LOG ( ( 5, "%s[%p]: SendSCSICommandforCBIProtocol returned %x", getName(), this, status ) );
		
	}

	if ( status == kIOReturnSuccess )
	{	
		*serviceResponse = kSCSIServiceResponse_Request_In_Process;
	}

	accepted = true;

Exit:

	STATUS_LOG ( ( 6, "%s[%p]: SendSCSICommand returning accepted=%d", getName(), this, accepted ) );
	return accepted;
	
}


//--------------------------------------------------------------------------------------------------
//	CompleteSCSICommand																	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::CompleteSCSICommand ( SCSITaskIdentifier request, IOReturn status )
{

	SCSITaskStatus			taskStatus;

	check ( getWorkLoop()->inGate() == true );

	fBulkOnlyCommandStructInUse = false;
	fCBICommandStructInUse = false;

	//	Clear the count of consecutive I/Os which required a USB Device Reset.
	fConsecutiveResetCount = 0;
						
	RecordUSBTimeStamp (	UMC_TRACE ( kCompleteSCSICommand ),
							( uintptr_t ) this, ( uintptr_t ) request,
							status, NULL );
								
	if ( status == kIOReturnSuccess )
	{	
		taskStatus = kSCSITaskStatus_GOOD;
	}
	else
	{
		taskStatus = kSCSITaskStatus_CHECK_CONDITION;
	}

	STATUS_LOG ( ( 6, "%s[%p]: CompleteSCSICommand request=%p taskStatus=0x%02x", getName(), this, request, taskStatus ) );

	CommandCompleted ( request, kSCSIServiceResponse_TASK_COMPLETE, taskStatus );
	
}


//--------------------------------------------------------------------------------------------------
//	AbortSCSICommand																	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

SCSIServiceResponse
IOUSBMassStorageClass::AbortSCSICommand ( SCSITaskIdentifier abortTask )
{

 	IOReturn	status = kIOReturnSuccess;
 	
  	STATUS_LOG ( ( 6, "%s[%p]: AbortSCSICommand was called", getName(), this ) );
 	if ( abortTask == NULL )
 	{
 		// We were given an invalid SCSI Task object.  Let the client know.
  		STATUS_LOG ( ( 1, "%s[%p]: AbortSCSICommand was called with a NULL CDB object", getName(), this ) );
 		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
 	}
	
	if ( fTerminating == true )
	{
 		// We have an invalid interface, the device has probably been removed.
 		// Nothing else to do except to report an error.
  		STATUS_LOG ( ( 1, "%s[%p]: AbortSCSICommand was called with a NULL interface", getName(), this ) );
 		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	}
		
	RecordUSBTimeStamp (	UMC_TRACE( kAbortedTask ),
							( uintptr_t ) this, ( uintptr_t ) abortTask, NULL, NULL );
	
	if ( GetInterfaceReference()->GetInterfaceProtocol() == kProtocolBulkOnly )
	{
		status = AbortSCSICommandForBulkOnlyProtocol( abortTask );
   		STATUS_LOG ( ( 5, "%s[%p]: abortCDBforBulkOnlyProtocol returned %x", getName(), this, status ) );
	}
	else
	{
		status = AbortSCSICommandForCBIProtocol( abortTask );
   		STATUS_LOG ( ( 5, "%s[%p]: abortCDBforCBIProtocol returned %x", getName(), this, status ) );
	}

	// Since the driver currently does not support abort, return an error	
	return kSCSIServiceResponse_FUNCTION_REJECTED;
	
}


//--------------------------------------------------------------------------------------------------
//	IsProtocolServiceSupported															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageClass::IsProtocolServiceSupported ( 
										SCSIProtocolFeature 	feature, 
										void * 					serviceValue )
{

	bool			isSupported 	= false;
	OSDictionary * 	characterDict 	= NULL;
	
	STATUS_LOG ( ( 6,  "%s[%p]::IsProtocolServiceSupported called", getName(), this ) );
	
	characterDict = OSDynamicCast ( OSDictionary, ( getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
	
	switch ( feature )
	{
		
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
		{
			
			* ( ( UInt32 * ) serviceValue ) = GetMaxLogicalUnitNumber ( );
			isSupported = true;
			
		}
		break;
		
		//	We must state our maximum byte counts, because the default values chosen by the
		//	storage stack are too large for most USB devices.
		
		case kSCSIProtocolFeature_MaximumReadTransferByteCount:
		{
			
			UInt32		maxByteCount = kDefaultMaximumByteCountRead;
			
			if ( characterDict != NULL )
			{
				
				OSNumber * number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumByteCountReadKey ) );
				if ( number != NULL)
				{
					
					maxByteCount = number->unsigned32BitValue ( );
					
				}
				
			}
			
			*( ( UInt32 * ) serviceValue ) = maxByteCount;
			isSupported = true;
			
		}
		break;
		
		case kSCSIProtocolFeature_MaximumWriteTransferByteCount:
		{
			
			UInt32		maxByteCount = kDefaultMaximumByteCountWrite;
			
			if ( characterDict != NULL )
			{
				
				OSNumber * number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumByteCountWriteKey ) );
				if ( number != NULL)
				{
					
					maxByteCount = number->unsigned32BitValue ( );
					
				}
				
			}
			
			*( ( UInt32 * ) serviceValue ) = maxByteCount;
			isSupported = true;
			
		}
		break;
	
		//	We only state our maximum block counts if they are specified in the Device Characteristics dictionary.
		//	Otherwise, we let the storage stack calculate the block counts from our byte counts.

		case kSCSIProtocolFeature_MaximumReadBlockTransferCount:
		{
			
			OSNumber *		number = NULL;
			
			require_quiet ( characterDict, Exit );
			
			number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumBlockCountReadKey ) );
			require_quiet ( number, Exit );
			
			*( ( UInt32 * ) serviceValue ) = number->unsigned32BitValue ( );
			isSupported = true;
			
		}
		break;

		case kSCSIProtocolFeature_MaximumWriteBlockTransferCount:
		{
			
			OSNumber *		number = NULL;
			
			require_quiet ( characterDict, Exit );
			
			number = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOMaximumBlockCountWriteKey ) );
			require_quiet ( number, Exit );
			
			*( ( UInt32 * ) serviceValue ) = number->unsigned32BitValue ( );
			isSupported = true;
			
		}
		break;

		case kSCSIProtocolFeature_ProtocolSpecificPowerControl:
		{
			
			if ( fPortSuspendResumeForPMEnabled == true )
			{
				
				STATUS_LOG ( ( 6, "%s[%p]::IsProtocolServiceSupported - fPortSuspendResumeForPMEnabled enabled", getName ( ), this ) );
				isSupported = true;
				
			}
			
			if ( characterDict != NULL )
			{
				
				if ( characterDict->getObject ( kIOPropertyAutonomousSpinDownKey ) != NULL )
				{
					
					STATUS_LOG ( ( 6, "%s[%p]::IsProtocolServiceSupported - fAutonomousSpinDownWorkAround enabled", getName ( ), this ) );
					
					fAutonomousSpinDownWorkAround = true;
					isSupported = true;
					
				}
				
			}
			
		}
		break;
		
		default:
		break;
		
	}
	
	
Exit:
	
	
	return isSupported;
	
}


//--------------------------------------------------------------------------------------------------
//	HandleProtocolServiceFeature														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageClass::HandleProtocolServiceFeature ( 
										SCSIProtocolFeature 	feature, 
										void * 					serviceValue )
{

	bool	isSupported = false;
    
    
    STATUS_LOG ( ( 6,  "%s[%p]::HandleProtocolServiceFeature called, feature=%lu", getName(), this, ( UInt32 ) feature ) );
    
			
	switch ( feature )
	{
	
		case kSCSIProtocolFeature_ProtocolSpecificPowerControl:
		{
			
			IOReturn 	status          = kIOReturnError;
			UInt32		value			= * ( UInt32 * ) serviceValue;


			// If this device doesn't support suspend/resume bail.
			if ( fPortSuspendResumeForPMEnabled == true )
			{
				
				if ( value == kSCSIProtocolPowerStateOff )
				{
				
					 STATUS_LOG ( ( 6,  "%s[%p]::HandleProtocolServiceFeature suspend port", getName(), this ) );
					
					// Supsend the port. 
					status = SuspendPort ( true );
					require ( ( status == kIOReturnSuccess ), Exit );
				
				}
				
				if ( value == kSCSIProtocolPowerStateOn )
				{
				
					 STATUS_LOG ( ( 6,  "%s[%p]::HandleProtocolServiceFeature resume port", getName(), this ) );
					
					// Resume the port. 
					status = SuspendPort ( false );
					require ( ( status == kIOReturnSuccess ), Exit );
				
				}
				
				isSupported = true;
				
			}
			else if ( fAutonomousSpinDownWorkAround == true )
			{
				
				STATUS_LOG ( ( 6,  "%s[%p]::HandleProtocolServiceFeature NOP START_STOP", getName(), this ) );
				
				// This a workaround for devices which spin themselves up/down and can't
				// properly handle START_STOP commands from the host when the drive is 
				// already in the requested state.
				isSupported = true;
				
			}
				
		}
		break;
			
        // Default to super class. 
		default:
		{
		
			// Not a supported feature of this protocol.
			STATUS_LOG ( ( 6,  "%s[%p]::HandleProtocolServiceFeature called for a feature we do not support", getName(), this ) );
			
		}
		break;

	}
    
    
Exit:

	return isSupported;
    
}


#pragma mark -
#pragma mark *** Standard USB Command Methods ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	ClearFeatureEndpointStall -		Method to do the CLEAR_FEATURE command for
//									an ENDPOINT_STALL feature.		
//																						 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::ClearFeatureEndpointStall ( 
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
	thePipe->ClearPipeStall ( false );

	// Clear out the structure for the request
	bzero ( &fUSBDeviceRequest, sizeof ( IOUSBDevRequest ) );
	
	// Build the USB command
	fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType ( kUSBNone, kUSBStandard, kUSBEndpoint );
   	fUSBDeviceRequest.bRequest 			= kUSBRqClearFeature;
   	fUSBDeviceRequest.wValue			= 0;	// Zero is EndpointStall
	fUSBDeviceRequest.wIndex			= thePipe->GetEndpointNumber();
	if ( thePipe == GetBulkInPipe() )
	{
		fUSBDeviceRequest.wIndex		|= 0x80;
	}

	// Send the command over the control endpoint
	status = GetInterfaceReference()->DeviceRequest( &fUSBDeviceRequest, completion );
   	STATUS_LOG ( ( 5, "%s[%p]: ClearFeatureEndpointStall returned %x", getName(), this, status ) );
				
	RecordUSBTimeStamp (	UMC_TRACE ( kClearEndPointStall ), ( uintptr_t ) this, status, 
							thePipe->GetEndpointNumber(), NULL );
				
	return status;
	
}


//--------------------------------------------------------------------------------------------------
//	GetStatusEndpointStatus -	Method to do the GET_STATUS command for the
//								endpoint that the IOUSBPipe is connected to.		
//																						 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::GetStatusEndpointStatus ( 
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
	bzero( &fUSBDeviceRequest, sizeof ( IOUSBDevRequest ) );

	// Build the USB command	
    fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType ( kUSBIn, kUSBStandard, kUSBEndpoint );	
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
	status = GetInterfaceReference()->DeviceRequest ( &fUSBDeviceRequest, completion );
   	STATUS_LOG ( ( 5, "%s[%p]: GetStatusEndpointStatus returned %x", getName(), this, status ) );
	
	RecordUSBTimeStamp ( UMC_TRACE ( kGetEndPointStatus ),
						 ( uintptr_t ) this, status,
						 thePipe->GetEndpointNumber(), NULL );
						 
	return status;
	
}

#pragma mark -
#pragma mark *** Accessor Methods For All Protocol Variables ***
#pragma mark -


/* The following methods are for use only by this class */


//--------------------------------------------------------------------------------------------------
//	GetInterfaceReference -		Method to do the GET_STATUS command for the
//								endpoint that the IOUSBPipe is connected to.		
//																						 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOUSBInterface *
IOUSBMassStorageClass::GetInterfaceReference ( void )
{

   	if ( fInterface == NULL )
   	{
   		STATUS_LOG ( ( 2, "%s[%p]: GetInterfaceReference - Interface is NULL.", getName(), this ) );
   	}
   	
	return fInterface;
	
}


//--------------------------------------------------------------------------------------------------
//	SetInterfaceReference																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::SetInterfaceReference ( IOUSBInterface * newInterface )
{
	fInterface = newInterface;
}


//--------------------------------------------------------------------------------------------------
//	GetInterfaceSubClass																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

UInt8
IOUSBMassStorageClass::GetInterfaceSubclass ( void )
{
	return fPreferredSubclass;
}


//--------------------------------------------------------------------------------------------------
//	GetInterfaceProtocol																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

UInt8
IOUSBMassStorageClass::GetInterfaceProtocol ( void )
{
	return fPreferredProtocol;
}


//--------------------------------------------------------------------------------------------------
//	GetControlPipe																		 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetControlPipe ( void )
{

	if ( fTerminating == true )
	{
		return NULL;
	}
	
	return GetInterfaceReference()->GetDevice()->GetPipeZero();
	
}


//--------------------------------------------------------------------------------------------------
//	GetBulkInPipe																		 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetBulkInPipe ( void )
{
	return fBulkInPipe;
}


//--------------------------------------------------------------------------------------------------
//	GetBulkOutPipe																		 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetBulkOutPipe ( void )
{
	return fBulkOutPipe;
}


//--------------------------------------------------------------------------------------------------
//	GetInterruptPipe																	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOUSBPipe *
IOUSBMassStorageClass::GetInterruptPipe ( void )
{
	return fInterruptPipe;
}


//--------------------------------------------------------------------------------------------------
//	GetMaxLogicalUnitNumber																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

UInt8
IOUSBMassStorageClass::GetMaxLogicalUnitNumber ( void ) const
{
	return fMaxLogicalUnitNumber;
}


//--------------------------------------------------------------------------------------------------
//	SetMaxLogicalUnitNumber																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::SetMaxLogicalUnitNumber ( UInt8 maxLUN )
{
	fMaxLogicalUnitNumber = maxLUN;
}


#pragma mark -
#pragma mark *** Accessor Methods For CBI Protocol Variables ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	GetCBIRequestBlock																	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

CBIRequestBlock *	
IOUSBMassStorageClass::GetCBIRequestBlock ( void )
{

	// Return a pointer to the CBIRequestBlock
	return &fCBICommandRequestBlock;
	
}


//--------------------------------------------------------------------------------------------------
//	ReleaseCBIRequestBlock																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void	
IOUSBMassStorageClass::ReleaseCBIRequestBlock ( CBIRequestBlock * cbiRequestBlock )
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


//--------------------------------------------------------------------------------------------------
//	GetBulkOnlyRequestBlock															 	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

BulkOnlyRequestBlock *	
IOUSBMassStorageClass::GetBulkOnlyRequestBlock ( void )
{

	// Return a pointer to the BulkOnlyRequestBlock
	return &fBulkOnlyCommandRequestBlock;
	
}


//--------------------------------------------------------------------------------------------------
//	ReleaseBulkOnlyRequestBlock															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void	
IOUSBMassStorageClass::ReleaseBulkOnlyRequestBlock ( BulkOnlyRequestBlock * boRequestBlock )
{

	// Clear the request and completion to avoid possible double callbacks.
	boRequestBlock->request = NULL;

	// Since we only allow one command and the BulkOnlyRequestBlock is
	// a member variable, no need to do anything.
	return;
	
}


//--------------------------------------------------------------------------------------------------
//	GetNextBulkOnlyCommandTag															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

UInt32	
IOUSBMassStorageClass::GetNextBulkOnlyCommandTag ( void )
{

	fBulkOnlyCommandTag++;
	
	return fBulkOnlyCommandTag;
	
}


//--------------------------------------------------------------------------------------------------
//	HandlePowerOn - Will get called when a device has been resumed     						[PUBLIC]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::HandlePowerOn ( void )
{

	UInt8	eStatus[2];

	// The USB hub port that the device is connected to has been resumed,
	// check to see if the device is still responding correctly and if not, 
	// fix it so that it is.
	STATUS_LOG(( 6, "%s[%p]: HandlePowerOn", getName(), this ));
	
	if ( ( GetStatusEndpointStatus( GetBulkInPipe(), &eStatus[0], NULL) != kIOReturnSuccess ) ||
		 ( fRequiresResetOnResume == true ) )
	{   
		
		RecordUSBTimeStamp ( UMC_TRACE ( kHandlePowerOnUSBReset ), ( uintptr_t ) this, NULL, NULL, NULL );
							 
        ResetDeviceNow( true );
        
	}
	
	// If our port was suspended before sleep, it would have been resumed as part
	// of the global resume on system wake.
	fPortIsSuspended = false;
	
	return kIOReturnSuccess;
	
}


//--------------------------------------------------------------------------------------------------
//	handleOpen														   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageClass::handleOpen ( IOService *		client,
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


//--------------------------------------------------------------------------------------------------
//	handleClose													   							[PUBLIC]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::handleClose (	IOService *		client,
										IOOptionBits	options )
{
		
	if ( GetMaxLogicalUnitNumber() == 0 )
	{
		super::handleClose ( client, options );
		return;
	}
	
	require_nonzero ( fClients, Exit );
	
	if ( fClients->containsObject ( client ) )
	{
		fClients->removeObject( client );
		
		if ( ( fClients->getCount() == 0 ) && isInactive() )
		{
			message ( kIOMessageServiceIsRequestingClose, getProvider(), 0 );
		}
	}
	
	
Exit:
	
	
	return;
	
}


//--------------------------------------------------------------------------------------------------
//	handleIsOpen													   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageClass::handleIsOpen ( const IOService * client ) const
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


//--------------------------------------------------------------------------------------------------
//	sWaitForReset																 [STATIC][PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::sWaitForReset ( void * refcon )
{
	
	return ( ( IOUSBMassStorageClass * ) refcon )->GatedWaitForReset();
	
}


//--------------------------------------------------------------------------------------------------
//	GatedWaitForReset																	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::GatedWaitForReset ( void )
{
	
	IOReturn status = kIOReturnSuccess;
	
	while ( fResetInProgress == true )
	{
		status = fCommandGate->commandSleep ( &fResetInProgress, THREAD_UNINT );
	}
	
	return status;
	
}


//--------------------------------------------------------------------------------------------------
//	sWaitForTaskAbort															 [STATIC][PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::sWaitForTaskAbort ( void * refcon )
{
	return kIOReturnUnsupported;
}


//--------------------------------------------------------------------------------------------------
//	GatedWaitForTaskAbort																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::GatedWaitForTaskAbort ( void )
{
	return kIOReturnUnsupported;
}


//--------------------------------------------------------------------------------------------------
//	sResetDevice																 [STATIC][PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::sResetDevice ( void * refcon )
{
	
	IOUSBMassStorageClass *		driver          = NULL;;
	IOReturn					status          = kIOReturnError;
    IOUSBInterface *			interfaceRef	= NULL;
	UInt32						deviceInfo		= 0;
	IOUSBDevice *				deviceRef		= NULL;
    thread_t                    thread          = THREAD_NULL;
	
	driver = ( IOUSBMassStorageClass * ) refcon;
    require ( ( driver != NULL ), Exit );
    
	STATUS_LOG ( ( 4, "%s[%p]: sResetDevice Entered", driver->getName(), driver ) );

	// Check if we should bail out because we are
	// being terminated.
	if ( ( driver->fTerminating == true ) ||
		 ( driver->isInactive( ) == true ) )
	{
		STATUS_LOG ( ( 2, "%s[%p]: sResetDevice - We are being terminated!", driver->getName(), driver ) );
		RecordUSBTimeStamp ( ( UMC_TRACE ( kUSBDeviceResetWhileTerminating ) | DBG_FUNC_START ), ( uintptr_t ) driver, 
                             ( unsigned int ) driver->fTerminating, ( unsigned int ) driver->isInactive( ), NULL );
		goto ErrorExit;
	}
    
    interfaceRef = driver->GetInterfaceReference();
    require ( ( interfaceRef != NULL ), ErrorExit );
	
	deviceRef = interfaceRef->GetDevice();
	require ( ( deviceRef != NULL ), ErrorExit );
	
	// Check that we are still connected to the hub and that our port is enabled.
	status = deviceRef->GetDeviceInformation ( &deviceInfo );
	STATUS_LOG ( ( 5, "%s[%p]: GetDeviceInfo returned status = %x deviceInfo = %x", driver->getName(), driver, status, deviceInfo ) );
	
	if ( ( status != kIOReturnSuccess ) ||
		 ( ( deviceInfo & ( 1 << kUSBInformationDeviceIsConnectedBit ) ) == 0 ) ||
		 ( ( deviceInfo & ( 1 << kUSBInformationDeviceIsEnabledBit ) ) ) == 0 )
	{
		//	Device is either disconnected or its port is disabled.
		status = kIOReturnNoDevice;
	}

	if ( status != kIOReturnNoDevice )
	{
		
        // Ensure that devices has been resumed before we attempt to reset it ( GL137A-05 ).
        deviceRef->SuspendDevice ( false );
	
        // Device is still attached. Lets try resetting.
		status = deviceRef->ResetDevice();
		STATUS_LOG ( ( 5, "%s[%p]: ResetDevice() returned = %x", driver->getName(), driver, status ) );
		RecordUSBTimeStamp ( UMC_TRACE ( kUSBDeviceResetReturned ), ( uintptr_t ) driver, status, NULL, NULL );
						 
	}
	else
	{
		RecordUSBTimeStamp ( UMC_TRACE ( kUSBDeviceResetAfterDisconnect ), ( uintptr_t ) driver, NULL, NULL, NULL );   
	}
	
	if ( status == kIOReturnSuccess )
    {
     
        // Reset host side data toggles.
		if ( driver->fBulkInPipe != NULL )
		{
			driver->fBulkInPipe->ClearPipeStall ( false );
		}
		
		if ( driver->fBulkOutPipe != NULL )
		{
			driver->fBulkOutPipe->ClearPipeStall ( false );
        }
        
        if ( driver->fInterruptPipe != NULL )
		{
			driver->fInterruptPipe->ClearPipeStall ( false );
        }
        
    }
    else
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
		STATUS_LOG ( ( 2, "%s[%p]: sResetDevice - We are being terminated (ii) !", driver->getName(), driver ) );
		RecordUSBTimeStamp ( ( UMC_TRACE ( kUSBDeviceResetWhileTerminating ) | DBG_FUNC_END ), ( uintptr_t ) driver, 
                             ( unsigned int ) driver->fTerminating, ( unsigned int ) driver->isInactive( ), NULL );
		goto ErrorExit;
	}
	
	
ErrorExit:
	
	STATUS_LOG ( ( 2, "%s[%p]: sResetDevice status=0x%x fResetInProgress=%d", driver->getName(), driver, status, driver->fResetInProgress ) );

	if ( status != kIOReturnSuccess )
	{
    
        // We set the device state to detached so the proper status for the 
        // device is returned along with the aborted SCSITask.
        driver->fDeviceAttached = false;
        
        // Let the clients know that the device is gone.
        driver->SendNotification_DeviceRemoved( );
		
	}
     
	// We complete the failed I/O with kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE  
	// and either kSCSITaskStatus_DeliveryFailure or kSCSITaskStatus_DeviceNotPresent,
	// as per the fDeviceAttached flag.
	driver->AbortCurrentSCSITask();
    
    if ( ( driver->fTerminating == true ) && ( driver->GetInterfaceReference() != NULL ) ) 
    {

        IOUSBInterface * currentInterface;
        
        currentInterface = driver->GetInterfaceReference();
        
        // We set this to NULL first chance we get to prevent a potential race between
        // our reset thread and our didTeriminate(). 
        driver->SetInterfaceReference ( NULL );
        
        // Close and nullify our USB Interface.
        currentInterface->close ( driver );
        
    }
    	
	driver->fResetInProgress = false;
        
	if ( driver->fWaitingForReconfigurationMessage == false )
	{
        // Unblock our main thread.
		driver->fCommandGate->commandWakeup ( &driver->fResetInProgress, false );
        
	}
	
	STATUS_LOG ( ( 6, "%s[%p]: sResetDevice exiting.", driver->getName(), driver ) );
	
	// We retained the driver in ResetDeviceNow() when
	// we created a thread for sResetDevice().
	driver->release();
    
    // Terminate the thread.
	thread = current_thread ( );
    require ( ( thread != THREAD_NULL ), Exit );
    
	thread_deallocate ( thread );
	thread_terminate ( thread );
    
    
Exit:

    
	return;
	
}


//--------------------------------------------------------------------------------------------------
//	sAbortCurrentSCSITask														 [STATIC][PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::sAbortCurrentSCSITask ( void * refcon )
{
}


OSMetaClassDefineReservedUsed ( IOUSBMassStorageClass, 1 );


//--------------------------------------------------------------------------------------------------
//	StartDeviceRecovery -	The recovery sequence to restore functionality for
//							devices that stop responding (like many devices
//							after a Suspend/Resume).								  	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::StartDeviceRecovery ( void )
{
	// First check to see if the device is still connected.
	UInt8		eStatus[2];
	IOReturn	status = kIOReturnError;
	
	// The USB hub port that the device is connected to has been resumed,
	// check to see if the device is still responding correctly and if not, 
	// fix it so that it is. 
	STATUS_LOG ( ( 5, "%s[%p]: StartDeviceRecovery", getName(), this ) );
	
	if ( fBulkOnlyCommandStructInUse == true )
	{
		// Set up the IOUSBCompletion structure
		fBulkOnlyCommandRequestBlock.boCompletion.target 		= this;
		fBulkOnlyCommandRequestBlock.boCompletion.action 		= &this->DeviceRecoveryCompletionAction;
		status = GetStatusEndpointStatus ( GetBulkInPipe(), &eStatus[0], &fBulkOnlyCommandRequestBlock.boCompletion);
	}
	else if ( fCBICommandStructInUse == true )
	{
		// Set up the IOUSBCompletion structure
		fCBICommandRequestBlock.cbiCompletion.target 		= this;
		fCBICommandRequestBlock.cbiCompletion.action 		= &this->DeviceRecoveryCompletionAction;
		status = GetStatusEndpointStatus ( GetBulkInPipe(), &eStatus[0], &fCBICommandRequestBlock.cbiCompletion);
   	}
	
	return status;
}

OSMetaClassDefineReservedUsed ( IOUSBMassStorageClass, 2 );


//--------------------------------------------------------------------------------------------------
//	FinishDeviceRecovery																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::FinishDeviceRecovery ( IOReturn status )
{

	ResetDeviceNow( false );
}


//--------------------------------------------------------------------------------------------------
//	DeviceRecoveryCompletionAction														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageClass::DeviceRecoveryCompletionAction (
					                void *			target,
					                void *			parameter,
					                IOReturn		status,
					                UInt32			bufferSizeRemaining)
{

	UNUSED ( parameter );
	UNUSED ( bufferSizeRemaining );
	
	IOUSBMassStorageClass *		theMSC;
	
	theMSC = ( IOUSBMassStorageClass * ) target;
	theMSC->FinishDeviceRecovery ( status );
	
}


//--------------------------------------------------------------------------------------------------
//	ResetDeviceNow																		 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::ResetDeviceNow ( bool waitForReset )
{
    
    // Make sure we aren't terminating. 
	if ( ( fTerminating == false ) && ( isInactive() == false ) )
	{
	
        thread_t        thread = THREAD_NULL;
        kern_return_t   result = KERN_FAILURE;
        
		// We call retain here so that the driver will stick around long enough for
		// sResetDevice() to do it's thing in case we are being terminated.  The
		// retain() is balanced with a release in sResetDevice().
		retain();
		
		// The endpoint status could not be retrieved meaning that the device has
		// stopped responding. Or this could be a device we know needs a reset.
		// Begin the device reset sequence.
		
		STATUS_LOG ( ( 4, "%s[%p]: ResetDeviceNow waitForReset=%d", getName(), this, waitForReset ) );
		
		// Reset the device on its own thread so we don't deadlock.
		fResetInProgress = true;
        
        // When peform a USB device reset, we have two options. We can actively block on the reset thread,
        // or we can not block and wait for the IOUSBFamily to send us a message about our device being 
        // reset. If we're reseting from a callback thread, we can't block, so we have to use the message option. 
        
		fWaitingForReconfigurationMessage = false;
        if ( waitForReset == false )
        {
            fWaitingForReconfigurationMessage = true;
        }
        
		result = kernel_thread_start (   ( thread_continue_t ) &IOUSBMassStorageClass::sResetDevice,
                                        this,
                                        &thread );
            
		if ( result == KERN_SUCCESS )
		{
			if ( waitForReset == true )
			{
		
				STATUS_LOG ( ( 4, "%s[%p]: kIOMessageServiceIsResumed Waiting for Reset.", getName(), this ) );
				fCommandGate->runAction ( ( IOCommandGate::Action ) &IOUSBMassStorageClass::sWaitForReset );
			}
		}
		else
		{
			// Since we failed to start the thread we should give back the retained token.
			release();
		}
		
	}
    
}


//--------------------------------------------------------------------------------------------------
//	AbortCurrentSCSITask																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageClass::AbortCurrentSCSITask( void )
{
	
	SCSITaskIdentifier	currentTask = NULL;
	
	if ( fCommandGate->getWorkLoop()->inGate() == false )
	{
		
		fCommandGate->runAction (
					OSMemberFunctionCast (
						IOCommandGate::Action,
						this,
						&IOUSBMassStorageClass::AbortCurrentSCSITask ) );
		
		return;
		
	}	

	//	We are holding the workLoop gate.

	STATUS_LOG ( ( 4, "%s[%p]: AbortCurrentSCSITask Entered", getName(), this ) );
	
	if( fBulkOnlyCommandStructInUse == true )
	{
		currentTask = fBulkOnlyCommandRequestBlock.request;
	}
	else if( fCBICommandStructInUse == true )
	{
		currentTask = fCBICommandRequestBlock.request;
	}
	
	if ( currentTask != NULL )
	{
	
		fBulkOnlyCommandStructInUse = false;
		fCBICommandStructInUse = false;
		fBulkOnlyCommandRequestBlock.request = NULL;
		fCBICommandRequestBlock.request = NULL;

		//	Increment the count of consecutive I/Os which were aborted during a reset.	
		//	If that count is greater than the max, then consider the drive unusable, and terminate it
		//	so that the non-responsive volume doesn't hang restart, shutdown or applications.
		fConsecutiveResetCount++;
		if ( ( fConsecutiveResetCount > kMaxConsecutiveResets ) && ( fDeviceAttached == true ) )
		{
			IOLog ( "%s[%p]: The device is still unresponsive after %u consecutive USB Device Resets; it will be terminated.\n", getName(), this, fConsecutiveResetCount );
			fDeviceAttached = false;
		}
		else
		{
			STATUS_LOG ( ( 4, "%s[%p]: AbortCurrentSCSITask fConsecutiveResetCount=%u", getName(), this, fConsecutiveResetCount ) );
		}
		
		if ( fDeviceAttached == false )
		{ 
			STATUS_LOG ( ( 1, "%s[%p]: AbortCurrentSCSITask Aborting currentTask=%p with device not present.", getName(), this, currentTask ) );
			fTerminating = true;
			CommandCompleted ( currentTask, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE, kSCSITaskStatus_DeviceNotPresent );
			SendNotification_DeviceRemoved ( );
			terminate ( );
		}
		else
		{
			STATUS_LOG ( ( 1, "%s[%p]: AbortCurrentSCSITask Aborting currentTask=%p with delivery failure.", getName(), this, currentTask ) );
			CommandCompleted ( currentTask, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE, kSCSITaskStatus_DeliveryFailure );
		}
	
	}
	
	STATUS_LOG ( ( 4, "%s[%p]: AbortCurrentSCSITask Exiting", getName(), this ) );
	
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
	
    // Marking both captive and internal devices as internal. 
    if ( ( deviceInformation & ( 1 << kUSBInformationDeviceIsCaptiveBit ) ) ||
         ( deviceInformation & ( 1 << kUSBInformationDeviceIsInternalBit ) ) )
    {
        internal = true;
    }	
	
	
ErrorExit:

    return internal;
	
}


//-----------------------------------------------------------------------------
//	- SuspendPort                                                   [PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
IOUSBMassStorageClass::SuspendPort ( bool suspend )
{

	
	STATUS_LOG ( ( 4, "%s[%p]: SuspendPort called!", getName(), this ) );
	
    IOReturn            status          = kIOReturnError;
    IOUSBInterface *    usbInterfaceRef = NULL;
    IOUSBDevice *       usbDeviceRef    = NULL;


    // See if we're already in the desired state.
    if ( suspend == fPortIsSuspended )
    {
        
        STATUS_LOG ( ( 4, "%s[%p]: SuspendPort !!!ALREADY!!! in desired state.", getName(), this ) );
        
        status = kIOReturnSuccess;
        goto Exit;
        
    }
    
    // Get our Device referene safely.
    usbInterfaceRef = GetInterfaceReference();
    require ( usbInterfaceRef, Exit );
    
    usbDeviceRef = usbInterfaceRef->GetDevice();
    require ( usbDeviceRef, Exit );
    
    // Suspend the USB port so that a remote wakeup will detect new media. 
    if ( suspend == true )
    {
    
         STATUS_LOG ( ( 6,  "%s[%p]::SuspendPort suspend port", getName(), this ) );
        
        // Supsend the port. 
        status = usbDeviceRef->SuspendDevice ( true );
        require ( ( status == kIOReturnSuccess ), Exit );
        
        // Suspend was successful, our USB port is now suspended. 
        fPortIsSuspended = true;
    
    }

    // If the port was suspended, resume it, the host wants the drive back online. 
    if ( suspend == false )
    {
        
        STATUS_LOG ( ( 6,  "%s[%p]::SuspendPort resume port", getName(), this ) );
        
        // Reusme the port. 
        status = usbDeviceRef->SuspendDevice ( false );
        require ( ( status == kIOReturnSuccess ), Exit );
        
        // It takes the USB controller a little while to get back on the line.
        IOSleep ( 15 );
        // Resume was successful, our USB port is now active. 
        fPortIsSuspended = false;
            
    }
    
    
Exit:
    
    
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
