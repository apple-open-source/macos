/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#include "IOFireWireSerialBusProtocolTransport.h"
#include <IOKit/firewire/IOConfigDirectory.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/IOKitKeys.h>

#if 0
#pragma mark == Macros ==
#endif

//--------------------------------------------------------------------------------------------------
//	Macros
//--------------------------------------------------------------------------------------------------

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
#define DLOG(x)		IOLog x
#else
#define DLOG(x)
#endif

#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors (	IOFireWireSerialBusProtocolTransport, IOSCSIProtocolServices )

// Uncomment _TRANSPORT_FAILURE_RETRIES_ to enable tranport level retires for protocol timeouts.
// #define _TRANSPORT_FAILURE_RETRIES_			true

#if 0
#pragma mark == Constants ==
#endif

//--------------------------------------------------------------------------------------------------
//	Constants
//--------------------------------------------------------------------------------------------------

#define kPreferredNameKey								"Preferred Name"
#define kFireWireGUIDKey								"GUID"
#define kFireWireVendorNameKey							"FireWire Vendor Name"
#define kSBP2ReceiveBufferByteCountKey					"SBP2ReceiveBufferByteCount"
#define kDefaultIOBlockCount							256
#define kCRSModelInfo_ValidBitsMask						0x00FFFFFF
#define kCRSModelInfo_TargetDiskMode					0x0054444D
#define kIOFireWireMessageServiceIsRequestingClose		kIOFWMessageServiceIsRequestingClose
#define kDefaultTimeOutValue							30000
#define kCommandPoolOrbCount							1
#define kFWSBP2DefaultPageTableEntriesCount				512
#define kDoubleBufferCommandSizeCheckThreshold			512

enum 
{
	kFireWireSBP2CommandTransferDataToTarget = 0L,
	kFireWireSBP2CommandTransferDataFromTarget = kFWSBP2CommandTransferDataFromTarget
};

#if 0
#pragma mark == Static Debug Assertion Method ==
#endif

//--------------------------------------------------------------------------------------------------
//	IOFireWireSerialBusProtocolTransportDebugAssert										[STATIC]
//--------------------------------------------------------------------------------------------------

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
	
	if ( ( long ) errorCode != 0 ) { IOLog ( "error: %ld ( 0x%08lx )",
											 ( long ) errorCode,
											 ( long ) errorCode  ); }
	
	IOLog ( "\n" );
}

#endif

#if 0
#pragma mark == Public Methods ==
#endif

//--------------------------------------------------------------------------------------------------
// init - Called by IOKit to initialize us.												[PUBLIC]
//--------------------------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------------------------
// start - Called by IOKit to start our services.										[PUBLIC]
//--------------------------------------------------------------------------------------------------

