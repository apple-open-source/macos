/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include "IOFireWireSerialBusProtocolTransport.h"

#include <IOKit/firewire/IOConfigDirectory.h>
#include <IOKit/firewire/IOFireWireDevice.h>

#if 0
#pragma mark -
#pragma mark == Macros ==
#pragma mark -
#endif

//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 										0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING			"FireWire SBP Transport"

#include "IOFireWireSerialBusProtocolTransportDebugging.h"

#if DEBUG
#define FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL		0
#endif

#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors (	IOFireWireSerialBusProtocolTransport, IOSCSIProtocolServices )

#if 0
#pragma mark -
#pragma mark == Constants ==
#pragma mark -
#endif

//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------


#define kSBP2ReceiveBufferByteCountKey		"SBP2ReceiveBufferByteCount"
#define kDefaultIOBlockCount				256
#define kCRSModelInfo_ValidBitsMask			0x00FFFFFF
#define kCRSModelInfo_TargetDiskMode		0x0054444D
#define kIOFireWireMessageServiceIsRequestingClose kIOFWMessageServiceIsRequestingClose
#define kDefaultTimeOutValue				30000

enum 
{
	kFireWireSBP2CommandTransferDataToTarget = 0L,
	kFireWireSBP2CommandTransferDataFromTarget = kFWSBP2CommandTransferDataFromTarget
};

#if 0
#pragma mark -
#pragma mark == Static Debug Assertion Method ==
#pragma mark -
#endif

//-----------------------------------------------------------------------------
//	IOFireWireSerialBusProtocolTransportDebugAssert				   [STATIC]
//-----------------------------------------------------------------------------

#if !DEBUG_ASSERT_PRODUCTION_CODE

void
	IOFireWireSerialBusProtocolTransportDebugAssert (
		const char * 	componentNameString,
		const char * 	assertionString, 
		const char * 	exceptionLabelString,
		const char * 	errorString,
		const char * 	fileName,
		long 			lineNumber,
		int 			errorCode )
{
		
	IOLog ( "%s Assert failed: %s ", componentNameString, assertionString );
	
	if ( exceptionLabelString != NULL ) { IOLog ( "%s ", exceptionLabelString ); }
	
	if ( errorString != NULL ) { IOLog ( "%s ", errorString ); }
	
	if ( fileName != NULL ) { IOLog ( "file: %s ", fileName ); }
	
	if ( lineNumber != 0 ) { IOLog ( "line: %ld ", lineNumber ); }
	
	if ( ( long ) errorCode != 0 ) { IOLog ( "error: %ld ( 0x%08lx )", ( long ) errorCode, ( long ) errorCode  ); }
	
	IOLog ( "\n" );
	
}

#endif

#if 0
#pragma mark -
#pragma mark == Public Methods ==
#pragma mark -
#endif

//-----------------------------------------------------------------------------
//	¥ init - Called by IOKit to initialize us.						   [PUBLIC]
//-----------------------------------------------------------------------------

bool IOFireWireSerialBusProtocolTransport::init ( OSDictionary * propTable )
{
	
	fORB					= 0;
	fLogin					= 0;
	fLoginRetryCount		= 0;
	fReconnectCount			= 0;
	fLoggedIn				= false;
	fNeedLogin				= false;
	fLUNResetORB			= NULL;
	fDeferRegisterService	= true;
	fObjectIsOpen			= false;
	
	if ( super::init ( propTable ) == false )
	{
		return false;
	}
	
	return true;
	
}

//-----------------------------------------------------------------------------
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//-----------------------------------------------------------------------------

bool IOFireWireSerialBusProtocolTransport::start ( IOService * provider )
{
	
	IOReturn				status;
	Boolean					returnValue;
	Boolean 				openSucceeded;
	OSNumber * 				rto;
	OSNumber * 				wto;
	bool					sucess;

	status				= kIOReturnSuccess;
	returnValue			= false;
	openSucceeded		= false;
	rto					= NULL;
	wto					= NULL;
	sucess				= false;
	
	// See if there is a read time out duration passed in the property dictionary - if not
	// set the default to 30 seconds.
	
	rto = OSDynamicCast ( OSNumber, getProperty ( kIOPropertyReadTimeOutDurationKey ) );
	if ( rto == NULL )
	{
		rto = OSNumber::withNumber ( kDefaultTimeOutValue, 32 );
		require ( rto, exit );
		
		sucess = setProperty ( kIOPropertyReadTimeOutDurationKey, rto );
		check ( sucess );
		rto->release();

		rto = OSDynamicCast ( OSNumber, getProperty ( kIOPropertyReadTimeOutDurationKey ) );
	}

	STATUS_LOG ( ( "%s: start read time out = %ld\n", getName (), rto->unsigned32BitValue ( ) ) );
	
	// See if there is a write time out duration passed in the property dictionary - if not
	// set the default to 30 seconds.
	
	wto = OSDynamicCast ( OSNumber,  getProperty ( kIOPropertyWriteTimeOutDurationKey ) );
	if ( wto == NULL )
	{
		wto = OSNumber::withNumber ( kDefaultTimeOutValue, 32 );
		require ( wto, exit );
		
		sucess = setProperty ( kIOPropertyWriteTimeOutDurationKey, wto );
		check ( sucess );
		wto->release();
		
		wto = OSDynamicCast ( OSNumber, getProperty ( kIOPropertyWriteTimeOutDurationKey ) );
	}

	STATUS_LOG ( ( "%s: start read time out = %ld\n", getName (), wto->unsigned32BitValue ( ) ) );

	fSBPTarget = OSDynamicCast ( IOFireWireSBP2LUN, provider );
	require ( fSBPTarget, exit );
	
	// Add a retain here so we can keep IOFireWireSBP2LUN from doing garbage
	// collection on us when we are in the middle of our finalize method.
	
	fSBPTarget->retain ( );
	
	openSucceeded = super::start ( provider );
	require ( openSucceeded, exit );
	
	openSucceeded = provider->open ( this );
	require ( openSucceeded, exit );
		
	fUnit = fSBPTarget->getFireWireUnit ();
	require ( fUnit, exit );
	
	// Explicitly set the "enable retry on ack d" flag.
	
	fUnit->setNodeFlags ( kIOFWEnableRetryOnAckD );
	
	status = AllocateResources ();
	require_noerr ( status, exit );
	
	// Get us on the workloop so we can sleep the start thread.
	
	fCommandGate->runAction ( ConnectToDeviceStatic );
	
	if ( reserved->fLoginState == kLogginSucceededState )
	{
		registerService ();
	}
	
	STATUS_LOG ( ( "%s: start complete\n", getName () ) );
	
	returnValue = true;
	
	InitializePowerManagement ( provider );
	
exit:
	
	if ( returnValue == false )
	{
		
		STATUS_LOG ( ( "%s: start failed.  status = %x\n", getName (), status) );
		
		// Call the cleanUp method to clean up any allocated resources.
		cleanUp ();
	}
	
	return returnValue;
	
}

