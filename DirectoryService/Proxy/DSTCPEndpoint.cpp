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
 * Implementation of TCP Socket endpoint class.
 */

/*
	Note: all network addresses in method parameters and return values
	are in host byte order - they are converted to network byte order
	inside the methods for socket calls.
		 
	Note2: need to be aware of which routines are FW or Server exclusive
	for what type of logging
*/

#include <string.h>
#include <errno.h>			// system call error numbers
#include <unistd.h>			// for select call 
#include <stdlib.h>			// for calloc()
#include <poll.h>
#include <sys/time.h>		// for struct timeval
#include <machine/byte_order.h>

#include "DSCThread.h"		// for GetCurThreadRunState()
#include "DSTCPEndpoint.h"
#ifdef DSSERVERTCP
#include "CLog.h"
#endif
#include "SharedConsts.h"	// for sComData
#include "DirServicesConst.h"
#include "DSTCPEndian.h"

// ----------------------------------------------------------------------------
//	* DSTCPEndpoint Class (static) Methods
// ----------------------------------------------------------------------------
#pragma mark **** Class Methods ****

// ----------------------------------------------------------------------------
//	* DSTCPEndpoint Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** Instance Methods ****

// ----------------------------------------------------------------------------
//	* InitBuffers ()
//
// ----------------------------------------------------------------------------

void DSTCPEndpoint::InitBuffers ( void )
{
	::memset(&mMySockAddr, 0, sizeof(mMySockAddr));
	mRemoteHostIPString[0] = '\0';
	::memset(&mRemoteSockAddr, 0, sizeof(mRemoteSockAddr));

	// Allocate buffers
	try {

		mErrorBuffer = new char [kTCPErrorBufferLen];
		if ( mErrorBuffer == nil ) throw((SInt32)eMemoryAllocError);
	}

	catch( SInt32 err )
	{
		throw(err);
	}
} // InitBuffers


// ----------------------------------------------------------------------------
//	* DSTCPEndpoint ()
//
// ----------------------------------------------------------------------------

DSTCPEndpoint::DSTCPEndpoint (	const UInt32	inSessionID,
								const UInt32	inOpenTimeout,
								const UInt32	inRWTimeout ) :
	mLogMsgSessionID(inSessionID),
	mMyIPAddr (DSNetworkUtilities::GetOurIPAddress(0)),
	mRemoteHostIPAddr (0),
	mListenFD (0),
	mConnectFD (0),
	mErrorBuffer (NULL),
	mAborting (false),
	mWeHaveClosed (false),
	mOpenTimeout (inOpenTimeout),
	mRWTimeout (inRWTimeout),
	mDefaultTimeout(inRWTimeout)
{
	this->InitBuffers ();
} // DSTCPEndpoint Constructor


// ----------------------------------------------------------------------------
//	* DSTCPEndpoint () - Duplicating Constructor
//
// ----------------------------------------------------------------------------

DSTCPEndpoint::DSTCPEndpoint (	const DSTCPEndpoint	*inEndpoint,
								const UInt32 		inSessionID) :
	mLogMsgSessionID(inSessionID),
	mMyIPAddr (inEndpoint->mMyIPAddr),
	mRemoteHostIPAddr (inEndpoint->mRemoteHostIPAddr),
	mListenFD (inEndpoint->mListenFD),
	mConnectFD (inEndpoint->mConnectFD),
	mErrorBuffer (NULL),
	mAborting (false),
	mWeHaveClosed (false),
	mOpenTimeout (inEndpoint->mOpenTimeout),
	mRWTimeout (inEndpoint->mRWTimeout),
	mDefaultTimeout(inEndpoint->mDefaultTimeout)
{
	this->InitBuffers ();

	// Copy the relevent structures.
	::memcpy(&mMySockAddr, &inEndpoint->mMySockAddr, sizeof (mMySockAddr));
	::memcpy(mRemoteHostIPString, &inEndpoint->mRemoteHostIPString, sizeof (mRemoteHostIPString));
	::memcpy(&mRemoteSockAddr, &inEndpoint->mRemoteSockAddr, sizeof (mRemoteSockAddr));
} // DSTCPEndpoint Duplicating Constructor


// ----------------------------------------------------------------------------
//	* ~DSTCPEndpoint ()
//
// ----------------------------------------------------------------------------

DSTCPEndpoint::~DSTCPEndpoint ( void )
{
	// make sure we safely close the connection

	try
	{
		if ( mWeHaveClosed == false )
		{
			DoTCPCloseSocket( mConnectFD );
		}
	}
	catch( SInt32 err )
	{
	}

	if ( mErrorBuffer != NULL )
	{
		delete [] mErrorBuffer;
		mErrorBuffer = NULL;
	}

} // ~DSTCPEndpoint


// ----------------------------------------------------------------------------
//	* ConnectTo () *****ONLY used by CMessaging class
// 
//		- Make a connection to another socket defined by the IP address and
//			port number
// ----------------------------------------------------------------------------

