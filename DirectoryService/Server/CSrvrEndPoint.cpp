/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @header CSrvrEndPoint
 */

#include "CSrvrEndPoint.h"
#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "PrivateTypes.h"
#include "CLog.h"

#include <stdlib.h>				// for malloc()
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <string.h>
#include <syslog.h>

#define kIPCMsgSize	sizeof( sIPCMsg )

extern uInt32				gDaemonPID;

//------------------------------------------------------------------------------
//	* CSrvrEndPoint *****used by BOTH CHandler and CListener class
//
//------------------------------------------------------------------------------

CSrvrEndPoint::CSrvrEndPoint ( char *inSrvrName )
{
	fSrvrName = nil;

	if ( inSrvrName != nil )
	{
		fSrvrName = strdup(inSrvrName);
	}

	fServerPort		= MACH_PORT_NULL;
	fMachInitPort	= MACH_PORT_NULL;
	fHeadPtr		= nil;
} // CSrvrEndPoint


//------------------------------------------------------------------------------
//	* ~CSrvrEndPoint *****used by BOTH CHandler and CListener class
//
//------------------------------------------------------------------------------

CSrvrEndPoint::~CSrvrEndPoint ( void )
{
	kern_return_t	result	= eDSNoErr;

	if ( fSrvrName != nil )
	{
		free( fSrvrName );
		fSrvrName = nil;
	}

	if ( fServerPort != 0)
	{
		result = mach_port_destroy( mach_task_self(), fServerPort );
	}

} // ~CSrvrEndPoint


//------------------------------------------------------------------------------
//	* Initialize *****used only by CListener class
//
//------------------------------------------------------------------------------

sInt32 CSrvrEndPoint::Initialize ( void )
{
	kern_return_t		result	= eDSNoErr;
	mach_port_limits_t  qlimits;

	try
	{
		result = mach_port_allocate( (mach_task_self)(), MACH_PORT_RIGHT_RECEIVE, &fServerPort );
		if ( result != eDSNoErr )
		{
			DBGLOG1( kLogEndpoint, "mach_port_allocate() failed: %s", mach_error_string( result ) );
			ERRORLOG1( kLogEndpoint, "Unable to allocate mach port: %s", mach_error_string( result ) );
			throw( (sInt32)result );
		}

        qlimits.mpl_qlimit = MACH_PORT_QLIMIT_MAX;
		//increase the queue depth to the maximum
		result = mach_port_set_attributes(  (mach_task_self)(),
											fServerPort,
											MACH_PORT_LIMITS_INFO,
											(mach_port_info_t)&qlimits,
											MACH_PORT_LIMITS_INFO_COUNT);
		if ( result != eDSNoErr )
		{
			DBGLOG1( kLogEndpoint, "mach_port_set_attributes() failed: %s", mach_error_string( result ) );
			ERRORLOG1( kLogEndpoint, "Unable to increase the mach server listener queue depth to the maximum: %s", mach_error_string( result ) );
			throw( (sInt32)result );
		}

		result = mach_port_insert_right( (mach_task_self)(), fServerPort, fServerPort, MACH_MSG_TYPE_MAKE_SEND );
		if ( result != eDSNoErr )
		{
			DBGLOG1( kLogEndpoint, "mach_port_insert_right() failed: %s", mach_error_string( result ) );
			ERRORLOG1( kLogEndpoint, "Unable to set mach port rights: %s", mach_error_string( result ) );
			throw( (sInt32)result );
		}

		result = bootstrap_register( bootstrap_port, kDSServiceName, fServerPort );
		if ( result != eDSNoErr )
		{
			DBGLOG2( kLogEndpoint, "File: %s. Line: %d", __FILE__, __LINE__ );
			DBGLOG2( kLogEndpoint, "bootstrap_register() failed: %s (%d)", mach_error_string( result ), result );

			ERRORLOG2( kLogEndpoint, "Unable to register IPC name: \"%s\".  Received error: %s", fSrvrName, mach_error_string( result ) );
			throw( (sInt32)result );
		}
	}

	catch( sInt32 err )
	{
		result = err;
	}

	return( result );

} // Initialize


//------------------------------------------------------------------------------
//	* GetClientMessage *****ONLY used by CListener class
//
//------------------------------------------------------------------------------