//-----------------------------------------------------------------------------
//	¥ cleanUp - Called to deallocate any resources.					   [PUBLIC]
//-----------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::cleanUp ( void )
{
	
	STATUS_LOG ( ( "%s: cleanUp called\n", getName () ) );
	
	if ( fSBPTarget != NULL )
	{
		// Close SBP2 if we have opened it.
		
		if ( fSBPTarget->isOpen ( this ) )
		{
			fSBPTarget->close ( this );
		}
		
		fSBPTarget = NULL;
	}
	
}

//-----------------------------------------------------------------------------
// ¥ finalize - Terminates all power management.					[PROTECTED]
//-----------------------------------------------------------------------------

bool
	IOFireWireSerialBusProtocolTransport::finalize (
		IOOptionBits options )
{

	DeallocateResources ();
	
	// Release the retain we took to keep IOFireWireSBP2LUN from doing garbage
	// collection on us when we are in the middle of DeallocateResources.
		
	if ( fSBPTarget )
	{
		fSBPTarget->release ( );
	}
	
	return super::finalize ( options );
	
}

//-----------------------------------------------------------------------------
//	¥ free - Called to deallocate ExpansionData.					   [PUBLIC]
//-----------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::free ( void )
{
	
	if ( reserved != NULL )
	{
		
		if ( reserved->fCommandPool != NULL )
		{
			
			reserved->fCommandPool->release ();
			reserved->fCommandPool = NULL;
			
		}
		
		IOFree ( reserved, sizeof ( ExpansionData ) );
		reserved = NULL;
		
	}
	
	super::free ( );
	
}

#if 0
#pragma mark -
#pragma mark == Protected Methods ==
#pragma mark -
#endif

//-----------------------------------------------------------------------------
//	¥ CommandORBAccessor - Retrieves command orb.					[PROTECTED]
//-----------------------------------------------------------------------------

IOFireWireSBP2ORB * 
	IOFireWireSerialBusProtocolTransport::CommandORBAccessor ( 
		void )
{
	
	return fORB;
	
}

//-----------------------------------------------------------------------------
//	¥ SBP2LoginAccessor - Retrieves login orb.						[PROTECTED]
//-----------------------------------------------------------------------------

IOFireWireSBP2Login * 
	IOFireWireSerialBusProtocolTransport::SBP2LoginAccessor (
		void )
{

	return fLogin;
	
}

//-----------------------------------------------------------------------------
//	¥ message - Called by IOKit to deliver messages.				[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
	IOFireWireSerialBusProtocolTransport::message (
		UInt32 		type,
		IOService *	nub,
		void * 		arg )
{
	
	SBP2ClientOrbData *		clientData 	= NULL;
	IOReturn				status		= kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kIOMessageServiceIsSuspended:
			STATUS_LOG ( ( "%s: kIOMessageServiceIsSuspended\n", getName () ) );
			// bus reset started - set flag to stop submitting orbs.
			fLoggedIn = false;
			break;
		
		case kIOMessageServiceIsResumed:
			STATUS_LOG ( ( "%s: kIOMessageServiceIsResumed\n", getName () ) );
			// bus reset finished - if we have failed to log in previously, try again.
			if ( fNeedLogin )
			{
				
				fNeedLogin = false;
				fLoginRetryCount = 0;
				
				// In case we are resumed after a terminate.
				if ( fLogin != NULL )
				{
					
					login ();
					
				}
				
			}
			break;
			
		case kIOMessageFWSBP2ReconnectComplete:
			
			// As of this writing FireWireSBP2LUN will message all multi-LUN instances with this
			// message. So we qualify this message with our instance variable fLogin and ignore others.
			
			if( ( ( FWSBP2ReconnectParams* ) arg )->login == fLogin )
			{
				STATUS_LOG ( ( "%s: kIOMessageFWSBP2ReconnectComplete\n", getName () ) );

				fLoggedIn = true;
				
				clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
				if ( clientData != NULL )
				{
					
					if ( ( clientData->scsiTask != NULL ) && ( fReconnectCount < kMaxReconnectCount ) )
					{
						
						STATUS_LOG ( ( "%s: resubmit orb \n", getName () ) );
						fReconnectCount++;
						fLogin->submitORB ( fORB );
						
					}
					
					else
					{
						// We are unable to recover from bus reset storm
						// We have exhausted the fReconnectCount - punt...
						
						clientData->serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
						clientData->taskStatus		= kSCSITaskStatus_No_Status;
						CompleteSCSITask ( fORB );
					}
				}
			}
			
			break;
			
		case kIOMessageFWSBP2ReconnectFailed:

			// As of this writing FireWireSBP2LUN will message all multi-LUN instances with this
			// message. So we qualify this message with our instance variable fLogin and ignore others.
			
			if( ( ( FWSBP2ReconnectParams* ) arg )->login == fLogin )
			{
				STATUS_LOG ( ( "%s: kIOMessageFWSBP2ReconnectFailed\n", getName () ) );
				
				// Try to reestablish log in.
				fLoginRetryCount = 0;
				login ();
			}
			break;

		case kIOFireWireMessageServiceIsRequestingClose:
			STATUS_LOG ( ( "%s: kIOFireWireMessageServiceIsRequestingClose\n", getName () ) );
	
			// tell our super to message it's clients that the device is gone
			SendNotification_DeviceRemoved ();
			
			if ( fORB != NULL )
			{
				clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
				if ( clientData != NULL )
				{
					if ( clientData->scsiTask != NULL )
					{
						// We are unable to recover from bus reset storm
						// We have exhausted the fReconnectCount - punt...
						
						clientData->serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
						clientData->taskStatus		= kSCSITaskStatus_No_Status;
						CompleteSCSITask ( fORB );
					}
				}
			}
			
			if ( fSBPTarget != NULL )
			{		
				if ( fSBPTarget->isOpen ( this ) )
				{
					fSBPTarget->close ( this );
				}
			}
		
			break;
		
		case kIOMessageServiceIsTerminated:
			STATUS_LOG ( ( "%s: kIOMessageServiceIsTerminated\n", getName () ) );
			
			// let go of memory and what not
			cleanUp ();
			break;
			
		default:
			
			status = IOService::message (type, nub, arg);
			break;
		
	}
	
	return status;
	
}


