#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syslog.h>

#include <CoreGraphics/CGSDisplay.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <servers/bootstrap.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/USB.h>

//--- Debug Globals ---
io_service_t	service = NULL;

static void callback(CFMachPortRef port, void *msg, CFIndex size, void *info);
static void DoAudioDriverShutdown( void );
static void handler( int sig );

static void callback( CFMachPortRef port, void *msg, CFIndex size, void *info )
{
	return;
}

static void handler( int sig )
{
	DoAudioDriverShutdown ();

	//--- We've been asked nicely to quit, so quit ---
	exit( 0 );
}

int main( int arg, const char *argv[] )
{
	kern_return_t		kr;
	mach_port_t		bootstrap_port;
	CFMachPortRef		process_port;
	boolean_t		active;
	CFRunLoopSourceRef	source;
	int			status;
	int 			err;
	struct sigaction 	action;
			
	kr = task_get_bootstrap_port( mach_task_self(), &bootstrap_port );
	if ( kr != KERN_SUCCESS )
		exit( -1 );
	
	//--- Make sure we are not already running ---
	kr = bootstrap_status( bootstrap_port, "com.apple.AudioLogin", &active );
	switch ( kr )
	{
		case BOOTSTRAP_SUCCESS:
			if ( active )
				exit( 0 );
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE:
			break;		// good
		default:
			exit( -2 );
	}

	//--- Set up a connection port ---
	process_port = CFMachPortCreate( NULL, callback, NULL, NULL );
	if ( process_port == NULL )
		exit( -3 );
		
	//--- Create a source and add it to the run loop ---
	source = CFMachPortCreateRunLoopSource( NULL, process_port, 0 );
	if ( source == NULL )
		exit( -4 );
	CFRunLoopAddSource( CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode );
	CFRelease( source );

	//--- Register the service ---
	kr = bootstrap_register( bootstrap_port, "com.apple.AudioLogin", CFMachPortGetPort(process_port) );
	switch (kr) {
		case BOOTSTRAP_SUCCESS :
			break;					// good 
		case BOOTSTRAP_NOT_PRIVILEGED :
			exit (-5);
		case BOOTSTRAP_SERVICE_ACTIVE :
			exit (-6);
		default :
			exit (-7);
	}
	
	//--- DEBUG ---
	CGSServiceForDisplayNumber( CGSMainDisplayID(), &service );
		
	//--- Then set up the SIGTERM handler ---
	//--- Install SIGTERM handler ---
	action.sa_handler 	= handler;
	sigemptyset( &action.sa_mask );
	action.sa_flags		= SA_RESTART;
	
	err = sigaction( SIGTERM, &action, NULL );
	if ( err != 0 )
		exit( -8 );
	
	//--- Loop around ---
	while ( 1 )
		status = CFRunLoopRunInMode( kCFRunLoopDefaultMode, 1.0e10, TRUE );
	
	exit( 0 );
	return 0;
}

//===========================================================================================================================
//                                                        Audio stuff
//===========================================================================================================================

//===========================================================================================================================
//	Private constants
//===========================================================================================================================

typedef struct OpaqueAOARef *		AOARef;

enum
{
	kAOAShutdownIndex 	= 0
};

#define	kTexasDriverClassName		"AppleTexasAudio"
#define	kDACADriverClassName		"AppleDACAAudio"

//===========================================================================================================================
//	Private prototypes
//===========================================================================================================================

static IOReturn	SetupUserClient( void );
static void	TearDownUserClient( void );
static OSStatus	AOAShutdown( void );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static mach_port_t		gMasterPort		= 0;
static io_object_t		gDriverObject	= 0;
static io_connect_t		gDataPort 		= 0;

static void DoAudioDriverShutdown( void )
{
	IOReturn			err;

	err = AOAShutdown ();
	if( err != noErr ) {
		syslog (0, "AOAShutdown failed\n");
		goto exit;
	}

	TearDownUserClient ();

exit:
	return;
}

static IOReturn	SetupUserClient( void )
{
	IOReturn		err;
	CFDictionaryRef		matchingDictionary;
	io_iterator_t		serviceIter;
	
	// Initialize variables for easier cleanup.
	
	err			= kIOReturnSuccess;
	matchingDictionary 	= NULL;
	serviceIter		= NULL;
	
	// Exit quickly if we're already set up.
	
	if( gDataPort )
	{
		goto exit;
	}
	
	// Get a port so we can communicate with IOKit.
	
	err = IOMasterPort( NULL, &gMasterPort );
	if( err != kIOReturnSuccess ) {
		syslog (0, "IOMasterPort failed\n");
		goto exit;
	}

	// Build a dictionary of all the services matching our service name. Note that we do not release the dictionary
	// if IOServiceGetMatchingServices succeeds because it does the release itself.
	
	err = kIOReturnNotFound;
	matchingDictionary = IOServiceNameMatching( kTexasDriverClassName );
	if( !matchingDictionary ) {
		syslog (0, "matchingDictionary is NULL\n");
		goto exit;
	}

	err = IOServiceGetMatchingServices( gMasterPort, matchingDictionary, &serviceIter );
	if( err != kIOReturnSuccess ) {
		syslog (0, "IOServiceGetMatchingServices failed\n");
		goto exit;
	}
	matchingDictionary = NULL;

	err = kIOReturnNotFound;
	gDriverObject = IOIteratorNext( serviceIter );
	if( !gDriverObject ) {
		syslog (0, "didn't find AppleTexasAudio, looking for DACA\n");

		matchingDictionary = IOServiceNameMatching( kDACADriverClassName );
		if( !matchingDictionary ) {
			syslog (0, "matchingDictionary is NULL\n");
			goto exit;
		}

		err = IOServiceGetMatchingServices( gMasterPort, matchingDictionary, &serviceIter );
		if( err != kIOReturnSuccess ) {
			syslog (0, "IOServiceGetMatchingServices failed\n");
			goto exit;
		}
		matchingDictionary = NULL;

		err = kIOReturnNotFound;
		gDriverObject = IOIteratorNext( serviceIter );
		if( !gDriverObject ) {
			syslog (0, "gDriverObject is NULL\n");
			goto exit;
		}
	}

	// Open a connection to our service so we can talk to it.
	err = IOServiceOpen( gDriverObject, mach_task_self(), 0, &gDataPort );
	if( err != kIOReturnSuccess ) {
		syslog (0, "IOServiceOpen failed\n");
		goto exit;
	}

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

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	Shutdown
//===========================================================================================================================

static OSStatus	AOAShutdown( void )
{
	OSStatus		err;
	
	// Set up user client if not already set up.
	
	err = SetupUserClient();
	if( err != noErr ) {
		syslog (0, "AOAShutdown->SetupUserClient failed\n");
		goto exit;
	}
	
	// RPC to the kernel.
	
	err = IOConnectMethodScalarIScalarO( gDataPort, kAOAShutdownIndex, 0, 0 );
	if( err != noErr ) {
		syslog (0, "AOAShutdown->IOConnectMethodScalarIScalarO failed\n");
		goto exit;
	}
	
exit:
	return( err );
}
