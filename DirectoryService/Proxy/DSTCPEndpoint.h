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
 * @header DSTCPEndpoint
 * Interface to TCP Socket endpoint class.
 */

#ifndef __DSTCPEndpoint_h__
#define __DSTCPEndpoint_h__ 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>		// struct sockaddr_in

#include "DSNetworkUtilities.h"		// for some constants
#include "SharedConsts.h"

// specific to DSTCPEndpoint
const UInt32	kTCPOpenTimeout			= 120;
//KW need to revisit
#ifdef DSSERVERTCP
const UInt32	kTCPRWTimeout			= 60*60*24; //server 24 hour timeout
#else
const UInt32	kTCPRWTimeout			= 60*5; //client 5 min timeout like mach
#endif
const UInt32	kTCPMaxListenBackLog	= 1024;
const UInt32	kTCPErrorBufferLen		= 256;

const UInt32 kDSTCPEndpointMaxMessageSize	= 1024; //used for searching for the TCP message tag
const UInt32 kDSTCPEndpointMessageTagSize	= 4;	//for "DSPX" tag

// ----------------------------------------------------------------------------
// DSTCPEndpoint: implementation of endpoint based on BSD sockets.
// ----------------------------------------------------------------------------

class DSTCPEndpoint
{

public:

	// TCP related exception error codes
	enum eExceptions
	{
		kConnectionLostWarning	= eDSCannotAccessSession,		// We've lost our connection
		kAbortedWarning			= 'sok1',		// We've been signaled to abort all actions
		kTimeoutError			= eDSServerTimeout,		// Connection timed out
	};

	// timeout constant types
	enum eTimeoutType
	{
        kOpenTimeoutType = 1,
        kRWTimeoutType,
		kDefaultTimeoutType
    };

				DSTCPEndpoint			( const UInt32 inSessionID,
										  const UInt32 inOpenTimeOut = kTCPOpenTimeout,
										  const UInt32 inRdWrTimeOut = kTCPRWTimeout );

				DSTCPEndpoint			( const DSTCPEndpoint *inEndpoint,
										  const UInt32 inSessionID );

    virtual	   ~DSTCPEndpoint			( void );

	// Inline accessors.
	UInt32		GetSessionID			( void )				{ return mLogMsgSessionID; }
	UInt32		GetReverseAddress		( void ) const			{ return mRemoteHostIPAddr; }
	UInt32		GetIPAddress			( void ) const			{ return mMyIPAddr; } //never used
	const char *GetReverseAddressString	( void ) const			{ return mRemoteHostIPString; }
	int			GetCurrentConnection	( void ) const			{ return mConnectFD; }

	SInt32		SendClientReply			( void *inMsg );
	void*		GetClientMessage		( void );
	SInt32		SyncToMessageBody		( const Boolean inStripLeadZeroes, UInt32 *outBuffLen );
	SInt32		GetServerReply			( sComData **outMsg );
	SInt32		SendServerMessage		( void *inMsg );
	SInt32		SendBuffer				( void *inBuffer, UInt32 inLength );
	
	virtual void	EncryptData			( void *inData, const UInt32 inBuffSize, void *&outData, UInt32 &outBuffSize );
	virtual void	DecryptData			( void *inData, const UInt32 inBuffSize, void *&outData, UInt32 &outBuffSize );
	UInt32		WriteData				( const void *inData, const UInt32 inSize );
	Boolean		Connected				( void ) const ;
    void		Abort					( void );
	SInt32		ConnectTo ( const UInt32 inIPAddress, const UInt16 inPort ); //for client side
	void		ListenToPort			( const UInt16 inPort );
	void		ListenToPortOnAddress	( const UInt16 inPort, const UInt32 inWhichAddr );
	Boolean		AcceptConnection		( void );
	void		CloseConnection			( void );
	int			CloseListener			( void ); //KW do we need this?
	void		SetTimeout				( const int inWhichTimeout, const int inSeconds ); //not used now
	void		GetReverseAddressString	( char *ioBuffer, const int inBufferSize ) const ;
	UInt32		GetRemoteHostIPAddress	( void );
	UInt16		GetRemoteHostPort		( void );

protected:
		
	sComProxyData*  AllocToProxyStruct  ( sComData *inDataMsg );
	sComData*		AllocFromProxyStruct( sComProxyData *inProxyDataMsg );
	UInt32		DoTCPRecvFrom			( void *ioBuffer, const UInt32 inBufferSize );

private:
		
	/**** Instance methods accessible only to class. ****/
	void		InitBuffers				( void );
	int			DoTCPOpenSocket			( void );
	int			SetSocketOption			( const int inSocket, const int inSocketOption);
	int			DoTCPBind				( void );
	int			DoTCPListen				( void );
	int			DoTCPAccept				( void );
	int			DoTCPCloseSocket		( const int inSockFD );


protected:
	UInt32				mLogMsgSessionID; //set in constructor and never used for anything
	
	// network information
	UInt32				mMyIPAddr;		// in host byte order - set but never used
	struct sockaddr_in	mMySockAddr;	

	// remote host network information
	UInt32				mRemoteHostIPAddr;		// in host byte order
	IPAddrStr 			mRemoteHostIPString;	// IP address string
	struct sockaddr_in	mRemoteSockAddr;

	int					mListenFD;
	int					mConnectFD;
		
	// buffers
	char			   *mErrorBuffer;

	// states
	Boolean				mAborting;
	Boolean				mWeHaveClosed;

	// Timeouts
	int					mOpenTimeout;	// time out for opening connection
	int					mRWTimeout;		// time out for reading and writing
	int					mDefaultTimeout;
};

#endif // __DSTCPEndpoint_h__