//-----------------------------------------------------------------------------
//	¥ SendSCSICommand - Converts a SCSITask to an ORB.				[PROTECTED]
//-----------------------------------------------------------------------------

bool 
	IOFireWireSerialBusProtocolTransport::SendSCSICommand (
		SCSITaskIdentifier 		request,
		SCSIServiceResponse * 	serviceResponse,
		SCSITaskStatus * 		taskStatus )
{
	
	SBP2ClientOrbData *			clientData 			= NULL;
	IOFireWireSBP2ORB *			orb					= NULL;
	SCSICommandDescriptorBlock	cdb					= { 0 };
	UInt8						commandLength		= 0;
	UInt32						commandFlags		= 0;
	UInt32						timeOut				= 0;
	bool						commandProcessed	= true;
	
	STATUS_LOG ( ( "%s: SendSCSICommand called\n", getName () ) );
	
	*serviceResponse	= kSCSIServiceResponse_Request_In_Process;
	*taskStatus			= kSCSITaskStatus_No_Status;
	
	if ( isInactive () )
	{
		// device is disconnected - we can not service command request
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		commandProcessed = true;
		goto exit;
	}
	
	// get an orb from our orb pool and block until we get one
	orb = ( IOFireWireSBP2ORB * ) reserved->fCommandPool->getCommand ( true );
	if ( isInactive () )
	{
		reserved->fCommandPool->returnCommand ( orb );
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		commandProcessed = true;
		goto exit;
	}
	
	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	if ( clientData == NULL ) goto exit;
	
	GetCommandDescriptorBlock ( request, &cdb );
	commandLength = GetCommandDescriptorBlockSize ( request );
	
#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3 )
	if ( commandLength == kSCSICDBSize_6Byte )
	{
		
		STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0], cdb[1],
					 cdb[2], cdb[3], cdb[4], cdb[5] ) );
		
	}
	
	else if ( commandLength == kSCSICDBSize_10Byte )
	{
		
		STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
					cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
					cdb[9] ) );
		
	}
	
	else if ( commandLength == kSCSICDBSize_12Byte )
	{
		
		STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
					cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
					cdb[9], cdb[10], cdb[11] ) );
		
	}
#endif // (FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3)
	
	fReconnectCount	= 0;
	commandFlags	= ( GetDataTransferDirection ( request ) == kSCSIDataTransfer_FromTargetToInitiator ) ? 
															  kFireWireSBP2CommandTransferDataFromTarget : 
															  kFireWireSBP2CommandTransferDataToTarget;
	
	orb->setCommandFlags (	commandFlags |
							kFWSBP2CommandCompleteNotify |
							kFWSBP2CommandImmediate |
							kFWSBP2CommandNormalORB );
	
	
	SetCommandBuffers ( orb, request );
	
	orb->setCommandBlock ( cdb, commandLength );
	
	// SBP-2 needs a non-zero timeout to fire completion routines if timeout
	// is not expressed default to 0xFFFFFFFF.
	
	timeOut = GetTimeoutDuration ( request );
	if ( timeOut == 0 )
	{
		timeOut = 0xFFFFFFFF;
	}
	
	orb->setCommandTimeout ( timeOut );
	
	//	Close the gate here to eliminate potenially rare double append of
	//	orb. If on a DP machine and a bus reset occurs the login thread
	//	can append the orb as well as here.
	
	fCommandGate->runAction ( CriticalOrbSubmissionStatic, orb, request );
	
exit:
	
	
	STATUS_LOG ( ( "%s: SendSCSICommand exit, Service Response = %x\n", getName (), *serviceResponse) );
	
	return commandProcessed;
	
}

//-----------------------------------------------------------------------------
//	¥ SetCommandBuffers - Sets the command buffers in the ORB.		[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
	IOFireWireSerialBusProtocolTransport::SetCommandBuffers (
		IOFireWireSBP2ORB *	orb,
		SCSITaskIdentifier	request )
{
	
	return orb->setCommandBuffers (	GetDataBuffer ( request ),
								GetDataBufferOffset ( request ),
								GetRequestedDataTransferCount ( request ) );
	
}

