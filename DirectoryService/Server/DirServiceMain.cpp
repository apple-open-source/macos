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
time_t	gSunsetTime		= time( nil);
//Used for Power Management
io_object_t			gPMDeregisterNotifier;
io_connect_t		gPMKernelPort;
CFRunLoopRef		gServerRunLoop = NULL;


// Static ---------------------------------------------------------------------

#warning VERIFY the version string before each distinct build submission
static const char* strDaemonAppleVersion = "1.8.2";
static const char* strDaemonBuildVersion = "257.1";

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
	::fprintf( fp, _szpUsage, strDaemonAppleVersion );

} // _Version


// ---------------------------------------------------------------------------
//	* _AppleVersion ()
//
// ---------------------------------------------------------------------------

static void _AppleVersion ( FILE *fp )
{
	static const char * const	_szpUsage =
		"Apple Computer, Inc.  Version DirectoryService-%s\n";
	::fprintf( fp, _szpUsage, strDaemonBuildVersion );

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
//	* NetworkChangeCallBack ()
//
// ---------------------------------------------------------------------------

void NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *callback_argument)
{                       
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
	kern_return_t		machErr			= eDSNoErr;
	mach_port_t			serverPort		= MACH_PORT_NULL;
	bool				bDebugMode		= false;


//	struct rlimit rlp;

//	rlp.rlim_cur = RLIM_INFINITY;       /* current (soft) limit */
//	rlp.rlim_max = RLIM_INFINITY;       /* hard limit */
//	(void)setrlimit(RLIMIT_CORE, &rlp);

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
		
	openlog("DirectoryService", LOG_NDELAY | LOG_PID, LOG_DAEMON);

        if ( ourUID != 0 )
        {
                syslog(LOG_ALERT, "DirectoryService needs to be launched as root.\n");
                ::exit( 1 );
        }

	syslog(LOG_INFO,"Launched version %s (v%s)", strDaemonAppleVersion, strDaemonBuildVersion );

	if (!bDebugMode)
	{
		mach_port_t			mach_init_port		= MACH_PORT_NULL;
		mach_port_t			send_port			= MACH_PORT_NULL;
		mach_port_t			priv_bootstrap_port	= MACH_PORT_NULL;
		sIPCMsg				msg;

		// Is the server already running? - check the server port that is used by the clients
		machErr = ::bootstrap_look_up( bootstrap_port, kDSServiceName, &serverPort );
		if ( machErr == eDSNoErr )
		{
			syslog(LOG_ALERT, "DirectoryService server is already running since its mach server port is already registered.\n");
			syslog(LOG_ALERT, "Terminating this instantiated DirectoryService process.\n");
			exit( 0 );
		}

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
		status = bootstrap_check_in(bootstrap_port, kDSStdMachPortName, &mach_init_port);
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
					status = bootstrap_check_in(priv_bootstrap_port, kDSStdMachPortName, &mach_init_port);
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

		// receive the kickoff message and promptly ignore it so that mach_init will not immediately restart
		// the daemon unless a DS client actually requests it
		status = mach_msg( (mach_msg_header_t *)&msg, MACH_RCV_MSG | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0), 0, sizeof( sIPCMsg ), mach_init_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL );
		if ( status == MACH_MSG_SUCCESS )
		{
			//success in receiving kicker message
		}
		else
		{
			//failure in receiving kicker message but should never get here anyways
		}
					
        // We have no intention of responding to requests on the mach init port.
		// We are just using this mechanism for relaunch facilities.
        // So, we can dispose of all the rights we have for the mach init port.
		if (mach_init_port != MACH_PORT_NULL)
		{
			mach_port_destroy(mach_task_self(), mach_init_port);
			mach_init_port = MACH_PORT_NULL;
		}
	}


	try
	{
		// need to make sure this file is not present yet
		unlink( "/Library/Preferences/DirectoryService/.DSRunningSP4" );

		//global set to determine different behavior dependant on server build versus desktop
		if (stat( "/System/Library/CoreServices/ServerVersion.plist", &statResult ) == eDSNoErr)
		{
			gServerOS = true;
		}

		if (!gDebugLogging && stat( "/Library/Preferences/DirectoryService/.DSLogDebugAtStart", &statResult ) == eDSNoErr)
		{
			gDebugLogging = true;
			
		}
		
		// Open the log files
		CLog::Initialize( kLogEverything, kLogEverything, debugOpts, profileOpts, gDebugLogging, bProfiling );

		SRVRLOG( kLogApplication, "\n\n" );
		SRVRLOG2( kLogApplication,	"DirectoryService %s (v%s) starting up...",
                                    strDaemonAppleVersion,
                                    strDaemonBuildVersion );
		
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
