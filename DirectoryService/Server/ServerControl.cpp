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
 * @header ServerControl
 */

#if DEBUG_SRVR
# include <stdio.h>			// for stderr, fprintf(), et al
#endif

#include "ServerControl.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "CHandlers.h"
#include "DSTCPListener.h"
#include "CMsgQueue.h"
#include "CRefTable.h"
#include "DSMutexSemaphore.h"
#include "DSCThread.h"
#include "CServerPlugin.h"
#include "CPluginHandler.h"
#include "CNodeList.h"
#include "CLog.h"
#include "CPluginConfig.h"
#include "SharedConsts.h"
#include "CFile.h"
#include "CAuditUtils.h"

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/notify.h>
#include <sys/stat.h>							//used for mkdir and stat
#include <servers/bootstrap.h>
#include <IOKit/pwr_mgt/IOPMLib.h>				//required for power management handling
#include <syslog.h>								// for syslog()
#include <time.h>								// for time
#include <bsm/libbsm.h>

// This is for MIG
extern "C" {
	#include "DirectoryServiceMIGServer.h"
}

//#include <membershipPriv.h>		// can't use this header in C++
extern "C" int mbr_reset_cache();	// for memberd flush cache call
extern "C" int _lookupd_port(int);	// for lookupd flush cache calls
extern "C" int _lookup_link(mach_port_t, char *, int *);
extern "C" int _lookup_one(mach_port_t, int, char *, int, char **, int *);
extern "C" int _lu_running();

extern void LoggingTimerCallBack( CFRunLoopTimerRef timer, void *info );

//power management
extern void dsPMNotificationHandler ( void *refContext, io_service_t service, natural_t messageType, void *notificationID );
extern io_object_t		gPMDeregisterNotifier;
extern io_connect_t		gPMKernelPort;

//network change
extern void NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *callback_argument);
extern CFRunLoopRef		gServerRunLoop;

extern CFAbsoluteTime	gSunsetTime;
extern dsBool			gLogAPICalls;
extern dsBool			gDebugLogging;
extern dsBool			gDSFWCSBPDebugLogging;

extern	bool			gServerOS;
extern mach_port_t		gServerMachPort;

// ---------------------------------------------------------------------------
//	* Globals
//
// ---------------------------------------------------------------------------

uInt32					gAPICallCount		= 0;
ServerControl		   *gSrvrCntl			= nil;
CRefTable			   *gRefTable			= nil;
CPlugInList			   *gPlugins			= nil;
CMsgQueue			   *gTCPMsgQueue		= nil;
CPluginConfig		   *gPluginConfig		= nil;
CNodeList			   *gNodeList			= nil;
CPluginHandler		   *gPluginHandler		= nil;

DSMutexSemaphore	   *gTCPHandlerLock			= new DSMutexSemaphore();	//mutex on create and destroy of CHandler threads
DSMutexSemaphore	   *gPerformanceLoggingLock = new DSMutexSemaphore();	//mutex on manipulating performance logging matrix
DSMutexSemaphore	   *gLazyPluginLoadingLock	= new DSMutexSemaphore();	//mutex on loading plugins lazily
DSMutexSemaphore	   *gHashAuthFailedMapLock  = new DSMutexSemaphore();   //mutex on failed shadow hash login table
DSMutexSemaphore	   *gMachThreadLock			= new DSMutexSemaphore();	//mutex on count of mig handler threads

uInt32					gDaemonPID;
uInt32					gDaemonIPAddress;
uInt32					gRefCountWarningLimit						= 500;
uInt32					gDelayFailedLocalAuthReturnsDeltaInSeconds  = 1;
uInt32					gMaxHandlerThreadCount						= kMaxHandlerThreads;
dsBool					gToggleDebugging							= false;
mach_port_t				gMachAPISet									= 0;
map<mach_port_t, pid_t>	gPIDMachMap;
char				   *gNIHierarchyTagString   = nil;
uInt32					gActiveMachThreads							= 0;
uInt32					gActiveLongRequests							= 0;
bool					gFirstNetworkUpAtBoot						= false;

//PFIXdsBool					gLocalNodeNotAvailable	= true;

static void DoSearchPolicyChange(CFRunLoopTimerRef timer, void *info);
void DoSearchPolicyChange(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((ServerControl *)info)->DoNodeSearchPolicyChange();
	}
}// DoSearchPolicyChange

static void DoLookupDaemonFlushCache(CFRunLoopTimerRef timer, void *info);
void DoLookupDaemonFlushCache(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((ServerControl *)info)->FlushLookupDaemonCache();
	}
}// DoLookupDaemonFlushCache

static void DoMemberDaemonFlushCache(CFRunLoopTimerRef timer, void *info);
void DoMemberDaemonFlushCache(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((ServerControl *)info)->FlushMemberDaemonCache();
	}
}// DoMemberDaemonFlushCache

static void DoNIAutoSwitchNetworkChange(CFRunLoopTimerRef timer, void *info);
void DoNIAutoSwitchNetworkChange(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((ServerControl *)info)->NIAutoSwitchCheck();
	}
}// DoNIAutoSwitchNetworkChange


CFStringRef SearchPolicyChangeCopyStringCallback( const void *item );
CFStringRef SearchPolicyChangeCopyStringCallback( const void *item )
{
	return CFSTR("SearchPolicyChange");
}

CFStringRef LookupDaemonFlushCacheCopyStringCallback( const void *item );
CFStringRef LookupDaemonFlushCacheCopyStringCallback( const void *item )
{
	return CFSTR("LookupDaemonFlushCache");
}

CFStringRef MemberDaemonFlushCacheCopyStringCallback( const void *item );
CFStringRef MemberDaemonFlushCacheCopyStringCallback( const void *item )
{
	return CFSTR("MemberDaemonFlushCache");
}

CFStringRef NetworkChangeNIAutoSwitchCopyStringCallback( const void *item );
CFStringRef NetworkChangeNIAutoSwitchCopyStringCallback( const void *item )
{
	return CFSTR("NetworkChangeNIAutoSwitchCheck");
}

void DoPeriodicTask(CFRunLoopTimerRef timer, void *info);

CFStringRef PeriodicTaskCopyStringCallback( const void *item );
CFStringRef PeriodicTaskCopyStringCallback( const void *item )
{
	return CFSTR("PeriodicTask");
}

#pragma mark -
#pragma mark MIG Support Routines

static void mig_spawnonceifnecessary( void )
{
	// if this is a long request and we have no more than the maximum active threads
	if( gActiveMachThreads < gMaxHandlerThreadCount )
	{
		CMigHandlerThread* aMigHandlerThread = new CMigHandlerThread(DSCThread::kTSMigHandlerThread, true);
		if (aMigHandlerThread != NULL)
			aMigHandlerThread->StartThread();
		//we don't keep a handle to the mig handler threads and don't check if they get created
	}
}

#pragma mark -
#pragma mark MIG Call Handler Routines

kern_return_t dsmig_do_checkUsernameAndPassword( mach_port_t server,
												 sStringPtr username,
												 sStringPtr password,
												 int32_t *result,
												 audit_token_t atoken )
{
	CRequestHandler handler;
	char *debugDataTag = NULL;
	
	mig_spawnonceifnecessary();

	gMachThreadLock->Wait();
	gActiveLongRequests++;
	gMachThreadLock->Signal();
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		pid_t	aPID;
		audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );

		debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "checkpw()", "Server" );
		DBGLOG2( kLogHandler, "%s : dsmig DAC : Username = %s", debugDataTag, username );
	}

#if USE_BSM_AUDIT
	uid_t			auidp;
	uid_t			euidp;
	gid_t			egidp;
	uid_t			ruidp;
	gid_t			rgidp;
	pid_t			pidp;
	au_asid_t		asidp;
	au_tid_t		tidp;
	audit_token_to_au32( atoken, &auidp, &euidp, &egidp, &ruidp, &rgidp, &pidp, &asidp, &tidp );
	char *textStr = nil;
	uInt32 bsmEventCode = AuditForThisEvent( kCheckUserNameAndPassword, username, &textStr );
#endif
		
	*result = handler.DoCheckUserNameAndPassword( username, password, eDSExact, NULL, NULL );
	
#if USE_BSM_AUDIT
	// BSM Audit
	if ( bsmEventCode > 0 )
	{
		token_t *tok;
		
		if ( *result == eDSNoErr )
		{
			tok = au_to_text( textStr );
			audit_write_success( bsmEventCode, tok,
									auidp,
									euidp,
									egidp,
									ruidp,
									rgidp,
									pidp,
									asidp,
									&tidp );
		}
		else
		{
			audit_write_failure( bsmEventCode, textStr, (int)*result,
									auidp,
									euidp,
									egidp,
									ruidp,
									rgidp,
									pidp,
									asidp,
									&tidp );
		}
	}
	DSFreeString( textStr );	// sets to NULL; required
