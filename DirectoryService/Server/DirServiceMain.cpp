/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header DirServiceMain
 */

#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/stat.h>		// for file and dir stat calls
#include <sys/mount.h>		// for file system check
#include <sys/wait.h>		// for waitpid() et al
#include <sys/resource.h>	// for getrlimit()
#include <fcntl.h>			// for open() and O_* flags
#include <paths.h>			// for _PATH_DEVNULL
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>			// for signal handling
#include <time.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <sys/sysctl.h>				// for struct kinfo_proc and sysctl()
#include <syslog.h>					// for syslog()

#define USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>	//required for the configd kicker operation
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>		//required for power management handling
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_init.h>
#include <mach/port.h>

#include "DirServiceMain.h"
#include "ServerControl.h"
#include "CLog.h"
#include "PrivateTypes.h"
#include "COSUtils.h"
#include "CPlugInList.h"

#include "DirServicesTypes.h"

using namespace std;

dsBool	gLogAPICalls	= false;
dsBool	gDebugLogging	= false;
time_t	gSunsetTime		= time( nil);
//Used for Power Management
io_object_t			gPMDeregisterNotifier;
io_connect_t		gPMKernelPort;
CFRunLoopRef		gServerRunLoop = NULL;


// Static ---------------------------------------------------------------------
//	Used to notify the main thread when to terminate.

static bool					_Terminate			= false;
static pid_t				_ChildPid			= 0;
static int					_ChildStatus		= 0;

enum
{
	kSignalMessage		= 1000
};

typedef struct SignalMessage
{
	mach_msg_header_t	header;
	mach_msg_body_t		body;
	int					signum;
	mach_msg_trailer_t	trailer;
} SignalMessage;

static void SignalHandler(int signum);
static void SignalMessageHandler(CFMachPortRef port,SignalMessage *msg,CFIndex size,void *info);

static mach_port_t					gSignalPort = MACH_PORT_NULL;

void SignalHandler(int signum)
{
	SignalMessage	msg;
	
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0);
	msg.header.msgh_size = sizeof(msg) - sizeof(mach_msg_trailer_t);
	msg.header.msgh_remote_port = gSignalPort;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_id = kSignalMessage;
	msg.body.msgh_descriptor_count = 0;
	msg.signum = signum;
	
	mach_msg(&msg.header,(MACH_SEND_MSG | MACH_SEND_TIMEOUT),
			 msg.header.msgh_size,0,MACH_PORT_NULL,0,MACH_PORT_NULL);
}

// ---------------------------------------------------------------------------
//	* _HandleSigChildInParent ()
//
// ---------------------------------------------------------------------------

static void _HandleSigChildInParent ( ... )
{
	int		nStatus = 0;
	pid_t		pidChild;

	pidChild = ::waitpid( -1, &nStatus, WNOHANG );
	if ( pidChild == 0 )
	{
		return;
	}

	if ( pidChild != _ChildPid )
	{
		return;
	}

	// Only daemonizing parent should get here when child fails to daemonize!

	// Can't use CLog functions here because the parent doesn't open it.
	if ( WIFEXITED( nStatus ) )
	{
		::fprintf( stderr, "Daemonization failed with exit status %d.\n", WEXITSTATUS( nStatus ) );
	}
	else if ( WIFSIGNALED( nStatus ) )
	{
		::fprintf( stderr, "Daemonization failed due to signal %d.\n", WTERMSIG( nStatus ) );
	}
	else
	{
		::fprintf(stderr, "Daemonization failed with status %d.\n", nStatus );
	}

	// Parent should quit with an error.
	_ChildStatus = nStatus;
	_Terminate = true;
} // _HandleSigChildInParent


// ---------------------------------------------------------------------------
//	* _HandleSigTermInParent ()
//
// ---------------------------------------------------------------------------

static void _HandleSigTermInParent ( ... )
{
	_Terminate = true;

#if DEBUG
	fprintf( stderr, "Caught a terminating signal (SIGTERM or SIGABRT)\n" );
	fflush( stderr );
#endif

} // _HandleSigTermInParent

// ---------------------------------------------------------------------------
//	* _HandleSIGTERM ()
//
// ---------------------------------------------------------------------------
static void _HandleSIGTERM ( ... )
{
	CFRunLoopStop(CFRunLoopGetCurrent());
} // _HandleSIGTERM