void * CSrvrEndPoint::GetClientMessage ( void )
{
	sInt32				result		= MACH_MSG_SUCCESS;
	sComData		   *pOutMsg		= nil;
	sIPCMsg				msg;

 	// KW seems to be a forced method to complete a message during this call
	// what happens if it never completes?

	while ( result == MACH_MSG_SUCCESS )
	{
		pOutMsg = GetNextCompletedMsg();
		if ( pOutMsg != nil )
		{
			break;
		}
		::memset( &msg, 0, sizeof( sIPCMsg ) );

		result = ::mach_msg( (mach_msg_header_t *)&msg, MACH_RCV_MSG, 0, kIPCMsgSize, fServerPort, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL );
		if ( (	result == MACH_MSG_SUCCESS )
				&& (msg.fHeader.msgh_size == (kIPCMsgSize - sizeof( mach_msg_security_trailer_t ))) )
				//&& (msg.fHeader.msgh_bits == MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND ) ) )
		{
			AddDataToMessage( &msg );
		}

		// at this point we can put in an else statement to handle different messages
		// other than our "data" ones that have the fixed data size ie. notifications
		else
		{
			DBGLOG2( kLogEndpoint, "File: %s. Line: %d", __FILE__, __LINE__ );
			DBGLOG2( kLogEndpoint, "  *** mach_msg () failed: %s (%d)", mach_error_string( result ), result );

			if (result != MACH_MSG_SUCCESS)
				ERRORLOG1( kLogEndpoint, "mach message receive error: %s", mach_error_string( result ) );
		}
	}

	return( pOutMsg );

} // GetClientMessage


//------------------------------------------------------------------------------
//	* SendClientReply *****ONLY used by CHandler class
//
//------------------------------------------------------------------------------

sInt32 CSrvrEndPoint::SendClientReply ( void *inMsg )
{
	sInt32				result		= MACH_MSG_SUCCESS;
	uInt32				offset		= 0;
	sInt32				bytesLeft	= 0;
	sComData		   *pData		= (sComData *)inMsg;
	sIPCMsg				msg;

	msg.fHeader.msgh_bits			= MACH_MSGH_BITS( MACH_MSG_TYPE_COPY_SEND, 0 );
	msg.fHeader.msgh_size			= kIPCMsgSize - sizeof( mach_msg_security_trailer_t );
	msg.fHeader.msgh_id				= pData->head.msgh_id;
	msg.fHeader.msgh_remote_port	= pData->head.msgh_remote_port;
	msg.fHeader.msgh_local_port		= MACH_PORT_NULL;
	msg.fCount						= 1;
	msg.fPID						= gDaemonPID;
	msg.fOf							= pData->fDataLength / kMsgBlockSize;

	if ((pData-> fDataLength % kMsgBlockSize) != 0)
	{
		msg.fOf++;
	}

	//here for the server fDataLength is the
	//true size of the data buffer which might be zero
	//but in that case we must still send a message
	if (msg.fOf == 0)
	{
		msg.fOf = 1;
	}

	::memcpy( msg.obj, pData->obj, kObjSize );

	bytesLeft = pData-> fDataLength;

	while ( (msg.fCount <= msg.fOf) && (result == MACH_MSG_SUCCESS) )
	{
		::memset( msg.fData, 0, kIPCMsgLen );

		if ( bytesLeft >= (sInt32)kMsgBlockSize )
		{
			::memcpy( msg.fData, (char *)(pData->data) + offset, kMsgBlockSize );
			bytesLeft -= kMsgBlockSize;
		}
		else
		{
			if ( bytesLeft != 0 )
			{
				::memcpy( msg.fData, (char *)(pData->data) + offset, bytesLeft );
			}
			bytesLeft = 0;
		}

		if (msg.fCount == msg.fOf)  // delete send write after last message
		{
			msg.fHeader.msgh_bits = MACH_MSGH_BITS( MACH_MSG_TYPE_MOVE_SEND, 0 );
		}
		//only have wait of 15 secs for our reply back to our FW
		result = ::mach_msg(	(mach_msg_header_t *)&msg, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
								msg.fHeader.msgh_size, 0, MACH_PORT_NULL, 15 * 1000, MACH_PORT_NULL );
		if ( result == MACH_MSG_SUCCESS )
		{
			msg.fCount++;
			offset += kMsgBlockSize;
		}
		else
		{
			DBGLOG2( kLogEndpoint, "File: %s. Line: %d", __FILE__, __LINE__ );
			DBGLOG2( kLogEndpoint, "  *** mach_msg () failed: %s (%d)", mach_error_string( result ), result );

			ERRORLOG1( kLogEndpoint, "mach message send error: %s", mach_error_string( result ) );
		}
	}

	return( result );

} // SendClientReply


//------------------------------------------------------------------------------
//	* MakeNewMsgPtr *****ONLY used by CListener class
//
//------------------------------------------------------------------------------