#endif

	if ( debugDataTag )
	{
		DBGLOG3( kLogHandler, "%s : dsmig DAR : Username %s : Result code = %d", debugDataTag, username, *result );
		free( debugDataTag );
	}

	gMachThreadLock->Wait();
	gActiveLongRequests--;
	gMachThreadLock->Signal();
	
	return KERN_SUCCESS;
}

kern_return_t dsmig_do_create_api_session( mach_port_t server, mach_port_t *newServer, audit_token_t atoken )
{
	mach_port_t		oldTargetOfNotification	= MACH_PORT_NULL;
	
	(void) mach_port_allocate( mach_task_self(), MACH_PORT_RIGHT_RECEIVE, newServer );
	(void) mach_port_move_member( mach_task_self(), *newServer, gMachAPISet );
	
	// Request no-senders notification so we can tell when server dies
	(void) mach_port_request_notification( mach_task_self(), *newServer, MACH_NOTIFY_NO_SENDERS, 1, *newServer, MACH_MSG_TYPE_MAKE_SEND_ONCE, &oldTargetOfNotification );
	
	// let's get the audit data PID
	pid_t	aPID;
	audit_token_to_au32( atoken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );

	gMachThreadLock->Wait();
	gPIDMachMap[*newServer] = aPID;
	gMachThreadLock->Signal();

	return KERN_SUCCESS;
}

kern_return_t dsmig_do_api_call( mach_port_t server,
								 mach_msg_type_name_t serverPoly,
								 sComDataPtr msg_data,
								 mach_msg_type_number_t msg_dataCnt,
								 vm_offset_t msg_data_ool,
								 mach_msg_type_number_t msg_data_oolCnt,
								 sComDataPtr reply_msg,
								 mach_msg_type_number_t *reply_msgCnt,
								 vm_offset_t *reply_msg_ool,
								 mach_msg_type_number_t *reply_msg_oolCnt,
								 audit_token_t atoken )
{
	kern_return_t	kr			= KERN_FAILURE;
	sComDataPtr		pComData	= NULL;
	uInt32			uiLength	= 0;
	
	if( msg_dataCnt )
	{
		pComData = (sComDataPtr) msg_data;
		uiLength = msg_dataCnt;
	}
	else
	{
		pComData = (sComDataPtr) msg_data_ool;
		uiLength = msg_data_oolCnt;
	}
	
	// lets see if the packet is big enough.. and see if the length matches the msg_data size minus 1
	if( uiLength >= (sizeof(sComData) - 1) )
	{
		if( pComData->fDataLength == (uiLength - (sizeof(sComData) - 1)) )
		{
			// we need to copy because we will allocate/deallocate it in the handler
			//   but based on the size it thinks it is
			sComData *pRequest = (sComData *) calloc( sizeof(sComData) + pComData->fDataSize, 1 );
			CRequestHandler handler;
			
			bcopy( (void *)pComData, pRequest, uiLength );
			
			// let's get the audit data and add it to the sComData
			audit_token_to_au32( atoken, NULL, (uid_t *)&pRequest->fEffectiveUID, NULL, (uid_t *)&pRequest->fUID, NULL, (pid_t *)&pRequest->fPID, NULL, NULL );
			
			gMachThreadLock->Wait();
			gActiveLongRequests ++;
			gMachThreadLock->Signal();
			
			// spawn a thread request
			mig_spawnonceifnecessary();
			
			handler.HandleRequest( &pRequest );

			gMachThreadLock->Wait();
			gActiveLongRequests --;
			gMachThreadLock->Signal();
			
			// set the PID in the return to our PID for RefTable purposes
			pRequest->fPID = gDaemonPID;
			
			// if it will fit in the fixed buffer, use it otherwise use OOL
			if( sizeof(sComData) + pRequest->fDataLength <= *reply_msgCnt )
			{
				*reply_msgCnt = sizeof(sComData) + pRequest->fDataLength - 1;
				bcopy( pRequest, reply_msg, *reply_msgCnt );
				*reply_msg_oolCnt = 0;
			}
			else
			{
				*reply_msgCnt = 0; // ool, set the other to 0
				vm_read( mach_task_self(), (vm_address_t)pRequest, (sizeof(sComData) + pRequest->fDataLength - 1), reply_msg_ool, reply_msg_oolCnt );
			}

			// free our allocated request data...
			free( pRequest );
			pRequest = NULL;
			
			gAPICallCount++;
			
			if ( (gAPICallCount % 1023) == 1023 ) // every 1023 calls so we can do bit-wise check
			{
				if (gLogAPICalls)
				{
					syslog(LOG_CRIT,"API clients have called APIs %d times", gAPICallCount);
				}
			}

			kr = KERN_SUCCESS;
		}
		else
		{
			syslog( LOG_ALERT, "dsmig_do_api_call:  Bad message size %d, does not correlate with contents length %d + header %d", uiLength, pComData->fDataLength, (sizeof(sComData) - 1) );
		}
	}
	else
	{
		syslog( LOG_ALERT, "dsmig_do_api_call message is too small to be valid message %d < %d", uiLength, sizeof(sComData) - 1 );
	}
	
	if( msg_data_oolCnt )
	{
		vm_deallocate( mach_task_self(), msg_data_ool, msg_data_oolCnt );
	}
	
	return kr;
}

#pragma mark -
#pragma mark ServerControl Routines

// ---------------------------------------------------------------------------
//	* ServerControl()
//
// ---------------------------------------------------------------------------

ServerControl::ServerControl ( void )
{
    gDaemonPID			= getpid();
	//we use a non valid IP Address of zero for ourselves
	gDaemonIPAddress	= 0;

	fTCPListener				= nil;
	fTCPHandlerThreadsCnt		= 0;
	fSCDStore					= 0;	
	fPerformanceStatGatheringActive	= false; //default
	fTimeToCheckSearchPolicyChange	= 0;
	fTimeToCheckNIAutoSwitch	= 0;
	fTimeToCheckLookupDaemonCacheFlush	= 0;
	fLookupDaemonFlushCacheRequestCount	= 0;
	fTimeToCheckMemberDaemonCacheFlush	= 0;
	fMemberDaemonFlushCacheRequestCount	= 0;
	fHoldStore					= NULL;
	fTCPHandlers				= nil;
	
#ifdef BUILD_IN_PERFORMANCE
	fLastPluginCalled			= 0;
	fPerfTableNumPlugins		= 0;
	fPerfTable					= nil;
	
#if PERFORMANCE_STATS_ALWAYS_ON
	fPerformanceStatGatheringActive	= true;
#else
	fPerformanceStatGatheringActive	= false;
#endif
#endif

	fTCPHandlerSemaphore		= new DSSemaphore( fTCPHandlerThreadsCnt );
	
	fServiceNameString = CFStringCreateWithCString( NULL, kDSStdMachPortName, kCFStringEncodingUTF8 );
	
	if (gDaemonPID > 100) //assumption here is that we crashed and restarted so say netowrk is already running ie. we are usually less than 50
	{
		gFirstNetworkUpAtBoot = true;
	}
    
} // ServerControl


// ---------------------------------------------------------------------------
//	* ~ServerControl()
//
// ---------------------------------------------------------------------------

ServerControl::~ServerControl ( void )
{
	if ( fTCPHandlerSemaphore != nil )
	{
		delete( fTCPHandlerSemaphore );
		fTCPHandlerSemaphore = nil;
	}

} // ~ServerControl