bool IOFireWireSerialBusProtocolTransport::start ( IOService * provider )
{
	
	IOReturn			status;
	Boolean				returnValue;
	Boolean				openSucceeded;
	OSNumber *			rto;
	OSNumber *			wto;
	OSDictionary *		dict;
	bool				sucess;

	status			= kIOReturnSuccess;
	returnValue		= false;
	openSucceeded	= false;
	rto				= NULL;
	wto				= NULL;
	sucess			= false;
	dict			= NULL;
	
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

	DLOG ( ( "%s: start read time out = %ld\n", getName (), rto->unsigned32BitValue ( ) ) );
	
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

	DLOG ( ( "%s: start read time out = %ld\n", getName (), wto->unsigned32BitValue ( ) ) );

	// Set the default maximum page table constraints for SBP2 
	// $$$ ( this should probably be moved down to SBP2 - ping Collin )
	
	setProperty ( kIOMaximumSegmentCountReadKey, kFWSBP2DefaultPageTableEntriesCount, 32 );
	setProperty ( kIOMaximumSegmentCountWriteKey, kFWSBP2DefaultPageTableEntriesCount, 32 );
	
	setProperty ( kIOMaximumSegmentByteCountReadKey, kFWSBP2MaxPageClusterSize, 32 );
	setProperty ( kIOMaximumSegmentByteCountWriteKey, kFWSBP2MaxPageClusterSize, 32 );
		
	fSBPTarget = OSDynamicCast ( IOFireWireSBP2LUN, provider );
	require ( fSBPTarget, exit );
	
	// Add a retain here so we can keep IOFireWireSBP2LUN from doing garbage collection on us
	// when we are in the middle of our finalize method.
	
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
	
	DLOG ( ( "%s: start complete\n", getName () ) );
	
	returnValue = true;
	
	// Copy some values to the Protocol Characteristics Dictionary.
	
	dict = OSDynamicCast ( OSDictionary, getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( dict != NULL )
	{
		OSDictionary *		protocolDict;
		OSString *			string;
		
		protocolDict 	= NULL;
		string 			= NULL;
		
		// Make a copy of the existing Protocol Characteristics Dictionary to Modify.
		
		protocolDict = OSDictionary::withDictionary ( dict );
		check ( protocolDict );
		
		string = OSString::withCString ( kFireWireGUIDKey );
		if ( string != NULL )
		{
			protocolDict->setObject ( string,
									  getProperty ( kFireWireGUIDKey,
									  gIOServicePlane ) );
			string->release ( );
		}
		
		string = OSString::withCString ( kPreferredNameKey );
		if ( string != NULL )
		{
			protocolDict->setObject ( kPreferredNameKey,
							  getProperty ( kFireWireVendorNameKey,
							  gIOServicePlane ) );
			string->release ( );
		}

		// Replace the existing Protocol Characteristics Dictionary with our new one.
		
		setProperty ( kIOPropertyProtocolCharacteristicsKey, protocolDict );
		protocolDict->release ( );
	}
	
	InitializePowerManagement ( provider );
	
exit:
	
	if ( returnValue == false )
	{
		DLOG ( ( "%s: start failed.  status = %x\n", getName (), status) );
		
		// Call the cleanUp method to clean up any allocated resources.
		
		cleanUp ();
	}
	
	return returnValue;
}

//--------------------------------------------------------------------------------------------------
// cleanUp -																			  [PUBLIC]
// Note: This is misleadingly named due to binary compatibility burdens. The cleanUp method
// actually just closes the SBP2LUN. fSBPTarget is actually a SBP2LUN object. 
//--------------------------------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::cleanUp ( void )
{
	DLOG ( ( "%s: cleanUp called\n", getName () ) );
	
	if ( fSBPTarget != NULL )
	{
		// Close SBP2 if we have opened it.
		
		if ( fSBPTarget->isOpen ( this ) )
		{
			fSBPTarget->close ( this );
		}
	}
}

#if 0

//--------------------------------------------------------------------------------------------------
// close - attempt to log out of device.												[PROTECTED]
//--------------------------------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::close ( IOService * provider, IOOptionBits options )
{
	IOLog ( "**** DOES THIS EVEN GET CALLED AT SHUTDOWN OR RESTART? - TERM STATE = %d ****\n", isInactive () );
	
	if ( !isInactive () )
	{
		IOReturn		err;

		err = fLogin->submitLogout ();
	}
	
	super::close ( provider, options );
}

#endif

//--------------------------------------------------------------------------------------------------
// finalize - Terminates all power management.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
	IOFireWireSerialBusProtocolTransport::finalize (
		IOOptionBits options )
{
	DeallocateResources ( );
	
	// Release the retain we took to keep IOFireWireSBP2LUN from doing garbage collection on us
	// when we are in the middle of DeallocateResources.
		
	if ( fSBPTarget )
	{
		fSBPTarget->release ( );
		fSBPTarget = NULL;
	}
	
	return super::finalize ( options );
}

//--------------------------------------------------------------------------------------------------
// free - Called to deallocate ExpansionData.											[PUBLIC]
//--------------------------------------------------------------------------------------------------

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
#pragma mark == Protected Methods ==
#endif

//--------------------------------------------------------------------------------------------------
// CommandORBAccessor - Retrieves command orb.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

IOFireWireSBP2ORB * 
	IOFireWireSerialBusProtocolTransport::CommandORBAccessor ( 
		void )
{
	return fORB;
}

//--------------------------------------------------------------------------------------------------
// SBP2LoginAccessor - Retrieves login orb.												[PROTECTED]
//--------------------------------------------------------------------------------------------------

IOFireWireSBP2Login * 
	IOFireWireSerialBusProtocolTransport::SBP2LoginAccessor (
		void )
{
	return fLogin;
}

//--------------------------------------------------------------------------------------------------
// message - Called by IOKit to deliver messages.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn
	IOFireWireSerialBusProtocolTransport::message (
		UInt32 		type,
		IOService *	nub,
		void * 		arg )
{
	IOFireWireSBP2ORB *		orb;
	SBP2ClientOrbData *		clientData;
	IOReturn				status;
	
	orb 		= NULL;
	clientData 	= NULL;
	status		= kIOReturnSuccess;
		
	switch ( type )
	{
		case kIOMessageServiceIsSuspended:
			
			DLOG ( ( "%s: kIOMessageServiceIsSuspended\n", getName () ) );
			
			// Bus reset started - set flag to stop submitting orbs.
			
			fLoggedIn = false;
			
			break;
		
		case kIOMessageServiceIsResumed:
			
			DLOG ( ( "%s: kIOMessageServiceIsResumed\n", getName () ) );
			
			// Bus reset finished - if we have failed to log in previously, try again.
			
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
			// message. So qualify this message with our instance variable fLogin and ignore others.
			
			if( ( ( FWSBP2ReconnectParams* ) arg )->login == fLogin )
			{
				DLOG ( ( "%s: kIOMessageFWSBP2ReconnectComplete\n", getName () ) );

				fLoggedIn = true;

				if ( fReconnectCount < kMaxReconnectCount)
				{
					DLOG ( ( "%s: resubmit orb \n", getName () ) );
					fReconnectCount++;
					submitOrbFromQueue ();
				}
				else
				{
					// Unable to recover from bus reset storm. We have exhausted the
					// fReconnectCount - punt...
					
					if ( fORB )
					{
						clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
						if ( clientData != NULL )
						{	
							clientData->serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
							clientData->taskStatus		= kSCSITaskStatus_DeliveryFailure;
							CompleteSCSITask ( fORB );
						}
					}
				}
			}
			
			break;
			
		case kIOMessageFWSBP2ReconnectFailed:

			// As of this writing FireWireSBP2LUN will message all multi-LUN instances with this
			// message. So qualify this message with our instance variable fLogin and ignore others.
			
			if( ( ( FWSBP2ReconnectParams* ) arg )->login == fLogin )
			{
				DLOG ( ( "%s: kIOMessageFWSBP2ReconnectFailed\n", getName () ) );
				
				// Try to reestablish log in.
				
				fLoginRetryCount = 0;
				login ();
			}
			
			break;

		case kIOFireWireMessageServiceIsRequestingClose:
			DLOG ( ( "%s: kIOFireWireMessageServiceIsRequestingClose\n", getName () ) );
	
			// Tell our super to message it's clients that the device is gone.
			
			SendNotification_DeviceRemoved ();
			
			// We need to drain the queued commands. See if there is an in flight orb (e.g. fORB)
			// if not pull the first one out of the submit queue if there are any.
			
			orb = fORB;
			
			do {
				if ( orb )
				{
					clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
					if ( clientData )
					{
						if ( clientData->scsiTask)
						{
							clientData->serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
							clientData->taskStatus		= kSCSITaskStatus_DeviceNotPresent;
							CompleteSCSITask ( orb );
						}
					}
				}
			} while ( ( orb = ( IOFireWireSBP2ORB * ) reserved->fSubmitQueue->getCommand ( false ) ) );
			
			cleanUp ();
			
			break;
		
		case kIOMessageServiceIsTerminated:
			
			DLOG ( ( "%s: kIOMessageServiceIsTerminated\n", getName () ) );
			
			// Let go of memory and what not.
			
			cleanUp ();
			
			break;
			
		default:
			
			status = IOService::message (type, nub, arg);
			
			break;
	}
	
	return status;
}

//--------------------------------------------------------------------------------------------------
// SendSCSICommand - Converts a SCSITask to an ORB.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
	IOFireWireSerialBusProtocolTransport::SendSCSICommand (
		SCSITaskIdentifier 		request,
		SCSIServiceResponse * 	serviceResponse,
		SCSITaskStatus * 		taskStatus )
{
	SBP2ClientOrbData *				clientData;
	IOFireWireSBP2ORB *				orb;
	SCSICommandDescriptorBlock		cdb = { 0 };
	UInt8							commandLength;
	UInt32							commandFlags;
	UInt32							timeOut;
	bool							commandProcessed;
	
	DLOG ( ( "%s: SendSCSICommand called\n", getName () ) );

	clientData 				= NULL;
	orb						= NULL;
	commandLength			= 0;
	commandFlags			= 0;
	timeOut					= 0;
	commandProcessed		= true;
	
	*serviceResponse		= kSCSIServiceResponse_Request_In_Process;
	*taskStatus				= kSCSITaskStatus_No_Status;
	
	if ( isInactive () )
	{
		// Device is disconnected - we can not service command requests.
		
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		commandProcessed = true;
		goto exit;
	}
	
	// Get an orb from our orb pool and do not block until we get one.
	
	orb = ( IOFireWireSBP2ORB * ) reserved->fCommandPool->getCommand ( false );
	if ( orb == NULL )
	{
		// We're busy - return false - command will be resent next time CommandComplete is called.
		
		commandProcessed = false;
		goto exit;
	}
        
	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	if ( clientData == NULL ) goto exit;
	
	GetCommandDescriptorBlock ( request, &cdb );
	
	commandLength = GetCommandDescriptorBlockSize ( request );
	
#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3 )
	if ( commandLength == kSCSICDBSize_6Byte )
	{
		
		DLOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0], cdb[1],
					 cdb[2], cdb[3], cdb[4], cdb[5] ) );
		
	}
	else if ( commandLength == kSCSICDBSize_10Byte )
	{
		
		DLOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
					cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
					cdb[9] ) );
		
	}
	else if ( commandLength == kSCSICDBSize_12Byte )
	{
		
		DLOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
					cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
					cdb[9], cdb[10], cdb[11] ) );
		
	}
