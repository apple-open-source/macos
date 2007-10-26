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
 * Listener object implementation using DSTCPEndpoint.
 */

#ifndef __DSTCPListener_h__
#define __DSTCPListener_h__ 1

#include "DSCThread.h"
#include "DSTCPConnection.h"
#include "DSMutexSemaphore.h"

//---------------------------------------------------------
// DSTCPListener
// class will listen for connection requests
// from (possibly) remote machines, and upon receipt of a request,
// bind the given socket
// and the specified communications connection handler together.

//---------------------------------------------------------

typedef enum {
	kDSPXType		= 'DSPX'
} ConnectionType;

typedef struct
{
	DSTCPConnection	   *pConnection;
	struct timeval		startTime;
	char				fRemoteIP[kMaxHostNameLength];
	UInt32				fRemotePID;
	UInt32				fNumberAPICalls;
} tConnectStruct;

#define kDefaultMaxConnections 8
#define kMaxConnections 64

class DSTCPEndpoint;

class DSTCPListener: public DSCThread
{
	public:

		// Constructor and Destructor
							DSTCPListener ( const UInt16 inPort );
							DSTCPListener ( const UInt16 inPort, const UInt32 inMaxConnections );
		virtual				~DSTCPListener (void);
		
		static Boolean		Initialize(void);
		
		enum { kTCPIPListener = 11 }; //magic number 11 chosen

		Boolean		IsListening (void)	{ return mListening; }
		UInt32		ConnectionType (void)	{ return mConnectionType; }
		UInt16		TCPPort	(void)		{ return mPort; }
	
		SInt32				ThreadMain				( void );
		virtual	void		StartThread				( void );
		virtual	void		StopThread				( void );
		Boolean				SetMaxConnections		( UInt32 inMaxConnections );
		UInt32				GetMaxConnections		( void );
		UInt32				GetUsedConnections		( void );
		void				ConnectionClosed		( DSTCPConnection *inConnection );
		void				AddPIDForConnectionStat ( DSTCPConnection *inConnection, UInt32 inPID );
		
	protected:

		DSTCPEndpoint	   *fTCPEndpoint;
		static Boolean		sInitialized;
		
	private:
	
		SInt32		CreateTCPEndpoint ( void );	// create TCP endpoint, initialize and
												// set up to listen on our port
		Boolean		WaitForConnection (void);	// wait for connection to come in
		void		CreateConnection (void);	// bind the new connection to a connection
		void		BindSocket ( DSTCPEndpoint*& sock );
	
		Boolean		mListening;				//track whether this is actually listening or not
		Boolean		mStop;					//whether the listener is stopped not aborted
		UInt16		mPort;					//actual port number
		UInt32		mType;					//this is only kTCPIPListener
		UInt32		mConnectionType;		//this is the tag kDSPXType
		UInt32		fMaxConnections;		//Max number of TCP connections
		UInt32		fUsedConnections;		//used number of TCP connections
		DSMutexSemaphore
				   *fConnectionLock;
		tConnectStruct
				   *fConnections[kMaxConnections+1];
		
		

};

#endif // __DSTCPListener_h__