SInt32 DSTCPEndpoint::ConnectTo ( const UInt32 inIPAddress, const UInt16 inPort )
{
	int					err = eDSNoErr;
	int					result = 0;
	int					sockfd;
	int					len = sizeof(struct sockaddr_in);
	time_t				timesUp;
	struct sockaddr_in	serverAddr;
	int					rc = eDSNoErr;
	bool				releaseZeroFD = false;

	do //this is an INTENTIONAL temporary leak of the socket if it is zero since sockfd zero seems to always fail eventually
	{
		sockfd = DoTCPOpenSocket();
		if ( sockfd < 0 )
		{
			return( eDSTCPSendError );
		}

		mConnectFD = sockfd;
	
		::memset( &serverAddr, 0, len );
		serverAddr.sin_family		= AF_INET;
		serverAddr.sin_port			= htons( inPort );	// convert the numbers to network byte order
		serverAddr.sin_addr.s_addr	= htonl( inIPAddress );
	
		// although connect has its own timeout, to enable longer time out we use mOpenTimeout
		timesUp = ::time(NULL) + mOpenTimeout;
		while ( ::time(NULL) < timesUp )
		{
			result = ::connect( mConnectFD, (struct sockaddr *)&serverAddr, len );
	
			if ( mAborting == true )
			{
				return( kAbortedWarning );
			}
	
			if ( result == -1 )
			{
				err = errno;
				switch ( err )
				{
					case ETIMEDOUT:
						continue;	// returned from connect's timeout, keep trying until we time out
						break;
	
					case ECONNREFUSED:
						::memset(mErrorBuffer, 0, kTCPErrorBufferLen);
						::strncpy(mErrorBuffer, ::strerror(err), kTCPErrorBufferLen);
	
						LOG2( kStdErr, "TCP connect error (%d) %s.", err, mErrorBuffer );
						LOG2( kStdErr, "ConnectTo: connect() error: %d, %s", err, mErrorBuffer );
						return( eDSIPUnreachable );					
						break;
						
					default:	// other errors are serious
						::memset(mErrorBuffer, 0, kTCPErrorBufferLen);
						::strncpy(mErrorBuffer, ::strerror(err), kTCPErrorBufferLen);
	
						LOG2( kStdErr, "TCP connect error (%d) %s.", err, mErrorBuffer );
						LOG2( kStdErr, "ConnectTo: connect() error: %d, %s", err, mErrorBuffer );
						return( eDSTCPSendError );
						break;
				} // switch
			}
			else
			{ // connect succeeded
				if ( (sockfd != 0) && (releaseZeroFD) ) //cleanup the intentional temporary leak of the zero FD
				{
					int rcSock = 0;
					rcSock = ::close( 0 );
					if ( rcSock == -1 )
					{
						::memset( mErrorBuffer, 0, kTCPErrorBufferLen );
						err = errno;
						::strncpy( mErrorBuffer, ::strerror( err ), kTCPErrorBufferLen );
#ifdef DSSERVERTCP
						DbgLog( kLogTCPEndpoint, "DoTCPCloseSocket: close() on unused socket 0 failed with error %d: %s", err, mErrorBuffer );
#else
						LOG2( kStdErr, "DoTCPCloseSocket: close() on unused socket 0 failed with error %d: %s", err, mErrorBuffer );
#endif
					}
					else
					{
#ifdef DSSERVERTCP
						DbgLog( kLogTCPEndpoint, "DoTCPCloseSocket: close() on unused socket 0" );
#else
						LOG( kStdErr, "DoTCPCloseSocket: close() on unused socket 0" );
#endif
					}
				}
				break;
			}
		}
		if (sockfd == 0)
		{
			releaseZeroFD = true;
		}
	} while (sockfd == 0);

	if ( result == 0 )
	{ 
		// connection established, now we can safely copy the network information data members
		// mActive = true;
		::memcpy(&mRemoteSockAddr, &serverAddr, len);
		rc = this->SetSocketOption( mConnectFD, SO_NOSIGPIPE );
		if ( rc != 0 )
		{
			return( eDSTCPSendError );
		}
		LOG2( kStdErr, "Established TCP connection to %d on port %d.", inIPAddress, inPort );
		return(eDSNoErr);
	}
	else
	{
		// may have got to here by timeout
		LOG2( kStdErr, "Unable to connect to %d on port %d.", inIPAddress, inPort );
		return(eDSServerTimeout);
	}
} // ConnectTo

// ----------------------------------------------------------------------------
//	* ListenToPort ()
//
//		- Currently we listen on all network interfaces available. There may
//			 be a need to specify which interface to listen on.
// ----------------------------------------------------------------------------

void DSTCPEndpoint::ListenToPort ( const UInt16 inPort )
{
	this->ListenToPortOnAddress( inPort, INADDR_ANY );
} // ListenToPort


// ----------------------------------------------------------------------------
//	* ListenToPortOnAddress ()
//
//		- We set up to listen to the given port on only one network interface
//			(one address) - Does not accept connection yet, only sets up the port..
// ----------------------------------------------------------------------------

void DSTCPEndpoint::ListenToPortOnAddress ( const UInt16 inPort, const UInt32 inWhichAddress )
{
	int		rc = 0;
	int		sockfd;

	::memset( &mMySockAddr, '\0', sizeof( mMySockAddr ) );
	::memset( &mRemoteSockAddr, '\0', sizeof( mRemoteSockAddr ) );
	mMySockAddr.sin_family		= AF_INET;
	mMySockAddr.sin_addr.s_addr	= htonl( inWhichAddress );
	mMySockAddr.sin_port		= htons( inPort );

	sockfd = this->DoTCPOpenSocket();
	if ( sockfd < 0 )
	{
		throw( (SInt32)eDSTCPReceiveError );
	}
	mListenFD = sockfd;

	rc = this->SetSocketOption( mListenFD, SO_REUSEADDR );
	if ( rc != 0 )
	{
		throw( (SInt32)eDSTCPReceiveError );
	}

	rc = this->SetSocketOption( mListenFD, SO_REUSEPORT );
	if ( rc != 0 )
	{
		throw( (SInt32)eDSTCPReceiveError );
	}

	rc = this->DoTCPBind();
	if ( rc != 0 )
	{
		throw( (SInt32)eDSTCPReceiveError );
	}

	rc = this->DoTCPListen();
	if ( rc != 0 )
	{
		throw( (SInt32)eDSTCPReceiveError );
	}
} //  ListenToPortOnAddress


// ----------------------------------------------------------------------------
//	* AcceptConnection ()
//
// ----------------------------------------------------------------------------

Boolean DSTCPEndpoint::AcceptConnection ()
{
	mConnectFD = 0;

	return( (this->DoTCPAccept() == 0) );

} // AcceptConnection


// ----------------------------------------------------------------------------
//	* SetTimeout ()
//
// ----------------------------------------------------------------------------

void DSTCPEndpoint::SetTimeout ( const int inWhichTimeout, const int inSeconds )
{
	switch (inWhichTimeout)
	{
		case kOpenTimeoutType:
			mOpenTimeout = inSeconds;
			break;
		case kRWTimeoutType:
			mRWTimeout = inSeconds;
			break;
		case kDefaultTimeoutType:
			mDefaultTimeout = inSeconds;
			break;
		default:
			break;
	}
} // SetTimeout


// ----------------------------------------------------------------------------
//	* GetReverseAddressString ()
//
// ----------------------------------------------------------------------------

void DSTCPEndpoint::GetReverseAddressString (	char	*ioBuffer,
												const int	inBufferLen) const
{
	if ( ioBuffer != NULL )
	{
		::strncpy (ioBuffer, mRemoteHostIPString, inBufferLen);
	}
} // GetReverseAddressString


// ----------------------------------------------------------------------------
//	* Connected ()
//
//		- Is the socket connection still open?
// ----------------------------------------------------------------------------

Boolean DSTCPEndpoint::Connected ( void ) const
{
	struct pollfd fdToPoll;
	int result;

	if ( mAborting == true )
	{
		// throw((SInt32)kAbortedWarning);
		return false;
	}

	if ( mConnectFD == -1 )
		return false;
	
	fdToPoll.fd = mConnectFD;
	fdToPoll.events = POLLSTANDARD;
	fdToPoll.revents = 0;
	result = poll( &fdToPoll, 1, 0 );
	if ( result == -1 )
		return false;
	return ( (fdToPoll.revents & POLLHUP) == 0 );
} // Connected


