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
 * @header CHandlers
 */

#include "CHandlers.h"
#include "ServerControl.h"
#include "CMsgQueue.h"
#include "PrivateTypes.h"
#include "CLog.h"
#include "CServerPlugin.h"
#include "PluginData.h"
#include "CSrvrMessaging.h"
#include "CNodeList.h"
#include "DSNetworkUtilities.h"
#include "DSUtils.h"
#include "DirServices.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "DirServicesUtils.h"
#include "CRefTable.h"
#include "CSharedData.h"
#include "CAuditUtils.h"
#include "Mbrd_MembershipResolver.h"

#include <servers/bootstrap.h>
#include <stdlib.h>
#include <time.h>		// for time()
#include <syslog.h>		// for syslog()
#include <sys/sysctl.h>	// for struct kinfo_proc and sysctl()
#include <sys/time.h>	// for struct timespec and gettimeofday()
#include <mach/mach.h>	// for mach destroy of client port

typedef enum CheckpwResult {
	DS_CHECKPW_SUCCESS = 0,
	DS_CHECKPW_UNKNOWNUSER = -1,
	DS_CHECKPW_BADPASSWORD = -2,
	DS_CHECKPW_FAILURE = -3,
	DS_CHECKPW_LOCKOUT = -4
} CheckpwResult;

// This is for MIG
extern "C" {
	#include "DirectoryServiceMIGServer.h"
	#include "DSlibinfoMIGServer.h"
	#include "DSmemberdMIGServer.h"
	#include "DSlibinfoMIGAsyncReply.h"
}

static const char *sServerMsgType[ 14 ] =
{
	/* 00 */	"*** Start of list ***",
	/* 01 */	"dsOpenDirService()",
	/*    */	"dsCloseDirService()",
	/*    */	"dsGetDirNodeName()",
	/*    */	"dsGetDirNodeCount()",
	/* 05 */	"dsGetDirNodeChangeToken()",
	/*    */	"dsGetDirNodeList()",
	/*    */	"dsFindDirNodes()",
	/*    */	"dsVerifyDirRefNum()",
	/*    */	"checkpw()",
	/* 10 */	"dsAddChildPIDToReference()",
	/*    */	"dsOpenDirServiceProxy()",
	/*    */	"dsOpenDirServiceLocal()",
	/* 13 */	"*** End of list ***"
};

static const char *sPlugInMsgType[ 41 ] =
{
	/* 00 */	"*** Start of list ***",
	/* 01 */	"dsReleaseContinueData()",
	/*    */	"dsOpenDirNode()",
	/*    */	"dsCloseDirNode()",
	/*    */	"dsGetDirNodeInfo()",
	/* 05 */	"dsGetRecordList()",
	/*    */	"dsGetRecordEntry()",
	/*    */	"dsGetAttributeEntry()",
	/*    */	"dsGetAttributeValue()",
	/*    */	"dsOpenRecord()",
	/* 10 */	"dsGetRecordReferenceInfo()",
	/*    */	"dsGetRecordAttributeInfo()",
	/*    */	"dsGetRecordAttributeValueByID()",
	/*    */	"dsGetRecordAttributeValueByIndex()",
	/*    */	"dsFlushRecord()",
	/* 15 */	"dsCloseRecord()",
	/*    */	"dsSetRecordName()",
	/*    */	"dsSetRecordType()",
	/*    */	"dsDeleteRecord()",
	/*    */	"dsCreateRecord()",
	/* 20 */	"dsCreateRecordAndOpen()",
	/*    */	"dsAddAttribute()",
	/*    */	"dsRemoveAttribute()",
	/*    */	"dsAddAttributeValue()",
	/*    */	"dsRemoveAttributeValue()",
	/* 25 */	"dsSetAttributeValue()",
	/*    */	"dsDoDirNodeAuth()",
	/*    */	"dsDoAttributeValueSearch()",
	/*    */	"dsDoAttributeValueSearchWithData()",
	/*    */	"dsDoPlugInCustomCall()",
	/* 30 */	"dsCloseAttributeList()",
	/*    */	"dsCloseAttributeValueList()",
	/*    */	"HandleNetworkTransition()",
	/*    */	"ReceiveServerRunLoop()",
	/*    */	"dsDoDirNodeAuthOnRecordType()",
	/* 35 */	"DoCheckNIAutoSwitch()",
	/*    */	"dsGetRecordAttributeValueByValue()",
	/*    */	"dsDoMultipleAttributeValueSearch()",
	/*    */	"dsDoMultipleAttributeValueSearchWithData()",
	/* 39 */	"dsSetAttributeValues()",
	/*    */	"*** End of list ***"
};

// --------------------------------------------------------------------------------
//	* Globals
// --------------------------------------------------------------------------------

static tDirReference			gCheckPasswordDSRef			= 0;
static tDirNodeReference		gCheckPasswordSearchNodeRef	= 0;

// --------------------------------------------------------------------------------
//	* Externs
// --------------------------------------------------------------------------------

extern dsBool					gLogAPICalls;
extern dsBool					gDebugLogging;
extern dsBool					gDSFWCSBPDebugLogging;
extern dsBool					gDSLocalOnlyMode;

//API Call Count
extern UInt32					gAPICallCount;

extern UInt32					gDaemonPID;

extern UInt32					gDaemonIPAddress;

extern DSMutexSemaphore		   *gMachThreadLock;
extern UInt32					gActiveMachThreads;
extern UInt32					gActiveLongRequests;
extern mach_port_t				gMachMIGSet;
extern map<mach_port_t, pid_t>	gPIDMachMap;

extern DSMutexSemaphore		   *gLibinfoQueueLock;
extern char					   *gDSLocalFilePath;
extern UInt32					gLocalSessionCount;
extern CCachePlugin			   *gCacheNode;

extern CMsgQueue               *gLibinfoQueue;
extern CCachePlugin            *gCacheNode;
extern const char              *lookupProcedures[];

extern CFRunLoopRef				gPluginRunLoop;
extern DSEventSemaphore			gPluginRunLoopEvent;

extern void	mig_spawnonceifnecessary( void );

extern	CPlugInList			   *gPlugins;
// ---------------------------------------------------------------------------
//	* MIG routines
//
// ---------------------------------------------------------------------------

// used to process the notify messages to clean up the pid list...
static boolean_t dsmig_demux_notify( mach_msg_header_t *request, mach_msg_header_t *reply )
{
	if (request->msgh_id >= 60000 ) // 60000 are memberd requests
	{
		return DSmemberdMIG_server( request, reply );
	}
	else if( request->msgh_id >= 50000 ) // 50000 are libinfo requests
	{
		return DSlibinfoMIG_server( request, reply );
	}

	// all other requests are API calls
	boolean_t returnValue = DirectoryServiceMIG_server(request, reply);

	if( request->msgh_id == MACH_NOTIFY_NO_SENDERS )
	{
		// let's spawn another thread to handle requests while this is working, since we do not know how long this will go
		gMachThreadLock->WaitLock();
		gActiveLongRequests++;
		gMachThreadLock->SignalLock();

		// we should spawn the thread after we've incremented the number of requests active, otherwise thread will spawn and exit too soon
		mig_spawnonceifnecessary();

		gMachThreadLock->WaitLock();
		pid_t aPID = gPIDMachMap[request->msgh_local_port];
		gPIDMachMap.erase( request->msgh_local_port );
		
		// before we erase the refs, lets see if there is a mach port already here for the same PID
		map<mach_port_t, pid_t>::const_iterator iter;
		
		for ( iter = gPIDMachMap.begin(); iter != gPIDMachMap.end() && iter->second != aPID; iter++ )
		{
			// we're looking to see if the map has a pid with another mach port
		}
		gMachThreadLock->SignalLock();

		// TODO: should we have to keep our lock?
		if ( iter == gPIDMachMap.end() )
		{
			DbgLog( kLogHandler, "dsmig_demux_notify:: Client PID: %d has exited and/or closed its mach port", aPID );
			CRefTable::CleanClientRefs( gDaemonIPAddress, aPID );				
		}
		else
		{
			DbgLog( kLogHandler, "dsmig_demux_notify:: Client PID: %d closed and re-opened another mach port", aPID );
		}

		gMachThreadLock->WaitLock();
		gActiveLongRequests--;
		gMachThreadLock->SignalLock();

		// clean up the mach message port
		mach_port_mod_refs( mach_task_self(), request->msgh_local_port, MACH_PORT_RIGHT_RECEIVE, -1 );
	}

	return returnValue;
}

#pragma mark -
#pragma mark Main Handler Routines
#pragma mark -

//--------------------------------------------------------------------------------------------------
//	* CHandlerThread(const UInt32 inThreadSignature)
//
//--------------------------------------------------------------------------------------------------

CHandlerThread::CHandlerThread ( const UInt32 inThreadSignature, UInt32 iThread ) : CInternalDispatchThread(inThreadSignature)
{
	fThreadSignature = inThreadSignature;

	fThreadIndex	= iThread;
	fTCPEndPt		= nil;
} // CHandlerThread ( UInt32 inThreadSignature )

//--------------------------------------------------------------------------------------------------
//	* ~CHandlerThread()
//
//--------------------------------------------------------------------------------------------------

CHandlerThread::~CHandlerThread()
{
} // ~CHandlerThread

//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CHandlerThread::StartThread ( void )
{
	if ( this == nil ) throw((SInt32)eMemoryError);

	this->Resume();
} // StartThread


//--------------------------------------------------------------------------------------------------
//	* LastChance()
//
//--------------------------------------------------------------------------------------------------

void CHandlerThread:: LastChance ( void )
{
	//StopAHandler needs to be done atomically within MainThread!
	//ie. this is only a safety net in case of an unknown throw

    if (fThreadSignature == kTSLibinfoQueueThread)
	{
		gLibinfoQueueLock->WaitLock();
		gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
		gLibinfoQueueLock->SignalLock();
	}
	
} // LastChance


//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CHandlerThread::StopThread ( void )
{
	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


// ----------------------------------------------------------------------------
//	* LogQueueDepth()
//
// ----------------------------------------------------------------------------

void CHandlerThread::LogQueueDepth ( void )
{
} // LogQueueDepth

//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

SInt32 CHandlerThread::ThreadMain ( void )
{
    if (fThreadSignature == kTSLibinfoQueueThread)
	{
		while ( GetThreadRunState() != kThreadStop )
		{
			try
			{
                while ( GetThreadRunState() != kThreadStop )
            	{
                    // when we come up we immediately attempt to handle a message
                    HandleMessage();
                    
                    // if we got false, we timed out waiting on messages
                    // double check if there is no queue to prevent race condition
                    DbgLog( kLogHandler, "CHandlerThread::LibinfoHandler %d sleeping for 2 secs", fThreadIndex );
                    if( gLibinfoQueue->WaitOnMessage(2 * kMilliSecsPerSec) == false )
                    {
                        DbgLog( kLogHandler, "CHandlerThread::LibinfoHandler %d timed out", fThreadIndex );
                        
                        // timer gave up, let's quit this thread
                        gLibinfoQueueLock->WaitLock();
						gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
                        StopThread();
						gLibinfoQueueLock->SignalLock();
                    }
				} // while thread not stopped state
			} // try
			
			catch( SInt32 err1 )
			{
                DbgLog( kLogHandler, "File: %s. Line: %d", __FILE__, __LINE__ );
                DbgLog( kLogHandler, "  *** Caught exception (#2).  Error = %d", err1 );
			}
		} //while loop over run state        
	}
	
	return( 0 );

} // ThreadMain


//--------------------------------------------------------------------------------------------------
//	* HandleMessage()
//
//--------------------------------------------------------------------------------------------------

void CHandlerThread::HandleMessage ( void )
{
	void		   *reqMsg		= nil;
	CRequestHandler handler;

    if (fThreadSignature == kTSLibinfoQueueThread)
	{
		if (gLibinfoQueue != nil)
		{
			while( gLibinfoQueue->DequeueMessage( &reqMsg ) )
            {
                sLibinfoRequest *pRequest = (sLibinfoRequest *)reqMsg;
                
                //now this method owns the pRequest data
                if ( pRequest != nil )
                {
                    char    *debugDataTag = NULL;
                    pid_t   aPID;
                    
                    audit_token_to_au32( pRequest->fToken, NULL, NULL, NULL, NULL, NULL, &aPID, NULL, NULL );
                    
                    if ( (gDebugLogging) || (gLogAPICalls) )
                    {
                        debugDataTag = handler.BuildAPICallDebugDataTag( gDaemonIPAddress, aPID, "libinfo", "Server" );
                        DbgLog( kLogHandler, "%s : libinfomig DAC : Async Procedure = %s (%d) : Handle request %X", debugDataTag, lookupProcedures[pRequest->fProcedure], pRequest->fProcedure, pRequest );
                    }                    
                    
                    // now process the request
                    kvbuf_t *pResponse = gCacheNode->ProcessLookupRequest( pRequest->fProcedure, pRequest->fBuffer, pRequest->fBufferLen, aPID );
                    
                    if ( pResponse != NULL )
                    {
                        if( pResponse->datalen <= MAX_MIG_INLINE_DATA )
                        {
                            if ( libinfoDSmig_Response_async( pRequest->fReplyPort, pResponse->databuf, pResponse->datalen, 0, 0, pRequest->fCallbackAddr ) != MACH_MSG_SUCCESS )
                            {
                                // need to clean up mach port if we got any error, we can't recover
								mach_port_destroy( mach_task_self(), pRequest->fReplyPort );
                            }
                        }
                        else
                        {
                            vm_offset_t data = 0;
                            mach_msg_type_number_t dataCnt = 0;
                            
                            vm_read( mach_task_self(), (vm_address_t) pResponse->databuf, pResponse->datalen, &data, &dataCnt );
                            
                            if ( libinfoDSmig_Response_async( pRequest->fReplyPort, "", 0, data, dataCnt, pRequest->fCallbackAddr ) != MACH_MSG_SUCCESS )
                            {
                                // if we get any error, then we need to clean up the vm_dealloc
                                vm_deallocate( mach_task_self(), data, dataCnt );
								mach_port_destroy( mach_task_self(), pRequest->fReplyPort );
                            }
                        }
                    }
                    else
                    {
                        if ( libinfoDSmig_Response_async( pRequest->fReplyPort, "", 0, 0, 0, pRequest->fCallbackAddr ) != MACH_MSG_SUCCESS )
                        {
							// need to clean up mach port if we got any error, we can't recover
							mach_port_destroy( mach_task_self(), pRequest->fReplyPort );
                        }
                    }
					
					kvbuf_free( pResponse );
					pResponse = NULL;

                    if( debugDataTag )
                    {
                        DbgLog( kLogHandler, "%s : libinfomig DAR : Async Procedure = %s (%d)", debugDataTag, lookupProcedures[pRequest->fProcedure], pRequest->fProcedure );
                        free( debugDataTag );
                        debugDataTag = NULL;
                    }
                    
                    free( pRequest->fBuffer );
                    pRequest->fBuffer = NULL;
                    
                    free( pRequest );
                    pRequest = NULL;
                }
			}            
		}
	}

} // HandleMessage


// ----------------------------------------------------------------------------
//	* GetOurThreadRunState()
//
// ----------------------------------------------------------------------------

UInt32 CHandlerThread::GetOurThreadRunState ( void )
{
	return( GetThreadRunState() );
} // GetOurThreadRunState


// ----------------------------------------------------------------------------
//	* RefDeallocProc()
//    used to clean up plug-in specific data for a reference
// ----------------------------------------------------------------------------

SInt32 CHandlerThread::RefDeallocProc ( UInt32 inRefNum, UInt32 inRefType, CServerPlugin *inPluginPtr )
{
	SInt32	dsResult	= eDSNoErr;
	double	inTime		= 0;
	double	outTime		= 0;

	if (inPluginPtr != nil)
	{
		// we should call the plug-in to clean up its table
		sCloseDirNode closeData;
		closeData.fResult = eDSNoErr;
		closeData.fInNodeRef = inRefNum;
		switch (inRefType)
		{
			case eNodeRefType:
				closeData.fType = kCloseDirNode;
				break;

			case eRecordRefType:
				closeData.fType = kCloseRecord;
				break;

			case eAttrListRefType:
				closeData.fType = kCloseAttributeList;
				break;

			case eAttrValueListRefType:
				closeData.fType = kCloseAttributeValueList;
				break;

			default:
				closeData.fType = 0;
				break;
		}
		if (closeData.fType != 0)
		{
			if (gLogAPICalls)
			{
				inTime = dsTimestamp();
			}
			inPluginPtr->ProcessRequest( &closeData );
			dsResult = closeData.fResult;
			if (gLogAPICalls)
			{
				outTime = dsTimestamp();
				syslog(LOG_CRIT,"Ref table dealloc callback, API Call: %s, PlugIn Used: %s, Result: %d, Duration: %.2f usec",
					CRequestHandler::GetCallName( closeData.fType ), inPluginPtr->GetPluginName(), dsResult,
					(outTime - inTime));
			}
		}
	}
	
	return( dsResult );
} // RefDeallocProc

#pragma mark -
#pragma mark Plugin Runloop Thread
#pragma mark -

//--------------------------------------------------------------------------------------------------
//	* CPluginRunLoopThread()
//
//--------------------------------------------------------------------------------------------------

CPluginRunLoopThread::CPluginRunLoopThread ( void ) : CInternalDispatchThread(kTSPluginRunloopThread)
{
	fThreadSignature	= kTSMigHandlerThread;
} // CMigHandlerThread


//--------------------------------------------------------------------------------------------------
//	* ~CPluginRunLoopThread()
//
//--------------------------------------------------------------------------------------------------

CPluginRunLoopThread::~CMigHandlerThread()
{
} // ~CMigHandlerThread

//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CPluginRunLoopThread::StartThread ( void )
{
	Resume();
} // StartThread


//--------------------------------------------------------------------------------------------------
//	* LastChance()
//
//--------------------------------------------------------------------------------------------------

void CPluginRunLoopThread:: LastChance ( void )
{
	//nothing to do here
} // LastChance


//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CPluginRunLoopThread::StopThread ( void )
{
	SetThreadRunState( kThreadStop );		// Tell our thread to stop
} // StopThread

static CFDataRef BogusHandler ( CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info )
{
	// this function serves no purpose other than to have a source on the CPluginRunLoopThread runloop
	return NULL;
}

//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

SInt32 CPluginRunLoopThread::ThreadMain ( void )
{
	CFMessagePortRef myPort;
	CFRunLoopSourceRef rlSource;
	CFMessagePortContext context = {1, NULL, NULL, NULL, NULL};
	CFStringRef cfName = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("DirectoryService::CPluginRunLoopThread.%d"), getpid() );
	
	gPluginRunLoop = CFRunLoopGetCurrent();

	myPort = CFMessagePortCreateLocal( NULL, cfName, &BogusHandler, &context, NULL);
	rlSource = CFMessagePortCreateRunLoopSource(NULL, myPort, 0);
	
	if (rlSource)
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rlSource, kCFRunLoopCommonModes);

	CFRelease( cfName );
	
	gPluginRunLoopEvent.PostEvent();
	
	CFRunLoopRun();
	
	//not really needed
	StopThread();
	
	return( 0 );
	
} // ThreadMain


#pragma mark -
#pragma mark Specific MIG Handler Routines
#pragma mark -

//--------------------------------------------------------------------------------------------------
//	* CMigHandlerThread(const UInt32 inThreadSignature, bool bMigHelper)
//
//--------------------------------------------------------------------------------------------------

CMigHandlerThread::CMigHandlerThread ( const UInt32 inThreadSignature, bool bMigHelper ) : CInternalDispatchThread(inThreadSignature)
{
	fThreadSignature	= inThreadSignature;
	bMigHelperThread	= bMigHelper;
} // CMigHandlerThread ( UInt32 inThreadSignature, bool bMigHelper )

//--------------------------------------------------------------------------------------------------
//	* ~CMigHandlerThread()
//
//--------------------------------------------------------------------------------------------------

CMigHandlerThread::~CMigHandlerThread()
{
} // ~CMigHandlerThread

//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CMigHandlerThread::StartThread ( void )
{
	if ( this == nil ) throw((SInt32)eMemoryError);

	this->Resume();
} // StartThread


//--------------------------------------------------------------------------------------------------
//	* LastChance()
//
//--------------------------------------------------------------------------------------------------

void CMigHandlerThread:: LastChance ( void )
{
	//nothing to do here
} // LastChance