//-----------------------------------------------------------------------------
//	¥ CompleteSCSITask - Complets a task.							[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::CompleteSCSITask (
		IOFireWireSBP2ORB * orb )
{
	
	SBP2ClientOrbData *	clientData = NULL;
	
	STATUS_LOG ( ( "%s: CompleteSCSITask called\n", getName () ) );

	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	if ( clientData != NULL )
	{
		
		if ( clientData->scsiTask != NULL )
		{
			
			SCSITaskIdentifier		scsiTask		= NULL;
			SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
			SCSITaskStatus			taskStatus		= kSCSITaskStatus_No_Status;
			IOByteCount				bytesTransfered = 0;
			
			//	/!\ WARNING - because SBP-2 can send status information at different
			//	stage of an orb's life ( or send none at all ) the caller of this routine
			//	has determined that the orb is indeed done. So we need to explicitly tell
			//	SBP-2 to left go of the buffer reference by calling releaseCommandBuffers.
			
			orb->releaseCommandBuffers ();
			
			
			if ( clientData->taskStatus == kSCSITaskStatus_GOOD )
			{
				
				bytesTransfered = GetRequestedDataTransferCount ( clientData->scsiTask );
				
			}
			
			SetRealizedDataTransferCount ( clientData->scsiTask, bytesTransfered );
			
			// re-entrancy protection
			scsiTask				= clientData->scsiTask;
			serviceResponse 		= clientData->serviceResponse;
			taskStatus				= clientData->taskStatus;
			clientData->scsiTask	= NULL;
			
			reserved->fCommandPool->returnCommand ( orb );
				
			CommandCompleted ( scsiTask, serviceResponse, taskStatus );
			
		}
		
	}
	
}

//-----------------------------------------------------------------------------
//	¥ AbortSCSICommand - Aborts an outstanding I/O.					[PROTECTED]
//-----------------------------------------------------------------------------

SCSIServiceResponse
	IOFireWireSerialBusProtocolTransport::AbortSCSICommand (
		SCSITaskIdentifier request )
{

	DEBUG_UNUSED ( request );
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_FUNCTION_REJECTED;
	
	STATUS_LOG ( ( "%s: AbortSCSICommand called\n", getName () ) );
	
	return serviceResponse;
	
}

//-----------------------------------------------------------------------------
//	¥ IsProtocolServiceSupported -	Checks for valid protocol services
//									supported by this device.		[PROTECTED]
//-----------------------------------------------------------------------------

bool
	IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported (
		SCSIProtocolFeature	feature,
		void *				serviceValue )
{
	
	bool	isSupported = false;
	
	STATUS_LOG ( ( "IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported called\n" ) );
	
	switch ( feature )
	{
		
		case kSCSIProtocolFeature_CPUInDiskMode:
			
			isSupported = IsDeviceCPUInDiskMode ();
			
			break;
		
		case kSCSIProtocolFeature_MaximumReadBlockTransferCount:
		case kSCSIProtocolFeature_MaximumWriteBlockTransferCount:
			
			*( UInt32 * ) serviceValue = kDefaultIOBlockCount;
			isSupported = true;
			
			break;
			
		case kSCSIProtocolFeature_MaximumReadTransferByteCount:
			
			OSNumber *		valuePointer;
			
			// If the property SBP2ReceiveBufferByteCount exists we have a FireWire host
			// with the physical unit off and there is a software FIFO. ( i.e. Lynx )
			
			// We should tell clients to deblock on the SBP2ReceiveBufferByteCount.
			// bounds to avoid stalled I/O.
			
			valuePointer = OSDynamicCast ( OSNumber, getProperty ( kSBP2ReceiveBufferByteCountKey, gIOServicePlane ) );
			if ( valuePointer != NULL )
			{
				
				*( UInt64 * ) serviceValue = valuePointer->unsigned32BitValue ();
				isSupported = true;			
				
			}
			
			break;
			
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
			
			*( UInt32 * ) serviceValue = kMaxFireWireLUN;
			isSupported = true;
			
			break;
			
		default:
			break;
		
	}
	
	return isSupported;
	
}


//-----------------------------------------------------------------------------
//	¥ HandleProtocolServiceFeature - Handles protocol service features.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

bool
	IOFireWireSerialBusProtocolTransport::HandleProtocolServiceFeature (
		SCSIProtocolFeature	feature, 
		void *				serviceValue )
{
	
	DEBUG_UNUSED ( feature );
	DEBUG_UNUSED ( serviceValue );
	
	return false;
	
}


//-----------------------------------------------------------------------------
//	¥ IsDeviceCPUInDiskMode - 	Checks if the device is a CPU in
//								FireWire Target Disk Mode.			[PROTECTED]
//-----------------------------------------------------------------------------

bool IOFireWireSerialBusProtocolTransport::IsDeviceCPUInDiskMode ( void )
{
	
	UInt32						csrModelInfo 	= 0;
	IOConfigDirectory *			directory		= NULL;
	IOFireWireDevice *			device			= NULL;
	IOReturn					status			= kIOReturnSuccess;
	IOService *					providerService = fUnit->getProvider ();
	bool						isCPUDiskMode	= false;
	
	STATUS_LOG ( ( "%s: IsDeviceCPUInDiskMode was called\n", getName () ) );
	
	if ( providerService == NULL )
	{
		goto exit;
	}
	
	device = OSDynamicCast ( IOFireWireDevice, providerService );
	if ( device == NULL )
	{
		goto exit;
	}
	
	status = device->getConfigDirectory ( directory );	
	if ( status == kIOReturnSuccess )
	{
		
		status = directory->getKeyValue ( kCSRModelInfoKey, csrModelInfo );
		
		if ( status == kIOReturnSuccess )
		{
			
			if ( ( csrModelInfo & kCRSModelInfo_ValidBitsMask ) == kCRSModelInfo_TargetDiskMode )
			{
				isCPUDiskMode = true;
			}
			
		}
		
	}
	
	
exit:
	
	
	STATUS_LOG ( ( "%s: CPU Disk Mode = %d\n", getName (), isCPUDiskMode ) );
	
	return isCPUDiskMode;
	
}


//-----------------------------------------------------------------------------
//	¥ StatusNotifyStatic - 	C->C++ glue method.						[PROTECTED]
//-----------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::StatusNotifyStatic (
		void *					refCon,
		FWSBP2NotifyParams *	params )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->StatusNotify ( params );
	
}