// ----------------------------------------------------------------------------
// ¥ EncryptDataInPlace
//	Encrypt a block.
// ----------------------------------------------------------------------------

void DSTCPEndpoint::EncryptData ( void *inData, const UInt32 inBuffSize, void *&outData, UInt32 &outBuffSize )
{
	// do nothing, only applies to encrypted connections
	outBuffSize = 0;
	return;
}


// ----------------------------------------------------------------------------
// ¥ DecryptDataInPlace
//	Decrypt a block.
// ----------------------------------------------------------------------------

void DSTCPEndpoint::DecryptData ( void *inData, const UInt32 inBuffSize, void *&outData, UInt32 &outBuffSize )
{
	// do nothing, only applies to encrypted connections
	outBuffSize = 0;
	return;
}


// ----------------------------------------------------------------------------
//	* WriteData ()
//
//		- Send data to the connected peer
// ----------------------------------------------------------------------------

UInt32 DSTCPEndpoint::WriteData ( const void *inData, const UInt32 inSize )
{
	struct timeval	tvTimeout	= { mRWTimeout, 0 };
	const char		*aPtr 		= (const char *) inData;
	int				err			= eDSNoErr;
	int				rc			= 0;
	UInt32			dataSize	= inSize;
	UInt32			bytesWrote	= 0;
	fd_set			aWriteSet;

	while ( dataSize > 0 && aPtr != NULL ) 
	{
		struct timeval	tvTimeoutTime;
		::gettimeofday( &tvTimeoutTime, NULL );
		tvTimeoutTime.tv_sec += mRWTimeout;

		tvTimeout.tv_sec = mRWTimeout;
		
		//if ( !this->Connected() )
		//{
			//throw( (SInt32)kConnectionLostWarning );
		//}

		// This ridiculous code is to handle "interrupted system calls"
		// which are frequent on 10.0.1.

		do {
			FD_ZERO( &aWriteSet );
			FD_SET( mConnectFD, &aWriteSet );
			rc = ::select( mConnectFD+1, NULL, &aWriteSet, NULL, &tvTimeout );
	
			// Recompute the timeout and break if timeout exceeded.
			if ( !mAborting && (rc == -1) && (EINTR == errno) )
			{
				struct timeval	tvNow;
				::gettimeofday( &tvNow, NULL );
				timersub( &tvTimeoutTime, &tvNow, &tvTimeout );
				if ( tvTimeout.tv_sec < 0 )
				{
					break;
				}
			}
		} while ( !mAborting && (rc == -1) && (EINTR == errno) );

		if ( mAborting == true )
		{
			throw( (SInt32)kAbortedWarning );
		}

		if ( rc == 0 ) 
		{
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "WriteData(): select() timed out on %s", mRemoteHostIPString );
#else
			LOG1( kStdErr, "WriteData(): select() timed out on %s", mRemoteHostIPString );
#endif
			throw( (SInt32)kTimeoutError );
		}
		else if ( rc == -1 ) 
		{
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "WriteData: select() error %d: %s on %A.\n", errno, ::strerror( errno ), mRemoteHostIPAddr );
#else
			LOG3( kStdErr, "WriteData: select() error %d: %s on %A.\n", errno, ::strerror( errno ), mRemoteHostIPAddr );
#endif
			throw( (SInt32)eDSTCPSendError);
		}
		else if ( FD_ISSET(mConnectFD, &aWriteSet) )
		{
			//TODO do we need a socket level timeout here ie. setsocketopt with SO_SNDTIMEO
			do
			{
				rc = ::sendto(mConnectFD, aPtr, dataSize, 0, NULL, 0);
				if (mAborting == true)
				{
					throw((SInt32)kAbortedWarning);
				}
			} while ( (rc == -1) && (errno == EAGAIN) );
			
			if ( rc == -1 )
			{
				// handle error
				err = errno;
				::memset(mErrorBuffer, 0, kTCPErrorBufferLen);
				::strncpy(mErrorBuffer, ::strerror(err), kTCPErrorBufferLen);
#ifdef DSSERVERTCP
				DbgLog( kLogTCPEndpoint, "WriteData: select() error %d: %s", err, mErrorBuffer );
#else
				LOG2( kStdErr, "WriteData: select() error %d: %s", err, mErrorBuffer );
#endif
				throw( (SInt32)eDSTCPSendError);
			}
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "WriteData(): sent %d bytes with endpoint %d and connectFD %d", rc, (UInt32)this, mConnectFD );
#else
			LOG3( kStdErr, "WriteData(): sent %d bytes with endpoint %d and connectFD %d", rc, (UInt32)this, mConnectFD );
#endif
			dataSize -= rc;
			aPtr += rc;
			bytesWrote += rc;
		} 
	} // while

#ifdef DSSERVERTCP
	DbgLog( kLogTCPEndpoint, "WriteData(): sent %d total bytes with endpoint %d and connectFD %d", bytesWrote, (UInt32)this, mConnectFD );
#else
	LOG3( kStdErr, "WriteData(): sent %d total bytes with endpoint %d and connectFD %d", bytesWrote, (UInt32)this, mConnectFD );
#endif
	return bytesWrote;
} // WriteData

// ----------------------------------------------------------------------------
//	* CloseConnection()
//
// ----------------------------------------------------------------------------

void DSTCPEndpoint::CloseConnection ( void )
{
	if ( mConnectFD > 0 )
	{
		int err = this->DoTCPCloseSocket( mConnectFD );
		if ( err == eDSNoErr )
		{
			mConnectFD = 0;
			mWeHaveClosed = true;
		}
	}
}


// ----------------------------------------------------------------------------
//	* CloseListener()
//
// ----------------------------------------------------------------------------

int DSTCPEndpoint::CloseListener ( void )
{
	int rc = 0;

	if ( mListenFD > 0 )
	{
		rc = this->DoTCPCloseSocket( mListenFD );
		if ( rc == eDSNoErr )
		{
			mListenFD = 0;
		}
	}
	return rc;
} // CloseListener


// ----------------------------------------------------------------------------
//	* Abort ()
//
// ----------------------------------------------------------------------------

inline void DSTCPEndpoint::Abort ( void )
{
#ifdef DSSERVERTCP
	DbgLog( kLogTCPEndpoint, "Aborting a TCPEndpoint..." );
#else
	LOG( kStdErr, "Aborting a TCPEndpoint..." );
#endif
	mAborting = true;
	this->CloseConnection();
} // Abort


