/*
 *  AppleOnboardAudioUserClient.cpp
 *  AppleOnboardAudio
 *
 *  Created by Aram Lindahl on Tue Apr 15 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include "AppleOnboardAudioUserClient.h"
#include "AppleOnboardAudio.h"
#include "AudioHardwareUtilities.h"
#include "PlatformInterface.h"

const IOExternalMethod		AppleOnboardAudioUserClient::sMethods[] =
{
	//	get state, pass in selector
	{
		NULL,																// object
		( IOMethod ) &AppleOnboardAudioUserClient::getState,				// func
        kIOUCScalarIStructO,   												// Struct Input, Struct Output.
		2,																	// count of input parameters
        kUserClientStateStructSize, 										// The size of the input struct.
 	},
	//	set state, pass in selector for which state
	{
		NULL,																// object
		( IOMethod ) &AppleOnboardAudioUserClient::setState,				// func
		kIOUCScalarIStructI,												// flags
		2,																	// count of input parameters
        kUserClientStateStructSize, 										// The size of the input struct.
	},
	// get the current sample frame
	{
		NULL,																// object
		( IOMethod ) &AppleOnboardAudioUserClient::getCurrentSampleFrame,	// func
		kIOUCScalarIScalarO,												// flags
		0,																	// count of input parameters
		1																	// count of output parameters
	}
};

const IOItemCount 	AppleOnboardAudioUserClient::sMethodCount = sizeof (AppleOnboardAudioUserClient::sMethods) / sizeof (AppleOnboardAudioUserClient::sMethods[0]);

OSDefineMetaClassAndStructors( AppleOnboardAudioUserClient, IOUserClient )

//===========================================================================================================================
//	Create
//===========================================================================================================================

AppleOnboardAudioUserClient *	AppleOnboardAudioUserClient::Create( AppleOnboardAudio *inDriver, task_t inTask )
{
    AppleOnboardAudioUserClient *		userClient;
    
    userClient = new AppleOnboardAudioUserClient;
	if( !userClient )
	{
		debugIOLog( "[AppleOnboardAudio] create user client object failed\n" );
		goto exit;
	}
    
    if( !userClient->initWithDriver( inDriver, inTask ) )
	{
		debugIOLog( "[AppleOnboardAudio] initWithDriver failed\n" );
		
		userClient->release();
		userClient = NULL;
		goto exit;
	}
	
//	debug2IOLog( "[AppleOnboardAudio] User client created for task 0x%08lX\n", ( UInt32 ) inTask );
	
exit:
	return( userClient );
}

//===========================================================================================================================
//	initWithDriver
//===========================================================================================================================

bool	AppleOnboardAudioUserClient::initWithDriver( AppleOnboardAudio *inDriver, task_t inTask )
{
	bool		result;
	
//	debug3IOLog( "AppleOnboardAudioUserClient::initWithDriver (%p, %p)\n", inDriver, (void *)inTask);
	
	result = false;
    if( !initWithTask( inTask, NULL, 0 ) )
	{
		debugIOLog( "   initWithTask failed\n" );
		goto exit;
    }
    if( !inDriver )
	{
		debugIOLog( "   initWithDriver failed (null input driver)\n" );
        goto exit;
    }
    
    mDriver 	= inDriver;
    mClientTask = inTask;
    result		= true;
	
exit:
	return( result );
}

//===========================================================================================================================
//	free
//===========================================================================================================================

void	AppleOnboardAudioUserClient::free( void )
{
//	debugIOLog( "AppleOnboardAudioUserClient::free ()\n" );
	
    IOUserClient::free();
}

//===========================================================================================================================
//	clientClose
//===========================================================================================================================

IOReturn	AppleOnboardAudioUserClient::clientClose( void )
{
//	debugIOLog( "AppleOnboardAudioUserClient::clientClose ()\n" );
	
    if( !isInactive() )
	{
        mDriver = NULL;
    }
    return( kIOReturnSuccess );
}

//===========================================================================================================================
//	clientDied
//===========================================================================================================================

IOReturn	AppleOnboardAudioUserClient::clientDied( void )
{
//	debugIOLog( "AppleOnboardAudioUserClient::clientDied ()\n" );
	
    return( clientClose() );
}

//===========================================================================================================================
//	getTargetAndMethodForIndex
//===========================================================================================================================

IOExternalMethod *	AppleOnboardAudioUserClient::getTargetAndMethodForIndex( IOService **outTarget, UInt32 inIndex )
{
	IOExternalMethod *		methodPtr;
	
	methodPtr = NULL;
	if( inIndex <= sMethodCount )  {
        *outTarget = this;
		methodPtr = ( IOExternalMethod * ) &sMethods[ inIndex ];
    } else {
		debug2IOLog( "[AppleOnboardAudio] getTargetAndMethodForIndex - bad index (index=%lu)\n", inIndex );
	}
	return( methodPtr );
}


//===========================================================================================================================
//	getState - 2 scalar in, 1 struct out
//===========================================================================================================================

IOReturn AppleOnboardAudioUserClient::getState (UInt32 selector, UInt32 arg2, void * outState) {
	IOReturn						err;
	
	err = kIOReturnError;

	if ( NULL != mDriver && NULL != outState ) {
		switch (selector) {
			case kPlatformSelector:
				err = mDriver->getPlatformState ( arg2, (PlatformStateStructPtr)outState );
				break;
			case kHardwarePluginSelector:
				err = mDriver->getPluginState ( (HardwarePluginType)arg2, (HardwarePluginDescriptorPtr)outState );
				break;
			case kDMASelector:
				err = mDriver->getDMAStateAndFormat ( arg2, outState );
				break;
			case kSoftwareProcessingSelector:
				err = mDriver->getSoftwareProcessingState ( arg2, outState );
				break;
			case kAppleOnboardAudioSelector:
				err = mDriver->getAOAState ( arg2, outState );
				break;
			case kTransportInterfaceSelector:
				err = mDriver->getTransportInterfaceState ( arg2, outState );
				break;
			default:
				debug2IOLog ("Unknown user client selector (%ld)\n", selector);
				break;
		}
	}
	return (err);
}

//===========================================================================================================================
//	setState - 2 scalar in, 1 struct in
//===========================================================================================================================

IOReturn AppleOnboardAudioUserClient::setState (UInt32 selector, UInt32 arg2, void * inState) {
	IOReturn						err;
	
	err = kIOReturnError;

	if ( NULL != mDriver ) {
		switch (selector) {
			case kPlatformSelector:
				err = mDriver->setPlatformState ( arg2, (PlatformStateStructPtr)inState );
				break;
			case kHardwarePluginSelector:
				err = mDriver->setPluginState ( (HardwarePluginType)arg2, (HardwarePluginDescriptorPtr)inState );
				break;
			case kDMASelector:
				err = mDriver->setDMAState ( arg2, inState );
				break;
			case kSoftwareProcessingSelector:
				err = mDriver->setSoftwareProcessingState ( arg2, inState );
				break;
			case kAppleOnboardAudioSelector:
				err = mDriver->setAOAState ( arg2, inState );
				break;
			case kTransportInterfaceSelector:
				err = mDriver->setTransportInterfaceState ( arg2, inState );
				break;
			default:
				debug2IOLog ("Unknown user client selector (%ld)\n", selector);
				break;
		}
	}
	return (err);
}

//===========================================================================================================================
//	getCurrentSampleFrame - 0 scalar in, 1 scalar out
//===========================================================================================================================

IOReturn AppleOnboardAudioUserClient::getCurrentSampleFrame (UInt32 * outCurrentSampleFrame) {
	IOReturn						err;
	
	err = kIOReturnError;
	
	if ( NULL != mDriver && NULL != outCurrentSampleFrame ) {
		*outCurrentSampleFrame = mDriver->getCurrentSampleFrame ();
		err = kIOReturnSuccess;
	}
	
	return (err);
}


#if 0
//===========================================================================================================================
//		The following code goes into whatever application wants to call into AppleOnboardAudio
//===========================================================================================================================

//===========================================================================================================================
//	Private constants
//===========================================================================================================================

enum
{
	kAOAUserClientGetStateIndex	 		=	0,		//	returns data from gpio																
	kAOAUserClientSetStateIndex,						//	writes data to gpio
	kAOAUserClientGetCurrentSampleFrame				// returns TRUE if gpio is active high													
};

//===========================================================================================================================
//	Private prototypes
//===========================================================================================================================

static IOReturn	SetupUserClient( void );
static void		TearDownUserClient( void );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static mach_port_t		gMasterPort		= 0;
static io_object_t		gDriverObject	= 0;
static io_connect_t		gDataPort 		= 0;

//===========================================================================================================================
//	SetupUserClient
//===========================================================================================================================

static IOReturn	SetupUserClient( void )
{
	IOReturn			err;
	CFDictionaryRef		matchingDictionary;
	io_iterator_t		serviceIter;
	
	// Initialize variables for easier cleanup.
	
	err					= kIOReturnSuccess;
	matchingDictionary 	= NULL;
	serviceIter			= NULL;
	
	// Exit quickly if we're already set up.
	
	if( gDataPort )
	{
		goto exit;
	}
	
	// Get a port so we can communicate with IOKit.
	
	err = IOMasterPort( NULL, &gMasterPort );
	if( err != kIOReturnSuccess ) goto exit;
	
	// Build a dictionary of all the services matching our service name. Note that we do not release the dictionary
	// if IOServiceGetMatchingServices succeeds because it does the release itself.
	
	err = kIOReturnNotFound;
	matchingDictionary = IOServiceNameMatching( "AppleOnboardAudio" );
	if( !matchingDictionary ) goto exit;
	
	err = IOServiceGetMatchingServices( gMasterPort, matchingDictionary, &serviceIter );
	if( err != kIOReturnSuccess ) goto exit;
	matchingDictionary = NULL;
	
	err = kIOReturnNotFound;
	gDriverObject = IOIteratorNext( serviceIter );
	if( !gDriverObject ) goto exit;
	
	// Open a connection to our service so we can talk to it.
	
	err = IOServiceOpen( gDriverObject, mach_task_self(), 0, &gDataPort );
	if( err != kIOReturnSuccess ) goto exit;
	
	// Success. Clean up stuff and we're done.
	
exit:
	if( serviceIter )
	{
		IOObjectRelease( serviceIter );
	}
	if( matchingDictionary )
	{
		CFRelease( matchingDictionary );
	}
	if( err != kIOReturnSuccess )
	{
		TearDownUserClient();
	}
	return( err );
}

//===========================================================================================================================
//	TearDownUserClient
//===========================================================================================================================

static void	TearDownUserClient( void )
{
	if( gDataPort )
	{
		IOServiceClose( gDataPort );
		gDataPort = 0;
	}
	if( gDriverObject )
	{
		IOObjectRelease( gDriverObject );
		gDriverObject = NULL;
	}
	if( gMasterPort )
	{
		mach_port_deallocate( mach_task_self(), gMasterPort );
		gMasterPort = 0;
	}
}

//===========================================================================================================================
//	getState
//===========================================================================================================================

OSStatus	getState( UInt32 selector, UInt32 target, UInt32 arg2, void * outState )
{	
	OSStatus		err;
	
	// Set up user client if not already set up.
	
	err = SetupUserClient();
	if( err != noErr ) goto exit;
	
	// RPC to the kernel.
	
	err = IOConnectMethodScalarIStructO( gDataPort, kGetStateIndex, 2, kAOAUserClientStructSize, selector, target, outState );
	if( err != noErr ) goto exit;
	
exit:
	return( err );
}

//===========================================================================================================================
//	setState
//===========================================================================================================================

OSStatus	setState( UInt32 selector, UInt32 target, UInt32 arg2 void * inState )
{	
	OSStatus		err;
	
	// Set up user client if not already set up.
	
	err = SetupUserClient();
	if( err != noErr ) goto exit;
	
	// RPC to the kernel.
	
	err = IOConnectMethodScalarIStructI( gDataPort, kSetStateIndex, 2, kAOAUserClientStructSize, selector, target, inState );
	if( err != noErr ) goto exit;
	
exit:
	return( err );
}

//===========================================================================================================================
//	getCurrentSampleFrame
//===========================================================================================================================

OSStatus	getCurrentSampleFrame( UInt32 selector, UInt32 * outCurrentSampleFrame )
{
	OSStatus		err;
	
	// Set up user client if not already set up.
	
	err = SetupUserClient();
	if( err != noErr ) goto exit;
	
	// RPC to the kernel.
	
	err = IOConnectMethodScalarIScalarO( gDataPort, kgpiogetAddressIndex, 2, selector, outCurrentSampleFrame );
	if( err != noErr ) goto exit;
	
exit:
	return( err );
}
#endif
