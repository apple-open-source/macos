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
#include "CSrvrEndPoint.h"
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

#include <servers/bootstrap.h>
#include <stdlib.h>
#include <time.h>		// for time()
#include <syslog.h>		// for syslog()
#include <sys/sysctl.h>	// for sysctl()
#include <sys/time.h>	// for struct timespec and gettimeofday()
#include <mach/mach.h>	// for mach destroy of client port

static const char *sServerMsgType[ 13 ] =
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
	/* 12 */	"*** End of list ***"
};

static const char *sPlugInMsgType[ 37 ] =
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
	/*    */	"DoCheckNIAutoSwitch()",
	/* 36 */	"*** End of list ***"
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

//PFIXextern dsBool					gLocalNodeNotAvailable;

//API Call Count
extern uInt32					gAPICallCount;

extern uInt32					gDaemonPID;

extern uInt32					gDaemonIPAddress;


//--------------------------------------------------------------------------------------------------
//	* CHandlerThread()
//
//--------------------------------------------------------------------------------------------------

CHandlerThread::CHandlerThread ( void ) : CInternalDispatchThread(kTSHandlerThread)
{
	fThreadSignature = kTSHandlerThread;

	fTCPEndPt		= nil;
	fEndPt			= nil;
	fThreadIndex   	= kMaxHandlerThreads;  //not to be used with StopAHandler call
} // CHandlerThread


//--------------------------------------------------------------------------------------------------
//	* CHandlerThread(const FourCharCode inThreadSignature)
//
//--------------------------------------------------------------------------------------------------

CHandlerThread::CHandlerThread ( const FourCharCode inThreadSignature, uInt32 iThread ) : CInternalDispatchThread(inThreadSignature)
{
	fThreadSignature = inThreadSignature;

	fThreadIndex	= iThread;
	fTCPEndPt		= nil;
	fEndPt			= nil;
} // CHandlerThread ( FourCharCode inThreadSignature )

//--------------------------------------------------------------------------------------------------
//	* ~CHandlerThread()
//
//--------------------------------------------------------------------------------------------------

CHandlerThread::~CHandlerThread()
{
	if (fThreadSignature != kTSTCPHandlerThread)
	{
		if ( fEndPt != nil )
		{
			delete( fEndPt );
			fEndPt = nil;
		}
	}
	else
	{
		if ( fTCPEndPt != nil )
		{
			// this is actually owned by the DSTCPConnection
			//delete( fTCPEndPt );
			//fTCPEndPt = nil;
		}
	}
	
	if ( (fThreadSignature == kTSHandlerThread) || (fThreadSignature == kTSTCPHandlerThread) )
	{
		fThreadIndex   	= kMaxHandlerThreads;  //not to be used with StopAHandler call
	}
	if (fThreadSignature == kTSInternalHandlerThread)
	{
		fThreadIndex   	= kMaxInternalHandlerThreads;  //not to be used with StopAHandler call
	}
	if (fThreadSignature == kTSCheckpwHandlerThread)
	{
		fThreadIndex   	= kMaxCheckpwHandlerThreads;  //not to be used with StopAHandler call
	}
} // ~CHandlerThread

//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CHandlerThread::StartThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

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

	if (fThreadSignature == kTSTCPHandlerThread)
	{
		// we stop our own thread after notifying the ServerControl
		gTCPHandlerLock->Wait();
		gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
		gTCPHandlerLock->Signal();
	}
	else if (fThreadSignature == kTSHandlerThread)
	{
		// we stop our own thread after notifying the ServerControl
		gHandlerLock->Wait();
		gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
		gHandlerLock->Signal();
	}
	else if (fThreadSignature == kTSInternalHandlerThread)
	{
		// we stop our own thread after notifying the ServerControl
		gInternalHandlerLock->Wait();
		gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
		gInternalHandlerLock->Signal();
	}
	else if (fThreadSignature == kTSCheckpwHandlerThread)
	{
		// we stop our own thread after notifying the ServerControl
		gCheckpwHandlerLock->Wait();
		gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
		gCheckpwHandlerLock->Signal();
	}

} // LastChance


//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CHandlerThread::StopThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

long CHandlerThread::ThreadMain ( void )
{
	volatile	uInt32			msgCount	= 0;
	volatile	uInt32			loopAgain	= 2 + fThreadIndex;		// each thread has different count here
	volatile	uInt32			aWaitTime	= 8; 					// this is controlled by fThreadIndex used in the loopAgain var
																	// 8 secs
				sInt32			result		= eDSNoErr;

	if (fThreadSignature != kTSTCPHandlerThread)
	{
		result = CreateEndpoint();
		if ( result != eDSNoErr )
		{
			DBGLOG2( kLogHandler, "File: %s. Line: %d", __FILE__, __LINE__ );
			DBGLOG1( kLogThreads, "  ***CreateEndpoint() call failed = %d", result );
			StopThread();
			return( -1 );
		}
	}
	//else we already have a TCP endpoint given to us from the DSConnection
	else
	{
		while ( GetThreadRunState() != kThreadStop )
		{
			try
			{
            	while ( GetThreadRunState() != kThreadStop )
            	{
                	//run the loop within the Try unless there is an actual Throw
                	msgCount = 0;

					gSrvrCntl->SleepAHandler( fThreadSignature, aWaitTime * kMilliSecsPerSec );

                	// Check for work to do
                	if ( gTCPMsgQueue != nil )
                	{
						msgCount = gTCPMsgQueue->GetMsgCount();
                	}

                	// Do the work
                	if ( msgCount != 0 )
                	{
                    	HandleMessage();
						if (!(gAPICallCount % 7549)) //using a large prime here
						{
							if (gLogAPICalls)
							{
								syslog(LOG_INFO,"API clients have called %d times - resetting counter", gAPICallCount);
							}
							gAPICallCount = 0;
							//do this here even though loop is for TCP only
							CRefTable::CheckClientPIDs(true, gDaemonIPAddress, 0);
						}
						loopAgain = 2 + fThreadIndex;
                	}
                	else
					{
                 	   // try again?
						loopAgain--;
					}
					
					if (loopAgain == 0) //try to shutdown path
					{
                    	// we stop our own thread after notifying the ServerControl
						gTCPHandlerLock->Wait();
					
						if ( gTCPMsgQueue != nil )
						{
							msgCount = 0;
							msgCount = gTCPMsgQueue->GetMsgCount();
							if ( msgCount != 0 )
							{
								loopAgain = 2 + fThreadIndex;
								gTCPHandlerLock->Signal();
							}
							else
							{
                    			gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
								StopThread();
								gTCPHandlerLock->Signal();
							}
						}
						else
						{
							gTCPHandlerLock->Signal();
						}
					}
				
				} // while thread not stopped state
			} // try

			catch( sInt32 err1 )
			{
				gTCPHandlerLock->Signal();
				DBGLOG2( kLogHandler, "File: %s. Line: %d", __FILE__, __LINE__ );
				DBGLOG1( kLogHandler, "  *** Caught exception (#2).  Error = %d", err1 );
			}
		} //while loop over run state
	} //else (fThreadSignature == kTSTCPHandlerThread)
	
	if (fThreadSignature == kTSHandlerThread)
	{
		while ( GetThreadRunState() != kThreadStop )
		{
			try
			{
            	while ( GetThreadRunState() != kThreadStop )
            	{
                	//run the loop within the Try unless there is an actual Throw
                	msgCount = 0;

					gSrvrCntl->SleepAHandler( fThreadSignature, aWaitTime * kMilliSecsPerSec );

                	// Check for work to do
                	if ( gMsgQueue != nil )
                	{
						msgCount = gMsgQueue->GetMsgCount();
                	}

                	// Do the work
                	if ( msgCount != 0 )
                	{
                    	HandleMessage();
						if (!(gAPICallCount % 7549)) //using a large prime here
						{
							if (gLogAPICalls)
							{
								syslog(LOG_INFO,"API clients have called %d times - resetting counter", gAPICallCount);
							}
							gAPICallCount = 0;
							CRefTable::CheckClientPIDs(true, gDaemonIPAddress, 0);
						}
						loopAgain = 2 + fThreadIndex;
                	}
                	else
					{
                 	   // try again?
						loopAgain--;
					}
					
					if (loopAgain == 0) //try to shutdown path
					{
                    	// we stop our own thread after notifying the ServerControl
						gHandlerLock->Wait();
					
						if ( gMsgQueue != nil )
						{
							msgCount = 0;
							msgCount = gMsgQueue->GetMsgCount();
							if ( msgCount != 0 )
							{
								loopAgain = 2 + fThreadIndex;
								gHandlerLock->Signal();
							}
							else
							{
								//check if we are the last CHandler if so then check Client PIDS
								if (gSrvrCntl->GetHandlerCount(fThreadSignature) == 1)
								{
									CRefTable::CheckClientPIDs(false, gDaemonIPAddress, 0);
								}
                    			gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
								StopThread();
								gHandlerLock->Signal();
							}
						}
						else
						{
							gHandlerLock->Signal();
						}
					}
				
				} // while thread not stopped state
			} // try

			catch( sInt32 err1 )
			{
				gHandlerLock->Signal();
				DBGLOG2( kLogHandler, "File: %s. Line: %d", __FILE__, __LINE__ );
				DBGLOG1( kLogHandler, "  *** Caught exception (#2).  Error = %d", err1 );
			}
		} //while loop over run state
	} //if (fThreadSignature == kTSHandlerThread)
	
	if (fThreadSignature == kTSInternalHandlerThread)
	{
		while ( GetThreadRunState() != kThreadStop )
		{
			try
			{
            	while ( GetThreadRunState() != kThreadStop )
            	{
                	//run the loop within the Try unless there is an actual Throw
                	msgCount = 0;

					gSrvrCntl->SleepAHandler( fThreadSignature, aWaitTime * kMilliSecsPerSec );

                	// Check for work to do
                	if ( gInternalMsgQueue != nil )
                	{
						msgCount = gInternalMsgQueue->GetMsgCount();
                	}

                	// Do the work
                	if ( msgCount != 0 )
                	{
                    	HandleMessage();
						loopAgain = 2 + fThreadIndex;
                	}
                	else
					{
                 	   // try again?
						loopAgain--;
					}
					
					if (loopAgain == 0) //try to shutdown path
					{
                    	// we stop our own thread after notifying the ServerControl
						gInternalHandlerLock->Wait();
					
						if ( gInternalMsgQueue != nil )
						{
							msgCount = 0;
							msgCount = gInternalMsgQueue->GetMsgCount();
							if ( msgCount != 0 )
							{
								loopAgain = 2 + fThreadIndex;
								gInternalHandlerLock->Signal();
							}
							else
							{
                    				gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
								StopThread();
								gInternalHandlerLock->Signal();
							}
						}
						else
						{
							gInternalHandlerLock->Signal();
						}
					}
				
				} // while thread not stopped state
			} // try

			catch( sInt32 err1 )
			{
				gInternalHandlerLock->Signal();
				DBGLOG2( kLogHandler, "File: %s. Line: %d", __FILE__, __LINE__ );
				DBGLOG1( kLogHandler, "  *** Caught exception (#2).  Error = %d", err1 );
			}
		} //while loop over run state
	} //if (fThreadSignature == kTSInternalHandlerThread)
	
	if (fThreadSignature == kTSCheckpwHandlerThread)
	{
		while ( GetThreadRunState() != kThreadStop )
		{
			try
			{
            	while ( GetThreadRunState() != kThreadStop )
            	{
                	//run the loop within the Try unless there is an actual Throw
                	msgCount = 0;

					gSrvrCntl->SleepAHandler( fThreadSignature, aWaitTime * kMilliSecsPerSec );

                	// Check for work to do
                	if ( gCheckpwMsgQueue != nil )
                	{
						msgCount = gCheckpwMsgQueue->GetMsgCount();
                	}

                	// Do the work
                	if ( msgCount != 0 )
                	{
                    	HandleMessage();
						loopAgain = 2 + fThreadIndex;
                	}
                	else
					{
                 	   // try again?
						loopAgain--;
					}
					
					if (loopAgain == 0) //try to shutdown path
					{
                    	// we stop our own thread after notifying the ServerControl
						gCheckpwHandlerLock->Wait();
					
						if ( gCheckpwMsgQueue != nil )
						{
							msgCount = 0;
							msgCount = gCheckpwMsgQueue->GetMsgCount();
							if ( msgCount != 0 )
							{
								loopAgain = 2 + fThreadIndex;
								gCheckpwHandlerLock->Signal();
							}
							else
							{
                    				gSrvrCntl->StopAHandler(fThreadSignature, fThreadIndex, (CHandlerThread *)this);
								StopThread();
								gCheckpwHandlerLock->Signal();
							}
						}
						else
						{
							gCheckpwHandlerLock->Signal();
						}
					}
				
				} // while thread not stopped state
			} // try

			catch( sInt32 err1 )
			{
				gCheckpwHandlerLock->Signal();
				DBGLOG2( kLogHandler, "File: %s. Line: %d", __FILE__, __LINE__ );
				DBGLOG1( kLogHandler, "  *** Caught exception (#2).  Error = %d", err1 );
			}
		} //while loop over run state
	} //if (fThreadSignature == kTSCheckpwHandlerThread)
	
	return( 0 );

} // ThreadMain