//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CMigHandlerThread::StopThread ( void )
{
	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

SInt32 CMigHandlerThread::ThreadMain ( void )
{
	kern_return_t kr;
	
	if (bMigHelperThread) //secondary mig handler threads spawned as worker threads up to a maximum
	{
		gMachThreadLock->WaitLock();
		gActiveMachThreads++;
		gMachThreadLock->SignalLock();
		
		// we loop while our threads are less than or equal to the number of active long requests
		while( 1 )
		{
			gMachThreadLock->WaitLock();
			if( gActiveMachThreads <= gActiveLongRequests )
			{
				gMachThreadLock->SignalLock();
				
				(void) mach_msg_server_once( dsmig_demux_notify, 
											 kMaxMIGMsg, // see SharedConsts.h for notes
											 gMachMIGSet,
											 MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) );
			}
			else
			{
				gActiveMachThreads--;
				gMachThreadLock->SignalLock();
				break;
			}
		};
	}
	else //main mig listener thread
	{
		// here we initialize memberd because we need to be on an internal thread to do it and before our listeners go live..
		if (!gDSLocalOnlyMode)
			Mbrd_Initialize();

		// if someone sends us the wrong size..
		do
		{
			//mach_msg_server only returns if port set removed or for the MACH_RCV_TOO_LARGE error
			kr = mach_msg_server( dsmig_demux_notify,
								  kMaxMIGMsg, // see SharedConsts.h for notes
								  gMachMIGSet,
								  MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) );
			if( kr == MACH_RCV_TOO_LARGE )
			{
				syslog( LOG_ALERT, "mig_server received a message that was too large, someone sending bad data?" );
			}
		} while ( kr == MACH_RCV_TOO_LARGE );

		(void) mach_port_destroy( mach_task_self(), gMachMIGSet );
		gMachMIGSet = MACH_PORT_NULL;
	}
	
	//not really needed
	StopThread();
	
	return( 0 );

} // ThreadMain

#pragma mark -
#pragma mark Specific memberd kernel Handler Routines
#pragma mark -

//--------------------------------------------------------------------------------------------------
//	* CMemberdKernelHandlerThread(const UInt32 inThreadSignature)
//
//--------------------------------------------------------------------------------------------------

CMemberdKernelHandlerThread::CMemberdKernelHandlerThread ( const UInt32 inThreadSignature ) : CInternalDispatchThread(inThreadSignature)
{
	fThreadSignature	= inThreadSignature;
} // CMemberdKernelHandlerThread ( UInt32 inThreadSignature )

//--------------------------------------------------------------------------------------------------
//	* ~CMemberdKernelHandlerThread()
//
//--------------------------------------------------------------------------------------------------

CMemberdKernelHandlerThread::~CMemberdKernelHandlerThread()
{
} // ~CMemberdKernelHandlerThread

//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CMemberdKernelHandlerThread::StartThread ( void )
{
	if ( this == nil ) throw((SInt32)eMemoryError);

	this->Resume();
} // StartThread


//--------------------------------------------------------------------------------------------------
//	* LastChance()
//
//--------------------------------------------------------------------------------------------------

void CMemberdKernelHandlerThread:: LastChance ( void )
{
	//nothing to do here
} // LastChance


//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CMemberdKernelHandlerThread::StopThread ( void )
{
	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

SInt32 CMemberdKernelHandlerThread::ThreadMain ( void )
{
	static pthread_mutex_t		localMutex		= PTHREAD_MUTEX_INITIALIZER;
	static int					threadWaiting	= 1; // we start with one
	kauth_identity_extlookup	request;
	int							result;
	int							workresult		= 0;
	int							loop			= 1;

	do
	{
		result = syscall(SYS_identitysvc, KAUTH_EXTLOOKUP_WORKER | (workresult ? KAUTH_EXTLOOKUP_RESULT : 0), &request);
		if (result != 0)
		{
			syslog(LOG_CRIT, "Fatal error %d submitting to kernel (%d: %s)\n", result, errno, strerror(errno));
			exit(1);
		}

		pthread_mutex_lock( &localMutex );
		threadWaiting--;
		if (threadWaiting <= 0)
		{
			// This thread was the only thread waiting, so fire off another one
			threadWaiting++;
			CMemberdKernelHandlerThread *aThread = new CMemberdKernelHandlerThread(kTSMemberdKernelHndlrThread);
			aThread->StartThread();
		}
		pthread_mutex_unlock( &localMutex );
		
		if ( (gDebugLogging) || (gLogAPICalls) )
		{
			DbgLog( kLogAPICalls, "Client: kernel, PID: 0, API: mbrd_syscall, Server Used : DAC : Handling Kernel lookup request" );
		}                    

		Mbrd_ProcessLookup( &request );
		
		if ( (gDebugLogging) || (gLogAPICalls) )
		{
			DbgLog( kLogAPICalls, "Client: kernel, PID: 0, API: mbrd_syscall, Server Used : DAC : Kernel lookup result" );
		}

		// we only want 1 thread waiting at any given time, no way to time them out, so we have to spawn them on demand
		pthread_mutex_lock( &localMutex );
		if (threadWaiting >= 1)
			loop = 0;
		else
			threadWaiting++;
		pthread_mutex_unlock( &localMutex );

		workresult = 1;
	} while( loop );
	
	// send the last reply
	result = syscall(SYS_identitysvc, KAUTH_EXTLOOKUP_RESULT, &request);
	
	//not really needed
	StopThread();
	
	return( 0 );

} // ThreadMain


#pragma mark -
#pragma mark Request Handler Routines
#pragma mark -

CRequestHandler::CRequestHandler( void )
{
	bClosePort		= false;
}

bool CRequestHandler::HandleRequest ( sComData **inMsg )
{
	SInt32			siResult	= eDSNoErr;
	UInt32			uiMsgType	= 0;

	if ( IsServerRequest( *inMsg ) == true )
	{
		siResult = HandleServerCall( inMsg );
	}
	else if ( IsPluginRequest( *inMsg ) == true )
	{
		siResult = HandlePluginCall( inMsg );
	}
	else
	{
		(void)HandleUnknownCall( *inMsg );
	}
	
	if ( siResult != eDSNoErr )
	{
		(void)SetRequestResult( *inMsg, siResult );
	
		uiMsgType = GetMsgType( *inMsg );
		if (siResult != eDSRecordTypeDisabled)
		{
			DbgLog(	kLogMsgTrans, "Port: %l Call: %s == %l", (*inMsg)->head.msgh_remote_port, GetCallName( uiMsgType ), siResult );
		}
	}
    
    bool result = bClosePort;
    bClosePort = false;
    
    return result;
}


//--------------------------------------------------------------------------------------------------
//	* IsServerRequest()
//
//--------------------------------------------------------------------------------------------------

bool CRequestHandler::IsServerRequest ( sComData *inMsg )
{
	bool				bResult		= false;
	UInt32				uiMsgType	= 0;

	uiMsgType = GetMsgType( inMsg );

	if ( (uiMsgType >= kOpenDirService) && (uiMsgType < kDSServerCallsEnd)  )
	{
		bResult = true;
	}

	return( bResult );

} // IsServerRequest


//--------------------------------------------------------------------------------------------------
//	* IsPluginRequest()
//
//--------------------------------------------------------------------------------------------------

bool CRequestHandler::IsPluginRequest ( sComData *inMsg )
{
	bool				bResult		= false;
	UInt32				uiMsgType	= 0;

	uiMsgType = GetMsgType( inMsg );

	if ( (uiMsgType > kDSPlugInCallsBegin) && (uiMsgType < kDSPlugInCallsEnd) )
	{
		bResult = true;
	}

	return( bResult );

} // IsPluginRequest


//--------------------------------------------------------------------------------------------------
//	* HandleServerCall()
//
//--------------------------------------------------------------------------------------------------

SInt32 CRequestHandler::HandleServerCall ( sComData **inMsg )
{
	SInt32			siResult		= eDSNoErr;
	UInt32			uiMsgType		= 0;
	UInt32			count			= 0;
	UInt32			changeToken		= 0;
	UInt32			uiDirRef		= 0;
	UInt32			uiGenericRef   	= 0;
	UInt32			uiChildPID		= 0;
	tDataBuffer*	dataBuff		= NULL;
	tDataBuffer*	versDataBuff	= NULL;
	CSrvrMessaging	cMsg;
	SInt32			aClientPID		= -1;
	UInt32			anIPAddress		= 0;
	char		   *aMsgName		= nil;
	char		   *debugDataTag	= nil;
	double			inTime			= 0;
	double			outTime			= 0;
	bool			performanceStatGatheringActive = gSrvrCntl->IsPeformanceStatGatheringActive();

	uiMsgType	= GetMsgType( (*inMsg) );
	aMsgName	= GetCallName( uiMsgType );

	aClientPID	= (*inMsg)->fPID;
	anIPAddress	= (*inMsg)->fIPAddress;
	
	if ( (gDebugLogging) || (gLogAPICalls) )
	{
		debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, "Server");
	}
	
	if ( gLogAPICalls || performanceStatGatheringActive )
	{
		inTime = dsTimestamp();
	}

	switch ( uiMsgType )
	{
		case kOpenDirService:
		{
            siResult = CRefTable::NewDirRef( &uiDirRef, aClientPID, anIPAddress );

			DbgLog( kLogAPICalls, "%s : DAR : Dir Ref %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );
			if ( siResult == eDSNoErr )
			{
				// Add the dir reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), uiDirRef, ktDirRef );
			}
			
			break;
		}

		case kOpenDirServiceProxy:
		{
			// need to check version match
			//KW might prefer a constant for DSProxy1.3 and the number 10
			siResult = cMsg.Get_tDataBuff_FromMsg( (*inMsg), &versDataBuff, ktDataBuff );
			if ( siResult == eDSNoErr )
			{
				if ( (versDataBuff->fBufferLength == 10) && (strncmp(versDataBuff->fBufferData, "DSProxy1.3", 10) == 0 ) )
				{
					siResult = cMsg.Get_tDataBuff_FromMsg( (*inMsg), &dataBuff, kAuthStepBuff );
					if ( siResult == eDSNoErr )
					{
						SInt32 curr = 0;
						UInt32 len = 0;
						
						char* userName = NULL;
						char* password = NULL;
						char* shortName = NULL;
						uid_t localUID = (uid_t)-2;
						
						siResult = -3;
		
						if ( dataBuff->fBufferLength >= 8 )
						{ // need at least 8 bytes for lengths
							// User Name
							::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( UInt32 ) );
							curr += sizeof( UInt32 );
							if ( len <= dataBuff->fBufferLength - curr )
							{
								userName = (char*)::calloc( len + 1, sizeof( char ) );
								::memcpy( userName, &(dataBuff->fBufferData[ curr ]), len );
								curr += len;
								DbgLog( kLogAPICalls, "%s : DAC : 1 : Username = %s", debugDataTag, userName );
		
								// Password
								::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( UInt32 ) );
								curr += sizeof ( UInt32 );
								if ( len <= dataBuff->fBufferLength - curr )
								{
									password = (char*)::calloc( len + 1, sizeof( char ) );
									::memcpy( password, &(dataBuff->fBufferData[ curr ]), len );
									curr += len;
									DbgLog( kLogAPICalls, "%s : DAC : 2 : A password was received", debugDataTag );
		
									//case insensitivity of username allowed for DSProxy
									siResult = DoCheckUserNameAndPassword( userName, password, eDSiExact, &localUID, &shortName );
									// now we need to check that the user is in the admin group
									if ( (siResult == 0) && (localUID != 0) && !UserInGivenGroup(shortName, "admin") &&
										 !UserInGivenGroup(shortName, "com.apple.access_dsproxy") &&
										 !UserInGivenGroup(shortName, "com.apple.admin_dsproxy") )
									{
										DbgLog( kLogAPICalls, "%s : DAC : 3 : Username = %s is not an ADMIN user nor DSProxy SACL member", debugDataTag, userName );
										siResult = -3;
									}
								}
							}
						}
						if ( shortName != NULL )
						{
							::free( shortName );
							shortName = NULL;
						}
						if ( userName != NULL )
						{
							::free( userName );
							userName = NULL;
						}
						if ( password != NULL )
						{
							bzero(password, strlen(password));
							::free( password );
							password = NULL;
						}
					}
		
					if (siResult == eDSNoErr) //if auth succeeded
					{
						siResult = CRefTable::NewDirRef( &uiDirRef, aClientPID, anIPAddress );
						UInt32 serverVersion = 1;
		
						DbgLog( kLogAPICalls, "%s : DAR : Dir Ref %u : Server version = %u : Result code = %d", debugDataTag, uiDirRef, serverVersion, siResult );
						siResult = SetRequestResult( (*inMsg), siResult );
						if ( siResult == eDSNoErr )
						{
							// Add the dir reference
							siResult = cMsg.Add_Value_ToMsg( (*inMsg), uiDirRef, ktDirRef );
							// Add the server DSProxy version of 1
							siResult = cMsg.Add_Value_ToMsg( (*inMsg), serverVersion, kNodeCount );
						}
					}
					else
					{
						siResult = eDSAuthFailed;
					}
					if ( dataBuff != NULL )
					{
						bzero(dataBuff->fBufferData, dataBuff->fBufferLength);
						dsDataBufferDeallocatePriv( dataBuff );
						dataBuff = NULL;
					}
				}
				else
				{
					siResult = eDSTCPVersionMismatch;
				}
			}
			if ( versDataBuff != NULL )
			{
				dsDataBufferDeallocatePriv( versDataBuff );
				versDataBuff = NULL;
			}
			
			break;
		}

		case kOpenDirServiceLocal:
		{
			if (gDSLocalOnlyMode)
			{
				siResult = cMsg.Get_tDataBuff_FromMsg( (*inMsg), &versDataBuff, ktDataBuff );
				if ( siResult == eDSNoErr )
				{
					if ( strncmp(versDataBuff->fBufferData, "Default", 7) == 0 )
					{
						//gDSLocalFilePath stays NULL?
					}
					else if ( versDataBuff->fBufferLength > 0 ) //assume file path provided
					{
						if (gDSLocalFilePath != NULL)
						{
							if (gLocalSessionCount == 0) //let us in new since we have closed all previous sessions
							{
								DSFreeString(gDSLocalFilePath);
								gDSLocalFilePath = strdup(versDataBuff->fBufferData);
							}
							else //let us in again if we ask for the same node
							{
								if (strcmp(gDSLocalFilePath, versDataBuff->fBufferData) != 0)
								{
									siResult = eDSCannotAccessSession;
								}
							}
						}
						else
						{
							gDSLocalFilePath = strdup(versDataBuff->fBufferData);
						}
					}
					else
					{
						siResult = eDSCannotAccessSession;
					}
				}
				if ( versDataBuff != NULL )
				{
					dsDataBufferDeallocatePriv( versDataBuff );
					versDataBuff = NULL;
				}
				
				if (siResult == eDSNoErr)
				{
					siResult = CRefTable::NewDirRef( &uiDirRef, aClientPID, anIPAddress );

					DbgLog( kLogAPICalls, "%s : DAR : Dir Ref %u : Result code = %d", debugDataTag, uiDirRef, siResult );
					siResult = SetRequestResult( (*inMsg), siResult );
					if ( siResult == eDSNoErr )
					{
						// Add the dir reference
						siResult = cMsg.Add_Value_ToMsg( (*inMsg), uiDirRef, ktDirRef );
						gLocalSessionCount++;
					}
				}
			}
			else
			{
				siResult = eDSCannotAccessSession;
			}
			break;
		}

		case kCloseDirService:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			DbgLog( kLogAPICalls, "%s : DAC : Dir Ref %u ", debugDataTag, uiDirRef );
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &count, kNodeCount );

			siResult = CRefTable::RemoveDirRef( uiDirRef, aClientPID, anIPAddress );
			if ( gDSLocalOnlyMode && (siResult == eDSNoErr) )
			{
				gLocalSessionCount--;
				if (gLocalSessionCount < 1)
				{
					gLocalSessionCount = 0; //periodic task will check to see if we can exit this localonly daemon
				}
			}

			DbgLog( kLogAPICalls, "%s : DAR : Dir Ref %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );
			if ( count == 1 )
			{
				bClosePort = true;
			}
			
			break;
		}

		case kGetDirNodeCount:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			DbgLog( kLogAPICalls, "%s : DAC : Dir Ref %u ", debugDataTag, uiDirRef );
			if ( siResult == eDSNoErr )
			{
				siResult = CRefTable::VerifyDirRef( uiDirRef, nil, aClientPID, anIPAddress );

				cMsg.ClearDataBlock( (*inMsg) );

				if ( siResult == eDSNoErr )
				{
					count = gNodeList->GetNodeCount();
					DbgLog( kLogAPICalls, "%s : DAR : Dir Ref = %u : Node Count = %u : Result code = %d", debugDataTag, uiDirRef, count, siResult );
					siResult = SetRequestResult( (*inMsg), siResult );
					if ( siResult == eDSNoErr )
					{
						// Add the nodeCount
						siResult = cMsg.Add_Value_ToMsg( (*inMsg), count, kNodeCount );
					}
				}
				else
				{
					siResult = SetRequestResult( (*inMsg), siResult );
				}
			}
			break;
		}

		case kGetDirNodeChangeToken:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			DbgLog( kLogAPICalls, "%s : DAC : Dir Ref %u ", debugDataTag, uiDirRef );
			if ( siResult == eDSNoErr )
			{
				siResult = CRefTable::VerifyDirRef( uiDirRef, nil, aClientPID, anIPAddress );
				
				cMsg.ClearDataBlock( (*inMsg) );

				if ( siResult == eDSNoErr )
				{
					DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
					siResult = SetRequestResult( (*inMsg), siResult );
					if ( siResult == eDSNoErr )
					{
						count = gNodeList->GetNodeCount();
						// Add the nodeCount
						siResult = cMsg.Add_Value_ToMsg( (*inMsg), count, kNodeCount );

						changeToken = gNodeList->GetNodeChangeToken();
						// Add the changeToken
						siResult = cMsg.Add_Value_ToMsg( (*inMsg), changeToken, kNodeChangeToken );
						DbgLog( kLogAPICalls, "%s : DAR : 2 : Dir Ref = %u : Node Count = %u : Change Token = %u", debugDataTag, uiDirRef, count, changeToken );
					}
				}
				else
				{
					siResult = SetRequestResult( (*inMsg), siResult );
				}
			}
			break;
		}

		case kGetDirNodeList:
		{
			void	*newData	= nil;

			if (gDebugLogging)
			{
				UInt32 aLength = 0;
				cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
				cMsg.Get_Value_FromMsg( (*inMsg), &aLength, kOutBuffLen ); //no need to check return
				DbgLog( kLogAPICalls, "%s : DAC : Dir Ref %u : Data buffer size = %u", debugDataTag, uiDirRef, aLength );
			}
			
			newData = GetNodeList( (*inMsg), &siResult );
			if ( newData != nil )
			{
				if ( siResult == eDSNoErr )
				{
					siResult = PackageReply( newData, inMsg );
				}

				DoFreeMemory( newData );

				free( newData );
				newData = nil;
			}

			if ( siResult != eDSNoErr )
			{
				cMsg.ClearDataBlock( (*inMsg) );
				SetRequestResult( (*inMsg), siResult );
			}
			DbgLog( kLogAPICalls, "%s : DAR : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			break;
		}

		case kFindDirNodes:
		{
			void	*newData	= nil;

			if (gDebugLogging)
			{
				UInt32 aLength	= 0;
				cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
				cMsg.Get_Value_FromMsg( (*inMsg), &aLength, kOutBuffLen ); //no need to check return
				DbgLog( kLogAPICalls, "%s : DAC : Dir Ref %u : Data buffer size = %u", debugDataTag, uiDirRef, aLength );
			}
			
			newData = FindDirNodes( (*inMsg), &siResult, debugDataTag );
			if ( newData != nil )
			{
				if ( siResult == eDSNoErr )
				{
					siResult = PackageReply( newData, inMsg );
				}

				DoFreeMemory( newData );

				free( newData );
				newData = nil;
			}

			if ( siResult != eDSNoErr )
			{
				cMsg.ClearDataBlock( (*inMsg) );
				SetRequestResult( (*inMsg), siResult );
			}
			DbgLog( kLogAPICalls, "%s : DAR : 2 : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			break;
		}

		case kGetDirNodeName:
		{
			//KW this should no longer be used since handled in the FW now
			siResult = eNoLongerSupported;
			break;
		}

		case kVerifyDirRefNum:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			DbgLog( kLogAPICalls, "%s : DAC : Dir Ref %u", debugDataTag, uiDirRef );
			if ( siResult == eDSNoErr )
			{
				if ( uiDirRef == 0x00F0F0F0 )
				{
					siResult = eDSNoErr;
				}
				else
				{
					siResult = CRefTable::VerifyDirRef( uiDirRef, nil, aClientPID, anIPAddress );
				}
				
				cMsg.ClearDataBlock( (*inMsg) );
			}

			DbgLog( kLogAPICalls, "%s : DAR : Dir Ref %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );
			break;
		}

		case kAddChildPIDToReference:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiGenericRef, ktGenericRef );
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiChildPID, ktPidRef );

			siResult = CRefTable::AddChildPIDToRef( uiGenericRef, aClientPID, uiChildPID, anIPAddress );

			DbgLog( kLogAPICalls, "%s : DAR : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );
			break;
		}

		case kCheckUserNameAndPassword:
		{
		#if USE_BSM_AUDIT
			char *textStr = nil;
			UInt32 bsmEventCode = 0;
		#endif
		
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			siResult = cMsg.Get_tDataBuff_FromMsg( (*inMsg), &dataBuff, ktDataBuff );
			if ( siResult == eDSNoErr )
			{
				SInt32 curr = 0;
				UInt32 len = 0;
				
				char* userName = NULL;
				char* password = NULL;
				
				siResult = -3;

                if ( dataBuff->fBufferLength >= 8 )
                { // sanity check, need at least 8 bytes for lengths
					// User Name
					::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( UInt32 ) );
					curr += sizeof( UInt32 );
					if ( len <= dataBuff->fBufferLength - curr )
                    { // sanity check
						userName = (char*)::calloc( len + 1, sizeof( char ) );
						::memcpy( userName, &(dataBuff->fBufferData[ curr ]), len );
						curr += len;
						DbgLog( kLogAPICalls, "%s : DAC : 1 : Dir Ref = %u : Username = %s", debugDataTag, uiDirRef, userName );

					#if USE_BSM_AUDIT
						// need to evaluate the event
						bsmEventCode = AuditForThisEvent( kCheckUserNameAndPassword, userName, &textStr );
					#endif

						// Password
						::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( UInt32 ) );
						curr += sizeof ( UInt32 );
						if ( len <= dataBuff->fBufferLength - curr )
                        { // sanity check
							password = (char*)::calloc( len + 1, sizeof( char ) );
							::memcpy( password, &(dataBuff->fBufferData[ curr ]), len );
							curr += len;
							DbgLog( kLogAPICalls, "%s : DAC : 2 : Dir Ref = %u : A password was received", debugDataTag, uiDirRef );

							//username must be exact match for kCheckUserNameAndPassword
							siResult = DoCheckUserNameAndPassword( userName, password, eDSExact, NULL, NULL );
						}
					}
				}
				if ( dataBuff != NULL )
				{
					bzero(dataBuff->fBufferData, dataBuff->fBufferLength);
					dsDataBufferDeallocatePriv( dataBuff );
					dataBuff = NULL;
				}
				if ( userName != NULL )
				{
					::free( userName );
					userName = NULL;
				}
				if ( password != NULL )
				{
					bzero(password, strlen(password));
					::free( password );
					password = NULL;
				}
			}

			DbgLog( kLogAPICalls, "%s : DAR : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );

		#if USE_BSM_AUDIT
			// BSM Audit
			if ( bsmEventCode > 0 )
			{
				token_t *tok;
				au_tid_t tid = {0,0};
				
				if ( inMsg != NULL )
				{
					if ( siResult == eDSNoErr )
					{
						tok = au_to_text( textStr );
						audit_write_success( bsmEventCode, tok,
												(*inMsg)->fAuditUID,
												(*inMsg)->fEffectiveUID,
												(*inMsg)->fEffectiveGID,
												(*inMsg)->fUID,
												(*inMsg)->fGID,
												aClientPID,
												(*inMsg)->fAuditSID,
												&((*inMsg)->fTerminalID) );
					}
					else
					{
						audit_write_failure( bsmEventCode, textStr, (int)siResult,
												(*inMsg)->fAuditUID,
												(*inMsg)->fEffectiveUID,
												(*inMsg)->fEffectiveGID,
												(*inMsg)->fUID,
												(*inMsg)->fGID,
												aClientPID,
												(*inMsg)->fAuditSID,
												&((*inMsg)->fTerminalID) );
					}
				}
				else
				{
					if ( siResult == eDSNoErr )
					{
						tok = au_to_text( textStr );
						audit_write_success( bsmEventCode, tok, 0,0,0,0,0, aClientPID, 0, &tid );
					}
					else
					{
						audit_write_failure( bsmEventCode, textStr, (int)siResult, 0,0,0,0,0, aClientPID, 0, &tid );
					}
				}
			}
			DSFreeString( textStr );	// sets to NULL; required
		#endif
			break;
		}
	}

