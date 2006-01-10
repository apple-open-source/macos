/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>		//required for power management handling
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_init.h>
#include <mach/port.h>

#include "DirServiceMain.h"
#include "ServerControl.h"
#include "CLog.h"
#include "PrivateTypes.h"
#include "DirServicesPriv.h"
#include "DirServicesConst.h"
#include "CPlugInList.h"

#include "DirServicesTypes.h"

using namespace std;

dsBool	gServerOS		= false;	//indicates whether this is running on Server or not
dsBool	gLogAPICalls	= false;
dsBool	gDebugLogging	= false;
dsBool	gDSFWCSBPDebugLogging   = false;
CFAbsoluteTime	gSunsetTime		= 0;
//Used for Power Management
io_object_t			gPMDeregisterNotifier;
io_connect_t		gPMKernelPort;
CFRunLoopRef		gServerRunLoop = NULL;
DSMutexSemaphore    *gKerberosMutex = NULL;
mach_port_t			gServerMachPort = MACH_PORT_NULL;

#warning VERIFY the version string before each distinct build submission
const char* gStrDaemonAppleVersion = "2.1";
const char* gStrDaemonBuildVersion = "352.1";

enum
{
	kSignalMessage		= 1000
};

typedef struct SignalMessage
{
	mach_msg_header_t	header;
	mach_msg_body_t		body;
	int					signum;
	mach_msg_audit_trailer_t	trailer;
} SignalMessage;

void LoggingTimerCallBack( CFRunLoopTimerRef timer, void *info );

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
	if ( gSrvrCntl != nil )
	{
		gSrvrCntl->ResetDebugging(); //ignore return status
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
		syslog(LOG_ALERT,"Logging of API Calls turned OFF after receiving USR2 signal.");
	}
	else
	{
		gLogAPICalls = true;
		gSunsetTime		= CFAbsoluteTimeGetCurrent() + 300;
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	kCFAllocatorDefault,
														gSunsetTime + 1, // set timer a little ahead
														0,
														0,
														0,
														LoggingTimerCallBack,
														NULL );
		
		CFRunLoopAddTimer( gServerRunLoop, timer, kCFRunLoopDefaultMode );
		CFRelease( timer );
		timer = NULL;
		syslog(LOG_ALERT,"Logging of API Calls turned ON after receiving USR2 signal.");
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
	else if (msg->signum == SIGPIPE)
	{
		//don't do anything for a SIGPIPE
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
	::fprintf( fp, _szpUsage, gStrDaemonAppleVersion );

} // _Version


// ---------------------------------------------------------------------------
//	* _AppleVersion ()
//
// ---------------------------------------------------------------------------

static void _AppleVersion ( FILE *fp )
{
	static const char * const	_szpUsage =
		"Apple Computer, Inc.  Version DirectoryService-%s\n";
	::fprintf( fp, _szpUsage, gStrDaemonBuildVersion );

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
		"	-appleoptions   	List these options.\n"
		"	-appleperformance   Log everything and run in the foreground.\n"
		"	-appleversion   	Display the Apple build version.\n"
		"	-v              	Display the release version.\n";
	::fprintf( fp, _szpUsage, argv0 );
} // _AppleOptions

// ---------------------------------------------------------------------------
//	* LoggingTimerCallBack ()
//
// ---------------------------------------------------------------------------

void LoggingTimerCallBack( CFRunLoopTimerRef timer, void *info )
{
	if ( gLogAPICalls && CFAbsoluteTimeGetCurrent() >= gSunsetTime )
	{
		gLogAPICalls	= false;
		syslog(LOG_CRIT,"Logging of API Calls automatically turned OFF at reaching sunset duration of five minutes.");
	}
}

// ---------------------------------------------------------------------------
//	* NetworkChangeCallBack ()
//
// ---------------------------------------------------------------------------

void NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *callback_argument)
{                       
	for( CFIndex i=0; i<CFArrayGetCount(changedKeys); i++ )
	{
		char		keyName[256];
		CFStringGetCString( (CFStringRef)CFArrayGetValueAtIndex( changedKeys, i ), keyName, sizeof(keyName), kCFStringEncodingUTF8 );
		DBGLOG1( kLogApplication, "NetworkChangeCallBack key: %s", keyName );
	}
	
	if ( gSrvrCntl != nil )
	{
		gSrvrCntl->HandleNetworkTransition(); //ignore return status
	}
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
		case kIOMessageSystemHasPoweredOn:
			DBGLOG( kLogApplication, "dsPMNotificationHandler(): kIOMessageSystemHasPoweredOn\n" );
			break;
		case kIOMessageSystemWillPowerOn:      
			DBGLOG( kLogApplication, "dsPMNotificationHandler(): kIOMessageSystemWillPowerOn\n" );
			gSrvrCntl->HandleSystemWillPowerOn();
			break;

		case kIOMessageSystemWillSleep:
			DBGLOG( kLogApplication, "dsPMNotificationHandler(): kIOMessageSystemWillSleep\n" );
			gSrvrCntl->HandleSystemWillSleep();

		case kIOMessageSystemWillPowerOff:
		case kIOMessageCanSystemSleep:
		case kIOMessageCanSystemPowerOff:
		case kIOMessageCanDevicePowerOff:
            IOAllowPowerChange(gPMKernelPort, (long)notificationID);	// don't want to slow up machine from going to sleep
		break;

		case kIOMessageSystemWillNotSleep:
		break;

#ifdef DEBUG
		case kIOMessageServiceIsTerminated:
		case kIOMessageServiceIsSuspended:
		case kIOMessageServiceIsRequestingClose:
		case kIOMessageServiceWasClosed:
		case kIOMessageServiceIsResumed:        
		case kIOMessageServiceBusyStateChange:
		case kIOMessageDeviceWillPowerOff:
		case kIOMessageDeviceWillNotPowerOff:
		case kIOMessageSystemWillNotPowerOff:
#endif

		default:
			//SRVRLOG( kLogApplication, "dsPMNotificationHandler(): called but nothing done" );
			break;

	}
} // dsPMNotificationHandler

int
sys_server_status(char *name)
{
	kern_return_t status;
	int active;

	status = bootstrap_status(bootstrap_port, name, &active);
	if (status == BOOTSTRAP_UNKNOWN_SERVICE) return 0;
	if (status != KERN_SUCCESS) return -1;

	return active;
}	

// ---------------------------------------------------------------------------
//	* main ()
//
// ---------------------------------------------------------------------------