// ----------------------------------------------------------------------------
//	Private Methods
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
//	* DoTCPOpenSocket ()
//
//		- Open a new socket
// ----------------------------------------------------------------------------

int DSTCPEndpoint::DoTCPOpenSocket (void)
{
	int		err;
	int		sockfd;

#ifdef DSSERVERTCP
	DbgLog( kLogTCPEndpoint, "Open socket." );
#else
	LOG( kStdErr, "Open socket." );
#endif
	sockfd = ::socket( AF_INET, SOCK_STREAM, 0 );

	if ( sockfd == -1 )
	{
		if ( mAborting == true )
		{
			throw( (SInt32)kAbortedWarning );
		}
		::memset( mErrorBuffer, 0, kTCPErrorBufferLen );
		err = errno;
		::strncpy( mErrorBuffer, ::strerror(err), kTCPErrorBufferLen );
#ifdef DSSERVERTCP
		ErrLog( kLogTCPEndpoint, "Unable to open a socket. error %d: %s", err, mErrorBuffer );
		DbgLog( kLogTCPEndpoint, "DoTCPOpenSocket: socket() error %d: %s", err, mErrorBuffer );
#else
		LOG2( kStdErr, "DoTCPOpenSocket: Unable to open a socket with error %d: %s", err, mErrorBuffer );
#endif
	}
	err = errno;
	if (err != 0)
	{
		::strncpy( mErrorBuffer, ::strerror(err), kTCPErrorBufferLen );
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "DoTCPOpenSocket: socket error %d: %s with sockfd %d", err, mErrorBuffer, sockfd );
#else
		LOG3( kStdErr, "DoTCPOpenSocket: socket error %d: %s with sockfd %d", err, mErrorBuffer, sockfd );
#endif
	}

	return( sockfd );
}

// ----------------------------------------------------------------------------
//	* SetSocketOption ()
//
//		- Set the socket level option
// ----------------------------------------------------------------------------

int DSTCPEndpoint::SetSocketOption ( const int inSocket, const int inSocketOption )
{
	int rc		= 0;
	int err		= 0;
	int val		= 1;
	int len		= sizeof(val);

	if ( inSocket != 0 )
	{
		if ( (inSocket != mListenFD) && (inSocket != mConnectFD) )
		{
#ifdef DSSERVERTCP
			ErrLog( kLogTCPEndpoint, "SetSocketOption: invalid socket: %d", inSocket );
#else
			LOG1( kStdErr, "SetSocketOption: invalid socket: %d", inSocket );
#endif
			return( -1 );
		}

		rc = ::setsockopt( inSocket, SOL_SOCKET, inSocketOption, &val, len );
		if ( rc != 0 ) 
		{
			if ( mAborting == true )
			{
				throw( (SInt32)kAbortedWarning );
			}

			::memset( mErrorBuffer, 0, kTCPErrorBufferLen );

			err = errno;
			::strncpy( mErrorBuffer, ::strerror( errno ), kTCPErrorBufferLen );

#ifdef DSSERVERTCP
			ErrLog( kLogTCPEndpoint, "Unable to set socket option: Message: \"%s\", Error: %d", mErrorBuffer, err );
			DbgLog( kLogTCPEndpoint, "Unable to set socket option: Message: \"%s\", Error: %d", mErrorBuffer, err );
#else
			LOG2( kStdErr, "Unable to set socket option: Message: \"%s\", Error: %d", mErrorBuffer, err );
#endif
		}
	}

	return( 0 );

} // SetSocketOption


// ----------------------------------------------------------------------------
//	* DoTCPBind ()
//
//		- Bind a socket to a port
// ----------------------------------------------------------------------------

int DSTCPEndpoint::DoTCPBind ( void )
{
			 int		err = eDSNoErr;
	volatile int		rc = 0;
 
	if ( mAborting == true ) 
	{
		throw( (SInt32)kAbortedWarning );
	}

	rc = ::bind( mListenFD, (struct sockaddr *)&mMySockAddr, sizeof(mMySockAddr) );
	if ( rc != 0 ) 
	{
		err = errno;
		::memset( mErrorBuffer, 0, kTCPErrorBufferLen );
		::strncpy( mErrorBuffer, ::strerror( err ), kTCPErrorBufferLen );
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "DSTCPEndpoint: bind() error %d: %s", err, mErrorBuffer );
#else
		LOG2( kStdErr, "DSTCPEndpoint: bind() error %d: %s", err, mErrorBuffer );
#endif
	}

	return( err );
} //DoTCPBind

// ----------------------------------------------------------------------------
//	* DoTCPListen ()
//
// ----------------------------------------------------------------------------

int DSTCPEndpoint::DoTCPListen ( void )
{
	int err = eDSNoErr;
	int rc;

	rc = ::listen( mListenFD, kTCPMaxListenBackLog );
	if ( rc == -1 )
	{
		if ( mAborting == true )
		{
			return( rc );
		}
		::memset( mErrorBuffer, 0, kTCPErrorBufferLen );
		err = errno;
		::strncpy(mErrorBuffer, ::strerror(err), kTCPErrorBufferLen);
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "DoTCPListen: listen() error %d: %s", err, mErrorBuffer );
#else
		LOG2( kStdErr, "DoTCPListen: listen() error %d: %s", err, mErrorBuffer );
#endif
	}
	return (err);
} //DoTCPlisten

// ----------------------------------------------------------------------------
//	* DoTCPAccept ()
//
//		- Wait for connection on the listening port. We use select() to avoid
//			blocking in user time when no connection is available. The kernel
//			will wake us up when a connection has been completed.
// ----------------------------------------------------------------------------

int DSTCPEndpoint::DoTCPAccept ( void )
{
	int			err		= eDSNoErr;
	socklen_t	aLen	= sizeof( mRemoteSockAddr );
	int			rc		= eDSNoErr;
	fd_set		readSet;
	
	do {
		FD_ZERO( &readSet );
		FD_SET( mListenFD, &readSet );

		// select blocks in kernel until a connection has been established.
		rc = ::select( mListenFD + 1, &readSet, NULL, NULL, NULL );
		if ( mAborting == true )
		{
			throw( (SInt32)kAbortedWarning );
		}

		if ( rc == -1 )
		{
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "DoTCPAccept: select() returned error %d: %s\n", errno, ::strerror( errno ) );
#else
			LOG2( kStdErr, "DoTCPAccept: select() returned error %d: %s\n", errno, ::strerror( errno ) );
#endif

			if ( errno != EINTR )
			{
				throw( (SInt32)eDSTCPReceiveError );
			}

			// Clear the bit and try again if call was interrupted.
			FD_CLR( mListenFD, &readSet );
		}
	} while ( !FD_ISSET( mListenFD, &readSet ) );

	mConnectFD = ::accept( mListenFD, (struct sockaddr *)&mRemoteSockAddr, (socklen_t*)&aLen );

	if ( mAborting == true )
	{
		throw( (SInt32)kAbortedWarning );
	}

	if ( mConnectFD == -1 )
	{
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint,  "DoTCPAccept: select error %d: %s", errno, ::strerror( err ) );
#else
		LOG2( kStdErr,  "DoTCPAccept: select error %d: %s", errno, ::strerror( err ) );