#ifdef BUILD_IN_PERFORMANCE
	if ( performanceStatGatheringActive )
	{
		outTime = dsTimestamp();
		gSrvrCntl->HandlePerformanceStats( uiMsgType, 0, siResult, aClientPID, inTime, outTime );
	}
#endif
	LogAPICall( inTime, debugDataTag, siResult);
	
	if ( siResult != eDSNoErr )
	{
		DbgLog( kLogHandler, "%s : HandleServerCall failed with error = %l", debugDataTag, siResult );
	}
	
	if (debugDataTag != nil)
	{
		free(debugDataTag);
		debugDataTag = nil;
	}

	return( siResult );

} // HandleServerCall


//--------------------------------------------------------------------------------------------------
//	* HandlePluginCall()
//
//--------------------------------------------------------------------------------------------------

SInt32 CRequestHandler::HandlePluginCall ( sComData **inMsg )
{
	SInt32			siResult		= eDSNoErr;
	void		   *pData			= nil;
	bool			shouldProcess   = true;
	bool			found			= false;
	UInt32			uiState			= 0;
	SInt32			aClientPID		= -1;
	UInt32			anIPAddress		= 0;
	char		   *aMsgName		= nil;
	char		   *debugDataTag	= nil;
	double			inTime			= 0;
	double			outTime			= 0;
	bool			performanceStatGatheringActive = gSrvrCntl->IsPeformanceStatGatheringActive();
	SInt32			pluginResult	= eDSNoErr;
	UInt32			type			= 0;
	char		   *textStr			= nil;
	
	// Set this to nil, it will get set in next call
	fPluginPtr = nil;

	aClientPID	= (*inMsg)->fPID;
	anIPAddress	= (*inMsg)->fIPAddress;
	
	type = GetMsgType( *inMsg );
	aMsgName = GetCallName( type );
	
	pData = GetRequestData( (*inMsg), &siResult, &shouldProcess ); //fPluginPtr gets set inside this call
	if ( siResult == eDSNoErr )
	{
		if ( pData != nil )
		{
			if ( fPluginPtr != nil )
			{
				siResult = gPlugins->GetState( fPluginPtr->GetPluginName(), &uiState );
				if ( siResult == eDSNoErr )
				{
					//debug output the request always
					if ( (gDebugLogging) || (gLogAPICalls) )
					{
						debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, fPluginPtr->GetPluginName());
						DebugAPIPluginCall(pData, debugDataTag);
					}

					if ( (uiState & kInitialized) && (!(uiState & kActive)) && (GetMsgType( *inMsg ) == kOpenDirNode) &&
						((fPluginPtr->GetPluginName()) != nil) && (strcmp(fPluginPtr->GetPluginName(),"Search") == 0 ) )
					{
						//wait to overcome race condition on setting search node to active?
						DSEventSemaphore    initWait;
						time_t              waitForIt = ::time( nil ) + 2;

						while (!( uiState & kActive ))
						{
							siResult = gPlugins->GetState( fPluginPtr->GetPluginName(), &uiState );
							if ( siResult == eDSNoErr )
							{
								// Wait for .2 seconds
								initWait.WaitForEvent( (UInt32)(.2 * kMilliSecsPerSec) );

								// Let's give it a couple of seconds, then we bail
								if ( ::time( nil ) > waitForIt )
								{
									break;
								}
							}
							else
							{
								break;
							}
						}
					}
					// always allow custom calls so we can configure even when the plug-in is disabled
					if ( (uiState & kActive) || (GetMsgType( *inMsg ) == kDoPlugInCustomCall) )
					{
						if ( gLogAPICalls || performanceStatGatheringActive )
						{
							inTime = dsTimestamp();
						}

						if ( shouldProcess )
						{
							#if USE_BSM_AUDIT
								// need to evaluate the event before calling the dispatch in case the
								// event is a DeleteRecord event. 
								UInt32 bsmEventCode = AuditForThisEvent( type, pData, &textStr );
							#endif
							
							siResult = fPluginPtr->ProcessRequest( pData );
							
							#if USE_BSM_AUDIT
								// BSM Audit
								if ( bsmEventCode > 0 )
								{
									token_t *tok;
									au_tid_t tid = {0,0};
									
									if ( inMsg != NULL )
									{
										if ( siResult == eDSNoErr )
										{
											tok = au_to_text( textStr );
											audit_write_success( bsmEventCode, tok,
																	(*inMsg)->fAuditUID,
																	(*inMsg)->fEffectiveUID,
																	(*inMsg)->fEffectiveGID,
																	(*inMsg)->fUID,
																	(*inMsg)->fGID,
																	aClientPID,
																	(*inMsg)->fAuditSID,
																	&((*inMsg)->fTerminalID) );
										}
										else
										{
											audit_write_failure( bsmEventCode, textStr, (int)siResult,
																	(*inMsg)->fAuditUID,
																	(*inMsg)->fEffectiveUID,
																	(*inMsg)->fEffectiveGID,
																	(*inMsg)->fUID,
																	(*inMsg)->fGID,
																	aClientPID,
																	(*inMsg)->fAuditSID,
																	&((*inMsg)->fTerminalID) );
										}
									}
									else
									{
										if ( siResult == eDSNoErr )
										{
											tok = au_to_text( textStr );
											audit_write_success( bsmEventCode, tok, 0,0,0,0,0, aClientPID, 0, &tid );
										}
										else
										{
											audit_write_failure( bsmEventCode, textStr, (int)siResult, 0,0,0,0,0, aClientPID, 0, &tid );
										}
									}
								}
								DSFreeString( textStr );	// sets to NULL; required
							#endif
						}
						else
						{
							siResult = eDSNoErr;
						}

						//need to return continue data if the buffer is too small to
						//pick up the call from the point it is after the client
						//increases his buffer size
						pluginResult = siResult;
						if ( ( siResult == eDSNoErr ) || (siResult == eDSBufferTooSmall) )
						{
							siResult = PackageReply( pData, inMsg );
						}
						else
						{
							//remove reference from list called below
						}
	
#ifdef BUILD_IN_PERFORMANCE
						if ( performanceStatGatheringActive )
						{
							outTime = dsTimestamp();
							gSrvrCntl->HandlePerformanceStats( GetMsgType( (*inMsg) ), fPluginPtr->GetSignature(), pluginResult, aClientPID, inTime, outTime );
						}
#endif						
						LogAPICall( inTime, debugDataTag, pluginResult);
						DebugAPIPluginResponse( pData, debugDataTag, pluginResult);

					}
					else if ( uiState & kUninitialized )
					{
						DbgLog( kLogHandler, "Plugin state is uninitialized so retrying" );
						// If the plugin is not finished initializing, let's hang out
						//	for a while and see if it finishes sucessefully
						if ( uiState & kUninitialized )
						{
							DSEventSemaphore	initWait;
							time_t              waitForIt = ::time( nil ) + 120;

							while ( uiState & kUninitialized )
							{
								siResult = gPlugins->GetState( fPluginPtr->GetPluginName(), &uiState );
								if ( siResult == eDSNoErr )
								{
									// Wait for .5 seconds
									initWait.WaitForEvent( (UInt32)(.5 * kMilliSecsPerSec) );

									// Let's give it a couple of minutes, then we bail
									if ( ::time( nil ) > waitForIt )
									{
										break;
									}
								}
								else
								{
									break;
								}
							}
						}

						if ( uiState & kActive )
						{
							DbgLog( kLogHandler, "Plugin state became active" );
							if ( gLogAPICalls || performanceStatGatheringActive )
							{
								inTime = dsTimestamp();
							}

							if ( shouldProcess )
							{
								#if USE_BSM_AUDIT
									// need to evaluate the event before calling the dispatch in case the
									// event is a DeleteRecord event. 
									UInt32 bsmEventCode = AuditForThisEvent( type, pData, &textStr );
								#endif
								
								siResult = fPluginPtr->ProcessRequest( pData );
								
								#if USE_BSM_AUDIT
									// BSM Audit
									if ( bsmEventCode > 0 )
									{
										token_t *tok;
										
										if ( inMsg != NULL )
										{
											if ( siResult == eDSNoErr )
											{
												tok = au_to_text( textStr );
												audit_write_success( bsmEventCode, tok,
																		(*inMsg)->fAuditUID,
																		(*inMsg)->fEffectiveUID,
																		(*inMsg)->fEffectiveGID,
																		(*inMsg)->fUID,
																		(*inMsg)->fGID,
																		aClientPID,
																		(*inMsg)->fAuditSID,
																		&((*inMsg)->fTerminalID) );
											}
											else
											{
												audit_write_failure( bsmEventCode, textStr, (int)siResult,
																		(*inMsg)->fAuditUID,
																		(*inMsg)->fEffectiveUID,
																		(*inMsg)->fEffectiveGID,
																		(*inMsg)->fUID,
																		(*inMsg)->fGID,
																		aClientPID,
																		(*inMsg)->fAuditSID,
																		&((*inMsg)->fTerminalID) );
											}
										}
										else
										{
											if ( siResult == eDSNoErr )
											{
												tok = au_to_text( textStr );
												audit_write_success( bsmEventCode, tok, 0,0,0,0,0, aClientPID, 0, &((*inMsg)->fTerminalID) );
											}
											else
											{
												audit_write_failure( bsmEventCode, textStr, (int)siResult, 0,0,0,0,0, aClientPID, 0, &((*inMsg)->fTerminalID) );
											}
										}
									}
									DSFreeString( textStr );	// sets to NULL; required
								#endif
							}
							else
							{
								siResult = eDSNoErr;
							}

							//need to return continue data if the buffer is too small to
							//pick up the call from the point it is after the client
							//increases his buffer size
							pluginResult = siResult;
							if ( ( siResult == eDSNoErr ) || (siResult == eDSBufferTooSmall) )
							{
								siResult = PackageReply( pData, inMsg );
							}
							else
							{
								//remove reference from list called below
							}
#ifdef BUILD_IN_PERFORMANCE
							if ( performanceStatGatheringActive )
							{
								outTime = dsTimestamp();
								gSrvrCntl->HandlePerformanceStats( GetMsgType( (*inMsg) ), fPluginPtr->GetSignature(), pluginResult, aClientPID, inTime, outTime );
							}
#endif	
							LogAPICall( inTime, debugDataTag, pluginResult);
							DebugAPIPluginResponse( pData, debugDataTag, pluginResult);

						}
						else
						{
							DbgLog( kLogHandler, "Plugin state did not become active" );
						}
					}
					else if ( uiState & kInactive )
					{
						DbgLog( kLogHandler, "Plugin state is inactive" );
						siResult = ePlugInNotActive;
					}
					else if ( uiState & kFailedToInit )
					{
						DbgLog( kLogHandler, "Plugin state failed to init" );
						siResult = ePlugInInitError;
					}
					else
					{
						DbgLog( kLogHandler, "Plugin state is indeterminate as %d", uiState );
						siResult = ePlugInInitError;
					}
				}
				else
				{
					DbgLog( kLogHandler, "Unable to retrieve plugin state" );
					siResult = ePlugInInitError;
				}
			}
			else
			{
				sHeader				*p			= (sHeader *)pData;
				sOpenDirNode		*pDataMask	= (sOpenDirNode *)pData;
				CServerPlugin		*pPlugin	= nil;
				tDataNode			*aNodePtr	= nil;
				tDataBufferPriv		*pPrivData	= nil;
				char				*nodePrefix = nil;
				
				// this path is to handle likely opendirnode calls that have not been registered
				DbgLog( kLogDebug, "Unable to determine fPluginPtr from node table" );
				
				if ( (p->fType == kOpenDirNode) && shouldProcess )
				{
					if ( gPlugins != nil )
					{
						//retrieve the node Prefix out of the tDataList pDataMask->fInDirNodeName
						aNodePtr = ::dsGetThisNodePriv( pDataMask->fInDirNodeName->fDataListHead, 1 );
						if (aNodePtr != nil)
						{
							pPrivData = (tDataBufferPriv *)aNodePtr;
							nodePrefix = dsCStrFromCharacters( pPrivData->fBufferData, pPrivData->fBufferLength );
						}
						
						//try to get correct plugin to handle this request					
						pPlugin = gPlugins->GetPlugInPtr(nodePrefix, true);
						if ( pPlugin != nil )
						{
							DbgLog( kLogDebug, "Determined plugin ptr for call" );
							if ( (gDebugLogging) || (gLogAPICalls) )
							{
								debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, pPlugin->GetPluginName());
								DebugAPIPluginCall(pData, debugDataTag);
							}

							if ( gLogAPICalls || performanceStatGatheringActive )
							{
								inTime = dsTimestamp();
							}

							#if USE_BSM_AUDIT
								// need to evaluate the event before calling the dispatch in case the
								// event is a DeleteRecord event. 
								UInt32 bsmEventCode = AuditForThisEvent( type, pData, &textStr );
							#endif
							
							siResult = pPlugin->ProcessRequest( pData );
							DbgLog( kLogDebug, "Determined plugin ptr used and returns result %d", siResult );
							
							#if USE_BSM_AUDIT
								// BSM Audit
								if ( bsmEventCode > 0 )
								{
									token_t *tok;
									au_tid_t tid = {0,0};
									
									if ( inMsg != NULL )
									{
										if ( siResult == eDSNoErr )
										{
											tok = au_to_text( textStr );
											audit_write_success( bsmEventCode, tok,
																	(*inMsg)->fAuditUID,
																	(*inMsg)->fEffectiveUID,
																	(*inMsg)->fEffectiveGID,
																	(*inMsg)->fUID,
																	(*inMsg)->fGID,
																	aClientPID,
																	(*inMsg)->fAuditSID,
																	&((*inMsg)->fTerminalID) );
										}
										else
										{
											audit_write_failure( bsmEventCode, textStr, (int)siResult,
																	(*inMsg)->fAuditUID,
																	(*inMsg)->fEffectiveUID,
																	(*inMsg)->fEffectiveGID,
																	(*inMsg)->fUID,
																	(*inMsg)->fGID,
																	aClientPID,
																	(*inMsg)->fAuditSID,
																	&((*inMsg)->fTerminalID) );
										}
									}
									else
									{
										if ( siResult == eDSNoErr )
										{
											tok = au_to_text( textStr );
											audit_write_success( bsmEventCode, tok, 0,0,0,0,0, aClientPID, 0, &tid );
										}
										else
										{
											audit_write_failure( bsmEventCode, textStr, (int)siResult, 0,0,0,0,0, aClientPID, 0, &tid );
										}
									}
								}
								DSFreeString( textStr );	// sets to NULL; required
							#endif
							
							pluginResult = siResult;
							if ( ( siResult == eDSNoErr ) || (siResult == eDSBufferTooSmall) )
							{
								found = true;
								siResult = PackageReply( pData, inMsg );

								if ( siResult == eDSNoErr )
								{
									siResult = CRefTable::SetNodePluginPtr( pDataMask->fOutNodeRef, pPlugin );
								}
							}
							//always log debug result
							LogAPICall( inTime, debugDataTag, pluginResult);
							DebugAPIPluginResponse( pData, debugDataTag, pluginResult);

#ifdef BUILD_IN_PERFORMANCE
							if ( performanceStatGatheringActive )
							{
								outTime = dsTimestamp();
								gSrvrCntl->HandlePerformanceStats( GetMsgType( (*inMsg) ), pPlugin->GetSignature(), pluginResult, aClientPID, inTime, outTime );
							}
#endif								
						}

						if ( siResult != eDSNoErr )
						{
							siResult = eDSUnknownNodeName;
						}
					}
				}
				else
				{
					siResult = ePlugInNotFound;
				}

				if ( (p->fType == kOpenDirNode) && shouldProcess && !found )
				{
					siResult = eDSNodeNotFound;
					char *nodeName = dsGetPathFromList(	0, pDataMask->fInDirNodeName, "/" );
					DbgLog( kLogDebug, "*** Error NULL plug-in pointer for node %s. Returning error = %l.",
							nodeName ? nodeName : "(null)", siResult );
					DSFreeString( nodeName );
				}
				
				DSFreeString( nodePrefix );
			}
		}
		else
		{
			siResult = eMemoryError;
			DbgLog( kLogHandler, "*** Error NULL plug-in data. Returning error = %l.", siResult );
		}
	}
	else if ( siResult == -1212 )
	{
		DbgLog( kLogHandler, "GetRequestData error returned %d", siResult );
		//KW this -1212 only possible from DoReleaseContinueData()
		siResult = eDSNoErr;
	}
	else
	{
		if ( siResult != eDSRecordTypeDisabled )
		{
			DbgLog( kLogHandler, "GetRequestData error returned %d", siResult );
		}
	}

	if ( siResult == eDSBufferTooSmall )
	{
		siResult = eDSNoErr;
	}

	if ( siResult != eDSNoErr )
	{
		if ( (debugDataTag == nil) && ( gDebugLogging || gLogAPICalls ) ) //early reference error caused failure
		{
			if (fPluginPtr != NULL)
			{
				debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, fPluginPtr->GetPluginName() ? fPluginPtr->GetPluginName() : (char *)"No Plugin");
			}
			else
			{
				debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, (char *)"No Plugin");
			}
			DebugAPIPluginCall(nil, debugDataTag);
		}

		if ( pData != nil )
		{
			sHeader   *p		= (sHeader *)pData;

			char *pType = GetCallName( p->fType );
			if ( pType != nil )
			{
				SInt32 cleanUpResult = eDSNoErr;
				cleanUpResult = FailedCallRefCleanUp( pData, aClientPID, p->fType, anIPAddress );
				DbgLog( kLogHandler, "Plug-in call \"%s\" failed with error = %l.", pType, siResult );
			}
			else
			{
				DbgLog( kLogHandler, "Plug-in call failed with error = %l, type = %d.", siResult, p->fType );
			}
		}
		else
		{
			DbgLog( kLogHandler, "Plug-in call failed with error = %l (NULL data).", siResult );
		}

		CSrvrMessaging		cMsg;
		cMsg.ClearDataBlock( (*inMsg) );
		SetRequestResult( (*inMsg), siResult );
	}

	if ( pData != nil )
	{
		DoFreeMemory( pData );

		free( pData );
		pData = nil;
	}

	if (debugDataTag != nil)
	{
		free(debugDataTag);
		debugDataTag = nil;
	}

	return( siResult );

} // HandlePluginCall


