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
 * @header DSTCPListener
 * Listener object implementation using TCPEndpoint.
 */

/*
	Note: all network addresses in method parameters and return
	values are in host byte order - they are converted to network
	byte order inside the methods for socket calls.
*/

#include <syslog.h>		// for syslog()
#include <stdlib.h>		// for calloc()
#include <sys/time.h>	// for struct timespec and gettimeofday()

#include "CLog.h"
#include "DSTCPListener.h"
#include "DSTCPEndpoint.h"
#include "DSEncryptedEndpoint.h"
#include "DSNetworkUtilities.h"
#include "DSTCPConnection.h"

//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Static data members
//------------------------------------------------------------------------------
Boolean DSTCPListener::sInitialized = false;

//------------------------------------------------------------------------------
// DSTCPListener
//------------------------------------------------------------------------------
DSTCPListener::DSTCPListener( const uInt16 inPort )
	:DSCThread(kTSTCPListenerThread),
	mListening(false),
	mStop(false),
	mPort(inPort),
	mType(kTCPIPListener),
	mConnectionType(kDSPXType),
	fMaxConnections(kDefaultMaxConnections),
	fUsedConnections(0)
{
	fConnectionLock = new DSMutexSemaphore();
	fTCPEndpoint	= nil;
	memset(fConnections,0,sizeof(tConnectStruct*)*(kMaxConnections+1));
}

//------------------------------------------------------------------------------
// DSTCPListener
//------------------------------------------------------------------------------
DSTCPListener::DSTCPListener( const uInt16 inPort, const uInt32 inMaxConnections )
	:DSCThread(kTSTCPListenerThread),
	mListening(false),
	mStop(false),
	mPort(inPort),
	mType(kTCPIPListener),
	mConnectionType(kDSPXType),
	fMaxConnections(inMaxConnections),
	fUsedConnections(0)

{
	fConnectionLock = new DSMutexSemaphore();
	fTCPEndpoint	= nil;
	memset(fConnections,0,sizeof(tConnectStruct*)*(kMaxConnections+1));
}

//------------------------------------------------------------------------------
// ~DSTCPListener
//------------------------------------------------------------------------------

DSTCPListener::~DSTCPListener ( void )
{
	struct timeval	timeNow;

	if ( fTCPEndpoint != NULL )
	{
		delete( fTCPEndpoint );
		fTCPEndpoint = NULL;
	}
	
	if (fConnectionLock != nil)
	{
		//TODO need to tear down any existing connections before we go away
		fConnectionLock->Wait();
	
		for (uInt32 idx = 0; idx < fUsedConnections; idx++)
		{
			if (fConnections[idx]->pConnection != nil)
			{
				//log the closing of this connection here
				gettimeofday(&timeNow,NULL);
				DBGLOG3( kLogConnection, "Connection Shutdown by Listener Cleanup:: %s connected from PID %u for %u usec", fConnections[idx]->fRemoteIP, fConnections[idx]->fRemotePID, 1000000*(timeNow.tv_sec - (fConnections[idx]->startTime).tv_sec) + timeNow.tv_usec - (fConnections[idx]->startTime).tv_usec );
				
				//stop the connection thread
				//this assumes that the TCP endpoint has a timeout at which it will retry if
				//the thread is NOT to go away
				fConnections[idx]->pConnection->StopThread();
				
				free(fConnections[idx]);
			}
		}
		fUsedConnections = 0;
	
		fConnectionLock->Signal();
		
		delete(fConnectionLock);
		fConnectionLock = nil;
	}
		
}