int main ( int argc, char * const *argv )
{
#if DEBUG
	OptionBits			debugOpts		= kLogEverything;
	bool				bProfiling		= true;
	OptionBits			profileOpts		= kLogEverything;
#else
	OptionBits			debugOpts		= kLogMeta;
	bool				bProfiling		= false;
	OptionBits			profileOpts		= kLogMeta;
#endif
	char			   *p				= nil;
	bool				bFound			= false;
	struct stat			statResult;
	pid_t				ourUID			= ::getuid();
	bool				bDebugMode		= false;


//	struct rlimit rlp;

//	rlp.rlim_cur = RLIM_INFINITY;       /* current (soft) limit */
//	rlp.rlim_max = RLIM_INFINITY;       /* hard limit */
//	(void)setrlimit(RLIMIT_CORE, &rlp);
    
    // this changes the logging format to show the PID and to cause syslog to launch
    openlog( "DirectoryService", LOG_PID | LOG_NOWAIT, LOG_DAEMON );

	if ( argc > 1 )
	{
		p = strstr( argv[1], "-" );

		if ( p != NULL )
		{
			if ( strstr( p, "appledebug" ) && ourUID == 0 )
			{
				// Turn debugging on
				bFound		= true;
				debugOpts	= kLogEverything;
				gDebugLogging = true;
				bDebugMode = true;
			}

			if ( strstr( p, "appleperformance" ) && ourUID == 0 )
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
		
        if ( ourUID != 0 )
        {
                syslog(LOG_ALERT, "DirectoryService needs to be launched as root.\n");
                ::exit( 1 );
        }

	syslog(LOG_ALERT,"Launched version %s (v%s)", gStrDaemonAppleVersion, gStrDaemonBuildVersion );
	
	if (!bDebugMode)
	{
		mach_port_t			send_port			= MACH_PORT_NULL;
		mach_port_t			priv_bootstrap_port	= MACH_PORT_NULL;

		//check if mach_init has already launched DirectoryService
		int status = sys_server_status(kDSStdMachPortName);
		if (status == BOOTSTRAP_STATUS_ACTIVE)
		{
			syslog(LOG_ALERT, "DirectoryService is already running!\n");
			exit(0);
		}

        /*
         * See if our service name is already registered and if we have privilege to check in.
         */
		status = bootstrap_check_in(bootstrap_port, kDSStdMachPortName, &gServerMachPort);
		if (status == BOOTSTRAP_SUCCESS)
		{
			/*
			* If so, we must be a followup instance of an already defined server (i.e. mach_init).  In that case,
			* the bootstrap port we inherited from our parent is the server's privilege port, so set
			* that in case we have to unregister later (which requires the privilege port).
			*/
			priv_bootstrap_port = bootstrap_port;
		}
		else if (status == BOOTSTRAP_NOT_PRIVILEGED)
		{
			syslog(LOG_ALERT, "DirectoryService instance is already starting up - exiting this instance" );
			exit(0);
		}
		else if (status == BOOTSTRAP_SERVICE_ACTIVE)
		{
			syslog(LOG_ALERT, "DirectoryService instance is already running - exiting this instance" );
			exit(0);
		}
		else if (status == BOOTSTRAP_UNKNOWN_SERVICE)
		{
			syslog(LOG_ALERT, "bootstrap_check_in() for mach_init port returned BOOTSTRAP_UNKNOWN_SERVICE so we'll create our own portset" );
			//immediate and not on demand launch
			status = bootstrap_create_server(bootstrap_port, "/usr/sbin/DirectoryService", 0, false, &priv_bootstrap_port);
			if (status == KERN_SUCCESS)
			{
				status = bootstrap_create_service(priv_bootstrap_port, kDSStdMachPortName, &send_port);
				if (status == KERN_SUCCESS)
				{
					status = bootstrap_check_in(priv_bootstrap_port, kDSStdMachPortName, &gServerMachPort);
					if (status != KERN_SUCCESS)
					{
						syslog(LOG_ALERT, "unable to create our own portset - exiting" );
						exit(0);
					}
				}
			}
		}

		//we don't want to pass our priviledged bootstrap port along to any spawned helpers so...
        status = bootstrap_unprivileged(priv_bootstrap_port, &bootstrap_port);
        if (status != BOOTSTRAP_SUCCESS)
		{
			syslog(LOG_ALERT, "bootstrap_unprivileged() for bootstrap port did not return BOOTSTRAP_SUCCESS so forked processes may block restarts of DirectoryService" );
        }
        status = task_set_bootstrap_port(mach_task_self(), bootstrap_port);        
		if (status != BOOTSTRAP_SUCCESS)
		{
			syslog(LOG_ALERT, "task_set_bootstrap_port() for bootstrap port did not return BOOTSTRAP_SUCCESS so forked processes may block restarts of DirectoryService" );
        }
		
		// we are the real daemon by now, let's set ourselves for delayed termination.
		int		mib[6]		= { 0 };
		int		oldstate	= 0;
		size_t	oldsize		= 4;
		int		newstate	= 1;
		
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROCDELAYTERM;
		
		if (sysctl(mib, 2, &oldstate, &oldsize, &newstate, 4) < 0)
		{
			syslog(LOG_INFO, "cannot mark for delayed termination");
		}
	}


	try
	{
		// need to make sure this file is not present yet
		unlink( "/var/run/.DSRunningSP4" );

		//global set to determine different behavior dependant on server build versus desktop
		if (stat( "/System/Library/CoreServices/ServerVersion.plist", &statResult ) == eDSNoErr)
		{
			gServerOS = true;
		}

		if (!gDebugLogging && stat( "/Library/Preferences/DirectoryService/.DSLogDebugAtStart", &statResult ) == eDSNoErr)
		{
			gDebugLogging = true;
			debugOpts = kLogEverything;
		}
		
		// Open the log files
		CLog::Initialize( kLogEverything, kLogEverything, debugOpts, profileOpts, gDebugLogging, bProfiling );

		SRVRLOG( kLogApplication, "\n\n" );
		SRVRLOG2( kLogApplication,	"DirectoryService %s (v%s) starting up...",
                                    gStrDaemonAppleVersion,
                                    gStrDaemonBuildVersion );
		
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
		
		//set up a mutex semaphore for all plugins using Kerberos
		gKerberosMutex = new DSMutexSemaphore();
		
		// Do setup after parent is removed if daemonizing
		gSrvrCntl = new ServerControl();
		if ( gSrvrCntl == nil ) throw( (sInt32)eMemoryAllocError );

		if ( gDebugLogging )
		{
			gSrvrCntl->ResetDebugging(); //ignore return status
		}

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

	catch ( sInt32 err )
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