#endif
		throw( (SInt32)eDSTCPReceiveError );
	}

	rc = this->SetSocketOption( mListenFD, SO_KEEPALIVE );
	if ( rc != 0 )
	{
		throw( (SInt32)eDSTCPReceiveError );
	}
	rc = this->SetSocketOption( mListenFD, SO_NOSIGPIPE );
	if ( rc != 0 )
	{
		throw( (SInt32)eDSTCPReceiveError );
	}

	if ( err == eDSNoErr )
	{
		// connection has established. remember the remote host.
		mRemoteHostIPAddr = ntohl( mRemoteSockAddr.sin_addr.s_addr );
		DSNetworkUtilities::IPAddrToString( mRemoteHostIPAddr, mRemoteHostIPString, MAXIPADDRSTRLEN );
	}
 
	return( err );

} // DoTCPAccept


// ----------------------------------------------------------------------------
//	* DoTCPCloseSocket ()
//
// ----------------------------------------------------------------------------

int DSTCPEndpoint::DoTCPCloseSocket ( const int inSockFD )
{
	int err = eDSNoErr;
	int rc	= 0;

	if ( inSockFD <= 0 )
	{
		return( eDSNoErr );
	}

#ifdef DSSERVERTCP
	DbgLog( kLogTCPEndpoint, "Close socket." );
#endif
	rc = ::close( inSockFD );
	if ( rc == -1 )
	{
		::memset( mErrorBuffer, 0, kTCPErrorBufferLen );
		err = errno;
		::strncpy( mErrorBuffer, ::strerror( err ), kTCPErrorBufferLen );
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "DoTCPCloseSocket: close() on socket %d failed with error %d: %s", inSockFD, err, mErrorBuffer );
#else
		LOG3( kStdErr, "DoTCPCloseSocket: close() on socket %d failed with error %d: %s", inSockFD, err, mErrorBuffer );
#endif
	}

	return( err );

} // DoTCPCloseSocket


// ----------------------------------------------------------------------------
//	* DoTCPRecvFrom ()
// ----------------------------------------------------------------------------

UInt32 DSTCPEndpoint::DoTCPRecvFrom ( void *ioBuffer, const UInt32 inBufferSize )
{
	int				rc;
	int				err;
	int				bytesRead = 0;
	fd_set			readSet;
	struct timeval	tvTimeout		= { mRWTimeout, 0 };
	struct timeval	tvTimeoutTime	= { mRWTimeout, 0 };
	time_t			timeoutTime;

	timeoutTime = ::time( NULL ) + mRWTimeout;

	::gettimeofday (&tvTimeoutTime, NULL);
	tvTimeoutTime.tv_sec += mRWTimeout;
	
	do {
		FD_ZERO( &readSet );
		FD_SET( mConnectFD, &readSet );

		rc = ::select( mConnectFD+1, &readSet, NULL, NULL, &tvTimeout );

		// Recompute the timeout and break if timeout exceeded.
		if ( !mAborting && (rc == -1) && (EINTR == errno) )
		{
			struct timeval	tvNow;
			::gettimeofday( &tvNow, NULL );
			timersub( &tvTimeoutTime, &tvNow, &tvTimeout );
			if ( tvTimeout.tv_sec < 0 )
			{
#ifdef DSSERVERTCP
				DbgLog( kLogTCPEndpoint, "DoTCPRecvFrom: connection timeout?" );
#else
				LOG( kStdErr, "DoTCPRecvFrom: connection timeout?" );
#endif
				throw( (SInt32)eDSTCPReceiveError );
			}
		}
	} while ( !mAborting && (rc == -1) && (EINTR == errno) );

	if ( mAborting == true )
	{
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "DSTCPEndpoint::DoTCPRecvFrom(): We have been aborted." );
#else
		LOG( kStdErr, "DSTCPEndpoint::DoTCPRecvFrom(): We have been aborted." );
#endif
		throw( (SInt32)kAbortedWarning );
	}

	if ( rc == 0 )
	{
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "DoTCPRecvFrom: timed out waiting for response." );
#else
			LOG( kStdErr, "DoTCPRecvFrom: timed out waiting for response." );
#endif
			throw( (SInt32)kTimeoutError );
	}
	else if ( rc == -1 )
	{
 		err = errno;
		::memset(mErrorBuffer, 0, kTCPErrorBufferLen);
		::strncpy(mErrorBuffer, ::strerror(err), kTCPErrorBufferLen);
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "DoTCPRecvFrom: select() error %d: %s", err, mErrorBuffer );
#else
		LOG2( kStdErr, "DoTCPRecvFrom: select() error %d: %s", err, mErrorBuffer );
#endif
		throw((SInt32)eDSTCPReceiveError);
	} 
	else if ( FD_ISSET(mConnectFD, &readSet) )
	{
		// socket is ready for read - blocks until all read
		//KW need a socket level timeout for this read to complete ie. setsocketopt call with SO_RCVTIMEO
		//bytesRead = ::recvfrom( mConnectFD, ioBuffer, inBufferSize, MSG_DONTWAIT, NULL, NULL );
		do
		{
			bytesRead = ::recvfrom( mConnectFD, ioBuffer, inBufferSize, MSG_WAITALL, NULL, NULL );
	
			if ( mAborting == true )
			{
				throw( (SInt32)kAbortedWarning );
			}
		} while ( (bytesRead == -1) && (errno == EAGAIN) );
		
		if ( bytesRead == 0 )
		{
			// connection closed from the other side
			err = errno;
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "DoTCPRecvFrom: connection closed by peer - error is %d", err );
#else
			LOG1( kStdErr, "DoTCPRecvFrom: connection closed by peer - error is %d", err );
#endif
			throw( (SInt32)eDSTCPReceiveError );
		}
		else if ( bytesRead == -1 )
		{
			::memset( mErrorBuffer, 0, kTCPErrorBufferLen );
			err = errno;
			::strncpy( mErrorBuffer, ::strerror(err), kTCPErrorBufferLen );
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "DoTCPRecvFrom: recvfrom error %d: %s", err, mErrorBuffer );
#else
			LOG2( kStdErr, "DoTCPRecvFrom: recvfrom error %d: %s", err, mErrorBuffer );