// ---------------------------------------------------------------------------
//	* StartUpServer ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::StartUpServer ( void )
{
	sInt32				result		= eDSNoErr;
	struct stat			statResult;
	try
	{
		if ( gNodeList == nil )
		{
			gNodeList = new CNodeList();
			if ( gNodeList == nil ) throw((sInt32)eMemoryAllocError);
		}

		if ( gRefTable == nil )
		{
			gRefTable = new CRefTable( CHandlerThread::RefDeallocProc );
			if ( gRefTable == nil ) throw( (sInt32)eMemoryAllocError );
		}

		if ( gPluginConfig == nil )
		{
			gPluginConfig = new CPluginConfig();
			if ( gPluginConfig == nil ) throw( (sInt32)eMemoryAllocError );
			gPluginConfig->Initialize();
		}
		
		//gMaxHandlerThreadCount may be discovered in the DS plist config file read by gPluginConfig above
		fTCPHandlers = (CHandlerThread **)calloc(gMaxHandlerThreadCount, sizeof(CHandlerThread *));

		if ( gPlugins == nil )
		{
			gPlugins = new CPlugInList();
			if ( gPlugins == nil ) throw( (sInt32)eMemoryAllocError );
		}

		if ( gTCPMsgQueue == nil )
		{
			gTCPMsgQueue = new CMsgQueue();
			if ( gTCPMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if (::stat( "/Library/Preferences/DirectoryService/.DSLogAPIAtStart", &statResult ) == eDSNoErr)
		{
			gSunsetTime		= CFAbsoluteTimeGetCurrent() + 300;
			CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	kCFAllocatorDefault,
															gSunsetTime + 1,
															0,
															0,
															0,
															LoggingTimerCallBack,
															NULL );
			
			CFRunLoopAddTimer( gServerRunLoop, timer, kCFRunLoopDefaultMode );
			CFRelease( timer );
			timer = NULL;

			gLogAPICalls	= true;
			syslog(LOG_ALERT,"Logging of API Calls turned ON at Startup of DS Daemon.");
			gDebugLogging	= true;
			CLog::StartDebugLog();
			syslog(LOG_ALERT,"Debug Logging turned ON at Startup of DS Daemon.");
		}

		// let's start the MIG listener
		fMigListener = new CMigHandlerThread();
		if ( fMigListener == nil ) throw((sInt32)eMemoryAllocError);
		fMigListener->StartThread();
		
		// see if we need TCP too
		if ( 	(	(::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult ) == eDSNoErr) ||
					(gServerOS) ) &&
					(::stat( "/Library/Preferences/DirectoryService/.DSTCPNotListening", &statResult ) != eDSNoErr) )
		{
			// Start the TCP listener thread
			result = StartTCPListener(kDSDefaultListenPort);
			if ( result != eDSNoErr ) throw( result );
		}

		if ( gPluginHandler == nil )
		{
			gPluginHandler = new CPluginHandler();
			if ( gPluginHandler == nil ) throw((sInt32)eMemoryAllocError);

			//this call could throw
			gPluginHandler->StartThread();
		}

		result = RegisterForSystemPower();
		if ( result != eDSNoErr ) throw( result );

		result = (sInt32)RegisterForNetworkChange();
		if ( result != eDSNoErr ) throw( result );
		
		result = SetUpPeriodicTask();
		if ( result != eDSNoErr ) throw( result );

		//at boot we wait the same as a network transition for the NI auto switch check
		HandleMultipleNetworkTransitionsForNIAutoSwitch();
	}

	catch( sInt32 err )
	{
		result = err;
	}

	return( result );

} // StartUpServer


// ---------------------------------------------------------------------------
//	* ShutDownServer ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::ShutDownServer ( void )
{
	sInt32				result		= eDSNoErr;
	uInt32				i			= 0;
	uInt32				uiStopCnt	= 0;
	struct stat			statResult;

	try
	{

		result = (sInt32)UnRegisterForNetworkChange();
		if ( result != eDSNoErr ) throw( result );

//		result = UnRegisterForSystemPower();
//		if ( result != eDSNoErr ) throw( result );

		//fMigListener is stopped by destroying the port set
		mach_port_destroy( mach_task_self(), gMachAPISet );
		gMachAPISet = MACH_PORT_NULL;
		
		if (::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult ) == eDSNoErr)
		{
			//need to stop the TCP listener before anything else
			//assume that the listener itself will close all of its connections
			if (fTCPListener != nil)
			{
				fTCPListener->StopThread();
			
				gTCPHandlerLock->Wait();
				// Stop the handler threads
				for ( i = 0; i < gMaxHandlerThreadCount; i++ )
				{
					if ( fTCPHandlers[ i ] != nil )
					{
						uiStopCnt += 1;
		
						fTCPHandlers[ i ]->StopThread();
		
						fTCPHandlers[ i ] = nil;
					}
				}
				gTCPHandlerLock->Signal();
		
				while (uiStopCnt > 0)
				{
					WakeAHandler(DSCThread::kTSTCPHandlerThread);
					uiStopCnt--;
				}
		
				uiStopCnt	= 0;
			}
		}

		//no need to delete the global objects as this process is going away and
		//we don't want to create a race condition on the threads dying that
		//could lead to a crash
/*
		if ( gNodeList != nil )
		{
			delete( gNodeList );
			gNodeList = nil;
		}

		if ( gRefTable != nil )
		{
			delete( gRefTable );
			gRefTable = nil;
		}

		if ( gPlugins != nil )
		{
			delete( gPlugins );
			gPlugins = nil;
		}

		if ( gTCPMsgQueue != nil )
		{
			delete( gTCPMsgQueue );
			gTCPMsgQueue = nil;
		}

		if ( gMsgQueue != nil )
		{
			delete( gMsgQueue );
			gMsgQueue = nil;
		}

		if ( gInternalMsgQueue != nil )
		{
			delete( gInternalMsgQueue );
			gInternalMsgQueue = nil;
		}

		if ( gCheckpwMsgQueue != nil )
		{
			delete( gCheckpwMsgQueue );
			gCheckpwMsgQueue = nil;
		}
*/
		//no need to delete the global mutexes as this process is going away and
		//we don't want to create a reace condition on the threads dying that
		//could lead to a crash
		//delete(gTCPHandlerLock);
		//delete(gHandlerLock);
		//delete(gInternalHandlerLock);
		//delete(gCheckpwHandlerLock);
		//delete(gPerformanceLoggingLock);
		
		CLog::Deinitialize();
	}

	catch( sInt32 err )
	{
		result = err;
	}

	return( result );

} // ShutDownServer