//-----------------------------------------------------------------------------
//	¥ StatusNotify - Status notify handler.							[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::StatusNotify (
		FWSBP2NotifyParams * params )
{

	IOFireWireSBP2ORB *		orb				= NULL;
	FWSBP2StatusBlock *		statusBlock 	= NULL;
	SBP2ClientOrbData *		clientData		= NULL;
	SCSI_Sense_Data *		targetData		= NULL;
	UInt8			 		senseData[kSenseDefaultSize + 8] = { 0 };
	
	targetData = ( SCSI_Sense_Data * ) &senseData[0];
	
	if ( ( params->message != NULL ) && ( params->length != 0 ) )
	{
		
		orb			= ( IOFireWireSBP2ORB * ) params->commandObject;
		statusBlock = ( FWSBP2StatusBlock * ) params->message;
		
		if ( orb != NULL )
		{
			
			clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
			
		}
		
	}
	
	switch ( params->notificationEvent )
	{
		
		case kFWSBP2NormalCommandStatus:
			
			//	Read the status block detail bits see SBP-2 spec section 5.3
			//	check the dead bit ( is 'd' field == 1 ).
			
			if ( ( clientData != NULL ) && ( statusBlock->details & 0x08 ) ) 	
			{
				
				SetValidAutoSenseData ( clientData, statusBlock, targetData );
				
				//	Wait for fetch agent to reset before calling CompleteSCSITask
				//	which will be called in FetchAgentResetComplete.
				
				fLogin->submitFetchAgentReset ();
				
			}
			else if ( clientData &&
					( ( statusBlock->details & 0x30 ) == 0 ) &&		// ( is 'resp' field == 0 )
					( ( statusBlock->details & 0x07 ) == 1 ) && 	// ( is 'len' field == 1 )
					( statusBlock->sbpStatus == 0 ) )				// ( is 'sbp_status' field == 0 )
			{
				
				clientData->serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
				clientData->taskStatus		= kSCSITaskStatus_GOOD;
				
				CompleteSCSITask ( orb );
				
				STATUS_LOG ( ( "%s: StatusNotify normal complete \n", getName () ) );
				
			}
			else if ( clientData )
			{
				
				SetValidAutoSenseData ( clientData, statusBlock, targetData );
				
				CompleteSCSITask ( orb );
				
				STATUS_LOG ( ( "%s: StatusNotify unexpected error? \n", getName () ) );
				
			}
			
			break;

		case kFWSBP2NormalCommandTimeout:
			STATUS_LOG ( ( "%s: kFWSBP2NormalCommandTimeout \n", getName () ) );

			if ( clientData )
			{
				
				if ( clientData->scsiTask )
				{
					
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( orb );
					
				}
				
			}
			
			// reset LUN as good measure in case device is wedged
			fLUNResetORB->submit ();
			
			break;
			
		case kFWSBP2NormalCommandReset:
			STATUS_LOG ( ( "%s: kFWSBP2NormalCommandReset\n", getName () ) );
			
			//	kFWSBP2NormalCommandReset - is a misleading definition
			//	A pending command has failed so we need notify
			//	the upper layers to complete failed command.
			
			if ( clientData != NULL )
			{
				
				if ( clientData->scsiTask != NULL )
				{
					
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( orb );
					
				}
				
			}
			
			break;
			
		default:
			STATUS_LOG ( ( "%s: StatusNotify with unknown notificationEvent\n", getName () ) );
			break;
		
	}
	
}

//-----------------------------------------------------------------------------
//	¥ SetValidAutoSenseData - Sets any valid sense data.			[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::SetValidAutoSenseData (
		SBP2ClientOrbData *	clientData,
		FWSBP2StatusBlock *	statusBlock,
		SCSI_Sense_Data *	targetData )
{
	
	UInt8		quadletCount = 0;
	
	clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	clientData->taskStatus		= kSCSITaskStatus_No_Status;

	quadletCount = ( statusBlock->details & 0x07 ) - 1 ;
	
	// see if we have any valid sense data
	if ( ( statusBlock->details & 0x30 ) == 0 )
	{
		
		clientData->serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
		clientData->taskStatus		= CoalesceSenseData ( statusBlock, quadletCount, targetData );
		
	}
	
	if ( clientData->taskStatus == kSCSITaskStatus_CHECK_CONDITION )
	{
		
		if ( clientData->scsiTask != NULL )
		{
			
			SetAutoSenseData ( clientData->scsiTask, targetData, kSenseDefaultSize + 8 );
 			
		}
		
	}
	
}

//-----------------------------------------------------------------------------
//	¥ CoalesceSenseData - Sets sense data in the data buffer.		[PROTECTED]
//-----------------------------------------------------------------------------