//--------------------------------------------------------------------------------------------------
//	* CreateEndpoint()
//
//--------------------------------------------------------------------------------------------------

sInt32 CHandlerThread::CreateEndpoint ( void )
{
	sInt32		result = eDSNoErr;

	fEndPt = new CSrvrEndPoint( kDSServiceName );
	if ( fEndPt == nil )
	{
		result = eMemoryError;
	}

	return( result );

} // CreateEndpoint


//--------------------------------------------------------------------------------------------------
//	* HandleMessage()
//
//--------------------------------------------------------------------------------------------------

void CHandlerThread::HandleMessage ( void )
{
	sInt32			siResult	= eDSNoErr;
	sComData	   *pRequest	= nil;
	void		   *reqMsg		= nil;
	CRequestHandler handler;

	if (fThreadSignature == kTSTCPHandlerThread)
	{
		if ( gTCPMsgQueue != nil )
		{
			siResult = gTCPMsgQueue->DequeueMessage( &reqMsg );
			pRequest = (sComData *)reqMsg;
			//now this method owns the pRequest data
			if ( (siResult == eDSNoErr) && (pRequest != nil) )
			{
				handler.HandleRequest(&pRequest);
				
				//send back reply using TCP endpoint from the data message itself
				fTCPEndPt = (DSTCPEndpoint *)pRequest->fPort;
				if (fTCPEndPt != nil )
				{
					(void)fTCPEndPt->SendClientReply( pRequest );
					//endpoint is owned by DSTCPConnection parent
					//delete (fTCPEndPt);
					//fTCPEndPt = nil;
				}
				else
				{
					ERRORLOG( kLogTCPEndpoint, "TCPHandlerThread not provided correct TCP Endpoint through the data message fPort member");
				}

				//TODO the connection is to go away since call was to close DS
				//DSTCPConnection will know this but also need to cleanup any lingering refs in the ref table?
				
				if ( pRequest != nil )
				{
					free( pRequest );
					pRequest = nil;
				}
			}
		}
	}

	if (fThreadSignature == kTSHandlerThread)
	{
		if ( gMsgQueue != nil )
		{
			siResult = gMsgQueue->DequeueMessage( &reqMsg );
			pRequest = (sComData *)reqMsg;
			//now this method owns the pRequest data
			if ( (siResult == eDSNoErr) && (pRequest != nil) )
			{
				bool closePort = handler.HandleRequest(&pRequest);
				
				(void)fEndPt->SendClientReply( pRequest );

				if (closePort)
				{
					mach_port_destroy((mach_task_self)(), pRequest->head.msgh_remote_port);
				}

				if ( pRequest != nil )
				{
					free( pRequest );
					pRequest = nil;
				}
			}
		}
	}

	if (fThreadSignature == kTSInternalHandlerThread)
	{
		if ( gInternalMsgQueue != nil )
		{
			siResult = gInternalMsgQueue->DequeueMessage( &reqMsg );
			pRequest = (sComData *)reqMsg;
			//now this method owns the pRequest data
			if ( (siResult == eDSNoErr) && (pRequest != nil) )
			{
				handler.HandleRequest(&pRequest);
				
				(void)fEndPt->SendClientReply( pRequest );

				if ( pRequest != nil )
				{
					free( pRequest );
					pRequest = nil;
				}
			}
		}
	}

	if (fThreadSignature == kTSCheckpwHandlerThread)
	{
		if ( gCheckpwMsgQueue != nil )
		{
			siResult = gCheckpwMsgQueue->DequeueMessage( &reqMsg );
			pRequest = (sComData *)reqMsg;
			//now this method owns the pRequest data
			if ( (siResult == eDSNoErr) && (pRequest != nil) )
			{
				handler.HandleRequest(&pRequest);
				
				(void)fEndPt->SendClientReply( pRequest );

				if ( pRequest != nil )
				{
					free( pRequest );
					pRequest = nil;
				}
			}
		}
	}

} // HandleMessage


CRequestHandler::CRequestHandler( void )
{
	bClosePort		= false;
}