// ---------------------------------------------------------------------------
//	* StartTCPListener ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::StartTCPListener ( uInt32 inPort )
{
	sInt32	result	= eDSNoErr;

	try
	{
		fTCPListener = new DSTCPListener(inPort);
		if ( fTCPListener == nil ) throw((sInt32)eMemoryAllocError);

		//this call could throw
		fTCPListener->StartThread();
	}

	catch( sInt32 err )
	{
		result = err;
		DBGLOG2( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DBGLOG1( kLogApplication, "  Caught exception = %d.", err );
	}

	return( result );

} // StartTCPListener


// ---------------------------------------------------------------------------
//	* StopTCPListener ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::StopTCPListener ( void )
{
	sInt32	result	= eDSNoErr;

	try
	{
		if ( fTCPListener == nil ) throw((sInt32)eMemoryAllocError);

		//this call could throw
		fTCPListener->StopThread();
	}

	catch( sInt32 err )
	{
		result = err;
		DBGLOG2( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DBGLOG1( kLogApplication, "  Caught exception = %d.", err );
	}

	return( result );

} // StopTCPListener


// ---------------------------------------------------------------------------
//	* StartAHandler ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl:: StartAHandler ( const FourCharCode inThreadSignature )
{
	volatile	uInt32		iThread;
				sInt32		result	= eDSNoErr;

	try
	{
		// If we have less than the max handlers then we add one
		//decide from which set of handlers to start one

		if (inThreadSignature == DSCThread::kTSTCPHandlerThread)
		{
			if ( (fTCPHandlerThreadsCnt >= 0) && (fTCPHandlerThreadsCnt < gMaxHandlerThreadCount) )
			{
				for (iThread =0; iThread < gMaxHandlerThreadCount; iThread++)
				{
					if (fTCPHandlers[ iThread ] == nil)
					{
						// Start a handler thread
						fTCPHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSTCPHandlerThread, iThread);
						if ( fTCPHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						fTCPHandlerThreadsCnt++;

						//this call could throw
						fTCPHandlers[ iThread ]->StartThread();
						break;
					}
					else if ( fTCPHandlers[iThread]->GetOurThreadRunState() == DSCThread::kThreadStop)
					{
						// Start a handler thread
						fTCPHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSTCPHandlerThread, iThread);
						if ( fTCPHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						//fTCPHandlerThreadsCnt++; //no need since replacing

						//this call could throw
						fTCPHandlers[ iThread ]->StartThread();
						break;
					}
				}
			}
		}

	}

	catch( sInt32 err )
	{
		result = err;
	}

	return( result );

} // StartAHandler


//--------------------------------------------------------------------------------------------------
//	* WakeAHandler()
//
//--------------------------------------------------------------------------------------------------

void ServerControl:: WakeAHandler ( const FourCharCode inThreadSignature )
{
	if (inThreadSignature == DSCThread::kTSTCPHandlerThread)
	{
		fTCPHandlerSemaphore->Signal();
	}
} // WakeAHandler


// ---------------------------------------------------------------------------
//	* StopAHandler ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl:: StopAHandler ( const FourCharCode inThreadSignature, uInt32 iThread, CHandlerThread *inThread )
{
	sInt32			result		= eDSNoErr;

	try
	{
//		DBGLOG2( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
//		DBGLOG2( kLogApplication, "StopAHandler: sig = %d and index = %d", inThreadSignature, iThread );
		if (inThreadSignature == DSCThread::kTSTCPHandlerThread)
		{
			if ( (iThread >= 0) && (iThread < gMaxHandlerThreadCount) )
			{
				if (fTCPHandlers[ iThread ] == inThread)
				{
					// Remove a handler thread from the list
					fTCPHandlers[ iThread ] = nil;

					fTCPHandlerThreadsCnt--;
				}
			}
		}
	}

	catch( sInt32 err )
	{
		result = err;
	}

	return( result );

} // StopAHandler


//--------------------------------------------------------------------------------------------------
//	* SleepAHandler(const FourCharCode inThreadSignature, uInt32 waitTime)
//
//--------------------------------------------------------------------------------------------------

void ServerControl:: SleepAHandler ( const FourCharCode inThreadSignature, uInt32 waitTime )
{
	if (inThreadSignature == DSCThread::kTSTCPHandlerThread)
	{
		fTCPHandlerSemaphore->Wait( waitTime );
	}
} // SleepAHandler


//--------------------------------------------------------------------------------------------------
//	* GetHandlerCount(const FourCharCode inThreadSignature)
//
//--------------------------------------------------------------------------------------------------

uInt32 ServerControl::GetHandlerCount ( const FourCharCode inThreadSignature )
{
	if (inThreadSignature == DSCThread::kTSTCPHandlerThread)
	{
		return fTCPHandlerThreadsCnt;
	}
	return 0;
} // GetHandlerCount


// ---------------------------------------------------------------------------
//	* RegisterForNetworkChange ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl:: RegisterForNetworkChange ( void )
{
	sInt32				scdStatus			= eDSNoErr;
	CFStringRef			ipKey				= 0;	//ip changes key
	CFStringRef			dhcpKey				= 0;	//DHCP changes key
	CFStringRef			niKey				= 0;	//NetInfo changes key
	CFMutableArrayRef	notifyKeys			= 0;
	CFMutableArrayRef	notifyPatterns		= 0;
	Boolean				setStatus			= FALSE;
	CFStringRef			aPIDString			= NULL;
	SCDynamicStoreRef	store				= NULL;
    CFRunLoopSourceRef	rls					= NULL;
	
	DBGLOG( kLogApplication, "RegisterForNetworkChange(): " );

	notifyKeys		= CFArrayCreateMutable(	kCFAllocatorDefault,
											0,
											&kCFTypeArrayCallBacks);
	notifyPatterns	= CFArrayCreateMutable(	kCFAllocatorDefault,
											0,
											&kCFTypeArrayCallBacks);
											
	// ip changes
	/* watch for IPv4 configuration changes (e.g. new default route) */
	ipKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
	CFArrayAppendValue(notifyKeys, ipKey);
	CFRelease(ipKey);

	/* watch for IPv4 interface configuration changes */
	ipKey = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);
	CFArrayAppendValue(notifyPatterns, ipKey);
	CFRelease(ipKey);

	//DHCP changes
	dhcpKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetDHCP);
	CFArrayAppendValue(notifyPatterns, dhcpKey);
	CFRelease(dhcpKey);
		
	// NetInfo changes
	niKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetNetInfo);
	CFArrayAppendValue(notifyKeys, niKey);
	CFRelease(niKey);
			
	//not checking bool return
	store = SCDynamicStoreCreate(NULL, fServiceNameString, NetworkChangeCallBack, NULL);
	if (store != NULL && notifyKeys != NULL && notifyPatterns != NULL)
	{
		SCDynamicStoreSetNotificationKeys(store, notifyKeys, notifyPatterns);
		rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
		if (rls != NULL)
		{
			CFRunLoopAddSource(gServerRunLoop, rls, kCFRunLoopDefaultMode);
			CFRelease(rls);
			rls = NULL;
		}
		else
		{
			syslog(LOG_ALERT,"Unable to add source to RunLoop for SystemConfiguration registration for Network Notification");
		}
		CFRelease(notifyKeys);
		notifyKeys = NULL;
		CFRelease(notifyPatterns);
		notifyPatterns = NULL;
		CFRelease(store);
		store = NULL;
	}
	else
	{
		syslog(LOG_ALERT,"Unable to create DirectoryService store for SystemConfiguration registration for Network Notification");
	}
	
	if (fHoldStore == NULL)
	{
		fHoldStore = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
	}

	if (fHoldStore != NULL)
	{
		//this is update code for things like NetInfo and DHCP issues
		//ie. calls to the search node that verify or re-establish the default NI and LDAPv3(from DHCP) nodes
		
		//send SIGHUP on a network transition
		//setStatus = SCDynamicStoreNotifySignal( fHoldStore, getpid(), SIGHUP);
		
		aPIDString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), (uInt32)getpid());
		if (aPIDString != NULL)
		{
			setStatus = SCDynamicStoreAddTemporaryValue( fHoldStore, CFSTR("DirectoryService:PID"), aPIDString );
			CFRelease(aPIDString);
		}
		else
		{
			syslog(LOG_ALERT,"Unable to create DirectoryService:PID string for SystemConfiguration registration - DSAgent will be disabled in lookupd");
		}
		//DO NOT release the store here since we use SCDynamicStoreAddTemporaryValue above
		//CFRelease(fHoldStore);
		//fHoldStore = NULL;
	}
	else
	{
		syslog(LOG_ALERT,"Unable to create DirectoryService store for SystemConfiguration registration of DirectoryService:PID string - DSAgent will be disabled in lookupd");
	}

	return scdStatus;
	
} // RegisterForNetworkChange

// ---------------------------------------------------------------------------
//	* UnRegisterForNetworkChange ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl:: UnRegisterForNetworkChange ( void )
{
	sInt32		scdStatus = eDSNoErr;

	DBGLOG( kLogApplication, "UnRegisterForNetworkChange(): " );

	return scdStatus;

} // UnRegisterForNetworkChange


// ---------------------------------------------------------------------------
//	* RegisterForSystemPower ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::RegisterForSystemPower ( void )
{
	IONotificationPortRef	pmNotificationPortRef;
	CFRunLoopSourceRef		pmNotificationRunLoopSource;
	
	DBGLOG( kLogApplication, "RegisterForSystemPower(): " );

	gPMKernelPort = IORegisterForSystemPower(this, &pmNotificationPortRef, dsPMNotificationHandler, &gPMDeregisterNotifier);
	if (gPMKernelPort == nil || pmNotificationPortRef == nil)
	{
		ERRORLOG( kLogApplication, "RegisterForSystemPower(): IORegisterForSystemPower failed" );
	}
	else
	{
		pmNotificationRunLoopSource = IONotificationPortGetRunLoopSource(pmNotificationPortRef);
		
		if (pmNotificationRunLoopSource == nil)
		{
			ERRORLOG( kLogApplication, "RegisterForSystemPower(): IONotificationPortGetRunLoopSource failed" );
			gPMKernelPort = nil;
		}
		else
		{
			CFRunLoopAddSource(gServerRunLoop, pmNotificationRunLoopSource, kCFRunLoopCommonModes);
		}
	}
	
	return (gPMKernelPort != nil) ? eDSNoErr : -1;
} // RegisterForSystemPower

// ---------------------------------------------------------------------------
//	* UnRegisterForSystemPower ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::UnRegisterForSystemPower ( void )
{
	sInt32 ioResult = eDSNoErr;

	DBGLOG( kLogApplication, "UnRegisterForSystemPower(): " );

	if (gPMKernelPort != nil) {
		gPMKernelPort = nil;
        ioResult = (sInt32)IODeregisterForSystemPower(&gPMDeregisterNotifier);
		if (ioResult != eDSNoErr)
		{
			DBGLOG1( kLogApplication, "UnRegisterForSystemPower(): IODeregisterForSystemPower failed, error= %d", ioResult );
		}
    }
   return ioResult;
} // UnRegisterForSystemPower


