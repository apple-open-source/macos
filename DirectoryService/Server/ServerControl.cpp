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
 * @header ServerControl
 */

#if DEBUG_SRVR
# include <stdio.h>			// for stderr, fprintf(), et al
#endif

#include "ServerControl.h"
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

#include <LDAP/ldap.h>
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
extern boolean_t NetworkChangeCallBack(SCDynamicStoreRef aSCDStore, void *callback_argument);
extern CFRunLoopRef		gServerRunLoop;

extern time_t			gSunsetTime;
extern dsBool			gLogAPICalls;

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
CNodeList			   *gNodeList			= nil;

name_t					gServerName				= "DirectoryService";	//KW use constant string here
DSMutexSemaphore	   *gTCPHandlerLock			= new DSMutexSemaphore();	//mutex on create and destroy of CHandler threads
DSMutexSemaphore	   *gHandlerLock			= new DSMutexSemaphore();	//mutex on create and destroy of CHandler threads
DSMutexSemaphore	   *gInternalHandlerLock	= new DSMutexSemaphore();	//mutex on create and destroy of CHandler Internal threads
DSMutexSemaphore	   *gCheckpwHandlerLock		= new DSMutexSemaphore();	//mutex on create and destroy of CHandler Checkpw threads

uInt32					gDaemonPID;
uInt32					gDaemonIPAddress;

static mach_port_t		gClient_Death_Notify_Port = NULL;

void ClientDeathCallback(CFMachPortRef port, void *voidmsg, CFIndex size, void *info);
void ClientDeathCallback(CFMachPortRef port, void *voidmsg, CFIndex size, void *info)
{
	mach_msg_header_t *msg = (mach_msg_header_t *)voidmsg;
	if (msg->msgh_id == MACH_NOTIFY_DEAD_NAME)
	{
		const mach_dead_name_notification_t *const deathMessage = (mach_dead_name_notification_t *)msg;
		DBGLOG1( kLogApplication, "Client that used port %d has died or port has been deallocated.", deathMessage->not_port );

		// Deallocate the send right that came in the dead name notification
		mach_port_destroy( mach_task_self(), deathMessage->not_port );
	}
}// ClientDeathCallback

