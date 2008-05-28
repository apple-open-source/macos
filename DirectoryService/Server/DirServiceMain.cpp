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
#include <asl.h>

#define USE_SYSTEMCONFIGURATION_PUBLIC_APIS
#include <SystemConfiguration/SystemConfiguration.h>	//required for the configd kicker operation
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>		//required for power management handling
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_init.h>
#include <mach/port.h>

#if HAVE_CORE_SERVER
#include <XSEventPort.h>
#endif

#include "DirServiceMain.h"
#include "ServerControl.h"
#include "CLog.h"
#include "PrivateTypes.h"
#include "DirServicesPriv.h"
#include "DirServicesConst.h"
#include "CPlugInList.h"
#include "CHandlers.h"

#include "DirServicesTypes.h"
#include "CDSLocalPlugin.h"
#include "COSUtils.h"
#include "buildnumber.h"

#define kDSPIDFile			"/var/run/DirectoryService.pid"
#define kDSRunningFile		"/Library/Preferences/DirectoryService/.DSIsRunning"

using namespace std;

dsBool	gServerOS				= false;	//indicates whether this is running on Server or not
dsBool	gLogAPICalls			= false;
dsBool	gDebugLogging			= false;
dsBool	gDSFWCSBPDebugLogging   = false;
dsBool	gIgnoreSunsetTime		= false;
dsBool	gDSDebugMode			= false;
dsBool	gDSLocalOnlyMode		= false;
dsBool	gDSInstallDaemonMode	= false;
dsBool	gProperShutdown			= false;
dsBool	gSafeBoot				= false;
CFAbsoluteTime	gSunsetTime		= 0;

#if HAVE_CORE_SERVER
XSEventPortRef	gEventPort		= NULL;
#else
void			*gEventPort		= NULL;
#endif

//Used for Power Management
io_object_t			gPMDeregisterNotifier;
io_connect_t		gPMKernelPort;
CFRunLoopRef		gPluginRunLoop = NULL;	// this is not our main runloop, this is our plugin runloop
DSMutexSemaphore    *gKerberosMutex = NULL;
mach_port_t			gMachMIGSet = MACH_PORT_NULL;
DSEventSemaphore	gPluginRunLoopEvent;

extern CDSLocalPlugin	*gLocalNode;

#warning VERIFY the version string before each software release
const char* gStrDaemonAppleVersion = "5.3"; //match this with x.y in 10.x.y

const char* gStrDaemonBuildVersion = "unlabeled/engineering";

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
														gSunsetTime, // set timer a little ahead
														0,
														0,
														0,
														LoggingTimerCallBack,
														NULL );
		
		// this does not block the runloop
		CFRunLoopAddTimer( CFRunLoopGetMain(), timer, kCFRunLoopDefaultMode );
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
	else if (msg->signum == SIGPIPE || msg->signum == SIGURG)
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
		"Version %s (build %s)\n";
	::fprintf( fp, _szpUsage, gStrDaemonAppleVersion, gStrDaemonBuildVersion );

} // _Version


// ---------------------------------------------------------------------------
//	* _AppleVersion ()
//
// ---------------------------------------------------------------------------

static void _AppleVersion ( FILE *fp )
{
	static const char * const	_szpUsage =
		"DirectoryService-%s\n";
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
		"	-appleversion   	Display the Apple build version.\n"
		"	-v              	Display the release version.\n"
		"	-localonly			Separate daemon runs with only the local node accessible.\n";
	::fprintf( fp, _szpUsage, argv0 );
} // _AppleOptions

// ---------------------------------------------------------------------------
//	* LoggingTimerCallBack ()
//
// ---------------------------------------------------------------------------