#endif // (FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3)
	
	commandFlags	= ( GetDataTransferDirection ( request ) == kSCSIDataTransfer_FromTargetToInitiator ) ? 
															  kFireWireSBP2CommandTransferDataFromTarget : 
															  kFireWireSBP2CommandTransferDataToTarget;
	
	orb->setCommandFlags (	commandFlags |
							kFWSBP2CommandCompleteNotify |
							kFWSBP2CommandImmediate |
							kFWSBP2CommandNormalORB );
	
	require ( ( SetCommandBuffers ( orb, request ) == kIOReturnSuccess ), exit );
	
	orb->setCommandBlock ( cdb, commandLength );
	
	// SBP-2 needs a non-zero timeout to fire completion routines if timeout is not expressed
	// default to 0xFFFFFFFF.
	
	timeOut = GetTimeoutDuration ( request );
	if ( timeOut == 0 )
	{
		timeOut = 0xFFFFFFFF;
	}
	
	orb->setCommandTimeout ( timeOut );

#ifdef _TRANSPORT_FAILURE_RETRIES_	
	reserved->fLUNResetCount = 3;
#endif

	// Close the gate here to eliminate potenially rare double append of orb. If on a DP machine
	// and a bus reset occurs the login thread can append the orb as well as here.
	
	fCommandGate->runAction ( CriticalOrbSubmissionStatic, orb, request );
	
exit:
	
	DLOG ( ( "%s: SendSCSICommand exit, Service Response = %x\n", getName (), *serviceResponse) );
	
	return commandProcessed;
}

//--------------------------------------------------------------------------------------------------
// SetCommandBuffers - Sets the command buffers in the ORB.						 	[PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn
	IOFireWireSerialBusProtocolTransport::SetCommandBuffers (
		IOFireWireSBP2ORB *	orb,
		SCSITaskIdentifier	request )
{
	SBP2ClientOrbData *		clientData;
	IOReturn				status;
	
	clientData 	= NULL;
	status 		= kIOReturnError;
	
	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	require ( clientData, Exit );
	
	clientData->quadletAlignedBuffer = NULL;
	if ( GetDataBuffer ( request ) != NULL )
	{
			
		// Does this command require double buffering in order to ensure quadlet alignment?
		if ( ( GetDataBuffer ( request )->getLength() < kDoubleBufferCommandSizeCheckThreshold ) &&
			( ( GetDataBuffer ( request )->getLength() & 3 ) != 0 ) )
		{
			// Create quadlet aligned IOBufferMemoryDescriptor, to be released in CompleteSCSITask().
			clientData->quadletAlignedBuffer = IOBufferMemoryDescriptor::withOptions ( 	kIODirectionOutIn,
																						GetDataBuffer ( request )->getLength(),
																						4 );
																						
			require ( clientData->quadletAlignedBuffer, Exit );
																				
			// If necessary copy data from the non-aligned buffer to the aligned buffer.
			if ( GetDataTransferDirection ( request ) == kSCSIDataTransfer_FromInitiatorToTarget )
			{
				GetDataBuffer ( request )->readBytes (	GetDataBufferOffset ( request ),
														clientData->quadletAlignedBuffer->getBytesNoCopy ( ),
														GetDataBuffer ( request )->getLength() );
			}	
			
			status = orb->setCommandBuffers (	clientData->quadletAlignedBuffer,
												GetDataBufferOffset ( request ),
												GetRequestedDataTransferCount ( request ) );
												
			require ( ( status == kIOReturnSuccess ), Exit );

		}
	}
	
	if ( clientData->quadletAlignedBuffer == NULL )
	{
		status = orb->setCommandBuffers (	GetDataBuffer ( request ),
											GetDataBufferOffset ( request ),
											GetRequestedDataTransferCount ( request ) );
	}
	
Exit:
	
	return status;
}