#endif
			throw( (SInt32)eDSTCPReceiveError );
		}
		else
		{
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "DoTCPRecvFrom(): received %d bytes with endpoint %d and connectFD %d", bytesRead, (UInt32)this, mConnectFD );
#else
			LOG3( kStdErr, "DoTCPRecvFrom(): received %d bytes with endpoint %d and connectFD %d", bytesRead, (UInt32)this, mConnectFD );
#endif
		}
	}

	return( (UInt32)bytesRead );

} // DoTCPRecvFrom

//------------------------------------------------------------------------------
//	* GetClientMessage *****ONLY used by DSTCPConnection class
//
//------------------------------------------------------------------------------

void * DSTCPEndpoint::GetClientMessage ( void )
{
	sComData			   *pOutMsg			= nil;
	sComProxyData		   *pOutProxyMsg	= nil;
	void				   *tmpOutMsg		= nil;
	UInt32					buffLen			= 0;
	UInt32					readBytes		= 0;
	SInt32					siResult		= eDSNoErr;
	void				   *inBuffer		= nil;
	UInt32					inLength		= 0;

	//need to read a tag and then a buffer length
	siResult = SyncToMessageBody(true, &inLength);
	
	if ( (siResult == eDSNoErr) && (inLength != 0) )
	{
		//then alloc a data structure
		inBuffer = (void *) calloc(1, inLength);
		
		if (inBuffer != nil)
		{
			try
			{
				//go ahead and read the message body of length inLength
				//put the message data into inBuffer
				readBytes = DoTCPRecvFrom(inBuffer, inLength);
				if (readBytes != inLength)
				{
					//TODO need to recover somehow
	#ifdef DSSERVERTCP
					ErrLog( kLogTCPEndpoint, "GetClientMessage: Couldn't read entire message block" );
	#endif
					free(inBuffer);
					inBuffer = nil;
				}
				else
				{
					DecryptData(inBuffer, inLength, tmpOutMsg, buffLen);
					pOutProxyMsg = (sComProxyData *) tmpOutMsg;
					if (buffLen == 0)
					{
						pOutProxyMsg= (sComProxyData *)inBuffer;
						inBuffer 	= nil;
						buffLen		= inLength;
					}
					if (pOutProxyMsg != nil)
					{
						if (NXSwapBigIntToHost(pOutProxyMsg->fDataSize) > buffLen - sizeof(sComProxyData))
						{
							//fprintf(stderr,"bad message fDataSize!\n");
							//let's just throw the message out since it is probably malformed
							free(pOutProxyMsg);
							pOutProxyMsg = nil;
						}
						//else
						//{
							//place the endpoint handle into the pOutProxyMsg struct
							//don't create a duplicate
							//pOutProxyMsg->fPort = (UInt32) this; //don't need this since using direct dispatch
							//KW use of this endpoint needs to be mutex protected?
							//not likely since we force a single thread on the open API connection
						//}
					}
				}
			}
			catch( SInt32 err )
			{
				if (pOutProxyMsg != nil)
				{
					free(pOutProxyMsg);
					pOutProxyMsg = nil;
				}
				siResult = eDSTCPReceiveError; //not actually used
			}
			free(inBuffer);
			inBuffer = nil;
		}//if (inBuffer != nil)
	}
#ifndef __BIG_ENDIAN__
	DSTCPEndian swapper(pOutProxyMsg, kDSSwapToHost);
	swapper.AddIPAndPort( mRemoteHostIPAddr, ntohs( mRemoteSockAddr.sin_port ));
    swapper.SwapMessage();
#endif
    
	pOutMsg = AllocFromProxyStruct( pOutProxyMsg );
	if (pOutProxyMsg != nil)
	{
		free(pOutProxyMsg);
		pOutProxyMsg = nil;
	}
    return( pOutMsg );

} // GetClientMessage


// ----------------------------------------------------------------------------
// * SyncToMessageBody():	read tag and buffer length from the endpoint
//							returns the buffer length
// ----------------------------------------------------------------------------

SInt32 DSTCPEndpoint::SyncToMessageBody(const Boolean inStripLeadZeroes, UInt32 *outBuffLen)
{
	UInt32			index = 0;
	UInt32			readBytes = 0;
	UInt32			newLen = 0;
	UInt32			curIndex = kDSTCPEndpointMessageTagSize;
	char		   *ourBuffer;
	UInt32			buffLen = 0;
	SInt32			result	= eDSNoErr;

	ourBuffer = (char *) calloc(kDSTCPEndpointMaxMessageSize, 1);
	
	try 
	{
		readBytes = DoTCPRecvFrom(ourBuffer, kDSTCPEndpointMessageTagSize);
		
		if (readBytes != kDSTCPEndpointMessageTagSize)
		{
			//couldn't read even the minimum tag size so return zero
			free(ourBuffer);
			*outBuffLen = 0;
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "SyncToMessageBody: attempted read of %d bytes failed with %d bytes read", kDSTCPEndpointMessageTagSize, readBytes );
#else
			LOG2( kStdErr, "SyncToMessageBody: attempted read of %d bytes failed with %d bytes read", kDSTCPEndpointMessageTagSize, readBytes );
#endif
			return eDSTCPReceiveError;
		}
	}
	catch( SInt32 err )
	{
		if (ourBuffer != nil)
		{
			free(ourBuffer);
		}
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "SyncToMessageBody: attempted read of %d bytes failed in DoTCPRecvFrom with error %d", kDSTCPEndpointMessageTagSize, err );
#else
		LOG2( kStdErr, "SyncToMessageBody: attempted read of %d bytes failed in DoTCPRecvFrom with error %d", kDSTCPEndpointMessageTagSize, err );