SCSITaskStatus
	IOFireWireSerialBusProtocolTransport::CoalesceSenseData (
		FWSBP2StatusBlock *	sourceData,
		UInt8				quadletCount,
		SCSI_Sense_Data *	targetData )
{
	
	SCSITaskStatus	returnValue			= kSCSITaskStatus_GOOD;
	UInt8 			statusBlockFormat	= 0;
	
	// Pull bits out of SBP-2 status block ( see SBP-2 Annex B section B.2 )
	// and copy them into sense data block ( see SPC-2 section 7.23.2 )
		
	if ( quadletCount > 0 )
	{
		
		statusBlockFormat = ( sourceData->status[0] >> 30 ) & 0x03;
		returnValue = ( SCSITaskStatus ) ( ( sourceData->status[0] >> 24 ) & 0x3F );
		
		if ( statusBlockFormat == 0 ) 
		{
			
			targetData->VALID_RESPONSE_CODE = kSENSE_RESPONSE_CODE_Current_Errors;
			
		}
		else if ( statusBlockFormat == 1 )
		{
			
			targetData->VALID_RESPONSE_CODE = kSENSE_RESPONSE_CODE_Deferred_Errors;
			
		}
		
		if ( statusBlockFormat < 2 )
		{
			
			targetData->VALID_RESPONSE_CODE |= ( sourceData->status[0] >> 16 ) & 0x80;
			targetData->ADDITIONAL_SENSE_CODE = ( sourceData->status[0] >> 8 ) & 0xFF;
			targetData->ADDITIONAL_SENSE_CODE_QUALIFIER = sourceData->status[0] & 0xFF;
			targetData->SENSE_KEY = ( sourceData->status[0] >> 16 ) & 0x0F;
			
			// Set the M, E, I bits: M->FileMark, E->EOM, I->ILI
			targetData->SENSE_KEY |= ( ( sourceData->status[0] >> 16 ) & 0x70 ) << 1;
			
			if ( quadletCount > 1 )
			{
				
				targetData->INFORMATION_1 = ( sourceData->status[1] >> 24 ) & 0xFF;
				targetData->INFORMATION_2 = ( sourceData->status[1] >> 16 ) & 0xFF;
				targetData->INFORMATION_3 = ( sourceData->status[1] >> 8 ) & 0xFF;
				targetData->INFORMATION_4 = sourceData->status[1] & 0xFF;
				targetData->ADDITIONAL_SENSE_LENGTH = 6;
				
			}
			
			if ( quadletCount > 2 )
			{
				
				targetData->COMMAND_SPECIFIC_INFORMATION_1 = ( sourceData->status[2] >> 24 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_2 = ( sourceData->status[2] >> 16 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_3 = ( sourceData->status[2] >> 8 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_4 = sourceData->status[2] & 0xFF;
				targetData->ADDITIONAL_SENSE_LENGTH = 6;
				
			}
			
			if ( quadletCount > 3 )
			{
				
				UInt8	count;
				
				// Get bytes to copy and clip if greater than sizeof SCSI_Sense_Data
				
				count = ( quadletCount - 3 ) * sizeof ( UInt32 );
				if ( count > 4 ) count = 4;
								
				bcopy ( &sourceData->status[3],
						&targetData->FIELD_REPLACEABLE_UNIT_CODE,
						count );
				
				targetData->ADDITIONAL_SENSE_LENGTH = count + 6;
				
			}
			
		}
		
	}
	
	return returnValue;
	
}

//-----------------------------------------------------------------------------
//	¥ LoginCompletionStatic - 	C->C++ glue method.					[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::LoginCompletionStatic (
		void *						refCon,
		FWSBP2LoginCompleteParams *	params )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LoginCompletion ( params );
	
}

//-----------------------------------------------------------------------------
//	¥ LoginCompletion - Login completion handler.					[PROTECTED]
//-----------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::LoginCompletion ( 
		FWSBP2LoginCompleteParams * params )
{
	
	SBP2ClientOrbData *		clientData = NULL;
	
	STATUS_LOG ( ( "%s: LoginCompletion complete \n", getName () ) );
	
	if	( ( params->status == kIOReturnSuccess ) &&				// ( kIOReturnSuccess )
		( ( params->statusBlock->details & 0x30 ) == 0 ) &&		// ( is 'resp' field == 0 )
		  ( params->statusBlock->sbpStatus == 0 ) )				// ( is 'sbp_status' field == 0 )
	{
		
		fLoginRetryCount	= 0;
		fLoggedIn			= true;
		fNeedLogin 			= false;
		
		if ( reserved->fLoginState == kFirstTimeLoggingInState )
		{
			
			reserved->fLoginState = kLogginSucceededState;
			fCommandGate->commandWakeup ( ( void * ) &reserved->fLoginState );
			
		}
		
		clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
		if ( clientData != NULL )
		{
			
			if ( clientData->scsiTask )
			{
				
				fLogin->submitORB ( fORB );
				
			}
			
		}
		
		loginResumed ();
		
	}
	
	else
	{
		
		if ( fLoginRetryCount < kMaxLoginRetryCount )
		{
			
			fLoginRetryCount++;
			STATUS_LOG ( ( "%s: resubmitting Login\n", getName () ) );
			
			IOReturn status = login ();
			if ( status != kIOReturnSuccess )
			{
				if ( reserved->fLoginState == kFirstTimeLoggingInState )
				{
					
					reserved->fLoginState = kLogginFailedState;
					
					// wake up sleeping start thread
					fCommandGate->commandWakeup ( ( void * ) &reserved->fLoginState );
					
				}
			}
		}
		else
		{
			//	Device can not be logged into after kMaxLoginRetryCount
			//	attemptes let's reset the need login flag in case
			//	the device was unplugged during login
			
			fNeedLogin = true;
	//		fLoggedIn = false;
			loginLost ();

			if ( reserved->fLoginState == kFirstTimeLoggingInState )
			{
				
				reserved->fLoginState = kLogginFailedState;
				
				// wake up sleeping start thread
				fCommandGate->commandWakeup ( ( void * ) &reserved->fLoginState );
				
			}
		}
		
	}
	
}

//-----------------------------------------------------------------------------
//	¥ LogoutCompletionStatic - C->C++ glue.							[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::LogoutCompletionStatic (
		void *							refCon,
		FWSBP2LogoutCompleteParams *	params )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LogoutCompletion ( params );
	
}

//-----------------------------------------------------------------------------
//	¥ LogoutCompletion - Logout completion handler.					[PROTECTED]
//-----------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::LogoutCompletion ( 
		FWSBP2LogoutCompleteParams * params )
{

	DEBUG_UNUSED ( params );

	STATUS_LOG ( ( "%s: LogoutCompletion complete \n", getName () ) );
	
}


//-----------------------------------------------------------------------------
//	¥ UnsolicitedStatusNotifyStatic - C->C++ glue.					[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotifyStatic (
		void * refCon,
		FWSBP2NotifyParams * params )
{
		
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->UnsolicitedStatusNotify ( params );
	
}