//--------------------------------------------------------------------------------------------------
//	* HandleUnknownCall()
//
//--------------------------------------------------------------------------------------------------

SInt32 CRequestHandler::HandleUnknownCall ( sComData *inMsg )
{
	SInt32			siResult	= eDSNoErr;

	siResult = SetRequestResult( inMsg, eUnknownAPICall );

	return( siResult );

} // HandleUnknownCall


//--------------------------------------------------------------------------------------------------
//	* GetRequestData()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::GetRequestData ( sComData *inMsg, SInt32 *outResult, bool *outShouldProcess )
{
	void			   *outData			= nil;
	UInt32				uiMsgType		= 0;

	if (outShouldProcess != nil)
	{
		*outShouldProcess = true;
	}
	uiMsgType = GetMsgType( inMsg );

	switch ( uiMsgType )
	{
		case kReleaseContinueData:
			outData = DoReleaseContinueData( inMsg, outResult );
			break;

		case kFlushRecord:
			outData = DoFlushRecord( inMsg, outResult );
			break;

		case kDoPlugInCustomCall:
			outData = DoPlugInCustomCall( inMsg, outResult );
			break;

		case kDoAttributeValueSearch:
			outData = DoAttributeValueSearch( inMsg, outResult );
			break;

		case kDoMultipleAttributeValueSearch:
			outData = DoMultipleAttributeValueSearch( inMsg, outResult );
			break;

		case kDoAttributeValueSearchWithData:
			outData = DoAttributeValueSearchWithData( inMsg, outResult );
			break;

		case kDoMultipleAttributeValueSearchWithData:
			outData = DoMultipleAttributeValueSearchWithData( inMsg, outResult );
			break;

		case kOpenDirNode:
			outData = DoOpenDirNode( inMsg, outResult );
			break;

		case kCloseDirNode:
			outData = DoCloseDirNode( inMsg, outResult );
			if (outShouldProcess != nil)
			{
				*outShouldProcess = false; //plug-in gets called through callback
			}
			break;

		case kGetDirNodeInfo:
			outData = DoGetDirNodeInfo( inMsg, outResult );
			break;

		case kGetRecordList:
			outData = DoGetRecordList( inMsg, outResult );
			break;

		case kGetRecordEntry:
			outData = DoGetRecordEntry( inMsg, outResult );
			break;

		case kGetAttributeEntry:
			outData = DoGetAttributeEntry( inMsg, outResult );
			break;

		case kGetAttributeValue:
			outData = DoGetAttributeValue( inMsg, outResult );
			break;

		case kCloseAttributeList:
			outData = DoCloseAttributeList( inMsg, outResult );
			if (outShouldProcess != nil)
			{
				*outShouldProcess = false; //plug-in gets called through callback
			}
			break;

		case kCloseAttributeValueList:
			outData = DoCloseAttributeValueList( inMsg, outResult );
			if (outShouldProcess != nil)
			{
				*outShouldProcess = false; //plug-in gets called through callback
			}
			break;

		case kOpenRecord:
			outData = DoOpenRecord( inMsg, outResult );
			break;

		case kGetRecordReferenceInfo:
			outData = DoGetRecRefInfo( inMsg, outResult );
			break;

		case kGetRecordAttributeInfo:
			outData = DoGetRecAttribInfo( inMsg, outResult );
			break;

		case kGetRecordAttributeValueByID:
			outData = DoGetRecordAttributeValueByID( inMsg, outResult );
			break;

		case kGetRecordAttributeValueByIndex:
			outData = DoGetRecordAttributeValueByIndex( inMsg, outResult );
			break;

		case kGetRecordAttributeValueByValue:
			outData = DoGetRecordAttributeValueByValue( inMsg, outResult );
			break;

		case kCloseRecord:
			outData = DoCloseRecord( inMsg, outResult );
			if (outShouldProcess != nil)
			{
				*outShouldProcess = false; //plug-in gets called through callback
			}
			break;

		case kSetRecordName:
			outData = DoSetRecordName( inMsg, outResult );
			break;

		case kSetRecordType:
			outData = DoSetRecordType( inMsg, outResult );
			break;

		case kDeleteRecord:
			outData = DoDeleteRecord( inMsg, outResult );
			break;

		case kCreateRecord:
		case kCreateRecordAndOpen:
			outData = DoCreateRecord( inMsg, outResult );
			break;

		case kAddAttribute:
			outData = DoAddAttribute( inMsg, outResult );
			break;

		case kRemoveAttribute:
			outData = DoRemoveAttribute( inMsg, outResult );
			break;

		case kAddAttributeValue:
			outData = DoAddAttributeValue( inMsg, outResult );
			break;

		case kRemoveAttributeValue:
			outData = DoRemoveAttributeValue( inMsg, outResult );
			break;

		case kSetAttributeValue:
			outData = DoSetAttributeValue( inMsg, outResult );
			break;

		case kSetAttributeValues:
			outData = DoSetAttributeValues( inMsg, outResult );
			break;

		case kDoDirNodeAuth:
			outData = DoAuthentication( inMsg, outResult );
			break;

		case kDoDirNodeAuthOnRecordType:
			outData = DoAuthenticationOnRecordType( inMsg, outResult );
			break;
	}

	return( outData );

} // GetRequestData


//--------------------------------------------------------------------------------------------------
//	* SetRequestResult()
//
//--------------------------------------------------------------------------------------------------

SInt32 CRequestHandler::SetRequestResult ( sComData *inMsg, SInt32 inResult )
{
	SInt32			siResult	= -8088;
	CSrvrMessaging		cMsg;

	if ( inMsg != nil )
	{
		cMsg.ClearMessageBlock( inMsg ); //KW This might be a redundant call here - PackageReply code path

		// Add the result code
		siResult = cMsg.Add_Value_ToMsg( inMsg, inResult, kResult );
	}

	return( siResult );

} // SetRequestResult


//--------------------------------------------------------------------------------------------------
//	* DebugAPIPluginResponse()
//
//--------------------------------------------------------------------------------------------------

