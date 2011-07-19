/*
  File: AppleSCSIEmulatorAdapterUC.cpp

  Contains:

  Version: 1.0.0
  
  Copyright: Copyright (c) 2007 by Apple Inc., All Rights Reserved.

Disclaimer:IMPORTANT:  This Apple software is supplied to you by Apple Inc. 
("Apple") in consideration of your agreement to the following terms, and your use, 
installation, modification or redistribution of this Apple software constitutes acceptance 
of these terms.  If you do not agree with these terms, please do not use, install, modify or 
redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under Apple's
copyrights in this original Apple software (the "Apple Software"), to use, reproduce, 
modify and redistribute the Apple Software, with or without modifications, in source and/or
binary forms; provided that if you redistribute the Apple Software in its entirety
and without modifications, you must retain this notice and the following text
and disclaimers in all such redistributions of the Apple Software.  Neither the
name, trademarks, service marks or logos of Apple Inc. may be used to
endorse or promote products derived from the Apple Software without specific prior
written permission from Apple.  Except as expressly stated in this notice, no
other rights or licenses, express or implied, are granted by Apple herein,
including but not limited to any patent rights that may be infringed by your derivative
works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE
OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. IN NO EVENT SHALL APPLE 
BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include "AppleSCSIEmulatorAdapterUC.h"
#include "AppleSCSIEmulatorAdapter.h"


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												1
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"AdapterUC"

#if DEBUG
#define ADAPTER_UC_DEBUGGING_LEVEL							3
#endif

#include "DebugSupport.h"

#if ( ADAPTER_UC_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
#else
#define PANIC_NOW(x)		
#endif

#if ( ADAPTER_UC_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x; IOSleep(1)
#else
#define ERROR_LOG(x)		
#endif

#if ( ADAPTER_UC_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x; IOSleep(1)
#else
#define STATUS_LOG(x)		
#endif


// Define superclass
#define super IOUserClient
OSDefineMetaClassAndStructors ( AppleSCSIEmulatorAdapterUserClient, IOUserClient );


#if 0
#pragma mark -
#pragma mark Public Methods
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	 initWithTask - Save task_t and validate the connection type	[PUBLIC]
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapterUserClient::initWithTask (
	task_t 			owningTask,
	void * 			securityToken,
	UInt32 			type,
	OSDictionary *	properties )
{
	
	bool	result	= false;
	
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapterUserClient::initWithTask called\n" ) );
	
	result = super::initWithTask ( owningTask, securityToken, type, properties );
	require ( result, ErrorExit );
	
	fTask = owningTask;
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	 start - Start providing services								[PUBLIC]
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapterUserClient::start ( IOService * provider )
{
	
	bool			result		= false;
	IOWorkLoop *	workLoop	= NULL;
	
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapterUserClient::start\n" ) );
	
	require ( ( fProvider == 0 ), ErrorExit );
	require ( super::start ( provider ), ErrorExit );
	
	// Save the provider
	fProvider = provider;
	
	STATUS_LOG ( ( "Creating command gate\n" ) );
	
	fCommandGate = IOCommandGate::commandGate ( this );
	require_nonzero ( fCommandGate, ErrorExit );
	
	workLoop = getWorkLoop ( );
	require_nonzero_action ( workLoop,
							 ErrorExit,
							 fCommandGate->release ( ) );

	STATUS_LOG ( ( "Adding event source\n" ) );
	
	workLoop->addEventSource ( fCommandGate );
	
	STATUS_LOG ( ( "Opening provider\n" ) );
	
	result = provider->open ( this, kSCSIEmulatorAdapterUserClientAccessMask, 0 );
	require_action ( result,
					 ErrorExit,
					 workLoop->removeEventSource ( fCommandGate );
					 fCommandGate->release ( );
					 fCommandGate = NULL );
	
	fWorkLoop = workLoop;
	
	
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	 clientClose - Called to when a client closes/dies				   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
AppleSCSIEmulatorAdapterUserClient::clientClose ( void )
{
	
	if ( fProvider != NULL )
	{
		
		if ( fProvider->isOpen ( this ) == true )
		{
			
			fProvider->close ( this, kSCSIEmulatorAdapterUserClientAccessMask );
			
		}
		
		detach ( fProvider );
		fProvider = NULL;
		
	}
	
	return super::clientClose ( );
	
}


//-----------------------------------------------------------------------------
//	 finalize - Called to finalize resources						   [PUBLIC]
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapterUserClient::finalize ( IOOptionBits options )
{
	
	clientClose ( );
	return super::finalize ( options );
	
}


//-----------------------------------------------------------------------------
//	 free - Releases any items we need to release.					   [PUBLIC]
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapterUserClient::free ( void )
{
	
	// Remove the command gate from the workloop
	if ( fWorkLoop != NULL )
	{
		
		fWorkLoop->removeEventSource ( fCommandGate );
		fWorkLoop = NULL;
		
	}
	
	// Release the command gate
	if ( fCommandGate != NULL )
	{
		
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
	super::free ( );
	
}


//-----------------------------------------------------------------------------
//	 externalMethod - 	Main dispatcher.							   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
AppleSCSIEmulatorAdapterUserClient::externalMethod (
					uint32_t						selector,
					IOExternalMethodArguments * 	args,
					IOExternalMethodDispatch * 		dispatch,
					OSObject *						target,
					void *							reference )
{
	
	IOReturn	status = kIOReturnBadArgument;
	
	// We only have synchronous requests. If the asyncWakePort is a port,
	// this is an async request and we should reject it.
	require ( ( args->asyncWakePort == MACH_PORT_NULL ), ErrorExit );
	require ( ( selector < kUserClientMethodCount ), ErrorExit );
	
	if ( selector == kUserClientCreateLUN )
	{
		
		require ( ( args->structureInputSize == sizeof ( EmulatorTargetParamsStruct ) ), ErrorExit );
		require ( ( args->structureOutputSize == 0 ), ErrorExit );
		
		STATUS_LOG ( ( "args->structureInputSize = %u\n", args->structureInputSize ) );
		
		status = ( ( AppleSCSIEmulatorAdapter * ) fProvider )->CreateLUN ( ( EmulatorTargetParamsStruct * ) args->structureInput, fTask );
		
	}
	
	else if ( selector == kUserClientDestroyLUN )
	{
		
		require ( ( args->scalarInputCount == 2 ), ErrorExit );
		require ( ( args->scalarOutputCount == 0 ), ErrorExit );
		
		STATUS_LOG ( ( "args->scalarInputCount = %u\n", args->scalarInputCount ) );
		STATUS_LOG ( ( "args->scalarInput[0] = %qd, args->scalarInput[1] = %qd\n", args->scalarInput[0], args->scalarInput[1] ) );
		
		status = ( ( AppleSCSIEmulatorAdapter * ) fProvider )->DestroyLUN ( args->scalarInput[0], args->scalarInput[1] );
		
	}

	else if ( selector == kUserClientDestroyTarget )
	{
		
		require ( ( args->scalarInputCount == 1 ), ErrorExit );
		require ( ( args->scalarOutputCount == 0 ), ErrorExit );
		
		STATUS_LOG ( ( "args->scalarInputCount = %u\n", args->scalarInputCount ) );
		STATUS_LOG ( ( "args->scalarInput[0] = %qd\n", args->scalarInput[0] ) );
		
		status = ( ( AppleSCSIEmulatorAdapter * ) fProvider )->DestroyTarget ( args->scalarInput[0] );
		
	}
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "externalMethod status = 0x%08x\n", status ) );
	
	return status;
	
}