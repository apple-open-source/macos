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
 * @header CClientEndPoint
 * Mach endpoint for DirectoryService Framework.
 */

#include <stdio.h>
#include <stdlib.h>				// for malloc()
#include <string.h>
#include <unistd.h>			   	// for getpid()
#include <time.h>				// for time()
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "CClientEndPoint.h"
#include "DirServicesTypes.h"
#include "PrivateTypes.h"

#define kMsgSize	sizeof( sComData )
#define kIPCMsgSize	sizeof( sIPCMsg )

uInt32 CClientEndPoint::fMessageID	 = 0; //start with zero since GetMessageID pre-increments

extern sInt32 gProcessPID;

//------------------------------------------------------------------------------
//	* CClientEndPoint:
//
//------------------------------------------------------------------------------

CClientEndPoint::CClientEndPoint ( const char *inSrvrName )
{
	fSrvrName = nil;

	if ( inSrvrName != nil )
	{
		fSrvrName = new char[ ::strlen( inSrvrName ) + 1 ];
		if( fSrvrName == nil ) throw((sInt32)eMemoryAllocError);

		::strcpy( fSrvrName, inSrvrName );
	}

	fServerPort		= 0;
	fReplyPort		= 0;

} // CClientEndPoint


//------------------------------------------------------------------------------
//	* ~CClientEndPoint:
//
//------------------------------------------------------------------------------