#endif
		return eDSTCPReceiveError;
	}
	
	//TODO need to handle corrupted data? ie. continue searching for tag?
	if (inStripLeadZeroes)
	{
		// strip any leading zeroes
		for ( index=0; (index < kDSTCPEndpointMessageTagSize) && (ourBuffer[index] == 0x00); index++ )
		{
			readBytes--;
		}
	
		try
		{
			//keep reading one at a time if we encounter any leading zeroes
			//don't expect this to ever happen
			while ( (readBytes < kDSTCPEndpointMessageTagSize) && (curIndex < kDSTCPEndpointMaxMessageSize) )
			{
				newLen = DoTCPRecvFrom(ourBuffer+curIndex, 1);
				if (newLen != 1)
				{
					//couldn't read even one byte so return zero
					free(ourBuffer);
					*outBuffLen = 0;
#ifdef DSSERVERTCP
					DbgLog( kLogTCPEndpoint, "SyncToMessageBody: align frame by skipping leading zeroes - attempted read of one byte failed with %d bytes read", newLen );
#else
					LOG1( kStdErr, "SyncToMessageBody: align frame by skipping leading zeroes - attempted read of one byte failed with %d bytes read", newLen );
#endif
					return eDSTCPReceiveError;
				}
				if (ourBuffer[curIndex] != 0x00)
				{
					readBytes++;
				}
				curIndex++;
			}
		}		
		catch( SInt32 err )
		{
			if (ourBuffer != nil)
			{
				free(ourBuffer);
			}
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "SyncToMessageBody: align frame by skipping leading zeroes - failed in DoTCPRecvFrom with error %l", err );
#else
			LOG1( kStdErr, "SyncToMessageBody: align frame by skipping leading zeroes - failed in DoTCPRecvFrom with error %l", err );
#endif
			return eDSTCPReceiveError;
		}
	}

	//check if we found the tag we are looking for
	if ( (readBytes == kDSTCPEndpointMessageTagSize) && (strncmp(ourBuffer+curIndex-kDSTCPEndpointMessageTagSize,"DSPX",kDSTCPEndpointMessageTagSize) == 0) )
	{
		try
		{
			//now get the buffer length
			//check here to determine if buffLen is at least sizeof(sComData)
			newLen = DoTCPRecvFrom(&buffLen , 4);
			if (newLen != 4) //|| (buffLen < sizeof(sComData)) )
			{
#ifdef DSSERVERTCP
				DbgLog( kLogTCPEndpoint, "SyncToMessageBody: get the buffer length - attempted read of four bytes failed with %d bytes read", newLen );
#else
				LOG1( kStdErr, "SyncToMessageBody: get the buffer length - attempted read of four bytes failed with %d bytes read", newLen );
#endif
				*outBuffLen = 0;
			}
			else
			{
				*outBuffLen = NXSwapBigIntToHost(buffLen);
			}
		}		
		catch( SInt32 err )
		{
			if (ourBuffer != nil)
			{
				free(ourBuffer);
			}
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "SyncToMessageBody: get the buffer length - failed in DoTCPRecvFrom with error %l", err );
#else
			LOG1( kStdErr, "SyncToMessageBody: get the buffer length - failed in DoTCPRecvFrom with error %l", err );
#endif
			return eDSTCPReceiveError;
		}
	}
	
	free(ourBuffer);
	return result;

} // SyncToMessageBody


//------------------------------------------------------------------------------
//	* SendClientReply *****ONLY used by CHandler class
//
//------------------------------------------------------------------------------

SInt32 DSTCPEndpoint::SendClientReply ( void *inMsg )
{
	UInt32                  messageSize = 0;
	sComProxyData  *inProxyMsg  = nil;
	SInt32                  sendResult  = eDSNoErr;

	inProxyMsg = AllocToProxyStruct( (sComData *)inMsg );
	//let us only send the data that is present and not the entire buffer
	inProxyMsg->fDataSize = inProxyMsg->fDataLength;
	messageSize = sizeof(sComProxyData) + inProxyMsg->fDataLength;
#ifndef __BIG_ENDIAN__
	DSTCPEndian swapper(inProxyMsg, kDSSwapToBig);
	swapper.AddIPAndPort( mRemoteHostIPAddr, ntohs( mRemoteSockAddr.sin_port ));
	swapper.SwapMessage();
#endif
	sendResult = SendBuffer(inProxyMsg, messageSize);
	free(inProxyMsg);
	inProxyMsg = nil;
	
	return(sendResult);
} // SendClientReply


//------------------------------------------------------------------------------
//	* SendServerMessage *****ONLY used by CMessaging class
//
//------------------------------------------------------------------------------

SInt32 DSTCPEndpoint::SendServerMessage ( void *inMsg )
{
	UInt32                  messageSize = 0;
	sComProxyData  *inProxyMsg  = nil;
	SInt32                  sendResult  = eDSNoErr;

	inProxyMsg = AllocToProxyStruct( (sComData *)inMsg );
	//let us only send the data that is present and not the entire buffer
	inProxyMsg->fDataSize = inProxyMsg->fDataLength;
	messageSize = sizeof(sComProxyData) + inProxyMsg->fDataLength;
#ifndef __BIG_ENDIAN__
	DSTCPEndian swapper(inProxyMsg, kDSSwapToBig);
	swapper.SwapMessage();
#endif
	sendResult = SendBuffer(inProxyMsg, messageSize);
	free(inProxyMsg);
	inProxyMsg = nil;
	
	return(sendResult);
} // SendServerMessage


//------------------------------------------------------------------------------
//	* SendBuffer
//
//------------------------------------------------------------------------------

SInt32 DSTCPEndpoint::SendBuffer ( void *inBuffer, UInt32 inLength )
{
	SInt32				result		= eDSNoErr;
	char			   *sendBuffer	= nil;
	UInt32				dataBuffLen	= 0;
	UInt32				sendBuffLen	= 0;
	UInt32				sentBytes	= 0;
	void			   *outBuffer	= nil;
	UInt32				outLength	= 0;
	bool				bFreeOutBuff= true;

	EncryptData(inBuffer, inLength, outBuffer, outLength);
	if (outLength == 0)
	{
		outBuffer		= inBuffer;
		outLength		= inLength;
		bFreeOutBuff	= false;
	}
	//need to build the return message with the following parts
	//tag, data length, data
	//use char * variable of length kDSTCPEndpointMessageTagSize + 4 + sizeof(sComData) + pData->fDataSize
	dataBuffLen = outLength;
	sendBuffLen = kDSTCPEndpointMessageTagSize + 4 + dataBuffLen;
	sendBuffer = (char *)calloc(sendBuffLen, 1);
	strcpy(sendBuffer,"DSPX");
	*(SInt32*)(sendBuffer+kDSTCPEndpointMessageTagSize) = NXSwapHostIntToBig(dataBuffLen);
	memcpy(sendBuffer+kDSTCPEndpointMessageTagSize+4, outBuffer, outLength);

	try
	{
		sentBytes = WriteData(sendBuffer, sendBuffLen);
		//don't worry about "\r\n" at the end of the send?
		if (sentBytes != sendBuffLen)
		{
			//TODO need to cleanup on error here
#ifdef DSSERVERTCP
			DbgLog( kLogTCPEndpoint, "SendBuffer(): attempted send of %d bytes only sent %d bytes", sendBuffLen, sentBytes );
#else
			LOG2( kStdErr, "SendBuffer(): attempted send of %d bytes only sent %d bytes", sendBuffLen, sentBytes );
#endif
			result = eDSTCPSendError;
		}
	}
	catch( SInt32 err )
	{
#ifdef DSSERVERTCP
		DbgLog( kLogTCPEndpoint, "SendBuffer(): failed send of %d bytes", sendBuffLen );
#else
		LOG1( kStdErr, "SendBuffer(): failed send of %d bytes", sendBuffLen );
#endif
		result = eDSTCPSendError;
	}
	
	if (sendBuffer != NULL)
	{
		free(sendBuffer);
		sendBuffer = NULL;
	}
	if (bFreeOutBuff)
	{
		if (outBuffer != NULL)
		{
			free(outBuffer);
			outBuffer = NULL;
		}
	}

	return( result );

} // SendBuffer