// ---------------------------------------------------------------------------
//	* HandleSystemWillSleep ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::HandleSystemWillSleep ( void )
{
	sInt32						siResult		= eDSNoErr;
	uInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	SRVRLOG( kLogApplication, "Sleep Notification occurred.");
	
	aHeader.fType			= kHandleSystemWillSleep;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only notify Active plugins
			{
				siResult = eDSNoErr;
				siResult = pPlugin->ProcessRequest( (void*)&aHeader );
				if (siResult != eDSNoErr && siResult != eNotHandledByThisNode && siResult != eNotYetImplemented)
				{
					if (pPIInfo != nil)
					{
						ERRORLOG2( kLogApplication, "SystemWillSleep Notification in %s plugin returned error %d", pPIInfo->fName, siResult );
					}
					else
					{
						ERRORLOG1( kLogApplication, "SystemWillSleep Notification of unnamed plugin returned error %d", siResult );
					}
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	return siResult;
}

// ---------------------------------------------------------------------------
//	* HandleSystemWillPowerOn ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::HandleSystemWillPowerOn ( void )
{
	sInt32						siResult		= eDSNoErr;
	uInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	SRVRLOG( kLogApplication, "Will Power On (Wake) Notification occurred.");
	
	aHeader.fType			= kHandleSystemWillPowerOn;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only notify Active plugins
			{
				siResult = eDSNoErr;
				siResult = pPlugin->ProcessRequest( (void*)&aHeader );
				if (siResult != eDSNoErr && siResult != eNotHandledByThisNode && siResult != eNotYetImplemented)
				{
					if (pPIInfo != nil)
					{
						ERRORLOG2( kLogApplication, "WillPowerOn Notification in %s plugin returned error %d", pPIInfo->fName, siResult );
					}
					else
					{
						ERRORLOG1( kLogApplication, "WillPowerOn Notification of unnamed plugin returned error %d", siResult );
					}
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	return siResult;
}

// ---------------------------------------------------------------------------
//	* HandleNetworkTransition ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::HandleNetworkTransition ( void )
{
	sInt32						siResult		= eDSNoErr;
	uInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	CServerPlugin			   *searchPlugin	= nil;
	uInt32						searchIterator	= 0;
	
	aHeader.fType			= kHandleNetworkTransition;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	SRVRLOG( kLogApplication, "Network transition occurred." );
	gFirstNetworkUpAtBoot = true;
	//call thru to each plugin
	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only notify Active plugins
			{
				if ( ::strcmp(pPIInfo->fName,"Search") != 0)
				{
					siResult = eDSNoErr;
					siResult = pPlugin->ProcessRequest( (void*)&aHeader );
					if (siResult != eDSNoErr)
					{
						if (pPIInfo != nil)
						{
							ERRORLOG2( kLogApplication, "Network transition in %s plugin returned error %d", pPIInfo->fName, siResult );
						}
						else
						{
							ERRORLOG1( kLogApplication, "Network transition of unnamed plugin returned error %d", siResult );
						}
					}
				}
				else
				{
					searchIterator	= iterator;
					searchPlugin	= pPlugin;
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
	
	//handle the search plugin transition last to ensure at least NetInfo and LDAPv3 have gone first
	if (searchPlugin != nil)
	{
		siResult = eDSNoErr;
		//now do the network transition itself
		aHeader.fType = kHandleNetworkTransition;
		siResult = searchPlugin->ProcessRequest( (void*)&aHeader );
		if (siResult != eDSNoErr)
		{
			ERRORLOG1( kLogApplication, "Network transition in Search returned error %d", siResult );
		}
	}
	
	//NIAutoSwitch checking
	HandleMultipleNetworkTransitionsForNIAutoSwitch();
				
	return siResult;
} // HandleNetworkTransition


// ---------------------------------------------------------------------------
//	* SetUpPeriodicTask ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::SetUpPeriodicTask ( void )
{
	sInt32		siResult	= eDSNoErr;
	void	   *ptInfo		= nil;
	
	CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, PeriodicTaskCopyStringCallback};
	
	CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
													CFAbsoluteTimeGetCurrent() + 120,
													30,
													0,
													0,
													DoPeriodicTask,
													(CFRunLoopTimerContext*)&c);
		
	CFRunLoopAddTimer(gServerRunLoop, timer, kCFRunLoopDefaultMode);
	if (timer) CFRelease(timer);
					
	return siResult;
} // SetUpPeriodicTask

void ServerControl::NodeSearchPolicyChanged( void )
{
	void	   *ptInfo		= nil;
	
	//wait one second to fire off the timer
	fTimeToCheckSearchPolicyChange = CFAbsoluteTimeGetCurrent() + 1;
	
	if (gServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, SearchPolicyChangeCopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														fTimeToCheckSearchPolicyChange + 1,
														0,
														0,
														0,
														DoSearchPolicyChange,
														(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(gServerRunLoop, timer, kCFRunLoopDefaultMode);
		if (timer) CFRelease(timer);
	}
}

void ServerControl::DoNodeSearchPolicyChange( void )
{
	SCDynamicStoreRef	store		= NULL;
	
	//do something if the delay period has passed
	if (CFAbsoluteTimeGetCurrent() >= fTimeToCheckSearchPolicyChange)
	{
	DBGLOG( kLogApplication, "NodeSearchPolicyChanged" );

	store = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
	if (store != NULL)
	{
		if ( !SCDynamicStoreSetValue( store, CFSTR(kDSStdNotifySearchPolicyChanged), CFSTR("") ) )
			{
				ERRORLOG( kLogApplication, "Could not set the DirectoryService:SearchPolicyChangeToken in System Configuration" );
			}
			CFRelease(store);
			store = NULL;
		}
		else
		{
			ERRORLOG( kLogApplication, "ServerControl::NodeSearchPolicyChanged SCDynamicStoreCreate not yet available from System Configuration" );
		}
		
		LaunchKerberosAutoConfigTool();
	}
}// DoNodeSearchPolicyChange

void ServerControl::NotifySearchPolicyFoundNIParent( void )
{
	SCDynamicStoreRef	store		= NULL;
	
	DBGLOG( kLogApplication, "NotifySearchPolicyFoundNIParent" );

	store = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
	if (store != NULL)
	{
		if ( !SCDynamicStoreSetValue( store, CFSTR(kDSStdNotifySearchPolicyFoundNIParent), CFSTR("") ) )
		{
			ERRORLOG( kLogApplication, "Could not set the DirectoryService:NotifySearchPolicyFoundNIParent in System Configuration" );
		}
		CFRelease(store);
		store = NULL;
	}
	else
	{
		ERRORLOG( kLogApplication, "ServerControl::NotifySearchPolicyFoundNIParent SCDynamicStoreCreate not yet available from System Configuration" );
	}
}

void ServerControl::NotifyDirNodeAdded( const char* newNode )
{
	SCDynamicStoreRef	store	= NULL;

	if ( newNode != nil )
	{
		CFStringRef		newNodeRef = CFStringCreateWithCString( NULL, newNode, kCFStringEncodingUTF8 );
		
		if ( newNodeRef == NULL )
		{
			ERRORLOG1( kLogApplication, "Could not notify that dir node: (%s) was added due to an encoding problem", newNode );
		}
		else
		{
			store = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
			if (store != NULL)
			{
				if ( !SCDynamicStoreSetValue( store, CFSTR(kDSStdNotifyDirectoryNodeAdded), newNodeRef ) )
				{
					ERRORLOG( kLogApplication, "Could not set the DirectoryService:NotifyDirNodeAdded in System Configuration" );
				}
				CFRelease(store);
				store = NULL;
			}
			else
			{
				ERRORLOG( kLogApplication, "ServerControl::NotifyDirNodeAdded SCDynamicStoreCreate not yet available from System Configuration" );
			}
			CFRelease( newNodeRef );
			newNodeRef = NULL;
		}
	}
}

void ServerControl::NotifyDirNodeDeleted( char* oldNode )
{
	SCDynamicStoreRef	store	= NULL;

	if ( oldNode != nil )
	{
		CFStringRef		oldNodeRef = CFStringCreateWithCString( NULL, oldNode, kCFStringEncodingUTF8 );
		
		if ( oldNodeRef == NULL )
		{
			ERRORLOG1( kLogApplication, "Could not notify that dir node: (%s) was deleted due to an encoding problem", oldNode );
		}
		else
		{
			store = SCDynamicStoreCreate(NULL, fServiceNameString, NULL, NULL);
			if (store != NULL)
			{
				if ( !SCDynamicStoreSetValue( store, CFSTR(kDSStdNotifyDirectoryNodeDeleted), oldNodeRef ) )
				{
					ERRORLOG( kLogApplication, "Could not set the DirectoryService:NotifyDirNodeDeleted in System Configuration" );
				}
				CFRelease(store);
				store = NULL;
			}
			else
			{
				ERRORLOG( kLogApplication, "ServerControl::NotifyDirNodeDeleted SCDynamicStoreCreate not yet available from System Configuration" );
			}
			CFRelease( oldNodeRef );
			oldNodeRef = NULL;
		}
	}
}

#ifdef BUILD_IN_PERFORMANCE
void ServerControl::DeletePerfStatTable( void )
{
	PluginPerformanceStats**	table = fPerfTable;
	uInt32						pluginCount = fPerfTableNumPlugins;

	fPerfTable = NULL;
	fPerfTableNumPlugins = 0;
	
	if ( table )
	{
		for ( uInt32 i=0; i<pluginCount+1; i++ )
		{
			if ( table[i] )
			{
				free( table[i] );
				table[i] = NULL;
			}
		}
		
		free( table );
	}
}

PluginPerformanceStats** ServerControl::CreatePerfStatTable( void )
{
DBGLOG( kLogPerformanceStats, "ServerControl::CreatePerfStatTable called\n" );

	PluginPerformanceStats**	table = NULL;
	uInt32						pluginCount = gPlugins->GetPlugInCount();
	
	if ( fPerfTable )
		DeletePerfStatTable();
		
	// how many plugins?
	table = (PluginPerformanceStats**)calloc( sizeof(PluginPerformanceStats*), pluginCount+1 );	// create table for #plugins + 1 for server

	for ( uInt32 i=0; i<pluginCount; i++ )
	{
		table[i] = (PluginPerformanceStats*)calloc( sizeof(PluginPerformanceStats), 1 );
		table[i]->pluginSignature = gPlugins->GetPlugInInfo(i)->fKey;
		table[i]->pluginName = gPlugins->GetPlugInInfo(i)->fName;
	}
	
	table[pluginCount] = (PluginPerformanceStats*)calloc( sizeof(PluginPerformanceStats), 1 );
	table[pluginCount]->pluginSignature = 0;
	table[pluginCount]->pluginName = "Server";
	
	fPerfTableNumPlugins = pluginCount;
	fPerfTable = table;
	
	return table;
}

double gLastDump =0;
#define	kNumSecsBetweenDumps	60*2
void ServerControl::HandlePerformanceStats( uInt32 msgType, FourCharCode pluginSig, sInt32 siResult, sInt32 clientPID, double inTime, double outTime )
{
	// Since the number of plugins is so small, just doing an O(n)/2 search is probably fine...
	gPerformanceLoggingLock->Wait();
	PluginPerformanceStats*		curPluginStats = NULL;
	uInt32						pluginCount = gPlugins->GetPlugInCount();

	if ( !fPerfTable || fPerfTableNumPlugins != pluginCount )
	{
		// first api call, or number of plugins changed, (re)create the table
		fPerfTable = CreatePerfStatTable();
	}
	
	if ( !pluginSig )
		curPluginStats = fPerfTable[pluginCount];	// last entry in the table is reserved for the server
		
	if ( fPerfTable[fLastPluginCalled]->pluginSignature == pluginSig )
		curPluginStats = fPerfTable[fLastPluginCalled];
		
	for ( uInt32 i=0; !curPluginStats && i<pluginCount; i++ )
	{
		if ( pluginSig == fPerfTable[i]->pluginSignature )
		{
			curPluginStats = fPerfTable[i];
			fLastPluginCalled = i;
		}
	}
	
	if ( curPluginStats )
	{
		PluginPerformanceAPIStat*	curAPI = &(curPluginStats->apiStats[msgType]);
		double						duration = outTime-inTime;
		curAPI->msgCnt++;
		
		if ( siResult )
		{
			for( int i=kNumErrorsToTrack-1; i>0; i-- )
			{
				curAPI->lastNErrors[i].error = curAPI->lastNErrors[i-1].error;
				curAPI->lastNErrors[i].clientPID = curAPI->lastNErrors[i-1].clientPID;
			}
			
			curAPI->lastNErrors[0].error = siResult;
			curAPI->lastNErrors[0].clientPID = clientPID;
			curAPI->errCnt++;
		}
		
		if ( curAPI->minTime == 0 || curAPI->minTime > duration )
			curAPI->minTime = duration;
			
		if ( curAPI->maxTime == 0 || curAPI->maxTime < duration )
			curAPI->maxTime = duration;
		
		curAPI->totTime += duration;
	}
	
	gPerformanceLoggingLock->Signal();
}

#define USEC_PER_HOUR	(double)60*60*USEC_PER_SEC	/* microseconds per hour */
#define USEC_PER_DAY	(double)24*USEC_PER_HOUR	/* microseconds per day */

void ServerControl::LogStats( void )
{
	PluginPerformanceStats*		curPluginStats = NULL;
	uInt32						pluginCount = fPerfTableNumPlugins;
	char						logBuf[1024];
	char						totTimeStr[256];
	
	gPerformanceLoggingLock->Wait();

	syslog( LOG_CRIT, "**Usage Stats**\n");
	syslog( LOG_CRIT, "\tPlugin\tAPI\tMsgCnt\tErrCnt\tminTime (usec)\tmaxTime (usec)\taverageTime (usec)\ttotTime (usec|secs|hours|days)\tLast PID\tLast Error\tPrev PIDs/Errors\n" );
	
	for ( uInt32 i=0; i<pluginCount+1; i++ )	// server is at the end
	{
		if ( !fPerfTable[i] )
			continue;
			
		curPluginStats = fPerfTable[i];
				
		for ( uInt32 j=0; j<kDSPlugInCallsEnd; j++ )
		{
			if ( curPluginStats->apiStats[j].msgCnt > 0 )
			{
				if ( curPluginStats->apiStats[j].totTime < USEC_PER_SEC )
					sprintf( totTimeStr, "%0.f usecs", curPluginStats->apiStats[j].totTime );
				else if ( curPluginStats->apiStats[j].totTime < USEC_PER_HOUR )
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_SEC;
					sprintf( totTimeStr, "%0.4f secs", time );
				}
				else if ( curPluginStats->apiStats[j].totTime < USEC_PER_DAY )
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_HOUR;
					sprintf( totTimeStr, "%0.4f hours", time );
				}
				else
				{
					double		time = curPluginStats->apiStats[j].totTime / USEC_PER_DAY;
					sprintf( totTimeStr, "%0.4f days", time );
				}
				
				sprintf( logBuf, "\t%s\t%s\t%ld\t%ld\t%.0f\t%0.f\t%0.f\t%s\t%ld/%ld\t%ld/%ld\t%ld/%ld\t%ld/%ld\t%ld/%ld\n",
								curPluginStats->pluginName,
								CRequestHandler::GetCallName(j),
								curPluginStats->apiStats[j].msgCnt,
								curPluginStats->apiStats[j].errCnt,
								curPluginStats->apiStats[j].minTime,
								curPluginStats->apiStats[j].maxTime,
								(curPluginStats->apiStats[j].totTime/curPluginStats->apiStats[j].msgCnt),
								totTimeStr,
								curPluginStats->apiStats[j].lastNErrors[0].clientPID,
								curPluginStats->apiStats[j].lastNErrors[0].error,
								curPluginStats->apiStats[j].lastNErrors[1].clientPID,
								curPluginStats->apiStats[j].lastNErrors[1].error,
								curPluginStats->apiStats[j].lastNErrors[2].clientPID,
								curPluginStats->apiStats[j].lastNErrors[2].error,
								curPluginStats->apiStats[j].lastNErrors[3].clientPID,
								curPluginStats->apiStats[j].lastNErrors[3].error,
								curPluginStats->apiStats[j].lastNErrors[4].clientPID,
								curPluginStats->apiStats[j].lastNErrors[4].error );
				
				syslog( LOG_CRIT, logBuf );
			}
		}
	}
	
	gPerformanceLoggingLock->Signal();
}
#endif

// ---------------------------------------------------------------------------
//	* DoPeriodicTask ()
//
// ---------------------------------------------------------------------------

void DoPeriodicTask(CFRunLoopTimerRef timer, void *info)
{
	sInt32						siResult		= eDSNoErr;
	uInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	//call thru to each plugin
	if ( gPlugins != nil )
	{
		pPlugin = gPlugins->Next( &iterator );
		while (pPlugin != nil)
		{
			pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
			if (pPIInfo->fState & kActive) //only pulse the Active plugins
			{
				siResult = pPlugin->PeriodicTask();
				if (siResult != eDSNoErr)
				{
					if (pPIInfo != nil)
					{
							DBGLOG2( kLogApplication, "Periodic Task in %s plugin returned error %d", pPIInfo->fName, siResult );
					}
					else
					{
							DBGLOG1( kLogApplication, "Periodic Task of unnamed plugin returned error %d", siResult );
					}
				}
			}
			pPlugin = gPlugins->Next( &iterator );
		}
	}
					
	return;
} // DoPeriodicTask

// ---------------------------------------------------------------------------
//	* FlushLookupDaemonCache ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::FlushLookupDaemonCache ( void )
{
	sInt32		siResult	= eDSNoErr;
	int			i			= 0;
	int			proc		= 0;
	char		str[32];
	mach_port_t	port		= MACH_PORT_NULL;

	//do something if the delay period has passed
	if ( (CFAbsoluteTimeGetCurrent() >= fTimeToCheckLookupDaemonCacheFlush) ||
		(fLookupDaemonFlushCacheRequestCount > kDSActOnThisNumberOfFlushRequests) )
	{
		fLookupDaemonFlushCacheRequestCount = 0;
		DBGLOG( kLogApplication, "Sending lookupd flushcache" );
		_lu_running();
		port = _lookupd_port(0);

		if( port != MACH_PORT_NULL )
		{
			_lookup_link(port, "_invalidatecache", &proc);
			_lookup_one(port, proc, NULL, 0, (char **)&str, &i);
		}
	}
	
	return(siResult);
}// FlushLookupDaemonCache


// ---------------------------------------------------------------------------
//	* FlushMemberDaemonCache ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::FlushMemberDaemonCache ( void )
{
	sInt32 siResult = eDSNoErr;
	
	//do something if the delay period has passed
	if ( (CFAbsoluteTimeGetCurrent() >= fTimeToCheckMemberDaemonCacheFlush) ||
		(fMemberDaemonFlushCacheRequestCount > kDSActOnThisNumberOfFlushRequests) )
	{
		fMemberDaemonFlushCacheRequestCount = 0;
		//routine created for potential other additions
		DBGLOG( kLogApplication, "Sending memberd flushcache" );
		mbr_reset_cache();
	}
	
	return(siResult);
}// FlushMemberDaemonCache

// ---------------------------------------------------------------------------
//	* NIAutoSwitchCheck ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::NIAutoSwitchCheck ( void )
{
	sInt32						siResult		= eDSNoErr;
	uInt32						iterator		= 0;
	CServerPlugin			   *pPlugin			= nil;
	sHeader						aHeader;
	CPlugInList::sTableData	   *pPIInfo			= nil;
	
	aHeader.fType			= kCheckNIAutoSwitch;
	aHeader.fResult			= eDSNoErr;
	aHeader.fContextData	= nil;

	//should be assured that the search plugin is online already at boot
	//but regardless since the plugin will block the request for up
	//to two minutes allowing the plugin to initialize
	//TODO KW looks  like the plugin ptr is not there yet always when this is called at boot

	//do something if the delay period has passed
	if (CFAbsoluteTimeGetCurrent() >= fTimeToCheckNIAutoSwitch)
	{
		// we know we need the search node, so let's wait until it is available.
		gNodeList->WaitForAuthenticationSearchNode();
			
		//call thru to only the search policy plugin
		if ( gPlugins != nil )
		{
			pPlugin = gPlugins->Next( &iterator );
			while (pPlugin != nil)
			{
				pPIInfo = gPlugins->GetPlugInInfo( iterator-1 );
				if ( ( strcmp(pPIInfo->fName,"Search") == 0) && (pPIInfo->fState & kActive) )
				{
					siResult = pPlugin->ProcessRequest( (void*)&aHeader );
					if (siResult == eDSContinue)
					{
						//here we need to turn off netinfo bindings
						sInt32 unbindResult = UnbindToNetInfo();
						if (unbindResult == eDSNoErr)
						{
							DBGLOG( kLogApplication, "NIAutoSwitchCheck(): NIAutoSwitch record found in NetInfo parent has directed addition of LDAP directory to the search policy" );
							//if success then NIBindings turn off will generate a network transition itself
						}
						else
						{
							DBGLOG( kLogApplication, "NIAutoSwitchCheck(): NIAutoSwitch record found in NetInfo parent has directed addition of LDAP directory to the search policy but NetInfo Unbind failed" );
						}
					}
					break;
				}
				pPlugin = gPlugins->Next( &iterator );
			}
		}
	}
	
	return(siResult);
} // NIAutoSwitchCheck


// ---------------------------------------------------------------------------
//	* UnbindToNetInfo ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::UnbindToNetInfo ( void )
{
	sInt32					siResult			= eUnknownServerError;
	SCPreferencesRef		scpRef				= NULL;
	bool					scpStatus			= false;
	CFTypeRef				cfTypeRef			= NULL;
	char				   *pCurrSet			= nil;
	char					charArray[ 1024 ];
	const char			   *NetInfoPath			= "%s/Network/Global/NetInfo";
	CFStringRef				cfNetInfoKey		= NULL;
	const char			   *InactiveTag			= 
"<dict>\
	<key>__INACTIVE__</key>\
	<integer>1</integer>\
</dict>";
	unsigned long			uiDataLen			= 0;
	CFDataRef				dataRef				= NULL;
	CFPropertyListRef		plistRef			= NULL;
	CFDictionaryRef			dictRef				= NULL;
	CFMutableDictionaryRef	dictMutableRef		= NULL;

	scpRef = SCPreferencesCreate( NULL, CFSTR("NIAutoSwitch"), 0 );
	if ( scpRef == NULL )
	{
		return(siResult);
	}

	//	Get the current set
	cfTypeRef = SCPreferencesGetValue( scpRef, CFSTR( "CurrentSet" ) );
	if ( cfTypeRef != NULL )
	{
		pCurrSet = (char *)CFStringGetCStringPtr( (CFStringRef)cfTypeRef, kCFStringEncodingMacRoman );
		if ( pCurrSet != nil )
		{
			sprintf( charArray, NetInfoPath, pCurrSet );
		}
	}
	else
	{
		// Modify the system config file
		sprintf( charArray, NetInfoPath, "/Sets/0" );
	}
			
	cfNetInfoKey = CFStringCreateWithCString( kCFAllocatorDefault, charArray, kCFStringEncodingMacRoman );
	
	if (cfNetInfoKey != NULL)
	{
		dictRef = SCPreferencesPathGetValue( scpRef, cfNetInfoKey);
		if (dictRef != NULL)
		{
			dictMutableRef = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dictRef);
			if ( dictMutableRef	!= NULL)
			{
				int intValue = 1;
				CFNumberRef cfNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &intValue);
				// add the INACTIVE key/value with the old binding methods still present
				CFDictionarySetValue( dictMutableRef, CFSTR( "__INACTIVE__"), cfNumber );
			}
			//CFRelease(dictRef); //retrieved with Get so don't release
		}
		else
		{
			uiDataLen = strlen( InactiveTag );
			dataRef = CFDataCreate( kCFAllocatorDefault, (const UInt8 *)InactiveTag, uiDataLen );
			if ( dataRef != nil )
			{
				plistRef = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, dataRef, kCFPropertyListMutableContainers, nil );
				if ( plistRef != nil )
				{
					dictMutableRef = (CFMutableDictionaryRef)plistRef;
				}
				CFRelease( dataRef );
			}
		}
		if (dictMutableRef != NULL) //either of two ways to get the dict better have succeeded
		{
			//update the local copy with the dict entry
			scpStatus = SCPreferencesPathSetValue( scpRef, cfNetInfoKey, dictMutableRef );
			CFRelease( dictMutableRef );
		}
		CFRelease( cfNetInfoKey );
	}

	if (scpStatus)
	{
		scpStatus = SCPreferencesCommitChanges( scpRef );
		if (scpStatus)
		{
			scpStatus = SCPreferencesApplyChanges( scpRef );
		}
	}
	CFRelease( scpRef );

	if (scpStatus) siResult = eDSNoErr;
	
	return(siResult);
} // UnbindToNetInfo