//when we receive the message first time we do this
void EnableDeathNotificationForClient(mach_port_t port);
void EnableDeathNotificationForClient(mach_port_t port)
{
	mach_port_t prev;
	if (gClient_Death_Notify_Port != NULL)
	{
		kern_return_t r = mach_port_request_notification(	mach_task_self(),
															port,
															MACH_NOTIFY_DEAD_NAME,
															0,
															gClient_Death_Notify_Port,
															MACH_MSG_TYPE_MAKE_SEND_ONCE,
															&prev);
		// If the port already died while we thought to register for its death, then log this
		if (r != KERN_SUCCESS)
		{
			DBGLOG1( kLogApplication, "Can't register for dead port notification since port %d already gone.", port );
		}
	}
} // EnableDeathNotificationForClient

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
			gNodeList = new CNodeList;
			if ( gNodeList == nil ) throw((sInt32)eMemoryAllocError);
		}

		if ( gRefTable == nil )
		{
			gRefTable = new CRefTable( CHandlerThread::RefDeallocProc );
			if ( gRefTable == nil ) throw( (sInt32)eMemoryAllocError );
		}

		if ( gPlugins == nil )
		{
			gPlugins = new CPlugInList();
			if ( gPlugins == nil ) throw( (sInt32)eMemoryAllocError );
		}

		if ( gTCPMsgQueue == nil )
		{
			gTCPMsgQueue = new CMsgQueue;
			if ( gTCPMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if ( gMsgQueue == nil )
		{
			gMsgQueue = new CMsgQueue;
			if ( gMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if ( gInternalMsgQueue == nil )
		{
			gInternalMsgQueue = new CMsgQueue;
			if ( gInternalMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if ( gCheckpwMsgQueue == nil )
		{
			gCheckpwMsgQueue = new CMsgQueue;
			if ( gCheckpwMsgQueue == nil ) throw((sInt32)eMemoryAllocError);
		}

		if (::stat( "/Library/Preferences/DirectoryService/.DSLogAPIAtStart", &statResult ) == eDSNoErr)
		{
			gLogAPICalls	= true;
			gSunsetTime		= time(nil) + 300;
			syslog(LOG_INFO,"Logging of API Calls turned ON at Startup of DS Daemon.");
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
		
		if (::stat( "/Library/Preferences/DirectoryService/.DSTCPListening", &statResult ) == eDSNoErr)
		{
			// Start the TCP listener thread
			result = StartTCPListener(kDSDefaultListenPort);
			if ( result != eDSNoErr ) throw( result );
		}

		if ( gPluginHandler == nil )
		{
			gPluginHandler = new CPluginHandler;
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

		delete(gTCPHandlerLock);
		delete(gHandlerLock);
		delete(gInternalHandlerLock);
		delete(gCheckpwHandlerLock);

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
	CFStringRef			atKey				= 0;	//Appletalk changes key
//	CFStringRef			dnsKey				= 0;	//DNS changes key
//	CFStringRef			nisKey				= 0;	//NIS changes key
//	CFStringRef			computerNameKey 	= 0;	//computer name changes key
	CFMutableArrayRef	notifyKeys			= 0;
	CFMutableArrayRef	notifyPatterns		= 0;
	Boolean				setStatus			= FALSE;
	CFStringRef			aPIDString			= 0;
	
	DBGLOG( kLogApplication, "RegisterForNetworkChange(): " );

	fSCDStore = SCDynamicStoreCreate(NULL, CFSTR("DirectoryService"), NULL, NULL); //KW use constant string instead

	if (fSCDStore != 0)
	{
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
		atKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetAppleTalk);
		CFArrayAppendValue(notifyKeys, atKey);
		CFRelease(atKey);
    
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
               
        setStatus = SCDynamicStoreSetNotificationKeys(fSCDStore, notifyKeys, notifyPatterns);
		CFRelease(notifyKeys);
		CFRelease(notifyPatterns);

		//SCDOptionSet(NULL, kSCDOptionUseCFRunLoop, FALSE);
		
		//not checking bool return
		SCDynamicStoreNotifyCallback( fSCDStore, gServerRunLoop, NetworkChangeCallBack, this );
		
		//this is update code for things like NetInfo and DHCP issues
		//ie. calls to the search node that verify or re-establish the default NI and LDAPv3(from DHCP) nodes
		
		//send SIGHUP on a network transition
		//setStatus = SCDynamicStoreNotifySignal( fSCDStore, getpid(), SIGHUP);
		
		aPIDString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), (uInt32)getpid());
		setStatus = SCDynamicStoreAddTemporaryValue( fSCDStore, CFSTR("DirectoryService:PID"), aPIDString );
   		CFRelease(aPIDString);
		
	} // SCDSessionRef okay

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

	if (fSCDStore != 0)
	{
		CFRelease(fSCDStore);
		fSCDStore = 0;
	}
	
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
		pPIInfo = gPlugins->GetPlugInInfo( searchIterator-1 );
		siResult = eDSNoErr;
		siResult = searchPlugin->ProcessRequest( (void*)&aHeader );
		if (siResult != eDSNoErr)
		{
			if (pPIInfo != nil)
			{
				ERRORLOG1( kLogApplication, "Network transition in Search returned error %d", siResult );
			}
		}
	}
				
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
	//IMPORTANT:LDAPv3 Idle TimeOut requires the 30 second periodic task here set
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
			siResult = eDSNoErr;
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
			pPlugin = gPlugins->Next( &iterator );
		}
	}
					
	return;
} // DoPeriodicTask


