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
#include <syslog.h>

#include "CClientEndPoint.h"
#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "PrivateTypes.h"
#include "DSSemaphore.h"
#include "DSCThread.h"

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
		fSrvrName = strdup(inSrvrName);
	}

	fServerPort		= MACH_PORT_NULL;
	fReplyPort		= MACH_PORT_NULL;
	fNotifyPort		= MACH_PORT_NULL;
    fRcvPortSet		= MACH_PORT_NULL;

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
	if ( fNotifyPort != 0)
	{
		result = mach_port_destroy( mach_task_self(), fNotifyPort );
		if ( result != eDSNoErr )
		{
			LOG3( kStdErr, "*** DS Error in: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
		}
	}
	if ( fRcvPortSet != 0)
	{
		result = mach_port_destroy( mach_task_self(), fRcvPortSet );
		if ( result != eDSNoErr )
		{
			LOG3( kStdErr, "*** DS Error in: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
		}
	}

	if ( fSrvrName != nil )
	{
		free( fSrvrName );
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
	kern_return_t		result	= eDSNoErr;
	mach_port_limits_t  qlimits;

	try
	{
		result = mach_port_allocate( mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &fReplyPort );
		if ( result != eDSNoErr )
		{
			LOG3( kStdErr, "*** DS Error with mach_port_allocate: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
			throw( (sInt32)result );
		}

        qlimits.mpl_qlimit = MACH_PORT_QLIMIT_MAX;
		//increase the queue depth to the maximum
		result = mach_port_set_attributes(  (mach_task_self)(),
											fReplyPort,
											MACH_PORT_LIMITS_INFO,
											(mach_port_info_t)&qlimits,
											MACH_PORT_LIMITS_INFO_COUNT);
		if ( result != eDSNoErr )
		{
			LOG3( kStdErr, "*** DS Error with mach_port_set_attributes: unable to set reply port queue depth to maximum: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
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
	kern_return_t	result			= eDSNoErr;
	DSSemaphore		timedWait;
	time_t			waitTime		= 0;
	mach_port_t		mach_init_port	= MACH_PORT_NULL;
	int				numberTries		= 3;

	fServerPort = MACH_PORT_NULL;
	result = bootstrap_look_up( bootstrap_port, kDSServiceName, &fServerPort );
	if ( result != eDSNoErr )
	{
		do
		{
			numberTries--;
			
			//lookup mach init port to start/restart DS daemon on demand
			result = bootstrap_look_up( bootstrap_port, kDSStdMachPortName, &mach_init_port );
			if ( result != eDSNoErr )
			{
				syslog( LOG_ALERT, "Error with bootstrap_look_up on mach_init port: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
			}
			else
			{
				sIPCMsg aMsg;
				
				aMsg.fHeader.msgh_bits			= MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND );
				aMsg.fHeader.msgh_size			= kIPCMsgSize - sizeof( mach_msg_security_trailer_t );
				aMsg.fHeader.msgh_id			= 0;
				aMsg.fHeader.msgh_remote_port	= mach_init_port;
				aMsg.fHeader.msgh_local_port	= fReplyPort;
			
				aMsg.fMsgType	= 0;
				aMsg.fCount		= 1;
				aMsg.fPort		= fReplyPort;
				aMsg.fPID		= 0;
				aMsg.fMsgID		= ::time( nil );
				aMsg.fOf		= 1;
				//tickle the mach init port - should this really be required to start the daemon - 1 sec timeout
				mach_msg((mach_msg_header_t *)&aMsg, MACH_SEND_MSG | MACH_SEND_TIMEOUT, aMsg.fHeader.msgh_size, 0, MACH_PORT_NULL, 1 * 1000, MACH_PORT_NULL);
				//don't retain the mach init port since only using it to kickstart the DS daemon if it is not running
				mach_port_destroy(mach_task_self(), mach_init_port);
				mach_init_port = MACH_PORT_NULL;
				syslog( LOG_ALERT, "Sent launch request message to DirectoryService mach_init port\n");
			}
			
			waitTime = ::time( nil ) + 5;

			//try getting the server port regardless if mach_init port lookup succeeded or not
			do
			{
				fServerPort = MACH_PORT_NULL;
				result = bootstrap_look_up( bootstrap_port, kDSServiceName, &fServerPort );
				if (result == eDSNoErr)
				{
					break;
				}
				// Check every .5 seconds
				timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

				// Wait for 5 seconds MAX
				if ( ::time( nil ) > waitTime )
				{
					break;
				}
			}
			while (result != eDSNoErr);
			
			if ( result != eDSNoErr )
			{
				syslog( LOG_ALERT, "Error with bootstrap_look_up on server port after 10 tries: %s at: %d: Msg (%d) = %s\n", __FILE__, __LINE__, result, mach_error_string( result ) );
				result = eServerNotRunning;
				if (mach_init_port != MACH_PORT_NULL)
				{
					mach_port_destroy(mach_task_self(), mach_init_port);
					mach_init_port = MACH_PORT_NULL;
				}
				// Check every 1 second
				timedWait.Wait( (uInt32)(1 * kMilliSecsPerSec) );
			}
		}
		while( (result != eDSNoErr) && (numberTries > 0) );
	}
	
	if ( (result == eDSNoErr) && (fReplyPort != MACH_PORT_NULL) && (fServerPort != MACH_PORT_NULL) )
	{
		fNotifyPort	= MACH_PORT_NULL;
		fRcvPortSet	= MACH_PORT_NULL;
		mach_port_t previous = MACH_PORT_NULL;

		//set up the port set to get the reply and use our fReplyPort
		mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(fNotifyPort));
		mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &(fRcvPortSet));
		mach_port_move_member(mach_task_self(), fReplyPort, fRcvPortSet);
		mach_port_move_member(mach_task_self(), fNotifyPort, fRcvPortSet);
	
		// sign up for port-death notifications of the fServerPort on the fNotifyPort port.
		mach_port_request_notification(	mach_task_self(), fServerPort,
										MACH_NOTIFY_DEAD_NAME, TRUE, fNotifyPort,
										MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);

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

		result = ::mach_msg(	(mach_msg_header_t *)&msg, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
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
			else if ( (result == MACH_SEND_INVALID_DEST) || (result == MACH_SEND_INVALID_REPLY) )
			{
				if ( result == MACH_SEND_INVALID_DEST )
				{
					LOG1( kStdErr, "Error (%s) trying to send mach message to DirectoryServices.  Retrying...\n", mach_error_string( result ) );

					result = Initialize();
					result = CheckForServer();
					if ( result == eDSNoErr)
					{
						msg.fHeader.msgh_remote_port	= fServerPort;
						msg.fHeader.msgh_local_port		= fReplyPort;
						msg.fPort						= fReplyPort;
	
						result = ::mach_msg(	(mach_msg_header_t *)&msg, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
												msg.fHeader.msgh_size, 0, MACH_PORT_NULL, 300 * 1000, MACH_PORT_NULL );
				
						if ( result == MACH_MSG_SUCCESS )
						{
							msg.fCount++;
							offset += kMsgBlockSize;
						}
						else
						{
							result = eIPCSendError;
						}
					}
				}
				else
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

		result = ::mach_msg( (mach_msg_header_t *)&msg, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
								0, kIPCMsgSize, fRcvPortSet, 300 * 1000, MACH_PORT_NULL );

		if ( (	result == MACH_MSG_SUCCESS )
				&& (msg.fHeader.msgh_size == (kIPCMsgSize - sizeof( mach_msg_security_trailer_t )))
				&& ( msg.fHeader.msgh_local_port != fNotifyPort ) )
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
		else if ( msg.fHeader.msgh_local_port == fNotifyPort )
		{
			syslog( LOG_INFO, "DirectoryService daemon has been shutdown - future client API calls can restart the daemon\n" );
			result = eDSCannotAccessSession;

			// this means the server either died or was shutdown - so clean up
			typedef union {
				mach_port_deleted_notification_t del;
				mach_port_destroyed_notification_t des;
				mach_no_senders_notification_t nms;
				mach_send_once_notification_t so;
				mach_dead_name_notification_t dn;
			} notification_t;
			notification_t *notice = (notification_t *)&msg.fHeader;
			mach_port_t dead_port = MACH_PORT_NULL;

			switch (msg.fHeader.msgh_id)
			{
				case MACH_NOTIFY_PORT_DELETED:
					dead_port = notice->del.not_port;
					break;
				case MACH_NOTIFY_PORT_DESTROYED:
					dead_port = notice->des.not_port.name;
					break;
				case MACH_NOTIFY_DEAD_NAME:
					dead_port = notice->dn.not_port;
					break;
				case MACH_NOTIFY_NO_SENDERS:
				case MACH_NOTIFY_SEND_ONCE:
					break;
			}
			if (dead_port != MACH_PORT_NULL) {
				// discard any remaining rights
				mach_port_destroy(mach_task_self(), dead_port);
			}
			break;
		}
		else
		{
			LOG3( kStdErr, "*** DS Error in: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( result ) );
			if (result == MACH_RCV_TIMED_OUT)
			{
				result = eDSServerTimeout;
			}
			else if ( result == MACH_RCV_PORT_DIED )
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