//------------------------------------------------------------------------------------
//	* HandleLookupDaemonFlushCache
//------------------------------------------------------------------------------------

void ServerControl::HandleLookupDaemonFlushCache ( void )
{
	void	   *ptInfo		= nil;
	
	//wait one second to fire off the timer
	//consolidate multiple requests that come in faster than one second
	fTimeToCheckLookupDaemonCacheFlush = CFAbsoluteTimeGetCurrent() + 1;
	fLookupDaemonFlushCacheRequestCount++;
	
	if (gServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, LookupDaemonFlushCacheCopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														fTimeToCheckLookupDaemonCacheFlush + 1,
														0,
														0,
														0,
														DoLookupDaemonFlushCache,
														(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(gServerRunLoop, timer, kCFRunLoopDefaultMode);
		if (timer) CFRelease(timer);
	}
} // HandleLookupDaemonFlushCache

//------------------------------------------------------------------------------------
//	* HandleMemberDaemonFlushCache
//------------------------------------------------------------------------------------

void ServerControl::HandleMemberDaemonFlushCache ( void )
{
	void	   *ptInfo		= nil;
	
	//wait one second to fire off the timer
	//consolidate multiple requests that come in faster than one second
	fTimeToCheckMemberDaemonCacheFlush = CFAbsoluteTimeGetCurrent() + 1;
	fMemberDaemonFlushCacheRequestCount++;
	
	if (gServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, MemberDaemonFlushCacheCopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														fTimeToCheckMemberDaemonCacheFlush + 1,
														0,
														0,
														0,
														DoMemberDaemonFlushCache,
														(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(gServerRunLoop, timer, kCFRunLoopDefaultMode);
		if (timer) CFRelease(timer);
	}
} // HandleMemberDaemonFlushCache

//------------------------------------------------------------------------------------
//	* HandleMultipleNetworkTransitionsForNIAutoSwitch
//------------------------------------------------------------------------------------

void ServerControl::HandleMultipleNetworkTransitionsForNIAutoSwitch ( void )
{
	void	   *ptInfo		= nil;
	
	//let us be smart about doing the check
	//we would like to wait a short period for the Network transitions to subside
	//since we don't want to re-init multiple times during this wait period
	//however we do go ahead and fire off timers each time
	//each call in here we update the delay time by 11 seconds
	//Need to ensure that this does not conflict with NetInfo connection
	//re-establishment after a network transition
	fTimeToCheckNIAutoSwitch = CFAbsoluteTimeGetCurrent() + 10;

	if (gServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, NetworkChangeNIAutoSwitchCopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														fTimeToCheckNIAutoSwitch + 1,
														0,
														0,
														0,
														DoNIAutoSwitchNetworkChange,
														(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(gServerRunLoop, timer, kCFRunLoopDefaultMode);
		if (timer) CFRelease(timer);
	}
} // HandleMultipleNetworkTransitionsForNIAutoSwitch

//------------------------------------------------------------------------------
//	* LaunchKerberosAutoConfigTool
//
//------------------------------------------------------------------------------

void ServerControl:: LaunchKerberosAutoConfigTool ( void )
{
	sInt32			result			= eDSNoErr;
	mach_port_t		mach_init_port	= MACH_PORT_NULL;

	//lookup mach init port to launch Kerberos AutoConfig Tool on demand
	result = bootstrap_look_up( bootstrap_port, "com.apple.KerberosAutoConfig", &mach_init_port );
	if ( result != eDSNoErr )
	{
		syslog( LOG_ALERT, "Error with bootstrap_look_up for com.apple.KerberosAutoConfig on mach_init port: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
	}
	else
	{
		sIPCMsg aMsg;
		
		aMsg.fHeader.msgh_bits			= MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND );
		aMsg.fHeader.msgh_size			= sizeof(sIPCMsg) - sizeof( mach_msg_audit_trailer_t );
		aMsg.fHeader.msgh_id			= 0;
		aMsg.fHeader.msgh_remote_port	= mach_init_port;
		aMsg.fHeader.msgh_local_port	= MACH_PORT_NULL;
	
		aMsg.fMsgType	= 0;
		aMsg.fCount		= 1;
		aMsg.fPort		= MACH_PORT_NULL;
		aMsg.fPID		= 0;
		aMsg.fMsgID		= 0;
		aMsg.fOf		= 1;
		//tickle the mach init port - should this really be required to start the daemon
		mach_msg((mach_msg_header_t *)&aMsg, MACH_SEND_MSG | MACH_SEND_TIMEOUT, aMsg.fHeader.msgh_size, 0, MACH_PORT_NULL, 1, MACH_PORT_NULL);
		//don't retain the mach init port since only using it to launch the Kerberos AutoConfig tool
		mach_port_destroy(mach_task_self(), mach_init_port);
		mach_init_port = MACH_PORT_NULL;
	}

} // CheckForServer


// ---------------------------------------------------------------------------
//	* ResetDebugging ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::ResetDebugging ( void )
{
	sInt32					siResult	= eDSNoErr;
	uInt32					uiDataSize	= 0;
	char				   *pData		= nil;
	CFile				   *pFile		= nil;
	struct stat				statbuf;
	CFDataRef				dataRef		= nil;
	CFBooleanRef			cfBool		= false;
	bool					bDebugging  = false;
	bool					bFileUsed   = false;
	

	if (gToggleDebugging)
	{
		gToggleDebugging = false;
		//here we turn everything off
		if (gDebugLogging)
		{
			CLog::StopDebugLog();
			gDebugLogging = false;
			syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
		}
		gDSFWCSBPDebugLogging = false;
	}
	else
	{
		//next time this is called we turn everything off
		gToggleDebugging = true;
		
		// Does the debug config file exist
		siResult = ::stat( kDSDebugConfigFilePath, &statbuf );
		if ( siResult == eDSNoErr )
		{
			// Attempt to get config info from file
			pFile = new CFile( kDSDebugConfigFilePath );
			if (pFile != nil) 
			{
				if ( (pFile->is_open()) && (pFile->FileSize() > 0) )
				{
					// Allocate space for the file data
					pData = (char *)::calloc( 1, pFile->FileSize() + 1 );
					if ( pData != nil )
					{
						// Read from the config file
						uiDataSize = pFile->ReadBlock( pData, pFile->FileSize() );
						dataRef = ::CFDataCreate( nil, (const uInt8 *)pData, uiDataSize );
						if ( dataRef != nil )
						{
							CFPropertyListRef   aPlistRef   = 0;
							CFDictionaryRef		aDictRef	= 0;
							// Is it valid XML data
							aPlistRef = ::CFPropertyListCreateFromXMLData( kCFAllocatorDefault, dataRef, kCFPropertyListImmutable, nil );
							if ( aPlistRef != nil )
							{
								// Is it a plist type
								if ( ::CFDictionaryGetTypeID() == ::CFGetTypeID( aPlistRef ) )
								{
								
									bFileUsed = true;
									
									aDictRef = (CFDictionaryRef)aPlistRef;
									
									//now set up the debugging according to the plist settings
									
									//debug logging boolean
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSDebugLoggingKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSDebugLoggingKey ) );
										if (cfBool != nil)
										{
											bDebugging = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
											if (gDebugLogging && !bDebugging)
											{
												CLog::StopDebugLog();
												gDebugLogging = false;
												syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
											}
											else if (!gDebugLogging && bDebugging)
											{
												gDebugLogging = true;
												CLog::StartDebugLog();
												syslog(LOG_ALERT,"Debug Logging turned ON after receiving USR1 signal.");
											}
										}
									}
									else if (gDebugLogging)
									{
										CLog::StopDebugLog();
										gDebugLogging = false;
										syslog(LOG_ALERT,"Debug Logging turned OFF after receiving USR1 signal.");
									}

									//FW CSBP debug logging boolean
									if ( CFDictionaryContainsKey( aDictRef, CFSTR( kXMLDSCSBPDebugLoggingKey ) ) )
									{
										cfBool= (CFBooleanRef)CFDictionaryGetValue( aDictRef, CFSTR( kXMLDSCSBPDebugLoggingKey ) );
										if (cfBool != nil)
										{
											gDSFWCSBPDebugLogging = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
										}
									}
									else
									{
										gDSFWCSBPDebugLogging = false;
									}

									aDictRef = 0;
								}
								//free the propertylist
								CFRelease(aPlistRef);
							}
							CFRelease( dataRef );
							dataRef = nil;
						}
						free( pData );
						pData = nil;
					}
				}
				delete( pFile );
				pFile = nil;
			}
		}
		
		if (!bFileUsed)
		{
			//write a default file and setup debugging
			sInt32 result = eDSNoErr;
			gDebugLogging = true;
			CLog::StartDebugLog();
			syslog(LOG_ALERT,"Debug Logging turned ON after receiving USR1 signal.");
			uiDataSize = ::strlen( kDefaultDebugConfig );
			dataRef = ::CFDataCreate( nil, (const uInt8 *)kDefaultDebugConfig, uiDataSize );
			if ( dataRef != nil )
			{
				//see if the file exists
				//if not then make sure the directories exist or create them
				//then create a new file if necessary
				result = ::stat( kDSDebugConfigFilePath, &statbuf );
				//if file does not exist
				if (result != eDSNoErr)
				{
					//move down the path from the system defined local directory and check if it exists
					//if not create it
					result = ::stat( "/Library/Preferences", &statbuf );
					//if first sub directory does not exist
					if (result != eDSNoErr)
					{
						::mkdir( "/Library/Preferences", 0775 );
						::chmod( "/Library/Preferences", 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
					}
					result = ::stat( "/Library/Preferences/DirectoryService", &statbuf );
					//if second sub directory does not exist
					if (result != eDSNoErr)
					{
						::mkdir( "/Library/Preferences/DirectoryService", 0775 );
						::chmod( "/Library/Preferences/DirectoryService", 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
					}
				}

				UInt8 *pData = (UInt8*)::calloc( CFDataGetLength(dataRef), 1 );
				CFDataGetBytes(	dataRef, CFRangeMake(0,CFDataGetLength(dataRef)), pData );
				if ( (pData != nil) && (pData[0] != 0) )
				{
					try
					{
						CFile *pFile = new CFile( kDSDebugConfigFilePath, true );
						if ( pFile != nil )
						{
							if ( pFile->is_open() )
							{
								pFile->seteof( 0 );
			
								pFile->write( pData, CFDataGetLength(dataRef) );
			
								::chmod( kDSDebugConfigFilePath, 0600 );
							}
							
							delete( pFile );
							pFile = nil;
						}
					}
					catch ( ... )
					{
					}
					free(pData);
				}

				CFRelease( dataRef );
				dataRef = nil;
			}
		}
	}
	
	return(siResult);
	
} // ResetDebugging


