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
 * @header CListener
 */

#include "CListener.h"
#include "CHandlers.h"
#include "CSrvrEndPoint.h"
#include "ServerControl.h"
#include "CMsgQueue.h"
#include "CLog.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"

#include <servers/bootstrap.h>
#include <sched.h>
#include <syslog.h>		// for syslog()


// --------------------------------------------------------------------------------
//	* Globals:
// --------------------------------------------------------------------------------

//API Call Count
uInt32 gAPICallCount = 0;

//API logging
extern dsBool				gLogAPICalls;
extern time_t				gSunsetTime;
extern uInt32				gDaemonPID;

//dead port notifications
//extern mach_port_t			gClient_Death_Notify_Port;
//extern void EnableDeathNotificationForClient(mach_port_t port);

//--------------------------------------------------------------------------------------------------
//	* CListener()
//
//--------------------------------------------------------------------------------------------------

CListener::CListener ( void )
	: DSCThread( kTSListenerThread )
{
	fMsgBlock			= nil;
	fThreadSignature	= kTSListenerThread;
	fEndPt				= nil;
} // CListener



//--------------------------------------------------------------------------------------------------
//	* ~CListener()
//
//--------------------------------------------------------------------------------------------------

CListener::~CListener()
{
	if ( fEndPt != nil )
	{
		delete( fEndPt );
		fEndPt = nil;
	}
} // ~CListener



//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CListener::StartThread ( void )
{
	sInt32	result = eDSNoErr;
	
	if ( this == nil ) throw((sInt32)eMemoryError);

	result = CreateEndpoint();
	if ( result != eDSNoErr )
	{
		DBGLOG2( kLogThreads, "File: %s. Line: %d", __FILE__, __LINE__ );
		DBGLOG1( kLogThreads, "  ***CreateEndpoint() returned = %d", result );

		ERRORLOG1( kLogEndpoint, "Unable to create mach connection: %d", result );
		
		throw( result );
	}

	this->Resume();
} // StartThread



//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CListener::StopThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	// Check that the current thread context is not our thread context
//	SignalIf_( this->IsCurrent() );

	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread



//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

long CListener::ThreadMain ( void )
{
	bool		done		= false;

	if ( GetThreadRunState() != kThreadStop )
	{
		//would like to enhance the priority of this thread here to more than a handler thread
		//since it handles ALL incoming messages for ALL the handler threads
		int			myPolicy;
		int			minPriority;
		int			maxPriority;
		sched_param	myStruct;

		if (pthread_getschedparam( pthread_self(), &myPolicy, &myStruct) == 0)
		{
			DBGLOG1( kLogThreads, "Thread priority defaults to %d for listener thread.", myStruct.sched_priority );
			minPriority = sched_get_priority_min(myPolicy);
			maxPriority = sched_get_priority_max(myPolicy);
			if (maxPriority > myStruct.sched_priority)
			{
				myStruct.sched_priority = ((int)((maxPriority - myStruct.sched_priority)/2)) + myStruct.sched_priority;

				if (pthread_setschedparam( pthread_self(), myPolicy, &myStruct) == 0)
				{
					DBGLOG3( kLogThreads, "Thread priority set to %d for listener thread within range of [ %d , %d ].", myStruct.sched_priority, minPriority, maxPriority );
				}
			}
		}
	}

	while ( GetThreadRunState() != kThreadStop )
	{
        //this while will only go once since a throw below sets the kThreadStop
		try
		{
			while ( !done )
			{
				// Listen for incomming messages
				if ( ListenForMessage() == true )
				{
					QueueMessage();
				}

				//sunset value on the looging of API calls if it accidentally gets turned on or never turned off
				if ( gLogAPICalls )
				{
					if (::time( nil ) > gSunsetTime)
					{
						gLogAPICalls	= false;
						syslog(LOG_INFO,"Logging of API Calls automatically turned OFF at reaching sunset duration of five minutes.");
					}
				}
				
				if ( GetThreadRunState() == kThreadStop )
				{
					done = true;
				}
			}
		}

		catch( sInt32 err )
		{
			DBGLOG2( kLogThreads, "File: %s. Line: %d", __FILE__, __LINE__ );
			DBGLOG1( kLogThreads, "  ***CListener::ThreadMain error = %d", err );
			this->SetThreadRunState( kThreadStop );
		}
	}

	return( 0 );

} // ThreadMain