sComData* CSrvrEndPoint::MakeNewMsgPtr ( sIPCMsg *inMsg )
{
	uInt32				msgSize		= 0;
	sComData		   *pOutMsg		= nil;

	msgSize					= sizeof( sComData ) + (inMsg->fOf * kIPCMsgLen);
	pOutMsg					= (sComData *)::calloc( 1, msgSize );
	pOutMsg->fDataSize		= (inMsg->fOf * kIPCMsgLen);
	pOutMsg->fDataLength	= 0;
	

	pOutMsg->head.msgh_bits			 = inMsg->fHeader.msgh_bits;
	pOutMsg->head.msgh_size			 = inMsg->fHeader.msgh_size;
	pOutMsg->head.msgh_remote_port	 = inMsg->fHeader.msgh_remote_port;
	pOutMsg->head.msgh_local_port	 = inMsg->fHeader.msgh_local_port;
	pOutMsg->head.msgh_reserved		 = inMsg->fHeader.msgh_reserved;
	pOutMsg->head.msgh_id			 = inMsg->fHeader.msgh_id;
	pOutMsg->type.msgt_name			 = inMsg->fMsgType;

	pOutMsg->fMsgID		= inMsg->fMsgID;
	pOutMsg->fPID		= inMsg->fPID;
	pOutMsg->fPort		= inMsg->fHeader.msgh_remote_port;
	
	::memcpy( pOutMsg->obj, inMsg->obj, kObjSize );

	return( pOutMsg );

} // MakeNewMsgPtr


//------------------------------------------------------------------------------
//	* GetNextCompletedMsg *****ONLY used by CListener class
//
//------------------------------------------------------------------------------

sComData* CSrvrEndPoint::GetNextCompletedMsg ( void )
{
	sComData	   *pOutMsg		= nil;
	sMsgList	   *pThisPtr	= fHeadPtr;
	sMsgList	   *pPrevPtr	= fHeadPtr;

	while ( pThisPtr != nil )
	{
		if ( pThisPtr->fComplete == true )
		{
			if ( pThisPtr == fHeadPtr )
			{
				fHeadPtr = pThisPtr->fNext;
			}
			else
			{
				pPrevPtr->fNext = pThisPtr->fNext;
			}

			pOutMsg = pThisPtr->fData;
			delete( pThisPtr );
			pThisPtr = nil;
			break;
		}
		pPrevPtr = pThisPtr;
		pThisPtr = pThisPtr->fNext;
	}

	return( pOutMsg );

} // GetNextCompletedMsg


//------------------------------------------------------------------------------
//	* AddNewMessage *****ONLY used by CListener class
//
//------------------------------------------------------------------------------

void CSrvrEndPoint::AddNewMessage ( sComData *inNewMsg, sIPCMsg *inMsgData )
{
	sMsgList	   *pNewMsg	= nil;

	pNewMsg = (sMsgList *)::calloc( sizeof( sMsgList ), sizeof( char ) );
	if ( pNewMsg != nil )
	{
		pNewMsg->fComplete = false;

		pNewMsg->fData = inNewMsg;
		pNewMsg->fMsgID = inMsgData->fMsgID;
		pNewMsg->fPortID = inMsgData->fPID;
		pNewMsg->fTime = ::time( nil );
		pNewMsg->fOffset = 0;
		//KW let's be consistent in using the port attributes
//		pNewMsg->fPort = inMsgData->fHeader.msgh_remote_port;

		::memcpy( (char *)(inNewMsg->data) + pNewMsg->fOffset, inMsgData->fData, kMsgBlockSize );
		pNewMsg->fOffset += kMsgBlockSize;

		//KW can we be sure that the data is sent in the correct ORDER ie. thread latency reorders the data?
		if ( inMsgData->fCount == inMsgData->fOf )
		{
			pNewMsg->fComplete = true;
		}

		pNewMsg->fNext = fHeadPtr;
		fHeadPtr = pNewMsg;
	}

} // AddNewMessage


//------------------------------------------------------------------------------
//	* AddDataToMessage *****ONLY used by CListener class
//
//------------------------------------------------------------------------------

void CSrvrEndPoint::AddDataToMessage ( sIPCMsg *inMsgData )
{
	sMsgList	   *pThisPtr	= fHeadPtr;
	sComData	   *pTheMsg		= nil;
	bool			bFound   = false;

	while ( pThisPtr != nil )
	{
//KW would like to check more than these three items
		if ( (pThisPtr->fMsgID == inMsgData->fMsgID) &&
			 (pThisPtr->fPortID == inMsgData->fPID) &&
			 (pThisPtr->fComplete == false) )
		{
			bFound  = true;

			pTheMsg = pThisPtr->fData;
			::memcpy( (char *)(pTheMsg->data) + pThisPtr->fOffset, inMsgData->fData, kMsgBlockSize );
			pThisPtr->fOffset += kMsgBlockSize;

			if ( inMsgData->fCount == inMsgData->fOf )
			{
				pThisPtr->fComplete = true;
			}
			break;
		}
		pThisPtr = pThisPtr->fNext;
	}

	if ( !bFound )
	{
		sComData *pComData = MakeNewMsgPtr(inMsgData);
		AddNewMessage(pComData, inMsgData);
	}
} // AddDataToMessage