//--------------------------------------------------------------------------------------------------
// CompleteSCSITask - Completes a task.													[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::CompleteSCSITask (
		IOFireWireSBP2ORB * orb )
{
	SBP2ClientOrbData *	clientData = NULL;
	
	DLOG ( ( "%s: CompleteSCSITask called\n", getName () ) );

	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	if ( clientData != NULL )
	{
		if ( clientData->scsiTask != NULL )
		{
			SCSITaskIdentifier		scsiTask		= NULL;
			SCSIServiceResponse		serviceResponse;
			SCSITaskStatus			taskStatus;
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
			
			// Did we double buffer this command to ensure quadlet alignment?
			// If so, copy over memory if necessary.
			if ( clientData->quadletAlignedBuffer != NULL )
			{
				if ( GetDataTransferDirection ( clientData->scsiTask ) == kSCSIDataTransfer_FromTargetToInitiator )
				{	
					GetDataBuffer ( clientData->scsiTask )->writeBytes (	GetDataBufferOffset ( clientData->scsiTask ),
																			clientData->quadletAlignedBuffer->getBytesNoCopy ( ),
																			clientData->quadletAlignedBuffer->getLength() );
				}		
				
				clientData->quadletAlignedBuffer->release();
				clientData->quadletAlignedBuffer = NULL;
			}
			
			// Re-entrancy protection.
			
			scsiTask				= clientData->scsiTask;
			serviceResponse 		= clientData->serviceResponse;
			taskStatus				= clientData->taskStatus;
			clientData->scsiTask	= NULL;
			fORB					= NULL;
			
			submitOrbFromQueue ();
			
			reserved->fCommandPool->returnCommand ( orb );
			
			CommandCompleted ( scsiTask, serviceResponse, taskStatus );

		}
	}
}

//--------------------------------------------------------------------------------------------------
// AbortSCSICommand - Aborts an outstanding I/O.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

SCSIServiceResponse
	IOFireWireSerialBusProtocolTransport::AbortSCSICommand (
		SCSITaskIdentifier request )
{
	SCSIServiceResponse		serviceResponse;
	
	DEBUG_UNUSED ( request );
	
	DLOG ( ( "%s: AbortSCSICommand called\n", getName () ) );
	
	serviceResponse = kSCSIServiceResponse_FUNCTION_REJECTED;
	
	return serviceResponse;
}

//--------------------------------------------------------------------------------------------------
// IsProtocolServiceSupported -	Checks for protocol services supported by this device.	[PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
	IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported (
		SCSIProtocolFeature	feature,
		void *				serviceValue )
{
	bool	isSupported = false;
	
	DLOG ( ( "IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported called\n" ) );
	
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
			
			valuePointer = OSDynamicCast ( OSNumber,
										   getProperty ( kSBP2ReceiveBufferByteCountKey,
										   gIOServicePlane ) );

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
		
		case kSCSIProtocolFeature_ProtocolAlwaysReportsAutosenseData:
			
			isSupported = true;
			break;
			
		default:
			
			break;
	}
	
	return isSupported;
}

//--------------------------------------------------------------------------------------------------
// HandleProtocolServiceFeature - Handles protocol service features.					[PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
	IOFireWireSerialBusProtocolTransport::HandleProtocolServiceFeature (
		SCSIProtocolFeature	feature, 
		void *				serviceValue )
{
	DEBUG_UNUSED ( feature );
	DEBUG_UNUSED ( serviceValue );
	
	return false;
}

//--------------------------------------------------------------------------------------------------
// IsDeviceCPUInDiskMode - 	Checks if device is a CPU in FireWire Target Disk Mode.		[PROTECTED]
//--------------------------------------------------------------------------------------------------

bool IOFireWireSerialBusProtocolTransport::IsDeviceCPUInDiskMode ( void )
{
	UInt32						csrModelInfo;
	IOConfigDirectory *			directory;
	IOFireWireDevice *			device;
	IOReturn					status;
	IOService *					providerService;
	bool						isCPUDiskMode;
	
	DLOG ( ( "%s: IsDeviceCPUInDiskMode was called\n", getName () ) );

	csrModelInfo 	= 0;
	directory		= NULL;
	device			= NULL;
	status			= kIOReturnSuccess;
	providerService = fUnit->getProvider ();
	isCPUDiskMode	= false;
	
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
	
	DLOG ( ( "%s: CPU Disk Mode = %d\n", getName (), isCPUDiskMode ) );
	
	return isCPUDiskMode;
}

//--------------------------------------------------------------------------------------------------
// StatusNotifyStatic - C->C++ glue method.												[PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::StatusNotifyStatic (
		void *					refCon,
		FWSBP2NotifyParams *	params )
{
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->StatusNotify ( params );
}