//------------------------------------------------------------------------------
// Initialize -- needs to be called after the constructor
//
//------------------------------------------------------------------------------
Boolean
DSTCPListener::Initialize(void)
{

	if (sInitialized == true) 
		return true;
	
	if (DSNetworkUtilities::Initialize() == eDSNoErr)
	{
		sInitialized = true;
		return true;
	}
	else
	{
		if (DSNetworkUtilities::IsTCPAvailable() == true)
		{
			sInitialized = true;
			return true;
		}
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
// Wait for tcp connection requests, and upon getting a good one,
// bind it to a connection class and let the connection class run free
//
//--------------------------------------------------------------------------------------------------

long DSTCPListener::ThreadMain ( void )
{
	bool		done		= false;
	sInt32		result		= eDSNoErr;

//KW need to consider this
/*
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
			DBGLOG1( kLogThreads, "Thread priority defaults to %d for TCP listener thread.", myStruct.sched_priority );
			minPriority = sched_get_priority_min(myPolicy);
			maxPriority = sched_get_priority_max(myPolicy);
			if (maxPriority > myStruct.sched_priority)
			{
				myStruct.sched_priority = ((int)((maxPriority - myStruct.sched_priority)/2)) + myStruct.sched_priority;

				if (pthread_setschedparam( pthread_self(), myPolicy, &myStruct) == 0)
				{
					DBGLOG3( kLogThreads, "Thread priority set to %d for TCP listener thread within range of [ %d , %d ].", myStruct.sched_priority, minPriority, maxPriority );
				}
			}
		}
	}
*/

	while ( GetThreadRunState() != kThreadStop )
	{
        //this while will only go once since a throw below sets the kThreadStop
		try
		{
			result = CreateTCPEndpoint();
			if ( result != eDSNoErr )
			{
				DBGLOG2( kLogThreads, "File: %s. Line: %d", __FILE__, __LINE__ );
				DBGLOG1( kLogThreads, "  ***CreateTCPEndpoint() returned = %d", result );

				ERRORLOG1( kLogEndpoint, "Unable to create TCP connection: %d", result );
			}
			if ( result != eDSNoErr ) throw( result );

			while ( !done )
			{
				// Listen for incoming requests to connect
				if ( WaitForConnection() == true )
				{
					CreateConnection();
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
			DBGLOG1( kLogThreads, "  ***DSTCPListener::ThreadMain error = %d", err );
			this->SetThreadRunState( kThreadStop );
		}
	}

	return( 0 );

} // ThreadMain


//------------------------------------------------------------------------------
//	* WaitForConnection ()
//
//		- This calls the TCPEndpoint method to accept connection
//			this function will block until a connetion is available
//------------------------------------------------------------------------------

Boolean DSTCPListener::WaitForConnection ( void )
{
	bool	gotRequest	= false;

	if ( fTCPEndpoint->AcceptConnection() )
	{
		// at this point, we returned from an accept and have an open connection
		DBGLOG1( kLogConnection, "Got a connection from %s.", fTCPEndpoint->GetReverseAddressString() );
		gotRequest = true;
	}
	
	return gotRequest;


} // WaitForConnection


//----------------------------------------------------------------------------------
// * BindSocket() - Create a new connection object, and bind the given socket to it.
// 					Then let the connection object run free. It now owns this socket. 
// ---------------------------------------------------------------------------------

void DSTCPListener::BindSocket( DSTCPEndpoint* &sock )
{
	tConnectStruct	   *aConnectStructPtr	= NULL;
	DSTCPConnection	   *aConnection			= NULL;
	try
	{
		fConnectionLock->Wait();
		// Create the Connection object, and tell it which socket to use.
		if ( fUsedConnections < fMaxConnections )
		{
			aConnection = new DSTCPConnection(this);

			//keep track of the connections here
			aConnectStructPtr = (tConnectStruct *)calloc(1, sizeof(tConnectStruct));
			aConnectStructPtr->pConnection = aConnection;
			gettimeofday(&(aConnectStructPtr->startTime),NULL);
			sock->GetReverseAddressString( aConnectStructPtr->fRemoteIP, kMaxHostNameLength );
			fConnections[fUsedConnections] = aConnectStructPtr;
			fUsedConnections++;
		}
		fConnectionLock->Signal();
	}
	catch( sInt32 err )
	{
		fConnectionLock->Signal();
		throw(err);
	}
	
	aConnection->SetEndpoint(sock);

	// Set sock to null to indicate we no longer control it
	sock = NULL;
	
	// Tell the connection thread it's ok to run
	aConnection->StartThread();
}

//------------------------------------------------------------------------------
// CreateConnection
//------------------------------------------------------------------------------
// This is called after a connection has been established. Create a new Endpoint
// with that connection and bind it to a Connection class to handle the connection. 

void DSTCPListener::CreateConnection ( void )
{
	static DSTCPEndpoint	   *pNewEndpoint	= NULL;
	static uInt32			sessionID		= ::time(NULL);

	// Create a copy of this endpoint 

	fConnectionLock->Wait();
	//check to make sure number of connections NOT exceeded
	if ( fUsedConnections < fMaxConnections )
	{
		pNewEndpoint = new DSEncryptedEndpoint( fTCPEndpoint, sessionID );
	}
	fConnectionLock->Signal();

	if ( pNewEndpoint != NULL )
	{
		try
		{
			this->BindSocket( (DSTCPEndpoint *&)pNewEndpoint ); // binds the socket to a connection
			DBGLOG2(kLogConnection, "Created TCP connection to %s on port %d", fTCPEndpoint->GetReverseAddressString(), mPort);
		}

		catch ( int err )
		{
			if ( pNewEndpoint != nil )
			{
				delete( pNewEndpoint );
				pNewEndpoint = nil;
			}
			throw( (sInt32)err );
		}

		catch ( ... )
		{
			if ( pNewEndpoint != nil )
			{
				delete( pNewEndpoint );
				pNewEndpoint = nil;
			}
			throw( -1 );
		}

 		if ( pNewEndpoint != nil ) throw( (sInt32)eMemoryError ); // fTCPEndpoint should have been set to NULL in BindSocket.
	}
	else
	{
		//TODO need BETTER WAY? to return that we have reached max sessions
		fTCPEndpoint->CloseConnection();
	}
}


//--------------------------------------------------------------------------------------------------
//	* CreateTCPEndpoint()
//
//--------------------------------------------------------------------------------------------------

sInt32 DSTCPListener::CreateTCPEndpoint ( void )
{
	sInt32		result = eDSNoErr;

	if ( fTCPEndpoint == nil )
	{
		fTCPEndpoint = new DSTCPEndpoint((uInt32)::time(NULL), kTCPOpenTimeout, kTCPRWTimeout);
		if ( fTCPEndpoint != nil )
		{
			// set up for listening to a port
			// but does not accept any connection yet.
			fTCPEndpoint->ListenToPort( mPort );
			mListening = true;
		}
		else
		{
			result = eMemoryError;
		}
	}

	return( result );

} // CreateTCPEndpoint


//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void DSTCPListener::StartThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	this->Resume();
} // StartThread



//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void DSTCPListener::StopThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread

		
//--------------------------------------------------------------------------------------------------
//	* GetUsedConnections()
//
//--------------------------------------------------------------------------------------------------

uInt32 DSTCPListener::GetUsedConnections ( void )
{

	return fUsedConnections;

} // GetUsedConnections


//--------------------------------------------------------------------------------------------------
//	* SetMaxConnections()
//
//--------------------------------------------------------------------------------------------------

Boolean DSTCPListener::SetMaxConnections ( uInt32 inMaxConnections )
{
	Boolean	succeeded	= false;

	fConnectionLock->Wait();
	//only allow the number to go up from the default OR up from used connections
	if ( ( inMaxConnections >= fUsedConnections ) && ( inMaxConnections > kDefaultMaxConnections ) && ( inMaxConnections <= kMaxConnections ) )
	{
		fMaxConnections = inMaxConnections;
		succeeded = true;
	}
	fConnectionLock->Signal();
	
	return succeeded;

} // SetMaxConnections


//--------------------------------------------------------------------------------------------------
//	* GetMaxConnections()
//
//--------------------------------------------------------------------------------------------------

uInt32 DSTCPListener::GetMaxConnections ( void )
{

	return fMaxConnections;

} // GetMaxConnections



//--------------------------------------------------------------------------------------------------
//	* ConnectionClosed()
//
//--------------------------------------------------------------------------------------------------

void DSTCPListener::ConnectionClosed ( DSTCPConnection *inConnection )
{
	struct timeval	timeNow;
	Boolean			bSlide = false;
	
	fConnectionLock->Wait();

	if (inConnection != nil)
	{
		for (uInt32 idx = 0; idx < fUsedConnections; idx++)
		{
			if (bSlide)
			{
				fConnections[idx] = fConnections[idx+1];
			}
			else if (fConnections[idx]->pConnection == inConnection)
			{
				bSlide = true;
				//log the closing of this connection here
				gettimeofday(&timeNow,NULL);
				SRVRLOG3( kLogConnection, "Connection:: %s connected from PID %u for %u usec", fConnections[idx]->fRemoteIP, fConnections[idx]->fRemotePID, 1000000*(timeNow.tv_sec - (fConnections[idx]->startTime).tv_sec) + timeNow.tv_usec - (fConnections[idx]->startTime).tv_usec );

				free(fConnections[idx]);
				fConnections[idx] = fConnections[idx+1];
			}
		}
		if (bSlide)
		{
			fUsedConnections--;
		}
	}

	fConnectionLock->Signal();

} // ConnectionClosed


//--------------------------------------------------------------------------------------------------
//	* AddPIDForConnectionStat()
//
//--------------------------------------------------------------------------------------------------

void DSTCPListener::AddPIDForConnectionStat ( DSTCPConnection *inConnection, uInt32 inPID )
{
	fConnectionLock->Wait();

	if (inConnection != nil)
	{
		for (uInt32 idx = 0; idx < fUsedConnections; idx++)
		{
			if (fConnections[idx]->pConnection == inConnection)
			{
				fConnections[idx]->fRemotePID = inPID;
				break;
			}
		}
	}

	fConnectionLock->Signal();

} // AddPIDForConnectionStat


