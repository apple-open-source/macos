/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#include "CListener.h"
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

#include <ldap.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <sys/stat.h>							//used for mkdir and stat
#include <servers/bootstrap.h>
#include <IOKit/pwr_mgt/IOPMLib.h>				//required for power management handling
#include <syslog.h>								// for syslog()
#include <time.h>								// for time

//power management
extern void dsPMNotificationHandler ( void *refContext, io_service_t service, natural_t messageType, void *notificationID );
extern io_object_t		gPMDeregisterNotifier;
extern io_connect_t		gPMKernelPort;

//network change
extern void NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *callback_argument);
extern CFRunLoopRef		gServerRunLoop;

extern time_t			gSunsetTime;
extern dsBool			gLogAPICalls;
extern dsBool			gDebugLogging;

extern	bool			gServerOS;


// ---------------------------------------------------------------------------
//	* Globals
//
// ---------------------------------------------------------------------------

ServerControl		   *gSrvrCntl			= nil;
CRefTable			   *gRefTable			= nil;
CPlugInList			   *gPlugins			= nil;
CMsgQueue			   *gTCPMsgQueue		= nil;
CMsgQueue			   *gMsgQueue			= nil;
CMsgQueue			   *gInternalMsgQueue	= nil;
CMsgQueue			   *gCheckpwMsgQueue	= nil;
CPluginConfig		   *gPluginConfig		= nil;
CNodeList			   *gNodeList			= nil;
CPluginHandler		   *gPluginHandler		= nil;

DSMutexSemaphore	   *gTCPHandlerLock			= new DSMutexSemaphore();	//mutex on create and destroy of CHandler threads
DSMutexSemaphore	   *gHandlerLock			= new DSMutexSemaphore();	//mutex on create and destroy of CHandler threads
DSMutexSemaphore	   *gInternalHandlerLock	= new DSMutexSemaphore();	//mutex on create and destroy of CHandler Internal threads
DSMutexSemaphore	   *gCheckpwHandlerLock		= new DSMutexSemaphore();	//mutex on create and destroy of CHandler Checkpw threads
DSMutexSemaphore	   *gPerformanceLoggingLock = new DSMutexSemaphore();	//mutex on manipulating performance logging matrix
DSMutexSemaphore	   *gLazyPluginLoadingLock	= new DSMutexSemaphore();	//mutex on loading plugins lazily

uInt32					gDaemonPID;
uInt32					gDaemonIPAddress;

//PFIXdsBool					gLocalNodeNotAvailable	= true;

static void DoNIAutoSwitchNetworkChange(CFRunLoopTimerRef timer, void *info);
void DoNIAutoSwitchNetworkChange(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((ServerControl *)info)->NIAutoSwitchCheck();
	}
}// DoNIAutoSwitchNetworkChange


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

// ---------------------------------------------------------------------------
//	* ServerControl()
//
// ---------------------------------------------------------------------------