//-----------------------------------------------------------------------------
//	¥ UnsolicitedStatusNotify - Unsolicited status handler.			[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotify (
		FWSBP2NotifyParams * params )
{
	
	DEBUG_UNUSED ( params );
	
	STATUS_LOG ( ( "%s: UnsolicitedStatusNotify called\n", getName () ) );
	
	// Parse and handle unsolicited status
	
	fLogin->enableUnsolicitedStatus ();
	
}

//-----------------------------------------------------------------------------
//	¥ FetchAgentResetCompleteStatic - C->C++ glue.					[PROTECTED]
//-----------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::FetchAgentResetCompleteStatic (
		void *		refCon,
		IOReturn	status )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->FetchAgentResetComplete ( status );
	
}

//-----------------------------------------------------------------------------
//	¥ FetchAgentResetComplete - Fetch agent reset handler.			[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::FetchAgentResetComplete ( 
		IOReturn status )
{

	DEBUG_UNUSED ( status );
	
	SBP2ClientOrbData *		clientData = NULL;
	
	STATUS_LOG ( ( "%s: FetchAgentResetComplete called\n", getName () ) );
	
	// When orb chaining is implemented we will notify upper layer
	// to reconfigure device state and resubmitting commands
	
	clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
	if ( clientData != NULL )
	{
		
		if ( clientData->scsiTask != NULL )
		{
			CompleteSCSITask ( fORB );
		}
		
	}
	
}

//-----------------------------------------------------------------------------
//	¥ LunResetCompleteStatic - C->C++ glue.							[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::LunResetCompleteStatic (
		void *							refCon,
		IOReturn						status,
		IOFireWireSBP2ManagementORB *	orb )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LunResetComplete ( status, orb );
	
}

//-----------------------------------------------------------------------------
//	¥ LunResetComplete - LUN reset completion handler.				[PROTECTED]
//-----------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::LunResetComplete (
		IOReturn						status,
		IOFireWireSBP2ManagementORB *	orb )
{

	DEBUG_UNUSED ( status );
	DEBUG_UNUSED ( orb );

	STATUS_LOG ( ( "%s: LunResetComplete called\n", getName () ) );
	
	fLogin->submitFetchAgentReset ();
	
}

//-----------------------------------------------------------------------------
//	¥ ConnectToDeviceStatic - C->C++ glue.							[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn 
	IOFireWireSerialBusProtocolTransport::ConnectToDeviceStatic (
		OSObject *	refCon, 
		void *		val1,
		void *		val2,
		void *		val3,
		void *		val4 )
{
	
	DEBUG_UNUSED ( val1 );
	DEBUG_UNUSED ( val2 );
	DEBUG_UNUSED ( val3 );
	DEBUG_UNUSED ( val4 );
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->ConnectToDevice ();
	
	return kIOReturnSuccess;
	
}

//-----------------------------------------------------------------------------
//	¥ ConnectToDevice - Connects to the device.						[PROTECTED]
//-----------------------------------------------------------------------------


void IOFireWireSerialBusProtocolTransport::ConnectToDevice ( void )
{
	
	STATUS_LOG ( ( "%s: ConnectToDevice called\n", getName () ) );
	
	IOReturn status;
	
	// avoid double logins during login phase
	fNeedLogin = false;
	fLoginRetryCount = 0;
	
	status = login ();
	if ( status == kIOReturnSuccess )
	{
		// sleep the start thread - we'll wake it up on login completion
		fCommandGate->commandSleep ( ( void * ) &reserved->fLoginState , THREAD_UNINT );
		
	}
}

//-----------------------------------------------------------------------------
//	¥ DisconnectFromDevice - Disconnects from device.				[PROTECTED]
//-----------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::DisconnectFromDevice ( void )
{
	
	STATUS_LOG ( ( "%s: DisconnectFromDevice called\n", getName () ) );
	
	fLoggedIn = false;
	
	// avoid logins during a logout phase
	fNeedLogin = false;
	
	fLogin->submitLogout ();
	
}

//-----------------------------------------------------------------------------
//	¥ CriticalOrbSubmissionStatic - C->C++ glue.					[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
	IOFireWireSerialBusProtocolTransport::CriticalOrbSubmissionStatic ( 
		OSObject *	refCon, 
		void *		val1,
		void *		val2,
		void *		val3,
		void *		val4 )
{

	DEBUG_UNUSED ( val3 );
	DEBUG_UNUSED ( val4 );
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->CriticalOrbSubmission ( ( IOFireWireSBP2ORB * ) val1, 
																				   ( SCSITaskIdentifier  ) val2 );
	
	return kIOReturnSuccess;
	
}

//-----------------------------------------------------------------------------
//	¥ CriticalOrbSubmission - submitORB on workloop.				[PROTECTED]
//-----------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::CriticalOrbSubmission (
		IOFireWireSBP2ORB *	orb,
		SCSITaskIdentifier	request )
{
	
	SBP2ClientOrbData *		clientData;

	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	require ( clientData, exit );
	
	clientData->scsiTask = request;
	
	if ( fLoggedIn )
	{
		fLogin->submitORB ( orb );
	}

exit:
	
	return;
	
}