// ---------------------------------------------------------------------------
//	* _HandleSigHup ()
//
// ---------------------------------------------------------------------------
static void _HandleSigHup ( ... )
{
	if ( gSrvrCntl != nil )
	{
		gSrvrCntl->HandleNetworkTransition(); //ignore return status
	}
} // _HandleSigHup

// ---------------------------------------------------------------------------
//	* _HandleSIGUSR1 ()
//
// ---------------------------------------------------------------------------
static void _HandleSIGUSR1 ( ... )
{
	//toggle the global
	if (gDebugLogging)
	{
		gDebugLogging = false;
		CLog::StopDebugLog();
		syslog(LOG_INFO,"Debug Logging turned OFF after receiving USR1 signal.");
	}
	else
	{
		gDebugLogging = true;
		CLog::StartDebugLog();
		syslog(LOG_INFO,"Debug Logging turned ON after receiving USR1 signal.");
	}
} // _HandleSIGUSR1

// ---------------------------------------------------------------------------
//	* _HandleSIGUSR2 ()
//
// ---------------------------------------------------------------------------
static void _HandleSIGUSR2 ( ... )
{
	//toggle the global
	if (gLogAPICalls)
	{
		gLogAPICalls = false;
		syslog(LOG_INFO,"Logging of API Calls turned OFF after receiving USR2 signal.");
	}
	else
	{
		gLogAPICalls = true;
		gSunsetTime = time(nil) + 300;
		syslog(LOG_INFO,"Logging of API Calls turned ON after receiving USR2 signal.");
	}
} // _HandleSIGUSR2


void SignalMessageHandler(CFMachPortRef port,SignalMessage *msg,CFIndex size,void *info)
{
	//handle SIGTERM, SIGHUP, SIGUSR1, SIGUSR2, SIGABRT, SIGINT, SIGPIPE
	if (msg->signum == SIGUSR1)
	{
		_HandleSIGUSR1();
	}
	else if (msg->signum == SIGUSR2)
	{
		_HandleSIGUSR2();
	}
	else if (msg->signum == SIGHUP)
	{
		_HandleSigHup();
	}
	else if ( (msg->signum == SIGTERM) || (msg->signum == SIGABRT) )
	{
		_HandleSIGTERM();
	}
}

// ---------------------------------------------------------------------------
//	* _Usage ()
//
// ---------------------------------------------------------------------------

static void _Usage ( FILE *fp, const char *argv0 )
{
	static const char * const	_szpUsage =
		"Usage:\t%s [-hv]\n"
		"	-h	Display this list of options.\n"
		"	-v	Display the release version.\n";
	::fprintf( fp, _szpUsage, argv0 );

} // _Usage


// ---------------------------------------------------------------------------
//	* _Version ()
//
// ---------------------------------------------------------------------------

static void _Version ( FILE *fp )
{
	static const char * const	_szpUsage =
		"Apple Computer, Inc.  Version DirectoryService %s\n";
	::fprintf( fp, _szpUsage, COSUtils::GetStringFromList( kSysStringListID, kStrVersion ) );

} // _Version


// ---------------------------------------------------------------------------
//	* _AppleVersion ()
//
// ---------------------------------------------------------------------------

static void _AppleVersion ( FILE *fp )
{
	static const char * const	_szpUsage =
		"Apple Computer, Inc.  Version DirectoryService-%s\n";
	::fprintf( fp, _szpUsage, COSUtils::GetStringFromList( kSysStringListID, kStrBuildNumber ) );

} // _AppleVersion


// ---------------------------------------------------------------------------
//	* _AppleOptions ()
//
// ---------------------------------------------------------------------------

static void _AppleOptions ( FILE *fp, const char *argv0 )
{
	static const char * const	_szpUsage =
		"Usage:\t%s [-applexxxxx OR -v]\n"
		"	-appledebug     	Run the daemon in debug mode as standalone.\n"
		"	-appleframework		Start the daemon like the Framework does using configd(normal operation).\n"
		"	-applenodaemon  	Run the daemon in the foreground.\n"
		"	-appleoptions   	List these options.\n"
		"	-appleperformance   Log everything and run in the foreground.\n"
		"	-appleversion   	Display the Apple build version.\n"
		"	-v              	Display the release version.\n";
	::fprintf( fp, _szpUsage, argv0 );
} // _AppleOptions