//--------------------------------------------------------------------------------------------------
// StatusNotify - Status notify handler.												[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::StatusNotify (
		FWSBP2NotifyParams * params )
{
	IOFireWireSBP2ORB *		orb;
	FWSBP2StatusBlock *		statusBlock;
	SBP2ClientOrbData *		clientData;
	SCSI_Sense_Data *		targetData;
	UInt8			 		senseData [ kSenseDefaultSize + 8 ] = { 0 };

	statusBlock 	= NULL;
	clientData		= NULL;
	targetData = ( SCSI_Sense_Data * ) &senseData [ 0 ];
	
	if ( ( params->message != NULL ) && ( params->length != 0 ) )
	{
		statusBlock = ( FWSBP2StatusBlock * ) params->message;
	}
	
	orb	= ( IOFireWireSBP2ORB * ) params->commandObject;
	if ( orb )
	{
		clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	}
	
	switch ( params->notificationEvent )
	{
		
		case kFWSBP2NormalCommandStatus:
			
			// Read the status block detail bits see SBP-2 spec section 5.3 check the dead bit
			// ( i.e. that the 'd' field == 1 ).
			
			if ( ( clientData != NULL ) && ( statusBlock->details & 0x08 ) ) 	
			{
				SetValidAutoSenseData ( clientData, statusBlock, targetData );
				
				// Wait for fetch agent to reset before calling CompleteSCSITask which will
				// be called in FetchAgentResetComplete.
				
				fLogin->submitFetchAgentReset ();
			}
			else if ( clientData &&
					( ( statusBlock->details & 0x30 ) == 0 ) && // ( is 'resp' field == 0 )
					( ( statusBlock->details & 0x07 ) == 1 ) && // ( is 'len' field == 1 )
					( statusBlock->sbpStatus == 0 ) )			// ( is 'sbp_status' field == 0 )
			{
				clientData->serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
				clientData->taskStatus		= kSCSITaskStatus_GOOD;
				CompleteSCSITask ( orb );
				
				DLOG ( ( "%s: StatusNotify normal complete \n", getName () ) );
			}
			else if ( clientData )
			{
				SetValidAutoSenseData ( clientData, statusBlock, targetData );
				CompleteSCSITask ( orb );
				
				DLOG ( ( "%s: StatusNotify have sense data or an unexpected error? \n", getName () ) );
			}
			
			break;

		case kFWSBP2NormalCommandTimeout:
			
			DLOG ( ( "%s: kFWSBP2NormalCommandTimeout \n", getName () ) );
					
			if ( clientData )
			{
				if ( clientData->scsiTask )
				{
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_ProtocolTimeoutOccurred;
				}
			} 
			
			// Set flag so FetchAgentReset knows it is being called from a timeout path.
			
			reserved->fLUNResetPathFlag = true;	
			
			// We reset the LUN as good measure in case device is wedged. The LUN reset completion
			// handler will call the fetch agent to be reset. The FetchAgentReset completion handler
			// will call CompleteSCSITask and complete the command with the approrpiate task status.
			
			fLUNResetORB->submit ();
			
			break;
			
		case kFWSBP2NormalCommandReset:
			
			DLOG ( ( "%s: kFWSBP2NormalCommandReset\n", getName () ) );
						
			// kFWSBP2NormalCommandReset - is a misleading definition. A pending command has
			// failed so we need notify the upper layers to complete failed command.
			
			if ( clientData != NULL )
			{
				if ( clientData->scsiTask != NULL )
				{
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_DeliveryFailure;
					CompleteSCSITask ( orb );
				}
			}
			
			break;
			
		default:
			DLOG ( ( "%s: StatusNotify with unknown notificationEvent\n", getName () ) );
			break;
		
	}
}

//--------------------------------------------------------------------------------------------------
//	SetValidAutoSenseData - Sets any valid sense data.									[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::SetValidAutoSenseData (
		SBP2ClientOrbData *	clientData,
		FWSBP2StatusBlock *	statusBlock,
		SCSI_Sense_Data *	targetData )
{
	UInt8		quadletCount;
	
	clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	clientData->taskStatus		= kSCSITaskStatus_No_Status;

	quadletCount = ( statusBlock->details & 0x07 ) - 1 ;
	
	// See if we have any valid sense data.
	
	if ( ( statusBlock->details & 0x30 ) == 0 )
	{
		clientData->serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
		clientData->taskStatus		= CoalesceSenseData ( statusBlock, quadletCount, targetData );
	}
	
	if ( clientData->taskStatus == kSCSITaskStatus_CHECK_CONDITION || reserved->fAlwaysSetSenseData )
	{
		if ( clientData->scsiTask != NULL )
		{
			SetAutoSenseData ( clientData->scsiTask, targetData, kSenseDefaultSize + 8 );
		}
	}
}

//--------------------------------------------------------------------------------------------------
// CoalesceSenseData - Sets sense data in the data buffer.								[PROTECTED]
//--------------------------------------------------------------------------------------------------