void LoggingTimerCallBack( CFRunLoopTimerRef timer, void *info )
{
	if ( gLogAPICalls && !gIgnoreSunsetTime )
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
	bool bNotify = false;
	
	for( CFIndex i=0; i<CFArrayGetCount(changedKeys); i++ )
	{
		char		keyName[256];
		
		CFStringGetCString( (CFStringRef)CFArrayGetValueAtIndex( changedKeys, i ), keyName, sizeof(keyName), kCFStringEncodingUTF8 );
		
		// we do not care about lo0 changes
		if (strstr(keyName, "/lo0/") != NULL)
		{
			DbgLog( kLogApplication, "NetworkChangeCallBack key: %s - skipping loopback", keyName );
			continue;
		}

		DbgLog( kLogApplication, "NetworkChangeCallBack key: %s", keyName );

		bNotify = true;
	}
	
	if ( gSrvrCntl != nil && bNotify )
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
	//SrvrLog( kLogApplication, "dsPMNotificationHandler(): messageType=%d\n", messageType );
        
	switch (messageType)
	{
		case kIOMessageSystemWillPowerOn:      
			DbgLog( kLogApplication, "dsPMNotificationHandler(): kIOMessageSystemWillPowerOn\n" );
			gSrvrCntl->HandleSystemWillPowerOn();
			break;

		case kIOMessageSystemHasPoweredOn:
			break;
			
		case kIOMessageSystemWillSleep:
			DbgLog( kLogApplication, "dsPMNotificationHandler(): kIOMessageSystemWillSleep\n" );
			gSrvrCntl->HandleSystemWillSleep();

		case kIOMessageSystemWillPowerOff:
		case kIOMessageCanSystemSleep:
		case kIOMessageCanSystemPowerOff:
		case kIOMessageCanDevicePowerOff:
            IOAllowPowerChange(gPMKernelPort, (SInt32)notificationID);	// don't want to slow up machine from going to sleep
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
			//SrvrLog( kLogApplication, "dsPMNotificationHandler(): called but nothing done" );
			break;

	}
} // dsPMNotificationHandler

// ---------------------------------------------------------------------------
//	* dsPostEvent ()
//
// ---------------------------------------------------------------------------