CClientEndPoint::~CClientEndPoint ( void )
{
	kern_return_t	result	= eDSNoErr;

	if ( fReplyPort != 0)
	{
		result = mach_port_destroy( mach_task_self(), fReplyPort );
		if ( result != eDSNoErr )
		{
			LOG3( kStdErr, "*** DS Error in: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
		}
	}
	if ( fSrvrName != nil )
	{
		delete( fSrvrName );
		fSrvrName = nil;
	}
} // ~CClientEndPoint


//------------------------------------------------------------------------------
//	* ~CClientEndPoint:
//
//------------------------------------------------------------------------------

uInt32 CClientEndPoint::GetMessageID ( void )
{
	return( ++fMessageID );
} // GetMessageID


//------------------------------------------------------------------------------
//	* Initialize
//
//------------------------------------------------------------------------------

sInt32 CClientEndPoint::Initialize ( void )
{
	kern_return_t	result	= eDSNoErr;

	try
	{
		result = mach_port_allocate( mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &fReplyPort );
		if ( result != eDSNoErr )
		{
			LOG3( kStdErr, "*** DS Error with mach_port_allocate: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
			throw( (sInt32)result );
		}
	}

	catch( sInt32 err )
	{
		LOG3( kStdErr, "*** DS Error in Initialize: %s at: %d: Exception = %d\n", __FILE__, __LINE__, err );
		result = err;
	}

	return( result );

} // Initialize


//------------------------------------------------------------------------------
//	* CheckForServer
//
//------------------------------------------------------------------------------

sInt32 CClientEndPoint:: CheckForServer ( void )
{
	kern_return_t	result	= eDSNoErr;
	mach_port_t		bsPort	= 0;

	try
	{
		if (fSrvrName != nil)
		{
			result = task_get_bootstrap_port( mach_task_self(), &bsPort );
			if ( result != eDSNoErr )
			{
				LOG3( kStdErr, "*** DS Error with task_get_bootstrap_port: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
				throw( (sInt32)result );
			}

			fServerPort = 0;
			result = bootstrap_look_up( bsPort, fSrvrName, &fServerPort );
			if ( result != eDSNoErr )
			{
				LOG3( kStdErr, "*** DS Error with bootstrap_look_up: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
				throw( (sInt32)result );
			}
		}
	}

	catch( sInt32 err )
	{
		LOG3( kStdErr, "*** DS Error in CheckForServer: %s at: %d: Exception = %d\n", __FILE__, __LINE__, err );
		result = err;
	}

	return( result );

} // CheckForServer


//------------------------------------------------------------------------------
//	* SendServerMessage
//
//------------------------------------------------------------------------------

sInt32 CClientEndPoint::SendServerMessage ( sComData *inMsg )
{
	sInt32				result		= MACH_MSG_SUCCESS;
	uInt32				offset		= 0;
	sInt32				bytesLeft	= 0;
	sIPCMsg				msg;

	msg.fHeader.msgh_bits			= MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND );
	msg.fHeader.msgh_size			= kIPCMsgSize - sizeof( mach_msg_security_trailer_t );
	msg.fHeader.msgh_id				= GetMessageID();
	msg.fHeader.msgh_remote_port	= fServerPort;
	msg.fHeader.msgh_local_port		= fReplyPort;

	msg.fMsgType	= inMsg->type.msgt_name;
	msg.fCount		= 1;
	msg.fPort		= fReplyPort;
	msg.fPID		= gProcessPID; //::getpid();
	msg.fMsgID		= ::time( nil ) + fMessageID;
	msg.fOf			= inMsg->fDataLength / kMsgBlockSize;
	
	if ((inMsg->fDataLength % kMsgBlockSize) != 0)
	{
		msg.fOf++;
	}

	//here for the client fDataSize is the
	//allocated buffer size of the data buffer which
	//will never be zero but we check to make sure a
	//message is sent anyways
	if (msg.fOf == 0)
	{
		msg.fOf = 1;
	}

	::memcpy( msg.obj, inMsg->obj, kObjSize );

	bytesLeft = inMsg->fDataLength;

	while ( (msg.fCount <= msg.fOf) && (result == err_none) )
	{
		::memset( msg.fData, 0, kIPCMsgLen );

		if ( bytesLeft >= (sInt32)kMsgBlockSize )
		{
			::memcpy( msg.fData, (char *)(inMsg->data) + offset, kMsgBlockSize );

			bytesLeft -= kMsgBlockSize;
		}
		else
		{
			if ( bytesLeft != 0 )
			{
				::memcpy( msg.fData, (char *)(inMsg->data) + offset, bytesLeft );
			}
			bytesLeft = 0;
		}

		result = ::mach_msg(	(mach_msg_header_t *)&msg, MACH_SEND_MSG | MACH_SEND_TIMEOUT | MACH_SEND_INTERRUPT,
								msg.fHeader.msgh_size, 0, MACH_PORT_NULL, 300 * 1000, MACH_PORT_NULL );
		if ( result == MACH_MSG_SUCCESS )
		{
			msg.fCount++;
			offset += kMsgBlockSize;
		}
		else
		{
			LOG3( kStdErr, "*** DS Error in: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
			if (result == MACH_SEND_TIMED_OUT)
			{
				result = eDSServerTimeout;
			}
			else if ( (result == MACH_SEND_INTERRUPTED) || (result == MACH_SEND_INVALID_DEST) || (result == MACH_SEND_INVALID_REPLY) )
			{
				result = eDSCannotAccessSession;
			}
			else
			{
				result = eIPCSendError;
			}
		}
	}

	return( result );

} // SendServerMessage


//------------------------------------------------------------------------------
//	* GetServerReply
//
//------------------------------------------------------------------------------

sInt32 CClientEndPoint::GetServerReply ( sComData **outMsg )
{
	sInt32				result		= MACH_MSG_SUCCESS;
	uInt32				offset		= 0;
	uInt32				last		= 0;
	bool				done		= false;
	sIPCMsg				msg;
	sComData		   *aMsg		= nil;

	while ( !done && (result == MACH_MSG_SUCCESS) )
	{
		::memset( msg.fData, 0, kIPCMsgLen );

		result = ::mach_msg( (mach_msg_header_t *)&msg, MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_INTERRUPT,
								0, kIPCMsgSize, fReplyPort, 300 * 1000, MACH_PORT_NULL );
		if ( (	result == MACH_MSG_SUCCESS )
				&& (msg.fHeader.msgh_size == (kIPCMsgSize - sizeof( mach_msg_security_trailer_t ))) )
		{
			// let's make sure that this message is actually the intended reply
			// if we timed out any requests we may need to ignore some messages in order to catch up
			if ( ((uInt32)msg.fHeader.msgh_id) != fMessageID )
			{
				// if it is an old message, skip it and look for the appropriate reply
				continue;
			}
			if ( msg.fCount == 1 )
			{
				if ( (*outMsg)->fDataSize < (kMsgBlockSize * msg.fOf) )
				{					
					//calloc the new block
					aMsg = (sComData *)::calloc( 1, sizeof(sComData) + (kMsgBlockSize * msg.fOf) );
					//copy the old over mainly for the header info
					memcpy(aMsg, *outMsg, sizeof(sComData) + (*outMsg)->fDataSize); //not used?
					//free the old block
					free(*outMsg);
					//assign the block back
					*outMsg = aMsg;
					//clear the temp block
					aMsg = nil;
					//get the new size
					(*outMsg)->fDataSize = kMsgBlockSize * msg.fOf;
				}
				::memcpy( (*outMsg)->obj, msg.obj, kObjSize );
			}

			if ( msg.fCount == ++last )
			{
				// Copy the data into the destination buffer
				::memcpy( (char *)((*outMsg)->data) + offset, msg.fData, kMsgBlockSize );

				// Move the offset
				// xxxx check for buffer overflow
				offset += kMsgBlockSize;

				if ( msg.fCount >= msg.fOf )
				{
					done = true;
				}
			}
			else
			{
				result = eServerSendError;
				LOG3( kStdErr, "*** DS Error in: %s at: %d: err = %d\n", __FILE__, __LINE__, result );
			}
		}
		//could actually handle messages that are not of the "data" size now ie. notifications
		else
		{
			LOG3( kStdErr, "*** DS Error in: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
			if (result == MACH_RCV_TIMED_OUT)
			{
				result = eDSServerTimeout;
			}
			else if ( (result == MACH_RCV_INTERRUPTED) || (result == MACH_RCV_PORT_DIED) )
			{
				result = eDSCannotAccessSession;
			}
			else
			{
				result = eIPCReceiveError;
			}
		}
	}

	return( result );

} // GetServerReply