//--------------------------------------------------------------------------------------------------
//	* CreateEndpoint()
//
//--------------------------------------------------------------------------------------------------

sInt32 CListener::CreateEndpoint ( void )
{
	sInt32		result = eDSNoErr;

	if ( fEndPt == nil )
	{
		fEndPt = new CSrvrEndPoint( kDSServiceName );
		if ( fEndPt != nil )
		{
			result = fEndPt->Initialize();
		}
		else
		{
			result = eMemoryError;
		}
	}

	return( result );

} // CreateEndpoint


//--------------------------------------------------------------------------------------------------
//	* ListenForMessage()
//
//--------------------------------------------------------------------------------------------------

bool CListener::ListenForMessage ( void )
{
	bool	gotOne	= false;

	fMsgBlock = (sComData *)fEndPt->GetClientMessage();
	if ( fMsgBlock != nil )
	{
		gotOne = true;

		//we know this is a mach message so the IP address is for this server so we use zero
		//then let's add it to the fMsgBlock for the handler threads and ref table to use
		fMsgBlock->fIPAddress = 0;
	
	}

	return( gotOne );

} // ListenForMessage


//--------------------------------------------------------------------------------------------------
//	* QueueMessage()
//
//--------------------------------------------------------------------------------------------------

sInt32 CListener::QueueMessage ( void )
{
	sInt32	result	= eDSNoErr;

	//check if this call came in from custom call to do checkpw
	if (fMsgBlock->type.msgt_name == kCheckUserNameAndPassword)
	{
		if ( gCheckpwMsgQueue != nil )
		{
			gCheckpwHandlerLock->Wait();
			result = gCheckpwMsgQueue->QueueMessage( fMsgBlock );
			//passed off the message so now nil the pointer
			fMsgBlock = nil;

			gSrvrCntl->StartAHandler(kTSCheckpwHandlerThread);
			
			gSrvrCntl->WakeAHandler(kTSCheckpwHandlerThread);
			gCheckpwHandlerLock->Signal();
		
		}
		else
		{
			result = kEmptyQueueObj;
		}
	}
	//if this call came from us then put it on the internal queue
	else if (fMsgBlock->fPID == gDaemonPID)
	{
		if ( gInternalMsgQueue != nil )
		{
			gInternalHandlerLock->Wait();
			result = gInternalMsgQueue->QueueMessage( fMsgBlock );
			//passed off the message so now nil the pointer
			fMsgBlock = nil;

			gSrvrCntl->StartAHandler(kTSInternalHandlerThread);
			
			gSrvrCntl->WakeAHandler(kTSInternalHandlerThread);
			gInternalHandlerLock->Signal();
		
		}
		else
		{
			result = kEmptyQueueObj;
		}
	}
	//else this call came from an external client
	else
	{
		if ( gMsgQueue != nil )
		{
			//let's check if this is a kOpenDirService so that we can register to get dead port notifications
			//if (fMsgBlock->type.msgt_name == kOpenDirService)
			//{
				//EnableDeathNotificationForClient(fMsgBlock->head.msgh_remote_port);
			//}
			gHandlerLock->Wait();
			result = gMsgQueue->QueueMessage( fMsgBlock );
			//passed off the message so now nil the pointer
			fMsgBlock = nil;

			//only track external client API calls
			gAPICallCount++;

			gSrvrCntl->StartAHandler(kTSHandlerThread);
			
			gSrvrCntl->WakeAHandler(kTSHandlerThread);
			gHandlerLock->Signal();
		
		}
		else
		{
			result = kEmptyQueueObj;
		}
	}

	return( result );

} // QueueMessage