ServerControl::ServerControl ( void )
{
	uInt32	i	= 0;

    gDaemonPID			= getpid();
	//we use a non valid IP Address of zero for ourselves
	gDaemonIPAddress	= 0;

	fListener					= nil;
	fTCPListener				= nil;
	fTCPHandlerThreadsCnt		= 0;
	fHandlerThreadsCnt			= 0;
	fInternalHandlerThreadsCnt	= 0;
	fCheckpwHandlerThreadsCnt	= 0;
	fSCDStore					= 0;	
	fPerformanceStatGatheringActive	= false; //default
	fTimeToCheckNIAutoSwitch	= 0;
	fHoldStore					= NULL;
	
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

	for ( i = 0; i < kMaxHandlerThreads; i++)
	{
		fTCPHandlers[ i ] = nil;
	}

	for ( i = 0; i < kMaxHandlerThreads; i++)
	{
		fHandlers[ i ] = nil;
	}

	for ( i = 0; i < kMaxInternalHandlerThreads; i++)
	{
		fInternalHandlers[ i ] = nil;
	}

	for ( i = 0; i < kMaxCheckpwHandlerThreads; i++)
	{
		fCheckpwHandlers[ i ] = nil;
	}

	fTCPHandlerSemaphore		= new DSSemaphore( fTCPHandlerThreadsCnt );
	fHandlerSemaphore			= new DSSemaphore( fHandlerThreadsCnt );
	fInternalHandlerSemaphore	= new DSSemaphore( fInternalHandlerThreadsCnt );
	fCheckpwHandlerSemaphore	= new DSSemaphore( fCheckpwHandlerThreadsCnt );
	
	fServiceNameString = CFStringCreateWithCString( NULL, kDSServiceName, kCFStringEncodingUTF8 );
    
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

	if ( fHandlerSemaphore != nil )
	{
		delete( fHandlerSemaphore );
		fHandlerSemaphore = nil;
	}

	if ( fInternalHandlerSemaphore != nil )
	{
		delete( fInternalHandlerSemaphore );
		fInternalHandlerSemaphore = nil;
	}

	if ( fCheckpwHandlerSemaphore != nil )
	{
		delete( fCheckpwHandlerSemaphore );
		fCheckpwHandlerSemaphore = nil;
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
	LDAP*				aHost		= nil;
	int					rc			= LDAP_SUCCESS;
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

		if ( gMsgQueue == nil )
		{
			gMsgQueue = new CMsgQueue();
			if ( gMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if ( gInternalMsgQueue == nil )
		{
			gInternalMsgQueue = new CMsgQueue();
			if ( gInternalMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if ( gCheckpwMsgQueue == nil )
		{
			gCheckpwMsgQueue = new CMsgQueue();
			if ( gCheckpwMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if (::stat( "/Library/Preferences/DirectoryService/.DSLogAPIAtStart", &statResult ) == eDSNoErr)
		{
			gLogAPICalls	= true;
			gSunsetTime		= time(nil) + 300;
			syslog(LOG_INFO,"Logging of API Calls turned ON at Startup of DS Daemon.");
			gDebugLogging	= true;
			CLog::StartDebugLog();
			syslog(LOG_INFO,"Debug Logging turned ON at Startup of DS Daemon.");
		}

		// initialize LDAP before any plug-ins
		rc = ldap_initialize(&aHost, NULL);
		if (rc == LDAP_SUCCESS) {
			ldap_unbind(aHost);
			aHost = NULL;
		}
		
		// Start the listener thread
		result = StartListener();
		if ( result != eDSNoErr ) throw( result );
		
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

//		result = RegisterForSystemPower();
//		if ( result != eDSNoErr ) throw( result );

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

		//need to stop the listener before anything else
		fListener->StopThread();
		
		if (::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult ) == eDSNoErr)
		{
			//need to stop the TCP listener before anything else
			//assume that the listener itself will close all of its connections
			if (fTCPListener != nil)
			{
				fTCPListener->StopThread();
			
				gTCPHandlerLock->Wait();
				// Stop the handler threads
				for ( i = 0; i < kMaxHandlerThreads; i++ )
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
		
		gHandlerLock->Wait();
		// Stop the handler threads
		for ( i = 0; i < kMaxHandlerThreads; i++ )
		{
			if ( fHandlers[ i ] != nil )
			{
				uiStopCnt += 1;

				fHandlers[ i ]->StopThread();

				fHandlers[ i ] = nil;
			}
		}
		gHandlerLock->Signal();

		while (uiStopCnt > 0)
		{
			WakeAHandler(DSCThread::kTSHandlerThread);
			uiStopCnt--;
		}

		uiStopCnt	= 0;

		gInternalHandlerLock->Wait();
		// Stop the internal handler threads
		for ( i = 0; i < kMaxInternalHandlerThreads; i++ )
		{
			if ( fInternalHandlers[ i ] != nil )
			{
				uiStopCnt += 1;

				fInternalHandlers[ i ]->StopThread();

				fInternalHandlers[ i ] = nil;
			}
		}
		gInternalHandlerLock->Signal();

		while (uiStopCnt > 0)
		{
			WakeAHandler(DSCThread::kTSInternalHandlerThread);
			uiStopCnt--;
		}
		
		uiStopCnt	= 0;

		gCheckpwHandlerLock->Wait();
		// Stop the Checkpw handler threads
		for ( i = 0; i < kMaxCheckpwHandlerThreads; i++ )
		{
			if ( fCheckpwHandlers[ i ] != nil )
			{
				uiStopCnt += 1;

				fCheckpwHandlers[ i ]->StopThread();

				fCheckpwHandlers[ i ] = nil;
			}
		}
		gCheckpwHandlerLock->Signal();

		while (uiStopCnt > 0)
		{
			WakeAHandler(DSCThread::kTSCheckpwHandlerThread);
			uiStopCnt--;
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
//	* StartListener ()
//
// ---------------------------------------------------------------------------

sInt32 ServerControl::StartListener ( void )
{
	sInt32	result	= eDSNoErr;

	try
	{
		fListener = new CListener();
		if ( fListener == nil ) throw((sInt32)eMemoryAllocError);

		//this call could throw
		fListener->StartThread();
		//fListenerStarted = true;
		
	}

	catch( sInt32 err )
	{
		result = err;
		DBGLOG2( kLogApplication, "File: %s. Line: %d", __FILE__, __LINE__ );
		DBGLOG1( kLogApplication, "  Caught exception = %d.", err );
	}
/*
	if ( (result == eDSNoErr) && (gServerRunLoop != NULL) )
	{
		//let's add in capability to clean up dead client mach ports if listener started
		CFMachPortRef      d_port = CFMachPortCreate(NULL, ClientDeathCallback, NULL, NULL);
		CFRunLoopSourceRef d_rls  = CFMachPortCreateRunLoopSource(NULL, d_port, 0);
		gClient_Death_Notify_Port = CFMachPortGetPort(d_port);
		CFRunLoopAddSource(gServerRunLoop, d_rls, kCFRunLoopDefaultMode);
		CFRelease(d_rls);
		//do we need to release d_port?
	}
*/	
	return( result );

} // StartListener


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
			if ( (fTCPHandlerThreadsCnt >= 0) && (fTCPHandlerThreadsCnt < kMaxHandlerThreads) )
			{
				for (iThread =0; iThread < kMaxHandlerThreads; iThread++)
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

		if (inThreadSignature == DSCThread::kTSHandlerThread)
		{
			if ( (fHandlerThreadsCnt >= 0) && (fHandlerThreadsCnt < kMaxHandlerThreads) )
			{
				for (iThread =0; iThread < kMaxHandlerThreads; iThread++)
				{
					if (fHandlers[ iThread ] == nil)
					{
						// Start a handler thread
						fHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSHandlerThread, iThread);
						if ( fHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						fHandlerThreadsCnt++;

						//this call could throw
						fHandlers[ iThread ]->StartThread();
						break;
					}
					else if ( fHandlers[iThread]->GetOurThreadRunState() == DSCThread::kThreadStop)
					{
						// Start a handler thread
						fHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSHandlerThread, iThread);
						if ( fHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						//fHandlerThreadsCnt++; //no need since replacing

						//this call could throw
						fHandlers[ iThread ]->StartThread();
						break;
					}
				}
			}
		}

		if (inThreadSignature == DSCThread::kTSInternalHandlerThread)
		{
			if ( (fInternalHandlerThreadsCnt >= 0) && (fInternalHandlerThreadsCnt < kMaxInternalHandlerThreads) )
			{
				for (iThread =0; iThread < kMaxInternalHandlerThreads; iThread++)
				{
					if (fInternalHandlers[ iThread ] == nil)
					{
						// Start a handler thread
						fInternalHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSInternalHandlerThread, iThread);
						if ( fInternalHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						fInternalHandlerThreadsCnt++;

						//this call could throw
						fInternalHandlers[ iThread ]->StartThread();
						break;
					}
					else if ( fInternalHandlers[iThread]->GetOurThreadRunState() == DSCThread::kThreadStop)
					{
						// Start a handler thread
						fInternalHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSInternalHandlerThread, iThread);
						if ( fInternalHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						//fInternalHandlerThreadsCnt++; //no need since replacing

						//this call could throw
						fInternalHandlers[ iThread ]->StartThread();
						break;
					}
				}
			}
		}

		if (inThreadSignature == DSCThread::kTSCheckpwHandlerThread)
		{
			if ( (fCheckpwHandlerThreadsCnt >= 0) && (fCheckpwHandlerThreadsCnt < kMaxCheckpwHandlerThreads) )
			{
				for (iThread =0; iThread < kMaxCheckpwHandlerThreads; iThread++)
				{
					if (fCheckpwHandlers[ iThread ] == nil)
					{
						// Start a handler thread
						fCheckpwHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSCheckpwHandlerThread, iThread);
						if ( fCheckpwHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						fCheckpwHandlerThreadsCnt++;

						//this call could throw
						fCheckpwHandlers[ iThread ]->StartThread();
						break;
					}
					else if ( fCheckpwHandlers[iThread]->GetOurThreadRunState() == DSCThread::kThreadStop)
					{
						// Start a handler thread
						fCheckpwHandlers[ iThread ] = new CHandlerThread(DSCThread::kTSCheckpwHandlerThread, iThread);
						if ( fCheckpwHandlers[ iThread ] == nil ) throw((sInt32)eMemoryAllocError);

						//fCheckpwHandlerThreadsCnt++; //no need since replacing

						//this call could throw
						fCheckpwHandlers[ iThread ]->StartThread();
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
	if (inThreadSignature == DSCThread::kTSHandlerThread)
	{
		fHandlerSemaphore->Signal();
	}
	if (inThreadSignature == DSCThread::kTSInternalHandlerThread)
	{
		fInternalHandlerSemaphore->Signal();
	}
	if (inThreadSignature == DSCThread::kTSCheckpwHandlerThread)
	{
		fCheckpwHandlerSemaphore->Signal();
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
			if ( (iThread >= 0) && (iThread < kMaxHandlerThreads) )
			{
				if (fTCPHandlers[ iThread ] == inThread)
				{
					// Remove a handler thread from the list
					fTCPHandlers[ iThread ] = nil;

					fTCPHandlerThreadsCnt--;
				}
			}
		}
		if (inThreadSignature == DSCThread::kTSHandlerThread)
		{
			if ( (iThread >= 0) && (iThread < kMaxHandlerThreads) )
			{
				if (fHandlers[ iThread ] == inThread)
				{
					// Remove a handler thread from the list
					fHandlers[ iThread ] = nil;

					fHandlerThreadsCnt--;
				}
			}
		}
		if (inThreadSignature == DSCThread::kTSInternalHandlerThread)
		{
			if ( (iThread >= 0) && (iThread < kMaxInternalHandlerThreads) )
			{
				if (fInternalHandlers[ iThread ] == inThread)
				{
					// Remove a handler thread from the list
					fInternalHandlers[ iThread ] = nil;

					fInternalHandlerThreadsCnt--;
				}
			}
		}
		if (inThreadSignature == DSCThread::kTSCheckpwHandlerThread)
		{
			if ( (iThread >= 0) && (iThread < kMaxCheckpwHandlerThreads) )
			{
				if (fCheckpwHandlers[ iThread ] == inThread)
				{
					// Remove a handler thread from the list
					fCheckpwHandlers[ iThread ] = nil;

					fCheckpwHandlerThreadsCnt--;
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
	if (inThreadSignature == DSCThread::kTSHandlerThread)
	{
		fHandlerSemaphore->Wait( waitTime );
	}
	if (inThreadSignature == DSCThread::kTSInternalHandlerThread)
	{
		fInternalHandlerSemaphore->Wait( waitTime );
	}
	if (inThreadSignature == DSCThread::kTSCheckpwHandlerThread)
	{
		fCheckpwHandlerSemaphore->Wait( waitTime );
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
	if (inThreadSignature == DSCThread::kTSHandlerThread)
	{
		return fHandlerThreadsCnt;
	}
	if (inThreadSignature == DSCThread::kTSInternalHandlerThread)
	{
		return fInternalHandlerThreadsCnt;
	}
	if (inThreadSignature == DSCThread::kTSCheckpwHandlerThread)
	{
		return fCheckpwHandlerThreadsCnt;
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
	CFStringRef			dhcpKey				= 0;	//DHCP changes key
	CFStringRef			ipKey				= 0;	//ip changes key
	CFStringRef			niKey				= 0;	//NetInfo changes key
//	CFStringRef			atKey				= 0;	//Appletalk changes key
//	CFStringRef			dnsKey				= 0;	//DNS changes key
//	CFStringRef			nisKey				= 0;	//NIS changes key
//	CFStringRef			computerNameKey 	= 0;	//computer name changes key
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
	//CFArrayAppendValue(notifyPatterns, kSCCompAnyRegex); //formerly kSCDRegexKey
											
	// ip changes
	ipKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
	CFArrayAppendValue(notifyKeys, ipKey);
	CFRelease(ipKey);

	//DHCP changes
	dhcpKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetDHCP);
	CFArrayAppendValue(notifyPatterns, dhcpKey);
	CFRelease(dhcpKey);
		
	// Appletalk changes
	//atKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetAppleTalk);
	//CFArrayAppendValue(notifyKeys, atKey);
	//CFRelease(atKey);

	// NetInfo changes
	niKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetNetInfo);
	CFArrayAppendValue(notifyKeys, niKey);
	CFRelease(niKey);
			
	// DNS changed
	//dnsKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetDNS);
	//CFArrayAppendValue(notifyKeys, dnsKey);
	//CFRelease(dnsKey);
			
	// NIS changed
	//nisKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetNIS);
	//CFArrayAppendValue(notifyKeys, nisKey);
	//CFRelease(nisKey);

	//same mechanism that lookupd daemon currently uses to restart itself
	//although in our case we simply stop and allow any client through our
	//framework to restart us when required
	//CFArrayAppendValue(notifyKeys, CFSTR("File:/var/run/nibindd.pid"));

	// computer name changes
	//computerNameKey = SCDynamicStoreKeyCreateHostName();
	//CFArrayAppendValue(notifyKeys, computerNameKey);
	
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
			syslog(LOG_INFO,"Unable to add source to RunLoop for SystemConfiguration registration for Network Notification");
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
		syslog(LOG_INFO,"Unable to create DirectoryService store for SystemConfiguration registration for Network Notification");
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
			syslog(LOG_INFO,"Unable to create DirectoryService:PID string for SystemConfiguration registration - DSAgent will be disabled in lookupd");
		}
		//DO NOT release the store here since we use SCDynamicStoreAddTemporaryValue above
		//CFRelease(fHoldStore);
		//fHoldStore = NULL;
	}
	else
	{
		syslog(LOG_INFO,"Unable to create DirectoryService store for SystemConfiguration registration of DirectoryService:PID string - DSAgent will be disabled in lookupd");
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
	IONotificationPortRef pmNotificationPortRef;

	DBGLOG( kLogApplication, "RegisterForSystemPower(): " );

	gPMKernelPort = IORegisterForSystemPower(this, &pmNotificationPortRef, dsPMNotificationHandler, &gPMDeregisterNotifier);
	if (gPMKernelPort == nil)
	{
		DBGLOG( kLogApplication, "RegisterForSystemPower(): IORegisterForSystemPower failed" );
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
	SCDynamicStoreRef	store		= NULL;
	
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
					ERRORLOG( kLogApplication, "Could not set the DirectoryService:NotifyDirNodeAdded in System Configuration" );
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

	syslog( LOG_INFO, "**Usage Stats**\n");
	syslog( LOG_INFO, "\tPlugin\tAPI\tMsgCnt\tErrCnt\tminTime (usec)\tmaxTime (usec)\taverageTime (usec)\ttotTime (usec|secs|hours|days)\tLast PID\tLast Error\tPrev PIDs/Errors\n" );
	
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
				
				syslog( LOG_INFO, logBuf );
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
	if (dsTimestamp() >= fTimeToCheckNIAutoSwitch)
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
			dictMutableRef = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, NULL, dictRef);
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
//	* HandleMultipleNetworkTransitionsForNIAutoSwitch
//------------------------------------------------------------------------------------

void ServerControl::HandleMultipleNetworkTransitionsForNIAutoSwitch ( void )
{
	void	   *ptInfo		= nil;
	
	//let us be smart about doing the check
	//we would like to wait a short period for the Network transitions to subside
	//since we don't want to re-init multiple times during this wait period
	//however we do go ahead and fire off timers each time
	//each call in here we update the delay time by 5 seconds
	fTimeToCheckNIAutoSwitch = dsTimestamp() + USEC_PER_SEC*5;

	if (gServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, NetworkChangeNIAutoSwitchCopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														CFAbsoluteTimeGetCurrent() + 5,
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
		aMsg.fHeader.msgh_size			= sizeof(sIPCMsg) - sizeof( mach_msg_security_trailer_t );
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