//------------------------------------------------------------------------------
//	* GetServerReply *****ONLY used by CMessaging class
//    if the outMsg needs to grow we can allocate a bigger buffer here
//    postcondition: *outMsg != nil
//------------------------------------------------------------------------------

SInt32 DSTCPEndpoint::GetServerReply ( sComData **outMsg )
{
	SInt32					siResult		= eDSNoErr;
	UInt32					buffLen			= 0;
	UInt32					readBytes 		= 0;
	void				   *inBuffer		= nil;
	UInt32					inLength		= 0;
	sComProxyData		   *outProxyMsg		= nil;

	//need to read a tag and then a buffer length
	siResult = SyncToMessageBody(true, &inLength);
	
	if ( (siResult == eDSNoErr) && (inLength != 0) )
	{
		try
		{
			//go ahead and read the message body of length inLength
			//put the message data into inBuffer
			inBuffer = (void *)calloc(1,inLength);
			readBytes = DoTCPRecvFrom(inBuffer, inLength);
			if (readBytes != inLength)
			{
				//TODO need to recover somehow
				LOG( kStdErr, "GetServerReply: Couldn't read entire message block" );
				siResult = eDSTCPReceiveError;
			}
			else
			{
				void *tmpOutMsg = nil;
				DecryptData(inBuffer, inLength, tmpOutMsg, buffLen);
				outProxyMsg = (sComProxyData *)tmpOutMsg;
				if (buffLen == 0)
				{
					free(outProxyMsg);
					outProxyMsg	= (sComProxyData *)inBuffer;
					inBuffer	= nil;
					buffLen		= inLength;
				}
			}
		}
		catch( SInt32 err )
		{
			siResult = eDSTCPReceiveError;
		}
	}
	
	if (inBuffer != nil)
	{
		free(inBuffer);
		inBuffer = nil;
	}
	
    if (outProxyMsg != nil)
    {
#ifndef __BIG_ENDIAN__
        DSTCPEndian swapper(outProxyMsg, kDSSwapToHost);
        swapper.SwapMessage();
#endif
		*outMsg = AllocFromProxyStruct( outProxyMsg );
		free(outProxyMsg);
		outProxyMsg = nil;
    }

	return( siResult );

} // GetServerReply


//------------------------------------------------------------------------------
//	* GetRemoteHostIPAddress
//
//------------------------------------------------------------------------------

UInt32 DSTCPEndpoint::GetRemoteHostIPAddress ( void )
{
	return mRemoteHostIPAddr;
}

//------------------------------------------------------------------------------
//	* GetRemoteHostPort
//
//------------------------------------------------------------------------------

UInt16 DSTCPEndpoint::GetRemoteHostPort ( void )
{
	return ( ntohs( mRemoteSockAddr.sin_port ) );
}

//------------------------------------------------------------------------------
//     * AllocToProxyStruct
//
//------------------------------------------------------------------------------

sComProxyData* DSTCPEndpoint::AllocToProxyStruct ( sComData *inDataMsg )
{
	sComProxyData      *outProxyDataMsg = nil;
	int					objIndex;

	if (inDataMsg != nil)
	{
		outProxyDataMsg = (sComProxyData *)calloc( 1, sizeof(sComProxyData) + inDataMsg->fDataSize );
		
		// this is copying the head data from sComProxyData to sComData
		bcopy( inDataMsg, outProxyDataMsg, (char *)(outProxyDataMsg->obj) - (char *)outProxyDataMsg );

		// this copies the sObject and the actual data
		bcopy( inDataMsg->obj, outProxyDataMsg->obj, kObjSize + inDataMsg->fDataSize );
		
		//need to adjust the offsets since they are relative to the start of the message
		for ( objIndex = 0; objIndex < 10; objIndex++ )
		{
			if ( outProxyDataMsg->obj[ objIndex ].offset != 0 )
			{
				outProxyDataMsg->obj[ objIndex ].offset -= sizeof(sComData) - sizeof(sComProxyData);
			}
		}
	}

	return ( outProxyDataMsg );
}

//------------------------------------------------------------------------------
//     * AllocFromProxyStruct
//
//------------------------------------------------------------------------------

sComData* DSTCPEndpoint::AllocFromProxyStruct ( sComProxyData *inProxyDataMsg )
{
	sComData                   *outDataMsg          = nil;
	int							objIndex;

	if (inProxyDataMsg != nil)
	{
		outDataMsg = (sComData *)calloc( 1, sizeof(sComData) + inProxyDataMsg->fDataSize );
		
		// this is copying the head data from sComProxyData to sComData
		bcopy( inProxyDataMsg, outDataMsg, (char *)(inProxyDataMsg->obj) - (char *)inProxyDataMsg );
		
		// this copies the sObject and the actual data
		bcopy( inProxyDataMsg->obj, outDataMsg->obj, kObjSize + inProxyDataMsg->fDataSize );
		
		//need to adjust the offsets since they are relative to the start of the message
		for ( objIndex = 0; objIndex < 10; objIndex++ )
		{
			if ( outDataMsg->obj[ objIndex ].offset != 0 )
			{
				outDataMsg->obj[ objIndex ].offset += sizeof(sComData) - sizeof(sComProxyData);
			}
		}
		
		// set the effective UIDs to -2...
		outDataMsg->fUID = outDataMsg->fEffectiveUID = (uid_t) -2;
	}

	return ( outDataMsg );
}