// ---------------------------------------------------------------------------
//	* NetworkChangeCallBack ()
//
// ---------------------------------------------------------------------------

boolean_t NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, void *callback_argument)
{                       
	if ( gSrvrCntl != nil )
	{
		gSrvrCntl->HandleNetworkTransition(); //ignore return status
	}
	return true;
}// NetworkChangeCallBack

// ---------------------------------------------------------------------------
//	* dsPMNotificationHandler ()
//
// ---------------------------------------------------------------------------

void dsPMNotificationHandler ( void *refContext, io_service_t service, natural_t messageType, void *notificationID )
{               
	//SRVRLOG1( kLogApplication, "dsPMNotificationHandler(): messageType=%d\n", messageType );
        
	switch (messageType)
	{
		case kIOMessageSystemWillSleep:
		case kIOMessageSystemWillPowerOff:
			//DBGLOG( kLogApplication, "dsPMNotificationHandler: kIOMessageSystemWillSleep OR kIOMessageSystemWillPowerOff" );
			//KW have server control go into "sleep" mode 
		break;

		case kIOMessageSystemWillNotSleep:
		case kIOMessageSystemHasPoweredOn:      
			//DBGLOG( kLogApplication, "dsPMNotificationHandler: kIOMessageSystemWillNotSleep OR kIOMessageSystemHasPoweredOn" );
			//KW have server control go into "wake" mode 
		break;

#ifdef DEBUG
		case kIOMessageServiceIsTerminated:
		case kIOMessageServiceIsSuspended:
		case kIOMessageServiceIsRequestingClose:
		case kIOMessageServiceWasClosed:
		case kIOMessageServiceIsResumed:        
		case kIOMessageServiceBusyStateChange:
		case kIOMessageCanDevicePowerOff:
		case kIOMessageDeviceWillPowerOff:
		case kIOMessageDeviceWillNotPowerOff:
		case kIOMessageDeviceHasPoweredOn:
		case kIOMessageCanSystemPowerOff:
		case kIOMessageSystemWillNotPowerOff:
		case kIOMessageCanSystemSleep:
#endif

		default:
			//SRVRLOG( kLogApplication, "dsPMNotificationHandler(): called but nothing done" );
			break;

	}
} // dsPMNotificationHandler


// ---------------------------------------------------------------------------
//	* main ()
//
// ---------------------------------------------------------------------------