void CRequestHandler::DebugAPIPluginResponse (	void		   *inData,
												char		   *inDebugDataTag,
												SInt32			inResult)
{
	sHeader		   *pMsgHdr		= nil;

	if ( (!gDebugLogging) || (inData == nil) || (inDebugDataTag == nil) )
	{
		return;
	}

	pMsgHdr = (sHeader *)inData;

	switch ( pMsgHdr->fType )
	{
		case kOpenDirNode:
		{
			sOpenDirNode *p = (sOpenDirNode *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Dir Ref = %u : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInDirRef, p->fOutNodeRef, inResult );
			break;
		}

		case kCloseDirNode:
		{
			sCloseDirNode *p = (sCloseDirNode *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
			break;
		}

		case kOpenRecord:
		{
			sOpenRecord *p = (sOpenRecord *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Record Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecRef, inResult );
			break;
		}

		case kCreateRecord:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Record Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecRef, inResult );
			break;
		}

		case kCreateRecordAndOpen:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Record Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecRef, inResult );
			break;
		}

		case kCloseRecord:
		{
			sCloseRecord *p = (sCloseRecord *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}
		
		case kDeleteRecord:
		{
			sDeleteRecord *p = (sDeleteRecord *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}
		
		case kGetRecordReferenceInfo:
		{
			sGetRecRefInfo *p = (sGetRecRefInfo *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordAttributeInfo:
		{
			sGetRecAttribInfo *p = (sGetRecAttribInfo *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordAttributeValueByID:
		{
			sGetRecordAttributeValueByID *p = (sGetRecordAttributeValueByID *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordAttributeValueByIndex:
		{
			sGetRecordAttributeValueByIndex *p = (sGetRecordAttributeValueByIndex *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordAttributeValueByValue:
		{
			sGetRecordAttributeValueByValue *p = (sGetRecordAttributeValueByValue *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetDirNodeInfo:
		{
			sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
			break;
		}

		case kGetRecordEntry:
		{
			sGetRecordEntry *p = (sGetRecordEntry *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : No Std Buffer : Node Ref = %u : Attr List Ref = %u", inDebugDataTag, p->fInNodeRef, p->fOutAttrListRef );
			break;
		}

		case kGetAttributeEntry:
		{
			sGetAttributeEntry *p = (sGetAttributeEntry *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : No Std Buffer : Node Ref = %u : Attr List Ref = %u : Attr Value List Ref = %u", inDebugDataTag, p->fInNodeRef, p->fInAttrListRef, p->fOutAttrValueListRef );
			break;
		}

		case kGetAttributeValue:
		{
			sGetAttributeValue *p = (sGetAttributeValue *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : No Std Buffer : Node Ref = %u : Attr Value Index = %u : Attr Value List Ref = %u", inDebugDataTag, p->fInNodeRef, p->fInAttrValueIndex, p->fInAttrValueListRef );
			break;
		}

		case kSetRecordName:
		{
			sSetRecordName *p = (sSetRecordName *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kAddAttribute:
		{
			sAddAttribute *p = (sAddAttribute *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kRemoveAttribute:
		{
			sRemoveAttribute *p = (sRemoveAttribute *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kAddAttributeValue:
		{
			sAddAttributeValue *p = (sAddAttributeValue *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kRemoveAttributeValue:
		{
			sRemoveAttributeValue *p = (sRemoveAttributeValue *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kSetAttributeValue:
		{
			sSetAttributeValue *p = (sSetAttributeValue *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kSetAttributeValues:
		{
			sSetAttributeValues *p = (sSetAttributeValues *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordList:
		{
			sGetRecordList *p = (sGetRecordList *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u  : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecEntryCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoAttributeValueSearch:
		{
			sDoAttrValueSearch *p = (sDoAttrValueSearch *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u  : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoMultipleAttributeValueSearch:
		{
			sDoMultiAttrValueSearch *p = (sDoMultiAttrValueSearch *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u  : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoAttributeValueSearchWithData:
		{
			sDoAttrValueSearchWithData *p = (sDoAttrValueSearchWithData *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoMultipleAttributeValueSearchWithData:
		{
			sDoMultiAttrValueSearchWithData *p = (sDoMultiAttrValueSearchWithData *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoDirNodeAuth:
		{
			sDoDirNodeAuth *p = (sDoDirNodeAuth *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
			break;
		}
		
		case kDoDirNodeAuthOnRecordType:
		{
			sDoDirNodeAuthOnRecordType *p = (sDoDirNodeAuthOnRecordType *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
			break;
		}
		
		case kDoPlugInCustomCall:
		{
			sDoPlugInCustomCall *p = (sDoPlugInCustomCall *)inData;
			DbgLog( kLogAPICalls, "%s : DAR : Node Ref = %u : Request Code = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fInRequestCode, inResult );
			break;
		}

		default:
			break;
	}

	return;
				
} //DebugAPIPluginResponse

//--------------------------------------------------------------------------------------------------
//	* DebugAPIPluginCall()
//
//--------------------------------------------------------------------------------------------------

void CRequestHandler::DebugAPIPluginCall ( void *inData, char *inDebugDataTag )
{
	sHeader		   *pMsgHdr		= nil;

	if ( gDebugLogging && (inData == nil) && (inDebugDataTag != nil) ) //error before data parsing
	{
		DbgLog( kLogAPICalls, "%s : DAC : Failed during argument validity or record search restriction checking", inDebugDataTag );
		return;
	}
	
	if ( (!gDebugLogging) || (inData == nil) || (inDebugDataTag == nil) )
	{
		return;
	}

	pMsgHdr = (sHeader *)inData;

	switch ( pMsgHdr->fType )
	{
		case kOpenDirNode:
		{
			sOpenDirNode *p = (sOpenDirNode *)inData;
			char* aNodeName = dsGetPathFromListPriv( p->fInDirNodeName, (const char *)"/" );
			if (aNodeName != nil)
			{
				DbgLog( kLogAPICalls, "%s : DAC : Dir Ref = %u : Node Name = %s", inDebugDataTag, p->fInDirRef, aNodeName );
				free(aNodeName);
				aNodeName = nil;
			}
			break;
		}

		case kCloseDirNode:
		{
			sCloseDirNode *p = (sCloseDirNode *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : Node Ref = %u", inDebugDataTag, p->fInNodeRef );
			break;
		}

		case kOpenRecord:
		{
			sOpenRecord *p = (sOpenRecord *)inData;
			if ( (p->fInRecType != nil) && (p->fInRecName != nil) && (p->fInRecType->fBufferData != nil) && (p->fInRecName->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Node Ref = %u : Rec Type = %s : Rec Name = %s", inDebugDataTag, p->fInNodeRef, p->fInRecType->fBufferData,  p->fInRecName->fBufferData );
			}
			break;
		}

		case kCreateRecord:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			if ( (p->fInRecType != nil) && (p->fInRecName != nil) && (p->fInRecType->fBufferData != nil) && (p->fInRecName->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Node Ref = %u : Rec Type = %s : Rec Name = %s", inDebugDataTag, p->fInNodeRef, p->fInRecType->fBufferData,  p->fInRecName->fBufferData );
			}
			break;
		}

		case kCreateRecordAndOpen:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			if ( (p->fInRecType != nil) && (p->fInRecName != nil) && (p->fInRecType->fBufferData != nil) && (p->fInRecName->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Node Ref = %u : Rec Type = %s : Rec Name = %s : Open Rec Flag = %u", inDebugDataTag, p->fInNodeRef, p->fInRecType->fBufferData,  p->fInRecName->fBufferData, p->fInOpen );
			}
			break;
		}

		case kCloseRecord:
		{
			sCloseRecord *p = (sCloseRecord *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u", inDebugDataTag, p->fInRecRef );
			break;
		}
		
		case kDeleteRecord:
		{
			sDeleteRecord *p = (sDeleteRecord *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u", inDebugDataTag, p->fInRecRef );
			break;
		}
		
		case kGetRecordReferenceInfo:
		{
			sGetRecRefInfo *p = (sGetRecRefInfo *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u", inDebugDataTag, p->fInRecRef );
			break;
		}

		case kGetRecordAttributeInfo:
		{
			sGetRecAttribInfo *p = (sGetRecAttribInfo *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData );
			}
			break;
		}

		case kGetRecordAttributeValueByID:
		{
			sGetRecordAttributeValueByID *p = (sGetRecordAttributeValueByID *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr ID = %u", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData, p->fInValueID );
			}
			break;
		}

		case kGetRecordAttributeValueByIndex:
		{
			sGetRecordAttributeValueByIndex *p = (sGetRecordAttributeValueByIndex *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr Index = %u", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData, p->fInAttrValueIndex );
			}
			break;
		}

		case kGetRecordAttributeValueByValue:
		{
			sGetRecordAttributeValueByValue *p = (sGetRecordAttributeValueByValue *)inData;
			if (	(p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) &&
					(p->fInAttrValue != nil)  && (p->fInAttrValue->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr Value = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData, p->fInAttrValue->fBufferData );
			}
			break;
		}

		case kGetDirNodeInfo:
		{
			sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;
			char* requestedAttrs = dsGetPathFromListPriv( p->fInDirNodeInfoTypeList, (const char *)";" );
			if (requestedAttrs != nil)
			{
				DbgLog( kLogAPICalls, "%s : DAC : Node Ref = %u : Requested Attrs = %s : Attr Type Only Flag = %u", inDebugDataTag, p->fInNodeRef, requestedAttrs+1, p->fInAttrInfoOnly );
				free(requestedAttrs);
				requestedAttrs = nil;
			}
			break;
		}
		
		case kGetRecordEntry:
		{
			sGetRecordEntry *p = (sGetRecordEntry *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : No Std Buffer : Node Ref = %u : Rec Entry Index = %u", inDebugDataTag, p->fInNodeRef, p->fInRecEntryIndex );
			break;
		}

		case kGetAttributeEntry:
		{
			sGetAttributeEntry *p = (sGetAttributeEntry *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : No Std Buffer : Node Ref = %u : Attr List Ref = %u : Attr Info Index = %u", inDebugDataTag, p->fInNodeRef, p->fInAttrListRef, p->fInAttrInfoIndex );
			break;
		}

		case kGetAttributeValue:
		{
			sGetAttributeValue *p = (sGetAttributeValue *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : No Std Buffer : Node Ref = %u : Attr Value Index = %u : Attr Value List Ref = %u", inDebugDataTag, p->fInNodeRef, p->fInAttrValueIndex, p->fInAttrValueListRef );
			break;
		}

		case kSetRecordName:
		{
			sSetRecordName *p = (sSetRecordName *)inData;
			if ( (p->fInNewRecName != nil)  && (p->fInNewRecName->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInNewRecName->fBufferData );
			}
			break;
		}

		case kAddAttribute:
		{
			sAddAttribute *p = (sAddAttribute *)inData;
			if ( (p->fInNewAttr != nil) && (p->fInFirstAttrValue != nil) && (p->fInNewAttr->fBufferData != nil) && (p->fInFirstAttrValue->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr Value = %s", inDebugDataTag, p->fInRecRef, p->fInNewAttr->fBufferData,  p->fInFirstAttrValue->fBufferData );
			}
			break;
		}

		case kRemoveAttribute:
		{
			sRemoveAttribute *p = (sRemoveAttribute *)inData;
			if ( (p->fInAttribute != nil)  && (p->fInAttribute->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInAttribute->fBufferData );
			}
			break;
		}

		case kAddAttributeValue:
		{
			sAddAttributeValue *p = (sAddAttributeValue *)inData;
			if ( (p->fInAttrType != nil) && (p->fInAttrValue != nil) && (p->fInAttrType->fBufferData != nil) && (p->fInAttrValue->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr Value = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData,  p->fInAttrValue->fBufferData );
			}
			break;
		}

		case kRemoveAttributeValue:
		{
			sRemoveAttributeValue *p = (sRemoveAttributeValue *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr ID = %u", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData, p->fInAttrValueID );
			}
			break;
		}

		case kSetAttributeValue:
		{
			sSetAttributeValue *p = (sSetAttributeValue *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData );
			}
			break;
		}

		case kSetAttributeValues:
		{
			sSetAttributeValues *p = (sSetAttributeValues *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData );
			}
			break;
		}

		case kGetRecordList:
		{
			sGetRecordList *p = (sGetRecordList *)inData;
			char* requestedRecNames = dsGetPathFromListPriv( p->fInRecNameList, (const char *)";" );
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			char* requestedAttrs = dsGetPathFromListPriv( p->fInAttribTypeList, (const char *)";" );
			if ( (requestedRecNames != nil) && (requestedRecTypes != nil) && (requestedAttrs != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : 1 : Node Ref = %u : Requested Rec Names = %s : Rec Name Pattern Match:%u = %s : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, requestedRecNames+1, p->fInPatternMatch, dsGetPatternMatchName(p->fInPatternMatch), requestedRecTypes+1 );
				DbgLog( kLogAPICalls, "%s : DAC : 2 : Node Ref = %u : Requested Attrs = %s : Attr Type Only Flag = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, requestedAttrs+1, p->fInAttribInfoOnly, p->fOutRecEntryCount, p->fIOContinueData );
				free(requestedRecNames);
				requestedRecNames = nil;
				free(requestedRecTypes);
				requestedRecTypes = nil;
				free(requestedAttrs);
				requestedAttrs = nil;
			}

			break;
		}

		case kDoAttributeValueSearch:
		{
			sDoAttrValueSearch *p = (sDoAttrValueSearch *)inData;
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			if ( 	(p->fInAttrType != nil) && (p->fInAttrType->fBufferData != nil) &&
					(p->fInPatt2Match != nil) && (p->fInPatt2Match->fBufferData != nil) &&
					(requestedRecTypes != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : 1 : Node Ref = %u : Requested Attr Type = %s : Attr Match String = %s : Attr Pattern Match:%u = %s : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, p->fInAttrType->fBufferData, p->fInPatt2Match->fBufferData, p->fInPattMatchType, dsGetPatternMatchName(p->fInPattMatchType), requestedRecTypes+1 );
				DbgLog( kLogAPICalls, "%s : DAC : 2 : Node Ref = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData );
				free(requestedRecTypes);
				requestedRecTypes = nil;
			}

			break;
		}

		case kDoMultipleAttributeValueSearch:
		{
			sDoMultiAttrValueSearch *p  = (sDoMultiAttrValueSearch *)inData;
			char* requestedRecTypes		= dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			char* requestedAttrValues   = dsGetPathFromListPriv( p->fInPatterns2MatchList, (const char *)";" );
			if ( 	(p->fInAttrType != nil) && (p->fInAttrType->fBufferData != nil) &&
					(requestedRecTypes != nil) && (requestedAttrValues != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : 1 : Node Ref = %u : Requested Attr Type = %s : Attr Match Strings = %s : Attr Pattern Match:%u = %s : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, p->fInAttrType->fBufferData, requestedAttrValues+1, p->fInPattMatchType, dsGetPatternMatchName(p->fInPattMatchType), requestedRecTypes+1 );
				DbgLog( kLogAPICalls, "%s : DAC : 2 : Node Ref = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData );
				free(requestedRecTypes);
				requestedRecTypes = nil;
				free(requestedAttrValues);
				requestedAttrValues = nil;
			}

			break;
		}

		case kDoAttributeValueSearchWithData:
		{
			sDoAttrValueSearchWithData *p = (sDoAttrValueSearchWithData *)inData;
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			char* requestedAttrs = dsGetPathFromListPriv( p->fInAttrTypeRequestList, (const char *)";" );
			if ( 	(p->fInAttrType != nil) && (p->fInAttrType->fBufferData != nil) &&
					(p->fInPatt2Match != nil) && (p->fInPatt2Match->fBufferData != nil) &&
					(requestedRecTypes != nil) && (requestedAttrs != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : 1 : Node Ref = %u : Requested Attr Type = %s : Attr Match String = %s : Attr Pattern Match:%u = %s : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, p->fInAttrType->fBufferData, p->fInPatt2Match->fBufferData, p->fInPattMatchType, dsGetPatternMatchName(p->fInPattMatchType), requestedRecTypes+1 );
				DbgLog( kLogAPICalls, "%s : DAC : 2 : Node Ref = %u : Requested Attrs = %s : Attr Type Only Flag = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, requestedAttrs+1, p->fInAttrInfoOnly, p->fOutMatchRecordCount, p->fIOContinueData );
				free(requestedRecTypes);
				requestedRecTypes = nil;
				free(requestedAttrs);
				requestedAttrs = nil;
			}

			break;
		}

		case kDoMultipleAttributeValueSearchWithData:
		{
			sDoMultiAttrValueSearchWithData *p  = (sDoMultiAttrValueSearchWithData *)inData;
			char* requestedRecTypes				= dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			char* requestedAttrs				= dsGetPathFromListPriv( p->fInAttrTypeRequestList, (const char *)";" );
			char* requestedAttrValues			= dsGetPathFromListPriv( p->fInPatterns2MatchList, (const char *)";" );
			if ( 	(p->fInAttrType != nil) && (p->fInAttrType->fBufferData != nil) &&
					(requestedRecTypes != nil) && (requestedAttrs != nil) && (requestedAttrValues != nil) )
			{
				DbgLog( kLogAPICalls, "%s : DAC : 1 : Node Ref = %u : Requested Attr Type = %s : Attr Match Strings = %s : Attr Pattern Match:%u = %s : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, p->fInAttrType->fBufferData, requestedAttrValues+1, p->fInPattMatchType, dsGetPatternMatchName(p->fInPattMatchType), requestedRecTypes+1 );
				DbgLog( kLogAPICalls, "%s : DAC : 2 : Node Ref = %u : Requested Attrs = %s : Attr Type Only Flag = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, requestedAttrs+1, p->fInAttrInfoOnly, p->fOutMatchRecordCount, p->fIOContinueData );
				free(requestedRecTypes);
				requestedRecTypes = nil;
				free(requestedAttrs);
				requestedAttrs = nil;
				free(requestedAttrValues);
				requestedAttrValues = nil;
			}

			break;
		}

		case kDoDirNodeAuth:
		{
			sDoDirNodeAuth *p = (sDoDirNodeAuth *)inData;
			if ( 	(p->fInAuthMethod != nil) && (p->fInAuthMethod->fBufferData != nil) &&
					(p->fInAuthStepData != nil) && (p->fInAuthStepData->fBufferData != nil) )
			{
				//KW can we extract out at least the username for those auth methods that require it?
				//would require check on auth method and parse the buffer
				char *userName = nil;
				tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(p->fInAuthStepData);
				if (dataList != nil)
				{
					userName = dsDataListGetNodeStringPriv(dataList, 1);
					// this allocates a copy of the string
					
					dsDataListDeallocatePriv(dataList);
					free(dataList);
					dataList = NULL;
				}
				if (userName == nil)
				{
					userName = strdup("");
				}
				DbgLog( kLogAPICalls, "%s : DAC : Node Ref = %u : User Name = %s : Auth Method = %s : Auth Only Flag = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, userName, p->fInAuthMethod->fBufferData, p->fInDirNodeAuthOnlyFlag, p->fIOContinueData );
				free(userName);
				userName = nil;
			}
			break;
		}
		
		case kDoDirNodeAuthOnRecordType:
		{
			sDoDirNodeAuthOnRecordType *p = (sDoDirNodeAuthOnRecordType *)inData;
			if ( 	(p->fInAuthMethod != nil) && (p->fInAuthMethod->fBufferData != nil) &&
					(p->fInAuthStepData != nil) && (p->fInAuthStepData->fBufferData != nil) &&
					 (p->fInRecordType != nil) && (p->fInRecordType->fBufferData != nil) )
			{
				//KW can we extract out at least the username for those auth methods that require it?
				//would require check on auth method and parse the buffer
				char *userName = nil;
				tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(p->fInAuthStepData);
				if (dataList != nil)
				{
					userName = dsDataListGetNodeStringPriv(dataList, 1);
					// this allocates a copy of the string
					
					dsDataListDeallocatePriv(dataList);
					free(dataList);
					dataList = NULL;
				}
				if (userName == nil)
				{
					userName = strdup("");
				}
				DbgLog( kLogAPICalls, "%s : DAC : 1 : Node Ref = %u : User Name = %s : Auth Method = %s : Auth Only Flag = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, userName, p->fInAuthMethod->fBufferData, p->fInDirNodeAuthOnlyFlag, p->fIOContinueData );
				DbgLog( kLogAPICalls, "%s : DAC : 2 : Node Ref = %u : Record Type = %s", inDebugDataTag, p->fInNodeRef, p->fInRecordType->fBufferData );
				free(userName);
				userName = nil;
			}
			break;
		}
		
		case kDoPlugInCustomCall:
		{
			sDoPlugInCustomCall *p = (sDoPlugInCustomCall *)inData;
			DbgLog( kLogAPICalls, "%s : DAC : Node Ref = %u : Request Code = %u", inDebugDataTag, p->fInNodeRef, p->fInRequestCode );
			break;
		}

		default:
			break;
	}
		
	return;
	
} // DebugAPIPluginCall


//--------------------------------------------------------------------------------------------------
//	* PackageReply()
//
//--------------------------------------------------------------------------------------------------

SInt32 CRequestHandler::PackageReply ( void *inData, sComData **inMsg )
{
	SInt32			siResult	= -8088;
	UInt32			uiMsgType	= 0;
	sHeader		   *pMsgHdr		= nil;
	CSrvrMessaging		cMsg;
	SInt32			aClientPID	= -1;
	UInt32			anIPAddress	= 0;
	UInt32			aContinue	= 0;
	SInt32			aContextReq	= eDSNoErr;

	try
	{
		if ( (inData == nil) || ((*inMsg) == nil) )
		{
			return( siResult );
		}

		aClientPID	= (*inMsg)->fPID;
		anIPAddress	= (*inMsg)->fIPAddress;
		
		//check to see if the client sent in context data container
		aContextReq = cMsg.Get_Value_FromMsg( (*inMsg), &aContinue, kContextData );

		pMsgHdr = (sHeader *)inData;

		uiMsgType = GetMsgType( (*inMsg) );

		siResult = SetRequestResult( (*inMsg), pMsgHdr->fResult );

		switch ( uiMsgType )
		{
			case kReleaseContinueData:
			{
				// This is a noop
				break;
			}

			case kDoPlugInCustomCall:
			{
				sDoPlugInCustomCall *p = (sDoPlugInCustomCall *)inData;

				if ( p->fOutRequestResponse != nil )
				{
					// Add the data buffer
					siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutRequestResponse, ktDataBuff );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );
				}

				//need these back in to determine possible endian byte swapping
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fInNodeRef, ktNodeRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fInNodeRefMap, ktNodeRefMap );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 3 );
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fInRequestCode, kCustomRequestCode );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 4 );

				break;
			}

			case kDoAttributeValueSearch:
			{
				sDoAttrValueSearch *p = (sDoAttrValueSearch *)inData;

				// Add the record count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutMatchRecordCount, kMatchRecCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				if (gDSFWCSBPDebugLogging)
				{
					//adjust the buffer tag to allow DS FW debugging
					//offset by size and length ie. 8 bytes into the tDataBuffer
					if (memcmp((p->fOutDataBuff)+8, "StdA", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgA", 4 );
					}
					else if (memcmp((p->fOutDataBuff)+8, "StdB", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgB", 4 );
					}
				}
				
				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fIOContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fIOContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}

			case kDoMultipleAttributeValueSearch:
			{
				sDoMultiAttrValueSearch *p = (sDoMultiAttrValueSearch *)inData;

				// Add the record count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutMatchRecordCount, kMatchRecCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				if (gDSFWCSBPDebugLogging)
				{
					//adjust the buffer tag to allow DS FW debugging
					//offset by size and length ie. 8 bytes into the tDataBuffer
					if (memcmp((p->fOutDataBuff)+8, "StdA", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgA", 4 );
					}
					else if (memcmp((p->fOutDataBuff)+8, "StdB", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgB", 4 );
					}
				}
				
				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fIOContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fIOContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}

			case kDoAttributeValueSearchWithData:
			{
				sDoAttrValueSearchWithData *p = (sDoAttrValueSearchWithData *)inData;

				// Add the record count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutMatchRecordCount, kMatchRecCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				if (gDSFWCSBPDebugLogging)
				{
					//adjust the buffer tag to allow DS FW debugging
					//offset by size and length ie. 8 bytes into the tDataBuffer
					if (memcmp((p->fOutDataBuff)+8, "StdA", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgA", 4 );
					}
					else if (memcmp((p->fOutDataBuff)+8, "StdB", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgB", 4 );
					}
				}
				
				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fIOContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fIOContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}

			case kDoMultipleAttributeValueSearchWithData:
			{
				sDoMultiAttrValueSearchWithData *p = (sDoMultiAttrValueSearchWithData *)inData;

				// Add the record count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutMatchRecordCount, kMatchRecCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				if (gDSFWCSBPDebugLogging)
				{
					//adjust the buffer tag to allow DS FW debugging
					//offset by size and length ie. 8 bytes into the tDataBuffer
					if (memcmp((p->fOutDataBuff)+8, "StdA", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgA", 4 );
					}
					else if (memcmp((p->fOutDataBuff)+8, "StdB", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgB", 4 );
					}
				}
				
				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fIOContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fIOContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}

			case kGetDirNodeList:
			{
				sGetDirNodeList *p = (sGetDirNodeList *)inData;

				// Add the node count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutNodeCount, kNodeCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				if (aContextReq == eDSNoErr)
				{
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				}
				siResult = eDSNoErr;

				break;
			}

			case kFindDirNodes:
			{
				sFindDirNodes *p = (sFindDirNodes *)inData;

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the node reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutDirNodeCount, kNodeCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				if (aContextReq == eDSNoErr)
				{
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fOutContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				}
				siResult = eDSNoErr;

				break;
			}

			case kOpenDirNode:
			{
				sOpenDirNode *p = (sOpenDirNode *)inData;

				// Add the node reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutNodeRef, ktNodeRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kGetDirNodeInfo:
			{
				sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;

				if (gDSFWCSBPDebugLogging)
				{
					//adjust the buffer tag to allow DS FW debugging
					//offset by size and length ie. 8 bytes into the tDataBuffer
					if (memcmp((p->fOutDataBuff)+8, "StdA", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgA", 4 );
					}
					else if (memcmp((p->fOutDataBuff)+8, "StdB", 4) == 0)
					{
						memcpy((p->fOutDataBuff)+8, "DbgB", 4 );
					}
				}
				
				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the attribute info count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutAttrInfoCount, kAttrInfoCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				// Add the attribute list reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutAttrListRef, ktAttrListRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fOutContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 3 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fOutContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fOutContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}

			case kGetRecordList:
			{
				sGetRecordList *p = (sGetRecordList *)inData;

				if (gDSFWCSBPDebugLogging)
				{
					//adjust the buffer tag to allow DS FW debugging
					//offset by size and length ie. 8 bytes into the tDataBuffer
					if (strncmp(p->fInDataBuff->fBufferData, "StdA", 4) == 0)
					{
						memcpy(p->fInDataBuff->fBufferData, "DbgA", 4 );
					}
					else if (strncmp(p->fInDataBuff->fBufferData, "StdB", 4) == 0)
					{
						memcpy(p->fInDataBuff->fBufferData, "DbgB", 4 );
					}
				}
				
				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the record entry count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutRecEntryCount, kAttrRecEntryCount );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 2 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fIOContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fIOContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}

			case kGetRecordEntry:
			{
				sGetRecordEntry *p = (sGetRecordEntry *)inData;

				// Add the attribute list reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutAttrListRef, ktAttrListRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the record entry
				siResult = cMsg.Add_tRecordEntry_ToMsg( inMsg, p->fOutRecEntryPtr );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				break;
			}

			case kGetAttributeEntry:
			{
				sGetAttributeEntry *p = (sGetAttributeEntry *)inData;

				// Add the attribute value list reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutAttrValueListRef, ktAttrValueListRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the attribute info
				siResult = cMsg.Add_tAttrEntry_ToMsg( inMsg, p->fOutAttrInfoPtr );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );

				break;
			}

			case kGetAttributeValue:
			{
				sGetAttributeValue *p = (sGetAttributeValue *)inData;

				// Add the attribute value
				siResult = cMsg.Add_tAttrValueEntry_ToMsg( inMsg, p->fOutAttrValue );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kOpenRecord:
			{
				sOpenRecord *p = (sOpenRecord *)inData;

				// Add the recrod reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutRecRef, ktRecRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kDeleteRecord:
			{
				sDeleteRecord *p = (sDeleteRecord *)inData;

				CRefTable::RemoveRecordRef( p->fInRecRef, aClientPID, anIPAddress );
				
				break;
			}
			
			case kGetRecordReferenceInfo:
			{
				sGetRecRefInfo *p = (sGetRecRefInfo *)inData;

				// Add the record info
				siResult = cMsg.Add_tRecordEntry_ToMsg( inMsg, p->fOutRecInfo );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kGetRecordAttributeInfo:
			{
				sGetRecAttribInfo *p = (sGetRecAttribInfo *)inData;

				// Add the attribute info
				siResult = cMsg.Add_tAttrEntry_ToMsg( inMsg, p->fOutAttrInfoPtr );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kGetRecordAttributeValueByID:
			{
				sGetRecordAttributeValueByID *p = (sGetRecordAttributeValueByID *)inData;

				// Add the attribute info
				siResult = cMsg.Add_tAttrValueEntry_ToMsg( inMsg, p->fOutEntryPtr );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kGetRecordAttributeValueByIndex:
			{
				sGetRecordAttributeValueByIndex *p = (sGetRecordAttributeValueByIndex *)inData;

				// Add the attribute info
				siResult = cMsg.Add_tAttrValueEntry_ToMsg( inMsg, p->fOutEntryPtr );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kGetRecordAttributeValueByValue:
			{
				sGetRecordAttributeValueByValue *p = (sGetRecordAttributeValueByValue *)inData;

				// Add the attribute info
				siResult = cMsg.Add_tAttrValueEntry_ToMsg( inMsg, p->fOutEntryPtr );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kCreateRecordAndOpen:
			{
				sCreateRecord *p = (sCreateRecord *)inData;

				// Add the attribute info
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutRecRef, ktRecRef );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );

				break;
			}

			case kDoDirNodeAuth:
			{
				sDoDirNodeAuth *p = (sDoDirNodeAuth *)inData;

				if ( p->fOutAuthStepDataResponse != nil )
				{
					// Add the record entry count
					siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutAuthStepDataResponse, kAuthStepDataResponse );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );
				}

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fIOContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fIOContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}

			case kDoDirNodeAuthOnRecordType:
			{
				sDoDirNodeAuthOnRecordType *p = (sDoDirNodeAuthOnRecordType *)inData;

				if ( p->fOutAuthStepDataResponse != nil )
				{
					// Add the record entry count
					siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutAuthStepDataResponse, kAuthStepDataResponse );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError );
				}

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (UInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (SInt32)eServerSendError - 1 );
				}
				else //clean up continue data if client does not want it
				{
					if ( (fPluginPtr != nil) && (p->fIOContinueData != nil) )
					{
						//build the release continue struct
						sReleaseContinueData	aContinueStruct;
						aContinueStruct.fType			= kReleaseContinueData;
						aContinueStruct.fResult			= eDSNoErr;
						aContinueStruct.fInDirReference	= p->fInNodeRef;
						aContinueStruct.fInContinueData	= p->fIOContinueData;
						
						//call the plugin - don't check the result
						fPluginPtr->ProcessRequest( &aContinueStruct );
					}
					siResult = eDSNoErr;
				}

				break;
			}
			default:
				break;
		}

		//here we update the ValidDataStamp for the plugin
		//and here we empty the entire negative cache if something is added anywhere
		switch ( uiMsgType )
		{
			case kFlushRecord:
			case kSetRecordName:
			case kSetRecordType:
			case kCreateRecord:
			case kCreateRecordAndOpen:
			case kAddAttribute:
			case kAddAttributeValue:
			case kSetAttributeValue:
			case kSetAttributeValues:
				if (gCacheNode != NULL)
					gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_NEGATIVE );
			case kDeleteRecord:
//				if (gCacheNode != NULL)
//					gCacheNode->EmptyCacheEntriesForNode( fPluginPtr->GetPluginName() );
			case kRemoveAttribute:
			case kRemoveAttributeValue:
				if ( ( gPlugins != NULL ) && ( fPluginPtr != NULL ) )
					gPlugins->UpdateValidDataStamp( fPluginPtr->GetPluginName() );
				break;
			default:
				break;
		}
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );
	
} // PackageReply


//--------------------------------------------------------------------------------------------------
//	* DoFreeMemory()
//
//--------------------------------------------------------------------------------------------------

void CRequestHandler::DoFreeMemory ( void *inData )
{
	sHeader	   *hdr		= (sHeader *)inData;

	if ( inData == nil )
	{
		return;
	}

	switch ( hdr->fType )
	{
		case kGetDirNodeList:
		{
			sGetDirNodeList *p = (sGetDirNodeList *)inData;

			if ( p->fOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}
		}
		break;

		case kFindDirNodes:
		{
			sFindDirNodes *p = (sFindDirNodes *)inData;

			if ( p->fOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}

			if ( p->fInNodeNamePattern != nil )
			{
				::dsDataListDeallocatePriv( p->fInNodeNamePattern );
				//need to free the datalist structure itself
				free(p->fInNodeNamePattern);
				p->fInNodeNamePattern = nil;
			}

		}
		break;

		case kOpenDirNode:
		{
			sOpenDirNode *p = (sOpenDirNode *)inData;

			if ( p->fInDirNodeName != nil )
			{
				::dsDataListDeallocatePriv( p->fInDirNodeName );
				//need to free the datalist structure itself
				free(p->fInDirNodeName);
				p->fInDirNodeName = nil;
			}
		}
		break;

		case kGetDirNodeInfo:
		{
			sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;

			if ( p->fInDirNodeInfoTypeList != nil )
			{
				::dsDataListDeallocatePriv( p->fInDirNodeInfoTypeList );
				//need to free the datalist structure itself
				free(p->fInDirNodeInfoTypeList);
				p->fInDirNodeInfoTypeList = nil;
			}

			if ( p->fOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}
		}
		break;

		case kGetRecordList:
		{
			sGetRecordList *p = (sGetRecordList *)inData;

			if ( p->fInDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInDataBuff );
				p->fInDataBuff = nil;
			}

			if ( p->fInRecNameList != nil )
			{
				::dsDataListDeallocatePriv( p->fInRecNameList );
				//need to free the datalist structure itself
				free(p->fInRecNameList);
				p->fInRecNameList = nil;
			}

			if ( p->fInRecTypeList != nil )
			{
				::dsDataListDeallocatePriv( p->fInRecTypeList );
				//need to free the datalist structure itself
				free(p->fInRecTypeList);
				p->fInRecTypeList = nil;
			}

			if ( p->fInAttribTypeList != nil )
			{
				::dsDataListDeallocatePriv( p->fInAttribTypeList );
				//need to free the datalist structure itself
				free(p->fInAttribTypeList);
				p->fInAttribTypeList = nil;
			}
		}
		break;

		case kGetRecordEntry:
		{
			sGetRecordEntry *p = (sGetRecordEntry *)inData;

			if ( p->fInOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInOutDataBuff );
				p->fInOutDataBuff = nil;
			}

			if ( p->fOutRecEntryPtr != nil )
			{
				free( p->fOutRecEntryPtr );		// okay since calloc used on original create
				p->fOutRecEntryPtr = nil;
			}
		}
		break;

		case kGetAttributeEntry:
		{
			sGetAttributeEntry *p = (sGetAttributeEntry *)inData;

			if ( p->fInOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInOutDataBuff );
				p->fInOutDataBuff = nil;
			}

			if ( p->fOutAttrInfoPtr != nil )
			{
				free( p->fOutAttrInfoPtr );		// okay since calloc used on original create
				p->fOutAttrInfoPtr = nil;
			}
		}
		break;

		case kGetAttributeValue:
		{
			sGetAttributeValue *p = (sGetAttributeValue *)inData;

			if ( p->fInOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInOutDataBuff );
				p->fInOutDataBuff = nil;
			}

			if ( p->fOutAttrValue != nil )
			{
				free( p->fOutAttrValue );		// okay since calloc used on original create
				p->fOutAttrValue = nil;
			}
		}
		break;

		case kOpenRecord:
		{
			sOpenRecord *p = (sOpenRecord *)inData;

			if ( p->fInRecType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInRecType );
				p->fInRecType = nil;
			}

			if ( p->fInRecName != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInRecName );
				p->fInRecName = nil;
			}
		}
		break;

		case kGetRecordReferenceInfo:
		{
			sGetRecRefInfo *p = (sGetRecRefInfo *)inData;

			if ( p->fOutRecInfo != nil )
			{
				free( p->fOutRecInfo );		// okay since calloc used on original create
				p->fOutRecInfo = nil;
			}
		}
		break;

		case kGetRecordAttributeInfo:
		{
			sGetRecAttribInfo *p = (sGetRecAttribInfo *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fOutAttrInfoPtr != nil )
			{
				free( p->fOutAttrInfoPtr );		// okay since calloc used on original create
				p->fOutAttrInfoPtr = nil;
			}
		}
		break;

		case kGetRecordAttributeValueByID:
		{
			sGetRecordAttributeValueByID *p = (sGetRecordAttributeValueByID *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fOutEntryPtr != nil )
			{
				free( p->fOutEntryPtr );		// okay since calloc used on original create
				p->fOutEntryPtr = nil;
			}
		}
		break;

		case kGetRecordAttributeValueByIndex:
		{
			sGetRecordAttributeValueByIndex *p = (sGetRecordAttributeValueByIndex *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fOutEntryPtr != nil )
			{
				free( p->fOutEntryPtr );		// okay since calloc used on original create
				p->fOutEntryPtr = nil;
			}
		}
		break;

		case kGetRecordAttributeValueByValue:
		{
			sGetRecordAttributeValueByValue *p = (sGetRecordAttributeValueByValue *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInAttrValue != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrValue );
				p->fInAttrValue = nil;
			}

			if ( p->fOutEntryPtr != nil )
			{
				free( p->fOutEntryPtr );		// okay since calloc used on original create
				p->fOutEntryPtr = nil;
			}
		}
		break;

		case kSetRecordName:
		{
			sSetRecordName *p = (sSetRecordName *)inData;

			if ( p->fInNewRecName != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInNewRecName );
				p->fInNewRecName = nil;
			}
		}
		break;

		case kSetRecordType:
		{
			sSetRecordType *p = (sSetRecordType *)inData;

			if ( p->fInNewRecType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInNewRecType );
				p->fInNewRecType = nil;
			}
		}
		break;

		case kCreateRecord:
		case kCreateRecordAndOpen:
		{
			sCreateRecord *p = (sCreateRecord *)inData;

			if ( p->fInRecType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInRecType );
				p->fInRecType = nil;
			}

			if ( p->fInRecName != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInRecName );
				p->fInRecName = nil;
			}
		}
		break;

		case kAddAttribute:
		{
			sAddAttribute *p = (sAddAttribute *)inData;

			if ( p->fInNewAttr != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInNewAttr );
				p->fInNewAttr = nil;
			}

			if ( p->fInFirstAttrValue != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInFirstAttrValue );
				p->fInFirstAttrValue = nil;
			}
		}
		break;

		case kRemoveAttribute:
		{
			sRemoveAttribute *p = (sRemoveAttribute *)inData;

			if ( p->fInAttribute != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttribute );
				p->fInAttribute = nil;
			}
		}
		break;

		case kAddAttributeValue:
		{
			sAddAttributeValue *p = (sAddAttributeValue *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInAttrValue != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrValue );
				p->fInAttrValue = nil;
			}
		}
		break;

		case kRemoveAttributeValue:
		{
			sRemoveAttributeValue *p = (sRemoveAttributeValue *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}
		}
		break;

		case kSetAttributeValue:
		{
			sSetAttributeValue *p = (sSetAttributeValue *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInAttrValueEntry != nil )
			{
				free( p->fInAttrValueEntry );		//KW not sure if calloc used on original create?
				p->fInAttrValueEntry = nil;
			}
		}
		break;

		case kSetAttributeValues:
		{
			sSetAttributeValues *p = (sSetAttributeValues *)inData;

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInAttrValueList != nil )
			{
				::dsDataListDeallocatePriv( p->fInAttrValueList );
				//need to free the datalist structure itself
				free( p->fInAttrValueList );
				p->fInAttrValueList = nil;
			}
		}
		break;

		case kDoDirNodeAuth:
		{
			sDoDirNodeAuth *p = (sDoDirNodeAuth *)inData;

			if ( p->fInAuthMethod != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAuthMethod );
				p->fInAuthMethod = nil;
			}

			if ( p->fInAuthStepData != nil )
			{
				bzero(p->fInAuthStepData->fBufferData, p->fInAuthStepData->fBufferLength);
				::dsDataBufferDeallocatePriv( p->fInAuthStepData );
				p->fInAuthStepData = nil;
			}

			if ( p->fOutAuthStepDataResponse != nil )
			{
				bzero(p->fOutAuthStepDataResponse->fBufferData, p->fOutAuthStepDataResponse->fBufferLength);
				::dsDataBufferDeallocatePriv( p->fOutAuthStepDataResponse );
				p->fOutAuthStepDataResponse = nil;
			}
		}
		break;

		case kDoDirNodeAuthOnRecordType:
		{
			sDoDirNodeAuthOnRecordType *p = (sDoDirNodeAuthOnRecordType *)inData;

			if ( p->fInAuthMethod != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAuthMethod );
				p->fInAuthMethod = nil;
			}

			if ( p->fInRecordType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInRecordType );
				p->fInRecordType = nil;
			}

			if ( p->fInAuthStepData != nil )
			{
				bzero(p->fInAuthStepData->fBufferData, p->fInAuthStepData->fBufferLength);
				::dsDataBufferDeallocatePriv( p->fInAuthStepData );
				p->fInAuthStepData = nil;
			}

			if ( p->fOutAuthStepDataResponse != nil )
			{
				bzero(p->fOutAuthStepDataResponse->fBufferData, p->fOutAuthStepDataResponse->fBufferLength);
				::dsDataBufferDeallocatePriv( p->fOutAuthStepDataResponse );
				p->fOutAuthStepDataResponse = nil;
			}
		}
		break;

		case kDoAttributeValueSearch:
		{
			sDoAttrValueSearch *p = (sDoAttrValueSearch *)inData;

			if ( p->fOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}

			if ( p->fInRecTypeList != nil )
			{
				::dsDataListDeallocatePriv( p->fInRecTypeList );
				//need to free the datalist structure itself
				free(p->fInRecTypeList);
				p->fInRecTypeList = nil;
			}

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInPatt2Match != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInPatt2Match );
				p->fInPatt2Match = nil;
			}
		}
		break;

		case kDoMultipleAttributeValueSearch:
		{
			sDoMultiAttrValueSearch *p = (sDoMultiAttrValueSearch *)inData;

			if ( p->fOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}

			if ( p->fInRecTypeList != nil )
			{
				::dsDataListDeallocatePriv( p->fInRecTypeList );
				//need to free the datalist structure itself
				free(p->fInRecTypeList);
				p->fInRecTypeList = nil;
			}

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInPatterns2MatchList != nil )
			{
				::dsDataListDeallocatePriv( p->fInPatterns2MatchList );
				//need to free the datalist structure itself
				free(p->fInPatterns2MatchList);
				p->fInPatterns2MatchList = nil;
			}
		}
		break;

		case kDoAttributeValueSearchWithData:
		{
			sDoAttrValueSearchWithData *p = (sDoAttrValueSearchWithData *)inData;

			if ( p->fOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}

			if ( p->fInRecTypeList != nil )
			{
				::dsDataListDeallocatePriv( p->fInRecTypeList );
				//need to free the datalist structure itself
				free(p->fInRecTypeList);
				p->fInRecTypeList = nil;
			}

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInPatt2Match != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInPatt2Match );
				p->fInPatt2Match = nil;
			}

			if ( p->fInAttrTypeRequestList != nil )
			{
				::dsDataListDeallocatePriv( p->fInAttrTypeRequestList );
				//need to free the datalist structure itself
				free(p->fInAttrTypeRequestList);
				p->fInAttrTypeRequestList = nil;
			}
		}
		break;

		case kDoMultipleAttributeValueSearchWithData:
		{
			sDoMultiAttrValueSearchWithData *p = (sDoMultiAttrValueSearchWithData *)inData;

			if ( p->fOutDataBuff != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}

			if ( p->fInRecTypeList != nil )
			{
				::dsDataListDeallocatePriv( p->fInRecTypeList );
				//need to free the datalist structure itself
				free(p->fInRecTypeList);
				p->fInRecTypeList = nil;
			}

			if ( p->fInAttrType != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInAttrType );
				p->fInAttrType = nil;
			}

			if ( p->fInPatterns2MatchList != nil )
			{
				::dsDataListDeallocatePriv( p->fInPatterns2MatchList );
				//need to free the datalist structure itself
				free(p->fInPatterns2MatchList);
				p->fInPatterns2MatchList = nil;
			}

			if ( p->fInAttrTypeRequestList != nil )
			{
				::dsDataListDeallocatePriv( p->fInAttrTypeRequestList );
				//need to free the datalist structure itself
				free(p->fInAttrTypeRequestList);
				p->fInAttrTypeRequestList = nil;
			}
		}
		break;

		case kDoPlugInCustomCall:
		{
			sDoPlugInCustomCall *p = (sDoPlugInCustomCall *)inData;

			if ( p->fInRequestData != nil )
			{
				::dsDataBufferDeallocatePriv( p->fInRequestData );
				p->fInRequestData = nil;
			}

			if ( p->fOutRequestResponse != nil )
			{
				::dsDataBufferDeallocatePriv( p->fOutRequestResponse );
				p->fOutRequestResponse = nil;
			}
		}
		break;
		default:
		break;
	}

} // DoFreeMemory


//--------------------------------------------------------------------------------------------------
//	* GetMsgType()
//
//--------------------------------------------------------------------------------------------------

UInt32 CRequestHandler::GetMsgType ( sComData *inMsg )
{
	return( inMsg->type.msgt_name );
} // GetMsgType


#pragma mark -
#pragma mark Retreive Request Data Routines
#pragma mark -

//--------------------------------------------------------------------------------------------------
//	* DoReleaseContinueData()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoReleaseContinueData ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32					siResult	= eServerReceiveError;
	sReleaseContinueData   *p			= nil;
	CSrvrMessaging			cMsg;
	SInt32					aClientPID	= -1;
	UInt32					anIPAddress	= 0;

	try
	{
		p = (sReleaseContinueData *)::calloc( sizeof( sReleaseContinueData ), sizeof( char ) );
		if ( p != nil )
		{
			p->fType = GetMsgType( inMsg );

			aClientPID	= inMsg->fPID;
			anIPAddress	= inMsg->fIPAddress;

			siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirReference, ktDirRef );
			if ( siResult != eDSNoErr ) throw( siResult );

			// Verify the Directory Reference
			siResult = CRefTable::VerifyNodeRef( p->fInDirReference, &fPluginPtr, aClientPID, anIPAddress );
			if ( siResult == eDSInvalidRefType )
			{
				siResult = CRefTable::VerifyDirRef( p->fInDirReference, &fPluginPtr, aClientPID, anIPAddress );
				if ( siResult != eDSNoErr )
				{
					DbgLog( kLogError, "dsDoReleaseContinueData - PID %d called with <%d> that is not a node or directory reference", 
						    aClientPID, p->fInDirReference );
					throw( siResult );
				}
			}
			else if ( siResult != eDSNoErr )
			{
				DbgLog( kLogError, "dsDoReleaseContinueData - PID %d error %d while checking if reference <%d> is a node", aClientPID, siResult,
					    p->fInDirReference );
				throw( siResult );
			}
			
			if ( fPluginPtr == nil )
			{
				// weird problem if we make it here
				free( p );
				p = nil;
				*outStatus = -1212;
			}
			else
			{
				*outStatus = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fInContinueData, kContextData );
			}
		}
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoReleaseContinueData


//--------------------------------------------------------------------------------------------------
//	* DoFlushRecord()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoFlushRecord ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sFlushRecord	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sFlushRecord *) ::calloc(sizeof(sFlushRecord), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoFlushRecord


//--------------------------------------------------------------------------------------------------
//	* DoPlugInCustomCall()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoPlugInCustomCall ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32					siResult	= -8088;
	sDoPlugInCustomCall	   *p			= nil;
	UInt32					uiBuffSize	= 0;
	CSrvrMessaging			cMsg;
	SInt32					aClientPID	= -1;
	UInt32					anIPAddress	= 0;

	try
	{
		p = (sDoPlugInCustomCall *) ::calloc(sizeof(sDoPlugInCustomCall), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRequestCode, kCustomRequestCode );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRequestData, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		p->fOutRequestResponse = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutRequestResponse == nil ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRefMap, ktNodeRefMap );
		//no error check here since only required for endian issues
		//if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoPlugInCustomCall


//--------------------------------------------------------------------------------------------------
//	* DoAttributeValueSearch()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAttributeValueSearch ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult		= -8088;
	sDoAttrValueSearch *p				= nil;
	UInt32				uiBuffSize		= 0;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID		= -1;
	UInt32				anIPAddress		= 0;

	try
	{
		p = (sDoAttrValueSearch *) ::calloc(sizeof(sDoAttrValueSearch), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (SInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrType );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fInPattMatchType, kAttrPattMatch );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInPatt2Match, kAttrMatch );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 6 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fOutMatchRecordCount, kMatchRecCount );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 7 );

		//Record Type Restrictions Checking
		//might be performance impact to do this here wrt nodenames?
		if (fPluginPtr != NULL)
		{
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			if ( gPlugins->IsOKToServiceQuery( fPluginPtr->GetPluginName(), CRefTable::GetNodeRefName(p->fInNodeRef), requestedRecTypes, p->fInRecTypeList->fDataNodeCount ) )
			{
				*outStatus = eDSNoErr;
			}
			else
			{
				*outStatus = eDSRecordTypeDisabled;
			}
			DSFreeString(requestedRecTypes);
		}
		else
		{
			*outStatus = eDSNoErr;
		}
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoAttributeValueSearch


//--------------------------------------------------------------------------------------------------
//	* DoMultipleAttributeValueSearch()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoMultipleAttributeValueSearch ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32						siResult		= -8088;
	sDoMultiAttrValueSearch    *p				= nil;
	UInt32						uiBuffSize		= 0;
	CSrvrMessaging				cMsg;
	SInt32						aClientPID		= -1;
	UInt32						anIPAddress		= 0;

	try
	{
		p = (sDoMultiAttrValueSearch *) ::calloc(sizeof(sDoMultiAttrValueSearch), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (SInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrType );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fInPattMatchType, kAttrPattMatch );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInPatterns2MatchList, kAttrMatches );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 6 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fOutMatchRecordCount, kMatchRecCount );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 7 );

		//Record Type Restrictions Checking
		//might be performance impact to do this here wrt nodenames
		if (fPluginPtr != NULL)
		{
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			if ( gPlugins->IsOKToServiceQuery( fPluginPtr->GetPluginName(), CRefTable::GetNodeRefName(p->fInNodeRef), requestedRecTypes, p->fInRecTypeList->fDataNodeCount ) )
			{
				*outStatus = eDSNoErr;
			}
			else
			{
				*outStatus = eDSRecordTypeDisabled;
			}
			DSFreeString(requestedRecTypes);
		}
		else
		{
			*outStatus = eDSNoErr;
		}
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoMultipleAttributeValueSearch


//--------------------------------------------------------------------------------------------------
//	* DoAttributeValueSearchWithData()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAttributeValueSearchWithData ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32						siResult		= -8088;
	sDoAttrValueSearchWithData *p				= nil;
	UInt32						uiBuffSize		= 0;
	CSrvrMessaging				cMsg;
	SInt32						aClientPID		= -1;
	UInt32						anIPAddress		= 0;
	UInt32						aBoolValue		= 0;

	try
	{
		p = (sDoAttrValueSearchWithData *) ::calloc(sizeof(sDoAttrValueSearchWithData), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (SInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrType );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fInPattMatchType, kAttrPattMatch );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInPatt2Match, kAttrMatch );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 6 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInAttrTypeRequestList, kAttrTypeRequestList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 7 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAttrInfoOnly );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 8 );
		p->fInAttrInfoOnly = (bool)aBoolValue;

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fOutMatchRecordCount, kMatchRecCount );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 9 );

		//Record Type Restrictions Checking
		//might be performance impact to do this here wrt nodenames
		if (fPluginPtr != NULL)
		{
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			if ( gPlugins->IsOKToServiceQuery( fPluginPtr->GetPluginName(), CRefTable::GetNodeRefName(p->fInNodeRef), requestedRecTypes, p->fInRecTypeList->fDataNodeCount ) )
			{
				*outStatus = eDSNoErr;
			}
			else
			{
				*outStatus = eDSRecordTypeDisabled;
			}
			DSFreeString(requestedRecTypes);
		}
		else
		{
			*outStatus = eDSNoErr;
		}
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			if ( p->fOutDataBuff != nil )
			{
				delete( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}

			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoAttributeValueSearchWithData


//--------------------------------------------------------------------------------------------------
//	* DoMultipleAttributeValueSearchWithData()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoMultipleAttributeValueSearchWithData ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32									siResult		= -8088;
	sDoMultiAttrValueSearchWithData		   *p				= nil;
	UInt32									uiBuffSize		= 0;
	CSrvrMessaging							cMsg;
	SInt32									aClientPID		= -1;
	UInt32									anIPAddress		= 0;
	UInt32									aBoolValue		= 0;

	try
	{
		p = (sDoMultiAttrValueSearchWithData *) ::calloc(sizeof(sDoMultiAttrValueSearchWithData), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (SInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrType );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fInPattMatchType, kAttrPattMatch );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInPatterns2MatchList, kAttrMatches );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 6 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInAttrTypeRequestList, kAttrTypeRequestList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 7 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAttrInfoOnly );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 8 );
		p->fInAttrInfoOnly = (bool)aBoolValue;

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fOutMatchRecordCount, kMatchRecCount );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 9 );

		//Record Type Restrictions Checking
		//might be performance impact to do this here wrt nodenames
		if (fPluginPtr != NULL)
		{
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			if ( gPlugins->IsOKToServiceQuery( fPluginPtr->GetPluginName(), CRefTable::GetNodeRefName(p->fInNodeRef), requestedRecTypes, p->fInRecTypeList->fDataNodeCount ) )
			{
				*outStatus = eDSNoErr;
			}
			else
			{
				*outStatus = eDSRecordTypeDisabled;
			}
			DSFreeString(requestedRecTypes);
		}
		else
		{
			*outStatus = eDSNoErr;
		}
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			if ( p->fOutDataBuff != nil )
			{
				delete( p->fOutDataBuff );
				p->fOutDataBuff = nil;
			}

			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoMultipleAttributeValueSearchWithData


//--------------------------------------------------------------------------------------------------
//	* DoOpenDirNode()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoOpenDirNode ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32			siResult		= eDSNoErr;
	sOpenDirNode   *p				= nil;
	CSrvrMessaging	cMsg;

	try
	{
		p = (sOpenDirNode *) ::calloc(sizeof(sOpenDirNode), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirRef, ktDirRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the directory reference
		siResult = CRefTable::VerifyDirRef( p->fInDirRef, nil, inMsg->fPID, inMsg->fIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInDirNodeName, kDirNodeName );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );
        
		p->fInUID = inMsg->fUID;
		p->fInEffectiveUID = inMsg->fEffectiveUID;
		
		DbgLog( kLogApplication, "Client: Requesting dsOpenDirNode with PID = %d, UID = %d, and EUID = %d", inMsg->fPID, inMsg->fUID, inMsg->fEffectiveUID );

		if ( gNodeList != nil )
		{
			char *pNodeName = nil;

			pNodeName = ::dsGetPathFromListPriv( p->fInDirNodeName, gNodeList->GetDelimiter() );
			if ( pNodeName != nil )
			{
				//wait on ALL calls if local node is not yet available
				if ( ( strcmp(pNodeName, "/Local/Default") == 0 ) || ( strcmp(pNodeName, "/Local/Target") == 0 ) )
				{
					gNodeList->WaitForLocalNode();
				}
				
				//wait on ALL calls if configure node is not yet available
				if ( strcmp(pNodeName, "/Configure") == 0 )
				{
					gNodeList->WaitForConfigureNode();
				}
				
				//wait on ALL calls if DHCPLDAPv3 node is not yet ascertained
				if ( strcmp(pNodeName, "/LDAPv3") == 0 )
				{
					gNodeList->WaitForDHCPLDAPv3Init();
				}
				
				//this call means that plugins CANNOT register nodes for other plugins unless
				//they know the other plugin's fPluginSignature
				//ie. the RegisterNode call will associate the node name registered with the plugin token
				//and then use the plugin token to add the pluginPtr
				if ( gNodeList->GetPluginHandle( pNodeName, &fPluginPtr ) == false )
				{
					// Node is not registered
					fPluginPtr = nil;
				}
				free( pNodeName );
				pNodeName = nil;
			}
		}

		char *aNodeName = nil;
		aNodeName = ::dsGetPathFromListPriv( p->fInDirNodeName, "/" );
		siResult = CRefTable::NewNodeRef( &p->fOutNodeRef, fPluginPtr, p->fInDirRef, inMsg->fPID, inMsg->fIPAddress, aNodeName );
		DSFreeString(aNodeName);
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoOpenDirNode


//--------------------------------------------------------------------------------------------------
//	* DoCloseDirNode()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoCloseDirNode ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32			siResult		= -8088;
	sCloseDirNode  *p				= nil;
	CSrvrMessaging	cMsg;
	SInt32			aClientPID		= -1;
	UInt32			anIPAddress		= 0;
	
	try
	{
		p = (sCloseDirNode *) ::calloc(sizeof(sCloseDirNode), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		//KW need to clean up Ref here in server before calling to the plugin so that the plugin also cleans up the Ref
		// Remove the Node Reference
		siResult = CRefTable::RemoveNodeRef( p->fInNodeRef, aClientPID, anIPAddress );
		//if ( siResult != eDSNoErr ) throw( siResult );
		
		p->fResult = siResult; //make sure the return code from the callback makes it to the client

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoCloseDirNode


//--------------------------------------------------------------------------------------------------
//	* DoGetDirNodeInfo()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetDirNodeInfo ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult		= -8088;
	sGetDirNodeInfo	   *p				= nil;
	UInt32				uiBuffSize		= 0;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID		= -1;
	UInt32				anIPAddress		= 0;
	UInt32				aBoolValue		= 0;

	try
	{
		p = (sGetDirNodeInfo *) ::calloc(sizeof(sGetDirNodeInfo), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInDirNodeInfoTypeList, kNodeInfoTypeList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAttrInfoOnly );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );
		p->fInAttrInfoOnly = (bool)aBoolValue;

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fOutContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fOutContinueData = nil;
			siResult = eDSNoErr;
		}

		// Create a new attribute list reference
		siResult = CRefTable::NewAttrListRef( &p->fOutAttrListRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetDirNodeInfo


//--------------------------------------------------------------------------------------------------
//	* DoGetRecordList()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetRecordList ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	UInt32				uiBuffSize	= 0;
	sGetRecordList	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;
	UInt32				aBoolValue	= 0;

	try
	{
		p = (sGetRecordList *)::calloc( sizeof( sGetRecordList ), sizeof( char ) );
		if ( p == nil ) throw( (SInt32)eMemoryError );

		if ( p != nil )
		{
			p->fType = GetMsgType( inMsg );

			aClientPID	= inMsg->fPID;
			anIPAddress	= inMsg->fIPAddress;
			
			// Get the node ref
			siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
			if ( siResult != eDSNoErr ) throw( siResult );

			// Verify the node reference
			siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
			if ( siResult != eDSNoErr ) throw( siResult );

			//is this an update corresponding to server version 1?
			siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
			//whole empty data buffer is no longer being sent
			if ( siResult == eDSNoErr )
			{
				p->fInDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
				if ( p->fInDataBuff != nil )
				{
					siResult = eDSNoErr;
				}
				else
					throw( eMemoryAllocError );
			}
			else
			{
				//old client where empty data buffer is being sent
				siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );
			}

			siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecNameList, kRecNameList );
			if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

			siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fInPatternMatch, ktDirPattMatch );
			if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

			siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
			if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

			siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInAttribTypeList, kAttrTypeList );
			if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 5 );

			siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAttrInfoOnly );
			p->fInAttribInfoOnly = (bool)aBoolValue;
			if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 6 );

			siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fOutRecEntryCount, kAttrRecEntryCount );
			if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 7 );

			siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
			if ( siResult != eDSNoErr )
			{
				p->fIOContinueData = nil;
				siResult = eDSNoErr;
			}
		}

		//Record Type Restrictions Checking
		//might be performance impact to do this here wrt nodenames
		if (fPluginPtr != NULL)
		{
			char* requestedRecTypes = dsGetPathFromListPriv( p->fInRecTypeList, (const char *)";" );
			if ( gPlugins->IsOKToServiceQuery( fPluginPtr->GetPluginName(), CRefTable::GetNodeRefName(p->fInNodeRef), requestedRecTypes, p->fInRecTypeList->fDataNodeCount ) )
			{
				*outStatus = eDSNoErr;
			}
			else
			{
				*outStatus = eDSRecordTypeDisabled;
			}
			DSFreeString(requestedRecTypes);
		}
		else
		{
			*outStatus = eDSNoErr;
		}
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetRecordList


//--------------------------------------------------------------------------------------------------
//	* DoGetRecordEntry()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetRecordEntry ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult		= -8088;
	sGetRecordEntry	   *p				= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID		= -1;
	UInt32				anIPAddress		= 0;

	try
	{
		p = (sGetRecordEntry *) ::calloc(sizeof(sGetRecordEntry), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Make sure that this is a valid node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInOutDataBuff, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecEntryIndex, kRecEntryIndex );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		// Create a new attribute list reference
		siResult = CRefTable::NewAttrListRef( &p->fOutAttrListRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetRecordEntry


//--------------------------------------------------------------------------------------------------
//	* DoGetAttribEntry()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetAttributeEntry ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult		= -8088;
	sGetAttributeEntry *p				= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID		= -1;
	UInt32				anIPAddress		= 0;

	try
	{
		p = (sGetAttributeEntry *) ::calloc(sizeof(sGetAttributeEntry), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInOutDataBuff, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrListRef, ktAttrListRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		// Verify the Attribute List Reference
		siResult = CRefTable::VerifyAttrListRef( p->fInAttrListRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrInfoIndex, kAttrInfoIndex );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		// Create a new Attribute Value List Reference
		siResult = CRefTable::NewAttrValueRef( &p->fOutAttrValueListRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetAttribEntry


//--------------------------------------------------------------------------------------------------
//	* DoGetAttributeValue()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetAttributeValue ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sGetAttributeValue *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sGetAttributeValue *) ::calloc(sizeof(sGetAttributeValue), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInOutDataBuff, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueIndex, kAttrValueIndex );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueListRef, ktAttrValueListRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		// Verify the Attribute Value List Reference
		siResult = CRefTable::VerifyAttrValueRef( p->fInAttrValueListRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetAttributeValue


//--------------------------------------------------------------------------------------------------
//	* DoCloseAttributeList ()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler:: DoCloseAttributeList ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32					siResult		= -8088;
	sCloseAttributeList	   *p				= nil;
	CSrvrMessaging			cMsg;
	SInt32					aClientPID		= -1;
	UInt32					anIPAddress		= 0;

	try
	{
		p = (sCloseAttributeList *) ::calloc(sizeof(sCloseAttributeList), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttributeListRef, ktAttrListRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Attr List Reference
		siResult = CRefTable::VerifyAttrListRef( p->fInAttributeListRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		//KW need to clean up Ref here in server before calling to the plugin so that the plugin also cleans up the Ref
		// Remove the Attr List Reference
		siResult = CRefTable::RemoveAttrListRef( p->fInAttributeListRef, aClientPID, anIPAddress );
		//if ( siResult != eDSNoErr ) throw( siResult );
		
		p->fResult = siResult; //make sure the return code from the callback makes it to the client

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoCloseAttributeList


//--------------------------------------------------------------------------------------------------
//	* DoCloseAttributeValueList ()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler:: DoCloseAttributeValueList ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32						siResult		= -8088;
	sCloseAttributeValueList   *p				= nil;
	CSrvrMessaging				cMsg;
	SInt32						aClientPID		= -1;
	UInt32						anIPAddress		= 0;

	try
	{
		p = (sCloseAttributeValueList *) ::calloc(sizeof(sCloseAttributeValueList), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttributeValueListRef, ktAttrValueListRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Attr Value List Reference
		siResult = CRefTable::VerifyAttrValueRef( p->fInAttributeValueListRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		//KW need to clean up Ref here in server before calling to the plugin so that the plugin also cleans up the Ref
		// Remove the Attr Value List Reference
		siResult = CRefTable::RemoveAttrValueRef( p->fInAttributeValueListRef, aClientPID, anIPAddress );
		//if ( siResult != eDSNoErr ) throw( siResult );
		
		p->fResult = siResult; //make sure the return code from the callback makes it to the client

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoCloseAttributeValueList


//--------------------------------------------------------------------------------------------------
//	* DoOpenRecord()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoOpenRecord ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult		= -8088;
	sOpenRecord		   *p				= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID		= -1;
	UInt32				anIPAddress		= 0;

	try
	{
		p = (sOpenRecord *) ::calloc(sizeof(sOpenRecord), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecName, kRecNameBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		// Create a new Attribute List Reference
		siResult = CRefTable::NewRecordRef( &p->fOutRecRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoOpenRecord


//--------------------------------------------------------------------------------------------------
//	* DoGetRecRefInfo()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetRecRefInfo ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sGetRecRefInfo	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sGetRecRefInfo *) ::calloc(sizeof(sGetRecRefInfo), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;
		
		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetRecRefInfo


//--------------------------------------------------------------------------------------------------
//	* DoGetRecAttribInfo()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetRecAttribInfo ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sGetRecAttribInfo  *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sGetRecAttribInfo *) ::calloc(sizeof(sGetRecAttribInfo), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetRecAttribInfo


//--------------------------------------------------------------------------------------------------
//	* DoGetRecordAttributeValueByID()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetRecordAttributeValueByID ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32							siResult	= -8088;
	sGetRecordAttributeValueByID   *p			= nil;
	CSrvrMessaging					cMsg;
	SInt32							aClientPID	= -1;
	UInt32							anIPAddress	= 0;

	try
	{
		p = (sGetRecordAttributeValueByID *) ::calloc(sizeof(sGetRecordAttributeValueByID), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInValueID, kAttrValueID );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetRecordAttributeValueByID


//--------------------------------------------------------------------------------------------------
//	* DoGetRecordAttributeValueByIndex()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetRecordAttributeValueByIndex ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32								siResult	= -8088;
	sGetRecordAttributeValueByIndex	   *p			= nil;
	CSrvrMessaging						cMsg;
	SInt32								aClientPID	= -1;
	UInt32								anIPAddress	= 0;

	try
	{
		p = (sGetRecordAttributeValueByIndex *) ::calloc(sizeof(sGetRecordAttributeValueByIndex), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueIndex, kAttrValueIndex );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetRecordAttributeValueByIndex


//--------------------------------------------------------------------------------------------------
//	* DoGetRecordAttributeValueByValue()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoGetRecordAttributeValueByValue ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32								siResult	= -8088;
	sGetRecordAttributeValueByValue	   *p			= nil;
	CSrvrMessaging						cMsg;
	SInt32								aClientPID	= -1;
	UInt32								anIPAddress	= 0;

	try
	{
		p = (sGetRecordAttributeValueByValue *) ::calloc(sizeof(sGetRecordAttributeValueByValue), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrValue, kAttrValueBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoGetRecordAttributeValueByValue


//--------------------------------------------------------------------------------------------------
//	* DoCloseRecord()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoCloseRecord ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult		= -8088;
	sCloseRecord	   *p				= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID		= -1;
	UInt32				anIPAddress		= 0;

	try
	{
		p = (sCloseRecord *) ::calloc(sizeof(sCloseRecord), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		//KW need to clean up Ref here in server before calling to the plugin so that the plugin also cleans up the Ref
		// Remove the Record Reference
		siResult = CRefTable::RemoveRecordRef( p->fInRecRef, aClientPID, anIPAddress );
		//if ( siResult != eDSNoErr ) throw( siResult );

		p->fResult = siResult; //make sure the return code from the callback makes it to the client

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoCloseRecord


//--------------------------------------------------------------------------------------------------
//	* DoSetRecordName()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoSetRecordName ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sSetRecordName	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sSetRecordName *) ::calloc(sizeof(sSetRecordName), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInNewRecName, kRecNameBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoSetRecordName


//--------------------------------------------------------------------------------------------------
//	* DoSetRecordType()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoSetRecordType ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sSetRecordType	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sSetRecordType *) ::calloc(sizeof(sSetRecordType), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInNewRecType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoSetRecordType


//--------------------------------------------------------------------------------------------------
//	* DoDeleteRecord()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoDeleteRecord ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult		= -8088;
	sDeleteRecord	   *p				= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID		= -1;
	UInt32				anIPAddress		= 0;

	try
	{
		p = (sDeleteRecord *) ::calloc(sizeof(sDeleteRecord), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		//KW we need to remove the reference from the table AFTER the plugin processes the delete

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoDeleteRecord


//--------------------------------------------------------------------------------------------------
//	* DoCreateRecord()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoCreateRecord ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sCreateRecord	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;
	UInt32				aBoolValue	= 0;

	try
	{
		p = (sCreateRecord *) ::calloc(sizeof(sCreateRecord), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecName, kRecNameBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kOpenRecBool );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		p->fInOpen = (bool)aBoolValue;
		if ( p->fInOpen == true )
		{
			siResult = CRefTable::NewRecordRef( &p->fOutRecRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
			if ( siResult != eDSNoErr ) throw( siResult );
		}

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			delete( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoCreateRecord


//--------------------------------------------------------------------------------------------------
//	* DoAddAttribute()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAddAttribute ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sAddAttribute	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sAddAttribute *) ::calloc(sizeof(sAddAttribute), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInNewAttr, kNewAttrBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInFirstAttrValue, kFirstAttrBuff );
		//allow no intial value to be added
		//if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoAddAttribute


//--------------------------------------------------------------------------------------------------
//	* DoRemoveAttribute()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoRemoveAttribute ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sRemoveAttribute   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sRemoveAttribute *) ::calloc(sizeof(sRemoveAttribute), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttribute, kAttrBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoRemoveAttribute


//--------------------------------------------------------------------------------------------------
//	* DoAddAttributeValue()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAddAttributeValue ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sAddAttributeValue *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sAddAttributeValue *) ::calloc(sizeof(sAddAttributeValue), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrValue, kAttrValueBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoAddAttributeValue


//--------------------------------------------------------------------------------------------------
//	* DoRemoveAttributeValue()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoRemoveAttributeValue ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32					siResult	= -8088;
	sRemoveAttributeValue   *p			= nil;
	CSrvrMessaging			cMsg;
	SInt32					aClientPID	= -1;
	UInt32					anIPAddress	= 0;

	try
	{
		p = (sRemoveAttributeValue *) ::calloc(sizeof(sRemoveAttributeValue), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueID, kAttrValueID );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoRemoveAttributeValue


//--------------------------------------------------------------------------------------------------
//	* DoSetAttributeValue()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoSetAttributeValue ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sSetAttributeValue *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sSetAttributeValue *) ::calloc(sizeof(sSetAttributeValue), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tAttrValueEntry_FromMsg( inMsg, &p->fInAttrValueEntry, ktAttrValueEntry );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoSetAttributeValue


//--------------------------------------------------------------------------------------------------
//	* DoSetAttributeValues()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoSetAttributeValues ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sSetAttributeValues *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sSetAttributeValues *) ::calloc(sizeof(sSetAttributeValues), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInAttrValueList, kAttrValueList );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoSetAttributeValues


//--------------------------------------------------------------------------------------------------
//	* DoAuthentication()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAuthentication ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sDoDirNodeAuth	   *p			= nil;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;
	UInt32				aBoolValue	= 0;

	try
	{
		p = (sDoDirNodeAuth *) ::calloc(sizeof(sDoDirNodeAuth), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthMethod, kAuthMethod );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAuthOnlyBool );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );
		p->fInDirNodeAuthOnlyFlag = (bool)aBoolValue;

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthStepData, kAuthStepBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fOutAuthStepDataResponse, kAuthResponseBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoAuthentication


//--------------------------------------------------------------------------------------------------
//	* DoAuthenticationOnRecordType()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAuthenticationOnRecordType ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32							siResult	= -8088;
	sDoDirNodeAuthOnRecordType	   *p			= nil;
	CSrvrMessaging					cMsg;
	SInt32							aClientPID	= -1;
	UInt32							anIPAddress	= 0;
	UInt32							aBoolValue	= 0;

	try
	{
		p = (sDoDirNodeAuthOnRecordType *) ::calloc(sizeof(sDoDirNodeAuthOnRecordType), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthMethod, kAuthMethod );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAuthOnlyBool );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );
		p->fInDirNodeAuthOnlyFlag = (bool)aBoolValue;

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthStepData, kAuthStepBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fOutAuthStepDataResponse, kAuthResponseBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecordType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		*outStatus = eDSNoErr;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // DoAuthenticationOnRecordType


//------------------------------------------------------------------------------------
//	* FindDirNodes
//------------------------------------------------------------------------------------
  
void* CRequestHandler::FindDirNodes ( sComData *inMsg, SInt32 *outStatus, char *inDebugDataTag )
{
	SInt32					siResult	= -8088;
	sFindDirNodes		   *p			= nil;
	UInt32					uiBuffSize	= 0;
	CSrvrMessaging			cMsg;
	char				   *nodeName	= nil;
	SInt32					aClientPID	= -1;
	UInt32					anIPAddress	= 0;

	try
	{
		p = (sFindDirNodes *)::calloc(sizeof(sFindDirNodes), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirRef, ktDirRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Directory Reference
		siResult = CRefTable::VerifyDirRef( p->fInDirRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (SInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fInPatternMatchType, ktDirPattMatch );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fOutContinueData, kContextData );
		if (siResult != eDSNoErr)
		{
			p->fOutContinueData = nil;
			siResult = eDSNoErr;
		}
		
		//currently not using the continue data

		if ( (p->fInPatternMatchType != eDSLocalNodeNames) &&
			 (p->fInPatternMatchType != eDSAuthenticationSearchNodeName) &&
			 (p->fInPatternMatchType != eDSContactsSearchNodeName) &&
			 (p->fInPatternMatchType != eDSNetworkSearchNodeName) &&
			 (p->fInPatternMatchType != eDSLocalHostedNodes) &&
			 (p->fInPatternMatchType != eDSDefaultNetworkNodes) &&
			 (p->fInPatternMatchType != eDSConfigNodeName) )
		{
			siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInNodeNamePattern, kNodeNamePatt );
			if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 2 );

			nodeName = ::dsGetPathFromListPriv( p->fInNodeNamePattern, gNodeList->GetDelimiter() );
			if ( (nodeName != nil) && (::strlen( nodeName ) > 1) )
			{
	
				DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, nodeName );
				siResult = gNodeList->GetNodes( nodeName, p->fInPatternMatchType, p->fOutDataBuff );
				if (siResult == eDSNoErr)
				{
					::memcpy( &p->fOutDirNodeCount, p->fOutDataBuff->fBufferData + 4, 4 );
					if ( p->fOutDirNodeCount == 0 )
					{
						siResult = eDSNodeNotFound;
					}
				}
				free( nodeName );
				nodeName = nil;
			}
			else if ( nodeName != nil )
			{
				free( nodeName );
				nodeName = nil;
			}
		}
		else
		{
			if (gDebugLogging)
			{
				switch (p->fInPatternMatchType)
				{
					case eDSLocalNodeNames:
						DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrDefaultLocalNodeName );
						break;
					
					case eDSAuthenticationSearchNodeName:
						DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrAuthenticationNodeName );
						break;
					
					case eDSContactsSearchNodeName:
						DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrContactsNodeName );
						break;
					
					case eDSNetworkSearchNodeName:
						DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrNetworkNodeName );
						break;
					
					case eDSLocalHostedNodes:
						DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, "LocalHostedNodes" );
						break;
					
					case eDSDefaultNetworkNodes:
						DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, "DefaultNetworkNodes" );
						break;
					
					case eDSConfigNodeName:
						DbgLog( kLogAPICalls, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, "ConfigNode" );
						break;
					
					default:
						break;
				}
			}
			siResult = gNodeList->GetNodes( nil, p->fInPatternMatchType, p->fOutDataBuff );

			if (siResult == eDSNoErr)
			{
				::memcpy( &p->fOutDirNodeCount, p->fOutDataBuff->fBufferData + 4, 4 );
				if ( p->fOutDirNodeCount == 0 )
				{
					siResult = eDSNodeNotFound;
				}
			}
		}
		
		*outStatus = siResult;
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // FindDirNodes


//------------------------------------------------------------------------------------
//	* GetNodeList
//------------------------------------------------------------------------------------

void* CRequestHandler::GetNodeList ( sComData *inMsg, SInt32 *outStatus )
{
	SInt32				siResult	= -8088;
	sGetDirNodeList	   *p			= nil;
	UInt32				uiBuffSize	= 0;
	CSrvrMessaging		cMsg;
	SInt32				aClientPID	= -1;
	UInt32				anIPAddress	= 0;

	try
	{
		p = (sGetDirNodeList *)::calloc(sizeof(sGetDirNodeList), sizeof(char));
		if ( p == nil ) throw( (SInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirRef, ktDirRef );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError );

		// Verify the Directory Reference
		siResult = CRefTable::VerifyDirRef( p->fInDirRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (SInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (SInt32)eServerReceiveError - 3 );
		
		siResult = cMsg.Get_Value_FromMsg( inMsg, (UInt32 *)&p->fIOContinueData, kContextData );
		if (siResult != eDSNoErr)
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		//currently not using the continue data

		*outStatus = gNodeList->BuildNodeListBuff( p );
	}

	catch( SInt32 err )
	{
		if ( p != nil )
		{
			free ( p );
			p = nil;
		}
		*outStatus = err;
	}

	return( p );

} // GetNodeList


//------------------------------------------------------------------------------------
//	* DoCheckUserNameAndPassword
//------------------------------------------------------------------------------------

SInt32 CRequestHandler::DoCheckUserNameAndPassword (	const char *userName,
														const char *password,
														tDirPatternMatch inPatternMatch,
														uid_t *outUID,
														char **outShortName )
{
	SInt32					siResult		= eDSNoErr;
	SInt32					returnVal		= DS_CHECKPW_FAILURE;
	UInt32					nodeCount		= 0;
	UInt32					recCount		= 0;
	tContextData			context			= NULL;
	tDataListPtr			nodeName		= NULL;
	char*					authName		= NULL;
	char*					uidString		= NULL;
	tDataNodePtr			authMethod		= NULL;
	tDataListPtr			recName			= NULL;
	tDataListPtr			recType			= NULL;
	tDataListPtr			attrTypes		= NULL;
	tDataBufferPtr			dataBuff		= NULL;
	tDataBufferPtr			authBuff		= NULL;
	tDirNodeReference		nodeRef			= 0;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeListRef		attrListRef		= 0;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	tDirReference			aDSRef			= 0;
	tDirNodeReference		aSearchNodeRef	= 0;
	
	if (inPatternMatch == eDSExact) //true checkpw call
	{
		aDSRef = gCheckPasswordDSRef;
		aSearchNodeRef = gCheckPasswordSearchNodeRef;
	}
	//KW27 syslog(LOG_CRIT,"chkpasswd username %s\n", userName );
	
	try
	{
		if ( aDSRef == 0 )
		{
			//dsFindDirNodes below already blocks on the search node
			// we know we need the search node, so let's wait until it is available.
			//gNodeList->WaitForAuthenticationSearchNode();
			
			siResult = dsOpenDirService( &aDSRef );
			if ( siResult != eDSNoErr ) return( DS_CHECKPW_FAILURE );
			
			dataBuff = dsDataBufferAllocate( aDSRef, 2048 );
			if ( dataBuff == nil ) throw( DS_CHECKPW_FAILURE );
			
			siResult = dsFindDirNodes( aDSRef, dataBuff, nil, 
										eDSAuthenticationSearchNodeName, &nodeCount, &context );
			if ( siResult != eDSNoErr ) throw( DS_CHECKPW_FAILURE );
			if ( nodeCount < 1 ) throw( DS_CHECKPW_FAILURE );
	
			siResult = dsGetDirNodeName( aDSRef, dataBuff, 1, &nodeName );
			if ( siResult != eDSNoErr ) throw( DS_CHECKPW_FAILURE );
	
			siResult = dsOpenDirNode( aDSRef, nodeName, &aSearchNodeRef );
			if ( siResult != eDSNoErr ) throw( DS_CHECKPW_FAILURE );
			if ( nodeName != NULL )
			{
				dsDataListDeallocate( aDSRef, nodeName );
				free( nodeName );
				nodeName = NULL;
			}
			if ( inPatternMatch == eDSExact ) //true checkpw call
			{
				gCheckPasswordDSRef = aDSRef;
				gCheckPasswordSearchNodeRef = aSearchNodeRef;
			}
		}
		if ( dataBuff == NULL )
		{
			dataBuff = dsDataBufferAllocate( aDSRef, 2048 );	
		}
		recName = dsBuildListFromStrings( aDSRef, userName, NULL );
		recType = dsBuildListFromStrings( aDSRef, kDSStdRecordTypeUsers, NULL );
		attrTypes = dsBuildListFromStrings( aDSRef, kDSNAttrMetaNodeLocation, 
											kDSNAttrRecordName, NULL );
		if ( outUID != NULL )
		{
			dsAppendStringToListAlloc( aDSRef, attrTypes, kDS1AttrUniqueID );
		}
		recCount = 1; // only care about first match
		// now FW is a direct dispatch
		do 
		{
			siResult = dsGetRecordList( aSearchNodeRef, dataBuff, recName, inPatternMatch, recType,
										attrTypes, false, &recCount, &context);
			if (siResult == eDSBufferTooSmall)
			{
				UInt32 bufSize = dataBuff->fBufferSize;
				dsDataBufferDeallocatePriv( dataBuff );
				dataBuff = nil;
				dataBuff = ::dsDataBufferAllocate( aDSRef, bufSize * 2 );
			}
			else if ( siResult != eDSNoErr )
			{
				DbgLog( kLogHandler, "DoCheckUsernameAndPassword:: dsGetRecordList error <%d>", siResult );
			}
		} while ( (siResult == eDSBufferTooSmall) || ( (siResult == eDSNoErr) && (recCount == 0) && (context != nil) ) );
		//worry about multiple calls (ie. continue data) since we continue until first match or no more searching
		
		if ( (siResult == eDSNoErr) && (recCount > 0) )
		{
			siResult = ::dsGetRecordEntry( aSearchNodeRef, dataBuff, 1, &attrListRef, &pRecEntry );
			if ( (siResult == eDSNoErr) && (pRecEntry != nil) )
			{
				//index starts at one - should have two entries
				for (unsigned int i = 1; i <= pRecEntry->fRecordAttributeCount; i++)
				{
					siResult = ::dsGetAttributeEntry( aSearchNodeRef, dataBuff, attrListRef, i, &valueRef, &pAttrEntry );
					//need to have at least one value - get first only
					if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
					{
						// Get the first attribute value
						siResult = ::dsGetAttributeValue( aSearchNodeRef, dataBuff, 1, valueRef, &pValueEntry );
						// Is it what we expected
						if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
						{
							nodeName = dsBuildFromPath( aDSRef, pValueEntry->fAttributeValueData.fBufferData, "/" );
						}
						else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
						{
							authName = dsCStrFromCharacters( pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
						}
						else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrUniqueID ) == 0 )
						{
							uidString = dsCStrFromCharacters( pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
							if ( outUID != NULL )
							{
								char *endPtr = nil;
								*outUID = (uid_t)strtol( uidString, &endPtr, 10 );
							}
						}
						if ( pValueEntry != NULL )
						{
							dsDeallocAttributeValueEntry( aDSRef, pValueEntry );
							pValueEntry = NULL;
						}
					}
					dsCloseAttributeValueList(valueRef);
					if (pAttrEntry != nil)
					{
						dsDeallocAttributeEntry(aDSRef, pAttrEntry);
						pAttrEntry = nil;
					}
				} //loop over attrs requested
			}//found 1st record entry
			dsCloseAttributeList(attrListRef);
			if (pRecEntry != nil)
			{
				dsDeallocRecordEntry(aDSRef, pRecEntry);
				pRecEntry = nil;
			}
		}// got records returned
		else
		{
			returnVal = DS_CHECKPW_UNKNOWNUSER;
		}
		
		if ( (nodeName != NULL) && (authName != NULL) )
		{
			siResult = dsOpenDirNode( aDSRef, nodeName, &nodeRef );
			if ( siResult != eDSNoErr ) throw( DS_CHECKPW_FAILURE );
			
			authMethod = dsDataNodeAllocateString( aDSRef, kDSStdAuthNodeNativeClearTextOK );
			authBuff = dsDataBufferAllocate( aDSRef, strlen(authName) + strlen(password) + 16 );
			if ( authBuff == NULL )
				throw( DS_CHECKPW_FAILURE );
			
			siResult = dsFillAuthBuffer( authBuff, 2, strlen(authName), authName, strlen(password), password );
			if ( siResult != eDSNoErr )
				throw( DS_CHECKPW_FAILURE );
			
			siResult = dsDoDirNodeAuth( nodeRef, authMethod, true, authBuff, dataBuff, NULL );
			if (gLogAPICalls)
			{
				syslog(LOG_CRIT,"checkpw: dsDoDirNodeAuth returned %d", siResult);//KW27 
			}
			
			switch (siResult)
			{
				case eDSNoErr:
					returnVal = DS_CHECKPW_SUCCESS;
					break;
				case eDSAuthNewPasswordRequired:
				case eDSAuthPasswordExpired:
				case eDSAuthAccountInactive:
				case eDSAuthAccountExpired:
				case eDSAuthAccountDisabled:
					returnVal = DS_CHECKPW_LOCKOUT;
					break;
				default:
					returnVal = DS_CHECKPW_BADPASSWORD;
					break;
			}
		}
	}
	
	catch( SInt32 err )
	{
		returnVal = err;
	}	
	
	if ( recName != NULL )
	{
		dsDataListDeallocate( aDSRef, recName );
		free( recName );
		recName = NULL;
	}
	if ( recType != NULL )
	{
		dsDataListDeallocate( aDSRef, recType );
		free( recType );
		recType = NULL;
	}
	if ( attrTypes != NULL )
	{
		dsDataListDeallocate( aDSRef, attrTypes );
		free( attrTypes );
		attrTypes = NULL;
	}
	if ( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( aDSRef, dataBuff );
		dataBuff = NULL;
	}
	if ( nodeName != NULL )
	{
		dsDataListDeallocate( aDSRef, nodeName );
		free( nodeName );
		nodeName = NULL;
	}
	if ( authName != NULL )
	{
		if ( outShortName != NULL )
		{
			*outShortName = authName;
		}
		else
		{
			free( authName );
		}
		authName = NULL;
	}
	if ( uidString != NULL )
	{
		free( uidString );
		uidString = NULL;
	}
	if ( authMethod != NULL )
	{
		dsDataNodeDeAllocate( aDSRef, authMethod );
		authMethod = NULL;
	}
	if ( authBuff != NULL )
	{
		dsDataBufferDeAllocate( aDSRef, authBuff );
		authBuff = NULL;
	}
	if ( nodeRef != 0 )
	{
		dsCloseDirNode( nodeRef );
		nodeRef = 0;
	}
	if ( inPatternMatch == eDSiExact ) //DSProxy call
	{ 
		if ( aSearchNodeRef != 0 )
		{
			dsCloseDirNode( aSearchNodeRef );
			aSearchNodeRef = 0;
		}
		if ( aDSRef != 0 )
		{
			dsCloseDirService( aDSRef );
			aDSRef = 0;
		}
	}
	else
	{
		if (gCheckPasswordSearchNodeRef == 0)
		{
			if (gCheckPasswordDSRef != 0)
			{
				dsCloseDirService(gCheckPasswordDSRef);
				gCheckPasswordDSRef = 0;
			}
		}
	}
	
	return returnVal;
}


#pragma mark -
#pragma mark Utility Handler Routines
#pragma mark -

bool CRequestHandler::UserInGivenGroup( const char* shortName, const char* groupName )
{
	//check if user is in the groupName group
	bool returnVal = false;
	guid_t user_uuid;
	guid_t group_uuid;
	kauth_identity_extlookup	request;

	Mbrd_ProcessMapName( true, (char *)shortName, &user_uuid );
	Mbrd_ProcessMapName( false, (char *)groupName, &group_uuid );
	
	// we formulate the request ourself here so no need to dispatch to ourself..
	request.el_seqno = 1;  // used as byte order field
	request.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP;
	memcpy( &request.el_uguid, &user_uuid, sizeof(guid_t) );
	memcpy( &request.el_gguid, &group_uuid, sizeof(guid_t) );
	
	Mbrd_ProcessLookup( &request );
	
	if ((request.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) != 0 && (request.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) != 0)
	{
		returnVal = true;
	}
		
	return returnVal;
}


char* CRequestHandler::GetCallName ( SInt32 inType )
{
	char *outName	= nil;

	if ( (inType > 0) && (inType < kDSServerCallsEnd) )
	{
		outName = (char *)sServerMsgType[ inType ];
	}
	else if ( (inType > kDSPlugInCallsBegin) && (inType < kDSPlugInCallsEnd) )
	{
		outName = (char *)sPlugInMsgType[ inType - kDSPlugInCallsBegin ];
	}
	else
	{
		outName = (char *)"Unknown Name";
	}

	return( outName );
} // GetCallName

void CRequestHandler::LogAPICall (	double			inTime,
									char		   *inDebugDataTag,
									SInt32			inResult)
{
	double	outTime	= 0;
	
	if (gLogAPICalls)
	{
		outTime = dsTimestamp();
	
		if (inDebugDataTag != nil)
		{
			syslog(LOG_CRIT,"%s : Result: %d, Duration: %.2f usec", inDebugDataTag, inResult, (outTime - inTime));
		}
	}
			
} //Log API Call

char* CRequestHandler::BuildAPICallDebugDataTag (UInt32			inIPAddress,
												SInt32			inClientPID,
												char		   *inCallName,
												char		   *inName)
{
	char	   *outTag = nil;
	UInt32		suffixLen	= strlen(inCallName) + strlen(inName);
	
	if (inIPAddress == 0)
	{
		//this is locally from this machine
		char *pName = nil;
		pName = dsGetNameForProcessID(inClientPID);
		if (pName != nil)
		{
			outTag = (char *)calloc(1, 38 + strlen(pName) + 6 + suffixLen); //needs to match string being built here
			sprintf(outTag, "Client: %s, PID: %ld, API: %s, %s Used", pName, inClientPID, inCallName, inName);
			free(pName);
		}
		else
		{
			if (inClientPID == 0)
			{
				outTag = (char *)calloc(1, 35 + suffixLen); //needs to match string being built here
				sprintf(outTag, "Internal Dispatch, API: %s, %s Used", inCallName, inName);
			}
			else
			{
				outTag = (char *)calloc(1, 32 + 6 + suffixLen); //needs to match string being built here
				sprintf(outTag, "Client PID: %ld, API: %s, %s Used", inClientPID, inCallName, inName);
			}
		}
	}
	else
	{
		//this is from a remote machine via DSProxy
		IPAddrStr	ipAddrString;
		DSNetworkUtilities::IPAddrToString( inIPAddress, ipAddrString, MAXIPADDRSTRLEN );
		if (ipAddrString[0] != '\0')
		{
			outTag = (char *)calloc(1, 55 + strlen(ipAddrString) + 6 + suffixLen); //needs to match string being built here
			sprintf(outTag, "Remote IP Address: %s, Client PID: %ld, API: %s, %s Used", ipAddrString, inClientPID, inCallName, inName);
		}
		else
		{
			outTag = (char *)calloc(1, 55 + 12 + 6 + suffixLen); //needs to match string being built here
			sprintf(outTag, "Remote IP Address: %lu, Client PID: %ld, API: %s, %s Used", inIPAddress, inClientPID, inCallName, inName);
		}
	}
	
	return(outTag);
	
} //BuildAPICallDebugDataTag

//--------------------------------------------------------------------------------------------------
//	* FailedCallRefCleanUp()
//
//--------------------------------------------------------------------------------------------------

SInt32 CRequestHandler:: FailedCallRefCleanUp ( void *inData, SInt32 inClientPID, UInt32 inMsgType, UInt32 inIPAddress  )
{
	SInt32			siResult	= eDSNoErr;

	try
	{
		if (inData == nil)
		{
			return( siResult );
		}

		switch ( inMsgType )
		{
			case kOpenDirNode:
			{
				sOpenDirNode *p = (sOpenDirNode *)inData;

				siResult = CRefTable::RemoveNodeRef( p->fOutNodeRef, inClientPID, inIPAddress );
				
				break;
			}

			case kGetDirNodeInfo:
			{
				sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;

				siResult = CRefTable::RemoveAttrListRef( p->fOutAttrListRef, inClientPID, inIPAddress );

				break;
			}

			case kGetRecordEntry:
			{
				sGetRecordEntry *p = (sGetRecordEntry *)inData;

				siResult = CRefTable::RemoveAttrListRef( p->fOutAttrListRef, inClientPID, inIPAddress );

				break;
			}

			case kGetAttributeEntry:
			{
				sGetAttributeEntry *p = (sGetAttributeEntry *)inData;

				siResult = CRefTable::RemoveAttrValueRef( p->fOutAttrValueListRef, inClientPID, inIPAddress );

				break;
			}

			case kOpenRecord:
			{
				sOpenRecord *p = (sOpenRecord *)inData;

				siResult = CRefTable::RemoveRecordRef( p->fOutRecRef, inClientPID, inIPAddress );

				break;
			}

			case kCreateRecordAndOpen:
			{
				sCreateRecord *p = (sCreateRecord *)inData;

				siResult = CRefTable::RemoveRecordRef( p->fOutRecRef, inClientPID, inIPAddress );

				break;
			}

			default:
				break;
		}
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );
	
} // FailedCallRefCleanUp



		