int dsPostEvent( CFStringRef inEventType, CFDictionaryRef inEventData )
{
#if HAVE_CORE_SERVER
	return ((gEventPort != NULL) ? XSEventPortPostEvent(gEventPort, inEventType, inEventData) : -1);
#else
	return 0;
#endif
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

	if ( sizeof(BUILDNUMBER) > sizeof("") )
		gStrDaemonBuildVersion = BUILDNUMBER;

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
				bFound			= true;
				debugOpts		= kLogEverything;
				gDebugLogging	= true;
				gDSDebugMode	= true;
			}
			
			if ( strstr( p, "localonly" ) && ourUID == 0 )
			{
				bFound				= true;
				gDSLocalOnlyMode	= true;
			}

			if ( strstr( p, "installdaemon" ) && ourUID == 0 )
			{
				bFound				= true;
				gDSInstallDaemonMode= true;
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
		exit( 1 );
	}

	syslog(LOG_ALERT,"Launched version %s (v%s)", gStrDaemonAppleVersion, gStrDaemonBuildVersion );
	
	mach_port_t			send_port			= MACH_PORT_NULL;
	mach_port_t			priv_bootstrap_port	= MACH_PORT_NULL;
	mach_port_t			tempMachPort		= MACH_PORT_NULL;
	int					status				= eDSNoErr;

	mach_port_allocate( mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &gMachMIGSet );

	if (!gDSDebugMode)
	{
		char* usedPortName = nil;
	
		if (gDSLocalOnlyMode) //set up parameters to differentiate the daemon
		{
			usedPortName = kDSStdMachLocalPortName;
		}
		else
		{
			usedPortName = kDSStdMachPortName;
		}

        /*
         * See if our service name is already registered and if we have privilege to check in.
         */
		status = bootstrap_check_in(bootstrap_port, usedPortName, &tempMachPort);
		if ( status == BOOTSTRAP_SUCCESS )
		{
			mach_port_move_member( mach_task_self(), tempMachPort, gMachMIGSet );
			tempMachPort = MACH_PORT_NULL;

			if ( !gDSLocalOnlyMode )
			{
				// checkin for our libinfo name
				status = bootstrap_check_in(bootstrap_port, kDSStdMachDSLookupPortName, &tempMachPort);
				if ( status == BOOTSTRAP_SUCCESS )
				{
					mach_port_move_member( mach_task_self(), tempMachPort, gMachMIGSet );
					tempMachPort = MACH_PORT_NULL;
				}
			}
		}

		if (status == BOOTSTRAP_SERVICE_ACTIVE)
		{
			syslog(LOG_ALERT, "DirectoryService %s instance is already running - exiting this instance", usedPortName );
			exit(0);
		}
		else if ( (status != BOOTSTRAP_SUCCESS) && gDSLocalOnlyMode )
		{
			syslog(LOG_ALERT, "bootstrap_check_in() for mach_init local port returned BOOTSTRAP_UNKNOWN_SERVICE so we will create our own portset" );
			//immediate and not on demand launch
			status = bootstrap_create_server(bootstrap_port, "/usr/sbin/DirectoryService -localonly", 0, true, &priv_bootstrap_port);
			if (status == KERN_SUCCESS)
			{
				status = bootstrap_create_service(priv_bootstrap_port, kDSStdMachLocalPortName, &send_port);
				if (status == KERN_SUCCESS)
				{
					status = bootstrap_check_in(priv_bootstrap_port, kDSStdMachLocalPortName, &tempMachPort);
					if (status != KERN_SUCCESS)
					{
						syslog(LOG_ALERT, "unable to bootstrap_check_in our own debug portset - exiting" );
						exit(0);
					}
					
					mach_port_move_member( mach_task_self(), tempMachPort, gMachMIGSet );
					tempMachPort = MACH_PORT_NULL;
				}
				else
				{
					syslog(LOG_ALERT, "unable to bootstrap_create_service our own debug portset - exiting" );
					exit(0);
				}
			}
		}
		else if (status != BOOTSTRAP_SUCCESS) //we should never get here
		{
			syslog(LOG_ALERT, "launchd has failed to launch DirectoryService %s instance - exiting this instance with error <%d>", usedPortName, status );
			exit(0);
		}
	}
	else // this is only debug mode, we don't error anything
	{
        /*
         * See if our service name is already registered and if we have privilege to check in.
		 * This should never work for debug mode. - expect to get BOOTSTRAP_UNKNOWN_SERVICE
         */
		status = bootstrap_check_in(bootstrap_port, kDSStdMachDebugPortName, &tempMachPort);
		if (status == BOOTSTRAP_SUCCESS)
		{
			mach_port_move_member( mach_task_self(), tempMachPort, gMachMIGSet );
			tempMachPort = MACH_PORT_NULL;

			status = bootstrap_check_in(bootstrap_port, kDSStdMachDSLookupPortName, &tempMachPort);
			if (status == BOOTSTRAP_SUCCESS)
			{
				mach_port_move_member( mach_task_self(), tempMachPort, gMachMIGSet );
				tempMachPort = MACH_PORT_NULL;
			}
		}
		
		if (status == BOOTSTRAP_SERVICE_ACTIVE)
		{
			syslog(LOG_ALERT, "DirectoryService debug instance is already running - exiting this instance" );
			exit(0);
		}
		else if (status == BOOTSTRAP_UNKNOWN_SERVICE)
		{
			syslog(LOG_ALERT, "bootstrap_check_in() for mach_init debug port returned BOOTSTRAP_UNKNOWN_SERVICE so we will create our own portset" );
			
			//immediate and not on demand launch
			status = bootstrap_create_server(bootstrap_port, "/usr/sbin/DirectoryService", 0, false, &priv_bootstrap_port);
			if (status == KERN_SUCCESS)
			{
				status = bootstrap_create_service(priv_bootstrap_port, kDSStdMachDebugPortName, &send_port);
				if (status == KERN_SUCCESS)
				{
					status = bootstrap_check_in(priv_bootstrap_port, kDSStdMachDebugPortName, &tempMachPort);
					if (status != KERN_SUCCESS)
					{
						syslog(LOG_ALERT, "unable to create our own debug portset - exiting" );
						exit(0);
					}
					
					mach_port_move_member( mach_task_self(), tempMachPort, gMachMIGSet );
					tempMachPort = MACH_PORT_NULL;

					status = bootstrap_check_in(priv_bootstrap_port, kDSStdMachDSLookupPortName, &tempMachPort);
					if (status != KERN_SUCCESS)
					{
						status = bootstrap_create_service(priv_bootstrap_port, kDSStdMachDSLookupPortName, &send_port);
						if (status == KERN_SUCCESS)
						{
							status = bootstrap_check_in(priv_bootstrap_port, kDSStdMachDSLookupPortName, &tempMachPort);
							if (status != KERN_SUCCESS)
							{
								syslog(LOG_ALERT, "unable to create our own debug portset - exiting" );
								exit(0);
							}

							mach_port_move_member( mach_task_self(), tempMachPort, gMachMIGSet );
							tempMachPort = MACH_PORT_NULL;
						}
					}
				}
			}
		}
	}

	try
	{
		if (!gDSLocalOnlyMode)
		{
			// need to make sure this file is not present yet
			unlink( "/var/run/.DSRunningSP4" );
		}

		//global set to determine different behavior dependant on server build versus desktop
		if (stat( "/System/Library/CoreServices/ServerVersion.plist", &statResult ) == eDSNoErr)
		{
			gServerOS = true;
		}

		// if not properly shut down, the SQL index for the local node needs to be deleted
		// we look for the pid and the special file since /var/run gets cleaned at boot
		// and we could have crashed just as we were shutting down
		if ( gDSLocalOnlyMode || gDSInstallDaemonMode || (stat(kDSPIDFile, &statResult) != 0 &&
			stat(kDSRunningFile, &statResult) != 0) )
		{
			// file not present, last shutdown was normal
			gProperShutdown = true;
		}
			
		if ( !gDSLocalOnlyMode && !gDSInstallDaemonMode )
		{
			// create pid file
			char pidStr[256];
			int fd = open( kDSPIDFile, (O_CREAT | O_TRUNC | O_WRONLY | O_EXLOCK), 0644 );
			if ( fd != -1 )
			{
				snprintf( pidStr, sizeof(pidStr), "%d", getpid() );
				write( fd, pidStr, strlen(pidStr) );
				close( fd );
			}
			
			// let's not log if the file is there
			if ( stat(kDSRunningFile, &statResult) != 0 )
				dsTouch( kDSRunningFile );
		}
		
		if (!gDebugLogging && stat( "/Library/Preferences/DirectoryService/.DSLogDebugAtStart", &statResult ) == eDSNoErr)
		{
			gDebugLogging = true;
			debugOpts = kLogEverything;
		}
		
		if (gDSDebugMode)
		{
			debugOpts |= kLogDebugHeader;
		}
		
		// Open the log files
		CLog::Initialize( kLogEverything, kLogEverything, debugOpts, profileOpts, gDebugLogging, bProfiling, gDSLocalOnlyMode );

		SrvrLog( kLogApplication, "\n\n" );
		SrvrLog( kLogApplication,	"DirectoryService %s (v%s) starting up...",
                                    gStrDaemonAppleVersion,
                                    gStrDaemonBuildVersion );
		
		if ( gProperShutdown == false ) {
			DbgLog( kLogCritical, "Improper shutdown detected" );
			syslog( LOG_NOTICE, "Improper shutdown detected" );
		}
		
		int			sbmib[] = { CTL_KERN, KERN_SAFEBOOT };
		uint32_t	sb = 0;
		size_t		sbsz = sizeof(sb);
		
		if ( sysctl(sbmib, 2, &sb, &sbsz, NULL, 0) == -1 )
			gSafeBoot = false;
		
		if ( sb == true ) {
			gSafeBoot = true;
			SrvrLog( kLogApplication, "Safe Boot is enabled" );
		}
		
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
		
		//handle SIGTERM, SIGHUP, SIGUSR1, SIGUSR2, SIGABRT, SIGINT, SIGPIPE, SIGURG
		signal(SIGTERM,SignalHandler);
		signal(SIGHUP,SignalHandler);
		signal(SIGUSR1,SignalHandler);
		signal(SIGUSR2,SignalHandler);
		signal(SIGABRT,SignalHandler);
		signal(SIGINT,SignalHandler);
		signal(SIGPIPE,SignalHandler);
		signal(SIGURG,SignalHandler);
		
		// first thing we do is setup our plugin runloop for handling requests from plugins
		CPluginRunLoopThread *pluginRunLoopThread = new CPluginRunLoopThread();

		gPluginRunLoopEvent.ResetEvent();
		pluginRunLoopThread->StartThread();
		gPluginRunLoopEvent.WaitForEvent();
		
		//set up a mutex semaphore for all plugins using Kerberos
		gKerberosMutex = new DSMutexSemaphore("::gKerberosMutex");
		
		// Do setup after parent is removed if daemonizing
		gSrvrCntl = new ServerControl();
		if ( gSrvrCntl == nil ) throw( (SInt32)eMemoryAllocError );

		if ( gDebugLogging )
		{
			gSrvrCntl->ResetDebugging(); //ignore return status
		}

		// Create an XSEventPort for the adaptive firewall
#if HAVE_CORE_SERVER
		gEventPort = XSEventPortCreate( NULL );
#endif

		SInt32 startSrvr;
		startSrvr = gSrvrCntl->StartUpServer();
		if ( startSrvr != eDSNoErr ) throw( startSrvr );
		
		CFRunLoopRun();
		
		// stop our plugin runloop
		CFRunLoopStop( gPluginRunLoop );
		
		gLocalNode->CloseDatabases();
		
		if ( gSrvrCntl != NULL )
		{
			SrvrLog( kLogApplication, "Shutting down DirectoryService..." );
			gSrvrCntl->ShutDownServer();
		}
		
		if ( !gDSLocalOnlyMode && !gDSInstallDaemonMode )
		{
			fcntl( 0, F_FULLFSYNC ); // ensure FS is flushed before we remove the PID files
			
			dsRemove( kDSRunningFile );
			dsRemove( kDSPIDFile );
		}
	}

	catch ( SInt32 err )
	{
		DbgLog( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DbgLog( kLogApplication, "  ***main() error = %d.", err );
	}

	catch( ... )
	{
		// if we got here we are in trouble.
		DbgLog( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DbgLog( kLogApplication, "  *** Caught an unexpected exception in main()!!!!" );
	}

	exit( 0 );

} // main