int main ( int argc, char * const *argv )
{
#if DEBUG
	bool				bDebug			= true;
	OptionBits			debugOpts		= kLogEverything;
	bool				bProfiling		= true;
	OptionBits			profileOpts		= kLogEverything;
#else
	bool				bDebug			= false;
	OptionBits			debugOpts		= kLogMeta;
	bool				bProfiling		= false;
	OptionBits			profileOpts		= kLogMeta;
#endif
	bool				bDaemonize		= true;
	kern_return_t		machErr			= eDSNoErr;
	mach_port_t			aPort			= 0;
	mach_port_t			bPort			= 0;
	char			   *p				= nil;
	bool				bFound			= false;
	SCDynamicStoreRef	scdStore		= 0;
	Boolean				scdStatus		= FALSE;
	pid_t				pidval1			= -1;
	pid_t				pidval2			= -1;
	struct sigaction	sSigAction;
	struct sigaction	sSigOldAction;
	sigset_t			sSigMask		= 0;


	if ( argc > 1 )
	{
		p = strstr( argv[1], "-" );
		
		if ( strstr( argv[1], "daemon:DirectoryService") )
		{
			//configd Kicker no longer uses any argument for starting DirectoryService
			bDaemonize = true;
		}
		else if ( p != nil )
		{
			if ( strstr( p, "appledebug" ) )
			{
				// Turn debugging on
				bFound		= true;
				bDaemonize	= false;
				bDebug		= true;
				debugOpts	= kLogEverything;
			}

			if ( strstr( p, "applenodaemon" ) )
			{
				bFound		= true;
				bDaemonize	= false;
			}

			if ( strstr( p, "appleperformance" ) )
			{
				// future capability currently not implemented
				bFound		= true;
				bProfiling	= true;
				profileOpts	= kLogEverything;
			}

			if ( strstr( p, "appleversion" ) )
			{
				_AppleVersion( stdout );
				::exit( 1 );
			}

			if ( strstr( p, "appleoptions" ) )
			{
				_AppleOptions( stdout, argv[0] );
				::exit( 1 );
			}

			if ( strstr( p, "appleframework" ) )
			{
				// this will ensure that we are launched in the global Mach port space via configd kicker
				scdStore = SCDynamicStoreCreate(NULL, CFSTR("DirectoryService"), NULL, NULL); //KW use constant string instead
				if (scdStore != 0)
				{
					scdStatus = SCDynamicStoreNotifyValue(scdStore, CFSTR("daemon:DirectoryService"));
					CFRelease(scdStore);
					if (scdStatus)
					{
						exit(0);
					}
				}
				//if there is an error then we simply fall through as before
				bFound = true;
				printf("DirectoryService: Continuing after failing to relaunch itself via configd kicker.\n");
				printf("DirectoryService: No guarantee that process in in the global mach port space.\n");
			}


			if ( strstr( p, "v" ) )
			{
				_Version( stdout );
				::exit( 1 );
			}

			if ( strstr( p, "h" ) )
			{
				::_Usage( stderr, argv[0] );
				::exit( 1 );
			}

			if ( bFound == false )
			{
				::_Usage( stderr, argv[0] );
				::exit( 1 );
			}
		}
		else
		{
			::_Usage( stderr, argv[0] );
			::exit( 1 );
		}
	}

	// Is the server already running?
	machErr = task_get_bootstrap_port( mach_task_self(), &aPort );
	if ( machErr != eDSNoErr )
	{
		printf("Unable to launch DirectoryService server.\n");
		printf("  Error: task_get_bootstrap_port() failed: %s \n", mach_error_string( machErr ));
		exit( 0 );
	}

	machErr = ::bootstrap_look_up( aPort, (char *)"DirectoryService", &bPort );
	if ( machErr == eDSNoErr )
	{
		printf("DirectoryService server is already running.\n");
		printf("Unable to bind to mach port.\n");
		printf("Terminating this process.\n");
		exit( 0 );
	}

	//don't init the logging since there is some parent-child conflicts early on for the locking of the logs
	
	// OK, the following hoops need some explanation:
	// When a thread calls fork(), all other pthreads are lost. (It is not
	// clear whether the thread structures are left laying around but never
	// scheduled or if they are immediately terminated in the child.) This has
	// serious implications for server processes that have a thread listening
	// on a port when daemonizing: the port is bound but the binding thread
	// has disappeared!
	// To avoid this problem, the parent forks the child and blocks waiting
	// for a signal from the child. After the child performs its
	// initialization (which will likely spin threads), the child will either
	// exit with an error or send a SIGTERM to the parent; the former
	// indicates a startup error, the latter success.

	if ( bDaemonize == true )
	{
		::memset( &sSigAction, 0, sizeof( struct sigaction ) );
		::memset( &sSigOldAction, 0, sizeof( struct sigaction ) );
	
		sSigAction.sa_mask	= 0;
		sSigAction.sa_flags = 0;
		sigemptyset( &sSigAction.sa_mask );
		sSigAction.sa_handler = (void (*) (int))_HandleSigTermInParent;
		::sigaction( SIGTERM, &sSigAction, &sSigOldAction );
	
		sSigAction.sa_handler = (void (*) (int))_HandleSigTermInParent;
		sSigAction.sa_flags = 0;
		::sigaction( SIGABRT, &sSigAction, &sSigOldAction );
	
		sSigAction.sa_handler = (void (*) (int))_HandleSigChildInParent;
		sSigAction.sa_flags |= SA_NOCLDSTOP;
		::sigaction( SIGCHLD, &sSigAction, &sSigOldAction );
	
		sSigAction.sa_handler = (void (*) (int))SIG_IGN;
		sSigAction.sa_flags = 0;
		::sigaction( SIGPIPE, &sSigAction, &sSigOldAction );
	
		sigemptyset( &sSigMask );

		// Parent process waits for child to exit or for child to
		// terminate parent, signaling all is well.

		_ChildPid = ::fork();
		
		if ( 0 < _ChildPid )
		{
			if ( bDebug == true )
			{
				printf("Child forked, new pid is %d \n", (int)_ChildPid);
			}

			while ( !_Terminate )
			{
				::sigsuspend( &sSigMask );
			}

			if ( bDebug == true )
			{
				::printf( "Exiting; child status %x.\n", _ChildStatus );
			}

			::exit( _ChildStatus );
			return( _ChildStatus );
		}
		else if ( _ChildPid == -1 )
		{
			::printf( "fork() failed: errno= %d \n", (int)errno );
			::exit (errno);
			return errno;
		}
		else if ( _ChildPid == 0 )
		{
			// All is well so terminate parent
			pidval1 = ::getpid();
			::setpgid( 0, pidval1 );
			pidval2 = ::getppid();
			if (pidval2 != -1)
			{
				::kill( pidval2, SIGTERM );
			}
		}

		execl( "/usr/sbin/", "DirectoryService", "-applenodaemon", NULL );
		
		if ( bDebug == true )
		{
			::sleep( 20 );
		}
	}
	else
	{
		::fprintf( stderr, "Daemonization off.\n" );
	}

	try
	{
		//set the environment variable for the SMB plugin in this single thread
		//so that nmblookup will pass back encoded strings
		setenv( "__APPLE_NMBLOOKUP_HACK_2987131", "", 0 );
		
		// Open the log files
		CLog::Initialize( kLogEverything, kLogEverything, debugOpts, profileOpts, bDebug, bProfiling );
		SRVRLOG( kLogApplication, "\n\n" );
		SRVRLOG2( kLogApplication, "DirectoryService %s (v%s) starting up...",
					COSUtils::GetStringFromList( kSysStringListID, kStrVersion ),
					COSUtils::GetStringFromList( kSysStringListID, kStrBuildNumber ) );
		
		mach_port_limits_t	limits = { 1 };
		CFMachPortRef		port;
		
		port = CFMachPortCreate(NULL,(CFMachPortCallBack)SignalMessageHandler,NULL,NULL);
		CFRunLoopAddSource(	CFRunLoopGetCurrent(),
							CFMachPortCreateRunLoopSource(NULL,port,0),
							kCFRunLoopCommonModes);
		
		gSignalPort = CFMachPortGetPort(port);
		mach_port_set_attributes(	mach_task_self(),
									gSignalPort,
									MACH_PORT_LIMITS_INFO,
									(mach_port_info_t)&limits,
									sizeof(limits) / sizeof(natural_t));
		
		//handle SIGTERM, SIGHUP, SIGUSR1, SIGUSR2, SIGABRT, SIGINT, SIGPIPE
		signal(SIGTERM,SignalHandler);
		signal(SIGHUP,SignalHandler);
		signal(SIGUSR1,SignalHandler);
		signal(SIGUSR2,SignalHandler);
		signal(SIGABRT,SignalHandler);
		signal(SIGINT,SignalHandler);
		signal(SIGPIPE,SignalHandler);

		//set the global for the CFRunLoopRef
		gServerRunLoop = CFRunLoopGetCurrent();
		
		// Do setup after parent is removed if daemonizing
		gSrvrCntl = new ServerControl();
		if ( gSrvrCntl == nil ) throw( (sInt32)eMemoryAllocError );

		sInt32 startSrvr;
		startSrvr = gSrvrCntl->StartUpServer();
		if ( startSrvr != eDSNoErr ) throw( startSrvr );

		::CFRunLoopRun();
		
		if ( gSrvrCntl != NULL )
		{
			SRVRLOG( kLogApplication, "Shutting down DirectoryService..." );
			gSrvrCntl->ShutDownServer();
		}

	}

	catch ( eAppError err )
	{
		DBGLOG2( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DBGLOG1( kLogApplication, "  ***main() error = %d.", err );
	}

	catch( ... )
	{
		// if we got here we are in trouble.
		DBGLOG2( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DBGLOG( kLogApplication, "  *** Caught an unexpected exception in main()!!!!" );
	}

	exit( 0 );

} // main