bool CRequestHandler::HandleRequest ( sComData **inMsg )
{
	sInt32			siResult	= eDSNoErr;
	uInt32			uiMsgType	= 0;
	char *pType = GetCallName( GetMsgType( *inMsg ));

    if ( pType != nil )
	{
//		DBGLOG1( kLogHandler, "Making call -- %s --", pType );
	}
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
	
		DBGLOG3(	kLogMsgTrans, "Port: %l Call: %s == %l",
					(*inMsg)->head.msgh_remote_port,
					GetCallName( uiMsgType ),
					siResult );
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
	uInt32				uiMsgType	= 0;

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
	uInt32				uiMsgType	= 0;

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

sInt32 CRequestHandler::HandleServerCall ( sComData **inMsg )
{
	sInt32			siResult		= eDSNoErr;
	uInt32			uiMsgType		= 0;
	uInt32			count			= 0;
	uInt32			changeToken		= 0;
	uInt32			uiDirRef		= 0;
	uInt32			uiGenericRef   	= 0;
	uInt32			uiChildPID		= 0;
	tDataBuffer*	dataBuff		= NULL;
	tDataBuffer*	versDataBuff	= NULL;
	CSrvrMessaging	cMsg;
	sInt32			aClientPID		= -1;
	uInt32			anIPAddress		= 0;
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

			DBGLOG3( kLogHandler, "%s : DAR : Dir Ref %u : Result code = %d", debugDataTag, uiDirRef, siResult );
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
						sInt32 curr = 0;
						unsigned long len = 0;
						
						char* userName = NULL;
						char* password = NULL;
						char* shortName = NULL;
						uid_t localUID = (uid_t)-2;
						
						siResult = -3;
		
						if ( dataBuff->fBufferLength >= 8 )
						{ // need at least 8 bytes for lengths
							// User Name
							::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( unsigned long ) );
							curr += sizeof( unsigned long );
							if ( len <= dataBuff->fBufferLength - curr )
							{
								userName = (char*)::calloc( len + 1, sizeof( char ) );
								::memcpy( userName, &(dataBuff->fBufferData[ curr ]), len );
								curr += len;
								DBGLOG2( kLogHandler, "%s : DAC : 1 : Username = %s", debugDataTag, userName );
		
								// Password
								::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( unsigned long ) );
								curr += sizeof ( unsigned long );
								if ( len <= dataBuff->fBufferLength - curr )
								{
									password = (char*)::calloc( len + 1, sizeof( char ) );
									::memcpy( password, &(dataBuff->fBufferData[ curr ]), len );
									curr += len;
									DBGLOG1( kLogHandler, "%s : DAC : 2 : A password was received", debugDataTag );
		
									//case insensitivity of username allowed for DSProxy
									siResult = DoCheckUserNameAndPassword( userName, password, eDSiExact, &localUID, &shortName );
									// now we need to check that the user is in the admin group
									if ( (siResult == 0) && (localUID != 0) && !UserIsAdmin( shortName ) )
									{
										DBGLOG2( kLogHandler, "%s : DAC : 3 : Username = %s is not an ADMIN user", debugDataTag, userName );
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
							::free( password );
							password = NULL;
						}
					}
		
					if (siResult == eDSNoErr) //if auth succeeded
					{
						siResult = CRefTable::NewDirRef( &uiDirRef, aClientPID, anIPAddress );
						uInt32 serverVersion = 1;
		
						DBGLOG4( kLogHandler, "%s : DAR : Dir Ref %u : Server version = %u : Result code = %d", debugDataTag, uiDirRef, serverVersion, siResult );
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

		case kCloseDirService:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			DBGLOG2( kLogHandler, "%s : DAC : Dir Ref %u ", debugDataTag, uiDirRef );
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &count, kNodeCount );

			siResult = CRefTable::RemoveDirRef( uiDirRef, aClientPID, anIPAddress );

			DBGLOG3( kLogHandler, "%s : DAR : Dir Ref %u : Result code = %d", debugDataTag, uiDirRef, siResult );
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
			DBGLOG2( kLogHandler, "%s : DAC : Dir Ref %u ", debugDataTag, uiDirRef );
			if ( siResult == eDSNoErr )
			{
				siResult = CRefTable::VerifyDirRef( uiDirRef, nil, aClientPID, anIPAddress );

				cMsg.ClearDataBlock( (*inMsg) );

				if ( siResult == eDSNoErr )
				{
					count = gNodeList->GetNodeCount();
					DBGLOG4( kLogHandler, "%s : DAR : Dir Ref = %u : Node Count = %u : Result code = %d", debugDataTag, uiDirRef, count, siResult );
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
			DBGLOG2( kLogHandler, "%s : DAC : Dir Ref %u ", debugDataTag, uiDirRef );
			if ( siResult == eDSNoErr )
			{
				siResult = CRefTable::VerifyDirRef( uiDirRef, nil, aClientPID, anIPAddress );
				
				cMsg.ClearDataBlock( (*inMsg) );

				if ( siResult == eDSNoErr )
				{
					DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
					siResult = SetRequestResult( (*inMsg), siResult );
					if ( siResult == eDSNoErr )
					{
						count = gNodeList->GetNodeCount();
						// Add the nodeCount
						siResult = cMsg.Add_Value_ToMsg( (*inMsg), count, kNodeCount );

						changeToken = gNodeList->GetNodeChangeToken();
						// Add the changeToken
						siResult = cMsg.Add_Value_ToMsg( (*inMsg), changeToken, kNodeChangeToken );
						DBGLOG4( kLogHandler, "%s : DAR : 2 : Dir Ref = %u : Node Count = %u : Change Token = %u", debugDataTag, uiDirRef, count, changeToken );
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
				uInt32 aLength = 0;
				cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
				cMsg.Get_Value_FromMsg( (*inMsg), &aLength, kOutBuffLen ); //no need to check return
				DBGLOG3( kLogHandler, "%s : DAC : Dir Ref %u : Data buffer size = %u", debugDataTag, uiDirRef, aLength );
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
			else
			{
				siResult = eMemoryError;
			}

			if ( siResult != eDSNoErr )
			{
				cMsg.ClearDataBlock( (*inMsg) );
				SetRequestResult( (*inMsg), siResult );
			}
			DBGLOG3( kLogHandler, "%s : DAR : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			break;
		}

		case kFindDirNodes:
		{
			void	*newData	= nil;

			if (gDebugLogging)
			{
				uInt32 aLength	= 0;
				cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
				cMsg.Get_Value_FromMsg( (*inMsg), &aLength, kOutBuffLen ); //no need to check return
				DBGLOG3( kLogHandler, "%s : DAC : Dir Ref %u : Data buffer size = %u", debugDataTag, uiDirRef, aLength );
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
			else
			{
				siResult = eMemoryError;
			}

			if ( siResult != eDSNoErr )
			{
				cMsg.ClearDataBlock( (*inMsg) );
				SetRequestResult( (*inMsg), siResult );
			}
			DBGLOG3( kLogHandler, "%s : DAR : 2 : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
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
			DBGLOG2( kLogHandler, "%s : DAC : Dir Ref %u", debugDataTag, uiDirRef );
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

			DBGLOG3( kLogHandler, "%s : DAR : Dir Ref %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );
			break;
		}

		case kAddChildPIDToReference:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiGenericRef, ktGenericRef );
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiChildPID, ktPidRef );

			siResult = CRefTable::AddChildPIDToRef( uiGenericRef, aClientPID, uiChildPID, anIPAddress );

			DBGLOG3( kLogHandler, "%s : DAR : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );
			break;
		}

		case kCheckUserNameAndPassword:
		{
			siResult = cMsg.Get_Value_FromMsg( (*inMsg), &uiDirRef, ktDirRef );
			siResult = cMsg.Get_tDataBuff_FromMsg( (*inMsg), &dataBuff, ktDataBuff );
			if ( siResult == eDSNoErr )
			{
				SInt32 curr = 0;
				unsigned long len = 0;
				
				char* userName = NULL;
				char* password = NULL;
				
				siResult = -3;

                if ( dataBuff->fBufferLength >= 8 )
                { // sanity check, need at least 8 bytes for lengths
					// User Name
					::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( unsigned long ) );
					curr += sizeof( unsigned long );
					if ( len <= dataBuff->fBufferLength - curr )
                    { // sanity check
						userName = (char*)::calloc( len + 1, sizeof( char ) );
						::memcpy( userName, &(dataBuff->fBufferData[ curr ]), len );
						curr += len;
						DBGLOG3( kLogHandler, "%s : DAC : 1 : Dir Ref = %u : Username = %s", debugDataTag, uiDirRef, userName );

						// Password
						::memcpy( &len, &(dataBuff->fBufferData[ curr ]), sizeof( unsigned long ) );
						curr += sizeof ( unsigned long );
						if ( len <= dataBuff->fBufferLength - curr )
                        { // sanity check
							password = (char*)::calloc( len + 1, sizeof( char ) );
							::memcpy( password, &(dataBuff->fBufferData[ curr ]), len );
							curr += len;
							DBGLOG2( kLogHandler, "%s : DAC : 2 : Dir Ref = %u : A password was received", debugDataTag, uiDirRef );

							//username must be exact match for kCheckUserNameAndPassword
							siResult = DoCheckUserNameAndPassword( userName, password, eDSExact, NULL, NULL );
						}
					}
				}
				if ( dataBuff != NULL )
				{
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
					::free( password );
					password = NULL;
				}
			}

			DBGLOG3( kLogHandler, "%s : DAR : Dir Ref = %u : Result code = %d", debugDataTag, uiDirRef, siResult );
			siResult = SetRequestResult( (*inMsg), siResult );
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
		DBGLOG2( kLogHandler, "%s : HandleServerCall failed with error = %l", debugDataTag, siResult );
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

sInt32 CRequestHandler::HandlePluginCall ( sComData **inMsg )
{
	sInt32			siResult		= eDSNoErr;
	void		   *pData			= nil;
	bool			shouldProcess   = true;
	bool			found			= false;
	uInt32			uiState			= 0;
	sInt32			aClientPID		= -1;
	uInt32			anIPAddress		= 0;
	char		   *aMsgName		= nil;
	char		   *debugDataTag	= nil;
	double			inTime			= 0;
	double			outTime			= 0;
	bool			performanceStatGatheringActive = gSrvrCntl->IsPeformanceStatGatheringActive();
	sInt32			pluginResult	= eDSNoErr;

	// Set this to nil, it will get set in next call
	fPluginPtr = nil;

	aClientPID	= (*inMsg)->fPID;
	anIPAddress	= (*inMsg)->fIPAddress;
	aMsgName	= GetCallName( GetMsgType( *inMsg ) );

	pData = GetRequestData( (*inMsg), &siResult, &shouldProcess ); //fPluginPtr gets set inside this call
	if ( siResult == eDSNoErr )
	{
		if ( pData != nil )
		{
			if ( fPluginPtr != nil )
			{
				siResult = gPlugins->GetState( fPluginPtr->GetPluginName(), &uiState );
				if ( siResult == kPlugInListNoErr )
				{
					// always allow custom calls so we can configure even when the plug-in is disabled
					if ( (uiState & kActive) || (GetMsgType( *inMsg ) == kDoPlugInCustomCall) )
					{
						if ( (gDebugLogging) || (gLogAPICalls) )
						{
							debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, fPluginPtr->GetPluginName());
							DebugAPIPluginCall(pData, debugDataTag);
						}

						if ( gLogAPICalls || performanceStatGatheringActive )
						{
							inTime = dsTimestamp();
						}

						if ( shouldProcess )
						{
							siResult = fPluginPtr->ProcessRequest( pData );
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
						// If the plugin is not finished initializing, let's hang out
						//	for a while and see if it finishes sucessefully
						if ( uiState & kUninitialized )
						{
							DSSemaphore	initWait;
							time_t		waitForIt = ::time( nil ) + 120;

							while ( uiState & kUninitialized )
							{
								siResult = gPlugins->GetState( fPluginPtr->GetPluginName(), &uiState );
								if ( siResult == kPlugInListNoErr )
								{
									// Wait for .5 seconds
									initWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

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
							if ( (gDebugLogging) || (gLogAPICalls) )
							{
								debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, fPluginPtr->GetPluginName());
								DebugAPIPluginCall(pData, debugDataTag);
							}

							if ( gLogAPICalls || performanceStatGatheringActive )
							{
								inTime = dsTimestamp();
							}

							if ( shouldProcess )
							{
								siResult = fPluginPtr->ProcessRequest( pData );
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
					}
					else if ( uiState & kInactive )
					{
						siResult = ePlugInNotActive;
					}
					else if ( uiState & kFailedToInit )
					{
						siResult = ePlugInInitError;
					}
				}
			}
			else
			{
				sHeader		   *p			= (sHeader *)pData;
				CServerPlugin	*pPlugin	= nil;
				tDataNode	   *aNodePtr	= nil;
				tDataBufferPriv	   *pPrivData	= nil;

				if ( (p->fType == kOpenDirNode) && shouldProcess )
				{
					sOpenDirNode *pDataMask = (sOpenDirNode *)pData;
					if ( gPlugins != nil )
					{
						char *nodePrefix = nil;
						//retrieve the node Prefix out of the tDataList pDataMask->fInDirNodeName
						aNodePtr = ::dsGetThisNodePriv( pDataMask->fInDirNodeName->fDataListHead, 1 );
						if (aNodePtr != nil)
						{
							pPrivData = (tDataBufferPriv *)aNodePtr;
							nodePrefix = (char *)calloc(1, pPrivData->fBufferLength + 1);
							strncpy(nodePrefix, pPrivData->fBufferData, pPrivData->fBufferLength);
						}
						//try to get correct plugin to handle this request					
						pPlugin = gPlugins->GetPlugInPtr(nodePrefix);
						//cleanup the temp string
						if (nodePrefix != nil)
						{
							free(nodePrefix);
							nodePrefix = nil;
						}

						if ( pPlugin != nil )
						{
							if ( (gDebugLogging) || (gLogAPICalls) )
							{
								debugDataTag = BuildAPICallDebugDataTag(anIPAddress, aClientPID, aMsgName, pPlugin->GetPluginName());
								DebugAPIPluginCall(pData, debugDataTag);
							}

							if ( gLogAPICalls || performanceStatGatheringActive )
							{
								inTime = dsTimestamp();
							}

							siResult = pPlugin->ProcessRequest( pData );

							pluginResult = siResult;
							if ( ( siResult == eDSNoErr ) || (siResult == eDSBufferTooSmall) )
							{
								found = true;
								siResult = PackageReply( pData, inMsg );

								LogAPICall( inTime, debugDataTag, pluginResult);
								DebugAPIPluginResponse( pData, debugDataTag, pluginResult);

								if ( siResult == eDSNoErr )
								{
									siResult = CRefTable::SetNodePluginPtr( pDataMask->fOutNodeRef, pPlugin );
								}
							}
							else
							{
								//remove reference from list called below
								LogAPICall( inTime, debugDataTag, pluginResult);
								DebugAPIPluginResponse( pData, debugDataTag, pluginResult);
							}

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

				if ( (p->fType == kOpenDirNode) && !found )
				{
					DBGLOG1( kLogHandler, "*** Error NULL plug-in pointer.  Returning error = %l.", eMemoryError );
					siResult = eDSNodeNotFound;
				}
			}
		}
		else
		{
			DBGLOG1( kLogHandler, "*** Error NULL data ub plug-in handler.  Returning error = %l.", eMemoryError );
			siResult = eMemoryError;
		}
	}
	else if ( siResult == -1212 )
	{
		//KW this -1212 only possible from DoReleaseContinueData()
		siResult = eDSNoErr;
	}

	if ( siResult == eDSBufferTooSmall )
	{
		siResult = eDSNoErr;
	}

	if ( siResult != eDSNoErr )
	{
		if ( pData != nil )
		{
			sHeader   *p		= (sHeader *)pData;

			char *pType = GetCallName( p->fType );
			if ( pType != nil )
			{
				sInt32 cleanUpResult = eDSNoErr;
				cleanUpResult = FailedCallRefCleanUp( pData, aClientPID, p->fType, anIPAddress );
				DBGLOG2( kLogHandler, "Plug-in call \"%s\" failed with error = %l.", pType, siResult );
			}
			else
			{
				DBGLOG2( kLogHandler, "Plug-in call failed with error = %l, type = %d.", siResult, p->fType );
			}
		}
		else
		{
			DBGLOG1( kLogHandler, "Plug-in call failed with error = %l (NULL data).", siResult );
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

sInt32 CRequestHandler::HandleUnknownCall ( sComData *inMsg )
{
	sInt32			siResult	= eDSNoErr;

	siResult = SetRequestResult( inMsg, eUnknownAPICall );

	return( siResult );

} // HandleUnknownCall


//--------------------------------------------------------------------------------------------------
//	* GetRequestData()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::GetRequestData ( sComData *inMsg, sInt32 *outResult, bool *outShouldProcess )
{
	void			   *outData			= nil;
	uInt32				uiMsgType		= 0;

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

		case kDoAttributeValueSearchWithData:
			outData = DoAttributeValueSearchWithData( inMsg, outResult );
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

sInt32 CRequestHandler::SetRequestResult ( sComData *inMsg, sInt32 inResult )
{
	sInt32			siResult	= -8088;
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
												sInt32			inResult)
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
			DBGLOG4( kLogHandler, "%s : DAR : Dir Ref = %u : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInDirNodeName, p->fOutNodeRef, inResult );
			break;
		}

		case kCloseDirNode:
		{
			sCloseDirNode *p = (sCloseDirNode *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
			break;
		}

		case kOpenRecord:
		{
			sOpenRecord *p = (sOpenRecord *)inData;
			DBGLOG4( kLogHandler, "%s : DAR : Node Ref = %u : Record Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecRef, inResult );
			break;
		}

		case kCreateRecord:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			DBGLOG4( kLogHandler, "%s : DAR : Node Ref = %u : Record Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecRef, inResult );
			break;
		}

		case kCreateRecordAndOpen:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			DBGLOG4( kLogHandler, "%s : DAR : Node Ref = %u : Record Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecRef, inResult );
			break;
		}

		case kCloseRecord:
		{
			sCloseRecord *p = (sCloseRecord *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}
		
		case kDeleteRecord:
		{
			sDeleteRecord *p = (sDeleteRecord *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}
		
		case kGetRecordReferenceInfo:
		{
			sGetRecRefInfo *p = (sGetRecRefInfo *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordAttributeInfo:
		{
			sGetRecAttribInfo *p = (sGetRecAttribInfo *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordAttributeValueByID:
		{
			sGetRecordAttributeValueByID *p = (sGetRecordAttributeValueByID *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordAttributeValueByIndex:
		{
			sGetRecordAttributeValueByIndex *p = (sGetRecordAttributeValueByIndex *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetDirNodeInfo:
		{
			sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
			break;
		}

		case kSetRecordName:
		{
			sSetRecordName *p = (sSetRecordName *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kAddAttribute:
		{
			sAddAttribute *p = (sAddAttribute *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kRemoveAttribute:
		{
			sRemoveAttribute *p = (sRemoveAttribute *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kAddAttributeValue:
		{
			sAddAttributeValue *p = (sAddAttributeValue *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kRemoveAttributeValue:
		{
			sRemoveAttributeValue *p = (sRemoveAttributeValue *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kSetAttributeValue:
		{
			sSetAttributeValue *p = (sSetAttributeValue *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Rec Ref = %u : Result code = %d", inDebugDataTag, p->fInRecRef, inResult );
			break;
		}

		case kGetRecordList:
		{
			sGetRecordList *p = (sGetRecordList *)inData;
			DBGLOG5( kLogHandler, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u  : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutRecEntryCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoAttributeValueSearch:
		{
			sDoAttrValueSearch *p = (sDoAttrValueSearch *)inData;
			DBGLOG5( kLogHandler, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u  : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoAttributeValueSearchWithData:
		{
			sDoAttrValueSearchWithData *p = (sDoAttrValueSearchWithData *)inData;
			DBGLOG5( kLogHandler, "%s : DAR : Node Ref = %u : Number of Found Records = %u : Continue Data = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData, inResult );
			break;
		}

		case kDoDirNodeAuth:
		{
			sDoDirNodeAuth *p = (sDoDirNodeAuth *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
			break;
		}
		
		case kDoDirNodeAuthOnRecordType:
		{
			sDoDirNodeAuthOnRecordType *p = (sDoDirNodeAuthOnRecordType *)inData;
			DBGLOG3( kLogHandler, "%s : DAR : Node Ref = %u : Result code = %d", inDebugDataTag, p->fInNodeRef, inResult );
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
				DBGLOG3( kLogHandler, "%s : DAC : Dir Ref = %u : Node Name = %s", inDebugDataTag, p->fInDirRef, aNodeName );
				free(aNodeName);
				aNodeName = nil;
			}
			break;
		}

		case kCloseDirNode:
		{
			sCloseDirNode *p = (sCloseDirNode *)inData;
			DBGLOG2( kLogHandler, "%s : DAC : Node Ref = %u", inDebugDataTag, p->fInNodeRef );
			break;
		}

		case kOpenRecord:
		{
			sOpenRecord *p = (sOpenRecord *)inData;
			if ( (p->fInRecType != nil) && (p->fInRecName != nil) && (p->fInRecType->fBufferData != nil) && (p->fInRecName->fBufferData != nil) )
			{
				DBGLOG4( kLogHandler, "%s : DAC : Node Ref = %u : Rec Type = %s : Rec Name = %s", inDebugDataTag, p->fInNodeRef, p->fInRecType->fBufferData,  p->fInRecName->fBufferData );
			}
			break;
		}

		case kCreateRecord:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			if ( (p->fInRecType != nil) && (p->fInRecName != nil) && (p->fInRecType->fBufferData != nil) && (p->fInRecName->fBufferData != nil) )
			{
				DBGLOG4( kLogHandler, "%s : DAC : Node Ref = %u : Rec Type = %s : Rec Name = %s", inDebugDataTag, p->fInNodeRef, p->fInRecType->fBufferData,  p->fInRecName->fBufferData );
			}
			break;
		}

		case kCreateRecordAndOpen:
		{
			sCreateRecord *p = (sCreateRecord *)inData;
			if ( (p->fInRecType != nil) && (p->fInRecName != nil) && (p->fInRecType->fBufferData != nil) && (p->fInRecName->fBufferData != nil) )
			{
				DBGLOG5( kLogHandler, "%s : DAC : Node Ref = %u : Rec Type = %s : Rec Name = %s : Open Rec Flag = %u", inDebugDataTag, p->fInNodeRef, p->fInRecType->fBufferData,  p->fInRecName->fBufferData, p->fInOpen );
			}
			break;
		}

		case kCloseRecord:
		{
			sCloseRecord *p = (sCloseRecord *)inData;
			DBGLOG2( kLogHandler, "%s : DAC : Rec Ref = %u", inDebugDataTag, p->fInRecRef );
			break;
		}
		
		case kDeleteRecord:
		{
			sDeleteRecord *p = (sDeleteRecord *)inData;
			DBGLOG2( kLogHandler, "%s : DAC : Rec Ref = %u", inDebugDataTag, p->fInRecRef );
			break;
		}
		
		case kGetRecordReferenceInfo:
		{
			sGetRecRefInfo *p = (sGetRecRefInfo *)inData;
			DBGLOG2( kLogHandler, "%s : DAC : Rec Ref = %u", inDebugDataTag, p->fInRecRef );
			break;
		}

		case kGetRecordAttributeInfo:
		{
			sGetRecAttribInfo *p = (sGetRecAttribInfo *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DBGLOG3( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData );
			}
			break;
		}

		case kGetRecordAttributeValueByID:
		{
			sGetRecordAttributeValueByID *p = (sGetRecordAttributeValueByID *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DBGLOG4( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr ID = %u", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData, p->fInValueID );
			}
			break;
		}

		case kGetRecordAttributeValueByIndex:
		{
			sGetRecordAttributeValueByIndex *p = (sGetRecordAttributeValueByIndex *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DBGLOG4( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr Index = %u", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData, p->fInAttrValueIndex );
			}
			break;
		}

		case kGetDirNodeInfo:
		{
			sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;
			char* requestedAttrs = dsGetPathFromListPriv( p->fInDirNodeInfoTypeList, (const char *)";" );
			if (requestedAttrs != nil)
			{
				DBGLOG4( kLogHandler, "%s : DAC : Node Ref = %u : Requested Attrs = %s : Attr Type Only Flag = %u", inDebugDataTag, p->fInNodeRef, requestedAttrs+1, p->fInAttrInfoOnly );
				free(requestedAttrs);
				requestedAttrs = nil;
			}
			break;
		}

		case kSetRecordName:
		{
			sSetRecordName *p = (sSetRecordName *)inData;
			if ( (p->fInNewRecName != nil)  && (p->fInNewRecName->fBufferData != nil) )
			{
				DBGLOG3( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInNewRecName->fBufferData );
			}
			break;
		}

		case kAddAttribute:
		{
			sAddAttribute *p = (sAddAttribute *)inData;
			if ( (p->fInNewAttr != nil) && (p->fInFirstAttrValue != nil) && (p->fInNewAttr->fBufferData != nil) && (p->fInFirstAttrValue->fBufferData != nil) )
			{
				DBGLOG4( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr Value = %s", inDebugDataTag, p->fInRecRef, p->fInNewAttr->fBufferData,  p->fInFirstAttrValue->fBufferData );
			}
			break;
		}

		case kRemoveAttribute:
		{
			sRemoveAttribute *p = (sRemoveAttribute *)inData;
			if ( (p->fInAttribute != nil)  && (p->fInAttribute->fBufferData != nil) )
			{
				DBGLOG3( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInAttribute->fBufferData );
			}
			break;
		}

		case kAddAttributeValue:
		{
			sAddAttributeValue *p = (sAddAttributeValue *)inData;
			if ( (p->fInAttrType != nil) && (p->fInAttrValue != nil) && (p->fInAttrType->fBufferData != nil) && (p->fInAttrValue->fBufferData != nil) )
			{
				DBGLOG4( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr Value = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData,  p->fInAttrValue->fBufferData );
			}
			break;
		}

		case kRemoveAttributeValue:
		{
			sRemoveAttributeValue *p = (sRemoveAttributeValue *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DBGLOG4( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s : Attr ID = %u", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData, p->fInAttrValueID );
			}
			break;
		}

		case kSetAttributeValue:
		{
			sSetAttributeValue *p = (sSetAttributeValue *)inData;
			if ( (p->fInAttrType != nil)  && (p->fInAttrType->fBufferData != nil) )
			{
				DBGLOG3( kLogHandler, "%s : DAC : Rec Ref = %u : Attr Type = %s", inDebugDataTag, p->fInRecRef, p->fInAttrType->fBufferData );
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
				DBGLOG5( kLogHandler, "%s : DAC : 1 : Node Ref = %u : Requested Rec Names = %s : Rec Name Pattern Match = %u : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, requestedRecNames+1, p->fInPatternMatch, requestedRecTypes+1 );
				DBGLOG6( kLogHandler, "%s : DAC : 2 : Node Ref = %u : Requested Attrs = %s : Attr Type Only Flag = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, requestedAttrs+1, p->fInAttribInfoOnly, p->fOutRecEntryCount, p->fIOContinueData );
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
				DBGLOG6( kLogHandler, "%s : DAC : 1 : Node Ref = %u : Requested Attr Type = %s : Attr Match String = %s : Attr Pattern Match = %u : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, p->fInAttrType->fBufferData, p->fInPatt2Match->fBufferData, p->fInPattMatchType, requestedRecTypes+1 );
				DBGLOG4( kLogHandler, "%s : DAC : 2 : Node Ref = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, p->fOutMatchRecordCount, p->fIOContinueData );
				free(requestedRecTypes);
				requestedRecTypes = nil;
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
				DBGLOG6( kLogHandler, "%s : DAC : 1 : Node Ref = %u : Requested Attr Type = %s : Attr Match String = %s : Attr Pattern Match = %u : Requested Rec Types = %s", inDebugDataTag, p->fInNodeRef, p->fInAttrType->fBufferData, p->fInPatt2Match->fBufferData, p->fInPattMatchType, requestedRecTypes+1 );
				DBGLOG6( kLogHandler, "%s : DAC : 2 : Node Ref = %u : Requested Attrs = %s : Attr Type Only Flag = %u : Record Count Limit = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, requestedAttrs+1, p->fInAttrInfoOnly, p->fOutMatchRecordCount, p->fIOContinueData );
				free(requestedRecTypes);
				requestedRecTypes = nil;
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
				DBGLOG6( kLogHandler, "%s : DAC : Node Ref = %u : User Name = %s : Auth Method = %s : Auth Only Flag = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, userName, p->fInAuthMethod->fBufferData, p->fInDirNodeAuthOnlyFlag, p->fIOContinueData );
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
				DBGLOG6( kLogHandler, "%s : DAC : 1 : Node Ref = %u : User Name = %s : Auth Method = %s : Auth Only Flag = %u : Continue Data = %u", inDebugDataTag, p->fInNodeRef, userName, p->fInAuthMethod->fBufferData, p->fInDirNodeAuthOnlyFlag, p->fIOContinueData );
				DBGLOG3( kLogHandler, "%s : DAC : 2 : Node Ref = %u : Record Type = %s", inDebugDataTag, p->fInNodeRef, p->fInRecordType->fBufferData );
				free(userName);
				userName = nil;
			}
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

sInt32 CRequestHandler::PackageReply ( void *inData, sComData **inMsg )
{
	sInt32			siResult	= -8088;
	uInt32			uiMsgType	= 0;
	sHeader		   *pMsgHdr		= nil;
	CSrvrMessaging		cMsg;
	sInt32			aClientPID	= -1;
	uInt32			anIPAddress	= 0;
	uInt32			aContinue	= 0;
	sInt32			aContextReq	= eDSNoErr;

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
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );
				}

				break;
			}

			case kDoAttributeValueSearch:
			{
				sDoAttrValueSearch *p = (sDoAttrValueSearch *)inData;

				// Add the record count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutMatchRecordCount, kMatchRecCount );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 2 );
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
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 2 );
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
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );

				if (aContextReq == eDSNoErr)
				{
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 2 );
				}
				siResult = eDSNoErr;

				break;
			}

			case kFindDirNodes:
			{
				sFindDirNodes *p = (sFindDirNodes *)inData;

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the node reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutDirNodeCount, kNodeCount );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				if (aContextReq == eDSNoErr)
				{
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fOutContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 2 );
				}
				siResult = eDSNoErr;

				break;
			}

			case kOpenDirNode:
			{
				sOpenDirNode *p = (sOpenDirNode *)inData;

				// Add the node reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutNodeRef, ktNodeRef );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				break;
			}

			case kGetDirNodeInfo:
			{
				sGetDirNodeInfo *p = (sGetDirNodeInfo *)inData;

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the attribute info count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutAttrInfoCount, kAttrInfoCount );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );

				// Add the attribute list reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutAttrListRef, ktAttrListRef );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 2 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fOutContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 3 );
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

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the record entry count
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutRecEntryCount, kAttrRecEntryCount );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 2 );
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
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the record entry
				siResult = cMsg.Add_tRecordEntry_ToMsg( inMsg, p->fOutRecEntryPtr );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );

				break;
			}

			case kGetAttributeEntry:
			{
				sGetAttributeEntry *p = (sGetAttributeEntry *)inData;

				// Add the attribute value list reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutAttrValueListRef, ktAttrValueListRef );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the attribute info
				siResult = cMsg.Add_tAttrEntry_ToMsg( inMsg, p->fOutAttrInfoPtr );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );

				break;
			}

			case kGetAttributeValue:
			{
				sGetAttributeValue *p = (sGetAttributeValue *)inData;

				// Add the attribute value
				siResult = cMsg.Add_tAttrValueEntry_ToMsg( inMsg, p->fOutAttrValue );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				// Add the data buffer
				siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fInOutDataBuff, ktDataBuff );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				break;
			}

			case kOpenRecord:
			{
				sOpenRecord *p = (sOpenRecord *)inData;

				// Add the recrod reference
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutRecRef, ktRecRef );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

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
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				break;
			}

			case kGetRecordAttributeInfo:
			{
				sGetRecAttribInfo *p = (sGetRecAttribInfo *)inData;

				// Add the attribute info
				siResult = cMsg.Add_tAttrEntry_ToMsg( inMsg, p->fOutAttrInfoPtr );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				break;
			}

			case kGetRecordAttributeValueByID:
			{
				sGetRecordAttributeValueByID *p = (sGetRecordAttributeValueByID *)inData;

				// Add the attribute info
				siResult = cMsg.Add_tAttrValueEntry_ToMsg( inMsg, p->fOutEntryPtr );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				break;
			}

			case kGetRecordAttributeValueByIndex:
			{
				sGetRecordAttributeValueByIndex *p = (sGetRecordAttributeValueByIndex *)inData;

				// Add the attribute info
				siResult = cMsg.Add_tAttrValueEntry_ToMsg( inMsg, p->fOutEntryPtr );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				break;
			}

			case kCreateRecordAndOpen:
			{
				sCreateRecord *p = (sCreateRecord *)inData;

				// Add the attribute info
				siResult = cMsg.Add_Value_ToMsg( (*inMsg), p->fOutRecRef, ktRecRef );
				if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );

				break;
			}

			case kDoDirNodeAuth:
			{
				sDoDirNodeAuth *p = (sDoDirNodeAuth *)inData;

				if ( p->fOutAuthStepDataResponse != nil )
				{
					// Add the record entry count
					siResult = cMsg.Add_tDataBuff_ToMsg( inMsg, p->fOutAuthStepDataResponse, kAuthStepDataResponse );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );
				}

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );
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
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError );
				}

				if ( aContextReq == eDSNoErr )
				{
					// Add the context data
					siResult = cMsg.Add_Value_ToMsg( (*inMsg), (uInt32)p->fIOContinueData, kContextData );
					if ( siResult != eDSNoErr ) throw( (sInt32)eServerSendError - 1 );
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
	}

	catch( sInt32 err )
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
				::dsDataBufferDeallocatePriv( p->fInAuthStepData );
				p->fInAuthStepData = nil;
			}

			if ( p->fOutAuthStepDataResponse != nil )
			{
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
				::dsDataBufferDeallocatePriv( p->fInAuthStepData );
				p->fInAuthStepData = nil;
			}

			if ( p->fOutAuthStepDataResponse != nil )
			{
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

uInt32 CRequestHandler::GetMsgType ( sComData *inMsg )
{
	return( inMsg->type.msgt_name );
} // GetMsgType


//--------------------------------------------------------------------------------------------------
//	* DoReleaseContinueData()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoReleaseContinueData ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32					siResult	= eServerReceiveError;
	sReleaseContinueData   *p			= nil;
	CSrvrMessaging			cMsg;
	sInt32					aClientPID	= -1;
	uInt32					anIPAddress	= 0;

	try
	{
		p = (sReleaseContinueData *)::calloc( sizeof( sReleaseContinueData ), sizeof( char ) );
		if ( p != nil )
		{
			p->fType = GetMsgType( inMsg );

			aClientPID	= inMsg->fPID;
			anIPAddress	= inMsg->fIPAddress;

			siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirReference, ktDirRef );
			if ( siResult == eDSNoErr )
			{
				// Verify the Directory Reference
				siResult = CRefTable::VerifyNodeRef( p->fInDirReference, &fPluginPtr, aClientPID, anIPAddress );
				if ( siResult != eDSNoErr )
				{
					siResult = CRefTable::VerifyDirRef( p->fInDirReference, &fPluginPtr, aClientPID, anIPAddress );
				}
				if ( siResult == eDSNoErr )
				{
					if ( fPluginPtr == nil )
					{
						// weird problem if we make it here
						free( p );
						p = nil;
						*outStatus = -1212;
					}
					else
					{
						*outStatus = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fInContinueData, kContextData );
					}
				}
			}
		}
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoFlushRecord ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sFlushRecord	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sFlushRecord *) ::calloc(sizeof(sFlushRecord), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoPlugInCustomCall ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32					siResult	= -8088;
	sDoPlugInCustomCall	   *p			= nil;
	uInt32					uiBuffSize	= 0;
	CSrvrMessaging			cMsg;
	sInt32					aClientPID	= -1;
	uInt32					anIPAddress	= 0;

	try
	{
		p = (sDoPlugInCustomCall *) ::calloc(sizeof(sDoPlugInCustomCall), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRequestCode, kCustomRequestCode );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRequestData, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		p->fOutRequestResponse = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutRequestResponse == nil ) throw( (sInt32)eServerReceiveError - 3 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoAttributeValueSearch ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult		= -8088;
	sDoAttrValueSearch *p				= nil;
	uInt32				uiBuffSize		= 0;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID		= -1;
	uInt32				anIPAddress		= 0;

	try
	{
		p = (sDoAttrValueSearch *) ::calloc(sizeof(sDoAttrValueSearch), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (sInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrType );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fInPattMatchType, kAttrPattMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInPatt2Match, kAttrMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 6 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fOutMatchRecordCount, kMatchRecCount );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 7 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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
//	* DoAttributeValueSearchWithData()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAttributeValueSearchWithData ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32						siResult		= -8088;
	sDoAttrValueSearchWithData *p				= nil;
	uInt32						uiBuffSize		= 0;
	CSrvrMessaging				cMsg;
	sInt32						aClientPID		= -1;
	uInt32						anIPAddress		= 0;
	uInt32						aBoolValue		= 0;

	try
	{
		p = (sDoAttrValueSearchWithData *) ::calloc(sizeof(sDoAttrValueSearchWithData), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

//		DBGLOG3( kLogEndpoint, "Error: File: %s. Line: %d:\n", __FILE__, __LINE__ );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (sInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrType );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fInPattMatchType, kAttrPattMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInPatt2Match, kAttrMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 6 );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInAttrTypeRequestList, kAttrTypeRequestList );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 7 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAttrInfoOnly );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 8 );
		p->fInAttrInfoOnly = (bool)aBoolValue;

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fOutMatchRecordCount, kMatchRecCount );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 9 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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
//	* DoOpenDirNode()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoOpenDirNode ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32			siResult		= eDSNoErr;
	sOpenDirNode   *p				= nil;
	CSrvrMessaging	cMsg;
	sInt32			aClientPID		= -1;
	uInt32			anIPAddress		= 0;

	try
	{
		p = (sOpenDirNode *) ::calloc(sizeof(sOpenDirNode), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;
		
		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirRef, ktDirRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the directory reference
		siResult = CRefTable::VerifyDirRef( p->fInDirRef, nil, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInDirNodeName, kDirNodeName );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		if ( anIPAddress == 0 )
		{
			GetUIDsForProcessID( aClientPID, &p->fInUID, &p->fInEffectiveUID );
		}
		else
		{
			// treat remote as untrusted
			p->fInUID = (uid_t)-2;
			p->fInEffectiveUID = (uid_t)-2;
		}
		if ( gNodeList != nil )
		{
			char *pNodeName = nil;

			pNodeName = ::dsGetPathFromListPriv( p->fInDirNodeName, gNodeList->GetDelimiter() );
			if ( pNodeName != nil )
			{
				//wait on ALL calls if local node is not yet available
				if ( strcmp(pNodeName, "~NetInfo~DefaultLocalNode") == 0 )
				{
					gNodeList->WaitForLocalNode();
				}
				
				//wait on ALL calls if configure node is not yet available
				if ( strcmp(pNodeName, "~Configure") == 0 )
				{
					gNodeList->WaitForConfigureNode();
				}
				
				//wait on ALL calls if DHCPLDAPv3 node is not yet ascertained
				if ( strcmp(pNodeName, "~LDAPv3") == 0 )
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

		siResult = CRefTable::NewNodeRef( &p->fOutNodeRef, fPluginPtr, p->fInDirRef, inMsg->fPID, inMsg->fIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoCloseDirNode ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32			siResult		= -8088;
	sCloseDirNode  *p				= nil;
	CSrvrMessaging	cMsg;
	sInt32			aClientPID		= -1;
	uInt32			anIPAddress		= 0;
	
	try
	{
		p = (sCloseDirNode *) ::calloc(sizeof(sCloseDirNode), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

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

	catch( sInt32 err )
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

void* CRequestHandler::DoGetDirNodeInfo ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult		= -8088;
	sGetDirNodeInfo	   *p				= nil;
	uInt32				uiBuffSize		= 0;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID		= -1;
	uInt32				anIPAddress		= 0;
	uInt32				aBoolValue		= 0;

	try
	{
		p = (sGetDirNodeInfo *) ::calloc(sizeof(sGetDirNodeInfo), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInDirNodeInfoTypeList, kNodeInfoTypeList );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (sInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAttrInfoOnly );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 4 );
		p->fInAttrInfoOnly = (bool)aBoolValue;

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fOutContinueData, kContextData );
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

	catch( sInt32 err )
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

void* CRequestHandler::DoGetRecordList ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	uInt32				uiBuffSize	= 0;
	sGetRecordList	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;
	uInt32				aBoolValue	= 0;

	try
	{
		p = (sGetRecordList *)::calloc( sizeof( sGetRecordList ), sizeof( char ) );
		if ( p == nil ) throw( (sInt32)eMemoryError );

		if ( p != nil )
		{
			p->fType = GetMsgType( inMsg );

			aClientPID	= inMsg->fPID;
			anIPAddress	= inMsg->fIPAddress;
			
			// Get the node ref
			siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
			if ( siResult == eDSNoErr )
			{
				// Verify the node reference
				siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
				if ( siResult == eDSNoErr )
				{
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
					}
					else
					{
						//old client where empty data buffer is being sent
						siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInDataBuff, ktDataBuff );
					}
					if (siResult == eDSNoErr)
					{
						siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecNameList, kRecNameList );
						if ( siResult == eDSNoErr )
						{
							siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fInPatternMatch, ktDirPattMatch );
							if ( siResult == eDSNoErr )
							{
								siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInRecTypeList, kRecTypeList );
								if ( siResult == eDSNoErr )
								{
									siResult = cMsg.Get_tDataList_FromMsg( inMsg, &p->fInAttribTypeList, kAttrTypeList );
									if ( siResult == eDSNoErr )
									{
										siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAttrInfoOnly );
										p->fInAttribInfoOnly = (bool)aBoolValue;
										if ( siResult == eDSNoErr )
										{
											siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fOutRecEntryCount, kAttrRecEntryCount );
											if ( siResult == eDSNoErr )
											{
												siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fIOContinueData, kContextData );
												if ( siResult != eDSNoErr )
												{
													p->fIOContinueData = nil;
													siResult = eDSNoErr;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		*outStatus = siResult;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoGetRecordEntry ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult		= -8088;
	sGetRecordEntry	   *p				= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID		= -1;
	uInt32				anIPAddress		= 0;

	try
	{
		p = (sGetRecordEntry *) ::calloc(sizeof(sGetRecordEntry), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Make sure that this is a valid node reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInOutDataBuff, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecEntryIndex, kRecEntryIndex );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		// Create a new attribute list reference
		siResult = CRefTable::NewAttrListRef( &p->fOutAttrListRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoGetAttributeEntry ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult		= -8088;
	sGetAttributeEntry *p				= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID		= -1;
	uInt32				anIPAddress		= 0;

	try
	{
		p = (sGetAttributeEntry *) ::calloc(sizeof(sGetAttributeEntry), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInOutDataBuff, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrListRef, ktAttrListRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		// Verify the Attribute List Reference
		siResult = CRefTable::VerifyAttrListRef( p->fInAttrListRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrInfoIndex, kAttrInfoIndex );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		// Create a new Attribute Value List Reference
		siResult = CRefTable::NewAttrValueRef( &p->fOutAttrValueListRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoGetAttributeValue ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sGetAttributeValue *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sGetAttributeValue *) ::calloc(sizeof(sGetAttributeValue), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInOutDataBuff, ktDataBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueIndex, kAttrValueIndex );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueListRef, ktAttrValueListRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		// Verify the Attribute Value List Reference
		siResult = CRefTable::VerifyAttrValueRef( p->fInAttrValueListRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler:: DoCloseAttributeList ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32					siResult		= -8088;
	sCloseAttributeList	   *p				= nil;
	CSrvrMessaging			cMsg;
	sInt32					aClientPID		= -1;
	uInt32					anIPAddress		= 0;

	try
	{
		p = (sCloseAttributeList *) ::calloc(sizeof(sCloseAttributeList), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttributeListRef, ktAttrListRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

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

	catch( sInt32 err )
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

void* CRequestHandler:: DoCloseAttributeValueList ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32						siResult		= -8088;
	sCloseAttributeValueList   *p				= nil;
	CSrvrMessaging				cMsg;
	sInt32						aClientPID		= -1;
	uInt32						anIPAddress		= 0;

	try
	{
		p = (sCloseAttributeValueList *) ::calloc(sizeof(sCloseAttributeValueList), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttributeValueListRef, ktAttrValueListRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

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

	catch( sInt32 err )
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

void* CRequestHandler::DoOpenRecord ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult		= -8088;
	sOpenRecord		   *p				= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID		= -1;
	uInt32				anIPAddress		= 0;

	try
	{
		p = (sOpenRecord *) ::calloc(sizeof(sOpenRecord), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecName, kRecNameBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		// Create a new Attribute List Reference
		siResult = CRefTable::NewRecordRef( &p->fOutRecRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoGetRecRefInfo ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sGetRecRefInfo	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sGetRecRefInfo *) ::calloc(sizeof(sGetRecRefInfo), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;
		
		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoGetRecAttribInfo ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sGetRecAttribInfo  *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sGetRecAttribInfo *) ::calloc(sizeof(sGetRecAttribInfo), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoGetRecordAttributeValueByID ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32							siResult	= -8088;
	sGetRecordAttributeValueByID   *p			= nil;
	CSrvrMessaging					cMsg;
	sInt32							aClientPID	= -1;
	uInt32							anIPAddress	= 0;

	try
	{
		p = (sGetRecordAttributeValueByID *) ::calloc(sizeof(sGetRecordAttributeValueByID), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInValueID, kAttrValueID );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoGetRecordAttributeValueByIndex ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32								siResult	= -8088;
	sGetRecordAttributeValueByIndex	   *p			= nil;
	CSrvrMessaging						cMsg;
	sInt32								aClientPID	= -1;
	uInt32								anIPAddress	= 0;

	try
	{
		p = (sGetRecordAttributeValueByIndex *) ::calloc(sizeof(sGetRecordAttributeValueByIndex), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueIndex, kAttrValueIndex );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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
//	* DoCloseRecord()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoCloseRecord ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult		= -8088;
	sCloseRecord	   *p				= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID		= -1;
	uInt32				anIPAddress		= 0;

	try
	{
		p = (sCloseRecord *) ::calloc(sizeof(sCloseRecord), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

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

	catch( sInt32 err )
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

void* CRequestHandler::DoSetRecordName ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sSetRecordName	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sSetRecordName *) ::calloc(sizeof(sSetRecordName), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInNewRecName, kRecNameBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoSetRecordType ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sSetRecordType	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sSetRecordType *) ::calloc(sizeof(sSetRecordType), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInNewRecType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoDeleteRecord ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult		= -8088;
	sDeleteRecord	   *p				= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID		= -1;
	uInt32				anIPAddress		= 0;

	try
	{
		p = (sDeleteRecord *) ::calloc(sizeof(sDeleteRecord), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		//KW we need to remove the reference from the table AFTER the plugin processes the delete

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoCreateRecord ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sCreateRecord	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;
	uInt32				aBoolValue	= 0;

	try
	{
		p = (sCreateRecord *) ::calloc(sizeof(sCreateRecord), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecName, kRecNameBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kOpenRecBool );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		p->fInOpen = (bool)aBoolValue;
		if ( p->fInOpen == true )
		{
			siResult = CRefTable::NewRecordRef( &p->fOutRecRef, fPluginPtr, p->fInNodeRef, aClientPID, anIPAddress );
			if ( siResult != eDSNoErr ) throw( siResult );
		}

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoAddAttribute ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sAddAttribute	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sAddAttribute *) ::calloc(sizeof(sAddAttribute), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInNewAttr, kNewAttrBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInFirstAttrValue, kFirstAttrBuff );
		//allow no intial value to be added
		//if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoRemoveAttribute ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sRemoveAttribute   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sRemoveAttribute *) ::calloc(sizeof(sRemoveAttribute), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttribute, kAttrBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoAddAttributeValue ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sAddAttributeValue *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sAddAttributeValue *) ::calloc(sizeof(sAddAttributeValue), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrValue, kAttrValueBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoRemoveAttributeValue ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32					siResult	= -8088;
	sRemoveAttributeValue   *p			= nil;
	CSrvrMessaging			cMsg;
	sInt32					aClientPID	= -1;
	uInt32					anIPAddress	= 0;

	try
	{
		p = (sRemoveAttributeValue *) ::calloc(sizeof(sRemoveAttributeValue), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInAttrValueID, kAttrValueID );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoSetAttributeValue ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sSetAttributeValue *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sSetAttributeValue *) ::calloc(sizeof(sSetAttributeValue), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInRecRef, ktRecRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Record Reference
		siResult = CRefTable::VerifyRecordRef( p->fInRecRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAttrType, kAttrTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_tAttrValueEntry_FromMsg( inMsg, &p->fInAttrValueEntry, ktAttrValueEntry );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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
//	* DoAuthentication()
//
//--------------------------------------------------------------------------------------------------

void* CRequestHandler::DoAuthentication ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sDoDirNodeAuth	   *p			= nil;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;
	uInt32				aBoolValue	= 0;

	try
	{
		p = (sDoDirNodeAuth *) ::calloc(sizeof(sDoDirNodeAuth), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthMethod, kAuthMethod );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAuthOnlyBool );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );
		p->fInDirNodeAuthOnlyFlag = (bool)aBoolValue;

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthStepData, kAuthStepBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fOutAuthStepDataResponse, kAuthResponseBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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

void* CRequestHandler::DoAuthenticationOnRecordType ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32							siResult	= -8088;
	sDoDirNodeAuthOnRecordType	   *p			= nil;
	CSrvrMessaging					cMsg;
	sInt32							aClientPID	= -1;
	uInt32							anIPAddress	= 0;
	uInt32							aBoolValue	= 0;

	try
	{
		p = (sDoDirNodeAuthOnRecordType *) ::calloc(sizeof(sDoDirNodeAuthOnRecordType), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInNodeRef, ktNodeRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Node Reference
		siResult = CRefTable::VerifyNodeRef( p->fInNodeRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthMethod, kAuthMethod );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &aBoolValue, kAuthOnlyBool );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );
		p->fInDirNodeAuthOnlyFlag = (bool)aBoolValue;

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInAuthStepData, kAuthStepBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fOutAuthStepDataResponse, kAuthResponseBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 4 );

		siResult = cMsg.Get_tDataBuff_FromMsg( inMsg, &p->fInRecordType, kRecTypeBuff );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 5 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fIOContinueData, kContextData );
		if ( siResult != eDSNoErr )
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		*outStatus = eDSNoErr;
	}

	catch( sInt32 err )
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
  
void* CRequestHandler::FindDirNodes ( sComData *inMsg, sInt32 *outStatus, char *inDebugDataTag )
{
	sInt32					siResult	= -8088;
	sFindDirNodes		   *p			= nil;
	uInt32					uiBuffSize	= 0;
	CSrvrMessaging			cMsg;
	char				   *nodeName	= nil;
	sInt32					aClientPID	= -1;
	uInt32					anIPAddress	= 0;

	try
	{
		p = (sFindDirNodes *)::calloc(sizeof(sFindDirNodes), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirRef, ktDirRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Directory Reference
		siResult = CRefTable::VerifyDirRef( p->fInDirRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (sInt32)eServerReceiveError - 2 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fInPatternMatchType, ktDirPattMatch );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 3 );

		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fOutContinueData, kContextData );
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
			if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 2 );

			nodeName = ::dsGetPathFromListPriv( p->fInNodeNamePattern, gNodeList->GetDelimiter() );
			if ( (nodeName != nil) && (::strlen( nodeName ) > 1) )
			{
	
				DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, nodeName );
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
						DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrDefaultLocalNodeName );
//PFIX						if (gLocalNodeNotAvailable)
//PFIX						{
//PFIX							throw((sInt32)eDSNodeNotFound);
//PFIX						}
						break;
					
					case eDSAuthenticationSearchNodeName:
						DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrAuthenticationNodeName );
//PFIX						if (gLocalNodeNotAvailable)
//PFIX						{
//PFIX							throw((sInt32)eDSNodeNotFound);
//PFIX						}
						break;
					
					case eDSContactsSearchNodeName:
						DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrContactsNodeName );
//PFIX						if (gLocalNodeNotAvailable)
//PFIX						{
//PFIX							throw((sInt32)eDSNodeNotFound);
//PFIX						}
						break;
					
					case eDSNetworkSearchNodeName:
						DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, kstrNetworkNodeName );
//PFIX						if (gLocalNodeNotAvailable)
//PFIX						{
//PFIX							throw((sInt32)eDSNodeNotFound);
//PFIX						}
						break;
					
					case eDSLocalHostedNodes:
						DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, "LocalHostedNodes" );
//PFIX						if (gLocalNodeNotAvailable)
//PFIX						{
//PFIX							throw((sInt32)eDSNodeNotFound);
//PFIX						}
						break;
					
					case eDSDefaultNetworkNodes:
						DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, "DefaultNetworkNodes" );
						break;
					
					case eDSConfigNodeName:
						DBGLOG3( kLogHandler, "%s : DAR : 1 : Dir Ref = %u : Requested nodename = %s", inDebugDataTag, p->fInDirRef, "ConfigNode" );
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

	catch( sInt32 err )
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

void* CRequestHandler::GetNodeList ( sComData *inMsg, sInt32 *outStatus )
{
	sInt32				siResult	= -8088;
	sGetDirNodeList	   *p			= nil;
	uInt32				uiBuffSize	= 0;
	CSrvrMessaging		cMsg;
	sInt32				aClientPID	= -1;
	uInt32				anIPAddress	= 0;

	try
	{
		p = (sGetDirNodeList *)::calloc(sizeof(sGetDirNodeList), sizeof(char));
		if ( p == nil ) throw( (sInt32)eMemoryError );

		p->fType = GetMsgType( inMsg );

		aClientPID	= inMsg->fPID;
		anIPAddress	= inMsg->fIPAddress;

		siResult = cMsg.Get_Value_FromMsg( inMsg, &p->fInDirRef, ktDirRef );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError );

		// Verify the Directory Reference
		siResult = CRefTable::VerifyDirRef( p->fInDirRef, &fPluginPtr, aClientPID, anIPAddress );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = cMsg.Get_Value_FromMsg( inMsg, &uiBuffSize, kOutBuffLen );
		if ( siResult != eDSNoErr ) throw( (sInt32)eServerReceiveError - 1 );

		p->fOutDataBuff = ::dsDataBufferAllocatePriv( uiBuffSize );
		if ( p->fOutDataBuff == nil ) throw( (sInt32)eServerReceiveError - 3 );
		
		siResult = cMsg.Get_Value_FromMsg( inMsg, (uInt32 *)&p->fIOContinueData, kContextData );
		if (siResult != eDSNoErr)
		{
			p->fIOContinueData = nil;
			siResult = eDSNoErr;
		}

		//currently not using the continue data

		*outStatus = gNodeList->BuildNodeListBuff( p );
	}

	catch( sInt32 err )
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

sInt32 CRequestHandler::DoCheckUserNameAndPassword (	const char *userName,
														const char *password,
														tDirPatternMatch inPatternMatch,
														uid_t *outUID,
														char **outShortName )
{
	sInt32					siResult		= eDSNoErr;
	sInt32					returnVal		= -3;
	unsigned long			nodeCount		= 0;
	unsigned long			recCount		= 0;
	unsigned long			length			= 0;
	char*					ptr				= NULL;
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
	
	//included definition of the returnVal possibilities from the checkpw.h file in the security FW
	//enum {
		//CHECKPW_SUCCESS = 0,
		//CHECKPW_UNKNOWNUSER = -1,
		//CHECKPW_BADPASSWORD = -2,
		//CHECKPW_FAILURE = -3
	//};
	
	try
	{
		if (inPatternMatch == eDSExact) //true checkpw call
		{
			aDSRef = gCheckPasswordDSRef;
			aSearchNodeRef = gCheckPasswordSearchNodeRef;
		}
		//KW27 syslog(LOG_INFO,"chkpasswd username %s\n", userName );
		if ( aDSRef == 0 )
		{
			//dsFindDirNodes below already blocks on the search node
			// we know we need the search node, so let's wait until it is available.
			//gNodeList->WaitForAuthenticationSearchNode();
			
			siResult = dsOpenDirService( &aDSRef );
			if ( siResult != eDSNoErr ) throw( -3 );
	
			dataBuff = dsDataBufferAllocate( aDSRef, 2048 );
			if ( dataBuff == nil ) throw( -3 );
			
			siResult = dsFindDirNodes( aDSRef, dataBuff, nil, 
										eDSAuthenticationSearchNodeName, &nodeCount, &context );
			if ( siResult != eDSNoErr ) throw( -3 );
			if ( nodeCount < 1 ) throw( -3 );
	
			siResult = dsGetDirNodeName( aDSRef, dataBuff, 1, &nodeName );
			if ( siResult != eDSNoErr ) throw( -3 );
	
			siResult = dsOpenDirNode( aDSRef, nodeName, &aSearchNodeRef );
			if ( siResult != eDSNoErr ) throw( -3 );
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
				uInt32 bufSize = dataBuff->fBufferSize;
				dsDataBufferDeallocatePriv( dataBuff );
				dataBuff = nil;
				dataBuff = ::dsDataBufferAllocate( aDSRef, bufSize * 2 );
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
							authName = (char*)calloc( 1, pValueEntry->fAttributeValueData.fBufferLength + 1 );
							strncpy( authName, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
						}
						else if ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrUniqueID ) == 0 )
						{
							uidString = (char*)calloc( 1, pValueEntry->fAttributeValueData.fBufferLength + 1 );
							strncpy( uidString, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
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
			returnVal = -1;
		}
		
		if ( (nodeName != NULL) && (authName != NULL) )
		{
			siResult = dsOpenDirNode( aDSRef, nodeName, &nodeRef );
			if ( siResult != eDSNoErr ) throw( -3 );
			
			authMethod = dsDataNodeAllocateString( aDSRef, kDSStdAuthNodeNativeClearTextOK );
			authBuff = dsDataBufferAllocate( aDSRef, strlen( authName ) + strlen( password ) + 10 );
			// 4 byte length + username + null byte + 4 byte length + password + null byte
			if ( authBuff == nil ) throw( -3 );
			length = strlen( authName ) + 1;
			ptr = authBuff->fBufferData;
			::memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			::memcpy( ptr, authName, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			length = strlen( password ) + 1;
			::memcpy( ptr, &length, 4 );
			ptr += 4;
			authBuff->fBufferLength += 4;
			
			::memcpy( ptr, password, length );
			ptr += length;
			authBuff->fBufferLength += length;
			
			siResult = dsDoDirNodeAuth( nodeRef, authMethod, true, authBuff, dataBuff, NULL );
			if (gLogAPICalls)
			{
				syslog(LOG_INFO,"checkpw: dsDoDirNodeAuth returned %d", siResult);//KW27 
			}
			
			switch (siResult)
			{
				case eDSNoErr:
					returnVal = 0;
					break;
				case eDSAuthNewPasswordRequired:
				case eDSAuthPasswordExpired:
				case eDSAuthAccountInactive:
				case eDSAuthAccountExpired:
				case eDSAuthAccountDisabled:
					returnVal = -4;
					break;
				default:
					returnVal = -2;
					break;
			}
		}
	}
	
	catch( sInt32 err )
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


bool CRequestHandler::UserIsAdmin( const char* shortName )
{
	sInt32				siResult	= eDSNoErr;
	bool				returnVal	= false;
	unsigned long		nodeCount	= 0;
	tContextData		context		= NULL;
	tDataListPtr		nodeName	= NULL;
	tDataBufferPtr		dataBuff	= NULL;
	tDirReference		aDSRef		= 0;
	tDirNodeReference	localNodeRef= 0;

	try
	{
		siResult = dsOpenDirService( &aDSRef );
		if ( siResult != eDSNoErr ) throw( -3 );
	
		dataBuff = dsDataBufferAllocate( aDSRef, 8192 );
		if ( dataBuff == nil ) throw( -3 );

		siResult = dsFindDirNodes( aDSRef, dataBuff, nil,
								   eDSLocalNodeNames, &nodeCount, &context );
		if ( siResult != eDSNoErr ) throw( -3 );
		if ( nodeCount < 1 ) throw( -3 );

		siResult = dsGetDirNodeName( aDSRef, dataBuff, 1, &nodeName );
		if ( siResult != eDSNoErr ) throw( -3 );

		siResult = dsOpenDirNode( aDSRef, nodeName, &localNodeRef );
		if ( siResult != eDSNoErr ) throw( -3 );

		returnVal = UserIsMemberOfGroup( aDSRef, localNodeRef, shortName, "admin" );
	}
	catch( sInt32 err )
	{
		returnVal = false;
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
	if ( localNodeRef != 0 )
	{
		dsCloseDirNode( localNodeRef );
		localNodeRef = 0;
	}

	if ( aDSRef != 0 )
	{
		dsCloseDirService( aDSRef );
		aDSRef = 0;
	}
	
	return returnVal;
}


bool CRequestHandler::UserIsMemberOfGroup( tDirReference inDirRef, tDirNodeReference inDirNodeRef, const char* shortName, const char* groupName )
{
	bool						isInGroup 			= false;
	tDirStatus					dsStatus			= eDSNoErr;
	tAttributeEntryPtr			attrPtr				= nil;
	tAttributeValueEntryPtr		pValueEntry			= nil;
	UInt32						i;
	tDataListPtr				attrTypeList		= NULL;
	tDataListPtr				recNames			= NULL;
	tDataListPtr				recTypes			= NULL;
	tDataBufferPtr				dataBuff			= NULL;
	UInt32						curRecCount			= 1;
	tContextData				context				= NULL;
	tAttributeListRef			attrListRef			= 0;
	tAttributeValueListRef		attrValueListRef	= 0;
	tRecordEntryPtr				recEntry			= NULL;

	if ( groupName == NULL || shortName == NULL )
	{
		return false;
	}

	attrTypeList = dsBuildListFromStringsPriv( kDSNAttrGroupMembership, nil );
	recNames = dsBuildListFromStringsPriv( groupName, nil );
	recTypes = dsBuildListFromStringsPriv( kDSStdRecordTypeGroups, nil );
	dataBuff = dsDataBufferAllocatePriv( 4096 );

	do
	{
		dsStatus = dsGetRecordList( inDirNodeRef, dataBuff, recNames, eDSExact,
							  recTypes, attrTypeList, false, &curRecCount, &context );
		if ( dsStatus == eDSBufferTooSmall )
		{
			UInt32 buffSize = dataBuff->fBufferSize;
			dsDataBufferDeAllocate( inDirRef, dataBuff );
			dataBuff = NULL;
			dataBuff = dsDataBufferAllocate( inDirRef, buffSize * 2 );
		}
	} while (((dsStatus == eDSNoErr) && (curRecCount == 0) && (context != NULL)) || (dsStatus == eDSBufferTooSmall));

	if ( ( dsStatus == eDSNoErr ) && ( curRecCount > 0 ) )
	{
		// now walk through the list of users in this group and look for a match
		dsStatus = dsGetRecordEntry( inDirNodeRef, dataBuff, 1, &attrListRef, &recEntry );
		if (dsStatus == eDSNoErr )
		{
			dsStatus = dsGetAttributeEntry( inDirNodeRef, dataBuff, attrListRef, 1, &attrValueListRef,
								   &attrPtr );
		}

		if ( dsStatus == eDSNoErr )
		{
			for ( i = 1; i <= attrPtr->fAttributeValueCount && (dsStatus == eDSNoErr) && (isInGroup == false); i++ )
			{
				// note that since we only asked for one attribute type (group membership)
				// we can assume that if we got this far that is what we're looking at
				dsStatus = dsGetAttributeValue( inDirNodeRef, dataBuff, i, attrValueListRef, &pValueEntry );

				// now compare the member of the group against the user name we were given
				if ( dsStatus == eDSNoErr && ::strcmp( pValueEntry->fAttributeValueData.fBufferData, shortName ) == 0 )
					isInGroup = true;
				if ( pValueEntry != nil )
				{
					dsStatus = dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
					pValueEntry = nil;
				}
			}
		}
		if ( attrPtr != NULL ) {
			dsDeallocAttributeEntry( inDirRef, attrPtr );
			attrPtr = NULL;
		}
		if ( attrValueListRef != 0 ) {
			dsCloseAttributeValueList( attrValueListRef );
			attrValueListRef = 0;
		}
		if ( recEntry != NULL ) {
			dsDeallocRecordEntry( inDirRef, recEntry );
			recEntry = NULL;
		}
		if ( attrListRef != 0 ) {
			dsCloseAttributeList( attrListRef );
			attrListRef = 0;
		}
	}
	if ( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( inDirRef, dataBuff );
		dataBuff = NULL;
	}
	if ( context != NULL )
	{
		dsReleaseContinueData( inDirNodeRef, context );
		context = NULL;
	}
	if ( attrTypeList != NULL)
	{
		dsDataListDeallocate( inDirRef, attrTypeList );
		free( attrTypeList );
		attrTypeList = NULL;
	}
	if ( recNames != NULL)
	{
		dsDataListDeallocate( inDirRef, recNames );
		free( recNames );
		recNames = NULL;
	}
	if ( recTypes != NULL)
	{
		dsDataListDeallocate( inDirRef, recTypes );
		free( recTypes );
		recTypes = NULL;
	}

	return isInGroup;

}


void CRequestHandler::GetUIDsForProcessID ( pid_t inPID, uid_t *outUID, uid_t *outEUID )
{
	int mib [] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)inPID };
	size_t ulSize = 0;

	// Look for a given pid
	if (inPID > 1) {
		struct kinfo_proc kpsInfo;
		ulSize = sizeof (kpsInfo);
		if (!::sysctl (mib, 4, &kpsInfo, &ulSize, NULL, 0)
			&& (kpsInfo.kp_proc.p_pid == inPID))
		{
			if (outUID != NULL) 
				*outUID = kpsInfo.kp_eproc.e_pcred.p_ruid;
			if (outEUID != NULL) 
				*outEUID = kpsInfo.kp_eproc.e_ucred.cr_uid;
			return;
		}
	}

	// if we don't know, treat it as nobody
	if (outUID != NULL) *outUID = (uid_t)-2;
	if (outEUID != NULL) *outEUID = (uid_t)-2;
}


char* CRequestHandler::GetNameForProcessID ( pid_t inPID )
{
	int mib []		= { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)inPID };
	size_t ulSize	= 0;
	char *procName	= nil;

	// Look for a given pid
	if (inPID > 1) {
		struct kinfo_proc kpsInfo;
		ulSize = sizeof (kpsInfo);
		if (!::sysctl (mib, 4, &kpsInfo, &ulSize, NULL, 0)
			&& (kpsInfo.kp_proc.p_pid == inPID))
		{
			if (kpsInfo.kp_proc.p_comm != NULL)
			{
				procName = (char *)calloc(1, 1+strlen(kpsInfo.kp_proc.p_comm));
				strcpy(procName,kpsInfo.kp_proc.p_comm);
			}
		}
	}

	return( procName );
}


char* CRequestHandler::GetCallName ( sInt32 inType )
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
									sInt32			inResult)
{
	double	outTime	= 0;
	
	if (gLogAPICalls)
	{
		outTime = dsTimestamp();
	
		if (inDebugDataTag != nil)
		{
			syslog(LOG_INFO,"%s : Result: %d, Duration: %.2f usec", inDebugDataTag, inResult, (outTime - inTime));
		}
	}
			
} //Log API Call

char* CRequestHandler::BuildAPICallDebugDataTag (uInt32			inIPAddress,
												sInt32			inClientPID,
												char		   *inCallName,
												char		   *inName)
{
	char	   *outTag = nil;
	uInt32		suffixLen	= strlen(inCallName) + strlen(inName);
	
	if (inIPAddress == 0)
	{
		//this is locally from this machine
		char *pName = nil;
		pName = GetNameForProcessID(inClientPID);
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

// ----------------------------------------------------------------------------
//	* GetOurThreadRunState()
//
// ----------------------------------------------------------------------------

OSType CHandlerThread::GetOurThreadRunState ( void )
{
	return( GetThreadRunState() );
} // GetOurThreadRunState


// ----------------------------------------------------------------------------
//	* RefDeallocProc()
//    used to clean up plug-in specific data for a reference
// ----------------------------------------------------------------------------

sInt32 CHandlerThread::RefDeallocProc ( uInt32 inRefNum, uInt32 inRefType, CServerPlugin *inPluginPtr )
{
	sInt32	dsResult	= eDSNoErr;
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
				syslog(LOG_INFO,"Ref table dealloc callback, API Call: %s, PlugIn Used: %s, Result: %d, Duration: %.2f usec",
					CRequestHandler::GetCallName( closeData.fType ), inPluginPtr->GetPluginName(), dsResult,
					(outTime - inTime));
			}
		}
	}
	
	return( dsResult );
} // RefDeallocProc


//--------------------------------------------------------------------------------------------------
//	* FailedCallRefCleanUp()
//
//--------------------------------------------------------------------------------------------------

sInt32 CRequestHandler:: FailedCallRefCleanUp ( void *inData, sInt32 inClientPID, uInt32 inMsgType, uInt32 inIPAddress  )
{
	sInt32			siResult	= eDSNoErr;

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

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );
	
} // FailedCallRefCleanUp