SCSITaskStatus
	IOFireWireSerialBusProtocolTransport::CoalesceSenseData (
		FWSBP2StatusBlock *	sourceData,
		UInt8				quadletCount,
		SCSI_Sense_Data *	targetData )
{
	SCSITaskStatus			returnValue;
	uint32_t 				status [ 4 ];
	uint32_t 				i;
	uint8_t 				statusBlockFormat;

	for ( i = 0; i < 4; i++ )
	{
		if ( i < quadletCount) status [ i ] = OSSwapBigToHostInt32 ( sourceData->status[i] );
	}
	
	returnValue			= kSCSITaskStatus_GOOD;
	statusBlockFormat	= 0;
		
	// Pull bits out of SBP-2 status block ( see SBP-2 Annex B section B.2 ) 
	// and copy them into sense data block ( see SPC-2 section 7.23.2 )
		
	if ( quadletCount > 0 )
	{
		statusBlockFormat = ( status[0] >> 30 ) & 0x03;
		returnValue = ( SCSITaskStatus ) ( ( status[0] >> 24 ) & 0x3F );
		
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
			targetData->VALID_RESPONSE_CODE |= ( status[0] >> 16 ) & 0x80;
			targetData->ADDITIONAL_SENSE_CODE = ( status[0] >> 8 ) & 0xFF;
			targetData->ADDITIONAL_SENSE_CODE_QUALIFIER = status[0] & 0xFF;
			targetData->SENSE_KEY = ( status[0] >> 16 ) & 0x0F;
			
			// Set the M, E, I bits: M->FileMark, E->EOM, I->ILI.
			
			targetData->SENSE_KEY |= ( ( status[0] >> 16 ) & 0x70 ) << 1;
			
			if ( quadletCount > 1 )
			{
				targetData->INFORMATION_1 = ( status[1] >> 24 ) & 0xFF;
				targetData->INFORMATION_2 = ( status[1] >> 16 ) & 0xFF;
				targetData->INFORMATION_3 = ( status[1] >> 8 ) & 0xFF;
				targetData->INFORMATION_4 = status[1] & 0xFF;
				targetData->ADDITIONAL_SENSE_LENGTH = 6;
			}
			
			if ( quadletCount > 2 )
			{
				targetData->COMMAND_SPECIFIC_INFORMATION_1 = ( status[2] >> 24 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_2 = ( status[2] >> 16 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_3 = ( status[2] >> 8 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_4 = status[2] & 0xFF;
				targetData->ADDITIONAL_SENSE_LENGTH = 6;
			}
			
			if ( quadletCount > 3 )
			{
				
				UInt8		count;
				
				// Get bytes to copy and clip if greater than sizeof SCSI_Sense_Data.
				
				count = ( quadletCount - 3 ) * sizeof ( UInt32 );
				if ( count > 4 ) count = 4;
								
				bcopy ( &status[3],
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

//--------------------------------------------------------------------------------------------------
// LoginCompletion - Login completion handler.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::LoginCompletion ( 
		FWSBP2LoginCompleteParams * params )
{	
	DLOG ( ( "%s: LoginCompletion complete \n", getName () ) );
	
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
		
		submitOrbFromQueue ();
		loginResumed ();
		
	}
	else
	{
		if ( fLoginRetryCount < kMaxLoginRetryCount )
		{
			fLoginRetryCount++;
			
			DLOG ( ( "%s: resubmitting Login\n", getName () ) );
			
			IOReturn status = login ();
			if ( status != kIOReturnSuccess )
			{
				if ( reserved->fLoginState == kFirstTimeLoggingInState )
				{
					reserved->fLoginState = kLogginFailedState;
					
					// Wake up sleeping start thread.
					
					fCommandGate->commandWakeup ( ( void * ) &reserved->fLoginState );
				}
			}
		}
		else
		{
			// Device can not be logged into after kMaxLoginRetryCount attemptes let's reset
			// the need login flag in case the device was unplugged during login.
			
			fNeedLogin = true;
			loginLost ();

			if ( reserved->fLoginState == kFirstTimeLoggingInState )
			{
				reserved->fLoginState = kLogginFailedState;
				
				// Wake up sleeping start thread.
				
				fCommandGate->commandWakeup ( ( void * ) &reserved->fLoginState );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
// LogoutCompletionStatic - C->C++ glue.												[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::LogoutCompletionStatic (
		void *							refCon,
		FWSBP2LogoutCompleteParams *	params )
{
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LogoutCompletion ( params );
}

//--------------------------------------------------------------------------------------------------
// LogoutCompletion - Logout completion handler.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::LogoutCompletion ( 
		FWSBP2LogoutCompleteParams * params )
{
	DEBUG_UNUSED ( params );

	DLOG ( ( "%s: LogoutCompletion complete \n", getName () ) );
}

//--------------------------------------------------------------------------------------------------
// UnsolicitedStatusNotifyStatic - C->C++ glue.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotifyStatic (
		void * refCon,
		FWSBP2NotifyParams * params )
{
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->UnsolicitedStatusNotify ( params );
}

//--------------------------------------------------------------------------------------------------
// UnsolicitedStatusNotify - Unsolicited status handler.								[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotify (
		FWSBP2NotifyParams * params )
{
	DEBUG_UNUSED ( params );
	
	DLOG ( ( "%s: UnsolicitedStatusNotify called\n", getName () ) );
	
	// Parse and handle unsolicited status.
	
	fLogin->enableUnsolicitedStatus ();
}

//--------------------------------------------------------------------------------------------------
// FetchAgentResetCompleteStatic - C->C++ glue.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::FetchAgentResetCompleteStatic (
		void *		refCon,
		IOReturn	status )
{
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->FetchAgentResetComplete ( status );
}

//--------------------------------------------------------------------------------------------------
// FetchAgentResetComplete - Fetch agent reset handler.									[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::FetchAgentResetComplete ( 
		IOReturn status )
{
	SBP2ClientOrbData *		clientData;
	
	DEBUG_UNUSED ( status );
	
	DLOG ( ( "%s: FetchAgentResetComplete called\n", getName () ) );
	
	require ( fORB, exit );
	
	// When orb chaining is implemented we will notify upper layer
	// to reconfigure device state and resubmitting commands
	
	clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
	if ( clientData != NULL )
	{
		if ( clientData->scsiTask != NULL )
		{
#ifdef _TRANSPORT_FAILURE_RETRIES_	
			if ( reserved->fLUNResetPathFlag && ( reserved->fLUNResetCount > 0 ) )
			{
				reserved->fLUNResetCount--;
				submitOrbFromQueue ();
			}
			else
#endif
			{
				CompleteSCSITask ( fORB );
			}
		}
	}
	
exit:
	
	reserved->fLUNResetPathFlag = false;
}

//--------------------------------------------------------------------------------------------------
// LunResetCompleteStatic - C->C++ glue.												[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::LunResetCompleteStatic (
		void *							refCon,
		IOReturn						status,
		IOFireWireSBP2ManagementORB *	orb )
{
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LunResetComplete ( status, orb );
}

//--------------------------------------------------------------------------------------------------
// LunResetComplete - LUN reset completion handler.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
	IOFireWireSerialBusProtocolTransport::LunResetComplete (
		IOReturn						status,
		IOFireWireSBP2ManagementORB *	orb )
{
	DEBUG_UNUSED ( status );
	DEBUG_UNUSED ( orb );

	DLOG ( ( "%s: LunResetComplete called\n", getName () ) );
	
	fLogin->submitFetchAgentReset ();
}

//--------------------------------------------------------------------------------------------------
// ConnectToDeviceStatic - C->C++ glue.													[PROTECTED]
//--------------------------------------------------------------------------------------------------

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

//--------------------------------------------------------------------------------------------------
//	ConnectToDevice - Connects to the device.											[PROTECTED]
//--------------------------------------------------------------------------------------------------


void IOFireWireSerialBusProtocolTransport::ConnectToDevice ( void )
{
	IOReturn		status;

	DLOG ( ( "%s: ConnectToDevice called\n", getName () ) );
	
	// Avoid double logins during login phase.
	
	fNeedLogin = false;
	fLoginRetryCount = 0;
	
	status = login ();
	if ( status == kIOReturnSuccess )
	{
		// Sleep the start thread - we'll wake it up on login completion.
		
		fCommandGate->commandSleep ( ( void * ) &reserved->fLoginState , THREAD_UNINT );
	}
}

//--------------------------------------------------------------------------------------------------
//	DisconnectFromDevice - Disconnects from device.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::DisconnectFromDevice ( void )
{
	DLOG ( ( "%s: DisconnectFromDevice called\n", getName () ) );
	
	fLoggedIn = false;
	
	// Avoid logins during a logout phase.
	
	fNeedLogin = false;
	
	fLogin->submitLogout ();
}

//--------------------------------------------------------------------------------------------------
// CriticalOrbSubmissionStatic - C->C++ glue.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

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
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->CriticalOrbSubmission (
		( IOFireWireSBP2ORB * ) val1, ( SCSITaskIdentifier  ) val2 );
	
	return kIOReturnSuccess;
}

//--------------------------------------------------------------------------------------------------
//	CriticalOrbSubmission - add command to queue on workloop.							[PROTECTED]
//--------------------------------------------------------------------------------------------------

void
	IOFireWireSerialBusProtocolTransport::CriticalOrbSubmission (
		IOFireWireSBP2ORB *	orb,
		SCSITaskIdentifier	request )
{
	SBP2ClientOrbData *		clientData;
	
	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
	require ( clientData, exit );
	
	clientData->scsiTask = request;
	
	reserved->fSubmitQueue->returnCommand ( orb );

	// Avoid double appending an active orb ( not this one ).
	
	if ( !fORB )
	{
		submitOrbFromQueue ();
	}
	
exit:
	
	return;
}

//--------------------------------------------------------------------------------------------------
// CriticalOrbSubmission - submitORB on workloop.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  6 );

void IOFireWireSerialBusProtocolTransport::submitOrbFromQueue ( )
{
	DLOG ( ( "%s: submitOrbFromQueue called\n", getName () ) );

	// This check is necessary because we may be draining the queue on the requesting close path.
	
	if ( fLoggedIn )
	{
		if ( !fORB )
		{   
			fORB = 	( IOFireWireSBP2ORB * ) reserved->fSubmitQueue->getCommand (false);
			fReconnectCount	= 0;
		}
		
		if ( fORB )
		{
			fLogin->submitORB ( fORB );
		}
	}
}

//--------------------------------------------------------------------------------------------------
// AllocateResources - Allocates resources.												[PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn IOFireWireSerialBusProtocolTransport::AllocateResources ( void )
{
	IOReturn 				status;
	IOWorkLoop * 			workLoop	= NULL;
	
	DLOG ( ( "%s: AllocateResources called\n", getName () ) );
	
	fLogin = fSBPTarget->createLogin ();
	require_action ( fLogin, exit,  status = kIOReturnNoMemory );
	
	fLogin->setLoginFlags ( kFWSBP2ExclusiveLogin );
	fLogin->setLoginRetryCountAndDelayTime ( 32, 1000000 ); //$$$ define constants for these
	fLogin->setMaxPayloadSize ( kMaxFireWirePayload );
	fLogin->setStatusNotifyProc ( this, StatusNotifyStatic );
	fLogin->setUnsolicitedStatusNotifyProc ( this, UnsolicitedStatusNotifyStatic );
	fLogin->setLoginCompletion ( this, LoginCompletionStatic );
	fLogin->setLogoutCompletion ( this, LogoutCompletionStatic );
	fLogin->setFetchAgentResetCompletion ( this, FetchAgentResetCompleteStatic );
	
	// Set BUSY_TIMEOUT register value see SBP-2 spec section 6.2
	// also see IEEE Std 1394-1995 section 8.3.2.3.5 ( no I am not kidding )
	
	fLogin->setBusyTimeoutRegisterValue ( kDefaultBusyTimeoutValue );
	
	fLUNResetORB = fSBPTarget->createManagementORB ( this, LunResetCompleteStatic );
	require_action ( fLUNResetORB, exit, status = kIOReturnNoMemory );
	
	fLUNResetORB->setCommandFunction ( kFWSBP2LogicalUnitReset );
	fLUNResetORB->setManageeCommand ( fLogin );
	
	// Allocate expansion data.
	
	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	require_action ( reserved, exit, status = kIOReturnNoMemory );
	bzero ( reserved, sizeof ( ExpansionData ) );
	
	reserved->fLoginState = kFirstTimeLoggingInState;
	
	// Cache this as a member variable since we don't want to do this on every command.
	#define kAlwaysSetAutoSenseData		"Always Set AutoSense Data"
	
	reserved->fAlwaysSetSenseData = getProperty ( kAlwaysSetAutoSenseData, gIOServicePlane ) ? TRUE : FALSE;
	
	workLoop = getWorkLoop ();
	require_action ( workLoop, exit, status = kIOReturnNoMemory );
	
	reserved->fCommandPool = IOCommandPool::withWorkLoop ( workLoop );
	require_action ( reserved->fCommandPool, exit, status = kIOReturnNoMemory );

	reserved->fSubmitQueue = IOCommandPool::withWorkLoop ( workLoop );
	require_action ( reserved->fSubmitQueue, exit, status = kIOReturnNoMemory );
	
	for ( UInt32 i = 0; i < kCommandPoolOrbCount; ++i )
	{
		IOFireWireSBP2ORB *		orb;
		SBP2ClientOrbData *		clientData	= NULL;
	
		orb = fLogin->createORB ();
		require_action ( orb, exit, status = kIOReturnNoMemory );
	
		clientData = ( SBP2ClientOrbData * ) IOMalloc ( sizeof ( SBP2ClientOrbData ) );
		require_action ( clientData, exit, status = kIOReturnNoMemory );
	
		bzero ( clientData, sizeof ( SBP2ClientOrbData ) );
		clientData->orb	= orb;
		orb->setRefCon ( ( void * ) clientData );
		

		// Enqueue the command in the free list.
		
		reserved->fCommandPool->returnCommand ( orb );
	}
	
	status = kIOReturnSuccess;
	
exit:
	
	return status;
}

//--------------------------------------------------------------------------------------------------
// DeallocateResources - Deallocates resources.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

void IOFireWireSerialBusProtocolTransport::DeallocateResources ( void )
{
	IOFireWireSBP2ORB *		orb;
	SBP2ClientOrbData *		clientData;
	
	orb = NULL;
	clientData = NULL;
	
	DLOG ( ( "%s: DeallocateResources called\n", getName () ) );
	
	// /!\ WARNING - always release orb's before logins.
	
	if ( fLUNResetORB != NULL )
	{
		fLUNResetORB->release ();
		fLUNResetORB = NULL;
	}
	
	// Drain the queue when requesting close so there are no in use commands when this is called.
	
	while ( ( orb = ( IOFireWireSBP2ORB * ) reserved->fCommandPool->getCommand ( false ) ) )
	{
		clientData = ( SBP2ClientOrbData * ) orb->getRefCon ();
		if ( clientData )
		{
			IOFree ( clientData, sizeof ( SBP2ClientOrbData ) );
			clientData = NULL;
		}
		
		orb->release ();
		orb = NULL;
	}
	
	reserved->fCommandPool->release ();
	reserved->fCommandPool = NULL;
	
	reserved->fSubmitQueue->release ();
	reserved->fSubmitQueue = NULL;
	
	if ( fLogin != NULL )
	{
		fLogin->release ();
		fLogin = NULL;
	}
	
}

//--------------------------------------------------------------------------------------------------
//	login - login bottleneck to track retries.											[PROTECTED]
//--------------------------------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  1 );

IOReturn IOFireWireSerialBusProtocolTransport::login ( void )
{
	DLOG ( ( "%s: submitting login.\n", getName () ) );
	
	IOReturn		status;
	
	fNeedLogin = false;
	fLoggedIn = false;
	
	// If we enter this again and fLoginRetryCount is already
	// at kMaxLoginRetryCount - we should default status to an error.
	
	status = kIOReturnError;
	
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

//--------------------------------------------------------------------------------------------------
// submitLogin - submitLogin bottleneck for subclass.									[PROTECTED]
//--------------------------------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  2 );

IOReturn IOFireWireSerialBusProtocolTransport::submitLogin ( void )
{
	IOReturn		status;

	DLOG ( ( "%s: submitting login.\n", getName () ) );
	
	status = fLogin->submitLogin ();
	
	return status;
}

//--------------------------------------------------------------------------------------------------
// loginLost - login lost bottleneck for subclass.										[PROTECTED]
//--------------------------------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  3 );

void IOFireWireSerialBusProtocolTransport::loginLost ( void )
{
	DLOG ( ( "%s: login lost.\n", getName () ) );
	
	// Notification that the login is lost.
}

//--------------------------------------------------------------------------------------------------
// loginSuspended - login resumed bottleneck for subclass.								[PROTECTED]
//--------------------------------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  4 );

void IOFireWireSerialBusProtocolTransport::loginSuspended ( void )
{
	DLOG ( ( "%s: login suspended.\n", getName () ) );
	
	// A successful reconnect orb is required.
	
}

//--------------------------------------------------------------------------------------------------
// loginResumed - login resumed bottleneck for subclass.								[PROTECTED]
//--------------------------------------------------------------------------------------------------

OSMetaClassDefineReservedUsed ( IOFireWireSerialBusProtocolTransport,  5 );

void IOFireWireSerialBusProtocolTransport::loginResumed ( void )
{
	DLOG ( ( "%s: login resumed.\n", getName () ) );
	
	// A reconnect orb has succeeded.
	
}

//--------------------------------------------------------------------------------------------------
// Binary compatibility reserved method space.
//--------------------------------------------------------------------------------------------------

#if 0
#pragma mark == VTable Padding ==
#endif

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

//--------------------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------------------