//-----------------------------------------------------------------------------
//	¥ AllocateResources - Allocates resources.						[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn IOFireWireSerialBusProtocolTransport::AllocateResources ( void )
{
	
	IOReturn 				status;
	SBP2ClientOrbData *		clientData	= NULL;
	IOWorkLoop * 			workLoop	= NULL;
	
	STATUS_LOG ( ( "%s: AllocateResources called\n", getName () ) );
	
	fLogin = fSBPTarget->createLogin ();
	require_action ( fLogin, exit,  status = kIOReturnNoMemory );
	
	fORB = fLogin->createORB ();
	require_action ( fORB, exit, status = kIOReturnNoMemory );
	
	clientData = ( SBP2ClientOrbData * ) IOMalloc ( sizeof ( SBP2ClientOrbData ) );
	require_action ( clientData, exit, status = kIOReturnNoMemory );
	
	bzero ( clientData, sizeof ( SBP2ClientOrbData ) );
	clientData->orb	= fORB;
	fORB->setRefCon ( ( void * ) clientData );
	
	fLogin->setLoginFlags ( kFWSBP2ExclusiveLogin );
	fLogin->setLoginRetryCountAndDelayTime ( 32, 1000000 );
	fLogin->setMaxPayloadSize ( kMaxFireWirePayload );
	fLogin->setStatusNotifyProc ( this, StatusNotifyStatic );
	fLogin->setUnsolicitedStatusNotifyProc ( this, UnsolicitedStatusNotifyStatic );
	fLogin->setLoginCompletion ( this, LoginCompletionStatic );
	fLogin->setLogoutCompletion ( this, LogoutCompletionStatic );
	fLogin->setFetchAgentResetCompletion ( this, FetchAgentResetCompleteStatic );
	
	// set BUSY_TIMEOUT register value see SBP-2 spec section 6.2
	// also see IEEE Std 1394-1995 section 8.3.2.3.5 ( no I am not kidding )
	
	fLogin->setBusyTimeoutRegisterValue ( kDefaultBusyTimeoutValue );
	
	fLUNResetORB = fSBPTarget->createManagementORB ( this, LunResetCompleteStatic );
	require_action ( fLUNResetORB, exit, status = kIOReturnNoMemory );
	
	fLUNResetORB->setCommandFunction ( kFWSBP2LogicalUnitReset );
	fLUNResetORB->setManageeCommand ( fLogin );
	
	// allocate expansion data
	
	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	require_action ( reserved, exit, status = kIOReturnNoMemory );
	bzero ( reserved, sizeof ( ExpansionData ) );
	
	reserved->fLoginState = kFirstTimeLoggingInState;
	
	workLoop = getWorkLoop ();
	require_action ( workLoop, exit, status = kIOReturnNoMemory );
	
	reserved->fCommandPool = IOCommandPool::withWorkLoop ( workLoop );
	require_action ( reserved->fCommandPool, exit, status = kIOReturnNoMemory );
	
	// enqueue the command in the free list
	
	reserved->fCommandPool->returnCommand ( fORB );
	
	status = kIOReturnSuccess;
	
exit:
	
	return status;
	
}

//-----------------------------------------------------------------------------
//	¥ DeallocateResources - Deallocates resources.					[PROTECTED]
//-----------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::DeallocateResources ( void )
{
	
	SBP2ClientOrbData *		clientData = NULL;
	
	STATUS_LOG ( ( "%s: DeallocateResources called\n", getName () ) );
	
	// /!\ WARNING - always release orb's before logins
	
	if ( fLUNResetORB != NULL )
	{
		
		fLUNResetORB->release ();
		fLUNResetORB = NULL;
		
	}
	
	if ( fORB != NULL )
	{
		
		clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
		if ( clientData != NULL )
		{
			
			IOFree ( clientData, sizeof ( SBP2ClientOrbData ) );
			
		}
		
		fORB->release ();
		fORB = NULL;
		
	}
	
	if ( fLogin != NULL )
	{
		
		fLogin->release ();
		fLogin = NULL;
		
	}
	
}

//-----------------------------------------------------------------------------
//	¥ login - .					[PROTECTED]
//-----------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  1 );

IOReturn IOFireWireSerialBusProtocolTransport::login ( void )
{
	
	STATUS_LOG ( ( "%s: submitting login.\n", getName () ) );
	
	IOReturn		status;
	
	fNeedLogin = false;
	fLoggedIn = false;
	status = kIOReturnSuccess;
	
	for ( ; fLoginRetryCount < kMaxLoginRetryCount; ++fLoginRetryCount )
	{
		
		status = submitLogin ();
		if ( status == kIOReturnSuccess )
		{
			
			break;
			
		}
		
	}
	
	if ( status != kIOReturnSuccess )
	{
		
		fNeedLogin = true;
		fLoggedIn = false;
		loginLost ();
		
	}
	
	return status;
	
}

//-----------------------------------------------------------------------------
//	¥ submitLogin - submitLogin	  bottleneck for subclass			[PROTECTED]
//-----------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  2 );

IOReturn IOFireWireSerialBusProtocolTransport::submitLogin ( void )
{
	
	STATUS_LOG ( ( "%s: submitting login.\n", getName () ) );
	
	IOReturn status = kIOReturnSuccess;
	
	status = fLogin->submitLogin ();
	
	return status;
	
}

//-----------------------------------------------------------------------------
//	¥ loginLost - login lost bottleneck for subclass				[PROTECTED]
//-----------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  3 );

void IOFireWireSerialBusProtocolTransport::loginLost ( void )
{
	
	STATUS_LOG ( ( "%s: login lost.\n", getName () ) );
	
}

//-----------------------------------------------------------------------------
//	¥ loginSuspended - login resumed bottleneck for subclass		[PROTECTED]
//-----------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  4 );

void IOFireWireSerialBusProtocolTransport::loginSuspended ( void )
{
	
	STATUS_LOG ( ( "%s: login suspended.\n", getName () ) );
	
	// a successful reconnect orb is required.
	
}

//-----------------------------------------------------------------------------
//	¥ loginResumed - login resumed bottleneck for subclass			[PROTECTED]
//-----------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  5 );

void IOFireWireSerialBusProtocolTransport::loginResumed ( void )
{
	
	STATUS_LOG ( ( "%s: login resumed.\n", getName () ) );
	
	// a reconnect orb has succeeded.
	
}

//-----------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark == VTable Padding ==
#pragma mark -
#endif

// binary compatibility reserved method space

OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  6 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  7 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  8 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  9 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 10 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 11 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 12 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 13 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 14 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 15 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 16 );

//-----------------------------------------------------------------------------

